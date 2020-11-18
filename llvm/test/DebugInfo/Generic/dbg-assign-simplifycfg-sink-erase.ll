; RUN: opt %s -S -o - | FileCheck %s

;; Reduced from function cpmx_calc_new_bk in
;; MultiSource/Benchmarks/mafft/tddis.c in the llvm test suite.
;;
;; $ cat reduce.cpp
;; int a;
;; void b() {
;;   int c = 0;
;;   for (; c < 6; c++)
;;     for (; a;)
;;       ;
;; }
;;
;; IR grabbed before simplifycfg in:
;; clang++ -O2 -g -Xclang -debug-coffee-chat
;;
;; simplifycfg will transform this cfg into:
;;
;; [entry]
;; |    |       +---+
;; |    v       v   |
;; |    [for.cond1]-+
;; v
;; [for.inc.split.5]
;;
;; Check that there is only one dbg.assign (for c) in the final block.
; CHECK: for.inc.split.5:
; CHECK-COUNT-1: call void @llvm.dbg.assign

@a = dso_local local_unnamed_addr global i32 0, align 4, !dbg !0

; Function Attrs: norecurse nounwind readonly uwtable mustprogress
define dso_local void @_Z1bv() local_unnamed_addr #0 !dbg !11 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !15, metadata !DIExpression(), metadata !16, metadata i32* undef), !dbg !17
  call void @llvm.dbg.assign(metadata i32 0, metadata !15, metadata !DIExpression(), metadata !18, metadata i32* undef), !dbg !17
  %.pr = load i32, i32* @a, align 4, !tbaa !19
  %tobool.not = icmp eq i32 %.pr, 0
  call void @llvm.dbg.value(metadata i32 0, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 %tobool.not, label %for.inc.split, label %for.cond1.preheader, !dbg !23

for.cond1.preheader:                              ; preds = %for.inc.split.4.for.cond1.preheader_crit_edge, %for.inc.split.3.for.cond1.preheader_crit_edge, %for.inc.split.2.for.cond1.preheader_crit_edge, %for.inc.split.1.for.cond1.preheader_crit_edge, %for.inc.split.for.cond1.preheader_crit_edge, %entry
  br label %for.cond1, !dbg !28

for.cond1:                                        ; preds = %for.cond1.preheader, %for.cond1
  br label %for.cond1, !dbg !28

for.inc.split:                                    ; preds = %entry
  call void @llvm.dbg.assign(metadata i32 1, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 1, metadata !15, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.value(metadata i32 1, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 true, label %for.inc.split.1, label %for.inc.split.for.cond1.preheader_crit_edge, !dbg !23

for.inc.split.for.cond1.preheader_crit_edge:      ; preds = %for.inc.split
  br label %for.cond1.preheader, !dbg !23

for.inc.split.1:                                  ; preds = %for.inc.split
  call void @llvm.dbg.assign(metadata i32 2, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 2, metadata !15, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.value(metadata i32 2, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 true, label %for.inc.split.2, label %for.inc.split.1.for.cond1.preheader_crit_edge, !dbg !23

for.inc.split.1.for.cond1.preheader_crit_edge:    ; preds = %for.inc.split.1
  br label %for.cond1.preheader, !dbg !23

for.inc.split.2:                                  ; preds = %for.inc.split.1
  call void @llvm.dbg.assign(metadata i32 3, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 3, metadata !15, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.value(metadata i32 3, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 true, label %for.inc.split.3, label %for.inc.split.2.for.cond1.preheader_crit_edge, !dbg !23

for.inc.split.2.for.cond1.preheader_crit_edge:    ; preds = %for.inc.split.2
  br label %for.cond1.preheader, !dbg !23

for.inc.split.3:                                  ; preds = %for.inc.split.2
  call void @llvm.dbg.assign(metadata i32 4, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 4, metadata !15, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.value(metadata i32 4, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 true, label %for.inc.split.4, label %for.inc.split.3.for.cond1.preheader_crit_edge, !dbg !23

for.inc.split.3.for.cond1.preheader_crit_edge:    ; preds = %for.inc.split.3
  br label %for.cond1.preheader, !dbg !23

for.inc.split.4:                                  ; preds = %for.inc.split.3
  call void @llvm.dbg.assign(metadata i32 5, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 5, metadata !15, metadata !DIExpression()), !dbg !17
  call void @llvm.dbg.value(metadata i32 5, metadata !15, metadata !DIExpression()), !dbg !17
  br i1 true, label %for.inc.split.5, label %for.inc.split.4.for.cond1.preheader_crit_edge, !dbg !23

for.inc.split.4.for.cond1.preheader_crit_edge:    ; preds = %for.inc.split.4
  br label %for.cond1.preheader, !dbg !23

for.inc.split.5:                                  ; preds = %for.inc.split.4
  call void @llvm.dbg.assign(metadata i32 6, metadata !15, metadata !DIExpression(), metadata !29, metadata i32* undef), !dbg !17
  call void @llvm.dbg.value(metadata i32 6, metadata !15, metadata !DIExpression()), !dbg !17
  ret void, !dbg !30
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #1
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!7, !8, !9}
!llvm.ident = !{!10}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 1, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "reduce.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!7 = !{i32 7, !"Dwarf Version", i32 4}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{i32 1, !"wchar_size", i32 4}
!10 = !{!"clang version 12.0.0"}
!11 = distinct !DISubprogram(name: "b", linkageName: "_Z1bv", scope: !3, file: !3, line: 2, type: !12, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !14)
!12 = !DISubroutineType(types: !13)
!13 = !{null}
!14 = !{!15}
!15 = !DILocalVariable(name: "c", scope: !11, file: !3, line: 3, type: !6)
!16 = distinct !DIAssignID()
!17 = !DILocation(line: 0, scope: !11)
!18 = distinct !DIAssignID()
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !21, i64 0}
!21 = !{!"omnipotent char", !22, i64 0}
!22 = !{!"Simple C++ TBAA"}
!23 = !DILocation(line: 5, column: 12, scope: !24)
!24 = distinct !DILexicalBlock(scope: !25, file: !3, line: 5, column: 5)
!25 = distinct !DILexicalBlock(scope: !26, file: !3, line: 5, column: 5)
!26 = distinct !DILexicalBlock(scope: !27, file: !3, line: 4, column: 3)
!27 = distinct !DILexicalBlock(scope: !11, file: !3, line: 4, column: 3)
!28 = !DILocation(line: 5, column: 5, scope: !25)
!29 = distinct !DIAssignID()
!30 = !DILocation(line: 7, column: 1, scope: !11)
