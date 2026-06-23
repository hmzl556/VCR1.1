
#ifndef __CS_CORE_H__
#define __CS_CORE_H__
#include <stdint.h>
#include <stdbool.h>



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
#endif /* __CS_CORE_H__ */
