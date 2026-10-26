#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw_md.h"
#include "files.h"

uint16_t gen_lvl2 = 0;

void write_byte(unsigned int dst, unsigned char val) { *(volatile unsigned char *)dst = val; }
void write_word(unsigned int dst, unsigned short val) { *(volatile unsigned short *)dst = val; }
void write_long(unsigned int dst, unsigned int val) { *(volatile unsigned int *)dst = val; }
unsigned char read_byte(unsigned int src) { gen_lvl2++; return *(volatile unsigned char *)src; }
unsigned short read_word(unsigned int src) { return *(volatile unsigned short *)src; }
unsigned int read_long(unsigned int src) { return *(volatile unsigned int *)src; }
void scd_init_pcm(void) {}
void bump_fm(void) { gen_lvl2++; }

int mystrlen(const char* string) {
    int rc = 0;
    while (*(string++)) rc++;
    return rc;
}

extern int sh2_app_length;
extern char sh2_app_start[];
extern char Sub_Start[];
extern char Sub_End[];
extern uint16_t InitCD(void);
extern volatile int scd_last_timeout_code;

extern int64_t scd_open_file(const char *name);
extern void scd_read_sectors(void *ptr, int lba, int len, void (*wait)(void));
extern void scd_stream_cmd(char cmd); 
extern void scd_sub_read(void *ptr, int lba, int len);
extern int  scd_cd_status(void);                           
extern int  scd_track_info(int track);                     
extern void scd_cdda_play(int track);                       
extern int  scd_bios_call(int fn, int d1, void *a0);       

/* STREAM-1 (OL_STREAM, Makefile STREAM=1, the default; STREAM=0 builds this file as before).
   From OpenJazz R7 / Hurrican P2b.  The chunks of a file (CMD 46 / 47) come from a drive that
   keeps reading: cd_stream_read starts it once (ROMREADN to the end of the file) and takes the
   sectors as they come, checking each one's header; a missed or wrong sector starts it again at
   the sector wanted.  Before, every 32 KB chunk was a CDCSTOP + ROMREADN (a seek each time).
   Anything else that uses the drive (other reads, opening a file, the CD audio, any BIOS call)
   stops the stream first (the macros below), so it starts afresh. */
#ifndef OL_STREAM
#define OL_STREAM 0
#endif
#ifndef OL_FMVRING
#define OL_FMVRING 0            /* FMV-2: CMD 46 ring mode for streamed movies (Makefile FMVRING) */
#endif
#ifndef OL_CARTW
#define OL_CARTW 0              /* CARTW-1: CMD 46 copies to the cart in words (Makefile CARTW) */
#endif
#ifndef OL_UB
#define OL_UB 0                 /* UB-1: the Unfinished Business level files (Makefile UB) */
#endif
#ifndef OL_SLOTS
#define OL_SLOTS 0              /* SLOTS-1: CMD 61 also writes the save slot area (Makefile SLOTS) */
#endif
#if OL_STREAM
extern void scd_stream_start(int lba, int count);
extern int  scd_stream_sector(void *buf, void *hdr, int spins);
/*   scd_stream_start(lba, count)        CDCSTOP, then ROMREADN for count sectors from lba
     scd_stream_sector(buf, hdr, spins)  the next sector into buf (2048 bytes), its header (minute,
                                         second, frame in BCD, mode) into hdr; 0 done, -1 the
                                         transfer failed, -2 no sector after spins looks */
__asm__(
    "       .pushsection .text\n"
    "       .align  2\n"
    "       .globl  scd_stream_start\n"
    "scd_stream_start:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       move.w  #0x0089,%d0\n"            /* CDCSTOP */
    "       jsr     0x5F22.w\n"
    "       lea     48(%sp),%a0\n"            /* -> lba, count (the arguments) */
    "       move.w  #0x0020,%d0\n"            /* ROMREADN */
    "       jsr     0x5F22.w\n"
    "       movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .globl  scd_stream_sector\n"
    "scd_stream_sector:\n"
    "       movem.l %d2-%d7/%a2-%a6,-(%sp)\n"
    "       move.l  56(%sp),%d7\n"            /* spins */
    "1:     move.w  #0x008A,%d0\n"            /* CDCSTAT */
    "       jsr     0x5F22.w\n"
    "       bcc.b   2f\n"
    "       subq.l  #1,%d7\n"
    "       bne.b   1b\n"
    "       moveq   #-2,%d0\n"
    "       bra.b   9f\n"
    "2:     andi.w  #0xF8FF,0x8004.w\n"       /* CDC destination: Sub-CPU */
    "       ori.w   #0x0300,0x8004.w\n"
    "3:     move.w  #0x008B,%d0\n"            /* CDCREAD */
    "       jsr     0x5F22.w\n"
    "       bcs.b   3b\n"
    "       movea.l 48(%sp),%a0\n"            /* buf */
    "       movea.l 52(%sp),%a1\n"            /* header */
    "       move.w  #0x008C,%d0\n"            /* CDCTRN */
    "       jsr     0x5F22.w\n"
    "       bcc.b   4f\n"
    "       moveq   #-1,%d0\n"
    "       bra.b   9f\n"
    "4:     move.w  #0x008D,%d0\n"            /* CDCACK */
    "       jsr     0x5F22.w\n"
    "       moveq   #0,%d0\n"
    "9:     movem.l (%sp)+,%d2-%d7/%a2-%a6\n"
    "       rts\n"
    "       .popsection\n"
);
static int st_next = -1, st_end;               /* the drive delivers st_next .. st_end - 1 */
static uint32_t st_restarts, st_low, st_high, st_err;   /* diagnostics */
#define scd_sub_read(p, l, n)  (st_next = -1, scd_sub_read(p, l, n))
#define scd_open_file(n)       (st_next = -1, scd_open_file(n))
#define scd_bios_call(f, d, a) (st_next = -1, scd_bios_call(f, d, a))
static int unbcd(int v) { return (v >> 4) * 10 + (v & 15); }
static void cd_stream_read(uint8_t *dst, int lba, int n, int file_end)
{
    int fails = 0;
    while (n > 0) {
        if (lba != st_next || lba >= st_end) {
            int count = file_end - lba;
            if (count < n) count = n;
            scd_stream_start(lba, count);
            st_next = lba; st_end = lba + count;
            st_restarts++;
        }
        uint8_t hdr[4];
        const int r = scd_stream_sector(dst, hdr, 400000);
        const int got = unbcd(hdr[0]) * 4500 + unbcd(hdr[1]) * 75 + unbcd(hdr[2]) - 150;
        if (r == 0 && got < lba && got >= lba - 64) {    /* an earlier sector (the drive's run-up): skip it */
            st_low++;
            continue;
        }
        if (r != 0 || got != lba) {                      /* failed, or sectors were missed */
            if (r != 0) st_err++; else st_high++;
            st_next = -1;                                /* start again at this sector */
            if (++fails > 64) { scd_sub_read(dst, lba, n); return; }   /* the old way */
            continue;
        }
        st_next++; lba++; n--; dst += 2048;
    }
}
#endif

/* 68K VBlank Tick Macro. Sub-CPU accesses Main CPU Comm Registers via 0xFF8000 */
#define GET_TICKS read_long(0xFF801C)

#define SUB_MAIN_FLAG  0xFF800E
#define SUB_SUB_FLAG   0xFF800F

static void fourcc(char *out, unsigned int v)
{
    int i, ok = 1;
    for (i = 0; i < 4; i++) {
        unsigned char c = (v >> (24 - 8 * i)) & 0xFF;
        if (c < 0x20 || c > 0x7E) ok = 0;
    }
    if (ok) {
        for (i = 0; i < 4; i++) out[i] = (v >> (24 - 8 * i)) & 0xFF;
        out[4] = 0;
    } else {
        sprintf(out, "%08X", v);
    }
}

extern int do_md_cmd0(int cmd);
extern int do_md_cmd1(int cmd, int arg1);
extern int do_md_cmd2(int cmd, int arg1, int arg2);
extern int do_md_cmd3(int cmd, int arg1, int arg2, int arg3);

#if OL_STREAM
/* STREAM-1: read-ahead.  While the 68000 copies a chunk to the cart, and while the Sub-CPU waits
   for the next command, the drive goes on delivering sectors; they would be lost (its buffer
   holds only a few), so cd_pump takes each one as it arrives into st_buf - the start of the
   next chunk.  cd_chunk_read uses what st_buf holds and reads the rest. */
#define ST_RING 32                                   /* sectors (64 KB): two chunks */
static uint8_t st_buf[ST_RING * 2048] __attribute__((aligned(4)));
static int st_buf_lba = -1, st_buf_n, st_buf_pos;      /* sector st_buf_lba is in slot st_buf_pos */
extern int wait_md_cmd(void);
static void cd_pump(void)
{
    if (st_next < 0 || st_buf_lba < 0 || st_buf_n >= ST_RING || st_next >= st_end ||
        st_next != st_buf_lba + st_buf_n)
        return;
    uint8_t hdr[4];
    const int r = scd_stream_sector(st_buf + ((st_buf_pos + st_buf_n) % ST_RING) * 2048, hdr, 1);
    if (r == -2) return;                                 /* no sector yet */
    const int got = unbcd(hdr[0]) * 4500 + unbcd(hdr[1]) * 75 + unbcd(hdr[2]) - 150;
    if (r == 0 && got < st_next && got >= st_next - 64) { st_low++; return; }   /* run-up */
    if (r != 0 || got != st_next) { st_next = -1; return; }                    /* lost: start again when asked */
    st_buf_n++; st_next++;
}
/* every 68000 command from here on keeps the drive's sectors coming (0xFF800E: the 68000 is busy).
   NOT for MD_CMD_DMA_SCREEN (17): send_md_cmd leaves the sub flag set for it and the 68000 waits
   for wait_md_cmd to clear it, so this pumping wait would never end.  main.c does not use it. */
