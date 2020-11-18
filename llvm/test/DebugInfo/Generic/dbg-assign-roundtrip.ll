; RUN: opt -verify %s | opt -verify -S | FileCheck %s

;; Round trip test for dbg.assign and DIAssignID metadata. DIAssignID links one
;; or more dbg.assign intrinsics to the assigning instruction.

; CHECK: store i32 0, i32* %local{{.*}} !DIAssignID ![[ID:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[LOCAL:[0-9]+]], metadata !DIExpression(), metadata ![[ID]], metadata i32* %local), !dbg ![[DBG:[0-9]+]]
; CHECK-DAG: ![[LOCAL]] = !DILocalVariable(name: "local",
; CHECK-DAG: ![[DBG]] = !DILocation(line: 2, column: 7,
; CHECK-DAG: ![[ID]] = distinct !DIAssignID()

define dso_local void @fun() !dbg !7 {
entry:
  %local = alloca i32, align 4
  store i32 0, i32* %local, align 4, !dbg !12, !DIAssignID !14
  call void @llvm.dbg.assign(metadata i32 0, metadata !10, metadata !DIExpression(), metadata !14, metadata i32* %local), !dbg !12
  ret void, !dbg !13
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 12.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.c", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "fun", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
!10 = !DILocalVariable(name: "local", scope: !7, file: !1, line: 2, type: !11)
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DILocation(line: 2, column: 7, scope: !7)
!13 = !DILocation(line: 3, column: 1, scope: !7)
!14 = distinct !DIAssignID()
