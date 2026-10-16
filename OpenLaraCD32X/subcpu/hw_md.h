#ifndef _HW_MD_H
#define _HW_MD_H

#define SEGA_CTRL_BUTTONS   0x0FFF
#define SEGA_CTRL_UP        0x0001
#define SEGA_CTRL_DOWN      0x0002
#define SEGA_CTRL_LEFT      0x0004
#define SEGA_CTRL_RIGHT     0x0008
#define SEGA_CTRL_B         0x0010
#define SEGA_CTRL_C         0x0020
#define SEGA_CTRL_A         0x0040
#define SEGA_CTRL_START     0x0080
#define SEGA_CTRL_Z         0x0100
#define SEGA_CTRL_Y         0x0200
#define SEGA_CTRL_X         0x0400
#define SEGA_CTRL_MODE      0x0800

#define SEGA_CTRL_TYPE      0xF000
#define SEGA_CTRL_THREE     0x0000
#define SEGA_CTRL_SIX       0x1000
#define SEGA_CTRL_NONE      0xF000

#define TEXT_WHITE          0x0000
#define TEXT_GREEN          0x2000
#define TEXT_RED            0x4000

#define Z80_BUS_REQUEST     0x0100
#define Z80_BUS_RELEASE     0x0000
#define Z80_ASSERT_RESET    0x0000
#define Z80_CLEAR_RESET     0x0100

#define GET_PAD(p) (*(volatile unsigned short *)(0xFF8018 + p*2))
#define GET_TICKS (*(volatile unsigned int *)0xFF801C)

enum {
    MD_CMD_INIT_HW = 1,
    MD_CMD_SET_SR,
    MD_CMD_GET_PAD,
    MD_CMD_CLEAR_B,
    MD_CMD_SET_VRAM,
    MD_CMD_NEXT_VRAM,
    MD_CMD_COPY_VRAM,
    MD_CMD_CLEAR_A,
    MD_CMD_PUT_STR,
    MD_CMD_PUT_CHR,
    MD_CMD_DELAY,
    MD_CMD_SET_PALETTE,
    MD_CMD_Z80_BUSREQUEST,
    MD_CMD_Z80_RESET,
    MD_CMD_Z80_MEMCLR,
    MD_CMD_Z80_MEMCPY,
    MD_CMD_DMA_SCREEN,
    MD_CMD_INIT_32X,
    MD_CMD_LOAD_SDRAM,
    MD_CMD_START_32X,
    MD_CMD_END
};
// --- FIXED POINT MATH MACROS ---
#define FIX_SHIFT 16
#define INT_TO_FIX(x)   ((x) << FIX_SHIFT)
#define FLOAT_TO_FIX(x) ((int)((x) * 65536.0f))
#define FIX_TO_INT(x)   ((x) >> FIX_SHIFT)
#define FIX_MUL(x, y)   (int)(((long long)(x) * (y)) >> FIX_SHIFT)
// -------------------------------
#ifdef __cplusplus
extern "C" {
#endif

extern int do_md_cmd0(int cmd);
extern int do_md_cmd1(int cmd, int arg1);
extern int do_md_cmd2(int cmd, int arg1, int arg2);
extern int do_md_cmd3(int cmd, int arg1, int arg2, int arg3);
extern int do_md_cmd4(int cmd, int arg1, int arg2, int arg3, int arg4);
extern void send_md_cmd0(int cmd);
extern void send_md_cmd1(int cmd, int arg1);
extern void send_md_cmd2(int cmd, int arg1, int arg2);
extern void send_md_cmd3(int cmd, int arg1, int arg2, int arg3);
extern void send_md_cmd4(int cmd, int arg1, int arg2, int arg3, int arg4);
extern int wait_md_cmd(void);
extern void switch_banks(void);

#ifdef __cplusplus
}
#endif

#endif