; RUN: opt -sroa -verify -S %s -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg"

; Check that allocas that go through a noop re-write keep their DIAssignID and
; dbg.assign intrinic.

;; $ cat test.cpp
;; struct LargeStruct {
;;   int A[6];
;;   int B;
;; };
;; LargeStruct From;
;; int example() {
;;   LargeStruct To = From;
;;   return To.A[To.B];
;; }
;; $ clang test.cpp -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes -O2 -g -c -S -emit-llvm -o -

;; (Un)split alloca.
; CHECK: entry:
; CHECK-NEXT: %To = alloca %struct.LargeStruct, align 4, !DIAssignID ![[ID_1:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR:[0-9]+]], metadata !DIExpression(), metadata ![[ID_1]], metadata %struct.LargeStruct* %To), !dbg

; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %1, i8* align 4 bitcast (%struct.LargeStruct* @From to i8*), i64 28, i1 false),{{.*}}!DIAssignID ![[ID_2:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR]], metadata !DIExpression(), metadata ![[ID_2]], metadata i8* %1), !dbg

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

%struct.LargeStruct = type { [6 x i32], i32 }

@From = dso_local global %struct.LargeStruct zeroinitializer, align 4, !dbg !0

; Function Attrs: nounwind uwtable mustprogress
define dso_local i32 @_Z7examplev() !dbg !18 {
entry:
  %To = alloca %struct.LargeStruct, align 4, !DIAssignID !23
  call void @llvm.dbg.assign(metadata i1 undef, metadata !22, metadata !DIExpression(), metadata !23, metadata %struct.LargeStruct* %To), !dbg !24
  %0 = bitcast %struct.LargeStruct* %To to i8*, !dbg !25
  call void @llvm.lifetime.start.p0i8(i64 28, i8* %0), !dbg !25
  %1 = bitcast %struct.LargeStruct* %To to i8*, !dbg !26
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %1, i8* align 4 bitcast (%struct.LargeStruct* @From to i8*), i64 28, i1 false), !dbg !26, !tbaa.struct !27, !DIAssignID !33
  call void @llvm.dbg.assign(metadata i1 undef, metadata !22, metadata !DIExpression(), metadata !33, metadata i8* %1), !dbg !26
  %A = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %To, i32 0, i32 0, !dbg !34
  %B = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %To, i32 0, i32 1, !dbg !35
  %2 = load i32, i32* %B, align 4, !dbg !35, !tbaa !36
  %idxprom = sext i32 %2 to i64, !dbg !38
  %arrayidx = getelementptr inbounds [6 x i32], [6 x i32]* %A, i64 0, i64 %idxprom, !dbg !38
  %3 = load i32, i32* %arrayidx, align 4, !dbg !38, !tbaa !31
  %4 = bitcast %struct.LargeStruct* %To to i8*, !dbg !39
  call void @llvm.lifetime.end.p0i8(i64 28, i8* %4), !dbg !39
  ret i32 %3, !dbg !40
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg)
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!14, !15, !16}
!llvm.ident = !{!17}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "From", scope: !2, file: !3, line: 5, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{}
!5 = !{!0}
!6 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "LargeStruct", file: !3, line: 1, size: 224, flags: DIFlagTypePassByValue, elements: !7, identifier: "_ZTS11LargeStruct")
!7 = !{!8, !13}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "A", scope: !6, file: !3, line: 2, baseType: !9, size: 192)
!9 = !DICompositeType(tag: DW_TAG_array_type, baseType: !10, size: 192, elements: !11)
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12}
!12 = !DISubrange(count: 6)
!13 = !DIDerivedType(tag: DW_TAG_member, name: "B", scope: !6, file: !3, line: 3, baseType: !10, size: 32, offset: 192)
!14 = !{i32 7, !"Dwarf Version", i32 4}
!15 = !{i32 2, !"Debug Info Version", i32 3}
!16 = !{i32 1, !"wchar_size", i32 4}
!17 = !{!"clang version 12.0.0"}
!18 = distinct !DISubprogram(name: "example", linkageName: "_Z7examplev", scope: !3, file: !3, line: 6, type: !19, scopeLine: 6, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !21)
!19 = !DISubroutineType(types: !20)
!20 = !{!10}
!21 = !{!22}
!22 = !DILocalVariable(name: "To", scope: !18, file: !3, line: 7, type: !6)
!23 = distinct !DIAssignID()
!24 = !DILocation(line: 0, scope: !18)
!25 = !DILocation(line: 7, column: 3, scope: !18)
!26 = !DILocation(line: 7, column: 20, scope: !18)
!27 = !{i64 0, i64 24, !28, i64 24, i64 4, !31}
!28 = !{!29, !29, i64 0}
!29 = !{!"omnipotent char", !30, i64 0}
!30 = !{!"Simple C++ TBAA"}
!31 = !{!32, !32, i64 0}
!32 = !{!"int", !29, i64 0}
!33 = distinct !DIAssignID()
!34 = !DILocation(line: 8, column: 13, scope: !18)
!35 = !DILocation(line: 8, column: 18, scope: !18)
!36 = !{!37, !32, i64 24}
!37 = !{!"_ZTS11LargeStruct", !29, i64 0, !32, i64 24}
!38 = !DILocation(line: 8, column: 10, scope: !18)
!39 = !DILocation(line: 9, column: 1, scope: !18)
!40 = !DILocation(line: 8, column: 3, scope: !18)
