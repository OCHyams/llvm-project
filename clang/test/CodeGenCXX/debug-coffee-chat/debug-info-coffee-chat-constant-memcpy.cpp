// Test an unusual use of a const, where a memcpy of a constant array is generated.

// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - | FileCheck %s
// CHECK:       call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 16 %1, i8* align 16 bitcast ([5 x i32]* @__const._ZL4fn_aj.array to i8*), i64 20, i1 false), !dbg !{{.*}}, !DIAssignID ![[ID:[0-9]+]]
// CHECK-NEXT:  call void @llvm.dbg.assign(metadata{{.*}}undef, metadata !{{[0-9]+}}, metadata !DIExpression(), metadata ![[ID]], metadata i8* %1)

int g_a = 25;
int g_b = 7;
int g_c = 0;
__attribute__((always_inline))
static int fn_a(unsigned i)
{
  i %= 5;
  int array[] {1, 3, 5, 25, 28};
  return array[i];
}
int main()
{
  if (g_a == fn_a(0)
   || g_a == fn_a(1)
   || g_a == fn_a(2)
   || g_a == fn_a(g_b))
    g_c = g_a;

  return g_c;
}
