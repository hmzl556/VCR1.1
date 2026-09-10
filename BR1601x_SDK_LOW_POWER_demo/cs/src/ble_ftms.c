/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Copyright (c) Barrot Technology Limited
 *
 * All rights reserved.
 *
---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
Module Name:
	ble_ftms.c
Abstract:
	An implementation of fitness machine service.
Author:
	Y.D.X
-----------------------------------------------------------------------------*/

#include "gatt.h"
#include "ble_ftms.h"
#include "cs_core.h"


#define CFG_FTMS_MAX_PEERS 3    /* MAX number of concurrent connected peers supported. SHOULD be replaced by a common configuration of the actual project. */

#define FTMS_DATA_FLAG_MORE_DATA (1<<0)

#define FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED    (1<<0) /* Fitness Machine Control Point indication enabled. */
#define FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED   (1<<1) /* Control procedure is permitted. */
#define FTMS_CTL_FLAG_WAIT4_CONTROL_RSP             (1<<2) /* Waiting for the response to control procedure request. */
#define FTMS_CTL_FLAG_WAIT4_TRAINING_STATUS_RSP     (1<<3) /* Waiting for the response to training status read request. */

typedef enum {
    FTMS_PAR_OK, /* GATT_OK */
    FTMS_PAR_GATT_ERROR,
    FTMS_PAR_FTMS_ERROR,
} ftms_check_par_res_t;

typedef struct {
    peer_t peer;
    uint16_t mtu;
    uint8_t  ctl_flag; /* FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED etc. */
} ftms_peer_t;

#define FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS         (1<<0) /* A control procedure is in progress. */

typedef struct {
    uint32_t       mgr_flags; /* FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS etc. */
    ftms_cbk_t     *cbk;
    void           *ctx;

    uint32_t       ftm_feat_mask;
    uint32_t       tgs_feat_mask;
    gatt_svc_t     ftms_svc;
    gatt_chr_t     treadmill_data_chr;
    gatt_chr_t     cross_trainer_data_chr;
    gatt_chr_t     step_climber_data_chr;
    gatt_chr_t     stair_climber_data_chr;
    gatt_chr_t     rower_data_chr;
    gatt_chr_t     indoor_bike_data_chr;
    gatt_chr_t     training_status_chr;
    gatt_chr_t     ctl_point_chr;
    gatt_chr_t     ftm_status_chr;

    ftms_peer_t    peers[CFG_FTMS_MAX_PEERS];
} ftms_mgr_t;

static ftms_mgr_t *s_ftms = NULL;
app_ftms_peer_t s_ftms_peer_dev;

#define FTMS_CBK(_ev, _par) \
do { \
    if (s_ftms->cbk) s_ftms->cbk((_ev), s_ftms->ctx, _par); \
} while (0)

#define FTMS_CCC_PROPS          (GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE | GATT_CHR_PROP_NOTIFY)
//#define FTMS_CCC_PERMS          (GATT_SECU_WRITE_AUTHEN | GATT_SECU_WRITE_ENCRYPT)
#define FTMS_CCC_PERMS          (ATT_PERM_READ | ATT_PERM_WRITE |ATT_PERM_WRITE_AUTHEN_REQUEIRED|ATT_PERM_WRITE_ENCRYPT_REQUEIRED)

#define FTMS_COPY_INT2(dest_p, src_int)\
	*(uint8_t *)((dest_p) + 1) = (uint8_t)((src_int) >> 8);\
	*(uint8_t *)(dest_p) = (uint8_t)(src_int);
#define FTMS_COPY_INT3(dest_p, src_int)\
	*(uint8_t *)((dest_p) + 2) = (uint8_t)((src_int) >> 16);\
	*(uint8_t *)((dest_p) + 1) = (uint8_t)((src_int) >> 8);\
	*(uint8_t *)(dest_p) = (uint8_t)(src_int);
#define FTMS_COPY_INT4(dest_p, src_int)\
	*(uint8_t *)((dest_p) + 3) = (uint8_t)((src_int) >> 24);\
	*(uint8_t *)((dest_p) + 2) = (uint8_t)((src_int) >> 16);\
	*(uint8_t *)((dest_p) + 1) = (uint8_t)((src_int) >> 8);\
	*(uint8_t *)(dest_p) = (uint8_t)(src_int);

#define FTMS_GET_INT2(ptr) 	(uint16_t)(*(uint8_t *)(ptr) | (*(uint8_t *)((ptr) + 1) << 8))
#define FTMS_GET_INT3(ptr)\
        (uint32_t)(*(uint8_t *)(ptr) | (*(uint8_t *)((ptr) + 1) << 8) \
        | (*(uint8_t *)((ptr) + 2) << 16))
#define FTMS_GET_INT4(ptr)\
        (uint32_t)(*(uint8_t *)(ptr) | (*(uint8_t *)((ptr) + 1) << 8) \
        | (*(uint8_t *)((ptr) + 2) << 16) | (*(uint8_t *)((ptr) + 3) << 24))

#define FTMS_DEFAULT_ATT_MTU     23

static ftms_peer_t * _ftms_find_peer(peer_t *peer)
{
    ftms_peer_t *cur;

    if (s_ftms != NULL) {
        ftms_peer_t *end = s_ftms->peers + CFG_FTMS_MAX_PEERS;

        cur = s_ftms->peers;
        while ((cur < end) && (cur->peer != peer)) {
            cur++;
        }
        if (cur == end) {
            cur = NULL;
        }
    } else {
        cur = NULL;
    }
    return cur;
}

static void _ftms_gap_conn_cmpl_cbk(gap_event_t ev, void *ctx, gap_conn_cmpl_t *par)
{
    /* event == GAP_EV_CONN_CMPL */
    if ((par->result == GATT_OK) && (s_ftms != NULL)) {
        ftms_peer_t *cur = s_ftms->peers;
        ftms_peer_t *end = s_ftms->peers + CFG_FTMS_MAX_PEERS;
        ftms_peer_t *tg = end;

        while (cur < end) {
            if ((cur->peer == NULL) && (tg == end)) {
                /* First free item */
                tg = cur;
            } else if (cur->peer == par->peer) {
                /* Repeated item */
                return;
            }
            cur++;
        }
        if (tg != end) {
            tg->peer = par->peer;
            tg->mtu = FTMS_DEFAULT_ATT_MTU;
            FTMS_CBK(FTMS_EV_ASK4_CONTROL_INDICATION_STATE, par->peer);
        }
    }
}

static void _ftms_gap_disc_cmpl_cbk(gap_event_t ev, void *ctx, gap_disc_cmpl_t *par)
{
    /* event == GAP_EV_DISC_CMPL */
    ftms_peer_t *ftms_peer = _ftms_find_peer(par->peer);
    if (ftms_peer != NULL) {
        if (ftms_peer->ctl_flag & FTMS_CTL_FLAG_WAIT4_CONTROL_RSP) {
            /* This is the only one procedure in process. */
            s_ftms->mgr_flags &= ~FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS;
        }
        memset(ftms_peer, 0, sizeof(ftms_peer_t));
    }
}

static void _ftms_gap_att_mtu_notify_cbk(gap_event_t ev, void *ctx, att_mtu_ntf_t *par)
{
    /* event == GAP_EV_ATT_MTU_NOTIFY */
    /* par->mtu >= FTMS_DEFAULT_ATT_MTU */
    ftms_peer_t *ftms_peer = _ftms_find_peer(par->peer);
    if (ftms_peer != NULL) {
        ftms_peer->mtu = par->mtu;
    }
}

