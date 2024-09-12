; ModuleID = '<stdin>'
source_filename = "<stdin>"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

@simple.targets = constant [2 x ptr] [ptr blockaddress(@simple, %bb0), ptr blockaddress(@simple, %bb1)], align 16
@multi.targets = constant [2 x ptr] [ptr blockaddress(@multi, %bb0), ptr blockaddress(@multi, %bb1)], align 16
@loop.targets = constant [2 x ptr] [ptr blockaddress(@loop, %bb0), ptr blockaddress(@loop, %bb1)], align 16
@nophi.targets = constant [2 x ptr] [ptr blockaddress(@nophi, %bb0), ptr blockaddress(@nophi, %bb1)], align 16
@noncritical.targets = constant [2 x ptr] [ptr blockaddress(@noncritical, %bb0), ptr blockaddress(@noncritical, %bb1)], align 16

declare void @use(i32) local_unnamed_addr

declare void @useptr(ptr) local_unnamed_addr

define void @simple(ptr nocapture readonly %p) !dbg !5 {
entry:
  %incdec.ptr = getelementptr inbounds i32, ptr %p, i64 1, !dbg !DILocation(line: 1, column: 1, scope: !5, atomGroup: 1, atomRank: 1)
  %initval = load i32, ptr %p, align 4, !dbg !DILocation(line: 2, column: 1, scope: !5, atomGroup: 2, atomRank: 1)
  %initop = load i32, ptr %incdec.ptr, align 4, !dbg !DILocation(line: 3, column: 1, scope: !5, atomGroup: 3, atomRank: 1)
  switch i32 %initop, label %exit [
    i32 0, label %bb0.clone
    i32 1, label %bb1.clone
  ], !dbg !DILocation(line: 4, column: 1, scope: !5, atomGroup: 4, atomRank: 1)

bb0:                                              ; preds = %indirectgoto
  br label %.split, !dbg !DILocation(line: 7, column: 1, scope: !5, atomGroup: 7, atomRank: 1)

.split:                                           ; preds = %bb0.clone, %bb0
  %merge = phi ptr [ %ptr, %bb0 ], [ %incdec.ptr, %bb0.clone ]
  %merge2 = phi i32 [ 0, %bb0 ], [ %initval, %bb0.clone ]
  tail call void @use(i32 %merge2), !dbg !DILocation(line: 7, column: 1, scope: !5, atomGroup: 7, atomRank: 1)
  br label %indirectgoto, !dbg !DILocation(line: 8, column: 1, scope: !5, atomGroup: 8, atomRank: 1)

bb1:                                              ; preds = %indirectgoto
  br label %.split3, !dbg !DILocation(line: 11, column: 1, scope: !5, atomGroup: 11, atomRank: 1)

.split3:                                          ; preds = %bb1.clone, %bb1
  %merge5 = phi ptr [ %ptr, %bb1 ], [ %incdec.ptr, %bb1.clone ]
  %merge7 = phi i32 [ 1, %bb1 ], [ %initval, %bb1.clone ]
  tail call void @use(i32 %merge7), !dbg !DILocation(line: 11, column: 1, scope: !5, atomGroup: 11, atomRank: 1)
  br label %indirectgoto, !dbg !DILocation(line: 12, column: 1, scope: !5, atomGroup: 12, atomRank: 1)

indirectgoto:                                     ; preds = %.split3, %.split
  %p.addr.sink = phi ptr [ %merge5, %.split3 ], [ %merge, %.split ], !dbg !DILocation(line: 13, column: 1, scope: !5, atomGroup: 13, atomRank: 1)
  %ptr = getelementptr inbounds i32, ptr %p.addr.sink, i64 1, !dbg !DILocation(line: 14, column: 1, scope: !5, atomGroup: 14, atomRank: 1)
  %newp = load i32, ptr %p.addr.sink, align 4, !dbg !DILocation(line: 15, column: 1, scope: !5, atomGroup: 15, atomRank: 1)
  %idx = sext i32 %newp to i64, !dbg !DILocation(line: 16, column: 1, scope: !5, atomGroup: 16, atomRank: 1)
  %arrayidx = getelementptr inbounds [2 x ptr], ptr @simple.targets, i64 0, i64 %idx, !dbg !DILocation(line: 17, column: 1, scope: !5, atomGroup: 17, atomRank: 1)
  %newop = load ptr, ptr %arrayidx, align 8, !dbg !DILocation(line: 18, column: 1, scope: !5, atomGroup: 18, atomRank: 1)
  indirectbr ptr %newop, [label %bb0, label %bb1], !dbg !DILocation(line: 19, column: 1, scope: !5, atomGroup: 19, atomRank: 1)

exit:                                             ; preds = %entry
  ret void, !dbg !DILocation(line: 20, column: 1, scope: !5, atomGroup: 20, atomRank: 1)

bb0.clone:                                        ; preds = %entry
  br label %.split, !dbg !DILocation(line: 7, column: 1, scope: !5, atomGroup: 81, atomRank: 1)

bb1.clone:                                        ; preds = %entry
  br label %.split3, !dbg !DILocation(line: 11, column: 1, scope: !5, atomGroup: 84, atomRank: 1)
}

