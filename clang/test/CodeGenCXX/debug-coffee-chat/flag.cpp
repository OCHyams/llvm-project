// RUN: %clang %s -Xclang -debug-coffee-chat -O0 -g -o - -emit-llvm -S \
// RUN: | FileCheck %s

// Check some assignment-tracking stuff appears in the output when the flag
// -debug-coffee-chat is used.

// CHECK: DIAssignID
// CHECK: dbg.assign

void fun(int a) {}
