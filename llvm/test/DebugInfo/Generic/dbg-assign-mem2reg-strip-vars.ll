; RUN: opt %s -S -mem2reg -mem2reg-strip-vars -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg."

;; Check that -mem2reg-strip-vars strips debug info for promoted vars.

;; Generated from this source, with some intrinsics added manually:
;; $ cat test.c
;; int fun(int a, int b) {
;;   int c = a + b;
;;   if (c == 0)
;;     c +=1;
;;   else
;;     c -=75;
;;   return c;
;; }
;; $ clang -Xclang -debug-coffee-chat -Xclang -disable-llvm-passes -O2 -g -emit-llvm -S -o test.ll test.c

; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: nounwind uwtable
define dso_local i32 @fun(i32 %a, i32 %b) !dbg !7 {
entry:
  %a.addr = alloca i32, align 4, !DIAssignID !15
  ;; ADDED THIS IN MANUALLY:
  call void @llvm.dbg.declare(metadata i32* %a.addr, metadata !12, metadata !DIExpression()), !dbg !16
  call void @llvm.dbg.assign(metadata i1 undef, metadata !12, metadata !DIExpression(), metadata !15, metadata i32* %a.addr), !dbg !16
  %b.addr = alloca i32, align 4, !DIAssignID !17
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !17, metadata i32* %b.addr), !dbg !16
  %c = alloca i32, align 4, !DIAssignID !18
  call void @llvm.dbg.assign(metadata i1 undef, metadata !14, metadata !DIExpression(), metadata !18, metadata i32* %c), !dbg !16
  store i32 %a, i32* %a.addr, align 4, !tbaa !19, !DIAssignID !23
  call void @llvm.dbg.assign(metadata i32 %a, metadata !12, metadata !DIExpression(), metadata !23, metadata i32* %a.addr), !dbg !16
  store i32 %b, i32* %b.addr, align 4, !tbaa !19, !DIAssignID !24
  call void @llvm.dbg.assign(metadata i32 %b, metadata !13, metadata !DIExpression(), metadata !24, metadata i32* %b.addr), !dbg !16
  %0 = bitcast i32* %c to i8*, !dbg !25
  call void @llvm.lifetime.start.p0i8(i64 4, i8* %0), !dbg !25
  %1 = load i32, i32* %a.addr, align 4, !dbg !26, !tbaa !19
  %2 = load i32, i32* %b.addr, align 4, !dbg !27, !tbaa !19
  %add = add nsw i32 %1, %2, !dbg !28
  store i32 %add, i32* %c, align 4, !dbg !29, !tbaa !19, !DIAssignID !30
  call void @llvm.dbg.assign(metadata i32 %add, metadata !14, metadata !DIExpression(), metadata !30, metadata i32* %c), !dbg !29
  ;; ADDED THIS IN MANUALLY:
  call void @llvm.dbg.value(metadata i32 %add, metadata !12, metadata !DIExpression()), !dbg !16
  %3 = load i32, i32* %c, align 4, !dbg !31, !tbaa !19
  %cmp = icmp eq i32 %3, 0, !dbg !33
  br i1 %cmp, label %if.then, label %if.else, !dbg !34

if.then:                                          ; preds = %entry
  %4 = load i32, i32* %c, align 4, !dbg !35, !tbaa !19
  %add1 = add nsw i32 %4, 1, !dbg !35
  store i32 %add1, i32* %c, align 4, !dbg !35, !tbaa !19, !DIAssignID !36
  call void @llvm.dbg.assign(metadata i32 %add1, metadata !14, metadata !DIExpression(), metadata !36, metadata i32* %c), !dbg !35
  br label %if.end, !dbg !37

if.else:                                          ; preds = %entry
  %5 = load i32, i32* %c, align 4, !dbg !38, !tbaa !19
  %sub = sub nsw i32 %5, 75, !dbg !38
  store i32 %sub, i32* %c, align 4, !dbg !38, !tbaa !19, !DIAssignID !39
  call void @llvm.dbg.assign(metadata i32 %sub, metadata !14, metadata !DIExpression(), metadata !39, metadata i32* %c), !dbg !38
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %6 = load i32, i32* %c, align 4, !dbg !40, !tbaa !19
  %7 = bitcast i32* %c to i8*, !dbg !41
  call void @llvm.lifetime.end.p0i8(i64 4, i8* %7), !dbg !41
  ret i32 %6, !dbg !42
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)
declare void @llvm.dbg.declare(metadata, metadata, metadata)
declare void @llvm.dbg.value(metadata, metadata, metadata)
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
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "fun", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10, !10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12, !13, !14}
!12 = !DILocalVariable(name: "a", arg: 1, scope: !7, file: !1, line: 1, type: !10)
!13 = !DILocalVariable(name: "b", arg: 2, scope: !7, file: !1, line: 1, type: !10)
!14 = !DILocalVariable(name: "c", scope: !7, file: !1, line: 2, type: !10)
!15 = distinct !DIAssignID()
!16 = !DILocation(line: 0, scope: !7)
!17 = distinct !DIAssignID()
!18 = distinct !DIAssignID()
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !21, i64 0}
!21 = !{!"omnipotent char", !22, i64 0}
!22 = !{!"Simple C/C++ TBAA"}
!23 = distinct !DIAssignID()
!24 = distinct !DIAssignID()
!25 = !DILocation(line: 2, column: 3, scope: !7)
!26 = !DILocation(line: 2, column: 11, scope: !7)
!27 = !DILocation(line: 2, column: 15, scope: !7)
!28 = !DILocation(line: 2, column: 13, scope: !7)
!29 = !DILocation(line: 2, column: 7, scope: !7)
!30 = distinct !DIAssignID()
!31 = !DILocation(line: 3, column: 7, scope: !32)
!32 = distinct !DILexicalBlock(scope: !7, file: !1, line: 3, column: 7)
!33 = !DILocation(line: 3, column: 9, scope: !32)
!34 = !DILocation(line: 3, column: 7, scope: !7)
!35 = !DILocation(line: 4, column: 7, scope: !32)
!36 = distinct !DIAssignID()
!37 = !DILocation(line: 4, column: 5, scope: !32)
!38 = !DILocation(line: 6, column: 7, scope: !32)
!39 = distinct !DIAssignID()
!40 = !DILocation(line: 7, column: 10, scope: !7)
!41 = !DILocation(line: 8, column: 1, scope: !7)
!42 = !DILocation(line: 7, column: 3, scope: !7)