define void @multi(ptr nocapture readonly %p) !dbg !26 {
entry:
  %incdec.ptr = getelementptr inbounds i32, ptr %p, i64 1, !dbg !DILocation(line: 21, column: 1, scope: !26, atomGroup: 21, atomRank: 1)
  %initval = load i32, ptr %p, align 4, !dbg !DILocation(line: 22, column: 1, scope: !26, atomGroup: 22, atomRank: 1)
  %initop = load i32, ptr %incdec.ptr, align 4, !dbg !DILocation(line: 23, column: 1, scope: !26, atomGroup: 23, atomRank: 1)
  switch i32 %initop, label %exit [
    i32 0, label %bb0
    i32 1, label %bb1
  ], !dbg !DILocation(line: 24, column: 1, scope: !26, atomGroup: 24, atomRank: 1)

bb0:                                              ; preds = %bb1, %bb0, %entry
  %p.addr.0 = phi ptr [ %incdec.ptr, %entry ], [ %next0, %bb0 ], [ %next1, %bb1 ], !dbg !DILocation(line: 25, column: 1, scope: !26, atomGroup: 25, atomRank: 1)
  %opcode.0 = phi i32 [ %initval, %entry ], [ 0, %bb0 ], [ 1, %bb1 ], !dbg !DILocation(line: 26, column: 1, scope: !26, atomGroup: 26, atomRank: 1)
  tail call void @use(i32 %opcode.0), !dbg !DILocation(line: 27, column: 1, scope: !26, atomGroup: 27, atomRank: 1)
  %next0 = getelementptr inbounds i32, ptr %p.addr.0, i64 1, !dbg !DILocation(line: 28, column: 1, scope: !26, atomGroup: 28, atomRank: 1)
  %newp0 = load i32, ptr %p.addr.0, align 4, !dbg !DILocation(line: 29, column: 1, scope: !26, atomGroup: 29, atomRank: 1)
  %idx0 = sext i32 %newp0 to i64, !dbg !DILocation(line: 30, column: 1, scope: !26, atomGroup: 30, atomRank: 1)
  %arrayidx0 = getelementptr inbounds [2 x ptr], ptr @multi.targets, i64 0, i64 %idx0, !dbg !DILocation(line: 31, column: 1, scope: !26, atomGroup: 31, atomRank: 1)
  %newop0 = load ptr, ptr %arrayidx0, align 8, !dbg !DILocation(line: 32, column: 1, scope: !26, atomGroup: 32, atomRank: 1)
  indirectbr ptr %newop0, [label %bb0, label %bb1], !dbg !DILocation(line: 33, column: 1, scope: !26, atomGroup: 33, atomRank: 1)

bb1:                                              ; preds = %bb1, %bb0, %entry
  %p.addr.1 = phi ptr [ %incdec.ptr, %entry ], [ %next0, %bb0 ], [ %next1, %bb1 ], !dbg !DILocation(line: 34, column: 1, scope: !26, atomGroup: 34, atomRank: 1)
  %opcode.1 = phi i32 [ %initval, %entry ], [ 0, %bb0 ], [ 1, %bb1 ], !dbg !DILocation(line: 35, column: 1, scope: !26, atomGroup: 35, atomRank: 1)
  tail call void @use(i32 %opcode.1), !dbg !DILocation(line: 36, column: 1, scope: !26, atomGroup: 36, atomRank: 1)
  %next1 = getelementptr inbounds i32, ptr %p.addr.1, i64 1, !dbg !DILocation(line: 37, column: 1, scope: !26, atomGroup: 37, atomRank: 1)
  %newp1 = load i32, ptr %p.addr.1, align 4, !dbg !DILocation(line: 38, column: 1, scope: !26, atomGroup: 38, atomRank: 1)
  %idx1 = sext i32 %newp1 to i64, !dbg !DILocation(line: 39, column: 1, scope: !26, atomGroup: 39, atomRank: 1)
  %arrayidx1 = getelementptr inbounds [2 x ptr], ptr @multi.targets, i64 0, i64 %idx1, !dbg !DILocation(line: 40, column: 1, scope: !26, atomGroup: 40, atomRank: 1)
  %newop1 = load ptr, ptr %arrayidx1, align 8, !dbg !DILocation(line: 41, column: 1, scope: !26, atomGroup: 41, atomRank: 1)
  indirectbr ptr %newop1, [label %bb0, label %bb1], !dbg !DILocation(line: 42, column: 1, scope: !26, atomGroup: 42, atomRank: 1)

exit:                                             ; preds = %entry
  ret void, !dbg !DILocation(line: 43, column: 1, scope: !26, atomGroup: 43, atomRank: 1)
}

