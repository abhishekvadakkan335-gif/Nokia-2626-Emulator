/* Android port of the local RM-291 06.81 exploratory probe.
 * Samsung model only. This is not a verified hardware implementation.
 * No firmware bundled. Flash mappings remain read-only.
 */
#include <jni.h>
#include <unicorn/unicorn.h>
#include <unicorn/arm.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#define BASE 0x01000000u
#define LEN 0x00d60000u
#define ALIAS 0x09000000u
#define ROM 0x00840000u
#define LIMIT 3000000u
typedef struct {uint64_t a,pc,v;unsigned n,c;} entry;
typedef struct {
 uc_engine *uc;const unsigned char *im,*raw;
 unsigned count,phase,timer,pages,execs,nr,nw,reads,unknown,config,rom_steps,port;
 unsigned nor_step;bool id,explore,pending,stopped;int type,width;
 uint64_t last,addr,val,deadline;uint32_t page[64];entry r[512],w[1024];
 char out[12000];size_t pos;char dbg[1024];unsigned debug;
} trace;
typedef struct{trace*t;uint64_t base;} dev;
static void logline(trace*t,const char*f,...){if(t->pos>=sizeof(t->out)-1)return;va_list ap;va_start(ap,f);int n=vsnprintf(t->out+t->pos,sizeof(t->out)-t->pos,f,ap);va_end(ap);if(n>0)t->pos+=(size_t)n<sizeof(t->out)-t->pos?(size_t)n:sizeof(t->out)-t->pos-1;}
static uint64_t now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static uint32_t reg(trace*t,int r){uint32_t v=0;uc_reg_read(t->uc,r,&v);return v;}
static bool eqmem(trace*t,uint64_t a,const unsigned char*b,size_t n){unsigned char x[64];return n<=sizeof x&&uc_mem_read(t->uc,a,x,n)==UC_ERR_OK&&!memcmp(x,b,n);}
static void stop(trace*t,int ty,uint64_t a,unsigned n,uint64_t v){t->stopped=true;t->type=ty;t->addr=a;t->width=n;t->val=v;uc_emu_stop(t->uc);}
static void code(uc_engine*u,uint64_t a,uint32_t n,void*data){(void)u;(void)n;trace*t=data;t->count++;t->last=a;
 if(a==0x10992b2){t->timer=2000;t->phase=0;}
 if(a==0x2030&&++t->phase>1){unsigned s=t->phase<13?100:2;t->timer=t->timer>s?t->timer-s:1;}
 if(a==0x109c0ce||a==0x109c0d2)logline(t,"Native checksum PC %08llx: %04x vs %04x\n",(unsigned long long)a,reg(t,UC_ARM_REG_R0),reg(t,UC_ARM_REG_R1));
 if(t->count>=LIMIT)stop(t,-1,a,n,0);
}
static bool invalid(uc_engine*u,uc_mem_type ty,uint64_t a,int n,int64_t v,void*data){
 trace*t=data;uint64_t p=a&~4095ULL;
 if(ty==UC_MEM_WRITE_UNMAPPED&&n>0&&(a+n<=0x100000||p==0x7ff000)&&a+n<=p+4096&&t->pages<64&&uc_mem_map(u,p,4096,3)==UC_ERR_OK){t->page[t->pages++]=(uint32_t)p;return true;}
 if(ty==UC_MEM_FETCH_PROT&&(a==0x2030||a==0x2e0||a==0x1400)){
  unsigned char bytes[32];bool known=p==0;for(unsigned i=0;i<t->pages;i++)if(t->page[i]==p)known=true;
  if(known&&uc_mem_read(u,a,bytes,32)==UC_ERR_OK){
   for(size_t i=0;i+32<=LEN;i++)if(t->im[i]==bytes[0]&&!memcmp(t->im+i,bytes,32)){
    if(uc_mem_protect(u,p,4096,7)==UC_ERR_OK){t->execs++;logline(t,"RAM code %08llx matches flash %08x\n",(unsigned long long)a,(unsigned)(BASE+i));return true;}break;
   }
  }
 }
 if(ty==UC_MEM_WRITE_PROT&&n==2&&a>=BASE&&a<BASE+LEN&&t->last==0x2d4&&reg(t,UC_ARM_REG_PC)==t->last&&eqmem(t,t->last,(const unsigned char*)"\x80\x0a",2)&&reg(t,UC_ARM_REG_R1)==a&&(reg(t,UC_ARM_REG_R2)&65535)==(v&65535)&&(reg(t,UC_ARM_REG_CPSR)&32)){
 t->pending=true;t->addr=a;t->val=v&65535;return false;}
 t->stopped=true;t->type=ty;t->addr=a;t->width=n;t->val=v;return false;
}
static unsigned swap16(unsigned v){return ((v&255)<<8)|((v>>8)&255);}
static uint64_t rd(uc_engine*u,uint64_t o,unsigned n,void*d){(void)u;dev*m=d;trace*t=m->t;uint64_t a=m->base+o,v=0;unsigned c=0;
 if(a==0x4000000||m->base==0x4000000){stop(t,UC_MEM_READ,a,n,0);return 0;}
 if(t->nr>=512||t->reads>=512){stop(t,UC_MEM_READ,a,n,0);return 0;}
 unsigned i;for(i=0;i<t->nr;i++)if(t->r[i].a==a&&t->r[i].pc==t->last&&t->r[i].n==n)break;
 if(i==t->nr){t->r[i]=(entry){a,t->last,0,n,0};t->nr++;}c=++t->r[i].c;t->reads++;
 if(m->base==0x1ee0000||m->base==0x9ee0000){v=n>=8?~0ULL:((1ULL<<(8*n))-1);if(t->id&&n==2){if(o==0)v=0xec00;else if(o==2)v=0xe822;}return v;}
 if(t->explore&&a==0x601042a&&n==2&&(t->last==0x12c0a3c||t->last==0x12c0a56||t->last==0x12c0a96))v=c>=3?(t->last==0x12c0a3c?0x100:t->last==0x12c0a56?0x200:0x400):0;
 else if((a==0x6008000||a==0x6009000)&&t->last==0x142c)v=c>=3?0x100:0;
 else if(a==0x6010422&&t->last==0x109b4c8)v=c>=3?4:0;
 else if(a==0x6010114&&t->last==0x10992cc)v=swap16(2000);
 else if(a==0x6010112&&(t->last==0x10992ce||t->last==0x10992d0||t->last==0x10992e4||t->last==0x10992e6||t->last==0x10992f8||t->last==0x10992fa))v=swap16(t->timer);
 else if(a==0x6010000||a==0x600d00e||a==0x600d010)v=0;
 else {bool found=false;for(unsigned j=0;j<t->nw;j++)if(t->w[j].a==a&&t->w[j].n==n){v=t->w[j].v;found=true;break;}if(!found&&++t->unknown>128)stop(t,UC_MEM_READ,a,n,0);}
 return v;
}
static void wr(uc_engine*u,uint64_t o,unsigned n,uint64_t v,void*d){(void)u;dev*m=d;trace*t=m->t;uint64_t a=m->base+o;
 if(m->base==0x4000000){if(o==0&&n==1&&(t->last==0x109e652||t->last==0x109e68e||t->last==0x109e70e)&&t->port<32){t->port++;logline(t,"Port captured %02llx at PC %08llx\n",(unsigned long long)v,(unsigned long long)t->last);return;}stop(t,UC_MEM_WRITE,a,n,v);return;}
 if(m->base==0x1ee0000){if(n==2){unsigned cmd=v>>8;if(cmd==0xff||cmd==0xf0){t->id=false;t->nor_step=0;}else if(cmd==0xaa&&o==0xaaa)t->nor_step=1;else if(cmd==0x55&&o==0x554&&t->nor_step==1)t->nor_step=2;else if(cmd==0x90&&o==0xaaa&&t->nor_step==2){t->id=true;t->nor_step=0;logline(t,"Samsung ID mode\n");}else t->nor_step=0;}return;}
 if(a==0x4ef0000&&(n==1||n==2||n==4)){for(unsigned i=0;i<n&&t->debug<sizeof(t->dbg)-1;i++){unsigned ch=(v>>(i*8))&255;t->dbg[t->debug++]=(ch>=32&&ch<127)?ch:' ';}return;}
 if(a==0x4fd0000&&n==1){if(t->debug<sizeof(t->dbg)-1)t->dbg[t->debug++]='\n';return;}
 for(unsigned j=0;j<t->nw;)if(a<t->w[j].a+t->w[j].n&&t->w[j].a<a+n)t->w[j]=t->w[--t->nw];else j++;
 if(t->nw>=1024){stop(t,UC_MEM_WRITE,a,n,v);return;}t->w[t->nw++]=(entry){a,t->last,v,n,0};
}
static uc_err run(trace*t,uint32_t pc){uint64_t now=now_us();if(now>=t->deadline||t->count>=LIMIT)return UC_ERR_OK;return uc_emu_start(t->uc,pc,BASE+LEN,t->deadline-now,LIMIT-t->count);}
static bool romcall(trace*t,uc_err e,uint32_t pc,uint32_t lr){return t->explore&&e==UC_ERR_INSN_INVALID&&!t->stopped&&reg(t,UC_ARM_REG_PC)==pc&&t->last==pc&&reg(t,UC_ARM_REG_LR)==lr&&eqmem(t,pc,(const unsigned char*)"\xde\x00",2);}
static bool verify(trace*t,uint64_t a,const unsigned char*orig){unsigned char block[4096];for(size_t i=0;i<LEN;i+=sizeof block){if(uc_mem_read(t->uc,a+i,block,sizeof block)!=UC_ERR_OK||memcmp(block,orig+i,sizeof block))return false;}return true;}
JNIEXPORT jstring JNICALL Java_com_retro_n2626_NativeEngine_traceFull(JNIEnv*env,jclass cls,jbyteArray decoded,jbyteArray raw,jboolean explore){
 (void)cls;if(!decoded||!raw||(*env)->GetArrayLength(env,decoded)!=LEN||(*env)->GetArrayLength(env,raw)!=LEN)return (*env)->NewStringUTF(env,"Unexpected image size");
 trace*t=calloc(1,sizeof *t);if(!t)return (*env)->NewStringUTF(env,"Allocation failed");
 jbyte*im=(*env)->GetByteArrayElements(env,decoded,NULL);jbyte*rm=(*env)->GetByteArrayElements(env,raw,NULL);
 t->im=(const unsigned char*)im;t->raw=(const unsigned char*)rm;t->explore=explore;t->timer=2000;
 uc_err e=UC_ERR_NOMEM;uc_hook h1,h2;dev ds[8];bool mapped=false;
 logline(t,"RM-291 06.81 full flash Android trial\nExperimental Samsung model; %s\n",explore?"opt-in ROM hypotheses enabled":"stop at unimplemented ROM entry");
 if(!im||!rm)goto end;
 #define CHECK(x) do{e=(x);if(e!=UC_ERR_OK)goto end;}while(0)
 CHECK(uc_open(UC_ARCH_ARM,UC_MODE_ARM|UC_MODE_BIG_ENDIAN,&t->uc));CHECK(uc_ctl_set_cpu_model(t->uc,UC_CPU_ARM_TI925T));
 CHECK(uc_mem_map(t->uc,BASE,LEN,7));CHECK(uc_mem_write(t->uc,BASE,im,LEN));CHECK(uc_mem_protect(t->uc,BASE,LEN,5));
 CHECK(uc_mem_map(t->uc,ALIAS,LEN,3));CHECK(uc_mem_write(t->uc,ALIAS,rm,LEN));CHECK(uc_mem_protect(t->uc,ALIAS,LEN,1));
 unsigned char rom[4096];for(unsigned i=0;i<sizeof rom;i+=2){rom[i]=0xde;rom[i+1]=0;}
 // B to 0x100, then candidate Thumb copy routine.
 rom[0]=0xe0;rom[1]=0x7e;
 const unsigned char copy[]={0xb4,0x0f,0x2a,0x00,0xd0,0x04,0x78,0x0b,0x70,0x03,0x30,0x01,0x31,0x01,0x3a,0x01,0xd1,0xf9,0xbc,0x0f,0x47,0x70};
 memcpy(rom+0x100,copy,sizeof copy);
 CHECK(uc_mem_map(t->uc,ROM,4096,7));CHECK(uc_mem_write(t->uc,ROM,rom,4096));CHECK(uc_mem_protect(t->uc,ROM,4096,5));
 const uint32_t ra[]={0,0x10000,0xd0000,0x3000000,0x7000000},rn[]={4096,65536,65536,0x200000,8192};
 for(unsigned i=0;i<5;i++)CHECK(uc_mem_map(t->uc,ra[i],rn[i],3));
 uint32_t cpsr=(reg(t,UC_ARM_REG_CPSR)&~0x3f)|0xd3;CHECK(uc_reg_write(t->uc,UC_ARM_REG_CPSR,&cpsr));
 const uint32_t da[]={0x6000000,0x6100000,0x4ef0000,0x4fd0000,0x1ee0000,0x9ee0000,0x5100000,0x4000000},dn[]={0x20000,0x1000,0x1000,0x1000,0x10000,0x10000,0x1000,0x1000};
 for(unsigned i=0;i<(explore?8u:7u);i++){ds[i]=(dev){t,da[i]};CHECK(uc_mmio_map(t->uc,da[i],dn[i],rd,&ds[i],wr,&ds[i]));}
 CHECK(uc_hook_add(t->uc,&h1,UC_HOOK_CODE,(void*)code,t,1,0));CHECK(uc_hook_add(t->uc,&h2,UC_HOOK_MEM_INVALID,(void*)invalid,t,1,0));mapped=true;t->deadline=now_us()+15000000;
 e=run(t,BASE+0x100);
 while(t->pending&&t->config<3&&!t->stopped){
  unsigned i=t->config;uint32_t addresses[]={0x1000aaa,0x1000554,0x1004aaa};unsigned values[]={0xaa,0x55,0xc0};
  if(t->addr!=addresses[i]||t->val!=values[i])break;
  t->pending=false;t->config++;logline(t,"NOR configuration cycle %u (array unchanged)\n",t->config);e=run(t,0x2d7);
 }
 unsigned char erased[40];memset(erased,255,sizeof erased);
 if(romcall(t,e,0x84000e,0x109c06d)&&reg(t,UC_ARM_REG_R0)==0x1ebd4&&reg(t,UC_ARM_REG_R1)==40&&reg(t,UC_ARM_REG_R2)==0x1ec24&&reg(t,UC_ARM_REG_R3)==0x20&&eqmem(t,0x1ebd4,erased,40)){
  t->rom_steps++;logline(t,"Erased-record negative test: transformation omitted, native checksum retained\n");e=run(t,reg(t,UC_ARM_REG_LR));
 }
 if(romcall(t,e,0x840016,0x109c1cb)&&reg(t,UC_ARM_REG_R0)==0x018e0000&&reg(t,UC_ARM_REG_R1)==0x02000000&&reg(t,UC_ARM_REG_R2)==0x02600000&&reg(t,UC_ARM_REG_R3)==0x02800000){
  t->rom_steps++;logline(t,"Range configuration hypothesis: omitted\n");e=run(t,reg(t,UC_ARM_REG_LR));
 }
 if(romcall(t,e,0x840014,0x109c1f5)&&reg(t,UC_ARM_REG_R0)==0x0900006c&&reg(t,UC_ARM_REG_R1)==0x840015&&eqmem(t,0x0900006c,(const unsigned char*)"\x87\x20\x8d\xaf\x8f\x61\x70\x34",8)){
  t->rom_steps++;logline(t,"Header configuration hypothesis: omitted\n");e=run(t,reg(t,UC_ARM_REG_LR));
 }
 if(romcall(t,e,0x84001c,0x109c251)&&reg(t,UC_ARM_REG_R0)==0x84001d&&reg(t,UC_ARM_REG_R5)==2){
  uint32_t zero=0;CHECK(uc_reg_write(t->uc,UC_ARM_REG_R0,&zero));t->rom_steps++;logline(t,"Status word hypothesis: R0=0\n");e=run(t,reg(t,UC_ARM_REG_LR));
 }
 end:
 logline(t,"Engine: %s\nInstruction callbacks: %u\nPC %08x SP %08x LR %08x\nStopped access %08llx type %d width %d value %llx\nRAM pages %u; executable RAM pages %u; ROM hypotheses used %u; port writes %u\n",uc_strerror(e),t->count,t->uc?reg(t,UC_ARM_REG_PC):0,t->uc?reg(t,UC_ARM_REG_SP):0,t->uc?reg(t,UC_ARM_REG_LR):0,(unsigned long long)t->addr,t->type,t->width,(unsigned long long)t->val,t->pages,t->execs,t->rom_steps,t->port);
 if(mapped)logline(t,"Flash views unchanged: %s\n",verify(t,BASE,t->im)&&verify(t,ALIAS,t->raw)?"YES":"NO");
 logline(t,"Firmware diagnostic output:\n%s\nNot a completed OS boot. Existing phone UI is a simulation.\n",t->dbg);
 if(t->uc)uc_close(t->uc);
 if(im)(*env)->ReleaseByteArrayElements(env,decoded,im,JNI_ABORT);if(rm)(*env)->ReleaseByteArrayElements(env,raw,rm,JNI_ABORT);
 jstring result=(*env)->NewStringUTF(env,t->out);free(t);return result;
}
