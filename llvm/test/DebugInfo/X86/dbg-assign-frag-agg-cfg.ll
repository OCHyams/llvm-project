; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=false \
; RUN: | FileCheck %s
; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=true \
; RUN: | FileCheck %s

;; Check that the frag-agg pseudo-pass works on a simple CFG. When LLVM sees a
;; dbg.value with an overlapping fragment it essentially consideres the
;; previous location as valid for all bits in that fragment. The pass inserts
;; dbg.value fragments to preserve memory locations for bits in memory when
;; overlapping fragments are encountered.

;; nums lives in mem, except prior to the second call to step() where there has
;; been some DSE. At this point, the memory loc for nums.c is invalid.  But the
;; rest of num's bits, [0, 64), are in memory, so check there's a dbg.value for
;; them.

;; $ cat test.cpp
;; struct Nums { int a, b, c; };
;; Nums glob;
;; __attribute__((noinline)) void esc1(struct Nums* p) { glob = *p; }
;; __attribute__((noinline)) void esc2(struct Nums* p) { glob = *p; }
;; bool step();
;;
;; int main() {
;;   struct Nums nums = { 1, 2, 1 };
;;   if (step())
;;     esc1(&nums);
;;   else
;;     esc2(&nums);
;;
;;   nums.c = 2; //< Include some DSE to force a non-mem location.
;;   step();
;;
;;   nums.c = nums.a;
;;
;;   esc1(&nums);
;;   return 0;
;; }
;;
;; $ clang++ test.cpp -O2 -g -Xclang -debug-coffee-chat -emit-llvm -S -o -

;; Most check lines are inline in main.
; CHECK-DAG: ![[nums:[0-9]+]] = !DILocalVariable(name: "nums",

target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Nums = type { i32, i32, i32 }

@glob = dso_local local_unnamed_addr global %struct.Nums zeroinitializer, align 4, !dbg !0
@__const.main.nums = private unnamed_addr constant %struct.Nums { i32 1, i32 2, i32 1 }, align 4

; Function Attrs: mustprogress nofree noinline nosync nounwind uwtable willreturn
define dso_local void @_Z4esc1P4Nums(%struct.Nums* nocapture noundef readonly %p) local_unnamed_addr #0 !dbg !16 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !21, metadata !DIExpression(), metadata !22, metadata %struct.Nums** undef), !dbg !23
  call void @llvm.dbg.assign(metadata %struct.Nums* %p, metadata !21, metadata !DIExpression(), metadata !24, metadata %struct.Nums** undef), !dbg !23
  %0 = bitcast %struct.Nums* %p to i8*, !dbg !25
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* noundef nonnull align 4 dereferenceable(12) bitcast (%struct.Nums* @glob to i8*), i8* noundef nonnull align 4 dereferenceable(12) %0, i64 12, i1 false), !dbg !25, !tbaa.struct !26
  ret void, !dbg !31
}

; Function Attrs: argmemonly mustprogress nofree nounwind willreturn
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* noalias nocapture writeonly, i8* noalias nocapture readonly, i64, i1 immarg) #1

; Function Attrs: mustprogress nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #2

; Function Attrs: mustprogress nofree noinline nosync nounwind uwtable willreturn
define dso_local void @_Z4esc2P4Nums(%struct.Nums* nocapture noundef readonly %p) local_unnamed_addr #0 !dbg !32 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !34, metadata !DIExpression(), metadata !35, metadata %struct.Nums** undef), !dbg !36
  call void @llvm.dbg.assign(metadata %struct.Nums* %p, metadata !34, metadata !DIExpression(), metadata !37, metadata %struct.Nums** undef), !dbg !36
  %0 = bitcast %struct.Nums* %p to i8*, !dbg !38
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* noundef nonnull align 4 dereferenceable(12) bitcast (%struct.Nums* @glob to i8*), i8* noundef nonnull align 4 dereferenceable(12) %0, i64 12, i1 false), !dbg !38, !tbaa.struct !26
  ret void, !dbg !39
}

