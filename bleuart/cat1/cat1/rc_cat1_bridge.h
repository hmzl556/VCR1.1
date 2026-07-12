#ifndef RC_CAT1_BRIDGE_H
#define RC_CAT1_BRIDGE_H

#include <stdint.h>
#include "rc_protocol.h"

void rc_cat1_init(void);
void rc_cat1_poll(void);
void rc_cat1_poll_ota_exclusive(void);
int rc_cat1_on_tlv_ready(uint16_t tlv_len, uint32_t pack_index);
void rc_cat1_on_udp_done(uint8_t result, uint32_t pack_index);
void rc_cat1_get_cloud_policy(uint8_t policy[6]);
void rc_cat1_cancel_pending(void);

#endif /* RC_CAT1_BRIDGE_H */