static int md_wait_pumping(void)
{
    while (read_byte(0xFF800E)) cd_pump();
    return wait_md_cmd();
}
static int md_cmd0_p(int c) { send_md_cmd0(c); return md_wait_pumping(); }
static int md_cmd1_p(int c, int a) { send_md_cmd1(c, a); return md_wait_pumping(); }
static int md_cmd2_p(int c, int a, int b) { send_md_cmd2(c, a, b); return md_wait_pumping(); }
static int md_cmd3_p(int c, int a, int b, int d) { send_md_cmd3(c, a, b, d); return md_wait_pumping(); }
static int md_cmd4_p(int c, int a, int b, int d, int e) { send_md_cmd4(c, a, b, d, e); return md_wait_pumping(); }
#define do_md_cmd0 md_cmd0_p
#define do_md_cmd1 md_cmd1_p
#define do_md_cmd2 md_cmd2_p
#define do_md_cmd3 md_cmd3_p
#define do_md_cmd4 md_cmd4_p
static void cd_chunk_read(uint8_t *dst, int lba, int n, int file_end)
{
    int k = 0;
    if (st_buf_lba >= 0 && lba >= st_buf_lba && lba < st_buf_lba + st_buf_n) {
        const int skip = lba - st_buf_lba;               /* sectors read ahead that are not wanted */
        st_buf_pos = (st_buf_pos + skip) % ST_RING; st_buf_lba = lba; st_buf_n -= skip;
        k = st_buf_n < n ? st_buf_n : n;
        for (int i = 0; i < k; i++)
            memcpy(dst + i * 2048, st_buf + ((st_buf_pos + i) % ST_RING) * 2048, 2048);
        st_buf_pos = (st_buf_pos + k) % ST_RING; st_buf_lba += k; st_buf_n -= k;
    }
    if (k < n) {
        cd_stream_read(dst + k * 2048, lba + k, n - k, file_end);
        st_buf_lba = lba + n; st_buf_n = 0; st_buf_pos = 0;   /* the read-ahead goes on from here */
    }
}
#endif

#define MD_CMD_CLEAR_B        4
#define MD_CMD_CLEAR_A        8
#define MD_CMD_READ_32X_16    19
#define MD_CMD_WRITE_32X_16   20
#define MD_CMD_READ_32X_32    21
#define MD_CMD_WRITE_32X_32   22
#define MD_CMD_COPY_FROM_CART 24
#define MD_CMD_COPY_WORDS     25
#define MD_CMD_SAFE_CART_XFER 26   
#define MD_CMD_SAFE_CART_XFER_W 27     
#define MD_CMD_CHILLY_MEMCPY  32         
#define MD_CMD_CHILLY_RV_MEMCPY 33       
#define MD_CMD_BRAM_PUT       28         
#define MD_CMD_BRAM_TO_FB     29         
#define MD_CMD_BRAM_GET       30         
#define MD_CMD_INSTALL_EXC    31         /* HW1: 68K exception trap (K29), prints 68K EXC in red */
#define MD_CMD_SET_VDP_REG    34
#define MD_CMD_START_D32XR    35         /* D32X M1: 68K -> D32XR's _start (never returns) */
/* KOBO K19: hw_md.s set_vdp_reg - one VDP register word */

/* KOBO K19 - GREEN DIAGNOSTIC TEXT.  1 = printed as before (C on the pad shows/hides it).
   0 = print_diag() prints nothing at all: every print_diag() call stays in the code, so
   setting 1 again brings all the text back.  (Chilly's white 32X start-up messages come
   from hw_md.s and are not affected.) */
#ifndef KOBO_DIAG_TEXT
#define KOBO_DIAG_TEXT        0          /* D32X M1: on - the boot steps must be visible (Makefile DIAG=1: on) */
#endif

/* K30 - BOOT TEXT.  0 = the Genesis display is switched OFF at the very start of main(), so
   neither the green text nor Chilly's white 32X start-up messages appear (dev request): the
   SEGA logo is followed by the splash.  The text is still written to video RAM, so setting 1
   (or pressing C in the game) shows it.  Set 1 when debugging a start-up problem. */
#define KOBO_HW_QUIET         0          /* HW9: 1 = once the game reports 4B10, the Sub-CPU stops polling
                                                   the 68K; it only writes its heartbeat twice a second. */
static int sub_quiet = 0;
static int ol_game_running = 0;   /* OL-2b: STATE 4B10 seen - no more per-pass text */
#ifndef KOBO_BOOT_TEXT
#define KOBO_BOOT_TEXT        0          /* K31: on - Chilly's white 32X start-up messages show
                                                   (green text stays off: KOBO_DIAG_TEXT 0) */
#endif

#define LEGACY_68K_CART 0

#ifndef OL_CART_MB
#define OL_CART_MB 0                     /* CART-1: 8 or 16 = cart test build (build.sh CART=8/16) */
#endif
#ifndef OL_CART_ED
#define OL_CART_ED 0                     /* CART-ED: 1 = the Mega EverDrive PRO cart test build (CART=ED) */
#endif
#ifndef OL_EDSAVE
#define OL_EDSAVE 0                      /* EDSAVE-1 (Makefile EDSAVE=1, with CART=ED MODE1=1): the settings
                                            and the save slots in the EverDrive's save bank (bank 31, kept
                                            on the SD card as bram.srm) instead of the cart's PSRAM, which
                                            the EverDrive menu wipes.  The cartridge-boot listener (hw_md.s
                                            MODE1-7, M1SAVE=1) reaches that bank as cart 0x800000. */
#endif
#ifndef OL_EDSAVE_NOVERIFY
#define OL_EDSAVE_NOVERIFY 0
#endif
#if OL_EDSAVE
#define ED_SAVE_68K   0x800000UL         /* EDSAVE-1: the save bank as the 68000 side addresses it */
/* the bank's layout: the 64 KB save area (settings, old single save, records) at 0, the 16 slots at
   0x10000 - the same piece numbers as before (CMD 61: bit 8 = a slot piece) */
static uint32_t ed_save_off(int piece)
{
    return (piece & 0x100) ? 0x10000UL + (uint32_t)(piece & 31) * 4096 : (uint32_t)(piece & 15) * 4096;
}
#endif

static uint32_t saved_lba = 0;
/* K10: length of the file opened by CMD 38, kept here (like saved_lba) so CMD 50 never has
   to trust COMM8 - in K9 CMD 50 read a wrong length from COMM8 and read 0 sectors. */
static uint32_t saved_len = 0;
static int quiet = 0;      
#define SLIDESHOW_DEBUG 1

static const char *find_sh2_marker(const char *p, int len)
{
    int i;
    for (i = 0; i + 8 <= len; i++)
        if (p[i] == 'S' && p[i+1] == 'H' && p[i+2] == '2' && p[i+3] == '-' && p[i+4] == 'V')
            return p + i + 5;
    return 0;
}

#if KOBO_DIAG_TEXT
/* EDSAVE-D: the save commands traced on row 21 (DIAG=1 launcher): T:<cmd> P<piece> S<step> N<count> */
static char *trace_tmp;
static unsigned trace_n57, trace_n58, trace_n61, trace_n62;
static void trace_save(int cmd, int piece, int step)
{
    char tb[48];
    void print_diag(char *temp, const char *msg, int x, int y);
    sprintf(tb, "T%02d P%03X S%d 57:%u 58:%u 61:%u 62:%u  ", cmd, piece & 0xFFF, step,
            trace_n57 % 10000, trace_n58 % 10000, trace_n61 % 100, trace_n62 % 1000);
    print_diag(trace_tmp, tb, 2, 21);
}
#define TRACE_SAVE(c, p, st) trace_save((c), (p), (st))
#else
#define TRACE_SAVE(c, p, st) ((void)0)
#endif

void print_diag(char *temp, const char *msg, int x, int y) {
    if (quiet) return;
#if !KOBO_DIAG_TEXT
    return;                                   /* K19: green text switched off (see top) */
#endif
    strcpy(temp, msg);
    switch_banks();
    do_md_cmd4(MD_CMD_PUT_STR, 0x200000, TEXT_GREEN, x, y);
    switch_banks();
}

/* =====================================================================================
 * KOBO STORAGE MODE - must match d32xr-master/sh2_main.c KOBO_NOCART
 *
 *   1 = NO-CART (development / Fusion): the RAM cart is never touched - not even the
 *       start-up cart check (accessing a missing cart crashes the console).  The SH2 reads
 *       KOBOGFX.BIN straight from CD with CMD 38 (open) + CMD 50 (one 32 KB chunk into the
 *       frame buffer).  Runs in Fusion and on the console, cart plugged in or not.
 *   0 = CART (real hardware + MicroNut99 cart): KOBOGFX.BIN is stored in the cart at
 *       offset 2 MB with CMD 38 + CMD 46 (Chilly's rules, verified), the SH2 reads it at
 *       0x22000000.  Proven on hardware.
 * ===================================================================================== */