static const gap_ev_cbk_t s_ftms_gap_cbks[] = {
    {GAP_EV_CONN_CMPL, (gap_cbk_t *)_ftms_gap_conn_cmpl_cbk},
    {GAP_EV_DISC_CMPL, (gap_cbk_t *)_ftms_gap_disc_cmpl_cbk},
    {GAP_EV_ATT_MTU_NOTIFY, (gap_cbk_t *)_ftms_gap_att_mtu_notify_cbk}
};

static void _ftms_start_cfm(ftms_cbk_t *cbk, void *ctx, int8_t res)
{
    ftms_start_t rst;
    rst.result = res;
    if (cbk != NULL) cbk(FTMS_EV_START, ctx, &rst);
}

static void _ftms_state_report_ind(ftms_event_t evt, peer_t peer, uint8_t on_off_state)
{
    ftms_report_state_t rst;
    rst.peer = peer;
    rst.state = on_off_state;
    s_ftms->cbk(evt, s_ftms->ctx, &rst);
}

static void _ftms_cbk_svc(gatt_event_t event, void *context, gatt_svc_start_t *in)
{
    do {
        if (NULL == s_ftms) break;
        if (in->svc != s_ftms->ftms_svc) break;

        switch (event) {
            case GATTS_EV_SERVICE_START: {
                ftms_start_t par = {in->result};
                co_printf("ftm svc start rst:%X\n", in->result);
                gap_ev_subscribe(s_ftms_gap_cbks, sizeof(s_ftms_gap_cbks) / sizeof(gap_ev_cbk_t));
                FTMS_CBK(FTMS_EV_START, &par);
                break;
            }

            case GATTS_EV_SERVICE_STOP: {
                FTMS_CBK(FTMS_EV_STOP, NULL);
                brt_free(s_ftms);
                s_ftms = NULL;
                break;
            }

            default:
                break;
        }
    } while (0);
}

static void _ftms_chr_treadmill_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->treadmill_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_TREADMILL_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_cross_trainer_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->cross_trainer_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_CROSS_TRAINER_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_step_climber_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->step_climber_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_STEP_CLIMBER_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_stair_climber_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->stair_climber_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_STAIR_CLIMBER_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_rower_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->rower_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_ROWER_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_indoor_bike_data_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->indoor_bike_data_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_INDOOR_BIKE_DATA_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

static void _ftms_chr_training_status_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->training_status_chr == param->chr)) {
        if (event == GATTS_EV_CHR_VALUE_READ_REQ) {
            /* gatt_chr_val_read_req_t *param */
            ftms_peer->ctl_flag |= FTMS_CTL_FLAG_WAIT4_TRAINING_STATUS_RSP;
            s_ftms->cbk(FTMS_EV_READ_TRAINING_STATUS_IND, s_ftms->ctx, param->peer);
        } else {
            /* event == GATTS_EV_CHR_VALUE_SEND_START */
            _ftms_state_report_ind(FTMS_EV_TRAINING_STATUS_REPORT_STATE, param->peer, (uint8_t)param->type);
        }
    } else if (event == GATTS_EV_CHR_VALUE_READ_REQ) {
        gatts_chr_val_read_rsp(param->peer, param->chr, GATT_RES_UNLIKELY_ERROR, NULL, 0);
    }
}

