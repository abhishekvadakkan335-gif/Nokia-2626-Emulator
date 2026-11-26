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
/* Hypothetical low RAM window containing the observed stack and write. */
#define FW_RAM_BASE 0x00010000u
#define FW_RAM_SIZE 0x00010000u
/* Observed memory-copy destination at 0x20; do not assume contiguous RAM. */
#define FW_ZERO_BASE 0x00000000u
#define FW_ZERO_SIZE 0x00001000u
/* Capacity from schematic; base inferred from observed two-halfword RAM test. */
#define FW_EXT_RAM_BASE 0x03000000u
#define FW_EXT_RAM_SIZE 0x00200000u
#define FW_MAX_INSNS 200000u
/* Exploratory MMIO region. 0x06009000 has speculative readback.
 * Two one-shot startup reads at 0x0600D00E/10 return hypothetical zero. */
#define PROBE_MMIO 0x06000000u
#define PROBE_MMIO_SIZE 0x20000u
/* Additional provisional page around observed 0x06100000 write. */
#define PROBE_MMIO2 0x06100000u
#define PROBE_MMIO2_SIZE 0x1000u
#define MAX_MMIO_WRITES 8192u
#define MAX_MMIO_READS 512u
#define TRACE_KEEP 16u
typedef struct {uint64_t addr,value,pc; unsigned width;} fw_write;
typedef struct {
    unsigned count,writes,reads;
    unsigned ram_reads,ram_writes,ext_reads,ext_writes;
    unsigned zero_reads,zero_writes;
    uint64_t zero_min,zero_max,zero_last_pc;
    bool zero_mapped;
    uint64_t ext_min,ext_max,ext_last_pc;
    bool ext_mapped;
    uint64_t ram_min,ram_max,ram_last_pc;
    bool ram_mapped;
    bool reg9000_valid,reg10002_valid,read_limit;
    unsigned reg10002_reads;
    bool d00e_seen,d010_seen,reg10000_seen;
    uint16_t reg9000,reg10002;
    uint64_t pc[24],recent_pc[TRACE_KEEP],fault,last,fault_value;
    uint32_t recent_size[TRACE_KEEP];
    uint32_t size[24];
    int type,width;
    bool faulted,unsupported_mmio,write_limit;
    fw_write first_writes[TRACE_KEEP],last_writes[TRACE_KEEP];
    fw_write first_reads[TRACE_KEEP],last_reads[TRACE_KEEP];
} fw_trace;
/* Keep first and last samples; total counters remain independent of display. */
static void fw_record(fw_write *first,fw_write *last,unsigned *count,fw_write event){
    unsigned i=*count;
    if(i<TRACE_KEEP)first[i]=event;
    last[i%TRACE_KEEP]=event;
    *count=i+1;
}
static uint64_t fw_mmio_read(uc_engine *uc,uint64_t offset,unsigned size,void *data){
    fw_trace *t=data;
    if(!t->faulted && offset==0x9000 && size==2 && t->reg9000_valid && t->reads<MAX_MMIO_READS){
        uint16_t value=t->reg9000;
        fw_record(t->first_reads,t->last_reads,&t->reads,(fw_write){PROBE_MMIO+offset,value,t->last,size});
        return value;
    }
    /* Observed read-modify-write of 0x06010002. Preserve raw callback
     * byte order, as with the existing 0x06009000 experiment.
     * Readback is a hypothesis; no physical register side effects modeled. */
    if(!t->faulted && offset==0x10002 && size==2 && t->reg10002_valid && t->reads<MAX_MMIO_READS){
        uint16_t value=t->reg10002;
        t->reg10002_reads++;
        fw_record(t->first_reads,t->last_reads,&t->reads,(fw_write){PROBE_MMIO+offset,value,t->last,size});
        return value;
    }
    /* These exact Thumb LDRH sites follow the observed D000 initialization.
     * No device semantics or reset register values are established. */
    if(!t->faulted && size==2 && t->reads<MAX_MMIO_READS){
        if(offset==0xd00e && t->last==0x01247610 && !t->d00e_seen){
            t->d00e_seen=true;
            fw_record(t->first_reads,t->last_reads,&t->reads,(fw_write){PROBE_MMIO+offset,0,t->last,size});
            return 0;
        }
        if(offset==0xd010 && t->last==0x01247612 && t->d00e_seen && !t->d010_seen){
            t->d010_seen=true;
            fw_record(t->first_reads,t->last_reads,&t->reads,(fw_write){PROBE_MMIO+offset,0,t->last,size});
            return 0;
        }
    }
    /* One-shot diagnostic hypothesis, not a verified reset/status value. */
    if(!t->faulted && offset==0x10000 && size==2 && t->last==0x0109bd5a && !t->reg10000_seen && t->reads<MAX_MMIO_READS){
        t->reg10000_seen=true;
        fw_record(t->first_reads,t->last_reads,&t->reads,(fw_write){PROBE_MMIO+offset,0,t->last,size});
        return 0;
    }
    t->faulted=true;t->unsupported_mmio=true;t->fault=PROBE_MMIO+offset;
    t->type=UC_MEM_READ;t->width=(int)size;
    t->read_limit=t->reads>=MAX_MMIO_READS;
    uc_emu_stop(uc);
    return 0; /* required placeholder; trace stops on unsupported reads */
}
static void fw_mmio_write(uc_engine *uc,uint64_t offset,unsigned size,uint64_t value,void *data){
    fw_trace *t=data;
    if(!t->faulted && t->writes<MAX_MMIO_WRITES && (size==1||size==2||size==4) && offset+size<=PROBE_MMIO_SIZE){
        fw_record(t->first_writes,t->last_writes,&t->writes,(fw_write){PROBE_MMIO+offset,value,t->last,size});
        if(offset==0x9000&&size==2){t->reg9000=(uint16_t)value;t->reg9000_valid=true;}
        else if(offset<0x9002 && offset+size>0x9000)t->reg9000_valid=false;
        if(offset==0x10002&&size==2){t->reg10002=(uint16_t)value;t->reg10002_valid=true;}
        else if(offset<0x10004 && offset+size>0x10002)t->reg10002_valid=false;
        return; /* all other writes are logged only */
    }
    t->faulted=true;t->unsupported_mmio=true;t->write_limit=t->writes>=MAX_MMIO_WRITES;
    t->fault=PROBE_MMIO+offset;t->fault_value=value;t->type=UC_MEM_WRITE;t->width=(int)size;
    uc_emu_stop(uc);
}
static uint64_t fw_mmio2_read(uc_engine *uc,uint64_t offset,unsigned size,void *data){
    fw_trace *t=data;
    t->faulted=true;t->unsupported_mmio=true;t->fault=PROBE_MMIO2+offset;
    t->type=UC_MEM_READ;t->width=(int)size;
    uc_emu_stop(uc);
    return 0; /* no assumed read response */
}
static void fw_mmio2_write(uc_engine *uc,uint64_t offset,unsigned size,uint64_t value,void *data){
    fw_trace *t=data;
    if(!t->faulted && t->writes<MAX_MMIO_WRITES && (size==1||size==2||size==4) && offset+size<=PROBE_MMIO2_SIZE){
        fw_record(t->first_writes,t->last_writes,&t->writes,(fw_write){PROBE_MMIO2+offset,value,t->last,size});
        return; /* diagnostic write sink, no modeled side effects */
    }
    t->faulted=true;t->unsupported_mmio=true;t->write_limit=t->writes>=MAX_MMIO_WRITES;
    t->fault=PROBE_MMIO2+offset;t->fault_value=value;t->type=UC_MEM_WRITE;t->width=(int)size;
    uc_emu_stop(uc);
}
static void fw_code(uc_engine *uc,uint64_t address,uint32_t size,void *data){
    fw_trace *t=data;
    if(t->faulted){uc_emu_stop(uc);return;}
    if(t->count<24){t->pc[t->count]=address;t->size[t->count]=size;}
    t->recent_pc[t->count%TRACE_KEEP]=address;
    t->recent_size[t->count%TRACE_KEEP]=size;
    t->count++;t->last=address;
}
static void fw_ram_access(uc_engine *uc,uc_mem_type type,uint64_t address,int size,int64_t value,void *data){
    (void)uc;(void)value;fw_trace *t=data;
    if(t->faulted||size<=0)return;
    if(t->ram_reads==0&&t->ram_writes==0){t->ram_min=address;t->ram_max=address+(uint64_t)size-1;}
    if(address<t->ram_min)t->ram_min=address;
    if(address+(uint64_t)size-1>t->ram_max)t->ram_max=address+(uint64_t)size-1;
    if(type==UC_MEM_READ)t->ram_reads++;
    if(type==UC_MEM_WRITE)t->ram_writes++;
    t->ram_last_pc=t->last;
}
static void fw_ext_ram_access(uc_engine *uc,uc_mem_type type,uint64_t address,int size,int64_t value,void *data){
    (void)uc;(void)value;fw_trace *t=data;
    if(t->faulted||size<=0)return;
    if(t->ext_reads==0&&t->ext_writes==0){t->ext_min=address;t->ext_max=address+(uint64_t)size-1;}
    if(address<t->ext_min)t->ext_min=address;
    if(address+(uint64_t)size-1>t->ext_max)t->ext_max=address+(uint64_t)size-1;
    if(type==UC_MEM_READ)t->ext_reads++;
    if(type==UC_MEM_WRITE)t->ext_writes++;
    t->ext_last_pc=t->last;
}
static void fw_zero_access(uc_engine *uc,uc_mem_type type,uint64_t address,int size,int64_t value,void *data){
    (void)uc;(void)value;fw_trace *t=data;
    if(t->faulted||size<=0)return;
    if(t->zero_reads==0&&t->zero_writes==0){t->zero_min=address;t->zero_max=address+(uint64_t)size-1;}
    if(address<t->zero_min)t->zero_min=address;
    if(address+(uint64_t)size-1>t->zero_max)t->zero_max=address+(uint64_t)size-1;
    if(type==UC_MEM_READ)t->zero_reads++;
    if(type==UC_MEM_WRITE)t->zero_writes++;
    t->zero_last_pc=t->last;
}
static bool fw_invalid(uc_engine *uc,uc_mem_type type,uint64_t address,int size,int64_t value,void *data){
    (void)uc;fw_trace *t=data;
    if(!t->faulted){t->faulted=true;t->fault=address;t->type=type;t->width=size;t->fault_value=(uint64_t)value;}
    return false;
}
JNIEXPORT jstring JNICALL Java_com_retro_n2626_NativeEngine_traceFirmware(JNIEnv *env,jclass cls,jbyteArray input){
    (void)cls;
    if(!input||(*env)->GetArrayLength(env,input)!=0x896f84)return (*env)->NewStringUTF(env,"Unexpected flash image size.");
    const size_t length=0x896f84,mapped=(length+4095)&~(size_t)4095;
    uc_engine *uc=NULL;uc_err err;uc_hook h1,h2,h3,h4,h5;fw_trace t={0};
    uint32_t pc=0,cpsr=0,sp=0,r0=0,r1=0,r2=0,lr=0;char msg[10000];const char *stage="open";
    err=uc_open(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN,&uc);if(err)goto done;
    stage="select experimental ARMv4T model";err=uc_ctl_set_cpu_model(uc,UC_CPU_ARM_TI925T);if(err)goto done;
    stage="map flash";err=uc_mem_map(uc,FW_BASE,mapped,UC_PROT_ALL);if(err)goto done;
    jbyte *bytes=(*env)->GetByteArrayElements(env,input,NULL);
    if(!bytes){uc_close(uc);return NULL;}
    stage="copy flash";err=uc_mem_write(uc,FW_BASE,bytes,length);
    (*env)->ReleaseByteArrayElements(env,input,bytes,JNI_ABORT);if(err)goto done;
    stage="protect flash";err=uc_mem_protect(uc,FW_BASE,mapped,UC_PROT_READ|UC_PROT_EXEC);if(err)goto done;
    stage="map provisional low RAM";
    err=uc_mem_map(uc,FW_RAM_BASE,FW_RAM_SIZE,UC_PROT_READ|UC_PROT_WRITE);if(err)goto done;
    t.ram_mapped=true;
    stage="map provisional zero page";err=uc_mem_map(uc,FW_ZERO_BASE,FW_ZERO_SIZE,UC_PROT_READ|UC_PROT_WRITE);if(err)goto done;
    t.zero_mapped=true;
    stage="map provisional external RAM";err=uc_mem_map(uc,FW_EXT_RAM_BASE,FW_EXT_RAM_SIZE,UC_PROT_READ|UC_PROT_WRITE);if(err)goto done;
    t.ext_mapped=true;
    stage="map experimental MMIO readback";err=uc_mmio_map(uc,PROBE_MMIO,PROBE_MMIO_SIZE,fw_mmio_read,&t,fw_mmio_write,&t);if(err)goto done;
    stage="map additional experimental MMIO page";
    err=uc_mmio_map(uc,PROBE_MMIO2,PROBE_MMIO2_SIZE,fw_mmio2_read,&t,fw_mmio2_write,&t);if(err)goto done;
    stage="initialize CPSR";err=uc_reg_read(uc,UC_ARM_REG_CPSR,&cpsr);if(err)goto done;
    cpsr=(cpsr&~0x3fu)|0xd3u;err=uc_reg_write(uc,UC_ARM_REG_CPSR,&cpsr);if(err)goto done;
    stage="install hooks";err=uc_hook_add(uc,&h1,UC_HOOK_CODE,(void*)fw_code,&t,1,0);if(err)goto done;
    err=uc_hook_add(uc,&h2,UC_HOOK_MEM_INVALID,(void*)fw_invalid,&t,1,0);if(err)goto done;
    err=uc_hook_add(uc,&h3,UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,(void*)fw_ram_access,&t,FW_RAM_BASE,FW_RAM_BASE+FW_RAM_SIZE-1);if(err)goto done;
    err=uc_hook_add(uc,&h4,UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,(void*)fw_ext_ram_access,&t,FW_EXT_RAM_BASE,FW_EXT_RAM_BASE+FW_EXT_RAM_SIZE-1);if(err)goto done;
    err=uc_hook_add(uc,&h5,UC_HOOK_MEM_READ|UC_HOOK_MEM_WRITE,(void*)fw_zero_access,&t,FW_ZERO_BASE,FW_ZERO_BASE+FW_ZERO_SIZE-1);if(err)goto done;
    stage="guest execution";err=uc_emu_start(uc,FW_ENTRY,FW_BASE+length,2000000,FW_MAX_INSNS);
    uc_reg_read(uc,UC_ARM_REG_PC,&pc);uc_reg_read(uc,UC_ARM_REG_CPSR,&cpsr);uc_reg_read(uc,UC_ARM_REG_SP,&sp);
    uc_reg_read(uc,UC_ARM_REG_R0,&r0);uc_reg_read(uc,UC_ARM_REG_R1,&r1);uc_reg_read(uc,UC_ARM_REG_R2,&r2);uc_reg_read(uc,UC_ARM_REG_LR,&lr);
done:;
    size_t used=(size_t)snprintf(msg,sizeof(msg),"RM-291 startup probe\nEntry: 0x%08x\nExperimental big-endian ARMv4T\nInstruction callbacks: %u (includes faulting instruction)\nStage: %s\nEngine: %s\nPC: 0x%08x\nCPSR: 0x%08x\nSP: 0x%08x\n",FW_ENTRY,t.count,stage,uc_strerror(err),pc,cpsr,sp);
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Provisional RAM: %s\nWindow: 0x00010000-0x0001FFFF (64 KiB, initially zero)\nRAM reads: %u | RAM writes: %u\n",t.ram_mapped?"mapped":"not mapped",t.ram_reads,t.ram_writes);
    if((t.ram_reads||t.ram_writes)&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"RAM touched: 0x%08" PRIx64 "-0x%08" PRIx64 "\nLast RAM access PC: 0x%08" PRIx64 "\n",t.ram_min,t.ram_max,t.ram_last_pc);
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"External RAM: %s (provisional 2 MiB)\nWindow: 0x03000000-0x031FFFFF\nExternal reads: %u | writes: %u\n",t.ext_mapped?"mapped":"not mapped",t.ext_reads,t.ext_writes);
    if((t.ext_reads||t.ext_writes)&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"External RAM touched: 0x%08" PRIx64 "-0x%08" PRIx64 "\nLast external RAM PC: 0x%08" PRIx64 "\n",t.ext_min,t.ext_max,t.ext_last_pc);
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Zero page: %s (provisional 4 KiB, read/write only)\nZero page reads: %u | writes: %u\n",t.zero_mapped?"mapped":"not mapped",t.zero_reads,t.zero_writes);
    if((t.zero_reads||t.zero_writes)&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Zero page touched: 0x%08" PRIx64 "-0x%08" PRIx64 "\nLast zero page PC: 0x%08" PRIx64 "\n",t.zero_min,t.zero_max,t.zero_last_pc);
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"R0: 0x%08x | R1: 0x%08x\nR2: 0x%08x | LR: 0x%08x\n",r0,r1,r2,lr);
    if(!t.faulted&&err==UC_ERR_OK&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"No recorded fault; probe ended (instruction/time limit or end address). Not proof of boot.\n");
    if(t.faulted&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Stopped access: 0x%08" PRIx64 "\nType: %d, size: %d\n",t.fault,t.type,t.width);
    if(t.faulted&&(t.type==UC_MEM_WRITE||t.type==UC_MEM_WRITE_UNMAPPED||t.type==UC_MEM_WRITE_PROT)&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Attempted write value: 0x%" PRIx64 "\n",t.fault_value);
    if(t.unsupported_mmio&&used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Stopped: %s. Engine OK does not imply boot success.\n",t.write_limit?"exploratory write limit":t.read_limit?"exploratory read limit":"unsupported MMIO");
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"Last callback PC: 0x%08" PRIx64 "\n\nExploratory writes: %u / %u\nAddress | Raw MMIO callback value | Guest PC | Bytes\n",t.last,t.writes,MAX_MMIO_WRITES);
    for(unsigned i=0;i<t.writes&&used<sizeof(msg);i++){
        if(i==TRACE_KEEP && t.writes>2*TRACE_KEEP){
            used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"... %u middle writes omitted ...\n",t.writes-2*TRACE_KEEP);
            i=t.writes-TRACE_KEEP;
            if(used>=sizeof(msg))break;
        }
        fw_write *w=i<TRACE_KEEP?&t.first_writes[i]:&t.last_writes[i%TRACE_KEEP];
        used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | 0x%08" PRIx64 "\nPC 0x%08" PRIx64 " | %u\n",w->addr,w->value,w->pc,w->width);
    }
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\n0x06010002 readback: %u reads | last raw write: 0x%04x | valid: %s\n",t.reg10002_reads,(unsigned)t.reg10002,t.reg10002_valid?"yes":"no");
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\nSpeculative D000 zero reads: D00E=%s | D010=%s\n",t.d00e_seen?"used":"not used",t.d010_seen?"used":"not used");
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\nExperimental reads: %u / %u\nAddress | Raw MMIO return value | Guest PC | Bytes\n",t.reads,MAX_MMIO_READS);
    for(unsigned i=0;i<t.reads&&used<sizeof(msg);i++){
        if(i==TRACE_KEEP && t.reads>2*TRACE_KEEP){
            used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"... %u middle reads omitted ...\n",t.reads-2*TRACE_KEEP);
            i=t.reads-TRACE_KEEP;
            if(used>=sizeof(msg))break;
        }
        fw_write *r=i<TRACE_KEEP?&t.first_reads[i]:&t.last_reads[i%TRACE_KEEP];
        used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | 0x%08" PRIx64 "\nPC 0x%08" PRIx64 " | %u\n",r->addr,r->value,r->pc,r->width);
    }
    if(used<sizeof(msg))used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\nSpeculative 0x06010000 zero read: %s\nRecent instruction callbacks (address | bytes):\n",t.reg10000_seen?"used":"not used");
    for(unsigned i=t.count>TRACE_KEEP?t.count-TRACE_KEEP:0;i<t.count&&used<sizeof(msg);i++){
        unsigned idx=i%TRACE_KEEP,n=t.recent_size[idx];
        uint64_t addr=t.recent_pc[idx];
        unsigned char raw[4]={0};
        if(uc && (n==2||n==4) && addr>=FW_BASE && addr+n<=FW_BASE+length && uc_mem_read(uc,addr,raw,n)==UC_ERR_OK){
            if(n==2)used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | %02x%02x\n",addr,(unsigned)raw[0],(unsigned)raw[1]);
            else used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | %02x%02x%02x%02x\n",addr,(unsigned)raw[0],(unsigned)raw[1],(unsigned)raw[2],(unsigned)raw[3]);
        }else used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | unavailable (size %u)\n",addr,n);
    }
    /* Read only flash bytes around the stop PC; never sample MMIO. */
    if(uc && t.count && t.last>=FW_BASE+16 && t.last+48<=FW_BASE+length && used<sizeof(msg)){
        uint64_t addr=t.last-16;
        unsigned char raw[64];
        if(uc_mem_read(uc,addr,raw,sizeof(raw))==UC_ERR_OK){
            used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"\nFlash bytes around stopping instruction:\n");
            for(unsigned i=0;i<sizeof(raw)&&used<sizeof(msg);i+=8){
                used+=(size_t)snprintf(msg+used,sizeof(msg)-used,"0x%08" PRIx64 " | %02x%02x %02x%02x %02x%02x %02x%02x\n",addr+i,(unsigned)raw[i],(unsigned)raw[i+1],(unsigned)raw[i+2],(unsigned)raw[i+3],(unsigned)raw[i+4],(unsigned)raw[i+5],(unsigned)raw[i+6],(unsigned)raw[i+7]);
            }
        }
    }
    if(used<sizeof(msg))snprintf(msg+used,sizeof(msg)-used,"\nProbe windows: 0x06000000-0x0601FFFF and 0x06100000-0x06100FFF. 0x06009000 and 0x06010002 support speculative 16-bit last-write readback. Two one-shot startup reads at 0x0600D00E and 0x0600D010 return hypothetical zero only at PCs 0x01247610 and 0x01247612. One one-shot read at 0x06010000 / PC 0x0109BD5A also returns hypothetical zero. Other reads stop. RAM windows, zero page and MMIO are experimental. MMIO callback values may differ in byte order from guest CPU write values; no verified hardware side effects. Not a completed OS boot.");
    if(uc)uc_close(uc);return (*env)->NewStringUTF(env,msg);
}
