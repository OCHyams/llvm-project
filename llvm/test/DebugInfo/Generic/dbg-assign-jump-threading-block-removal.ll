; RUN: opt %s -S -jump-threading -o - \
; RUN:   | FileCheck %s --implicit-check-not="call void dbg.assign"

;; Jump threading removes unreachable blocks. If the only use of a DIAssignID
;; comes from an unreachable block the definition must be removed. I.e. The
;; DIAssignID must be removed from the assigning instruction.

;; This was reduced from real C++ (clang-3.4), but then further reduced with
;; llvm-reduce and by hand, so there is no meaningful source to share.

; CHECK: @_Z3funv()
; CHECK-NEXT: if.end:
;; Important part: check that there is no DIAssignID attachment on the alloca.
; CHECK-NEXT: %a = alloca i32, align 4{{$}}
; CHECK-NEXT: ret

define dso_local zeroext i1 @_Z3funv() !dbg !11 {
entry:
  %a = alloca i32, align 4, !DIAssignID !17
   br i1 true, label %if.end, label %if.then, !dbg !27

if.then:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !15, metadata !DIExpression(), metadata !17, metadata i32* %a), !dbg !18
  br label %if.end, !dbg !32

if.end:                                           ; preds = %if.then, %entry
  ret i1 0, !dbg !35
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!7, !8, !9}
!llvm.ident = !{!10}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "glob", scope: !2, file: !3, line: 1, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!7 = !{i32 7, !"Dwarf Version", i32 4}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{i32 1, !"wchar_size", i32 4}
!10 = !{!"clang version 12.0.0"}
!11 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !3, file: !3, line: 2, type: !12, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !14)
!12 = !DISubroutineType(types: !13)
!13 = !{!6}
!14 = !{!15}
!15 = !DILocalVariable(name: "a", scope: !11, file: !3, line: 3, type: !16)
!16 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!17 = distinct !DIAssignID()
!18 = !DILocation(line: 0, scope: !11)
!19 = !DILocation(line: 3, column: 3, scope: !11)
!20 = !DILocation(line: 4, column: 7, scope: !21)
!21 = distinct !DILexicalBlock(scope: !11, file: !3, line: 4, column: 7)
!22 = !{!23, !23, i64 0}
!23 = !{!"bool", !24, i64 0}
!24 = !{!"omnipotent char", !25, i64 0}
!25 = !{!"Simple C++ TBAA"}
!26 = !{i8 0, i8 2}
!27 = !DILocation(line: 4, column: 7, scope: !11)
!28 = !DILocation(line: 5, column: 7, scope: !21)
!29 = !{!30, !30, i64 0}
!30 = !{!"int", !24, i64 0}
!31 = distinct !DIAssignID()
!32 = !DILocation(line: 5, column: 5, scope: !21)
!33 = !DILocation(line: 6, column: 10, scope: !11)
!34 = !DILocation(line: 7, column: 1, scope: !11)
!35 = !DILocation(line: 6, column: 3, scope: !11)
