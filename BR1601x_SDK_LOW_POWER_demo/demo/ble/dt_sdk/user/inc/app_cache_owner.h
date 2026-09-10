/**
 * @file app_cache_owner.h
 * @brief Shared application-cache ownership interface.
 */
#ifndef APP_CACHE_OWNER_H
#define APP_CACHE_OWNER_H

#include <stdbool.h>

typedef enum
{
    APP_CACHE_OWNER_NONE,
    APP_CACHE_OWNER_IMAGE,
    APP_CACHE_OWNER_DFU,
} app_cache_owner_t;

void app_cache_owner_init(void);
bool app_cache_acquire(app_cache_owner_t owner);
void app_cache_release(app_cache_owner_t owner);
app_cache_owner_t app_cache_owner_get(void);

#endif
