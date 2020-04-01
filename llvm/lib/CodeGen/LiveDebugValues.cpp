//===- LiveDebugValues.cpp - Tracking Debug Value MIs ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// This pass implements a data flow analysis that propagates debug location
/// information by inserting additional DBG_VALUE insts into the machine
/// instruction stream. Before running, each DBG_VALUE inst corresponds to a
/// source assignment of a variable. Afterwards, a DBG_VALUE inst specifies a
/// variable location for the current basic block (see SourceLevelDebugging.rst).
///
/// This is a separate pass from DbgValueHistoryCalculator to facilitate
/// testing and improve modularity.
///
/// Each variable location is represented by a VarLoc object that identifies the
/// source variable, its current machine-location, and the DBG_VALUE inst that
/// specifies the location. Each VarLoc is indexed in the (function-scope)
/// VarLocMap, giving each VarLoc a unique index. Rather than operate directly
/// on machine locations, the dataflow analysis in this pass identifies
/// locations by their index in the VarLocMap, meaning all the variable
/// locations in a block can be described by a sparse vector of VarLocMap
/// indexes.
///
//===----------------------------------------------------------------------===//

#include "llvm/ADT/CoalescingBitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/UniqueVector.h"
#include "llvm/CodeGen/LexicalScopes.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/PseudoSourceValue.h"
#include "llvm/CodeGen/RegisterScavenging.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <functional>
#include <queue>
#include <tuple>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "livedebugvalues"

STATISTIC(NumInserted, "Number of DBG_VALUE instructions inserted");
STATISTIC(NumRemoved, "Number of DBG_VALUE instructions removed");

namespace {

using VarLocSet = CoalescingBitVector<uint64_t>;

// The location at which a spilled variable resides. It consists of a
// register and an offset.
struct SpillLoc {
  unsigned SpillBase;
  int SpillOffset;
  bool operator==(const SpillLoc &Other) const {
    return SpillBase == Other.SpillBase && SpillOffset == Other.SpillOffset;
  }
  bool operator<(const SpillLoc &Other) const {
    return std::tie(SpillBase, SpillOffset) < std::tie(Other.SpillBase, Other.SpillOffset);
  }
};

class ValueIDNum {
public:
  uint64_t BlockNo : 16;
  uint64_t InstNo : 20;
  uint64_t LocNo : 14;

  uint64_t asU64() const {
    uint64_t tmp_block = BlockNo;
    uint64_t tmp_inst = InstNo;
    return tmp_block << 34ull | tmp_inst << 14 | LocNo;
  }

  static ValueIDNum fromU64(uint64_t v) {
    return {v >> 34ull, ((v >> 14) & 0xFFFFF), v & 0x3FFF};
  }

 bool operator<(const ValueIDNum &Other) const {
   return asU64() < Other.asU64();
 }

 bool operator==(const ValueIDNum &Other) const {
   return std::tie(BlockNo, InstNo, LocNo) ==
          std::tie(Other.BlockNo, Other.InstNo, Other.LocNo);
 }

   bool operator!=(const ValueIDNum &Other) const {
    return !(*this == Other);
   }

  std::string asString(const TargetRegisterInfo *TRI) const {
    std::string regname;
    if (LocNo < TRI->getNumRegs())
      regname = TRI->getRegAsmName(LocNo).str();
    else
      regname = Twine("slot ").concat(Twine(LocNo - TRI->getNumRegs())).str();
    assert(regname != "");
    return Twine("bb ").concat(
           Twine(BlockNo).concat(
           Twine(" inst ").concat(
           Twine(InstNo).concat(
           Twine(" loc ").concat(
           Twine(regname)))))).str();
  }
};
} // end anon namespace

namespace llvm {
template <> struct DenseMapInfo<ValueIDNum> {
  // NB, there's a risk of overlap of uint64_max with legitmate numbering if
  // there are very many machine locations. Fix by not bit packing so hard.
  static const uint64_t MaxVal = std::numeric_limits<uint64_t>::max();

  static inline ValueIDNum getEmptyKey() { return ValueIDNum::fromU64(MaxVal); }

  static inline ValueIDNum getTombstoneKey() { return ValueIDNum::fromU64(MaxVal - 1); }

  static unsigned getHashValue(ValueIDNum num) {
    return hash_value(num.asU64());
  }

  static bool isEqual(const ValueIDNum &A, const ValueIDNum &B) { return A == B; }
};
} // end namespace llvm


namespace {

class VarLocPos {
public:
  ValueIDNum ID;
  uint64_t CurrentLoc : 14;

  uint64_t asU64() const {
    return ID.asU64() << 14 | CurrentLoc;
  }

  static VarLocPos fromU64(uint64_t v) {
    return {ValueIDNum::fromU64(v >> 14), v & 0x3FFF};
  }

  bool operator==(const VarLocPos &Other) const {
    return std::tie(ID, CurrentLoc) == std::tie(Other.ID, Other.CurrentLoc);
  }

  std::string asString(const TargetRegisterInfo *TRI) const {
    std::string regname;
    if (CurrentLoc < TRI->getNumRegs())
      regname = TRI->getRegAsmName(CurrentLoc).str();
    else
      regname = Twine("slot ").concat(Twine(CurrentLoc - TRI->getNumRegs())).str();
    return Twine("VLP(").concat(ID.asString(TRI)).concat(",cur ").concat(regname).concat(")").str();
  }
};

typedef std::pair<const DIExpression *, bool> MetaVal;

class ValueRec {
public:
  ValueIDNum ID;
  Optional<MachineOperand> MO;
  MetaVal meta;
  unsigned BlockPHI = 0;

  typedef enum { Def, Const, PHI } KindT;
  KindT Kind;

  void dump(const TargetRegisterInfo *TRI) const {
    if (Kind == Const) {
      MO->dump();
    } else if (Kind == PHI) {
      dbgs() << "PHI-bb" << BlockPHI << "\n";
    } else {
      assert(Kind == Def);
      dbgs() << ID.asString(TRI);
    }
    if (meta.second)
      dbgs() << " indir";
    if (meta.first)
      dbgs() << " " << *meta.first;
  }

  bool operator<(const ValueRec &Other) const {
    if (meta != Other.meta)
      return meta < Other.meta;

    if (Kind == Const && Other.Kind == Const) {
      if (MO->getType() == Other.MO->getType()) {
        if (MO->isImm())
          return MO->getImm() < Other.MO->getImm(); 
        else if (MO->isCImm())
          return MO->getCImm() < Other.MO->getCImm(); 
        else if (MO->isFPImm())
          return MO->getFPImm() < Other.MO->getFPImm(); 
        else
          abort();
      } else {
        return MO->getType() < Other.MO->getType();
      }
    } else if (Kind == PHI && Other.Kind == PHI) {
      return BlockPHI < Other.BlockPHI;
    } else if (Kind == Def && Other.Kind == Def) {
      return ID < Other.ID;
    } else {
      return Kind < Other.Kind;
    }
  }
};

typedef UniqueVector<std::pair<DebugVariable, ValueRec>> lolnumberingt;
typedef DenseMap<uint64_t, uint64_t> vphitomphit;
typedef DenseMap<std::pair<const MachineBasicBlock *, ValueIDNum>, ValueIDNum> mphiremapt;

class MLocTracker {
public:
  VarLocSet::Allocator &Alloc;
  std::vector<ValueIDNum> MachineLocsToIDNums;
  unsigned NumRegs;
  UniqueVector<SpillLoc> SpillsToMLocs;

  MLocTracker(VarLocSet::Allocator &Alloc, unsigned NumRegs)
    : Alloc(Alloc), NumRegs(NumRegs) {
    MachineLocsToIDNums.resize(NumRegs);
    reset();
  }

  VarLocPos getVarLocPos(unsigned idx) const {
    return {MachineLocsToIDNums[idx], idx};
  }

  unsigned getNumLocs(void) const {
    return MachineLocsToIDNums.size();
  }

  VarLocSet makeVarLocSet(void) const {
    VarLocSet set(Alloc);
    for (unsigned idx = 0; idx < MachineLocsToIDNums.size(); ++idx) {
      if (MachineLocsToIDNums[idx].LocNo == 0)
        continue;
      set.set(getVarLocPos(idx).asU64());
    }
    return set;
  }

  void loadFromVarLocSet(const VarLocSet &vls, unsigned cur_bb) {
    // Quickly reset everything to being itself at inst 0, representing a phi.
    for (unsigned ID = 0; ID < MachineLocsToIDNums.size(); ++ID) {
      MachineLocsToIDNums[ID] = {cur_bb, 0, ID};
    }

    for (auto ID : vls) {
      auto pos = VarLocPos::fromU64(ID);
      MachineLocsToIDNums[pos.CurrentLoc] = pos.ID;
    }
  }