static uint8_t _ftms_decode_ctl_parameter(gatt_chr_val_write_req_t *inp, ftms_control_point_write_t *outp)
{
    uint8_t res = FTMS_CTL_RESULT_SUCCESS;
    const uint8_t *val = inp->val;

    outp->peer = inp->peer;
    outp->request.op_code = *val++;
    switch (*inp->val) {
        case FTMS_CTL_OPC_REQUEST_CONTROL:
        case FTMS_CTL_OPC_RESET:
        case FTMS_CTL_OPC_START_OR_RESUME:
            /* No additional parameter */
            break;
        case FTMS_CTL_OPC_SET_TARGET_SPEED:
        case FTMS_CTL_OPC_SET_TARGET_INCLINATION:
        case FTMS_CTL_OPC_SET_TARGET_POWER:
        case FTMS_CTL_OPC_SET_TARGETED_EXPENDED_ENERGY:
        case FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STEPS:
        case FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STRIDES:
        case FTMS_CTL_OPC_SET_TARGETED_TRAINING_TIME:
        case FTMS_CTL_OPC_SET_WHEEL_CIRCUMFERENCE:
        case FTMS_CTL_OPC_SET_TARGETED_CADENCE:
            /* UINT16/INT16 */
            if (inp->val_len == 3) {
                outp->request.target.speed = FTMS_GET_INT2(val);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_TARGET_RESISTANCE_LEVEL:
        case FTMS_CTL_OPC_SET_TARGET_HEART_RATE:
        case FTMS_CTL_OPC_STOP_OR_PAUSE:
        case FTMS_CTL_OPC_SPIN_DOWN_CONTROL:
            /* UINT8/INT8 */
            if (inp->val_len == 2) {
                outp->request.target.resistance_level = *val;
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_TARGETED_DISTANCE:
            /* UINT24 */
            if (inp->val_len == 4) {
                outp->request.target.distance = FTMS_GET_INT3(val);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES:
            if (inp->val_len == 5) {
                outp->request.target.two_zone_times.burn_zone = FTMS_GET_INT2(val);
                outp->request.target.two_zone_times.fitness_zone = FTMS_GET_INT2(val + 2);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES:
            if (inp->val_len == 7) {
                outp->request.target.three_zone_times.light_zone = FTMS_GET_INT2(val);
                outp->request.target.three_zone_times.moderate_zone = FTMS_GET_INT2(val + 2);
                outp->request.target.three_zone_times.hard_zone = FTMS_GET_INT2(val + 4);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES:
            if (inp->val_len == 11) {
                outp->request.target.five_zone_times.very_light_zone = FTMS_GET_INT2(val);
                outp->request.target.five_zone_times.light_zone = FTMS_GET_INT2(val + 2);
                outp->request.target.five_zone_times.moderate_zone = FTMS_GET_INT2(val + 4);
                outp->request.target.five_zone_times.hard_zone = FTMS_GET_INT2(val + 6);
                outp->request.target.five_zone_times.maximum_zone = FTMS_GET_INT2(val + 8);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        case FTMS_CTL_OPC_SET_INDOOR_BIKE_SIMULATION_PARAMETERS:
            if (inp->val_len == 7) {
                outp->request.target.simulation_parameter.wind_speed = FTMS_GET_INT2(val);
                outp->request.target.simulation_parameter.grade = FTMS_GET_INT2(val + 2);
                outp->request.target.simulation_parameter.crr = *(val + 4);
                outp->request.target.simulation_parameter.cw = *(val + 5);
                break;
            }
            res = FTMS_CTL_RESULT_INVALID_PARAMETER;
            break;
        default:
            res = FTMS_CTL_RESULT_OP_CODE_NOT_SUPPORTED;
            break;
    }
    return res;
}

static void _ftms_chr_ctl_point_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    uint8_t res;
    ftms_peer_t *ftms_peer;
    ftms_control_point_write_t cpw;

    do {
        if (s_ftms->ctl_point_chr != param->chr) {
            res = GATT_RES_INVALID_HANDLE;
            break;
        }

        ftms_peer = _ftms_find_peer(param->peer);
        if (NULL == ftms_peer) {
            res = GATT_RES_INSUFFICIENT_RESOURCES;
            break;
        }

        if (event == GATTS_EV_CHR_VALUE_SEND_START) {
            if (param->type) {
                ftms_peer->ctl_flag |= FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED;
            } else {
                ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED;
                if (ftms_peer->ctl_flag & FTMS_CTL_FLAG_WAIT4_CONTROL_RSP) {
                    /* Indication is disabled and response is forbidden. */
                    ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_WAIT4_CONTROL_RSP;
                    s_ftms->mgr_flags &= ~FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS;
                }
            }
            _ftms_state_report_ind(FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_REPORT_STATE, param->peer, (uint8_t)param->type);
            break;
        }

        /* event == GATTS_EV_CHR_VALUE_WRITE_REQ */
        if (0 == (ftms_peer->ctl_flag & FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED)) {
            /* If an Op Code is written to the Fitness Machine Control Point characteristic
             * and the Client Characteristic Configuration descriptor of the Fitness Machine
             * Control Point is not configured for indications, the Server shall return an
             * error response with the Attribute Protocol error code set to "Client
             * Characteristic Configuration Descriptor Improperly Configured".
             */
            co_printf("%s %d, Control Point indication not enabled\r\n", __FUNCTION__, __LINE__);
            res = 0x12;//res ?//GATT_RES_CCCD_IMPROPERLY_CONFIGURED;
            break;
        }
        if (s_ftms->mgr_flags & FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS) {
            /* If an Op Code is written to the Fitness Machine Control Point characteristic
             * while the Server is performing a previously triggered Fitness Machine Control
             * Point operation, the Server shall return an error response with the Attribute
             * Protocol error code set to "Procedure Already In Progress".
             */
            res = 0x013;//res ?//GATT_RES_PROCEDURE_ALREADY_IN_PROGRESS;
            co_printf("%s %d, Control procedure in progress\r\n", __FUNCTION__, __LINE__);
            break;
        }

        gatt_chr_val_write_req_t *wrp = (gatt_chr_val_write_req_t *)param;
        if ((NULL == wrp->val) || (0 == wrp->val_len)) {
            res = GATT_RES_INVALID_ATTRIBUTE_VALUE_LENGTH;
            break;
        }
        res = GATT_OK;
    } while (0);

    if (GATTS_EV_CHR_VALUE_WRITE_REQ == event) {
        /* Send GATT Write Response first */
        gatts_chr_val_write_rsp(param->peer, param->chr, (gatt_rst_t)res);

        if (GATT_OK == res) {
            res = _ftms_decode_ctl_parameter((gatt_chr_val_write_req_t *)param, &cpw);
            if (FTMS_CTL_RESULT_SUCCESS == res) {
                if ((ftms_peer->ctl_flag & FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED) ||
                    (FTMS_CTL_OPC_REQUEST_CONTROL == cpw.request.op_code)) {
                    /* Control permission already given or being required by this request */
                    if ((0 == (ftms_peer->ctl_flag & FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED)) ||
                        (cpw.request.op_code != FTMS_CTL_OPC_REQUEST_CONTROL)) {
                        /* Not to report repeated RequestControl which is responded with a SUCCESS directly. */
                        ftms_peer->ctl_flag |= FTMS_CTL_FLAG_WAIT4_CONTROL_RSP;
                        s_ftms->mgr_flags |= FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS;
                        s_ftms->cbk(FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_WRITE_IND, s_ftms->ctx, &cpw);
                        return;
                    }
                } else {
                    res = FTMS_CTL_RESULT_CONTROL_NOT_PERMITTED;
                }
            } else {
            }

            uint8_t pkt[3];
            *pkt = FTMS_CTL_OPC_RESPONSE_CODE;
            *(pkt + 1) = cpw.request.op_code;
            *(pkt + 2) = res;
            gatts_chr_val_indicate(param->peer, param->chr, pkt, 3);
        }
    }
}

static void _ftms_chr_ftm_status_cbk(gatt_event_t event, void *context, gatt_chr_val_send_start_t *param)
{
    /* event == GATTS_EV_CHR_VALUE_SEND_START */
    ftms_peer_t *ftms_peer = _ftms_find_peer(param->peer);
    if ((ftms_peer != NULL) && (s_ftms->ftm_status_chr == param->chr)) {
        _ftms_state_report_ind(FTMS_EV_FITNESS_MACHINE_STATUS_REPORT_STATE, param->peer, (uint8_t)param->type);
    }
}

void ftms_start(ftms_inf_t *inf, ftms_cbk_t *cbk, void *ctx)
{
    if (NULL != s_ftms) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        _ftms_start_cfm(cbk, ctx, -1);
        return;
    }
    if (NULL == cbk) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }
    if (NULL == inf) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        _ftms_start_cfm(cbk, ctx, -1);
        return;
    }

    s_ftms = (ftms_mgr_t *)brt_malloc(sizeof(ftms_mgr_t));
    if (NULL == s_ftms) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        _ftms_start_cfm(cbk, ctx, -1);
        return;
    }
    memset(s_ftms, 0, sizeof(ftms_mgr_t));
    s_ftms->cbk = cbk;
    s_ftms->ctx = ctx;

    co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
    /*0x1826*/
    uuid_t att_uuid = {
          UUID_LEN_16
        , UUID16_VAL_SET(0x1826)
    };
    gatt_svc_t ftms_svc;
    s_ftms->ftms_svc = ftms_svc = gatts_svc_create(&att_uuid, (gatt_cbk_t *)_ftms_cbk_svc, NULL);
    s_ftms->ftm_feat_mask = inf->fitness_machine_features;
    s_ftms->tgs_feat_mask = inf->target_setting_features;

    /* Fitness Machine Feature - Read */
    uint8_t val_b[8];
    att_uuid.val.uuid16 = 0x2ACC;
    FTMS_COPY_INT4(val_b, inf->fitness_machine_features);
    FTMS_COPY_INT4(val_b + 4, inf->target_setting_features);
    gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                           , GATT_CHR_PROP_READ
                           , ATT_PERM_READ
                           , NULL, NULL, val_b, 8);

    /* Treadmill Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_TREADMILL_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2ACD;
       s_ftms->treadmill_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_treadmill_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->treadmill_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Cross Trainer Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_CROSS_TRAINER_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2ACE;
       s_ftms->cross_trainer_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_cross_trainer_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->cross_trainer_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Step Climber Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_STEP_CLIMBER_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2ACF;
       s_ftms->step_climber_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_step_climber_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->step_climber_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Stair Climber Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_STAIR_CLIMBER_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD0;
       s_ftms->stair_climber_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_stair_climber_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->stair_climber_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Power Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_ROWER_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD1;
       s_ftms->rower_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_rower_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->rower_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Indoor Bike Data - Notify */
    if (inf->supported_characteristics & FTMS_CHR_INDOOR_BIKE_DATA_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD2;
       s_ftms->indoor_bike_data_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_indoor_bike_data_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->indoor_bike_data_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Training Status - Read, Notify */
    if (inf->supported_characteristics & FTMS_CHR_TRAINING_STATUS_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD3;
       s_ftms->training_status_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_READ |GATT_CHR_PROP_NOTIFY
                              , 0
                              , (gatt_cbk_t *)_ftms_chr_training_status_cbk, NULL);

       gatts_dsc_ccc_add(s_ftms->training_status_chr
                      , FTMS_CCC_PROPS
                      , FTMS_CCC_PERMS);
    }

    /* Supported Speed Range - Read */
    if (inf->target_setting_features & FTMS_TGSET_FEATURE_SPEED_TARGET_SETTING_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD4;
       FTMS_COPY_INT2(val_b, inf->speed_range.min_value);
       FTMS_COPY_INT2(val_b + 2, inf->speed_range.max_value);
       FTMS_COPY_INT2(val_b + 4, inf->speed_range.min_increment);
       gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                              , GATT_CHR_PROP_READ
                              , ATT_PERM_READ
                              , NULL, NULL, val_b, 6);
    }

    /* Supported Inclination Range - Read */
    if (inf->target_setting_features & FTMS_TGSET_FEATURE_INCLINATION_TARGET_SETTING_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD5;
       FTMS_COPY_INT2(val_b, inf->inclination_range.min_value);
       FTMS_COPY_INT2(val_b + 2, inf->inclination_range.max_value);
       FTMS_COPY_INT2(val_b + 4, inf->inclination_range.min_increment);
       gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                              , GATT_CHR_PROP_READ
                              , ATT_PERM_READ
                              , NULL, NULL, val_b, 6);
    }

    /* Supported Resistance Level Range - Read */
    if (inf->target_setting_features & FTMS_TGSET_FEATURE_RESISTANCE_TARGET_SETTING_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD6;
       gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                              , GATT_CHR_PROP_READ
                              , ATT_PERM_READ
                              , NULL, NULL, (uint8_t *)&inf->resistance_level_range, 3);
    }

    /* Supported Heart Rate Range - Read */
    if (inf->target_setting_features & FTMS_TGSET_FEATURE_HEART_RATE_TARGET_SETTING_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD7;
       gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                              , GATT_CHR_PROP_READ
                              , ATT_PERM_READ
                              , NULL, NULL, (uint8_t *)&inf->heart_rate_range, 3);
    }

    /* Supported Power Range - Read */
    if (inf->target_setting_features & FTMS_TGSET_FEATURE_POWER_TARGET_SETTING_SUPPORTED) {
       att_uuid.val.uuid16 = 0x2AD8;
       FTMS_COPY_INT2(val_b, inf->power_range.min_value);
       FTMS_COPY_INT2(val_b + 2, inf->power_range.max_value);
       FTMS_COPY_INT2(val_b + 4, inf->power_range.min_increment);
       gatts_chr_ro_add_uuid16(ftms_svc, att_uuid.val.uuid16
                              , GATT_CHR_PROP_READ
                              , ATT_PERM_READ
                              , NULL, NULL, val_b, 6);
    }

    if ((inf->supported_characteristics & FTMS_CHR_FTM_CONTROL_POINT_SUPPORTED) == FTMS_CHR_FTM_CONTROL_POINT_SUPPORTED) {
        /* Fitness Machine Control Point - Write, Indicate */
        att_uuid.val.uuid16 = 0x2AD9;
        s_ftms->ctl_point_chr = gatts_chr_add(ftms_svc, &att_uuid
                              , GATT_CHR_PROP_WRITE |GATT_CHR_PROP_INDICATE
                              , FTMS_CCC_PERMS
                              , (gatt_cbk_t *)_ftms_chr_ctl_point_cbk, NULL);

        gatts_dsc_ccc_add(s_ftms->ctl_point_chr
                        , GATT_CHR_PROP_READ | GATT_CHR_PROP_WRITE | GATT_CHR_PROP_INDICATE
                        , FTMS_CCC_PERMS);
    }

    if (inf->supported_characteristics & FTMS_CHR_FTM_MACHINE_STATUS_SUPPORTED) {
        /* Fitness Machine Status - Notify */
        att_uuid.val.uuid16 = 0x2ADA;
        s_ftms->ftm_status_chr = gatts_chr_add(ftms_svc, &att_uuid
                               , GATT_CHR_PROP_NOTIFY
                               , 0
                               , (gatt_cbk_t *)_ftms_chr_ftm_status_cbk, NULL);

        gatts_dsc_ccc_add(s_ftms->ftm_status_chr
                       , FTMS_CCC_PROPS
                       , FTMS_CCC_PERMS);
    }
    co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
    gatts_svc_start(s_ftms->ftms_svc);
}

