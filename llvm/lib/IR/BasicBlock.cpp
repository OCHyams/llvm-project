//===-- BasicBlock.cpp - Implement BasicBlock related methods -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the BasicBlock class for the IR library.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "SymbolTableListTraitsImpl.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Compiler.h"

#include "LLVMContextImpl.h"

using namespace llvm;

#define DEBUG_TYPE "ir"
STATISTIC(NumInstrRenumberings, "Number of renumberings across all blocks");

DbgMarker *BasicBlock::createMarker(Instruction *I) {
  if (I->DebugMarker)
    return I->DebugMarker;
  DbgMarker *Marker = new DbgMarker();
  Marker->MarkedInstr = I;
  I->DebugMarker = Marker;
  return Marker;
}

DbgMarker *BasicBlock::createMarker(InstListType::iterator It) {
  if (It != end())
    return createMarker(&*It);
  DbgMarker *DM = getTrailingDbgRecords();
  if (DM)
    return DM;
  DM = new DbgMarker();
  setTrailingDbgRecords(DM);
  return DM;
}

void BasicBlock::convertToNewDbgValues() {
  // Iterate over all instructions in the instruction list, collecting debug
  // info intrinsics and converting them to DbgRecords. Once we find a "real"
  // instruction, attach all those DbgRecords to a DbgMarker in that
  // instruction.
  SmallVector<DbgRecord *, 4> DbgVarRecs;
  for (Instruction &I : make_early_inc_range(InstList)) {
    if (DbgVariableIntrinsic *DVI = dyn_cast<DbgVariableIntrinsic>(&I)) {
      // Convert this dbg.value to a DbgVariableRecord.
      DbgVariableRecord *Value = new DbgVariableRecord(DVI);
      DbgVarRecs.push_back(Value);
      DVI->eraseFromParent();
      continue;
    }

    if (DbgLabelInst *DLI = dyn_cast<DbgLabelInst>(&I)) {
      DbgVarRecs.push_back(
          new DbgLabelRecord(DLI->getLabel(), DLI->getDebugLoc()));
      DLI->eraseFromParent();
      continue;
    }

    if (DbgVarRecs.empty())
      continue;

    // Create a marker to store DbgRecords in.
    createMarker(&I);
    DbgMarker *Marker = I.DebugMarker;

    for (DbgRecord *DVR : DbgVarRecs)
      Marker->insertDbgRecord(DVR, false);

    DbgVarRecs.clear();
  }
}

void BasicBlock::convertFromNewDbgValues() {
  invalidateOrders();

  // Iterate over the block, finding instructions annotated with DbgMarkers.
  // Convert any attached DbgRecords to debug intrinsics and insert ahead of the
  // instruction.
  for (auto &Inst : *this) {
    if (!Inst.DebugMarker)
      continue;

    DbgMarker &Marker = *Inst.DebugMarker;
    for (DbgRecord &DR : Marker.getDbgRecordRange())
      InstList.insert(Inst.getIterator(),
                      DR.createDebugIntrinsic(getModule(), nullptr));

    Marker.eraseFromParent();
  }

  // Assume no trailing DbgRecords: we could technically create them at the end
  // of the block, after a terminator, but this would be non-cannonical and
  // indicates that something else is broken somewhere.
  assert(!getTrailingDbgRecords());
}

#ifndef NDEBUG
void BasicBlock::dumpDbgValues() const {
  for (auto &Inst : *this) {
    if (!Inst.DebugMarker)
      continue;

    dbgs() << "@ " << Inst.DebugMarker << " ";
    Inst.DebugMarker->dump();
  };
}
#endif

ValueSymbolTable *BasicBlock::getValueSymbolTable() {
  if (Function *F = getParent())
    return F->getValueSymbolTable();
  return nullptr;
}

LLVMContext &BasicBlock::getContext() const {
  return getType()->getContext();
}

template <> void llvm::invalidateParentIListOrdering(BasicBlock *BB) {
  BB->invalidateOrders();
}

