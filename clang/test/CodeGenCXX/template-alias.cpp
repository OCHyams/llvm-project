// RUN: %clang_cc1 -triple x86_64-unk-unk -o - -emit-llvm -debug-info-kind=standalone -debugger-tuning=sce %s | FileCheck %s --check-prefixes=SCE,ALL
// RUN: %clang_cc1 -triple x86_64-unk-unk -o - -emit-llvm -debug-info-kind=standalone -debugger-tuning=gdb %s | FileCheck %s --check-prefixes=GDB,ALL

template<typename T, typename U>
struct X {
  T m1;
  U m2;
};

template<typename V>
using Y = X<V, int>;

Y<int> y = {1, 2};

// GDB: !DIDerivedType(tag: DW_TAG_typedef, name: "Y<int>", file: !5, line: 11, baseType: ![[baseType:[0-9]+]])
// SCE: !DIDerivedType(tag: DW_TAG_template_alias, name: "Y", file: ![[#]], line: [[#]], baseType: ![[baseType:[0-9]+]], extraData: ![[extraData:[0-9]+]])
// ALL: ![[baseType]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "X<int, int>",
// ALL: ![[int:[0-9]+]] = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
// SCE: ![[extraData]] = !{![[V:[0-9]+]]}
// SCE: ![[V]] = !DITemplateTypeParameter(name: "V", type: ![[int]])

