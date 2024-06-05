//===-  ReduceInlinedAt.cpp - Specialized Delta pass for DebugInfo --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//

//
//===----------------------------------------------------------------------===//

#include "ReduceDIMetadata.h"
#include "Delta.h"
#include "llvm/ADT/Sequence.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/InstIterator.h"
#include <tuple>
#include <vector>

using namespace llvm;

using MDNodeList = SmallVector<MDNode *>;

static void extractInlinedAtChain(Oracle &O, ReducerWorkItem &WorkItem) {
  Module &Program = WorkItem.getModule();

  MDNodeList MDs;
  for (Function &F : Program.functions()) {
    F.getMetadata(llvm::LLVMContext::MD_dbg, MDs);
    for (Instruction &I : instructions(F))
      if (auto *DI = I.getMetadata(llvm::LLVMContext::MD_dbg))
        MDs.push_back(DI);
  }
  identifyUninterestingMDNodes(O, MDs);
}

void llvm::reduceDIMetadataDeltaPass(TestRunner &Test) {
  runDeltaPass(Test, extractInlinedAtChain, "Reducing inlined at chains");
}