void ftms_stop(void)
{
    if (NULL == s_ftms) {
        return;
    }

    if (INST_INVALID != s_ftms->ftms_svc) {
        gap_ev_unsubscribe(s_ftms_gap_cbks);
        gatts_svc_stop(s_ftms->ftms_svc);
    }
}

static uint16_t _ftms_pre_set_data_packet(uint32_t *fmts, uint16_t mps, uint32_t *flags_left, uint32_t *flags_set)
{
    uint32_t bit = 1;
    uint32_t mask = 2;
    uint32_t item_size;
    uint32_t fmt = *fmts >> 4;

    *flags_set = 0;
    while (*flags_left) {
        if (mask & *flags_left) {
            item_size = fmt & 0x0F;
            if (mps >= item_size) {
                mps -= item_size;
                *flags_left &= ~mask;
                *flags_set |= mask;
            } else {
                mps = 0; /* Another notification is required for bit 0 corresponding field. */
                break;
            }
        }
        bit++;
        mask <<= 1;
        if (0 == (bit & 7)) {
            fmt = fmts[bit >> 3];
        } else {
            fmt >>= 4;
        }
    }
    return mps; /* Size available for bit 0 correspoding field. */
}

static void _ftms_send_treadmill_data_fragment(peer_t *client, ftms_treadmill_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_TRMD_FLAG_AVERAGE_SPEED_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT2(p_cur, flags);
    p_cur += 2;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        FTMS_COPY_INT2(p_cur, data->instantaneous_speed);
        p_cur += 2;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_TRMD_FLAG_AVERAGE_SPEED_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_speed);
                p_cur += 2;
                break;
            case FTMS_TRMD_FLAG_TOTAL_DISTANCE_PRESENT:
                FTMS_COPY_INT3(p_cur, data->total_distance);
                p_cur += 3;
                break;
            case FTMS_TRMD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT:
                FTMS_COPY_INT2(p_cur, data->inclination);
                FTMS_COPY_INT2(p_cur + 2, data->ramp_angle_setting);
                p_cur += 4;
                break;
            case FTMS_TRMD_FLAG_ELEVATION_GAIN_PRESENT:
                FTMS_COPY_INT2(p_cur, data->positive_elevation_gain);
                FTMS_COPY_INT2(p_cur + 2, data->negative_elevation_gain);
                p_cur += 4;
                break;
            case FTMS_TRMD_FLAG_INSTANTANEOUS_PACE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_pace);
                p_cur += 2;
                break;
            case FTMS_TRMD_FLAG_AVERAGE_PACE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_pace);
                p_cur += 2;
                break;
            case FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_TRMD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_TRMD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
            case FTMS_TRMD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_TRMD_FLAG_REMAINING_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            case FTMS_TRMD_FLAG_FORCE_ON_BELT_AND_POWER_OUTPUT_PRESENT:
                FTMS_COPY_INT2(p_cur, data->force_on_belt);
                FTMS_COPY_INT2(p_cur + 2, data->power_output);
                p_cur += 4;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->treadmill_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_treadmill_data(peer_t *client, ftms_treadmill_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_TRMD_FLAGS_ALL;
            uint32_t fmts[] = {0x52244320, 0x42211};

            /* At most two notifications in case of minimum BLE ATT_MTU (23) */
            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags*/,
                                                  &flags_left, &flags_set);
            if (flags_left || (bit0_size < 2)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_treadmill_data_fragment(client, data, flags_set, pkt);
                flags_set = flags_left;
            }
            _ftms_send_treadmill_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

