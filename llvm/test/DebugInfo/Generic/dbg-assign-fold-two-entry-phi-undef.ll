; RUN: opt %s -S -simplifycfg -o - | FileCheck %s

;; Check the FoldTwoEntryPhi inserts an undef in the successor block of if and if.then.

;; $cat test.cpp
;; typedef struct {
;;   short a;
;; } b;
;; long c;
;; void d(b *e) {
;;   if (c < 4)
;;     e->a = c;
;;   else
;;     e->a = c - 3;
;; }
;; NOTE: This test was generated with -track-ptr-arg-dest, which is no longer a
;; thing. Test adjusted by hand, but this source no longer runs into the
;; problem naturally

; CHECK:      call void @llvm.dbg.assign(metadata i16 undef,
; CHECK:      call void @llvm.dbg.assign(metadata i16 undef,
; CHECK-NEXT: %conv1.sink = select i1 %cmp, i16 %conv, i16 %conv1,

; ModuleID = './test.cpp'

source_filename = "./test.cpp"

%struct.b = type { i16 }

@c = dso_local local_unnamed_addr global i64 0, align 8, !dbg !0

; Function Attrs: nofree norecurse nounwind uwtable mustprogress
define dso_local void @_Z1dP1b(%struct.b* nocapture %e) local_unnamed_addr #0 !dbg !12 {
entry:
  call void @llvm.dbg.assign(metadata %struct.b undef, metadata !23, metadata !DIExpression(), metadata !24, metadata %struct.b* %e), !dbg !25
  %0 = load i64, i64* @c, align 8, !dbg !26, !tbaa !28
  %cmp = icmp slt i64 %0, 4, !dbg !32
  br i1 %cmp, label %if.then, label %if.else, !dbg !33

if.then:                                          ; preds = %entry
  %conv = trunc i64 %0 to i16, !dbg !34
  call void @llvm.dbg.assign(metadata i16 %conv, metadata !23, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 16), metadata !35, metadata i16* %2), !dbg !25
  br label %if.end, !dbg !36

if.else:                                          ; preds = %entry
  %1 = trunc i64 %0 to i16, !dbg !37
  %conv1 = add i16 %1, -3, !dbg !37
  call void @llvm.dbg.assign(metadata i16 %conv1, metadata !23, metadata !DIExpression(DW_OP_LLVM_fragment, 0, 16), metadata !35, metadata i16* %2), !dbg !25
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %conv1.sink = phi i16 [ %conv, %if.then ], [ %conv1, %if.else ], !dbg !38
  %2 = getelementptr inbounds %struct.b, %struct.b* %e, i64 0, i32 0, !dbg !39
  store i16 %conv1.sink, i16* %2, align 2, !dbg !40, !DIAssignID !35
  ret void, !dbg !41
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #1

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!8, !9, !10}
!llvm.ident = !{!11}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "c", scope: !2, file: !6, line: 4, type: !7, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "")
!4 = !{}
!5 = !{!0}
!6 = !DIFile(filename: "./test.cpp", directory: "")
!7 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!8 = !{i32 7, !"Dwarf Version", i32 4}
!9 = !{i32 2, !"Debug Info Version", i32 3}
!10 = !{i32 1, !"wchar_size", i32 4}
!11 = !{!"clang version 12.0.0"}
!12 = distinct !DISubprogram(name: "d", linkageName: "_Z1dP1b", scope: !6, file: !6, line: 5, type: !13, scopeLine: 5, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !21)
!13 = !DISubroutineType(types: !14)
!14 = !{null, !15}
!15 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !16, size: 64)
!16 = !DIDerivedType(tag: DW_TAG_typedef, name: "b", file: !6, line: 3, baseType: !17)
!17 = distinct !DICompositeType(tag: DW_TAG_structure_type, file: !6, line: 1, size: 16, flags: DIFlagTypePassByValue, elements: !18, identifier: "_ZTS1b")
!18 = !{!19}
!19 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !17, file: !6, line: 2, baseType: !20, size: 16)
!20 = !DIBasicType(name: "short", size: 16, encoding: DW_ATE_signed)
!21 = !{!22}
!22 = !DILocalVariable(name: "e", arg: 1, scope: !12, file: !6, line: 5, type: !15)
!23 = !DILocalVariable(name: "hand-adjusted-variable", scope: !12, file: !6, type: !15)
!24 = distinct !DIAssignID()
!25 = !DILocation(line: 0, scope: !12)
!26 = !DILocation(line: 6, column: 7, scope: !27)
!27 = distinct !DILexicalBlock(scope: !12, file: !6, line: 6, column: 7)
!28 = !{!29, !29, i64 0}
!29 = !{!"long", !30, i64 0}
!30 = !{!"omnipotent char", !31, i64 0}
!31 = !{!"Simple C++ TBAA"}
!32 = !DILocation(line: 6, column: 9, scope: !27)
!33 = !DILocation(line: 6, column: 7, scope: !12)
!34 = !DILocation(line: 7, column: 12, scope: !27)
!35 = distinct !DIAssignID()
!36 = !DILocation(line: 7, column: 5, scope: !27)
!37 = !DILocation(line: 9, column: 12, scope: !27)
!38 = !DILocation(line: 0, scope: !27)
!39 = !DILocation(line: 7, column: 8, scope: !27)
!40 = !DILocation(line: 7, column: 10, scope: !27)
!41 = !DILocation(line: 10, column: 1, scope: !12)
