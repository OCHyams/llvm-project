// RUN: %clang -### -target x86_64 -c -gdwarf -gkey-instructions %s 2>&1 | FileCheck %s --check-prefixes=KEY-INSTRUCTIONS
// RUN: %clang -### -target x86_64 -c -gdwarf -gno-key-instructions %s 2>&1 | FileCheck %s --check-prefixes=NO-KEY-INSTRUCTIONS
//// Default: Off.
// RUN: %clang -### -target x86_64 -c -gdwarf %s 2>&1 | FileCheck %s --check-prefixes=NO-KEY-INSTRUCTIONS

//// Help hidden.
// RUN %clang --help | FileCheck %s --check-prefix=HELP
// HELP-NOT: key-instructions

// KEY-INSTRUCTIONS: "-gkey-instructions"
// KEY-INSTRUCTIONS: "-mllvm" "-dwarf-use-key-instructions"

// NO-KEY-INSTRUCTIONS-NOT: key-instructions

//// Smoketest.
// RUN: %clang -target x86_64 -c -gdwarf -gkey-instructions %s -emit-llvm -S -o - | FileCheck %s --check-prefixes=SMOKETEST-ON
// RUN: %clang -target x86_64 -c -gdwarf %s -emit-llvm -S -o - | FileCheck %s --check-prefixes=SMOKETEST-OFF
void f() {}
// SMOKETEST-ON: DISubprogram({{.*}}, nextAtomGroup: 2)
// SMOKETEST-OFF-NOT: atom
