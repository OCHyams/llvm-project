; RUN: opt -S %s -sink-common-insts -simplifycfg -o - \
; RUN: | FileCheck %s --implicit-check-not="call void @llvm.dbg.assign"

;; sink-common-insts switch added because simplifycfg option efaults apear to
;; be different for clang and opt, and we need the common instructions in
;; if.then and if.else to sink to trigger the two-entry-phi-node folding.

;; $ cat test.cpp
;; class a {
;;   float b;
;; };
;; class c {
;; public:
;;   a d();
;; };
;; class e {
;; public:
;;   c &f();
;; };
;; class g {
;; public:
;;   void h(a &);
;; };
;; class i {
;;   g j;
;;   e k;
;;   e l;
;;   bool m;
;;   void n();
;; };
;; void i::n() {
;;   a o;
;;   if (m)
;;     o = k.f().d(); // <- Sink common tails & fold two-entry-phi.
;;   else             // <-
;;     o = l.f().d(); // <-
;;   j.h(o);
;; }
;; $ clang -O2 -g -Xclang -debug-coffee-chat
;;   ...Verifier assertion failure...
;;   !DIAssignID should be used by at least one llvm.dbg.assign intrinsic

;; Check that SimplifyCFG doesn't incorrectly delete dbg.assign intrinsics, and
;; that the hoisted dbg.assign intrinsics' value components are made undef.

