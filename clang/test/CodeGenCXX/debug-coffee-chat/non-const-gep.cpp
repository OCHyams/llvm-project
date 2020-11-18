// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - \
// RUN: | FileCheck %s --implicit-check-not="call void @llvm.dbg.assign"

// CHECK: call void @llvm.dbg.declare

//// i is global so that test is the only local.
int i;
int main() {
  int test[4];
  for (i = 0; i < 4; ++i)
    test[i] = i;
  return test[0];
}
