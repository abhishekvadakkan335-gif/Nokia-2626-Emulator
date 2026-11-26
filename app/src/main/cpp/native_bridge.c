#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <unicorn/unicorn.h>

/* Synthetic diagnostic map only, not Nokia hardware addresses. */
#define ROM_BASE 0x1000u
#define RAM_BASE 0x2000u
#define PAGE_SIZE 0x1000u

JNIEXPORT jstring JNICALL
Java_com_retro_n2626_NativeEngine_testCPU(JNIEnv *env, jclass cls) {
    (void)cls;
    uc_engine *uc = NULL;
    uc_err err = UC_ERR_OK;
    uint32_t r0 = 0, r2 = 0, r3 = 0;
    uint32_t ram_addr = RAM_BASE, rom_addr = ROM_BASE + 0x100;
    unsigned char ram[4] = {0}, rom[4] = {0x78,0x56,0x34,0x12};
    int cpu_ok = 0, ram_ok = 0, rom_ok = 0, protect_ok = 0;
    const char *stage = "open";
    char message[512];
    const unsigned char code[] = {
        0x01,0x00,0xa0,0xe3, /* MOV R0,#1 */
        0x02,0x00,0x80,0xe2, /* ADD R0,R0,#2 */
        0x00,0x00,0x81,0xe5, /* STR R0,[R1] */
        0x00,0x20,0x91,0xe5, /* LDR R2,[R1] */
        0x00,0x30,0x94,0xe5, /* LDR R3,[R4] */
        0x00,0x00,0x84,0xe5  /* STR R0,[R4]: must fault */
    };
#define CHECK(label, call) do { stage = label; err = (call); if (err != UC_ERR_OK) goto done; } while (0)
    CHECK("open", uc_open(UC_ARCH_ARM, UC_MODE_ARM, &uc));
    CHECK("ROM map", uc_mem_map(uc, ROM_BASE, PAGE_SIZE, UC_PROT_ALL));
    CHECK("RAM map", uc_mem_map(uc, RAM_BASE, PAGE_SIZE, UC_PROT_READ | UC_PROT_WRITE));
    CHECK("load code", uc_mem_write(uc, ROM_BASE, code, sizeof(code)));
    CHECK("load ROM data", uc_mem_write(uc, rom_addr, rom, sizeof(rom)));
    CHECK("protect ROM", uc_mem_protect(uc, ROM_BASE, PAGE_SIZE, UC_PROT_READ | UC_PROT_EXEC));
    CHECK("RAM pointer", uc_reg_write(uc, UC_ARM_REG_R1, &ram_addr));
    CHECK("ROM pointer", uc_reg_write(uc, UC_ARM_REG_R4, &rom_addr));
    CHECK("execute ARM", uc_emu_start(uc, ROM_BASE, ROM_BASE+20, 1000000, 5));
    CHECK("read R0", uc_reg_read(uc, UC_ARM_REG_R0, &r0));
    cpu_ok = (r0 == 3);
    CHECK("read R2", uc_reg_read(uc, UC_ARM_REG_R2, &r2));
    CHECK("read RAM", uc_mem_read(uc, RAM_BASE, ram, sizeof(ram)));
    ram_ok = r2 == 3 && ram[0] == 3 && ram[1] == 0 && ram[2] == 0 && ram[3] == 0;
    CHECK("read R3", uc_reg_read(uc, UC_ARM_REG_R3, &r3));
    rom_ok = (r3 == 0x12345678u);
    stage = "ROM write protection";
    err = uc_emu_start(uc, ROM_BASE+20, ROM_BASE+24, 1000000, 1);
    protect_ok = (err == UC_ERR_WRITE_PROT);
    if (protect_ok) err = UC_ERR_OK;
    if (err != UC_ERR_OK) goto done;
    CHECK("verify ROM data", uc_mem_read(uc, rom_addr, rom, sizeof(rom)));
    rom_ok = rom_ok && rom[0] == 0x78 && rom[1] == 0x56 && rom[2] == 0x34 && rom[3] == 0x12;
    stage = "complete";
done:
    snprintf(message, sizeof(message),
        "ARM CPU: %s | R0 = %u\nVirtual RAM: %s\nVirtual ROM read: %s\nROM write protection: %s\nStage: %s (%s)",
        cpu_ok ? "PASS" : "FAIL", (unsigned)r0,
        ram_ok ? "PASS" : "FAIL", rom_ok ? "PASS" : "FAIL",
        protect_ok ? "PASS" : "FAIL", stage, uc_strerror(err));
    if (uc) uc_close(uc);
    return (*env)->NewStringUTF(env, message);
#undef CHECK
}

/* Only the known eight-byte diagnostic is executable in this milestone. */
JNIEXPORT jstring JNICALL
Java_com_retro_n2626_NativeEngine_runImportedTest(JNIEnv *env, jclass cls, jbyteArray input) {
    (void)cls;
    const unsigned char expected[]={1,0,0xa0,0xe3,2,0,0x80,0xe2};
    unsigned char code[8];
    if(!input || (*env)->GetArrayLength(env,input)!=8)
        return (*env)->NewStringUTF(env,"Rejected: only the known diagnostic sample is supported.");
    (*env)->GetByteArrayRegion(env,input,0,8,(jbyte*)code);
    if((*env)->ExceptionCheck(env)) return NULL;
    for(int i=0;i<8;i++) if(code[i]!=expected[i])
        return (*env)->NewStringUTF(env,"Rejected: unrecognized diagnostic instructions.");
    uc_engine *uc=NULL;
    uc_err err;
    uint32_t r0=0,pc=0;
    char msg[256];
    err=uc_open(UC_ARCH_ARM,UC_MODE_ARM,&uc);
    if(err!=UC_ERR_OK) goto finish;
    err=uc_mem_map(uc,ROM_BASE,PAGE_SIZE,UC_PROT_ALL);
    if(err!=UC_ERR_OK) goto finish;
    err=uc_mem_write(uc,ROM_BASE,code,sizeof(code));
    if(err!=UC_ERR_OK) goto finish;
    err=uc_mem_protect(uc,ROM_BASE,PAGE_SIZE,UC_PROT_READ|UC_PROT_EXEC);
    if(err!=UC_ERR_OK) goto finish;
    err=uc_emu_start(uc,ROM_BASE,ROM_BASE+8,1000000,2);
    if(err!=UC_ERR_OK) goto finish;
    err=uc_reg_read(uc,UC_ARM_REG_R0,&r0);
    if(err!=UC_ERR_OK) goto finish;
    err=uc_reg_read(uc,UC_ARM_REG_PC,&pc);
finish:
    snprintf(msg,sizeof(msg),"Imported ARM test: %s\nR0 = %u\nPC = 0x%08x\nEngine: %s\nNot Nokia firmware boot.",
        err==UC_ERR_OK&&r0==3&&pc==ROM_BASE+8?"PASS":"FAIL",(unsigned)r0,(unsigned)pc,uc_strerror(err));
    if(uc) uc_close(uc);
    return (*env)->NewStringUTF(env,msg);
}
