; RUN: opt %s -sink-common-insts -simplifycfg -verify -S -o - | FileCheck %s
;; sink-common-insts switch added because simplifycfg option efaults apear to
;; be different for clang and opt.

;; Check that the following IR compiles without the verifier complaining.
;;
;; $ cat test.c
;; int a;
;; char b[];
;; char c;
;; void d() {
;;   char e[8];
;;   if (a) {
;;     snprintf(e, sizeof(e), &c);
;;     e[sizeof 1] = 0;
;;   } else
;;     e[1] = 0;
;;   sprintf(b, e);
;; }
;; $ clang -O2 -g -Xclang -debug-coffee-chat
;;   ...Verifier assertion failure...
;;   Expected linked instr address to match instrs
;;
;; Before simplifycfg runs there is a dbg.assign in if.then and another in
;; if.else. These mark assignments to different offsets into %e. The stores and
;; geps are sunk and merged; the store DIAssignIDs are merged and the stored
;; value is the result of a phi built with a phi value for the offset.  Make
;; sure that all dbg.assign(Addr) uses of the merged values are replaced with
;; the merged value. This may create use-before-defs, as it does in this
;; example. This is fine as use-before-def is legal in debug intrinsics and is
;; essentially the same as "undef". The reason that we go to the trouble is to
;; keep the verifier happy; it expects the Addr part of a dbg.assign to match
;; the store-dest of any linked instr.

;; Note that if.else is removed after this transformation because it contains
;; no non-debug intrstructions.

; CHECK: if.then:
; CHECK-NEXT: %call = call i32 (i8*, i64, i8*, ...) @snprintf
;; Ensure the address component is the sunken gep (%arrayidx1).
; CHECK-NEXT: @llvm.dbg.assign({{.+}}, {{.+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 32, 8), {{.+}}, metadata i8* %arrayidx1), !dbg
; CHECK: if.end:
; CHECK-NEXT: %.sink = phi i64 [ 4, %if.then ], [ 1, %entry ]
;; -- This dbg.assign has been sunk from the now deleted if.else.
; CHECK-NEXT: @llvm.dbg.assign({{.+}}, {{.+}}, metadata !DIExpression(DW_OP_LLVM_fragment, 8, 8), {{.+}}, metadata i8* %arrayidx1), !dbg
; CHECK-NEXT: %arrayidx1 = getelementptr inbounds [8 x i8], [8 x i8]* %e, i64 0, i64 %.sink, !dbg

@a = dso_local local_unnamed_addr global i32 0, align 4, !dbg !0
@c = dso_local global i8 0, align 1, !dbg !12
@b = dso_local global [1 x i8] zeroinitializer, align 1, !dbg !6

; Function Attrs: nofree nounwind uwtable
define dso_local void @d() local_unnamed_addr #0 !dbg !19 {
entry:
  %e = alloca [8 x i8], align 1, !DIAssignID !27
  call void @llvm.dbg.assign(metadata i1 undef, metadata !23, metadata !DIExpression(), metadata !27, metadata [8 x i8]* %e), !dbg !28
  %0 = getelementptr inbounds [8 x i8], [8 x i8]* %e, i64 0, i64 0, !dbg !29
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %0) #4, !dbg !29
  %1 = load i32, i32* @a, align 4, !dbg !30, !tbaa !32
  %tobool.not = icmp eq i32 %1, 0, !dbg !30
  br i1 %tobool.not, label %if.else, label %if.then, !dbg !36

if.then:                                          ; preds = %entry
  %call = call i32 (i8*, i64, i8*, ...) @snprintf(i8* nonnull %0, i64 8, i8* nonnull @c), !dbg !37
  %arrayidx = getelementptr inbounds [8 x i8], [8 x i8]* %e, i64 0, i64 4, !dbg !39
  store i8 0, i8* %arrayidx, align 1, !dbg !40, !tbaa !41, !DIAssignID !42
  call void @llvm.dbg.assign(metadata i8 0, metadata !23, metadata !DIExpression(DW_OP_LLVM_fragment, 32, 8), metadata !42, metadata i8* %arrayidx), !dbg !28
  br label %if.end, !dbg !43

if.else:                                          ; preds = %entry
  %arrayidx1 = getelementptr inbounds [8 x i8], [8 x i8]* %e, i64 0, i64 1, !dbg !44
  store i8 0, i8* %arrayidx1, align 1, !dbg !45, !tbaa !41, !DIAssignID !46
  call void @llvm.dbg.assign(metadata i8 0, metadata !23, metadata !DIExpression(DW_OP_LLVM_fragment, 8, 8), metadata !46, metadata i8* %arrayidx1), !dbg !28
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %call3 = call i32 (i8*, i8*, ...) @sprintf(i8* nonnull dereferenceable(1) getelementptr inbounds ([1 x i8], [1 x i8]* @b, i64 0, i64 0), i8* nonnull %0), !dbg !47
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %0) #4, !dbg !48
  ret void, !dbg !48
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1
declare dso_local noundef i32 @snprintf(i8* noalias nocapture noundef writeonly, i64 noundef, i8* nocapture noundef readonly, ...) local_unnamed_addr #2
declare dso_local noundef i32 @sprintf(i8* noalias nocapture noundef writeonly, i8* nocapture noundef readonly, ...) local_unnamed_addr #2
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #3

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!15, !16, !17}
!llvm.ident = !{!18}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 1, type: !14, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C99, file: !3, producer: "clang version 12.0.0)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.c", directory: "/")
!4 = !{}
!5 = !{!0, !6, !12}
!6 = !DIGlobalVariableExpression(var: !7, expr: !DIExpression())
!7 = distinct !DIGlobalVariable(name: "b", scope: !2, file: !3, line: 2, type: !8, isLocal: false, isDefinition: true)
!8 = !DICompositeType(tag: DW_TAG_array_type, baseType: !9, size: 8, elements: !10)
!9 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!10 = !{!11}
!11 = !DISubrange(count: 1)
!12 = !DIGlobalVariableExpression(var: !13, expr: !DIExpression())
!13 = distinct !DIGlobalVariable(name: "c", scope: !2, file: !3, line: 3, type: !9, isLocal: false, isDefinition: true)
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{i32 7, !"Dwarf Version", i32 4}
!16 = !{i32 2, !"Debug Info Version", i32 3}
!17 = !{i32 1, !"wchar_size", i32 4}
!18 = !{!"clang version 12.0.0)"}
!19 = distinct !DISubprogram(name: "d", scope: !3, file: !3, line: 4, type: !20, scopeLine: 4, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !22)
!20 = !DISubroutineType(types: !21)
!21 = !{null}
!22 = !{!23}
!23 = !DILocalVariable(name: "e", scope: !19, file: !3, line: 5, type: !24)
!24 = !DICompositeType(tag: DW_TAG_array_type, baseType: !9, size: 64, elements: !25)
!25 = !{!26}
!26 = !DISubrange(count: 8)
!27 = distinct !DIAssignID()
!28 = !DILocation(line: 0, scope: !19)
!29 = !DILocation(line: 5, column: 3, scope: !19)
!30 = !DILocation(line: 6, column: 7, scope: !31)
!31 = distinct !DILexicalBlock(scope: !19, file: !3, line: 6, column: 7)
!32 = !{!33, !33, i64 0}
!33 = !{!"int", !34, i64 0}
!34 = !{!"omnipotent char", !35, i64 0}
!35 = !{!"Simple C/C++ TBAA"}
!36 = !DILocation(line: 6, column: 7, scope: !19)
!37 = !DILocation(line: 7, column: 5, scope: !38)
!38 = distinct !DILexicalBlock(scope: !31, file: !3, line: 6, column: 10)
!39 = !DILocation(line: 8, column: 5, scope: !38)
!40 = !DILocation(line: 8, column: 17, scope: !38)
!41 = !{!34, !34, i64 0}
!42 = distinct !DIAssignID()
!43 = !DILocation(line: 9, column: 3, scope: !38)
!44 = !DILocation(line: 10, column: 5, scope: !31)
!45 = !DILocation(line: 10, column: 10, scope: !31)
!46 = distinct !DIAssignID()
!47 = !DILocation(line: 11, column: 3, scope: !19)
!48 = !DILocation(line: 12, column: 1, scope: !19)