#define KOBO_NOCART     0          /* HW1: CART MODE - real hardware WITH the cart plugged in */

#define CART_TARGET     0              /* MicroNut99 cart layout (only used when KOBO_NOCART 0) */
#define KOBO_PACK_NAME  "KOBOGFX.BIN"
#define KOBO_PACK_SLOT  16             /* cart mode: 16 * CART_SLOT = 2 MB into the 4 MB cart */
#define CART_WINDOW_READS    1   
#define CART_VERIFY_READBACK 1   

#define CART_IMG_BASE   0x000000
#define CART_SLOT       0x20000
#define CMD_CHUNK_TO_CART 46
#define CMD_CHUNK_COMPARE 47       /* D32X M1c: CMD 46 without writing - is the chunk already in the cart? */
#define CMD_CART_TO_FB    47             
static int cur_img = 0;

/* OL-2: OpenLara levels.  CMD 38 with file number 100 + LevelID opens the level; CMD 46
   stores it at cart slot 16 (2 MB), where the SH2 reads it (0x02200000).  The ORDER must
   match the engine's enum LevelID (OpenLara-src/src/fixed/common.h). */
#define OL_LEVEL_BASE   100
#define OL_LEVEL_SLOT   16
#define OL_CAVES_BASE   200            /* CAVES-1: 200 + LevelID -> slot 4 = cart 0x080000 */
#define OL_CAVES_SLOT   4
static const char *const ol_level_files[] = {
    "TITLE.PKD",   "GYM.PKD",     "LEVEL1.PKD",  "LEVEL2.PKD",  "LEVEL3A.PKD",
    "LEVEL3B.PKD", "CUT1.PKD",    "LEVEL4.PKD",  "LEVEL5.PKD",  "LEVEL6.PKD",
    "LEVEL7A.PKD", "LEVEL7B.PKD", "CUT2.PKD",    "LEVEL8A.PKD", "LEVEL8B.PKD",
    "LEVEL8C.PKD", "LEVEL10A.PKD","CUT3.PKD",    "LEVEL10B.PKD","CUT4.PKD",
    "LEVEL10C.PKD"
#if OL_UB
    /* UB-1 (OL_UB, Makefile UB=1, the default): Unfinished Business - LevelID 21-24, so file
       100 + id (and 200 + id) finds them like every other level.  A disc without them: CMD 38
       answers "not found" (length 0), the game then leaves them out of the passport. */
    , "EGYPT.PKD", "CAT.PKD", "END.PKD", "END2.PKD"
#endif
};
#define OL_LEVEL_COUNT  ((int)(sizeof(ol_level_files) / sizeof(ol_level_files[0])))
/* FMV-1: the movies, file 300 + movie (the game's order) -> the level slot (2 MB) */
#define OL_FMV_BASE     300
static const char *const ol_fmv_files[] = {
    "CORE.ROQ",    "MANSION.ROQ", "CAFE.ROQ",    "SNOW.ROQ",    "LIFT.ROQ",    "VISION.ROQ",
    "CANYON.ROQ",  "PYRAMID.ROQ", "PRISON.ROQ",  "ESCAPE.ROQ",  "END.ROQ"
};
#define OL_FMV_COUNT    ((int)(sizeof(ol_fmv_files) / sizeof(ol_fmv_files[0])))
static int g_nexc = 0;                   

#define XB(off)   (0x0C0100 + (off))       
#define XM(off)   (0x200100 + (off))       
#define PB(off)   (*(volatile uint8_t *)XB(off))

static void cx(int src, int dst, int len)  
{
    do_md_cmd3(MD_CMD_SAFE_CART_XFER, src, dst, len);
}

static int safe_cart_present(char *temp, char *tbuf)
{
    int ok;
#if CART_TARGET == 1
    uint8_t id, orig, rb;
    PB(0) = 0; PB(1) = 0; PB(2) = 0x5A; PB(3) = 0; PB(4) = 1; PB(5) = 0;
    switch_banks();                              
    cx(0x400001, XM(0), 1);                      
    cx(0x600001, XM(1), 1);                      
    cx(XM(4), 0x7FFFFF, 1);                      
    cx(XM(2), 0x600001, 1);                      
    cx(0x600001, XM(3), 1);                      
    cx(XM(1), 0x600001, 1);                      
    cx(XM(5), 0x7FFFFF, 1);                      
    switch_banks();                              
    id = PB(0); orig = PB(1); rb = PB(3);
    ok = (rb == 0x5A);
    sprintf(tbuf, "BRAM ID:%02X (%uKB) OLD:%02X        ", id, (id & 0x80) ? 0 : (0x2000u << (id & 7)) >> 10, orig);
    print_diag(temp, tbuf, 2, 23);
    sprintf(tbuf, "BRAM @600001: WROTE 5A READ %02X  ", rb);
#else
    uint16_t orig = 0, via9 = 0xFFFF, via4 = 0xFFFF;
    PB(0) = 0x5A; PB(1) = 0xA5; PB(2) = 0; PB(3) = 0;
#if CART_WINDOW_READS
    do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, 0);                       
    orig = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000);
#endif
    switch_banks();
    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000, XM(0), 2);          
    switch_banks();                                                     
    via4 = 0xFFFF;                                                      
#if CART_WINDOW_READS
    via9 = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000);        
    PB(0) = orig >> 8; PB(1) = orig & 0xFF;
    switch_banks();
    do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000, XM(0), 2);          
    switch_banks();
#endif
#if !CART_WINDOW_READS && !CART_VERIFY_READBACK
    ok = 1;                                                             
#else
    ok = (via4 == 0x5AA5 || via9 == 0x5AA5);
#endif
    sprintf(tbuf, "OLD:%04X  VIA 400000:%04X      ", orig, via4);
    print_diag(temp, tbuf, 2, 23);
    sprintf(tbuf, "WROTE 5AA5  VIA 900000:%04X    ", via9);
#endif
    print_diag(temp, tbuf, 2, 24);
    print_diag(temp, ok ? "SAFE CART: FOUND               " : "SAFE CART: NOT FOUND           ", 2, 25);
    return ok;
}

static uint16_t cdda_track_word;       

static int bios_wait_ready(unsigned int max_ticks)
{
    (void)max_ticks;
    while (scd_cd_status() & 0x80000000) {
        bump_fm();
    }
    return 1;
}


/* =====================================================================================
 * K33 SOUND EFFECTS - Sega CD PCM chip (RF5C164), driven directly by the Sub-CPU.
 *   KOBOSFX.BIN (tools/kobo_sfx.py, from Kobo's own rendered .agw sounds): a table + a
 *   ready-made 64 KB wave-RAM image, loaded ONCE at boot by CMD 53 (before the music:
 *   no CD reads after the music starts).
 *   Playing: the SH2 puts an effect number in the upper byte of its pad request
 *   (COMM0 = 48 | id << 8); the fast path below starts it - no extra exchange, no 68K.
 *   Channel 7 = the player's gun (restarted each shot), channels 0-6 = round robin.
 *   Chip registers (odd bytes at 0xFF0001..): ENV volume, PAN, FDL/FDH step (0x800 = 32552
 *   Hz), LSL/LSH loop address, ST start page, CTRL (bit 7 sound on, bit 6: 1 = choose a
 *   channel for the registers above, 0 = choose a 4 KB wave-RAM bank), ONOFF (bit = 1 off).
 *   Wave RAM window: 0xFF2001 + 2 * n, n < 4096, of the bank chosen in CTRL.
 * ===================================================================================== */
#define PCM_ENV    0xFF0001
#define PCM_PAN    0xFF0003
#define PCM_FDL    0xFF0005
#define PCM_FDH    0xFF0007
#define PCM_LSL    0xFF0009
#define PCM_LSH    0xFF000B
#define PCM_ST     0xFF000D
#define PCM_CTRL   0xFF000F   /* K35b FIX: register 7 (was 0xFF0011 = the ON/OFF register!)    */
#define PCM_ONOFF  0xFF0011   /* K35b FIX: register 8 (was 0xFF0013 - nothing there).  Order of
                                 the registers: ENV PAN FDL FDH LSL LSH ST CTRL ON/OFF at
                                 0xFF0001, 03 ... 11.  K33/K35 wrote the channel/bank selects
                                 into ON/OFF: channels switched on/off at random and never
                                 stopped cleanly - the shot kept looping (seen in Fusion). */
#define PCM_WAVE   0xFF2001
#define SFX_MAX    31
typedef struct { uint8_t st, vol; uint16_t fd, loop, len; } sfx_t;
static sfx_t sfx_tab[SFX_MAX + 1];            /* [1..count] */
static int sfx_count = 0;
static uint8_t sfx_map[256];                  /* OL-6: engine sample index -> effect */
static int sfx_has_map = 0;
static uint8_t pcm_onoff = 0xFF;              /* shadow: the ONOFF register cannot be read */
static int pcm_next = 0;

/* SFXVOL-1: vol = 1-64 from the SH2 (64 = full, lower = farther away; 0 = full).  The
   chip's ENV register scales the sample linearly, so ENV = the effect's own level * vol / 64. */
