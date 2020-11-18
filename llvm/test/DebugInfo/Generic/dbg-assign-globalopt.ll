; RUN: opt %s -S -o - | FileCheck %s

;; Check that globalopt replaces debug uses with undef.
;; FIXME: This test could be smaller.

;; Derrived from this source compiled at O2:
;; #include <cstring>
;;
;; struct V3i { long x, y, z; };
;; void fun() {
;;   V3i point = {0, 0, 0};
;;   point.z = 5000;
;;   V3i other = {10, 9, 8};
;;   std::memcpy(&point.y, &other.x, sizeof(long) * 2);
;; }

;; Ensure there are no dbg.assign intrinsics with empty metadata components.
;; CHECK-NOT: @llvm.dbg.assign(metadata !{{[0-9]+}},
;; CHECK-NOT: @llvm.dbg.assign({{.+}}, metadata !{{[0-9]+}})

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.V3i = type { i64, i64, i64 }

@glob = dso_local local_unnamed_addr global i8 1, align 1, !dbg !0
@__const.main.arr1 = private unnamed_addr constant [5 x i32] [i32 1, i32 2, i32 3, i32 4, i32 5], align 16
@__const.main.other = private unnamed_addr constant %struct.V3i { i64 10, i64 9, i64 8 }, align 8
@__const.main.last = private unnamed_addr constant %struct.V3i { i64 1, i64 1, i64 1 }, align 8

; Function Attrs: noinline nounwind optnone uwtable mustprogress
define dso_local void @_Z4stepv() local_unnamed_addr !dbg !104 {
entry:
  ret void, !dbg !107
}

; Function Attrs: norecurse nounwind uwtable mustprogress
define dso_local i32 @main() local_unnamed_addr !dbg !108 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !112, metadata !DIExpression(), metadata !131, metadata [5 x i32]* @__const.main.arr1), !dbg !132
  call void @llvm.dbg.assign(metadata [3 x i32]* undef, metadata !116, metadata !DIExpression(), metadata !133, metadata [3 x i32]* undef), !dbg !132
  %point.sroa.0 = alloca i64, align 8, !DIAssignID !134
  call void @llvm.dbg.assign(metadata i1 undef, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata !134, metadata i64* %point.sroa.0), !dbg !132
  call void @llvm.dbg.assign(metadata i1 undef, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !135, metadata i64* undef), !dbg !132
  %point.sroa.7 = alloca i64, align 8, !DIAssignID !136
  call void @llvm.dbg.assign(metadata i1 undef, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 128, 64), metadata !136, metadata i64* %point.sroa.7), !dbg !132
  call void @llvm.dbg.assign(metadata i1 undef, metadata !127, metadata !DIExpression(), metadata !137, metadata i64** undef), !dbg !132
  call void @llvm.dbg.assign(metadata i1 undef, metadata !129, metadata !DIExpression(), metadata !138, metadata %struct.V3i* @__const.main.other), !dbg !132
  call void @llvm.dbg.assign(metadata i1 undef, metadata !130, metadata !DIExpression(), metadata !139, metadata %struct.V3i* @__const.main.last), !dbg !132
  call void @llvm.dbg.assign(metadata [5 x i32]* @__const.main.arr1, metadata !112, metadata !DIExpression(), metadata !140, metadata i8* undef), !dbg !141
  call void @llvm.dbg.assign(metadata i8 0, metadata !116, metadata !DIExpression(), metadata !142, metadata i8* undef), !dbg !143
  tail call void @_Z4stepv(), !dbg !144
  %point.sroa.0.0..sroa_cast = bitcast i64* %point.sroa.0 to i8*, !dbg !145
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %point.sroa.0.0..sroa_cast), !dbg !145
  %point.sroa.7.0..sroa_cast = bitcast i64* %point.sroa.7 to i8*, !dbg !145
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %point.sroa.7.0..sroa_cast), !dbg !145
  call void @llvm.dbg.assign(metadata i64 0, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata !146, metadata i64* %point.sroa.0), !dbg !147
  call void @llvm.dbg.assign(metadata i64 0, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !148, metadata i64* undef), !dbg !147
  call void @llvm.dbg.assign(metadata i64 0, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 128, 64), metadata !149, metadata i64* %point.sroa.7), !dbg !147
  %0 = load i8, i8* @glob, align 1, !dbg !150, !tbaa !151, !range !155
  %tobool.not = icmp eq i8 %0, 0, !dbg !150
  %cond = select i1 %tobool.not, i64* %point.sroa.7, i64* %point.sroa.0, !dbg !150
  call void @llvm.dbg.assign(metadata i64* %cond, metadata !127, metadata !DIExpression(), metadata !156, metadata i64** undef), !dbg !157
  store i64 5, i64* %cond, align 8, !dbg !158, !tbaa !159
  tail call void @_Z4stepv(), !dbg !161
  call void @llvm.dbg.assign(metadata i64 50, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata !162, metadata i64* %point.sroa.0), !dbg !163
  call void @llvm.dbg.assign(metadata i64 500, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !164, metadata i64* undef), !dbg !165
  call void @llvm.dbg.assign(metadata i64 5000, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 128, 64), metadata !166, metadata i64* %point.sroa.7), !dbg !167
  call void @llvm.dbg.assign(metadata %struct.V3i* @__const.main.other, metadata !129, metadata !DIExpression(), metadata !168, metadata i8* undef), !dbg !169
  tail call void @_Z4stepv(), !dbg !170
  call void @llvm.dbg.assign(metadata i64 undef, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !171, metadata i64* undef), !dbg !172
  call void @llvm.dbg.assign(metadata i64 9, metadata !120, metadata !DIExpression(DW_OP_LLVM_fragment, 128, 64), metadata !173, metadata i64* %point.sroa.7), !dbg !172
  tail call void @_Z4stepv(), !dbg !174
  call void @llvm.dbg.assign(metadata %struct.V3i* @__const.main.last, metadata !130, metadata !DIExpression(), metadata !175, metadata i8* undef), !dbg !176
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %point.sroa.0.0..sroa_cast), !dbg !177
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %point.sroa.7.0..sroa_cast), !dbg !177
  ret i32 0, !dbg !178
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg)
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg)
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)
declare void @llvm.dbg.declare(metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!100, !101, !102}
!llvm.ident = !{!103}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "glob", scope: !2, file: !3, line: 6, type: !99, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, imports: !6, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !{!7, !21, !25, !31, !35, !39, !49, !53, !55, !57, !61, !65, !69, !73, !77, !79, !81, !83, !87, !91, !95, !97}
!7 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !9, file: !20, line: 75)
!8 = !DINamespace(name: "std", scope: null)
!9 = !DISubprogram(name: "memchr", scope: !10, file: !10, line: 90, type: !11, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!10 = !DIFile(filename: "/usr/include/string.h", directory: "")
!11 = !DISubroutineType(types: !12)
!12 = !{!13, !14, !16, !17}
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !15, size: 64)
!15 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!16 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!17 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !18, line: 46, baseType: !19)
!18 = !DIFile(filename: "llvm/coffee-chat/build-rel/lib/clang/12.0.0/include/stddef.h", directory: "/home/och/dev")
!19 = !DIBasicType(name: "long unsigned int", size: 64, encoding: DW_ATE_unsigned)
!20 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/7.5.0/../../../../include/c++/7.5.0/cstring", directory: "")
!21 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !22, file: !20, line: 76)
!22 = !DISubprogram(name: "memcmp", scope: !10, file: !10, line: 63, type: !23, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!23 = !DISubroutineType(types: !24)
!24 = !{!16, !14, !14, !17}
!25 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !26, file: !20, line: 77)
!26 = !DISubprogram(name: "memcpy", scope: !10, file: !10, line: 42, type: !27, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!27 = !DISubroutineType(types: !28)
!28 = !{!13, !29, !30, !17}
!29 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !13)
!30 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !14)
!31 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !32, file: !20, line: 78)
!32 = !DISubprogram(name: "memmove", scope: !10, file: !10, line: 46, type: !33, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!33 = !DISubroutineType(types: !34)
!34 = !{!13, !13, !14, !17}
!35 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !36, file: !20, line: 79)
!36 = !DISubprogram(name: "memset", scope: !10, file: !10, line: 60, type: !37, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!37 = !DISubroutineType(types: !38)
!38 = !{!13, !13, !16, !17}
!39 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !40, file: !20, line: 80)
!40 = !DISubprogram(name: "strcat", scope: !10, file: !10, line: 129, type: !41, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!41 = !DISubroutineType(types: !42)
!42 = !{!43, !45, !46}
!43 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !44, size: 64)
!44 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!45 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !43)
!46 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !47)
!47 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !48, size: 64)
!48 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !44)
!49 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !50, file: !20, line: 81)
!50 = !DISubprogram(name: "strcmp", scope: !10, file: !10, line: 136, type: !51, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!51 = !DISubroutineType(types: !52)
!52 = !{!16, !47, !47}
!53 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !54, file: !20, line: 82)
!54 = !DISubprogram(name: "strcoll", scope: !10, file: !10, line: 143, type: !51, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!55 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !56, file: !20, line: 83)
!56 = !DISubprogram(name: "strcpy", scope: !10, file: !10, line: 121, type: !41, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!57 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !58, file: !20, line: 84)
!58 = !DISubprogram(name: "strcspn", scope: !10, file: !10, line: 272, type: !59, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!59 = !DISubroutineType(types: !60)
!60 = !{!17, !47, !47}
!61 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !62, file: !20, line: 85)
!62 = !DISubprogram(name: "strerror", scope: !10, file: !10, line: 396, type: !63, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!63 = !DISubroutineType(types: !64)
!64 = !{!43, !16}
!65 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !66, file: !20, line: 86)
!66 = !DISubprogram(name: "strlen", scope: !10, file: !10, line: 384, type: !67, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!67 = !DISubroutineType(types: !68)
!68 = !{!17, !47}
!69 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !70, file: !20, line: 87)
!70 = !DISubprogram(name: "strncat", scope: !10, file: !10, line: 132, type: !71, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!71 = !DISubroutineType(types: !72)
!72 = !{!43, !45, !46, !17}
!73 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !74, file: !20, line: 88)
!74 = !DISubprogram(name: "strncmp", scope: !10, file: !10, line: 139, type: !75, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!75 = !DISubroutineType(types: !76)
!76 = !{!16, !47, !47, !17}
!77 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !78, file: !20, line: 89)
!78 = !DISubprogram(name: "strncpy", scope: !10, file: !10, line: 124, type: !71, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!79 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !80, file: !20, line: 90)
!80 = !DISubprogram(name: "strspn", scope: !10, file: !10, line: 276, type: !59, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!81 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !82, file: !20, line: 91)
!82 = !DISubprogram(name: "strtok", scope: !10, file: !10, line: 335, type: !41, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!83 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !84, file: !20, line: 92)
!84 = !DISubprogram(name: "strxfrm", scope: !10, file: !10, line: 146, type: !85, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!85 = !DISubroutineType(types: !86)
!86 = !{!17, !45, !46, !17}
!87 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !88, file: !20, line: 93)
!88 = !DISubprogram(name: "strchr", scope: !10, file: !10, line: 225, type: !89, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!89 = !DISubroutineType(types: !90)
!90 = !{!43, !47, !16}
!91 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !92, file: !20, line: 94)
!92 = !DISubprogram(name: "strpbrk", scope: !10, file: !10, line: 302, type: !93, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!93 = !DISubroutineType(types: !94)
!94 = !{!43, !47, !47}
!95 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !96, file: !20, line: 95)
!96 = !DISubprogram(name: "strrchr", scope: !10, file: !10, line: 252, type: !89, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!97 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !8, entity: !98, file: !20, line: 96)
!98 = !DISubprogram(name: "strstr", scope: !10, file: !10, line: 329, type: !93, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!99 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!100 = !{i32 7, !"Dwarf Version", i32 4}
!101 = !{i32 2, !"Debug Info Version", i32 3}
!102 = !{i32 1, !"wchar_size", i32 4}
!103 = !{!"clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git a0dc99630d0eb1d1376291659fda797184ad2534)"}
!104 = distinct !DISubprogram(name: "step", linkageName: "_Z4stepv", scope: !3, file: !3, line: 3, type: !105, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !4)
!105 = !DISubroutineType(types: !106)
!106 = !{null}
!107 = !DILocation(line: 3, column: 39, scope: !104)
!108 = distinct !DISubprogram(name: "main", scope: !3, file: !3, line: 7, type: !109, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !111)
!109 = !DISubroutineType(types: !110)
!110 = !{!16}
!111 = !{!112, !116, !120, !127, !129, !130}
!112 = !DILocalVariable(name: "arr1", scope: !108, file: !3, line: 8, type: !113)
!113 = !DICompositeType(tag: DW_TAG_array_type, baseType: !16, size: 160, elements: !114)
!114 = !{!115}
!115 = !DISubrange(count: 5)
!116 = !DILocalVariable(name: "arr2", scope: !108, file: !3, line: 9, type: !117)
!117 = !DICompositeType(tag: DW_TAG_array_type, baseType: !16, size: 96, elements: !118)
!118 = !{!119}
!119 = !DISubrange(count: 3)
!120 = !DILocalVariable(name: "point", scope: !108, file: !3, line: 12, type: !121)
!121 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "V3i", file: !3, line: 5, size: 192, flags: DIFlagTypePassByValue, elements: !122, identifier: "_ZTS3V3i")
!122 = !{!123, !125, !126}
!123 = !DIDerivedType(tag: DW_TAG_member, name: "x", scope: !121, file: !3, line: 5, baseType: !124, size: 64)
!124 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!125 = !DIDerivedType(tag: DW_TAG_member, name: "y", scope: !121, file: !3, line: 5, baseType: !124, size: 64, offset: 64)
!126 = !DIDerivedType(tag: DW_TAG_member, name: "z", scope: !121, file: !3, line: 5, baseType: !124, size: 64, offset: 128)
!127 = !DILocalVariable(name: "p", scope: !108, file: !3, line: 13, type: !128)
!128 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !124, size: 64)
!129 = !DILocalVariable(name: "other", scope: !108, file: !3, line: 23, type: !121)
!130 = !DILocalVariable(name: "last", scope: !108, file: !3, line: 29, type: !121)
!131 = distinct !DIAssignID()
!132 = !DILocation(line: 0, scope: !108)
!133 = distinct !DIAssignID()
!134 = distinct !DIAssignID()
!135 = distinct !DIAssignID()
!136 = distinct !DIAssignID()
!137 = distinct !DIAssignID()
!138 = distinct !DIAssignID()
!139 = distinct !DIAssignID()
!140 = distinct !DIAssignID()
!141 = !DILocation(line: 8, column: 7, scope: !108)
!142 = distinct !DIAssignID()
!143 = !DILocation(line: 9, column: 7, scope: !108)
!144 = !DILocation(line: 10, column: 3, scope: !108)
!145 = !DILocation(line: 12, column: 3, scope: !108)
!146 = distinct !DIAssignID()
!147 = !DILocation(line: 12, column: 7, scope: !108)
!148 = distinct !DIAssignID()
!149 = distinct !DIAssignID()
!150 = !DILocation(line: 13, column: 13, scope: !108)
!151 = !{!152, !152, i64 0}
!152 = !{!"bool", !153, i64 0}
!153 = !{!"omnipotent char", !154, i64 0}
!154 = !{!"Simple C++ TBAA"}
!155 = !{i8 0, i8 2}
!156 = distinct !DIAssignID()
!157 = !DILocation(line: 13, column: 9, scope: !108)
!158 = !DILocation(line: 14, column: 6, scope: !108)
!159 = !{!160, !160, i64 0}
!160 = !{!"long", !153, i64 0}
!161 = !DILocation(line: 15, column: 3, scope: !108)
!162 = distinct !DIAssignID()
!163 = !DILocation(line: 17, column: 11, scope: !108)
!164 = distinct !DIAssignID()
!165 = !DILocation(line: 19, column: 11, scope: !108)
!166 = distinct !DIAssignID()
!167 = !DILocation(line: 21, column: 11, scope: !108)
!168 = distinct !DIAssignID()
!169 = !DILocation(line: 23, column: 7, scope: !108)
!170 = !DILocation(line: 24, column: 3, scope: !108)
!171 = distinct !DIAssignID()
!172 = !DILocation(line: 26, column: 3, scope: !108)
!173 = distinct !DIAssignID()
!174 = !DILocation(line: 27, column: 3, scope: !108)
!175 = distinct !DIAssignID()
!176 = !DILocation(line: 29, column: 7, scope: !108)
!177 = !DILocation(line: 31, column: 1, scope: !108)
!178 = !DILocation(line: 30, column: 3, scope: !108)
