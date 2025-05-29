; RUN: opt %s -o - -S | llvm-as - | llvm-dis - | FileCheck %s

; Key Instructions enabled.
define dso_local void @f() !dbg !5 {
entry:
  ret void, !dbg !8  ; atomGroup: 1, atomRank: 1
}

; Key Instructions enabled, atomGroup > 1 and forward-ref scope in DILocation.
define dso_local void @g() !dbg !10 {
entry:
  ret void, !dbg !9  ; atomGroup: 2, atomRank: 1
}

; Key Instructions not enabled.
define dso_local void @h() !dbg !11 {
entry:
  ret void, !dbg !12 ; atomGroup: 0, atomRank: 0
}

; Key Instructions, inlined instruction (from g).
define dso_local void @i() !dbg !13 {
entry:
  ret void, !dbg !14 ; scope g, inlinedAt i:4, atomGroup: 2, atomRank: 0
}

!llvm.dbg.cu = !{!0}
!llvm.debugify = !{!2, !3}
!llvm.module.flags = !{!4}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "debugify", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "roundtrip.ll", directory: "/")
!2 = !{i32 2}
!3 = !{i32 0}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = distinct !DISubprogram(name: "f", linkageName: "f", scope: null, file: !1, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, nextAtomGroup: 2)
!6 = !DISubroutineType(types: !7)
!7 = !{}
!8 = !DILocation(line: 1, scope: !5, atomGroup: 1, atomRank: 1)
!9 = !DILocation(line: 2, scope: !10, atomGroup: 2, atomRank: 1) ; fwd-ref scope.
!10 = distinct !DISubprogram(name: "g", linkageName: "g", scope: null, file: !1, line: 2, type: !6, scopeLine: 2, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, nextAtomGroup: 3)
!11 = distinct !DISubprogram(name: "h", linkageName: "h", scope: null, file: !1, line: 3, type: !6, scopeLine: 3, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!12 = !DILocation(line: 3, scope: !11)
!13 = distinct !DISubprogram(name: "i", linkageName: "i", scope: null, file: !1, line: 4, type: !6, scopeLine: 4, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, nextAtomGroup: 3)
!14 = !DILocation(line: 2, scope: !10, inlinedAt: !15, atomGroup: 2, atomRank: 1) ; fwd-ref inlinedAt.
!15 = distinct !DILocation(line: 4, scope: !13) ; fwd-ref scope.