static void pcm_play(int id, int vol) {
    const sfx_t *e;
    int ch;
    if (id < 1 || id > sfx_count) return;
    e = &sfx_tab[id];
    ch = (id == 1) ? 7 : pcm_next;            /* id 1 = player gun: its own channel */
    if (id != 1) pcm_next = pcm_next < 6 ? pcm_next + 1 : 0;
    pcm_onoff |= (uint8_t)(1 << ch);          /* key off (restart from the start page) */
    write_byte(PCM_ONOFF, pcm_onoff);
    write_byte(PCM_CTRL, 0xC0 | ch);          /* sound on + registers of channel ch */
    if (vol <= 0 || vol > 64) vol = 64;
    write_byte(PCM_ENV, (uint8_t)((e->vol * vol) >> 6));
    write_byte(PCM_PAN, 0xFF);
    write_byte(PCM_FDL, e->fd & 0xFF);
    write_byte(PCM_FDH, e->fd >> 8);
    write_byte(PCM_LSL, e->loop & 0xFF);
    write_byte(PCM_LSH, e->loop >> 8);
    write_byte(PCM_ST, e->st);
    pcm_onoff &= (uint8_t)~(1 << ch);         /* key on */
    write_byte(PCM_ONOFF, pcm_onoff);
}

/* CMD 53: KOBOSFX.BIN -> PCM wave RAM.  Returns the number of effects (0 = failed). */
static int pcm_load(const char *name) {           /* OL-6: SFX<level>.BIN */
    int64_t fi = scd_open_file(name);
    uint32_t len, lba, nsec, img, i, b;
    volatile uint8_t *buf = (volatile uint8_t *)0x0C0000;   /* Sub-CPU's Word RAM bank */
    if (fi < 0) return 0;
    len = (uint32_t)(fi >> 32); lba = (uint32_t)(fi & 0xFFFFFFFF);
    nsec = (len + 2047) / 2048;
    if (nsec > 48) nsec = 48;
    scd_sub_read((void *)0x0C0000, lba, nsec);
    if (buf[0] != 'K' || buf[1] != 'S' || buf[2] != 'F' || buf[3] != 'X') return 0;
    sfx_count = (buf[4] << 8) | buf[5];
    img = (uint32_t)((buf[6] << 8) | buf[7]);
    if (img == 0) img = 65536;
    sfx_has_map = 0;                                           /* OL-6: map after the image */
    if (len >= 256 + img + 256) {
        for (i = 0; i < 256; i++) sfx_map[i] = buf[256 + img + i];
        sfx_has_map = 1;
    }
    if (sfx_count > SFX_MAX) sfx_count = SFX_MAX;
    for (i = 0; i < (uint32_t)sfx_count; i++) {
        volatile uint8_t *t = buf + 8 + i * 8;
        sfx_tab[i + 1].st = t[0]; sfx_tab[i + 1].vol = t[1];
        sfx_tab[i + 1].fd = (uint16_t)((t[2] << 8) | t[3]);
        sfx_tab[i + 1].loop = (uint16_t)((t[4] << 8) | t[5]);
        sfx_tab[i + 1].len = (uint16_t)((t[6] << 8) | t[7]);
    }
    pcm_onoff = 0xFF;
    write_byte(PCM_ONOFF, 0xFF);                               /* all channels off */
    for (b = 0; b < 16; b++) {                                 /* 16 banks x 4 KB */
        write_byte(PCM_CTRL, (uint8_t)b);                      /* sound off, wave bank b */
        for (i = 0; i < 4096; i++) {
            uint32_t a = b * 4096 + i;
            write_byte(PCM_WAVE + 2 * i, a < img ? buf[256 + a] : 0xFF);   /* unused = stop */
        }
    }
    write_byte(PCM_CTRL, 0x80);                                /* sound on */
    return sfx_count;
}


/* =====================================================================================
 * K36 BACKUP RAM (Sega CD internal) - high scores + options, one 128-byte file.
 *   BIOS entry _BURAM = 0x5F16: d0 = function, a0/a1 = parameters, carry set = error.
 *   BRMINIT 0 (a0 = 0x640-byte work area, a1 = 12-byte string buffer), BRMREAD 3 (a0 = 11-char
 *   file name, a1 = buffer), BRMWRITE 4 (a0 = name(11) + mode(1: 0 = normal) + blocks(word,
 *   64 bytes each), a1 = data).  All registers are saved around the call.
 *   The SH2 exchanges the 128 bytes one word at a time over COMM (CMD 57 get / 58 put) - no
 *   frame buffer, no Word RAM, only at start-up and after a game.
 * ===================================================================================== */
#define BRAM_BYTES 4096                 /* OL-5: OpenLara settings + saved game */
static uint8_t bram_work[0x640];
static uint8_t bram_str[16];
static uint8_t bram_buf[BRAM_BYTES];
static const char bram_name[12] = "OPENLARA_TR";   /* OL-5 */
static uint8_t bram_param[14];
static int bram_ready = 0;

static int bram_call(int fn, const void *pa0, void *pa1) {
    register int d0 __asm__("d0") = fn;
    register const void *a0 __asm__("a0") = pa0;
    register void *a1 __asm__("a1") = pa1;
    register int d1 __asm__("d1") = 0;
    __asm__ volatile (
        "movem.l %%d2-%%d7/%%a2-%%a6,-(%%sp)\n\t"
        "jsr 0x5F16.w\n\t"
        "scs %%d1\n\t"                          /* carry (error) -> d1 low byte 0xFF */
        "movem.l (%%sp)+,%%d2-%%d7/%%a2-%%a6"
        : "+d"(d0), "+a"(a0), "+a"(a1), "+d"(d1) : : "cc", "memory");
    return (d1 & 0xFF) ? -1 : d0;
}
static int bram_init(void) {
    if (!bram_ready) bram_ready = bram_call(0, bram_work, bram_str) >= 0 ? 1 : -1;
    return bram_ready > 0;
}
static int bram_load(void) {                    /* 1 = loaded, 0 = no file / no BRAM */
    int i;
    for (i = 0; i < BRAM_BYTES; i++) bram_buf[i] = 0;
    if (!bram_init()) return 0;
    return bram_call(3, bram_name, bram_buf) >= 0 ? 1 : 0;
}
static int bram_save(void) {
    int i;
    if (!bram_init()) return 0;
    for (i = 0; i < 11; i++) bram_param[i] = (uint8_t)bram_name[i];
    bram_param[11] = 0;                         /* normal (not protected) mode */
    bram_param[12] = 0; bram_param[13] = BRAM_BYTES / 64;   /* blocks */
    return bram_call(4, bram_param, bram_buf) >= 0 ? 1 : 0;
}

static int cdda_start(int track, int *st_out, int once)   /* OL-4 play once */
{
    int st = 0;
    bios_wait_ready(0);
    scd_bios_call(0x0089, 0, 0);            
    scd_bios_call(0x0002, 0, 0);            
    scd_bios_call(0x0085, 0x0400, 0);       
    cdda_track_word = (uint16_t)track;
    scd_bios_call(once ? 0x0012 : 0x0013, 0, &cdda_track_word);   /* MSCPLAY1 once / MSCPLAYR repeat */  
    st = scd_cd_status();
    *st_out = st; 
    return 1;
}

int main(void)
{
    write_byte(0x8003, read_byte(0x8003) & ~1);
    while (read_byte(0x8003) & 2);
#if !KOBO_BOOT_TEXT
    do_md_cmd1(MD_CMD_SET_VDP_REG, 0x8134);   /* K30: Genesis display off (md_init_hw left it on) */
#endif

    char *temp = (char *)0x0C0000;
    char tbuf[64];
#if KOBO_DIAG_TEXT
    trace_tmp = temp;
#endif
    uint32_t build_counter = 0;
    int cart_ok = 0;

    print_diag(temp, "BUILD: D32XR CD M1C QUICK CHECK     ", 2, 0);
    print_diag(temp, "LOADING 32XCD ENGINE...           ", 2, 2);

    {
        const char *m = find_sh2_marker(sh2_app_start, sh2_app_length);
        if (m) sprintf(tbuf, "32X PAYLOAD: %d BYTES SH2-V%c%c        ", sh2_app_length, m[0], m[1]);
        else   sprintf(tbuf, "32X PAYLOAD: %d BYTES SH2:NONE       ", sh2_app_length);
        print_diag(temp, tbuf, 2, 4);
    }

    memcpy(temp, sh2_app_start, sh2_app_length);
    switch_banks();
    {   
        int r32 = do_md_cmd2(MD_CMD_INIT_32X, 0x200000, sh2_app_length);
        do_md_cmd0(MD_CMD_INSTALL_EXC);            /* HW1: hook the 68K error vectors (after the 32X is on) */
        g_nexc = 0;
        char a[12], b[12], c[12];
        fourcc(a, read_long(0xFF8014));   
        fourcc(b, read_long(0xFF8018));   
        fourcc(c, read_long(0xFF801C));   
        switch_banks();
        
        sprintf(tbuf, "INIT32X:%d 1ST:%s              ", r32, a);
        print_diag(temp, tbuf, 2, 19);
        sprintf(tbuf, "END C0:%s C4:%s          ", b, c);
        print_diag(temp, tbuf, 2, 20);
        if (r32 < 0) {
            print_diag(temp, "32X INIT FAILED - STOPPED       ", 2, 21);
            while (1) bump_fm();
        }

        do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15124, 0x1234);

        {
            uint16_t ready = 0;
            volatile int wait = 0;
            
            {
                unsigned int t0 = GET_TICKS;
                for (wait = 0; wait < 1000000; wait++) {
                    ready = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512A);
                    if (ready == 0x0AAA) break;
                    if ((GET_TICKS - t0) >= 180) break;
                }
            }
            sprintf(tbuf, "SH2 READY FOR ISOLATED TEST: %04X    ", ready);
            print_diag(temp, tbuf, 2, 19);

            if (ready != 0x0AAA) {
                print_diag(temp, "SH2 NEVER SIGNALLED READY - STOPPED", 2, 20);
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512A, 0x0BBB);   
                while (1) bump_fm();
            }
        }

