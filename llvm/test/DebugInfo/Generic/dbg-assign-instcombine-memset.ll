; RUN: opt %s -S -instcombine -o - | FileCheck %s

;; $ cat test.cpp
;; #include <cstring>
;; void esc(int*);
;; void fun() {
;;   int local[5];
;;   std::memset(local, 0, 4 * 4);
;;   std::memset(local, 8, 2 * 4);
;;   esc(local);
;; }
;; IR grabbed before instcombine in:
;; clang++ -O2 -g -Xclang -debug-coffee-chat

;; Instcombine is going to turn th second memset into a store. Check that it
;; inherits the DIAssignID from the memset and that the dbg.assign's value
;; component is correct.

; CHECK:      store i64 578721382704613384, i64* %2, align 16{{.*}}, !DIAssignID ![[ID:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i64 578721382704613384, metadata !{{.*}}, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata ![[ID]], metadata i8* %1)

; Function Attrs: uwtable mustprogress
define dso_local void @_Z3funv() local_unnamed_addr #0 !dbg !100 {
entry:
  %local = alloca [5 x i32], align 16, !DIAssignID !108
  call void @llvm.dbg.assign(metadata i1 undef, metadata !104, metadata !DIExpression(), metadata !108, metadata [5 x i32]* %local), !dbg !109
  %0 = bitcast [5 x i32]* %local to i8*, !dbg !110
  call void @llvm.lifetime.start.p0i8(i64 20, i8* %0) #5, !dbg !110
  %arraydecay = getelementptr inbounds [5 x i32], [5 x i32]* %local, i64 0, i64 0, !dbg !111
  %1 = bitcast i32* %arraydecay to i8*, !dbg !111
  call void @llvm.memset.p0i8.i64(i8* align 16 %1, i8 0, i64 16, i1 false), !dbg !111, !DIAssignID !112
  call void @llvm.dbg.assign(metadata i8 0, metadata !104, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 128), metadata !112, metadata i8* %1), !dbg !109
  call void @llvm.memset.p0i8.i64(i8* align 16 %1, i8 8, i64 8, i1 false), !dbg !113, !DIAssignID !114
  call void @llvm.dbg.assign(metadata i1 undef, metadata !104, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 64), metadata !114, metadata i8* %1), !dbg !109
  call void @_Z3escPi(i32* %arraydecay), !dbg !115
  call void @llvm.lifetime.end.p0i8(i64 20, i8* %0) #5, !dbg !116
  ret void, !dbg !116
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn writeonly
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #2

