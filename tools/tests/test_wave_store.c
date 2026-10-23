#include "mock_hw.h"
#include "ir_raw.c"
#include <stdlib.h>
static unsigned malloc_failed;
static void *wave_malloc(size_t n){return malloc_failed?NULL:malloc(n);}
#define malloc wave_malloc
#include "ir_wave_store.c"
static uint8_t flash[0x6000],snapshot[0x6000];static unsigned operations,fail_at,writes;static bool capture,pending;
bool ir_tx_pending(void){return pending;}
bool ir_learn_is_active(void){return capture;}
static unsigned offset(uint32_t a,unsigned n){assert(a>=A && a+n<=A+sizeof(flash));return a-A;}
uint32_t Qflash_Read(uint32_t a,uint8_t *b,uint32_t n){assert(n<=256u);if(++operations==fail_at)return 1;memcpy(b,flash+offset(a,n),n);return 0;}
uint32_t Qflash_Erase_Sector(uint32_t a){unsigned o=offset(a,4096);++writes;if(++operations==fail_at){memset(flash+o,255,2048);return 1;}memset(flash+o,255,4096);return 0;}
uint32_t Qflash_Write(uint32_t a,uint8_t *b,uint32_t n){unsigned o=offset(a,n),i;assert(n<=256u && (a&255u)+n<=256u);++writes;
 if(++operations==fail_at){for(i=0;i<n/2;i++)flash[o+i]&=b[i];return 1;}for(i=0;i<n;++i)flash[o+i]&=b[i];return 0;}
int main(void)
{
    ir_raw_t old,next,out;unsigned i,total,before;uint32_t first;
    assert(A==0x0106B000u && B==0x0106E000u && FLASH_UNALLOCATED_START==0x01071000u);
    assert(A==FLASH_USER_CFG_START+FLASH_USER_CFG_SIZE && B==A+FLASH_IR_RAW_BANK_SIZE);
    memset(flash,255,sizeof(flash));ir_wave_store_load();assert(!s_bank&&!ir_wave_store_has(1));assert(!ir_wave_store_get(1,&old));
    memset(&old,0,sizeof(old));old.count=67;old.period_ticks=1684;old.kind=IR_NEC_KIND;
    old.widths[0]=9000;old.widths[1]=4500;old.widths[66]=562;
    for(i=0;i<32;++i){old.widths[2+2*i]=562;old.widths[3+2*i]=(0xFF00AE51u&(1ul<<i))?1687:562;}
    /* Valid record at the previous 0x0106D000 base is intentionally ignored. */
    {header_t h;uint32_t crc=0xffffffffu;unsigned off=0x2000;
     memset(flash+off,0,sizeof(h)+BYTES);memcpy(flash+off+sizeof(h),&old,sizeof(old));
     h.magic=MAGIC;h.version=VERSION;h.sequence=99;crc=crc_add(crc,&h.version,8);
     h.crc=~crc_add(crc,flash+off+sizeof(h),BYTES);memcpy(flash+off,&h,sizeof(h));
     ir_wave_store_load();assert(!s_bank&&!ir_wave_store_has(1));}
    assert(ir_wave_store_save(1,&old));
    assert(!ir_wave_store_has(4));assert(!ir_wave_store_save(4,&old));
    assert(!ir_wave_store_save(15,&old));assert(!ir_wave_store_get(15,&out));
    assert(ir_wave_store_save(17,&old));first=s_bank;assert(ir_wave_store_has(1)&&ir_wave_store_has(17));
    memcpy(snapshot,flash,sizeof(flash));
    before=writes;malloc_failed=1;ir_wave_store_load();malloc_failed=0;
    assert(!s_metadata_ready&&!ir_wave_store_save(2,&old)&&writes==before);
    ir_wave_store_load();assert(s_metadata_ready&&ir_wave_store_has(1)&&ir_wave_store_has(17));
    operations=0;fail_at=1;ir_wave_store_load();fail_at=0;
    assert(!s_metadata_ready&&!ir_wave_store_save(2,&old)&&writes==before);
    ir_wave_store_load();assert(s_metadata_ready&&ir_wave_store_has(1));
    next=old;next.widths[3]=1687;next.kind=IR_RAW_KIND;
    operations=0;assert(ir_wave_store_save(17,&next));total=operations;
    /* Interrupt every read/write/erase boundary, including partial page writes. */
    for(i=1;i<=total;++i){memcpy(flash,snapshot,sizeof(flash));fail_at=0;ir_wave_store_load();operations=0;fail_at=i;
        (void)ir_wave_store_save(17,&next);fail_at=0;ir_wave_store_load();
        assert(ir_wave_store_get(17,&out));assert(!memcmp(&out,&old,sizeof(out))||!memcmp(&out,&next,sizeof(out)));
        assert(ir_wave_store_has(1));
    }
    memcpy(flash,snapshot,sizeof(flash));ir_wave_store_load();assert(s_bank==first);
    assert(ir_wave_store_save(17,&next));assert(s_bank!=first);flash[s_bank-A+12]^=1;ir_wave_store_load();
    assert(s_bank==first&&ir_wave_store_get(17,&out)&&!memcmp(&out,&old,sizeof(out)));
    before=writes;ota_busy=1;assert(!ir_wave_store_save(17,&next)&&!ir_wave_store_clear());ota_busy=0;
    capture=true;assert(!ir_wave_store_save(17,&next));capture=false;pending=true;assert(!ir_wave_store_save(17,&next));pending=false;
    modem_busy=1;assert(!ir_wave_store_clear());modem_busy=0;assert(writes==before);
    assert(ir_wave_store_clear());ir_wave_store_load();assert(!ir_wave_store_has(1)&&!ir_wave_store_has(17));
    assert(ir_wave_store_save(2,&old));ir_wave_store_load();assert(ir_wave_store_has(2)&&!ir_wave_store_has(1));
    next=old;next.kind=IR_GEHUA_KIND;next.frame=0x80102658u;
    assert(ir_wave_store_save(17,&next));ir_wave_store_load();assert(ir_wave_store_frame(17,&first)&&first==next.frame);
    assert(!ir_wave_store_frame(2,&first));
    /* A CRC-valid legacy voice slot must never become a learned key again. */
    {unsigned off=s_bank-A;header_t h;uint32_t crc;
     memcpy(flash+off+sizeof(h)+14*sizeof(next),&next,sizeof(next));
     memcpy(&h,flash+off,sizeof(h));crc=crc_add(0xffffffffu,&h.version,8);
     h.crc=~crc_add(crc,flash+off+sizeof(h),BYTES);memcpy(flash+off,&h,sizeof(h));
     ir_wave_store_load();assert(ir_wave_store_has(17));
     assert(!ir_wave_store_has(15)&&!ir_wave_store_get(15,&out)&&!ir_wave_store_frame(15,&first));}
    operations=0;fail_at=1;ir_wave_store_load();fail_at=0;assert(!s_metadata_ready);
    assert(ir_wave_store_clear());ir_wave_store_load();assert(!ir_wave_store_has(2)&&!ir_wave_store_has(17));
    assert(!ir_wave_store_frame(17,&first));
    printf("wave store: %u torn/read/erase failure points, boot restore, CRC fallback, compacted layout, clear/no resurrection, page limits, policy/OTA/CAT1 PASS\n",total);return 0;
}
