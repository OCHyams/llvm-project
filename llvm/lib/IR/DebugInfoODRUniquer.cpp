//===- llvm/IR/DebugInfoODRUniquer.cpp - Debug Information Builder --------===//
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

#include "llvm/IR/DebugInfoODRUniquer.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

bool Uniquify;
cl::opt<bool, true> UniquifyX("new-odr-uniquer", cl::location(Uniquify), cl::init(false));


DISubprogram *DebugInfoODRUniquer::getODRSubprogramDecl(Metadata *Scope, StringRef LinkageName, Metadata *Type, Metadata* TemplateParams) {
  if (!Uniquify)
    return nullptr; // using old uniqer?

  SPLookup SP = {Scope, LinkageName, Type, TemplateParams};
  auto R = FnDecls.find_as(SP);
  if (R != FnDecls.end())
    return *R;
  return nullptr;
}


// DISubprogram *DebugInfoODRUniquer::getODRSubprogramDecl(DISubprogram *SP) {
//   if (!Uniquify)
//       return SP;
//   return *FnDecls.insert(SP).first;
// }
