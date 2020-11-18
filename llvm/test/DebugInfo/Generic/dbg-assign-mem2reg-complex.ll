; RUN: opt -S %s -mem2reg -o - | FileCheck %s

;; $ cat reduce.c
;; short a;
;; static int b(int* p) { return *p; }
;; int c(int d) {
;;   while (1) {
;;     if (b(&d))
;;       return 1;
;;     d &= 65535;
;;     a = d;
;;   }
;; }
;; 
;; IR grabbed before mem2reg after inlining in:
;; $ clang -O2 -g -Xclang -debug-coffee-chat
;;
;; Check that mem2reg's assignment tracking phi tracker workaround
;; can work with complex expressions (check it doesn't assert).

; CHECK: while.body:
; CHECK-NEXT: %d.addr.0 = phi i32 [ %d, %entry ], [ %and, %if.end ]
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %d.addr.0,{{.+}}, metadata !DIExpression(DW_OP_constu, 65535, DW_OP_and, DW_OP_stack_value))

@a = dso_local local_unnamed_addr global i16 0, align 2, !dbg !0

; Function Attrs: nofree norecurse nounwind uwtable writeonly
define dso_local i32 @_Z1ci(i32 %d) local_unnamed_addr #0 !dbg !11 {
entry:
  %d.addr = alloca i32, align 4, !DIAssignID !17
  call void @llvm.dbg.assign(metadata i1 undef, metadata !16, metadata !DIExpression(), metadata !17, metadata i32* %d.addr), !dbg !18
  store i32 %d, i32* %d.addr, align 4, !tbaa !19, !DIAssignID !23
  call void @llvm.dbg.assign(metadata i32 %d, metadata !16, metadata !DIExpression(), metadata !23, metadata i32* %d.addr), !dbg !18
  br label %while.body, !dbg !24

while.body:                                       ; preds = %entry, %if.end
  call void @llvm.dbg.assign(metadata i1 undef, metadata !25, metadata !DIExpression(), metadata !31, metadata i32** undef), !dbg !32
  call void @llvm.dbg.assign(metadata i32* %d.addr, metadata !25, metadata !DIExpression(), metadata !36, metadata i32** undef), !dbg !32
  %0 = load i32, i32* %d.addr, align 4, !dbg !37, !tbaa !19
  %tobool.not = icmp eq i32 %0, 0, !dbg !38
  br i1 %tobool.not, label %if.end, label %if.then, !dbg !39

if.then:                                          ; preds = %while.body
  ret i32 1, !dbg !40

if.end:                                           ; preds = %while.body
  %1 = load i32, i32* %d.addr, align 4, !dbg !41, !tbaa !19
  %and = and i32 %1, 65535, !dbg !41
  store i32 %and, i32* %d.addr, align 4, !dbg !41, !tbaa !19, !DIAssignID !42
  call void @llvm.dbg.assign(metadata i32 %1, metadata !16, metadata !DIExpression(DW_OP_constu, 65535, DW_OP_and, DW_OP_stack_value), metadata !42, metadata i32* %d.addr), !dbg !18
  %conv = trunc i32 %1 to i16, !dbg !43
  store i16 %conv, i16* @a, align 2, !dbg !44, !tbaa !45
  br label %while.body, !dbg !24, !llvm.loop !47
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!7, !8, !9}
!llvm.ident = !{!10}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "a", scope: !2, file: !3, line: 1, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "reduce.c", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = !DIBasicType(name: "short", size: 16, encoding: DW_ATE_signed)
!7 = !{i32 7, !"Dwarf Version", i32 4}
!8 = !{i32 2, !"Debug Info Version", i32 3}
!9 = !{i32 1, !"wchar_size", i32 4}
!10 = !{!"clang version 12.0.0"}
!11 = distinct !DISubprogram(name: "c", linkageName: "_Z1ci", scope: !3, file: !3, line: 3, type: !12, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !15)
!12 = !DISubroutineType(types: !13)
!13 = !{!14, !14}
!14 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!15 = !{!16}
!16 = !DILocalVariable(name: "d", arg: 1, scope: !11, file: !3, line: 3, type: !14)
!17 = distinct !DIAssignID()
!18 = !DILocation(line: 0, scope: !11)
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !21, i64 0}
!21 = !{!"omnipotent char", !22, i64 0}
!22 = !{!"Simple C++ TBAA"}
!23 = distinct !DIAssignID()
!24 = !DILocation(line: 4, column: 3, scope: !11)
!25 = !DILocalVariable(name: "p", arg: 1, scope: !26, file: !3, line: 2, type: !29)
!26 = distinct !DISubprogram(name: "b", linkageName: "_ZL1bPi", scope: !3, file: !3, line: 2, type: !27, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagLocalToUnit | DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !30)
!27 = !DISubroutineType(types: !28)
!28 = !{!14, !29}
!29 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!30 = !{!25}
!31 = distinct !DIAssignID()
!32 = !DILocation(line: 0, scope: !26, inlinedAt: !33)
!33 = distinct !DILocation(line: 5, column: 9, scope: !34)
!34 = distinct !DILexicalBlock(scope: !35, file: !3, line: 5, column: 9)
!35 = distinct !DILexicalBlock(scope: !11, file: !3, line: 4, column: 13)
!36 = distinct !DIAssignID()
!37 = !DILocation(line: 2, column: 31, scope: !26, inlinedAt: !33)
!38 = !DILocation(line: 5, column: 9, scope: !34)
!39 = !DILocation(line: 5, column: 9, scope: !35)
!40 = !DILocation(line: 6, column: 7, scope: !34)
!41 = !DILocation(line: 7, column: 7, scope: !35)
!42 = distinct !DIAssignID()
!43 = !DILocation(line: 8, column: 9, scope: !35)
!44 = !DILocation(line: 8, column: 7, scope: !35)
!45 = !{!46, !46, i64 0}
!46 = !{!"short", !21, i64 0}
!47 = distinct !{!47, !24, !48}
!48 = !DILocation(line: 9, column: 3, scope: !11)
