; RUN: llc %s --filetype=obj -o - --dwarf-use-key-instructions \
; RUN: | llvm-objdump -d - --no-show-raw-insn \
; RUN: | FileCheck %s --check-prefix=OBJ
;
; RUN: llc %s --filetype=obj -o - --dwarf-use-key-instructions \
; RUN: | llvm-dwarfdump - --debug-line \
; RUN: | FileCheck %s --check-prefix=DBG

; OBJ: 0000000000000000 <fun>:
; OBJ-NEXT:  0:  pushq   %rbx
; OBJ-NEXT:  1:  movq    (%rip), %rax
; OBJ-NEXT:  8:  movl    $0x0, (%rax)
; OBJ-NEXT:  e:  movq    (%rip), %rax
; OBJ-NEXT: 15:  movl    (%rax), %ebx
; OBJ-NEXT: 17:  callq   0x1c <fun+0x1c>
; OBJ-NEXT: 1c:  callq   0x21 <fun+0x21>
; OBJ-NEXT: 21:  addl    %ebx, %eax
; OBJ-NEXT: 23:  popq    %rbx
; OBJ-NEXT: 24:  retq

; DBG:      Address            Line   Column File   ISA Discriminator OpIndex Flags
; DBG-NEXT: ------------------ ------ ------ ------ --- ------------- ------- -------------
; DBG-NEXT: 0x0000000000000000      1      0      0   0             0       0  is_stmt
; DBG-NEXT: 0x0000000000000001      1      0      0   0             0       0  is_stmt prologue_end
; DBG-NEXT: 0x000000000000000e      2      0      0   0             0       0
; DBG-NEXT: 0x0000000000000017      3      0      0   0             0       0  is_stmt
; DBG-NEXT: 0x000000000000001c      4      0      0   0             0       0  is_stmt
; DBG-NEXT: 0x0000000000000021      5      0      0   0             0       0  is_stmt
; DBG-NEXT: 0x0000000000000023      6      0      0   0             0       0  is_stmt epilogue_begin
; DBG-NEXT: 0x0000000000000025      6      0      0   0             0       0  is_stmt end_sequence

;; Check the 1st call gets is_stmt despite having no atom group. Check the 2nd
;; call gets is_stmt applied despite being part of group 1 and having lower
;; precedence than the add. Check that the add stil gets is_stmt applied.

;; The store is added to prevent a rotten-green test. Non-key-instructions mode
;; will add is_stmt to each entry as each is a new line. Key Instructions mode
;; skips line 2 as it's not a call, pro/epi end/begin, or part of an atom.

target triple = "x86_64-unknown-linux-gnu"

@a = global i32 0
@z = global i32 0

define hidden i32 @fun() local_unnamed_addr !dbg !11 {
entry:
  store i32 0, ptr @z,     !dbg !DILocation(line: 1, scope: !11)
  %b = load i32, ptr @a,   !dbg !DILocation(line: 2, scope: !11)
  tail call void @f(),     !dbg !DILocation(line: 3, scope: !11)
  %x = tail call i32 @g(), !dbg !DILocation(line: 4, scope: !11, atomGroup: 1, atomRank: 2)
  %y = add i32 %x, %b,     !dbg !DILocation(line: 5, scope: !11, atomGroup: 1, atomRank: 1)
  ret i32 %y,              !dbg !DILocation(line: 6, scope: !11)
}

declare void @f() local_unnamed_addr
declare i32  @g() local_unnamed_addr

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3}
!llvm.ident = !{!10}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_17, file: !1, producer: "clang version 19.0.0", isOptimized: true, runtimeVersion: 0, emissionKind: LineTablesOnly, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "test.cpp", directory: "/")
!2 = !{i32 7, !"Dwarf Version", i32 5}
!3 = !{i32 2, !"Debug Info Version", i32 3}
!10 = !{!"clang version 19.0.0"}
!11 = distinct !DISubprogram(name: "fun", scope: !1, file: !1, line: 1, type: !12, scopeLine: 1, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0)
!12 = !DISubroutineType(types: !13)
!13 = !{}
