!-----------------------------------------------------------------------
! SEGA 32X support code for SH2 - CORRECT CD-mode boot table.
!
! Based on a real disassembly of the Master and Slave SH-2 boot ROMs'
! actual CD-mode logic (Mega Drive Developers Collective forum, "Sega CD
! General" thread), plus Sega's own 32X DDK sample header. The boot ROM
! reads a FIXED address - frame buffer + 0x18 - for a CD-specific 56-byte
! table, completely independent of any header format embedded within our
! own payload. Every previous test failed because none of them ever
! constructed this table; the boot ROM was never looking at any header
! WE built, under any of our tests.
!
! Real Master SH-2 boot ROM logic (from the disassembly):
!   waits for "_CD_" in COMM0
!   gets frame buffer access
!   r8 = 0x24000018 (FIXED address - frame buffer base + 0x18)
!   r9 = @(0,r8)   = Destination (absolute SDRAM address to copy to)
!   r0 = @(4,r8)   = Length of program data
!   r10= @(8,r8)   = Master entry point (absolute address)
!   r11= @(0x10,r8)= Master VBR (absolute address)
!   r8 += 0x20      (now points at frame buffer + 0x38 = program data)
!   copies Length bytes from frame buffer+0x38 to SDRAM at Destination
!   writes "M_OK" to COMM0, sets VBR=r11, jumps to r10
!
! Real Slave SH-2 boot ROM logic:
!   waits for "_CD_", waits for Master's M_OK
!   r8 = 0x24000018 (SAME fixed address)
!   r10= @(0xC,r8) = Slave entry point (absolute address)
!   r11= @(0x14,r8)= Slave VBR (absolute address)
!   writes "S_OK" to COMM4, sets VBR=r11, jumps to r10
!
! This table (56 bytes / 0x38) MUST be the very first thing copied to
! the frame buffer (address 0x840000 on the 68K side). The real
! compiled SH-2 program (starting at pri_vbr, below) MUST immediately
! follow it, at frame-buffer offset 0x38.
!-----------------------------------------------------------------------

        .section .header,"ax",@progbits

        .ascii  "CD BOOT TABLE   "         /* module name, 16 bytes */
        .long   0x00000000                 /* version */
        .long   0x00000000                 /* not used */
        .long   0x06000000                 /* Destination - ABSOLUTE SDRAM address */
        .long   __code_size                /* Length of program data that follows */
        .long   0x06000000 + (pri_start - pri_vbr)  /* Master entry point - ABSOLUTE */
        .long   0x06000000 + (sec_start - pri_vbr)  /* Slave entry point - ABSOLUTE */
        .long   0x06000000                          /* Master VBR - ABSOLUTE (pri_vbr lands at SDRAM base) */
        .long   0x06000000 + (sec_vbr - pri_vbr)    /* Slave VBR - ABSOLUTE */
        .long   0x00000000                 /* not used */
        .long   0x00000000                 /* not used - table is exactly 0x38 bytes total */

        .section .text,"ax",@progbits

! Stack must live in genuine writable SDRAM. These were part of the
! original cartridge header block that's now been replaced entirely -
! restoring just these two constants, which are still required.
        .equ    pri_stack, 0x0603F400
        .equ    sec_stack, 0x06040000

pri_vbr:
        .long   pri_start       /* Cold Start PC */
        .long   pri_stack       /* Cold Start SP */
        .long   pri_start       /* Manual Reset PC */
        .long   pri_stack       /* Manual Reset SP */
        .long   pri_err         /* Illegal instruction */
        .long   0x00000000      /* reserved */
        .long   pri_err         /* Invalid slot instruction */
        .long   0x00000000      /* reserved */
        .long   0x00000000      /* reserved */
        .long   pri_err         /* CPU address error */
        .long   pri_err         /* DMA address error */
        .long   pri_err         /* NMI vector */
        .long   pri_err         /* User break vector */
        .space  76              /* reserved */
        .long   pri_err         /* TRAPA #32 */
        .long   pri_err         /* TRAPA #33 */
        .long   pri_err         /* TRAPA #34 */
        .long   pri_err         /* TRAPA #35 */
        .long   pri_err         /* TRAPA #36 */
        .long   pri_err         /* TRAPA #37 */
        .long   pri_err         /* TRAPA #38 */
        .long   pri_err         /* TRAPA #39 */
        .long   pri_err         /* TRAPA #40 */
        .long   pri_err         /* TRAPA #41 */
        .long   pri_err         /* TRAPA #42 */
        .long   pri_err         /* TRAPA #43 */
        .long   pri_err         /* TRAPA #44 */
        .long   pri_err         /* TRAPA #45 */
        .long   pri_err         /* TRAPA #46 */
        .long   pri_err         /* TRAPA #47 */
        .long   pri_err         /* TRAPA #48 */
        .long   pri_err         /* TRAPA #49 */
        .long   pri_err         /* TRAPA #50 */
        .long   pri_err         /* TRAPA #51 */
        .long   pri_err         /* TRAPA #52 */
        .long   pri_err         /* TRAPA #53 */
        .long   pri_err         /* TRAPA #54 */
        .long   pri_err         /* TRAPA #55 */
        .long   pri_err         /* TRAPA #56 */
        .long   pri_err         /* TRAPA #57 */
        .long   pri_err         /* TRAPA #58 */
        .long   pri_err         /* TRAPA #59 */
        .long   pri_err         /* TRAPA #60 */
        .long   pri_err         /* TRAPA #61 */
        .long   pri_err         /* TRAPA #62 */
        .long   pri_err         /* TRAPA #63 */
        .long   pri_irq         /* FRT interrupt (Level 1) */
        .long   pri_irq         /* WDT interrupt (Level 2 & 3) */
        .long   pri_irq         /* DMA interrupt (Level 4 & 5) */
        .long   pri_irq         /* PWM interupt (Level 6 & 7) */
        .long   pri_irq         /* Command interupt (Level 8 & 9) */
        .long   pri_irq         /* H Blank interupt (Level 10 & 11) */
        .long   pri_irq         /* V Blank interupt (Level 12 & 13) */
        .long   pri_irq         /* Reset Button (Level 14 & 15) */

!-----------------------------------------------------------------------
! Secondary Vector Base Table
!-----------------------------------------------------------------------

sec_vbr:
        .long   sec_start       /* Cold Start PC */
        .long   sec_stack       /* Cold Start SP */
        .long   sec_start       /* Manual Reset PC */
        .long   sec_stack       /* Manual Reset SP */
        .long   sec_err         /* Illegal instruction */
        .long   0x00000000      /* reserved */
        .long   sec_err         /* Invalid slot instruction */
        .long   0x00000000      /* reserved */
        .long   0x00000000      /* reserved */
        .long   sec_err         /* CPU address error */
        .long   sec_err         /* DMA address error */
        .long   sec_err         /* NMI vector */
        .long   sec_err         /* User break vector */
        .space  76              /* reserved */
        .long   sec_err         /* TRAPA #32 */
        .long   sec_err         /* TRAPA #33 */
        .long   sec_err         /* TRAPA #34 */
        .long   sec_err         /* TRAPA #35 */
        .long   sec_err         /* TRAPA #36 */
        .long   sec_err         /* TRAPA #37 */
        .long   sec_err         /* TRAPA #38 */
        .long   sec_err         /* TRAPA #39 */
        .long   sec_err         /* TRAPA #40 */
        .long   sec_err         /* TRAPA #41 */
        .long   sec_err         /* TRAPA #42 */
        .long   sec_err         /* TRAPA #43 */
        .long   sec_err         /* TRAPA #44 */
        .long   sec_err         /* TRAPA #45 */
        .long   sec_err         /* TRAPA #46 */
        .long   sec_err         /* TRAPA #47 */
        .long   sec_err         /* TRAPA #48 */
        .long   sec_err         /* TRAPA #49 */
        .long   sec_err         /* TRAPA #50 */
        .long   sec_err         /* TRAPA #51 */
        .long   sec_err         /* TRAPA #52 */
        .long   sec_err         /* TRAPA #53 */
        .long   sec_err         /* TRAPA #54 */
        .long   sec_err         /* TRAPA #55 */
        .long   sec_err         /* TRAPA #56 */
        .long   sec_err         /* TRAPA #57 */
        .long   sec_err         /* TRAPA #58 */
        .long   sec_err         /* TRAPA #59 */
        .long   sec_err         /* TRAPA #60 */
        .long   sec_err         /* TRAPA #61 */
        .long   sec_err         /* TRAPA #62 */
        .long   sec_err         /* TRAPA #63 */
        .long   sec_irq         /* FRT interrupt (Level 1) */
        .long   sec_irq         /* WDT interrupt (Level 2 & 3) */
        .long   sec_irq         /* DMA interrupt (Level 4 & 5) */
        .long   sec_irq         /* PWM interupt (Level 6 & 7) */
        .long   sec_irq         /* Command interupt (Level 8 & 9) */
        .long   sec_irq         /* H Blank interupt (Level 10 & 11 */
        .long   sec_irq         /* V Blank interupt (Level 12 & 13) */
        .long   sec_irq         /* Reset Button (Level 14 & 15) */

!-----------------------------------------------------------------------
! The Primary SH2 starts here
!-----------------------------------------------------------------------

pri_start:
        ! DIAGNOSTIC CHECKPOINT 1: pri_start entered at all
        mov.l   _ckpt_addr,r0
        mov     #1,r1
        mov.l   r1,@r0

        ! clear interrupt flags
        mov.l   _pri_int_clr,r1
        mov.w   r0,@-r1                 /* PWM INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* CMD INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* H INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* V INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* VRES INT clear */
        mov.w   r0,@r1

        mov.l   _pri_sh2_frtctl,r1      /* Set Free Run Timer */
        mov     #0x00,r0
        mov.b   r0,@(0x00,r1)           /* TIER = ints disabled */
        mov     #0xE2,r0
        mov.b   r0,@(0x07,r1)           /* TOCR = select OCRA, output 1 on compare match */
        mov     #0x00,r0
        mov.b   r0,@(0x04,r1)           /* OCR_H */
        mov     #0x01,r0
        mov.b   r0,@(0x05,r1)           /* OCR_L => OCRA = 0x0001 */
        mov     #0,r0
        mov.b   r0,@(0x06,r1)           /* TCR = input captured on falling edge, CKS = Fs/8 */
        mov     #1,r0
        mov.b   r0,@(0x01,r1)           /* TCSR = clear FRC on match OCRA */
        mov     #0x00,r0
        mov.b   r0,@(0x03,r1)           /* FRC_L */
        mov.b   r0,@(0x02,r1)           /* FRC_H => clear FRC */

        mov.l   _pri_stk,r15

        ! purge cache and turn it off
        mov.l   _pri_cctl,r0
        mov     #0x10,r1                /* CP = cache purge, /CE = cache disabled */
        mov.b   r1,@r0

        ! DIAGNOSTIC CHECKPOINT 2: int/FRT setup done
        mov.l   _ckpt_addr,r0
        mov     #2,r1
        mov.l   r1,@r0

        ! clear bss
        mov     #0,r0
        mov.l   _bss_dst,r1
        mov.l   _bss_end,r2
0:
        mov.b   r0,@r1
        add     #1,r1
        cmp/eq  r1,r2
        bf      0b

        ! wait for 68000 to finish init
        mov.l   _pri_sts,r0
        mov.l   _pri_ok,r1
1:
        mov.l   @r0,r2
        nop
        nop
        cmp/eq  r1,r2
        bt      1b

        ! let Secondary SH2 run
        mov     #0,r1
        mov.l   r1,@(4,r0)              /* clear secondary status */

        mov     #0x80,r0
        mov.l   _pri_adapter,r1
        mov.b   r0,@r1                  /* set FM */
        mov     #0x0A,r0                /* vbi and cmd enabled */
        mov.b   r0,@(1,r1)              /* set int enables */
        mov     #0x10,r0
        ldc     r0,sr                   /* allow ints */

        ! purge cache, turn it on, and run main()
        mov.l   _pri_cctl,r0
        mov     #0x11,r1                /* CP = cache purge, CE = cache enabled */
        mov.b   r1,@r0

        ! DIAGNOSTIC CHECKPOINT 3: about to jump into main()
        mov.l   _ckpt_addr,r0
        mov     #3,r1
        mov.l   r1,@r0

        mov.l   _pri_go,r0
        jmp     @r0
        nop

        .align   2
_ckpt_addr:
        .long   0x20004028               /* COMM8 - unused until file streaming starts */
_pri_int_clr:
        .long   0x2000401E              /* one word passed last int clr reg */
_pri_stk:
        .long   pri_stack               /* Cold Start SP */
_pri_sts:
        .long   0x20004020
_pri_sh2_frtctl:
        .long   0xfffffe10
_pri_ok:
        .ascii  "M_OK"
_pri_adapter:
        .long   0x20004000
_pri_cctl:
        .long   0xFFFFFE92
_pri_go:
        .long   _main

_bss_dst:
        .long   __bss_start
_bss_end:
        .long   __bss_end

!-----------------------------------------------------------------------
! Primary exception handler
!-----------------------------------------------------------------------

pri_err:
        rte
        nop

!-----------------------------------------------------------------------
! Primary IRQ handler
!-----------------------------------------------------------------------

pri_irq:
        mov.l   r0,@-r15
        mov.l   r1,@-r15
        mov.l   r2,@-r15

        stc     sr,r1                   /* SR holds IRQ level in I3-I0 */
        mov     #0x10,r2
        or      r2,r1                   /* IRQ level for bumped irq */
        mov.w   p_int_off,r2
        ldc     r2,sr                   /* disallow ints */

        mov.l   p_sys_frt_tocr,r2
        mov     #0xE0,r0                /* TOCR = select OCRA, output 0 on compare match */
        mov.b   r0,@r2
        mov.b   @r2,r0

        sts.l   pr,@-r15
        mov     r1,r0
        shlr2   r0
        and     #0x3C,r0                /* int level to table offset */
        mov.l   p_int_jtable,r2
        mov.l   @(r0,r2),r0
        jsr     @r0
        ldc     r1,sr                   /* restore IRQ level */

        lds.l   @r15+,pr
        mov.l   @r15+,r2
        mov.l   @r15+,r1
        mov.l   @r15+,r0
        rte
        nop

        .align  2
p_sys_frt_tocr:
        .long   0xFFFFFE17
p_int_jtable:
        .long   _p_int_jtable
p_int_off:
        .word   0x00F0

        .align  4
_p_int_jtable:
        .long   pri_no_irq              /* level 0 (ILL) */
        .long   pri_no_irq              /* level 1 (FRT) */
        .long   pri_wdt_irq             /* level 2 (WDT) */
        .long   pri_wdt_irq             /* level 3 (WDT) */
        .long   pri_dma_irq             /* level 4 (DMA) */
        .long   pri_dma_irq             /* level 5 (DMA) */
        .long   pri_pwm_irq             /* level 6 (PWM) */
        .long   pri_pwm_irq             /* level 7 (PWM) */
        .long   pri_cmd_irq             /* level 8 (CMD) */
        .long   pri_cmd_irq             /* level 9 (CMD) */
        .long   pri_h_irq               /* level 10 (HBI) */
        .long   pri_h_irq               /* level 11 (HBI) */
        .long   pri_v_irq               /* level 12 (VBI) */
        .long   pri_v_irq               /* level 13 (VBI) */
        .long   pri_vres_irq            /* level 14 (VRES) */
        .long   pri_vres_irq            /* level 15 (VRES) */

!-----------------------------------------------------------------------
! Primary No IRQ handler
!-----------------------------------------------------------------------

pri_no_irq:
        rts
        nop

!-----------------------------------------------------------------------
! Primary V Blank IRQ handler
!-----------------------------------------------------------------------

pri_v_irq:
        ! bump ints if necessary
        mov.l   pvi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   pvi_mars_adapter,r1
        mov.w   r0,@(0x16,r1)           /* clear V IRQ */

        ! handle V IRQ - save registers
        sts.l   pr,@-r15
        mov.l   r3,@-r15
        mov.l   r4,@-r15
        mov.l   r5,@-r15
        mov.l   r6,@-r15
        mov.l   r7,@-r15
        sts.l   mach,@-r15
        sts.l   macl,@-r15

        mov.l   pvbi_handler_ptr,r0
        jsr     @r0
        nop

        ! restore registers
        lds.l   @r15+,macl
        lds.l   @r15+,mach
        mov.l   @r15+,r7
        mov.l   @r15+,r6
        mov.l   @r15+,r5
        mov.l   @r15+,r4
        mov.l   @r15+,r3
        lds.l   @r15+,pr
        rts
        nop

        .align  2
pvi_mars_adapter:
        .long   0x20004000
pvbi_handler_ptr:
        .long   _pri_vbi_handler
pvi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Primary H Blank IRQ handler
!-----------------------------------------------------------------------

pri_h_irq:
        ! bump ints if necessary
        mov.l   phi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   phi_mars_adapter,r1
        mov.w   r0,@(0x18,r1)           /* clear H IRQ */
        nop
        nop
        nop
        nop

        ! handle H IRQ (remove nops if more than 8 cycles)

        rts
        nop

        .align  2
phi_mars_adapter:
        .long   0x20004000
phi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Primary Command IRQ handler
!-----------------------------------------------------------------------

pri_cmd_irq:
        ! bump ints if necessary
        mov.l   pci_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   pci_mars_adapter,r1
        mov.w   r0,@(0x1A,r1)           /* clear CMD IRQ */

        ! handle wait in sdram
        mov.l   pci_cmd_comm0,r1
        mov.w   @r1,r0
        mov.l   r0,@-r15                /* save COMM0 reg */
        mov.w   @(2,r1),r0
        mov.l   r0,@-r15                /* save COMM2 regs */
        mov.w   pci_cmd_resp,r0
        mov.w   r0,@r1                  /* respond to m68k */
0:
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bt      0b                      /* wait for command from m68k */

        mov.w   pci_cmd_exit,r0
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bf      3f                      /* not an exit command - call general handler */

        mov.l   @r15+,r0
        #mov.w   r0,@(2,r1)             /* do NOT restore COMM2 reg to avoid stomping */
                                        /* on the value the m68k might have written in */
                                        /* its command handler further up the call chain */
        mov.l   @r15+,r0
        mov.w   r0,@r1                  /* restore COMM0 reg */

        rts
        nop
3:
        ! handle general CMD IRQ
        sts.l   pr,@-r15
        mov.l   r3,@-r15
        mov.l   r4,@-r15
        mov.l   r5,@-r15
        mov.l   r6,@-r15
        mov.l   r7,@-r15
        sts.l   mach,@-r15
        sts.l   macl,@-r15

        mov.l   pci_cmd_handler,r0
        jsr     @r0
        nop

        mov.w   pci_cmd_resp,r0
        mov.l   pci_cmd_comm0,r1
        mov.w   r0,@r1
4:
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bt      4b                      /* handshake with m68k */

        ! restore registers
        lds.l   @r15+,macl
        lds.l   @r15+,mach
        mov.l   @r15+,r7
        mov.l   @r15+,r6
        mov.l   @r15+,r5
        mov.l   @r15+,r4
        mov.l   @r15+,r3
        lds.l   @r15+,pr

        mov.l   @r15+,r0
        mov.w   r0,@(2,r1)              /* restore COMM2 reg */
        mov.l   @r15+,r0
        mov.w   r0,@r1                  /* restore COMM0 reg */

        rts
        nop

        .align  2
pci_mars_adapter:
        .long   0x20004000
pci_sh2_frtctl:
        .long   0xfffffe10
pci_cmd_handler:
        .long   _pri_cmd_handler

pci_cmd_comm0:
        .long   0x20004020
pci_cmd_resp:
        .word   0xA55A
pci_cmd_exit:
        .word   0xFFFE

!-----------------------------------------------------------------------
! Primary PWM IRQ handler
!-----------------------------------------------------------------------

pri_pwm_irq:
        ! bump ints if necessary
        mov.l   ppi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   ppi_mars_adapter,r1
        mov.w   r0,@(0x1C,r1)           /* clear PWM IRQ */
        nop
        nop
        nop
        nop

        ! handle PWM IRQ (remove nops if more than 8 cycles)

        rts
        nop

        .align  2
ppi_mars_adapter:
        .long   0x20004000
ppi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Primary DMA IRQ handler
!-----------------------------------------------------------------------

pri_dma_irq:
        ! bump ints if necessary
        mov.l   pdi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        ! handle DMA IRQ

        rts
        nop

        .align  2
pdi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Primary WDT IRQ handler
!-----------------------------------------------------------------------

pri_wdt_irq:
        ! bump ints if necessary
        mov.l   pwi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   pwi_sh2_wdtctl,r1
        mov.b   @r1,r0                  /* read WTCSR */
        tst     #0x80,r0                /* check OVF */
        bt      1f                      /* no overflow */
        mov.w   pwi_clr_ovf,r0
        mov.w   r0,@r1                  /* clear OVF */

        ! handle WDT overflow
        mov.l   pwi_ovf_count,r1
        mov.l   @r1,r0
        add     #1,r0
        mov.l   r0,@r1
1:
        rts
        nop

        .align  2
pwi_sh2_frtctl:
        .long   0xfffffe10
pwi_sh2_wdtctl:
        .long   0xfffffe80
pwi_ovf_count:
        .long   _mars_pwdt_ovf_count
pwi_clr_ovf:
        .word   0xa53e                  /* A5 = sel WTCSR, 3E = clr OVF, IT mode, timer enabled, clksel = Fs/4096 */

!-----------------------------------------------------------------------
! Primary RESET IRQ handler
!-----------------------------------------------------------------------

pri_vres_irq:
        mov.l   pvri_mars_adapter,r1
        mov.w   r0,@(0x14,r1)           /* clear VRES IRQ */

        mov     #0x0D,r0                /* prevent all normal ints, but not reset */
        shll2   r0
        shll2   r0
        ldc     r0,sr                   /* disallow ints */

        mov.l   pvri_pri_stk,r15
        mov.l   pvri_pri_vres,r0
        jmp     @r0
        nop

        .align  2
pvri_mars_adapter:
        .long   0x20004000
pvri_pri_stk:
        .long   pri_stack               /* Cold Start SP */
pvri_pri_vres:
        .long   pri_reset

!-----------------------------------------------------------------------
! The Secondary SH2 starts here
!-----------------------------------------------------------------------

sec_start:
        ! clear interrupt flags
        mov.l   _sec_int_clr,r1
        mov.w   r0,@-r1                 /* PWM INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* CMD INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* H INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* V INT clear */
        mov.w   r0,@r1
        mov.w   r0,@-r1                 /* VRES INT clear */
        mov.w   r0,@r1

        mov.l   _sec_sh2_frtctl,r1      /* Set Free Run Timer */
        mov     #0x00,r0
        mov.b   r0,@(0x00,r1)           /* TIER = ints disabled */
        mov     #0xE2,r0
        mov.b   r0,@(0x07,r1)           /* TOCR = select OCRA, output 1 on compare match */
        mov     #0x00,r0
        mov.b   r0,@(0x04,r1)           /* OCR_H */
        mov     #0x01,r0
        mov.b   r0,@(0x05,r1)           /* OCR_L => OCRA = 0x0001 */
        mov     #0,r0
        mov.b   r0,@(0x06,r1)           /* TCR = input captured on falling edge, CKS = Fs/8 */
        mov     #1,r0
        mov.b   r0,@(0x01,r1)           /* TCSR = clear FRC on match OCRA */
        mov     #0x00,r0
        mov.b   r0,@(0x03,r1)           /* FRC_L */
        mov.b   r0,@(0x02,r1)           /* FRC_H => clear FRC */

        mov.l   _sec_stk,r15

        ! wait for Primary SH2 and 68000 to finish init
        mov.l   _sec_sts,r0
        mov.l   _sec_ok,r1
1:
        mov.l   @r0,r2
        nop
        nop
        cmp/eq  r1,r2
        bt      1b

        mov.l   _sec_adapter,r1
        mov     #0x02,r0                /* cmd enabled */
        mov.b   r0,@(1,r1)              /* set int enables */
        mov     #0x10,r0
        ldc     r0,sr                   /* allow ints */

! purge cache, turn it on, and run secondary()
        mov.l   _sec_cctl,r0
        mov     #0x11,r1                /* CP = cache purge, CE = cache enabled */
        mov.b   r1,@r0

        mov.l   _sec_go,r0
        jmp     @r0
        nop

        .align   2
_sec_int_clr:
        .long   0x2000401E              /* one word passed last int clr reg */
_sec_stk:
        .long   sec_stack               /* Cold Start SP */
_sec_sts:
        .long   0x20004024
_sec_sh2_frtctl:
        .long   0xfffffe10
_sec_ok:
        .ascii  "S_OK"
_sec_adapter:
        .long   0x20004000
_sec_cctl:
        .long   0xFFFFFE92
_sec_go:
        .long   _secondary

!-----------------------------------------------------------------------
! Secondary exception handler
!-----------------------------------------------------------------------

sec_err:
        rte
        nop

!-----------------------------------------------------------------------
! Secondary IRQ handler
!-----------------------------------------------------------------------

sec_irq:
        mov.l   r0,@-r15
        mov.l   r1,@-r15
        mov.l   r2,@-r15

        stc     sr,r1                   /* SR holds IRQ level in I3-I0 */
        mov     #0x10,r2
        or      r2,r1                   /* IRQ level for bumped irq */
        mov.w   s_int_off,r2
        ldc     r2,sr                   /* disallow ints */

        mov.l   s_sys_frt_tocr,r2
        mov     #0xE0,r0                /* TOCR = select OCRA, output 0 on compare match */
        mov.b   r0,@r2
        mov.b   @r2,r0

        sts.l   pr,@-r15
        mov     r1,r0
        shlr2   r0
        and     #0x3C,r0                /* int level to table offset */
        mov.l   s_int_jtable,r2
        mov.l   @(r0,r2),r0
        jsr     @r0
        ldc     r1,sr                   /* restore IRQ level */

        lds.l   @r15+,pr
        mov.l   @r15+,r2
        mov.l   @r15+,r1
        mov.l   @r15+,r0
        rte
        nop

        .align  2
s_sys_frt_tocr:
        .long   0xFFFFFE17
s_int_jtable:
        .long   _s_int_jtable
s_int_off:
        .word   0x00F0

        .align  4
_s_int_jtable:
        .long   sec_no_irq              /* level 0 (ILL) */
        .long   sec_no_irq              /* level 1 (FRT) */
        .long   sec_wdt_irq             /* level 2 (WDT) */
        .long   sec_wdt_irq             /* level 3 (WDT) */
        .long   sec_dma_irq             /* level 4 (DMA) */
        .long   sec_dma_irq             /* level 5 (DMA) */
        .long   sec_pwm_irq             /* level 6 (PWM) */
        .long   sec_pwm_irq             /* level 7 (PWM) */
        .long   sec_cmd_irq             /* level 8 (CMD) */
        .long   sec_cmd_irq             /* level 9 (CMD) */
        .long   sec_h_irq               /* level 10 (HBI) */
        .long   sec_h_irq               /* level 11 (HBI) */
        .long   sec_v_irq               /* level 12 (VBI) */
        .long   sec_v_irq               /* level 13 (VBI) */
        .long   sec_vres_irq            /* level 14 (VRES) */
        .long   sec_vres_irq            /* level 15 (VRES) */

!-----------------------------------------------------------------------
! Secondary No IRQ handler
!-----------------------------------------------------------------------

sec_no_irq:
        rts
        nop

!-----------------------------------------------------------------------
! Secondary V Blank IRQ handler
!-----------------------------------------------------------------------

sec_v_irq:
        ! bump ints if necessary
        mov.l   svi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   svi_mars_adapter,r1
        mov.w   r0,@(0x16,r1)           /* clear V IRQ */
        nop
        nop
        nop
        nop

        ! handle V IRQ (remove nops if more than 8 cycles)

        rts
        nop

        .align  2
svi_mars_adapter:
        .long   0x20004000
svi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Secondary H Blank IRQ handler
!-----------------------------------------------------------------------

sec_h_irq:
        ! bump ints if necessary
        mov.l   shi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   shi_mars_adapter,r1
        mov.w   r0,@(0x18,r1)           /* clear H IRQ */
        nop
        nop
        nop
        nop

        ! handle H IRQ (remove nops if more than 8 cycles)

        rts
        nop

        .align  2
shi_mars_adapter:
        .long   0x20004000
shi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Secondary Command IRQ handler
!-----------------------------------------------------------------------

sec_cmd_irq:
        ! bump ints if necessary
        mov.l   sci_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   sci_mars_adapter,r1
        mov.w   r0,@(0x1A,r1)           /* clear CMD IRQ */

        ! handle wait in sdram
        mov.l   sci_cmd_comm4,r1
        mov.w   @r1,r0
        mov.l   r0,@-r15                /* save COMM4 reg */
        mov.w   sci_cmd_resp,r0
        mov.w   r0,@r1                  /* respond to m68k */
0:
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bt      0b                      /* wait for command from m68k */

        mov.w   sci_cmd_exit,r0
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bf      3f                      /* not an exit command - call general handler */

        mov.l   @r15+,r0
        mov.w   r0,@r1                  /* restore COMM4 reg */
        
        rts
        nop
3:
        ! handle general CMD IRQ
        sts.l   pr,@-r15
        mov.l   r3,@-r15
        mov.l   r4,@-r15
        mov.l   r5,@-r15
        mov.l   r6,@-r15
        mov.l   r7,@-r15
        sts.l   mach,@-r15
        sts.l   macl,@-r15

        mov.l   sci_cmd_handler,r0
        jsr     @r0
        nop

        mov.w   sci_cmd_resp,r0
        mov.l   sci_cmd_comm4,r1
        mov.w   r0,@r1
4:
        mov.w   @r1,r2
        cmp/eq  r2,r0
        bt      4b                      /* handshake with m68k */

        ! restore registers
        lds.l   @r15+,macl
        lds.l   @r15+,mach
        mov.l   @r15+,r7
        mov.l   @r15+,r6
        mov.l   @r15+,r5
        mov.l   @r15+,r4
        mov.l   @r15+,r3
        lds.l   @r15+,pr

        mov.l   @r15+,r0
        mov.w   r0,@r1                  /* restore COMM4 reg */

        rts
        nop

        .align  2
sci_mars_adapter:
        .long   0x20004000
sci_sh2_frtctl:
        .long   0xfffffe10
sci_cmd_handler:
        .long   _sec_cmd_handler

sci_cmd_comm4:
        .long   0x20004024
sci_cmd_resp:
        .word   0xA55A
sci_cmd_exit:
        .word   0xFFFE

!-----------------------------------------------------------------------
! Secondary PWM IRQ handler
!-----------------------------------------------------------------------

sec_pwm_irq:
        ! bump ints if necessary
        mov.l   spi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   spi_mars_adapter,r1
        mov.w   r0,@(0x1C,r1)           /* clear PWM IRQ */
        nop
        nop
        nop
        nop

        ! handle PWM IRQ (remove nops if more than 8 cycles)

        rts
        nop

        .align  2
spi_mars_adapter:
        .long   0x20004000
spi_sh2_frtctl:
        .long   0xfffffe10

!-----------------------------------------------------------------------
! Secondary DMA IRQ handler
!-----------------------------------------------------------------------

sec_dma_irq:
        ! bump ints if necessary
        mov.l   sdi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        ! handle DMA IRQ
        sts.l   pr,@-r15
        mov.l   r3,@-r15
        mov.l   r4,@-r15
        mov.l   r5,@-r15
        mov.l   r6,@-r15
        mov.l   r7,@-r15
        sts.l   mach,@-r15
        sts.l   macl,@-r15

        mov.l   sdi_dma_handler,r0
        jsr     @r0
        nop

        ! restore registers
        lds.l   @r15+,macl
        lds.l   @r15+,mach
        mov.l   @r15+,r7
        mov.l   @r15+,r6
        mov.l   @r15+,r5
        mov.l   @r15+,r4
        mov.l   @r15+,r3
        lds.l   @r15+,pr

        rts
        nop

        .align  2
sdi_sh2_frtctl:
        .long   0xfffffe10
sdi_dma_handler:
        .long   _sec_dma1_handler

!-----------------------------------------------------------------------
! Secondary WDT IRQ handler
!-----------------------------------------------------------------------

sec_wdt_irq:
        ! bump ints if necessary
        mov.l   swi_sh2_frtctl,r1
        mov     #0xE2,r0                /* TOCR = select OCRA, output 1 on compare match */
        mov.b   r0,@(0x07,r1)           /* write TOCR */
        mov.b   @(0x07,r1),r0           /* read TOCR */

        mov.l   swi_sh2_wdtctl,r1
        mov.b   @r1,r0                  /* read WTCSR */
        tst     #0x80,r0                /* check OVF */
        bt      1f                      /* no overflow */
        mov.w   swi_clr_ovf,r0
        mov.w   r0,@r1                  /* clear OVF */

        ! handle WDT overflow
        mov.l   swi_ovf_count,r1
        mov.l   @r1,r0
        add     #1,r0
        mov.l   r0,@r1
1:
        rts
        nop

        .align  2
swi_sh2_frtctl:
        .long   0xfffffe10
swi_sh2_wdtctl:
        .long   0xfffffe80
swi_ovf_count:
        .long   _mars_swdt_ovf_count
swi_clr_ovf:
        .word   0xa53e                  /* A5 = sel WTCSR, 3E = clr OVF, IT mode, timer enabled, clksel = Fs/4096 */

!-----------------------------------------------------------------------
! Secondary RESET IRQ handler
!-----------------------------------------------------------------------

sec_vres_irq:
        mov.l   svri_mars_adapter,r1
        mov.w   r0,@(0x14,r1)           /* clear VRES IRQ */

        mov     #0x0D,r0                /* prevent all normal ints, but not reset */
        shll2   r0
        shll2   r0
        ldc     r0,sr                   /* disallow ints */

        mov.l   svri_sec_stk,r15
        mov.l   svri_sec_vres,r0
        jmp     @r0
        nop

        .align  2
svri_mars_adapter:
        .long   0x20004000
svri_sec_stk:
        .long   sec_stack               /* Cold Start SP */
svri_sec_vres:
        .long   sec_reset


!-----------------------------------------------------------------------
!-----------------------------------------------------------------------
! Support Functions
!-----------------------------------------------------------------------
!-----------------------------------------------------------------------

! void fast_memcpy(int *dst, int *src, int len);
! Fast memcpy function - copies longs, runs from sdram for speed
! On entry: r4 = dst, r5 = src, r6 = len (in longs)

        .align  4
        .global _fast_memcpy
_fast_memcpy:
        mov.l   @r5+,r3
        mov.l   r3,@r4
        dt      r6
        bf/s    _fast_memcpy
        add     #4,r4
        rts
        nop


! void CacheControl(int mode);
! Cache control function
! On entry: r4 = cache mode => 0x10 = CP, 0x08 = TW, 0x01 = CE

        .align  4
        .global _CacheControl
_CacheControl:
        mov.l   _sh2_cctl,r0
        mov.b   r4,@r0
        rts
        nop

        .align  2

_sh2_cctl:
        .long   0xFFFFFE92


! int SetSH2SR(int level);
! On entry: r4 = new irq level
! On exit:  r0 = old irq level
        .align  4
        .global _SetSH2SR
_SetSH2SR:
        stc     sr,r1
        mov     #0x0F,r0
        shll2   r0
        shll2   r0
        and     r0,r1                   /* just the irq mask */
        shlr2   r1
        shlr2   r1
        not     r0,r0
        stc     sr,r2
        and     r0,r2
        shll2   r4
        shll2   r4
        or      r4,r2
        ldc     r2,sr
        rts
        mov     r1,r0

!-----------------------------------------------------------------------
! Primary and Secondary RESET code
!-----------------------------------------------------------------------

        .align  2

        .text

pri_reset:
        ! do any primary SH2 specific reset code here

        mov.l   sec_st,r0
        mov.l   sec_ok,r1
0:
        mov.l   @r0,r2
        nop
        nop
        cmp/eq  r1,r2
        bf      0b                      /* wait for secondary sh2 */

        ! recopy rom data to sdram
        mov.l   rom_header,r1
        mov.l   @r1,r2                  /* src relative to start of rom */
        mov.l   @(4,r1),r3              /* dst relative to start of sdram */
        mov.l   @(8,r1),r4              /* size (longword aligned) */
        mov.l   rom_start,r1
        add     r1,r2
        mov.l   sdram_start,r1
        add     r1,r3
        shlr2   r4                      /* number of longs */
        add     #-1,r4
1:
        mov.l   @r2+,r0
        mov.l   r0,@r3
        add     #4,r3
        dt      r4
        bf      1b

        mov.l   pri_st,r0
        mov.l   pri_ok,r1
        mov.l   r1,@r0                  /* tell everyone reset complete */

        mov.l   pri_go,r0
        jmp     @r0
        nop

sec_reset:
        ! do any secondary SH2 specific reset code here

        mov.l   sec_st,r0
        mov.l   sec_ok,r1
        mov.l   r1,@r0                  /* tell primary to start reset */

        mov.l   pri_st,r0
        mov.l   pri_ok,r1
0:
        mov.l   @r0,r2
        nop
        nop
        cmp/eq  r1,r2
        bf      0b                      /* wait for primary to do the work */

        mov.l   sec_go,r0
        jmp     @r0
        nop

        .align  2
pri_st:
        .long   0x20004020
pri_ok:
        .ascii  "M_OK"
pri_go:
        .long   pri_start
rom_header:
        .long   0x220003D4
rom_start:
        .long   0x22000000
sdram_start:
        .long   0x26000000

sec_st:
        .long   0x20004024
sec_ok:
        .ascii  "S_OK"
sec_go:
        .long   sec_start


! this suppresses a warning in the linker about missing start()

        .global _start
_start:
