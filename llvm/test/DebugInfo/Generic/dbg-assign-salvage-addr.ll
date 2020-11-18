; RUN: opt -S %s -instcombine | FileCheck %s

;; Hand-written. Check that salvaging a dbg.assign address works as expected:
;;
;; When expression is not required (bitcast, zero-offset-gep) we should just
;; replace the address.
; CHECK: call void @llvm.dbg.assign(metadata i32 %v,{{.+}}, metadata !DIExpression(),{{.+}}, metadata i32* %p)

;; Otherwise, we can't currently encode the offset, so set as undef.
;; FIXME: Add expressions for the address component. Sometimes an store still
;; takes place without having an offset to the particular address hanging around
;; (e.g. after vectorizing. Run any pass that cleans up redundant instructions
;; over llvm/test/DebugInfo/X86/dbg-assign-slp-vectorizer.ll after vectorizing
;; and notice that the address components of the dbg.assings become undef
;; despite the fact the stores do take place (merged into one store).
; CHECK: call void @llvm.dbg.assign(metadata i32 %v,{{.+}}, metadata !DIExpression(),{{.+}}, metadata i32* undef)

define dso_local void @_Z6assignPii(i32* %p, i32 %v) #0 !dbg !7 {
entry:
  %arrayidx0 = getelementptr inbounds i32, i32* %p, i32 0
  call void @llvm.dbg.assign(metadata i32 %v, metadata !14, metadata !DIExpression(), metadata !28, metadata i32* %arrayidx0), !dbg !17
  %arrayidx1 = getelementptr inbounds i32, i32* %p, i32 1
  call void @llvm.dbg.assign(metadata i32 %v, metadata !14, metadata !DIExpression(), metadata !28, metadata i32* %arrayidx1), !dbg !17
  ret void, !dbg !19
}

declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata)

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "assign", linkageName: "_Z6assignPii", scope: !1, file: !1, line: 1, type: !8, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!8 = !DISubroutineType(types: !9)
!9 = !{null, !10, !11}
!10 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !11, size: 64)
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !DILocalVariable(name: "p", arg: 1, scope: !7, file: !1, line: 1, type: !10)
!13 = !DILocation(line: 1, column: 18, scope: !7)
!14 = !DILocalVariable(name: "v", arg: 2, scope: !7, file: !1, line: 1, type: !11)
!15 = !DILocation(line: 1, column: 24, scope: !7)
!16 = !DILocation(line: 1, column: 36, scope: !7)
!17 = !DILocation(line: 1, column: 29, scope: !7)
!18 = !DILocation(line: 1, column: 34, scope: !7)
!19 = !DILocation(line: 1, column: 39, scope: !7)
!20 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funv", scope: !1, file: !1, line: 2, type: !21, scopeLine: 2, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!21 = !DISubroutineType(types: !22)
!22 = !{!11}
!23 = !DILocalVariable(name: "a", scope: !20, file: !1, line: 3, type: !11)
!24 = !DILocation(line: 3, column: 7, scope: !20)
!25 = !DILocation(line: 4, column: 3, scope: !20)
!26 = !DILocation(line: 5, column: 10, scope: !20)
!27 = !DILocation(line: 5, column: 3, scope: !20)
!28 = distinct !DIAssignID()
