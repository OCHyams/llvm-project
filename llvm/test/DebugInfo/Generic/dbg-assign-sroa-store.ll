; RUN: opt -sroa -verify -S %s -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg"

; Check that the new slices of an alloca and memset intructions get dbg.assign
; intrinsics with the correct fragment info.

;; $ cat test.cpp
;; void do_something();
;; struct LargeStruct {
;;   int A, B, C;
;;   int Var;
;;   int D, E, F;
;; };
;; int Glob;
;; bool Cond;
;; int use(LargeStruct);
;; int example() {
;;   LargeStruct S = {0};
;;   S.Var = Glob;
;;   use(S);
;;   return S.Var;
;; }
;; $ clang test.cpp -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes -O2 -g -c -S -emit-llvm -o -

; CHECK: entry:
; CHECK-NEXT:   %S.sroa.0 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_1:[0-9]+]]
; CHECK-NEXT:   call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR:[0-9]+]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_1]], metadata { i32, i32, i32 }* %S.sroa.0), !dbg

; CHECK-NEXT:   call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg

; CHECK-NEXT:   %S.sroa.6 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_3:[0-9]+]]
; CHECK-NEXT:   call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_3]], metadata { i32, i32, i32 }* %S.sroa.6), !dbg

;; The memset has been split into [0, 96)[96, 128)[128, 224) bit slices. The
;; memset for the middle slice has been removed.
; CHECK: call void @llvm.memset.p0i8.i64(i8* align 8 %S.sroa.0.0..sroa_cast19, i8 0, i64 12, i1 false), !dbg !{{.+}}, !DIAssignID ![[ID_4:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 0, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_4]], metadata i8* %S.sroa.0.0..sroa_cast19), !dbg
;;
;; This is the one we care about most in this test: check that a memset->store
;; gets a correct dbg.assign.
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg
;;
; CHECK-NEXT: %S.sroa.6.0..sroa_cast13 = bitcast { i32, i32, i32 }* %S.sroa.6 to i8*, !dbg
; CHECK-NEXT: call void @llvm.memset.p0i8.i64(i8* align 8 %S.sroa.6.0..sroa_cast13, i8 0, i64 12, i1 false), !dbg !{{.+}}, !DIAssignID ![[ID_5:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 0, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_5]], metadata i8* %S.sroa.6.0..sroa_cast13), !dbg

;; The load from global+store becomes a load.
;; FIXME: In reality it is actually stored again later on.
; CHECK-NEXT: %0 = load i32, i32* @Glob, align 4, !dbg !{{.+}}, !tbaa !{{.+}}
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %0, metadata ![[VAR]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg !


target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

%struct.LargeStruct = type { i32, i32, i32, i32, i32, i32, i32 }

@Glob = dso_local global i32 0, align 4, !dbg !0
@Cond = dso_local global i8 0, align 1, !dbg !6

; Function Attrs: uwtable mustprogress
define dso_local i32 @_Z7examplev() #0 !dbg !14 {
entry:
  %S = alloca %struct.LargeStruct, align 4, !DIAssignID !28
  call void @llvm.dbg.assign(metadata i1 undef, metadata !18, metadata !DIExpression(), metadata !28, metadata %struct.LargeStruct* %S), !dbg !29
  %agg.tmp = alloca %struct.LargeStruct, align 8
  %0 = bitcast %struct.LargeStruct* %S to i8*, !dbg !30
  call void @llvm.lifetime.start.p0i8(i64 28, i8* %0) #5, !dbg !30
  %1 = bitcast %struct.LargeStruct* %S to i8*, !dbg !31
  call void @llvm.memset.p0i8.i64(i8* align 4 %1, i8 0, i64 28, i1 false), !dbg !31, !DIAssignID !32
  call void @llvm.dbg.assign(metadata i8 0, metadata !18, metadata !DIExpression(), metadata !32, metadata i8* %1), !dbg !31
  %2 = load i32, i32* @Glob, align 4, !dbg !33, !tbaa !34
  %Var = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %S, i32 0, i32 3, !dbg !38
  store i32 %2, i32* %Var, align 4, !dbg !39, !tbaa !40, !DIAssignID !42
  call void @llvm.dbg.assign(metadata i32 %2, metadata !18, metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !42, metadata i32* %Var), !dbg !39
  %3 = bitcast %struct.LargeStruct* %agg.tmp to i8*, !dbg !43
  %4 = bitcast %struct.LargeStruct* %S to i8*, !dbg !43
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 4 %3, i8* align 4 %4, i64 28, i1 false), !dbg !43, !tbaa.struct !44
  %call = call i32 @_Z3use11LargeStruct(%struct.LargeStruct* byval(%struct.LargeStruct) align 8 %agg.tmp), !dbg !45
  %Var1 = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %S, i32 0, i32 3, !dbg !46
  %5 = load i32, i32* %Var1, align 4, !dbg !46, !tbaa !40
  %6 = bitcast %struct.LargeStruct* %S to i8*, !dbg !47
  call void @llvm.lifetime.end.p0i8(i64 28, i8* %6) #5, !dbg !47
  ret i32 %5, !dbg !48
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn writeonly
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg) #2

