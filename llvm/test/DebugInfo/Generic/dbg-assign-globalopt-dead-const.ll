; RUN: opt -globalopt -S %s -o - | FileCheck %s

;; $ cat test.cpp
;; char a;
;; bool b(unsigned char *p1, long c) {
;;   for (long d; c;)
;;     if (a != p1[d])
;;       return false;
;;   unsigned char e[]{1, 0, 7};
;;   b(e, 7);
;; }
;; IR grabbed before globalopt in:
;; $ clang++ -fno-omit-frame-pointer -mllvm -mem2reg-strip-vars -O2 -g

;; Check that the inline (const) gep of @__const._Z1bPhl.e in a dbg.assign is
;; replaced with undef when @__const._Z1bPhl.e (dead global) is
;; removed. Otherwise, the dbg.assign operand is replaced with empty metadata
;; and may get cleared away by cleanup passes.

; CHECK: for.cond.i:
; CHECK-NEXT: call void @llvm.dbg.assign
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, {{.+}},{{.+}},{{.+}}, metadata i8* undef)
; CHECK-NEXT: br label %for.cond.i

@a = dso_local local_unnamed_addr global i8 0, align 1, !dbg !0
@__const._Z1bPhl.e = private unnamed_addr constant [3 x i8] c"\01\00\07", align 1

; Function Attrs: nounwind readonly uwtable mustprogress
define dso_local zeroext i1 @_Z1bPhl(i8* nocapture readonly %p1, i64 %c) local_unnamed_addr #0 !dbg !12 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !24, metadata !DIExpression(), metadata !31, metadata [3 x i8]* @__const._Z1bPhl.e), !dbg !30
  %tobool.not = icmp eq i64 %c, 0
  %0 = load i8, i8* @a, align 1
  br i1 %tobool.not, label %for.cond.i.preheader.split, label %for.cond.preheader, !dbg !32

for.cond.preheader:                               ; preds = %entry
  %conv = sext i8 %0 to i32
  %arrayidx = getelementptr inbounds i8, i8* %p1, i64 undef
  %.pre = load i8, i8* %arrayidx, align 1, !dbg !33, !tbaa !36
  %conv1 = zext i8 %.pre to i32
  %cmp.not = icmp eq i32 %conv, %conv1
  br label %for.cond, !dbg !39

for.cond:                                         ; preds = %for.cond.preheader, %for.cond
  br i1 %cmp.not, label %for.cond, label %return, !dbg !40, !llvm.loop !41

for.cond.i.preheader.split:                       ; preds = %entry
  %cmp.not.i = icmp eq i8 %0, 1
  tail call void @llvm.assume(i1 %cmp.not.i), !dbg !30
  br label %for.cond.i, !dbg !44

for.cond.i:                                       ; preds = %for.cond.i.preheader.split, %for.cond.i
  call void @llvm.dbg.assign(metadata i1 undef, metadata !24, metadata !DIExpression(), metadata !46, metadata [3 x i8]* undef), !dbg !47
  call void @llvm.dbg.assign(metadata i1 undef, metadata !24, metadata !DIExpression(), metadata !48, metadata i8* getelementptr inbounds ([3 x i8], [3 x i8]* @__const._Z1bPhl.e, i64 0, i64 0)), !dbg !30
  br label %for.cond.i, !dbg !44

return:                                           ; preds = %for.cond
  ret i1 false, !dbg !49
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #2

; Function Attrs: nofree nosync nounwind willreturn
declare void @llvm.assume(i1 noundef) #3

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!8, !9, !10}
!llvm.ident = !{!11}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !6, line: 1, type: !7, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "reduce.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIFile(filename: "reduce.cpp", directory: "/")
!7 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!8 = !{i32 7, !"Dwarf Version", i32 4}
!9 = !{i32 2, !"Debug Info Version", i32 3}
!10 = !{i32 1, !"wchar_size", i32 4}
!11 = !{!"clang version 12.0.0"}
!12 = distinct !DISubprogram(name: "b", linkageName: "_Z1bPhl", scope: !6, file: !6, line: 2, type: !13, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !19)
!13 = !DISubroutineType(types: !14)
!14 = !{!15, !16, !18}
!15 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!16 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !17, size: 64)
!17 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!18 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!19 = !{!20, !21, !22, !24}
!20 = !DILocalVariable(name: "p1", arg: 1, scope: !12, file: !6, line: 2, type: !16)
!21 = !DILocalVariable(name: "c", arg: 2, scope: !12, file: !6, line: 2, type: !18)
!22 = !DILocalVariable(name: "d", scope: !23, file: !6, line: 3, type: !18)
!23 = distinct !DILexicalBlock(scope: !12, file: !6, line: 3, column: 3)
!24 = !DILocalVariable(name: "e", scope: !12, file: !6, line: 6, type: !25)
!25 = !DICompositeType(tag: DW_TAG_array_type, baseType: !17, size: 24, elements: !26)
!26 = !{!27}
!27 = !DISubrange(count: 3)
!29 = distinct !DIAssignID()
!30 = !DILocation(line: 0, scope: !12)
!31 = distinct !DIAssignID()
!32 = !DILocation(line: 3, column: 8, scope: !23)
!33 = !DILocation(line: 4, column: 14, scope: !34)
!34 = distinct !DILexicalBlock(scope: !35, file: !6, line: 4, column: 9)
!35 = distinct !DILexicalBlock(scope: !23, file: !6, line: 3, column: 3)
!36 = !{!37, !37, i64 0}
!37 = !{!"omnipotent char", !38, i64 0}
!38 = !{!"Simple C++ TBAA"}
!39 = !DILocation(line: 3, column: 3, scope: !23)
!40 = !DILocation(line: 4, column: 9, scope: !35)
!41 = distinct !{!41, !39, !42, !43}
!42 = !DILocation(line: 5, column: 14, scope: !23)
!43 = !{!"llvm.loop.mustprogress"}
!44 = !DILocation(line: 4, column: 9, scope: !35, inlinedAt: !45)
!45 = distinct !DILocation(line: 7, column: 3, scope: !12)
!46 = distinct !DIAssignID()
!47 = !DILocation(line: 0, scope: !12, inlinedAt: !45)
!48 = distinct !DIAssignID()
!49 = !DILocation(line: 8, column: 1, scope: !12)
