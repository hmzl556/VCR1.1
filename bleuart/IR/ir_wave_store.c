#include "app_heap.h"
/* Reference-compatible 18-slot format, streamed to keep RAM bounded.
 * Only four policy-approved keys may be read/written. Commit magic last.
 */
#include "ir_wave_store.h"
#include "ir_key_policy.h"
#include "ir_learn.h"
#include "ir_key_map.h"
#include "qflash.h"
#include "flash_layout.h"
#include "udp_ota.h"
#include "cat1_skt_udp.h"
#include <string.h>
#include <stdlib.h>
#include "co_printf.h"
#define MAGIC 0x49525231ul
#define VERSION 1ul
#define A FLASH_IR_RAW_START
#define B FLASH_IR_RAW_BACKUP
#define BYTES (18u*sizeof(ir_raw_t))
typedef struct {uint32_t magic,version,sequence,crc;} header_t;
#ifdef __CC_ARM
#define IR_ZI __attribute__((section(".bss.ir_wave_store"),zero_init))
#else
#define IR_ZI
#endif
/* Keep zero state out of the small RW area before the SDK's 256-byte aligned
 * data section; otherwise four words can consume an extra alignment page. */
static uint32_t s_bank IR_ZI,s_sequence IR_ZI,s_mask IR_ZI,s_rc6_mask IR_ZI,s_frames[4];
static bool s_read_failed, s_metadata_ready;
static unsigned slot(unsigned key)
{static const uint8_t keys[4]={1,2,3,17};unsigned i;for(i=0;i<4;++i)if(keys[i]==key)return i;return 0;}
bool ir_wave_store_frame(unsigned key,uint32_t *f)
{if(!ir_key_learnable(key) || !(s_rc6_mask&(1ul<<key)))return false;*f=s_frames[slot(key)];return true;}
static int read_chunks(uint32_t addr,void *out,uint32_t n)
{
    uint8_t *p=out;while(n){uint32_t part=n>256u?256u:n;
        if(Qflash_Read(addr,p,part)!=FlashOperationSuccess){s_read_failed=true;return 0;}
        p+=part;addr+=part;n-=part;}return 1;
}
static int write_pages(uint32_t addr,const void *in,uint32_t n)
{
    const uint8_t *p=in;while(n){uint32_t part=256u-(addr&255u);if(part>n)part=n;
        if(Qflash_Write(addr,(uint8_t *)p,part)!=FlashOperationSuccess)return 0;
        p+=part;addr+=part;n-=part;}return 1;
}
static uint32_t crc_add(uint32_t crc,const void *data,unsigned n)
{
    const uint8_t *p=data;unsigned bit;while(n--){crc^=*p++;
        for(bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1u)?0xEDB88320ul:0u);}return crc;
}
static int valid(uint32_t bank,header_t *h,bool committed)
{
    uint8_t block[256];uint32_t left=BYTES,addr=bank+sizeof(*h),crc=0xffffffffu,part;
    if(!read_chunks(bank,h,sizeof(*h)) || h->magic!=(committed?MAGIC:0xffffffffu) || h->version!=VERSION)return 0;
    crc=crc_add(crc,&h->version,8);
    while(left){part=left>256u?256u:left;if(!read_chunks(addr,block,part))return 0;
        crc=crc_add(crc,block,part);left-=part;addr+=part;}return ~crc==h->crc;
}
static int read_slot(uint32_t bank,unsigned key,ir_raw_t *w)
{return read_chunks(bank+sizeof(header_t)+(key-1u)*sizeof(*w),w,sizeof(*w));}
static bool load_metadata(uint32_t bank,ir_raw_t *w)
{
    unsigned k;s_mask=0;s_rc6_mask=0;
    for(k=1;k<=18;++k)if(ir_key_learnable(k)){
        if(!read_slot(bank,k,w))return false;
        if(ir_raw_valid(w) && !ir_raw_rc6_damaged(w)){
            s_mask|=1ul<<k;if(w->kind==IR_GEHUA_KIND){s_rc6_mask|=1ul<<k;s_frames[slot(k)]=w->frame;}
        }
    }return true;
}
void ir_wave_store_load(void)
{
    header_t a,b;int va,vb;
    s_read_failed=false;s_metadata_ready=false;
    va=valid(A,&a,true);vb=valid(B,&b,true);
    ir_raw_t *w=NULL;bool allocated=false;uint32_t other=0,other_seq=0;
    s_bank=0;s_sequence=0;s_mask=0;s_rc6_mask=0;
    if(vb && (!va || (int32_t)(b.sequence-a.sequence)>0)){
        s_bank=B;s_sequence=b.sequence;if(va){other=A;other_seq=a.sequence;}
    }else if(va){s_bank=A;s_sequence=a.sequence;if(vb){other=B;other_seq=b.sequence;}}
    if(s_bank){
        w=app_heap_alloc(sizeof(*w));
        if(w){
            allocated=true;
            if(!load_metadata(s_bank,w)){
                if(other && load_metadata(other,w)){s_bank=other;s_sequence=other_seq;}
                else {s_bank=0;s_sequence=0;s_mask=0;s_rc6_mask=0;}
            }app_heap_free(w);
        }
    }
    s_metadata_ready = !s_read_failed && (!s_bank || allocated);
    co_printf("[IR STORE] A=%u B=%u selected=%08lx seq=%lu mask=%lx\r\n",va,vb,(unsigned long)s_bank,(unsigned long)s_sequence,(unsigned long)s_mask);
}
bool ir_wave_store_has(unsigned key)
{return ir_key_learnable(key) && s_bank && (s_mask&(1ul<<key))!=0;}
bool ir_wave_store_get(unsigned key,ir_raw_t *w)
{
    if(!w || !ir_wave_store_has(key))return false;
    return read_slot(s_bank,key,w) && ir_raw_valid(w) && !ir_raw_rc6_damaged(w);
}
static bool commit(unsigned key,const ir_raw_t *wave,bool clear)
{
    header_t h,check;ir_raw_t *w;unsigned k;uint32_t bank,offset,crc=0xffffffffu,mask=0;
    bool ok=false;
    if((!clear && !s_metadata_ready) || udp_ota_session_busy() || ir_learn_is_active() || ir_tx_pending() || cat1_modem_is_busy())return false;
    w=app_heap_alloc(sizeof(*w));if(!w)return false;
    bank=s_bank==A?B:A;h.magic=0xffffffffu;h.version=VERSION;h.sequence=s_sequence+1u;
    crc=crc_add(crc,&h.version,8);
    /* First pass computes CRC without retaining 18 waveforms in RAM. */
    for(k=1;k<=18;++k){
        if(!clear && k==key)*w=*wave;
        else if(clear || !ir_wave_store_has(k))memset(w,0,sizeof(*w));
        else if(!ir_wave_store_get(k,w))goto done;
        if(w->count)mask|=1ul<<k;crc=crc_add(crc,w,sizeof(*w));
    }
    h.crc=~crc;
    for(offset=0;offset<FLASH_IR_RAW_BANK_SIZE;offset+=0x1000u)if(Qflash_Erase_Sector(bank+offset)!=FlashOperationSuccess)goto done;
    if(!write_pages(bank+4u,&h.version,sizeof(h)-4u))goto done;
    for(k=1;k<=18;++k){
        if(!clear && k==key)*w=*wave;
        else if(clear || !ir_wave_store_has(k))memset(w,0,sizeof(*w));
        else if(!ir_wave_store_get(k,w))goto done;
        if(!write_pages(bank+sizeof(h)+(k-1u)*sizeof(*w),w,sizeof(*w)))goto done;
    }
    if(!valid(bank,&check,false))goto done;
    h.magic=MAGIC;if(!write_pages(bank,&h.magic,4) || !valid(bank,&check,true))goto done;
    /* Same chunked slot read as startup, before publishing SAVED. */
    if(!clear && (!read_slot(bank,key,w) || memcmp(w,wave,sizeof(*w))))goto done;
    s_bank=bank;s_sequence=h.sequence;s_mask=mask;s_metadata_ready=true;
    if(clear)s_rc6_mask=0;
    else {s_rc6_mask&=~(1ul<<key);if(wave->kind==IR_GEHUA_KIND){s_rc6_mask|=1ul<<key;s_frames[slot(key)]=wave->frame;}}
    ok=true;
 done:app_heap_free(w);return ok;
}
bool ir_wave_store_save(unsigned key,const ir_raw_t *wave)
{
    ir_raw_t *old;bool same=false;
    if(!s_metadata_ready || !ir_key_learnable(key) || !wave || !ir_raw_valid(wave) || ir_raw_rc6_damaged(wave))return false;
    if(udp_ota_session_busy() || ir_learn_is_active() || ir_tx_pending() || cat1_modem_is_busy())return false;
    old=app_heap_alloc(sizeof(*old));if(!old)return false;
    if(ir_wave_store_get(key,old))same=!memcmp(old,wave,sizeof(*old));app_heap_free(old);
    return same || commit(key,wave,false);
}
bool ir_wave_store_clear(void)
{
    /* Reload sequence selection after a failed boot read before committing a
     * clear; otherwise an unseen newer bank could resurrect old learned keys. */
    if(!s_metadata_ready)ir_wave_store_load();
    return s_metadata_ready && commit(0,0,true);
}
