; RUN: opt -S %s -sroa -o - | FileCheck %s --implicit-check-not="call void @llvm.dbg."

;; Check that mem2reg inserts dbg.value for PHIs when promoting allocas for
;; variables described by dbg.assign intrinsics. The --implicit-check-not
;; switch there to check that mem2reg only inserts the dbg.values for PHIs
;; (i.e. not also for stores like it would outside of the prototype).
;;
;; $ cat test.cpp
;; void do_something();
;; int example(int in, bool cond) {
;;   if (cond) {
;;     do_something();
;;     in = 0;
;;   }
;;   return in;
;; }
;; $ clang++ -c -O2 -g test.cpp  -Xclang -debug-coffee-chat -emit-llvm -S -o - -Xclang -disable-llvm-passes

;; The parameter allocas have been promoted away so we have two undefs at the start.
; CHECK: entry:
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[IN:[0-9]+]], metadata !DIExpression(), metadata !{{.+}}, metadata i32* undef), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i1 undef, metadata ![[COND:[0-9]+]], metadata !DIExpression(), metadata !{{.+}}, metadata i8* undef), !dbg

;; Here are the parameter value locations.
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 %in, metadata ![[IN]], metadata !DIExpression(), metadata !{{.+}}, metadata i32* undef), !dbg
; CHECK-NEXT: %frombool = zext i1 %cond to i8
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i8 %frombool, metadata ![[COND]], metadata !DIExpression(), metadata !{{.+}}, metadata i8* undef), !dbg

; CHECK: if.then:
; CHECK-NEXT: call void @_Z12do_somethingv(), !dbg
; CHECK-NEXT: call void @llvm.dbg.assign(metadata i32 0, metadata ![[IN]], metadata !DIExpression(), metadata !{{.+}}, metadata i32* undef), !dbg

; CHECK: if.end:
; CHECK-NEXT: %in.addr.0 = phi i32 [ 0, %if.then ], [ %in, %entry ]
; CHECK-NEXT: call void @llvm.dbg.value(metadata i32 %in.addr.0, metadata ![[IN]], metadata !DIExpression()), !dbg

source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: uwtable mustprogress
define dso_local i32 @_Z7exampleib(i32 %in, i1 zeroext %cond) #0 !dbg !7 {
entry:
  %in.addr = alloca i32, align 4, !DIAssignID !15
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !15, metadata i32* %in.addr), !dbg !16
  %cond.addr = alloca i8, align 1, !DIAssignID !17
  call void @llvm.dbg.assign(metadata i1 undef, metadata !14, metadata !DIExpression(), metadata !17, metadata i8* %cond.addr), !dbg !16
  store i32 %in, i32* %in.addr, align 4, !tbaa !18, !DIAssignID !22
  call void @llvm.dbg.assign(metadata i32 %in, metadata !13, metadata !DIExpression(), metadata !22, metadata i32* %in.addr), !dbg !16
  %frombool = zext i1 %cond to i8
  store i8 %frombool, i8* %cond.addr, align 1, !tbaa !23, !DIAssignID !25
  call void @llvm.dbg.assign(metadata i8 %frombool, metadata !14, metadata !DIExpression(), metadata !25, metadata i8* %cond.addr), !dbg !16
  %0 = load i8, i8* %cond.addr, align 1, !dbg !26, !tbaa !23, !range !28
  %tobool = trunc i8 %0 to i1, !dbg !26
  br i1 %tobool, label %if.then, label %if.end, !dbg !29

if.then:                                          ; preds = %entry
  call void @_Z12do_somethingv(), !dbg !30
  store i32 0, i32* %in.addr, align 4, !dbg !32, !tbaa !18, !DIAssignID !33
  call void @llvm.dbg.assign(metadata i32 0, metadata !13, metadata !DIExpression(), metadata !33, metadata i32* %in.addr), !dbg !32
  br label %if.end, !dbg !34

if.end:                                           ; preds = %if.then, %entry
  %1 = load i32, i32* %in.addr, align 4, !dbg !35, !tbaa !18
  ret i32 %1, !dbg !36
}

declare !dbg !37 dso_local void @_Z12do_somethingv() #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #2

attributes #0 = { uwtable mustprogress "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nofree nosync nounwind readnone speculatable willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git de230fab85ca817d3a4d69cf7cd27bff8c99beea)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/home/och/dev/bugs/scratch")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git de230fab85ca817d3a4d69cf7cd27bff8c99beea)"}
!7 = distinct !DISubprogram(name: "example", linkageName: "_Z7exampleib", scope: !1, file: !1, line: 2, type: !8, scopeLine: 2, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !12)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !10, !11}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!12 = !{!13, !14}
!13 = !DILocalVariable(name: "in", arg: 1, scope: !7, file: !1, line: 2, type: !10)
!14 = !DILocalVariable(name: "cond", arg: 2, scope: !7, file: !1, line: 2, type: !11)
!15 = distinct !DIAssignID()
!16 = !DILocation(line: 0, scope: !7)
!17 = distinct !DIAssignID()
!18 = !{!19, !19, i64 0}
!19 = !{!"int", !20, i64 0}
!20 = !{!"omnipotent char", !21, i64 0}
!21 = !{!"Simple C++ TBAA"}
!22 = distinct !DIAssignID()
!23 = !{!24, !24, i64 0}
!24 = !{!"bool", !20, i64 0}
!25 = distinct !DIAssignID()
!26 = !DILocation(line: 3, column: 7, scope: !27)
!27 = distinct !DILexicalBlock(scope: !7, file: !1, line: 3, column: 7)
!28 = !{i8 0, i8 2}
!29 = !DILocation(line: 3, column: 7, scope: !7)
!30 = !DILocation(line: 4, column: 5, scope: !31)
!31 = distinct !DILexicalBlock(scope: !27, file: !1, line: 3, column: 13)
!32 = !DILocation(line: 5, column: 8, scope: !31)
!33 = distinct !DIAssignID()
!34 = !DILocation(line: 6, column: 3, scope: !31)
!35 = !DILocation(line: 7, column: 10, scope: !7)
!36 = !DILocation(line: 7, column: 3, scope: !7)
!37 = !DISubprogram(name: "do_something", linkageName: "_Z12do_somethingv", scope: !1, file: !1, line: 1, type: !38, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!38 = !DISubroutineType(types: !39)
!39 = !{null}
