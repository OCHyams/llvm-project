; RUN:  opt %s -o - -S 2>&1 |  FileCheck %s
;; Check the verifier enforces DILocation::atomRank < DISubprogram::nextAtomGroup.

; CHECK: DbgLoc's atomGroup should be less than SP's nextAtomGroup
; CHECK-NEXT: ![[#]] = !DILocation(line: 1, scope: ![[f:.*]], atomGroup: 1, atomRank: 1)
; CHECK-NEXT: 1
; CHECK-NEXT: ![[f]] = distinct !DISubprogram(name: "f"{{.*}}, nextAtomGroup: 1)
; CHECK-NEXT: 1

; CHECK: DbgLoc's atomGroup should be less than SP's nextAtomGroup
; CHECK-NEXT: ![[#]] = !DILocation(line: 2, scope: ![[g:.*]], atomGroup: 4, atomRank: 1)
; CHECK-NEXT: 4
; CHECK-NEXT: ![[g]] = distinct !DISubprogram(name: "g"{{.*}}, nextAtomGroup: 3)
; CHECK-NEXT: 3

define dso_local void @f() !dbg !10 {
entry:
  ret void, !dbg !13
}

define dso_local void @g() !dbg !15 {
entry:
  ret void, !dbg !14
}

!llvm.module.flags = !{!3}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 21.0.0git", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!3 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{!"clang version 21.0.0git"}
!10 = distinct !DISubprogram(name: "f", scope: !1, file: !1, line: 1, type: !11, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, nextAtomGroup: 1)
!11 = !DISubroutineType(types: !12)
!12 = !{null}
!13 = !DILocation(line: 1, scope: !10, atomGroup: 1, atomRank: 1)
!14 = !DILocation(line: 2, scope: !15, atomGroup: 4, atomRank: 1)
!15 = distinct !DISubprogram(name: "g", scope: !1, file: !1, line: 1, type: !11, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, nextAtomGroup: 3)