#if KOBO_NOCART
        cart_ok = 0;                                   /* no-cart build: never touch the cart */
        print_diag(temp, "CART: NOT USED (NO-CART BUILD)     ", 2, 25);
#else
        cart_ok = safe_cart_present(temp, tbuf);  
#endif
        (void)cart_ok; 

        do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512A, 0x0BBB);   
    }

    uint16_t init_err = InitCD();

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-INITCD COMM: %02X          ", c);
        print_diag(temp, tbuf, 2, 1);
    }

    if (init_err != 0) {
    } else {
        print_diag(temp, "CD DRIVE READY                ", 2, 5);
        print_diag(temp, "BOOTING SUB-CPU...            ", 2, 6);
    }

    {   
        int st = scd_cd_status();
        int t2 = scd_track_info(2);
        sprintf(tbuf, "CD TRACKS %02X-%02X  TRACK2 %08X      ", (st >> 8) & 0xFF, st & 0xFF, t2);
        print_diag(temp, tbuf, 2, 14);
    }

    memcpy(temp, Sub_Start, Sub_End - Sub_Start);

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-MEMCPY2 COMM: %02X         ", c);
        print_diag(temp, tbuf, 2, 3);
    }

    print_diag(temp, "INITIALIZING SUB-CPU Sfx...   ", 2, 7);
    send_md_cmd0('I'); 

    {
        unsigned char c = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "POST-SENDMD COMM: %02X          ", c);
        print_diag(temp, tbuf, 2, 18);
    }

    print_diag(temp, "68K LISTENER ACTIVE           ", 2, 8);

    {
        unsigned char boot_comm = read_byte(SUB_MAIN_FLAG);
        sprintf(tbuf, "BOOT COMM PORT: %02X            ", boot_comm);
        print_diag(temp, tbuf, 2, 9);
    }

    print_diag(temp, "(TEXT BLINKS TO SHOW SH2 IS ALIVE)", 2, 26);

    while (1) {
        build_counter++;

        if (build_counter == 1) {
            unsigned char first_iter_comm = read_byte(SUB_MAIN_FLAG);
            sprintf(tbuf, "ITER1 COMM PORT: %02X           ", first_iter_comm);
            print_diag(temp, tbuf, 2, 15);
        }

        bump_fm();

#if KOBO_HW_QUIET
        if (sub_quiet) {                 /* HW9 QUIET TEST: no 68K traffic except this heartbeat */
            static unsigned int t_last = 0;
            static uint16_t q_count = 0;
            unsigned int now = GET_TICKS;             /* read directly - no 68K command */
            if (now - t_last >= 30) {
                t_last = now;
                q_count++;
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512E,
                           ((read_byte(0xFF8038) & 0x0F) << 12) | (q_count & 0x0FFF));
            }
            continue;
        }
#endif
        {   /* HW1 SUB-CPU HEARTBEAT (K29): every 16 passes the pass counter goes to COMM14.  Only
               the Sub-CPU writes COMM14; the SH2 only reads it (hardware readout "P"). */
            static uint16_t hb_pass = 0;
            /* HW8: top digit = the CD drive's status (CDD status register 0xFF8038, first
               nibble: 0 stopped, 1 playing, 2 seeking, 3 scanning, 4 paused, 5 tray open,
               9 reading TOC, 0xB no disc / error ...), lower 3 digits = pass count / 64 */
            if ((++hb_pass & 63) == 0) {
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512E,
                           ((read_byte(0xFF8038) & 0x0F) << 12) | ((hb_pass >> 6) & 0x0FFF));
#if KOBO_DIAG_TEXT   /* OL-5 */
                /* OL-2 DIAG 3: Sub-CPU heartbeat on screen + the command waiting in COMM0 */
                sprintf(tbuf, "S%04X C0:%04X ST:%04X M:%04X SL:%04X", hb_pass,   /* DIAG 4 */
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15120),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA15126),
                        do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512A));
                if (!ol_game_running) print_diag(temp, tbuf, 2, 19);   /* OL-2b */
                else if ((hb_pass & 1023) == 0) {
                    /* EDSAVE-D: in the game, every 16th heartbeat: all eight COMM registers, so a
                       freeze shows what is pending (C0 = a Sub-CPU command, C4/C6 = the SH2s' own) */
                    sprintf(tbuf, "G%04X C0 %04X C2 %04X C4 %04X C6 %04X", hb_pass,
                            do_md_cmd1(MD_CMD_READ_32X_16, 0xA15120), do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122),
                            do_md_cmd1(MD_CMD_READ_32X_16, 0xA15124), do_md_cmd1(MD_CMD_READ_32X_16, 0xA15126));
                    print_diag(temp, tbuf, 2, 19);
                    sprintf(tbuf, "C8 %04X CA %04X CC %04X CE %04X        ",
                            do_md_cmd1(MD_CMD_READ_32X_16, 0xA15128), do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512A),
                            do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C), do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512E));
                    print_diag(temp, tbuf, 2, 20);
                }
#endif
            }
        }
        /* SFXVOL-1: COMM0 (command) and COMM2 (its parameter) in ONE read - the effect's
           volume comes in COMM2 with the pad request, at no extra round trip through the 68K */
        uint32_t cmd32 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0xA15120);
        uint16_t cmd = (uint16_t)(cmd32 >> 16);
        uint16_t cmdParam = (uint16_t)(cmd32 & 0xFFFF);
        if (cmd == 0) {                  /* HW6: idle - pause ~0.2 ms before the 68K reads COMM0
                                            again (fewer reads while the SH2 may be writing it) */
            volatile int k;
#if OL_STREAM
            for (k = 0; k < 150; k++) { if ((k & 3) == 0) cd_pump(); }   /* STREAM-1: read ahead */
#else
            for (k = 0; k < 150; k++) { }
#endif
        }

        /* KOBO K5 - PAD FAST PATH.  WHY: the SH2 asks for the pad (CMD 48) every frame and
           only waits a short time.  The rest of this loop (3 more register reads + redrawing
           row 10, each a round trip through the 68K) took longer than that, so the SH2 gave
           up and scrolled with pad = 0.  Answer CMD 48 first and start the next pass. */
        if ((cmd & 0xFF) == 48) {
            uint16_t pad = GET_PAD(0);
            if (cmd >> 8) pcm_play(sfx_has_map ? sfx_map[((cmd >> 8) - 1) & 0xFF] : (cmd >> 8), cmdParam);   /* OL-6 */           /* K33: sound effect requested */
            /* K32: reply = pad (COMM8, upper 16 bits) + the 68K's vblank counter (COMM10, lower
               16 bits) in ONE 32-bit write.  The SH2 uses the counter to run Kobo's logic at
               its fixed 30 ms step, whatever the frame rate.  COMM10 is only used by the
               start-up handshake, long finished by now. */
            do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, ((uint32_t)pad << 16) | (GET_TICKS & 0xFFFF));
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);     /* done           */
            continue;
        }

#if KOBO_DIAG_TEXT   /* OL-5: no register reads for text nobody sees */
        uint16_t comm4 = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15124);
        uint16_t sh2_state = do_md_cmd1(MD_CMD_READ_32X_16, 0xA1512C);

        {   /* HW5: redraw row 10 ONLY when the SH2's step code changes, and never again once the
               game runs (0x4B10).  WHY: the old test also watched COMM0 (flips 00/30 every
               frame with the pad request) and COMM8/10 (pad + frame counter since K32), so on
               hardware (text on) the Sub-CPU reprinted a text row EVERY pass - each print swaps
               Word RAM and goes through the 68K - and the pad replies waited: 1-2 fps. */
            static uint16_t l_st = 0xFFFF;
            static int running = 0;
            if (!running && sh2_state != l_st) {
                uint32_t ckpt = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
                l_st = sh2_state;
                sprintf(tbuf, "32X:%X 32XReg:%X STATE:%X CKPT:%04X     ", cmd, comm4, sh2_state, (unsigned int)(ckpt >> 16));
                print_diag(temp, tbuf, 2, 10);
                if (sh2_state == 0x4B10) { running = 1; ol_game_running = 1; sub_quiet = KOBO_HW_QUIET; }   /* HW9 */
            }
        }

        if (comm4 == 0x0EEE) {
            print_diag(temp, "SH2 REACHED THE LDC LINE              ", 2, 18); 
        } else if (!ol_game_running && comm4 != 0x1234 && comm4 != 0x0000) {   /* OL-2b: quiet once the game runs */
            print_diag(temp, "SH2 NEVER EVEN REACHED THIS FILE      ", 2, 18); 
        }