  void lolremap(const MachineBasicBlock *MBB, const mphiremapt &mphiremap) {
    for (unsigned ID = 0; ID < MachineLocsToIDNums.size(); ++ID) {
      if (MachineLocsToIDNums[ID].InstNo == 0) {
        auto it = mphiremap.find(std::make_pair(MBB, MachineLocsToIDNums[ID]));
        if (it != mphiremap.end())
          MachineLocsToIDNums[ID] = it->second;
      }
    }
  }

  void reset(void) {
    memset(&MachineLocsToIDNums[0], 0, MachineLocsToIDNums.size() * sizeof(ValueIDNum));
  }

  void clear(void) {
    MachineLocsToIDNums.clear();
    //SpillsToMLocs.reset(); XXX can't reset?
    SpillsToMLocs = decltype(SpillsToMLocs)();
  }

  void defReg(Register r, unsigned bb, unsigned inst) {
    ValueIDNum id = {bb, inst, r};
    MachineLocsToIDNums[r] = id;
  }

  void setReg(Register r, ValueIDNum id) {
    MachineLocsToIDNums[r] = id;
  }

  // Because we need to replicate values only having one location for now.
  void lolwipe(Register r) {
    MachineLocsToIDNums[r] = {0, 0, 0};
  }

  void defSpill(SpillLoc l, unsigned bb, unsigned inst) {
    unsigned SpillID = SpillsToMLocs.idFor(l);
    if (SpillID == 0) {
      SpillID = SpillsToMLocs.insert(l);
      SpillID += NumRegs - 1;
      ValueIDNum id = {bb, inst, SpillID};
      MachineLocsToIDNums.push_back(id);
      assert(MachineLocsToIDNums.size() == SpillID + 1);
    } else {
      ValueIDNum id = {bb, inst, SpillID + NumRegs - 1};
      MachineLocsToIDNums[NumRegs + SpillID - 1] = id;
    }
  }

  // xxx duplication
  void setSpill(SpillLoc l, ValueIDNum id) {
    unsigned SpillID = SpillsToMLocs.idFor(l);
    if (SpillID == 0) {
      SpillID = SpillsToMLocs.insert(l);
      SpillID += NumRegs - 1;
      MachineLocsToIDNums.push_back(id);
      assert(MachineLocsToIDNums.size() == SpillID + 1);
    } else {
      MachineLocsToIDNums[NumRegs + SpillID - 1] = id;
    }
  }

  void lolwipe(SpillLoc l) {
    unsigned SpillID = SpillsToMLocs.idFor(l);
    assert(SpillID != 0);
    MachineLocsToIDNums[NumRegs + SpillID - 1] = {0, 0, 0};
  }



  ValueIDNum readReg(Register r) {
    return MachineLocsToIDNums[r];
  }

  ValueIDNum readSpill(SpillLoc l) {
    unsigned pos = SpillsToMLocs.idFor(l);
    if (pos == 0)
      // Returning no location -> 0 means $noreg and some hand wavey position
      return {0, 0, 0};
    return MachineLocsToIDNums[NumRegs + pos - 1];
  }

  unsigned getSpillMLoc(SpillLoc l) {
    unsigned SpillID = SpillsToMLocs.idFor(l);
    if (SpillID == 0)
      return 0;
    SpillID += NumRegs - 1;
    return SpillID;
  }

  bool isSpill(unsigned mloc) const {
    return mloc >= NumRegs;
  }

  void dump(const TargetRegisterInfo *TRI) {
    for (unsigned int ID = 0; ID < NumRegs; ++ID) {
      auto &num = MachineLocsToIDNums[ID];
      if (num.LocNo == 0)
        continue;
      std::string defname = num.asString(TRI);
      dbgs() << TRI->getRegAsmName(ID) << " --> " << defname << "\n";
    }
    for (unsigned int ID = NumRegs; ID < MachineLocsToIDNums.size(); ++ID) {
      auto &num = MachineLocsToIDNums[ID];
      if (num.LocNo == 0)
        continue;
      std::string lolslot = Twine("slot ").concat(Twine(ID - NumRegs)).str();
      std::string defname = num.asString(TRI);
      dbgs() << lolslot << " --> " << defname << "\n";
    }
  }
};

// Types for recording sets of variable fragments that overlap. For a given
// local variable, we record all other fragments of that variable that could
// overlap it, to reduce search time.
using FragmentOfVar =
    std::pair<const DILocalVariable *, DIExpression::FragmentInfo>;
using OverlapMap =
    DenseMap<FragmentOfVar, SmallVector<DIExpression::FragmentInfo, 1>>;

class VLocTracker {
public:
  // Map the DebugVariable to recent primary location ID.
  // xxx determinism?
  // This is the one that actually reduces things :o
  MapVector<DebugVariable, ValueRec> Vars;

public:
  VLocTracker() {}

  void defVar(const MachineInstr &MI, ValueIDNum ID) {
    // XXX skipping overlapping fragments for now.
    assert(MI.isDebugValue());
    DebugVariable Var(MI.getDebugVariable(), MI.getDebugExpression(),
                      MI.getDebugLoc()->getInlinedAt());
    MetaVal m = {MI.getDebugExpression(), MI.getOperand(1).isImm()};
    Vars[Var] = {ID, None, m, 0, ValueRec::Def};
  }

  void defVar(const MachineInstr &MI, const MachineOperand &MO) {
    // XXX skipping overlapping fragments for now.
    assert(MI.isDebugValue());
    DebugVariable Var(MI.getDebugVariable(), MI.getDebugExpression(),
                      MI.getDebugLoc()->getInlinedAt());
    MetaVal m = {MI.getDebugExpression(), MI.getOperand(1).isImm()};
    Vars[Var] = {{0, 0, 0}, MO, m, 0, ValueRec::Const};
  }
};

class TransferTracker {
public:
  const TargetInstrInfo *TII;
  MLocTracker *mlocs;
  MachineFunction &MF;

  struct Transfer {
    MachineBasicBlock::iterator pos;
    MachineBasicBlock *MBB;
    std::vector<MachineInstr *> insts;
  };

  typedef std::pair<unsigned, MetaVal> hahaloc;
  std::vector<Transfer> Transfers;

  // MapVector for nondeterminism
  DenseMap<unsigned, MapVector<DebugVariable, unsigned>> ActiveMLocs;
  DenseMap<DebugVariable, hahaloc> ActiveVLocs;

  TransferTracker(const TargetInstrInfo *TII, MLocTracker *mlocs, MachineFunction &MF) : TII(TII), mlocs(mlocs), MF(MF) { }

  void loadInlocs(MachineBasicBlock &MBB, lolnumberingt &lolnumbering, const mphiremapt &mphiremap, VarLocSet &mlocs, VarLocSet &vlocs, unsigned cur_bb) {  
    ActiveMLocs.clear();
    ActiveVLocs.clear();

    DenseMap<ValueIDNum, unsigned> tmpmap;

    for (auto ID : mlocs) {
      // Each mloc is a VarLocPos
      auto VLP = VarLocPos::fromU64(ID);
      // Produce a map of value numbers to the current machine locs they live
      // in. There should only be one machine loc per value.
      assert(tmpmap.find(VLP.ID) == tmpmap.end()); // XXX expensie
      tmpmap[VLP.ID] = VLP.CurrentLoc;
    }

    // Now map variables to their current machine locs
    std::vector<MachineInstr *> inlocs;
    for (auto ID : vlocs) {
      auto &Var = lolnumbering[ID];
      if (Var.second.Kind == ValueRec::Const) {
        inlocs.push_back(emitMOLoc(*Var.second.MO, Var.first, Var.second.meta));
        continue;
      }

      // Unresolved PHI -> skip
      if (Var.second.Kind == ValueRec::PHI)
        continue;
      assert(Var.second.Kind == ValueRec::Def);

      auto InsertLiveIn = [&](unsigned m) {
        ActiveVLocs[Var.first] = std::make_pair(m, Var.second.meta);
        ActiveMLocs[m].insert(std::make_pair(Var.first, 0));
        assert(m != 0);
        inlocs.push_back(emitLoc(m, Var.first, Var.second.meta));
      };


      // Value unavailable / has no machine loc -> define no location.
      auto hahait = tmpmap.find(Var.second.ID);
      if (hahait != tmpmap.end()) {
        InsertLiveIn(hahait->second);
        continue;
      }

      // Unless this is actually an mloc phi,
      auto &IDNum = Var.second.ID;
      if (IDNum.InstNo != 0)
        continue;

      // Possssiiibbblly remap it.
      // Complete bullshit code, but just proving a point right now.
      auto mphiit= mphiremap.find(std::make_pair(&MBB, IDNum));
      if (mphiit != mphiremap.end()) {
        auto again = tmpmap.find(mphiit->second);
        if (again != tmpmap.end()) {
          InsertLiveIn(again->second);
        } else if (mphiit->second.BlockNo == cur_bb && mphiit->second.InstNo == 0) {
          InsertLiveIn(mphiit->second.LocNo);
        }
      } else if (IDNum.BlockNo == cur_bb) {
        InsertLiveIn(IDNum.LocNo);
      }
    }
    if (inlocs.size() > 0)
      Transfers.push_back({MBB.begin(), &MBB, std::move(inlocs)});
  }

