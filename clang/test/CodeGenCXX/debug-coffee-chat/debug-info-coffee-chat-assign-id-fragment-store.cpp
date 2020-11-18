// Check that codegen correctly uses the coffee chat method when fragments of a
// struct are written to.
// Each fragment that is written to should have a dbg.assign that has the DIAssignID
// of the write as an argument.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// CHECK: store i8 88, i8* %ch, align 4, !dbg !{{.*}}, !DIAssignID ![[id:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 88, metadata !{{.*}}, metadata !DIExpression(DW_OP_LLVM_fragment, 32, 8), metadata ![[id]], metadata i8* %ch), !dbg !{{.*}}


#include "string.h"

int FragmentDirect()
{
 struct record {
   int num;
   char ch;
}; 

 record dest;
 dest.ch = 'X';
 return 0;
}
