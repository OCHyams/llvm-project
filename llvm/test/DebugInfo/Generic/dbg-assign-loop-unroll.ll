; RUN: opt -S %s -loop-unroll | FileCheck %s
;;
;; Check that dbg.assign intrinsics and DIAssignID metadata are updated
;; correctly when instructions are cloned for loop unrolling. When instructions
;; which represent an assignment are cloned for loop unrolling the new
;; instruction should be considered a new distinct assignment. The DIAssignID
;; must be regenerated, and any linked dbg.assign intrincs must use this new
;; DIAssignID.
;;
;; NOTE: The above is no longer the case.
;; Now we expect all the stores and dbg.assign intrinsics to all keep the
;; same ID because the back end is expected to understand multiply-linked
;; instructions.
;;
;; Generated from the following source:
;; void esc(int*);
;; void d(int p) {
;;   for (int i = 0; i < 2; ++i) {
;;     p = i;
;;     esc(&p);
;;   }
;; }

; CHECK: for.body:
;; 1st unrolled iteration.
; CHECK-NEXT: store i32 0, i32* %p.addr,{{.*}}!DIAssignID ![[ID_1:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[P:[0-9]+]], metadata !DIExpression(), metadata ![[ID_1]], metadata i32* %p.addr), !dbg
; CHECK-NEXT: call void @_Z3escPi(i32* nonnull %p.addr)
;
;; 2nd unrolled iteration.
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 1, metadata ![[I:[0-9]+]], metadata !DIExpression(), metadata ![[ID_2:[0-9]+]], metadata i32* undef), !dbg
; CHECK-NEXT: store i32 1, i32* %p.addr,{{.*}}!DIAssignID ![[ID_1]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 1, metadata ![[P]], metadata !DIExpression(), metadata ![[ID_1]], metadata i32* %p.addr), !dbg
; CHECK-NEXT: call void @_Z3escPi(i32* nonnull %p.addr)
;
;; Loop counter variable final update.
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 2, metadata ![[I]], metadata !DIExpression(), metadata ![[ID_2]], metadata i32* undef), !dbg
;
; CHECK-DAG: ![[P]] = !DILocalVariable(name: "p",
; CHECK-DAG: ![[I]] = !DILocalVariable(name: "i",

target triple = "x86_64-unknown-linux-gnu"

define dso_local void @_Z1di(i32 %p) local_unnamed_addr !dbg !7 {
entry:
  %p.addr = alloca i32, align 4
  store i32 %p, i32* %p.addr, align 4, !tbaa !15, !DIAssignID !19
  call void @llvm.dbg.assign(metadata i32 %p, metadata !12, metadata !DIExpression(), metadata !19, metadata i32* %p.addr), !dbg !20
  call void @llvm.dbg.assign(metadata i32 0, metadata !13, metadata !DIExpression(), metadata !21, metadata i32* undef), !dbg !22
  br label %for.body, !dbg !23

for.cond.cleanup:                                 ; preds = %for.body
  ret void, !dbg !24

for.body:                                         ; preds = %entry, %for.body
  %i.04 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  store i32 %i.04, i32* %p.addr, align 4, !dbg !25, !tbaa !15, !DIAssignID !28
  call void @llvm.dbg.assign(metadata i32 %i.04, metadata !12, metadata !DIExpression(), metadata !28, metadata i32* %p.addr), !dbg !25
  call void @_Z3escPi(i32* nonnull %p.addr), !dbg !29
  %inc = add nuw nsw i32 %i.04, 1, !dbg !30
  call void @llvm.dbg.assign(metadata i32 %inc, metadata !13, metadata !DIExpression(), metadata !31, metadata i32* undef), !dbg !30
  %cmp = icmp eq i32 %i.04, 0, !dbg !32
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !dbg !23, !llvm.loop !33
}

declare !dbg !36 dso_local void @_Z3escPi(i32*) local_unnamed_addr
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

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
!7 = distinct !DISubprogram(name: "d", linkageName: "_Z1di", scope: !1, file: !1, line: 2, type: !8, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{null, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12, !13}
!12 = !DILocalVariable(name: "p", arg: 1, scope: !7, file: !1, line: 2, type: !10)
!13 = !DILocalVariable(name: "i", scope: !14, file: !1, line: 3, type: !10)
!14 = distinct !DILexicalBlock(scope: !7, file: !1, line: 3, column: 3)
!15 = !{!16, !16, i64 0}
!16 = !{!"int", !17, i64 0}
!17 = !{!"omnipotent char", !18, i64 0}
!18 = !{!"Simple C++ TBAA"}
!19 = distinct !DIAssignID()
!20 = !DILocation(line: 0, scope: !7)
!21 = distinct !DIAssignID()
!22 = !DILocation(line: 3, column: 12, scope: !14)
!23 = !DILocation(line: 3, column: 3, scope: !14)
!24 = !DILocation(line: 7, column: 1, scope: !7)
!25 = !DILocation(line: 4, column: 7, scope: !26)
!26 = distinct !DILexicalBlock(scope: !27, file: !1, line: 3, column: 31)
!27 = distinct !DILexicalBlock(scope: !14, file: !1, line: 3, column: 3)
!28 = distinct !DIAssignID()
!29 = !DILocation(line: 5, column: 5, scope: !26)
!30 = !DILocation(line: 3, column: 26, scope: !27)
!31 = distinct !DIAssignID()
!32 = !DILocation(line: 3, column: 21, scope: !27)
!33 = distinct !{!33, !23, !34, !35}
!34 = !DILocation(line: 6, column: 3, scope: !14)
!35 = !{!"llvm.loop.mustprogress"}
!36 = !DISubprogram(name: "esc", linkageName: "_Z3escPi", scope: !1, file: !1, line: 1, type: !37, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!37 = !DISubroutineType(types: !38)
!38 = !{null, !39}
!39 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
