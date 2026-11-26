# RM-291 host exploration: missing internal ROM

## Reproduced local baseline
- Decoded flash SHA-256: 1909ad142c0609e53596c2affdf5c23fc8e00a37f87a16718b488e6ea5beaddf.
- Exploratory host run: 931685 instruction callbacks. Firmware diagnostic: "HW init done, entering main()."
- Stop: UC_ERR_FETCH_UNMAPPED at 0x00840000; Thumb state; last instruction at 0x01406a92; LR 0x0109c0ad.
- Not a completed boot. Hardware responses, flash identification and RAM mappings are speculative.

## Evidence
- Historical DCT4 memory research identifies the ROM region: https://forum.gsmhosting.com/vbb/994801-post1.html
- DCT4+ research on Nokia 1208 locates separate on-chip ROM at 0x00840000 and describes its absence from update packages: https://forsooth.org/pocorgtfo/pocorgtfo03.pdf
- RM-291 repair log identifies ASIC 0x3040: https://forum.gsmhosting.com/vbb/5027475-post470.html

Neither memory-map source verifies the exact ROM revision or the particular routine on RM-291. A normal MCU/PPM/CNT package is not a confirmed substitute. Obtain a lawfully available compatible raw ROM image with provenance, base, size and hash. Do not fabricate missing code or claim the routine succeeded. Keep proprietary firmware out of the public repository.