// Explicit instantiation of SymbolTableListTraits since some of the methods
// are not in the public header file...
template class llvm::SymbolTableListTraits<
    Instruction, ilist_iterator_bits<true>, ilist_parent<BasicBlock>>;

BasicBlock::BasicBlock(LLVMContext &C, const Twine &Name, Function *NewParent,
                       BasicBlock *InsertBefore)
    : Value(Type::getLabelTy(C), Value::BasicBlockVal), Parent(nullptr) {

  if (NewParent)
    insertInto(NewParent, InsertBefore);
  else
    assert(!InsertBefore &&
           "Cannot insert block before another block with no function!");

  end().getNodePtr()->setParent(this);
  setName(Name);
}

void BasicBlock::insertInto(Function *NewParent, BasicBlock *InsertBefore) {
  assert(NewParent && "Expected a parent");
  assert(!Parent && "Already has a parent");

  if (InsertBefore)
    NewParent->insert(InsertBefore->getIterator(), this);
  else
    NewParent->insert(NewParent->end(), this);
}

BasicBlock::~BasicBlock() {
  validateInstrOrdering();

  // If the address of the block is taken and it is being deleted (e.g. because
  // it is dead), this means that there is either a dangling constant expr
  // hanging off the block, or an undefined use of the block (source code
  // expecting the address of a label to keep the block alive even though there
  // is no indirect branch).  Handle these cases by zapping the BlockAddress
  // nodes.  There are no other possible uses at this point.
  if (hasAddressTaken()) {
    assert(!use_empty() && "There should be at least one blockaddress!");
    BlockAddress *BA = cast<BlockAddress>(user_back());

    Constant *Replacement = ConstantInt::get(Type::getInt32Ty(getContext()), 1);
    BA->replaceAllUsesWith(
        ConstantExpr::getIntToPtr(Replacement, BA->getType()));
    BA->destroyConstant();
  }

  assert(getParent() == nullptr && "BasicBlock still linked into the program!");
  dropAllReferences();
  for (auto &Inst : *this) {
    if (!Inst.DebugMarker)
      continue;
    Inst.DebugMarker->eraseFromParent();
  }
  InstList.clear();
}

void BasicBlock::setParent(Function *parent) {
  // Set Parent=parent, updating instruction symtab entries as appropriate.
  if (Parent != parent)
    Number = parent ? parent->NextBlockNum++ : -1u;
  InstList.setSymTabObject(&Parent, parent);
}

void BasicBlock::removeFromParent() {
  getParent()->getBasicBlockList().remove(getIterator());
}

iplist<BasicBlock>::iterator BasicBlock::eraseFromParent() {
  return getParent()->getBasicBlockList().erase(getIterator());
}

void BasicBlock::moveBefore(SymbolTableList<BasicBlock>::iterator MovePos) {
  getParent()->splice(MovePos, getParent(), getIterator());
}

void BasicBlock::moveAfter(BasicBlock *MovePos) {
  MovePos->getParent()->splice(++MovePos->getIterator(), getParent(),
                               getIterator());
}

const Module *BasicBlock::getModule() const {
  return getParent()->getParent();
}

const DataLayout &BasicBlock::getDataLayout() const {
  return getModule()->getDataLayout();
}

const CallInst *BasicBlock::getTerminatingMustTailCall() const {
  if (InstList.empty())
    return nullptr;
  const ReturnInst *RI = dyn_cast<ReturnInst>(&InstList.back());
  if (!RI || RI == &InstList.front())
    return nullptr;

  const Instruction *Prev = RI->getPrevNode();
  if (!Prev)
    return nullptr;

  if (Value *RV = RI->getReturnValue()) {
    if (RV != Prev)
      return nullptr;

    // Look through the optional bitcast.
    if (auto *BI = dyn_cast<BitCastInst>(Prev)) {
      RV = BI->getOperand(0);
      Prev = BI->getPrevNode();
      if (!Prev || RV != Prev)
        return nullptr;
    }
  }

  if (auto *CI = dyn_cast<CallInst>(Prev)) {
    if (CI->isMustTailCall())
      return CI;
  }
  return nullptr;
}

