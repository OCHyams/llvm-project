// RUN: %clang -### -target x86_64 -c -gdwarf-5 -Xclang -gkey-instructions %s 2>&1 | FileCheck %s --check-prefixes=KEY-INSTRUCTIONS
// RUN: %clang -### -target x86_64 -c -gdwarf-5 -Xclang -gno-key-instructions %s 2>&1 | FileCheck %s --check-prefixes=NO-KEY-INSTRUCTIONS
// KEY-INSTRUCTIONS: "-gkey-instructions"
// NO-KEY-INSTRUCTIONS: "-gno-key-instructions"
// NO-KEY-INSTRUCTIONS-NOT: "-gkey-instructions"

// TODO: Add smoke test once some functionality has been added.
