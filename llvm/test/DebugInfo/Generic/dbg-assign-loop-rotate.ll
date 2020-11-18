; The loop rotate transformation can clone store instructions. When doing so
; it is necessary to emit a new DIAssignID and in this case update an existing
; dbg.assign.
; IR based on the following C source.

; int FuncA(const int& a)
; { return a * 10;
; }

;int main(int argc, char** argv)
;{ int val = 0;

; for (int i = 0; i < argc + 2; ++i)
;    val += FuncA(i); // DexWatch('i', 'val')

; return val;
;}

; RUN: opt -S -loop-rotate %s -o - | FileCheck %s
; CHECK:      call void @llvm.dbg.assign(metadata i32 0, metadata !35, metadata !DIExpression(), metadata ![[id:[0-9]+]], metadata i32* %i)
; CHECK-NEXT: store i32 0, i32* %i, align 4, !dbg !45, !tbaa !19, !DIAssignID ![[id]]
; CHECK:     ![[id]] = distinct !DIAssignID()

define dso_local i32 @_Z5FuncARKi(i32* nocapture nonnull readonly align 4 dereferenceable(4) %a) local_unnamed_addr  !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata i32* %a, metadata !15, metadata !DIExpression(), metadata !16, metadata i32** undef), !dbg !17
  %0 = load i32, i32* %a, align 4, !dbg !18, !tbaa !19
  %mul = mul nsw i32 %0, 10, !dbg !23
  ret i32 %mul, !dbg !24
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

; Function Attrs: norecurse nounwind readonly uwtable mustprogress
define dso_local i32 @main(i32 %argc, i8** nocapture readnone %argv) local_unnamed_addr !dbg !25 {
entry:
  %i = alloca i32, align 4
  call void @llvm.dbg.assign(metadata i32 %argc, metadata !32, metadata !DIExpression(), metadata !37, metadata i32* undef), !dbg !38
  call void @llvm.dbg.assign(metadata i8** %argv, metadata !33, metadata !DIExpression(), metadata !39, metadata i8*** undef), !dbg !38
  call void @llvm.dbg.assign(metadata i32 0, metadata !34, metadata !DIExpression(), metadata !40, metadata i32* undef), !dbg !41
  %0 = bitcast i32* %i to i8*, !dbg !42
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #4, !dbg !42
  call void @llvm.dbg.assign(metadata i32 0, metadata !35, metadata !DIExpression(), metadata !43, metadata i32* %i), !dbg !44
  br label %for.cond, !dbg !42

for.cond:                                         ; preds = %for.body, %entry
  %storemerge = phi i32 [ 0, %entry ], [ %inc, %for.body ], !dbg !45
  %val.0 = phi i32 [ 0, %entry ], [ %add1, %for.body ], !dbg !38
  store i32 %storemerge, i32* %i, align 4, !dbg !45, !tbaa !19, !DIAssignID !43
  %add = add nsw i32 %argc, 2, !dbg !46
  %cmp = icmp slt i32 %storemerge, %add, !dbg !48
  br i1 %cmp, label %for.body, label %for.cond.cleanup, !dbg !49

for.cond.cleanup:                                 ; preds = %for.cond
  %val.0.lcssa = phi i32 [ %val.0, %for.cond ], !dbg !38
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #4, !dbg !50
  ret i32 %val.0.lcssa, !dbg !51

for.body:                                         ; preds = %for.cond
  %call = call i32 @_Z5FuncARKi(i32* nonnull align 4 dereferenceable(4) %i), !dbg !52
  %add1 = add nsw i32 %call, %val.0, !dbg !53
  call void @llvm.dbg.assign(metadata i32 %add1, metadata !34, metadata !DIExpression(), metadata !54, metadata i32* undef), !dbg !53
  %inc = add nuw nsw i32 %storemerge, 1, !dbg !55
  call void @llvm.dbg.assign(metadata i32 %inc, metadata !35, metadata !DIExpression(), metadata !43, metadata i32* %i), !dbg !55
  br label %for.cond, !dbg !50, !llvm.loop !56
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)

declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None, sysroot: "/")
!1 = !DIFile(filename: "/home/binutils/git/dexter/tests/nostdlib/val_resets_to_zero/test.cpp", directory: "/home/binutils/git/coffee-chat")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "FuncA", linkageName: "_Z5FuncARKi", scope: !8, file: !8, line: 8, type: !9, scopeLine: 9, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !14)
!8 = !DIFile(filename: "test.cpp", directory: "/test")
!9 = !DISubroutineType(types: !10)
!10 = !{!11, !12}
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !13, size: 64)
!13 = !DIDerivedType(tag: DW_TAG_const_type, baseType: !11)
!14 = !{!15}
!15 = !DILocalVariable(name: "a", arg: 1, scope: !7, file: !8, line: 8, type: !12)
!16 = distinct !DIAssignID()
!17 = !DILocation(line: 0, scope: !7)
!18 = !DILocation(line: 10, column: 9, scope: !7)
!19 = !{!20, !20, i64 0}
!20 = !{!"int", !21, i64 0}
!21 = !{!"omnipotent char", !22, i64 0}
!22 = !{!"Simple C++ TBAA"}
!23 = !DILocation(line: 10, column: 11, scope: !7)
!24 = !DILocation(line: 10, column: 2, scope: !7)
!25 = distinct !DISubprogram(name: "main", scope: !8, file: !8, line: 13, type: !26, scopeLine: 14, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !31)
!26 = !DISubroutineType(types: !27)
!27 = !{!11, !11, !28}
!28 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !29, size: 64)
!29 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !30, size: 64)
!30 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!31 = !{!32, !33, !34, !35}
!32 = !DILocalVariable(name: "argc", arg: 1, scope: !25, file: !8, line: 13, type: !11)
!33 = !DILocalVariable(name: "argv", arg: 2, scope: !25, file: !8, line: 13, type: !28)
!34 = !DILocalVariable(name: "val", scope: !25, file: !8, line: 15, type: !11)
!35 = !DILocalVariable(name: "i", scope: !36, file: !8, line: 17, type: !11)
!36 = distinct !DILexicalBlock(scope: !25, file: !8, line: 17, column: 2)
!37 = distinct !DIAssignID()
!38 = !DILocation(line: 0, scope: !25)
!39 = distinct !DIAssignID()
!40 = distinct !DIAssignID()
!41 = !DILocation(line: 15, column: 6, scope: !25)
!42 = !DILocation(line: 17, column: 7, scope: !36)
!43 = distinct !DIAssignID()
!44 = !DILocation(line: 17, column: 11, scope: !36)
!45 = !DILocation(line: 0, scope: !36)
!46 = !DILocation(line: 17, column: 27, scope: !47)
!47 = distinct !DILexicalBlock(scope: !36, file: !8, line: 17, column: 2)
!48 = !DILocation(line: 17, column: 20, scope: !47)
!49 = !DILocation(line: 17, column: 2, scope: !36)
!50 = !DILocation(line: 17, column: 2, scope: !47)
!51 = !DILocation(line: 20, column: 2, scope: !25)
!52 = !DILocation(line: 18, column: 10, scope: !47)
!53 = !DILocation(line: 18, column: 7, scope: !47)
!54 = distinct !DIAssignID()
!55 = !DILocation(line: 17, column: 32, scope: !47)
!56 = distinct !{!56, !49, !57, !58}
!57 = !DILocation(line: 18, column: 17, scope: !36)
!58 = !{!"llvm.loop.mustprogress"}
