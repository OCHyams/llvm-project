; RUN: opt %s -S -loop-rotate -o - | FileCheck %s

;; $ cat reduce.c
;; a, b;
;; f() {
;;   int c = 0, d;
;;   int *e = b;
;;   for (; a;) {
;;     d = 0;
;;     for (; a; d++)
;;       e[c] = 0;
;;   }
;;   g(&c, &d);
;; }
;; IR grabbed before loop-rotate in:
;; $ clang reduce.c -O2 -g -Xclang -debug-coffee-chat

;; Check that both dbg.assign intrinsics linked to the `store i32 %inc, i32*
;; %d` in for.body3 remain linked to the cloned store hoisted into the
;; preheader. It doesn't particularly matter whether the ID they share is also
;; shared between the store and intrinsics in the loop body, either way is
;; correct. This test was reduced from a crash (see commit msg for details).

; CHECK: for.body3.preheader:
; CHECK:      @llvm.dbg.assign(metadata i32 %inc1, metadata ![[VAR:[0-9]+]], metadata !DIExpression(), metadata ![[ID:[0-9]+]], metadata i32* %d)
; CHECK-NEXT: @llvm.dbg.assign(metadata i32 undef, metadata ![[VAR]], metadata !DIExpression(), metadata ![[ID]], metadata i32* %d)
; CHECK-NEXT: store i32 %inc1, i32* %d, align 4,{{.+}}!DIAssignID ![[ID]]

@b = dso_local local_unnamed_addr global i32 0, align 4, !dbg !0
@a = dso_local local_unnamed_addr global i32 0, align 4, !dbg !6

define dso_local i32 @f() local_unnamed_addr #0 !dbg !13 {
entry:
  %c = alloca i32, align 4, !DIAssignID !21
  call void @llvm.dbg.assign(metadata i1 undef, metadata !17, metadata !DIExpression(), metadata !21, metadata i32* %c), !dbg !22
  %d = alloca i32, align 4, !DIAssignID !23
  call void @llvm.dbg.assign(metadata i1 undef, metadata !18, metadata !DIExpression(), metadata !23, metadata i32* %d), !dbg !22
  %0 = bitcast i32* %c to i8*, !dbg !24
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #4, !dbg !24
  %1 = bitcast i32* %d to i8*, !dbg !24
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1) #4, !dbg !24
  %2 = load i32, i32* @b, align 4, !dbg !25, !tbaa !26
  %conv = sext i32 %2 to i64, !dbg !25
  %3 = inttoptr i64 %conv to i32*, !dbg !25
  %.pr = load i32, i32* @a, align 4, !dbg !30, !tbaa !26
  %phi.cmp = icmp eq i32 %.pr, 0, !dbg !33
  br i1 %phi.cmp, label %for.end4, label %for.body3.preheader, !dbg !34

for.body3.preheader:                              ; preds = %entry
  call void @llvm.dbg.assign(metadata i32 0, metadata !18, metadata !DIExpression(), metadata !35, metadata i32* %d), !dbg !22
  store i32 0, i32* %d, align 4, !dbg !36, !tbaa !26, !DIAssignID !35
  br label %for.body3, !dbg !38

for.body3:                                        ; preds = %for.body3.for.body3_crit_edge, %for.body3.preheader
  %4 = phi i32 [ %.pre, %for.body3.for.body3_crit_edge ], [ undef, %for.body3.preheader ], !dbg !40
  %idxprom = sext i32 %4 to i64, !dbg !42
  %arrayidx = getelementptr inbounds i32, i32* %3, i64 %idxprom, !dbg !42
  store i32 0, i32* %arrayidx, align 4, !dbg !43, !tbaa !26
  %5 = load i32, i32* %d, align 4, !dbg !44, !tbaa !26
  %inc = add nsw i32 %5, 1, !dbg !44
  call void @llvm.dbg.assign(metadata i32 %inc, metadata !18, metadata !DIExpression(), metadata !45, metadata i32* %d), !dbg !22
  call void @llvm.dbg.assign(metadata i32 undef, metadata !18, metadata !DIExpression(), metadata !45, metadata i32* %d), !dbg !22
  store i32 %inc, i32* %d, align 4, !dbg !36, !tbaa !26, !DIAssignID !45
  %6 = load i32, i32* @a, align 4, !dbg !46, !tbaa !26
  %tobool2.not = icmp eq i32 %6, 0, !dbg !38
  br i1 %tobool2.not, label %for.end4.loopexit, label %for.body3.for.body3_crit_edge, !dbg !38, !llvm.loop !47