static void _ftms_send_cross_trainer_data_fragment(peer_t *client, ftms_cross_trainer_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_CTD_FLAG_AVERAGE_SPEED_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT3(p_cur, flags);
    p_cur += 3;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        FTMS_COPY_INT2(p_cur, data->instantaneous_speed);
        p_cur += 2;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_CTD_FLAG_AVERAGE_SPEED_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_speed);
                p_cur += 2;
                break;
            case FTMS_CTD_FLAG_TOTAL_DISTANCE_PRESENT:
                FTMS_COPY_INT3(p_cur, data->total_distance);
                p_cur += 3;
                break;
            case FTMS_CTD_FLAG_STEP_COUNT_PRESENT:
                FTMS_COPY_INT2(p_cur, data->step_per_minute);
                FTMS_COPY_INT2(p_cur + 2, data->average_step_rate);
                p_cur += 4;
                break;
            case FTMS_CTD_FLAG_STRIDE_COUNT_PRESENT:
                FTMS_COPY_INT2(p_cur, data->stride_count);
                p_cur += 2;
                break;
            case FTMS_CTD_FLAG_ELEVATION_GAIN_PRESENT:
                FTMS_COPY_INT2(p_cur, data->positive_elevation_gain);
                FTMS_COPY_INT2(p_cur + 2, data->negative_elevation_gain);
                p_cur += 4;
                break;
            case FTMS_CTD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT:
                FTMS_COPY_INT2(p_cur, data->inclination);
                FTMS_COPY_INT2(p_cur + 2, data->ramp_angle_setting);
                p_cur += 4;
                break;
            case FTMS_CTD_FLAG_RESISTANCE_LEVEL_PRESENT:
                *p_cur++ = data->resistance_level;
                break;
            case FTMS_CTD_FLAG_INSTANTANEOUS_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_power);
                p_cur += 2;
                break;
            case FTMS_CTD_FLAG_AVERAGE_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_power);
                p_cur += 2;
                break;
            case FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_CTD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_CTD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
             case FTMS_CTD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_CTD_FLAG_REMAINING_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->cross_trainer_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_cross_trainer_data(peer_t *client, ftms_cross_trainer_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_CTD_FLAGS_ALL_PRESENT;
            uint32_t fmts[] = {0x14424320, 0x2211522};

            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 3 /*Flags*/,
                                                  &flags_left, &flags_set);
            /* At most three notifications in case of minimum BLE ATT_MTU (23) */
            if (flags_left || (bit0_size < 2)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_cross_trainer_data_fragment(client, data, flags_set, pkt);
                bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 3 /*Flags*/,
                                                      &flags_left, &flags_set);
                if (flags_left || (bit0_size < 2)) {
                    flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                    _ftms_send_cross_trainer_data_fragment(client, data, flags_set, pkt);
                    flags_set = flags_left;
                }
            }
            flags_set |= (data->flags & FTMS_CTD_FLAG_MOVE_BACKWARD);
            _ftms_send_cross_trainer_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

static void _ftms_send_step_climber_data_fragment(peer_t *client, ftms_step_climber_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT2(p_cur, flags);
    p_cur += 2;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        FTMS_COPY_INT2(p_cur, data->floors);
        FTMS_COPY_INT2(p_cur + 2, data->step_count);
        p_cur += 4;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->step_per_minute);
                p_cur += 2;
                break;
            case FTMS_STPCD_FLAG_AVERAGE_STEP_RATE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_step_rate);
                p_cur += 2;
                break;
            case FTMS_STPCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT:
                FTMS_COPY_INT2(p_cur, data->positive_elevation_gain);
                p_cur += 2;
                break;
            case FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_STPCD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_STPCD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
             case FTMS_STPCD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_STPCD_FLAG_REMAINING_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->step_climber_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_step_climber_data(peer_t *client, ftms_step_climber_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_STPCD_FLAGS_ALL;
            uint32_t fmts[] = {0x21152220, 0x02};

            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags*/,
                                                  &flags_left, &flags_set);
            /* At most two notifications in case of minimum BLE ATT_MTU (23) */
            if (flags_left || (bit0_size < 4)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_step_climber_data_fragment(client, data, flags_set, pkt);
                flags_set = flags_left;
            }
            _ftms_send_step_climber_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

static void _ftms_send_stair_climber_data_fragment(peer_t *client, ftms_stair_climber_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT2(p_cur, flags);
    p_cur += 2;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        FTMS_COPY_INT2(p_cur, data->floors);
        p_cur += 2;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->step_per_minute);
                p_cur += 2;
                break;
            case FTMS_STRCD_FLAG_AVERAGE_STEP_RATE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_step_rate);
                p_cur += 2;
                break;
            case FTMS_STRCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT:
                FTMS_COPY_INT2(p_cur, data->positive_elevation_gain);
                p_cur += 2;
                break;
            case FTMS_STRCD_FLAG_STRIDE_COUNT_PRESENT:
                FTMS_COPY_INT2(p_cur, data->stride_count);
                p_cur += 2;
                break;
            case FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_STRCD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_STRCD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
             case FTMS_STRCD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_STRCD_FLAG_REMAINING_TIME_PRESENT:
                /* This is the last item */
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->stair_climber_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_stair_climber_data(peer_t *client, ftms_stair_climber_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_STRCD_FLAGS_ALL;
            uint32_t fmts[] = {0x11522220, 0x22};

            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags*/,
                                                  &flags_left, &flags_set);
            /* At most two notifications in case of minimum BLE ATT_MTU (23) */
            if (flags_left || (bit0_size < 2)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_stair_climber_data_fragment(client, data, flags_set, pkt);
                flags_set = flags_left;
            }
            _ftms_send_stair_climber_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

static void _ftms_send_rower_data_fragment(peer_t *client, ftms_rower_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT2(p_cur, flags);
    p_cur += 2;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        *p_cur = data->stroke_rate;
        FTMS_COPY_INT2(p_cur + 1, data->stroke_count);
        p_cur += 3;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT:
                *p_cur++ = data->average_stroke_rate;
                break;
            case FTMS_RD_FLAG_TOTAL_DISTANCE_PRESENT:
                FTMS_COPY_INT3(p_cur, data->total_distance);
                p_cur += 3;
                break;
            case FTMS_RD_FLAG_INSTANTANEOUS_PACE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_pace);
                p_cur += 2;
                break;
            case FTMS_RD_FLAG_AVERAGE_PACE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_pace);
                p_cur += 2;
                break;
            case FTMS_RD_FLAG_INSTANTANEOUS_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_power);
                p_cur += 2;
                break;
            case FTMS_RD_FLAG_AVERAGE_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_power);
                p_cur += 2;
                break;
            case FTMS_RD_FLAG_RESISTANCE_LEVEL_PRESENT:
                *p_cur++ = data->resistance_level;
                break;
            case FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_RD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_RD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
            case FTMS_RD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_RD_FLAG_REMAINING_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->rower_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_rower_data(peer_t *client, ftms_rower_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_RD_FLAGS_ALL;
            uint32_t fmts[] = {0x12222310, 0x22115};

            /* At most two notifications in case of minimum BLE ATT_MTU (23) */
            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags*/,
                                                  &flags_left, &flags_set);
            if (flags_left || (bit0_size < 3)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_rower_data_fragment(client, data, flags_set, pkt);
                flags_set = flags_left;
            }
            _ftms_send_rower_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

