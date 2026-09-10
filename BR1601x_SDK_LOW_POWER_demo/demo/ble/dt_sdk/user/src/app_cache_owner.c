/**
 * @file app_cache_owner.c
 * @brief Coordinates exclusive IMAGE and DFU access to application cache.
 */
#include "app_cache_owner.h"

static app_cache_owner_t cache_owner;

void app_cache_owner_init(void)
{
    cache_owner = APP_CACHE_OWNER_NONE;
}

bool app_cache_acquire(app_cache_owner_t owner)
{
    if (owner == APP_CACHE_OWNER_NONE)
    {
        return false;
    }
    if (cache_owner == APP_CACHE_OWNER_NONE || cache_owner == owner)
    {
        cache_owner = owner;
        return true;
    }
    return false;
}

void app_cache_release(app_cache_owner_t owner)
{
    if (cache_owner == owner)
    {
        cache_owner = APP_CACHE_OWNER_NONE;
    }
}

app_cache_owner_t app_cache_owner_get(void)
{
    return cache_owner;
}
