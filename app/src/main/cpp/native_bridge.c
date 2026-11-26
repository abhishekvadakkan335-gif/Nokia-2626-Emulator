#include <jni.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
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

#define FW_BASE 0x01000000u
#define FW_ENTRY 0x01000100u
typedef struct {unsigned count; uint64_t pc[24],fault; uint32_t size[24]; int type,width; bool faulted;} fw_trace;
static void fw_code(uc_engine *uc,uint64_t address,uint32_t size,void *data){
    (void)uc; fw_trace *t=data;
    if(t->count<24){t->pc[t->count]=address;t->size[t->count]=size;}t->count++;
}
static bool fw_invalid(uc_engine *uc,uc_mem_type type,uint64_t address,int size,int64_t value,void *data){
    (void)uc;(void)value;fw_trace *t=data;t->faulted=true;t->fault=address;t->type=type;t->width=size;return false;
}
JNIEXPORT jstring JNICALL Java_com_retro_n2626_NativeEngine_traceFirmware(JNIEnv *env,jclass cls,jbyteArray input){
    (void)cls;
    if(!input||(*env)->GetArrayLength(env,input)!=0x896f84)return (*env)->NewStringUTF(env,"Unexpected flash image size.");
    const size_t length=0x896f84,mapped=(length+4095)&~(size_t)4095;
    uc_engine *uc=NULL;uc_err err;uc_hook h1,h2;fw_trace t={0};
    uint32_t pc=0,cpsr=0,sp=0;char msg[2400];const char *stage="open";
    err=uc_open(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN,&uc);if(err)goto done;
    stage="select experimental ARMv4T model";err=uc_ctl_set_cpu_model(uc,UC_CPU_ARM_TI925T);if(err)goto done;
    stage="map flash";err=uc_mem_map(uc,FW_BASE,mapped,UC_PROT_ALL);if(err)goto done;
    jbyte *bytes=(*env)->GetByteArrayElements(env,input,NULL);
    if(!bytes){uc_close(uc);return NULL;}
    stage="copy flash";err=uc_mem_write(uc,FW_BASE,bytes,length);
    (*env)->ReleaseByteArrayElements(env,input,bytes,JNI_ABORT);if(err)goto done;
    stage="protect flash";err=uc_mem_protect(uc,FW_BASE,mapped,UC_PROT_READ|UC_PROT_EXEC);if(err)goto done;
    stage="initialize CPSR";err=uc_reg_read(uc,UC_ARM_REG_CPSR,&cpsr);if(err)goto done;
    cpsr=(cpsr&~0x3fu)|0xd3u;err=uc_reg_write(uc,UC_ARM_REG_CPSR,&cpsr);if(err)goto done;
    stage="install hooks";err=uc_hook_add(uc,&h1,UC_HOOK_CODE,(void*)fw_code,&t,1,0);if(err)goto done;
    err=uc_hook_add(uc,&h2,UC_HOOK_MEM_INVALID,(void*)fw_invalid,&t,1,0);if(err)goto done;
    stage="guest execution";err=uc_emu_start(uc,FW_ENTRY,FW_BASE+length,1000000,64);
    uc_reg_read(uc,UC_ARM_REG_PC,&pc);uc_reg_read(uc,UC_ARM_REG_CPSR,&cpsr);uc_reg_read(uc,UC_ARM_REG_SP,&sp);
done:;
    size_t used=(size_t)snprintf(msg,sizeof(msg),"RM-291 startup probe\nEntry: 0x%08x\nExperimental big-endian ARMv4T\nInstruction callbacks: %u (includes faulting instruction)\nStage: %s\nEngine: %s\nPC: 0x%08x\nCPSR: 0x%08x\nSP: 0x%08x\n",FW_ENTRY,t.count,stage,uc_strerror(err),pc,cpsr,sp);
    if(t.faulted&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Unmapped/protected access: 0x%08" PRIx64 "\nType: %d, size: %d\n",t.fault,t.type,t.width);
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\nFirst instruction addresses (bytes):\n");
    for(unsigned i=0;i<t.count&&i<24&&used<sizeof(msg);i++)used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " (%u)\n",t.pc[i],t.size[i]);
    if(used<sizeof(msg))snprintf(msg+used,sizeof(msg)-used,"\nNo RAM or peripherals mapped. Not a completed OS boot.");
    if(uc)uc_close(uc);return (*env)->NewStringUTF(env,msg);
}