; CHECK: dbg.assign(metadata i1 undef
; CHECK: dbg.assign(metadata float undef
; CHECK: dbg.assign(metadata float undef

%class.i = type { %class.g, %class.e, %class.e, i8 }
%class.g = type { i8 }
%class.e = type { i8 }
%class.a = type { float }
%class.c = type { i8 }

; Function Attrs: uwtable
define dso_local void @_ZN1i1nEv(%class.i* %this) local_unnamed_addr #0 align 2 !dbg !7 {
entry:
  %o = alloca %class.a, align 4, !DIAssignID !47
  call void @llvm.dbg.assign(metadata i1 undef, metadata !46, metadata !DIExpression(), metadata !47, metadata %class.a* %o), !dbg !48
  %0 = bitcast %class.a* %o to i8*, !dbg !49
  call void @llvm.lifetime.start.p0i8(i64 4, i8* nonnull %0) #4, !dbg !49
  %m = getelementptr inbounds %class.i, %class.i* %this, i64 0, i32 3, !dbg !50
  %1 = load i8, i8* %m, align 1, !dbg !50, !tbaa !52, !range !59
  %tobool.not = icmp eq i8 %1, 0, !dbg !50
  br i1 %tobool.not, label %if.else, label %if.then, !dbg !60

if.then:                                          ; preds = %entry
  %k = getelementptr inbounds %class.i, %class.i* %this, i64 0, i32 1, !dbg !61
  %call = tail call nonnull align 1 dereferenceable(1) %class.c* @_ZN1e1fEv(%class.e* nonnull %k), !dbg !62
  %call2 = tail call float @_ZN1c1dEv(%class.c* nonnull %call), !dbg !63
  call void @llvm.dbg.assign(metadata float %call2, metadata !46, metadata !DIExpression(), metadata !64, metadata float* %2), !dbg !48
  br label %if.end, !dbg !65

if.else:                                          ; preds = %entry
  %l = getelementptr inbounds %class.i, %class.i* %this, i64 0, i32 2, !dbg !66
  %call4 = tail call nonnull align 1 dereferenceable(1) %class.c* @_ZN1e1fEv(%class.e* nonnull %l), !dbg !67
  %call5 = tail call float @_ZN1c1dEv(%class.c* nonnull %call4), !dbg !68
  call void @llvm.dbg.assign(metadata float %call5, metadata !46, metadata !DIExpression(), metadata !64, metadata float* %2), !dbg !48
  br label %if.end

if.end:                                           ; preds = %if.else, %if.then
  %call2.sink = phi float [ %call5, %if.else ], [ %call2, %if.then ], !dbg !70
  %2 = getelementptr inbounds %class.a, %class.a* %o, i64 0, i32 0, !dbg !71
  store float %call2.sink, float* %2, align 4, !dbg !71, !DIAssignID !64
  %j = getelementptr inbounds %class.i, %class.i* %this, i64 0, i32 0, !dbg !72
  call void @_ZN1g1hER1a(%class.g* %j, %class.a* nonnull align 4 dereferenceable(4) %o), !dbg !73
  call void @llvm.lifetime.end.p0i8(i64 4, i8* nonnull %0) #4, !dbg !74
  ret void, !dbg !74
}

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local nonnull align 1 dereferenceable(1) %class.c* @_ZN1e1fEv(%class.e*) local_unnamed_addr #2

declare dso_local float @_ZN1c1dEv(%class.c*) local_unnamed_addr #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #1

declare dso_local void @_ZN1g1hER1a(%class.g*, %class.a* nonnull align 4 dereferenceable(4)) local_unnamed_addr #2

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #3

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "clang version 12.0.0)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "n", linkageName: "_ZN1i1nEv", scope: !8, file: !1, line: 23, type: !40, scopeLine: 23, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !39, retainedNodes: !43)
!8 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "i", file: !1, line: 16, size: 32, flags: DIFlagTypePassByValue, elements: !9, identifier: "_ZTS1i")
!9 = !{!10, !22, !36, !37, !39}
!10 = !DIDerivedType(tag: DW_TAG_member, name: "j", scope: !8, file: !1, line: 17, baseType: !11, size: 8)
!11 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "g", file: !1, line: 12, size: 8, flags: DIFlagTypePassByValue, elements: !12, identifier: "_ZTS1g")
!12 = !{!13}
!13 = !DISubprogram(name: "h", linkageName: "_ZN1g1hER1a", scope: !11, file: !1, line: 14, type: !14, scopeLine: 14, flags: DIFlagPublic | DIFlagPrototyped, spFlags: DISPFlagOptimized)
!14 = !DISubroutineType(types: !15)
!15 = !{null, !16, !17}
!16 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !11, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!17 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !18, size: 64)
!18 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "a", file: !1, line: 1, size: 32, flags: DIFlagTypePassByValue, elements: !19, identifier: "_ZTS1a")
!19 = !{!20}
!20 = !DIDerivedType(tag: DW_TAG_member, name: "b", scope: !18, file: !1, line: 2, baseType: !21, size: 32)
!21 = !DIBasicType(name: "float", size: 32, encoding: DW_ATE_float)
!22 = !DIDerivedType(tag: DW_TAG_member, name: "k", scope: !8, file: !1, line: 18, baseType: !23, size: 8, offset: 8)
!23 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "e", file: !1, line: 8, size: 8, flags: DIFlagTypePassByValue, elements: !24, identifier: "_ZTS1e")
!24 = !{!25}
!25 = !DISubprogram(name: "f", linkageName: "_ZN1e1fEv", scope: !23, file: !1, line: 10, type: !26, scopeLine: 10, flags: DIFlagPublic | DIFlagPrototyped, spFlags: DISPFlagOptimized)
!26 = !DISubroutineType(types: !27)
!27 = !{!28, !35}
!28 = !DIDerivedType(tag: DW_TAG_reference_type, baseType: !29, size: 64)
!29 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "c", file: !1, line: 4, size: 8, flags: DIFlagTypePassByValue, elements: !30, identifier: "_ZTS1c")
!30 = !{!31}
!31 = !DISubprogram(name: "d", linkageName: "_ZN1c1dEv", scope: !29, file: !1, line: 6, type: !32, scopeLine: 6, flags: DIFlagPublic | DIFlagPrototyped, spFlags: DISPFlagOptimized)
!32 = !DISubroutineType(types: !33)
!33 = !{!18, !34}
!34 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !29, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!35 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !23, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!36 = !DIDerivedType(tag: DW_TAG_member, name: "l", scope: !8, file: !1, line: 19, baseType: !23, size: 8, offset: 16)
!37 = !DIDerivedType(tag: DW_TAG_member, name: "m", scope: !8, file: !1, line: 20, baseType: !38, size: 8, offset: 24)
!38 = !DIBasicType(name: "bool", size: 8, encoding: DW_ATE_boolean)
!39 = !DISubprogram(name: "n", linkageName: "_ZN1i1nEv", scope: !8, file: !1, line: 21, type: !40, scopeLine: 21, flags: DIFlagPrototyped, spFlags: DISPFlagOptimized)
!40 = !DISubroutineType(types: !41)
!41 = !{null, !42}
!42 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!43 = !{!44, !46}
!44 = !DILocalVariable(name: "this", arg: 1, scope: !7, type: !45, flags: DIFlagArtificial | DIFlagObjectPointer)
!45 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!46 = !DILocalVariable(name: "o", scope: !7, file: !1, line: 24, type: !18)
!47 = distinct !DIAssignID()
!48 = !DILocation(line: 0, scope: !7)
!49 = !DILocation(line: 24, column: 3, scope: !7)
!50 = !DILocation(line: 25, column: 7, scope: !51)
!51 = distinct !DILexicalBlock(scope: !7, file: !1, line: 25, column: 7)
!52 = !{!53, !56, i64 3}
!53 = !{!"_ZTS1i", !54, i64 0, !55, i64 1, !55, i64 2, !56, i64 3}
!54 = !{!"_ZTS1g"}
!55 = !{!"_ZTS1e"}
!56 = !{!"bool", !57, i64 0}
!57 = !{!"omnipotent char", !58, i64 0}
!58 = !{!"Simple C++ TBAA"}
!59 = !{i8 0, i8 2}
!60 = !DILocation(line: 25, column: 7, scope: !7)
!61 = !DILocation(line: 26, column: 9, scope: !51)
!62 = !DILocation(line: 26, column: 11, scope: !51)
!63 = !DILocation(line: 26, column: 15, scope: !51)
!64 = distinct !DIAssignID()
!65 = !DILocation(line: 26, column: 5, scope: !51)
!66 = !DILocation(line: 28, column: 9, scope: !51)
!67 = !DILocation(line: 28, column: 11, scope: !51)
!68 = !DILocation(line: 28, column: 15, scope: !51)
!69 = distinct !DIAssignID()
!70 = !DILocation(line: 0, scope: !51)
!71 = !DILocation(line: 28, column: 7, scope: !51)
!72 = !DILocation(line: 29, column: 3, scope: !7)
!73 = !DILocation(line: 29, column: 5, scope: !7)
!74 = !DILocation(line: 30, column: 1, scope: !7)
