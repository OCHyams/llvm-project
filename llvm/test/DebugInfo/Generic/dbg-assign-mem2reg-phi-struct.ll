; RUN: opt -S %s -sroa -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg."

;; Check that mem2reg inserts dbg.value for PHIs when promoting allocas for
;; variables described by dbg.assign intrinsics. The --implicit-check-not
;; switch there to check that mem2reg only inserts the dbg.values for PHIs
;; (i.e. not also for stores like it would outside of the prototype).
;;
;; SROA is going to break up LargeStruct. Ensure that we still insert a
;; dbg.value for S.Var.
;;
;; $ cat test.cpp
;; void do_something();
;; struct LargeStruct {
;;   int A, B, C;
;;   int Var;
;;   int D, E, F;
;; };
;; int example(int In, bool Cond) {
;;   LargeStruct S = {0};
;;   S.Var = In;
;;   if (Cond) {
;;     do_something();
;;     S.Var = 0;
;;   }
;;   return S.Var;
;; }
;; $ clang++ -c -O2 -g test.cpp  -Xclang -debug-coffee-chat -emit-llvm -S -o - -Xclang -disable-llvm-passes

; CHECK: entry:
;; The parameter allocas have been promoted (no allocas).
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR_In:[0-9]+]], metadata !DIExpression(), metadata !25, metadata i32* undef), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR_Cond:[0-9]+]], metadata !DIExpression(), metadata !27, metadata i8* undef), !dbg

;; The alloca for S has been broken up into three slices.
;; Slice [0, 96):
; CHECK-NEXT: %S.sroa.0 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_1:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR_S:[0-9]+]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_1]], metadata { i32, i32, i32 }* %S.sroa.0), !dbg
;;
;; Slice [96, 128) has been fully promoted (no alloca remains):
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg
;;
;; Slice [128, 224):
; CHECK-NEXT: %S.sroa.6 = alloca { i32, i32, i32 }, align 8, !DIAssignID ![[ID_3:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_3]], metadata { i32, i32, i32 }* %S.sroa.6), !dbg

;; These dbg.assigns remain from the parameter store-to-allocas,
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %In, metadata ![[VAR_In]], metadata !DIExpression(), metadata !{{.+}}, metadata i32* undef), !dbg
; CHECK: call void @llvm.dbg.assign(metadata i8 %{{.+}}, metadata ![[VAR_Cond]], metadata !DIExpression(), metadata !{{.+}}, metadata i8* undef), !dbg

;; The memset for `S = {0}` is split into 3 slices.
; CHECK: call void @llvm.memset.p0i8.i64(i8* align 8 %S.sroa.0.0..sroa_cast13, i8 0, i64 12, i1 false),{{.*}}!DIAssignID ![[ID_4:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 0, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 0, 96), metadata ![[ID_4]], metadata i8* %S.sroa.0.0..sroa_cast13), !dbg
;;
;; This slice has been fully promoted and we know the value is 0 here.
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg
;;
; CHECK: call void @llvm.memset.p0i8.i64(i8* align 8 %S.sroa.6.0..sroa_cast8, i8 0, i64 12, i1 false),{{.*}}!DIAssignID ![[ID_6:[0-9]+]]
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 0, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 128, 96), metadata ![[ID_6]], metadata i8* %S.sroa.6.0..sroa_cast8), !dbg

;; S.Var = In directly uses the parameter value:
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %In, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.*}}, metadata i32* undef), !dbg

; CHECK: if.then:
; CHECK-NEXT: call void @_Z12do_somethingv(), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !{{.+}}, metadata i32* undef), !dbg

;; Finally, ensure there is a dbg.value inserted for the merged value.
; CHECK: if.end:
; CHECK-NEXT: %S.sroa.3.0 = phi i32 [ 0, %if.then ], [ %In, %entry ], !dbg
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %S.sroa.3.0, metadata ![[VAR_S]], metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32)), !dbg

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"

