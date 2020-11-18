; RUN: opt -instcombine %s -S -o - | FileCheck %s

;; $ cat test.cpp
;; class a {
;;   virtual ~a();
;; };
;; a::~a() { ; }
;;
;; IR grabbed before instcombine in:
;; $ clang -O2 -g test.c -o - -Xclang -debug-coffee-chat -mllvm -track-ptr-arg-dest
;;                                                       ^^^^^^^^^^^^^^^^^^^^^^^^^^
;;                                               NOTE: This isn't a thing any more.
;;                                               Test adjusted by hand, but this
;;                                               source no longer runs into the
;;                                               problem naturally.

;; Check that InstCombine's constant folding updates debug intrinsics that
;; might be using the same constant. Otherwise the debug use may be the only
;; remaining use of the (unfolded) constant which makes it liable to be cleaned
;; up later by another pass. Such change will at best reduce variable location
;; coverage unecessarily, and at worst delete the use, causing the debug
;; intrinsic to be considered dead, thus introducing variable location
;; inaccuracies.

; CHECK: store i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV1a, i64 0, inrange i32 0, i64 2) to i32 (...)**)
; CHECK-NEXT:  call void @llvm.dbg.assign(metadata i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV1a, i64 0, inrange i32 0, i64 2) to i32 (...)**)


%class.a = type { i32 (...)** }

@_ZTV1a = dso_local unnamed_addr constant { [4 x i8*] } { [4 x i8*] [i8* null, i8* bitcast ({ i8*, i8* }* @_ZTI1a to i8*), i8* bitcast (void (%class.a*)* @_ZN1aD2Ev to i8*), i8* bitcast (void (%class.a*)* @_ZN1aD0Ev to i8*)] }, align 8
@_ZTVN10__cxxabiv117__class_type_infoE = external dso_local global i8*
@_ZTS1a = dso_local constant [3 x i8] c"1a\00", align 1
@_ZTI1a = dso_local constant { i8*, i8* } { i8* bitcast (i8** getelementptr inbounds (i8*, i8** @_ZTVN10__cxxabiv117__class_type_infoE, i64 2) to i8*), i8* getelementptr inbounds ([3 x i8], [3 x i8]* @_ZTS1a, i32 0, i32 0) }, align 8

@_ZN1aD1Ev = dso_local unnamed_addr alias void (%class.a*), void (%class.a*)* @_ZN1aD2Ev

; Function Attrs: nounwind uwtable
define dso_local void @_ZN1aD2Ev(%class.a* %this) unnamed_addr #0 align 2 !dbg !7 {
entry:
  call void @llvm.dbg.assign(metadata %class.a undef, metadata !23, metadata !DIExpression(), metadata !24, metadata %class.a* %this), !dbg !25
  %0 = bitcast %class.a* %this to i32 (...)***, !dbg !26
  store i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV1a, i32 0, inrange i32 0, i32 2) to i32 (...)**), i32 (...)*** %0, align 8, !dbg !26, !tbaa !27, !DIAssignID !30
  call void @llvm.dbg.assign(metadata i32 (...)** bitcast (i8** getelementptr inbounds ({ [4 x i8*] }, { [4 x i8*] }* @_ZTV1a, i32 0, inrange i32 0, i32 2) to i32 (...)**), metadata !23, metadata !DIExpression(), metadata !30, metadata i32 (...)*** %0), !dbg !25
  ret void, !dbg !31
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.assign(metadata, metadata, metadata, metadata, metadata) #1
define dso_local void @_ZN1aD0Ev(%class.a* %this) unnamed_addr #2 align 2 personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*) !dbg !32 {
  ; deleted
  ret void
}

declare dso_local i32 @__gxx_personality_v0(...)

; Function Attrs: nobuiltin nounwind
declare dso_local void @_ZdlPv(i8*) local_unnamed_addr #3

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5}
!llvm.ident = !{!6}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus, file: !1, producer: "clang version 12.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "reduce.cpp", directory: "/")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{!"clang version 12.0.0"}
!7 = distinct !DISubprogram(name: "~a", linkageName: "_ZN1aD2Ev", scope: !8, file: !1, line: 4, type: !17, scopeLine: 4, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !16, retainedNodes: !20)
!8 = distinct !DICompositeType(tag: DW_TAG_class_type, name: "a", file: !1, line: 1, size: 64, flags: DIFlagTypePassByReference | DIFlagNonTrivial, elements: !9, vtableHolder: !8)
!9 = !{!10, !16}
!10 = !DIDerivedType(tag: DW_TAG_member, name: "_vptr$a", scope: !1, file: !1, baseType: !11, size: 64, flags: DIFlagArtificial)
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = !DIDerivedType(tag: DW_TAG_pointer_type, name: "__vtbl_ptr_type", baseType: !13, size: 64)
!13 = !DISubroutineType(types: !14)
!14 = !{!15}
!15 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!16 = !DISubprogram(name: "~a", scope: !8, file: !1, line: 2, type: !17, scopeLine: 2, containingType: !8, virtualIndex: 0, flags: DIFlagPrototyped, spFlags: DISPFlagVirtual | DISPFlagOptimized)
!17 = !DISubroutineType(types: !18)
!18 = !{null, !19}

!19 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64, flags: DIFlagArtificial | DIFlagObjectPointer)
!20 = !{!21}
!21 = !DILocalVariable(name: "this", arg: 1, scope: !7, type: !22, flags: DIFlagArtificial | DIFlagObjectPointer)
!22 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !8, size: 64)
!23 = !DILocalVariable(name: "hand-adjusted-variable", scope: !7, type: !22)
!24 = distinct !DIAssignID()
!25 = !DILocation(line: 0, scope: !7)
!26 = !DILocation(line: 4, column: 9, scope: !7)
!27 = !{!28, !28, i64 0}
!28 = !{!"vtable pointer", !29, i64 0}
!29 = !{!"Simple C++ TBAA"}
!30 = distinct !DIAssignID()
!31 = !DILocation(line: 4, column: 13, scope: !7)
!32 = distinct !DISubprogram(name: "~a", linkageName: "_ZN1aD0Ev", scope: !8, file: !1, line: 4, type: !17, scopeLine: 4, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, declaration: !16, retainedNodes: !33)
!33 = !{!34}
!34 = !DILocalVariable(name: "this", arg: 1, scope: !32, type: !22, flags: DIFlagArtificial | DIFlagObjectPointer)
!35 = !DILocalVariable(name: "hand-adjusted-variable", scope: !32, type: !22)
!36 = distinct !DIAssignID()
!37 = !DILocation(line: 0, scope: !32)
!38 = !DILocation(line: 4, column: 9, scope: !32)
!39 = !DILocation(line: 4, column: 13, scope: !32)
