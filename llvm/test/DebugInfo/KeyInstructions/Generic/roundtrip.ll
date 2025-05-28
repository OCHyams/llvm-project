; RUN: opt %s -o - -S | llvm-as - | llvm-dis - | FileCheck %s

define dso_local void @f() !dbg !5 {
entry:
  ret void, !dbg !8
}

define dso_local void @g() !dbg !9 {
entry:
  ret void, !dbg !10
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
!8 = !DILocation(line: 1, column: 1, scope: !5, atomGroup: 1, atomRank: 1)
!9 = distinct !DISubprogram(name: "g", linkageName: "g", scope: null, file: !1, line: 2, type: !6, scopeLine: 2, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, nextAtomGroup: 3)
!10 = !DILocation(line: 2, column: 1, scope: !9, atomGroup: 2, atomRank: 1)
