/**
 * @file    ir_learn.h
 * @brief   红外学习模块 — TIM3 输入捕获解码 NEC 码
 * @note    硬件方案：PB5 = TIM3_CH2（AF2）双边沿捕获，TIM3 时钟 64MHz
 *          分频 1MHz（1 tick = 1μs），中断里记录脉宽，主循环解码。
 *
 *          学习流程：ir_learn_start() 进入学习 → 接收 NEC 帧并解码 →
 *          通过回调通知结果（成功/超时/取消）。15 秒超时（GY/T 407-2024 §6.2）。
 */
#ifndef _IR_LEARN_H_
#define _IR_LEARN_H_

#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * 一、学习结果事件
 *===========================================================================*/

typedef enum
{
    IR_LEARN_EVT_DONE = 0,   /* 解码成功，user/cmd 有效 */
    IR_LEARN_EVT_FAILED,     /* 收到帧但解码失败 */
    IR_LEARN_EVT_TIMEOUT,    /* 15 秒未收到有效帧 */
    IR_LEARN_EVT_CANCELED,   /* 主动取消 */
} ir_learn_evt_t;

/**
 * @brief  学习结果回调
 * @param  evt  事件类型
 * @param  user 解码出的 NEC 用户码（仅 DONE 时有效）
 * @param  cmd  解码出的 NEC 数据码（仅 DONE 时有效）
 */
typedef void (*ir_learn_cb_t)(ir_learn_evt_t evt, uint8_t user, uint8_t cmd);

/*===========================================================================
 * 二、公共 API
 *===========================================================================*/

/**
 * @brief  初始化学习模块（PB5 → TIM3_CH2 + NVIC）
 * @param  cb 学习结果回调
 * @note   初始化后 TIM3 计数常开，不进入学习态。
 */
void ir_learn_init(ir_learn_cb_t cb);

/**
 * @brief  开始学习（进入学习态，等待红外帧）
 * @return true = 已进入学习，false = 已在学习中
 */
bool ir_learn_start(void);

/**
 * @brief  停止学习（主动取消）
 */
void ir_learn_stop(void);

/**
 * @brief  查询是否处于学习态
 * @return true = 学习中，false = 空闲
 */
bool ir_learn_is_active(void);

/**
 * @brief  学习主循环轮询（须周期性调用）
 * @note   负责 15 秒超时检测与解码结果上报。
 */
void ir_learn_process(void);

/**
 * @brief  设置本次学习的目标按键号
 * @param  key 物理按键号（KEYSCAN_KEY1..17）
 * @note   学习成功后回调里通过 ir_learn_get_target() 获取并落库。
 */
void ir_learn_set_target(uint8_t key);

/**
 * @brief  获取本次学习的目标按键号
 * @return 目标按键号；未设置返回 0
 */
uint8_t ir_learn_get_target(void);

#endif /* _IR_LEARN_H_ */