#endif

        if (cmd == 38) { 
            int img_idx = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            char img_name[24];
            cur_img = img_idx < 0 ? 0 : img_idx;         
            /* KOBO M1: file number 16 = the Kobo graphics pack (tools/kobo_gfx.py).
               CMD 46 then stores it at cart offset 16 * CART_SLOT = 2 MB. */
            if (img_idx == 16) { strcpy(img_name, KOBO_PACK_NAME); cur_img = KOBO_PACK_SLOT; }
            else if (img_idx == 20) { strcpy(img_name, "OPENLARA.32X"); cur_img = 0; }  /* D32X M1: ROM at cart 0 */
            else if (img_idx >= OL_LEVEL_BASE && img_idx < OL_LEVEL_BASE + OL_LEVEL_COUNT) {
                strcpy(img_name, ol_level_files[img_idx - OL_LEVEL_BASE]);   /* OL-2 */
                cur_img = OL_LEVEL_SLOT;
            }
            else if (img_idx >= OL_CAVES_BASE && img_idx < OL_CAVES_BASE + OL_LEVEL_COUNT) {
                /* CAVES-1: Caves (the title menu) stays in its own place, cart 0x080000 */
                strcpy(img_name, ol_level_files[img_idx - OL_CAVES_BASE]);
                cur_img = OL_CAVES_SLOT;
            }
            else if (img_idx >= OL_FMV_BASE && img_idx < OL_FMV_BASE + OL_FMV_COUNT) {
                strcpy(img_name, ol_fmv_files[img_idx - OL_FMV_BASE]);   /* FMV-1 */
                cur_img = OL_LEVEL_SLOT;
            }
            else if (img_idx <= 0) strcpy(img_name, "IMAGE.RAW");
            else sprintf(img_name, "IMAGE%d.RAW", img_idx);

            sprintf(tbuf, "OPENING %s...              ", img_name);
            print_diag(temp, tbuf, 2, 11);

            unsigned char entry_comm = read_byte(SUB_MAIN_FLAG);
            if (entry_comm != 0) {
                sprintf(tbuf, "ENTRY COMM FAULT: %02X        ", entry_comm);
                print_diag(temp, tbuf, 2, 12);
            }

            int64_t file_info = scd_open_file(img_name);
            if (file_info < 0) {
                sprintf(tbuf, "ERR: %s NOT FOUND          ", img_name);
                print_diag(temp, tbuf, 2, 12);
                sprintf(tbuf, "TIMEOUT: %d (0=none 1=cmd 2=ack)", scd_last_timeout_code);
                print_diag(temp, tbuf, 2, 13);
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, 0); 
                saved_len = 0;
            } else {
                uint32_t length = (uint32_t)(file_info >> 32);
                saved_lba = (uint32_t)(file_info & 0xFFFFFFFF);
                saved_len = length;                                   /* K10: for CMD 50 */
                sprintf(tbuf, "%s %lu BYTES%s          ", img_name, (unsigned long)length,
                        length > 128000 ? " (TOO TALL)" : " OK");
                print_diag(temp, tbuf, 2, 12);
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA15128, length); 
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0); 
        }
        else if (cmd == CMD_CHUNK_TO_CART || cmd == CMD_CHUNK_COMPARE) {
            int chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int novfy = chunk & 0x8000;          /* OL-13: level loads skip the read-back */
#if OL_FMVRING
            /* FMV-2 (OL_FMVRING, Makefile FMVRING=1, the default): COMM2 bit 14 = ring mode for a
               movie that plays while it streams: file chunk (COMM2 & 0x1FFF) goes to ring slot
               chunk % 60 of the level slot (60 x 32 KB), or with bit 13 to slot 60 (the copy of
               slot 0 after the ring's end, so a movie chunk across the wrap stays in one piece).
               The 68000 copies 4 KB at a time (short RV stalls for the SH2s playing the movie). */
            int ring = chunk & 0x4000, guard = chunk & 0x2000;
            chunk &= ring ? 0x1FFF : 0x7FFF;
#else
            chunk &= 0x7FFF;
#endif
            int length = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            int off = chunk * 32768;
            int n = 0, bytes, p, k, bad = 0;
            uint32_t coff = CART_IMG_BASE + (uint32_t)cur_img * CART_SLOT + (uint32_t)off;
            int piece = 0x4000;
#if OL_FMVRING
            if (ring) {
                coff = CART_IMG_BASE + (uint32_t)cur_img * CART_SLOT + (uint32_t)(guard ? 60 : chunk % 60) * 32768;
                piece = 0x1000;
            }
#endif

            if (off < length) n = (length - off + 2047) / 2048;
            if (n > 16) n = 16;
            bytes = n * 2048;

#if CART_TARGET == 1
            if (n > 0) {
                volatile uint8_t *flag = (volatile uint8_t *)0x0DF000;   
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * 16, n);
                flag[0] = 1; flag[1] = 0;
                switch_banks();                                   
                cx(0x21F000, 0x7FFFFF, 1);                        
                for (p = 0; p < bytes; p += 4096)
                    do_md_cmd3(MD_CMD_BRAM_PUT, 0x200000 + p, 0x600001 + 2 * (coff + p),
                               bytes - p < 4096 ? bytes - p : 4096);
                cx(0x21F001, 0x7FFFFF, 1);                        
                for (p = 0; p < bytes; p += 4096)                 
                    do_md_cmd3(MD_CMD_BRAM_GET, 0x600001 + 2 * (coff + p), 0x208000 + p,
                               bytes - p < 4096 ? bytes - p : 4096);
                switch_banks();                                   
                {
                    volatile uint8_t *disc = (volatile uint8_t *)0x0C0000;
                    volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
                    for (k = 0; !novfy && k < bytes; k++) {
                        if (disc[k] != cart[k]) {
                            sprintf(tbuf, "MISMATCH C%d +%05X DISC %02X CART %02X ", chunk, k,
                                    disc[k], cart[k]);
                            print_diag(temp, tbuf, 2, 17);
                            bad = 1;
                            break;
                        }
                    }
                }
            }
#else
            if (n > 0) {
#if OL_STREAM
                cd_chunk_read((uint8_t *)0x0C0000, saved_lba + chunk * 16, n,   /* STREAM-1 */
                              (int)(saved_lba + (saved_len + 2047) / 2048));
#else
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * 16, n);   
#endif
                switch_banks();                                   
#if OL_FMVRING
                /* FMV-2: COMM12 bit 15 while the cart is written in ring mode: the slave SH2 (the
                   movie's sound) does not read the cart then, so RV never holds it */
                if (ring) do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0x8000);
#endif
#if OL_CARTW
                /* CARTW-1 (OL_CARTW, Makefile CARTW=1, the default): the 68000 copies words
                   (hw_md.s command 27, safe_cart_xfer_w: 15 cycles a byte) instead of bytes
                   (command 33: 42 cycles a byte).  Every chunk reaches the cart almost 3 times
                   faster, and RV (the SH2s locked out of the cart) is on for a third of the time.
                   Command 27 masks the 68000's interrupts, so the pieces are 4 KB (8 ms, less than
                   a frame: the vblank waits, it is never lost).  Arguments: source, destination. */
                (void)piece;
                if (cmd != CMD_CHUNK_COMPARE)
                    for (p = 0; p < bytes; p += 0x1000)
                        do_md_cmd3(MD_CMD_SAFE_CART_XFER_W, 0x200000 + p, 0x400000 + coff + p,
                                   bytes - p < 0x1000 ? bytes - p : 0x1000);
#else
                if (cmd != CMD_CHUNK_COMPARE)          /* CMD 47: compare only, no write */
                    for (p = 0; p < bytes; p += piece)
                        do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + coff + p, 0x200000 + p,
                                   bytes - p < piece ? bytes - p : piece);
#endif
#if OL_FMVRING
                if (ring) do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, 0);
#endif
#if CART_VERIFY_READBACK
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);   
                for (p = 0; !novfy && p < bytes; p += 0x4000)
                    do_md_cmd3(MD_CMD_CHILLY_MEMCPY, 0x208000 + p,
                               0x900000 + ((coff + p) & 0xFFFFF),
                               bytes - p < 0x4000 ? bytes - p : 0x4000);
#endif
                switch_banks();                                   
#if CART_VERIFY_READBACK
                {   
                    volatile uint8_t *disc = (volatile uint8_t *)0x0C0000;
                    volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
#if OL_STREAM
                    {   /* STREAM-1: four bytes at a time, pumping; the byte loop below finds the place */
                        const volatile uint32_t *dw = (const volatile uint32_t *)disc;
                        const volatile uint32_t *cw = (const volatile uint32_t *)cart;
                        int w;
                        for (w = 0; !novfy && w < bytes / 4; w++) {
                            if ((w & 255) == 0) cd_pump();
                            if (dw[w] != cw[w]) break;
                        }
                        k = novfy ? bytes : w * 4;
                    }
                    for (; !novfy && k < bytes; k++) {
#else
                    for (k = 0; !novfy && k < bytes; k++) {
#endif
                        if (disc[k] != cart[k]) {
                            sprintf(tbuf, "MISMATCH C%d +%05X DISC %02X CART %02X ", chunk, k,
                                    disc[k], cart[k]);
                            print_diag(temp, tbuf, 2, 17);
                            bad = 1;
                            break;
                        }
                    }
                }
#endif
#if CART_WINDOW_READS
                {   
                    int wbad = 0;
                    uint16_t want = 0, got = 0;
                    uint32_t o = 0;
                    do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);   
                    for (k = 0; k < 4 && !wbad; k++) {
                        o = (uint32_t)(((bytes - 2) / 3 * k) & ~1);
                        want = *(volatile uint16_t *)(0x0C0000 + o);
                        got  = (uint16_t)do_md_cmd1(MD_CMD_READ_32X_16, 0x900000 + ((coff + o) & 0xFFFFF));
                        if (got != want) wbad = 1;
                    }
                    if (wbad)
                        sprintf(tbuf, "32X WIN C%d +%05lX W%04X R%04X   ", chunk, (unsigned long)o, want, got);
                    else
                        sprintf(tbuf, "32X WIN C%d 900000: OK            ", chunk);
                    print_diag(temp, tbuf, 2, 16);
                }
