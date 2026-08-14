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
 * @file pwr.h
 * @author Barrot Firmware Team
 * @version v1.1.0
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */
#ifndef __PWR_H__
#define __PWR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "br1601x.h"

/** @addtogroup StdPeriph_Driver
 * @{
 */

/**
 * @}
 */

/** @defgroup SLEEP_mode_entry 
  * @{
  */
#define PWR_IDLEENTRY_WFI         ((uint8_t)0x01)
#define PWR_IDLEENTRY_WFE         ((uint8_t)0x02)

 
/**
  * @}
  */


/** @defgroup SLEEP_mode_entry 
  * @{
  */

#define PWR_SLEEPENTRY_WFI        ((uint8_t)0x01)
#define PWR_SLEEPENTRY_WFE        ((uint8_t)0x02)

#define PWR_STANDBYENTRY_WFI      ((uint8_t)0x01)
#define PWR_STANDBYENTRY_WFE      ((uint8_t)0x02)

/** @defgroup Powerdown_mode_entry
  * @{
  */

#define PWR_PDENTRY_WFI         ((uint8_t)0x01)
#define PWR_PDENTRY_WFE         ((uint8_t)0x02)


/** @addtogroup PWR_Exported_Functions
 * @{
 */

void PWR_DeInit(void);
void PWR_EnterIDLEMode(uint8_t IDLEONEXIT, uint8_t PWR_IdleEntry);
void PWR_EnterSLEEPMode(uint8_t PWR_SleepEntry);
void PWR_EnterSTANDBYMode(uint8_t PWR_StandbyEntry);
void PWR_EnterPDMode(uint8_t PWR_PDEntry);

#ifdef __cplusplus
}
#endif

#endif /* __PWR_H__ */
       /**
        * @}
        */

/**
 * @}
 */

/**
 * @}
 */
