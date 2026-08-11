//===- llvm/IR/DebugInfoODRUniquer.h - Debug info metadata ------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines a class used to merge debug info for ODR types.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Metadata.h"
#include <tuple>

extern bool Uniquify;

namespace llvm {
class Metadata;

/// Dense set/map find_as key for use alongside DISubprogramODRInfo to
/// merge function declarations of ODR types.
struct DISubprogramODRKey {
  Metadata *Scope;
  StringRef LinkageName;
  Metadata *Type; // ??? why did I put this in here
  // TODO: Can we remove TemplateParams?
  Metadata *TemplateParams;

  DISubprogramODRKey(Metadata *Scope, StringRef LinkageName, Metadata *Type,
                     Metadata *TemplateParams)
      : Scope(Scope), LinkageName(LinkageName), Type(nullptr),
        TemplateParams(TemplateParams) {}
  DISubprogramODRKey(DISubprogram *SP)
      : Scope(SP->getRawScope()), LinkageName(SP->getLinkageName()),
        Type(nullptr), TemplateParams(SP->getRawTemplateParams()) {}
};

/// Dense set/map info to merge function declarations of ODR types.
struct DISubprogramODRInfo {
  static unsigned getHashValue(const DISubprogramODRKey &SP) {
    // xxx should we remove LinkageName for hash speed?
    return hash_combine(SP.Scope, SP.LinkageName, SP.Type, SP.TemplateParams);
  }

  static bool isEqual(const DISubprogramODRKey &LHS, const DISubprogram *RHS) {
    if (!LHS.Scope || LHS.LinkageName.empty())
      return false;
    auto *CT = dyn_cast_or_null<DICompositeType>(LHS.Scope);
    if (!CT || !CT->getRawIdentifier())
      return false;

    if (!RHS->getRawLinkageName())
      return false;

    return /*LHS->isDefinition() == RHS->isDefinition() &&*/
        LHS.Scope == RHS->getRawScope() &&
        LHS.LinkageName == RHS->getLinkageName() &&
        LHS.Type == RHS->getRawType() &&
        LHS.TemplateParams == RHS->getRawTemplateParams();
  }

  static bool isEqual(const DISubprogram *LHS, const DISubprogram *RHS) {
    if (LHS->isDefinition() || !LHS->getRawScope() || !LHS->getRawLinkageName())
      return false;

    auto *CT = dyn_cast_or_null<DICompositeType>(LHS->getRawScope());
    if (!CT || !CT->getRawIdentifier())
      return false;

    return LHS->isDefinition() == RHS->isDefinition() &&
           LHS->getRawScope() == RHS->getRawScope() &&
           LHS->getRawLinkageName() == RHS->getRawLinkageName() &&
           LHS->getRawType() == RHS->getRawType() &&
           LHS->getRawTemplateParams() == RHS->getRawTemplateParams();
  }
};

class DebugInfoODRUniquer {
  /// Function declarations scoped to ODR types.
  DenseSet<DISubprogram *, DISubprogramODRInfo> FnDecls;

public:
  // err I suppose we don't want to construct any unecessarily...
  // but this will do for a quick test xxx
  // DISubprogram *getODRSubprogramDecl(DISubprogram *Decl);
  DISubprogram *getODRSubprogramDecl(Metadata *Scope, StringRef LinkageName,
                                     Metadata *Type, Metadata *TemplateParams);
  void addSubprogramDecl(DISubprogram *SP);
};

} // namespace llvm