const CallInst *BasicBlock::getTerminatingDeoptimizeCall() const {
  if (InstList.empty())
    return nullptr;
  auto *RI = dyn_cast<ReturnInst>(&InstList.back());
  if (!RI || RI == &InstList.front())
    return nullptr;

  if (auto *CI = dyn_cast_or_null<CallInst>(RI->getPrevNode()))
    if (Function *F = CI->getCalledFunction())
      if (F->getIntrinsicID() == Intrinsic::experimental_deoptimize)
        return CI;

  return nullptr;
}

const CallInst *BasicBlock::getPostdominatingDeoptimizeCall() const {
  const BasicBlock* BB = this;
  SmallPtrSet<const BasicBlock *, 8> Visited;
  Visited.insert(BB);
  while (auto *Succ = BB->getUniqueSuccessor()) {
    if (!Visited.insert(Succ).second)
      return nullptr;
    BB = Succ;
  }
  return BB->getTerminatingDeoptimizeCall();
}

const Instruction *BasicBlock::getFirstMayFaultInst() const {
  if (InstList.empty())
    return nullptr;
  for (const Instruction &I : *this)
    if (isa<LoadInst>(I) || isa<StoreInst>(I) || isa<CallBase>(I))
      return &I;
  return nullptr;
}

const Instruction* BasicBlock::getFirstNonPHI() const {
  for (const Instruction &I : *this)
    if (!isa<PHINode>(I))
      return &I;
  return nullptr;
}

Instruction *BasicBlock::getFirstNonPHI() {
  for (Instruction &I : *this)
    if (!isa<PHINode>(I))
      return &I;
  return nullptr;
}

BasicBlock::const_iterator BasicBlock::getFirstNonPHIIt() const {
  for (const Instruction &I : *this) {
    if (isa<PHINode>(I))
      continue;

    BasicBlock::const_iterator It = I.getIterator();
    // Set the head-inclusive bit to indicate that this iterator includes
    // any debug-info at the start of the block. This is a no-op unless the
    // appropriate CMake flag is set.
    It.setHeadBit(true);
    return It;
  }

  return end();
}

BasicBlock::const_iterator
BasicBlock::getFirstNonPHIOrDbg(bool SkipPseudoOp) const {
  for (const Instruction &I : *this) {
    if (isa<PHINode>(I) || isa<DbgInfoIntrinsic>(I))
      continue;

    if (SkipPseudoOp && isa<PseudoProbeInst>(I))
      continue;

    BasicBlock::const_iterator It = I.getIterator();
    // This position comes after any debug records, the head bit should remain
    // unset.
    assert(!It.getHeadBit());
    return It;
  }
  return end();
}

BasicBlock::const_iterator
BasicBlock::getFirstNonPHIOrDbgOrLifetime(bool SkipPseudoOp) const {
  for (const Instruction &I : *this) {
    if (isa<PHINode>(I) || isa<DbgInfoIntrinsic>(I))
      continue;

    if (I.isLifetimeStartOrEnd())
      continue;

    if (SkipPseudoOp && isa<PseudoProbeInst>(I))
      continue;

    BasicBlock::const_iterator It = I.getIterator();
    // This position comes after any debug records, the head bit should remain
    // unset.
    assert(!It.getHeadBit());

    return It;
  }
  return end();
}

BasicBlock::const_iterator BasicBlock::getFirstInsertionPt() const {
  const_iterator InsertPt = getFirstNonPHIIt();
  if (InsertPt == end())
    return end();

  if (InsertPt->isEHPad()) ++InsertPt;
  // Set the head-inclusive bit to indicate that this iterator includes
  // any debug-info at the start of the block. This is a no-op unless the
  // appropriate CMake flag is set.
  InsertPt.setHeadBit(true);
  return InsertPt;
}

