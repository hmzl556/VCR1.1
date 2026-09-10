/**
 * @file prf_image.h
 * @brief Bluetooth label image-transfer GATT service.
 */
#ifndef PRF_IMAGE_H
#define PRF_IMAGE_H

#include <stdint.h>

int prf_image_init(void);
uint32_t prf_image_send(uint16_t conhdl, const uint8_t *data, uint16_t length);

void prf_image_disconnect(void);
#endif