define i64 @loop(ptr nocapture readonly %p) !dbg !50 {
entry:
  br label %.split, !dbg !DILocation(line: 46, column: 1, scope: !50, atomGroup: 86, atomRank: 1)

bb0:                                              ; preds = %.split
  br label %.split, !dbg !DILocation(line: 46, column: 1, scope: !50, atomGroup: 46, atomRank: 1)

.split:                                           ; preds = %entry, %bb0
  %merge = phi i64 [ %i.next, %bb0 ], [ 0, %entry ]
  %tmp0 = getelementptr inbounds i64, ptr %p, i64 %merge, !dbg !DILocation(line: 46, column: 1, scope: !50, atomGroup: 46, atomRank: 1)
  store i64 %merge, ptr %tmp0, align 4, !dbg !DILocation(line: 47, column: 1, scope: !50, atomGroup: 47, atomRank: 1)
  %i.next = add nuw nsw i64 %merge, 1, !dbg !DILocation(line: 48, column: 1, scope: !50, atomGroup: 48, atomRank: 1)
  %idx = srem i64 %merge, 2, !dbg !DILocation(line: 49, column: 1, scope: !50, atomGroup: 49, atomRank: 1)
  %arrayidx = getelementptr inbounds [2 x ptr], ptr @loop.targets, i64 0, i64 %idx, !dbg !DILocation(line: 50, column: 1, scope: !50, atomGroup: 50, atomRank: 1)
  %target = load ptr, ptr %arrayidx, align 8, !dbg !DILocation(line: 51, column: 1, scope: !50, atomGroup: 51, atomRank: 1)
  indirectbr ptr %target, [label %bb0, label %bb1], !dbg !DILocation(line: 52, column: 1, scope: !50, atomGroup: 52, atomRank: 1)

bb1:                                              ; preds = %.split
  ret i64 %i.next, !dbg !DILocation(line: 53, column: 1, scope: !50, atomGroup: 53, atomRank: 1)
}