BasicBlock::const_iterator BasicBlock::getFirstNonPHIOrDbgOrAlloca() const {
  const_iterator InsertPt = getFirstNonPHIIt();
  if (InsertPt == end())
    return end();

  if (InsertPt->isEHPad())
    ++InsertPt;

  if (isEntryBlock()) {
    const_iterator End = end();
    while (InsertPt != End &&
           (isa<AllocaInst>(*InsertPt) || isa<DbgInfoIntrinsic>(*InsertPt) ||
            isa<PseudoProbeInst>(*InsertPt))) {
      if (const AllocaInst *AI = dyn_cast<AllocaInst>(&*InsertPt)) {
        if (!AI->isStaticAlloca())
          break;
      }
      ++InsertPt;
    }
  }

  // Signal that this comes after any debug records.
  InsertPt.setHeadBit(false);
  return InsertPt;
}

void BasicBlock::dropAllReferences() {
  for (Instruction &I : *this)
    I.dropAllReferences();
}

const BasicBlock *BasicBlock::getSinglePredecessor() const {
  const_pred_iterator PI = pred_begin(this), E = pred_end(this);
  if (PI == E) return nullptr;         // No preds.
  const BasicBlock *ThePred = *PI;
  ++PI;
  return (PI == E) ? ThePred : nullptr /*multiple preds*/;
}

const BasicBlock *BasicBlock::getUniquePredecessor() const {
  const_pred_iterator PI = pred_begin(this), E = pred_end(this);
  if (PI == E) return nullptr; // No preds.
  const BasicBlock *PredBB = *PI;
  ++PI;
  for (;PI != E; ++PI) {
    if (*PI != PredBB)
      return nullptr;
    // The same predecessor appears multiple times in the predecessor list.
    // This is OK.
  }
  return PredBB;
}

bool BasicBlock::hasNPredecessors(unsigned N) const {
  return hasNItems(pred_begin(this), pred_end(this), N);
}

bool BasicBlock::hasNPredecessorsOrMore(unsigned N) const {
  return hasNItemsOrMore(pred_begin(this), pred_end(this), N);
}

const BasicBlock *BasicBlock::getSingleSuccessor() const {
  const_succ_iterator SI = succ_begin(this), E = succ_end(this);
  if (SI == E) return nullptr; // no successors
  const BasicBlock *TheSucc = *SI;
  ++SI;
  return (SI == E) ? TheSucc : nullptr /* multiple successors */;
}

const BasicBlock *BasicBlock::getUniqueSuccessor() const {
  const_succ_iterator SI = succ_begin(this), E = succ_end(this);
  if (SI == E) return nullptr; // No successors
  const BasicBlock *SuccBB = *SI;
  ++SI;
  for (;SI != E; ++SI) {
    if (*SI != SuccBB)
      return nullptr;
    // The same successor appears multiple times in the successor list.
    // This is OK.
  }
  return SuccBB;
}

iterator_range<BasicBlock::phi_iterator> BasicBlock::phis() {
  PHINode *P = empty() ? nullptr : dyn_cast<PHINode>(&*begin());
  return make_range<phi_iterator>(P, nullptr);
}

void BasicBlock::removePredecessor(BasicBlock *Pred,
                                   bool KeepOneInputPHIs) {
  // Use hasNUsesOrMore to bound the cost of this assertion for complex CFGs.
  assert((hasNUsesOrMore(16) || llvm::is_contained(predecessors(this), Pred)) &&
         "Pred is not a predecessor!");

  // Return early if there are no PHI nodes to update.
  if (empty() || !isa<PHINode>(begin()))
    return;

  unsigned NumPreds = cast<PHINode>(front()).getNumIncomingValues();
  for (PHINode &Phi : make_early_inc_range(phis())) {
    Phi.removeIncomingValue(Pred, !KeepOneInputPHIs);
    if (KeepOneInputPHIs)
      continue;

    // If we have a single predecessor, removeIncomingValue may have erased the
    // PHI node itself.
    if (NumPreds == 1)
      continue;

    // Try to replace the PHI node with a constant value.
    if (Value *PhiConstant = Phi.hasConstantValue()) {
      Phi.replaceAllUsesWith(PhiConstant);
      Phi.eraseFromParent();
    }
  }
}

