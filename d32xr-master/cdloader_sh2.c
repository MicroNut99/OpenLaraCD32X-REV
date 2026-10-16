#include <stddef.h>
#include <stdint.h>
/* SELF-CONTAINED: no D32XR headers.  This loader is built with its OWN CD-boot start-up
   (cdl_crt0.s - with the 56-byte CD boot table the 32X boot ROM reads in CD mode) and its own
   linker script (cdloader.ld).  D32XR's crt0.s / linker scripts are for a cartridge: built with
   them, the SH2 started inside D32XR's cartridge start-up and never reached main() (console
   test 1: "SH2 READY FOR ISOLATED TEST: FFFF"). */
#define MARS_SYS_INTMSK  (*(volatile uint16_t *)0x20004000)
#define MARS_SYS_COMM0   (*(volatile uint16_t *)0x20004020)

/* ===========================================================================
 * D32XR CD32X - MILESTONE 1: THE SH2 LOADER  (sh2_main.c, master + slave SH2)
 *
 * Booted by our proven CD32X start-up (frame buffer, "_CD_").  It turns the RAM cart into
 * D32XR's cartridge ROM and then does what the 32X boot ROM does for a real cartridge:
 *   1. start-up handshake with the Sub-CPU (COMM10 0x0AAA / 0x0BBB), as in Kobo;
 *   2. CMD 38 (file 20 = D32XR.32X) + CMD 46 per 32 KB chunk -> RAM cart at offset 0 (the
 *      SH2 sees it at 0x02000000 = where D32XR expects its ROM).  Every chunk is verified by
 *      the Sub-CPU; a loading bar shows the progress;
 *   3. reads the 32X header from the cart (0x3D4: source, destination, size, entries, VBRs);
 *   4. copies a small assembly routine to the top of SDRAM (TRAMP) and parks the slave there;
 *   5. CMD 60: the Sub-CPU makes the 68000 jump to D32XR's Genesis code (0x880800);
 *   6. jumps into the routine: it copies D32XR's SDRAM part over THIS program (that is why it
 *      runs from the top of SDRAM), writes "M_OK" / "S_OK" (D32XR's 68K start-up waits for
 *      them), and starts both SH2s at D32XR's entries with their VBR and stack.
 * Status codes (COMM12, shown by the Sub-CPU as STATE:xxxx in the green text):
 *   4D01 opening the file   4D06 quick check (is it already in the cart?)   4D07 yes - copy
 *   skipped   4C00..4C3F chunk n loaded (progress)   4D02 loaded + verified   4D03 header OK   4D04 slave parked
 *   4D05 68K started -> jump.   Errors: 4DE1 file not found, 4DE2 bad chunk(s), 4DE3 not a
 *   32X ROM / bad header, 4DE4 slave did not park.
 * =========================================================================== */

__attribute__((used)) const char sh2_build_marker[] = "SH2-D32XR-CD-M1-LOADER";

void pri_vbi_handler(void) {}
#define SC_CMD_CLR       (*(volatile uint16_t *)0x2000401A)
void pri_cmd_handler(void) { SC_CMD_CLR = 0; }
void sec_cmd_handler(void) { SC_CMD_CLR = 0; }
void sec_dma1_handler(void) {}
volatile unsigned mars_pwdt_ovf_count = 0;
volatile unsigned mars_swdt_ovf_count = 0;
uint32_t _text_end;
uint32_t _data_size;
uint32_t _bss_start;
uint32_t _bss_end;
int main(void);
void _main(void) { main(); }

/* no C library (-nostdlib): our own, volatile so GCC cannot turn them into calls to themselves */
void *memset(void *d, int c, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)d;
    while (n--) *p++ = (uint8_t)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n) {
    volatile uint8_t *p = (volatile uint8_t *)d;
    const volatile uint8_t *q = (const volatile uint8_t *)s;
    while (n--) *p++ = *q++;
    return d;
}
void *memmove(void *d, const void *s, size_t n) { return memcpy(d, s, n); }

