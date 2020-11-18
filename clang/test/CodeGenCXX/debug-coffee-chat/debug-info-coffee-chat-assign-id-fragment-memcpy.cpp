// Check that codegen correctly uses the coffee chat method when fragments of a
// struct are written to.
// Each fragment that is written to should have a dbg.assign that has the DIAssignID
// of the write as an argument. The fragment offset and size should match the member
// of the struct overwritten.
// Each of the scenarios below results in slightly different arguments generated for
// the memcpy.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %ch, i8* align 1 %src, i64 1, i1 false), !dbg !{{[0-9]+}}, !DIAssignID ![[memberID:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 32, 8), metadata ![[memberID]], metadata i8* %ch)

#include "string.h"
#include <cstdint>

// Test write a complete struct member only.
void FragmentWhole()
{
 struct record {
   uint32_t num;
   char ch;
}; 

 record dest;
 char src = '\0';
 memcpy(&dest.ch, &src, sizeof(char));
}

// CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %1, i8* align 1 %2, i64 5, i1 false), !dbg !{{[0-9]+}}, !DIAssignID ![[exceed:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 40), metadata ![[exceed]], metadata i8* %1)

// Write starting at a member and overlapping part of another.
void FragmentWholeToPartial()
{
 struct record {
   uint32_t num1;
   uint32_t num2;
}; 

 record dest;
 char src[5]="\0\0\0\0";
 memcpy(&dest.num1, &src, 5);
}

// CHECK:       call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 1 %add.ptr, i8* align 1 %2, i64 5, i1 false), !dbg !{{[0-9]+}}, !DIAssignID ![[addendID:[0-9]+]]
// CHECK-NEXT:   call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{.*}}, metadata !DIExpression(DW_OP_LLVM_fragment, 56, 40), metadata ![[addendID]], metadata i8* %add.ptr)

// Write starting between members.
void FragmentPartialToWhole()
{
 struct record {
   uint32_t num1;
   uint32_t num2;
   uint32_t num3;
}; 

 record dest;
 char src[5]="\0\0\0\0";
 memcpy((char*)&(dest.num2) + 3, &src, 5);
}


