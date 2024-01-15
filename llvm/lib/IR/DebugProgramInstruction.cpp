//======- DebugProgramInstruction.cpp - Implement DbgRecord/DbgMarkers -======//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugProgramInstruction.h"
#include "llvm/IR/DIBuilder.h"
#include "llvm/IR/IntrinsicInst.h"

namespace llvm {

DbgVariableRecord::DbgVariableRecord(const DbgVariableIntrinsic *DVI)
    : DbgRecord(ValueKind, DVI->getDebugLoc()),
      DebugValueUser(DVI->getRawLocation()), Variable(DVI->getVariable()),
      Expression(DVI->getExpression()) {
  switch (DVI->getIntrinsicID()) {
  case Intrinsic::dbg_value:
    Type = LocationType::Value;
    break;
  case Intrinsic::dbg_declare:
    Type = LocationType::Declare;
    break;
  default:
    llvm_unreachable(
        "Trying to create a DbgRecord with an invalid intrinsic type!");
  }
}

DbgVariableRecord::DbgVariableRecord(const DbgVariableRecord &DPV)
    : DbgRecord(ValueKind, DPV.getDebugLoc()),
      DebugValueUser(DPV.getRawLocation()), Type(DPV.getType()),
      Variable(DPV.getVariable()), Expression(DPV.getExpression()) {}

DbgVariableRecord::DbgVariableRecord(Metadata *Location, DILocalVariable *DV,
                                     DIExpression *Expr, const DILocation *DI,
                                     LocationType Type)
    : DbgRecord(ValueKind, DI), DebugValueUser(Location), Type(Type),
      Variable(DV), Expression(Expr) {}

void DbgRecord::deleteRecord() {
  switch (RecordKind) {
  case ValueKind:
    delete cast<DbgVariableRecord>(this);
    break;
  default:
    llvm_unreachable("unsupported record kind");
  }
}
void DbgRecord::print(raw_ostream &O, bool IsForDebug) const {
  switch (RecordKind) {
  case ValueKind:
    cast<DPValue>(this)->print(O, IsForDebug);
    break;
  default:
    llvm_unreachable("unsupported record kind");
  };
}

void DbgRecord::print(raw_ostream &O, ModuleSlotTracker &MST,
                      bool IsForDebug) const {
  switch (RecordKind) {
  case ValueKind:
    cast<DPValue>(this)->print(O, MST, IsForDebug);
    break;
  default:
    llvm_unreachable("unsupported record kind");
  };
}

iterator_range<DbgVariableRecord::location_op_iterator>
DbgVariableRecord::location_ops() const {
  auto *MD = getRawLocation();
  // If a Value has been deleted, the "location" for this record will be
  // replaced by nullptr. Return an empty range.
  if (!MD)
    return {location_op_iterator(static_cast<ValueAsMetadata *>(nullptr)),
            location_op_iterator(static_cast<ValueAsMetadata *>(nullptr))};

  // If operand is ValueAsMetadata, return a range over just that operand.
  if (auto *VAM = dyn_cast<ValueAsMetadata>(MD))
    return {location_op_iterator(VAM), location_op_iterator(VAM + 1)};

  // If operand is DIArgList, return a range over its args.
  if (auto *AL = dyn_cast<DIArgList>(MD))
    return {location_op_iterator(AL->args_begin()),
            location_op_iterator(AL->args_end())};

  // Operand is an empty metadata tuple, so return empty iterator.
  assert(cast<MDNode>(MD)->getNumOperands() == 0);
  return {location_op_iterator(static_cast<ValueAsMetadata *>(nullptr)),
          location_op_iterator(static_cast<ValueAsMetadata *>(nullptr))};
}

unsigned DbgVariableRecord::getNumVariableLocationOps() const {
  if (hasArgList())
    return cast<DIArgList>(getRawLocation())->getArgs().size();
  return 1;
}

Value *DbgVariableRecord::getVariableLocationOp(unsigned OpIdx) const {
  auto *MD = getRawLocation();
  if (!MD)
    return nullptr;

  if (auto *AL = dyn_cast<DIArgList>(MD))
    return AL->getArgs()[OpIdx]->getValue();
  if (isa<MDNode>(MD))
    return nullptr;
  assert(isa<ValueAsMetadata>(MD) &&
         "Attempted to get location operand from DbgVariableRecord with none.");
  auto *V = cast<ValueAsMetadata>(MD);
  assert(OpIdx == 0 && "Operand Index must be 0 for a debug intrinsic with a "
                       "single location operand.");
  return V->getValue();
}

static ValueAsMetadata *getAsMetadata(Value *V) {
  return isa<MetadataAsValue>(V) ? dyn_cast<ValueAsMetadata>(
                                       cast<MetadataAsValue>(V)->getMetadata())
                                 : ValueAsMetadata::get(V);
}

void DbgVariableRecord::replaceVariableLocationOp(Value *OldValue,
                                                  Value *NewValue,
                                                  bool AllowEmpty) {
  assert(NewValue && "Values must be non-null");
  auto Locations = location_ops();
  auto OldIt = find(Locations, OldValue);
  if (OldIt == Locations.end()) {
    if (AllowEmpty)
      return;
    llvm_unreachable("OldValue must be a current location");
  }

  if (!hasArgList()) {
    // Set our location to be the MAV wrapping the new Value.
    setRawLocation(isa<MetadataAsValue>(NewValue)
                       ? cast<MetadataAsValue>(NewValue)->getMetadata()
                       : ValueAsMetadata::get(NewValue));
    return;
  }

  // We must be referring to a DIArgList, produce a new operands vector with the
  // old value replaced, generate a new DIArgList and set it as our location.
  SmallVector<ValueAsMetadata *, 4> MDs;
  ValueAsMetadata *NewOperand = getAsMetadata(NewValue);
  for (auto *VMD : Locations)
    MDs.push_back(VMD == *OldIt ? NewOperand : getAsMetadata(VMD));
  setRawLocation(DIArgList::get(getVariableLocationOp(0)->getContext(), MDs));
}

void DbgVariableRecord::replaceVariableLocationOp(unsigned OpIdx,
                                                  Value *NewValue) {
  assert(OpIdx < getNumVariableLocationOps() && "Invalid Operand Index");

  if (!hasArgList()) {
    setRawLocation(isa<MetadataAsValue>(NewValue)
                       ? cast<MetadataAsValue>(NewValue)->getMetadata()
                       : ValueAsMetadata::get(NewValue));
    return;
  }

  SmallVector<ValueAsMetadata *, 4> MDs;
  ValueAsMetadata *NewOperand = getAsMetadata(NewValue);
  for (unsigned Idx = 0; Idx < getNumVariableLocationOps(); ++Idx)
    MDs.push_back(Idx == OpIdx ? NewOperand
                               : getAsMetadata(getVariableLocationOp(Idx)));

  setRawLocation(DIArgList::get(getVariableLocationOp(0)->getContext(), MDs));
}

void DbgVariableRecord::addVariableLocationOps(ArrayRef<Value *> NewValues,
                                               DIExpression *NewExpr) {
  assert(NewExpr->hasAllLocationOps(getNumVariableLocationOps() +
                                    NewValues.size()) &&
         "NewExpr for debug variable intrinsic does not reference every "
         "location operand.");
  assert(!is_contained(NewValues, nullptr) && "New values must be non-null");
  setExpression(NewExpr);
  SmallVector<ValueAsMetadata *, 4> MDs;
  for (auto *VMD : location_ops())
    MDs.push_back(getAsMetadata(VMD));
  for (auto *VMD : NewValues)
    MDs.push_back(getAsMetadata(VMD));
  setRawLocation(DIArgList::get(getVariableLocationOp(0)->getContext(), MDs));
}

void DbgVariableRecord::setKillLocation() {
  // TODO: When/if we remove duplicate values from DIArgLists, we don't need
  // this set anymore.
  SmallPtrSet<Value *, 4> RemovedValues;
  for (Value *OldValue : location_ops()) {
    if (!RemovedValues.insert(OldValue).second)
      continue;
    Value *Poison = PoisonValue::get(OldValue->getType());
    replaceVariableLocationOp(OldValue, Poison);
  }
}

bool DbgVariableRecord::isKillLocation() const {
  return (getNumVariableLocationOps() == 0 &&
          !getExpression()->isComplex()) ||
         any_of(location_ops(), [](Value *V) { return isa<UndefValue>(V); });
}

std::optional<uint64_t> DbgVariableRecord::getFragmentSizeInBits() const {
  if (auto Fragment = getExpression()->getFragmentInfo())
    return Fragment->SizeInBits;
  return getVariable()->getSizeInBits();
}

DbgRecord *DbgRecord::clone() const {
  switch (RecordKind) {
  case ValueKind:
    return cast<DbgVariableRecord>(this)->clone();
  default:
    llvm_unreachable("unsupported record kind");
  };
}

DbgVariableRecord *DbgVariableRecord::clone() const {
  return new DbgVariableRecord(*this);
}

DbgVariableIntrinsic *
DbgVariableRecord::createDebugIntrinsic(Module *M,
                                        Instruction *InsertBefore) const {
  [[maybe_unused]] DICompileUnit *Unit =
      getDebugLoc().get()->getScope()->getSubprogram()->getUnit();
  assert(M && Unit &&
         "Cannot clone from BasicBlock that is not part of a Module or "
         "DICompileUnit!");
  LLVMContext &Context = getDebugLoc()->getContext();
  Value *Args[] = {MetadataAsValue::get(Context, getRawLocation()),
                   MetadataAsValue::get(Context, getVariable()),
                   MetadataAsValue::get(Context, getExpression())};
  Function *IntrinsicFn;

  // Work out what sort of intrinsic we're going to produce.
  switch (getType()) {
  case DbgVariableRecord::LocationType::Declare:
    IntrinsicFn = Intrinsic::getDeclaration(M, Intrinsic::dbg_declare);
    break;
  case DbgVariableRecord::LocationType::Value:
    IntrinsicFn = Intrinsic::getDeclaration(M, Intrinsic::dbg_value);
    break;
  case DbgVariableRecord::LocationType::End:
  case DbgVariableRecord::LocationType::Any:
    llvm_unreachable("Invalid LocationType");
    break;
  }

  // Create the intrinsic from this records's information, optionally insert
  // into the target location.
  DbgVariableIntrinsic *DVI = cast<DbgVariableIntrinsic>(
      CallInst::Create(IntrinsicFn->getFunctionType(), IntrinsicFn, Args));
  DVI->setTailCall();
  DVI->setDebugLoc(getDebugLoc());
  if (InsertBefore)
    DVI->insertBefore(InsertBefore);

  return DVI;
}

void DbgVariableRecord::handleChangedLocation(Metadata *NewLocation) {
  resetDebugValue(NewLocation);
}

const BasicBlock *DbgRecord::getParent() const {
  return Marker->MarkedInstr->getParent();
}

BasicBlock *DbgRecord::getParent() { return Marker->MarkedInstr->getParent(); }

BasicBlock *DbgRecord::getBlock() { return Marker->getParent(); }

const BasicBlock *DbgRecord::getBlock() const { return Marker->getParent(); }

Function *DbgRecord::getFunction() { return getBlock()->getParent(); }

const Function *DbgRecord::getFunction() const {
  return getBlock()->getParent();
}

Module *DbgRecord::getModule() { return getFunction()->getParent(); }

const Module *DbgRecord::getModule() const {
  return getFunction()->getParent();
}

LLVMContext &DbgRecord::getContext() { return getBlock()->getContext(); }

const LLVMContext &DbgRecord::getContext() const {
  return getBlock()->getContext();
}

///////////////////////////////////////////////////////////////////////////////

// An empty, global, DbgMarker for the purpose of describing empty ranges of
// DbgRecords.
DbgMarker DbgMarker::EmptyDbgMarker;

void DbgMarker::dropDbgRecords() {
  while (!StoredDbgRecords.empty()) {
    auto It = StoredDbgRecords.begin();
    DbgRecord *DPR = &*It;
    StoredDbgRecords.erase(It);
    DPR->deleteRecord();
  }
}

void DbgMarker::dropOneDbgRecord(DbgRecord *DPR) {
  assert(DPR->getMarker() == this);
  StoredDbgRecords.erase(DPR->getIterator());
  DPR->deleteRecord();
}

const BasicBlock *DbgMarker::getParent() const {
  return MarkedInstr->getParent();
}

BasicBlock *DbgMarker::getParent() { return MarkedInstr->getParent(); }

void DbgMarker::removeMarker() {
  // Are there any records in this DbgMarker? If not, nothing to preserve.
  Instruction *Owner = MarkedInstr;
  if (StoredDbgRecords.empty()) {
    eraseFromParent();
    Owner->DbgRecordMarker = nullptr;
    return;
  }

  // The attached records need to be preserved; attach them to the next
  // instruction. If there isn't a next instruction, put them on the
  // "trailing" list.
  DbgMarker *NextMarker = Owner->getParent()->getNextMarker(Owner);
  if (NextMarker == nullptr) {
    NextMarker = new DbgMarker();
    Owner->getParent()->setTrailingDbgRecords(NextMarker);
  }
  NextMarker->absorbDbgRecords(*this, true);

  eraseFromParent();
}

void DbgMarker::removeFromParent() {
  MarkedInstr->DbgRecordMarker = nullptr;
  MarkedInstr = nullptr;
}

void DbgMarker::eraseFromParent() {
  if (MarkedInstr)
    removeFromParent();
  dropDbgRecords();
  delete this;
}

iterator_range<DbgRecord::self_iterator> DbgMarker::getDbgValueRange() {
  return make_range(StoredDbgRecords.begin(), StoredDbgRecords.end());
}

void DbgRecord::removeFromParent() {
  getMarker()->StoredDbgRecords.erase(getIterator());
}

void DbgRecord::eraseFromParent() {
  removeFromParent();
  deleteRecord();
}

void DbgMarker::insertDbgRecord(DbgRecord *New, bool InsertAtHead) {
  auto It = InsertAtHead ? StoredDbgRecords.begin() : StoredDbgRecords.end();
  StoredDbgRecords.insert(It, *New);
  New->setMarker(this);
}

void DbgMarker::absorbDbgRecords(DbgMarker &Src, bool InsertAtHead) {
  auto It = InsertAtHead ? StoredDbgRecords.begin() : StoredDbgRecords.end();
  for (DbgRecord &DPV : Src.StoredDbgRecords)
    DPV.setMarker(this);

  StoredDbgRecords.splice(It, Src.StoredDbgRecords);
}

void DbgMarker::absorbDbgRecords(iterator_range<DbgRecord::self_iterator> Range,
                                 DbgMarker &Src, bool InsertAtHead) {
  for (DbgRecord &DPR : Range)
    DPR.setMarker(this);

  auto InsertPos =
      (InsertAtHead) ? StoredDbgRecords.begin() : StoredDbgRecords.end();

  StoredDbgRecords.splice(InsertPos, Src.StoredDbgRecords, Range.begin(),
                          Range.end());
}

iterator_range<simple_ilist<DbgRecord>::iterator> DbgMarker::cloneDebugInfoFrom(
    DbgMarker *From, std::optional<simple_ilist<DbgRecord>::iterator> from_here,
    bool InsertAtHead) {
  DbgRecord *First = nullptr;
  // Work out what range of records to clone: normally all the contents of the
  // "From" marker, optionally we can start from the from_here position down to
  // end().
  auto Range =
      make_range(From->StoredDbgRecords.begin(), From->StoredDbgRecords.end());
  if (from_here.has_value())
    Range = make_range(*from_here, From->StoredDbgRecords.end());

  // Clone each record and insert into StoredDbgRecords; optionally place them
  // at the start or the end of the list.
  auto Pos = (InsertAtHead) ? StoredDbgRecords.begin() : StoredDbgRecords.end();
  for (DbgRecord &DPR : Range) {
    DbgRecord *New = DPR.clone();
    New->setMarker(this);
    StoredDbgRecords.insert(Pos, *New);
    if (!First)
      First = New;
  }

  if (!First)
    return {StoredDbgRecords.end(), StoredDbgRecords.end()};

  if (InsertAtHead)
    // If InsertAtHead is set, we cloned a range onto the front of of the
    // StoredDbgRecords collection, return that range.
    return {StoredDbgRecords.begin(), Pos};
  else
    // We inserted a block at the end, return that range.
    return {First->getIterator(), StoredDbgRecords.end()};
}

} // end namespace llvm
