; RUN: opt %s -S -jump-threading -o - | FileCheck %s
;; This test supersedes dbg-assign-jump-threading-clone.ll, written with the
;; new backend in mind.
;;
;; Before jump threading:
;;
;; [if.then.i (entry)]
;;  |          |
;;  |          V
;;  |         [if.end.i]
;;  |          |
;;  V          V
;; [interesting.block]
;;  |         |
;;  V         |
;; [if.then]  |
;;        |   |
;;        V   V
;;       [if.end (exit)]
;;
;; Jump threading duplicates the code in interesting.block into if.then
;; After jump threading:
;;
;; [if.then.i (entry)]
;;  |   |
;;  |   V
;;  |   [interesting.block.thread]
;;  V         |
;; [if.then]  |
;;        |   |
;;        V   V
;;       [if.end (exit)]
;;
;; Ensure that the store and dbg.assign are copied into if.then with the same
;; DIAssignID.
;;
;; This was reduced from real C++ (clang-3.4), but then further reduced with
;; llvm-reduce and by hand, so there is no meaningful source to share.
;; Reduced from llvm/tools/clang/utils/TableGen/NeonEmitter.cpp, then manually
;; added an alloca so that the stores could store somewhere rather than undef.
;;
; CHECK: interesting.block.thread:
; CHECK-NEXT: store i8 0, i8* %xyz, align 1, !DIAssignID [[ID:![0-9]+]]
;; TODO @OCH: The Value component of this dbg.assign could be i8 0 (like the
;; store) rather than the use-before-def %retval.0.i. This is an issue for
;; normal debug intrinsics also. It should be an easy fix?
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 %retval.0.i, {{.+}}, {{.+}}, metadata [[ID]]

; CHECK: if.then:
; CHECK-NEXT: %retval.0.i = phi i8 [ 98, %if.then.i ]
; CHECK-NEXT: store i8 %retval.0.i, i8* %xyz, align 1, !DIAssignID [[ID]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 %retval.0.i, {{.+}}, {{.+}}, metadata [[ID]]

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)
define dso_local void @_Z3funv() !dbg !7 {
if.then.i:
  %xyz = alloca i8, align 1, !DIAssignID !15
  call void @llvm.dbg.assign(metadata i8 undef, metadata !11, metadata !DIExpression(), metadata !15, metadata i8* %xyz), !dbg !13
  %call2.i = call i8 undef()
  %cond.i = icmp eq i8 %call2.i, 99
  br i1 %cond.i, label %interesting.block, label %if.end.i

if.end.i:                                         ; preds = %if.then.i
  br label %interesting.block

interesting.block:                       ; preds = %if.end.i, %if.then.i
  %retval.0.i = phi i8 [ 0, %if.end.i ], [ 98, %if.then.i ]
  store i8 %retval.0.i, i8* %xyz, align 1, !DIAssignID !10
  call void @llvm.dbg.assign(metadata i8 %retval.0.i, metadata !11, metadata !DIExpression(), metadata !10, metadata i8* %xyz), !dbg !13
  %tobool.not = icmp eq i8 %retval.0.i, 0
  br i1 %tobool.not, label %if.end, label %if.then

if.then:                                          ; preds = %interesting.block
  br label %if.end

if.end:                                           ; preds = %if.then, %interesting.block
  ret void
}

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
!10 = distinct !DIAssignID()
!11 = !DILocalVariable(name: "a", scope: !7, file: !1, line: 2, type: !12)
!12 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!13 = !DILocation(line: 0, scope: !7)
!14 = !DILocation(line: 3, column: 1, scope: !7)
!15 = distinct !DIAssignID()
