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
 * @file dfu_action.h
 * @author Barrot Firmware Team
 * @version v1.0.0
 */
#ifndef _DFU_ACTION_H
#define _DFU_ACTION_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
/* Public define ------------------------------------------------------------*/
#define DFU_OPCODE_START                    (0xF1)
#define DFU_OPCODE_DATA                     (0xF2)
#define DFU_OPCODE_END                      (0xF3)
#define DFU_OPCODE_READCHIPVER              (0xF4)
#define DFU_OPCODE_ACK                      (0xFC)

/* Public typedef -----------------------------------------------------------*/
struct s_start
{
    uint8_t mtu;
    uint8_t credit;
    uint8_t checksum;
};

struct s_data
{
    uint16_t index;
    uint8_t checksum;
};

struct s_end
{
    uint8_t checksum;
};

union u_rsp
{
    uint8_t *data;
    struct s_start  start;
    struct s_data  trans;
    struct s_end  end;
};

typedef struct
{
    uint8_t length; // @ref DFU_RESP_SIZE
    uint8_t rsp_code;
    uint8_t opcode;
    uint8_t result;
    union u_rsp rsp;
}dfu_response_t;

void dfu_action_init(void);
void dfu_action_write(uint8_t *data, uint32_t len, dfu_response_t *p_out);

#endif  //_DFU_ACTION_H
