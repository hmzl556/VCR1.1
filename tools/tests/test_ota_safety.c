#include "mock_merge.h"
#include <stdlib.h>
#define __weak
#define FLASH_SECTOR_SIZE 4096u
#define SystemGetMacAddr mock_mac
static const uint8_t *mock_mac(void){static uint8_t m[6];return m;}
static uint8_t flash_image[0x80000];
static unsigned reads,writes,erases,fail_read,fail_write,fail_erase,tear,flash_feeds;
void IWDG_ReloadKey(void){++flash_feeds;}
static uint32_t index_of(uint32_t addr,unsigned n){assert(addr>=0x01000000u&&addr-0x01000000u+n<=sizeof(flash_image));return addr-0x01000000u;}
static void Qflash_Init(void){}
static uint32_t Qflash_Read(uint32_t a,uint8_t *p,uint32_t n){unsigned x=index_of(a,n);if(++reads==fail_read)return 1;memcpy(p,flash_image+x,n);return 0;}
static uint32_t Qflash_Write(uint32_t a,uint8_t *p,uint32_t n){unsigned i,x=index_of(a,n);++writes;if(writes==fail_write){for(i=0;i<n&&i<tear;i++)flash_image[x+i]&=p[i];return 1;}for(i=0;i<n;i++)flash_image[x+i]&=p[i];return 0;}
static uint32_t Qflash_Erase_Sector(uint32_t a){unsigned x=index_of(a,4096);assert(!(x&4095));if(++erases==fail_erase)return 1;memset(flash_image+x,255,4096);return 0;}
#include "ota_crc.c"
#include "ota_frame.c"
#include "udp_bootload.c"
static void reset(void){memset(flash_image,255,sizeof(flash_image));reads=writes=erases=fail_read=fail_write=fail_erase=tear=0;}
static void frame_header(uint8_t *f,unsigned n){unsigned body=n-5;memset(f,0,n);f[0]=0x41;ota_encode_length(f+1,body);f[3]=OTA_CTRL_DOWN;f[10]=OTA_AFN;f[n-2]=ota_frame_crc_to_cs(ota_frame_get_crc(f+3,body));f[n-1]=0x16;}
int main(void){OTA_CHECKPOINT_STRUCT cp={0,0x0201,400,8,0x1234,204800},got;BOOTLOAD_PARA_STRUCT para={FLASH_APP2_START,512,0x1234,FLASH_APP1_START,0},out;
 unsigned i,j;uint16_t sum;uint8_t frame[547],data[512],before[16];ota_downlink_info_t info;
 /* Short packets have valid framing/CRC but lack the fixed 22-byte header. */
 for(i=15;i<35;i++){frame_header(frame,i);memset(&info,0xa5,sizeof(info));assert(ota_frame_parse_downlink(frame,i,&info)<0);assert(((uint8_t*)&info)[0]==0xa5);}
 frame_header(frame,35);assert(!ota_frame_parse_downlink(frame,35,&info));frame[1]=0xfa;assert(ota_frame_parse_downlink(frame,35,&info)<0);
 frame_header(frame,547);frame[14]=1;frame[31]=2;frame[32]=0;frame[545]=ota_frame_crc_to_cs(ota_frame_get_crc(frame+3,542));assert(!ota_frame_parse_downlink(frame,547,&info)&&info.chunk_len==512);
 for(i=0;i<547;i++)assert(ota_frame_parse_downlink(frame,i,&info)<0);
 reset();assert(!udp_bootload_write_checkpoint(&cp));cp.written_seq=16;assert(!udp_bootload_write_checkpoint(&cp));assert(udp_bootload_read_checkpoint(&got)&&got.written_seq==16&&erases==0);
 for(i=0;i<32;i++){reset();cp.written_seq=8;assert(!udp_bootload_write_checkpoint(&cp));cp.written_seq=16;fail_write=writes+1;tear=i;assert(udp_bootload_write_checkpoint(&cp)<0);fail_write=0;assert(udp_bootload_read_checkpoint(&got)&&got.written_seq==8);}
 for(i=0;i<4;i++){reset();cp.written_seq=8;assert(!udp_bootload_write_checkpoint(&cp));cp.written_seq=16;fail_write=writes+2;tear=i;assert(udp_bootload_write_checkpoint(&cp)<0);fail_write=0;assert(udp_bootload_read_checkpoint(&got)&&got.written_seq==8);}
 reset();for(i=1;i<=CP_RECORD_COUNT+2;i++){cp.written_seq=(uint16_t)(i+1);assert(!udp_bootload_write_checkpoint(&cp));}assert(erases==1&&udp_bootload_read_checkpoint(&got)&&got.written_seq==CP_RECORD_COUNT+3);
 udp_bootload_clear_checkpoint();assert(!udp_bootload_read_checkpoint(&got));
 reset();memset(data,0x5a,sizeof(data));assert(!udp_bootload_stage_chunk(FLASH_APP2_START,data,512)&&!udp_bootload_commit_staged());assert(!ota_prog_checksum16_flash_checked(FLASH_APP2_START,512,&sum)&&sum==46080);
 fail_read=reads+1;sum=0xbeef;assert(ota_prog_checksum16_flash_checked(FLASH_APP2_START,512,&sum)<0&&sum==0xbeef);fail_read=0;
 assert(udp_bootload_stage_chunk(0xffffff00u,data,512)<0);
 /* Every failed config operation leaves the install flag uncommitted. */
 for(i=1;i<=2;i++){reset();fail_write=i;tear=0;assert(udp_bootload_write_cfg(&para)<0);fail_write=0;assert(!udp_bootload_read_para(&out)&&out.prog_success_flag!=OTA_UPGRADE_FLAG);}
 reset();assert(!udp_bootload_write_cfg(&para));memcpy(before,flash_image+FLASH_OTA_CFG_START-0x01000000u,16);udp_bootload_clear_checkpoint();assert(!udp_bootload_read_para(&out)&&out.prog_success_flag==OTA_UPGRADE_FLAG&&!memcmp(before,&out,16));
 reset();for(j=0;j<512;j++)data[j]=(uint8_t)j;assert(!udp_bootload_stage_chunk(FLASH_APP2_START,data,512));fail_read=reads+1;assert(udp_bootload_commit_staged()<0);
 puts("OTA safety: short/truncated/BCD frames, append-only checkpoints, 36 torn writes, rollover, read failures, chunk readback, install flag last PASS");return 0;}