bool BasicBlock::canSplitPredecessors() const {
  const_iterator FirstNonPHI = getFirstNonPHIIt();
  if (isa<LandingPadInst>(FirstNonPHI))
    return true;
  // This is perhaps a little conservative because constructs like
  // CleanupBlockInst are pretty easy to split.  However, SplitBlockPredecessors
  // cannot handle such things just yet.
  if (FirstNonPHI->isEHPad())
    return false;
  return true;
}

bool BasicBlock::isLegalToHoistInto() const {
  auto *Term = getTerminator();
  // No terminator means the block is under construction.
  if (!Term)
    return true;

  // If the block has no successors, there can be no instructions to hoist.
  assert(Term->getNumSuccessors() > 0);

  // Instructions should not be hoisted across special terminators, which may
  // have side effects or return values.
  return !Term->isSpecialTerminator();
}

bool BasicBlock::isEntryBlock() const {
  const Function *F = getParent();
  assert(F && "Block must have a parent function to use this API");
  return this == &F->getEntryBlock();
}

BasicBlock *BasicBlock::splitBasicBlock(iterator I, const Twine &BBName) {
  assert(getTerminator() && "Can't use splitBasicBlock on degenerate BB!");
  assert(I != InstList.end() &&
         "Trying to get me to create degenerate basic block!");

  BasicBlock *New = BasicBlock::Create(getContext(), BBName, getParent(),
                                       this->getNextNode());

  // Save DebugLoc of split point before invalidating iterator.
  DebugLoc Loc = I->getStableDebugLoc();
  if (Loc)
    Loc = Loc->getWithoutAtom();

  // Move all of the specified instructions from the original basic block into
  // the new basic block.
  New->splice(New->end(), this, I, end());

  // Add a branch instruction to the newly formed basic block.
  UncondBrInst *BI = UncondBrInst::Create(New, this);
  BI->setDebugLoc(Loc);

  // Now we must loop through all of the successors of the New block (which
  // _were_ the successors of the 'this' block), and update any PHI nodes in
  // successors.  If there were PHI nodes in the successors, then they need to
  // know that incoming branches will be from New, not from Old (this).
  //
  New->replaceSuccessorsPhiUsesWith(this, New);
  return New;
}

BasicBlock *BasicBlock::splitBasicBlockBefore(iterator I, const Twine &BBName) {
  assert(getTerminator() &&
         "Can't use splitBasicBlockBefore on degenerate BB!");
  assert(I != InstList.end() &&
         "Trying to get me to create degenerate basic block!");

  assert((!isa<PHINode>(*I) || getSinglePredecessor()) &&
         "cannot split on multi incoming phis");

  BasicBlock *New = BasicBlock::Create(getContext(), BBName, getParent(), this);
  // Save DebugLoc of split point before invalidating iterator.
  DebugLoc Loc = I->getDebugLoc();
  if (Loc)
    Loc = Loc->getWithoutAtom();

  // Move all of the specified instructions from the original basic block into
  // the new basic block.
  New->splice(New->end(), this, begin(), I);

  // Loop through all of the predecessors of the 'this' block (which will be the
  // predecessors of the New block), replace the specified successor 'this'
  // block to point at the New block and update any PHI nodes in 'this' block.
  // If there were PHI nodes in 'this' block, the PHI nodes are updated
  // to reflect that the incoming branches will be from the New block and not
  // from predecessors of the 'this' block.
  // Save predecessors to separate vector before modifying them.
  SmallVector<BasicBlock *, 4> Predecessors(predecessors(this));
  for (BasicBlock *Pred : Predecessors) {
    Instruction *TI = Pred->getTerminator();
    TI->replaceSuccessorWith(this, New);
    this->replacePhiUsesWith(Pred, New);
  }
  // Add a branch instruction from  "New" to "this" Block.
  UncondBrInst *BI = UncondBrInst::Create(this, New);
  BI->setDebugLoc(Loc);

  return New;
}