  void redefVar(const MachineInstr &MI) {
    DebugVariable Var(MI.getDebugVariable(), MI.getDebugExpression(),
                      MI.getDebugLoc()->getInlinedAt());
    const MachineOperand &MO = MI.getOperand(0);

    // Erase any previous location,
    auto It = ActiveVLocs.find(Var);
    if (It != ActiveVLocs.end()) {
      ActiveMLocs[It->second.first].erase(Var);
    }

    // Insert a new vloc. Ignore non-register locations, we don't transfer
    // those, and can't current describe spill locs independently of regs.
    if (!MO.isReg() || MO.getReg() == 0) {
      if (It != ActiveVLocs.end())
        ActiveVLocs.erase(It);
      return;
    }

    unsigned Reg = MO.getReg();
    MetaVal meta = {MI.getDebugExpression(), MI.getOperand(1).isImm()};

    ActiveMLocs[Reg].insert(std::make_pair(Var, 0));
    if (It == ActiveVLocs.end()) {
      ActiveVLocs.insert(std::make_pair(Var, std::make_pair(Reg, meta)));
    } else {
      It->second.first = Reg;
      It->second.second = meta;
    }
  }

  void clobberMloc(unsigned mloc, MachineBasicBlock::iterator pos) {
    auto It = ActiveMLocs.find(mloc);
    if (It == ActiveMLocs.end())
      return;

    std::vector<MachineInstr *>insts;
    for (auto &Var : It->second) {
      auto ALoc = ActiveVLocs.find(Var.first);
      if (mlocs->isSpill(mloc)) {
        // Create an undef. We can't feed in a nullptr DIExpression alas,
        // so use the variables last expression.
        const DIExpression *Expr = ALoc->second.second.first;
        insts.push_back(emitLoc(0, Var.first, {Expr, false}));
      }
      ActiveVLocs.erase(ALoc);
    }
    if (insts.size() != 0)
      Transfers.push_back({std::next(pos), pos->getParent(), std::move(insts)});

    It->second.clear();
  }

  void transferMlocs(unsigned src, unsigned dst, MachineBasicBlock::iterator pos) {
    // Legitimate scenario on account of un-clobbered slot being assigned to?
    //assert(ActiveMLocs[dst].size() == 0);
    ActiveMLocs[dst] = ActiveMLocs[src];

    std::vector<MachineInstr *> instrs;
    for (auto &Var : ActiveMLocs[src]) {
      auto it = ActiveVLocs.find(Var.first);
      assert(it != ActiveVLocs.end());
      it->second.first = dst;

      assert(dst != 0);
      MachineInstr *MI = emitLoc(dst, Var.first, it->second.second);
      instrs.push_back(MI);
    }
    ActiveMLocs[src].clear();
    if (instrs.size() > 0)
      Transfers.push_back({std::next(pos), pos->getParent(), std::move(instrs)});
  }

  MachineInstrBuilder 
  emitMOLoc(const MachineOperand &MO,
              const DebugVariable &Var, const MetaVal &meta) {
    DebugLoc DL = DebugLoc::get(0, 0, Var.getVariable()->getScope(), Var.getInlinedAt());
    auto MIB = BuildMI(MF, DL, TII->get(TargetOpcode::DBG_VALUE));
    MIB.add(MO);
    if (meta.second)
      MIB.addImm(0);
    else
      MIB.addReg(0);
    MIB.addMetadata(Var.getVariable());
    MIB.addMetadata(meta.first);
    return MIB;
  }

  MachineInstrBuilder 
  emitLoc(unsigned MLoc, const DebugVariable &Var, const MetaVal &meta) {
    DebugLoc DL = DebugLoc::get(0, 0, Var.getVariable()->getScope(), Var.getInlinedAt());
    auto MIB = BuildMI(MF, DL, TII->get(TargetOpcode::DBG_VALUE));

    const DIExpression *Expr = meta.first;
    if (MLoc < mlocs->NumRegs) {
      MIB.addReg(MLoc, RegState::Debug);
      if (meta.second)
        MIB.addImm(0);
      else
        MIB.addReg(0, RegState::Debug);
    } else {
      const SpillLoc &Loc = mlocs->SpillsToMLocs[MLoc - mlocs->NumRegs + 1];
      Expr = DIExpression::prepend(Expr, DIExpression::ApplyOffset, Loc.SpillOffset);
      unsigned Base = Loc.SpillBase;
      MIB.addReg(Base, RegState::Debug);
      MIB.addImm(0);
    }

    MIB.addMetadata(Var.getVariable());
    MIB.addMetadata(Expr);
    return MIB;
  }
};

class LiveDebugValues : public MachineFunctionPass {
private:
  const TargetRegisterInfo *TRI;
  const TargetInstrInfo *TII;
  const TargetFrameLowering *TFI;
  BitVector CalleeSavedRegs;
  LexicalScopes LS;
  VarLocSet::Allocator Alloc;

  MLocTracker *tracker;
  unsigned cur_bb;
  unsigned cur_inst;
  VLocTracker *vtracker;
  TransferTracker *ttracker;

  using FragmentInfo = DIExpression::FragmentInfo;
  using OptFragmentInfo = Optional<DIExpression::FragmentInfo>;

  using VarLocInMBB = SmallDenseMap<const MachineBasicBlock *, VarLocSet>;

  // Helper while building OverlapMap, a map of all fragments seen for a given
  // DILocalVariable.
  using VarToFragments =
      DenseMap<const DILocalVariable *, SmallSet<FragmentInfo, 4>>;

  VarLocSet &getVarLocsInMBB(const MachineBasicBlock *MBB, VarLocInMBB &Locs) {
    auto Result = Locs.try_emplace(MBB, Alloc);
    return Result.first->second;
  }

  const VarLocSet &getVarLocsInMBB(const MachineBasicBlock *MBB,
                                   const VarLocInMBB &Locs) const {
    auto It = Locs.find(MBB);
    assert(It != Locs.end() && "MBB not in map");
    return It->second;
  }

  /// Tests whether this instruction is a spill to a stack location.
  bool isSpillInstruction(const MachineInstr &MI, MachineFunction *MF);

  /// Decide if @MI is a spill instruction and return true if it is. We use 2
  /// criteria to make this decision:
  /// - Is this instruction a store to a spill slot?
  /// - Is there a register operand that is both used and killed?
  /// TODO: Store optimization can fold spills into other stores (including
  /// other spills). We do not handle this yet (more than one memory operand).
  bool isLocationSpill(const MachineInstr &MI, MachineFunction *MF,
                       unsigned &Reg);

  /// If a given instruction is identified as a spill, return the spill location
  /// and set \p Reg to the spilled register.
  Optional<SpillLoc> isRestoreInstruction(const MachineInstr &MI,
                                                  MachineFunction *MF,
                                                  unsigned &Reg);
  /// Given a spill instruction, extract the register and offset used to
  /// address the spill location in a target independent way.
  SpillLoc extractSpillBaseRegAndOffset(const MachineInstr &MI);

  void transferDebugValue(const MachineInstr &MI);
  void transferSpillOrRestoreInst(MachineInstr &MI);
  void transferRegisterCopy(MachineInstr &MI);
  void transferRegisterDef(MachineInstr &MI);

  void process(MachineInstr &MI);

  void accumulateFragmentMap(MachineInstr &MI, VarToFragments &SeenFragments,
                             OverlapMap &OLapMap);

  bool join(MachineBasicBlock &MBB, VarLocInMBB &OutLocs, VarLocInMBB &InLocs,
            SmallPtrSet<const MachineBasicBlock *, 16> &Visited,
            SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks);