/* ---- registers ---- */
#define SC_COMM2         (*(volatile uint16_t *)0x20004022)
#define SC_COMM8_32      (*(volatile uint32_t *)0x20004028)
#define SC_COMM10        (*(volatile uint16_t *)0x2000402A)
#define SC_COMM12        (*(volatile uint16_t *)0x2000402C)   /* 16-bit: COMM14 = Sub heartbeat */
#define SC_SYS_ADAPTER_B (*(volatile uint8_t  *)0x20004000)
#define SC_VDP_DISPMODE  (*(volatile uint16_t *)0x20004100)
#define SC_VDP_FBCTL     (*(volatile uint16_t *)0x2000410A)
#define SC_CRAM          ((volatile uint16_t *)0x20004200)
#define SC_FRAMEBUFFER   ((volatile uint16_t *)0x24000000)

#define CMD_OPEN_FILE      38
#define CMD_CHUNK_TO_CART  46
#define CMD_CHUNK_COMPARE  47                /* M1c: compare a chunk, no write */
#define CMD_START_D32XR    60
#define FILE_D32XR         20
#define CHUNK_BYTES        32768
#define ROM_MAX            0x400000          /* the whole 4 MB cart */
#define CART               ((volatile uint8_t *)0x22000000)   /* cart, cache-through */

/* The hand-over routine and its parameter block live at the top of SDRAM, cache-through
   (0x26...), above D32XR's 28 KB start image and below the stacks. */
#define TRAMP_ADDR   0x2603C000u
#define INFO         ((volatile uint32_t *)0x2603DF00u)
/* INFO[0] go (slave)  [1] master entry  [2] master VBR  [3] slave entry  [4] slave VBR
   [5] park request (master -> slave)  [6] parked (slave -> master) */

static void show_status(uint16_t code) { SC_COMM12 = code; }

static void comm_pause(void) {           /* HW6 rule: never hammer a COMM register */
    volatile int k;
    for (k = 0; k < 64; k++) { }
}
static void wait_cmd(uint16_t cmd) {
    uint32_t n;
    MARS_SYS_COMM0 = cmd;
    for (n = 0; n < 2000000UL && MARS_SYS_COMM0 != 0; n++) comm_pause();
}
static void take_fb(void) {
    SC_SYS_ADAPTER_B = 0x80;
    while ((MARS_SYS_INTMSK & 0x8000) == 0) { }
}
static void flip_wait(void) {
    uint16_t cur = SC_VDP_FBCTL & 1;
    SC_VDP_FBCTL = cur ^ 1;
    while ((SC_VDP_FBCTL & 1) == cur);
}

static void halt(uint16_t code) {
    show_status(code);                   /* stays on screen as STATE:4DEx */
    while (1) { }
}

/* ---- the hand-over routine (position-independent SH2 code, copied to TRAMP_ADDR) ----
   tramp_master(r4 = source in the cart, r5 = destination in SDRAM (cache-through),
                r6 = number of longs, r7 = INFO)
   tramp_slave(r4 = INFO): waits for INFO[0] == 1, then starts the slave the same way. */
