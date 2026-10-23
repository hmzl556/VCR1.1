#ifndef APP_GATT_SAFE_H
#define APP_GATT_SAFE_H
#include "gatt.h"
#include "br1601x.h"
#include <stdlib.h>
/* This SDK queues len+12 bytes and a 16-byte FSM node without NULL checks.
 * Preflight a contiguous block with 512 bytes of spare capacity. Keep IRQs
 * masked across probe/free/submission so another allocator cannot steal it.
 * This protects entry to the observed SDK path, not later stack allocations.
 * SDK GATT_OK means submission only, never remote receipt. */
static gatt_rst_t app_gatt_submit(peer_t peer,gatt_chr_t chr,uint8_t *data,uint16_t len,int indicate)
{
    uint32_t mask;
    void *probe;
    gatt_rst_t result;
    if (!peer || !chr || !data || !len) return GATT_RES_INVALID_HANDLE;
    mask=__get_PRIMASK();__disable_irq();
    probe=malloc((size_t)len+512u);
    if(!probe){__set_PRIMASK(mask);return GATT_RES_INSUFFICIENT_RESOURCES;}
    free(probe);
    result=indicate?gatts_chr_val_indicate(peer,chr,data,len):gatts_chr_val_notif(peer,chr,data,len);
    __set_PRIMASK(mask);
    return result;
}
#define app_gatt_notify(p,c,d,n) app_gatt_submit((p),(c),(d),(n),0)
#define app_gatt_indicate(p,c,d,n) app_gatt_submit((p),(c),(d),(n),1)
#endif
