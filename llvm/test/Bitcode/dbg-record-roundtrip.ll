; RUN: llvm-as %s | llvm-dis | FileCheck %s

; CHECK: dbg.declare

@g = dso_local global i32 0, align 4, !dbg !0

define dso_local noundef i32 @_Z3funv() !dbg !13 {
entry:
  %a = alloca i32, align 4
  tail call void @llvm.dbg.declare(metadata ptr %a, metadata !17, metadata !DIExpression()), !dbg !19
  %0 = load i32, ptr @g, align 4, !dbg !20
  store i32 %0, ptr %a, align 4, !dbg !19
  %1 = load i32, ptr %a, align 4, !dbg !25
  ret i32 %1, !dbg !27
}

declare void @llvm.dbg.declare(metadata, metadata, metadata)
declare void @llvm.dbg.value(metadata, metadata, metadata)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!6, !7, !8, !9, !10, !11}
!llvm.ident = !{!12}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "g", scope: !2, file: !3, line: 1, type: !5, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 19.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{!0}
!5 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!6 = !{i32 7, !"Dwarf Version", i32 5}
!7 = !{i32 2, !"Debug Info Version", i32 3}
!8 = !{i32 1, !"wchar_size", i32 4}
!9 = !{i32 8, !"PIC Level", i32 2}
!10 = !{i32 7, !"PIE Level", i32 2}
!11 = !{i32 7, !"uwtable", i32 2}
!12 = !{!"clang version 19.0.0"}
!13 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !3, file: !3, line: 2, type: !14, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !16)
!14 = !DISubroutineType(types: !15)
!15 = !{!5}
!16 = !{!17}
!17 = !DILocalVariable(name: "a", scope: !13, file: !3, line: 3, type: !5)
!18 = !DILocation(line: 3, column: 3, scope: !13)
!19 = !DILocation(line: 3, column: 7, scope: !13)
!20 = !DILocation(line: 3, column: 11, scope: !13)
!25 = !DILocation(line: 4, column: 12, scope: !13)
!26 = !DILocation(line: 5, column: 1, scope: !13)
!27 = !DILocation(line: 4, column: 5, scope: !13)