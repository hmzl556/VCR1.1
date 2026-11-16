/**
 * @file upg_session.c
 * @brief 键值下载独占会话实现
 */
#include "upg_session.h"
#include "udp_bootload.h"
#include "udp_ota.h"
#include "gmem.h"
#include "co_printf.h"
#include <stddef.h>
#include <string.h>

/** 会话空闲超时：超过该时间无帧交互即退出，恢复 CAT1 与广播 */
#define UPG_SESSION_IDLE_MS   30000u

/* 会话态放在借来的缓冲里，静态只保留两个标志 */
static bool      s_active;
static upg_ws_t *s_ws;

/* 编译期保证工作区不超过借来的 512B（另有 ≤4B 对齐余量） */
typedef char upg_ws_size_check[(sizeof(upg_ws_t) <= UPG_WS_NEED) ? 1 : -1];

bool upg_session_begin(void)
{
    uint8_t *raw;
    uint16_t raw_len = 0u;
    uint32_t addr;

    if (s_active) {
        return true;
    }
    /* OTA 会话优先；两者不可并存（共用 Flash 写缓冲与独占语义）。
     * 用 session_busy 而非 is_active：PAUSE 退避期间 phase 已回 IDLE，
     * 但 OTA 仍占用 Flash 写缓冲与独占资源。 */
    if (udp_ota_session_busy() != 0u) {
        co_printf("[UPG ] begin denied: OTA active\r\n");
        return false;
    }

    raw = udp_bootload_borrow_buf(&raw_len);
    if (raw == NULL || raw_len < (uint16_t)(UPG_WS_NEED + 4u)) {
        co_printf("[UPG ] begin denied: buf %u\r\n", (unsigned)raw_len);
        return false;
    }

    /* Cortex-M0 不支持非对齐字访问：把工作区起点抬到 4 字节边界 */
    addr = (uint32_t)(size_t)raw;
    if ((addr & 3u) != 0u) {
        raw += (4u - (addr & 3u));
    }
    s_ws = (upg_ws_t *)(void *)raw;
    memset(s_ws, 0, sizeof(*s_ws));

    s_active = true;
    s_ws->st.last_ms = app_now_ms();
    s_ws->st.entered = 0u;      /* 独占资源在任务上下文中获取，见 upg_session_poll() */
    return true;
}

void upg_session_end(void)
{
    if (!s_active) {
        return;
    }
    if (s_ws != NULL && s_ws->st.entered != 0u) {
        udp_ota_session_exit();   /* 恢复广播，须在任务上下文 */
    }
    s_active = false;
    s_ws = NULL;
    co_printf("[UPG ] session end\r\n");
}

bool upg_session_busy(void)
{
    return s_active;
}

void upg_session_touch(void)
{
    if (s_active && s_ws != NULL) {
        s_ws->st.last_ms = app_now_ms();
    }
}

void upg_session_poll(void)
{
    if (!s_active || s_ws == NULL) {
        return;
    }
    /* 停广播/停语音会调用协议栈，必须在任务上下文执行，不能放在 GATT 回调里 */
    if (s_ws->st.entered == 0u) {
        udp_ota_session_enter();
        s_ws->st.entered = 1u;
        co_printf("[UPG ] session begin\r\n");
    }
    if ((uint32_t)(app_now_ms() - s_ws->st.last_ms) >= UPG_SESSION_IDLE_MS) {
        co_printf("[UPG ] idle timeout\r\n");
        upg_session_end();
    }
}

upg_ws_t *upg_ws(void)
{
    return s_active ? s_ws : NULL;
}