BasicBlock::iterator BasicBlock::erase(BasicBlock::iterator FromIt,
                                       BasicBlock::iterator ToIt) {
  for (Instruction &I : make_early_inc_range(make_range(FromIt, ToIt)))
    I.eraseFromParent();
  return ToIt;
}

void BasicBlock::replacePhiUsesWith(BasicBlock *Old, BasicBlock *New) {
  // N.B. This might not be a complete BasicBlock, so don't assume
  // that it ends with a non-phi instruction.
  for (Instruction &I : *this) {
    PHINode *PN = dyn_cast<PHINode>(&I);
    if (!PN)
      break;
    PN->replaceIncomingBlockWith(Old, New);
  }
}

void BasicBlock::replaceSuccessorsPhiUsesWith(BasicBlock *Old,
                                              BasicBlock *New) {
  Instruction *TI = getTerminatorOrNull();
  if (!TI)
    // Cope with being called on a BasicBlock that doesn't have a terminator
    // yet. Clang's CodeGenFunction::EmitReturnBlock() likes to do this.
    return;
  for (BasicBlock *Succ : successors(TI))
    Succ->replacePhiUsesWith(Old, New);
}

void BasicBlock::replaceSuccessorsPhiUsesWith(BasicBlock *New) {
  this->replaceSuccessorsPhiUsesWith(this, New);
}

bool BasicBlock::isLandingPad() const {
  return isa<LandingPadInst>(getFirstNonPHIIt());
}

const LandingPadInst *BasicBlock::getLandingPadInst() const {
  return dyn_cast<LandingPadInst>(getFirstNonPHIIt());
}

std::optional<uint64_t> BasicBlock::getIrrLoopHeaderWeight() const {
  const Instruction *TI = getTerminator();
  if (MDNode *MDIrrLoopHeader =
      TI->getMetadata(LLVMContext::MD_irr_loop)) {
    MDString *MDName = cast<MDString>(MDIrrLoopHeader->getOperand(0));
    if (MDName->getString() == "loop_header_weight") {
      auto *CI = mdconst::extract<ConstantInt>(MDIrrLoopHeader->getOperand(1));
      return std::optional<uint64_t>(CI->getValue().getZExtValue());
    }
  }
  return std::nullopt;
}

BasicBlock::iterator llvm::skipDebugIntrinsics(BasicBlock::iterator It) {
  while (isa<DbgInfoIntrinsic>(It))
    ++It;
  return It;
}

void BasicBlock::renumberInstructions() {
  unsigned Order = 0;
  for (Instruction &I : *this)
    I.Order = Order++;

  // Set the bit to indicate that the instruction order valid and cached.
  SubclassOptionalData |= InstrOrderValid;

  NumInstrRenumberings++;
}

void BasicBlock::flushTerminatorDbgRecords() {
  // If we erase the terminator in a block, any DbgRecords will sink and "fall
  // off the end", existing after any terminator that gets inserted. With
  // dbg.value intrinsics we would just insert the terminator at end() and
  // the dbg.values would come before the terminator. With DbgRecords, we must
  // do this manually.
  // To get out of this unfortunate form, whenever we insert a terminator,
  // check whether there's anything trailing at the end and move those
  // DbgRecords in front of the terminator.

  // If there's no terminator, there's nothing to do.
  Instruction *Term = getTerminatorOrNull();
  if (!Term)
    return;

  // Are there any dangling DbgRecords?
  DbgMarker *TrailingDbgRecords = getTrailingDbgRecords();
  if (!TrailingDbgRecords)
    return;

  // Transfer DbgRecords from the trailing position onto the terminator.
  createMarker(Term);
  Term->DebugMarker->absorbDebugValues(*TrailingDbgRecords, false);
  TrailingDbgRecords->eraseFromParent();
  deleteTrailingDbgRecords();
}

