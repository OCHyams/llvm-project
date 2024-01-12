//===-- llvm/DebugProgramInstruction.h - Stream of debug info -------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Data structures for storing variable assignment information in LLVM. In the
// dbg.value design, a dbg.value intrinsic specifies the position in a block
// a source variable take on an LLVM Value:
//
//    %foo = add i32 1, %0
//    dbg.value(metadata i32 %foo, ...)
//    %bar = void call @ext(%foo);
//
// and all information is stored in the Value / Metadata hierachy defined
// elsewhere in LLVM. In the "DPValue" design, each instruction /may/ have a
// connection with a DbgMarker, which identifies a position immediately before the
// instruction, and each DbgMarker /may/ then have connections to DPValues which
// record the variable assignment information. To illustrate:
//
//    %foo = add i32 1, %0
//       ; foo->DbgRecordMarker == nullptr
//       ;; There are no variable assignments / debug records "in front" of
//       ;; the instruction for %foo, therefore it has no DbgRecordMarker.
//    %bar = void call @ext(%foo)
//       ; bar->DbgRecordMarker = {
//       ;   StoredDPValues = {
//       ;     DPValue(metadata i32 %foo, ...)
//       ;   }
//       ; }
//       ;; There is a debug-info record in front of the %bar instruction,
//       ;; thus it points at a DbgMarker object. That DbgMarker contains a
//       ;; DPValue in it's ilist, storing the equivalent information to the
//       ;; dbg.value above: the Value, DILocalVariable, etc.
//
// This structure separates the two concerns of the position of the debug-info
// in the function, and the Value that it refers to. It also creates a new
// "place" in-between the Value / Metadata hierachy where we can customise
// storage and allocation techniques to better suite debug-info workloads.
// NB: as of the initial prototype, none of that has actually been attempted
// yet.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_DEBUGPROGRAMINSTRUCTION_H
#define LLVM_IR_DEBUGPROGRAMINSTRUCTION_H

#include "llvm/ADT/ilist.h"
#include "llvm/ADT/ilist_node.h"
#include "llvm/ADT/iterator.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/Support/Casting.h"

namespace llvm {

class Instruction;
class BasicBlock;
class MDNode;
class Module;
class DbgVariableIntrinsic;
class DbgMarker;
class DPValue;
class raw_ostream;

/// Base class for non-instruction debug metadata records that have positions
/// within IR. Features various methods copied across from the Instruction
/// class to aid ease-of-use. DbgRecords should always be linked into a
/// DbgMarker's StoredDPValues list. The marker connects a DbgRecord back to
/// it's position in the BasicBlock.
///
/// We need a discriminator for dyn/isa casts. In order to avoid paying for a
/// vtable for "virtual" functions too, subclasses must add a new discriminator
/// value (RecordKind) and cases to a few functions in the base class:
///   deleteRecord()
///   clone()
///   both print methods
class DbgRecord : public ilist_node<DbgRecord> {
public:
  /// Marker that this DbgRecord is linked into.
  DbgMarker *Marker = nullptr;
  /// Subclass discriminator.
  enum Kind : uint8_t { ValueKind };

protected:
  DebugLoc DbgLoc;
  Kind RecordKind; ///< Subclass discriminator.

public:
  DbgRecord(Kind RecordKind, DebugLoc DL)
      : DbgLoc(DL), RecordKind(RecordKind) {}

  /// Methods requiring subclass implementations.
  ///@{
  void deleteRecord();
  DbgRecord *clone() const;
  void print(raw_ostream &O, bool IsForDebug = false) const;
  void print(raw_ostream &O, ModuleSlotTracker &MST, bool IsForDebug) const;
  ///@}

  Kind getRecordKind() const { return RecordKind; }

  void setMarker(DbgMarker *M) { Marker = M; }

  DbgMarker *getMarker() { return Marker; }
  const DbgMarker *getMarker() const { return Marker; }

  BasicBlock *getBlock();
  const BasicBlock *getBlock() const;

  Function *getFunction();
  const Function *getFunction() const;

  Module *getModule();
  const Module *getModule() const;

  LLVMContext &getContext();
  const LLVMContext &getContext() const;

  const BasicBlock *getParent() const;
  BasicBlock *getParent();