; Function Attrs: mustprogress norecurse uwtable
define dso_local noundef i32 @main() local_unnamed_addr #3 !dbg !40 {
; CHECK: name: main
entry:
  %nums = alloca %struct.Nums, align 4, !DIAssignID !45
  call void @llvm.dbg.assign(metadata i1 undef, metadata !44, metadata !DIExpression(), metadata !45, metadata %struct.Nums* %nums), !dbg !46
; CHECK: DBG_VALUE %stack.0.nums, $noreg, ![[nums]], !DIExpression(DW_OP_deref)
  %0 = bitcast %struct.Nums* %nums to i8*, !dbg !47
  call void @llvm.lifetime.start.p0i8(i64 12, i8* nonnull %0) #6, !dbg !47
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* noundef nonnull align 4 dereferenceable(12) %0, i8* noundef nonnull align 4 dereferenceable(12) bitcast (%struct.Nums* @__const.main.nums to i8*), i64 12, i1 false), !dbg !48, !DIAssignID !49
  call void @llvm.dbg.assign(metadata i1 undef, metadata !44, metadata !DIExpression(), metadata !49, metadata i8* %0), !dbg !46
  %call = tail call noundef zeroext i1 @_Z4stepv(), !dbg !50
  br i1 %call, label %if.then, label %if.else, !dbg !52

if.then:                                          ; preds = %entry
  call void @_Z4esc1P4Nums(%struct.Nums* noundef nonnull %nums), !dbg !53
  br label %if.end, !dbg !53

if.else:                                          ; preds = %entry
  call void @_Z4esc2P4Nums(%struct.Nums* noundef nonnull %nums), !dbg !54
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
; CHECK: bb.3.if.end:
  %c = getelementptr inbounds %struct.Nums, %struct.Nums* %nums, i64 0, i32 2, !dbg !55
  call void @llvm.dbg.assign(metadata i32 2, metadata !44, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 32), metadata !56, metadata i32* %c), !dbg !46
; CHECK-NEXT: DBG_VALUE 2, $noreg, !40, !DIExpression(DW_OP_LLVM_fragment, 64, 32), debug-location !41
; CHECK-NEXT: DBG_VALUE %stack.0.nums, $noreg, !40, !DIExpression(DW_OP_deref, DW_OP_LLVM_fragment, 0, 64)
  %call1 = tail call noundef zeroext i1 @_Z4stepv(), !dbg !57
  store i32 1, i32* %c, align 4, !dbg !58, !tbaa !59, !DIAssignID !61
  call void @llvm.dbg.assign(metadata i32 1, metadata !44, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 32), metadata !61, metadata i32* %c), !dbg !46
  call void @_Z4esc1P4Nums(%struct.Nums* noundef nonnull %nums), !dbg !62
  call void @llvm.lifetime.end.p0i8(i64 12, i8* nonnull %0) #6, !dbg !63
  ret i32 0, !dbg !64
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #4
declare !dbg !65 dso_local noundef zeroext i1 @_Z4stepv() local_unnamed_addr #5
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #4

!llvm.dbg.cu = !{!2}
!llvm.module.flags = !{!11, !12, !13, !14}
!llvm.ident = !{!15}