%struct.LargeStruct = type { i32, i32, i32, i32, i32, i32, i32 }

define dso_local i32 @_Z7exampleib(i32 %In, i1 zeroext %Cond) !dbg !7 {
entry:
  %In.addr = alloca i32, align 4, !DIAssignID !25
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !25, metadata i32* %In.addr), !dbg !26
  %Cond.addr = alloca i8, align 1, !DIAssignID !27
  call void @llvm.dbg.assign(metadata i1 undef, metadata !14, metadata !DIExpression(), metadata !27, metadata i8* %Cond.addr), !dbg !26
  %S = alloca %struct.LargeStruct, align 4, !DIAssignID !28
  call void @llvm.dbg.assign(metadata i1 undef, metadata !15, metadata !DIExpression(), metadata !28, metadata %struct.LargeStruct* %S), !dbg !26
  store i32 %In, i32* %In.addr, align 4, !tbaa !29, !DIAssignID !33
  call void @llvm.dbg.assign(metadata i32 %In, metadata !13, metadata !DIExpression(), metadata !33, metadata i32* %In.addr), !dbg !26
  %frombool = zext i1 %Cond to i8
  store i8 %frombool, i8* %Cond.addr, align 1, !tbaa !34, !DIAssignID !36
  call void @llvm.dbg.assign(metadata i8 %frombool, metadata !14, metadata !DIExpression(), metadata !36, metadata i8* %Cond.addr), !dbg !26
  %0 = bitcast %struct.LargeStruct* %S to i8*, !dbg !37
  call void @llvm.lifetime.start.p0i8(i64 28, i8* %0), !dbg !37
  %1 = bitcast %struct.LargeStruct* %S to i8*, !dbg !38
  call void @llvm.memset.p0i8.i64(i8* align 4 %1, i8 0, i64 28, i1 false), !dbg !38, !DIAssignID !39
  call void @llvm.dbg.assign(metadata i8 0, metadata !15, metadata !DIExpression(), metadata !39, metadata i8* %1), !dbg !38
  %2 = load i32, i32* %In.addr, align 4, !dbg !40, !tbaa !29
  %Var = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %S, i32 0, i32 3, !dbg !41
  store i32 %2, i32* %Var, align 4, !dbg !42, !tbaa !43, !DIAssignID !45
  call void @llvm.dbg.assign(metadata i32 %2, metadata !15, metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !45, metadata i32* %Var), !dbg !42
  %3 = load i8, i8* %Cond.addr, align 1, !dbg !46, !tbaa !34, !range !48
  %tobool = trunc i8 %3 to i1, !dbg !46
  br i1 %tobool, label %if.then, label %if.end, !dbg !49

if.then:                                          ; preds = %entry
  call void @_Z12do_somethingv(), !dbg !50
  %Var1 = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %S, i32 0, i32 3, !dbg !52
  store i32 0, i32* %Var1, align 4, !dbg !53, !tbaa !43, !DIAssignID !54
  call void @llvm.dbg.assign(metadata i32 0, metadata !15, metadata !DIExpression(DW_OP_LLVM_fragment, 96, 32), metadata !54, metadata i32* %Var1), !dbg !53
  br label %if.end, !dbg !55

