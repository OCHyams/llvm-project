; RUN: opt %s -S -passes=loop-rotate | FileCheck %s

; Check the instruction(s) duplicated into the header get unique atom groups.

; CHECK-LABEL: define void @test1() !dbg ![[#]] {
; CHECK-NEXT:  entry:
; CHECK-NEXT:    [[ARRAY:%.*]] = alloca [20 x i32], align 16
; CHECK-NEXT:    br label [[FOR_BODY:%.*]], !dbg [[DBG8:![0-9]+]]
; CHECK:       for.body:
; CHECK-NEXT:    [[I_01:%.*]] = phi i32 [ 0, [[ENTRY:%.*]] ], [ [[INC:%.*]], [[FOR_BODY]] ]
; CHECK-NEXT:    store i32 0, ptr [[ARRAY]], align 16
; CHECK-NEXT:    [[INC]] = add nsw i32 [[I_01]], 1
; CHECK-NEXT:    [[CMP:%.*]] = icmp slt i32 [[INC]], 100
; CHECK-NEXT:    br i1 [[CMP]], label [[FOR_BODY]], label [[FOR_END:%.*]], !dbg [[DBG9:![0-9]+]]

; CHECK: [[DBG8]] = !DILocation(line: 5, column: 1, scope: ![[#]], atomGroup: 6, atomRank: 1)
; CHECK: [[DBG9]] = !DILocation(line: 5, column: 1, scope: ![[#]], atomGroup: 5, atomRank: 1)

target triple = "x86_64-unknown-linux"

define void @test1() !dbg !5 {
entry:
  %array = alloca [20 x i32], align 16
  br label %for.cond

for.cond:                                         ; preds = %for.body, %entry
  %i.0 = phi i32 [ 0, %entry ], [ %inc, %for.body ]
  %cmp = icmp slt i32 %i.0, 100
  br i1 %cmp, label %for.body, label %for.end, !dbg !8

for.body:                                         ; preds = %for.cond
  store i32 0, ptr %array, align 16
  %inc = add nsw i32 %i.0, 1
  br label %for.cond

for.end:                                          ; preds = %for.cond
  %arrayidx.lcssa = phi ptr [ %array, %for.cond ]
  call void @g(ptr %arrayidx.lcssa)
  ret void
}

declare void @g(ptr)

!llvm.dbg.cu = !{!0}
!llvm.debugify = !{!2, !3}
!llvm.module.flags = !{!4}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "debugify", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "test.ll", directory: "/")
!2 = !{i32 11}
!3 = !{i32 0}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = distinct !DISubprogram(name: "test1", linkageName: "test1", scope: null, file: !1, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{}
!8 = !DILocation(line: 5, column: 1, scope: !5, atomGroup: 5, atomRank: 1)