  bool vloc_join(const MachineBasicBlock &MBB, VarLocInMBB &VLOCOutLocs,
                 VarLocInMBB &VLOCInLocs, lolnumberingt &lolnumbering,
                 SmallPtrSet<const MachineBasicBlock *, 16> &VLOCVisited,
                 SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks,
                 VarLocInMBB &VLOCScopeMasks,
                 unsigned cur_bb);
  bool vloc_transfer(VarLocSet &ilocs, VarLocSet &transfer, VarLocSet &olocs, lolnumberingt &lolnumbering);


  void resolveMPHIs(mphiremapt &mphiremap, MachineBasicBlock &MBB, VarLocSet &InLocs, VarLocInMBB &MLOCOutLocs, unsigned cur_bb);
  void resolveVPHIs(vphitomphit &vphitomphi, const mphiremapt &mphiremap, lolnumberingt &lolnumbering, MachineBasicBlock &MBB, VarLocSet &InLocs, VarLocInMBB &VLOCOutLocs, VarLocInMBB &MLOCOutLocs, unsigned cur_bb);

  bool ExtendRanges(MachineFunction &MF);

public:
  static char ID;

  /// Default construct and initialize the pass.
  LiveDebugValues();

  /// Tell the pass manager which passes we depend on and what
  /// information we preserve.
  void getAnalysisUsage(AnalysisUsage &AU) const override;

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties().set(
        MachineFunctionProperties::Property::NoVRegs);
  }

  void emitDbgValue(MLocTracker *mlocs, ValueRec &theloc, DebugVariable &Var,
                    MachineBasicBlock::iterator pos);

  void UpdateVlocMask(lolnumberingt &lolnumbering, unsigned ID,
                      VarLocInMBB &VLOCScopeMasks,
                 SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks);

  /// Calculate the liveness information for the given machine function.
  bool runOnMachineFunction(MachineFunction &MF) override;
};

} // end anonymous namespace


//===----------------------------------------------------------------------===//
//            Implementation
//===----------------------------------------------------------------------===//

char LiveDebugValues::ID = 0;

char &llvm::LiveDebugValuesID = LiveDebugValues::ID;

INITIALIZE_PASS(LiveDebugValues, DEBUG_TYPE, "Live DEBUG_VALUE analysis",
                false, false)

/// Default construct and initialize the pass.
LiveDebugValues::LiveDebugValues() : MachineFunctionPass(ID) {
  initializeLiveDebugValuesPass(*PassRegistry::getPassRegistry());
}

/// Tell the pass manager which passes we depend on and what information we
/// preserve.
void LiveDebugValues::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  MachineFunctionPass::getAnalysisUsage(AU);
}

//===----------------------------------------------------------------------===//
//            Debug Range Extension Implementation
//===----------------------------------------------------------------------===//

#ifndef NDEBUG
// Something to restore in the future.
//void LiveDebugValues::printVarLocInMBB(..)
#endif

SpillLoc
LiveDebugValues::extractSpillBaseRegAndOffset(const MachineInstr &MI) {
  assert(MI.hasOneMemOperand() &&
         "Spill instruction does not have exactly one memory operand?");
  auto MMOI = MI.memoperands_begin();
  const PseudoSourceValue *PVal = (*MMOI)->getPseudoValue();
  assert(PVal->kind() == PseudoSourceValue::FixedStack &&
         "Inconsistent memory operand in spill instruction");
  int FI = cast<FixedStackPseudoSourceValue>(PVal)->getFrameIndex();
  const MachineBasicBlock *MBB = MI.getParent();
  unsigned Reg;
  int Offset = TFI->getFrameIndexReference(*MBB->getParent(), FI, Reg);
  return {Reg, Offset};
}

/// End all previous ranges related to @MI and start a new range from @MI
/// if it is a DBG_VALUE instr.
void LiveDebugValues::transferDebugValue(const MachineInstr &MI) {
  if (!MI.isDebugValue())
    return;
  const DILocalVariable *Var = MI.getDebugVariable();
  const DIExpression *Expr = MI.getDebugExpression();
  const DILocation *DebugLoc = MI.getDebugLoc();
  const DILocation *InlinedAt = DebugLoc->getInlinedAt();
  assert(Var->isValidLocationForIntrinsic(DebugLoc) &&
         "Expected inlined-at fields to agree");

  DebugVariable V(Var, Expr, InlinedAt);

  if (vtracker) {
    const MachineOperand &MO = MI.getOperand(0);
    if (MO.isReg()) {
      // Should read LocNo==0 on $noreg.
      auto ID = tracker->readReg(MO.getReg());
      vtracker->defVar(MI, ID);
    } else if (MI.getOperand(0).isImm() || MI.getOperand(0).isFPImm() ||
               MI.getOperand(0).isCImm()) {
      vtracker->defVar(MI, MI.getOperand(0));
    }
  }

  if (ttracker)
    ttracker->redefVar(MI);
}

/// A definition of a register may mark the end of a range.
void LiveDebugValues::transferRegisterDef(
    MachineInstr &MI) {

  // Meta Instructions do not affect the debug liveness of any register they
  // define.
  if (MI.isMetaInstruction())
    return;

  MachineFunction *MF = MI.getMF();
  const TargetLowering *TLI = MF->getSubtarget().getTargetLowering();
  unsigned SP = TLI->getStackPointerRegisterToSaveRestore();

  // Find the regs killed by MI, and find regmasks of preserved regs.
  // Max out the number of statically allocated elements in `DeadRegs`, as this
  // prevents fallback to std::set::count() operations.
  SmallSet<uint32_t, 32> DeadRegs;
  SmallVector<const uint32_t *, 4> RegMasks;
  for (const MachineOperand &MO : MI.operands()) {
    // Determine whether the operand is a register def.
    if (MO.isReg() && MO.isDef() && MO.getReg() &&
        Register::isPhysicalRegister(MO.getReg()) &&
        !(MI.isCall() && MO.getReg() == SP)) {
      // Remove ranges of all aliased registers.
      for (MCRegAliasIterator RAI(MO.getReg(), TRI, true); RAI.isValid(); ++RAI)
        // FIXME: Can we break out of this loop early if no insertion occurs?
        DeadRegs.insert(*RAI);
    } else if (MO.isRegMask()) {
      RegMasks.push_back(MO.getRegMask());
    }
  }

  // Erase VarLocs which reside in one of the dead registers. For performance
  // reasons, it's critical to not iterate over the full set of open VarLocs.
  // Iterate over the set of dying/used regs instead.
  VarLocSet KillSet(Alloc);
  for (uint32_t DeadReg : DeadRegs) {
    tracker->defReg(DeadReg, cur_bb, cur_inst);
    if (ttracker)
      ttracker->clobberMloc(DeadReg, MI.getIterator());
  }

  auto AnyRegMaskKillsReg = [RegMasks](Register Reg) -> bool {
    return any_of(RegMasks, [Reg](const uint32_t *RegMask) {
      return MachineOperand::clobbersPhysReg(RegMask, Reg);
    });
  };

  // All registers not in the mask may need re-deffing...
  for (unsigned Reg = 1; Reg < TRI->getNumRegs(); ++Reg) {
    if (Reg != SP && AnyRegMaskKillsReg(Reg)) {
      tracker->defReg(Reg, cur_bb, cur_inst);
      if (ttracker)
        ttracker->clobberMloc(Reg, MI.getIterator());
    }
  }
}

bool LiveDebugValues::isSpillInstruction(const MachineInstr &MI,
                                         MachineFunction *MF) {
  // TODO: Handle multiple stores folded into one.
  if (!MI.hasOneMemOperand())
    return false;

  if (!MI.getSpillSize(TII) && !MI.getFoldedSpillSize(TII))
    return false; // This is not a spill instruction, since no valid size was
                  // returned from either function.

  return true;
}

bool LiveDebugValues::isLocationSpill(const MachineInstr &MI,
                                      MachineFunction *MF, unsigned &Reg) {
  if (!isSpillInstruction(MI, MF))
    return false;

  auto isKilledReg = [&](const MachineOperand MO, unsigned &Reg) {
    if (!MO.isReg() || !MO.isUse()) {
      Reg = 0;
      return false;
    }
    Reg = MO.getReg();
    return MO.isKill();
  };

  for (const MachineOperand &MO : MI.operands()) {
    // In a spill instruction generated by the InlineSpiller the spilled
    // register has its kill flag set.
    if (isKilledReg(MO, Reg))
      return true;
    if (Reg != 0) {
      // Check whether next instruction kills the spilled register.
      // FIXME: Current solution does not cover search for killed register in
      // bundles and instructions further down the chain.
      auto NextI = std::next(MI.getIterator());
      // Skip next instruction that points to basic block end iterator.
      if (MI.getParent()->end() == NextI)
        continue;
      unsigned RegNext;
      for (const MachineOperand &MONext : NextI->operands()) {
        // Return true if we came across the register from the
        // previous spill instruction that is killed in NextI.
        if (isKilledReg(MONext, RegNext) && RegNext == Reg)
          return true;
      }
    }
  }
  // Return false if we didn't find spilled register.
  return false;
}

