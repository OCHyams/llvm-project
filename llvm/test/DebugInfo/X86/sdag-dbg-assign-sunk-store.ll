; XFAIL:*
; InstCombine has removed if.then which contained a dbg.assign linked to the
; store that was merged and sunk into if.end. Before this optimisation the
; assignment ID live-out of each block was entry:!39, if.then:!27, if.else:!27.
; The assignment live in to if.end was join(!27, !27) = !27. However, because
; if.else and its dbg.assign has gone away, the assignment live in to if.end
; has become join(!39, !27) = Unknown. Looking at the test it's "obvious" that
; we should reinstate the stack location after the store in if.end, however,
; because the "live" assignment is Unknown at the time of the store we have to
; defensively insert an undef dbg.value.

; RUN: llc %s -stop-before=finalize-isel -o - | FileCheck %s

;; The assignment `local = 2` has been sunk from the if.then and if.else
;; branches and merged into if.end. Check that the dbg.assign in if.then is
;; lowered into two DBG_VALUEs: One describing the stored value at the position
;; of the store before it was moved, and one describing the stack home of the
;; variable after the sunk store.

;; $ cat test.cpp
;; int c;
;; void esc(int*);
;; int get();
;; void fun() {
;;   int local;
;;   if (c) {
;;     get();
;;     local = 2;
;;   } else {
;;     local = 2;
;;   }
;;   esc(&local);
;; }
;; $ clang -O2 -g -emit -llvm -S test.cpp -o -

; CHECK: ![[LOCAL:[0-9]+]] = !DILocalVariable(name: "local",

; CHECK: bb.1.if.then:
; CHECK: DBG_VALUE 2, $noreg, ![[LOCAL]], !DIExpression(), debug-location ![[DBG:[0-9]+]]

; CHECK: bb.2.if.end:
; CHECK-NEXT: MOV32mi %[[DEST:.*]], 1, $noreg, 0, $noreg, 2
; CHECK-NEXT: DBG_VALUE %[[DEST]], $noreg, ![[LOCAL]], !DIExpression(DW_OP_deref), debug-location ![[DBG]]

@c = dso_local local_unnamed_addr global i32 0, align 4, !dbg !0

define dso_local void @_Z3funv() local_unnamed_addr !dbg !11 {
entry:
  %local = alloca i32, align 4, !DIAssignID !39
  call void @llvm.dbg.assign(metadata i32 undef, metadata !15, metadata !DIExpression(), metadata !39, metadata i32* %local), !dbg !27
  %0 = bitcast i32* %local to i8*, !dbg !16
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0), !dbg !16
  %1 = load i32, i32* @c, align 4, !dbg !17, !tbaa !19
  %tobool.not = icmp eq i32 %1, 0, !dbg !17
  br i1 %tobool.not, label %if.end, label %if.then, !dbg !23

if.then:                                          ; preds = %entry
  %call = tail call i32 @_Z3getv(), !dbg !24
  call void @llvm.dbg.assign(metadata i32 2, metadata !15, metadata !DIExpression(), metadata !26, metadata i32* %local), !dbg !27
  br label %if.end, !dbg !28

if.end:                                           ; preds = %entry, %if.then
  store i32 2, i32* %local, align 4, !dbg !29, !tbaa !19, !DIAssignID !26
  call void @_Z3escPi(i32* nonnull %local), !dbg !30
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0), !dbg !31
  ret void, !dbg !31
}

declare !dbg !32 dso_local i32 @_Z3getv() local_unnamed_addr
declare !dbg !35 dso_local void @_Z3escPi(i32*) local_unnamed_addr
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!7, !8, !9}
!llvm.ident = !{!10}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "c", scope: !2, file: !3, line: 1, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!7 = !{i32 7, !"Dwarf Version", i32 4}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{i32 1, !"wchar_size", i32 4}
!10 = !{!"clang version 12.0.0"}
!11 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !3, file: !3, line: 4, type: !12, scopeLine: 4, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !14)
!12 = !DISubroutineType(types: !13)
!13 = !{null}
!14 = !{!15}
!15 = !DILocalVariable(name: "local", scope: !11, file: !3, line: 5, type: !6)
!16 = !DILocation(line: 5, column: 3, scope: !11)
!17 = !DILocation(line: 6, column: 7, scope: !18)
!18 = distinct !DILexicalBlock(scope: !11, file: !3, line: 6, column: 7)
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !21, i64 0}
!21 = !{!"omnipotent char", !22, i64 0}
!22 = !{!"Simple C++ TBAA"}
!23 = !DILocation(line: 6, column: 7, scope: !11)
!24 = !DILocation(line: 7, column: 5, scope: !25)
!25 = distinct !DILexicalBlock(scope: !18, file: !3, line: 6, column: 10)
!26 = distinct !DIAssignID()
!27 = !DILocation(line: 8, column: 11, scope: !25)
!28 = !DILocation(line: 9, column: 3, scope: !25)
!29 = !DILocation(line: 0, scope: !18)
!30 = !DILocation(line: 12, column: 3, scope: !11)
!31 = !DILocation(line: 13, column: 1, scope: !11)
!32 = !DISubprogram(name: "get", linkageName: "_Z3getv", scope: !3, file: !3, line: 3, type: !33, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !4)
!33 = !DISubroutineType(types: !34)
!34 = !{!6}
!35 = !DISubprogram(name: "esc", linkageName: "_Z3escPi", scope: !3, file: !3, line: 2, type: !36, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !4)
!36 = !DISubroutineType(types: !37)
!37 = !{null, !38}
!38 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !6, size: 64)
!39 = distinct !DIAssignID()