extern char tramp_start[], tramp_master[], tramp_slave[], tramp_end[];
__asm__(
"        .text\n"
"        .align  2\n"
"        .global _tramp_start, _tramp_master, _tramp_slave, _tramp_end\n"
"_tramp_start:\n"
"_tramp_master:\n"
"        mov.l   @r4+,r0\n"              /* copy D32XR's SDRAM image */
"        mov.l   r0,@r5\n"
"        dt      r6\n"
"        bf/s    _tramp_master\n"
"        add     #4,r5\n"
"        mov.l   tr_ccr,r1\n"            /* purge + enable the cache */
"        mov     #0x11,r0\n"
"        mov.b   r0,@r1\n"
"        mov.l   tr_comm0,r1\n"          /* what the 32X boot ROM leaves for the 68000 */
"        mov.l   tr_mok,r0\n"
"        mov.l   r0,@r1\n"               /* COMM0/2 = \"M_OK\" */
"        mov.l   tr_sok,r0\n"
"        mov.l   r0,@(4,r1)\n"           /* COMM4/6 = \"S_OK\" */
"        mov     #1,r0\n"
"        mov.l   r0,@r7\n"               /* release the slave */
"        mov.l   @(8,r7),r1\n"           /* master VBR */
"        ldc     r1,vbr\n"
"        mov.l   @(4,r1),r15\n"          /* stack = vector 1 of D32XR's table */
"        mov.l   @(4,r7),r0\n"           /* master entry */
"        jmp     @r0\n"
"        nop\n"
"_tramp_slave:\n"
"1:      mov.l   @r4,r0\n"
"        cmp/eq  #1,r0\n"
"        bf      1b\n"
"        mov.l   tr_ccr,r1\n"
"        mov     #0x11,r0\n"
"        mov.b   r0,@r1\n"
"        mov.l   @(16,r4),r1\n"          /* slave VBR */
"        ldc     r1,vbr\n"
"        mov.l   @(4,r1),r15\n"
"        mov.l   @(12,r4),r0\n"          /* slave entry */
"        jmp     @r0\n"
"        nop\n"
"        .align  2\n"
"tr_ccr:   .long   0xFFFFFE92\n"
"tr_comm0: .long   0x20004020\n"
"tr_mok:   .ascii  \"M_OK\"\n"
"tr_sok:   .ascii  \"S_OK\"\n"
"_tramp_end:\n"
);
typedef void (*tramp_m_fn)(uint32_t src, uint32_t dst, uint32_t nlongs, volatile uint32_t *info);
typedef void (*tramp_s_fn)(volatile uint32_t *info);

/* the slave: park until the master asks, then wait inside the routine at the top of SDRAM
   (this program's own code is overwritten by D32XR's start image) */
void secondary(void) {
    __asm__ __volatile__("ldc %0,sr" : : "r"(0xF0) : "memory");    /* interrupts off */
    while (INFO[5] != 0x5A5A5A5Au) { comm_pause(); }
    INFO[6] = 0x5A5A5A5Au;                                        /* parked */
    ((tramp_s_fn)(TRAMP_ADDR + (uint32_t)(tramp_slave - tramp_start)))(INFO);
    while (1) { }
}

static uint32_t rd32(uint32_t off) {     /* big-endian long from the cart */
    return ((uint32_t)CART[off] << 24) | ((uint32_t)CART[off + 1] << 16) |
           ((uint32_t)CART[off + 2] << 8) | CART[off + 3];
}