static void _ftms_send_indoor_bike_data_fragment(peer_t *client, ftms_indoor_bike_data_t *data, uint32_t flags, uint8_t *pkt)
{
    uint16_t bit = FTMS_IBD_FLAG_AVERAGE_SPEED_PRESENT;
    uint8_t *p_cur = pkt;

    FTMS_COPY_INT2(p_cur, flags);
    p_cur += 2;
    if (flags & FTMS_DATA_FLAG_MORE_DATA) {
        flags &= ~FTMS_DATA_FLAG_MORE_DATA;
    } else {
        FTMS_COPY_INT2(p_cur, data->instantaneous_speed);
        p_cur += 2;
    }
    while (flags) {
        switch (bit & flags) {
            case FTMS_IBD_FLAG_AVERAGE_SPEED_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_speed);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_INSTANTANEOUS_CADENCE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_cadence);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_AVERAGE_CADENCE_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_cadence);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_TOTAL_DISTANCE_PRESENT:
                FTMS_COPY_INT3(p_cur, data->total_distance);
                p_cur += 3;
                break;
            case FTMS_IBD_FLAG_RESISTANCE_LEVEL_PRESENT:
                *p_cur++ = data->resistance_level;
                break;
            case FTMS_IBD_FLAG_INSTANTANEOUS_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->instantaneous_power);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_AVERAGE_POWER_PRESENT:
                FTMS_COPY_INT2(p_cur, data->average_power);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT:
                FTMS_COPY_INT2(p_cur, data->total_energy);
                FTMS_COPY_INT2(p_cur + 2, data->energy_per_hour);
                *(p_cur + 4) = data->energy_per_minute;
                p_cur += 5;
                break;
            case FTMS_IBD_FLAG_HEART_RATE_PRESENT:
                *p_cur++ = data->heart_rate;
                break;
            case FTMS_IBD_FLAG_METABOLIC_EQUIVALENT_PRESENT:
                *p_cur++ = data->metabolic_equivalent;
                break;
            case FTMS_IBD_FLAG_ELAPSED_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->elapsed_time);
                p_cur += 2;
                break;
            case FTMS_IBD_FLAG_REMAINING_TIME_PRESENT:
                FTMS_COPY_INT2(p_cur, data->remaining_time);
                p_cur += 2;
                break;
            default:
                break;
        }
        flags &= ~bit;
        bit <<= 1;
    }
    gatts_chr_val_notif(client, s_ftms->indoor_bike_data_chr, pkt, (uint16_t)(p_cur - pkt));
}

void ftms_send_indoor_bike_data(peer_t *client, ftms_indoor_bike_data_t *data)
{
    if ((NULL == client) || (NULL == data)) {
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt = brt_malloc(ftms_peer->mtu);

        if (pkt != NULL) {
            uint16_t bit0_size;
            uint32_t flags_set;
            uint32_t flags_left = data->flags & FTMS_IBD_FLAGS_ALL;
            uint32_t fmts[] = {0x22132220, 0x22115};

            /* At most two notifications in case of minimum BLE ATT_MTU (23) */
            bit0_size = _ftms_pre_set_data_packet(fmts, ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags*/,
                                                  &flags_left, &flags_set);
            if (flags_left || (bit0_size < 2)) {
                flags_set |= FTMS_DATA_FLAG_MORE_DATA;
                _ftms_send_indoor_bike_data_fragment(client, data, flags_set, pkt);
                flags_set = flags_left;
            }
            _ftms_send_indoor_bike_data_fragment(client, data, flags_set, pkt);
            brt_free(pkt);
        } else {
            co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
        }
    }
}

void ftms_send_training_status(peer_t *client, ftms_training_status_t *status)
{
    if ((NULL == client) || (NULL == status)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        uint8_t *pkt;
        uint16_t pkt_len;

        if (ftms_peer->ctl_flag & FTMS_CTL_FLAG_WAIT4_TRAINING_STATUS_RSP) {
            /* Read response with full value, lower will truncate the value
             * according to ATT_MTU and Read context. */
            pkt_len = 2; /*Flags & Status*/
            if ((status->flags & FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT) &&
                (status->status_string != NULL) && status->string_length) {
                /* Has Training Status string */
                pkt_len += status->string_length;
            }
            pkt = brt_malloc(pkt_len);
            if (pkt != NULL) {
                *pkt = status->flags & FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT;
                *(pkt + 1) = status->state;
                if (pkt_len > 2) {
                    memcpy(pkt + 2, status->status_string, status->string_length);
                    if (pkt_len > (ftms_peer->mtu - 3 /*Notification Header*/)) {
                        *pkt |= FTMS_TS_FLAG_EXTENDED_STRING_PRESENT;
                    }
                }
                gatts_chr_val_read_rsp(client, s_ftms->training_status_chr, GATT_OK, pkt, pkt_len);
                brt_free(pkt);
            } else {
                //co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
                uint8_t short_pkt[2];
                *short_pkt = 0;
                *(short_pkt + 1) = status->state;
                gatts_chr_val_read_rsp(client, s_ftms->training_status_chr, GATT_OK, short_pkt, 2);
            }
            ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_WAIT4_TRAINING_STATUS_RSP;

        } else {
            /* Notification with at most ATT_MTU-3 octects. */
            pkt = brt_malloc(ftms_peer->mtu - 3);
            if (pkt != NULL) {
                pkt_len = 0;
                *pkt = status->flags & FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT;
                *(pkt + 1) = status->state;
                if ((status->flags & FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT) &&
                    (status->status_string != NULL) && status->string_length) {
                    /* Has Training Status string */
                    pkt_len = ftms_peer->mtu - 3 /*Notification Header*/ - 2 /*Flags & Status*/;
                    if (status->string_length <= pkt_len) {
                        pkt_len = status->string_length;
                    } else {
                        *pkt |= FTMS_TS_FLAG_EXTENDED_STRING_PRESENT;
                    }
                    memcpy(pkt + 2, status->status_string, pkt_len);
                }
                gatts_chr_val_notif(client, s_ftms->training_status_chr, pkt, (uint16_t)(pkt_len + 2));
                brt_free(pkt);
            } else {
                co_printf("NO memory, %s %d\r\n", __FUNCTION__, __LINE__);
            }
        }
    }
}