declare !dbg !117 dso_local void @_Z3escPi(i32*) local_unnamed_addr #3

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #4

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!96, !97, !98}
!llvm.ident = !{!99}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, imports: !3, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{!4, !18, !22, !28, !32, !36, !46, !50, !52, !54, !58, !62, !66, !70, !74, !76, !78, !80, !84, !88, !92, !94}
!4 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !6, file: !17, line: 77)
!5 = !DINamespace(name: "std", scope: null)
!6 = !DISubprogram(name: "memchr", scope: !7, file: !7, line: 90, type: !8, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!7 = !DIFile(filename: "/usr/include/string.h", directory: "")
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11, !13, !14}
!10 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = !DIDerivedType(tag: DW_TAG_const_type, baseType: null)
!13 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!14 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !15, line: 46, baseType: !16)
!15 = !DIFile(filename: "llvm/coffee-chat/build-rel/lib/clang/12.0.0/include/stddef.h", directory: "/home/och/dev")
!16 = !DIBasicType(name: "long unsigned int", size: 64, encoding: DW_ATE_unsigned)
!17 = !DIFile(filename: "/usr/lib/gcc/x86_64-linux-gnu/11/../../../../include/c++/11/cstring", directory: "")
!18 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !19, file: !17, line: 78)
!19 = !DISubprogram(name: "memcmp", scope: !7, file: !7, line: 63, type: !20, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!20 = !DISubroutineType(types: !21)
!21 = !{!13, !11, !11, !14}
!22 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !23, file: !17, line: 79)
!23 = !DISubprogram(name: "memcpy", scope: !7, file: !7, line: 42, type: !24, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!24 = !DISubroutineType(types: !25)
!25 = !{!10, !26, !27, !14}
!26 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !10)
!27 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !11)
!28 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !29, file: !17, line: 80)
!29 = !DISubprogram(name: "memmove", scope: !7, file: !7, line: 46, type: !30, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!30 = !DISubroutineType(types: !31)
!31 = !{!10, !10, !11, !14}
!32 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !33, file: !17, line: 81)
!33 = !DISubprogram(name: "memset", scope: !7, file: !7, line: 60, type: !34, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!34 = !DISubroutineType(types: !35)
!35 = !{!10, !10, !13, !14}
!36 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !37, file: !17, line: 82)
!37 = !DISubprogram(name: "strcat", scope: !7, file: !7, line: 129, type: !38, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!38 = !DISubroutineType(types: !39)
!39 = !{!40, !42, !43}
!40 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !41, size: 64)
!41 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!42 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !40)
!43 = !DIDerivedType(tag: DW_TAG_restrict_type, baseType: !44)
!44 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !45, size: 64)
!45 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !41)
!46 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !47, file: !17, line: 83)
!47 = !DISubprogram(name: "strcmp", scope: !7, file: !7, line: 136, type: !48, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!48 = !DISubroutineType(types: !49)
!49 = !{!13, !44, !44}
!50 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !51, file: !17, line: 84)
!51 = !DISubprogram(name: "strcoll", scope: !7, file: !7, line: 143, type: !48, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!52 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !53, file: !17, line: 85)
!53 = !DISubprogram(name: "strcpy", scope: !7, file: !7, line: 121, type: !38, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!54 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !55, file: !17, line: 86)
!55 = !DISubprogram(name: "strcspn", scope: !7, file: !7, line: 272, type: !56, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!56 = !DISubroutineType(types: !57)
!57 = !{!14, !44, !44}
!58 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !59, file: !17, line: 87)
!59 = !DISubprogram(name: "strerror", scope: !7, file: !7, line: 396, type: !60, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!60 = !DISubroutineType(types: !61)
!61 = !{!40, !13}
!62 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !63, file: !17, line: 88)
!63 = !DISubprogram(name: "strlen", scope: !7, file: !7, line: 384, type: !64, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!64 = !DISubroutineType(types: !65)
!65 = !{!14, !44}
!66 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !67, file: !17, line: 89)
!67 = !DISubprogram(name: "strncat", scope: !7, file: !7, line: 132, type: !68, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!68 = !DISubroutineType(types: !69)
!69 = !{!40, !42, !43, !14}
!70 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !71, file: !17, line: 90)
!71 = !DISubprogram(name: "strncmp", scope: !7, file: !7, line: 139, type: !72, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!72 = !DISubroutineType(types: !73)
!73 = !{!13, !44, !44, !14}
!74 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !75, file: !17, line: 91)
!75 = !DISubprogram(name: "strncpy", scope: !7, file: !7, line: 124, type: !68, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!76 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !77, file: !17, line: 92)
!77 = !DISubprogram(name: "strspn", scope: !7, file: !7, line: 276, type: !56, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!78 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !79, file: !17, line: 93)
!79 = !DISubprogram(name: "strtok", scope: !7, file: !7, line: 335, type: !38, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!80 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !81, file: !17, line: 94)
!81 = !DISubprogram(name: "strxfrm", scope: !7, file: !7, line: 146, type: !82, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!82 = !DISubroutineType(types: !83)
!83 = !{!14, !42, !43, !14}
!84 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !85, file: !17, line: 95)
!85 = !DISubprogram(name: "strchr", scope: !7, file: !7, line: 225, type: !86, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!86 = !DISubroutineType(types: !87)
!87 = !{!40, !44, !13}
!88 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !89, file: !17, line: 96)
!89 = !DISubprogram(name: "strpbrk", scope: !7, file: !7, line: 302, type: !90, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!90 = !DISubroutineType(types: !91)
!91 = !{!40, !44, !44}
!92 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !93, file: !17, line: 97)
!93 = !DISubprogram(name: "strrchr", scope: !7, file: !7, line: 252, type: !86, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!94 = !DIImportedEntity(tag: DW_TAG_imported_declaration, scope: !5, entity: !95, file: !17, line: 98)
!95 = !DISubprogram(name: "strstr", scope: !7, file: !7, line: 329, type: !90, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!96 = !{i32 7, !"Dwarf Version", i32 4}
!97 = !{i32 2, !"Debug Info Version", i32 3}
!98 = !{i32 1, !"wchar_size", i32 4}
!99 = !{!"clang version 12.0.0"}
!100 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !1, file: !1, line: 3, type: !101, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !103)
!101 = !DISubroutineType(types: !102)
!102 = !{null}
!103 = !{!104}
!104 = !DILocalVariable(name: "local", scope: !100, file: !1, line: 4, type: !105)
!105 = !DICompositeType(tag: DW_TAG_array_type, baseType: !13, size: 160, elements: !106)
!106 = !{!107}
!107 = !DISubrange(count: 5)
!108 = distinct !DIAssignID()
!109 = !DILocation(line: 0, scope: !100)
!110 = !DILocation(line: 4, column: 3, scope: !100)
!111 = !DILocation(line: 5, column: 3, scope: !100)
!112 = distinct !DIAssignID()
!113 = !DILocation(line: 6, column: 3, scope: !100)
!114 = distinct !DIAssignID()
!115 = !DILocation(line: 7, column: 3, scope: !100)
!116 = !DILocation(line: 8, column: 1, scope: !100)
!117 = !DISubprogram(name: "esc", linkageName: "_Z3escPi", scope: !1, file: !1, line: 2, type: !118, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!118 = !DISubroutineType(types: !119)
!119 = !{null, !120}
!120 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64)
