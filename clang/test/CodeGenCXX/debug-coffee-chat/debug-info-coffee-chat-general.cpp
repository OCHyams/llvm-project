// Additional test used by Orlando that we can throw away if it is determined that
// the other debug-coffee-chat tests cover all the cases. Currently memset() is
// only covered by this test.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// CHECK:      call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %0, i8* align 16 bitcast ([5 x i32]* @__const.main.arr1 to i8*), i64 20, i1 false), !dbg !{{[0-9]+}}, !DIAssignID ![[arr:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[arr]], metadata i8* %0)

// CHECK:      call void @llvm.memset.p0i8.i64(i8* align 4 %1, i8 0, i64 12, i1 false), !dbg !{{[0-9]+}}, !DIAssignID ![[set:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 0, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[set]], metadata i8* %1), !dbg

// CHECK:      store i64 50, i64* %x, align 8, !dbg !{{[0-9]+}}, !DIAssignID ![[store:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata i64 50, metadata !{{[0-9]+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata ![[store]], metadata i64* %x), !dbg

// TODO: Some more stores and a memcpy in-between, but they are not unique test cases in this file.
//       Remove from source?

// CHECK:      call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %4, i8* align 8 %5, i64 16, i1 false), !dbg !{{.*}}, !DIAssignID ![[frag:[0-9]+]]
// CHECK-NEXT: call void @llvm.dbg.assign(metadata{{.*}} undef, metadata !{{[0-9]+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 128), metadata ![[frag]], metadata i8* %4), !dbg


#include <cstring>
struct V3i { long x, y, z; };
__attribute__((optnone)) void use(long *a, long *b) {}
__attribute__((optnone)) void step() {}
bool glob = true;
int main() {
  int arr1[] = { 1, 2, 3, 4, 5 };
  int arr2[] = { 0, 0, 0 };
  // memset zero for struct.
  V3i point = {4, 4, 4};
  step();
  // Store to fragment.
  point.x = 50;
  // Store to fragment.
  point.y = 500;
  // Store to fragment.
  point.z = 5000;
  // memcpy whole struct.
  V3i other = {10, 9, 8};
  step();
  // Partial memcpy.
  std::memcpy(&point.y, &other.x, sizeof(long) * 2);
  use(&point.x, &point.z);
  step();
  // memcpy non-zero for whole struct.
  V3i last = {1, 1, 1};
  return 0;
}
