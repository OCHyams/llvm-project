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

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Metadata.h"
#include <tuple>

extern bool Uniquify;

namespace llvm {
  class Metadata;
// template <> struct MDNodeSubsetEqualImpl<DISubprogram> {
//   using KeyTy = MDNodeKeyImpl<DISubprogram>;

//   static bool isSubsetEqual(const KeyTy &LHS, const DISubprogram *RHS) {
//     return isDeclarationOfODRMember(LHS.isDefinition(), LHS.Scope,
//                                     LHS.LinkageName, LHS.TemplateParams, RHS);
//   }

//   static bool isSubsetEqual(const DISubprogram *LHS, const DISubprogram *RHS) {
//     return isDeclarationOfODRMember(LHS->isDefinition(), LHS->getRawScope(),
//                                     LHS->getRawLinkageName(),
//                                     LHS->getRawTemplateParams(), RHS);
//   }

//   /// Subprograms compare equal if they declare the same function in an ODR
//   /// type.
//   static bool isDeclarationOfODRMember(bool IsDefinition, const Metadata *Scope,
//                                        const MDString *LinkageName,
//                                        const Metadata *TemplateParams,
//                                        const DISubprogram *RHS) {
//     // Check whether the LHS is eligible.
//     if (IsDefinition || !Scope || !LinkageName)
//       return false;

//     auto *CT = dyn_cast_or_null<DICompositeType>(Scope);
//     if (!CT || !CT->getRawIdentifier())
//       return false;

//     // Compare to the RHS.
//     // FIXME: We need to compare template parameters here to avoid incorrect
//     // collisions in mapMetadata when RF_ReuseAndMutateDistinctMDs and a
//     // ODR-DISubprogram has a non-ODR template parameter (i.e., a
//     // DICompositeType that does not have an identifier). Eventually we should
//     // decouple ODR logic from uniquing logic.
//     return IsDefinition == RHS->isDefinition() && Scope == RHS->getRawScope() &&
//            LinkageName == RHS->getRawLinkageName() &&
//            TemplateParams == RHS->getRawTemplateParams();
//   }
// };
// template <> struct MDNodeSubsetEqualImpl<DIDerivedType> {
//   using KeyTy = MDNodeKeyImpl<DIDerivedType>;

//   static bool isSubsetEqual(const KeyTy &LHS, const DIDerivedType *RHS) {
//     return isODRMember(LHS.Tag, LHS.Scope, LHS.Name, RHS);
//   }

//   static bool isSubsetEqual(const DIDerivedType *LHS,
//                             const DIDerivedType *RHS) {
//     return isODRMember(LHS->getTag(), LHS->getRawScope(), LHS->getRawName(),
//                        RHS);
//   }

//   /// Subprograms compare equal if they declare the same function in an ODR
//   /// type.
//   static bool isODRMember(unsigned Tag, const Metadata *Scope,
//                           const MDString *Name, const DIDerivedType *RHS) {
//     // Check whether the LHS is eligible.
//     if (Tag != dwarf::DW_TAG_member || !Name)
//       return false;

//     auto *CT = dyn_cast_or_null<DICompositeType>(Scope);
//     if (!CT || !CT->getRawIdentifier())
//       return false;

//     // Compare to the RHS.
//     return Tag == RHS->getTag() && Name == RHS->getRawName() &&
//            Scope == RHS->getRawScope();
//   }
// };

struct SPLookup {
  // assume declaration
  Metadata *Scope;
  StringRef LinkageName;
  Metadata *Type;
  Metadata* TemplateParams; //err maybe we can't drop this?xxx

  // SPLookup(DISubprogram *SP) :   Scope(SP->getRawScope()),
  //                       LinkageName(SP->getRawLinkageName()),
  //                       Type(SP->getRawType()),
  //                       TemplateParams(SP->getRawTemplateParams()){}
};

struct ODRSubprogramDeclInfo {
  // FIXME: We can probably remove template parameters from here now.

  static unsigned getHashValue(const SPLookup &SP) {
    return hash_combine(SP.Scope,
                        SP.LinkageName,
                        SP.Type,
                        SP.TemplateParams);
  }

  // static unsigned getHashValue(const DISubprogram *SP) {
  //   return hash_combine(/*SP->isDefinition(),*/
  //                       SP->getRawScope(),
  //                       SP->getRawLinkageName(),
  //                       SP->getRawType(),
  //                       SP->getRawTemplateParams());
  // }

  static bool isEqual(const SPLookup &LHS,
                      const DISubprogram *RHS) {
        // assume LHS declaration
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

  static bool isEqual(const DISubprogram *LHS,
                      const DISubprogram *RHS) {
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
  // struct DISubprogramODRKey {
  //   bool IsDefinition;
  //   Metadata *Scope;
  //   Metadata *LinkageName;
  //   Metadata *TemplateParams;
  //   DISubprogramODRKey(bool IsDefinition, Metadata *Scope, Metadata *LinkageName, Metadata *TemplateParams)
  //     : IsDefinition(IsDefinition), Scope(Scope), LinkageName(LinkageName), TemplateParams(TemplateParams) {}
      

  // };
  // definition?
  //using DISubprogramODRKey = std::tuple<Metadata*, Metadata*, Metadata*>;

  // will the win adl?

  DenseSet<DISubprogram *, ODRSubprogramDeclInfo> FnDecls;



public:

  // err I suppose we don't want to construct any unecessarily...
  // but this will do for a quick test xxx
  //DISubprogram *getODRSubprogramDecl(DISubprogram *Decl);
  DISubprogram *getODRSubprogramDecl(Metadata *Scope, StringRef LinkageName, Metadata *Type, Metadata* TemplateParams);
};

}