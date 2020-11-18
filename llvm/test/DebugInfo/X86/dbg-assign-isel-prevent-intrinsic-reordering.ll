; RUN: llc %s --filetype=obj -o - -stop-after=finalize-isel \
; RUN: | FileCheck %s --implicit-check-not="DBG_VALUE \$noreg"

;; $ cat test.cpp
;; int fun(int a) {
;;   return a;
;; }
;; IR grabbed before SelectionDAG in:
;;    $ clang++ test.cpp -O2 -g -c -Xclang -debug-coffee-chat
;;
;; SelectionDAG has a qurik whereby it'll hoist debug intrinsics that are in
;; the entry block that use a value that is an argument to the top of the
;; block.  This can reorder debug intrinsics unsoundly. In this example,
;; dbg.assign(%a) is hoisted above dbg.assign(undef), so the location is
;; immedately (and incorrectly) terminated.
;;
;; The Assignment Tracking backend has a work-around: delete all undef debug
;; intrinsics that come before non-undef debug intrinsics in the entry block,
;; as they don't provide any extra information.
;;
;; Check that we get a debug location and it isn't terminated
;; (--implicit-check-not).

; CHECK: DBG_VALUE

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i32 @_Z3funi(i32 returned %a) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !12, metadata !DIExpression(), metadata !13, metadata i32* undef), !dbg !14
  call void @llvm.dbg.assign(metadata i32 %a, metadata !12, metadata !DIExpression(), metadata !15, metadata i32* undef), !dbg !14
  ret i32 %a, !dbg !16
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #1

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
!7 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funi", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12}
!12 = !DILocalVariable(name: "a", arg: 1, scope: !7, file: !1, line: 1, type: !10)
!13 = distinct !DIAssignID()
!14 = !DILocation(line: 0, scope: !7)
!15 = distinct !DIAssignID()
!16 = !DILocation(line: 2, column: 3, scope: !7)