void BasicBlock::spliceDebugInfo(BasicBlock::iterator Dest, BasicBlock *Src,
                                 BasicBlock::iterator First,
                                 BasicBlock::iterator Last) {
  /* Do a quick normalisation before calling the real splice implementation. We
     might be operating on a degenerate basic block that has no instructions
     in it, a legitimate transient state. In that case, Dest will be end() and
     any DbgRecords temporarily stored in the TrailingDbgRecords map in
     LLVMContext. We might illustrate it thus:

                         Dest
                           |
     this-block:    ~~~~~~~~
      Src-block:            ++++B---B---B---B:::C
                                |               |
                               First           Last

     However: does the caller expect the "~" DbgRecords to end up before or
     after the spliced segment? This is communciated in the "Head" bit of Dest,
     which signals whether the caller called begin() or end() on this block.

     If the head bit is set, then all is well, we leave DbgRecords trailing just
     like how dbg.value instructions would trail after instructions spliced to
     the beginning of this block.

     If the head bit isn't set, then try to jam the "~" DbgRecords onto the
     front of the First instruction, then splice like normal, which joins the
     "~" DbgRecords with the "+" DbgRecords. However if the "+" DbgRecords are
     supposed to be left behind in Src, then:
      * detach the "+" DbgRecords,
      * move the "~" DbgRecords onto First,
      * splice like normal,
      * replace the "+" DbgRecords onto the Last position.
     Complicated, but gets the job done. */

  // If we're inserting at end(), and not in front of dangling DbgRecords, then
  // move the DbgRecords onto "First". They'll then be moved naturally in the
  // splice process.
  DbgMarker *MoreDanglingDbgRecords = nullptr;
  DbgMarker *OurTrailingDbgRecords = getTrailingDbgRecords();
  if (Dest == end() && !Dest.getHeadBit() && OurTrailingDbgRecords) {
    // Are the "+" DbgRecords not supposed to move? If so, detach them
    // temporarily.
    if (!First.getHeadBit() && First->hasDbgRecords()) {
      MoreDanglingDbgRecords = Src->getMarker(First);
      MoreDanglingDbgRecords->removeFromParent();
    }

    if (First->hasDbgRecords()) {
      // Place them at the front, it would look like this:
      //            Dest
      //              |
      // this-block:
      // Src-block: ~~~~~~~~++++B---B---B---B:::C
      //                        |               |
      //                       First           Last
      First->adoptDbgRecords(this, end(), true);
    } else {
      // No current marker, create one and absorb in. (FIXME: we can avoid an
      // allocation in the future).
      DbgMarker *CurMarker = Src->createMarker(&*First);
      CurMarker->absorbDebugValues(*OurTrailingDbgRecords, false);
      OurTrailingDbgRecords->eraseFromParent();
    }
    deleteTrailingDbgRecords();
    First.setHeadBit(true);
  }

  // Call the main debug-info-splicing implementation.
  spliceDebugInfoImpl(this, Dest, Src, First, Last);

  // Do we have some "+" DbgRecords hanging around that weren't supposed to
  // move, and we detached to make things easier?
  if (!MoreDanglingDbgRecords)
    return;

  // FIXME: we could avoid an allocation here sometimes. (adoptDbgRecords
  // requires an iterator).
  DbgMarker *LastMarker = Src->createMarker(Last);
  LastMarker->absorbDebugValues(*MoreDanglingDbgRecords, true);
  MoreDanglingDbgRecords->eraseFromParent();
}

void BasicBlock::splice(iterator Dest, BasicBlock *Src, iterator First,
                        iterator Last) {
#ifdef EXPENSIVE_CHECKS
  // Check that First is before Last.
  auto FromBBEnd = Src->end();
  for (auto It = First; It != Last; ++It)
    assert(It != FromBBEnd && "FromBeginIt not before FromEndIt!");
#endif // EXPENSIVE_CHECKS

  // Lots of horrible special casing for empty transfers: the dbg.values between
  // two positions could be spliced in dbg.value mode.
  if (First == Last) {
    spliceDebugInfoEmptyBlock(this, Dest, Src, First, Last);
    return;
  }

  spliceDebugInfo(Dest, Src, First, Last);

  // And move the instructions.
  getInstList().splice(Dest, Src->getInstList(), First, Last);

  flushTerminatorDbgRecords();
}

