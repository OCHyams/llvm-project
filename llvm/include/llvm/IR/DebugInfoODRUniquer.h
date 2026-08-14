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
#include "llvm/IR/Metadata.h"

namespace llvm {
class MDString;
class DICompositeType;

class DebugInfoODRUniquer {
public:
  DenseMap<const MDString *, DICompositeType *> DITypeMap;
};

} // namespace llvm