#else
                print_diag(temp, "32X WIN: OFF (V72 TEST)          ", 2, 16);
#endif
            }
#endif
            sprintf(tbuf, "%s I%d C%d @%06lX %s          ", cmd == CMD_CHUNK_COMPARE ? "CMP " : "CART", cur_img, chunk,
#if CART_TARGET == 1
                    (unsigned long)(0x600001 + 2 * coff),      
#else
                    (unsigned long)(0x400000 + coff),          
#endif
                    n == 0 ? "EMPTY" : bad ? "BAD" : (CART_VERIFY_READBACK ? "OK" : "NOVFY"));
            print_diag(temp, tbuf, 2, 13);
#if OL_FMVRING
            if (ring) bad |= 0x100;              /* FMV-2: "ring mode known" - the SH2 tests it */
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, bad);   /* HW4: 16-bit COMM12 - COMM14 is the heartbeat */
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == CMD_CART_TO_FB) {
            int slot_chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int slot = slot_chunk >> 8;
            int chunk = slot_chunk & 0xFF;
            int length = do_md_cmd1(MD_CMD_READ_32X_32, 0xA15128);
            int off = chunk * 32768, bytes = length - off, p;
            uint32_t coff = CART_IMG_BASE + (uint32_t)slot * CART_SLOT + (uint32_t)off;
            
            if (bytes > 32768) bytes = 32768;
            for (p = 0; p < bytes; p += 4096)
                do_md_cmd3(MD_CMD_BRAM_TO_FB, 0x600001 + 2 * (coff + p), 0x840200 + off + p,
                           bytes - p < 4096 ? bytes - p : 4096);
            
            sprintf(tbuf, "CART->FB S%d C%d %d BYTES          ", slot, chunk, bytes > 0 ? bytes : 0);
            print_diag(temp, tbuf, 2, 16);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 60) {
            /* D32X M1: the ROM image is in the cart.  Acknowledge the SH2 FIRST (the 68K will be
               gone after this), then make the 68K jump to D32XR's Genesis code (0x880800 via the
               32X window).  send_md_cmd only (no wait: the 68K never comes back).  The Sub-CPU
               has nothing else to do in M1 (D32XR finds no Sega CD and does not call it). */
            /* M1b: what does the 68000 see?  D32XR's _start at cart offset 0x800 must read
               7000323C through the fixed window 0x880000 (that is where the 68K will jump) -
               compare with the proven bank window 0x900000 (bank 0) and our patch at 0x40. */
            {
                uint32_t w88, w90, w40;
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, 0);              /* bank 0 */
                w88 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x880800);
                w90 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x900800);
                w40 = (uint32_t)do_md_cmd1(MD_CMD_READ_32X_32, 0x880040);
                sprintf(tbuf, "880800:%08X 900800:%08X  ", (unsigned)w88, (unsigned)w90);
                print_diag(temp, tbuf, 2, 22);
                sprintf(tbuf, "880040:%08X (PATCH 23FCC000)  ", (unsigned)w40);
                print_diag(temp, tbuf, 2, 23);
                /* M1c: where do the 68000's V-blank / H-blank / external-interrupt vectors
                   point with the 32X on?  RAM slots (FFFDxx) or the cart window (880xxx)? */
                sprintf(tbuf, "V78:%08X V70:%08X V68:%08X ",
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x78),
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x70),
                        (unsigned)do_md_cmd1(MD_CMD_READ_32X_32, 0x68));
                print_diag(temp, tbuf, 2, 24);
            }
            print_diag(temp, "D32XR: STARTING THE GAME FROM THE CART ", 2, 14);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
            send_md_cmd0(MD_CMD_START_D32XR);
            while (1) { }
        }
        else if (cmd == 44) {
#if !SLIDESHOW_DEBUG && !KOBO_DIAG_TEXT
            if (!quiet) { do_md_cmd0(MD_CMD_CLEAR_B); do_md_cmd0(MD_CMD_CLEAR_A); quiet = 1; }
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 50 || cmd == 51) {
            /* K15: CMD 51 = same as CMD 50, but chunk c goes to frame buffer byte
               0x200 + c * 32 KB (a whole picture in order - the splash screen). */
            /* KOBO NO-CART - CMD 50 "file chunk -> frame buffer (raw)".
               COMM2 = chunk number (32 KB each).  File length: saved_len from CMD 38 (K10 -
               no longer read from COMM8).  CD -> Word RAM (Sub-CPU) -> frame buffer at 68K 0x840200 via MD
               command 25 copy_words with flag 0 = plain copy (flag 1 would set bit 15 on every
               word, which is only right for 15-bit pictures).  The SH2 has handed the frame
               buffer to the 68K (FM=0) before sending this. */
            int chunk = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int length = (int)saved_len;           /* K10: from CMD 38, NOT from COMM8 */
            /* K31: CMD 50 chunks are 16 KB (8 sectors) - the off-screen loading window shrank
               to 16 KB so all 5 tile sets fit in spare frame-buffer memory (no CD reads after
               the music starts: a data read silences the CD audio).  CMD 51 stays 32 KB. */
            int sec = cmd == 51 ? 16 : 8;                   /* sectors per chunk */
            int off = chunk * sec * 2048;
            int n = 0;
            if (off < length) n = (length - off + 2047) / 2048;
            if (n > sec) n = sec;
            uint16_t w0 = 0;
            /* (K13 cleared Word RAM priority mode here; K13 showed it was never on - register 05
               before and after - so the write is removed in K15: no needless pokes at the Word
               RAM mode register on real hardware.  The real K12 fault was zero words not landing
               in the frame buffer - fixed on the SH2 side by clearing the area first.) */
            if (n > 0) {
                scd_sub_read((void *)0x0C0000, saved_lba + chunk * sec, n);  /* CD -> Word RAM */
                switch_banks();                                               /* -> 68K side    */
                do_md_cmd4(MD_CMD_COPY_WORDS, 0x200000,
                           cmd == 51 ? 0x840200 + off : 0x852000, n * 2048, 0);
                /* K30: CMD 50 now lands OFF-SCREEN (frame buffer byte 0x12000, past the
                   visible 320x224 picture which ends at 0x11A00): loading never overwrites
                   what is on screen (the panel stays intact).  SH2 side: FB_CHUNK_BUF. */
                switch_banks();                                               /* -> Sub side    */
                w0 = *(volatile uint16_t *)0x0C0000;   /* first word read from CD (read it
                                                          BEFORE print_diag reuses this buffer) */
            }
            /* K11 reply for the SH2's diagnostic strip (lines 1-3): chunk | sectors | length>>8.
               Written before COMM0 is cleared; the SH2 reads it only after that. */
            /* K14 reply: chunk | sectors | the word in WORD RAM at offset 0x210 of this chunk, read
               after the copy (the Sub-CPU's buffer at 0x0C0000).  For chunk 0 that is the high
               word of the tile set offset and must be 0000.  Correct here but wrong in the
               frame buffer (strip line T) = the 68K -> frame buffer copy loses zero data.
               (K13: priority mode was never on - register read 05 before and after.)        */
            {
                uint16_t wr = (n > 0) ? *(volatile uint16_t *)(0x0C0000 + 0x210) : 0xEEEE;
                do_md_cmd2(MD_CMD_WRITE_32X_32, 0xA1512C, (chunk << 24) | (n << 16) | wr);
            }
            (void)w0;
            sprintf(tbuf, "CD->FB CHUNK %d  %d SECTORS          ", chunk, n);
            print_diag(temp, tbuf, 2, 16);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 54) {
            /* K36 - CMD 54 "music off": BIOS MSCSTOP (the same call cdda_start makes first).
               Music on again = CMD 45 as always. */
            bios_wait_ready(0);
            scd_bios_call(0x0002, 0, 0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 56 || cmd == 59) {
            /* K36 - CMD 56 load / CMD 59 save the backup-RAM file.  Reply COMM12: 1 ok, 0 not. */
            int ok = cmd == 56 ? bram_load() : bram_save();
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 57 || cmd == 58) {
            /* K36 - CMD 57 get / CMD 58 put one word of the 128-byte buffer: COMM2 = word index,
               COMM8 = value (58: written by the SH2 before the command; 57: our reply). */
            int wi = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & (BRAM_BYTES / 2 - 1);   /* OL-5 */
#if KOBO_DIAG_TEXT
            if (cmd == 58) trace_n58++; else trace_n57++;
            if (((trace_n57 + trace_n58) & 255) == 0) TRACE_SAVE(cmd, wi, 0);
#endif
            if (cmd == 58) {
                int v = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15128);
                bram_buf[wi * 2] = (uint8_t)(v >> 8); bram_buf[wi * 2 + 1] = (uint8_t)v;
            } else {
                do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15128, (bram_buf[wi * 2] << 8) | bram_buf[wi * 2 + 1]);
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 61) {
            /* OL-9: save area - the 4 KB buffer (filled with CMD 58) -> RAM cart at
               0x3F0000 + COMM2 * 4 KB.  The SH2 cannot write the cart; the 68000 can (RV method,
               exactly as CMD 46).  Read back through the 32X window and compared.
               Reply COMM12: 1 ok, 0 failed. */
#if OL_SLOTS
            /* SLOTS-1 (OL_SLOTS, Makefile SLOTS=1, the default): COMM2 bit 8 set = a piece of the
               save slot area, cart 0x060000 + (COMM2 & 31) * 4 KB (OpenLara's 16 save slots, between
               the game and Caves); reply 0x101 instead of 1, so the SH2 knows the slots are there. */
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int slot_area = piece & 0x100;
            uint32_t coff = slot_area ? CART_IMG_BASE + 0x060000 + (uint32_t)(piece & 31) * 4096
                                      : CART_IMG_BASE + 0x3F0000 + (uint32_t)(piece & 15) * 4096;
#else
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 15;
            uint32_t coff = CART_IMG_BASE + 0x3F0000 + (uint32_t)piece * 4096;
#endif
            int k, ok = 1;
            volatile uint8_t *wr = (volatile uint8_t *)0x0C0000;
#if KOBO_DIAG_TEXT
            trace_n61++;
#endif
            TRACE_SAVE(61, piece, 1);                   /* (prints before the buffer is filled) */
            for (k = 0; k < 4096; k++) wr[k] = bram_buf[k];
            switch_banks();
#if OL_EDSAVE
            /* EDSAVE-1: into the EverDrive's save bank, and read back from it (both through the
               listener's window-1 route: RV = 1, W on for the write only) */
            (void)coff;
            do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, ED_SAVE_68K + ed_save_off(piece), 0x200000, 4096);
