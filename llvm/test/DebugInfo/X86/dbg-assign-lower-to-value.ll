; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=false \
; RUN: | FileCheck %s --check-prefixes=CHECK,DBGVALUE --implicit-check-not=DBG_VALUE
; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=true \
; RUN: | FileCheck %s --check-prefixes=CHECK,INSTRREF --implicit-check-not=DBG_VALUE

;; Check that dbg.assigns for a variable which lives on the stack
;; for some of its lifetime are lowered into an appropriate set
;; of DBG_VALUEs.
;;
;; $ cat test.cpp
;; void maybe_writes(int*);
;; void ext(int, int, int, int, int, int, int, int, int, int);
;; int example() {
;;    int local = 0;
;;    maybe_writes(&local);
;;    ext(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
;;    local += 2;
;;    return local;
;; }
;; $ clang++ -O2 -g -emit-llvm -S -c -Xclang -debug-coffee-chat

; CHECK: ![[VAR:[0-9]+]] = !DILocalVariable(name: "local",
;; Check we have no debug info for local in the side table.
; CHECK: stack:
; CHECK-NEXT: - { id: 0, name: local, type: default, offset: 0, size: 4, alignment: 4,
; CHECK-NEXT:     stack-id: default, callee-saved-register: '', callee-saved-restored: true,
; CHECK-NEXT:     debug-info-variable: '', debug-info-expression: '', debug-info-location: '' }

; CHECK: bb.0.entry:
; CHECK-NEXT: DBG_VALUE %stack.0.local, $noreg, ![[VAR]], !DIExpression(DW_OP_deref), debug-location
; CHECK-NEXT: LIFETIME_START %stack.0.local, debug-location
; CHECK-NEXT: MOV32mi %stack.0.local, 1, $noreg, 0, $noreg, 0, debug-location
;; No DBG_VALUE required because the stack location is still valid.

;; local no longer lives on the stack from the add.
; DBGVALUE: %9:gr32 = nsw ADD32ri8 %8, 2, implicit-def dead $eflags, debug-location
; DBGVALUE-NEXT: DBG_VALUE %9, $noreg, ![[VAR]], !DIExpression(), debug-location
; INSTRREF: %9:gr32 = nsw ADD32ri8 %8, 2, implicit-def dead $eflags, debug-instr-number 1
; INSTRREF-NEXT: DBG_INSTR_REF 1, 0, ![[VAR]], !DIExpression()

source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local i32 @_Z7examplev() local_unnamed_addr !dbg !7 {
entry:
  %local = alloca i32, align 4, !DIAssignID !13
  call void @llvm.dbg.assign(metadata i1 undef, metadata !12, metadata !DIExpression(), metadata !13, metadata i32* %local), !dbg !14
  %0 = bitcast i32* %local to i8*, !dbg !15
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0), !dbg !15
  store i32 0, i32* %local, align 4, !dbg !16, !tbaa !17, !DIAssignID !21
  call void @llvm.dbg.assign(metadata i32 0, metadata !12, metadata !DIExpression(), metadata !21, metadata i32* %local), !dbg !16
  call void @_Z12maybe_writesPi(i32* nonnull %local), !dbg !22
  call void @_Z3extiiiiiiiiii(i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7, i32 8, i32 9), !dbg !23
  %1 = load i32, i32* %local, align 4, !dbg !24, !tbaa !17
  %add = add nsw i32 %1, 2, !dbg !24
  call void @llvm.dbg.assign(metadata i32 %add, metadata !12, metadata !DIExpression(), metadata !25, metadata i32* %local), !dbg !24
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0), !dbg !26
  ret i32 %add, !dbg !27
}

declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture)
declare !dbg !28 dso_local void @_Z12maybe_writesPi(i32*) local_unnamed_addr
declare !dbg !32 dso_local void @_Z3extiiiiiiiiii(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32) local_unnamed_addr
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
!7 = distinct !DISubprogram(name: "example", linkageName: "_Z7examplev", scope: !1, file: !1, line: 3, type: !8, scopeLine: 3, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !11)
!8 = !DISubroutineType(types: !9)
!9 = !{!10}
!10 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!11 = !{!12}
!12 = !DILocalVariable(name: "local", scope: !7, file: !1, line: 4, type: !10)
!13 = distinct !DIAssignID()
!14 = !DILocation(line: 0, scope: !7)
!15 = !DILocation(line: 4, column: 4, scope: !7)
!16 = !DILocation(line: 4, column: 8, scope: !7)
!17 = !{!18, !18, i64 0}
!18 = !{!"int", !19, i64 0}
!19 = !{!"omnipotent char", !20, i64 0}
!20 = !{!"Simple C++ TBAA"}
!21 = distinct !DIAssignID()
!22 = !DILocation(line: 5, column: 4, scope: !7)
!23 = !DILocation(line: 6, column: 4, scope: !7)
!24 = !DILocation(line: 7, column: 10, scope: !7)
!25 = distinct !DIAssignID()
!26 = !DILocation(line: 9, column: 1, scope: !7)
!27 = !DILocation(line: 8, column: 4, scope: !7)
!28 = !DISubprogram(name: "maybe_writes", linkageName: "_Z12maybe_writesPi", scope: !1, file: !1, line: 1, type: !29, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!29 = !DISubroutineType(types: !30)
!30 = !{null, !31}
!31 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
!32 = !DISubprogram(name: "ext", linkageName: "_Z3extiiiiiiiiii", scope: !1, file: !1, line: 2, type: !33, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!33 = !DISubroutineType(types: !34)
!34 = !{null, !10, !10, !10, !10, !10, !10, !10, !10, !10, !10}