declare !dbg !49 dso_local i32 @_Z3use11LargeStruct(%struct.LargeStruct* byval(%struct.LargeStruct) align 8) #3

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #4

attributes #0 = { uwtable mustprogress "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { argmemonly nofree nosync nounwind willreturn writeonly }
attributes #3 = { "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #5 = { nounwind }

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!10, !11, !12}
!llvm.ident = !{!13}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "Glob", scope: !2, file: !3, line: 7, type: !9, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !4, globals: !5, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{}
!5 = !{!0, !6}
!6 = !DIGlobalVariableExpression(var: !7, expr: !DIExpression())
!7 = distinct !DIGlobalVariable(name: "Cond", scope: !2, file: !3, line: 8, type: !8, isLocal: false, isDefinition: true)
!8 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!9 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!10 = !{i32 7, !"Dwarf Version", i32 4}
!11 = !{i32 2, !"Debug Info Version", i32 3}
!12 = !{i32 1, !"wchar_size", i32 4}
!13 = !{!"clang version 12.0.0"}
!14 = distinct !DISubprogram(name: "example", linkageName: "_Z7examplev", scope: !3, file: !3, line: 10, type: !15, scopeLine: 10, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !17)
!15 = !DISubroutineType(types: !16)
!16 = !{!9}
!17 = !{!18}
!18 = !DILocalVariable(name: "S", scope: !14, file: !3, line: 11, type: !19)
!19 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "LargeStruct", file: !3, line: 2, size: 224, flags: DIFlagTypePassByValue, elements: !20, identifier: "_ZTS11LargeStruct")
!20 = !{!21, !22, !23, !24, !25, !26, !27}
!21 = !DIDerivedType(tag: DW_TAG_member, name: "A", scope: !19, file: !3, line: 3, baseType: !9, size: 32)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "B", scope: !19, file: !3, line: 3, baseType: !9, size: 32, offset: 32)
!23 = !DIDerivedType(tag: DW_TAG_member, name: "C", scope: !19, file: !3, line: 3, baseType: !9, size: 32, offset: 64)
!24 = !DIDerivedType(tag: DW_TAG_member, name: "Var", scope: !19, file: !3, line: 4, baseType: !9, size: 32, offset: 96)
!25 = !DIDerivedType(tag: DW_TAG_member, name: "D", scope: !19, file: !3, line: 5, baseType: !9, size: 32, offset: 128)
!26 = !DIDerivedType(tag: DW_TAG_member, name: "E", scope: !19, file: !3, line: 5, baseType: !9, size: 32, offset: 160)
!27 = !DIDerivedType(tag: DW_TAG_member, name: "F", scope: !19, file: !3, line: 5, baseType: !9, size: 32, offset: 192)
!28 = distinct !DIAssignID()
!29 = !DILocation(line: 0, scope: !14)
!30 = !DILocation(line: 11, column: 3, scope: !14)
!31 = !DILocation(line: 11, column: 15, scope: !14)
!32 = distinct !DIAssignID()
!33 = !DILocation(line: 12, column: 11, scope: !14)
!34 = !{!35, !35, i64 0}
!35 = !{!"int", !36, i64 0}
!36 = !{!"omnipotent char", !37, i64 0}
!37 = !{!"Simple C++ TBAA"}
!38 = !DILocation(line: 12, column: 5, scope: !14)
!39 = !DILocation(line: 12, column: 9, scope: !14)
!40 = !{!41, !35, i64 12}
!41 = !{!"_ZTS11LargeStruct", !35, i64 0, !35, i64 4, !35, i64 8, !35, i64 12, !35, i64 16, !35, i64 20, !35, i64 24}
!42 = distinct !DIAssignID()
!43 = !DILocation(line: 13, column: 7, scope: !14)
!44 = !{i64 0, i64 4, !34, i64 4, i64 4, !34, i64 8, i64 4, !34, i64 12, i64 4, !34, i64 16, i64 4, !34, i64 20, i64 4, !34, i64 24, i64 4, !34}
!45 = !DILocation(line: 13, column: 3, scope: !14)
!46 = !DILocation(line: 14, column: 12, scope: !14)
!47 = !DILocation(line: 15, column: 1, scope: !14)
!48 = !DILocation(line: 14, column: 3, scope: !14)
!49 = !DISubprogram(name: "use", linkageName: "_Z3use11LargeStruct", scope: !3, file: !3, line: 9, type: !50, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !4)
!50 = !DISubroutineType(types: !51)
!51 = !{!9, !19}