define void @nophi(ptr %p) !dbg !60 {
entry:
  %incdec.ptr = getelementptr inbounds i32, ptr %p, i64 1, !dbg !DILocation(line: 54, column: 1, scope: !60, atomGroup: 54, atomRank: 1)
  %initop = load i32, ptr %incdec.ptr, align 4, !dbg !DILocation(line: 55, column: 1, scope: !60, atomGroup: 55, atomRank: 1)
  switch i32 %initop, label %exit [
    i32 0, label %bb0
    i32 1, label %bb1
  ], !dbg !DILocation(line: 56, column: 1, scope: !60, atomGroup: 56, atomRank: 1)

bb0:                                              ; preds = %indirectgoto, %entry
  tail call void @use(i32 0), !dbg !DILocation(line: 57, column: 1, scope: !60, atomGroup: 57, atomRank: 1)
  br label %indirectgoto, !dbg !DILocation(line: 58, column: 1, scope: !60, atomGroup: 58, atomRank: 1)

bb1:                                              ; preds = %indirectgoto, %entry
  tail call void @use(i32 1), !dbg !DILocation(line: 59, column: 1, scope: !60, atomGroup: 59, atomRank: 1)
  br label %indirectgoto, !dbg !DILocation(line: 60, column: 1, scope: !60, atomGroup: 60, atomRank: 1)

indirectgoto:                                     ; preds = %bb1, %bb0
  %sunkaddr = getelementptr inbounds i8, ptr %p, i64 4, !dbg !DILocation(line: 61, column: 1, scope: !60, atomGroup: 61, atomRank: 1)
  %newp = load i32, ptr %sunkaddr, align 4, !dbg !DILocation(line: 61, column: 1, scope: !60, atomGroup: 61, atomRank: 1)
  %idx = sext i32 %newp to i64, !dbg !DILocation(line: 62, column: 1, scope: !60, atomGroup: 62, atomRank: 1)
  %arrayidx = getelementptr inbounds [2 x ptr], ptr @nophi.targets, i64 0, i64 %idx, !dbg !DILocation(line: 63, column: 1, scope: !60, atomGroup: 63, atomRank: 1)
  %newop = load ptr, ptr %arrayidx, align 8, !dbg !DILocation(line: 64, column: 1, scope: !60, atomGroup: 64, atomRank: 1)
  indirectbr ptr %newop, [label %bb0, label %bb1], !dbg !DILocation(line: 65, column: 1, scope: !60, atomGroup: 65, atomRank: 1)

exit:                                             ; preds = %entry
  ret void, !dbg !DILocation(line: 66, column: 1, scope: !60, atomGroup: 66, atomRank: 1)
}

define i32 @noncritical(i32 %k, ptr %p) !dbg !74 {
entry:
  %d = add i32 %k, 1, !dbg !DILocation(line: 67, column: 1, scope: !74, atomGroup: 67, atomRank: 1)
  indirectbr ptr %p, [label %bb0, label %bb1], !dbg !DILocation(line: 68, column: 1, scope: !74, atomGroup: 68, atomRank: 1)

bb0:                                              ; preds = %entry
  %r0 = sub i32 %k, %d, !dbg !DILocation(line: 71, column: 1, scope: !74, atomGroup: 71, atomRank: 1)
  br label %exit, !dbg !DILocation(line: 72, column: 1, scope: !74, atomGroup: 72, atomRank: 1)

bb1:                                              ; preds = %entry
  %r1 = sub i32 %d, %k, !dbg !DILocation(line: 75, column: 1, scope: !74, atomGroup: 75, atomRank: 1)
  br label %exit, !dbg !DILocation(line: 76, column: 1, scope: !74, atomGroup: 76, atomRank: 1)

exit:                                             ; preds = %bb1, %bb0
  %v = phi i32 [ %r0, %bb0 ], [ %r1, %bb1 ], !dbg !DILocation(line: 77, column: 1, scope: !74, atomGroup: 77, atomRank: 1)
  ret i32 %v, !dbg !DILocation(line: 78, column: 1, scope: !74, atomGroup: 78, atomRank: 1)
}

!llvm.dbg.cu = !{!0}
!llvm.debugify = !{!2, !3}
!llvm.module.flags = !{!4}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, producer: "debugify", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug)
!1 = !DIFile(filename: "<stdin>", directory: "/")
!2 = !{i32 78}
!3 = !{i32 0}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = distinct !DISubprogram(name: "simple", linkageName: "simple", scope: null, file: !1, line: 1, type: !6, scopeLine: 1, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!6 = !DISubroutineType(types: !7)
!7 = !{}
!8 = !DILocation(line: 1, column: 1, scope: !5, atomGroup: 1, atomRank: 1)
!9 = !DILocation(line: 2, column: 1, scope: !5, atomGroup: 2, atomRank: 1)
!10 = !DILocation(line: 3, column: 1, scope: !5, atomGroup: 3, atomRank: 1)
!11 = !DILocation(line: 4, column: 1, scope: !5, atomGroup: 4, atomRank: 1)
!12 = !DILocation(line: 7, column: 1, scope: !5, atomGroup: 7, atomRank: 1)
!13 = !DILocation(line: 8, column: 1, scope: !5, atomGroup: 8, atomRank: 1)
!14 = !DILocation(line: 11, column: 1, scope: !5, atomGroup: 11, atomRank: 1)
!15 = !DILocation(line: 12, column: 1, scope: !5, atomGroup: 12, atomRank: 1)
!16 = !DILocation(line: 13, column: 1, scope: !5, atomGroup: 13, atomRank: 1)
!17 = !DILocation(line: 14, column: 1, scope: !5, atomGroup: 14, atomRank: 1)
!18 = !DILocation(line: 15, column: 1, scope: !5, atomGroup: 15, atomRank: 1)
!19 = !DILocation(line: 16, column: 1, scope: !5, atomGroup: 16, atomRank: 1)
!20 = !DILocation(line: 17, column: 1, scope: !5, atomGroup: 17, atomRank: 1)
!21 = !DILocation(line: 18, column: 1, scope: !5, atomGroup: 18, atomRank: 1)
!22 = !DILocation(line: 19, column: 1, scope: !5, atomGroup: 19, atomRank: 1)
!23 = !DILocation(line: 20, column: 1, scope: !5, atomGroup: 20, atomRank: 1)
!24 = !DILocation(line: 7, column: 1, scope: !5, atomGroup: 81, atomRank: 1)
!25 = !DILocation(line: 11, column: 1, scope: !5, atomGroup: 84, atomRank: 1)
!26 = distinct !DISubprogram(name: "multi", linkageName: "multi", scope: null, file: !1, line: 21, type: !6, scopeLine: 21, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!27 = !DILocation(line: 21, column: 1, scope: !26, atomGroup: 21, atomRank: 1)
!28 = !DILocation(line: 22, column: 1, scope: !26, atomGroup: 22, atomRank: 1)
!29 = !DILocation(line: 23, column: 1, scope: !26, atomGroup: 23, atomRank: 1)
!30 = !DILocation(line: 24, column: 1, scope: !26, atomGroup: 24, atomRank: 1)
!31 = !DILocation(line: 25, column: 1, scope: !26, atomGroup: 25, atomRank: 1)
!32 = !DILocation(line: 26, column: 1, scope: !26, atomGroup: 26, atomRank: 1)
!33 = !DILocation(line: 27, column: 1, scope: !26, atomGroup: 27, atomRank: 1)
!34 = !DILocation(line: 28, column: 1, scope: !26, atomGroup: 28, atomRank: 1)
!35 = !DILocation(line: 29, column: 1, scope: !26, atomGroup: 29, atomRank: 1)
!36 = !DILocation(line: 30, column: 1, scope: !26, atomGroup: 30, atomRank: 1)
!37 = !DILocation(line: 31, column: 1, scope: !26, atomGroup: 31, atomRank: 1)
!38 = !DILocation(line: 32, column: 1, scope: !26, atomGroup: 32, atomRank: 1)
!39 = !DILocation(line: 33, column: 1, scope: !26, atomGroup: 33, atomRank: 1)
!40 = !DILocation(line: 34, column: 1, scope: !26, atomGroup: 34, atomRank: 1)
!41 = !DILocation(line: 35, column: 1, scope: !26, atomGroup: 35, atomRank: 1)
!42 = !DILocation(line: 36, column: 1, scope: !26, atomGroup: 36, atomRank: 1)
!43 = !DILocation(line: 37, column: 1, scope: !26, atomGroup: 37, atomRank: 1)
!44 = !DILocation(line: 38, column: 1, scope: !26, atomGroup: 38, atomRank: 1)
!45 = !DILocation(line: 39, column: 1, scope: !26, atomGroup: 39, atomRank: 1)
!46 = !DILocation(line: 40, column: 1, scope: !26, atomGroup: 40, atomRank: 1)
!47 = !DILocation(line: 41, column: 1, scope: !26, atomGroup: 41, atomRank: 1)
!48 = !DILocation(line: 42, column: 1, scope: !26, atomGroup: 42, atomRank: 1)
!49 = !DILocation(line: 43, column: 1, scope: !26, atomGroup: 43, atomRank: 1)
!50 = distinct !DISubprogram(name: "loop", linkageName: "loop", scope: null, file: !1, line: 44, type: !6, scopeLine: 44, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!51 = !DILocation(line: 46, column: 1, scope: !50, atomGroup: 86, atomRank: 1)
!52 = !DILocation(line: 46, column: 1, scope: !50, atomGroup: 46, atomRank: 1)
!53 = !DILocation(line: 47, column: 1, scope: !50, atomGroup: 47, atomRank: 1)
!54 = !DILocation(line: 48, column: 1, scope: !50, atomGroup: 48, atomRank: 1)
!55 = !DILocation(line: 49, column: 1, scope: !50, atomGroup: 49, atomRank: 1)
!56 = !DILocation(line: 50, column: 1, scope: !50, atomGroup: 50, atomRank: 1)
!57 = !DILocation(line: 51, column: 1, scope: !50, atomGroup: 51, atomRank: 1)
!58 = !DILocation(line: 52, column: 1, scope: !50, atomGroup: 52, atomRank: 1)
!59 = !DILocation(line: 53, column: 1, scope: !50, atomGroup: 53, atomRank: 1)
!60 = distinct !DISubprogram(name: "nophi", linkageName: "nophi", scope: null, file: !1, line: 54, type: !6, scopeLine: 54, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!61 = !DILocation(line: 54, column: 1, scope: !60, atomGroup: 54, atomRank: 1)
!62 = !DILocation(line: 55, column: 1, scope: !60, atomGroup: 55, atomRank: 1)
!63 = !DILocation(line: 56, column: 1, scope: !60, atomGroup: 56, atomRank: 1)
!64 = !DILocation(line: 57, column: 1, scope: !60, atomGroup: 57, atomRank: 1)
!65 = !DILocation(line: 58, column: 1, scope: !60, atomGroup: 58, atomRank: 1)
!66 = !DILocation(line: 59, column: 1, scope: !60, atomGroup: 59, atomRank: 1)
!67 = !DILocation(line: 60, column: 1, scope: !60, atomGroup: 60, atomRank: 1)
!68 = !DILocation(line: 61, column: 1, scope: !60, atomGroup: 61, atomRank: 1)
!69 = !DILocation(line: 62, column: 1, scope: !60, atomGroup: 62, atomRank: 1)
!70 = !DILocation(line: 63, column: 1, scope: !60, atomGroup: 63, atomRank: 1)
!71 = !DILocation(line: 64, column: 1, scope: !60, atomGroup: 64, atomRank: 1)
!72 = !DILocation(line: 65, column: 1, scope: !60, atomGroup: 65, atomRank: 1)
!73 = !DILocation(line: 66, column: 1, scope: !60, atomGroup: 66, atomRank: 1)
!74 = distinct !DISubprogram(name: "noncritical", linkageName: "noncritical", scope: null, file: !1, line: 67, type: !6, scopeLine: 67, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!75 = !DILocation(line: 67, column: 1, scope: !74, atomGroup: 67, atomRank: 1)
!76 = !DILocation(line: 68, column: 1, scope: !74, atomGroup: 68, atomRank: 1)
!77 = !DILocation(line: 71, column: 1, scope: !74, atomGroup: 71, atomRank: 1)
!78 = !DILocation(line: 72, column: 1, scope: !74, atomGroup: 72, atomRank: 1)
!79 = !DILocation(line: 75, column: 1, scope: !74, atomGroup: 75, atomRank: 1)
!80 = !DILocation(line: 76, column: 1, scope: !74, atomGroup: 76, atomRank: 1)
!81 = !DILocation(line: 77, column: 1, scope: !74, atomGroup: 77, atomRank: 1)
!82 = !DILocation(line: 78, column: 1, scope: !74, atomGroup: 78, atomRank: 1)
