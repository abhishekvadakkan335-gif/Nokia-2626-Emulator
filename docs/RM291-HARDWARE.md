# RM-291 hardware research

## Evidence and limitations
- Nokia 2626 product code RM-291; related 2610 products RH-86/RH-87 are not interchangeable by assumption. Source: Nokia 2610/2626 service manual (https://www.manualslib.com/manual/816506/Nokia-2626.html).
- RM-291 schematic sheet 3 identifies UPP4M V4.3 KFAB, UEMCLITE V3.0, and a combined 128 Mbit NOR / 16 Mbit PSRAM component (16 MiB flash and 2 MiB RAM). Sheet 2 shows a keypad matrix and LCD control signals. Source: https://altehandys.de/downloads/ser-no-2626-schematics.pdf . Parsed text was available; page screenshot could not be retrieved.
- Repair logs report DCT4, ASIC ID 0x3040, and filenames such as rm291__06.81.mcusw, .ppm_w, .image_w. Secondary evidence: https://forum.gsmhosting.com/vbb/5027475-post470.html and https://forum.gsmhosting.com/vbb/f350/solve-your-nokia-problem-within-24-hours-585214-print/index161.html .
- ARM7TDMI/ARMv4T is a working family-level hypothesis based on related DCT4 hardware, not a verified RM-291 CPU model. A Nokia 6100 research project discusses firmware extraction and memory addressing: https://github.com/sp00nznet/nokia-recomp . Its addresses must not be assumed to apply to RM-291.
- No original RM-291 binary has been examined yet. Endianness, flash container structure, address ranges, entry point and MMIO register behavior remain unverified. Current Unicorn tests use synthetic little-endian ARM instructions and synthetic memory regions.

## Next steps
1. Obtain a lawfully accessible RM-291 MCU file and matching PPM/CNT if available. Record versions, sizes and hashes.
2. Inspect actual container records and determine flash offsets and byte order; validate any proposed parser against real sample boundaries.
3. Identify instruction set and reset/entry points by disassembly and comparison with hardware evidence.
4. Implement a model-specific flash/RAM map and bounded trace of unmapped peripheral accesses.
5. Model timers, interrupts, display, keypad and other necessary peripherals incrementally. Do not claim firmware boot until original firmware executes and produces observable results.

The app only executes the recognized eight-byte synthetic sample. Filename classification is advisory. Firmware is not bundled and no real handset flashing is performed.
