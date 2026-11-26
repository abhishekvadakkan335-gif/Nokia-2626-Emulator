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

## Stage 27: raw flash alias and pointer provenance
- Reconstructed the raw flash image from the original MCU container (551 checksum-verified records), mapped read-only at 0x09000000 in the local Python probe. Raw image SHA-256: 3fb91d33e43843534c04f8b52203282379d37caeca4fe28432e89bfbb072c64c.
- Run still stops at 0x00840000 after 931685 callbacks. Guest initialized the pointer at 0x2244 to 0x00840001 via byte writes at PCs 0x0121fb24 and 0x0121fb50.
- At the call R0=0x0001ebd4, R1=0x0900003a, R2=0x28, R3=0x00840001. Static inspection shows the function pointer is also used to copy buffers; precise ROM behavior remains unknown.
- All 40 source bytes at raw flash offset 0x3a in the update container are FF; actual handset contents are unknown. Do not invent device-specific data.
- Nokia 2610 RH-86 repair logs also identify ASIC 0x3040: https://forum.gsmhosting.com/vbb/f311/successfully-unlocked-flashed-phones-hwk-798344-print/index61.html . A matching ASIC ID is not proof of ROM compatibility.
- The raw image and ROM are not distributed. A compatible dump or independently verified implementation is still needed.