void BasicBlock::insertDbgRecordAfter(DbgRecord *DR, Instruction *I) {
  assert(I->getParent() == this);

  iterator NextIt = std::next(I->getIterator());
  DbgMarker *NextMarker = createMarker(NextIt);
  NextMarker->insertDbgRecord(DR, true);
}

void BasicBlock::insertDbgRecordBefore(DbgRecord *DR,
                                       InstListType::iterator Where) {
  assert(Where == end() || Where->getParent() == this);
  bool InsertAtHead = Where.getHeadBit();
  DbgMarker *M = createMarker(Where);
  M->insertDbgRecord(DR, InsertAtHead);
}

DbgMarker *BasicBlock::getNextMarker(Instruction *I) {
  return getMarker(std::next(I->getIterator()));
}

DbgMarker *BasicBlock::getMarker(InstListType::iterator It) {
  if (It == end()) {
    DbgMarker *DM = getTrailingDbgRecords();
    return DM;
  }
  return It->DebugMarker;
}

void BasicBlock::reinsertInstInDbgRecords(
    Instruction *I, std::optional<DbgRecord::self_iterator> Pos) {
  // "I" was originally removed from a position where it was
  // immediately in front of Pos. Any DbgRecords on that position then "fell
  // down" onto Pos. "I" has been re-inserted at the front of that wedge of
  // DbgRecords, shuffle them around to represent the original positioning. To
  // illustrate:
  //
  //   Instructions:  I1---I---I0
  //       DbgRecords:    DDD DDD
  //
  // Instruction "I" removed,
  //
  //   Instructions:  I1------I0
  //       DbgRecords:    DDDDDD
  //                       ^Pos
  //
  // Instruction "I" re-inserted (now):
  //
  //   Instructions:  I1---I------I0
  //       DbgRecords:        DDDDDD
  //                           ^Pos
  //
  // After this method completes:
  //
  //   Instructions:  I1---I---I0
  //       DbgRecords:    DDD DDD

  // This happens if there were no DbgRecords on I0. Are there now DbgRecords
  // there?
  if (!Pos) {
    DbgMarker *NextMarker = getNextMarker(I);
    if (!NextMarker)
      return;
    if (NextMarker->StoredDbgRecords.empty())
      return;
    // There are DbgMarkers there now -- they fell down from "I".
    DbgMarker *ThisMarker = createMarker(I);
    ThisMarker->absorbDebugValues(*NextMarker, false);
    return;
  }

  // Is there even a range of DbgRecords to move?
  DbgMarker *DM = (*Pos)->getMarker();
  auto Range = make_range(DM->StoredDbgRecords.begin(), (*Pos));
  if (Range.begin() == Range.end())
    return;

  // Otherwise: splice.
  DbgMarker *ThisMarker = createMarker(I);
  assert(ThisMarker->StoredDbgRecords.empty());
  ThisMarker->absorbDebugValues(Range, *DM, true);
}

#ifndef NDEBUG
/// In asserts builds, this checks the numbering. In non-asserts builds, it
/// is defined as a no-op inline function in BasicBlock.h.
void BasicBlock::validateInstrOrdering() const {
  if (!isInstrOrderValid())
    return;
  const Instruction *Prev = nullptr;
  for (const Instruction &I : *this) {
    assert((!Prev || Prev->comesBefore(&I)) &&
           "cached instruction ordering is incorrect");
    Prev = &I;
  }
}
#endif

void BasicBlock::setTrailingDbgRecords(DbgMarker *foo) {
  getContext().pImpl->setTrailingDbgRecords(this, foo);
}

DbgMarker *BasicBlock::getTrailingDbgRecords() {
  return getContext().pImpl->getTrailingDbgRecords(this);
}

void BasicBlock::deleteTrailingDbgRecords() {
  getContext().pImpl->deleteTrailingDbgRecords(this);
}
