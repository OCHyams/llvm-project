// UNSUPPORTED: target={{.*}}-zos{{.*}}, target={{.*}}-aix{{.*}}

// REQUIRES: asserts
// RUN: rm -rf %t
// RUN: %clang_cc1 -std=c++11 -dwarf-ext-refs -fmodule-format=obj \
// RUN:     -fmodule-map-file=%S/Inputs/gmodules-preferred-name-alias.modulemap \
// RUN:     -fmodules-cache-path=%t -debug-info-kind=standalone -debugger-tuning=lldb \
// RUN:     -fmodules -mllvm -debug-only=pchcontainer -x c++ \
// RUN:     -I %S/Inputs %s &> %t.ll
// RUN: cat %t.ll | FileCheck %s

#include "gmodules-preferred-name-alias.h"

// CHECK: ![[#]] = !DIDerivedType(tag: DW_TAG_template_alias, name: "Bar", scope: ![[#]], file: ![[#]], line: [[#]], baseType: ![[PREF_BASE:[0-9]+]], extraData: ![[TPARAM:[0-9]+]])
// CHECK: ![[PREF_BASE]] = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Foo<char>"
// CHECK: ![[TPARAM]] = !{![[TPARAM_CHAR:[0-9]+]]}
// CHECK: ![[TPARAM_CHAR]] = !DITemplateTypeParameter(name: "T", type: ![[CHAR:[0-9]+]])
// CHECK: ![[CHAR]] = !DIBasicType(name: "char",
