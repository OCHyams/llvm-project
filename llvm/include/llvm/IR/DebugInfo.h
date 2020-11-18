//===- DebugInfo.h - Debug Information Helpers ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines a bunch of datatypes that are useful for creating and
// walking debug info in LLVM IR form. They essentially provide wrappers around
// the information in the global variables that's needed when constructing the
// DWARF information.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IR_DEBUGINFO_H
#define LLVM_IR_DEBUGINFO_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/TinyPtrVector.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Dominators.h"

namespace llvm {

class DbgDeclareInst;
class DbgValueInst;
class Instruction;
class Module;

/// Finds all intrinsics declaring local variables as living in the memory that
/// 'V' points to. This may include a mix of dbg.declare and
/// dbg.addr intrinsics.
TinyPtrVector<DbgVariableIntrinsic *> FindDbgAddrUses(Value *V);

/// Like \c FindDbgAddrUses, but only returns dbg.declare intrinsics, not
/// dbg.addr.
TinyPtrVector<DbgDeclareInst *> FindDbgDeclareUses(Value *V);

/// Finds the llvm.dbg.value intrinsics describing a value.
void findDbgValues(SmallVectorImpl<DbgValueInst *> &DbgValues, Value *V);

/// Finds the debug info intrinsics describing a value.
void findDbgUsers(SmallVectorImpl<DbgVariableIntrinsic *> &DbgInsts, Value *V);

/// Replace all the uses of an SSA value in @llvm.dbg intrinsics with
/// undef. This is useful for signaling that a variable, e.g. has been
/// found dead and hence it's unavailable at a given program point.
/// Returns true if the dbg values have been changed.
bool replaceDbgUsesWithUndef(Value *V);

/// Find subprogram that is enclosing this scope.
DISubprogram *getDISubprogram(const MDNode *Scope);

/// Strip debug info in the module if it exists.
///
/// To do this, we remove all calls to the debugger intrinsics and any named
/// metadata for debugging. We also remove debug locations for instructions.
/// Return true if module is modified.
bool StripDebugInfo(Module &M);
bool stripDebugInfo(Function &F);

/// Downgrade the debug info in a module to contain only line table information.
///
/// In order to convert debug info to what -gline-tables-only would have
/// created, this does the following:
///   1) Delete all debug intrinsics.
///   2) Delete all non-CU named metadata debug info nodes.
///   3) Create new DebugLocs for each instruction.
///   4) Create a new CU debug info, and similarly for every metadata node
///      that's reachable from the CU debug info.
///   All debug type metadata nodes are unreachable and garbage collected.
bool stripNonLineTableDebugInfo(Module &M);

/// Update the debug locations contained within the MD_loop metadata attached
/// to the instruction \p I, if one exists. \p Updater is applied to Metadata
/// operand in the MD_loop metadata: the returned value is included in the
/// updated loop metadata node if it is non-null.
void updateLoopMetadataDebugLocations(
    Instruction &I, function_ref<Metadata *(Metadata *)> Updater);

/// Return Debug Info Metadata Version by checking module flags.
unsigned getDebugMetadataVersionFromModule(const Module &M);

/// Utility to find all debug info in a module.
///
/// DebugInfoFinder tries to list all debug info MDNodes used in a module. To
/// list debug info MDNodes used by an instruction, DebugInfoFinder uses
/// processDeclare, processValue and processLocation to handle DbgDeclareInst,
/// DbgValueInst and DbgLoc attached to instructions. processModule will go
/// through all DICompileUnits in llvm.dbg.cu and list debug info MDNodes
/// used by the CUs.
class DebugInfoFinder {
public:
  /// Process entire module and collect debug info anchors.
  void processModule(const Module &M);
  /// Process a single instruction and collect debug info anchors.
  void processInstruction(const Module &M, const Instruction &I);

  /// Process DbgVariableIntrinsic.
  void processVariable(const Module &M, const DbgVariableIntrinsic &DVI);
  /// Process debug info location.
  void processLocation(const Module &M, const DILocation *Loc);

  /// Process subprogram.
  void processSubprogram(DISubprogram *SP);