#if KOBO_DIAG_TEXT
            switch_banks(); TRACE_SAVE(61, piece, 2); switch_banks();   /* written */
#endif
#if OL_EDSAVE_NOVERIFY
            /* EDSAVE-D2 (Makefile NOVERIFY=1, a diagnostic): no read-back - the buffer is copied over
               itself so the compare below passes.  Tells whether the 68000's read of the bank (RV = 1)
               is what freezes the game after a save. */
            { volatile uint8_t *w2 = (volatile uint8_t *)0x0C8000; switch_banks(); for (k = 0; k < 4096; k++) w2[k] = bram_buf[k]; switch_banks(); }
#else
            do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x208000, ED_SAVE_68K + ed_save_off(piece), 4096);
#endif
#if KOBO_DIAG_TEXT
            switch_banks(); TRACE_SAVE(61, piece, 3); switch_banks();   /* read back */
#endif
#else
            do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x400000 + coff, 0x200000, 4096);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15104, (int)(coff >> 20) & 3);
            do_md_cmd3(MD_CMD_CHILLY_MEMCPY, 0x208000, 0x900000 + (coff & 0xFFFFF), 4096);
#endif
            switch_banks();
            {
                volatile uint8_t *cart = (volatile uint8_t *)0x0C8000;
                for (k = 0; k < 4096; k++)
                    if (cart[k] != bram_buf[k]) { ok = 0; break; }
            }
#if OL_SLOTS
            if (ok && slot_area) ok = 0x101;        /* SLOTS-1 */
#endif
#if OL_EDSAVE
            if (ok) ok |= 0x200;                    /* EDSAVE-1: it went to the save bank */
#endif
            TRACE_SAVE(61, piece, ok ? 4 : 9);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#if OL_CART_ED
        else if (cmd == 62) {
            /* EDSAVE-1 - CMD 62 "save piece in": COMM2 = a piece (as CMD 61 numbers them) -> the 4 KB
               buffer, which the SH2 then reads word by word with CMD 57; COMM2 = 0x8000 asks only what
               this Sub-CPU does.  Reply COMM12: 1 | 0x100 (save slots, SLOTS-1) | 0x200 (the saves
               are in the EverDrive's save bank: read them this way, the cart has none). */
            int piece = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int caps = 1 | (OL_SLOTS ? 0x100 : 0) | (OL_EDSAVE ? 0x200 : 0);
#if KOBO_DIAG_TEXT
            trace_n62++;
#endif
            TRACE_SAVE(62, piece, 1);
#if OL_EDSAVE
            if (piece != 0x8000) {
                int k;
                volatile uint8_t *rd = (volatile uint8_t *)0x0C8000;
                switch_banks();
                do_md_cmd3(MD_CMD_CHILLY_RV_MEMCPY, 0x208000, ED_SAVE_68K + ed_save_off(piece), 4096);
                switch_banks();
                TRACE_SAVE(62, piece, 2);
                for (k = 0; k < 4096; k++) bram_buf[k] = rd[k];
            }
#endif
            TRACE_SAVE(62, piece, 3);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, caps);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
#if OL_CART_MB
        else if (cmd == 68) {
            /* CART-1 (test builds only: CART=8 / CART=16 bash build.sh) - CMD 68 "cart page":
               COMM2 = page 1-7 for window 1 (cart 2-4 MB) of the 16MB-PCB-CD32X.  The 68000
               writes the page as ONE even byte to 0xA130F2 (the board latches /UWR, D8-D10)
               with the existing listener routine 26 (safe_cart_xfer: interrupts off, RV = 1,
               byte copy, RV = 0).  No listener change.  Window 0 (the program) is never paged.
               Reply COMM12: 1 done, 0 refused.  docs: CD32X_SRAM_PAGING_PROPOSAL.md */
            int page = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFFFF;
#if OL_CART_ED
            /* CART-ED (CART=ED bash build.sh): the Mega EverDrive PRO, booted with EDBOOT.BIN, as the
               RAM cart.  Its extended SSF mapper has 512 KB windows: window 1 of the 16MB-PCB design
               (cart 2-4 MB) is the EverDrive's windows 4-7, CTRL4-7 at 0xA130F8-0xA130FE.  Page p =
               banks 4p .. 4p+3, so pages 1-6 are banks 4-27; page 7 (banks 28-31: the EverDrive's save
               area and its own software) is refused.  The four registers are written as words by the
               existing listener routine 27 (safe_cart_xfer_w: interrupts off, RV = 1, word copy,
               RV = 0) - the write edtest proved on the console.  No listener change. */
            int ok = page >= 1 && page <= 6;
            if (ok) {
                int k;
                for (k = 0; k < 4; k++) { PB(2 * k) = 0; PB(2 * k + 1) = (uint8_t)(4 * page + k); }
                switch_banks();                                   /* Word RAM -> 68000 */
                do_md_cmd3(MD_CMD_SAFE_CART_XFER_W, XM(0), 0xA130F8, 8);   /* CTRL4-7 */
                switch_banks();
            }
#else
            int ok = page >= 1 && page <= 7;
            if (ok) {
                PB(0) = (uint8_t)page;
                switch_banks();                                   /* Word RAM -> 68000 */
                cx(XM(0), 0xA130F2, 1);                           /* src, dst, 1 byte */
                switch_banks();
            }
#endif
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, ok);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
#endif
        else if (cmd == 53) {
            /* K33 - CMD 53 "load sound effects": KOBOSFX.BIN -> PCM chip.  Reply in COMM12:
               number of effects (0 = failed). */
            char sfx_name[16];                              /* OL-6: COMM2 = level */
            sprintf(sfx_name, "SFX%02d.BIN", do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 0xFF);
            int nfx = pcm_load(sfx_name);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA1512C, nfx);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 52) {
            /* KOBO K19 - CMD 52 "Genesis text layer on/off": COMM2 = 1 on, 0 off.
               Switches the Genesis display itself (VDP register 1 bit 6) - off blanks ALL
               Genesis text (green diagnostics and Chilly's white messages) to black and the
               32X picture shows alone; on brings everything back (it stays in video RAM).
               WHY: the 32X priority bit alone did not hide the text reliably (K17/K18). */
            int on = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122) & 1;
#if KOBO_DIAG_TEXT
            on = 1;                                     /* EDSAVE-D: the diagnostic launcher keeps the text on */
#endif
            do_md_cmd1(MD_CMD_SET_VDP_REG, on ? 0x8174 : 0x8134);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 48) {
            /* (K5: normally handled by the fast path at the top of the loop)
               KOBO M1 - CMD 48 "read pad": reply = pad 1 bits (SEGA_CTRL_*) in COMM8.
               WHY a command: the SH2 must never poll a comm register the 68K writes
               (32X manual p.69, and the COMM6 crash).  GET_PAD(0) is the copy the 68K's
               vblank handler keeps in its comm word - no extra 68K work. */
            uint16_t pad = GET_PAD(0);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15128, pad);
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
        else if (cmd == 45) {
            int track = do_md_cmd1(MD_CMD_READ_32X_16, 0xA15122);
            int once = track & 0x8000;                  /* OL-4: bit 15 = play once */
            track &= 0x7FFF;
            sprintf(tbuf, "CDDA: PLAY TRACK %d (REPEAT)        ", track);
            print_diag(temp, tbuf, 2, 27);
            
            {
                int st = 0;
                int tries = cdda_start(track, &st, once);
                sprintf(tbuf, "CDDA T%d ST %02X TRY %d%s          ", track, (st >> 24) & 0xFF,
                        tries, tries ? "" : " FAILED");
                print_diag(temp, tbuf, 2, 27);
            }
            do_md_cmd2(MD_CMD_WRITE_32X_16, 0xA15120, 0);
        }
    }
    return 0;
}