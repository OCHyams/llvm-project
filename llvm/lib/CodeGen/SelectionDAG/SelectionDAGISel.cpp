//===- SelectionDAGISel.cpp - Implement the SelectionDAGISel class --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This implements the SelectionDAGISel class.
//
//===----------------------------------------------------------------------===//

#include "llvm/CodeGen/SelectionDAGISel.h"
#include "ScheduleDAGSDNodes.h"
#include "SelectionDAGBuilder.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/IntervalMap.h"
#include "llvm/ADT/None.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/BranchProbabilityInfo.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/EHPersonalities.h"
#include "llvm/Analysis/LazyBlockFrequencyInfo.h"
#include "llvm/Analysis/LegacyDivergenceAnalysis.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/CodeGenCommonISel.h"
#include "llvm/CodeGen/FastISel.h"
#include "llvm/CodeGen/FunctionLoweringInfo.h"
#include "llvm/CodeGen/GCMetadata.h"
#include "llvm/CodeGen/ISDOpcodes.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineMemOperand.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePassRegistry.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SchedulerRegistry.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/CodeGen/StackProtector.h"
#include "llvm/CodeGen/SwiftErrorValueTracking.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DiagnosticInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicsWebAssembly.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PrintPasses.h"
#include "llvm/IR/Statepoint.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Pass.h"
#include "llvm/Support/BranchProbability.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/MachineValueType.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetIntrinsicInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace llvm;

#define DEBUG_TYPE "isel"

STATISTIC(NumFastIselFailures, "Number of instructions fast isel failed on");
STATISTIC(NumFastIselSuccess, "Number of instructions fast isel selected");
STATISTIC(NumFastIselBlocks, "Number of blocks selected entirely by fast isel");
STATISTIC(NumDAGBlocks, "Number of blocks selected using DAG");
STATISTIC(NumDAGIselRetries,"Number of times dag isel has to try another path");
STATISTIC(NumEntryBlocks, "Number of entry blocks encountered");
STATISTIC(NumFastIselFailLowerArguments,
          "Number of entry blocks where fast isel failed to lower arguments");

static cl::opt<unsigned> CoffeeMaxBB("coffee-max-blocks", cl::init(1500));
static cl::opt<bool> StripStackVars("isel-strip-stack-vars", cl::init(false));
static cl::opt<bool> PrintBeforeCoffee("print-before-coffee", cl::init(false));
static cl::opt<bool> PrintAfterCoffee("print-after-coffee", cl::init(false));
static cl::opt<bool> PrintWrapCoffee("print-wrap-coffee", cl::init(false));
static cl::opt<bool> DebugCoffee("debug-coffee", cl::init(false));
static cl::opt<bool> EnableFragAgg("frag-agg", cl::init(true));
static cl::opt<bool> PrintBeforeFragAgg("print-before-frag-agg",
                                        cl::init(false));
static cl::opt<bool> DebugFragAgg("debug-frag-agg", cl::init(false));

#define COFFEE_DEBUG(x)                                                        \
  do {                                                                         \
    if (DebugCoffee) {                                                         \
      x;                                                                       \
    }                                                                          \
  } while (false)

#define FRAG_DEBUG(x)                                                          \
  do {                                                                         \
    if (DebugFragAgg) {                                                        \
      x;                                                                       \
    }                                                                          \
  } while (false)

static cl::opt<int> EnableFastISelAbort(
    "fast-isel-abort", cl::Hidden,
    cl::desc("Enable abort calls when \"fast\" instruction selection "
             "fails to lower an instruction: 0 disable the abort, 1 will "
             "abort but for args, calls and terminators, 2 will also "
             "abort for argument lowering, and 3 will never fallback "
             "to SelectionDAG."));

static cl::opt<bool> EnableFastISelFallbackReport(
    "fast-isel-report-on-fallback", cl::Hidden,
    cl::desc("Emit a diagnostic when \"fast\" instruction selection "
             "falls back to SelectionDAG."));

static cl::opt<bool>
UseMBPI("use-mbpi",
        cl::desc("use Machine Branch Probability Info"),
        cl::init(true), cl::Hidden);

#ifndef NDEBUG
static cl::opt<std::string>
FilterDAGBasicBlockName("filter-view-dags", cl::Hidden,
                        cl::desc("Only display the basic block whose name "
                                 "matches this for all view-*-dags options"));
static cl::opt<bool>
ViewDAGCombine1("view-dag-combine1-dags", cl::Hidden,
          cl::desc("Pop up a window to show dags before the first "
                   "dag combine pass"));
static cl::opt<bool>
ViewLegalizeTypesDAGs("view-legalize-types-dags", cl::Hidden,
          cl::desc("Pop up a window to show dags before legalize types"));
static cl::opt<bool>
    ViewDAGCombineLT("view-dag-combine-lt-dags", cl::Hidden,
                     cl::desc("Pop up a window to show dags before the post "
                              "legalize types dag combine pass"));
static cl::opt<bool>
    ViewLegalizeDAGs("view-legalize-dags", cl::Hidden,
                     cl::desc("Pop up a window to show dags before legalize"));
static cl::opt<bool>
ViewDAGCombine2("view-dag-combine2-dags", cl::Hidden,
          cl::desc("Pop up a window to show dags before the second "
                   "dag combine pass"));
static cl::opt<bool>
ViewISelDAGs("view-isel-dags", cl::Hidden,
          cl::desc("Pop up a window to show isel dags as they are selected"));
static cl::opt<bool>
ViewSchedDAGs("view-sched-dags", cl::Hidden,
          cl::desc("Pop up a window to show sched dags as they are processed"));
static cl::opt<bool>
ViewSUnitDAGs("view-sunit-dags", cl::Hidden,
      cl::desc("Pop up a window to show SUnit dags after they are processed"));
#else
static const bool ViewDAGCombine1 = false, ViewLegalizeTypesDAGs = false,
                  ViewDAGCombineLT = false, ViewLegalizeDAGs = false,
                  ViewDAGCombine2 = false, ViewISelDAGs = false,
                  ViewSchedDAGs = false, ViewSUnitDAGs = false;
#endif

//===---------------------------------------------------------------------===//
///
/// RegisterScheduler class - Track the registration of instruction schedulers.
///
//===---------------------------------------------------------------------===//
MachinePassRegistry<RegisterScheduler::FunctionPassCtor>
    RegisterScheduler::Registry;

//===---------------------------------------------------------------------===//
///
/// ISHeuristic command line option for instruction schedulers.
///
//===---------------------------------------------------------------------===//
static cl::opt<RegisterScheduler::FunctionPassCtor, false,
               RegisterPassParser<RegisterScheduler>>
ISHeuristic("pre-RA-sched",
            cl::init(&createDefaultScheduler), cl::Hidden,
            cl::desc("Instruction schedulers available (before register"
                     " allocation):"));

static RegisterScheduler
defaultListDAGScheduler("default", "Best scheduler for the target",
                        createDefaultScheduler);

namespace llvm {

  //===--------------------------------------------------------------------===//
  /// This class is used by SelectionDAGISel to temporarily override
  /// the optimization level on a per-function basis.
  class OptLevelChanger {
    SelectionDAGISel &IS;
    CodeGenOpt::Level SavedOptLevel;
    bool SavedFastISel;

  public:
    OptLevelChanger(SelectionDAGISel &ISel,
                    CodeGenOpt::Level NewOptLevel) : IS(ISel) {
      SavedOptLevel = IS.OptLevel;
      SavedFastISel = IS.TM.Options.EnableFastISel;
      if (NewOptLevel == SavedOptLevel)
        return;
      IS.OptLevel = NewOptLevel;
      IS.TM.setOptLevel(NewOptLevel);
      LLVM_DEBUG(dbgs() << "\nChanging optimization level for Function "
                        << IS.MF->getFunction().getName() << "\n");
      LLVM_DEBUG(dbgs() << "\tBefore: -O" << SavedOptLevel << " ; After: -O"
                        << NewOptLevel << "\n");
      if (NewOptLevel == CodeGenOpt::None) {
        IS.TM.setFastISel(IS.TM.getO0WantsFastISel());
        LLVM_DEBUG(
            dbgs() << "\tFastISel is "
                   << (IS.TM.Options.EnableFastISel ? "enabled" : "disabled")
                   << "\n");
      }
    }

    ~OptLevelChanger() {
      if (IS.OptLevel == SavedOptLevel)
        return;
      LLVM_DEBUG(dbgs() << "\nRestoring optimization level for Function "
                        << IS.MF->getFunction().getName() << "\n");
      LLVM_DEBUG(dbgs() << "\tBefore: -O" << IS.OptLevel << " ; After: -O"
                        << SavedOptLevel << "\n");
      IS.OptLevel = SavedOptLevel;
      IS.TM.setOptLevel(SavedOptLevel);
      IS.TM.setFastISel(SavedFastISel);
    }
  };

  //===--------------------------------------------------------------------===//
  /// createDefaultScheduler - This creates an instruction scheduler appropriate
  /// for the target.
  ScheduleDAGSDNodes* createDefaultScheduler(SelectionDAGISel *IS,
                                             CodeGenOpt::Level OptLevel) {
    const TargetLowering *TLI = IS->TLI;
    const TargetSubtargetInfo &ST = IS->MF->getSubtarget();

    // Try first to see if the Target has its own way of selecting a scheduler
    if (auto *SchedulerCtor = ST.getDAGScheduler(OptLevel)) {
      return SchedulerCtor(IS, OptLevel);
    }

    if (OptLevel == CodeGenOpt::None ||
        (ST.enableMachineScheduler() && ST.enableMachineSchedDefaultSched()) ||
        TLI->getSchedulingPreference() == Sched::Source)
      return createSourceListDAGScheduler(IS, OptLevel);
    if (TLI->getSchedulingPreference() == Sched::RegPressure)
      return createBURRListDAGScheduler(IS, OptLevel);
    if (TLI->getSchedulingPreference() == Sched::Hybrid)
      return createHybridListDAGScheduler(IS, OptLevel);
    if (TLI->getSchedulingPreference() == Sched::VLIW)
      return createVLIWDAGScheduler(IS, OptLevel);
    if (TLI->getSchedulingPreference() == Sched::Fast)
      return createFastDAGScheduler(IS, OptLevel);
    if (TLI->getSchedulingPreference() == Sched::Linearize)
      return createDAGLinearizer(IS, OptLevel);
    assert(TLI->getSchedulingPreference() == Sched::ILP &&
           "Unknown sched type!");
    return createILPListDAGScheduler(IS, OptLevel);
  }

} // end namespace llvm

// EmitInstrWithCustomInserter - This method should be implemented by targets
// that mark instructions with the 'usesCustomInserter' flag.  These
// instructions are special in various ways, which require special support to
// insert.  The specified MachineInstr is created but not inserted into any
// basic blocks, and this method is called to expand it into a sequence of
// instructions, potentially also creating new basic blocks and control flow.
// When new basic blocks are inserted and the edges from MBB to its successors
// are modified, the method should insert pairs of <OldSucc, NewSucc> into the
// DenseMap.
MachineBasicBlock *
TargetLowering::EmitInstrWithCustomInserter(MachineInstr &MI,
                                            MachineBasicBlock *MBB) const {
#ifndef NDEBUG
  dbgs() << "If a target marks an instruction with "
          "'usesCustomInserter', it must implement "
          "TargetLowering::EmitInstrWithCustomInserter!\n";
#endif
  llvm_unreachable(nullptr);
}

void TargetLowering::AdjustInstrPostInstrSelection(MachineInstr &MI,
                                                   SDNode *Node) const {
  assert(!MI.hasPostISelHook() &&
         "If a target marks an instruction with 'hasPostISelHook', "
         "it must implement TargetLowering::AdjustInstrPostInstrSelection!");
}

//===----------------------------------------------------------------------===//
// SelectionDAGISel code
//===----------------------------------------------------------------------===//

SelectionDAGISel::SelectionDAGISel(TargetMachine &tm, CodeGenOpt::Level OL)
    : MachineFunctionPass(ID), TM(tm), FuncInfo(new FunctionLoweringInfo()),
      SwiftError(new SwiftErrorValueTracking()),
      CurDAG(new SelectionDAG(tm, OL)),
      SDB(std::make_unique<SelectionDAGBuilder>(*CurDAG, *FuncInfo, *SwiftError,
                                                OL)),
      OptLevel(OL) {
  initializeGCModuleInfoPass(*PassRegistry::getPassRegistry());
  initializeBranchProbabilityInfoWrapperPassPass(
      *PassRegistry::getPassRegistry());
  initializeAAResultsWrapperPassPass(*PassRegistry::getPassRegistry());
  initializeTargetLibraryInfoWrapperPassPass(*PassRegistry::getPassRegistry());
}

SelectionDAGISel::~SelectionDAGISel() {
  delete CurDAG;
  delete SwiftError;
}

void SelectionDAGISel::getAnalysisUsage(AnalysisUsage &AU) const {
  if (OptLevel != CodeGenOpt::None)
    AU.addRequired<AAResultsWrapperPass>();
  AU.addRequired<GCModuleInfo>();
  AU.addRequired<StackProtector>();
  AU.addPreserved<GCModuleInfo>();
  AU.addRequired<TargetLibraryInfoWrapperPass>();
  AU.addRequired<TargetTransformInfoWrapperPass>();
  if (UseMBPI && OptLevel != CodeGenOpt::None)
    AU.addRequired<BranchProbabilityInfoWrapperPass>();
  AU.addRequired<ProfileSummaryInfoWrapperPass>();
  if (OptLevel != CodeGenOpt::None)
    LazyBlockFrequencyInfoPass::getLazyBFIAnalysisUsage(AU);
  MachineFunctionPass::getAnalysisUsage(AU);
}

/// SplitCriticalSideEffectEdges - Look for critical edges with a PHI value that
/// may trap on it.  In this case we have to split the edge so that the path
/// through the predecessor block that doesn't go to the phi block doesn't
/// execute the possibly trapping instruction. If available, we pass domtree
/// and loop info to be updated when we split critical edges. This is because
/// SelectionDAGISel preserves these analyses.
/// This is required for correctness, so it must be done at -O0.
///
static void SplitCriticalSideEffectEdges(Function &Fn, DominatorTree *DT,
                                         LoopInfo *LI) {
  // Loop for blocks with phi nodes.
  for (BasicBlock &BB : Fn) {
    PHINode *PN = dyn_cast<PHINode>(BB.begin());
    if (!PN) continue;

  ReprocessBlock:
    // For each block with a PHI node, check to see if any of the input values
    // are potentially trapping constant expressions.  Constant expressions are
    // the only potentially trapping value that can occur as the argument to a
    // PHI.
    for (BasicBlock::iterator I = BB.begin(); (PN = dyn_cast<PHINode>(I)); ++I)
      for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
        ConstantExpr *CE = dyn_cast<ConstantExpr>(PN->getIncomingValue(i));
        if (!CE || !CE->canTrap()) continue;

        // The only case we have to worry about is when the edge is critical.
        // Since this block has a PHI Node, we assume it has multiple input
        // edges: check to see if the pred has multiple successors.
        BasicBlock *Pred = PN->getIncomingBlock(i);
        if (Pred->getTerminator()->getNumSuccessors() == 1)
          continue;

        // Okay, we have to split this edge.
        SplitCriticalEdge(
            Pred->getTerminator(), GetSuccessorNumber(Pred, &BB),
            CriticalEdgeSplittingOptions(DT, LI).setMergeIdenticalEdges());
        goto ReprocessBlock;
      }
  }
}

static void computeUsesMSVCFloatingPoint(const Triple &TT, const Function &F,
                                         MachineModuleInfo &MMI) {
  // Only needed for MSVC
  if (!TT.isWindowsMSVCEnvironment())
    return;

  // If it's already set, nothing to do.
  if (MMI.usesMSVCFloatingPoint())
    return;

  for (const Instruction &I : instructions(F)) {
    if (I.getType()->isFPOrFPVectorTy()) {
      MMI.setUsesMSVCFloatingPoint(true);
      return;
    }
    for (const auto &Op : I.operands()) {
      if (Op->getType()->isFPOrFPVectorTy()) {
        MMI.setUsesMSVCFloatingPoint(true);
        return;
      }
    }
  }
}

/// Walk backwards along constant GEPs and bitcasts to the base storage from \p
/// Start as far as possible. Prepend \Expression with an offset and deref.
static Value *walkToAllocaAndPrependOffsetDeref(const DataLayout &DL,
                                                Value *Start,
                                                DIExpression **Expression) {
  // This expression should *only* have a fragment part.
  assert(!(*Expression)->isComplex());
  APInt OffsetInBytes(DL.getTypeSizeInBits(Start->getType()), false);
  Value *End =
      Start->stripAndAccumulateInBoundsConstantOffsets(DL, OffsetInBytes);
  SmallVector<uint64_t, 3> Ops;
  if (OffsetInBytes.getBoolValue())
    Ops = {dwarf::DW_OP_plus_uconst, OffsetInBytes.getZExtValue()};
  Ops.push_back(dwarf::DW_OP_deref);
  *Expression = DIExpression::prependOpcodes(
      *Expression, Ops, /*StackValue=*/false, /*EntryValue=*/false);
  return End;
}

using DebugAggregate = std::pair<const DILocalVariable *, const DILocation *>;

/// In dwarf emission, the following sequence
///    1. dbg.value ... Fragment(0, 64)
///    2. dbg.value ... Fragment(0, 32)
/// effectively sets Fragment(32, 32) to undef (each def sets all bits not in
/// the intersection of the fragments to having "no location"). This makes
/// sense for values because splitting values would be troublesome, and is
/// probably quite uncommon.
/// When we convert dbg.assign to dbg.value+deref this kind of thing is common,
/// and describing a location (memory) rather than a value means we don't need
/// to worry about splitting any values.
/// This pass performs a(nother) dataflow analysis over the function, inserting
/// dbg.values such that any bits of a variable that have a memory location
/// retain that location at each subsequent dbg.value that doesn't overwrite
/// them. i.e., after a dbg.value, insert dbg.values with fragments for the
/// symmetric difference of "all bits currently in memory" and "the dbg.value's
/// fragment".
class AggregateFragmentsPass {
  Function &Fn;
  const DataLayout &Layout;
  DIBuilder DIB;

  using BaseAddress = Optional<Value *>;
  using OffsetInBitsTy = unsigned;
  using FragTraits = IntervalMapHalfOpenInfo<OffsetInBitsTy>;
  using FragsInMemMap = IntervalMap<
      OffsetInBitsTy, BaseAddress,
      IntervalMapImpl::NodeSizer<OffsetInBitsTy, BaseAddress>::LeafSize,
      FragTraits>;
  FragsInMemMap::Allocator IntervalMapAlloc;
  using Map = DenseMap<DebugAggregate, FragsInMemMap>;

  DenseMap<const BasicBlock *, Map> LiveIn;
  DenseMap<const BasicBlock *, Map> LiveOut;

  struct FragMemLoc {
    DebugAggregate Var;
    Value *Base;
    unsigned OffsetInBits;
    unsigned SizeInBits;
    DebugLoc DL;
  };
  using InsertMap = MapVector<Instruction *, SmallVector<FragMemLoc>>;
  DenseMap<const BasicBlock *, InsertMap> BBInsertAfterMap;

  static bool equ(const FragsInMemMap &A, const FragsInMemMap &B) {
    auto AIt = A.begin(), AEnd = A.end();
    auto BIt = B.begin(), BEnd = B.end();
    for (; AIt != AEnd; ++AIt, ++BIt) {
      if (BIt == BEnd)
        return false; // B has fewer elements than A.
      if (AIt.start() != BIt.start() || AIt.stop() != BIt.stop())
        return false; // Interval is different.
      if (AIt.value() != BIt.value())
        return false; // Value at interval is different.
    }
    // AIt == AEnd. Check BIt is also now at end.
    return BIt == BEnd;
  }

  static bool equ(const Map &A, const Map &B) {
    if (A.size() != B.size())
      return false;
    for (auto APair : A) {
      auto BIt = B.find(APair.first);
      if (BIt == B.end())
        return false;
      if (!equ(APair.second, BIt->second))
        return false;
    }
    return true;
  }

  /// Create a string from an optional value.
  static std::string toString(Optional<Value *> OptV) {
    if (OptV)
      return (*OptV)->getName().str();
    else
      return "None";
  }

  /// Format string describing an FragsInMemMap (IntervalMap) interval.
  static std::string toString(FragsInMemMap::const_iterator It,
                              bool Newline = true) {
    std::string String;
    std::stringstream S(String);
    if (It.valid()) {
      S << "[" << It.start() << ", " << It.stop()
        << "): " << toString(It.value());
    } else {
      S << "invalid iterator (end)";
    }
    if (Newline)
      S << "\n";
    return S.str();
  };

  FragsInMemMap join(FragsInMemMap &A, FragsInMemMap &B) {
    // TODO: prove/fix monotonicity (this feels shaky).
    FragsInMemMap Result(IntervalMapAlloc);
    for (auto AIt = A.begin(), AEnd = A.end(); AIt != AEnd; ++AIt) {
      FRAG_DEBUG(dbgs() << "a " << toString(AIt));
      // FIXME: This is basically copied from process() and inverted (process is
      // performing a union whereas this is more of an intersect. Thinking about
      // it, this is probably monotonic, and we just need to switch our terms
      // around (meet rather than join, since we're intersecting).

      // There's only work to do if (a) and (b) overlap at all.
      if (B.overlaps(AIt.start(), AIt.stop())) {
        // Does StartBit intersect an existing fragment?
        auto FirstOverlap = B.find(AIt.start());
        assert(FirstOverlap != B.end());
        bool IntersectStart = FirstOverlap.start() < AIt.start();
        FRAG_DEBUG(dbgs() << "- FirstOverlap " << toString(FirstOverlap, false)
                          << ", IntersectStart: " << IntersectStart << "\n");

        // Does EndBit intersect an existing fragment?
        auto LastOverlap = B.find(AIt.stop());
        bool IntersectEnd =
            LastOverlap != B.end() && LastOverlap.start() < AIt.stop();
        FRAG_DEBUG(dbgs() << "- LastOverlap " << toString(LastOverlap, false)
                          << ", IntersectEnd: " << IntersectEnd << "\n");

        // Both ends of (a) intersect the same interval (b).
        if (IntersectStart && IntersectEnd && FirstOverlap == LastOverlap) {
          // Insert (a) ((a) is contained in (b)) if the values match.
          FRAG_DEBUG(dbgs()
                     << "- a is contained within " << toString(FirstOverlap));
          if (AIt.value() && AIt.value() == FirstOverlap.value())
            Result.insert(AIt.start(), AIt.stop(), AIt.value());
          // FIXME: Emit dbg.values for these changes?
        } else {
          // Shorten any end-point intersections.
          //     [ - a - ]
          // [ - b - ]
          // -
          //     [ r ]
          auto Next = FirstOverlap;
          if (IntersectStart) {
            FRAG_DEBUG(dbgs() << "- insert intersection of a and "
                              << toString(FirstOverlap));
            if (AIt.value() && AIt.value() == FirstOverlap.value())
              Result.insert(AIt.start(), FirstOverlap.stop(), AIt.value());
            ++Next;
          }
          // [ - a - ]
          //     [ - b - ]
          // -
          //     [ r ]
          if (IntersectEnd) {
            FRAG_DEBUG(dbgs() << "- insert intersection of a and "
                              << toString(LastOverlap));
            if (AIt.value() && AIt.value() == LastOverlap.value())
              Result.insert(LastOverlap.start(), AIt.stop(), AIt.value());
          }

          // Insert any interval in B that is contained within (a) where
          // the values match.
          while (Next != B.end() && Next.start() < AIt.stop() &&
                 Next.stop() <= AIt.stop()) {
            FRAG_DEBUG(dbgs()
                       << "- insert intersection of a and " << toString(Next));
            if (AIt.value() && AIt.value() == Next.value())
              Result.insert(Next.start(), Next.stop(), Next.value());
            ++Next;
          }
        }
      }
    }
    return Result;
  }

  Map join(Map &A, Map &B) {
    // Join A and B.
    //
    // Result = join(a, b) for a in A, b in B where Var(a) == Var(b)
    // TODO: prove/fix monotonicity.
    Map Result;
    for (auto Pair : A) {
      auto AVar = Pair.first;
      auto &AFrags = Pair.second;
      auto BIt = B.find(AVar);
      if (BIt == B.end())
        continue; // Var has no bits defined in B,
      FRAG_DEBUG(dbgs() << "join fragment maps for " << AVar.first->getName()
                        << "\n");
      Result.insert(std::make_pair(AVar, join(AFrags, BIt->second)));
    }
    return A;
  }

  bool join(const BasicBlock &BB,
            const SmallPtrSet<BasicBlock *, 16> &Visited) {
    FRAG_DEBUG(dbgs() << "join block info from preds of " << BB.getName()
                      << "\n");

    Map BBLiveIn;
    unsigned NumJoined = 0;
    // For all predecessors of this MBB, find the set of FragMemLocs that
    // can be joined.
    for (auto I = pred_begin(&BB), E = pred_end(&BB); I != E; I++) {
      // Ignore backedges if we have not visited the predecessor yet. As the
      // predecessor hasn't yet had fragments propagated into it, none will be
      // valid yet, so treat them as all being uninitialized and potentially
      // valid. We will add fragments when we revisit this block, which will
      // cause the sucessors to get re-processed with that new info.  This is
      // essentially the same as initialising all bits to implicit ⊥ value.
      const BasicBlock *Pred = *I;
      if (!Visited.count(Pred))
        continue;
      auto Out = LiveOut.find(Pred);
      assert(Out != LiveOut.end());

      if (NumJoined == 0) {
        FRAG_DEBUG(dbgs() << "BBLiveIn = " << Pred->getName() << "\n");
        BBLiveIn = Out->second;
      } else {
        FRAG_DEBUG(dbgs() << "BBLiveIn = join BBLiveIn, " << Pred->getName()
                          << "\n");
        BBLiveIn = join(BBLiveIn, Out->second);
      }
      NumJoined++;
    }

    auto R = LiveIn.find(&BB);
    // Check if there isn't an entry, or there is but the LiveIn set has changed
    // (expensive check).
    if (R == LiveIn.end() || !equ(BBLiveIn, R->second)) {
      FRAG_DEBUG(dbgs() << "change=true on join on " << BB.getName() << "\n");
      LiveIn[&BB] = BBLiveIn;
      // A change has occured.
      return true;
    }
    FRAG_DEBUG(dbgs() << "change=false on join on " << BB.getName() << "\n");
    // No change.
    return false;
  }

  void insertMemLoc(BasicBlock &BB, Instruction &After, DebugAggregate Var,
                    unsigned StartBit, unsigned EndBit, Optional<Value *> Base,
                    DebugLoc DL) {
    if (!Base)
      return;
    FragMemLoc Loc;
    Loc.Var = Var;
    Loc.OffsetInBits = StartBit;
    Loc.SizeInBits = EndBit - StartBit;
    Loc.Base = *Base;
    Loc.DL = DL;
    BBInsertAfterMap[&BB][&After].push_back(Loc);
    FRAG_DEBUG(dbgs() << "INSERT dbg.value(mem) for " << Var.first->getName()
                      << " bits [" << StartBit << ", " << EndBit << ")\n");
  }