Optional<SpillLoc>
LiveDebugValues::isRestoreInstruction(const MachineInstr &MI,
                                      MachineFunction *MF, unsigned &Reg) {
  if (!MI.hasOneMemOperand())
    return None;

  // FIXME: Handle folded restore instructions with more than one memory
  // operand.
  if (MI.getRestoreSize(TII)) {
    Reg = MI.getOperand(0).getReg();
    return extractSpillBaseRegAndOffset(MI);
  }
  return None;
}

/// A spilled register may indicate that we have to end the current range of
/// a variable and create a new one for the spill location.
/// A restored register may indicate the reverse situation.
/// Any change in location will be recorded in \p OpenRanges, and \p Transfers
/// if it is non-null.
void LiveDebugValues::transferSpillOrRestoreInst(MachineInstr &MI) {
  MachineFunction *MF = MI.getMF();
  unsigned Reg;
  Optional<SpillLoc> Loc;

  LLVM_DEBUG(dbgs() << "Examining instruction: "; MI.dump(););

  // First, if there are any DBG_VALUEs pointing at a spill slot that is
  // written to, then close the variable location. The value in memory
  // will have changed.
  VarLocSet KillSet(Alloc);
  if (isSpillInstruction(MI, MF)) {
    Loc = extractSpillBaseRegAndOffset(MI);

    if (ttracker) {
      unsigned mloc = tracker->getSpillMLoc(*Loc);
      if (mloc != 0)
        ttracker->clobberMloc(mloc, MI.getIterator());
    }
  }

  // Try to recognise spill and restore instructions that may create a new
  // variable location.
  if (isLocationSpill(MI, MF, Reg)) {
    Loc = extractSpillBaseRegAndOffset(MI);
    auto id = tracker->readReg(Reg);
    tracker->setSpill(*Loc, id);
    assert(tracker->getSpillMLoc(*Loc) != 0);
    if (ttracker)
      ttracker->transferMlocs(Reg, tracker->getSpillMLoc(*Loc), MI.getIterator());
    tracker->lolwipe(Reg);

  } else {
    if (!(Loc = isRestoreInstruction(MI, MF, Reg)))
      return;
    auto id = tracker->readSpill(*Loc);
    if (id.LocNo != 0) {
      tracker->setReg(Reg, id);
      assert(tracker->getSpillMLoc(*Loc) != 0);
      if (ttracker)
        ttracker->transferMlocs(tracker->getSpillMLoc(*Loc), Reg, MI.getIterator());
      tracker->lolwipe(*Loc);
    }
  }
}

/// If \p MI is a register copy instruction, that copies a previously tracked
/// value from one register to another register that is callee saved, we
/// create new DBG_VALUE instruction  described with copy destination register.
void LiveDebugValues::transferRegisterCopy(MachineInstr &MI) {
  auto DestSrc = TII->isCopyInstr(MI);
  if (!DestSrc)
    return;

  const MachineOperand *DestRegOp = DestSrc->Destination;
  const MachineOperand *SrcRegOp = DestSrc->Source;

  if (!DestRegOp->isDef())
    return;

  auto isCalleeSavedReg = [&](unsigned Reg) {
    for (MCRegAliasIterator RAI(Reg, TRI, true); RAI.isValid(); ++RAI)
      if (CalleeSavedRegs.test(*RAI))
        return true;
    return false;
  };

  Register SrcReg = SrcRegOp->getReg();
  Register DestReg = DestRegOp->getReg();

  // We want to recognize instructions where destination register is callee
  // saved register. If register that could be clobbered by the call is
  // included, there would be a great chance that it is going to be clobbered
  // soon. It is more likely that previous register location, which is callee
  // saved, is going to stay unclobbered longer, even if it is killed.
  if (!isCalleeSavedReg(DestReg))
    return;

  if (!SrcRegOp->isKill())
    return;

      auto id = tracker->readReg(SrcReg);
      tracker->setReg(DestReg, id);
      if (ttracker)
        ttracker->transferMlocs(SrcReg, DestReg, MI.getIterator());
      tracker->lolwipe(SrcReg);
      return;
}

/// Accumulate a mapping between each DILocalVariable fragment and other
/// fragments of that DILocalVariable which overlap. This reduces work during
/// the data-flow stage from "Find any overlapping fragments" to "Check if the
/// known-to-overlap fragments are present".
/// \param MI A previously unprocessed DEBUG_VALUE instruction to analyze for
///           fragment usage.
/// \param SeenFragments Map from DILocalVariable to all fragments of that
///           Variable which are known to exist.
/// \param OverlappingFragments The overlap map being constructed, from one
///           Var/Fragment pair to a vector of fragments known to overlap.
void LiveDebugValues::accumulateFragmentMap(MachineInstr &MI,
                                            VarToFragments &SeenFragments,
                                            OverlapMap &OverlappingFragments) {
  DebugVariable MIVar(MI.getDebugVariable(), MI.getDebugExpression(),
                      MI.getDebugLoc()->getInlinedAt());
  FragmentInfo ThisFragment = MIVar.getFragmentOrDefault();

  // If this is the first sighting of this variable, then we are guaranteed
  // there are currently no overlapping fragments either. Initialize the set
  // of seen fragments, record no overlaps for the current one, and return.
  auto SeenIt = SeenFragments.find(MIVar.getVariable());
  if (SeenIt == SeenFragments.end()) {
    SmallSet<FragmentInfo, 4> OneFragment;
    OneFragment.insert(ThisFragment);
    SeenFragments.insert({MIVar.getVariable(), OneFragment});

    OverlappingFragments.insert({{MIVar.getVariable(), ThisFragment}, {}});
    return;
  }

  // If this particular Variable/Fragment pair already exists in the overlap
  // map, it has already been accounted for.
  auto IsInOLapMap =
      OverlappingFragments.insert({{MIVar.getVariable(), ThisFragment}, {}});
  if (!IsInOLapMap.second)
    return;

  auto &ThisFragmentsOverlaps = IsInOLapMap.first->second;
  auto &AllSeenFragments = SeenIt->second;

  // Otherwise, examine all other seen fragments for this variable, with "this"
  // fragment being a previously unseen fragment. Record any pair of
  // overlapping fragments.
  for (auto &ASeenFragment : AllSeenFragments) {
    // Does this previously seen fragment overlap?
    if (DIExpression::fragmentsOverlap(ThisFragment, ASeenFragment)) {
      // Yes: Mark the current fragment as being overlapped.
      ThisFragmentsOverlaps.push_back(ASeenFragment);
      // Mark the previously seen fragment as being overlapped by the current
      // one.
      auto ASeenFragmentsOverlaps =
          OverlappingFragments.find({MIVar.getVariable(), ASeenFragment});
      assert(ASeenFragmentsOverlaps != OverlappingFragments.end() &&
             "Previously seen var fragment has no vector of overlaps");
      ASeenFragmentsOverlaps->second.push_back(ThisFragment);
    }
  }

  AllSeenFragments.insert(ThisFragment);
}

/// This routine creates OpenRanges.
void LiveDebugValues::process(MachineInstr &MI) {
  transferDebugValue(MI);
  transferRegisterDef(MI);
  transferRegisterCopy(MI);
  transferSpillOrRestoreInst(MI);
}