void ftms_send_fitness_machine_status(peer_t *client, ftms_ftm_status_t *status)
{
    if ((NULL == client) || (NULL == status)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    if (((status->op_code > FTMS_FTMS_OPC_RFU0) && (status->op_code < FTMS_FTMS_OPC_RFU_START)) ||
        (FTMS_FTMS_OPC_CONTROL_PERMISSION_LOST == status->op_code)) {
        /* Supported OpCode */
        ftms_peer_t *ftms_peer = _ftms_find_peer(client);

        if (ftms_peer != NULL) {
            uint8_t pkt[12];
            uint16_t pkt_len = 1;

            *pkt = status->op_code;
            switch (status->op_code) {
                case FTMS_FTMS_OPC_RESET:
                case FTMS_FTMS_OPC_STOPPED_BY_SAFETY_KEY:
                case FTMS_FTMS_OPC_STARTED_OR_RESUMED_BY_USER:
                    /* No additional parameter */
                    break;
                case FTMS_FTMS_OPC_CONTROL_PERMISSION_LOST:
                    /* The control permission remains valid until the connection
                     * is terminated, the notification of the Fitness Machine Status
                     * is sent with the value set to Control Permission Lost, or the
                     * Reset procedure is initiated by the Client.
                     */
                    ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED;
                    break;
                case FTMS_FTMS_OPC_STOPPED_OR_PAUSED_BY_USER:
                case FTMS_FTMS_OPC_TARGET_RESISTANCE_LEVEL_CHANGED:
                case FTMS_FTMS_OPC_TARGET_HEART_RATE_CHANGED:
                case FTMS_FTMS_OPC_SPIN_DOWN_STATUS_CHANGED:
                    /* UINT8 or INT8 parameter */
                    *(pkt + 1) = status->parameter.stop_pause_op;
                    pkt_len++;
                    break;
                case FTMS_FTMS_OPC_TARGETED_DISTANCE_CHANGED:
                    /* UINT24 parameter */
                    FTMS_COPY_INT3(pkt + 1, status->parameter.distance);
                    pkt_len += 3;
                    break;
                case FTMS_FTMS_OPC_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES_CHANGED:
                    FTMS_COPY_INT2(pkt + 1, status->parameter.two_zone_times.burn_zone);
                    FTMS_COPY_INT2(pkt + 3, status->parameter.two_zone_times.fitness_zone);
                    pkt_len += 4;
                    break;
                case FTMS_FTMS_OPC_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES_CHANGED:
                    FTMS_COPY_INT2(pkt + 1, status->parameter.three_zone_times.light_zone);
                    FTMS_COPY_INT2(pkt + 3, status->parameter.three_zone_times.moderate_zone);
                    FTMS_COPY_INT2(pkt + 5, status->parameter.three_zone_times.hard_zone);
                    pkt_len += 6;
                    break;
                case FTMS_FTMS_OPC_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES_CHANGED:
                    FTMS_COPY_INT2(pkt + 1, status->parameter.five_zone_times.very_light_zone);
                    FTMS_COPY_INT2(pkt + 3, status->parameter.five_zone_times.light_zone);
                    FTMS_COPY_INT2(pkt + 5, status->parameter.five_zone_times.moderate_zone);
                    FTMS_COPY_INT2(pkt + 7, status->parameter.five_zone_times.hard_zone);
                    FTMS_COPY_INT2(pkt + 9, status->parameter.five_zone_times.maximum_zone);
                    pkt_len += 10;
                    break;
                case FTMS_FTMS_OPC_INDOOR_BIKE_SIMULATION_PARAMETERS_CHANGED:
                    FTMS_COPY_INT2(pkt + 1, status->parameter.simulation_parameter.wind_speed);
                    FTMS_COPY_INT2(pkt + 3, status->parameter.simulation_parameter.grade);
                    *(pkt + 5) = status->parameter.simulation_parameter.crr;
                    *(pkt + 6) = status->parameter.simulation_parameter.cw;
                    pkt_len += 6;
                    break;
                default:
                    /* UINT16 or INT16 parameter */
                    FTMS_COPY_INT2(pkt + 1, status->parameter.speed);
                    pkt_len += 2;
                    break;
            }
            gatts_chr_val_notif(client, s_ftms->ftm_status_chr, pkt, pkt_len);
        }
    } else {
        co_printf("Invalid status OpCode %d, %s %d\r\n", status->op_code, __FUNCTION__, __LINE__);
    }
}

void ftms_control_indication_enabled(peer_t *client)
{
    if (NULL == client) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if (ftms_peer != NULL) {
        ftms_peer->ctl_flag |= FTMS_CTL_FLAG_CONTROL_INDICATION_ENABLED;
    }
}

void ftms_control_response(peer_t *client, ftms_control_response_t *rsp_par)
{
    if ((NULL == client) || (NULL == rsp_par)) {
        co_printf("%s %d\r\n", __FUNCTION__, __LINE__);
        return;
    }

    ftms_peer_t *ftms_peer = _ftms_find_peer(client);

    if ((ftms_peer != NULL) && (ftms_peer->ctl_flag & FTMS_CTL_FLAG_WAIT4_CONTROL_RSP)) {
        uint8_t pkt[8];
        uint16_t pkt_len = 3;

        *pkt = FTMS_CTL_OPC_RESPONSE_CODE;
        *(pkt + 1) = rsp_par->op_code;
        *(pkt + 2) = rsp_par->result;
        if (FTMS_CTL_RESULT_SUCCESS == rsp_par->result) {
            switch (rsp_par->op_code) {
                case FTMS_CTL_OPC_REQUEST_CONTROL:
                    ftms_peer->ctl_flag |= FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED;
                    break;
                case FTMS_CTL_OPC_RESET:
                    /* The control permission remains valid until the connection
                     * is terminated, the notification of the Fitness Machine Status
                     * is sent with the value set to Control Permission Lost, or the
                     * Reset procedure is initiated by the Client.
                     */
                    ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_CONTROL_PROCEDURE_PERMITTED;
                    break;
                case FTMS_CTL_OPC_SPIN_DOWN_CONTROL:
                    /* Additional parameter */
                    FTMS_COPY_INT2(pkt + 3, rsp_par->parameter.spin_down_ok.target_speed_low);
                    FTMS_COPY_INT2(pkt + 5, rsp_par->parameter.spin_down_ok.target_speed_high);
                    pkt_len += 4;
                    break;
            }
        }
        gatts_chr_val_indicate(client, s_ftms->ctl_point_chr, pkt, pkt_len);

        /* Not waiting for response */
        ftms_peer->ctl_flag &= ~FTMS_CTL_FLAG_WAIT4_CONTROL_RSP;
        s_ftms->mgr_flags &= ~FTMS_MGR_FLAG_PROCEDURE_IN_PROGRESS;
    }
}

static void ftms_service_cbk(ftms_event_t event, void* ctx, void* par)
{
    switch (event) {
        case FTMS_EV_START: /* ftms_start_t* */
            co_printf("[CBK_EVT][FTMS started, result %d]\r\n", *(int8_t *)par);
            break;
        case FTMS_EV_STOP:
            co_printf("[CBK_EVT][FTMS stopped]\r\n");
            break;
        case FTMS_EV_ASK4_CONTROL_INDICATION_STATE: /* peer_t */
            co_printf("[CBK_EVT][Require Control Indication state]\r\n");
            s_ftms_peer_dev.inst = par;
            if (s_ftms_peer_dev.report_states & FTMS_CONTROL_POINT_REPORT_ON) {
                ftms_control_indication_enabled(par);
            }
            break;

        case FTMS_EV_TREADMILL_DATA_REPORT_STATE:     /* ftms_report_state_t* */
        case FTMS_EV_CROSS_TRAINER_DATA_REPORT_STATE:
        case FTMS_EV_STEP_CLIMBER_DATA_REPORT_STATE:
        case FTMS_EV_STAIR_CLIMBER_DATA_REPORT_STATE:
        case FTMS_EV_ROWER_DATA_REPORT_STATE:
        case FTMS_EV_INDOOR_BIKE_DATA_REPORT_STATE:
        case FTMS_EV_TRAINING_STATUS_REPORT_STATE:
        case FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_REPORT_STATE:
        case FTMS_EV_FITNESS_MACHINE_STATUS_REPORT_STATE:
            {
                ftms_report_state_t* report_state_par = (ftms_report_state_t *)par;
                int id = event - FTMS_EV_TREADMILL_DATA_REPORT_STATE;
                char* chr_name[] = {"Treadmill Data", "Cross Trainer Data", "Step Climber Data",
                                     "Stair Climber Data", "Rower Data", "Indoor Bike Data",
                                     "Training Status", "Control Response", "Machine Status"};
                if (report_state_par->state) {
                    co_printf("[CBK_EVT][%s notification is enabled]\r\n", chr_name[id]);
                    s_ftms_peer_dev.report_states |= (1 << id);
                }
                else {
                    co_printf("[CBK_EVT][%s notification is disabled]\r\n", chr_name[id]);
                    s_ftms_peer_dev.report_states &= ~(1 << id);
                }
                break;
            }

        case FTMS_EV_READ_TRAINING_STATUS_IND: /* peer_t */
            {
                ftms_training_status_t ts_rsp = {
                    .flags = FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT,
                    .state = FTMS_TRAINING_STATE_IDLE,
                    .string_length = sizeof(FTMS_LONG_STATUS_STRING),
                    .status_string = FTMS_LONG_STATUS_STRING,
                };
                ftms_send_training_status(par, &ts_rsp);
                break;
            }
        case FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_WRITE_IND: /* ftms_control_point_write_t* */
            {
                ftms_control_point_write_t* wrp = par;
                uint8_t* op_name[] = {"RequestControl", "Reset", "SetTargetSpeed", "SetTargetInclination", "SetTargetResistanceLevel",
                                      "SetTargetPower", "SetTargetHeartRate", "StartOrResume", "StopOrPause", "SetTargetedExpendedEnergy",
                                      "SetTargetedNumberOfSteps", "SetTargetedNumberOfStrides", "SetTargetedDisance", "SetTargetedTrainingTime",
                                      "SetTargetedTimeInTowZones", "SetTargetedTimeInThreeZones", "SetTargetedTimeInFiveZones",
                                      "SetIndoorBikeSimulationParameters", "SetWheelCircumference", "SpinDownControl", "SetTargetedCadence"};
                co_printf("[CBK_EVT][Procedure Request: %s]", op_name[wrp->request.op_code]);
                switch (wrp->request.op_code) {
                    case FTMS_CTL_OPC_SET_TARGET_SPEED:
                    case FTMS_CTL_OPC_SET_TARGET_INCLINATION:
                    case FTMS_CTL_OPC_SET_TARGET_POWER:
                    case FTMS_CTL_OPC_SET_TARGETED_EXPENDED_ENERGY:
                    case FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STEPS:
                    case FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STRIDES:
                    case FTMS_CTL_OPC_SET_TARGETED_TRAINING_TIME:
                    case FTMS_CTL_OPC_SET_WHEEL_CIRCUMFERENCE:
                    case FTMS_CTL_OPC_SET_TARGETED_CADENCE:
                        /* UINT16/INT16 */
                        co_printf("[%04x]\r\n", wrp->request.target.speed);
                        break;
                    case FTMS_CTL_OPC_SET_TARGET_RESISTANCE_LEVEL:
                    case FTMS_CTL_OPC_SET_TARGET_HEART_RATE:
                    case FTMS_CTL_OPC_STOP_OR_PAUSE:
                    case FTMS_CTL_OPC_SPIN_DOWN_CONTROL:
                        /* UINT8/INT8 */
                        co_printf("[%02x]\r\n", wrp->request.target.resistance_level);
                        break;
                    case FTMS_CTL_OPC_SET_TARGETED_DISTANCE:
                        /* UINT24 */
                        co_printf("[%06x]\r\n", wrp->request.target.distance);
                        break;
                    case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES:
                        co_printf("[BurnZone:%04x][FitnessZone:%04x]\r\n", wrp->request.target.two_zone_times.burn_zone,
                                wrp->request.target.two_zone_times.fitness_zone);
                        break;
                    case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES:
                        co_printf("[LightZone:%04x][ModerateZone:%04x][HardZone:%04x]\r\n", wrp->request.target.three_zone_times.light_zone,
                            wrp->request.target.three_zone_times.moderate_zone, wrp->request.target.three_zone_times.hard_zone);
                        break;
                    case FTMS_CTL_OPC_SET_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES:
                        co_printf("[VeryLigthZone:%04x][LightZone:%04x][ModerateZone:%04x][HardZone:%04x][MaximumZone:%04x]\r\n",
                            wrp->request.target.five_zone_times.very_light_zone, wrp->request.target.five_zone_times.light_zone,
                            wrp->request.target.five_zone_times.moderate_zone, wrp->request.target.five_zone_times.hard_zone,
                            wrp->request.target.five_zone_times.maximum_zone);
                        break;
                    case FTMS_CTL_OPC_SET_INDOOR_BIKE_SIMULATION_PARAMETERS:
                        co_printf("[WindSpeed:%04x][Grade:%02x][CRR:%02x][CW:%02x]\r\n",
                            wrp->request.target.simulation_parameter.wind_speed, wrp->request.target.simulation_parameter.grade,
                            wrp->request.target.simulation_parameter.crr, wrp->request.target.simulation_parameter.cw);
                        break;
                    default:
                        /* No additional parameter */
                        co_printf("\r\n");
                        break;
                    }

                ftms_control_response_t wrsp = {
                    .op_code = wrp->request.op_code,
                    .result = FTMS_CTL_RESULT_SUCCESS,
                    .parameter.spin_down_ok.target_speed_low = 0x1122,
                    .parameter.spin_down_ok.target_speed_high = 0x3344,
                };
                ftms_control_response(wrp->peer, &wrsp);
                break;
            }
    }
}

void ble_ftms_init(void)
{
    ftms_inf_t inf = {
        .fitness_machine_features = 0xFFFF,
        .target_setting_features = 0x1FFFF,
        .supported_characteristics = 0x1FF,
        .speed_range.min_value = 500,
        .speed_range.min_increment = 1,
        .speed_range.max_value = 2000,
        .inclination_range.min_value = 0,
        .inclination_range.min_increment = 1,
        .inclination_range.max_value = 30,
        .resistance_level_range.min_value = 0,
        .resistance_level_range.min_increment = 1,
        .resistance_level_range.max_value = 30,
        .power_range.min_value = 1,
        .power_range.min_increment = 1,
        .power_range.max_value = 50,
        .heart_rate_range.min_value = 60,
        .heart_rate_range.min_increment = 1,
        .heart_rate_range.max_value = 100,
    };
    ftms_start(&inf, ftms_service_cbk, NULL);
}

void ftms_done(void)
{
    ftms_stop();
}