  Optional<int64_t> getDerefOffsetInBytes(const DIExpression *DIExpr) {
    int64_t Offset = 0;
    const unsigned NumElements = DIExpr->getNumElements();
    const auto Elements = DIExpr->getElements();
    unsigned NextElement = 0;
    // Extract the offset.
    if (NumElements > 2 && Elements[0] == dwarf::DW_OP_plus_uconst) {
      Offset = Elements[1];
      NextElement = 2;
    } else if (NumElements > 3 && Elements[0] == dwarf::DW_OP_constu) {
      NextElement = 3;
      if (Elements[2] == dwarf::DW_OP_plus)
        Offset = Elements[1];
      else if (Elements[2] == dwarf::DW_OP_minus)
        Offset = -Elements[1];
      else
        return None;
    }

    // If that's all there is it means there's no deref.
    if (NextElement >= NumElements)
      return None;

    // Check this ends in deref or deref then fragment.
    if (Elements[NextElement] == dwarf::DW_OP_deref) {
      if (NumElements == NextElement + 1)
        return Offset; // Ends with deref.
      else if (NumElements == NextElement + 3 &&
               Elements[NextElement] == dwarf::DW_OP_LLVM_fragment)
        return Offset; // Ends with deref + fragment.
    }

    // Don't bother trying to interpret anything more complex.
    return None;
  }

  void addDef(DbgValueInst &I, Map &LiveSet) {
    if (skipVariable(I))
      return;
    BasicBlock &BB = *I.getParent();
    DebugAggregate Var = {I.getVariable(), I.getDebugLoc()->getInlinedAt()};

    // [StartBit: EndBit) are the bits affected by this def.
    const DIExpression *DIExpr = I.getExpression();
    unsigned StartBit;
    unsigned EndBit;
    if (auto Frag = DIExpr->getFragmentInfo()) {
      StartBit = Frag->OffsetInBits;
      EndBit = StartBit + Frag->SizeInBits;
    } else {
      assert(static_cast<bool>(I.getVariable()->getSizeInBits()));
      StartBit = 0;
      EndBit = *I.getVariable()->getSizeInBits();
    }

    // We will only aggregate fragments for simple memory-describing dbg.value
    // intrinsics. If the fragment offset is the same as the offset from the
    // base pointer, do The Thing, otherwise fall back to normal dbg.value
    // behaviour.
    // Note: AT backend has generated dbg.values written in terms of the base
    // pointer.
    const auto DerefOffsetInBytes = getDerefOffsetInBytes(DIExpr);
    const Optional<Value *> Base =
        DerefOffsetInBytes && *DerefOffsetInBytes * 8 == StartBit
            ? Optional<Value *>(I.getValue())
            : None;
    FRAG_DEBUG(dbgs() << "DEF " << Var.first->getName() << " [" << StartBit
                      << ", " << EndBit << "): " << toString(Base) << "\n");

    // First of all, any locs that use mem that are disrupted need reinstating.
    // Unfortunately, IntervalMap doesn't let us insert intervals that overlap
    // with existing intervals so this code involves a lot of fiddling around
    // with intervals to do that manually.
    auto FragIt = LiveSet.find(Var);
    if (FragIt != LiveSet.end()) {
      FragsInMemMap &FragMap = FragIt->second;
      // First check the easy case: no overlap with existing fragments.
      if (!FragMap.overlaps(StartBit, EndBit)) {
        FRAG_DEBUG(dbgs() << "- No overlaps\n");
        FragMap.insert(StartBit, EndBit, Base);
      } else {
        // There is at least one overlap.

        // Does StartBit intersect an existing fragment?
        auto FirstOverlap = FragMap.find(StartBit);
        assert(FirstOverlap != FragMap.end());
        bool IntersectStart = FirstOverlap.start() < StartBit;

        // Does EndBit intersect an existing fragment?
        auto LastOverlap = FragMap.find(EndBit);
        bool IntersectEnd = LastOverlap.valid() && LastOverlap.start() < EndBit;

        // Both ends intersect the same existing fragment.
        if (IntersectStart && IntersectEnd && FirstOverlap == LastOverlap) {
          FRAG_DEBUG(dbgs() << "- Intersect single interval @ both ends\n");
          // Shorten it.
          auto EndBitOfOverlap = FirstOverlap.stop();
          FirstOverlap.setStop(StartBit);
          insertMemLoc(BB, I, Var, FirstOverlap.start(), StartBit,
                       FirstOverlap.value(), I.getDebugLoc());

          // Insert a new one to represent the end part.
          FragMap.insert(EndBit, EndBitOfOverlap, FirstOverlap.value());
          insertMemLoc(BB, I, Var, EndBit, EndBitOfOverlap,
                       FirstOverlap.value(), I.getDebugLoc());

          // Insert the new (middle) fragment now there is space.
          FragMap.insert(StartBit, EndBit, Base);
        } else {
          // Shorten any end-point intersections.
          if (IntersectStart) {
            FRAG_DEBUG(dbgs() << "- Intersect interval at start\n");
            // Split off at the intersection.
            FirstOverlap.setStop(StartBit);
            insertMemLoc(BB, I, Var, FirstOverlap.start(), StartBit,
                         FirstOverlap.value(), I.getDebugLoc());
          }

          if (IntersectEnd) {
            FRAG_DEBUG(dbgs() << "- Intersect interval at end\n");
            // Split off at the intersection.
            LastOverlap.setStart(EndBit);
            insertMemLoc(BB, I, Var, EndBit, LastOverlap.stop(),
                         LastOverlap.value(), I.getDebugLoc());
          }

          FRAG_DEBUG(dbgs() << "- Erase intervals contained within\n");
          // FirstOverlap and LastOverlap have been shortened such that they're
          // no longer overlapping with [StartBit, EndBit). Delete any overlaps
          // that remain (these will be full contained within DEF).
          auto It = FirstOverlap;
          if (IntersectStart)
            ++It; // IntersectStart: first overlap has been shortened.
          while (It.valid() && It.start() >= StartBit && It.stop() <= EndBit) {
            FRAG_DEBUG(dbgs() << "- Erase " << toString(It));
            It.erase(); // This increments It after removing the interval.
          }
          // We've dealt with all the overlaps now!
          assert(!FragMap.overlaps(StartBit, EndBit));
          FRAG_DEBUG(dbgs() << "- Insert DEF into now-empty space\n");
          FragMap.insert(StartBit, EndBit, Base);
        }
      }
    } else {
      // Add this variable to the BB map.
      auto P =
          LiveSet.insert(std::make_pair(Var, FragsInMemMap(IntervalMapAlloc)));
      assert(P.second && "Var already in map?");
      // Add the interval to the fragment map.
      P.first->second.insert(StartBit, EndBit, Base);
    }
  }

  bool skipVariable(DbgValueInst &I) {
    return !I.getVariable()->getSizeInBits();
  }

  void process(BasicBlock &BB, Map &LiveSet) {
    BBInsertAfterMap[&BB].clear();
    for (auto &I : BB) {
      if (auto *DVI = dyn_cast<DbgValueInst>(&I))
        addDef(*DVI, LiveSet);
    }
  }

public:
  AggregateFragmentsPass(Function &Fn, const DataLayout &Layout)
      : Fn(Fn), Layout(Layout), DIB(*Fn.getParent()) {}

  void run() {
    if (!EnableFragAgg)
      return;

    // Prepare for traversal.
    //
    ReversePostOrderTraversal<Function *> RPOT(&Fn);
    std::priority_queue<unsigned int, std::vector<unsigned int>,
                        std::greater<unsigned int>>
        Worklist;
    std::priority_queue<unsigned int, std::vector<unsigned int>,
                        std::greater<unsigned int>>
        Pending;
    DenseMap<unsigned int, BasicBlock *> OrderToBB;
    DenseMap<BasicBlock *, unsigned int> BBToOrder;
    { // Init OrderToBB and BBToOrder.
      unsigned int RPONumber = 0;
      for (auto RI = RPOT.begin(), RE = RPOT.end(); RI != RE; ++RI) {
        OrderToBB[RPONumber] = *RI;
        BBToOrder[*RI] = RPONumber;
        Worklist.push(RPONumber);
        ++RPONumber;
      }
    }

    // Perform the traversal.
    //
    // This is a standard "union of predecessor outs" dataflow problem. To solve
    // it, we perform join() and process() using the two worklist method until
    // the LiveIn data for each block becomes unchanging. TODO: Prove/fix
    // monotonicity.
    //
    SmallPtrSet<BasicBlock *, 16> Visited;
    while (!Worklist.empty() || !Pending.empty()) {
      // We track what is on the pending worklist to avoid inserting the same
      // thing twice.  We could avoid this with a custom priority queue, but
      // this is probably not worth it.
      SmallPtrSet<BasicBlock *, 16> OnPending;
      FRAG_DEBUG(dbgs() << "Processing Worklist\n");
      while (!Worklist.empty()) {
        BasicBlock *BB = OrderToBB[Worklist.top()];
        FRAG_DEBUG(dbgs() << "\nPop BB " << BB->getName() << "\n");
        Worklist.pop();
        bool InChanged = join(*BB, Visited);
        // Always consider LiveIn changed on the first visit.
        InChanged |= Visited.insert(BB).second;
        if (InChanged) {
          FRAG_DEBUG(dbgs()
                     << BB->getName() << " has new InLocs, process it\n");
          //  Mutate a copy of LiveIn while processing BB. Once we've processed
          //  the terminator LiveSet is the LiveOut set for BB.
          //  This is an expensive copy!
          Map LiveSet = LiveIn[BB];

          // Process the instructions in the block; apply transfer functions.
          process(*BB, LiveSet);

          // Relatively expensive check: has anything changed in LiveOut for BB?
          if (!equ(LiveOut[BB], LiveSet)) {
            FRAG_DEBUG(dbgs() << BB->getName()
                              << " has new OutLocs, add succs to worklist: [ ");
            LiveOut[BB] = LiveSet;
            for (auto I = succ_begin(BB), E = succ_end(BB); I != E; I++) {
              if (OnPending.insert(*I).second) {
                FRAG_DEBUG(dbgs() << I->getName() << " ");
                Pending.push(BBToOrder[*I]);
              }
            }
            FRAG_DEBUG(dbgs() << "]\n");
          }
        }
      }
      Worklist.swap(Pending);
      // At this point, pending must be empty, since it was just the empty
      // worklist
      assert(Pending.empty() && "Pending should be empty");
    }

    // Insert new dbg.values.
    for (auto Pair : BBInsertAfterMap) {
      InsertMap &Map = Pair.second;
      for (auto Pair : Map) {
        Instruction *InsertAfter = Pair.first;
        auto &Ctx = InsertAfter->getContext();
        auto FragMemLocs = Pair.second;
        for (auto FragMemLoc : FragMemLocs) {
          DIExpression *Expr = DIExpression::get(Ctx, None);
          if (auto Result = DIExpression::createFragmentExpression(
                  Expr, FragMemLoc.OffsetInBits, FragMemLoc.SizeInBits))
            Expr = Result.getValue();
          else
            assert(false && "this shouldn't happen?");
          Expr = DIExpression::prepend(Expr, DIExpression::DerefAfter,
                                       FragMemLoc.OffsetInBits / 8);
          auto *DVI = DIB.insertDbgValueIntrinsic(
              FragMemLoc.Base,
              const_cast<DILocalVariable *>(FragMemLoc.Var.first), Expr,
              FragMemLoc.DL, InsertAfter);
          DVI->moveAfter(InsertAfter);
          (void)DVI;
          FRAG_DEBUG(dbgs() << "Insert: " << *DVI << "\n");
        }
      }
    }
  }
};

class CoffeeLoweringPass {
public:
  /// The kind of location in use for a variable where Mem is the stack home,
  /// Val is an SSA value or const, None means that there is not one single
  /// kind (either because there are multiple, or because there is none; it may
  /// prove useful to split this into two values in the future).
  ///
  /// LocKind is a join-semilattice with the partial order:
  /// None > Mem, Val
  ///
  /// i.e. evaluating from first to last:
  /// join(x, x) = x
  /// join(x, y) = None
  ///
  /// @OCH
  /// NOTE: We need LocKind in BlockInfo because we are not tracking Assignment
  /// PHI operands, just that there is one. This means we have no way to
  /// distinguish two seperate PHIs, which means that we cannot derrive the
  /// current location for a variable by looking at StackHomeValue and
  /// DebugValue alone. If/When proper PHI-tracking is implemented we can
  /// remove LocKind.
  ///
  /// Random example of another benefit of tracking assignment-PHIs: The
  /// (then-implicit) partial order could be changed to Val > Mem. If the ID of
  /// the assignment is the same then join(Mem, Val) could become Val rather
  /// than None.
  enum class LocKind { Mem, Val, None };

  /// An abstraction over the assignment of a value to a variable.
  /// Each assignment has an DIAssignID that represents the storing
  /// of a value to a source variable.
  ///
  /// An Assignment is Known or NoneOrPhi. A Known Assignment
  /// means we know the ID of the last assignment. UnknownOrPhi means that we
  /// don't (or can't) know the ID of the last assignment that took place.
  ///
  /// The Status of the Assignment (Known or NoneOrPhi) is another
  /// join-semilattice. The partial order is:
  /// NoneOrPhi > Known {id_0, id_1, ...id_N}
  ///
  /// e.g.
  /// join(x, x) = x
  /// join(x, y) = NoneOrPhi
  struct Assignment {
    enum S { Known, NoneOrPhi } Status;
    DIAssignID *ID;
    // TODO @OCH: Track Assignment PHIs. Split NoneOrPhi into None and Phi &
    // change this to vec<DAI*>.
    DbgAssignIntrinsic *Source;
    bool operator==(const Assignment &Other) const {
      // Don't include Source in the equality check. Assignments are
      // defined by their ID, not debug intrinsic(s).
      return std::tie(Status, ID) == std::tie(Other.Status, Other.ID);
    }
    void dump(raw_ostream &OS) {
      static const char *LUT[] = {"Known", "NoneOrPhi"};
      OS << LUT[Status] << "(id=";
      if (ID)
        OS << ID;
      else
        OS << "null";
      OS << ", s=";
      if (Source)
        OS << *Source;
      else
        OS << "null";
      OS << ")";
    }
    bool operator!=(const Assignment &Other) const {
      return !(*this == Other);
    }

    static Assignment make(DIAssignID *ID, DbgAssignIntrinsic *Source) {
      return Assignment(Known, ID, Source);
    }
    static Assignment makeNoneOrPhi() {
      return Assignment(NoneOrPhi, nullptr, nullptr);
    }
    // Again, need a Top value?
    Assignment()
        : Status(NoneOrPhi), ID(nullptr), Source(nullptr) {
    } // Can we delete this?
    Assignment(S Status, DIAssignID *Value, DbgAssignIntrinsic *Source)
        : Status(Status), ID(Value), Source(Source) {
      // If the Status is Kown then we expect there to be a Value.
      assert(Status != Known || Value != nullptr);
    }
  };
  using AssignmentMap = DenseMap<DebugVariable, Assignment>;
  using LocMap = DenseMap<DebugVariable, LocKind>;
  using OverlapMap = DenseMap<DebugVariable, SmallVector<DebugVariable, 4>>;

private:
  /// Map a variable to the set of variables that it fully contains.
  OverlapMap VarContains;

  // Machinery to defer inserting dbg.values.
  using InsertMap =
      MapVector<Instruction *, MapVector<DebugVariable, DbgValueInst *>>;
  InsertMap InsertBeforeMap;
  void emitDbgValue(LocKind Kind, const DbgAssignIntrinsic *Source,
                    Instruction *After);

  static bool equ(const AssignmentMap &A, const AssignmentMap &B) {
    if (A.size() != B.size())
      return false;
    for (auto Pair : A) {
      const DebugVariable &Var = Pair.first;
      const Assignment &AV = Pair.second;
      auto R = B.find(Var);
      // Check if this entry exists in B, otherwise ret false.
      if (R == B.end())
        return false;
      // Check that the assignment value is the same.
      if (AV != R->second)
        return false;
    }
    return true;
  }

  struct BlockInfo {
    /// A record of the currently live location kind for each variable.
    /// @OCH
    /// NOTE: We need to track LocKind because we are not tracking Assignment
    /// PHI operands, just that there is one. This means we have no way to
    /// distinguish two seperate PHIs, which means that we cannot derrive the
    /// current location for a variable by looking at StackHomeValue and
    /// DebugValue alone. If/When proper PHI-tracking is implemented we can
    /// remove LocKind.
    LocMap LiveLoc;

    AssignmentMap StackHomeValue;
    AssignmentMap DebugValue;
    // @OCH This is not a particularly speedy check!
    bool operator==(const BlockInfo &Other) const {
      return LiveLoc == Other.LiveLoc &&
             equ(StackHomeValue, Other.StackHomeValue) &&
             equ(DebugValue, Other.DebugValue);
    }
    bool operator!=(const BlockInfo &Other) const { return !(*this == Other); }
    bool isValid() {
      return LiveLoc.size() == DebugValue.size() &&
             LiveLoc.size() == StackHomeValue.size();
    }
  };

  Function &Fn;
  const DataLayout &Layout;
  DIBuilder DIB;

  DenseMap<const BasicBlock *, BlockInfo> LiveIn;
  DenseMap<const BasicBlock *, BlockInfo> LiveOut;

  // Helper for process and transfer* to track variables touched each frame.
  DenseSet<DebugVariable> VarsTouchedThisFrame;

  /// The set of variables that sometimes are not located in their stack home.
  DenseSet<DebugAggregate> NotAlwaysStackHomed;

  void handleDbgAssignTransfer(DbgAssignIntrinsic &DAI, BlockInfo *LiveSet);
  void handleDbgValueTransfer(DbgValueInst &DVI, BlockInfo *LiveSet);

  // Core functions.
  //
  // FIXME: @OCH These join functions are slow (aiming for simple working
  // implementation first!).
  static LocKind join(LocKind A, LocKind B);
  static LocMap join(const LocMap &A, const LocMap &B);
  static Assignment join(const Assignment &A,
                       const Assignment &B);
  static AssignmentMap join(const AssignmentMap &A, const AssignmentMap &B);
  static BlockInfo join(const BlockInfo &A, const BlockInfo &B);
  /// Join pred LiveOuts into BB LiveIn. Return True if LiveIn has changed.
  bool join(const BasicBlock &BB, const SmallPtrSet<BasicBlock *, 16> &Visited);
  void process(BasicBlock &BB, BlockInfo *LiveSet);
  void transferMemDef(Instruction &I, BlockInfo *LiveSet);
  void transferDbgDef(Instruction &I, BlockInfo *LiveSet);

  // Helper methods.
  void setLocKind(BlockInfo *LiveSet, DebugVariable Var, LocKind K);
  /// Get the live LocKind for a variable. Var must have already been defined
  /// through addMemDef or addDbgDef.
  LocKind getLocKind(BlockInfo *LiveSet, DebugVariable Var);
  // @OCH These ones could possibly be BlockInfo methods?  Keep them here for
  // now because setLocKind cannot be, and it seems less confusing to keep all
  // the methods in one place.
  void addMemDef(BlockInfo *LiveSet, DebugVariable Var, Assignment AV);
  void addDbgDef(BlockInfo *LiveSet, DebugVariable Var, Assignment AV);

public:
  CoffeeLoweringPass(Function &Fn, const DataLayout &Layout)
      : Fn(Fn), Layout(Layout), DIB(*Fn.getParent()) {}
  bool run();
};

void CoffeeLoweringPass::setLocKind(BlockInfo *LiveSet, DebugVariable Var,
                                    LocKind K) {
  auto SetKind = [this](BlockInfo *LiveSet, DebugVariable Var,
                        LocKind K) {
    VarsTouchedThisFrame.insert(Var);
    LiveSet->LiveLoc[Var] = K;
  };
  SetKind(LiveSet, Var, K);

  // Update the LocKind for all fragments contained within Var.
  for (DebugVariable Frag : VarContains[Var])
    SetKind(LiveSet, Frag, K);
}

CoffeeLoweringPass::LocKind CoffeeLoweringPass::getLocKind(BlockInfo *LiveSet,
                                                           DebugVariable Var) {
  auto Pair = LiveSet->LiveLoc.find(Var);
  assert(Pair != LiveSet->LiveLoc.end());
  return Pair->second;
}

void CoffeeLoweringPass::addMemDef(BlockInfo *LiveSet, DebugVariable Var,
                                   Assignment AV) {
  auto AddDef = [](BlockInfo *LiveSet, DebugVariable Var, Assignment AV) {
    LiveSet->StackHomeValue[Var] = AV;
    // Add (Var -> ⊤) to DebugValue if Var isn't in DebugValue yet.
    LiveSet->DebugValue.insert({Var, Assignment::makeNoneOrPhi()});
    // Add (Var -> ⊤) to LiveLocs if Var isn't in LiveLocs yet.
    LiveSet->LiveLoc.insert({Var, LocKind::None});
  };
  AddDef(LiveSet, Var, AV);

  // Use this assigment for all fragments contained within Var, but do not
  // provide a Source because we cannot convert Var's value to a value for the
  // fragment.
  Assignment FragAV = AV;
  FragAV.Source = nullptr;
  for (DebugVariable Frag : VarContains[Var])
    AddDef(LiveSet, Frag, FragAV);
}

void CoffeeLoweringPass::addDbgDef(BlockInfo *LiveSet, DebugVariable Var,
                                   Assignment AV) {
  auto AddDef = [](BlockInfo *LiveSet, DebugVariable Var, Assignment AV) {
    LiveSet->DebugValue[Var] = AV;
    // Set StackHome[Var] = ⊤ if Var isn't in StackHome yet.
    LiveSet->StackHomeValue.insert({Var, Assignment::makeNoneOrPhi()});
    // Add (Var -> ⊤) to LiveLocs if Var isn't in LiveLocs yet.
    LiveSet->LiveLoc.insert({Var, LocKind::None});
  };
  AddDef(LiveSet, Var, AV);

  // Use this assigment for all fragments contained within Var, but do not
  // provide a Source because we cannot convert Var's value to a value for the
  // fragment.
  Assignment FragAV = AV;
  FragAV.Source = nullptr;
  for (DebugVariable Frag : VarContains[Var])
    AddDef(LiveSet, Frag, FragAV);
}

static DebugVariable getVariable(const DbgVariableIntrinsic *DII) {
  return DebugVariable(DII->getVariable(),
                       DII->getExpression()->getFragmentInfo(),
                       DII->getDebugLoc().getInlinedAt());
}

static DIAssignID *getIDFromInst(const Instruction &I) {
  return cast<DIAssignID>(I.getMetadata(LLVMContext::MD_DIAssignID));
}

static DIAssignID *getIDFromMarker(const DbgAssignIntrinsic &DAI) {
  return cast<DIAssignID>(DAI.getAssignId());
}

/// Return true if Var exists in A and B and has the same value.
static bool hasVarWithValue(DebugVariable Var,
                            CoffeeLoweringPass::Assignment AV,
                            CoffeeLoweringPass::AssignmentMap &M) {
  auto R = M.find(Var);
  if (R == M.end())
    return false;
  return R->second == AV;
}

const char *locStr(CoffeeLoweringPass::LocKind Loc) {
  using LocKind = CoffeeLoweringPass::LocKind;
  switch (Loc) {
  case LocKind::Val:
    return "Val";
  case LocKind::Mem:
    return "Mem";
  case LocKind::None:
    return "None";
  };
  llvm_unreachable("unknown LocKind");
}

void CoffeeLoweringPass::emitDbgValue(CoffeeLoweringPass::LocKind Kind,
                                      const DbgAssignIntrinsic *Source,
                                      Instruction *After) {
  // 1. Build a dbg.value.
  Value *Val = nullptr;
  DIExpression *Expr = nullptr;
  DILocalVariable *LocalVar = Source->getVariable();
  DILocation *DL = Source->getDebugLoc();
  Instruction *InsertBefore = nullptr; // We will insert these later.
  switch (Kind) {
  case LocKind::Mem: {
    Val = Source->getAddress();
    Expr = DIExpression::get(Source->getContext(), None);
    if (auto OptFragInfo = Source->getExpression()->getFragmentInfo()) {
      auto FragInfo = OptFragInfo.getValue();
      if (auto Result = DIExpression::createFragmentExpression(
              Expr, FragInfo.OffsetInBits, FragInfo.SizeInBits))
        Expr = Result.getValue();
      else
        llvm_unreachable("createFragmentExpression unexpectedly failed");
    }
    Val = walkToAllocaAndPrependOffsetDeref(Layout, Val, &Expr);
  } break;
  case LocKind::Val: {
    /// Get the value component, converting to Undef if it is variadic.
    Val = Source->hasArgList() ? UndefValue::get(Source->getValue()->getType())
                               : Source->getValue();
    Expr = Source->getExpression();
  } break;
  case LocKind::None: {
    Val = UndefValue::get(Source->getValue()->getType());
    Expr = Source->getExpression();
  } break;
  }
  assert(Val && Expr);
  auto *DVI = cast<DbgValueInst>(
      DIB.insertDbgValueIntrinsic(Val, LocalVar, Expr, DL, InsertBefore));

  // 2. Find a suitable insert point.
  InsertBefore = After->getNextNode();
  assert(InsertBefore && "Shouldn't be inserting after a terminator");

  // 3. Delete existing dbg.value in map.
  // @OCH When this is all working, do something smarter (faster) here.
  DebugVariable Var = getVariable(Source);
  if (auto *Existing = InsertBeforeMap[InsertBefore][Var])
    delete Existing;

  // 4. Insert it into the map for later.
  InsertBeforeMap[InsertBefore][Var] = DVI;
}

