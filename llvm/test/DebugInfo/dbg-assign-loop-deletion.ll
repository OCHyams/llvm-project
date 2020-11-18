; RUN: opt %s -loop-deletion -S -o - | FileCheck %s

;; Generated at O2 from:
;; int escape(int*);
;; void c() {
;;   int first;
;;   int second;
;;   for (int i = 0; i < 2; i++) {
;;     first = i;
;;     second = 0;
;;   }
;;   escape(&first);
;;   escape(&second);
;; }
;;
;; Check that the dbg.assings are moved out of the loop before it is deleted.

; CHECK: for.cond.cleanup:
; CHECK: call void @llvm.dbg.assign(metadata i32 undef, metadata ![[first:[0-9]+]], metadata !DIExpression(), metadata ![[ID:[0-9]+]], metadata i32* %first), !dbg
; CHECK: call void @llvm.dbg.assign(metadata i32 0, metadata ![[second:[0-9]+]], metadata !DIExpression(), metadata !28, metadata i32* %second), !dbg
; CHECK: call void @llvm.dbg.assign(metadata i32 undef, metadata ![[i:[0-9]+]],
; CHECK: store i32 1, i32* %first, align 4,{{.*}}!DIAssignID ![[ID]]
; CHECK: ret

; CHECK: ![[first]] = !DILocalVariable(name: "first",
; CHECK: ![[second]] = !DILocalVariable(name: "second",
; CHECK: ![[i]] = !DILocalVariable(name: "i",

define dso_local void @_Z1cv() local_unnamed_addr !dbg !7 {
entry:
  %first = alloca i32, align 4, !DIAssignID !16
  call void @llvm.dbg.assign(metadata i1 undef, metadata !11, metadata !DIExpression(), metadata !16, metadata i32* %first), !dbg !17
  %second = alloca i32, align 4, !DIAssignID !18
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !18, metadata i32* %second), !dbg !17
  call void @llvm.dbg.assign(metadata i1 undef, metadata !14, metadata !DIExpression(), metadata !19, metadata i32* undef), !dbg !17
  %0 = bitcast i32* %first to i8*, !dbg !20
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0), !dbg !20
  %1 = bitcast i32* %second to i8*, !dbg !21
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %1), !dbg !21
  call void @llvm.dbg.assign(metadata i32 0, metadata !14, metadata !DIExpression(), metadata !22, metadata i32* undef), !dbg !23
  store i32 0, i32* %second, align 4, !tbaa !24, !DIAssignID !28
  br label %for.body, !dbg !29

for.cond.cleanup:                                 ; preds = %for.body
  call void @llvm.dbg.value(metadata i32 1, metadata !14, metadata !DIExpression()), !dbg !30
  store i32 1, i32* %first, align 4, !dbg !31, !tbaa !24, !DIAssignID !34
  %call = call i32 @_Z6escapePi(i32* nonnull %first), !dbg !35
  %call1 = call i32 @_Z6escapePi(i32* nonnull %second), !dbg !36
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %1), !dbg !37
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0), !dbg !37
  ret void, !dbg !37

for.body:                                         ; preds = %for.body, %entry
  call void @llvm.dbg.value(metadata i32 undef, metadata !14, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.assign(metadata i32 undef, metadata !11, metadata !DIExpression(), metadata !34, metadata i32* %first), !dbg !31
  call void @llvm.dbg.assign(metadata i32 0, metadata !13, metadata !DIExpression(), metadata !28, metadata i32* %second), !dbg !38
  call void @llvm.dbg.assign(metadata i32 undef, metadata !14, metadata !DIExpression(DW_OP_plus_uconst, 1, DW_OP_stack_value), metadata !39, metadata i32* undef), !dbg !40
  call void @llvm.dbg.value(metadata i32 undef, metadata !14, metadata !DIExpression(DW_OP_plus_uconst, 1, DW_OP_stack_value)), !dbg !30
  br i1 false, label %for.body, label %for.cond.cleanup, !dbg !29, !llvm.loop !41
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)
declare !dbg !44 dso_local i32 @_Z6escapePi(i32*) local_unnamed_addr
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
!10 = !{!11, !13, !14}
!11 = !DILocalVariable(name: "first", scope: !7, file: !1, line: 3, type: !12)
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !DILocalVariable(name: "second", scope: !7, file: !1, line: 4, type: !12)
!14 = !DILocalVariable(name: "i", scope: !15, file: !1, line: 5, type: !12)
!15 = distinct !DILexicalBlock(scope: !7, file: !1, line: 5, column: 3)
!16 = distinct !DIAssignID()
!17 = !DILocation(line: 0, scope: !7)
!18 = distinct !DIAssignID()
!19 = distinct !DIAssignID()
!20 = !DILocation(line: 3, column: 3, scope: !7)
!21 = !DILocation(line: 4, column: 3, scope: !7)
!22 = distinct !DIAssignID()
!23 = !DILocation(line: 5, column: 12, scope: !15)
!24 = !{!25, !25, i64 0}
!25 = !{!"int", !26, i64 0}
!26 = !{!"omnipotent char", !27, i64 0}
!27 = !{!"Simple C++ TBAA"}
!28 = distinct !DIAssignID()
!29 = !DILocation(line: 5, column: 3, scope: !15)
!30 = !DILocation(line: 0, scope: !15)
!31 = !DILocation(line: 6, column: 11, scope: !32)
!32 = distinct !DILexicalBlock(scope: !33, file: !1, line: 5, column: 31)
!33 = distinct !DILexicalBlock(scope: !15, file: !1, line: 5, column: 3)
!34 = distinct !DIAssignID()
!35 = !DILocation(line: 9, column: 3, scope: !7)
!36 = !DILocation(line: 10, column: 3, scope: !7)
!37 = !DILocation(line: 11, column: 1, scope: !7)
!38 = !DILocation(line: 7, column: 12, scope: !32)
!39 = distinct !DIAssignID()
!40 = !DILocation(line: 5, column: 27, scope: !33)
!41 = distinct !{!41, !29, !42, !43}
!42 = !DILocation(line: 8, column: 3, scope: !15)
!43 = !{!"llvm.loop.mustprogress"}
!44 = !DISubprogram(name: "escape", linkageName: "_Z6escapePi", scope: !1, file: !1, line: 1, type: !45, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!45 = !DISubroutineType(types: !46)
!46 = !{!12, !47}
!47 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