/// This routine joins the analysis results of all incoming edges in @MBB by
/// inserting a new DBG_VALUE instruction at the start of the @MBB - if the same
/// source variable in all the predecessors of @MBB reside in the same location.
bool LiveDebugValues::join(
    MachineBasicBlock &MBB, VarLocInMBB &OutLocs, VarLocInMBB &InLocs,
    SmallPtrSet<const MachineBasicBlock *, 16> &Visited,
    SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks) {
  LLVM_DEBUG(dbgs() << "join MBB: " << MBB.getNumber() << "\n");
  bool Changed = false;

  VarLocSet InLocsT(Alloc); // Temporary incoming locations.

  // For all predecessors of this MBB, find the set of VarLocs that
  // can be joined.
  int NumVisited = 0;
  for (auto p : MBB.predecessors()) {
    // Ignore backedges if we have not visited the predecessor yet. As the
    // predecessor hasn't yet had locations propagated into it, most locations
    // will not yet be valid, so treat them as all being uninitialized and
    // potentially valid. If a location guessed to be correct here is
    // invalidated later, we will remove it when we revisit this block.
    if (!Visited.count(p)) {
      LLVM_DEBUG(dbgs() << "  ignoring unvisited pred MBB: " << p->getNumber()
                        << "\n");
      continue;
    }
    auto OL = OutLocs.find(p);
    // Join is null in case of empty OutLocs from any of the pred.
    if (OL == OutLocs.end())
      return false;

    // Just copy over the Out locs to incoming locs for the first visited
    // predecessor, and for all other predecessors join the Out locs.
    if (!NumVisited)
      InLocsT = OL->second;
    else
      InLocsT &= OL->second;

#if 0
 // killed off varlocids..
    LLVM_DEBUG({
      if (!InLocsT.empty() && !mlocs) {
        for (uint64_t ID : InLocsT)
          dbgs() << "  gathered candidate incoming var: "
                 << VarLocIDs[LocIndex::fromRawInteger(ID)]
                        .Var.getVariable()
                        ->getName()
                 << "\n";
      }
    });
#endif

    NumVisited++;
  }

  // As we are processing blocks in reverse post-order we
  // should have processed at least one predecessor, unless it
  // is the entry block which has no predecessor.
  assert((NumVisited || MBB.pred_empty()) &&
         "Should have processed at least one predecessor");

  VarLocSet &ILS = getVarLocsInMBB(&MBB, InLocs);

  Changed = ILS != InLocsT;
  ILS = InLocsT;
  // Uhhhhhh, reimplement NumInserted and NumRemoved pls.
  return Changed;
}

bool LiveDebugValues::vloc_join(
  const MachineBasicBlock &MBB, VarLocInMBB &VLOCOutLocs,
   VarLocInMBB &VLOCInLocs, lolnumberingt &lolnumbering,
   SmallPtrSet<const MachineBasicBlock *, 16> &VLOCVisited,
   SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks,
   VarLocInMBB &VLOCScopeMasks,
   unsigned cur_bb) {
  LLVM_DEBUG(dbgs() << "join MBB: " << MBB.getNumber() << "\n");
  bool Changed = false;

  VarLocSet InLocsT(Alloc); // Temporary incoming locations.
  VarLocSet toBecomePHIs(Alloc);

  // For all predecessors of this MBB, find the set of VarLocs that
  // can be joined.
  int NumVisited = 0;
  for (auto p : MBB.predecessors()) {
    // Ignore backedges if we have not visited the predecessor yet. As the
    // predecessor hasn't yet had locations propagated into it, most locations
    // will not yet be valid, so treat them as all being uninitialized and
    // potentially valid. If a location guessed to be correct here is
    // invalidated later, we will remove it when we revisit this block.
    if (!VLOCVisited.count(p)) {
      LLVM_DEBUG(dbgs() << "  ignoring unvisited pred MBB: " << p->getNumber()
                        << "\n");
      continue;
    }
    auto OL = VLOCOutLocs.find(p);
    // Join is null in case of empty OutLocs from any of the pred.
    if (OL == VLOCOutLocs.end())
      return false;

    // Just copy over the Out locs to incoming locs for the first visited
    // predecessor, and for all other predecessors join the Out locs.
    if (!NumVisited) {
      InLocsT = OL->second;
      toBecomePHIs = OL->second;
    } else {
      InLocsT &= OL->second;
      toBecomePHIs |= OL->second;
    }

    // xXX jmorse deleted debug statement

    NumVisited++;
  }

  // Erm. We need to produce PHI nodes for vlocs that aren't in the same
  // location. Pick out variables that aren't in InLocsT.
  toBecomePHIs.intersectWithComplement(InLocsT);
  // set for nondeterminism
  MapVector<DebugVariable, unsigned> tophi;
  for (auto ID : toBecomePHIs) {
    tophi.insert(std::make_pair(lolnumbering[ID].first, 0));
  }

  for (auto Var : tophi) {
    ValueRec NewVR = {{0, 0, 0}, None, {nullptr, false}, cur_bb, ValueRec::PHI};
    auto NewPHI = std::make_pair(Var.first, NewVR);
    unsigned PreID = lolnumbering.idFor(NewPHI);
    unsigned ID = lolnumbering.insert(NewPHI);
    if (PreID == 0)
      UpdateVlocMask(lolnumbering, ID, VLOCScopeMasks, ArtificialBlocks);
    InLocsT.set(ID);
  }

  // Filter out DBG_VALUES that are out of scope.
  auto &Mask = getVarLocsInMBB(&MBB, VLOCScopeMasks);
  InLocsT &= Mask;

  // As we are processing blocks in reverse post-order we
  // should have processed at least one predecessor, unless it
  // is the entry block which has no predecessor.
  assert((NumVisited || MBB.pred_empty()) &&
         "Should have processed at least one predecessor");

  VarLocSet &ILS = getVarLocsInMBB(&MBB, VLOCInLocs);

  Changed = ILS != InLocsT;
  ILS = InLocsT;
  // Uhhhhhh, reimplement NumInserted and NumRemoved pls.
  return Changed;
}

bool LiveDebugValues::vloc_transfer(VarLocSet &ilocs, VarLocSet &transfer, VarLocSet &olocs, lolnumberingt &lolnumbering) {
  // Eeeerrmmmm...
  // quick implementation then, anything in transfer overrides ilocs. Filter
  // out anything that's been deleted in the meantime.

  VarLocSet new_olocs(Alloc);
  DenseMap<DebugVariable, ValueRec> set;
  for (auto ID : ilocs) {
    set.insert(lolnumbering[ID]);
  }

  for (auto ID : transfer) {
    set.erase(lolnumbering[ID].first);
    set.insert(lolnumbering[ID]);
  }

  // XXX erm, unset any empty locations.
  // XXX XXX are there any now that everything starts with mloc phis?
  for (auto &P : set) {
    if (P.second.Kind == ValueRec::Def && P.second.ID.LocNo == 0)
      continue;
    unsigned id = lolnumbering.idFor(P);
    assert(id != 0);
    new_olocs.set(id);
  }

  bool Changed = new_olocs != olocs;
  olocs = new_olocs;
  return Changed;
}

void LiveDebugValues::resolveMPHIs(mphiremapt &mphiremap, MachineBasicBlock &MBB, VarLocSet &InLocs, VarLocInMBB &MLOCOutLocs, unsigned cur_bb)
{

  // Take a look at any inlocs here that are PHIs; are they really PHIS?
  tracker->reset();
  tracker->loadFromVarLocSet(InLocs, cur_bb);
  std::vector<ValueIDNum> toexamine;
  for (unsigned Idx = 1; Idx < tracker->getNumLocs(); ++Idx) {
    VarLocPos Pos = tracker->getVarLocPos(Idx);
    if (Pos.ID.BlockNo == cur_bb && Pos.ID.InstNo == 0)
      toexamine.push_back(Pos.ID);
  }

  std::vector<ValueIDNum> seen_values = toexamine;
  // Look over predecessors...
  for (auto &p : MBB.predecessors()) {
    tracker->reset();
    tracker->loadFromVarLocSet(getVarLocsInMBB(p, MLOCOutLocs), p->getNumber());
    for (unsigned Idx = 0; Idx < toexamine.size(); ++Idx) {
      VarLocPos outpos = tracker->getVarLocPos(toexamine[Idx].LocNo);
      if (outpos.ID != seen_values[Idx] && outpos.ID != toexamine[Idx] &&
          seen_values[Idx] != toexamine[Idx])
        seen_values[Idx].LocNo = 0;
      else if (outpos.ID != toexamine[Idx])
        seen_values[Idx] = outpos.ID;
    }
  }

  // Any seen values that aren't nulled out means that the only incoming
  // values were the mphi value or one other value. We can remap to that other
  // value.
  for (unsigned Idx = 0; Idx < toexamine.size(); ++Idx) {
    if (seen_values[Idx].LocNo == 0)
      continue;
    //mphiremap.insert(std::make_pair(toexamine[Idx], seen_values[Idx]));
    mphiremap.insert(std::make_pair(std::make_pair(&MBB, seen_values[Idx]), toexamine[Idx]));
  }
}

