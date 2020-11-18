// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat \
// RUN:     -Xclang -disable-llvm-passes %s -o - \
// RUN: | FileCheck %s --implicit-check-not="dbg.assign"

// FIXME: fall back to dbg.declare for variables that share a single alloca.

// CHECK: if.then:
// CHECK-NEXT:  call void @llvm.dbg.declare(metadata %struct.v* %retval,
// CHECK: if.else:
// CHECK-NEXT:  call void @llvm.dbg.declare(metadata %struct.v* %retval,

int g;
struct v { float f; };
v fun() {
  if (g == 0) {
    v tmp1;
    return tmp1;
  } else {
    v tmp2;
    return tmp2;
  }
}