void CoffeeLoweringPass::transferMemDef(
    Instruction &I, CoffeeLoweringPass::BlockInfo *LiveSet) {
  auto Linked = at::getAssignmentMarkers(&I);
  if (Linked.empty())
    return;

  COFFEE_DEBUG(dbgs() << "transferMemDef on " << I << "\n");
  DenseSet<DebugVariable> Seen;
  for (DbgAssignIntrinsic *DAI : Linked) {
    DebugVariable Var = getVariable(DAI);
    if (!Seen.insert(Var).second)
      continue; // Ignore duplicate var entries. @OCH This feels sus.

    Assignment AV = Assignment::make(getIDFromInst(I), DAI);
    addMemDef(LiveSet, Var, AV);

    COFFEE_DEBUG(dbgs() << "   linked to " << *DAI << "\n");
    COFFEE_DEBUG(dbgs() << "   LiveLoc " << locStr(getLocKind(LiveSet, Var))
                        << " -> ");

    // The last assignment to the stack is now AV. Check if the last debug
    // assignment has a matching Assignment.
    if (hasVarWithValue(Var, AV, LiveSet->DebugValue)) {
      // The StackHomeValue and DebugValue for this variable match so we can
      // emit a stack home location here.
      COFFEE_DEBUG(dbgs() << "Mem, Stack matches Debug program\n";);
      COFFEE_DEBUG(dbgs() << "   Stack val: "; AV.dump(dbgs()); dbgs() << "\n");
      COFFEE_DEBUG(dbgs() << "   Debug val: ";
                   LiveSet->DebugValue[Var].dump(dbgs()); dbgs() << "\n");
      setLocKind(LiveSet, Var, LocKind::Mem);
      emitDbgValue(LocKind::Mem, DAI, &I);
    } else {
      // The StackHomeValue and DebugValue for this variable do not match. I.e.
      // The value currently stored in the stack is not what we'd expect to
      // see, so we cannot use emit a stack home location here. Now we will
      // look at the live LocKind for the variable and determine an appropriate
      // dbg.value to emit.
      LocKind PrevLoc = getLocKind(LiveSet, Var);
      switch (PrevLoc) {
      case LocKind::Val: {
        // The value memory in memory has changed but we're not currently using
        // the memory location. Do nothing.
        COFFEE_DEBUG(dbgs() << "Val, (unchanged)\n";);
        setLocKind(LiveSet, Var, LocKind::Val);
      } break;
      case LocKind::Mem: {
        // There's been an assignment to memory that we were using as a
        // location for this variable, and the Assignment doesn't match what
        // we'd expect to see in memory.
        if (LiveSet->DebugValue[Var].Status == Assignment::NoneOrPhi) {
          // We need to terminate any previously open location now.
          COFFEE_DEBUG(dbgs() << "None, No Debug value available\n";);
          setLocKind(LiveSet, Var, LocKind::None);
          emitDbgValue(LocKind::None, DAI, &I);
        } else {
          // The previous DebugValue Value can be used here.
          COFFEE_DEBUG(dbgs() << "Val, Debug value is Known\n";);
          setLocKind(LiveSet, Var, LocKind::Val);
          Assignment PrevAV = LiveSet->DebugValue.lookup(Var);
          if (PrevAV.Source) {
            emitDbgValue(LocKind::Val, PrevAV.Source, &I);
          } else {
            // PrevAV.Source is nullptr so we must emit undef here.
            emitDbgValue(LocKind::None, DAI, &I);
          }
        }
      } break;
      case LocKind::None: {
        // There's been an assignment to memory and we currently are
        // not tracking a location for the variable. Do not emit anything.
        COFFEE_DEBUG(dbgs() << "None, (unchanged)\n";);
        setLocKind(LiveSet, Var, LocKind::None);
      } break;
      }
    }
  }
}

void CoffeeLoweringPass::handleDbgAssignTransfer(DbgAssignIntrinsic &DAI,
                                                 BlockInfo *LiveSet) {
  DebugVariable Var = getVariable(&DAI);
  Assignment AV = Assignment::make(getIDFromMarker(DAI), &DAI);
  addDbgDef(LiveSet, Var, AV);

  COFFEE_DEBUG(dbgs() << "handleDbgAssignTransfer on " << DAI << "\n";);
  COFFEE_DEBUG(dbgs() << "   LiveLoc " << locStr(getLocKind(LiveSet, Var))
                      << " -> ");

  // Check if the DebugValue and StackHomeValue both hold the same
  // Assignment.
  if (hasVarWithValue(Var, AV, LiveSet->StackHomeValue)) {
    // They match. We can use the stack home because the debug intrinsics state
    // that an assignment happened here, and we know that specific assignment
    // was the last one to take place in memory for this variable.
    if (isa<UndefValue>(DAI.getAddress())) {
      // Address may be undef to indicate that although the store does take
      // place, this part of the original store has been elided.
      COFFEE_DEBUG(
          dbgs() << "Val, Stack matches Debug program but address is undef\n";);
      setLocKind(LiveSet, Var, LocKind::Val);
    } else {
      COFFEE_DEBUG(dbgs() << "Mem, Stack matches Debug program\n";);
      setLocKind(LiveSet, Var, LocKind::Mem);
    };
    emitDbgValue(LocKind::Mem, &DAI, &DAI);
  } else {
    // The last assignment to the memory location isn't the one that we want to
    // show to the user so emit a dbg.value(Value). Value may be undef.
    COFFEE_DEBUG(dbgs() << "Val, Stack contents is unknown\n";);
    setLocKind(LiveSet, Var, LocKind::Val);
    emitDbgValue(LocKind::Val, &DAI, &DAI);
  }
}
void CoffeeLoweringPass::handleDbgValueTransfer(DbgValueInst &DVI,
                                                BlockInfo *LiveSet) {
  DebugVariable Var = getVariable(&DVI);
  // We have no ID to create an Assignment with so we must mark this
  // assignment as NoneOrPhi. Note that the dbg.value still exists, we just
  // cannot determine the assignment responsible for setting this value.
  Assignment AV = Assignment::makeNoneOrPhi();
  addDbgDef(LiveSet, Var, AV);

  COFFEE_DEBUG(dbgs() << "handleDbgValueTransfer on " << DVI << "\n";);
  COFFEE_DEBUG(dbgs() << "   LiveLoc " << locStr(getLocKind(LiveSet, Var))
                      << " -> Val, dbg.value override");

  setLocKind(LiveSet, Var, LocKind::Val);
  // Do not emit anything. We already have a dbg.value here.
}

void CoffeeLoweringPass::transferDbgDef(
    Instruction &I, CoffeeLoweringPass::BlockInfo *LiveSet) {
  if (auto *DAI = dyn_cast<DbgAssignIntrinsic>(&I))
    handleDbgAssignTransfer(*DAI, LiveSet);
  else if (auto *DVI = dyn_cast<DbgValueInst>(&I))
    handleDbgValueTransfer(*DVI, LiveSet);
  // Ignore dbg.declares. A variable location(s) may be described with either a
  // dbg.declare or a set of {dbg.assign and dbg.value}.
}

void CoffeeLoweringPass::process(BasicBlock &BB, BlockInfo *LiveSet) {
  for (auto II = BB.begin(), EI = BB.end(); II != EI;) {
    assert(VarsTouchedThisFrame.empty());
    // Process the instructions in "frames". A "frame" includes a single
    // non-debug instruction followed any debug instructions before the
    // next non-debug instruction.
    if (!isa<DbgInfoIntrinsic>(&*II)) {
      transferMemDef(*II, LiveSet);
      assert(LiveSet->isValid());
      ++II;
    }
    while (II != EI) {
      if (!isa<DbgInfoIntrinsic>(&*II))
        break;
      transferDbgDef(*II, LiveSet);
      assert(LiveSet->isValid());
      ++II;
    }

    // We've processed everything in the "frame". Now determine which variables
    // cannot be represented by a dbg.declare.
    for (auto Var : VarsTouchedThisFrame) {
      LocKind Loc = getLocKind(LiveSet, Var);
      // If a variable's LocKind is anything other than LocKind::Mem then we
      // must note that it cannot be represented with a dbg.declare.
      // Note that this check is enough without having to check the result of
      // joins() because for join to produce anything other than Mem after
      // we've already seen a Mem we'd be joining None or Val with Mem. In that
      // case, we've already hit this codepath when we set the LocKind to Val
      // or None in that block.
      if (Loc != LocKind::Mem) {
        DebugAggregate Aggr{Var.getVariable(), Var.getInlinedAt()};
        NotAlwaysStackHomed.insert(Aggr);
      }
    }
    VarsTouchedThisFrame.clear();
  }
}

CoffeeLoweringPass::LocKind CoffeeLoweringPass::join(LocKind A,
                                                     LocKind B) {
  // Partial order:
  // None > Mem, Val
  return A == B ? A : LocKind::None;
}

CoffeeLoweringPass::LocMap CoffeeLoweringPass::join(const LocMap &A,
                                                    const LocMap &B) {
  // Join A and B.
  //
  // U = join(a, b) for a in A, b in B where Var(a) == Var(b)
  // D = join(x, ⊤) for x where Var(x) is in A xor B
  // Join = U ∪ D
  //
  // This is achieved by performing a join on elements from A and B with
  // variables common to both A and B (join elements indexed by var intersect),
  // then adding LocKind::None elements for vars in A xor B. The latter part is
  // equivalent to performing join on elements with variables in A xor B with
  // LocKind::None (⊤) since join(x, ⊤) = ⊤.
  LocMap Join;
  SmallVector<DebugVariable, 16> SymmetricDifference;
  // Insert the join of the elements with common vars into Join. Add the
  // remaining elements to into SymmetricDifference.
  for (auto Pair : A) {
    const DebugVariable &Var = Pair.first;
    const LocKind Loc = Pair.second;
    // If this Var doesn't exist in B then add it to the symmetric difference
    // set.
    auto R = B.find(Var);
    if (R == B.end()) {
      SymmetricDifference.push_back(Var);
      continue;
    }
    // There is an entry for Var in both, join it.
    Join[Var] = join(Loc, R->second);
  }
  unsigned IntersectSize = Join.size();
  (void)IntersectSize;

  // Add the elements in B with variables that are not in A into
  // SymmetricDifference.
  for (auto Pair : B) {
    const DebugVariable &Var = Pair.first;
    if (A.count(Var) == 0)
      SymmetricDifference.push_back(Var);
  }

  // Add SymmetricDifference elements to Join and return the result.
  for (auto Var : SymmetricDifference)
    Join.insert({Var, LocKind::None});

  assert(Join.size() == (IntersectSize + SymmetricDifference.size()));
  assert(Join.size() >= A.size() && Join.size() >= B.size());
  return Join;
}

CoffeeLoweringPass::Assignment
CoffeeLoweringPass::join(const Assignment &A,
                         const Assignment &B) {
  // Partial order:
  // NoneOrPhi(null, null) > Known(v, ?s)

  // If either are NoneOrPhi the join is NoneOrPhi.
  // If either value is different then the result is
  // NoneOrPhi (joining two values is a Phi).
  if (A.Status != B.Status || A.ID != B.ID)
    return Assignment::makeNoneOrPhi();

  if (A.Status == Assignment::NoneOrPhi)
    return Assignment::makeNoneOrPhi();

  // The source may differ in this situation:
  // Pred.1:
  //   dbg.assign i32 0, ..., !1, ...
  // Pred.2:
  //   dbg.assign i32 1, ..., !1, ...
  // Here the same assignment (!1) was performed in both preds in the source,
  // but we can't use either one unless they are identical (e.g. .we don't
  // want to arbitrarily pick between constant values).
  auto *Source = [&]() -> DbgAssignIntrinsic * {
    if (A.Source == B.Source)
      return A.Source;
    if (A.Source == nullptr || B.Source == nullptr)
      return nullptr;
    if (A.Source->isIdenticalTo(B.Source))
      return A.Source;
    return nullptr;
  }();
  assert(A.Status == B.Status && A.Status == Assignment::Known);
  assert(A.ID == B.ID);
  return Assignment::make(A.ID, Source);
}

CoffeeLoweringPass::AssignmentMap
CoffeeLoweringPass::join(const AssignmentMap &A, const AssignmentMap &B) {
  // Join A and B.
  //
  // U = join(a, b) for a in A, b in B where Var(a) == Var(b)
  // D = join(x, ⊤) for x where Var(x) is in A xor B
  // Join = U ∪ D
  //
  // This is achieved by performing a join on elements from A and B with
  // variables common to both A and B (join elements indexed by var intersect),
  // then adding LocKind::None elements for vars in A xor B. The latter part is
  // equivalent to performing join on elements with variables in A xor B with
  // Status::NoneOrPhi (⊤) since join(x, ⊤) = ⊤.
  AssignmentMap Join;
  SmallVector<DebugVariable, 16> SymmetricDifference;
  // Insert the join of the elements with common vars into Join. Add the
  // remaining elements to into SymmetricDifference.
  for (auto Pair : A) {
    const DebugVariable &Var = Pair.first;
    const Assignment AV = Pair.second;
    // If this Var doesn't exist in B then add it to the symmetric difference
    // set.
    auto R = B.find(Var);
    if (R == B.end()) {
      SymmetricDifference.push_back(Var);
      continue;
    }
    // There is an entry for Var in both, join it.
    Join[Var] = join(AV, R->second);
  }
  unsigned IntersectSize = Join.size();
  (void)IntersectSize;

  // Add the elements in B with variables that are not in A into
  // SymmetricDifference.
  for (auto Pair : B) {
    const DebugVariable &Var = Pair.first;
    if (A.count(Var) == 0)
      SymmetricDifference.push_back(Var);
  }

  // Add SymmetricDifference elements to Join and return the result.
  for (auto Var : SymmetricDifference)
    Join.insert({Var, Assignment::makeNoneOrPhi()});

  assert(Join.size() == (IntersectSize + SymmetricDifference.size()));
  assert(Join.size() >= A.size() && Join.size() >= B.size());
  return Join;
}

CoffeeLoweringPass::BlockInfo
CoffeeLoweringPass::join(const BlockInfo &A, const BlockInfo &B) {
  BlockInfo Join;
  Join.LiveLoc = join(A.LiveLoc, B.LiveLoc);
  Join.StackHomeValue = join(A.StackHomeValue, B.StackHomeValue);
  Join.DebugValue = join(A.DebugValue, B.DebugValue);
  assert(Join.isValid());
  return Join;
}

bool CoffeeLoweringPass::join(const BasicBlock &BB,
                              const SmallPtrSet<BasicBlock *, 16> &Visited) {
  BlockInfo BBLiveIn;
  unsigned NumJoined = 0;
  // For all predecessors of this MBB, find the set of VarLocs that
  // can be joined.
  for (auto I = pred_begin(&BB), E = pred_end(&BB); I != E; I++) {
    // Ignore backedges if we have not visited the predecessor yet. As the
    // predecessor hasn't yet had locations propagated into it, most locations
    // will not yet be valid, so treat them as all being uninitialized and
    // potentially valid. If a location guessed to be correct here is
    // invalidated later, we will remove it when we revisit this block.
    // This is essentially the same as initialising all LocKinds and
    // Assignments to an implicit ⊥ value.
    const BasicBlock *Pred = *I;
    if (!Visited.count(Pred))
      continue;
    auto Out = LiveOut.find(Pred);
    // Do not do the following because it is only correct when our top element
    // is an empty set (assuming we stick with "join" terminology and don't
    // switch to meet). It might be possible to model our dataflow that way
    // somehow, but it's easier to treat join on our BlockInfo like union.  In
    // any case, we always add a LiveOut entry after visiting a block, so this
    // codepath should never fire. Let's assert that now.
    assert(Out != LiveOut.end());
#if 0
    // Join is null in case of empty OutLocs from any of the pred.
    if (Out == LiveOut.end())
      return false;
#endif
    if (NumJoined == 0)
      BBLiveIn = Out->second;
    else
      BBLiveIn = join(BBLiveIn, Out->second);
    NumJoined++;
  }

  auto R = LiveIn.find(&BB);
  // Check if there isn't an entry, or there is but the LiveIn set has changed
  // (expensive check).
  if (R == LiveIn.end() || BBLiveIn != R->second) {
    LiveIn[&BB] = BBLiveIn;
    // A change has occured.
    return true;
  }
  // No change.
  return false;
}

/// Return true if A fully contains B.
static bool fullyContains(DIExpression::FragmentInfo A,
                          DIExpression::FragmentInfo B) {
  auto ALeft = A.OffsetInBits;
  auto BLeft = B.OffsetInBits;
  if (BLeft < ALeft)
    return false;

  auto ARight = ALeft + A.SizeInBits;
  auto BRight = BLeft + B.SizeInBits;
  if (BRight > ARight)
    return false;
  return true;
}

static CoffeeLoweringPass::OverlapMap buildOverlapMap(Function &Fn) {
  DenseSet<DebugVariable> Seen;
  DenseMap<DebugAggregate, SmallVector<DebugVariable, 8>> FragmentMap;
  for (auto &BB : Fn) {
    for (auto &I : BB) {
      if (auto *DII = dyn_cast<DbgVariableIntrinsic>(&I)) {
        DebugVariable DV = getVariable(DII);
        DebugAggregate DA = {DV.getVariable(), DV.getInlinedAt()};
        if (Seen.insert(DV).second)
          FragmentMap[DA].push_back(DV);
      }
    }
  }

  // Sort the fragment map for each DebugAggregate in non-descending
  // order of fragment size. Assert no entries are duplicates.
  for (auto &Pair : FragmentMap) {
    SmallVector<DebugVariable, 8> &Frags = Pair.second;
    std::sort(
        Frags.begin(), Frags.end(), [](DebugVariable Next, DebugVariable Elmt) {
          assert(!(Elmt.getFragmentOrDefault() == Next.getFragmentOrDefault()));
          return Elmt.getFragmentOrDefault().SizeInBits >
                 Next.getFragmentOrDefault().SizeInBits;
        });
  }

  // Build the map.
  CoffeeLoweringPass::OverlapMap Map;
  for (auto Pair : FragmentMap) {
    auto &Frags = Pair.second;
    for (auto It = Frags.begin(), IEnd = Frags.end(); It != IEnd; ++It) {
      DIExpression::FragmentInfo Frag = It->getFragmentOrDefault();
      // Find the frags that this is contained within.
      //
      // Because Frags is sorted by size and none have the same offset and
      // size, we know that this frag can only be contained by subsequent
      // elements.
      SmallVector<DebugVariable, 8>::iterator OtherIt = It;
      ++OtherIt;
      for (; OtherIt != IEnd; ++OtherIt) {
        DIExpression::FragmentInfo OtherFrag = OtherIt->getFragmentOrDefault();
        if (fullyContains(OtherFrag, Frag))
          Map[*OtherIt].push_back(*It);
      }
    }
  }

  return Map;
}

bool CoffeeLoweringPass::run() {
  if (Fn.size() > CoffeeMaxBB) {
    errs() << "[AT] Dropping var locs in: " << Fn.getName()
           << ": too many blocks (" << Fn.size() << ")\n";
    at::deleteAll(&Fn);
    return false;
  }

  // The general structure here is copy-paste-modified from VarLocBasedImp.cpp
  // (LiveDebugValues).

  // Build the variable fragment overlap map.
  // Note that this pass doesn't handle partial overlaps correctly (FWIW
  // neither does LiveDebugVariables) because that is difficult to do and
  // appears to be rare occurance.
  VarContains = buildOverlapMap(Fn);

  // Prepare for traversal.
  //
  ReversePostOrderTraversal<Function *> RPOT(&Fn);
  std::priority_queue<unsigned int, std::vector<unsigned int>,
                      std::greater<unsigned int>>
      Worklist;
  std::priority_queue<unsigned int, std::vector<unsigned int>,
                      std::greater<unsigned int>>
      Pending;
  DenseMap<unsigned int, BasicBlock *> OrderToBB;
  DenseMap<BasicBlock *, unsigned int> BBToOrder;
  { // Init OrderToBB and BBToOrder.
    unsigned int RPONumber = 0;
    for (auto RI = RPOT.begin(), RE = RPOT.end(); RI != RE; ++RI) {
      OrderToBB[RPONumber] = *RI;
      BBToOrder[*RI] = RPONumber;
      Worklist.push(RPONumber);
      ++RPONumber;
    }
  }

  // Perform the traversal.
  //
  // This is a standard "union of predecessor outs" dataflow problem. To solve
  // it, we perform join() and process() using the two worklist method until
  // the LiveIn data for each block becomes unchanging. The "proof" that this
  // terminates can be put together by looking at the comments around LocKind,
  // Assignment, and the various join methods, which show that all the elements
  // involved are made up of join-semilattices; LiveIn(n) can only
  // monotonically increase throughout the dataflow.
  //
  SmallPtrSet<BasicBlock *, 16> Visited;
  while (!Worklist.empty() || !Pending.empty()) {
    // We track what is on the pending worklist to avoid inserting the same
    // thing twice.  We could avoid this with a custom priority queue, but this
    // is probably not worth it.
    SmallPtrSet<BasicBlock *, 16> OnPending;
    COFFEE_DEBUG(dbgs() << "Processing Worklist\n");
    while (!Worklist.empty()) {
      BasicBlock *BB = OrderToBB[Worklist.top()];
      COFFEE_DEBUG(dbgs() << "\nPop BB " << BB->getName() << "\n");
      Worklist.pop();
      bool InChanged = join(*BB, Visited);
      // Always consider LiveIn changed on the first visit.
      InChanged |= Visited.insert(BB).second;
      if (InChanged) {
        COFFEE_DEBUG(dbgs()
                     << BB->getName() << " has new InLocs, process it\n");
        // Mutate a copy of LiveIn while processing BB. Once we've processed
        // the terminator LiveSet is the LiveOut set for BB.
        // This is an expensive copy!
        BlockInfo LiveSet = LiveIn[BB];

        // Process the instructions in the block; apply transfer functions.
        process(*BB, &LiveSet);

        // Relatively expensive check: has anything changed in LiveOut for BB?
        if (LiveOut[BB] != LiveSet) {
          COFFEE_DEBUG(dbgs() << BB->getName()
                              << " has new OutLocs, add succs to worklist: [ ");
          LiveOut[BB] = LiveSet;
          for (auto I = succ_begin(BB), E = succ_end(BB); I != E; I++) {
            if (OnPending.insert(*I).second) {
              COFFEE_DEBUG(dbgs() << I->getName() << " ");
              Pending.push(BBToOrder[*I]);
            }
          }
          COFFEE_DEBUG(dbgs() << "]\n");
        }
      }
    }
    Worklist.swap(Pending);
    // At this point, pending must be empty, since it was just the empty
    // worklist
    assert(Pending.empty() && "Pending should be empty");
  }

  // That's the hard part over. Now we just have some admin to do.

  // Record whether we inserted any intrinsics.
  bool InsertedAnyIntrinsics = false;

  // Insert dbg.declares if possible.
  //
  // Go through all of the dbg.values that we planed to insert. If the
  // aggregate variable it's a part of is not in the NotAlwaysStackHomed set we
  // can insert a dbg.declare for the aggregate instead of inserting the
  // dbg.value fragments. Add an entry to AlwaysStackHomed so that we don't try
  // to insert the dbg.values afterwards too.
  DenseSet<DebugAggregate> AlwaysStackHomed;
  for (auto Pair : InsertBeforeMap) {
    Instruction *InsertBefore = Pair.first;
    auto &MapVec = Pair.second;
    for (auto Pair : MapVec) {
      DebugVariable Var = Pair.first;
      DbgValueInst *DVI = Pair.second;
      DebugAggregate Aggr{Var.getVariable(), Var.getInlinedAt()};
      // Skip this Var if it's not always stack homed.
      if (NotAlwaysStackHomed.contains(Aggr))
        continue;
      // All source assignments to this variable remain and all stores to any
      // part of the variable store to the same address (with varying
      // offsets). We can just emit a dbg.declare for the whole variable.
      //
      // Make a dbg.declare for the aggregate if we haven't already.
      // FIXME: @OCH We should only do this if there's a dbg.assign on the
      // alloca for the full variable (all fragments).
      if (AlwaysStackHomed.insert(Aggr).second) {
        // The first dbg.value we encounter should be one linked to the
        // storage. @OCH: Can we check/assert for this?
        Value *Storage = DVI->getValue();
        DebugLoc DL = DVI->getDebugLoc();
        DIExpression *EmptyExpr = DIExpression::get(DVI->getContext(), None);
        auto *DDI = DIB.insertDeclare(Storage, DVI->getVariable(), EmptyExpr,
                                      DL, InsertBefore);
        (void)DDI;
        COFFEE_DEBUG(errs() << "[EMIT] STACK HOMED: " << *DDI << "\n");
        InsertedAnyIntrinsics = true;
      }
      // Finally, remove the dbg.value that we were going to insert.
      delete DVI;
    }
  }

  // Insert the new dbg.value intrinsics.
  //
  // Here are a couple of helpers to track whether we've seen a non-undef def
  // of a variable yet (see loop body comments).
  DenseMap<DebugAggregate, DenseSet<DIExpression::FragmentInfo>> VarsWithDef;
  auto DefineBits = [&VarsWithDef](DebugAggregate A, DebugVariable V) {
    VarsWithDef[A].insert(V.getFragmentOrDefault());
  };
  auto HasDefinedBits = [&VarsWithDef](DebugAggregate A, DebugVariable V) {
    auto FragsIt = VarsWithDef.find(A);
    if (FragsIt == VarsWithDef.end())
      return false;
    return llvm::any_of(FragsIt->second, [V](auto Frag) {
        return DIExpression::fragmentsOverlap(Frag, V.getFragmentOrDefault());
      });
  };
  // BasicBlock::isEntryBlock doesn't exist in this revision?
  auto isEntryBlock = [](Instruction* Instr) {
    return Instr->getParent() == &Instr->getFunction()->front();
  };
  // dbg.value insertion loop.
  for (auto Pair : InsertBeforeMap) {
    Instruction *InsertBefore = Pair.first;
    auto &MapVec = Pair.second;
    for (auto Pair : MapVec) {
      DebugVariable Var = Pair.first;
      DbgValueInst *DVI = Pair.second;
      DebugAggregate Aggr{Var.getVariable(), Var.getInlinedAt()};
      // If this variable is always stack homed then we have already inserted a
      // dbg.declare and deleted this dbg.value.
      if (AlwaysStackHomed.contains(Aggr))
        continue;
      // There's a final bit of work we need to do to keep SelectionDAG happy
      // and avoid its quirks. If we've not seen a non-undef location for this
      // variable yet, we're in the entry block, and this location is undef,
      // then don't emit the dbg.value.
      //
      // This is to work around the fact that SelectionDAG will hoist
      // dbg.values using argument values to the top of the entry block. This
      // can move arg dbg.values before undef and constant dbg.values that they
      // previously followed. The easiest thing to do is to just try to feed
      // SelectionDAG input it's happy with.
      if (isEntryBlock(InsertBefore) && isa<UndefValue>(DVI->getValue()) &&
          !HasDefinedBits(Aggr, Var)) {
        COFFEE_DEBUG(errs() << "[EMIT] DROP: " << *DVI << "\n");
        delete DVI;
        continue;
      }
      DefineBits(Aggr, Var);
      DVI->insertBefore(InsertBefore);
      COFFEE_DEBUG(errs() << "[EMIT]: " << *DVI << "\n");
      InsertedAnyIntrinsics = true;
    }
  }

  // Finally, delete all traces of the coffee chat method.
  at::deleteAll(&Fn);

  // There's another thing we need to do to keep SelectionDAG happy!  We
  // inserted a lot debug intrinsics; let's clean this up a bit to help
  // SelectionDAG do its best. Motivating example (observed behaviour):
  //
  // For some variable we generate:
  //     %inc = add nuw nsw i32 %i.0128, 1
  //     call void @llvm.dbg.value(metadata i32 undef, ...
  //     call void @llvm.dbg.value(metadata i32 %inc, ...
  //
  // SelectionDAG swaps the dbg.value positions:
  //     %31:gr32 = nuw nsw ADD32ri8 %30:gr32(tied-def 0), 1
  //     DBG_VALUE %31:gr32, ...
  //     DBG_VALUE $noreg, ...
  //
  if (InsertedAnyIntrinsics) {
    for (auto &BB : Fn)
      RemoveRedundantDbgInstrs(&BB);
  }

  return InsertedAnyIntrinsics;
}

static void stripDbgDeclares(Function &Fn) {
  SmallVector<DbgDeclareInst *, 8> ToRemove;
  for (auto &BB : Fn) {
    for (auto &I: BB) {
      if (auto *DDI = dyn_cast<DbgDeclareInst>(&I))
        ToRemove.push_back(DDI);
    }
  }
  for (auto *DDI: ToRemove)
    DDI->eraseFromParent();
}