  /// Clear all lists.
  void reset();

private:
  void processCompileUnit(DICompileUnit *CU);
  void processScope(DIScope *Scope);
  void processType(DIType *DT);
  bool addCompileUnit(DICompileUnit *CU);
  bool addGlobalVariable(DIGlobalVariableExpression *DIG);
  bool addScope(DIScope *Scope);
  bool addSubprogram(DISubprogram *SP);
  bool addType(DIType *DT);

public:
  using compile_unit_iterator =
      SmallVectorImpl<DICompileUnit *>::const_iterator;
  using subprogram_iterator = SmallVectorImpl<DISubprogram *>::const_iterator;
  using global_variable_expression_iterator =
      SmallVectorImpl<DIGlobalVariableExpression *>::const_iterator;
  using type_iterator = SmallVectorImpl<DIType *>::const_iterator;
  using scope_iterator = SmallVectorImpl<DIScope *>::const_iterator;

  iterator_range<compile_unit_iterator> compile_units() const {
    return make_range(CUs.begin(), CUs.end());
  }

  iterator_range<subprogram_iterator> subprograms() const {
    return make_range(SPs.begin(), SPs.end());
  }

  iterator_range<global_variable_expression_iterator> global_variables() const {
    return make_range(GVs.begin(), GVs.end());
  }

  iterator_range<type_iterator> types() const {
    return make_range(TYs.begin(), TYs.end());
  }

  iterator_range<scope_iterator> scopes() const {
    return make_range(Scopes.begin(), Scopes.end());
  }

  unsigned compile_unit_count() const { return CUs.size(); }
  unsigned global_variable_count() const { return GVs.size(); }
  unsigned subprogram_count() const { return SPs.size(); }
  unsigned type_count() const { return TYs.size(); }
  unsigned scope_count() const { return Scopes.size(); }

private:
  SmallVector<DICompileUnit *, 8> CUs;
  SmallVector<DISubprogram *, 8> SPs;
  SmallVector<DIGlobalVariableExpression *, 8> GVs;
  SmallVector<DIType *, 8> TYs;
  SmallVector<DIScope *, 8> Scopes;
  SmallPtrSet<const MDNode *, 32> NodesSeen;
};

// Assignment Tracking (at).
namespace at {
using IVecTy = SmallVector<Instruction *, 2>;
using AVecTy = SmallVector<DbgAssignIntrinsic *, 2>;
IVecTy getAssignmentInsts(DIAssignID *ID, const Function *F);
IVecTy getAssignmentInsts(const DbgAssignIntrinsic *DAI);
inline IVecTy getAssignmentInsts(const DbgAssignIntrinsic *DAI) {
  return getAssignmentInsts(cast<DIAssignID>(DAI->getAssignId()),
                            DAI->getFunction());
}
AVecTy getAssignmentMarkers(DIAssignID *ID, const Function *F);
AVecTy getAssignmentMarkers(const Instruction *Inst);
inline AVecTy getAssignmentMarkers(const Instruction *Inst) {
  if (auto *ID = Inst->getMetadata(LLVMContext::MD_DIAssignID))
    return getAssignmentMarkers(cast<DIAssignID>(ID), Inst->getFunction());
  else
    return {};
}
/// Replace all uses (and attachments) of \p Old with \p New in \p F
void RAUW(DIAssignID *Old, DIAssignID *New, const Function *F);
/// Remove all Assignment Tracking related intrinsics and metadata from \p F.
void deleteAll(Function *F);

/// Track assignments to \p Vars between \p Start and \p End. If \p FailedToTrack
/// is non-null then allocas with untrackable stores (due ot prototype limitations)
/// get no debug intrinsics at all, and an entry is added.
void trackAssignments(
    Function::iterator Start, Function::iterator End,
    const DenseMap<const AllocaInst *, DILocalVariable *> &Vars,
    const DataLayout &DL,
    DenseSet<const AllocaInst *> *FailedToTrack = nullptr,
    bool DebugPrints = false);
} // namespace at
} // end namespace llvm

#endif // LLVM_IR_DEBUGINFO_H
