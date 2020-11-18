; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=false \
; RUN: | FileCheck %s --check-prefixes=CHECK,DBGVALUE --implicit-check-not=DBG_VALUE
; RUN: llc %s -stop-before finalize-isel -o - \
; RUN:    -experimental-debug-variable-locations=true \
; RUN: | FileCheck %s --check-prefixes=CHECK,INSTRREF --implicit-check-not=DBG_VALUE

;; Check that dbg.assigns for an aggregate variable which lives on the stack
;; for some of its lifetime are lowered into an appropriate set of DBG_VALUEs.
;;
;; $ cat test.cpp
;; void esc(long* p);
;; struct Ex {
;;   long A;
;;   long B;
;; };
;; long fun(int In) {
;;   Ex X;
;;   X.B = 0;
;;   esc(&X.B);
;;   X.B += 2;
;;   X.B *= 2;
;;   esc(&X.B);
;;   return X.B;
;; }
;; $ clang++ test.cpp -O2 -g -emit-llvm -S -c -Xclang -debug-coffee-chat

; CHECK: ![[VAR:[0-9]+]] = !DILocalVariable(name: "X",

;; Initially the whole variable is on the stack.
; CHECK: bb.0.entry:
; CHECK-NEXT: DBG_VALUE
; CHECK-NEXT: DBG_VALUE %stack.0.X, $noreg, ![[VAR]], !DIExpression(DW_OP_deref), debug-location

;; Then there is a store to the upper 64 bits.
; CHECK: MOV64mi32 %stack.0.X, 1, $noreg, 8, $noreg, 0, debug-location
; CHECK-NEXT: DBG_VALUE %stack.0.X, $noreg, ![[VAR]], !DIExpression(DW_OP_plus_uconst, 8, DW_OP_deref, DW_OP_LLVM_fragment, 64, 64), debug-location
;; This DBG_VALUE is added by the frag-agg pass because bits [0, 64) are live
;; in memory too.
; CHECK-NEXT: DBG_VALUE %stack.0.X, $noreg, ![[VAR]], !DIExpression(DW_OP_deref, DW_OP_LLVM_fragment, 0, 64)

;; Next, a LEA in place of the add and mul. The result is then stored to the alloca.
;; Because the add assignment is not visible in the alloca (the add result is not
;; stored) we have to say that the stack location isn't valid for the entire scope.
; DBGVALUE: %2:gr64_nosp = MOV64rm %stack.0.X, 1, $noreg, 8, $noreg, debug-location
; DBGVALUE-NEXT: DBG_VALUE %2, $noreg, ![[VAR]], !DIExpression(DW_OP_plus_uconst, 2, DW_OP_stack_value, DW_OP_LLVM_fragment, 64, 64), debug-location
; INSTRREF: %2:gr64_nosp = MOV64rm %stack.0.X, 1, $noreg, 8, $noreg, debug-instr-number 1
; INSTRREF-NEXT: DBG_INSTR_REF 1, 0, ![[VAR]], !DIExpression(DW_OP_plus_uconst, 2, DW_OP_stack_value, DW_OP_LLVM_fragment, 64, 64)

; CHECK-NEXT: %3:gr64 = LEA64r %2, 1, %2, 4, $noreg, debug-location
; CHECK-NEXT: MOV64mr %stack.0.X, 1, $noreg, 8, $noreg, killed %3, debug-location
; CHECK-NEXT: DBG_VALUE %stack.0.X, $noreg, ![[VAR]], !DIExpression(DW_OP_plus_uconst, 8, DW_OP_deref, DW_OP_LLVM_fragment, 64, 64)

source_filename = "test.cpp"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct.Ex = type { i64, i64 }

; Function Attrs: uwtable mustprogress
define dso_local i64 @_Z3funi(i32 %In) local_unnamed_addr #0 !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata i1 undef, metadata !13, metadata !DIExpression(), metadata !19, metadata i32* undef), !dbg !20
  %X = alloca %struct.Ex, align 8, !DIAssignID !21
  call void @llvm.dbg.assign(metadata i1 undef, metadata !14, metadata !DIExpression(), metadata !21, metadata %struct.Ex* %X), !dbg !20
  call void @llvm.dbg.assign(metadata i32 %In, metadata !13, metadata !DIExpression(), metadata !22, metadata i32* undef), !dbg !20
  %0 = bitcast %struct.Ex* %X to i8*, !dbg !23
  call void @llvm.lifetime.start.p0i8(i64 16, i8* nonnull %0) #4, !dbg !23
  %B = getelementptr inbounds %struct.Ex, %struct.Ex* %X, i64 0, i32 1, !dbg !24
  store i64 0, i64* %B, align 8, !dbg !25, !tbaa !26, !DIAssignID !31
  call void @llvm.dbg.assign(metadata i64 0, metadata !14, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !31, metadata i64* %B), !dbg !25
  call void @_Z3escPl(i64* nonnull %B), !dbg !32
  %1 = load i64, i64* %B, align 8, !dbg !33, !tbaa !26
  call void @llvm.dbg.assign(metadata i64 %1, metadata !14, metadata !DIExpression(DW_OP_plus_uconst, 2, DW_OP_stack_value, DW_OP_LLVM_fragment, 64, 64), metadata !34, metadata i64* %B), !dbg !33
  %add = shl i64 %1, 1, !dbg !35
  %mul = add i64 %add, 4, !dbg !35
  store i64 %mul, i64* %B, align 8, !dbg !35, !tbaa !26, !DIAssignID !36
  call void @llvm.dbg.assign(metadata i64 %mul, metadata !14, metadata !DIExpression(DW_OP_LLVM_fragment, 64, 64), metadata !36, metadata i64* %B), !dbg !35
  call void @_Z3escPl(i64* nonnull %B), !dbg !37
  %2 = load i64, i64* %B, align 8, !dbg !38, !tbaa !26
  call void @llvm.lifetime.end.p0i8(i64 16, i8* nonnull %0) #4, !dbg !39
  ret i64 %2, !dbg !40
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare !dbg !41 dso_local void @_Z3escPl(i64*) local_unnamed_addr #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #3

attributes #0 = { uwtable mustprogress "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "min-legal-vector-width"="0" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nofree nosync nounwind willreturn }
attributes #2 = { "disable-tail-calls"="false" "frame-pointer"="none" "less-precise-fpmad"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #4 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git cb544e3e8c070a30227e1a269712bce0931a576d)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/home/och/dev/bugs/scratch")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0 (https://github.sie.sony.com/gbhyamso/coffee-chat.git cb544e3e8c070a30227e1a269712bce0931a576d)"}
!7 = distinct !DISubprogram(name: "fun", linkageName: "_Z3funi", scope: !1, file: !1, line: 6, type: !8, scopeLine: 6, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !12)
!8 = !DISubroutineType(types: !9)
!9 = !{!10, !11}
!10 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!11 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!12 = !{!13, !14}
!13 = !DILocalVariable(name: "In", arg: 1, scope: !7, file: !1, line: 6, type: !11)
!14 = !DILocalVariable(name: "X", scope: !7, file: !1, line: 7, type: !15)
!15 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "Ex", file: !1, line: 2, size: 128, flags: DIFlagTypePassByValue, elements: !16, identifier: "_ZTS2Ex")
!16 = !{!17, !18}
!17 = !DIDerivedType(tag: DW_TAG_member, name: "A", scope: !15, file: !1, line: 3, baseType: !10, size: 64)
!18 = !DIDerivedType(tag: DW_TAG_member, name: "B", scope: !15, file: !1, line: 4, baseType: !10, size: 64, offset: 64)
!19 = distinct !DIAssignID()
!20 = !DILocation(line: 0, scope: !7)
!21 = distinct !DIAssignID()
!22 = distinct !DIAssignID()
!23 = !DILocation(line: 7, column: 3, scope: !7)
!24 = !DILocation(line: 8, column: 5, scope: !7)
!25 = !DILocation(line: 8, column: 7, scope: !7)
!26 = !{!27, !28, i64 8}
!27 = !{!"_ZTS2Ex", !28, i64 0, !28, i64 8}
!28 = !{!"long", !29, i64 0}
!29 = !{!"omnipotent char", !30, i64 0}
!30 = !{!"Simple C++ TBAA"}
!31 = distinct !DIAssignID()
!32 = !DILocation(line: 9, column: 3, scope: !7)
!33 = !DILocation(line: 10, column: 7, scope: !7)
!34 = distinct !DIAssignID()
!35 = !DILocation(line: 11, column: 7, scope: !7)
!36 = distinct !DIAssignID()
!37 = !DILocation(line: 12, column: 3, scope: !7)
!38 = !DILocation(line: 13, column: 12, scope: !7)
!39 = !DILocation(line: 14, column: 1, scope: !7)
!40 = !DILocation(line: 13, column: 3, scope: !7)
!41 = !DISubprogram(name: "esc", linkageName: "_Z3escPl", scope: !1, file: !1, line: 1, type: !42, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized, retainedNodes: !2)
!42 = !DISubroutineType(types: !43)
!43 = !{null, !44}
!44 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !10, size: 64)