  void removeFromParent();
  void eraseFromParent();

  DebugLoc getDebugLoc() const { return DbgLoc; }
  void setDebugLoc(DebugLoc Loc) { DbgLoc = std::move(Loc); }

  void dump() const;

  using self_iterator = simple_ilist<DbgRecord>::iterator;
  using const_self_iterator = simple_ilist<DbgRecord>::const_iterator;

protected:
  /// Similarly to Value, we avoid paying the cost of a vtable
  /// by protecting the dtor and having deleteRecord dispatch
  /// cleanup.
  /// Use deleteRecord to delete a generic record.
  ~DbgRecord() = default;
};

/// Record of a variable value-assignment, aka a non instruction representation
/// of the dbg.value intrinsic.
///
/// This class inherits from DebugValueUser to allow LLVM's metadata facilities
/// to update our references to metadata beneath our feet.
class DPValue : public DbgRecord, protected DebugValueUser {
  friend class DebugValueUser;
public:
  enum class LocationType : uint8_t {
    Declare,
    Value,

    End, ///< Marks the end of the concrete types.
    Any, ///< To indicate all LocationTypes in searches.
  };
  /// Classification of the debug-info record that this DPValue represents.
  /// Essentially, "is this a dbg.value or dbg.declare?". dbg.declares are not
  /// currently supported, but it would be trivial to do so.
  /// FIXME: We could use spare padding bits from DbgRecord for this.
  LocationType Type;

  // NB: there is no explicit "Value" field in this class, it's effectively the
  // DebugValueUser superclass instead. The referred to Value can either be a
  // ValueAsMetadata or a DIArgList.

  DILocalVariable *Variable;
  DIExpression *Expression;

public:
  /// Create a new DPValue representing the intrinsic \p DVI, for example the
  /// assignment represented by a dbg.value.
  DPValue(const DbgVariableIntrinsic *DVI);
  DPValue(const DPValue &DPV);
  /// Directly construct a new DPValue representing a dbg.value intrinsic
  /// assigning \p Location to the DV / Expr / DI variable.
  DPValue(Metadata *Location, DILocalVariable *DV, DIExpression *Expr,
          const DILocation *DI, LocationType Type = LocationType::Value);

  /// Iterator for ValueAsMetadata that internally uses direct pointer iteration
  /// over either a ValueAsMetadata* or a ValueAsMetadata**, dereferencing to the
  /// ValueAsMetadata .
  class location_op_iterator
      : public iterator_facade_base<location_op_iterator,
                                    std::bidirectional_iterator_tag, Value *> {
    PointerUnion<ValueAsMetadata *, ValueAsMetadata **> I;

  public:
    location_op_iterator(ValueAsMetadata *SingleIter) : I(SingleIter) {}
    location_op_iterator(ValueAsMetadata **MultiIter) : I(MultiIter) {}

    location_op_iterator(const location_op_iterator &R) : I(R.I) {}
    location_op_iterator &operator=(const location_op_iterator &R) {
      I = R.I;
      return *this;
    }
    bool operator==(const location_op_iterator &RHS) const {
      return I == RHS.I;
    }
    const Value *operator*() const {
      ValueAsMetadata *VAM = I.is<ValueAsMetadata *>()
                                 ? I.get<ValueAsMetadata *>()
                                 : *I.get<ValueAsMetadata **>();
      return VAM->getValue();
    };
    Value *operator*() {
      ValueAsMetadata *VAM = I.is<ValueAsMetadata *>()
                                 ? I.get<ValueAsMetadata *>()
                                 : *I.get<ValueAsMetadata **>();
      return VAM->getValue();
    }
    location_op_iterator &operator++() {
      if (I.is<ValueAsMetadata *>())
        I = I.get<ValueAsMetadata *>() + 1;
      else
        I = I.get<ValueAsMetadata **>() + 1;
      return *this;
    }
    location_op_iterator &operator--() {
      if (I.is<ValueAsMetadata *>())
        I = I.get<ValueAsMetadata *>() - 1;
      else
        I = I.get<ValueAsMetadata **>() - 1;
      return *this;
    }
  };

  /// Get the locations corresponding to the variable referenced by the debug
  /// info intrinsic.  Depending on the intrinsic, this could be the
  /// variable's value or its address.
  iterator_range<location_op_iterator> location_ops() const;

