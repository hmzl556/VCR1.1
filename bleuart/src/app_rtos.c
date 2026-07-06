#include "app_rtos.h"
#include "app_voice_ctrl.h"
#include "rc_cat1_bridge.h"
#include "queue.h"

static QueueHandle_t s_cat1_to_ble_q = NULL;
static StaticQueue_t s_cat1_to_ble_q_struct;
static uint8_t s_cat1_to_ble_q_storage[4U * sizeof(cat1_ble_evt_t)];

/**
 * @brief  初始化 RTOS 相关资源
 */
void app_rtos_init(void)
{
	s_cat1_to_ble_q = xQueueCreateStatic(4U, sizeof(cat1_ble_evt_t),
	                                     s_cat1_to_ble_q_storage,
	                                     &s_cat1_to_ble_q_struct);
	voice_ctrl_init();
}

/**
 * @brief  向 BLE 任务发送 Cat.1 模块事件
 * @param  result 事件结果
 * @param  pack_index 相关的包索引
 * @retval pdTRUE 表示成功发送事件，pdFALSE 表示队列未创建或已满
 */
BaseType_t cat1_post_udp_done(uint8_t result, uint32_t pack_index)
{
	cat1_ble_evt_t evt;

	if (s_cat1_to_ble_q == NULL) {
		return pdFAIL;
	}
	evt.result = result;
	evt.pack_index = pack_index;
	return xQueueSend(s_cat1_to_ble_q, &evt, 0);
}

/**
 * @brief  处理 Cat.1 模块事件队列中的事件，应该在 BLE 任务的循环中定期调用以处理待处理的事件
 */
void cat1_ble_process_pending(void)
{
	cat1_ble_evt_t evt;

	if (s_cat1_to_ble_q == NULL) {
		return;
	}
	while (xQueueReceive(s_cat1_to_ble_q, &evt, 0) == pdTRUE) {
		rc_cat1_on_udp_done(evt.result, evt.pack_index);
	}
}
