; RUN: opt %s -loop-deletion -S -o - | FileCheck %s

;; Generated at O2 from:
;; int escape(int*);
;; void c() {
;;   int local = 0;
;;   for (;;) {
;;     local = i;
;;   }
;;   escape(&local);
;; }
;;
;; Check that the dbg.assings are not deleted by ADCE.

; CHECK: for.cond:
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 undef, metadata ![[counter:[0-9]+]], metadata !DIExpression()), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 undef, metadata ![[counter]], metadata !DIExpression({{.*}}), metadata !{{.+}}, metadata i32* undef), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 undef, metadata ![[local:[0-9]+]], metadata !DIExpression(), metadata !{{.+}}, metadata i32* undef), !dbg

; CHECK: ![[local]] = !DILocalVariable(name: "local",
; CHECK: ![[counter]] = !DILocalVariable(name: "counter",

define dso_local void @_Z1cv() local_unnamed_addr !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !11, metadata !DIExpression(), metadata !14, metadata i32* undef), !dbg !15
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !16, metadata i32* undef), !dbg !15
  call void @llvm.dbg.assign(metadata i32 0, metadata !11, metadata !DIExpression(), metadata !17, metadata i32* undef), !dbg !18
  call void @llvm.dbg.assign(metadata i32 0, metadata !13, metadata !DIExpression(), metadata !19, metadata i32* undef), !dbg !20
  br label %for.cond, !dbg !21

for.cond:                                         ; preds = %for.cond, %entry
  call void @llvm.dbg.value(metadata i32 undef, metadata !13, metadata !DIExpression()), !dbg !15
  call void @llvm.dbg.assign(metadata i32 undef, metadata !13, metadata !DIExpression(DW_OP_plus_uconst, 1, DW_OP_stack_value), metadata !22, metadata i32* undef), !dbg !23
  call void @llvm.dbg.assign(metadata i32 undef, metadata !11, metadata !DIExpression(), metadata !27, metadata i32* undef), !dbg !28
  br label %for.cond, !dbg !29, !llvm.loop !30
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)
declare void @llvm.dbg.value(metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "c", linkageName: "_Z1cv", scope: !1, file: !1, line: 2, type: !8, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !10)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
!10 = !{!11, !13}
!11 = !DILocalVariable(name: "local", scope: !7, file: !1, line: 3, type: !12)
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !DILocalVariable(name: "counter", scope: !7, file: !1, line: 4, type: !12)
!14 = distinct !DIAssignID()
!15 = !DILocation(line: 0, scope: !7)
!16 = distinct !DIAssignID()
!17 = distinct !DIAssignID()
!18 = !DILocation(line: 3, column: 7, scope: !7)
!19 = distinct !DIAssignID()
!20 = !DILocation(line: 4, column: 7, scope: !7)
!21 = !DILocation(line: 5, column: 3, scope: !7)
!22 = distinct !DIAssignID()
!23 = !DILocation(line: 6, column: 20, scope: !24)
!24 = distinct !DILexicalBlock(scope: !25, file: !1, line: 5, column: 12)
!25 = distinct !DILexicalBlock(scope: !26, file: !1, line: 5, column: 3)
!26 = distinct !DILexicalBlock(scope: !7, file: !1, line: 5, column: 3)
!27 = distinct !DIAssignID()
!28 = !DILocation(line: 6, column: 11, scope: !24)
!29 = !DILocation(line: 5, column: 3, scope: !25)
!30 = distinct !{!30, !31, !32}
!31 = !DILocation(line: 5, column: 3, scope: !26)
!32 = !DILocation(line: 7, column: 3, scope: !26)