  Value *getVariableLocationOp(unsigned OpIdx) const;

  void replaceVariableLocationOp(Value *OldValue, Value *NewValue,
                                 bool AllowEmpty = false);
  void replaceVariableLocationOp(unsigned OpIdx, Value *NewValue);
  /// Adding a new location operand will always result in this intrinsic using
  /// an ArgList, and must always be accompanied by a new expression that uses
  /// the new operand.
  void addVariableLocationOps(ArrayRef<Value *> NewValues,
                              DIExpression *NewExpr);

  void setVariable(DILocalVariable *NewVar) { Variable = NewVar; }

  void setExpression(DIExpression *NewExpr) { Expression = NewExpr; }

  unsigned getNumVariableLocationOps() const;

  bool hasArgList() const { return isa<DIArgList>(getRawLocation()); }
  /// Returns true if this DPValue has no empty MDNodes in its location list.
  bool hasValidLocation() const { return getVariableLocationOp(0) != nullptr; }

  /// Does this describe the address of a local variable. True for dbg.addr
  /// and dbg.declare, but not dbg.value, which describes its value.
  bool isAddressOfVariable() const { return Type != LocationType::Value; }
  LocationType getType() const { return Type; }

  void setKillLocation();
  bool isKillLocation() const;

  DILocalVariable *getVariable() const { return Variable; }

  DIExpression *getExpression() const { return Expression; }

  Metadata *getRawLocation() const { return DebugValue; }

  /// Use of this should generally be avoided; instead,
  /// replaceVariableLocationOp and addVariableLocationOps should be used where
  /// possible to avoid creating invalid state.
  void setRawLocation(Metadata *NewLocation) {
    assert(
        (isa<ValueAsMetadata>(NewLocation) || isa<DIArgList>(NewLocation) ||
         isa<MDNode>(NewLocation)) &&
        "Location for a DPValue must be either ValueAsMetadata or DIArgList");
    resetDebugValue(NewLocation);
  }

  /// Get the size (in bits) of the variable, or fragment of the variable that
  /// is described.
  std::optional<uint64_t> getFragmentSizeInBits() const;

  DPValue *clone() const;
  /// Convert this DPValue back into a dbg.value intrinsic.
  /// \p InsertBefore Optional position to insert this intrinsic.
  /// \returns A new dbg.value intrinsic representiung this DPValue.
  DbgVariableIntrinsic *createDebugIntrinsic(Module *M,
                                             Instruction *InsertBefore) const;

  /// Handle changes to the location of the Value(s) that we refer to happening
  /// "under our feet".
  void handleChangedLocation(Metadata *NewLocation);

  void print(raw_ostream &O, bool IsForDebug = false) const;
  void print(raw_ostream &ROS, ModuleSlotTracker &MST, bool IsForDebug) const;

  /// Filter the DbgRecord range to DPValue types only and downcast.
  static inline auto
  filter(iterator_range<simple_ilist<DbgRecord>::iterator> R) {
    return map_range(
        make_filter_range(R, [](DbgRecord &E) { return isa<DPValue>(E); }),
        [](DbgRecord &E) { return std::ref(cast<DPValue>(E)); });
  }

  /// Support type inquiry through isa, cast, and dyn_cast.
  static bool classof(const DbgRecord *E) {
    return E->getRecordKind() == ValueKind;
  }
};

/// Per-instruction record of debug-info. If an Instruction is the position of
/// some debugging information, it points at a DbgMarker storing that info. Each
/// marker points back at the instruction that owns it. Various utilities are
/// provided for manipulating the DbgRecords contained within this marker.
///
/// This class has a rough surface area, because it's needed to preserve the
/// one arefact that we can't yet eliminate from the intrinsic / dbg.value
/// debug-info design: the order of records is significant, and duplicates can
/// exist. Thus, if one has a run of debug-info records such as:
///    dbg.value(...
///    %foo = barinst
///    dbg.value(...
/// and remove barinst, then the dbg.values must be preserved in the correct
/// order. Hence, the use of iterators to select positions to insert things
/// into, or the occasional InsertAtHead parameter indicating that new records
/// should go at the start of the list.
///
/// There are only five or six places in LLVM that truly rely on this ordering,
/// which we can improve in the future. Additionally, many improvements in the
/// way that debug-info is stored can be achieved in this class, at a future
/// date.
class DbgMarker {
public:
  DbgMarker() {}
  /// Link back to the Instruction that owns this marker. Can be null during
  /// operations that move a marker from one instruction to another.
  Instruction *MarkedInstr = nullptr;

