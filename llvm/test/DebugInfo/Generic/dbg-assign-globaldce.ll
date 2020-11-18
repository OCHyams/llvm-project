; RUN: opt -S %s -globaldce -o - | FileCheck %s

;; Check that globaldce replaces the debug use of a dead global with undef.

; CHECK-NOT: @__const._Z1hv.i
; CHECK: call void @llvm.dbg.assign({{.+}},{{.+}},{{.+}},{{.+}}, metadata [3 x i8]* undef)

@__const._Z1hv.i = private unnamed_addr constant [3 x i8] c"\01\00\07", align 1

; Function Attrs: norecurse nounwind readnone uwtable
define dso_local void @fun() local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !11, metadata !DIExpression(), metadata !22, metadata [3 x i8]* @__const._Z1hv.i), !dbg !10
  ret void, !dbg !10
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.c", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0)"}
!7 = distinct !DISubprogram(name: "fun", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !2)
!8 = !DISubroutineType(types: !9)
!9 = !{null}
!10 = !DILocation(line: 2, column: 1, scope: !7)
!11 = !DILocalVariable(name: "i", scope: !7, file: !1, line: 2, type: !60)
!13 = !DIBasicType(name: "unsigned char", size: 8, encoding: DW_ATE_unsigned_char)
!22 = distinct !DIAssignID()
!60 = !DICompositeType(tag: DW_TAG_array_type, baseType: !13, size: 24, elements: !61)
!61 = !{!62}
!62 = !DISubrange(count: 3)
