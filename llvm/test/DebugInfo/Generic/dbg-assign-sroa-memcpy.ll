; RUN: opt -sroa -verify -S %s -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg"

;; Check that the new slices of an alloca and memcpy intructions get dbg.assign
;; intrinsics with the correct fragment info.
;;
;; Also check that the new dbg.assign intrinsics are inserted after each split
;; store. See llvm/test/DebugInfo/Generic/dbg-assign-sroa-id.ll for the
;; counterpart check.

;; $ cat test.cpp
;; struct LargeStruct {
;;   int A, B, C;
;;   int Var;
;;   int D, E, F;
;; };
;; LargeStruct From;
;; int example() {
;;   LargeStruct To = From;
;;   return To.Var;
;; }
;; $ clang test.cpp -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes -O2 -g -c -S -emit-llvm -o -

;; Split alloca.
; CHECK: entry:
; CHECK-NEXT: %To.sroa.0 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_1:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata {{.+}} undef, metadata ![[TO:[0-9]+]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_1]], metadata { i32, i32, i32 }* %To.sroa.0), !dbg

; CHECK-NEXT: call void @llvm.dbg.assign(metadata {{.+}} undef, metadata ![[TO]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg

; CHECK-NEXT: %To.sroa.4 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_3:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata {{.+}} undef, metadata ![[TO]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_3]], metadata { i32, i32, i32 }* %To.sroa.4), !dbg

;; Splt memcpy.
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %To.sroa.0.0..sroa_cast1, i8* align 4 bitcast (%struct.LargeStruct* @From to i8*), i64 12, i1 false),{{.*}}!DIAssignID ![[ID_4:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata {{.+}} undef, metadata ![[TO]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_4]], metadata i8* %To.sroa.0.0..sroa_cast1), !dbg

;; This slice has been split into a load/store->load.
; CHECK: %To.sroa.3.0.copyload = load i32, i32* getelementptr inbounds (%struct.LargeStruct, %struct.LargeStruct* @From, i64 0, i32 3), align 4
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %To.sroa.3.0.copyload, metadata ![[TO]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg

; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %To.sroa.4.0..sroa_cast4, i8* align 4 bitcast (i32* getelementptr inbounds (%struct.LargeStruct, %struct.LargeStruct* @From, i64 0, i32 4) to i8*), i64 12, i1 false){{.*}}!DIAssignID ![[ID_6:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata {{.+}} undef, metadata ![[TO]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_6]], metadata i8* %To.sroa.4.0..sroa_cast4), !dbg

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

%struct.LargeStruct = type { i32, i32, i32, i32, i32, i32, i32 }

@From = dso_local global %struct.LargeStruct zeroinitializer, align 4, !dbg !0

; Function Attrs: nounwind uwtable mustprogress
define dso_local i32 @_Z7examplev() #0 !dbg !20 {
entry:
  %To = alloca %struct.LargeStruct, align 4, !DIAssignID !25
  call void @llvm.dbg.assign(metadata i1 undef, metadata !24, metadata !DIExpression(), metadata !25, metadata %struct.LargeStruct* %To), !dbg !26
  %0 = bitcast %struct.LargeStruct* %To to i8*, !dbg !27
  call void @llvm.lifetime.start.p0i8(i64 28, i8* %0) #3, !dbg !27
  %1 = bitcast %struct.LargeStruct* %To to i8*, !dbg !28
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %1, i8* align 4 bitcast (%struct.LargeStruct* @From to i8*), i64 28, i1 false), !dbg !28, !tbaa.struct !29, !DIAssignID !34
  call void @llvm.dbg.assign(metadata i1 undef, metadata !24, metadata !DIExpression(), metadata !34, metadata i8* %1), !dbg !28
  %Var = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %To, i32 0, i32 3, !dbg !35
  %2 = load i32, i32* %Var, align 4, !dbg !35, !tbaa !36
  %3 = bitcast %struct.LargeStruct* %To to i8*, !dbg !38
  call void @llvm.lifetime.end.p0i8(i64 28, i8* %3) #3, !dbg !38
  ret i32 %2, !dbg !39
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #2

attributes #0 = { nounwind uwtable mustprogress "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #3 = { nounwind }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!16, !17, !18}
!llvm.ident = !{!19}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "From", scope: !2, file: !3, line: 6, type: !6, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git 4b1385b30705c53eb00e1471ec419c67ec98cc7b)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "sroa-test.cpp", directory: "/home/och/dev/bugs/scratch")
!4 = !{}
!5 = !{!0}
!6 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "LargeStruct", file: !3, line: 1, size: 224, flags: DIFlagTypePassByValue, elements: !7, identifier: "_ZTS11LargeStruct")
!7 = !{!8, !10, !11, !12, !13, !14, !15}
!8 = !DIDerivedType(tag: DW_TAG_member, name: "A", scope: !6, file: !3, line: 2, baseType: !9, size: 32)
!9 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!10 = !DIDerivedType(tag: DW_TAG_member, name: "B", scope: !6, file: !3, line: 2, baseType: !9, size: 32, offset: 32)
!11 = !DIDerivedType(tag: DW_TAG_member, name: "C", scope: !6, file: !3, line: 2, baseType: !9, size: 32, offset: 64)
!12 = !DIDerivedType(tag: DW_TAG_member, name: "Var", scope: !6, file: !3, line: 3, baseType: !9, size: 32, offset: 96)
!13 = !DIDerivedType(tag: DW_TAG_member, name: "D", scope: !6, file: !3, line: 4, baseType: !9, size: 32, offset: 128)
!14 = !DIDerivedType(tag: DW_TAG_member, name: "E", scope: !6, file: !3, line: 4, baseType: !9, size: 32, offset: 160)
!15 = !DIDerivedType(tag: DW_TAG_member, name: "F", scope: !6, file: !3, line: 4, baseType: !9, size: 32, offset: 192)
!16 = !{i32 7, !"Dwarf Version", i32 4}
!17 = !{i32 2, !"Debug Info Version", i32 3}
!18 = !{i32 1, !"wchar_size", i32 4}
!19 = !{!"clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git 4b1385b30705c53eb00e1471ec419c67ec98cc7b)"}
!20 = distinct !DISubprogram(name: "example", linkageName: "_Z7examplev", scope: !3, file: !3, line: 7, type: !21, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !23)
!21 = !DISubroutineType(types: !22)
!22 = !{!9}
!23 = !{!24}
!24 = !DILocalVariable(name: "To", scope: !20, file: !3, line: 8, type: !6)
!25 = distinct !DIAssignID()
!26 = !DILocation(line: 0, scope: !20)
!27 = !DILocation(line: 8, column: 3, scope: !20)
!28 = !DILocation(line: 8, column: 20, scope: !20)
!29 = !{i64 0, i64 4, !30, i64 4, i64 4, !30, i64 8, i64 4, !30, i64 12, i64 4, !30, i64 16, i64 4, !30, i64 20, i64 4, !30, i64 24, i64 4, !30}
!30 = !{!31, !31, i64 0}
!31 = !{!"int", !32, i64 0}
!32 = !{!"omnipotent char", !33, i64 0}
!33 = !{!"Simple C++ TBAA"}
!34 = distinct !DIAssignID()
!35 = !DILocation(line: 9, column: 13, scope: !20)
!36 = !{!37, !31, i64 12}
!37 = !{!"_ZTS11LargeStruct", !31, i64 0, !31, i64 4, !31, i64 8, !31, i64 12, !31, i64 16, !31, i64 20, !31, i64 24}
!38 = !DILocation(line: 10, column: 1, scope: !20)
!39 = !DILocation(line: 9, column: 3, scope: !20)
