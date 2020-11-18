// RUN: %clangxx -target x86_64-unknown-unknown -g \
// RUN:   %s -emit-llvm -S -o - -Xclang -debug-coffee-chat | FileCheck %s

struct Foo {
  Foo() = default;
  Foo(Foo &&other) { x = other.x; }
  int x;
};
void some_function(int);
Foo getFoo() {
  Foo foo;
  foo.x = 41;
  some_function(foo.x);
  return foo;
}

int main() {
  Foo bar = getFoo();
  return bar.x;
}

// Test copied from clang/test/CodeGenCXX/debug-info-nrvo.cpp, which tests:
// Check that NRVO variables are stored as a pointer with deref if they are
// stored in the return register.
//
// Check that we don't bother trying to use dbg.assign for variables that
// require non-empty expressions from the start. Eventually we will want to
// support all variables, but this is too much extra work right now.

// CHECK: %[[RESULT:.*]] = alloca i8*, align 8
// CHECK: call void @llvm.dbg.declare(metadata i8** %[[RESULT]],
// CHECK-SAME: metadata !DIExpression(DW_OP_deref)