int main(void) {
    uint32_t len, nchunks, c, n;
    int bad = 0;
    INFO[5] = 0; INFO[6] = 0;

    /* ---- start-up: identical to the working Kobo/V80 engine ---- */
    MARS_SYS_COMM0 = 0;
    SC_COMM10 = 0x0006;
    MARS_SYS_INTMSK |= 0x0002;
    SC_COMM10 = 0x0007;
    __asm__ __volatile__("ldc %0,sr" : : "r"(0) : "memory");
    SC_COMM10 = 0x0AAA;
    while (SC_COMM10 != 0x0BBB) { }
    take_fb();
    /* NOTHING is drawn on the 32X layer: its display stays BLANK (mode 0), so the Genesis
       layer - Chilly's white boot messages and our green diagnostics - is never covered.
       (The first version drew a loading bar in 256-colour mode; in Fusion that picture hid
       all the boot text.)  Progress = the status code, shown by the Sub-CPU as STATE:. */
    SC_VDP_DISPMODE = 0x0000;
    /* Genesis text layer ON (as Kobo's set_text): CMD 52 with COMM2 = 1 makes the Sub-CPU set
       VDP register 1 = 0x8174 (display on).  Without it the status codes were not visible
       (Fusion test: black screen with only the loading bar). */
    SC_COMM2 = 1;
    wait_cmd(52);
    show_status(0x4D01);

    /* ---- 1. the ROM image into the cart ---- */
    SC_COMM2 = FILE_D32XR;
    wait_cmd(CMD_OPEN_FILE);
    len = SC_COMM8_32;
    if ((int32_t)len <= 0) halt(0x4DE1);
    if (len > ROM_MAX) len = ROM_MAX;
    SC_COMM8_32 = len;
    nchunks = (len + CHUNK_BYTES - 1) >> 15;

    /* QUICK CHECK (dev idea): is this file already in the cart from an earlier boot?  Compare
       the first, middle and last chunks (CD vs cart, nothing written).  All equal -> skip the
       copy (boot in seconds).  Any difference -> copy everything as before.  (A "force copy"
       option can come later.) */
    {
        uint32_t probe[3];
        int same = 1, i;
        probe[0] = 0; probe[1] = nchunks >> 1; probe[2] = nchunks - 1;
        show_status(0x4D06);                                         /* checking the cart */
        for (i = 0; i < 3 && same; i++) {
            SC_COMM2 = (uint16_t)probe[i];
            SC_COMM12 = 0;
            wait_cmd(CMD_CHUNK_COMPARE);
            if (SC_COMM12 != 0) same = 0;
        }
        if (same) { show_status(0x4D07); goto loaded; }              /* already there */
    }

    for (c = 0; c < nchunks; c++) {
        SC_COMM2 = (uint16_t)c;
        SC_COMM12 = 0;                   /* the Sub-CPU replies 0 = chunk verified */
        wait_cmd(CMD_CHUNK_TO_CART);
        if (SC_COMM12 != 0) bad++;
        /* progress in the green text: STATE:4C00 .. 4C3F (chunk number; 64 chunks = 2 MB) */
        show_status((uint16_t)(0x4C00 | (c & 0xFF)));
    }
    if (bad) halt(0x4DE2);
loaded:
    show_status(0x4D02);

    /* ---- 2. the cartridge's 32X header (what the 32X boot ROM reads) ---- */
    if (CART[0x100] != 'S' || CART[0x101] != 'E' || CART[0x102] != 'G' || CART[0x103] != 'A') halt(0x4DE3);
    {
        uint32_t src = rd32(0x3D4), dst = rd32(0x3D8), size = rd32(0x3DC);
        uint32_t ment = rd32(0x3E0), sent = rd32(0x3E4), mvbr = rd32(0x3E8), svbr = rd32(0x3EC);
        if (size == 0 || size > 0x30000 || dst + size > 0x38000 ||
            (ment >> 24) != 0x06 || (sent >> 24) != 0x06) halt(0x4DE3);
        INFO[0] = 0; INFO[1] = ment; INFO[2] = mvbr; INFO[3] = sent; INFO[4] = svbr;
        show_status(0x4D03);

        /* ---- 3. the hand-over routine to the top of SDRAM ---- */
        {
            volatile uint8_t *t = (volatile uint8_t *)TRAMP_ADDR;
            const char *p = tramp_start;
            while (p < tramp_end) *t++ = (uint8_t)*p++;
        }
        /* ---- 4. park the slave there ---- */
        INFO[5] = 0x5A5A5A5Au;
        for (n = 0; n < 3000000UL && INFO[6] != 0x5A5A5A5Au; n++) comm_pause();
        if (INFO[6] != 0x5A5A5A5Au) halt(0x4DE4);
        show_status(0x4D04);

        /* ---- 5. the 68000 starts D32XR's Genesis code ---- */
        show_status(0x4D05);
        wait_cmd(CMD_START_D32XR);

        /* ---- 6. copy D32XR's start image over this program and start both SH2s ---- */
        __asm__ __volatile__("ldc %0,sr" : : "r"(0xF0) : "memory");   /* interrupts off */
        ((tramp_m_fn)(TRAMP_ADDR + (uint32_t)(tramp_master - tramp_start)))(
            0x22000000u + src, 0x26000000u + dst, (size + 3) >> 2, INFO);
    }
    while (1) { }
    return 0;
}