!0 = !DIGlobalVariableExpression(var: !1, expr: !DIExpression())
!1 = distinct !DIGlobalVariable(name: "glob", scope: !2, file: !3, line: 2, type: !5, isLocal: false, isDefinition: true)
!2 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !3, producer: "clang version 14.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, globals: !4, splitDebugInlining: false, nameTableKind: None)
!3 = !DIFile(filename: "test.cpp", directory: "/")
!4 = !{!0}
!5 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Nums", file: !3, line: 1, size: 96, flags: DIFlagTypePassByValue, elements: !6, identifier: "_ZTS4Nums")
!6 = !{!7, !9, !10}
!7 = !DIDerivedType(tag: DW_TAG_member, name: "a", scope: !5, file: !3, line: 1, baseType: !8, size: 32)
!8 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!9 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !5, file: !3, line: 1, baseType: !8, size: 32, offset: 32)
!10 = !DIDerivedType(tag: DW_TAG_member, name: "c", scope: !5, file: !3, line: 1, baseType: !8, size: 32, offset: 64)
!11 = !{i32 7, !"Dwarf Version", i32 5}
!12 = !{i32 2, !"Debug Info Version", i32 3}
!13 = !{i32 1, !"wchar_size", i32 4}
!14 = !{i32 7, !"uwtable", i32 1}
!15 = !{!"clang version 14.0.0"}
!16 = distinct !DISubprogram(name: "esc1", linkageName: "_Z4esc1P4Nums", scope: !3, file: !3, line: 3, type: !17, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !20)
!17 = !DISubroutineType(types: !18)
!18 = !{null, !19}
!19 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !5, size: 64)
!20 = !{!21}
!21 = !DILocalVariable(name: "p", arg: 1, scope: !16, file: !3, line: 3, type: !19)
!22 = distinct !DIAssignID()
!23 = !DILocation(line: 0, scope: !16)
!24 = distinct !DIAssignID()
!25 = !DILocation(line: 3, column: 60, scope: !16)
!26 = !{i64 0, i64 4, !27, i64 4, i64 4, !27, i64 8, i64 4, !27}
!27 = !{!28, !28, i64 0}
!28 = !{!"int", !29, i64 0}
!29 = !{!"omnipotent char", !30, i64 0}
!30 = !{!"Simple C++ TBAA"}
!31 = !DILocation(line: 3, column: 66, scope: !16)
!32 = distinct !DISubprogram(name: "esc2", linkageName: "_Z4esc2P4Nums", scope: !3, file: !3, line: 4, type: !17, scopeLine: 4, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !33)
!33 = !{!34}
!34 = !DILocalVariable(name: "p", arg: 1, scope: !32, file: !3, line: 4, type: !19)
!35 = distinct !DIAssignID()
!36 = !DILocation(line: 0, scope: !32)
!37 = distinct !DIAssignID()
!38 = !DILocation(line: 4, column: 60, scope: !32)
!39 = !DILocation(line: 4, column: 66, scope: !32)
!40 = distinct !DISubprogram(name: "main", scope: !3, file: !3, line: 7, type: !41, scopeLine: 7, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !2, retainedNodes: !43)
!41 = !DISubroutineType(types: !42)
!42 = !{!8}
!43 = !{!44}
!44 = !DILocalVariable(name: "nums", scope: !40, file: !3, line: 8, type: !5)
!45 = distinct !DIAssignID()
!46 = !DILocation(line: 0, scope: !40)
!47 = !DILocation(line: 8, column: 3, scope: !40)
!48 = !DILocation(line: 8, column: 15, scope: !40)
!49 = distinct !DIAssignID()
!50 = !DILocation(line: 9, column: 7, scope: !51)
!51 = distinct !DILexicalBlock(scope: !40, file: !3, line: 9, column: 7)
!52 = !DILocation(line: 9, column: 7, scope: !40)
!53 = !DILocation(line: 10, column: 5, scope: !51)
!54 = !DILocation(line: 12, column: 5, scope: !51)
!55 = !DILocation(line: 14, column: 8, scope: !40)
!56 = distinct !DIAssignID()
!57 = !DILocation(line: 15, column: 3, scope: !40)
!58 = !DILocation(line: 17, column: 10, scope: !40)
!59 = !{!60, !28, i64 8}
!60 = !{!"_ZTS4Nums", !28, i64 0, !28, i64 4, !28, i64 8}
!61 = distinct !DIAssignID()
!62 = !DILocation(line: 19, column: 3, scope: !40)
!63 = !DILocation(line: 21, column: 1, scope: !40)
!64 = !DILocation(line: 20, column: 3, scope: !40)
!65 = !DISubprogram(name: "step", linkageName: "_Z4stepv", scope: !3, file: !3, line: 5, type: !66, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !69)
!66 = !DISubroutineType(types: !67)
!67 = !{!68}
!68 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!69 = !{}
