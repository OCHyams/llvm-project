// RUN: %clang -c -g -emit-llvm -S -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes %s -o - \
// RUN:     | opt -S -verify -o - \
// RUN:     | FileCheck %s

//// Check that dbg.assign intrinsics get a !dbg with with the same scope as their variable.

// CHECK: call void @llvm.dbg.assign({{.+}}, metadata [[local:![0-9]+]], {{.+}}, {{.+}}, {{.+}}), !dbg [[dbg:![0-9]+]]
// CHECK-DAG: [[local]] = !DILocalVariable(name: "local", scope: [[scope:![0-9]+]],
// CHECK-DAG: [[dbg]] = !DILocation({{.+}}, scope: [[scope]])
// CHECK-DAG: [[scope]] = distinct !DILexicalBlock

void ext(int*);
void fun() {
  {
    int local;
    ext(&local);
  }
}

