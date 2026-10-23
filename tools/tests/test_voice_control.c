#include "mock_merge.h"
typedef int BaseType_t;
typedef int StaticQueue_t;
typedef void *QueueHandle_t;
#define pdFAIL 0
#define pdTRUE 1
#define VOICE_CMD_START 0u
#define VOICE_CMD_STOP 1u
#define BLE_DTSS_DATA_READY 1u
#define BLE_DTSS_CTRL_READY 2u
static uint8_t ready_channels=3u;
static uint8_t ble_dtss_ready_channels(void){return ready_channels;}
static uint8_t cmds[4];
static unsigned count,head,started,stopped,ir_busy,connected=1;
static QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,uint8_t *buf,StaticQueue_t *q){(void)n;(void)size;(void)buf;(void)q;return cmds;}
static BaseType_t xQueueSend(QueueHandle_t q,const uint8_t *cmd,unsigned wait){(void)q;(void)wait;if(count==4)return 0;cmds[(head+count++)%4]=*cmd;return 1;}
static BaseType_t xQueueReceive(QueueHandle_t q,uint8_t *cmd,unsigned wait){(void)q;(void)wait;if(!count)return 0;*cmd=cmds[head];head=(head+1)%4;--count;return 1;}
static BaseType_t xQueueReset(QueueHandle_t q){(void)q;count=head=0;return 1;}
static void Audio_Pcm_Start(void){++started;}
static void Audio_Pcm_Stop(void){++stopped;}
void pm_activity_notify(pm_activity_t a){(void)a;}
static bool task_keyscan_ir_busy(void){return ir_busy!=0;}
peer_t ble_gap_get_peer(void){return connected?(peer_t)1:0;}
#include "actual_voice_control.c"
int main(void){unsigned i;
 voice_ctrl_init();for(i=0;i<4;i++)assert(voice_ctrl_post_start());assert(voice_ctrl_post_stop());voice_ctrl_process_pending();assert(!started&&stopped==1);
 assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);
 ota_busy=1;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);ota_busy=0;
 ir_busy=1;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);ir_busy=0;
 connected=0;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);
 assert(voice_ctrl_post_stop());voice_ctrl_process_pending();assert(stopped==2);
 connected=1;
 ready_channels=0;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);
 ready_channels=1;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);
 ready_channels=2;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==1);
 ready_channels=3;assert(voice_ctrl_post_start());voice_ctrl_process_pending();assert(started==2);
 assert(voice_ctrl_post_start());assert(voice_ctrl_post_stop());voice_ctrl_process_pending();assert(started==2&&stopped==3);
 puts("voice control: STOP survives full queue, OTA/IR/disconnect reject stale START, STOP always works PASS");return 0;}
