# Key Instructions

A new LLVM feature that improves interactive debugging stepping by being smarter about is_stmt placement.

## Status

Enable from Clang front end with (both):
* `-Xclang -fkey-instructions`
* `-mllvm -dwarf-use-key-instructions`

The first tells clang to run the `key-instructions` pass, which applies the new metadata to unoptimized IR. The second tells the DWARF emission code to interpret the new metadata for better is_stmt placement.

## Implementation details

### Pre-optimisation

`DILocation` metadata has two new fields, `AtomGroup` and `AtomRank` which default to zero, meaning that the instruction isn't special.

The `key-instructions` pass replaces `DILocation`s on instructions that it deems important for stepping, using the two new fields. `AtomGroup`, which is shared between instructions that are part of a source atom that implement key functionality (i.e. something a user wants to see: ctrl-flow, assignments, calls). `AtomRank`, which describes a precedence between instructions in an `AtomGroup`. The lower the rank the higher the precedence; generally the lowest ranked instruction in the group is the is_stmt candidate.

Atoms are identified by a `{AtomGroup, InlinedAt}` pair, meaning AtomGroup numbers can be repeated across different functions. Transformations may need to assign new `AtomGroup`s. In order to guarentee that future transformations keep the numbers within functions unique, we just need to track the highest number found in all functions. We could also track a per-function "next", but that's more expensive and complicated.

The next atom group number is tracked globally by `LLVMContextImpl::NextAtomGroup`. When importing modules or functions it is important that this number is set to the highest group number in the imported entity if that is higher than `NextAtomGroup`.

`AtomGroup` is a 61-bit number. If _somehow_ it wraps, it's not disasterous. It would cause missing steps for some instructions within a function that are "accidentally" linked by having the same `{AtomGroup InlinedAt}` pair. The risk of this happening is almost non-existant.

### During optimisation

Throughout optimisation, the `DILocation` is propagated normally. Cloned instructions get that `DILocation`, the new fields get merged in `getMergedLocation`, etc. However, pass writers need to intercede in cases where a code path is duplicated, e.g. unrolling, jump-threading. In these cases we don't want the duplicated instructions to be related to the original so they must get new `AtomGroup` numbers, in a similar way that instruction operands must get remapped. There's facilities to help this: `mapAtomInstance(const DebugLoc &DL, ValueToValueMapTy &VMap)` adds an entry to `VMap` which can later be used for remapping using `llvm::RemapSourceAtom(Instruction *I, ValueToValueMapTy &VM)`. `mapAtomInstance` is called from `llvm::CloneBasicBlock` and `llvm::RemapSourceAtom` is called from `llvm::RemapInstruction` so in many cases no additional effort is actually needed.

`mapAtomInstance` ensure the global `LLVMContextImpl::NextAtomGroup` is kept up to date.

The `DILocations` carry over from IR to MIR without any changes.

### DWARF emission time

With this new approach, is_stmt locations are determined before constructing the line table. In each function, the instructions are iterated over. For each `{AtomGroup, InlinedAt}` pair we find the set of instructions sharing the lowest rank number (highest precedence). Only the last of these instructions in each basic block is included. These instructions get is_stmt applied to their source locations.
