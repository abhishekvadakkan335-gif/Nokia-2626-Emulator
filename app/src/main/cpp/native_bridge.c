#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <unicorn/unicorn.h>

JNIEXPORT jstring JNICALL
Java_com_retro_n2626_NativeEngine_testCPU(
    JNIEnv *env, jclass cls) {

    uc_engine *uc = NULL;
    uc_err err;
    uint32_t result = 0;
    char message[256];

    unsigned char code[] = {
        0x01, 0x00, 0xa0, 0xe3,
        0x02, 0x00, 0x80, 0xe2
    };

    err = uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc);
    if (err != UC_ERR_OK)
        goto done;

    err = uc_mem_map(uc, 0x1000, 0x1000, UC_PROT_ALL);
    if (err != UC_ERR_OK)
        goto done;

    err = uc_mem_write(uc, 0x1000, code, sizeof(code));
    if (err != UC_ERR_OK)
        goto done;

    err = uc_emu_start(uc, 0x1000, 0x1008, 0, 2);
    if (err != UC_ERR_OK)
        goto done;

    err = uc_reg_read(uc, UC_ARM_REG_R0, &result);

done:
    snprintf(message, sizeof(message),
        "ARM CPU test: %s | R0 = %u",
        err == UC_ERR_OK && result == 3
        ? "PASS" : "FAIL", result);

    if (uc)
        uc_close(uc);

    return (*env)->NewStringUTF(env, message);
}
