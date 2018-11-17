// Copyright 2018 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <asm.h>
#include <arch/asm_macros.h>
#include <zircon/boot/image.h>

#define SCTLR_I     (1 << 12)   // Instruction cache enable
#define SCTLR_C     (1 << 2)    // Cache enable
#define SCTLR_M     (1 << 0)    // MMU enable

// scratch register, not saved across function calls
tmp             .req x16

#define STACK_SIZE  4096

.section .text.boot0,"ax"
FUNCTION(_start)
    // magic instruction that gives us UEFI "MZ" signature
    add x13, x18, #0x16
    b header_end

    .quad   0             // image offset from start of ram (unused)
    .quad   0             // image size (unused)
    .quad   0
    .quad   0
    .quad   0
    .quad   0

    // arm64 magic number
    .byte   'A'
    .byte   'R'
    .byte   'M'
    .byte   0x64
    .align 3

header_end:
    // x0 typically points to device tree at entry

   // what EL are we running at?
    mrs     tmp, CurrentEL
    cmp     tmp, #(1 << 2)
    beq     cache_disable_el1

    // Disable caches and MMU (EL2 version)
    mrs     tmp, sctlr_el2
    bic     tmp, tmp, #SCTLR_I
    bic     tmp, tmp, #SCTLR_C
    bic     tmp, tmp, #SCTLR_M
    msr     sctlr_el2, tmp
    b       cache_disable_done

cache_disable_el1:
    // Disable caches and MMU (EL1 version)
    mrs     tmp, sctlr_el1
    bic     tmp, tmp, #SCTLR_I
    bic     tmp, tmp, #SCTLR_C
    bic     tmp, tmp, #SCTLR_M
    msr     sctlr_el1, tmp

cache_disable_done:
    // setup stack
    adr     tmp, stack_end
    mov     sp, tmp

    // x0: pointer to device tree
    bl      boot_shim

    // x0: bootdata_t* to pass to kernel
    // x1: kernel entry point
    br      x1
END_FUNCTION(_start)

/* void arch_invalidate_cache_all()
 *      should only be used early in boot, prior to enabling mmu/cache
 */
FUNCTION(arch_invalidate_cache_all)
    mrs     x0, clidr_el1
    and     w3, w0, #0x07000000
    lsr     w3, w3, #23
    cbz     w3, finished
    mov     w10, #0
    mov     w8, #1
loop1:
    add     w2, w10, w10, lsr #1
    lsr     w1, w0, w2
    and     w1, w1, #0x7
    cmp     w1, #2
    b.lt    skip
    msr     csselr_el1, x10
    isb
    mrs     x1, ccsidr_el1
    and     w2, w1, #7
    add     w2, w2, #4
    ubfx    w4, w1, #3, #10
    clz     w5, w4
    lsl     w9, w4, w5

    lsl     w16, w8, w5

loop2:
    ubfx    w7, w1, #13, #15
    lsl     w7, w7, w2
    lsl     w17, w8, w2
loop3:
    orr     w11, w10, w9
    orr     w11, w11, w7
    dc      isw, x11
    subs    w7, w7, w17
    b.ge    loop3

    subs    x9, x9, x16
    b.ge    loop2
skip:
    add     w10, w10, #2
    cmp     w3, w10
    dsb     sy
    b.gt    loop1
finished:
    ic      iallu
    ret
END_FUNCTION(arch_invalidate_cache_all)

.bss
.balign 16
LOCAL_DATA(stack)
    .skip STACK_SIZE
LOCAL_DATA(stack_end)
END_DATA(stack)