static void convertDbgAssignsToDbgValues(Function &Fn,
                                         const DataLayout &Layout) {
  CoffeeLoweringPass Pass(Fn, Layout);
  if (Pass.run()) {
    if (PrintBeforeFragAgg && isFunctionInPrintList(Fn.getName()))
      errs() << "IR before AggregateFragmentsPass on " << Fn.getName() << "\n"
             << Fn << "\n";
    AggregateFragmentsPass FragsCleanup(Fn, Layout);
    FragsCleanup.run();
  }
}

bool SelectionDAGISel::runOnMachineFunction(MachineFunction &mf) {
  // If we already selected that function, we do not need to run SDISel.
  if (mf.getProperties().hasProperty(
          MachineFunctionProperties::Property::Selected))
    return false;
  // Do some sanity-checking on the command-line options.
  assert((!EnableFastISelAbort || TM.Options.EnableFastISel) &&
         "-fast-isel-abort > 0 requires -fast-isel");

  const Function &Fn = mf.getFunction();
  MF = &mf;

  // Decide what flavour of variable location debug-info will be used, before
  // we change the optimisation level.
  UseInstrRefDebugInfo = mf.useDebugInstrRef();
  CurDAG->useInstrRefDebugInfo(UseInstrRefDebugInfo);

  // Reset the target options before resetting the optimization
  // level below.
  // FIXME: This is a horrible hack and should be processed via
  // codegen looking at the optimization level explicitly when
  // it wants to look at it.
  TM.resetTargetOptions(Fn);
  // Reset OptLevel to None for optnone functions.
  CodeGenOpt::Level NewOptLevel = OptLevel;
  if (OptLevel != CodeGenOpt::None && skipFunction(Fn))
    NewOptLevel = CodeGenOpt::None;
  OptLevelChanger OLC(*this, NewOptLevel);

  TII = MF->getSubtarget().getInstrInfo();
  TLI = MF->getSubtarget().getTargetLowering();
  RegInfo = &MF->getRegInfo();
  LibInfo = &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(Fn);
  GFI = Fn.hasGC() ? &getAnalysis<GCModuleInfo>().getFunctionInfo(Fn) : nullptr;
  ORE = std::make_unique<OptimizationRemarkEmitter>(&Fn);
  auto *DTWP = getAnalysisIfAvailable<DominatorTreeWrapperPass>();
  DominatorTree *DT = DTWP ? &DTWP->getDomTree() : nullptr;
  auto *LIWP = getAnalysisIfAvailable<LoopInfoWrapperPass>();
  LoopInfo *LI = LIWP ? &LIWP->getLoopInfo() : nullptr;
  auto *PSI = &getAnalysis<ProfileSummaryInfoWrapperPass>().getPSI();
  BlockFrequencyInfo *BFI = nullptr;
  if (PSI && PSI->hasProfileSummary() && OptLevel != CodeGenOpt::None)
    BFI = &getAnalysis<LazyBlockFrequencyInfoPass>().getBFI();

  LLVM_DEBUG(dbgs() << "\n\n\n=== " << Fn.getName() << "\n");
  bool FnInPrintList = isFunctionInPrintList(Fn.getName());
  if ((PrintWrapCoffee || PrintBeforeCoffee) && FnInPrintList)
    errs() << "*** IR Dump Before Coffee Lowering on " << Fn.getName() << " ***\n" << Fn;
  convertDbgAssignsToDbgValues(const_cast<Function &>(Fn), MF->getDataLayout());
  if (StripStackVars)
    stripDbgDeclares(const_cast<Function &>(Fn));
  if ((PrintWrapCoffee || PrintAfterCoffee) && FnInPrintList)
    errs() << "*** IR Dump After Coffee Lowering on " << Fn.getName() << " ***\n" << Fn;
  SplitCriticalSideEffectEdges(const_cast<Function &>(Fn), DT, LI);

  CurDAG->init(*MF, *ORE, this, LibInfo,
               getAnalysisIfAvailable<LegacyDivergenceAnalysis>(), PSI, BFI);
  FuncInfo->set(Fn, *MF, CurDAG);
  SwiftError->setFunction(*MF);

  // Now get the optional analyzes if we want to.
  // This is based on the possibly changed OptLevel (after optnone is taken
  // into account).  That's unfortunate but OK because it just means we won't
  // ask for passes that have been required anyway.

  if (UseMBPI && OptLevel != CodeGenOpt::None)
    FuncInfo->BPI = &getAnalysis<BranchProbabilityInfoWrapperPass>().getBPI();
  else
    FuncInfo->BPI = nullptr;

  if (OptLevel != CodeGenOpt::None)
    AA = &getAnalysis<AAResultsWrapperPass>().getAAResults();
  else
    AA = nullptr;

  SDB->init(GFI, AA, LibInfo);

  MF->setHasInlineAsm(false);

  FuncInfo->SplitCSR = false;

  // We split CSR if the target supports it for the given function
  // and the function has only return exits.
  if (OptLevel != CodeGenOpt::None && TLI->supportSplitCSR(MF)) {
    FuncInfo->SplitCSR = true;

    // Collect all the return blocks.
    for (const BasicBlock &BB : Fn) {
      if (!succ_empty(&BB))
        continue;

      const Instruction *Term = BB.getTerminator();
      if (isa<UnreachableInst>(Term) || isa<ReturnInst>(Term))
        continue;

      // Bail out if the exit block is not Return nor Unreachable.
      FuncInfo->SplitCSR = false;
      break;
    }
  }

  MachineBasicBlock *EntryMBB = &MF->front();
  if (FuncInfo->SplitCSR)
    // This performs initialization so lowering for SplitCSR will be correct.
    TLI->initializeSplitCSR(EntryMBB);

  SelectAllBasicBlocks(Fn);
  if (FastISelFailed && EnableFastISelFallbackReport) {
    DiagnosticInfoISelFallback DiagFallback(Fn);
    Fn.getContext().diagnose(DiagFallback);
  }

  // Replace forward-declared registers with the registers containing
  // the desired value.
  // Note: it is important that this happens **before** the call to
  // EmitLiveInCopies, since implementations can skip copies of unused
  // registers. If we don't apply the reg fixups before, some registers may
  // appear as unused and will be skipped, resulting in bad MI.
  MachineRegisterInfo &MRI = MF->getRegInfo();
  for (DenseMap<Register, Register>::iterator I = FuncInfo->RegFixups.begin(),
                                              E = FuncInfo->RegFixups.end();
       I != E; ++I) {
    Register From = I->first;
    Register To = I->second;
    // If To is also scheduled to be replaced, find what its ultimate
    // replacement is.
    while (true) {
      DenseMap<Register, Register>::iterator J = FuncInfo->RegFixups.find(To);
      if (J == E)
        break;
      To = J->second;
    }
    // Make sure the new register has a sufficiently constrained register class.
    if (Register::isVirtualRegister(From) && Register::isVirtualRegister(To))
      MRI.constrainRegClass(To, MRI.getRegClass(From));
    // Replace it.

    // Replacing one register with another won't touch the kill flags.
    // We need to conservatively clear the kill flags as a kill on the old
    // register might dominate existing uses of the new register.
    if (!MRI.use_empty(To))
      MRI.clearKillFlags(From);
    MRI.replaceRegWith(From, To);
  }

  // If the first basic block in the function has live ins that need to be
  // copied into vregs, emit the copies into the top of the block before
  // emitting the code for the block.
  const TargetRegisterInfo &TRI = *MF->getSubtarget().getRegisterInfo();
  RegInfo->EmitLiveInCopies(EntryMBB, TRI, *TII);

  // Insert copies in the entry block and the return blocks.
  if (FuncInfo->SplitCSR) {
    SmallVector<MachineBasicBlock*, 4> Returns;
    // Collect all the return blocks.
    for (MachineBasicBlock &MBB : mf) {
      if (!MBB.succ_empty())
        continue;

      MachineBasicBlock::iterator Term = MBB.getFirstTerminator();
      if (Term != MBB.end() && Term->isReturn()) {
        Returns.push_back(&MBB);
        continue;
      }
    }
    TLI->insertCopiesSplitCSR(EntryMBB, Returns);
  }

  DenseMap<unsigned, unsigned> LiveInMap;
  if (!FuncInfo->ArgDbgValues.empty())
    for (std::pair<unsigned, unsigned> LI : RegInfo->liveins())
      if (LI.second)
        LiveInMap.insert(LI);

  // Insert DBG_VALUE instructions for function arguments to the entry block.
  bool InstrRef = MF->useDebugInstrRef();
  for (unsigned i = 0, e = FuncInfo->ArgDbgValues.size(); i != e; ++i) {
    MachineInstr *MI = FuncInfo->ArgDbgValues[e - i - 1];
    assert(MI->getOpcode() != TargetOpcode::DBG_VALUE_LIST &&
           "Function parameters should not be described by DBG_VALUE_LIST.");
    bool hasFI = MI->getOperand(0).isFI();
    Register Reg =
        hasFI ? TRI.getFrameRegister(*MF) : MI->getOperand(0).getReg();
    if (Register::isPhysicalRegister(Reg))
      EntryMBB->insert(EntryMBB->begin(), MI);
    else {
      MachineInstr *Def = RegInfo->getVRegDef(Reg);
      if (Def) {
        MachineBasicBlock::iterator InsertPos = Def;
        // FIXME: VR def may not be in entry block.
        Def->getParent()->insert(std::next(InsertPos), MI);
      } else
        LLVM_DEBUG(dbgs() << "Dropping debug info for dead vreg"
                          << Register::virtReg2Index(Reg) << "\n");
    }

    // Don't try and extend through copies in instruction referencing mode.
    if (InstrRef)
      continue;

    // If Reg is live-in then update debug info to track its copy in a vreg.
    DenseMap<unsigned, unsigned>::iterator LDI = LiveInMap.find(Reg);
    if (LDI != LiveInMap.end()) {
      assert(!hasFI && "There's no handling of frame pointer updating here yet "
                       "- add if needed");
      MachineInstr *Def = RegInfo->getVRegDef(LDI->second);
      MachineBasicBlock::iterator InsertPos = Def;
      const MDNode *Variable = MI->getDebugVariable();
      const MDNode *Expr = MI->getDebugExpression();
      DebugLoc DL = MI->getDebugLoc();
      bool IsIndirect = MI->isIndirectDebugValue();
      if (IsIndirect)
        assert(MI->getOperand(1).getImm() == 0 &&
               "DBG_VALUE with nonzero offset");
      assert(cast<DILocalVariable>(Variable)->isValidLocationForIntrinsic(DL) &&
             "Expected inlined-at fields to agree");
      assert(MI->getOpcode() != TargetOpcode::DBG_VALUE_LIST &&
             "Didn't expect to see a DBG_VALUE_LIST here");
      // Def is never a terminator here, so it is ok to increment InsertPos.
      BuildMI(*EntryMBB, ++InsertPos, DL, TII->get(TargetOpcode::DBG_VALUE),
              IsIndirect, LDI->second, Variable, Expr);

      // If this vreg is directly copied into an exported register then
      // that COPY instructions also need DBG_VALUE, if it is the only
      // user of LDI->second.
      MachineInstr *CopyUseMI = nullptr;
      for (MachineRegisterInfo::use_instr_iterator
           UI = RegInfo->use_instr_begin(LDI->second),
           E = RegInfo->use_instr_end(); UI != E; ) {
        MachineInstr *UseMI = &*(UI++);
        if (UseMI->isDebugValue()) continue;
        if (UseMI->isCopy() && !CopyUseMI && UseMI->getParent() == EntryMBB) {
          CopyUseMI = UseMI; continue;
        }
        // Otherwise this is another use or second copy use.
        CopyUseMI = nullptr; break;
      }
      if (CopyUseMI &&
          TRI.getRegSizeInBits(LDI->second, MRI) ==
              TRI.getRegSizeInBits(CopyUseMI->getOperand(0).getReg(), MRI)) {
        // Use MI's debug location, which describes where Variable was
        // declared, rather than whatever is attached to CopyUseMI.
        MachineInstr *NewMI =
            BuildMI(*MF, DL, TII->get(TargetOpcode::DBG_VALUE), IsIndirect,
                    CopyUseMI->getOperand(0).getReg(), Variable, Expr);
        MachineBasicBlock::iterator Pos = CopyUseMI;
        EntryMBB->insertAfter(Pos, NewMI);
      }
    }
  }

  // For debug-info, in instruction referencing mode, we need to perform some
  // post-isel maintenence.
  if (UseInstrRefDebugInfo)
    MF->finalizeDebugInstrRefs();

  // Determine if there are any calls in this machine function.
  MachineFrameInfo &MFI = MF->getFrameInfo();
  for (const auto &MBB : *MF) {
    if (MFI.hasCalls() && MF->hasInlineAsm())
      break;

    for (const auto &MI : MBB) {
      const MCInstrDesc &MCID = TII->get(MI.getOpcode());
      if ((MCID.isCall() && !MCID.isReturn()) ||
          MI.isStackAligningInlineAsm()) {
        MFI.setHasCalls(true);
      }
      if (MI.isInlineAsm()) {
        MF->setHasInlineAsm(true);
      }
    }
  }

  // Determine if there is a call to setjmp in the machine function.
  MF->setExposesReturnsTwice(Fn.callsFunctionThatReturnsTwice());

  // Determine if floating point is used for msvc
  computeUsesMSVCFloatingPoint(TM.getTargetTriple(), Fn, MF->getMMI());

  // Release function-specific state. SDB and CurDAG are already cleared
  // at this point.
  FuncInfo->clear();

  LLVM_DEBUG(dbgs() << "*** MachineFunction at end of ISel ***\n");
  LLVM_DEBUG(MF->print(dbgs()));

  return true;
}

static void reportFastISelFailure(MachineFunction &MF,
                                  OptimizationRemarkEmitter &ORE,
                                  OptimizationRemarkMissed &R,
                                  bool ShouldAbort) {
  // Print the function name explicitly if we don't have a debug location (which
  // makes the diagnostic less useful) or if we're going to emit a raw error.
  if (!R.getLocation().isValid() || ShouldAbort)
    R << (" (in function: " + MF.getName() + ")").str();

  if (ShouldAbort)
    report_fatal_error(Twine(R.getMsg()));

  ORE.emit(R);
}

void SelectionDAGISel::SelectBasicBlock(BasicBlock::const_iterator Begin,
                                        BasicBlock::const_iterator End,
                                        bool &HadTailCall) {
  // Allow creating illegal types during DAG building for the basic block.
  CurDAG->NewNodesMustHaveLegalTypes = false;

  // Lower the instructions. If a call is emitted as a tail call, cease emitting
  // nodes for this block.
  for (BasicBlock::const_iterator I = Begin; I != End && !SDB->HasTailCall; ++I) {
    if (!ElidedArgCopyInstrs.count(&*I))
      SDB->visit(*I);
  }

  // Make sure the root of the DAG is up-to-date.
  CurDAG->setRoot(SDB->getControlRoot());
  HadTailCall = SDB->HasTailCall;
  SDB->resolveOrClearDbgInfo();
  SDB->clear();

  // Final step, emit the lowered DAG as machine code.
  CodeGenAndEmitDAG();
}

void SelectionDAGISel::ComputeLiveOutVRegInfo() {
  SmallPtrSet<SDNode *, 16> Added;
  SmallVector<SDNode*, 128> Worklist;

  Worklist.push_back(CurDAG->getRoot().getNode());
  Added.insert(CurDAG->getRoot().getNode());

  KnownBits Known;

  do {
    SDNode *N = Worklist.pop_back_val();

    // Otherwise, add all chain operands to the worklist.
    for (const SDValue &Op : N->op_values())
      if (Op.getValueType() == MVT::Other && Added.insert(Op.getNode()).second)
        Worklist.push_back(Op.getNode());

    // If this is a CopyToReg with a vreg dest, process it.
    if (N->getOpcode() != ISD::CopyToReg)
      continue;

    unsigned DestReg = cast<RegisterSDNode>(N->getOperand(1))->getReg();
    if (!Register::isVirtualRegister(DestReg))
      continue;

    // Ignore non-integer values.
    SDValue Src = N->getOperand(2);
    EVT SrcVT = Src.getValueType();
    if (!SrcVT.isInteger())
      continue;

    unsigned NumSignBits = CurDAG->ComputeNumSignBits(Src);
    Known = CurDAG->computeKnownBits(Src);
    FuncInfo->AddLiveOutRegInfo(DestReg, NumSignBits, Known);
  } while (!Worklist.empty());
}

