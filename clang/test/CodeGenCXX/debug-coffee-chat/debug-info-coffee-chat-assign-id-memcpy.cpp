// Check that codegen correctly uses the coffee chat method.
// Each store must have a distinct DIAssignID.
// dbg.declare intrinsics must not be emitted (they are normally emitted after
// alloca).
// Instead, dbg.assign intrinsics must be emitted after each store instruction
// and reference the store's DIAssignID and alloca's DILocalVariable.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// CHECK:      call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %0, i8* align 4 bitcast (%struct.record* @__const._Z10memoryCopyv.vals to i8*), i64 8, i1 false), !dbg !{{.*}}, !DIAssignID ![[setID:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[setID]], metadata i8* %0)
// CHECK:      call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %1, i8* align 4 %2, i64 8, i1 false), !dbg !{{.*}}, !DIAssignID ![[copyID:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[copyID]], metadata i8* %1)

#include "string.h"

int memoryCopy()
{
 struct record {
   int num;
   char ch;
}; 

 record dest;
 record vals = {42, 'z'};
 memcpy(&dest, &vals, sizeof(struct record));
 return 0;
}
