
#ifndef __CFG_USER_H__
#define __CFG_USER_H__

#include <stdint.h>

enum
{
    LSCI_32000HZ = 0,
    LSCI_32768HZ,
    LSCI_28800HZ,
    LSCE_32768HZ
};

typedef struct user_cfg_stru
{
    uint8_t scan_en;
    uint8_t lsc_type;
} user_cfg;

#endif
