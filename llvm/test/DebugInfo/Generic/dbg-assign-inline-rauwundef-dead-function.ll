; RUN: opt %s -S -o - -inline | FileCheck %s

;; $ cat test.c
;; char *a;
;; __attribute__((noinline))
;; static b(long c(void *)) { c(a); }
;; static long d(char *) { return 0; }
;; void e() { b(d); }
;;
;; IR taken after SROA/mem2reg for:
;; $ clang -O2 -g -Xclang -debug-coffee-chat -mllvm -track-ptr-arg-dest test.c
;;                                           ^^^^^^^^^^^^^^^^^^^^^^^^^^
;; NOTE: This test was generated with -track-ptr-arg-dest, which is no longer a
;; thing. Test adjusted by hand, but this source no longer runs into the
;; problem naturally
;;
;; Check that the dbg.assign destination component for the AnonPtrTarget
;; variable in @b is made undef - rather than just deleting the value - when
;; deleting @d after inlining @d into @b (ipsccp has made the indirect call to
;; @d through %c in @b).

; CHECK: define internal fastcc void @b()
; CHECK-NEXT: entry:
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef,{{.+}},{{.+}},{{.+}}, metadata i64 (i8*)* undef)

@a = dso_local local_unnamed_addr global i8* null, align 8, !dbg !0

define dso_local void @e() local_unnamed_addr #0 !dbg !12 {
entry:
  call fastcc void @b(), !dbg !15
  ret void, !dbg !16
}

define internal fastcc void @b() unnamed_addr #1 !dbg !17 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !28, metadata !DIExpression(), metadata !29, metadata i64 (i8*)* @d), !dbg !30
  %0 = load i8*, i8** @a, align 8, !dbg !31
  %call = call i64 @d(i8* %0), !dbg !32
  ret void, !dbg !33
}

define internal i64 @d(i8* nocapture readnone %0) #2 !dbg !34 {
entry:
  ret i64 0, !dbg !42
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)
attributes #1 = { noinline }


!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!8, !9, !10}
!llvm.ident = !{!11}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 1, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C99, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.c", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !7, size: 64)
!7 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!8 = !{i32 7, !"Dwarf Version", i32 4}
!9 = !{i32 2, !"Debug Info Version", i32 3}
!10 = !{i32 1, !"wchar_size", i32 4}
!11 = !{!"clang version 12.0.0"}
!12 = distinct !DISubprogram(name: "e", scope: !3, file: !3, line: 5, type: !13, scopeLine: 5, flags: DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !4)
!13 = !DISubroutineType(types: !14)
!14 = !{null}
!15 = !DILocation(line: 5, column: 12, scope: !12)
!16 = !DILocation(line: 5, column: 18, scope: !12)
!17 = distinct !DISubprogram(name: "b", scope: !3, file: !3, line: 3, type: !18, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !26)
!18 = !DISubroutineType(types: !19)
!19 = !{!20, !21}
!20 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!21 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !22, size: 64)
!22 = !DISubroutineType(types: !23)
!23 = !{!24, !25}
!24 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!25 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!26 = !{!27}
!27 = !DILocalVariable(name: "c", arg: 1, scope: !17, file: !3, line: 3, type: !21)
!28 = !DILocalVariable(name: "hand-adjusted-variable", scope: !17, file: !3, type: !21)
!29 = distinct !DIAssignID()
!30 = !DILocation(line: 0, scope: !17)
!31 = !DILocation(line: 3, column: 30, scope: !17)
!32 = !DILocation(line: 3, column: 28, scope: !17)
!33 = !DILocation(line: 3, column: 34, scope: !17)
!34 = distinct !DISubprogram(name: "d", scope: !3, file: !3, line: 4, type: !35, scopeLine: 4, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !37)
!35 = !DISubroutineType(types: !36)
!36 = !{!24, !6}
!37 = !{!38}
!38 = !DILocalVariable(arg: 1, scope: !34, file: !3, line: 4, type: !6)
!40 = distinct !DIAssignID()
!41 = !DILocation(line: 0, scope: !34)
!42 = !DILocation(line: 4, column: 25, scope: !34)
