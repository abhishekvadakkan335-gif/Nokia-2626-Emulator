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

## Stage 29: isolated, opt-in HLE copy experiment
- Reproduced strict missing-ROM baseline with 931685 callbacks on local Unicorn 2.1.4.
- A single exploratory HLE operation was tested at the exact observed call to 0x00840000 (LR 0x0109c0ad, R0=0x1ebd4, R1=0x0900003a, R2=40). It copied the unmodified source bytes to the requested guest destination and resumed at the caller. No ROM bytes, success flags or identity data were fabricated. This copy behavior is a hypothesis, not verified ROM semantics.
- Result: 931777 callbacks; second unresolved ROM fetch at 0x0084000e, LR 0x0109c06d, R0=0x1ebd4, R1=0x28, R2=0x1ec24, R3=0x20. The second routine is not implemented. Caller code later computes and compares a checksum, but the actual ROM transformation remains unknown.
- Running without the optional HLE switch preserves the earlier stop exactly. The experiment does not establish that any ROM function was faithfully implemented or the OS booted.

## Stage 30: ROM caller verification and stop condition
- Re-ran both the unchanged host baseline and the single opt-in copy experiment against the SHA-256-pinned 06.80 MCU image using the local Unicorn host library. Exact results: 931685 callbacks / unmapped fetch 0x00840000; 931777 callbacks / unmapped fetch 0x0084000e. Both were UC_ERR_FETCH_UNMAPPED. The original firmware was not modified.
- Disassembled the guest caller and the checksum helper with LLVM's big-endian Thumb target. The caller inspects the 40-byte buffer following the missing ROM transformation and checks its integrity before using the contents. The transformation itself remains unknown. The update package contains erased bytes in the corresponding raw flash area, not an authenticated handset-specific record.
- Further automatic substitutions are not a verified implementation. No new HLE replacement added, no success values fabricated, and no new APK is needed. The next substantive dependency is an identified compatible UPP4M ROM or independently documented compatible implementation, with appropriate local flash data. Do not distribute handset identity records or proprietary ROM bytes in this public repository.

## Stage 32: separate Samsung flash identification experiment
- Reference RM-291 repair log reports Samsung K8S2815ETB, manufacturer/device ID 00EC/22E8 (https://forum.gsmhosting.com/vbb/5027475-post470.html). The related K8S2815ETE datasheet describes autoselect (https://dtsheet.com/doc/345355/samsung-k8s2815ete). Exact silicon revision and full command set are unverified.
- Optional host-only experiment recognizes the observed AA/55/90 halfword command sequence at 0x01ee0aaa, 0x01ee0554, 0x01ee0aaa and responds at raw-flash alias 0x09ee0000 and +2 with byte-swapped callback values EC00/E822. Reset FF/F0 returns the model to array mode. No program/erase or CFI support.
- Guest debug output includes 00 EC 22 E8, and firmware takes a different initialization path. It stops at 924067 callbacks (UC_ERR_WRITE_PROT), Thumb PC 0x000002d4, write value 0x00aa, size 2, destination 0x01000aaa. This is the next main-bank command write and remains blocked; do not turn firmware flash into unrestricted writable RAM.
- Without the optional model the prior 931685 callback / 0x00840000 stop reproduces exactly. Both original container and decoded flash SHA-256 verified unchanged. The new path has not yet reached the ROM dependency; this experiment does not resolve the missing ROM or prove OS boot.

## Stage 33: NOR configuration experiment and four-path regression
- Samsung K8S2815ETE-family datasheet documents a three-cycle burst configuration command, AA/55/C0 with the configuration encoded in the third address: https://dtsheet.com/doc/345355/samsung-k8s2815ete . Exact ETB revision remains unverified.
- Locally tested an optional narrow model for the observed Thumb STRH r2,[r1] at 0x2d4. Three write-protection stops were handled as configuration command cycles, not array writes: 0x01000aaa/AA, 0x01000554/55, 0x01004aaa/C0. Recorded address code 4. No guest code patch, no writable flash, no erase/program support or burst timing model. Other writes remain blocked.
- Four independent regression paths passed: baseline 931685 callbacks -> 0x00840000; NOR ID only 924067 -> protected write 0x01000aaa; NOR ID + configuration 931585 -> 0x00840000; configuration + prior opt-in copy hypothesis 931677 -> 0x0084000e. Both image views retained their original SHA-256 in every run. The configuration-enabled path produced the original firmware diagnostic "HW init done, entering main()."
- A local v33 regression bundle contains the actual Python probe, decoder, automated runner and logs (no proprietary images). Android code was not changed in this experiment. Missing compatible ROM or verified function behavior remains the hard blocker; no full boot or LCD functionality was demonstrated.
