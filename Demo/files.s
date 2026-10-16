.text

        .align  4
        .global sh2_app_start
sh2_app_start:
        /* the SH2 program (D32XR CD loader), renamed and refreshed by the Makefile on every build
           (ROMS/$(SH2_PAYLOAD)).  Relative path: the old line pointed at the J: folder. */
        .incbin "ROMS/CDLOADER.BIN"

        .align  4
sh2_app_end:

        .global sh2_app_length
sh2_app_length:
        .long   sh2_app_end - sh2_app_start

