; RUN: opt %s -S -instcombine | FileCheck %s

;; Check that instcombine carries over DIAssignID metadata to the new store
;; when it changes the type of the pointer operand.
;;
;; Generated from the following source:
;; void esc(char**);
;; void fun(int *in) {
;;   char *local = (char *)&in;
;;   esc(&local);
;; }

;; The DIAssignID should match that used in the dbg.assign that follows.
; CHECK: store i32**{{.*}}, i32***{{.*}},{{.*}}!DIAssignID ![[ID:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign({{.*}},{{.*}},{{.*}}, metadata ![[ID]],{{.*}})

define dso_local void @fun(i32* %in) local_unnamed_addr !dbg !10 {
entry:
  %in.addr = alloca i32*, align 8
  %local = alloca i8*, align 8
  store i32* %in, i32** %in.addr, align 8, !tbaa !18, !DIAssignID !22
  call void @llvm.dbg.assign(metadata i32* %in, metadata !16, metadata !DIExpression(), metadata !22, metadata i32** %in.addr), !dbg !23
  %0 = bitcast i8** %local to i8*, !dbg !24
  %1 = bitcast i32** %in.addr to i8*, !dbg !25
  store i8* %1, i8** %local, align 8, !dbg !26, !tbaa !18, !DIAssignID !27
  call void @llvm.dbg.assign(metadata i8* %1, metadata !17, metadata !DIExpression(), metadata !27, metadata i8** %local), !dbg !26
  call void @esc(i8** %local), !dbg !28
  ret void, !dbg !29
}

declare !dbg !30 dso_local void @esc(i8**) local_unnamed_addr
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!6, !7, !8}
!llvm.ident = !{!9}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, retainedTypes: !3, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "reduce.c", directory: "/")
!2 = !{}
!3 = !{!4}
!4 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!5 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!6 = !{i32 7, !"Dwarf Version", i32 4}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{!"clang version 12.0.0"}
!10 = distinct !DISubprogram(name: "fun", scope: !1, file: !1, line: 2, type: !11, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !15)
!11 = !DISubroutineType(types: !12)
!12 = !{null, !13}
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{!16, !17}
!16 = !DILocalVariable(name: "in", arg: 1, scope: !10, file: !1, line: 2, type: !13)
!17 = !DILocalVariable(name: "local", scope: !10, file: !1, line: 3, type: !4)
!18 = !{!19, !19, i64 0}
!19 = !{!"any pointer", !20, i64 0}
!20 = !{!"omnipotent char", !21, i64 0}
!21 = !{!"Simple C/C++ TBAA"}
!22 = distinct !DIAssignID()
!23 = !DILocation(line: 0, scope: !10)
!24 = !DILocation(line: 3, column: 3, scope: !10)
!25 = !DILocation(line: 3, column: 17, scope: !10)
!26 = !DILocation(line: 3, column: 9, scope: !10)
!27 = distinct !DIAssignID()
!28 = !DILocation(line: 4, column: 3, scope: !10)
!29 = !DILocation(line: 5, column: 1, scope: !10)
!30 = !DISubprogram(name: "esc", scope: !1, file: !1, line: 1, type: !31, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!31 = !DISubroutineType(types: !32)
!32 = !{null, !33}
!33 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !4, size: 64)
