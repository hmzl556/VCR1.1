/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Barrot's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY BARROT'S "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL BARROT'S BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file app_gpio.c
 * @author Barrot Firmware Team
 * @version v1.0.0
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#include "app_gpio.h"
#include "app_ble.h"
#include "porting.h"
#include "btlist.h"
#include "hal_gpio.h"
#include "task.h"
#include "app_task.h"
#include "log.h"
/** @addtogroup 
 * @{
 */
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
#ifdef CFG_DISC_IO
static task_handler_t *s_disc_tid;
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
static void ble_disc_task(uint8_t ev, void *arg)
{
    extern struct ListStru g_conn_list;
    conn_record_t *node = (conn_record_t *)g_conn_list.head;
    while(node){
        ble_gap_disconn(node->conhdl, 0x13/*CO_ERROR_REMOTE_USER_TERM_CON*/);
        node = LNEXT(node);
    }
}

static void ble_disc_callback(void)
{
    if(hal_gpio_read_bit(BLE_DISC_PIN) == BIT_RESET){
        task_set_active(s_disc_tid);
    } 
}
#endif

void ble_disc_io_init(void)
{
#ifdef CFG_DISC_IO
    hal_gpio_init_t gpio_struct;

	gpio_struct.pin = BLE_DISC_PIN;
	gpio_struct.dir = BLE_DISC_DIR;
    gpio_struct.interrupt_mode = BLE_DISC_INT;
	gpio_struct.arg = NULL;
	gpio_struct.interrupt_callback = (hal_gpio_callback_t *)ble_disc_callback;
	hal_gpio_init(&gpio_struct);
    s_disc_tid = task_create(ble_disc_task, NULL, NULL, false, true);
#endif
}

void ble_conn_io_init(void)
{
#ifdef CFG_CONN_LED
    hal_gpio_init_t gpio_struct;

	gpio_struct.pin = BLE_CONN_IND_PIN;
	gpio_struct.dir = BLE_CONN_IND_DIR;
    gpio_struct.interrupt_mode = BLE_CONN_IND_INT;
	gpio_struct.arg = NULL;
	gpio_struct.interrupt_callback = (hal_gpio_callback_t *)NULL;
	hal_gpio_init(&gpio_struct);
    LED_CONN_IND_OFF();  
#endif
}

void ble_adv_io_init(void)
{
#ifdef CFG_ADV_LED
    hal_gpio_init_t gpio_struct;

	gpio_struct.pin = BLE_ADV_IND_PIN;
	gpio_struct.dir = BLE_ADV_IND_DIR;
    gpio_struct.interrupt_mode = BLE_ADV_IND_INT;
	gpio_struct.arg = NULL;
	gpio_struct.interrupt_callback = (hal_gpio_callback_t *)NULL;
	hal_gpio_init(&gpio_struct);
	LED_ADV_IND_ON();
#endif
}

void ble_adv_io_deinit(void)
{
#ifdef CFG_ADV_LED
    hal_gpio_deinit(BLE_ADV_IND_PIN);
#endif
}

/**
 * @}
 */
