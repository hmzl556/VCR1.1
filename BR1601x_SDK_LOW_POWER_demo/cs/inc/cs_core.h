
#ifndef __CS_CORE_H__
#define __CS_CORE_H__
#include <stdint.h>
#include <stdbool.h>
#include "br1601x.h"
typedef void (*dtm_rcv_t)(uint8_t *bufptr, uint16_t size);

/*
 * Initial the commucation system core
 */
void cs_init(uint8_t local_bd[6]);

/*
 * Loop the commucation system schedule
 */
void cs_schedule(void);
void cs_get_sdk_version(void);
void cs_event(uint8_t ev,void *func,void* arg);

/**
 * @brief  thread safe malloc
 * @param  uint32_t size
 * @return data pointer or NULL if allocate failed
 * @note this function will disable all the interrupt while call malloc();
 */
void *brt_malloc(uint32_t size);

/**
 * @brief  thread safe free
 * @param  void *ptr pointer to freed which was allocated by malloc()/or brt_malloc()
 * @return none
 * @note this function will disable all the interrupt while call free();
 */
void brt_free(void *ptr);

void host_init(void);

/**
 * @brief  Set the mac address in trim3 opt area
 * @param  uint8_t mac[6],the buffer to input
 * @return true if trim3 set the mac address successful
 * @note
 */
bool SystemSetMacAddr(uint8_t mac[6]);

/**
 * @brief  Get the mac address in trim3 opt area
 * @param  uint8_t mac[6],the buffer to output
 * @return true if trim3 has mac,false not
 * @note
 */
bool GetBarrotMacAddr(uint8_t mac[6]);

/**
 * @brief  Fixed frequency
 * @param  uint8_t channel, 0-39 ,Under normal circumstances, it is always 0
 * @return uint8_t  0: success | 1-255: error
 * @note
 */
uint8_t br_freq_fix(uint8_t channel);

/**
 * @brief  dtm data transmission init
 * @param  dtm_rcv_t rcv_evt , the callback function to receive the data
 * @note
 */
void dtm_hci_init(dtm_rcv_t rcv_evt);

/**
 * @brief  dtm data transmission send
 * @param  uint8_t *bufptr, the buffer to send
 * @param  uint32_t size, the size of the buffer
 * @note
 */
void dtm_send_cmd(uint8_t *bufptr, uint32_t size);

/**
 * @brief  irq remove
 * @param  IRQn_Type irq, irq
 * @return uint32_t, 0: success | other: error
 * @note
 */
uint32_t ModuleIrqRemoval(IRQn_Type irq);

/**
 * @brief  irq Register
 * @param  IRQn_Type irq, irq
 * @param  void* , irq handler cbk
 * @return uint32_t, 0: success | other: error
 * @note
 */
uint32_t ModuleIrqRegister(IRQn_Type irq,void*);
#endif /* __CS_CORE_H__ */