void LiveDebugValues::resolveVPHIs(vphitomphit &vphitomphi, const mphiremapt &mphiremap, lolnumberingt &lolnumbering, MachineBasicBlock &MBB, VarLocSet &InLocs, VarLocInMBB &VLOCOutLocs, VarLocInMBB &MLOCOutLocs, unsigned cur_bb) {
  // Take a look at each PHI in the inlocs.

  unsigned NumLocs = tracker->getNumLocs();
  unsigned NumPreds = MBB.pred_size();
  if (NumPreds == 0)
    return;

  // Fetch all the outgoing locations of all predecessors.
  std::vector<SmallVector<ValueIDNum, 4>> PredOutMLocs;
  std::vector<SmallVector<ValueRec, 4>> PredOutVLocs;
  DenseMap<DebugVariable, unsigned> PredOutVariables;

  PredOutMLocs.resize(NumLocs);

  for (auto p : MBB.predecessors()) {
    const VarLocSet &mlocs = getVarLocsInMBB(p, MLOCOutLocs);
    const VarLocSet &vlocs = getVarLocsInMBB(p, VLOCOutLocs);

    for (unsigned ID : vlocs) {
      const auto &loc = lolnumbering[ID];
      if (loc.second.Kind != ValueRec::Def) {
        // incoming other phis and constants can't be merged.
        if (PredOutVariables.count(loc.first) == 0)
          PredOutVariables[loc.first] = UINT_MAX;
      }

      if (PredOutVariables.count(loc.first) == 0) {
        unsigned &Num = PredOutVariables[loc.first];
        if (loc.second.Kind != ValueRec::Def) {
          Num = UINT_MAX;
          continue;
        }
        Num = PredOutVLocs.size();
        PredOutVLocs.push_back(SmallVector<ValueRec, 4>());
      }

      unsigned Idx = PredOutVariables[loc.first];
      if (Idx == UINT_MAX)
        continue;

      auto &VOutVec = PredOutVLocs[Idx];
      VOutVec.push_back(loc.second);
    }

    // Not guaranteed to fill all locs? Is guaranteed to be in pred order
    // though
    for (uint64_t mloc : mlocs) {
      VarLocPos n = VarLocPos::fromU64(mloc);
      PredOutMLocs[n.CurrentLoc].push_back(n.ID);
    }
  }

  // Index the first predecessor,
  const VarLocSet &mlocs = getVarLocsInMBB(*MBB.pred_begin(), MLOCOutLocs);
  DenseMap<ValueIDNum, unsigned> MBB1Idx;
  for (uint64_t mloc : mlocs) {
    VarLocPos n = VarLocPos::fromU64(mloc);
    MBB1Idx[n.ID] = n.CurrentLoc;
  }

  std::vector<std::pair<unsigned, unsigned>> toreplace;
  for (unsigned ID : InLocs) {
    auto &Pair = lolnumbering[ID];

    if (Pair.second.Kind != ValueRec::PHI)
      continue;

    // We should have an index for this right?
    if (PredOutVariables.count(Pair.first) == 0 ||
        PredOutVariables[Pair.first] == UINT_MAX)
      continue;

    auto &VOutVec = PredOutVLocs[PredOutVariables[Pair.first]];
    if (VOutVec.size() != NumPreds)
      continue;

    // Do they all have the same meta info?
    bool thesame = llvm::all_of(VOutVec, [&](const ValueRec &R) {
      return R.meta == VOutVec[0].meta;
    });
    if (!thesame)
      continue;

    // Where do we start looking?
    if (MBB1Idx.count(VOutVec[0].ID) == 0)
      continue;
    uint64_t mloc = MBB1Idx[VOutVec[0].ID];

    // Alright, do all those mlocs agree?
    auto &MOutVec = PredOutMLocs[mloc];
    if (MOutVec.size() != NumPreds)
      continue;

    bool match = true;
    for (unsigned Idx = 0; Idx < NumPreds; ++Idx) {
      if (MOutVec[Idx] != VOutVec[Idx].ID)
        match = false;
    }
    if (!match)
      continue;

    // Success.

    ValueIDNum newid = {cur_bb, 0, mloc};
    ValueRec r = {newid, None, VOutVec[0].meta, 0, ValueRec::Def};
    unsigned newnum = lolnumbering.insert({Pair.first, r});
    // No scope masking of this lolnumbering element because we're no longer
    // joining.
    // Record pair to mangle later.
    toreplace.push_back(std::make_pair(ID, newnum));
    assert(vphitomphi.find(ID) == vphitomphi.end());
    vphitomphi[ID] = newnum;
  }

  for (auto &P : toreplace) {
    InLocs.reset(P.first);
    InLocs.set(P.second);
  }
}

void LiveDebugValues::UpdateVlocMask(lolnumberingt &lolnumbering, unsigned ID,
                                     VarLocInMBB &VLOCScopeMasks,
               SmallPtrSetImpl<const MachineBasicBlock *> &ArtificialBlocks)
{
  // Maintain scope masking. Maybe cache in future?
  SmallPtrSet<const MachineBasicBlock *, 32> LBlocks;
  const DebugVariable &Var = lolnumbering[ID].first;
  DebugLoc DL = DebugLoc::get(0, 0, Var.getVariable()->getScope(), Var.getInlinedAt());
  LS.getMachineBasicBlocks(DL, LBlocks);
  LBlocks.insert(ArtificialBlocks.begin(), ArtificialBlocks.end());
  for (auto *MBB : LBlocks) {
    VarLocSet &Mask = getVarLocsInMBB(MBB, VLOCScopeMasks);
    Mask.set(ID);
  }
}