for.body3.for.body3_crit_edge:                    ; preds = %for.body3
  %.pre = load i32, i32* %c, align 4, !dbg !40, !tbaa !26
  br label %for.body3, !dbg !38

for.end4.loopexit:                                ; preds = %for.body3
  br label %for.end4, !dbg !50

for.end4:                                         ; preds = %for.end4.loopexit, %entry
  %call = call i32 (i32*, i32*, ...) bitcast (i32 (...)* @g to i32 (i32*, i32*, ...)*)(i32* nonnull %c, i32* nonnull %d) #4, !dbg !50
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1) #4, !dbg !51
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #4, !dbg !51
  ret i32 undef, !dbg !51
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local i32 @g(...) local_unnamed_addr #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #3

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!9, !10, !11}
!llvm.ident = !{!12}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "b", scope: !2, file: !3, line: 1, type: !8, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C99, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "reduce.c", directory: "/")
!4 = !{}
!5 = !{!6, !0}
!6 = !DIGlobalVariableExpression(var: !7, expr: !DIExpression())
!7 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 1, type: !8, isLocal: false, isDefinition: true)
!8 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!9 = !{i32 7, !"Dwarf Version", i32 4}
!10 = !{i32 2, !"Debug Info Version", i32 3}
!11 = !{i32 1, !"wchar_size", i32 4}
!12 = !{!"clang version 12.0.0"}
!13 = distinct !DISubprogram(name: "f", scope: !3, file: !3, line: 2, type: !14, scopeLine: 2, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !16)
!14 = !DISubroutineType(types: !15)
!15 = !{!8}
!16 = !{!17, !18, !19}
!17 = !DILocalVariable(name: "c", scope: !13, file: !3, line: 3, type: !8)
!18 = !DILocalVariable(name: "d", scope: !13, file: !3, line: 3, type: !8)
!19 = !DILocalVariable(name: "e", scope: !13, file: !3, line: 4, type: !20)
!20 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!21 = distinct !DIAssignID()
!22 = !DILocation(line: 0, scope: !13)
!23 = distinct !DIAssignID()
!24 = !DILocation(line: 3, column: 3, scope: !13)
!25 = !DILocation(line: 4, column: 12, scope: !13)
!26 = !{!27, !27, i64 0}
!27 = !{!"int", !28, i64 0}
!28 = !{!"omnipotent char", !29, i64 0}
!29 = !{!"Simple C/C++ TBAA"}
!30 = !DILocation(line: 5, column: 10, scope: !31)
!31 = distinct !DILexicalBlock(scope: !32, file: !3, line: 5, column: 3)
!32 = distinct !DILexicalBlock(scope: !13, file: !3, line: 5, column: 3)
!33 = !DILocation(line: 5, column: 3, scope: !13)
!34 = !DILocation(line: 5, column: 3, scope: !32)
!35 = distinct !DIAssignID()
!36 = !DILocation(line: 0, scope: !37)
!37 = distinct !DILexicalBlock(scope: !31, file: !3, line: 5, column: 14)
!38 = !DILocation(line: 7, column: 5, scope: !39)
!39 = distinct !DILexicalBlock(scope: !37, file: !3, line: 7, column: 5)
!40 = !DILocation(line: 8, column: 9, scope: !41)
!41 = distinct !DILexicalBlock(scope: !39, file: !3, line: 7, column: 5)
!42 = !DILocation(line: 8, column: 7, scope: !41)
!43 = !DILocation(line: 8, column: 12, scope: !41)
!44 = !DILocation(line: 7, column: 16, scope: !41)
!45 = distinct !DIAssignID()
!46 = !DILocation(line: 7, column: 12, scope: !41)
!47 = distinct !{!47, !38, !48, !49}
!48 = !DILocation(line: 8, column: 14, scope: !39)
!49 = !{!"llvm.loop.mustprogress"}
!50 = !DILocation(line: 10, column: 3, scope: !13)
!51 = !DILocation(line: 11, column: 1, scope: !13)
