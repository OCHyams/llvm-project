// RUN: %clang_cc1 -triple x86_64-unk-unk -o - -emit-llvm -debug-info-kind=standalone -debugger-tuning=sce %s | FileCheck %s --check-prefixes=SCE,ALL
// RUN: %clang_cc1 -triple x86_64-unk-unk -o - -emit-llvm -debug-info-kind=standalone -debugger-tuning=gdb %s | FileCheck %s --check-prefixes=GDB,ALL

//// Check that -gsce debugger tuning causes DW_TAG_template_alias emission
//// for template aliases. Test type and value template parameters.
template<typename Y, int Z>
struct X {
  Y m1 = Z;
};

template<typename B, int C>
using A = X<B, C>;

A<int, 5> a;

// GDB: !DIDerivedType(tag: DW_TAG_typedef, name: "A<int, 5>", file: ![[#]], line: [[#]], baseType: ![[baseType:[0-9]+]])
// SCE: !DIDerivedType(tag: DW_TAG_template_alias, name: "A", file: ![[#]], line: [[#]], baseType: ![[baseType:[0-9]+]], extraData: ![[extraData:[0-9]+]])
// ALL: ![[baseType]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "X<int, 5>",
// ALL: ![[int:[0-9]+]] = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
// SCE: ![[extraData]] = !{![[B:[0-9]+]], ![[C:[0-9]+]]}
// SCE: ![[B]] = !DITemplateTypeParameter(name: "B", type: ![[int]])
// SCE: ![[C]] = !DITemplateValueParameter(name: "C", type: ![[int]], value: i32 5)