void SelectionDAGISel::CodeGenAndEmitDAG() {
  StringRef GroupName = "sdag";
  StringRef GroupDescription = "Instruction Selection and Scheduling";
  std::string BlockName;
  bool MatchFilterBB = false; (void)MatchFilterBB;
#ifndef NDEBUG
  TargetTransformInfo &TTI =
      getAnalysis<TargetTransformInfoWrapperPass>().getTTI(*FuncInfo->Fn);
#endif

  // Pre-type legalization allow creation of any node types.
  CurDAG->NewNodesMustHaveLegalTypes = false;

#ifndef NDEBUG
  MatchFilterBB = (FilterDAGBasicBlockName.empty() ||
                   FilterDAGBasicBlockName ==
                       FuncInfo->MBB->getBasicBlock()->getName());
#endif
#ifdef NDEBUG
  if (ViewDAGCombine1 || ViewLegalizeTypesDAGs || ViewDAGCombineLT ||
      ViewLegalizeDAGs || ViewDAGCombine2 || ViewISelDAGs || ViewSchedDAGs ||
      ViewSUnitDAGs)
#endif
  {
    BlockName =
        (MF->getName() + ":" + FuncInfo->MBB->getBasicBlock()->getName()).str();
  }
  LLVM_DEBUG(dbgs() << "Initial selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

#ifndef NDEBUG
  if (TTI.hasBranchDivergence())
    CurDAG->VerifyDAGDivergence();
#endif

  if (ViewDAGCombine1 && MatchFilterBB)
    CurDAG->viewGraph("dag-combine1 input for " + BlockName);

  // Run the DAG combiner in pre-legalize mode.
  {
    NamedRegionTimer T("combine1", "DAG Combining 1", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    CurDAG->Combine(BeforeLegalizeTypes, AA, OptLevel);
  }

  LLVM_DEBUG(dbgs() << "Optimized lowered selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

#ifndef NDEBUG
  if (TTI.hasBranchDivergence())
    CurDAG->VerifyDAGDivergence();
#endif

  // Second step, hack on the DAG until it only uses operations and types that
  // the target supports.
  if (ViewLegalizeTypesDAGs && MatchFilterBB)
    CurDAG->viewGraph("legalize-types input for " + BlockName);

  bool Changed;
  {
    NamedRegionTimer T("legalize_types", "Type Legalization", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    Changed = CurDAG->LegalizeTypes();
  }

  LLVM_DEBUG(dbgs() << "Type-legalized selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

#ifndef NDEBUG
  if (TTI.hasBranchDivergence())
    CurDAG->VerifyDAGDivergence();
#endif

  // Only allow creation of legal node types.
  CurDAG->NewNodesMustHaveLegalTypes = true;

  if (Changed) {
    if (ViewDAGCombineLT && MatchFilterBB)
      CurDAG->viewGraph("dag-combine-lt input for " + BlockName);

    // Run the DAG combiner in post-type-legalize mode.
    {
      NamedRegionTimer T("combine_lt", "DAG Combining after legalize types",
                         GroupName, GroupDescription, TimePassesIsEnabled);
      CurDAG->Combine(AfterLegalizeTypes, AA, OptLevel);
    }

    LLVM_DEBUG(dbgs() << "Optimized type-legalized selection DAG: "
                      << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                      << "'\n";
               CurDAG->dump());

#ifndef NDEBUG
    if (TTI.hasBranchDivergence())
      CurDAG->VerifyDAGDivergence();
#endif
  }

  {
    NamedRegionTimer T("legalize_vec", "Vector Legalization", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    Changed = CurDAG->LegalizeVectors();
  }

  if (Changed) {
    LLVM_DEBUG(dbgs() << "Vector-legalized selection DAG: "
                      << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                      << "'\n";
               CurDAG->dump());

#ifndef NDEBUG
    if (TTI.hasBranchDivergence())
      CurDAG->VerifyDAGDivergence();
#endif

    {
      NamedRegionTimer T("legalize_types2", "Type Legalization 2", GroupName,
                         GroupDescription, TimePassesIsEnabled);
      CurDAG->LegalizeTypes();
    }

    LLVM_DEBUG(dbgs() << "Vector/type-legalized selection DAG: "
                      << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                      << "'\n";
               CurDAG->dump());

#ifndef NDEBUG
    if (TTI.hasBranchDivergence())
      CurDAG->VerifyDAGDivergence();
#endif

    if (ViewDAGCombineLT && MatchFilterBB)
      CurDAG->viewGraph("dag-combine-lv input for " + BlockName);

    // Run the DAG combiner in post-type-legalize mode.
    {
      NamedRegionTimer T("combine_lv", "DAG Combining after legalize vectors",
                         GroupName, GroupDescription, TimePassesIsEnabled);
      CurDAG->Combine(AfterLegalizeVectorOps, AA, OptLevel);
    }

    LLVM_DEBUG(dbgs() << "Optimized vector-legalized selection DAG: "
                      << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                      << "'\n";
               CurDAG->dump());

#ifndef NDEBUG
    if (TTI.hasBranchDivergence())
      CurDAG->VerifyDAGDivergence();
#endif
  }

  if (ViewLegalizeDAGs && MatchFilterBB)
    CurDAG->viewGraph("legalize input for " + BlockName);

  {
    NamedRegionTimer T("legalize", "DAG Legalization", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    CurDAG->Legalize();
  }

  LLVM_DEBUG(dbgs() << "Legalized selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

#ifndef NDEBUG
  if (TTI.hasBranchDivergence())
    CurDAG->VerifyDAGDivergence();
#endif

  if (ViewDAGCombine2 && MatchFilterBB)
    CurDAG->viewGraph("dag-combine2 input for " + BlockName);

  // Run the DAG combiner in post-legalize mode.
  {
    NamedRegionTimer T("combine2", "DAG Combining 2", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    CurDAG->Combine(AfterLegalizeDAG, AA, OptLevel);
  }

  LLVM_DEBUG(dbgs() << "Optimized legalized selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

#ifndef NDEBUG
  if (TTI.hasBranchDivergence())
    CurDAG->VerifyDAGDivergence();
#endif

  if (OptLevel != CodeGenOpt::None)
    ComputeLiveOutVRegInfo();

  if (ViewISelDAGs && MatchFilterBB)
    CurDAG->viewGraph("isel input for " + BlockName);

  // Third, instruction select all of the operations to machine code, adding the
  // code to the MachineBasicBlock.
  {
    NamedRegionTimer T("isel", "Instruction Selection", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    DoInstructionSelection();
  }

  LLVM_DEBUG(dbgs() << "Selected selection DAG: "
                    << printMBBReference(*FuncInfo->MBB) << " '" << BlockName
                    << "'\n";
             CurDAG->dump());

  if (ViewSchedDAGs && MatchFilterBB)
    CurDAG->viewGraph("scheduler input for " + BlockName);

  // Schedule machine code.
  ScheduleDAGSDNodes *Scheduler = CreateScheduler();
  {
    NamedRegionTimer T("sched", "Instruction Scheduling", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    Scheduler->Run(CurDAG, FuncInfo->MBB);
  }

  if (ViewSUnitDAGs && MatchFilterBB)
    Scheduler->viewGraph();

  // Emit machine code to BB.  This can change 'BB' to the last block being
  // inserted into.
  MachineBasicBlock *FirstMBB = FuncInfo->MBB, *LastMBB;
  {
    NamedRegionTimer T("emit", "Instruction Creation", GroupName,
                       GroupDescription, TimePassesIsEnabled);

    // FuncInfo->InsertPt is passed by reference and set to the end of the
    // scheduled instructions.
    LastMBB = FuncInfo->MBB = Scheduler->EmitSchedule(FuncInfo->InsertPt);
  }

  // If the block was split, make sure we update any references that are used to
  // update PHI nodes later on.
  if (FirstMBB != LastMBB)
    SDB->UpdateSplitBlock(FirstMBB, LastMBB);

  // Free the scheduler state.
  {
    NamedRegionTimer T("cleanup", "Instruction Scheduling Cleanup", GroupName,
                       GroupDescription, TimePassesIsEnabled);
    delete Scheduler;
  }

  // Free the SelectionDAG state, now that we're finished with it.
  CurDAG->clear();
}

namespace {

/// ISelUpdater - helper class to handle updates of the instruction selection
/// graph.
class ISelUpdater : public SelectionDAG::DAGUpdateListener {
  SelectionDAG::allnodes_iterator &ISelPosition;

public:
  ISelUpdater(SelectionDAG &DAG, SelectionDAG::allnodes_iterator &isp)
    : SelectionDAG::DAGUpdateListener(DAG), ISelPosition(isp) {}

  /// NodeDeleted - Handle nodes deleted from the graph. If the node being
  /// deleted is the current ISelPosition node, update ISelPosition.
  ///
  void NodeDeleted(SDNode *N, SDNode *E) override {
    if (ISelPosition == SelectionDAG::allnodes_iterator(N))
      ++ISelPosition;
  }
};

} // end anonymous namespace

// This function is used to enforce the topological node id property
// leveraged during instruction selection. Before the selection process all
// nodes are given a non-negative id such that all nodes have a greater id than
// their operands. As this holds transitively we can prune checks that a node N
// is a predecessor of M another by not recursively checking through M's
// operands if N's ID is larger than M's ID. This significantly improves
// performance of various legality checks (e.g. IsLegalToFold / UpdateChains).

// However, when we fuse multiple nodes into a single node during the
// selection we may induce a predecessor relationship between inputs and
// outputs of distinct nodes being merged, violating the topological property.
// Should a fused node have a successor which has yet to be selected,
// our legality checks would be incorrect. To avoid this we mark all unselected
// successor nodes, i.e. id != -1, as invalid for pruning by bit-negating (x =>
// (-(x+1))) the ids and modify our pruning check to ignore negative Ids of M.
// We use bit-negation to more clearly enforce that node id -1 can only be
// achieved by selected nodes. As the conversion is reversable to the original
// Id, topological pruning can still be leveraged when looking for unselected
// nodes. This method is called internally in all ISel replacement related
// functions.
void SelectionDAGISel::EnforceNodeIdInvariant(SDNode *Node) {
  SmallVector<SDNode *, 4> Nodes;
  Nodes.push_back(Node);

  while (!Nodes.empty()) {
    SDNode *N = Nodes.pop_back_val();
    for (auto *U : N->uses()) {
      auto UId = U->getNodeId();
      if (UId > 0) {
        InvalidateNodeId(U);
        Nodes.push_back(U);
      }
    }
  }
}

// InvalidateNodeId - As explained in EnforceNodeIdInvariant, mark a
// NodeId with the equivalent node id which is invalid for topological
// pruning.
void SelectionDAGISel::InvalidateNodeId(SDNode *N) {
  int InvalidId = -(N->getNodeId() + 1);
  N->setNodeId(InvalidId);
}

// getUninvalidatedNodeId - get original uninvalidated node id.
int SelectionDAGISel::getUninvalidatedNodeId(SDNode *N) {
  int Id = N->getNodeId();
  if (Id < -1)
    return -(Id + 1);
  return Id;
}

void SelectionDAGISel::DoInstructionSelection() {
  LLVM_DEBUG(dbgs() << "===== Instruction selection begins: "
                    << printMBBReference(*FuncInfo->MBB) << " '"
                    << FuncInfo->MBB->getName() << "'\n");

  PreprocessISelDAG();

  // Select target instructions for the DAG.
  {
    // Number all nodes with a topological order and set DAGSize.
    DAGSize = CurDAG->AssignTopologicalOrder();

    // Create a dummy node (which is not added to allnodes), that adds
    // a reference to the root node, preventing it from being deleted,
    // and tracking any changes of the root.
    HandleSDNode Dummy(CurDAG->getRoot());
    SelectionDAG::allnodes_iterator ISelPosition (CurDAG->getRoot().getNode());
    ++ISelPosition;

    // Make sure that ISelPosition gets properly updated when nodes are deleted
    // in calls made from this function.
    ISelUpdater ISU(*CurDAG, ISelPosition);

    // The AllNodes list is now topological-sorted. Visit the
    // nodes by starting at the end of the list (the root of the
    // graph) and preceding back toward the beginning (the entry
    // node).
    while (ISelPosition != CurDAG->allnodes_begin()) {
      SDNode *Node = &*--ISelPosition;
      // Skip dead nodes. DAGCombiner is expected to eliminate all dead nodes,
      // but there are currently some corner cases that it misses. Also, this
      // makes it theoretically possible to disable the DAGCombiner.
      if (Node->use_empty())
        continue;

#ifndef NDEBUG
      SmallVector<SDNode *, 4> Nodes;
      Nodes.push_back(Node);

      while (!Nodes.empty()) {
        auto N = Nodes.pop_back_val();
        if (N->getOpcode() == ISD::TokenFactor || N->getNodeId() < 0)
          continue;
        for (const SDValue &Op : N->op_values()) {
          if (Op->getOpcode() == ISD::TokenFactor)
            Nodes.push_back(Op.getNode());
          else {
            // We rely on topological ordering of node ids for checking for
            // cycles when fusing nodes during selection. All unselected nodes
            // successors of an already selected node should have a negative id.
            // This assertion will catch such cases. If this assertion triggers
            // it is likely you using DAG-level Value/Node replacement functions
            // (versus equivalent ISEL replacement) in backend-specific
            // selections. See comment in EnforceNodeIdInvariant for more
            // details.
            assert(Op->getNodeId() != -1 &&
                   "Node has already selected predecessor node");
          }
        }
      }
#endif

      // When we are using non-default rounding modes or FP exception behavior
      // FP operations are represented by StrictFP pseudo-operations.  For
      // targets that do not (yet) understand strict FP operations directly,
      // we convert them to normal FP opcodes instead at this point.  This
      // will allow them to be handled by existing target-specific instruction
      // selectors.
      if (!TLI->isStrictFPEnabled() && Node->isStrictFPOpcode()) {
        // For some opcodes, we need to call TLI->getOperationAction using
        // the first operand type instead of the result type.  Note that this
        // must match what SelectionDAGLegalize::LegalizeOp is doing.
        EVT ActionVT;
        switch (Node->getOpcode()) {
        case ISD::STRICT_SINT_TO_FP:
        case ISD::STRICT_UINT_TO_FP:
        case ISD::STRICT_LRINT:
        case ISD::STRICT_LLRINT:
        case ISD::STRICT_LROUND:
        case ISD::STRICT_LLROUND:
        case ISD::STRICT_FSETCC:
        case ISD::STRICT_FSETCCS:
          ActionVT = Node->getOperand(1).getValueType();
          break;
        default:
          ActionVT = Node->getValueType(0);
          break;
        }
        if (TLI->getOperationAction(Node->getOpcode(), ActionVT)
            == TargetLowering::Expand)
          Node = CurDAG->mutateStrictFPToFP(Node);
      }

      LLVM_DEBUG(dbgs() << "\nISEL: Starting selection on root node: ";
                 Node->dump(CurDAG));

      Select(Node);
    }

    CurDAG->setRoot(Dummy.getValue());
  }

  LLVM_DEBUG(dbgs() << "\n===== Instruction selection ends:\n");

  PostprocessISelDAG();
}

static bool hasExceptionPointerOrCodeUser(const CatchPadInst *CPI) {
  for (const User *U : CPI->users()) {
    if (const IntrinsicInst *EHPtrCall = dyn_cast<IntrinsicInst>(U)) {
      Intrinsic::ID IID = EHPtrCall->getIntrinsicID();
      if (IID == Intrinsic::eh_exceptionpointer ||
          IID == Intrinsic::eh_exceptioncode)
        return true;
    }
  }
  return false;
}

// wasm.landingpad.index intrinsic is for associating a landing pad index number
// with a catchpad instruction. Retrieve the landing pad index in the intrinsic
// and store the mapping in the function.
static void mapWasmLandingPadIndex(MachineBasicBlock *MBB,
                                   const CatchPadInst *CPI) {
  MachineFunction *MF = MBB->getParent();
  // In case of single catch (...), we don't emit LSDA, so we don't need
  // this information.
  bool IsSingleCatchAllClause =
      CPI->getNumArgOperands() == 1 &&
      cast<Constant>(CPI->getArgOperand(0))->isNullValue();
  // cathchpads for longjmp use an empty type list, e.g. catchpad within %0 []
  // and they don't need LSDA info
  bool IsCatchLongjmp = CPI->getNumArgOperands() == 0;
  if (!IsSingleCatchAllClause && !IsCatchLongjmp) {
    // Create a mapping from landing pad label to landing pad index.
    bool IntrFound = false;
    for (const User *U : CPI->users()) {
      if (const auto *Call = dyn_cast<IntrinsicInst>(U)) {
        Intrinsic::ID IID = Call->getIntrinsicID();
        if (IID == Intrinsic::wasm_landingpad_index) {
          Value *IndexArg = Call->getArgOperand(1);
          int Index = cast<ConstantInt>(IndexArg)->getZExtValue();
          MF->setWasmLandingPadIndex(MBB, Index);
          IntrFound = true;
          break;
        }
      }
    }
    assert(IntrFound && "wasm.landingpad.index intrinsic not found!");
    (void)IntrFound;
  }
}

/// PrepareEHLandingPad - Emit an EH_LABEL, set up live-in registers, and
/// do other setup for EH landing-pad blocks.
bool SelectionDAGISel::PrepareEHLandingPad() {
  MachineBasicBlock *MBB = FuncInfo->MBB;
  const Constant *PersonalityFn = FuncInfo->Fn->getPersonalityFn();
  const BasicBlock *LLVMBB = MBB->getBasicBlock();
  const TargetRegisterClass *PtrRC =
      TLI->getRegClassFor(TLI->getPointerTy(CurDAG->getDataLayout()));

  auto Pers = classifyEHPersonality(PersonalityFn);

  // Catchpads have one live-in register, which typically holds the exception
  // pointer or code.
  if (isFuncletEHPersonality(Pers)) {
    if (const auto *CPI = dyn_cast<CatchPadInst>(LLVMBB->getFirstNonPHI())) {
      if (hasExceptionPointerOrCodeUser(CPI)) {
        // Get or create the virtual register to hold the pointer or code.  Mark
        // the live in physreg and copy into the vreg.
        MCPhysReg EHPhysReg = TLI->getExceptionPointerRegister(PersonalityFn);
        assert(EHPhysReg && "target lacks exception pointer register");
        MBB->addLiveIn(EHPhysReg);
        unsigned VReg = FuncInfo->getCatchPadExceptionPointerVReg(CPI, PtrRC);
        BuildMI(*MBB, FuncInfo->InsertPt, SDB->getCurDebugLoc(),
                TII->get(TargetOpcode::COPY), VReg)
            .addReg(EHPhysReg, RegState::Kill);
      }
    }
    return true;
  }

  // Add a label to mark the beginning of the landing pad.  Deletion of the
  // landing pad can thus be detected via the MachineModuleInfo.
  MCSymbol *Label = MF->addLandingPad(MBB);

  const MCInstrDesc &II = TII->get(TargetOpcode::EH_LABEL);
  BuildMI(*MBB, FuncInfo->InsertPt, SDB->getCurDebugLoc(), II)
    .addSym(Label);

  // If the unwinder does not preserve all registers, ensure that the
  // function marks the clobbered registers as used.
  const TargetRegisterInfo &TRI = *MF->getSubtarget().getRegisterInfo();
  if (auto *RegMask = TRI.getCustomEHPadPreservedMask(*MF))
    MF->getRegInfo().addPhysRegsUsedFromRegMask(RegMask);

  if (Pers == EHPersonality::Wasm_CXX) {
    if (const auto *CPI = dyn_cast<CatchPadInst>(LLVMBB->getFirstNonPHI()))
      mapWasmLandingPadIndex(MBB, CPI);
  } else {
    // Assign the call site to the landing pad's begin label.
    MF->setCallSiteLandingPad(Label, SDB->LPadToCallSiteMap[MBB]);
    // Mark exception register as live in.
    if (unsigned Reg = TLI->getExceptionPointerRegister(PersonalityFn))
      FuncInfo->ExceptionPointerVirtReg = MBB->addLiveIn(Reg, PtrRC);
    // Mark exception selector register as live in.
    if (unsigned Reg = TLI->getExceptionSelectorRegister(PersonalityFn))
      FuncInfo->ExceptionSelectorVirtReg = MBB->addLiveIn(Reg, PtrRC);
  }

  return true;
}

/// isFoldedOrDeadInstruction - Return true if the specified instruction is
/// side-effect free and is either dead or folded into a generated instruction.
/// Return false if it needs to be emitted.
static bool isFoldedOrDeadInstruction(const Instruction *I,
                                      const FunctionLoweringInfo &FuncInfo) {
  return !I->mayWriteToMemory() && // Side-effecting instructions aren't folded.
         !I->isTerminator() &&     // Terminators aren't folded.
         !isa<DbgInfoIntrinsic>(I) && // Debug instructions aren't folded.
         !I->isEHPad() &&             // EH pad instructions aren't folded.
         !FuncInfo.isExportedInst(I); // Exported instrs must be computed.
}

/// Collect llvm.dbg.declare information. This is done after argument lowering
/// in case the declarations refer to arguments.
static void processDbgDeclares(FunctionLoweringInfo &FuncInfo) {
  MachineFunction *MF = FuncInfo.MF;
  const DataLayout &DL = MF->getDataLayout();
  for (const BasicBlock &BB : *FuncInfo.Fn) {
    for (const Instruction &I : BB) {
      const DbgDeclareInst *DI = dyn_cast<DbgDeclareInst>(&I);
      if (!DI)
        continue;

      assert(DI->getVariable() && "Missing variable");
      assert(DI->getDebugLoc() && "Missing location");
      const Value *Address = DI->getAddress();
      if (!Address) {
        LLVM_DEBUG(dbgs() << "processDbgDeclares skipping " << *DI
                          << " (bad address)\n");
        continue;
      }

      // Look through casts and constant offset GEPs. These mostly come from
      // inalloca.
      APInt Offset(DL.getTypeSizeInBits(Address->getType()), 0);
      Address = Address->stripAndAccumulateInBoundsConstantOffsets(DL, Offset);

      // Check if the variable is a static alloca or a byval or inalloca
      // argument passed in memory. If it is not, then we will ignore this
      // intrinsic and handle this during isel like dbg.value.
      int FI = std::numeric_limits<int>::max();
      if (const auto *AI = dyn_cast<AllocaInst>(Address)) {
        auto SI = FuncInfo.StaticAllocaMap.find(AI);
        if (SI != FuncInfo.StaticAllocaMap.end())
          FI = SI->second;
      } else if (const auto *Arg = dyn_cast<Argument>(Address))
        FI = FuncInfo.getArgumentFrameIndex(Arg);

      if (FI == std::numeric_limits<int>::max())
        continue;

      DIExpression *Expr = DI->getExpression();
      if (Offset.getBoolValue())
        Expr = DIExpression::prepend(Expr, DIExpression::ApplyOffset,
                                     Offset.getZExtValue());
      LLVM_DEBUG(dbgs() << "processDbgDeclares: setVariableDbgInfo FI=" << FI
                        << ", " << *DI << "\n");
      MF->setVariableDbgInfo(DI->getVariable(), Expr, FI, DI->getDebugLoc());
    }
  }
}

void SelectionDAGISel::SelectAllBasicBlocks(const Function &Fn) {
  FastISelFailed = false;
  // Initialize the Fast-ISel state, if needed.
  FastISel *FastIS = nullptr;
  if (TM.Options.EnableFastISel) {
    LLVM_DEBUG(dbgs() << "Enabling fast-isel\n");
    FastIS = TLI->createFastISel(*FuncInfo, LibInfo);
    if (FastIS)
      FastIS->useInstrRefDebugInfo(UseInstrRefDebugInfo);
  }

  ReversePostOrderTraversal<const Function*> RPOT(&Fn);

  // Lower arguments up front. An RPO iteration always visits the entry block
  // first.
  assert(*RPOT.begin() == &Fn.getEntryBlock());
  ++NumEntryBlocks;

  // Set up FuncInfo for ISel. Entry blocks never have PHIs.
  FuncInfo->MBB = FuncInfo->MBBMap[&Fn.getEntryBlock()];
  FuncInfo->InsertPt = FuncInfo->MBB->begin();

  CurDAG->setFunctionLoweringInfo(FuncInfo.get());

  if (!FastIS) {
    LowerArguments(Fn);
  } else {
    // See if fast isel can lower the arguments.
    FastIS->startNewBlock();
    if (!FastIS->lowerArguments()) {
      FastISelFailed = true;
      // Fast isel failed to lower these arguments
      ++NumFastIselFailLowerArguments;

      OptimizationRemarkMissed R("sdagisel", "FastISelFailure",
                                 Fn.getSubprogram(),
                                 &Fn.getEntryBlock());
      R << "FastISel didn't lower all arguments: "
        << ore::NV("Prototype", Fn.getType());
      reportFastISelFailure(*MF, *ORE, R, EnableFastISelAbort > 1);

      // Use SelectionDAG argument lowering
      LowerArguments(Fn);
      CurDAG->setRoot(SDB->getControlRoot());
      SDB->clear();
      CodeGenAndEmitDAG();
    }

    // If we inserted any instructions at the beginning, make a note of
    // where they are, so we can be sure to emit subsequent instructions
    // after them.
    if (FuncInfo->InsertPt != FuncInfo->MBB->begin())
      FastIS->setLastLocalValue(&*std::prev(FuncInfo->InsertPt));
    else
      FastIS->setLastLocalValue(nullptr);
  }

  bool Inserted = SwiftError->createEntriesInEntryBlock(SDB->getCurDebugLoc());

  if (FastIS && Inserted)
    FastIS->setLastLocalValue(&*std::prev(FuncInfo->InsertPt));

  processDbgDeclares(*FuncInfo);

  // Iterate over all basic blocks in the function.
  StackProtector &SP = getAnalysis<StackProtector>();
  for (const BasicBlock *LLVMBB : RPOT) {
    if (OptLevel != CodeGenOpt::None) {
      bool AllPredsVisited = true;
      for (const BasicBlock *Pred : predecessors(LLVMBB)) {
        if (!FuncInfo->VisitedBBs.count(Pred)) {
          AllPredsVisited = false;
          break;
        }
      }

      if (AllPredsVisited) {
        for (const PHINode &PN : LLVMBB->phis())
          FuncInfo->ComputePHILiveOutRegInfo(&PN);
      } else {
        for (const PHINode &PN : LLVMBB->phis())
          FuncInfo->InvalidatePHILiveOutRegInfo(&PN);
      }

      FuncInfo->VisitedBBs.insert(LLVMBB);
    }

    BasicBlock::const_iterator const Begin =
        LLVMBB->getFirstNonPHI()->getIterator();
    BasicBlock::const_iterator const End = LLVMBB->end();
    BasicBlock::const_iterator BI = End;

    FuncInfo->MBB = FuncInfo->MBBMap[LLVMBB];
    if (!FuncInfo->MBB)
      continue; // Some blocks like catchpads have no code or MBB.

    // Insert new instructions after any phi or argument setup code.
    FuncInfo->InsertPt = FuncInfo->MBB->end();

    // Setup an EH landing-pad block.
    FuncInfo->ExceptionPointerVirtReg = 0;
    FuncInfo->ExceptionSelectorVirtReg = 0;
    if (LLVMBB->isEHPad())
      if (!PrepareEHLandingPad())
        continue;

    // Before doing SelectionDAG ISel, see if FastISel has been requested.
    if (FastIS) {
      if (LLVMBB != &Fn.getEntryBlock())
        FastIS->startNewBlock();

      unsigned NumFastIselRemaining = std::distance(Begin, End);

      // Pre-assign swifterror vregs.
      SwiftError->preassignVRegs(FuncInfo->MBB, Begin, End);

      // Do FastISel on as many instructions as possible.
      for (; BI != Begin; --BI) {
        const Instruction *Inst = &*std::prev(BI);

        // If we no longer require this instruction, skip it.
        if (isFoldedOrDeadInstruction(Inst, *FuncInfo) ||
            ElidedArgCopyInstrs.count(Inst)) {
          --NumFastIselRemaining;
          continue;
        }

        // Bottom-up: reset the insert pos at the top, after any local-value
        // instructions.
        FastIS->recomputeInsertPt();

        // Try to select the instruction with FastISel.
        if (FastIS->selectInstruction(Inst)) {
          --NumFastIselRemaining;
          ++NumFastIselSuccess;
          // If fast isel succeeded, skip over all the folded instructions, and
          // then see if there is a load right before the selected instructions.
          // Try to fold the load if so.
          const Instruction *BeforeInst = Inst;
          while (BeforeInst != &*Begin) {
            BeforeInst = &*std::prev(BasicBlock::const_iterator(BeforeInst));
            if (!isFoldedOrDeadInstruction(BeforeInst, *FuncInfo))
              break;
          }
          if (BeforeInst != Inst && isa<LoadInst>(BeforeInst) &&
              BeforeInst->hasOneUse() &&
              FastIS->tryToFoldLoad(cast<LoadInst>(BeforeInst), Inst)) {
            // If we succeeded, don't re-select the load.
            BI = std::next(BasicBlock::const_iterator(BeforeInst));
            --NumFastIselRemaining;
            ++NumFastIselSuccess;
          }
          continue;
        }

        FastISelFailed = true;

        // Then handle certain instructions as single-LLVM-Instruction blocks.
        // We cannot separate out GCrelocates to their own blocks since we need
        // to keep track of gc-relocates for a particular gc-statepoint. This is
        // done by SelectionDAGBuilder::LowerAsSTATEPOINT, called before
        // visitGCRelocate.
        if (isa<CallInst>(Inst) && !isa<GCStatepointInst>(Inst) &&
            !isa<GCRelocateInst>(Inst) && !isa<GCResultInst>(Inst)) {
          OptimizationRemarkMissed R("sdagisel", "FastISelFailure",
                                     Inst->getDebugLoc(), LLVMBB);

          R << "FastISel missed call";

          if (R.isEnabled() || EnableFastISelAbort) {
            std::string InstStrStorage;
            raw_string_ostream InstStr(InstStrStorage);
            InstStr << *Inst;

            R << ": " << InstStr.str();
          }

          reportFastISelFailure(*MF, *ORE, R, EnableFastISelAbort > 2);

          if (!Inst->getType()->isVoidTy() && !Inst->getType()->isTokenTy() &&
              !Inst->use_empty()) {
            Register &R = FuncInfo->ValueMap[Inst];
            if (!R)
              R = FuncInfo->CreateRegs(Inst);
          }

          bool HadTailCall = false;
          MachineBasicBlock::iterator SavedInsertPt = FuncInfo->InsertPt;
          SelectBasicBlock(Inst->getIterator(), BI, HadTailCall);

          // If the call was emitted as a tail call, we're done with the block.
          // We also need to delete any previously emitted instructions.
          if (HadTailCall) {
            FastIS->removeDeadCode(SavedInsertPt, FuncInfo->MBB->end());
            --BI;
            break;
          }

          // Recompute NumFastIselRemaining as Selection DAG instruction
          // selection may have handled the call, input args, etc.
          unsigned RemainingNow = std::distance(Begin, BI);
          NumFastIselFailures += NumFastIselRemaining - RemainingNow;
          NumFastIselRemaining = RemainingNow;
          continue;
        }

        OptimizationRemarkMissed R("sdagisel", "FastISelFailure",
                                   Inst->getDebugLoc(), LLVMBB);

        bool ShouldAbort = EnableFastISelAbort;
        if (Inst->isTerminator()) {
          // Use a different message for terminator misses.
          R << "FastISel missed terminator";
          // Don't abort for terminator unless the level is really high
          ShouldAbort = (EnableFastISelAbort > 2);
        } else {
          R << "FastISel missed";
        }

        if (R.isEnabled() || EnableFastISelAbort) {
          std::string InstStrStorage;
          raw_string_ostream InstStr(InstStrStorage);
          InstStr << *Inst;
          R << ": " << InstStr.str();
        }

        reportFastISelFailure(*MF, *ORE, R, ShouldAbort);

        NumFastIselFailures += NumFastIselRemaining;
        break;
      }

      FastIS->recomputeInsertPt();
    }

    if (SP.shouldEmitSDCheck(*LLVMBB)) {
      bool FunctionBasedInstrumentation =
          TLI->getSSPStackGuardCheck(*Fn.getParent());
      SDB->SPDescriptor.initialize(LLVMBB, FuncInfo->MBBMap[LLVMBB],
                                   FunctionBasedInstrumentation);
    }

    if (Begin != BI)
      ++NumDAGBlocks;
    else
      ++NumFastIselBlocks;

    if (Begin != BI) {
      // Run SelectionDAG instruction selection on the remainder of the block
      // not handled by FastISel. If FastISel is not run, this is the entire
      // block.
      bool HadTailCall;
      SelectBasicBlock(Begin, BI, HadTailCall);

      // But if FastISel was run, we already selected some of the block.
      // If we emitted a tail-call, we need to delete any previously emitted
      // instruction that follows it.
      if (FastIS && HadTailCall && FuncInfo->InsertPt != FuncInfo->MBB->end())
        FastIS->removeDeadCode(FuncInfo->InsertPt, FuncInfo->MBB->end());
    }

    if (FastIS)
      FastIS->finishBasicBlock();
    FinishBasicBlock();
    FuncInfo->PHINodesToUpdate.clear();
    ElidedArgCopyInstrs.clear();
  }

  SP.copyToMachineFrameInfo(MF->getFrameInfo());

  SwiftError->propagateVRegs();

  delete FastIS;
  SDB->clearDanglingDebugInfo();
  SDB->SPDescriptor.resetPerFunctionState();
}

void
SelectionDAGISel::FinishBasicBlock() {
  LLVM_DEBUG(dbgs() << "Total amount of phi nodes to update: "
                    << FuncInfo->PHINodesToUpdate.size() << "\n";
             for (unsigned i = 0, e = FuncInfo->PHINodesToUpdate.size(); i != e;
                  ++i) dbgs()
             << "Node " << i << " : (" << FuncInfo->PHINodesToUpdate[i].first
             << ", " << FuncInfo->PHINodesToUpdate[i].second << ")\n");

  // Next, now that we know what the last MBB the LLVM BB expanded is, update
  // PHI nodes in successors.
  for (unsigned i = 0, e = FuncInfo->PHINodesToUpdate.size(); i != e; ++i) {
    MachineInstrBuilder PHI(*MF, FuncInfo->PHINodesToUpdate[i].first);
    assert(PHI->isPHI() &&
           "This is not a machine PHI node that we are updating!");
    if (!FuncInfo->MBB->isSuccessor(PHI->getParent()))
      continue;
    PHI.addReg(FuncInfo->PHINodesToUpdate[i].second).addMBB(FuncInfo->MBB);
  }

  // Handle stack protector.
  if (SDB->SPDescriptor.shouldEmitFunctionBasedCheckStackProtector()) {
    // The target provides a guard check function. There is no need to
    // generate error handling code or to split current basic block.
    MachineBasicBlock *ParentMBB = SDB->SPDescriptor.getParentMBB();

    // Add load and check to the basicblock.
    FuncInfo->MBB = ParentMBB;
    FuncInfo->InsertPt =
        findSplitPointForStackProtector(ParentMBB, *TII);
    SDB->visitSPDescriptorParent(SDB->SPDescriptor, ParentMBB);
    CurDAG->setRoot(SDB->getRoot());
    SDB->clear();
    CodeGenAndEmitDAG();

    // Clear the Per-BB State.
    SDB->SPDescriptor.resetPerBBState();
  } else if (SDB->SPDescriptor.shouldEmitStackProtector()) {
    MachineBasicBlock *ParentMBB = SDB->SPDescriptor.getParentMBB();
    MachineBasicBlock *SuccessMBB = SDB->SPDescriptor.getSuccessMBB();

    // Find the split point to split the parent mbb. At the same time copy all
    // physical registers used in the tail of parent mbb into virtual registers
    // before the split point and back into physical registers after the split
    // point. This prevents us needing to deal with Live-ins and many other
    // register allocation issues caused by us splitting the parent mbb. The
    // register allocator will clean up said virtual copies later on.
    MachineBasicBlock::iterator SplitPoint =
        findSplitPointForStackProtector(ParentMBB, *TII);

    // Splice the terminator of ParentMBB into SuccessMBB.
    SuccessMBB->splice(SuccessMBB->end(), ParentMBB,
                       SplitPoint,
                       ParentMBB->end());

    // Add compare/jump on neq/jump to the parent BB.
    FuncInfo->MBB = ParentMBB;
    FuncInfo->InsertPt = ParentMBB->end();
    SDB->visitSPDescriptorParent(SDB->SPDescriptor, ParentMBB);
    CurDAG->setRoot(SDB->getRoot());
    SDB->clear();
    CodeGenAndEmitDAG();

    // CodeGen Failure MBB if we have not codegened it yet.
    MachineBasicBlock *FailureMBB = SDB->SPDescriptor.getFailureMBB();
    if (FailureMBB->empty()) {
      FuncInfo->MBB = FailureMBB;
      FuncInfo->InsertPt = FailureMBB->end();
      SDB->visitSPDescriptorFailure(SDB->SPDescriptor);
      CurDAG->setRoot(SDB->getRoot());
      SDB->clear();
      CodeGenAndEmitDAG();
    }

    // Clear the Per-BB State.
    SDB->SPDescriptor.resetPerBBState();
  }

  // Lower each BitTestBlock.
  for (auto &BTB : SDB->SL->BitTestCases) {
    // Lower header first, if it wasn't already lowered
    if (!BTB.Emitted) {
      // Set the current basic block to the mbb we wish to insert the code into
      FuncInfo->MBB = BTB.Parent;
      FuncInfo->InsertPt = FuncInfo->MBB->end();
      // Emit the code
      SDB->visitBitTestHeader(BTB, FuncInfo->MBB);
      CurDAG->setRoot(SDB->getRoot());
      SDB->clear();
      CodeGenAndEmitDAG();
    }

    BranchProbability UnhandledProb = BTB.Prob;
    for (unsigned j = 0, ej = BTB.Cases.size(); j != ej; ++j) {
      UnhandledProb -= BTB.Cases[j].ExtraProb;
      // Set the current basic block to the mbb we wish to insert the code into
      FuncInfo->MBB = BTB.Cases[j].ThisBB;
      FuncInfo->InsertPt = FuncInfo->MBB->end();
      // Emit the code

      // If all cases cover a contiguous range, it is not necessary to jump to
      // the default block after the last bit test fails. This is because the
      // range check during bit test header creation has guaranteed that every
      // case here doesn't go outside the range. In this case, there is no need
      // to perform the last bit test, as it will always be true. Instead, make
      // the second-to-last bit-test fall through to the target of the last bit
      // test, and delete the last bit test.

      MachineBasicBlock *NextMBB;
      if ((BTB.ContiguousRange || BTB.FallthroughUnreachable) && j + 2 == ej) {
        // Second-to-last bit-test with contiguous range or omitted range
        // check: fall through to the target of the final bit test.
        NextMBB = BTB.Cases[j + 1].TargetBB;
      } else if (j + 1 == ej) {
        // For the last bit test, fall through to Default.
        NextMBB = BTB.Default;
      } else {
        // Otherwise, fall through to the next bit test.
        NextMBB = BTB.Cases[j + 1].ThisBB;
      }

      SDB->visitBitTestCase(BTB, NextMBB, UnhandledProb, BTB.Reg, BTB.Cases[j],
                            FuncInfo->MBB);

      CurDAG->setRoot(SDB->getRoot());
      SDB->clear();
      CodeGenAndEmitDAG();

      if ((BTB.ContiguousRange || BTB.FallthroughUnreachable) && j + 2 == ej) {
        // Since we're not going to use the final bit test, remove it.
        BTB.Cases.pop_back();
        break;
      }
    }

    // Update PHI Nodes
    for (const std::pair<MachineInstr *, unsigned> &P :
         FuncInfo->PHINodesToUpdate) {
      MachineInstrBuilder PHI(*MF, P.first);
      MachineBasicBlock *PHIBB = PHI->getParent();
      assert(PHI->isPHI() &&
             "This is not a machine PHI node that we are updating!");
      // This is "default" BB. We have two jumps to it. From "header" BB and
      // from last "case" BB, unless the latter was skipped.
      if (PHIBB == BTB.Default) {
        PHI.addReg(P.second).addMBB(BTB.Parent);
        if (!BTB.ContiguousRange) {
          PHI.addReg(P.second).addMBB(BTB.Cases.back().ThisBB);
         }
      }
      // One of "cases" BB.
      for (const SwitchCG::BitTestCase &BT : BTB.Cases) {
        MachineBasicBlock* cBB = BT.ThisBB;
        if (cBB->isSuccessor(PHIBB))
          PHI.addReg(P.second).addMBB(cBB);
      }
    }
  }
  SDB->SL->BitTestCases.clear();

  // If the JumpTable record is filled in, then we need to emit a jump table.
  // Updating the PHI nodes is tricky in this case, since we need to determine
  // whether the PHI is a successor of the range check MBB or the jump table MBB
  for (unsigned i = 0, e = SDB->SL->JTCases.size(); i != e; ++i) {
    // Lower header first, if it wasn't already lowered
    if (!SDB->SL->JTCases[i].first.Emitted) {
      // Set the current basic block to the mbb we wish to insert the code into
      FuncInfo->MBB = SDB->SL->JTCases[i].first.HeaderBB;
      FuncInfo->InsertPt = FuncInfo->MBB->end();
      // Emit the code
      SDB->visitJumpTableHeader(SDB->SL->JTCases[i].second,
                                SDB->SL->JTCases[i].first, FuncInfo->MBB);
      CurDAG->setRoot(SDB->getRoot());
      SDB->clear();
      CodeGenAndEmitDAG();
    }

    // Set the current basic block to the mbb we wish to insert the code into
    FuncInfo->MBB = SDB->SL->JTCases[i].second.MBB;
    FuncInfo->InsertPt = FuncInfo->MBB->end();
    // Emit the code
    SDB->visitJumpTable(SDB->SL->JTCases[i].second);
    CurDAG->setRoot(SDB->getRoot());
    SDB->clear();
    CodeGenAndEmitDAG();

    // Update PHI Nodes
    for (unsigned pi = 0, pe = FuncInfo->PHINodesToUpdate.size();
         pi != pe; ++pi) {
      MachineInstrBuilder PHI(*MF, FuncInfo->PHINodesToUpdate[pi].first);
      MachineBasicBlock *PHIBB = PHI->getParent();
      assert(PHI->isPHI() &&
             "This is not a machine PHI node that we are updating!");
      // "default" BB. We can go there only from header BB.
      if (PHIBB == SDB->SL->JTCases[i].second.Default)
        PHI.addReg(FuncInfo->PHINodesToUpdate[pi].second)
           .addMBB(SDB->SL->JTCases[i].first.HeaderBB);
      // JT BB. Just iterate over successors here
      if (FuncInfo->MBB->isSuccessor(PHIBB))
        PHI.addReg(FuncInfo->PHINodesToUpdate[pi].second).addMBB(FuncInfo->MBB);
    }
  }
  SDB->SL->JTCases.clear();

  // If we generated any switch lowering information, build and codegen any
  // additional DAGs necessary.
  for (unsigned i = 0, e = SDB->SL->SwitchCases.size(); i != e; ++i) {
    // Set the current basic block to the mbb we wish to insert the code into
    FuncInfo->MBB = SDB->SL->SwitchCases[i].ThisBB;
    FuncInfo->InsertPt = FuncInfo->MBB->end();

    // Determine the unique successors.
    SmallVector<MachineBasicBlock *, 2> Succs;
    Succs.push_back(SDB->SL->SwitchCases[i].TrueBB);
    if (SDB->SL->SwitchCases[i].TrueBB != SDB->SL->SwitchCases[i].FalseBB)
      Succs.push_back(SDB->SL->SwitchCases[i].FalseBB);

    // Emit the code. Note that this could result in FuncInfo->MBB being split.
    SDB->visitSwitchCase(SDB->SL->SwitchCases[i], FuncInfo->MBB);
    CurDAG->setRoot(SDB->getRoot());
    SDB->clear();
    CodeGenAndEmitDAG();

    // Remember the last block, now that any splitting is done, for use in
    // populating PHI nodes in successors.
    MachineBasicBlock *ThisBB = FuncInfo->MBB;

    // Handle any PHI nodes in successors of this chunk, as if we were coming
    // from the original BB before switch expansion.  Note that PHI nodes can
    // occur multiple times in PHINodesToUpdate.  We have to be very careful to
    // handle them the right number of times.
    for (unsigned i = 0, e = Succs.size(); i != e; ++i) {
      FuncInfo->MBB = Succs[i];
      FuncInfo->InsertPt = FuncInfo->MBB->end();
      // FuncInfo->MBB may have been removed from the CFG if a branch was
      // constant folded.
      if (ThisBB->isSuccessor(FuncInfo->MBB)) {
        for (MachineBasicBlock::iterator
             MBBI = FuncInfo->MBB->begin(), MBBE = FuncInfo->MBB->end();
             MBBI != MBBE && MBBI->isPHI(); ++MBBI) {
          MachineInstrBuilder PHI(*MF, MBBI);
          // This value for this PHI node is recorded in PHINodesToUpdate.
          for (unsigned pn = 0; ; ++pn) {
            assert(pn != FuncInfo->PHINodesToUpdate.size() &&
                   "Didn't find PHI entry!");
            if (FuncInfo->PHINodesToUpdate[pn].first == PHI) {
              PHI.addReg(FuncInfo->PHINodesToUpdate[pn].second).addMBB(ThisBB);
              break;
            }
          }
        }
      }
    }
  }
  SDB->SL->SwitchCases.clear();
}

/// Create the scheduler. If a specific scheduler was specified
/// via the SchedulerRegistry, use it, otherwise select the
/// one preferred by the target.
///
ScheduleDAGSDNodes *SelectionDAGISel::CreateScheduler() {
  return ISHeuristic(this, OptLevel);
}

//===----------------------------------------------------------------------===//
// Helper functions used by the generated instruction selector.
//===----------------------------------------------------------------------===//
// Calls to these methods are generated by tblgen.

/// CheckAndMask - The isel is trying to match something like (and X, 255).  If
/// the dag combiner simplified the 255, we still want to match.  RHS is the
/// actual value in the DAG on the RHS of an AND, and DesiredMaskS is the value
/// specified in the .td file (e.g. 255).
bool SelectionDAGISel::CheckAndMask(SDValue LHS, ConstantSDNode *RHS,
                                    int64_t DesiredMaskS) const {
  const APInt &ActualMask = RHS->getAPIntValue();
  const APInt &DesiredMask = APInt(LHS.getValueSizeInBits(), DesiredMaskS);

  // If the actual mask exactly matches, success!
  if (ActualMask == DesiredMask)
    return true;

  // If the actual AND mask is allowing unallowed bits, this doesn't match.
  if (!ActualMask.isSubsetOf(DesiredMask))
    return false;

  // Otherwise, the DAG Combiner may have proven that the value coming in is
  // either already zero or is not demanded.  Check for known zero input bits.
  APInt NeededMask = DesiredMask & ~ActualMask;
  if (CurDAG->MaskedValueIsZero(LHS, NeededMask))
    return true;

  // TODO: check to see if missing bits are just not demanded.

  // Otherwise, this pattern doesn't match.
  return false;
}

/// CheckOrMask - The isel is trying to match something like (or X, 255).  If
/// the dag combiner simplified the 255, we still want to match.  RHS is the
/// actual value in the DAG on the RHS of an OR, and DesiredMaskS is the value
/// specified in the .td file (e.g. 255).
bool SelectionDAGISel::CheckOrMask(SDValue LHS, ConstantSDNode *RHS,
                                   int64_t DesiredMaskS) const {
  const APInt &ActualMask = RHS->getAPIntValue();
  const APInt &DesiredMask = APInt(LHS.getValueSizeInBits(), DesiredMaskS);

  // If the actual mask exactly matches, success!
  if (ActualMask == DesiredMask)
    return true;

  // If the actual AND mask is allowing unallowed bits, this doesn't match.
  if (!ActualMask.isSubsetOf(DesiredMask))
    return false;

  // Otherwise, the DAG Combiner may have proven that the value coming in is
  // either already zero or is not demanded.  Check for known zero input bits.
  APInt NeededMask = DesiredMask & ~ActualMask;
  KnownBits Known = CurDAG->computeKnownBits(LHS);

  // If all the missing bits in the or are already known to be set, match!
  if (NeededMask.isSubsetOf(Known.One))
    return true;

  // TODO: check to see if missing bits are just not demanded.

  // Otherwise, this pattern doesn't match.
  return false;
}

/// SelectInlineAsmMemoryOperands - Calls to this are automatically generated
/// by tblgen.  Others should not call it.
void SelectionDAGISel::SelectInlineAsmMemoryOperands(std::vector<SDValue> &Ops,
                                                     const SDLoc &DL) {
  std::vector<SDValue> InOps;
  std::swap(InOps, Ops);

  Ops.push_back(InOps[InlineAsm::Op_InputChain]); // 0
  Ops.push_back(InOps[InlineAsm::Op_AsmString]);  // 1
  Ops.push_back(InOps[InlineAsm::Op_MDNode]);     // 2, !srcloc
  Ops.push_back(InOps[InlineAsm::Op_ExtraInfo]);  // 3 (SideEffect, AlignStack)

  unsigned i = InlineAsm::Op_FirstOperand, e = InOps.size();
  if (InOps[e-1].getValueType() == MVT::Glue)
    --e;  // Don't process a glue operand if it is here.

  while (i != e) {
    unsigned Flags = cast<ConstantSDNode>(InOps[i])->getZExtValue();
    if (!InlineAsm::isMemKind(Flags)) {
      // Just skip over this operand, copying the operands verbatim.
      Ops.insert(Ops.end(), InOps.begin()+i,
                 InOps.begin()+i+InlineAsm::getNumOperandRegisters(Flags) + 1);
      i += InlineAsm::getNumOperandRegisters(Flags) + 1;
    } else {
      assert(InlineAsm::getNumOperandRegisters(Flags) == 1 &&
             "Memory operand with multiple values?");

      unsigned TiedToOperand;
      if (InlineAsm::isUseOperandTiedToDef(Flags, TiedToOperand)) {
        // We need the constraint ID from the operand this is tied to.
        unsigned CurOp = InlineAsm::Op_FirstOperand;
        Flags = cast<ConstantSDNode>(InOps[CurOp])->getZExtValue();
        for (; TiedToOperand; --TiedToOperand) {
          CurOp += InlineAsm::getNumOperandRegisters(Flags)+1;
          Flags = cast<ConstantSDNode>(InOps[CurOp])->getZExtValue();
        }
      }

      // Otherwise, this is a memory operand.  Ask the target to select it.
      std::vector<SDValue> SelOps;
      unsigned ConstraintID = InlineAsm::getMemoryConstraintID(Flags);
      if (SelectInlineAsmMemoryOperand(InOps[i+1], ConstraintID, SelOps))
        report_fatal_error("Could not match memory address.  Inline asm"
                           " failure!");

      // Add this to the output node.
      unsigned NewFlags =
        InlineAsm::getFlagWord(InlineAsm::Kind_Mem, SelOps.size());
      NewFlags = InlineAsm::getFlagWordForMem(NewFlags, ConstraintID);
      Ops.push_back(CurDAG->getTargetConstant(NewFlags, DL, MVT::i32));
      llvm::append_range(Ops, SelOps);
      i += 2;
    }
  }

  // Add the glue input back if present.
  if (e != InOps.size())
    Ops.push_back(InOps.back());
}

/// findGlueUse - Return use of MVT::Glue value produced by the specified
/// SDNode.
///
static SDNode *findGlueUse(SDNode *N) {
  unsigned FlagResNo = N->getNumValues()-1;
  for (SDNode::use_iterator I = N->use_begin(), E = N->use_end(); I != E; ++I) {
    SDUse &Use = I.getUse();
    if (Use.getResNo() == FlagResNo)
      return Use.getUser();
  }
  return nullptr;
}

/// findNonImmUse - Return true if "Def" is a predecessor of "Root" via a path
/// beyond "ImmedUse".  We may ignore chains as they are checked separately.
static bool findNonImmUse(SDNode *Root, SDNode *Def, SDNode *ImmedUse,
                          bool IgnoreChains) {
  SmallPtrSet<const SDNode *, 16> Visited;
  SmallVector<const SDNode *, 16> WorkList;
  // Only check if we have non-immediate uses of Def.
  if (ImmedUse->isOnlyUserOf(Def))
    return false;

  // We don't care about paths to Def that go through ImmedUse so mark it
  // visited and mark non-def operands as used.
  Visited.insert(ImmedUse);
  for (const SDValue &Op : ImmedUse->op_values()) {
    SDNode *N = Op.getNode();
    // Ignore chain deps (they are validated by
    // HandleMergeInputChains) and immediate uses
    if ((Op.getValueType() == MVT::Other && IgnoreChains) || N == Def)
      continue;
    if (!Visited.insert(N).second)
      continue;
    WorkList.push_back(N);
  }

  // Initialize worklist to operands of Root.
  if (Root != ImmedUse) {
    for (const SDValue &Op : Root->op_values()) {
      SDNode *N = Op.getNode();
      // Ignore chains (they are validated by HandleMergeInputChains)
      if ((Op.getValueType() == MVT::Other && IgnoreChains) || N == Def)
        continue;
      if (!Visited.insert(N).second)
        continue;
      WorkList.push_back(N);
    }
  }

  return SDNode::hasPredecessorHelper(Def, Visited, WorkList, 0, true);
}

/// IsProfitableToFold - Returns true if it's profitable to fold the specific
/// operand node N of U during instruction selection that starts at Root.
bool SelectionDAGISel::IsProfitableToFold(SDValue N, SDNode *U,
                                          SDNode *Root) const {
  if (OptLevel == CodeGenOpt::None) return false;
  return N.hasOneUse();
}

/// IsLegalToFold - Returns true if the specific operand node N of
/// U can be folded during instruction selection that starts at Root.
bool SelectionDAGISel::IsLegalToFold(SDValue N, SDNode *U, SDNode *Root,
                                     CodeGenOpt::Level OptLevel,
                                     bool IgnoreChains) {
  if (OptLevel == CodeGenOpt::None) return false;

  // If Root use can somehow reach N through a path that that doesn't contain
  // U then folding N would create a cycle. e.g. In the following
  // diagram, Root can reach N through X. If N is folded into Root, then
  // X is both a predecessor and a successor of U.
  //
  //          [N*]           //
  //         ^   ^           //
  //        /     \          //
  //      [U*]    [X]?       //
  //        ^     ^          //
  //         \   /           //
  //          \ /            //
  //         [Root*]         //
  //
  // * indicates nodes to be folded together.
  //
  // If Root produces glue, then it gets (even more) interesting. Since it
  // will be "glued" together with its glue use in the scheduler, we need to
  // check if it might reach N.
  //
  //          [N*]           //
  //         ^   ^           //
  //        /     \          //
  //      [U*]    [X]?       //
  //        ^       ^        //
  //         \       \       //
  //          \      |       //
  //         [Root*] |       //
  //          ^      |       //
  //          f      |       //
  //          |      /       //
  //         [Y]    /        //
  //           ^   /         //
  //           f  /          //
  //           | /           //
  //          [GU]           //
  //
  // If GU (glue use) indirectly reaches N (the load), and Root folds N
  // (call it Fold), then X is a predecessor of GU and a successor of
  // Fold. But since Fold and GU are glued together, this will create
  // a cycle in the scheduling graph.

  // If the node has glue, walk down the graph to the "lowest" node in the
  // glueged set.
  EVT VT = Root->getValueType(Root->getNumValues()-1);
  while (VT == MVT::Glue) {
    SDNode *GU = findGlueUse(Root);
    if (!GU)
      break;
    Root = GU;
    VT = Root->getValueType(Root->getNumValues()-1);

    // If our query node has a glue result with a use, we've walked up it.  If
    // the user (which has already been selected) has a chain or indirectly uses
    // the chain, HandleMergeInputChains will not consider it.  Because of
    // this, we cannot ignore chains in this predicate.
    IgnoreChains = false;
  }

  return !findNonImmUse(Root, N.getNode(), U, IgnoreChains);
}

void SelectionDAGISel::Select_INLINEASM(SDNode *N) {
  SDLoc DL(N);

  std::vector<SDValue> Ops(N->op_begin(), N->op_end());
  SelectInlineAsmMemoryOperands(Ops, DL);

  const EVT VTs[] = {MVT::Other, MVT::Glue};
  SDValue New = CurDAG->getNode(N->getOpcode(), DL, VTs, Ops);
  New->setNodeId(-1);
  ReplaceUses(N, New.getNode());
  CurDAG->RemoveDeadNode(N);
}

void SelectionDAGISel::Select_READ_REGISTER(SDNode *Op) {
  SDLoc dl(Op);
  MDNodeSDNode *MD = cast<MDNodeSDNode>(Op->getOperand(1));
  const MDString *RegStr = cast<MDString>(MD->getMD()->getOperand(0));

  EVT VT = Op->getValueType(0);
  LLT Ty = VT.isSimple() ? getLLTForMVT(VT.getSimpleVT()) : LLT();
  Register Reg =
      TLI->getRegisterByName(RegStr->getString().data(), Ty,
                             CurDAG->getMachineFunction());
  SDValue New = CurDAG->getCopyFromReg(
                        Op->getOperand(0), dl, Reg, Op->getValueType(0));
  New->setNodeId(-1);
  ReplaceUses(Op, New.getNode());
  CurDAG->RemoveDeadNode(Op);
}

void SelectionDAGISel::Select_WRITE_REGISTER(SDNode *Op) {
  SDLoc dl(Op);
  MDNodeSDNode *MD = cast<MDNodeSDNode>(Op->getOperand(1));
  const MDString *RegStr = cast<MDString>(MD->getMD()->getOperand(0));

  EVT VT = Op->getOperand(2).getValueType();
  LLT Ty = VT.isSimple() ? getLLTForMVT(VT.getSimpleVT()) : LLT();

  Register Reg = TLI->getRegisterByName(RegStr->getString().data(), Ty,
                                        CurDAG->getMachineFunction());
  SDValue New = CurDAG->getCopyToReg(
                        Op->getOperand(0), dl, Reg, Op->getOperand(2));
  New->setNodeId(-1);
  ReplaceUses(Op, New.getNode());
  CurDAG->RemoveDeadNode(Op);
}

void SelectionDAGISel::Select_UNDEF(SDNode *N) {
  CurDAG->SelectNodeTo(N, TargetOpcode::IMPLICIT_DEF, N->getValueType(0));
}

void SelectionDAGISel::Select_FREEZE(SDNode *N) {
  // TODO: We don't have FREEZE pseudo-instruction in MachineInstr-level now.
  // If FREEZE instruction is added later, the code below must be changed as
  // well.
  CurDAG->SelectNodeTo(N, TargetOpcode::COPY, N->getValueType(0),
                       N->getOperand(0));
}

void SelectionDAGISel::Select_ARITH_FENCE(SDNode *N) {
  CurDAG->SelectNodeTo(N, TargetOpcode::ARITH_FENCE, N->getValueType(0),
                       N->getOperand(0));
}

/// GetVBR - decode a vbr encoding whose top bit is set.
LLVM_ATTRIBUTE_ALWAYS_INLINE static uint64_t
GetVBR(uint64_t Val, const unsigned char *MatcherTable, unsigned &Idx) {
  assert(Val >= 128 && "Not a VBR");
  Val &= 127;  // Remove first vbr bit.

  unsigned Shift = 7;
  uint64_t NextBits;
  do {
    NextBits = MatcherTable[Idx++];
    Val |= (NextBits&127) << Shift;
    Shift += 7;
  } while (NextBits & 128);

  return Val;
}

/// When a match is complete, this method updates uses of interior chain results
/// to use the new results.
void SelectionDAGISel::UpdateChains(
    SDNode *NodeToMatch, SDValue InputChain,
    SmallVectorImpl<SDNode *> &ChainNodesMatched, bool isMorphNodeTo) {
  SmallVector<SDNode*, 4> NowDeadNodes;

  // Now that all the normal results are replaced, we replace the chain and
  // glue results if present.
  if (!ChainNodesMatched.empty()) {
    assert(InputChain.getNode() &&
           "Matched input chains but didn't produce a chain");
    // Loop over all of the nodes we matched that produced a chain result.
    // Replace all the chain results with the final chain we ended up with.
    for (unsigned i = 0, e = ChainNodesMatched.size(); i != e; ++i) {
      SDNode *ChainNode = ChainNodesMatched[i];
      // If ChainNode is null, it's because we replaced it on a previous
      // iteration and we cleared it out of the map. Just skip it.
      if (!ChainNode)
        continue;

      assert(ChainNode->getOpcode() != ISD::DELETED_NODE &&
             "Deleted node left in chain");

      // Don't replace the results of the root node if we're doing a
      // MorphNodeTo.
      if (ChainNode == NodeToMatch && isMorphNodeTo)
        continue;

      SDValue ChainVal = SDValue(ChainNode, ChainNode->getNumValues()-1);
      if (ChainVal.getValueType() == MVT::Glue)
        ChainVal = ChainVal.getValue(ChainVal->getNumValues()-2);
      assert(ChainVal.getValueType() == MVT::Other && "Not a chain?");
      SelectionDAG::DAGNodeDeletedListener NDL(
          *CurDAG, [&](SDNode *N, SDNode *E) {
            std::replace(ChainNodesMatched.begin(), ChainNodesMatched.end(), N,
                         static_cast<SDNode *>(nullptr));
          });
      if (ChainNode->getOpcode() != ISD::TokenFactor)
        ReplaceUses(ChainVal, InputChain);

      // If the node became dead and we haven't already seen it, delete it.
      if (ChainNode != NodeToMatch && ChainNode->use_empty() &&
          !llvm::is_contained(NowDeadNodes, ChainNode))
        NowDeadNodes.push_back(ChainNode);
    }
  }

  if (!NowDeadNodes.empty())
    CurDAG->RemoveDeadNodes(NowDeadNodes);

  LLVM_DEBUG(dbgs() << "ISEL: Match complete!\n");
}

/// HandleMergeInputChains - This implements the OPC_EmitMergeInputChains
/// operation for when the pattern matched at least one node with a chains.  The
/// input vector contains a list of all of the chained nodes that we match.  We
/// must determine if this is a valid thing to cover (i.e. matching it won't
/// induce cycles in the DAG) and if so, creating a TokenFactor node. that will
/// be used as the input node chain for the generated nodes.
static SDValue
HandleMergeInputChains(SmallVectorImpl<SDNode*> &ChainNodesMatched,
                       SelectionDAG *CurDAG) {

  SmallPtrSet<const SDNode *, 16> Visited;
  SmallVector<const SDNode *, 8> Worklist;
  SmallVector<SDValue, 3> InputChains;
  unsigned int Max = 8192;

  // Quick exit on trivial merge.
  if (ChainNodesMatched.size() == 1)
    return ChainNodesMatched[0]->getOperand(0);

  // Add chains that aren't already added (internal). Peek through
  // token factors.
  std::function<void(const SDValue)> AddChains = [&](const SDValue V) {
    if (V.getValueType() != MVT::Other)
      return;
    if (V->getOpcode() == ISD::EntryToken)
      return;
    if (!Visited.insert(V.getNode()).second)
      return;
    if (V->getOpcode() == ISD::TokenFactor) {
      for (const SDValue &Op : V->op_values())
        AddChains(Op);
    } else
      InputChains.push_back(V);
  };

  for (auto *N : ChainNodesMatched) {
    Worklist.push_back(N);
    Visited.insert(N);
  }

  while (!Worklist.empty())
    AddChains(Worklist.pop_back_val()->getOperand(0));

  // Skip the search if there are no chain dependencies.
  if (InputChains.size() == 0)
    return CurDAG->getEntryNode();

  // If one of these chains is a successor of input, we must have a
  // node that is both the predecessor and successor of the
  // to-be-merged nodes. Fail.
  Visited.clear();
  for (SDValue V : InputChains)
    Worklist.push_back(V.getNode());

  for (auto *N : ChainNodesMatched)
    if (SDNode::hasPredecessorHelper(N, Visited, Worklist, Max, true))
      return SDValue();

  // Return merged chain.
  if (InputChains.size() == 1)
    return InputChains[0];
  return CurDAG->getNode(ISD::TokenFactor, SDLoc(ChainNodesMatched[0]),
                         MVT::Other, InputChains);
}

/// MorphNode - Handle morphing a node in place for the selector.
SDNode *SelectionDAGISel::
MorphNode(SDNode *Node, unsigned TargetOpc, SDVTList VTList,
          ArrayRef<SDValue> Ops, unsigned EmitNodeInfo) {
  // It is possible we're using MorphNodeTo to replace a node with no
  // normal results with one that has a normal result (or we could be
  // adding a chain) and the input could have glue and chains as well.
  // In this case we need to shift the operands down.
  // FIXME: This is a horrible hack and broken in obscure cases, no worse
  // than the old isel though.
  int OldGlueResultNo = -1, OldChainResultNo = -1;

  unsigned NTMNumResults = Node->getNumValues();
  if (Node->getValueType(NTMNumResults-1) == MVT::Glue) {
    OldGlueResultNo = NTMNumResults-1;
    if (NTMNumResults != 1 &&
        Node->getValueType(NTMNumResults-2) == MVT::Other)
      OldChainResultNo = NTMNumResults-2;
  } else if (Node->getValueType(NTMNumResults-1) == MVT::Other)
    OldChainResultNo = NTMNumResults-1;

  // Call the underlying SelectionDAG routine to do the transmogrification. Note
  // that this deletes operands of the old node that become dead.
  SDNode *Res = CurDAG->MorphNodeTo(Node, ~TargetOpc, VTList, Ops);

  // MorphNodeTo can operate in two ways: if an existing node with the
  // specified operands exists, it can just return it.  Otherwise, it
  // updates the node in place to have the requested operands.
  if (Res == Node) {
    // If we updated the node in place, reset the node ID.  To the isel,
    // this should be just like a newly allocated machine node.
    Res->setNodeId(-1);
  }

  unsigned ResNumResults = Res->getNumValues();
  // Move the glue if needed.
  if ((EmitNodeInfo & OPFL_GlueOutput) && OldGlueResultNo != -1 &&
      (unsigned)OldGlueResultNo != ResNumResults-1)
    ReplaceUses(SDValue(Node, OldGlueResultNo),
                SDValue(Res, ResNumResults - 1));

  if ((EmitNodeInfo & OPFL_GlueOutput) != 0)
    --ResNumResults;

  // Move the chain reference if needed.
  if ((EmitNodeInfo & OPFL_Chain) && OldChainResultNo != -1 &&
      (unsigned)OldChainResultNo != ResNumResults-1)
    ReplaceUses(SDValue(Node, OldChainResultNo),
                SDValue(Res, ResNumResults - 1));

  // Otherwise, no replacement happened because the node already exists. Replace
  // Uses of the old node with the new one.
  if (Res != Node) {
    ReplaceNode(Node, Res);
  } else {
    EnforceNodeIdInvariant(Res);
  }

  return Res;
}

/// CheckSame - Implements OP_CheckSame.
LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckSame(const unsigned char *MatcherTable, unsigned &MatcherIndex, SDValue N,
          const SmallVectorImpl<std::pair<SDValue, SDNode *>> &RecordedNodes) {
  // Accept if it is exactly the same as a previously recorded node.
  unsigned RecNo = MatcherTable[MatcherIndex++];
  assert(RecNo < RecordedNodes.size() && "Invalid CheckSame");
  return N == RecordedNodes[RecNo].first;
}

/// CheckChildSame - Implements OP_CheckChildXSame.
LLVM_ATTRIBUTE_ALWAYS_INLINE static bool CheckChildSame(
    const unsigned char *MatcherTable, unsigned &MatcherIndex, SDValue N,
    const SmallVectorImpl<std::pair<SDValue, SDNode *>> &RecordedNodes,
    unsigned ChildNo) {
  if (ChildNo >= N.getNumOperands())
    return false;  // Match fails if out of range child #.
  return ::CheckSame(MatcherTable, MatcherIndex, N.getOperand(ChildNo),
                     RecordedNodes);
}

/// CheckPatternPredicate - Implements OP_CheckPatternPredicate.
LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckPatternPredicate(const unsigned char *MatcherTable, unsigned &MatcherIndex,
                      const SelectionDAGISel &SDISel) {
  return SDISel.CheckPatternPredicate(MatcherTable[MatcherIndex++]);
}

/// CheckNodePredicate - Implements OP_CheckNodePredicate.
LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckNodePredicate(const unsigned char *MatcherTable, unsigned &MatcherIndex,
                   const SelectionDAGISel &SDISel, SDNode *N) {
  return SDISel.CheckNodePredicate(N, MatcherTable[MatcherIndex++]);
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckOpcode(const unsigned char *MatcherTable, unsigned &MatcherIndex,
            SDNode *N) {
  uint16_t Opc = MatcherTable[MatcherIndex++];
  Opc |= (unsigned short)MatcherTable[MatcherIndex++] << 8;
  return N->getOpcode() == Opc;
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckType(const unsigned char *MatcherTable, unsigned &MatcherIndex, SDValue N,
          const TargetLowering *TLI, const DataLayout &DL) {
  MVT::SimpleValueType VT = (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
  if (N.getValueType() == VT) return true;

  // Handle the case when VT is iPTR.
  return VT == MVT::iPTR && N.getValueType() == TLI->getPointerTy(DL);
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckChildType(const unsigned char *MatcherTable, unsigned &MatcherIndex,
               SDValue N, const TargetLowering *TLI, const DataLayout &DL,
               unsigned ChildNo) {
  if (ChildNo >= N.getNumOperands())
    return false;  // Match fails if out of range child #.
  return ::CheckType(MatcherTable, MatcherIndex, N.getOperand(ChildNo), TLI,
                     DL);
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckCondCode(const unsigned char *MatcherTable, unsigned &MatcherIndex,
              SDValue N) {
  return cast<CondCodeSDNode>(N)->get() ==
      (ISD::CondCode)MatcherTable[MatcherIndex++];
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckChild2CondCode(const unsigned char *MatcherTable, unsigned &MatcherIndex,
                    SDValue N) {
  if (2 >= N.getNumOperands())
    return false;
  return ::CheckCondCode(MatcherTable, MatcherIndex, N.getOperand(2));
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckValueType(const unsigned char *MatcherTable, unsigned &MatcherIndex,
               SDValue N, const TargetLowering *TLI, const DataLayout &DL) {
  MVT::SimpleValueType VT = (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
  if (cast<VTSDNode>(N)->getVT() == VT)
    return true;

  // Handle the case when VT is iPTR.
  return VT == MVT::iPTR && cast<VTSDNode>(N)->getVT() == TLI->getPointerTy(DL);
}

// Bit 0 stores the sign of the immediate. The upper bits contain the magnitude
// shifted left by 1.
static uint64_t decodeSignRotatedValue(uint64_t V) {
  if ((V & 1) == 0)
    return V >> 1;
  if (V != 1)
    return -(V >> 1);
  // There is no such thing as -0 with integers.  "-0" really means MININT.
  return 1ULL << 63;
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckInteger(const unsigned char *MatcherTable, unsigned &MatcherIndex,
             SDValue N) {
  int64_t Val = MatcherTable[MatcherIndex++];
  if (Val & 128)
    Val = GetVBR(Val, MatcherTable, MatcherIndex);

  Val = decodeSignRotatedValue(Val);

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(N);
  return C && C->getSExtValue() == Val;
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckChildInteger(const unsigned char *MatcherTable, unsigned &MatcherIndex,
                  SDValue N, unsigned ChildNo) {
  if (ChildNo >= N.getNumOperands())
    return false;  // Match fails if out of range child #.
  return ::CheckInteger(MatcherTable, MatcherIndex, N.getOperand(ChildNo));
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckAndImm(const unsigned char *MatcherTable, unsigned &MatcherIndex,
            SDValue N, const SelectionDAGISel &SDISel) {
  int64_t Val = MatcherTable[MatcherIndex++];
  if (Val & 128)
    Val = GetVBR(Val, MatcherTable, MatcherIndex);

  if (N->getOpcode() != ISD::AND) return false;

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  return C && SDISel.CheckAndMask(N.getOperand(0), C, Val);
}

LLVM_ATTRIBUTE_ALWAYS_INLINE static bool
CheckOrImm(const unsigned char *MatcherTable, unsigned &MatcherIndex, SDValue N,
           const SelectionDAGISel &SDISel) {
  int64_t Val = MatcherTable[MatcherIndex++];
  if (Val & 128)
    Val = GetVBR(Val, MatcherTable, MatcherIndex);

  if (N->getOpcode() != ISD::OR) return false;

  ConstantSDNode *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  return C && SDISel.CheckOrMask(N.getOperand(0), C, Val);
}

/// IsPredicateKnownToFail - If we know how and can do so without pushing a
/// scope, evaluate the current node.  If the current predicate is known to
/// fail, set Result=true and return anything.  If the current predicate is
/// known to pass, set Result=false and return the MatcherIndex to continue
/// with.  If the current predicate is unknown, set Result=false and return the
/// MatcherIndex to continue with.
static unsigned IsPredicateKnownToFail(const unsigned char *Table,
                                       unsigned Index, SDValue N,
                                       bool &Result,
                                       const SelectionDAGISel &SDISel,
                  SmallVectorImpl<std::pair<SDValue, SDNode*>> &RecordedNodes) {
  switch (Table[Index++]) {
  default:
    Result = false;
    return Index-1;  // Could not evaluate this predicate.
  case SelectionDAGISel::OPC_CheckSame:
    Result = !::CheckSame(Table, Index, N, RecordedNodes);
    return Index;
  case SelectionDAGISel::OPC_CheckChild0Same:
  case SelectionDAGISel::OPC_CheckChild1Same:
  case SelectionDAGISel::OPC_CheckChild2Same:
  case SelectionDAGISel::OPC_CheckChild3Same:
    Result = !::CheckChildSame(Table, Index, N, RecordedNodes,
                        Table[Index-1] - SelectionDAGISel::OPC_CheckChild0Same);
    return Index;
  case SelectionDAGISel::OPC_CheckPatternPredicate:
    Result = !::CheckPatternPredicate(Table, Index, SDISel);
    return Index;
  case SelectionDAGISel::OPC_CheckPredicate:
    Result = !::CheckNodePredicate(Table, Index, SDISel, N.getNode());
    return Index;
  case SelectionDAGISel::OPC_CheckOpcode:
    Result = !::CheckOpcode(Table, Index, N.getNode());
    return Index;
  case SelectionDAGISel::OPC_CheckType:
    Result = !::CheckType(Table, Index, N, SDISel.TLI,
                          SDISel.CurDAG->getDataLayout());
    return Index;
  case SelectionDAGISel::OPC_CheckTypeRes: {
    unsigned Res = Table[Index++];
    Result = !::CheckType(Table, Index, N.getValue(Res), SDISel.TLI,
                          SDISel.CurDAG->getDataLayout());
    return Index;
  }
  case SelectionDAGISel::OPC_CheckChild0Type:
  case SelectionDAGISel::OPC_CheckChild1Type:
  case SelectionDAGISel::OPC_CheckChild2Type:
  case SelectionDAGISel::OPC_CheckChild3Type:
  case SelectionDAGISel::OPC_CheckChild4Type:
  case SelectionDAGISel::OPC_CheckChild5Type:
  case SelectionDAGISel::OPC_CheckChild6Type:
  case SelectionDAGISel::OPC_CheckChild7Type:
    Result = !::CheckChildType(
                 Table, Index, N, SDISel.TLI, SDISel.CurDAG->getDataLayout(),
                 Table[Index - 1] - SelectionDAGISel::OPC_CheckChild0Type);
    return Index;
  case SelectionDAGISel::OPC_CheckCondCode:
    Result = !::CheckCondCode(Table, Index, N);
    return Index;
  case SelectionDAGISel::OPC_CheckChild2CondCode:
    Result = !::CheckChild2CondCode(Table, Index, N);
    return Index;
  case SelectionDAGISel::OPC_CheckValueType:
    Result = !::CheckValueType(Table, Index, N, SDISel.TLI,
                               SDISel.CurDAG->getDataLayout());
    return Index;
  case SelectionDAGISel::OPC_CheckInteger:
    Result = !::CheckInteger(Table, Index, N);
    return Index;
  case SelectionDAGISel::OPC_CheckChild0Integer:
  case SelectionDAGISel::OPC_CheckChild1Integer:
  case SelectionDAGISel::OPC_CheckChild2Integer:
  case SelectionDAGISel::OPC_CheckChild3Integer:
  case SelectionDAGISel::OPC_CheckChild4Integer:
    Result = !::CheckChildInteger(Table, Index, N,
                     Table[Index-1] - SelectionDAGISel::OPC_CheckChild0Integer);
    return Index;
  case SelectionDAGISel::OPC_CheckAndImm:
    Result = !::CheckAndImm(Table, Index, N, SDISel);
    return Index;
  case SelectionDAGISel::OPC_CheckOrImm:
    Result = !::CheckOrImm(Table, Index, N, SDISel);
    return Index;
  }
}

namespace {

struct MatchScope {
  /// FailIndex - If this match fails, this is the index to continue with.
  unsigned FailIndex;

  /// NodeStack - The node stack when the scope was formed.
  SmallVector<SDValue, 4> NodeStack;

  /// NumRecordedNodes - The number of recorded nodes when the scope was formed.
  unsigned NumRecordedNodes;

  /// NumMatchedMemRefs - The number of matched memref entries.
  unsigned NumMatchedMemRefs;

  /// InputChain/InputGlue - The current chain/glue
  SDValue InputChain, InputGlue;

  /// HasChainNodesMatched - True if the ChainNodesMatched list is non-empty.
  bool HasChainNodesMatched;
};

/// \A DAG update listener to keep the matching state
/// (i.e. RecordedNodes and MatchScope) uptodate if the target is allowed to
/// change the DAG while matching.  X86 addressing mode matcher is an example
/// for this.
class MatchStateUpdater : public SelectionDAG::DAGUpdateListener
{
  SDNode **NodeToMatch;
  SmallVectorImpl<std::pair<SDValue, SDNode *>> &RecordedNodes;
  SmallVectorImpl<MatchScope> &MatchScopes;

public:
  MatchStateUpdater(SelectionDAG &DAG, SDNode **NodeToMatch,
                    SmallVectorImpl<std::pair<SDValue, SDNode *>> &RN,
                    SmallVectorImpl<MatchScope> &MS)
      : SelectionDAG::DAGUpdateListener(DAG), NodeToMatch(NodeToMatch),
        RecordedNodes(RN), MatchScopes(MS) {}

  void NodeDeleted(SDNode *N, SDNode *E) override {
    // Some early-returns here to avoid the search if we deleted the node or
    // if the update comes from MorphNodeTo (MorphNodeTo is the last thing we
    // do, so it's unnecessary to update matching state at that point).
    // Neither of these can occur currently because we only install this
    // update listener during matching a complex patterns.
    if (!E || E->isMachineOpcode())
      return;
    // Check if NodeToMatch was updated.
    if (N == *NodeToMatch)
      *NodeToMatch = E;
    // Performing linear search here does not matter because we almost never
    // run this code.  You'd have to have a CSE during complex pattern
    // matching.
    for (auto &I : RecordedNodes)
      if (I.first.getNode() == N)
        I.first.setNode(E);

    for (auto &I : MatchScopes)
      for (auto &J : I.NodeStack)
        if (J.getNode() == N)
          J.setNode(E);
  }
};

} // end anonymous namespace

void SelectionDAGISel::SelectCodeCommon(SDNode *NodeToMatch,
                                        const unsigned char *MatcherTable,
                                        unsigned TableSize) {
  // FIXME: Should these even be selected?  Handle these cases in the caller?
  switch (NodeToMatch->getOpcode()) {
  default:
    break;
  case ISD::EntryToken:       // These nodes remain the same.
  case ISD::BasicBlock:
  case ISD::Register:
  case ISD::RegisterMask:
  case ISD::HANDLENODE:
  case ISD::MDNODE_SDNODE:
  case ISD::TargetConstant:
  case ISD::TargetConstantFP:
  case ISD::TargetConstantPool:
  case ISD::TargetFrameIndex:
  case ISD::TargetExternalSymbol:
  case ISD::MCSymbol:
  case ISD::TargetBlockAddress:
  case ISD::TargetJumpTable:
  case ISD::TargetGlobalTLSAddress:
  case ISD::TargetGlobalAddress:
  case ISD::TokenFactor:
  case ISD::CopyFromReg:
  case ISD::CopyToReg:
  case ISD::EH_LABEL:
  case ISD::ANNOTATION_LABEL:
  case ISD::LIFETIME_START:
  case ISD::LIFETIME_END:
  case ISD::PSEUDO_PROBE:
    NodeToMatch->setNodeId(-1); // Mark selected.
    return;
  case ISD::AssertSext:
  case ISD::AssertZext:
  case ISD::AssertAlign:
    ReplaceUses(SDValue(NodeToMatch, 0), NodeToMatch->getOperand(0));
    CurDAG->RemoveDeadNode(NodeToMatch);
    return;
  case ISD::INLINEASM:
  case ISD::INLINEASM_BR:
    Select_INLINEASM(NodeToMatch);
    return;
  case ISD::READ_REGISTER:
    Select_READ_REGISTER(NodeToMatch);
    return;
  case ISD::WRITE_REGISTER:
    Select_WRITE_REGISTER(NodeToMatch);
    return;
  case ISD::UNDEF:
    Select_UNDEF(NodeToMatch);
    return;
  case ISD::FREEZE:
    Select_FREEZE(NodeToMatch);
    return;
  case ISD::ARITH_FENCE:
    Select_ARITH_FENCE(NodeToMatch);
    return;
  }

  assert(!NodeToMatch->isMachineOpcode() && "Node already selected!");

  // Set up the node stack with NodeToMatch as the only node on the stack.
  SmallVector<SDValue, 8> NodeStack;
  SDValue N = SDValue(NodeToMatch, 0);
  NodeStack.push_back(N);

  // MatchScopes - Scopes used when matching, if a match failure happens, this
  // indicates where to continue checking.
  SmallVector<MatchScope, 8> MatchScopes;

  // RecordedNodes - This is the set of nodes that have been recorded by the
  // state machine.  The second value is the parent of the node, or null if the
  // root is recorded.
  SmallVector<std::pair<SDValue, SDNode*>, 8> RecordedNodes;

  // MatchedMemRefs - This is the set of MemRef's we've seen in the input
  // pattern.
  SmallVector<MachineMemOperand*, 2> MatchedMemRefs;

  // These are the current input chain and glue for use when generating nodes.
  // Various Emit operations change these.  For example, emitting a copytoreg
  // uses and updates these.
  SDValue InputChain, InputGlue;

  // ChainNodesMatched - If a pattern matches nodes that have input/output
  // chains, the OPC_EmitMergeInputChains operation is emitted which indicates
  // which ones they are.  The result is captured into this list so that we can
  // update the chain results when the pattern is complete.
  SmallVector<SDNode*, 3> ChainNodesMatched;

  LLVM_DEBUG(dbgs() << "ISEL: Starting pattern match\n");

  // Determine where to start the interpreter.  Normally we start at opcode #0,
  // but if the state machine starts with an OPC_SwitchOpcode, then we
  // accelerate the first lookup (which is guaranteed to be hot) with the
  // OpcodeOffset table.
  unsigned MatcherIndex = 0;

  if (!OpcodeOffset.empty()) {
    // Already computed the OpcodeOffset table, just index into it.
    if (N.getOpcode() < OpcodeOffset.size())
      MatcherIndex = OpcodeOffset[N.getOpcode()];
    LLVM_DEBUG(dbgs() << "  Initial Opcode index to " << MatcherIndex << "\n");

  } else if (MatcherTable[0] == OPC_SwitchOpcode) {
    // Otherwise, the table isn't computed, but the state machine does start
    // with an OPC_SwitchOpcode instruction.  Populate the table now, since this
    // is the first time we're selecting an instruction.
    unsigned Idx = 1;
    while (true) {
      // Get the size of this case.
      unsigned CaseSize = MatcherTable[Idx++];
      if (CaseSize & 128)
        CaseSize = GetVBR(CaseSize, MatcherTable, Idx);
      if (CaseSize == 0) break;

      // Get the opcode, add the index to the table.
      uint16_t Opc = MatcherTable[Idx++];
      Opc |= (unsigned short)MatcherTable[Idx++] << 8;
      if (Opc >= OpcodeOffset.size())
        OpcodeOffset.resize((Opc+1)*2);
      OpcodeOffset[Opc] = Idx;
      Idx += CaseSize;
    }

    // Okay, do the lookup for the first opcode.
    if (N.getOpcode() < OpcodeOffset.size())
      MatcherIndex = OpcodeOffset[N.getOpcode()];
  }

  while (true) {
    assert(MatcherIndex < TableSize && "Invalid index");
#ifndef NDEBUG
    unsigned CurrentOpcodeIndex = MatcherIndex;
#endif
    BuiltinOpcodes Opcode = (BuiltinOpcodes)MatcherTable[MatcherIndex++];
    switch (Opcode) {
    case OPC_Scope: {
      // Okay, the semantics of this operation are that we should push a scope
      // then evaluate the first child.  However, pushing a scope only to have
      // the first check fail (which then pops it) is inefficient.  If we can
      // determine immediately that the first check (or first several) will
      // immediately fail, don't even bother pushing a scope for them.
      unsigned FailIndex;

      while (true) {
        unsigned NumToSkip = MatcherTable[MatcherIndex++];
        if (NumToSkip & 128)
          NumToSkip = GetVBR(NumToSkip, MatcherTable, MatcherIndex);
        // Found the end of the scope with no match.
        if (NumToSkip == 0) {
          FailIndex = 0;
          break;
        }

        FailIndex = MatcherIndex+NumToSkip;

        unsigned MatcherIndexOfPredicate = MatcherIndex;
        (void)MatcherIndexOfPredicate; // silence warning.

        // If we can't evaluate this predicate without pushing a scope (e.g. if
        // it is a 'MoveParent') or if the predicate succeeds on this node, we
        // push the scope and evaluate the full predicate chain.
        bool Result;
        MatcherIndex = IsPredicateKnownToFail(MatcherTable, MatcherIndex, N,
                                              Result, *this, RecordedNodes);
        if (!Result)
          break;

        LLVM_DEBUG(
            dbgs() << "  Skipped scope entry (due to false predicate) at "
                   << "index " << MatcherIndexOfPredicate << ", continuing at "
                   << FailIndex << "\n");
        ++NumDAGIselRetries;

        // Otherwise, we know that this case of the Scope is guaranteed to fail,
        // move to the next case.
        MatcherIndex = FailIndex;
      }

      // If the whole scope failed to match, bail.
      if (FailIndex == 0) break;

      // Push a MatchScope which indicates where to go if the first child fails
      // to match.
      MatchScope NewEntry;
      NewEntry.FailIndex = FailIndex;
      NewEntry.NodeStack.append(NodeStack.begin(), NodeStack.end());
      NewEntry.NumRecordedNodes = RecordedNodes.size();
      NewEntry.NumMatchedMemRefs = MatchedMemRefs.size();
      NewEntry.InputChain = InputChain;
      NewEntry.InputGlue = InputGlue;
      NewEntry.HasChainNodesMatched = !ChainNodesMatched.empty();
      MatchScopes.push_back(NewEntry);
      continue;
    }
    case OPC_RecordNode: {
      // Remember this node, it may end up being an operand in the pattern.
      SDNode *Parent = nullptr;
      if (NodeStack.size() > 1)
        Parent = NodeStack[NodeStack.size()-2].getNode();
      RecordedNodes.push_back(std::make_pair(N, Parent));
      continue;
    }

    case OPC_RecordChild0: case OPC_RecordChild1:
    case OPC_RecordChild2: case OPC_RecordChild3:
    case OPC_RecordChild4: case OPC_RecordChild5:
    case OPC_RecordChild6: case OPC_RecordChild7: {
      unsigned ChildNo = Opcode-OPC_RecordChild0;
      if (ChildNo >= N.getNumOperands())
        break;  // Match fails if out of range child #.

      RecordedNodes.push_back(std::make_pair(N->getOperand(ChildNo),
                                             N.getNode()));
      continue;
    }
    case OPC_RecordMemRef:
      if (auto *MN = dyn_cast<MemSDNode>(N))
        MatchedMemRefs.push_back(MN->getMemOperand());
      else {
        LLVM_DEBUG(dbgs() << "Expected MemSDNode "; N->dump(CurDAG);
                   dbgs() << '\n');
      }

      continue;

    case OPC_CaptureGlueInput:
      // If the current node has an input glue, capture it in InputGlue.
      if (N->getNumOperands() != 0 &&
          N->getOperand(N->getNumOperands()-1).getValueType() == MVT::Glue)
        InputGlue = N->getOperand(N->getNumOperands()-1);
      continue;

    case OPC_MoveChild: {
      unsigned ChildNo = MatcherTable[MatcherIndex++];
      if (ChildNo >= N.getNumOperands())
        break;  // Match fails if out of range child #.
      N = N.getOperand(ChildNo);
      NodeStack.push_back(N);
      continue;
    }

    case OPC_MoveChild0: case OPC_MoveChild1:
    case OPC_MoveChild2: case OPC_MoveChild3:
    case OPC_MoveChild4: case OPC_MoveChild5:
    case OPC_MoveChild6: case OPC_MoveChild7: {
      unsigned ChildNo = Opcode-OPC_MoveChild0;
      if (ChildNo >= N.getNumOperands())
        break;  // Match fails if out of range child #.
      N = N.getOperand(ChildNo);
      NodeStack.push_back(N);
      continue;
    }

    case OPC_MoveParent:
      // Pop the current node off the NodeStack.
      NodeStack.pop_back();
      assert(!NodeStack.empty() && "Node stack imbalance!");
      N = NodeStack.back();
      continue;

    case OPC_CheckSame:
      if (!::CheckSame(MatcherTable, MatcherIndex, N, RecordedNodes)) break;
      continue;

    case OPC_CheckChild0Same: case OPC_CheckChild1Same:
    case OPC_CheckChild2Same: case OPC_CheckChild3Same:
      if (!::CheckChildSame(MatcherTable, MatcherIndex, N, RecordedNodes,
                            Opcode-OPC_CheckChild0Same))
        break;
      continue;

    case OPC_CheckPatternPredicate:
      if (!::CheckPatternPredicate(MatcherTable, MatcherIndex, *this)) break;
      continue;
    case OPC_CheckPredicate:
      if (!::CheckNodePredicate(MatcherTable, MatcherIndex, *this,
                                N.getNode()))
        break;
      continue;
    case OPC_CheckPredicateWithOperands: {
      unsigned OpNum = MatcherTable[MatcherIndex++];
      SmallVector<SDValue, 8> Operands;

      for (unsigned i = 0; i < OpNum; ++i)
        Operands.push_back(RecordedNodes[MatcherTable[MatcherIndex++]].first);

      unsigned PredNo = MatcherTable[MatcherIndex++];
      if (!CheckNodePredicateWithOperands(N.getNode(), PredNo, Operands))
        break;
      continue;
    }
    case OPC_CheckComplexPat: {
      unsigned CPNum = MatcherTable[MatcherIndex++];
      unsigned RecNo = MatcherTable[MatcherIndex++];
      assert(RecNo < RecordedNodes.size() && "Invalid CheckComplexPat");

      // If target can modify DAG during matching, keep the matching state
      // consistent.
      std::unique_ptr<MatchStateUpdater> MSU;
      if (ComplexPatternFuncMutatesDAG())
        MSU.reset(new MatchStateUpdater(*CurDAG, &NodeToMatch, RecordedNodes,
                                        MatchScopes));

      if (!CheckComplexPattern(NodeToMatch, RecordedNodes[RecNo].second,
                               RecordedNodes[RecNo].first, CPNum,
                               RecordedNodes))
        break;
      continue;
    }
    case OPC_CheckOpcode:
      if (!::CheckOpcode(MatcherTable, MatcherIndex, N.getNode())) break;
      continue;

    case OPC_CheckType:
      if (!::CheckType(MatcherTable, MatcherIndex, N, TLI,
                       CurDAG->getDataLayout()))
        break;
      continue;

    case OPC_CheckTypeRes: {
      unsigned Res = MatcherTable[MatcherIndex++];
      if (!::CheckType(MatcherTable, MatcherIndex, N.getValue(Res), TLI,
                       CurDAG->getDataLayout()))
        break;
      continue;
    }

    case OPC_SwitchOpcode: {
      unsigned CurNodeOpcode = N.getOpcode();
      unsigned SwitchStart = MatcherIndex-1; (void)SwitchStart;
      unsigned CaseSize;
      while (true) {
        // Get the size of this case.
        CaseSize = MatcherTable[MatcherIndex++];
        if (CaseSize & 128)
          CaseSize = GetVBR(CaseSize, MatcherTable, MatcherIndex);
        if (CaseSize == 0) break;

        uint16_t Opc = MatcherTable[MatcherIndex++];
        Opc |= (unsigned short)MatcherTable[MatcherIndex++] << 8;

        // If the opcode matches, then we will execute this case.
        if (CurNodeOpcode == Opc)
          break;

        // Otherwise, skip over this case.
        MatcherIndex += CaseSize;
      }

      // If no cases matched, bail out.
      if (CaseSize == 0) break;

      // Otherwise, execute the case we found.
      LLVM_DEBUG(dbgs() << "  OpcodeSwitch from " << SwitchStart << " to "
                        << MatcherIndex << "\n");
      continue;
    }

    case OPC_SwitchType: {
      MVT CurNodeVT = N.getSimpleValueType();
      unsigned SwitchStart = MatcherIndex-1; (void)SwitchStart;
      unsigned CaseSize;
      while (true) {
        // Get the size of this case.
        CaseSize = MatcherTable[MatcherIndex++];
        if (CaseSize & 128)
          CaseSize = GetVBR(CaseSize, MatcherTable, MatcherIndex);
        if (CaseSize == 0) break;

        MVT CaseVT = (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
        if (CaseVT == MVT::iPTR)
          CaseVT = TLI->getPointerTy(CurDAG->getDataLayout());

        // If the VT matches, then we will execute this case.
        if (CurNodeVT == CaseVT)
          break;

        // Otherwise, skip over this case.
        MatcherIndex += CaseSize;
      }

      // If no cases matched, bail out.
      if (CaseSize == 0) break;

      // Otherwise, execute the case we found.
      LLVM_DEBUG(dbgs() << "  TypeSwitch[" << EVT(CurNodeVT).getEVTString()
                        << "] from " << SwitchStart << " to " << MatcherIndex
                        << '\n');
      continue;
    }
    case OPC_CheckChild0Type: case OPC_CheckChild1Type:
    case OPC_CheckChild2Type: case OPC_CheckChild3Type:
    case OPC_CheckChild4Type: case OPC_CheckChild5Type:
    case OPC_CheckChild6Type: case OPC_CheckChild7Type:
      if (!::CheckChildType(MatcherTable, MatcherIndex, N, TLI,
                            CurDAG->getDataLayout(),
                            Opcode - OPC_CheckChild0Type))
        break;
      continue;
    case OPC_CheckCondCode:
      if (!::CheckCondCode(MatcherTable, MatcherIndex, N)) break;
      continue;
    case OPC_CheckChild2CondCode:
      if (!::CheckChild2CondCode(MatcherTable, MatcherIndex, N)) break;
      continue;
    case OPC_CheckValueType:
      if (!::CheckValueType(MatcherTable, MatcherIndex, N, TLI,
                            CurDAG->getDataLayout()))
        break;
      continue;
    case OPC_CheckInteger:
      if (!::CheckInteger(MatcherTable, MatcherIndex, N)) break;
      continue;
    case OPC_CheckChild0Integer: case OPC_CheckChild1Integer:
    case OPC_CheckChild2Integer: case OPC_CheckChild3Integer:
    case OPC_CheckChild4Integer:
      if (!::CheckChildInteger(MatcherTable, MatcherIndex, N,
                               Opcode-OPC_CheckChild0Integer)) break;
      continue;
    case OPC_CheckAndImm:
      if (!::CheckAndImm(MatcherTable, MatcherIndex, N, *this)) break;
      continue;
    case OPC_CheckOrImm:
      if (!::CheckOrImm(MatcherTable, MatcherIndex, N, *this)) break;
      continue;
    case OPC_CheckImmAllOnesV:
      if (!ISD::isConstantSplatVectorAllOnes(N.getNode()))
        break;
      continue;
    case OPC_CheckImmAllZerosV:
      if (!ISD::isConstantSplatVectorAllZeros(N.getNode()))
        break;
      continue;

    case OPC_CheckFoldableChainNode: {
      assert(NodeStack.size() != 1 && "No parent node");
      // Verify that all intermediate nodes between the root and this one have
      // a single use (ignoring chains, which are handled in UpdateChains).
      bool HasMultipleUses = false;
      for (unsigned i = 1, e = NodeStack.size()-1; i != e; ++i) {
        unsigned NNonChainUses = 0;
        SDNode *NS = NodeStack[i].getNode();
        for (auto UI = NS->use_begin(), UE = NS->use_end(); UI != UE; ++UI)
          if (UI.getUse().getValueType() != MVT::Other)
            if (++NNonChainUses > 1) {
              HasMultipleUses = true;
              break;
            }
        if (HasMultipleUses) break;
      }
      if (HasMultipleUses) break;

      // Check to see that the target thinks this is profitable to fold and that
      // we can fold it without inducing cycles in the graph.
      if (!IsProfitableToFold(N, NodeStack[NodeStack.size()-2].getNode(),
                              NodeToMatch) ||
          !IsLegalToFold(N, NodeStack[NodeStack.size()-2].getNode(),
                         NodeToMatch, OptLevel,
                         true/*We validate our own chains*/))
        break;

      continue;
    }
    case OPC_EmitInteger:
    case OPC_EmitStringInteger: {
      MVT::SimpleValueType VT =
        (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
      int64_t Val = MatcherTable[MatcherIndex++];
      if (Val & 128)
        Val = GetVBR(Val, MatcherTable, MatcherIndex);
      if (Opcode == OPC_EmitInteger)
        Val = decodeSignRotatedValue(Val);
      RecordedNodes.push_back(std::pair<SDValue, SDNode*>(
                              CurDAG->getTargetConstant(Val, SDLoc(NodeToMatch),
                                                        VT), nullptr));
      continue;
    }
    case OPC_EmitRegister: {
      MVT::SimpleValueType VT =
        (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
      unsigned RegNo = MatcherTable[MatcherIndex++];
      RecordedNodes.push_back(std::pair<SDValue, SDNode*>(
                              CurDAG->getRegister(RegNo, VT), nullptr));
      continue;
    }
    case OPC_EmitRegister2: {
      // For targets w/ more than 256 register names, the register enum
      // values are stored in two bytes in the matcher table (just like
      // opcodes).
      MVT::SimpleValueType VT =
        (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
      unsigned RegNo = MatcherTable[MatcherIndex++];
      RegNo |= MatcherTable[MatcherIndex++] << 8;
      RecordedNodes.push_back(std::pair<SDValue, SDNode*>(
                              CurDAG->getRegister(RegNo, VT), nullptr));
      continue;
    }

    case OPC_EmitConvertToTarget:  {
      // Convert from IMM/FPIMM to target version.
      unsigned RecNo = MatcherTable[MatcherIndex++];
      assert(RecNo < RecordedNodes.size() && "Invalid EmitConvertToTarget");
      SDValue Imm = RecordedNodes[RecNo].first;

      if (Imm->getOpcode() == ISD::Constant) {
        const ConstantInt *Val=cast<ConstantSDNode>(Imm)->getConstantIntValue();
        Imm = CurDAG->getTargetConstant(*Val, SDLoc(NodeToMatch),
                                        Imm.getValueType());
      } else if (Imm->getOpcode() == ISD::ConstantFP) {
        const ConstantFP *Val=cast<ConstantFPSDNode>(Imm)->getConstantFPValue();
        Imm = CurDAG->getTargetConstantFP(*Val, SDLoc(NodeToMatch),
                                          Imm.getValueType());
      }

      RecordedNodes.push_back(std::make_pair(Imm, RecordedNodes[RecNo].second));
      continue;
    }

    case OPC_EmitMergeInputChains1_0:    // OPC_EmitMergeInputChains, 1, 0
    case OPC_EmitMergeInputChains1_1:    // OPC_EmitMergeInputChains, 1, 1
    case OPC_EmitMergeInputChains1_2: {  // OPC_EmitMergeInputChains, 1, 2
      // These are space-optimized forms of OPC_EmitMergeInputChains.
      assert(!InputChain.getNode() &&
             "EmitMergeInputChains should be the first chain producing node");
      assert(ChainNodesMatched.empty() &&
             "Should only have one EmitMergeInputChains per match");

      // Read all of the chained nodes.
      unsigned RecNo = Opcode - OPC_EmitMergeInputChains1_0;
      assert(RecNo < RecordedNodes.size() && "Invalid EmitMergeInputChains");
      ChainNodesMatched.push_back(RecordedNodes[RecNo].first.getNode());

      // FIXME: What if other value results of the node have uses not matched
      // by this pattern?
      if (ChainNodesMatched.back() != NodeToMatch &&
          !RecordedNodes[RecNo].first.hasOneUse()) {
        ChainNodesMatched.clear();
        break;
      }

      // Merge the input chains if they are not intra-pattern references.
      InputChain = HandleMergeInputChains(ChainNodesMatched, CurDAG);

      if (!InputChain.getNode())
        break;  // Failed to merge.
      continue;
    }

    case OPC_EmitMergeInputChains: {
      assert(!InputChain.getNode() &&
             "EmitMergeInputChains should be the first chain producing node");
      // This node gets a list of nodes we matched in the input that have
      // chains.  We want to token factor all of the input chains to these nodes
      // together.  However, if any of the input chains is actually one of the
      // nodes matched in this pattern, then we have an intra-match reference.
      // Ignore these because the newly token factored chain should not refer to
      // the old nodes.
      unsigned NumChains = MatcherTable[MatcherIndex++];
      assert(NumChains != 0 && "Can't TF zero chains");

      assert(ChainNodesMatched.empty() &&
             "Should only have one EmitMergeInputChains per match");

      // Read all of the chained nodes.
      for (unsigned i = 0; i != NumChains; ++i) {
        unsigned RecNo = MatcherTable[MatcherIndex++];
        assert(RecNo < RecordedNodes.size() && "Invalid EmitMergeInputChains");
        ChainNodesMatched.push_back(RecordedNodes[RecNo].first.getNode());

        // FIXME: What if other value results of the node have uses not matched
        // by this pattern?
        if (ChainNodesMatched.back() != NodeToMatch &&
            !RecordedNodes[RecNo].first.hasOneUse()) {
          ChainNodesMatched.clear();
          break;
        }
      }

      // If the inner loop broke out, the match fails.
      if (ChainNodesMatched.empty())
        break;

      // Merge the input chains if they are not intra-pattern references.
      InputChain = HandleMergeInputChains(ChainNodesMatched, CurDAG);

      if (!InputChain.getNode())
        break;  // Failed to merge.

      continue;
    }

    case OPC_EmitCopyToReg:
    case OPC_EmitCopyToReg2: {
      unsigned RecNo = MatcherTable[MatcherIndex++];
      assert(RecNo < RecordedNodes.size() && "Invalid EmitCopyToReg");
      unsigned DestPhysReg = MatcherTable[MatcherIndex++];
      if (Opcode == OPC_EmitCopyToReg2)
        DestPhysReg |= MatcherTable[MatcherIndex++] << 8;

      if (!InputChain.getNode())
        InputChain = CurDAG->getEntryNode();

      InputChain = CurDAG->getCopyToReg(InputChain, SDLoc(NodeToMatch),
                                        DestPhysReg, RecordedNodes[RecNo].first,
                                        InputGlue);

      InputGlue = InputChain.getValue(1);
      continue;
    }

    case OPC_EmitNodeXForm: {
      unsigned XFormNo = MatcherTable[MatcherIndex++];
      unsigned RecNo = MatcherTable[MatcherIndex++];
      assert(RecNo < RecordedNodes.size() && "Invalid EmitNodeXForm");
      SDValue Res = RunSDNodeXForm(RecordedNodes[RecNo].first, XFormNo);
      RecordedNodes.push_back(std::pair<SDValue,SDNode*>(Res, nullptr));
      continue;
    }
    case OPC_Coverage: {
      // This is emitted right before MorphNode/EmitNode.
      // So it should be safe to assume that this node has been selected
      unsigned index = MatcherTable[MatcherIndex++];
      index |= (MatcherTable[MatcherIndex++] << 8);
      dbgs() << "COVERED: " << getPatternForIndex(index) << "\n";
      dbgs() << "INCLUDED: " << getIncludePathForIndex(index) << "\n";
      continue;
    }

    case OPC_EmitNode:     case OPC_MorphNodeTo:
    case OPC_EmitNode0:    case OPC_EmitNode1:    case OPC_EmitNode2:
    case OPC_MorphNodeTo0: case OPC_MorphNodeTo1: case OPC_MorphNodeTo2: {
      uint16_t TargetOpc = MatcherTable[MatcherIndex++];
      TargetOpc |= (unsigned short)MatcherTable[MatcherIndex++] << 8;
      unsigned EmitNodeInfo = MatcherTable[MatcherIndex++];
      // Get the result VT list.
      unsigned NumVTs;
      // If this is one of the compressed forms, get the number of VTs based
      // on the Opcode. Otherwise read the next byte from the table.
      if (Opcode >= OPC_MorphNodeTo0 && Opcode <= OPC_MorphNodeTo2)
        NumVTs = Opcode - OPC_MorphNodeTo0;
      else if (Opcode >= OPC_EmitNode0 && Opcode <= OPC_EmitNode2)
        NumVTs = Opcode - OPC_EmitNode0;
      else
        NumVTs = MatcherTable[MatcherIndex++];
      SmallVector<EVT, 4> VTs;
      for (unsigned i = 0; i != NumVTs; ++i) {
        MVT::SimpleValueType VT =
          (MVT::SimpleValueType)MatcherTable[MatcherIndex++];
        if (VT == MVT::iPTR)
          VT = TLI->getPointerTy(CurDAG->getDataLayout()).SimpleTy;
        VTs.push_back(VT);
      }

      if (EmitNodeInfo & OPFL_Chain)
        VTs.push_back(MVT::Other);
      if (EmitNodeInfo & OPFL_GlueOutput)
        VTs.push_back(MVT::Glue);

      // This is hot code, so optimize the two most common cases of 1 and 2
      // results.
      SDVTList VTList;
      if (VTs.size() == 1)
        VTList = CurDAG->getVTList(VTs[0]);
      else if (VTs.size() == 2)
        VTList = CurDAG->getVTList(VTs[0], VTs[1]);
      else
        VTList = CurDAG->getVTList(VTs);

      // Get the operand list.
      unsigned NumOps = MatcherTable[MatcherIndex++];
      SmallVector<SDValue, 8> Ops;
      for (unsigned i = 0; i != NumOps; ++i) {
        unsigned RecNo = MatcherTable[MatcherIndex++];
        if (RecNo & 128)
          RecNo = GetVBR(RecNo, MatcherTable, MatcherIndex);

        assert(RecNo < RecordedNodes.size() && "Invalid EmitNode");
        Ops.push_back(RecordedNodes[RecNo].first);
      }

      // If there are variadic operands to add, handle them now.
      if (EmitNodeInfo & OPFL_VariadicInfo) {
        // Determine the start index to copy from.
        unsigned FirstOpToCopy = getNumFixedFromVariadicInfo(EmitNodeInfo);
        FirstOpToCopy += (EmitNodeInfo & OPFL_Chain) ? 1 : 0;
        assert(NodeToMatch->getNumOperands() >= FirstOpToCopy &&
               "Invalid variadic node");
        // Copy all of the variadic operands, not including a potential glue
        // input.
        for (unsigned i = FirstOpToCopy, e = NodeToMatch->getNumOperands();
             i != e; ++i) {
          SDValue V = NodeToMatch->getOperand(i);
          if (V.getValueType() == MVT::Glue) break;
          Ops.push_back(V);
        }
      }

      // If this has chain/glue inputs, add them.
      if (EmitNodeInfo & OPFL_Chain)
        Ops.push_back(InputChain);
      if ((EmitNodeInfo & OPFL_GlueInput) && InputGlue.getNode() != nullptr)
        Ops.push_back(InputGlue);

      // Check whether any matched node could raise an FP exception.  Since all
      // such nodes must have a chain, it suffices to check ChainNodesMatched.
      // We need to perform this check before potentially modifying one of the
      // nodes via MorphNode.
      bool MayRaiseFPException = false;
      for (auto *N : ChainNodesMatched)
        if (mayRaiseFPException(N) && !N->getFlags().hasNoFPExcept()) {
          MayRaiseFPException = true;
          break;
        }

      // Create the node.
      MachineSDNode *Res = nullptr;
      bool IsMorphNodeTo = Opcode == OPC_MorphNodeTo ||
                     (Opcode >= OPC_MorphNodeTo0 && Opcode <= OPC_MorphNodeTo2);
      if (!IsMorphNodeTo) {
        // If this is a normal EmitNode command, just create the new node and
        // add the results to the RecordedNodes list.
        Res = CurDAG->getMachineNode(TargetOpc, SDLoc(NodeToMatch),
                                     VTList, Ops);

        // Add all the non-glue/non-chain results to the RecordedNodes list.
        for (unsigned i = 0, e = VTs.size(); i != e; ++i) {
          if (VTs[i] == MVT::Other || VTs[i] == MVT::Glue) break;
          RecordedNodes.push_back(std::pair<SDValue,SDNode*>(SDValue(Res, i),
                                                             nullptr));
        }
      } else {
        assert(NodeToMatch->getOpcode() != ISD::DELETED_NODE &&
               "NodeToMatch was removed partway through selection");
        SelectionDAG::DAGNodeDeletedListener NDL(*CurDAG, [&](SDNode *N,
                                                              SDNode *E) {
          CurDAG->salvageDebugInfo(*N);
          auto &Chain = ChainNodesMatched;
          assert((!E || !is_contained(Chain, N)) &&
                 "Chain node replaced during MorphNode");
          llvm::erase_value(Chain, N);
        });
        Res = cast<MachineSDNode>(MorphNode(NodeToMatch, TargetOpc, VTList,
                                            Ops, EmitNodeInfo));
      }

      // Set the NoFPExcept flag when no original matched node could
      // raise an FP exception, but the new node potentially might.
      if (!MayRaiseFPException && mayRaiseFPException(Res)) {
        SDNodeFlags Flags = Res->getFlags();
        Flags.setNoFPExcept(true);
        Res->setFlags(Flags);
      }

      // If the node had chain/glue results, update our notion of the current
      // chain and glue.
      if (EmitNodeInfo & OPFL_GlueOutput) {
        InputGlue = SDValue(Res, VTs.size()-1);
        if (EmitNodeInfo & OPFL_Chain)
          InputChain = SDValue(Res, VTs.size()-2);
      } else if (EmitNodeInfo & OPFL_Chain)
        InputChain = SDValue(Res, VTs.size()-1);

      // If the OPFL_MemRefs glue is set on this node, slap all of the
      // accumulated memrefs onto it.
      //
      // FIXME: This is vastly incorrect for patterns with multiple outputs
      // instructions that access memory and for ComplexPatterns that match
      // loads.
      if (EmitNodeInfo & OPFL_MemRefs) {
        // Only attach load or store memory operands if the generated
        // instruction may load or store.
        const MCInstrDesc &MCID = TII->get(TargetOpc);
        bool mayLoad = MCID.mayLoad();
        bool mayStore = MCID.mayStore();

        // We expect to have relatively few of these so just filter them into a
        // temporary buffer so that we can easily add them to the instruction.
        SmallVector<MachineMemOperand *, 4> FilteredMemRefs;
        for (MachineMemOperand *MMO : MatchedMemRefs) {
          if (MMO->isLoad()) {
            if (mayLoad)
              FilteredMemRefs.push_back(MMO);
          } else if (MMO->isStore()) {
            if (mayStore)
              FilteredMemRefs.push_back(MMO);
          } else {
            FilteredMemRefs.push_back(MMO);
          }
        }

        CurDAG->setNodeMemRefs(Res, FilteredMemRefs);
      }

      LLVM_DEBUG(if (!MatchedMemRefs.empty() && Res->memoperands_empty()) dbgs()
                     << "  Dropping mem operands\n";
                 dbgs() << "  " << (IsMorphNodeTo ? "Morphed" : "Created")
                        << " node: ";
                 Res->dump(CurDAG););

      // If this was a MorphNodeTo then we're completely done!
      if (IsMorphNodeTo) {
        // Update chain uses.
        UpdateChains(Res, InputChain, ChainNodesMatched, true);
        return;
      }
      continue;
    }

    case OPC_CompleteMatch: {
      // The match has been completed, and any new nodes (if any) have been
      // created.  Patch up references to the matched dag to use the newly
      // created nodes.
      unsigned NumResults = MatcherTable[MatcherIndex++];

      for (unsigned i = 0; i != NumResults; ++i) {
        unsigned ResSlot = MatcherTable[MatcherIndex++];
        if (ResSlot & 128)
          ResSlot = GetVBR(ResSlot, MatcherTable, MatcherIndex);

        assert(ResSlot < RecordedNodes.size() && "Invalid CompleteMatch");
        SDValue Res = RecordedNodes[ResSlot].first;

        assert(i < NodeToMatch->getNumValues() &&
               NodeToMatch->getValueType(i) != MVT::Other &&
               NodeToMatch->getValueType(i) != MVT::Glue &&
               "Invalid number of results to complete!");
        assert((NodeToMatch->getValueType(i) == Res.getValueType() ||
                NodeToMatch->getValueType(i) == MVT::iPTR ||
                Res.getValueType() == MVT::iPTR ||
                NodeToMatch->getValueType(i).getSizeInBits() ==
                    Res.getValueSizeInBits()) &&
               "invalid replacement");
        ReplaceUses(SDValue(NodeToMatch, i), Res);
      }

      // Update chain uses.
      UpdateChains(NodeToMatch, InputChain, ChainNodesMatched, false);

      // If the root node defines glue, we need to update it to the glue result.
      // TODO: This never happens in our tests and I think it can be removed /
      // replaced with an assert, but if we do it this the way the change is
      // NFC.
      if (NodeToMatch->getValueType(NodeToMatch->getNumValues() - 1) ==
              MVT::Glue &&
          InputGlue.getNode())
        ReplaceUses(SDValue(NodeToMatch, NodeToMatch->getNumValues() - 1),
                    InputGlue);

      assert(NodeToMatch->use_empty() &&
             "Didn't replace all uses of the node?");
      CurDAG->RemoveDeadNode(NodeToMatch);

      return;
    }
    }

    // If the code reached this point, then the match failed.  See if there is
    // another child to try in the current 'Scope', otherwise pop it until we
    // find a case to check.
    LLVM_DEBUG(dbgs() << "  Match failed at index " << CurrentOpcodeIndex
                      << "\n");
    ++NumDAGIselRetries;
    while (true) {
      if (MatchScopes.empty()) {
        CannotYetSelect(NodeToMatch);
        return;
      }

      // Restore the interpreter state back to the point where the scope was
      // formed.
      MatchScope &LastScope = MatchScopes.back();
      RecordedNodes.resize(LastScope.NumRecordedNodes);
      NodeStack.clear();
      NodeStack.append(LastScope.NodeStack.begin(), LastScope.NodeStack.end());
      N = NodeStack.back();

      if (LastScope.NumMatchedMemRefs != MatchedMemRefs.size())
        MatchedMemRefs.resize(LastScope.NumMatchedMemRefs);
      MatcherIndex = LastScope.FailIndex;

      LLVM_DEBUG(dbgs() << "  Continuing at " << MatcherIndex << "\n");

      InputChain = LastScope.InputChain;
      InputGlue = LastScope.InputGlue;
      if (!LastScope.HasChainNodesMatched)
        ChainNodesMatched.clear();

      // Check to see what the offset is at the new MatcherIndex.  If it is zero
      // we have reached the end of this scope, otherwise we have another child
      // in the current scope to try.
      unsigned NumToSkip = MatcherTable[MatcherIndex++];
      if (NumToSkip & 128)
        NumToSkip = GetVBR(NumToSkip, MatcherTable, MatcherIndex);

      // If we have another child in this scope to match, update FailIndex and
      // try it.
      if (NumToSkip != 0) {
        LastScope.FailIndex = MatcherIndex+NumToSkip;
        break;
      }

      // End of this scope, pop it and try the next child in the containing
      // scope.
      MatchScopes.pop_back();
    }
  }
}

/// Return whether the node may raise an FP exception.
bool SelectionDAGISel::mayRaiseFPException(SDNode *N) const {
  // For machine opcodes, consult the MCID flag.
  if (N->isMachineOpcode()) {
    const MCInstrDesc &MCID = TII->get(N->getMachineOpcode());
    return MCID.mayRaiseFPException();
  }

  // For ISD opcodes, only StrictFP opcodes may raise an FP
  // exception.
  if (N->isTargetOpcode())
    return N->isTargetStrictFPOpcode();
  return N->isStrictFPOpcode();
}

bool SelectionDAGISel::isOrEquivalentToAdd(const SDNode *N) const {
  assert(N->getOpcode() == ISD::OR && "Unexpected opcode");
  auto *C = dyn_cast<ConstantSDNode>(N->getOperand(1));
  if (!C)
    return false;

  // Detect when "or" is used to add an offset to a stack object.
  if (auto *FN = dyn_cast<FrameIndexSDNode>(N->getOperand(0))) {
    MachineFrameInfo &MFI = MF->getFrameInfo();
    Align A = MFI.getObjectAlign(FN->getIndex());
    int32_t Off = C->getSExtValue();
    // If the alleged offset fits in the zero bits guaranteed by
    // the alignment, then this or is really an add.
    return (Off >= 0) && (((A.value() - 1) & Off) == unsigned(Off));
  }
  return false;
}

void SelectionDAGISel::CannotYetSelect(SDNode *N) {
  std::string msg;
  raw_string_ostream Msg(msg);
  Msg << "Cannot select: ";

  if (N->getOpcode() != ISD::INTRINSIC_W_CHAIN &&
      N->getOpcode() != ISD::INTRINSIC_WO_CHAIN &&
      N->getOpcode() != ISD::INTRINSIC_VOID) {
    N->printrFull(Msg, CurDAG);
    Msg << "\nIn function: " << MF->getName();
  } else {
    bool HasInputChain = N->getOperand(0).getValueType() == MVT::Other;
    unsigned iid =
      cast<ConstantSDNode>(N->getOperand(HasInputChain))->getZExtValue();
    if (iid < Intrinsic::num_intrinsics)
      Msg << "intrinsic %" << Intrinsic::getBaseName((Intrinsic::ID)iid);
    else if (const TargetIntrinsicInfo *TII = TM.getIntrinsicInfo())
      Msg << "target intrinsic %" << TII->getName(iid);
    else
      Msg << "unknown intrinsic #" << iid;
  }
  report_fatal_error(Twine(Msg.str()));
}

char SelectionDAGISel::ID = 0;