  /// List of DbgRecords, the non-instruction equivalent of llvm.dbg.*
  /// intrinsics. There is a one-to-one relationship between each debug
  /// intrinsic in a block and each DbgRecord once the representation has been
  /// converted, and the ordering is meaningful in the same way.
  simple_ilist<DbgRecord> StoredDPValues;
  bool empty() const { return StoredDPValues.empty(); }

  const BasicBlock *getParent() const;
  BasicBlock *getParent();

  /// Handle the removal of a marker: the position of debug-info has gone away,
  /// but the stored debug records should not. Drop them onto the next
  /// instruction, or otherwise work out what to do with them.
  void removeMarker();
  void dump() const;

  void removeFromParent();
  void eraseFromParent();

  /// Implement operator<< on DbgMarker.
  void print(raw_ostream &O, bool IsForDebug = false) const;
  void print(raw_ostream &ROS, ModuleSlotTracker &MST, bool IsForDebug) const;

  /// Produce a range over all the DPValues in this Marker.
  iterator_range<simple_ilist<DbgRecord>::iterator> getDbgValueRange();
  /// Transfer any DPValues from \p Src into this DbgMarker. If \p InsertAtHead
  /// is true, place them before existing DPValues, otherwise afterwards.
  void absorbDebugValues(DbgMarker &Src, bool InsertAtHead);
  /// Transfer the DPValues in \p Range from \p Src into this DbgMarker. If
  /// \p InsertAtHead is true, place them before existing DPValues, otherwise
  // afterwards.
  void absorbDebugValues(iterator_range<DbgRecord::self_iterator> Range,
                         DbgMarker &Src, bool InsertAtHead);
  /// Insert a DPValue into this DbgMarker, at the end of the list. If
  /// \p InsertAtHead is true, at the start.
  void insertDPValue(DbgRecord *New, bool InsertAtHead);
  /// Clone all DbgMarkers from \p From into this marker. There are numerous
  /// options to customise the source/destination, due to gnarliness, see class
  /// comment.
  /// \p FromHere If non-null, copy from FromHere to the end of From's DPValues
  /// \p InsertAtHead Place the cloned DPValues at the start of StoredDPValues
  /// \returns Range over all the newly cloned DPValues
  iterator_range<simple_ilist<DbgRecord>::iterator>
  cloneDebugInfoFrom(DbgMarker *From,
                     std::optional<simple_ilist<DbgRecord>::iterator> FromHere,
                     bool InsertAtHead = false);
  /// Erase all DPValues in this DbgMarker.
  void dropDbgValues();
  /// Erase a single DbgRecord from this marker. In an ideal future, we would
  /// never erase an assignment in this way, but it's the equivalent to
  /// erasing a debug intrinsic from a block.
  void dropOneDbgValue(DbgRecord *DPE);

  /// We generally act like all llvm Instructions have a range of DPValues
  /// attached to them, but in reality sometimes we don't allocate the DbgMarker
  /// to save time and memory, but still have to return ranges of DPValues. When
  /// we need to describe such an unallocated DPValue range, use this static
  /// markers range instead. This will bite us if someone tries to insert a
  /// DPValue in that range, but they should be using the Official (TM) API for
  /// that.
  static DbgMarker EmptyDbgMarker;
  static iterator_range<simple_ilist<DbgRecord>::iterator>
  getEmptyDPValueRange() {
    return make_range(EmptyDbgMarker.StoredDPValues.end(),
                      EmptyDbgMarker.StoredDPValues.end());
  }
};

inline raw_ostream &operator<<(raw_ostream &OS, const DbgMarker &Marker) {
  Marker.print(OS);
  return OS;
}

inline raw_ostream &operator<<(raw_ostream &OS, const DPValue &Value) {
  Value.print(OS);
  return OS;
}

} // namespace llvm

#endif // LLVM_IR_DEBUGPROGRAMINSTRUCTION_H