/// Calculate the liveness information for the given machine function and
/// extend ranges across basic blocks.
bool LiveDebugValues::ExtendRanges(MachineFunction &MF) {
  LLVM_DEBUG(dbgs() << "\nDebug Range Extension\n");

  bool Changed = false;
  bool OLChanged = false;
  bool MBBJoined = false;

  OverlapMap OverlapFragments; // Map of overlapping variable fragments.

  VarLocInMBB MLOCOutLocs, MLOCInLocs;

  VarToFragments SeenFragments;

  // Blocks which are artificial, i.e. blocks which exclusively contain
  // instructions without locations, or with line 0 locations.
  SmallPtrSet<const MachineBasicBlock *, 16> ArtificialBlocks;

  DenseMap<unsigned int, MachineBasicBlock *> OrderToBB;
  DenseMap<MachineBasicBlock *, unsigned int> BBToOrder;
  std::priority_queue<unsigned int, std::vector<unsigned int>,
                      std::greater<unsigned int>>
      Worklist;
  std::priority_queue<unsigned int, std::vector<unsigned int>,
                      std::greater<unsigned int>>
      Pending;

  // Initialize per-block structures and scan for fragment overlaps.
  for (auto &MBB : MF) {
    for (auto &MI : MBB) {
      if (MI.isDebugValue())
        accumulateFragmentMap(MI, SeenFragments, OverlapFragments);
    }
  }

  auto hasNonArtificialLocation = [](const MachineInstr &MI) -> bool {
    if (const DebugLoc &DL = MI.getDebugLoc())
      return DL.getLine() != 0;
    return false;
  };
  for (auto &MBB : MF)
    if (none_of(MBB.instrs(), hasNonArtificialLocation))
      ArtificialBlocks.insert(&MBB);

  ReversePostOrderTraversal<MachineFunction *> RPOT(&MF);
  unsigned int RPONumber = 0;
  for (auto RI = RPOT.begin(), RE = RPOT.end(); RI != RE; ++RI) {
    OrderToBB[RPONumber] = *RI;
    BBToOrder[*RI] = RPONumber;
    Worklist.push(RPONumber);
    ++RPONumber;
  }

  // This is a standard "union of predecessor outs" dataflow problem.
  // To solve it, we perform join() and process() using the two worklist method
  // until the ranges converge.
  // Ranges have converged when both worklists are empty.
  SmallPtrSet<const MachineBasicBlock *, 16> Visited;
  while (!Worklist.empty() || !Pending.empty()) {
    // We track what is on the pending worklist to avoid inserting the same
    // thing twice.  We could avoid this with a custom priority queue, but this
    // is probably not worth it.
    SmallPtrSet<MachineBasicBlock *, 16> OnPending;
    LLVM_DEBUG(dbgs() << "Processing Worklist\n");
    while (!Worklist.empty()) {
      MachineBasicBlock *MBB = OrderToBB[Worklist.top()];
      cur_bb = MBB->getNumber();
      cur_inst = 1;
      Worklist.pop();

     // XXX jmorse
     // Also XXX, do we go around these loops too many times?
      MBBJoined = join(*MBB, MLOCOutLocs, MLOCInLocs,  Visited, ArtificialBlocks);
      MBBJoined |= Visited.insert(MBB).second;

      if (MBBJoined) {
        MBBJoined = false;
        Changed = true;
        // Now that we have started to extend ranges across BBs we need to
        // examine spill, copy and restore instructions to see whether they
        // operate with registers that correspond to user variables.
        // First load any pending inlocs.
        tracker->loadFromVarLocSet(getVarLocsInMBB(MBB, MLOCInLocs), cur_bb);
        for (auto &MI : *MBB) {
          process(MI);
          ++cur_inst;
        }

#if 0
        LLVM_DEBUG(printVarLocInMBB(MF, OutLocs, VarLocIDs,
                                    "OutLocs after propagating", dbgs()));
        LLVM_DEBUG(printVarLocInMBB(MF, InLocs, VarLocIDs,
                                    "InLocs after propagating", dbgs()));
#endif

        auto tmpset = tracker->makeVarLocSet();
        auto &replaceset = getVarLocsInMBB(MBB, MLOCOutLocs);
        OLChanged |= tmpset != replaceset;
        replaceset = tmpset;
        tracker->reset();

        if (OLChanged) {
          OLChanged = false;
          for (auto s : MBB->successors())
            if (OnPending.insert(s).second) {
              Pending.push(BBToOrder[s]);
            }
        }
      }
    }
    Worklist.swap(Pending);
    // At this point, pending must be empty, since it was just the empty
    // worklist
    assert(Pending.empty() && "Pending should be empty");
  }

  // vlocs and mlocs: go back over each block, this time tracking the vlocs
  // and building a transfer function between each block. 
  // XXX mv for nondeterminism
  MapVector<unsigned, VLocTracker *> vlocs;
  for (unsigned I = 0; I < MF.size(); ++I)
    vlocs[I] = new VLocTracker();

  // Accumulate things into the vloc tracker.
  for (auto RI = RPOT.begin(), RE = RPOT.end(); RI != RE; ++RI) {
    unsigned Idx = BBToOrder[*RI];
    cur_bb = (*RI)->getNumber();
    Worklist.push(Idx);
    auto *MBB = *RI;
    vtracker = vlocs[Idx];
    tracker->loadFromVarLocSet(getVarLocsInMBB(MBB, MLOCInLocs), cur_bb);
    cur_inst = 1;
    for (auto &MI : *MBB) { // XXX I think the empty open ranges does nufink
      process(MI);
      ++cur_inst;
    }
    tracker->reset();
  }

  // OK, we have some transfer functions. Number everything; do data flow.
  UniqueVector<std::pair<DebugVariable, ValueRec>> lolnumbering;
  VarLocInMBB VLOCOutLocs, VLOCInLocs, VLOCTransfer, VLOCScopeMask;

  for (auto &It : vlocs) {
    const MachineBasicBlock *MBB = OrderToBB[It.first];
    VarLocSet &transfer = getVarLocsInMBB(MBB, VLOCTransfer);
    for (auto &idx : It.second->Vars) {
      const DebugVariable &Var = idx.first;
      const ValueRec &Rec = idx.second;
      unsigned num = lolnumbering.insert(std::make_pair(Var, Rec));
      transfer.set(num);

    }
  }

  // Maintain scope masking. Maybe cache in future?
  // Don't use UpdateVlocMask, to avoid repeated reallocation of LBlocks
  // if there are a lot of them.
  SmallPtrSet<const MachineBasicBlock *, 32> LBlocks;
  // NB: UniqueVector is one-based counting.
  for (unsigned ID = 1; ID <= lolnumbering.size(); ++ID) {
    const DebugVariable &Var = lolnumbering[ID].first;
    DebugLoc DL = DebugLoc::get(0, 0, Var.getVariable()->getScope(), Var.getInlinedAt());
    LS.getMachineBasicBlocks(DL, LBlocks);
    LBlocks.insert(ArtificialBlocks.begin(), ArtificialBlocks.end());
    for (auto *MBB : LBlocks) {
      VarLocSet &Mask = getVarLocsInMBB(MBB, VLOCScopeMask);
      Mask.set(ID);
    }
  }

  SmallPtrSet<const MachineBasicBlock *, 16> VLOCVisited;
  while (!Worklist.empty() || !Pending.empty()) {
    // We track what is on the pending worklist to avoid inserting the same
    // thing twice.  We could avoid this with a custom priority queue, but this
    // is probably not worth it.
    SmallPtrSet<MachineBasicBlock *, 16> OnPending;
    LLVM_DEBUG(dbgs() << "Processing Worklist\n");
    while (!Worklist.empty()) {
      MachineBasicBlock *MBB = OrderToBB[Worklist.top()];
      cur_bb = MBB->getNumber();
      Worklist.pop();

      MBBJoined = vloc_join(*MBB, VLOCOutLocs, VLOCInLocs, lolnumbering,
                       VLOCVisited,
                       ArtificialBlocks, VLOCScopeMask, cur_bb);
      MBBJoined |= VLOCVisited.insert(MBB).second;

      if (MBBJoined) {
        MBBJoined = false;
        Changed = true;

        auto &ilocs = getVarLocsInMBB(MBB, VLOCInLocs);
        auto &transfers = getVarLocsInMBB(MBB, VLOCTransfer);
        auto &olocs = getVarLocsInMBB(MBB, VLOCOutLocs);
        OLChanged = vloc_transfer(ilocs, transfers, olocs, lolnumbering);

        if (OLChanged) {
          OLChanged = false;
          for (auto s : MBB->successors())
            if (OnPending.insert(s).second) {
              Pending.push(BBToOrder[s]);
            }
        }
      }
    }
    Worklist.swap(Pending);
    // At this point, pending must be empty, since it was just the empty
    // worklist
    assert(Pending.empty() && "Pending should be empty");
  }

  for (auto &It : VLOCInLocs) {
    for (auto lala : It.second) {
      auto &var = lolnumbering[lala];
      assert(var.second.Kind != ValueRec::Def || var.second.ID.LocNo != 0);
    }
  }

  // mloc argument only needs the posish -> spills map and the like.
  ttracker = new TransferTracker(TII, tracker, MF);

  // Reprocess all instructions a final time and record transfers. The live-in
  // locations should not change as we've reached a fixedpoint.
  vphitomphit vphitomphi;
  mphiremapt mphiremap;
  for (MachineBasicBlock &MBB : MF) {
    unsigned bbnum = MBB.getNumber();
    resolveMPHIs(mphiremap, MBB, getVarLocsInMBB(&MBB, MLOCInLocs), MLOCOutLocs, bbnum);
  }

  for (MachineBasicBlock &MBB : MF) {
    unsigned bbnum = MBB.getNumber();
    resolveVPHIs(vphitomphi, mphiremap, lolnumbering, MBB, getVarLocsInMBB(&MBB, VLOCInLocs), VLOCOutLocs, MLOCOutLocs, bbnum);
    ttracker->loadInlocs(MBB, lolnumbering, mphiremap, getVarLocsInMBB(&MBB, MLOCInLocs), getVarLocsInMBB(&MBB, VLOCInLocs), bbnum);
    tracker->reset();
    tracker->loadFromVarLocSet(getVarLocsInMBB(&MBB, MLOCInLocs), bbnum);
    tracker->lolremap(&MBB, mphiremap);

    for (auto &MI : MBB)
      process(MI);
  }

  for (auto &P : ttracker->Transfers) {
    MachineBasicBlock &MBB = *P.MBB;
    for (auto *MI : P.insts) {
      MBB.insert(P.pos, MI);
    }
  }

#if 0
  LLVM_DEBUG(printVarLocInMBB(MF, OutLocs, VarLocIDs, "Final OutLocs", dbgs()));
  LLVM_DEBUG(printVarLocInMBB(MF, InLocs, VarLocIDs, "Final InLocs", dbgs()));
#endif

  return Changed;
}

bool LiveDebugValues::runOnMachineFunction(MachineFunction &MF) {
  if (!MF.getFunction().getSubprogram())
    // LiveDebugValues will already have removed all DBG_VALUEs.
    return false;

  // Skip functions from NoDebug compilation units.
  if (MF.getFunction().getSubprogram()->getUnit()->getEmissionKind() ==
      DICompileUnit::NoDebug)
    return false;

  TRI = MF.getSubtarget().getRegisterInfo();
  TII = MF.getSubtarget().getInstrInfo();
  TFI = MF.getSubtarget().getFrameLowering();
  TFI->getCalleeSaves(MF, CalleeSavedRegs);
  LS.initialize(MF);

  tracker = new MLocTracker(Alloc, TRI->getNumRegs());
  vtracker = nullptr;
  ttracker = nullptr;

  bool Changed = ExtendRanges(MF);
  delete tracker;
  vtracker = nullptr;
  ttracker = nullptr;
  return Changed;
}
