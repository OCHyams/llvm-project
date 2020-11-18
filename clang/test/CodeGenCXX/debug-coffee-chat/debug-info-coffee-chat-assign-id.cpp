// Check that codegen correctly uses the coffee chat method.
// Each store must have a distinct DIAssignID.
// dbg.declare intrinsics must not be emitted (they are normally emitted after
// alloca).
// Instead, dbg.assign intrinsics must be emitted after each store instruction
// and reference the store's DIAssignID and alloca's DILocalVariable.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - \
// RUN: | opt -S -verify -o -

// CHECK:      %arg.addr = alloca i32, align 4, !DIAssignID ![[id0:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata !{{.*}}, metadata !DIExpression(), metadata ![[id0]], metadata i32* %arg.addr), !dbg

// CHECK-NEXT: %a = alloca i32, align 4, !DIAssignID ![[id1:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata !{{.*}}, metadata !DIExpression(), metadata ![[id1]], metadata i32* %a), !dbg

// CHECK-NEXT: %b = alloca i32, align 4, !DIAssignID ![[id2:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata !{{.*}}, metadata !DIExpression(), metadata ![[id2]], metadata i32* %b), !dbg

// CHECK-NEXT: store i32 %arg, i32* %arg.addr, align 4, !DIAssignID ![[idArg:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %arg, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[idArg]], metadata i32* %arg.addr)

// CHECK-NEXT: store i32 5, i32* %a, align 4, !dbg !{{[0-9]+}}, !DIAssignID ![[idA:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 5, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[idA]], metadata i32* %a)

// CHECK-NEXT: store i32 4, i32* %b, align 4, !dbg !{{[0-9]+}}, !DIAssignID ![[idB:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 4, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[idB]], metadata i32* %b)

int fun(int arg)
{
 int a;
 int b;
 a = 5;
 b = 4;
 return 0;
}