if.end:                                           ; preds = %if.then, %entry
  %Var2 = getelementptr inbounds %struct.LargeStruct, %struct.LargeStruct* %S, i32 0, i32 3, !dbg !56
  %4 = load i32, i32* %Var2, align 4, !dbg !56, !tbaa !43
  %5 = bitcast %struct.LargeStruct* %S to i8*, !dbg !57
  call void @llvm.lifetime.end.p0i8(i64 28, i8* %5), !dbg !57
  ret i32 %4, !dbg !58
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg)
declare !dbg !59 dso_local void @_Z12do_somethingv()
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "example", linkageName: "_Z7exampleib", scope: !1, file: !1, line: 7, type: !8, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !12)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10, !11}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!12 = !{!13, !14, !15}
!13 = !DILocalVariable(name: "In", arg: 1, scope: !7, file: !1, line: 7, type: !10)
!14 = !DILocalVariable(name: "Cond", arg: 2, scope: !7, file: !1, line: 7, type: !11)
!15 = !DILocalVariable(name: "S", scope: !7, file: !1, line: 8, type: !16)
!16 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "LargeStruct", file: !1, line: 2, size: 224, flags: DIFlagTypePassByValue, elements: !17, identifier: "_ZTS11LargeStruct")
!17 = !{!18, !19, !20, !21, !22, !23, !24}
!18 = !DIDerivedType(tag: DW_TAG_member, name: "A", scope: !16, file: !1, line: 3, baseType: !10, size: 32)
!19 = !DIDerivedType(tag: DW_TAG_member, name: "B", scope: !16, file: !1, line: 3, baseType: !10, size: 32, offset: 32)
!20 = !DIDerivedType(tag: DW_TAG_member, name: "C", scope: !16, file: !1, line: 3, baseType: !10, size: 32, offset: 64)
!21 = !DIDerivedType(tag: DW_TAG_member, name: "Var", scope: !16, file: !1, line: 4, baseType: !10, size: 32, offset: 96)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "D", scope: !16, file: !1, line: 5, baseType: !10, size: 32, offset: 128)
!23 = !DIDerivedType(tag: DW_TAG_member, name: "E", scope: !16, file: !1, line: 5, baseType: !10, size: 32, offset: 160)
!24 = !DIDerivedType(tag: DW_TAG_member, name: "F", scope: !16, file: !1, line: 5, baseType: !10, size: 32, offset: 192)
!25 = distinct !DIAssignID()
!26 = !DILocation(line: 0, scope: !7)
!27 = distinct !DIAssignID()
!28 = distinct !DIAssignID()
!29 = !{!30, !30, i64 0}
!30 = !{!"int", !31, i64 0}
!31 = !{!"omnipotent char", !32, i64 0}
!32 = !{!"Simple C++ TBAA"}
!33 = distinct !DIAssignID()
!34 = !{!35, !35, i64 0}
!35 = !{!"bool", !31, i64 0}
!36 = distinct !DIAssignID()
!37 = !DILocation(line: 8, column: 3, scope: !7)
!38 = !DILocation(line: 8, column: 15, scope: !7)
!39 = distinct !DIAssignID()
!40 = !DILocation(line: 9, column: 11, scope: !7)
!41 = !DILocation(line: 9, column: 5, scope: !7)
!42 = !DILocation(line: 9, column: 9, scope: !7)
!43 = !{!44, !30, i64 12}
!44 = !{!"_ZTS11LargeStruct", !30, i64 0, !30, i64 4, !30, i64 8, !30, i64 12, !30, i64 16, !30, i64 20, !30, i64 24}
!45 = distinct !DIAssignID()
!46 = !DILocation(line: 10, column: 7, scope: !47)
!47 = distinct !DILexicalBlock(scope: !7, file: !1, line: 10, column: 7)
!48 = !{i8 0, i8 2}
!49 = !DILocation(line: 10, column: 7, scope: !7)
!50 = !DILocation(line: 11, column: 5, scope: !51)
!51 = distinct !DILexicalBlock(scope: !47, file: !1, line: 10, column: 13)
!52 = !DILocation(line: 12, column: 7, scope: !51)
!53 = !DILocation(line: 12, column: 11, scope: !51)
!54 = distinct !DIAssignID()
!55 = !DILocation(line: 13, column: 3, scope: !51)
!56 = !DILocation(line: 14, column: 12, scope: !7)
!57 = !DILocation(line: 15, column: 1, scope: !7)
!58 = !DILocation(line: 14, column: 3, scope: !7)
!59 = !DISubprogram(name: "do_something", linkageName: "_Z12do_somethingv", scope: !1, file: !1, line: 1, type: !60, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!60 = !DISubroutineType(types: !61)
!61 = !{null}
