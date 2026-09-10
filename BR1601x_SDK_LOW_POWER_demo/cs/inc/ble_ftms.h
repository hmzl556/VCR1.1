/*****************************************************************************
 * Copyright (c) 2023, Barrot Technology Co.,Ltd..
 *
 * All rights reserved.
 * ****************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the disclaimer below.
 *
 * Barrot's name may not be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY BARROT'S "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * DISCLAIMED. IN NO EVENT SHALL BARROT'S BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ****************************************************************************/

/**
 * @file ble_ftms.h
 * @author Barrot Firmware Team
 * @version v1.0.2
 *
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 */

#ifndef _BLE_FTMS_H_
#define _BLE_FTMS_H_
/**
 *******************************************************************************
 * @defgroup FTMS   Barrot SDK FTMS Definitions
 *
 * @brief
 *
 * @{
 *******************************************************************************
 */

/*
 * INCLUDE FILES
 *******************************************************************************
 */
 #include <stdint.h>
#include "cs_core.h"
#include "gap.h"
#include "gatt.h"
#include "co_printf.h"
/*
 * TYPE DEFINITIONS
 *******************************************************************************
 */

/**
 * A ServiceDataAD field for Fitness Machine Service is suggested to be included
 * in the Advertising Data or Extended Inquiry Response:
 *   Length                         (UINT8)   0x06
 *   Service Data AD Type           (UINT8)   0x16
 *   Fitness Machine Service UUID   (UINT16)  0x1826
 *   Flags                          (UINT8)   Bit mask, each bit: 0-False; 1-True.
 *                                            Bit 0 - Fitness Machine Available
 *   Fitness Machine Type           (UINT16)  Bit mask, each bit: 0-False; 1-True.
 *                                            Bit 0 - Treadmill Supported
 *                                            Bit 1 - Cross Trainer Supported
 *                                            Bit 2 - Step Climber Supported
 *                                            Bit 3 - Stair Climber Supported
 *                                            Bit 4 - Rower Supported
 *                                            Bit 5 - Indoor Bike Supported
 *
 * Sample with Step Climber Supported: 0x06, 0x16, 0x26, 0x18, 0x01, 0x04, 0x00
 */

/**
 * When a field represents a scalar value, the represented value (R) is related
 * to the raw value (C) by the following equation:
 *     R = C * M * 10^d * 2^b
 * Where:
 *     M = multiplier, positive or negative integer (between -10 and +10)
 *     d = decimal exponent, positive or negative integer
 *     b = binary exponent, positive or negative integer
 * For example, to represent a length in meters with a resolution of 0.1 meter,
 * the following combination of values and units may be used:
 *     Unit: org.bluetooth.unit.length.metre
 *     M = 1, d = -1, b = 0
 */
#define FTMS_SCALAR_VALUE(_C, _M, _d, _b)   (_C * _M * (10^_d) * (2 ^ _b))

/**
 * Fitness Machine Features bit
 */
#define FTMS_FTM_FEATURE_AVERAGE_SPEED_SUPPORTED                   (1<<0)  /** If set, supports the Average Speed feature. */
#define FTMS_FTM_FEATURE_CADENCE_SUPPORTED                         (1<<1)  /** If set, supports the Cadence feature. */
#define FTMS_FTM_FEATURE_TOTAL_DISTANCE_SUPPORTED                  (1<<2)  /** If set, supports the Total Distance feature. */
#define FTMS_FTM_FEATURE_INCLINATION_SUPPORTED                     (1<<3)  /** If set, supports the Inclination feature. */
#define FTMS_FTM_FEATURE_ELEVATION_GAIN_SUPPORTED                  (1<<4)  /** If set, supports the Elevation Gain feature. */
#define FTMS_FTM_FEATURE_PACE_SUPPORTED                            (1<<5)  /** If set, supports the Pace feature. */
#define FTMS_FTM_FEATURE_STEP_COUNT_SUPPORTED                      (1<<6)  /** If set, supports the Step Count feature. */
#define FTMS_FTM_FEATURE_RESISTANCE_LEVEL_SUPPORTED                (1<<7)  /** If set, supports the Resistance Level feature. */
#define FTMS_FTM_FEATURE_STRIDE_COUNT_SUPPORTED                    (1<<8)  /** If set, supports the Stride Count feature. */
#define FTMS_FTM_FEATURE_EXPENDED_ENERGY_SUPPORTED                 (1<<9)  /** If set, supports the Expended Energy feature. */
#define FTMS_FTM_FEATURE_HEART_RATE_MEASUREMENT_SUPPORTED          (1<<10) /** If set, supports the Heart Rate Measurement feature. */
#define FTMS_FTM_FEATURE_METABOLIC_EQUIVALENT_SUPPORTED            (1<<11) /** If set, supports the Metabolic Equivalent feature. */
#define FTMS_FTM_FEATURE_ELAPSED_TIME_SUPPORTED                    (1<<12) /** If set, supports the Elapsed Time feature. */
#define FTMS_FTM_FEATURE_REMAINING_TIME_SUPPORTED                  (1<<13) /** If set, supports the Remaining Time feature. */
#define FTMS_FTM_FEATURE_POWER_MEASUREMENT_SUPPORTED               (1<<14) /** If set, supports the Power Measurement feature. */
#define FTMS_FTM_FEATURE_FORCE_ON_BELT_AND_POWER_OUTPUT_SUPPORTED  (1<<15) /** If set, supports the Force On Belt And Power Output feature. */
#define FTMS_FTM_FEATURE_USER_DATA_RETENTION_SUPPORTED             (1<<16) /** If set, supports the User Data Retention feature. */

/**
 * Target Setting Features bit
 */
#define FTMS_TGSET_FEATURE_SPEED_TARGET_SETTING_SUPPORTED                                      (1<<0)  /** If set, supports Speed Target setting. */
#define FTMS_TGSET_FEATURE_INCLINATION_TARGET_SETTING_SUPPORTED                                (1<<1)  /** If set, supports Inclination Target setting. */
#define FTMS_TGSET_FEATURE_RESISTANCE_TARGET_SETTING_SUPPORTED                                 (1<<2)  /** If set, supports Resistance Target setting. */
#define FTMS_TGSET_FEATURE_POWER_TARGET_SETTING_SUPPORTED                                      (1<<3)  /** If set, supports Power Target setting. */
#define FTMS_TGSET_FEATURE_HEART_RATE_TARGET_SETTING_SUPPORTED                                 (1<<4)  /** If set, supports Heart Rate Target setting. */
#define FTMS_TGSET_FEATURE_TARGETED_EXPENDED_ENERGY_CONFIGURATION_SUPPORTED                    (1<<5)  /** If set, supports Targeted Expended Energy configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_STEP_NUMBER_CONFIGURATION_SUPPORTED                        (1<<6)  /** If set, supports Targeted Step Number configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_STRIDE_NUMBER_CONFIGURATION_SUPPORTED                      (1<<7)  /** If set, supports Targeted Stride Number configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_DISTANCE_CONFIGURATION_SUPPORTED                           (1<<8)  /** If set, supports Targeted Distance configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_TRAINING_TIME_CONFIGURATION_SUPPORTED                      (1<<9)  /** If set, supports Targeted Training Time configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_TIME_IN_2_HEART_RATE_ZONES_CONFIGURATION_SUPPORTED         (1<<10) /** If set, supports Targeted Time in Two Heart Rate Zones configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_TIME_IN_3_HEART_RATE_ZONES_CONFIGURATION_SUPPORTED         (1<<11) /** If set, supports Targeted Time in Three Heart Rate Zones configuration. */
#define FTMS_TGSET_FEATURE_TARGETED_TIME_IN_5_HEART_RATE_ZONES_CONFIGURATION_SUPPORTED         (1<<12) /** If set, supports Targeted Time in Five Heart Rate Zones configuration. */
#define FTMS_TGSET_FEATURE_INDOOR_BIKE_SIMULATION_PARAMETERS_SUPPORTED                         (1<<13) /** If set, supports Indoor Bike Simulation Parameters. */
#define FTMS_TGSET_FEATURE_WHEEL_CIRCUMFERENCE_CONFIGURATION_SUPPORTED                         (1<<14) /** If set, supports Wheel Circumference configuration. */
#define FTMS_TGSET_FEATURE_SPIN_DOWN_CONTROL_SUPPORTED                                         (1<<15) /** If set, supports Spin Down Control. */
#define FTMS_TGSET_FEATURE_TARGETED_CADENCE_CONFIGURATION_SUPPORTED                            (1<<16) /** If set, supports Targeted Cadence configuration. */

/**
 * Optional Characteristic supported bit
 */
#define FTMS_CHR_TREADMILL_DATA_SUPPORTED           (1<<0) /** If set, Treadmill Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_CROSS_TRAINER_DATA_SUPPORTED       (1<<1) /** If set, Cross Trainer Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_STEP_CLIMBER_DATA_SUPPORTED        (1<<2) /** If set, Step Climber Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_STAIR_CLIMBER_DATA_SUPPORTED       (1<<3) /** If set, Stair Climber Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_ROWER_DATA_SUPPORTED               (1<<4) /** If set, Rower Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_INDOOR_BIKE_DATA_SUPPORTED         (1<<5) /** If set, Indoor Bike Data characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_TRAINING_STATUS_SUPPORTED          (1<<6) /** If set, Training Status characteristic shall be present in the local fitness machine service. */
#define FTMS_CHR_FTM_MACHINE_STATUS_SUPPORTED       (1<<7) /** If set, Fitness Machine Status characteristics shall be present in the local fitness machine service. */
#define FTMS_CHR_FTM_CONTROL_POINT_SUPPORTED        (3<<7) /** If set, Fitness Machine Control Point and Fitness Machine Status characteristics shall be present in the local fitness machine service. */


#define FTMS_TREADMILL_DATA_REPORT_ON        (1<<0)
#define FTMS_CROSS_TRAINER_DATA_REPORT_ON    (1<<1)
#define FTMS_STEP_CLIMBER_DATA_REPORT_ON     (1<<2)
#define FTMS_STAIR_CLIMBER_DATA_REPORT_ON    (1<<3)
#define FTMS_ROWER_DATA_REPORT_ON            (1<<4)
#define FTMS_INDOOR_BIKE_DATA_REPORT_ON      (1<<5)
#define TFTMS_TRAINING_STATUS_REPORT_ON       (1<<6)
#define FTMS_CONTROL_POINT_REPORT_ON         (1<<7)
#define FTMS_MACHINE_STATUS_REPORT_ON        (1<<8)

#define FTMS_CTL_FLAG_CONTROL_PERMITTED      (1<<0)

#define FTMS_LONG_STATUS_STRING              "0123456789012345678901234567890123456789"
#define FTMS_SHORT_STATUS_STRING             "0123456789012345678"

/**
 * FTMS events
 */
typedef enum {

    FTMS_EV_START, /** Fitness machine service started. ftms_start_t */
    FTMS_EV_STOP, /** Fitness machine service stopped. void */
    FTMS_EV_ASK4_CONTROL_INDICATION_STATE, /** LE link connected and get the previous fitness machine control point indication
                                             state from the application. Shall be replied with #ftms_control_indication_enabled
                                             if the connected device is a bonded device and the indicaiton has been enabled in
                                             a previous connection. peer_t. */

    FTMS_EV_TREADMILL_DATA_REPORT_STATE, /** Treadmill Data notification state changed. ftms_report_state_t */
    FTMS_EV_CROSS_TRAINER_DATA_REPORT_STATE, /** Cross Trainer Data notification state changed. ftms_report_state_t */
    FTMS_EV_STEP_CLIMBER_DATA_REPORT_STATE, /** Step Climber Data notification state changed. ftms_report_state_t */
    FTMS_EV_STAIR_CLIMBER_DATA_REPORT_STATE, /** Stair Climber Data notification state changed. ftms_report_state_t */
    FTMS_EV_ROWER_DATA_REPORT_STATE, /** Rower Data notification state changed. ftms_report_state_t */
    FTMS_EV_INDOOR_BIKE_DATA_REPORT_STATE, /** Indoor Bike Data notification state changed. ftms_report_state_t */
    FTMS_EV_TRAINING_STATUS_REPORT_STATE, /** Training Status notification state changed. ftms_report_state_t */
    FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_REPORT_STATE, /** Fitness Machine Control Point Indication state changed. ftms_report_state_t */
    FTMS_EV_FITNESS_MACHINE_STATUS_REPORT_STATE, /** Fitness Machine Status notification state changed. ftms_report_state_t */

    FTMS_EV_READ_TRAINING_STATUS_IND, /** Receive read request of current training status. Shall be replied with #ftms_send_training_status.
                                        peer_t. */
    FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_WRITE_IND, /** Receive Write request to the Fitness Machine Control Point characteristic.
                                                       Shall be replied with #ftms_control_response. ftms_control_point_write_t. */

} ftms_event_t;


/**
 * FTMS event callbavk function.
 *
 * @param[in] event  Event reported.
 * @param[in]  ctx   The costumized Context data for this task.
 * @param[in]  par   Event specific parameter.
 */
typedef void (ftms_cbk_t)(ftms_event_t event, void *ctx, void *par);


/**
 * The parameter of FTMS_EV_REPORT
 */
typedef struct {
    int8_t  result; /** Result
                        0     : Successful
                        others: Failed */
} ftms_start_t;

/**
 * The parameter of FTMS_EV_TREADMILL_DATA_REPORT_STATE etc.
 */
typedef struct ftms_report_state{
    peer_t   peer;  /** Peer device instance. */
    uint8_t  state; /** Notification/indication state.
                        0     : Notification/indication is disabled.
                        others: Notification/indication is enabled. */
} ftms_report_state_t;

/**
 * Fitness machine control procedure OP Code.
 */
typedef enum {
    FTMS_CTL_OPC_REQUEST_CONTROL,                              /** Initiates the procedure to request the control of a fitness machine. */
    FTMS_CTL_OPC_RESET,                                        /** Initiates the procedure to reset the controllable settings of a fitness machine. */
    FTMS_CTL_OPC_SET_TARGET_SPEED,                             /** Initiate the procedure to set the target speed of the Server. */
    FTMS_CTL_OPC_SET_TARGET_INCLINATION,                       /** Initiate the procedure to set the target inclination of the Server. */
    FTMS_CTL_OPC_SET_TARGET_RESISTANCE_LEVEL,                  /** Initiate the procedure to set the target resistance level of the Server. */
    FTMS_CTL_OPC_SET_TARGET_POWER,                             /** Initiate the procedure to set the target power of the Server. */
    FTMS_CTL_OPC_SET_TARGET_HEART_RATE,                        /** Initiate the procedure to set the target heart rate of the Server. */
    FTMS_CTL_OPC_START_OR_RESUME,                              /** Initiate the procedure to start or resume a training session on the Server. */
    FTMS_CTL_OPC_STOP_OR_PAUSE,                                /** Initiate the procedure to stop or pause a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_EXPENDED_ENERGY,                 /** Initiate the procedure to set the targeted expended energy for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STEPS,                 /** Initiate the procedure to set the targeted number of steps for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STRIDES,               /** Initiate the procedure to set the targeted number of strides for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_DISTANCE,                        /** Initiate the procedure to set the targeted distance for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_TRAINING_TIME,                   /** Initiate the procedure to set the targeted training time for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES,    /** Initiate the procedure to set the targeted time in two heart rate zones for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES,  /** Initiate the procedure to set the targeted time in three heart rate zones for a training session on the Server. */
    FTMS_CTL_OPC_SET_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES,   /** Initiate the procedure to set the targeted time in five heart rate zones for a training session on the Server. */
    FTMS_CTL_OPC_SET_INDOOR_BIKE_SIMULATION_PARAMETERS,        /** Initiate the procedure to set the simulation parameters for a training session on the Server. */
    FTMS_CTL_OPC_SET_WHEEL_CIRCUMFERENCE,                      /** Initiate the procedure to set the wheel circumference for a training session on the Server.*/
    FTMS_CTL_OPC_SPIN_DOWN_CONTROL,                            /** Initiate the spin down control procedure of a Server. */
    FTMS_CTL_OPC_SET_TARGETED_CADENCE,                         /** Initiate the procedure to set the targeted cadence for a training session on the Server. */

    FTMS_CTL_OPC_RESPONSE_CODE = 0x80,                         /** Response code. */
} ftms_control_op_code_t;

/**
 * Control information of the Stop or Pause procedure.
 */
typedef enum {
    FTMS_PROCEDURE_STOP = 1,
    FTMS_PROCEDURE_PAUSE,
} ftms_stop_pause_control_t;

/**
 * Spin Down Control parameter.
 */
typedef enum {
    FTMS_SPIN_DOWN_CTL_START = 1,
    FTMS_SPIN_DOWN_CTL_IGNOE,
} ftms_spin_down_control_t;

/**
 * Spin Down Status parameter
 */
typedef enum {
    FTMS_SPIN_DOWN_ST_SPIN_DOWN_REQUESTED = 1,
    FTMS_SPIN_DOWN_ST_SUCCESS,
    FTMS_SPIN_DOWN_ST_ERROR,
    FTMS_SPIN_DOWN_ST_STOP_PEDALING,
} ftms_spin_down_status_t;

/**
 * Training Time in Two Heart Rate Zones.
 */
typedef struct {
    uint16_t burn_zone;    /** Time for burn zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t fitness_zone; /** Time for fitness zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
} ftms_two_zones_t;

/**
 * Training Time in Three Heart Rate Zones.
 */
typedef struct {
    uint16_t light_zone;    /** Time for ligth zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t moderate_zone; /** Time for moderate zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t hard_zone;     /** Time for hard zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
} ftms_three_zones_t;

/**
 * Training Time in Five Heart Rate Zones.
 */
typedef struct {
    uint16_t very_light_zone; /** Time for very light zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t light_zone;      /** Time for light zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t moderate_zone;   /** Time for moderate zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t hard_zone;       /** Time for hard zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
    uint16_t maximum_zone;    /** Time for maximum zone, in seconds with a resolution of 1 second (M=1, d=0, b=0). */
} ftms_five_zones_t;

/**
 * Indoor Bike Simulation Parameter Array.
 */
typedef struct {
    int16_t wind_speed; /** Wind speed, in meters per second with a resolution of 0.001 mps (M=1, d=-3, b=0) */
    int16_t grade;      /** Grade, in percent with a resolution of 0.01% (M=1, d=-2, b=0) */
    uint8_t crr;        /** Coefficient of rolling resistance, unitless with a resolution of 0.0001 (M=1, d=-4, b=0) */
    uint8_t cw;         /** Wind resistance coefficient, in kilogram per meter with a resolution of 0.01 Kg/m (M=1, d=-2, b=0) */
} ftms_simulation_parameter_t;

/**
 * The parameter of fitness control point write request.
 */
typedef struct {
    uint8_t  op_code;   /** ftms_control_op_code_t. request OpCode */
    union {
        uint16_t speed;                      /** Target speed, in km/h with a resolution of 0.01 km/h (M=1, d=-2, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGET_SPEED. */
        int16_t  inclination;                /** Target inclination, in percent with a resolution of 0.1% (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGET_INCLINATION. */
        uint8_t  resistance_level;           /** Target resistance level to set, unitless with a resolution of 0.1 (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGET_RESISTANCE_LEVEL. */
        int16_t  power;                      /** Target power, in watt with a resolution of 1W (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGET_POWER. */
        uint8_t  heart_rate;                 /** Target heart rate, in BPM with a resolution of 1 BPM (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGET_HEART_RATE. */
        uint8_t  stop_pause_op;              /** ftms_stop_pause_control_t. Parameter of OP code FTMS_CTL_OPC_STOP_OR_PAUSE. */
        uint16_t expended_energy;            /** Targeted expended energy, in calories with a resolution of 1 calorie (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_EXPENDED_ENERGY. */
        uint16_t number_steps;               /** Targeted number of steps, in steps with a resolution of 1 step (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STEPS. */
        uint16_t number_strides;             /** Targeted number of strides, in strides with a resolution of 1 stride (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_NUMBER_OF_STRIDES. */
        uint32_t distance;                   /** Targeted distance, UINT24, in meters with a resolution of 1 meter (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_DISTANCE. */
        uint16_t training_time;              /** Targeted training time, in seconds with a resolution of 1 second (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_TRAINING_TIME. */
        ftms_two_zones_t two_zone_times;     /** Targeted training time in two heart rate zones.
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES. */
        ftms_three_zones_t three_zone_times; /** Targeted training time in three heart rate zones.
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES. */
        ftms_five_zones_t five_zone_times;   /** Targeted training time in five heart rate zones.
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES. */
        ftms_simulation_parameter_t simulation_parameter; /** Indoor bike simulation parameters.
                                                            Parameter of OP code FTMS_CTL_OPC_SET_INDOOR_BIKE_SIMULATION_PARAMETERS. */
        uint16_t wheel_circumference;        /** Target Wheel circumference, in millimeters with resolution of 0.1 mm (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_CTL_OPC_SET_WHEEL_CIRCUMFERENCE. */
        uint8_t  spin_down_op;               /** ftms_spin_down_control_t. Parameter of OP code FTMS_CTL_OPC_SPIN_DOWN_CONTROL. */
        uint16_t cadence;                    /** Targeted cadence, in 1/minute with a resolution of 0.5 1/minute (M=1, d=0, b=-1).
                                               Parameter of OP code FTMS_CTL_OPC_SET_TARGETED_CADENCE. */
    } target;           /** OpCode specific parameter. */
} ftms_control_request_t;

/**
 * The parameter of FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_WRITE_IND.
 */
typedef struct {
    peer_t                 peer;    /** Peer device instance. */
    ftms_control_request_t request; /** Fitness Control Point Write request parameter. */
} ftms_control_point_write_t;

/**
 * Result Code for the Fitness Machine Control procedure
 */
typedef enum {
    FTMS_CTL_RESULT_RFU0,                  /** Reserved for fture use */

    FTMS_CTL_RESULT_SUCCESS,               /** The requested control procedure completes successfully. */
    FTMS_CTL_RESULT_OP_CODE_NOT_SUPPORTED, /** The OpCode requested is unsupported by the server. */
    FTMS_CTL_RESULT_INVALID_PARAMETER,     /** The specified parameter value is invalid, e.g., improperly
                                             formated or outside the range of the supported values. */
    FTMS_CTL_RESULT_OPERATION_FAILED,      /** The requested control procedure results in an error condition,
                                             e.g., Start or Resume OpCode is received when the fitness machine
                                             has already been started. */
    FTMS_CTL_RESULT_CONTROL_NOT_PERMITTED, /** The requested control procedure is not permitted to that
                                             particular client. */

    FTMS_CTL_RESULT_RFU_START,
} ftms_control_result_t;

/**
 * The response parameter of a successful Spin Down operation.
 */
typedef struct {
    uint16_t target_speed_low;  /** Target speed low, in km/h with a resolution of 0.01 km/h (M=1, d=-2, b=0). */
    uint16_t target_speed_high; /** Target speed high, in km/h with a resolution of 0.01 km/h (M=1, d=-2, b=0). */
} ftms_spin_down_ok_parameter_t;

/**
 * The response parameter of the Fitness machine Control Procedure.
 */
typedef struct {
    uint8_t  op_code; /** ftms_control_op_code_t. OpCode of the requested procedure that generats the response. */
    uint8_t  result;  /** ftms_control_result_t. Result of the control procedure. */
    union {
        ftms_spin_down_ok_parameter_t spin_down_ok; /** Response parameter for a successful Spin Down operation.
                                                      Shall be set if op_code is FTMS_CTL_OPC_SPIN_DOWN_CONTROL
                                                      and result is FTMS_CTL_RESULT_SUCCESS. */
    } parameter;      /** Response specific parameter. */
} ftms_control_response_t;


/**
 * The range parameter. Each member of the structure represent a scalar value
 * (#FTMS_SCALAR_VALUE) of the same type.
 */

/**
 * Range paramete of 16bit unsigned integer.
 */
typedef struct {
    uint16_t min_value;     /** Minimum value of the Range. */
    uint16_t max_value;     /** Maximum value of the Range. */
    uint16_t min_increment; /** Minimum Increment value of the Range. */
} ftms_range_u16_t;

/**
 * Range paramete of 16bit signed integer.
 */
typedef struct {
    int16_t min_value;      /** Minimum value of the Range. */
    int16_t max_value;      /** Maximum value of the Range. */
    uint16_t min_increment; /** Minimum Increment value of the Range. */
} ftms_range_s16_t;

/**
 * Range paramete of 8bit unsigned integer.
 */
typedef struct {
    uint8_t min_value;      /** Minimum value of the Range. */
    uint8_t max_value;      /** Maximum value of the Range. */
    uint8_t min_increment;  /** Minimum Increment value of the Range. */
} ftms_range_u8_t;

/**
 * FTMS information
 */
typedef struct {
    uint32_t fitness_machine_features;  /** Bit mask, FTMS_FTM_FEATURE_AVERAGE_SPEED_SUPPORTED
                                         etc. The supported fitness machine features. */
    uint32_t target_setting_features;   /** Bit mask, FTMS_TGSET_FEATURE_SPEED_TARGET_SETTING_SUPPORTED
                                         etc. The supported target setting features. */
    uint32_t supported_characteristics; /** Bit mask, FTMS_CHR_TREADMILL_DATA_SUPPORTED etc.
                                         The supported optional characteristics. */
    ftms_range_u16_t speed_range;       /** The Supported Speed Range.
                                         Shall be set if FTMS_TGSET_FEATURE_SPEED_TARGET_SETTING_SUPPORTED
                                         is set in tgs_feat_mask. Ignored otherwise.
                                         Represent values: M = 1, d = -2, b = 0
                                         Unit is 1/100 of a kilometer per hour. */
    ftms_range_s16_t inclination_range; /** The Supported Inclination Range.
                                         Shall be set if FTMS_TGSET_FEATURE_INCLINATION_TARGET_SETTING_SUPPORTED
                                         is set in tgs_feat_mask. Ignored otherwise.
                                         Represent scalar values: M = 1, d = -1, b = 0
                                         Unit is 1/10 of a percent. */
    ftms_range_u8_t resistance_level_range; /** The Supported Resistance Level Range.
                                             Shall be set if FTMS_TGSET_FEATURE_RESISTANCE_TARGET_SETTING_SUPPORTED
                                             is set in tgs_feat_mask. Ignored otherwise.
                                             Represent scalar values: M = 1, d = 1, b = 0
                                             Unit is 1. */
    ftms_range_s16_t power_range;       /** The Supported Power Range.
                                         Shall be set if FTMS_TGSET_FEATURE_POWER_TARGET_SETTING_SUPPORTED
                                         is set in tgs_feat_mask. Ignored otherwise.
                                         Unit is watt. */
    ftms_range_u8_t heart_rate_range;   /** The Supported Heart Rate Range.
                                         Shall be set if FTMS_TGSET_FEATURE_HEART_RATE_TARGET_SETTING_SUPPORTED
                                         is set in tgs_feat_mask. Ignored otherwise.
                                         Unit is beats per minute. */
} ftms_inf_t;

/**
 * Possible Unavailable Data values
 */
#define FTMS_UNAVAILABLE_U8_DATA        0xFF    /** A special UINT8 value which means "Data Not Available" */
#define FTMS_UNAVAILABLE_U16_DATA       0xFFFF  /** A special UINT16 value which means "Data Not Available" */
#define FTMS_UNAVAILABLE_S16_DATA       0x7FFF  /** A special SINT16 value which means "Data Not Available" */

/**
 * Treadmill Data (TRMD) flags bit
 */
#define FTMS_TRMD_FLAG_AVERAGE_SPEED_PRESENT                               (1<<1)
#define FTMS_TRMD_FLAG_TOTAL_DISTANCE_PRESENT                              (1<<2)
#define FTMS_TRMD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT          (1<<3)
#define FTMS_TRMD_FLAG_ELEVATION_GAIN_PRESENT                              (1<<4)
#define FTMS_TRMD_FLAG_INSTANTANEOUS_PACE_PRESENT                          (1<<5)
#define FTMS_TRMD_FLAG_AVERAGE_PACE_PRESENT                                (1<<6)
#define FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT                             (1<<7)
#define FTMS_TRMD_FLAG_HEART_RATE_PRESENT                                  (1<<8)
#define FTMS_TRMD_FLAG_METABOLIC_EQUIVALENT_PRESENT                        (1<<9)
#define FTMS_TRMD_FLAG_ELAPSED_TIME_PRESENT                                (1<<10)
#define FTMS_TRMD_FLAG_REMAINING_TIME_PRESENT                              (1<<11)
#define FTMS_TRMD_FLAG_FORCE_ON_BELT_AND_POWER_OUTPUT_PRESENT              (1<<12)

#define FTMS_TRMD_FLAGS_ALL    (FTMS_TRMD_FLAG_AVERAGE_SPEED_PRESENT | FTMS_TRMD_FLAG_TOTAL_DISTANCE_PRESENT | \
                                FTMS_TRMD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT | \
                                FTMS_TRMD_FLAG_ELEVATION_GAIN_PRESENT | FTMS_TRMD_FLAG_INSTANTANEOUS_PACE_PRESENT | \
                                FTMS_TRMD_FLAG_AVERAGE_PACE_PRESENT | FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT | \
                                FTMS_TRMD_FLAG_HEART_RATE_PRESENT | FTMS_TRMD_FLAG_METABOLIC_EQUIVALENT_PRESENT | \
                                FTMS_TRMD_FLAG_ELAPSED_TIME_PRESENT | FTMS_TRMD_FLAG_REMAINING_TIME_PRESENT | \
                                FTMS_TRMD_FLAG_FORCE_ON_BELT_AND_POWER_OUTPUT_PRESENT)

/** Treadmill Data Report */
typedef struct {
    uint16_t flags;                     /** Bit mask, FTMS_TRMD_FLAG_AVERAGE_SPEED_FIELD etc.
                                          Specifies the availability of the correspoding data filed. */
    uint16_t instantaneous_speed;       /** Instantaneous speed of the belt of the treadmill. Shall always be set.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint16_t average_speed;             /** Average speed since the beginning of the training session. Shall be set if
                                          the Average Speed feature is supported and FTMS_TRMD_FLAG_AVERAGE_SPEED_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint32_t total_distance;            /** Total distance reported by the Server since the beginning of the training session.
                                          Shall be set if the Total Distance feature is supported and
                                          FTMS_TRMD_FLAG_TOTAL_DISTANCE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is meter. The maximum value if 0xFFFFFF. */
    int16_t  inclination;               /** Current inclination of the server. A positive value means that the user feels
                                          as if they are going uphill and a negative value means that the user feels as if
                                          they are going downhill. Shall be set if the Inclination feature is supported and
                                          FTMS_TRMD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT is set in flags.
                                          Ignored otherwise. FTMS_UNAVAILABLE_S16_DATA shall be used if this filed has to
                                          be present but the server does not support the calculation.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a percent. */
    int16_t  ramp_angle_setting;        /** Current setting of the ramp angel of the server. Shall be set if the Inclination
                                          feature is supported and FTMS_TRMD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_S16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a degree. */
    uint16_t positive_elevation_gain;   /** Positive elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_TRMD_FLAG_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a meter. */
    uint16_t negative_elevation_gain;   /** Negative elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_TRMD_FLAG_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a meter. */
    uint16_t instantaneous_pace;        /** Instantaneous pace of a user while exercising. Shall be set if the Pace feature
                                          is supported and FTMS_TRMD_FLAG_INSTANTANEOUS_PACE_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second per 500 meters. */
    uint16_t average_pace;              /** Average pace of a user since the beginning of the training session. Shall be set
                                          if the Pace feature is supported and FTMS_TRMD_FLAG_AVERAGE_PACE_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second per 500 meters. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_TRMD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_TRMD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_TRMD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Unit is metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_TRMD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_TRMD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
    int16_t  force_on_belt;             /** Force being applied to the treadmill belt by the user's steps. A positive value means
                                          that the belt is accelerated and a negative value means that the belt is slowed down.
                                          Shall be set if the Force On Belt And Power Output feature is supported and
                                          FTMS_TRMD_FLAG_FORCE_ON_BELT_AND_POWER_OUTPUT_PRESENT is set in flags. Ignored otherwise.
                                          FTMS_UNAVAILABLE_S16_DATA shall be used if this filed has to be present but the server
                                          does not support the calculation.
                                          Unit is newton. */
    int16_t  power_output;              /** Power being applied to the treadmill belt by the user's steps. A positive value means
                                          that the belt is accelerated and a negative value means that the belt is slowed down.
                                          Shall be set if the Force On Belt And Power Output feature is supported and
                                          FTMS_TRMD_FLAG_FORCE_ON_BELT_AND_POWER_OUTPUT_PRESENT is set in flags. Ignored otherwise.
                                          FTMS_UNAVAILABLE_S16_DATA shall be used if this filed has to be present but the server
                                          does not support the calculation.
                                          Unit is watt. */
} ftms_treadmill_data_t;

/**
 * Cross Trainer Data (CTD) flags bit
 */
#define FTMS_CTD_FLAG_AVERAGE_SPEED_PRESENT                       (1<<1)
#define FTMS_CTD_FLAG_TOTAL_DISTANCE_PRESENT                      (1<<2)
#define FTMS_CTD_FLAG_STEP_COUNT_PRESENT                          (1<<3)
#define FTMS_CTD_FLAG_STRIDE_COUNT_PRESENT                        (1<<4)
#define FTMS_CTD_FLAG_ELEVATION_GAIN_PRESENT                      (1<<5)
#define FTMS_CTD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT  (1<<6)
#define FTMS_CTD_FLAG_RESISTANCE_LEVEL_PRESENT                    (1<<7)
#define FTMS_CTD_FLAG_INSTANTANEOUS_POWER_PRESENT                 (1<<8)
#define FTMS_CTD_FLAG_AVERAGE_POWER_PRESENT                       (1<<9)
#define FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT                     (1<<10)
#define FTMS_CTD_FLAG_HEART_RATE_PRESENT                          (1<<11)
#define FTMS_CTD_FLAG_METABOLIC_EQUIVALENT_PRESENT                (1<<12)
#define FTMS_CTD_FLAG_ELAPSED_TIME_PRESENT                        (1<<13)
#define FTMS_CTD_FLAG_REMAINING_TIME_PRESENT                      (1<<14)
#define FTMS_CTD_FLAG_MOVE_BACKWARD                               (1<<15) /** If not set, move forward */

#define FTMS_CTD_FLAGS_ALL_PRESENT     (FTMS_CTD_FLAG_AVERAGE_SPEED_PRESENT | FTMS_CTD_FLAG_TOTAL_DISTANCE_PRESENT | \
                                        FTMS_CTD_FLAG_STEP_COUNT_PRESENT | FTMS_CTD_FLAG_STRIDE_COUNT_PRESENT | \
                                        FTMS_CTD_FLAG_ELEVATION_GAIN_PRESENT | FTMS_CTD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT | \
                                        FTMS_CTD_FLAG_RESISTANCE_LEVEL_PRESENT | FTMS_CTD_FLAG_INSTANTANEOUS_POWER_PRESENT | \
                                        FTMS_CTD_FLAG_AVERAGE_POWER_PRESENT | FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT | \
                                        FTMS_CTD_FLAG_HEART_RATE_PRESENT | FTMS_CTD_FLAG_METABOLIC_EQUIVALENT_PRESENT | \
                                        FTMS_CTD_FLAG_ELAPSED_TIME_PRESENT | FTMS_CTD_FLAG_REMAINING_TIME_PRESENT)

/** Cross Trainer Data Report */
typedef struct {
    uint32_t flags;                     /** UINT24 Bit mask, FTMS_CTD_FLAG_AVERAGE_SPEED_FIELD etc.
                                          Specifies the availability of the correspoding data filed. */
    uint16_t instantaneous_speed;       /** Instantaneous speed of the cross trainer. Shall always be set.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint16_t average_speed;             /** Average speed since the beginning of the training session. Shall be set if
                                          the Average Speed feature is supported and FTMS_CTD_FLAG_AVERAGE_SPEED_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint32_t total_distance;            /** Total distance reported by the Server since the beginning of the training session.
                                          Shall be set if the Total Distance feature is supported and
                                          FTMS_CTD_FLAG_TOTAL_DISTANCE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is meter. The maximum value if 0xFFFFFF. */
    uint16_t step_per_minute;           /** Average step rate of a user during a period of one minute.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_CTD_FLAG_STEP_COUNT_PRESENT is set in flags. Ignored otherwise.
                                          FTMS_UNAVAILABLE_U16_DATA shall be used if this filed has to be present but the
                                          server does not support the calculation. */
    uint16_t average_step_rate;        /** Average step rate since the beginning of the training session.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_CTD_FLAG_STEP_COUNT_PRESENT is set in flags. Ignored otherwise.
                                          FTMS_UNAVAILABLE_U16_DATA shall be used if this filed has to be present but the
                                          server does not support the calculation. */
    uint16_t stride_count;             /** Total number of strides since the beginning of the training session.
                                          Shall be set if the Stride Count feature is supported and
                                          FTMS_CTD_FLAG_STRIDE_COUNT_PRESENT is set in flags. Ignored otherwise. */
    uint16_t positive_elevation_gain;   /** Positive elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_CTD_FLAG_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a meter. */
    uint16_t negative_elevation_gain;   /** Negative elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_CTD_FLAG_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a meter. */
    int16_t  inclination;               /** Current inclination of the server. A positive value means that the user feels
                                          as if they are going uphill and a negative value means that the user feels as if
                                          they are going downhill. Shall be set if the Inclination feature is supported and
                                          FTMS_CTD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT is set in flags.
                                          Ignored otherwise. FTMS_UNAVAILABLE_S16_DATA shall be used if this filed has to
                                          be present but the server does not support the calculation.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a percent. */
    int16_t  ramp_angle_setting;        /** Current setting of the ramp angel of the server. Shall be set if the Inclination
                                          feature is supported and FTMS_CTD_FLAG_INCLINATION_AND_RAMP_ANGLE_SETTING_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_S16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 of a degree. */
    uint8_t  resistance_level;          /** The current value of the resistance level of the server. Shall be set if
                                          the Resistance Level feature is supported and FTMS_CTD_FLAG_RESISTANCE_LEVEL_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = 1, b = 0
                                          Unit is 1. */
    int16_t  instantaneous_power;       /** Instantaneous power measured by the server. Shall be set if the Power Measurement
                                          feature is supported and FTMS_CTD_FLAG_INSTANTANEOUS_POWER_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is watt. */
    int16_t  average_power;             /** Average power measured by the server since the beginning of the training session.
                                          Shall be set if the Power Management feature is supported and
                                          FTMS_CTD_FLAG_AVERAGE_POWER_PRESENT is set in flags. Ignored otherwise.
                                          Unit is watt. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_CTD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_CTD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_CTD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_CTD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_CTD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
} ftms_cross_trainer_data_t;

/**
 * Step Climber Data (STPCD) flags bit
 */
#define FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT           (1<<1)
#define FTMS_STPCD_FLAG_AVERAGE_STEP_RATE_PRESENT         (1<<2)
#define FTMS_STPCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT   (1<<3)
#define FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT           (1<<4)
#define FTMS_STPCD_FLAG_HEART_RATE_PRESENT                (1<<5)
#define FTMS_STPCD_FLAG_METABOLIC_EQUIVALENT_PRESENT      (1<<6)
#define FTMS_STPCD_FLAG_ELAPSED_TIME_PRESENT              (1<<7)
#define FTMS_STPCD_FLAG_REMAINING_TIME_PRESENT            (1<<8)

#define FTMS_STPCD_FLAGS_ALL   (FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT | FTMS_STPCD_FLAG_AVERAGE_STEP_RATE_PRESENT | \
                                FTMS_STPCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT | FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT | \
                                FTMS_STPCD_FLAG_HEART_RATE_PRESENT | FTMS_STPCD_FLAG_METABOLIC_EQUIVALENT_PRESENT | \
                                FTMS_STPCD_FLAG_ELAPSED_TIME_PRESENT | FTMS_STPCD_FLAG_REMAINING_TIME_PRESENT)

/** Step Climber Data Report */
typedef struct {
    uint16_t flags;                     /** Bit mask, FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT etc.
                                          Specifies the availability of the correspoding data filed. */
    uint16_t floors;                    /** Total number of floors counted by the server since the beginning of the
    									  training session. Shall always be set. */
    uint16_t step_count;                /** Total number of steps counted by the server since the beginning of the
    									  training session. Shall always be set. */
    uint16_t step_per_minute;           /** Average step rate of a user during a period of one minute.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_STPCD_FLAG_STEP_PER_MINUTE_PRESENT is set in flags. Ignored otherwise. */
    uint16_t average_step_rate;        /** Average step rate since the beginning of the training session.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_STPCD_FLAG_AVERAGE_STEP_RATE_PRESENT is set in flags. Ignored otherwise. */
    uint16_t positive_elevation_gain;   /** Positive elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_STPCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Unit is meter. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_STPCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_STPCD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_STPCD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_STPCD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_STPCD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
} ftms_step_climber_data_t;

/**
 * Stair Climber Data (STRCD) flags bit
 */
#define FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT           (1<<1)
#define FTMS_STRCD_FLAG_AVERAGE_STEP_RATE_PRESENT         (1<<2)
#define FTMS_STRCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT   (1<<3)
#define FTMS_STRCD_FLAG_STRIDE_COUNT_PRESENT              (1<<4)
#define FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT           (1<<5)
#define FTMS_STRCD_FLAG_HEART_RATE_PRESENT                (1<<6)
#define FTMS_STRCD_FLAG_METABOLIC_EQUIVALENT_PRESENT      (1<<7)
#define FTMS_STRCD_FLAG_ELAPSED_TIME_PRESENT              (1<<8)
#define FTMS_STRCD_FLAG_REMAINING_TIME_PRESENT            (1<<9)

#define FTMS_STRCD_FLAGS_ALL   (FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT | FTMS_STRCD_FLAG_AVERAGE_STEP_RATE_PRESENT | \
                                FTMS_STRCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT | FTMS_STRCD_FLAG_STRIDE_COUNT_PRESENT | \
                                FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT | FTMS_STRCD_FLAG_HEART_RATE_PRESENT | \
                                FTMS_STRCD_FLAG_METABOLIC_EQUIVALENT_PRESENT | FTMS_STRCD_FLAG_ELAPSED_TIME_PRESENT | \
                                FTMS_STRCD_FLAG_REMAINING_TIME_PRESENT)

/** Stair Climber Data Report */
typedef struct {
    uint16_t flags;                     /** Bit mask, FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT etc.
                                          Specifies the availability of the correspoding data filed. */
    uint16_t floors;                    /** Total number of floors counted by the server since the beginning of the
    									  training session. Shall always be set. */
    uint16_t step_per_minute;           /** Average step rate of a user during a period of one minute.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_STRCD_FLAG_STEP_PER_MINUTE_PRESENT is set in flags. Ignored otherwise. */
    uint16_t average_step_rate;         /** Average step rate since the beginning of the training session.
                                          Shall be set if the Step Count feature is supported and
                                          FTMS_STRCD_FLAG_AVERAGE_STEP_RATE_PRESENT is set in flags. Ignored otherwise. */
    uint16_t positive_elevation_gain;   /** Positive elevation gain since the training session has started. Shall be set if
                                          the Elevation Gain feature is supported and FTMS_STRCD_FLAG_POSITIVE_ELEVATION_GAIN_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Unit is meter. */
    uint16_t stride_count;              /** Total number of strides since the beginning of the training session.
                                          Shall be set if the Stride Count feature is supported and
                                          FTMS_STRCD_FLAG_STRIDE_COUNT_PRESENT is set in flags. Ignored otherwise. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_STRCD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_STRCD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_STRCD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_STRCD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_STRCD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
} ftms_stair_climber_data_t;

/**
 * Rower Data (RD) flags bit
 */
#define FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT                         (1<<1)
#define FTMS_RD_FLAG_TOTAL_DISTANCE_PRESENT                              (1<<2)
#define FTMS_RD_FLAG_INSTANTANEOUS_PACE_PRESENT                          (1<<3)
#define FTMS_RD_FLAG_AVERAGE_PACE_PRESENT                                (1<<4)
#define FTMS_RD_FLAG_INSTANTANEOUS_POWER_PRESENT                         (1<<5)
#define FTMS_RD_FLAG_AVERAGE_POWER_PRESENT                               (1<<6)
#define FTMS_RD_FLAG_RESISTANCE_LEVEL_PRESENT                            (1<<7)
#define FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT                             (1<<8)
#define FTMS_RD_FLAG_HEART_RATE_PRESENT                                  (1<<9)
#define FTMS_RD_FLAG_METABOLIC_EQUIVALENT_PRESENT                        (1<<10)
#define FTMS_RD_FLAG_ELAPSED_TIME_PRESENT                                (1<<11)
#define FTMS_RD_FLAG_REMAINING_TIME_PRESENT                              (1<<12)

#define FTMS_RD_FLAGS_ALL      (FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT | FTMS_RD_FLAG_TOTAL_DISTANCE_PRESENT | \
                                FTMS_RD_FLAG_INSTANTANEOUS_PACE_PRESENT | FTMS_RD_FLAG_AVERAGE_PACE_PRESENT | \
                                FTMS_RD_FLAG_INSTANTANEOUS_POWER_PRESENT | FTMS_RD_FLAG_AVERAGE_POWER_PRESENT | \
                                FTMS_RD_FLAG_RESISTANCE_LEVEL_PRESENT | FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT | \
                                FTMS_RD_FLAG_HEART_RATE_PRESENT | FTMS_RD_FLAG_METABOLIC_EQUIVALENT_PRESENT | \
                                FTMS_RD_FLAG_ELAPSED_TIME_PRESENT | FTMS_RD_FLAG_REMAINING_TIME_PRESENT)

/** Rower Data Report */
typedef struct {
    uint16_t flags;                     /** Bit mask, FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT etc.
                                          Specifies the availability of the correspoding data filed. */
    uint8_t  stroke_rate;               /** Instantaneous stroke rate measured by the server. Shall always be set.
                                          Represent scalar values: M = 1, d = 0, b = -1
                                          Unit is 1/2 of a stroke per minute. */
    uint16_t stroke_count;              /** Total number of strokes since the beginning of the training session. Shall
    								      always be set. */
    uint8_t  average_stroke_rate;       /** Average speed since the beginning of the training session. Shall be set if
                                          the Cadence feature is supported and FTMS_RD_FLAG_AVERAGE_STROKE_RATE_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = 0, b = -1
                                          Unit is 1/2 of a stroke per minute. */
    uint32_t total_distance;            /** Total distance reported by the Server since the beginning of the training
                                          session. Shall be set if the Total Distance feature is supported and
                                          FTMS_RD_FLAG_TOTAL_DISTANCE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is meter. The maximum value if 0xFFFFFF. */
    uint16_t instantaneous_pace;        /** Instantaneous pace of a user while exercising. Shall be set if the Pace feature
                                          is supported and FTMS_RD_FLAG_INSTANTANEOUS_PACE_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second per 500 meters. */
    uint16_t average_pace;              /** Average pace of a user since the beginning of the training session. Shall be set
                                          if the Pace feature is supported and FTMS_RD_FLAG_AVERAGE_PACE_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second per 500 meters. */
    int16_t instantaneous_power;       /** Instantaneous power measured by the server. Shall be set if the Power Measurement
                                          feature is supported and FTMS_RD_FLAG_INSTANTANEOUS_POWER_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is watt. */
    int16_t average_power;             /** Average power measured by the server since the beginning of the training session.
                                          Shall be set if the Power Management feature is supported and
                                          FTMS_RD_FLAG_AVERAGE_POWER_PRESENT is set in flags. Ignored otherwise.
                                          Unit is watt. */
    uint8_t  resistance_level;          /** The current value of the resistance level of the server. Shall be set if
                                          the Resistance Level feature is supported and FTMS_RD_FLAG_RESISTANCE_LEVEL_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = 1, b = 0
                                          Unit is 1. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_RD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_RD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_RD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_RD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_RD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
} ftms_rower_data_t;

/**
 * Indoor Bike Data (IBD) flags bit
 */
#define FTMS_IBD_FLAG_AVERAGE_SPEED_PRESENT                  (1<<1)
#define FTMS_IBD_FLAG_INSTANTANEOUS_CADENCE_PRESENT          (1<<2)
#define FTMS_IBD_FLAG_AVERAGE_CADENCE_PRESENT                (1<<3)
#define FTMS_IBD_FLAG_TOTAL_DISTANCE_PRESENT                 (1<<4)
#define FTMS_IBD_FLAG_RESISTANCE_LEVEL_PRESENT               (1<<5)
#define FTMS_IBD_FLAG_INSTANTANEOUS_POWER_PRESENT            (1<<6)
#define FTMS_IBD_FLAG_AVERAGE_POWER_PRESENT                  (1<<7)
#define FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT                (1<<8)
#define FTMS_IBD_FLAG_HEART_RATE_PRESENT                     (1<<9)
#define FTMS_IBD_FLAG_METABOLIC_EQUIVALENT_PRESENT           (1<<10)
#define FTMS_IBD_FLAG_ELAPSED_TIME_PRESENT                   (1<<11)
#define FTMS_IBD_FLAG_REMAINING_TIME_PRESENT                 (1<<12)

#define FTMS_IBD_FLAGS_ALL     (FTMS_IBD_FLAG_AVERAGE_SPEED_PRESENT | FTMS_IBD_FLAG_INSTANTANEOUS_CADENCE_PRESENT | \
                                FTMS_IBD_FLAG_AVERAGE_CADENCE_PRESENT | FTMS_IBD_FLAG_TOTAL_DISTANCE_PRESENT | \
                                FTMS_IBD_FLAG_RESISTANCE_LEVEL_PRESENT | FTMS_IBD_FLAG_INSTANTANEOUS_POWER_PRESENT | \
                                FTMS_IBD_FLAG_AVERAGE_POWER_PRESENT | FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT | \
                                FTMS_IBD_FLAG_HEART_RATE_PRESENT | FTMS_IBD_FLAG_METABOLIC_EQUIVALENT_PRESENT | \
                                FTMS_IBD_FLAG_ELAPSED_TIME_PRESENT | FTMS_IBD_FLAG_REMAINING_TIME_PRESENT)

/** Indoor Bike Data Report */
typedef struct {
    uint16_t flags;                     /** Bit mask, FTMS_IBD_FLAG_AVERAGE_SPEED_FIELD etc.
                                          Specifies the availability of the correspoding data filed. */
    uint16_t instantaneous_speed;       /** Instantaneous speed of the user. Shall always be set.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint16_t average_speed;             /** Average speed since the beginning of the training session. Shall be set if
                                          the Average Speed feature is supported and FTMS_IBD_FLAG_AVERAGE_SPEED_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = -2, b = 0
                                          Unit is 1/100 of a kilometer per hour. */
    uint16_t instantaneous_cadence;     /** Instantaneous cadence of the user. Shall be set if the Cadence feature is
                                          supported and FTMS_IBD_FLAG_INSTANTANEOUS_CADENCE_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Represent scalar values: M = 1, d = 0, b = -1
                                          Unit is 1/2 of a revolution per minute. */
    uint16_t average_cadence;           /** Average cadence since the beginning of the training session. Shall be set if
                                          the Cadence feature is supported and FTMS_IBD_FLAG_AVERAGE_CADENCE_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = 0, b = -1
                                          Unit is 1/2 of a revolution per minute. */
    uint32_t total_distance;            /** Total distance reported by the Server since the beginning of the training session.
                                          Shall be set if the Total Distance feature is supported and
                                          FTMS_IBD_FLAG_TOTAL_DISTANCE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is meter. The maximum value if 0xFFFFFF. */
    uint8_t  resistance_level;          /** The current value of the resistance level of the server. Shall be set if
                                          the Resistance Level feature is supported and FTMS_IBD_FLAG_RESISTANCE_LEVEL_PRESENT
                                          is set in flags. Ignored otherwise.
                                          Represent scalar values: M = 1, d = 1, b = 0
                                          Unit is 1. */
    int16_t instantaneous_power;       /** Instantaneous power measured by the server. Shall be set if the Power Measurement
                                          feature is supported and FTMS_IBD_FLAG_INSTANTANEOUS_POWER_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is watt. */
    int16_t average_power;             /** Average power measured by the server since the beginning of the training session.
                                          Shall be set if the Power Management feature is supported and
                                          FTMS_IBD_FLAG_AVERAGE_POWER_PRESENT is set in flags. Ignored otherwise.
                                          Unit is watt. */
    uint16_t total_energy;              /** Total expended energy of a user since the training session has started. Shall be
                                          set if the Expended Energy feature is supported and FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint16_t energy_per_hour;           /** Average expended energy of a user during a period of one hour. Shall be set if
                                          the Expended Energy feature is supported and FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U16_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  energy_per_minute;         /** Average expended energy of a user during a period of one minute. Shall be set if
                                          the Expended Energy feature is supported and FTMS_IBD_FLAG_EXPENDED_ENERGY_PRESENT
                                          is set in flags. Ignored otherwise. FTMS_UNAVAILABLE_U8_DATA shall be used if this
                                          filed has to be present but the server does not support the calculation.
                                          Unit is kilogram calorie. */
    uint8_t  heart_rate;                /** Current heart rate of the user. Shall be set if the Heart Rate Measurement feature
                                          is supported and FTMS_IBD_FLAG_HEART_RATE_PRESENT is set in flags. Ignored otherwise.
                                          Unit is beats per minute. */
    uint8_t  metabolic_equivalent;      /** Metabolic equivalent of the user. Shall be set if the Metabolic Equivalent feature
                                          is supported and FTMS_IBD_FLAG_METABOLIC_EQUIVALENT_PRESENT is set in flags. Ignored
                                          otherwise.
                                          Represent scalar values: M = 1, d = -1, b = 0
                                          Unit is 1/10 metabolic equivalent. */
    uint16_t elapsed_time;              /** Elapsed time of a training session since it has started. Shall be set if the Elapsed
                                          Time feature is supported and FTMS_IBD_FLAG_ELAPSED_TIME_PRESENT is set in flags.
                                          Ignored otherwise.
                                          Unit is second. */
    uint16_t remaining_time;            /** Remaining time of a training session that has been selected. Shall be set if the
                                          Remaining Time feature is supported and FTMS_IBD_FLAG_REMAINING_TIME_PRESENT is set
                                          in flags. Ignored otherwise.
                                          Unit is second. */
} ftms_indoor_bike_data_t;

/**
 * Training status flags bit
 */
#define FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT		(1<<0) /** If set, training status string shall be present. */
#define FTMS_TS_FLAG_EXTENDED_STRING_PRESENT			(1<<1) /** If set, the training status string is too long to be fit into one notification packet. */

/**
 * Training state values
 */
typedef enum {
	FTMS_TRAINING_STATE_OTHER,
	FTMS_TRAINING_STATE_IDLE,
	FTMS_TRAINING_STATE_WARMING_UP,
	FTMS_TRAINING_STATE_LOW_INTENSITY_INTERVAL,
	FTMS_TRAINING_STATE_HIGH_INTENSITY_INTERVAL,
	FTMS_TRAINING_STATE_RECOVERY_INTERVAL,
	FTMS_TRAINING_STATE_ISOMETRIC,
	FTMS_TRAINING_STATE_HEART_RATE_CONTROL,
	FTMS_TRAINING_STATE_FITNESS_TEST,
	FTMS_TRAINING_STATE_SPEED_OUTSIDE_CONTROL_LOW,
	FTMS_TRAINING_STATE_SPEED_OUTSIDE_CONTROL_HIGH,
	FTMS_TRAINING_STATE_COOL_DOWN,
	FTMS_TRAINING_STATE_WATT_CONTROL,
	FTMS_TRAINING_STATE_MANUAL_MODE,
	FTMS_TRAINING_STATE_PREWORKOUT,
	FTMS_TRAINING_STATE_POSTWORKOUT,

	FTMS_TRAINING_STATE_RFU_START,
} ftms_training_state_t;

/**
 * Training status
 */
typedef struct ftms_training_status{
	uint8_t flags;          /** Bit mask, FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT etc. */
	uint8_t state;          /** Current training state, one of #ftms_training_state_t. */
	uint16_t string_length; /** Total length of the training status string, including the ending 0 if present.
	                          Ignored if FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT is not set in flags or
	                          status_string is set to NULL. */
	uint8_t *status_string; /** Training status string in UTF-8 format. May be ended with 0.
	                          Ignored if FTMS_TS_FLAG_TRAINING_STATUS_STRING_PRESENT is not set in flags. */
} ftms_training_status_t;

/**
 * Fitness Machine status OpCode
 */
typedef enum {
	FTMS_FTMS_OPC_RFU0,
    FTMS_FTMS_OPC_RESET,
    FTMS_FTMS_OPC_STOPPED_OR_PAUSED_BY_USER,
    FTMS_FTMS_OPC_STOPPED_BY_SAFETY_KEY,
    FTMS_FTMS_OPC_STARTED_OR_RESUMED_BY_USER,
    FTMS_FTMS_OPC_TARGET_SPEED_CHANGED,
    FTMS_FTMS_OPC_TARGET_INCLINE_CHANGED,
    FTMS_FTMS_OPC_TARGET_RESISTANCE_LEVEL_CHANGED,
    FTMS_FTMS_OPC_TARGET_POWER_CHANGED,
    FTMS_FTMS_OPC_TARGET_HEART_RATE_CHANGED,
    FTMS_FTMS_OPC_TARGETED_EXPENDED_ENERGY_CHANGED,
    FTMS_FTMS_OPC_TARGETED_NUMBER_OF_STEPS_CHANGED,
    FTMS_FTMS_OPC_TARGETED_NUMBER_OF_STRIDES_CHANGED,
    FTMS_FTMS_OPC_TARGETED_DISTANCE_CHANGED,
    FTMS_FTMS_OPC_TARGETED_TRAINING_TIME_CHANGED,
    FTMS_FTMS_OPC_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES_CHANGED,
    FTMS_FTMS_OPC_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES_CHANGED,
    FTMS_FTMS_OPC_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES_CHANGED,
    FTMS_FTMS_OPC_INDOOR_BIKE_SIMULATION_PARAMETERS_CHANGED,
    FTMS_FTMS_OPC_WHEEL_CIRCUMFERENCE_CHANGED,
    FTMS_FTMS_OPC_SPIN_DOWN_STATUS_CHANGED,
    FTMS_FTMS_OPC_TARGETED_CADENCE_CHANGED,
    FTMS_FTMS_OPC_RFU_START,
    FTMS_FTMS_OPC_RFU_END = 0xFE,
    FTMS_FTMS_OPC_CONTROL_PERMISSION_LOST
} ftms_ftm_status_op_code_t;

/**
 * Fitness Machine Status value.
 */
typedef struct {
    uint8_t  op_code;                        /** ftms_ftm_status_op_code_t. fitness machine status OpCode */
    union {
        uint8_t  stop_pause_op;              /** ftms_stop_pause_control_t. Parameter of OP code FTMS_FTMS_OPC_STOPPED_OR_PAUSED_BY_USER_CHANGED. */
        uint16_t speed;                      /** New speed value, in km/h with a resolution of 0.01 km/h (M=1, d=-2, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGET_SPEED_CHANGED. */
        int16_t  inclination;                /** New inclination value, in percent with a resolution of 0.1% (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGET_INCLINE_CHANGED. */
        uint8_t  resistance_level;           /** New resistance level value, unitless with a resolution of 0.1 (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGET_RESISTANCE_LEVEL_CHANGED. */
        int16_t  power;                      /** New power value, in watt with a resolution of 1W (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGET_POWER_CHANGED. */
        uint8_t  heart_rate;                 /** New heart rate, in BPM with a resolution of 1 BPM (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGET_HEART_RATE_CHANGED. */
        uint16_t expended_energy;            /** New expended energy value, in calories with a resolution of 1 calorie (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_EXPENDED_ENERGY_CHANGED. */
        uint16_t number_steps;               /** New number of steps value, in steps with a resolution of 1 step (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_NUMBER_OF_STEPS_CHANGED. */
        uint16_t number_strides;             /** New number of strides value, in strides with a resolution of 1 stride (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_NUMBER_OF_STRIDES_CHANGED. */
        uint32_t distance;                   /** New distance value, UINT24, in meters with a resolution of 1 meter (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_DISTANCE_CHANGED. */
        uint16_t training_time;              /** New training time value, in seconds with a resolution of 1 second (M=1, d=0, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_TRAINING_TIME_CHANGED. */
        ftms_two_zones_t two_zone_times;     /** New training time in two heart rate zones.
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_TIME_IN_TWO_HEART_RATE_ZONES_CHANGED. */
        ftms_three_zones_t three_zone_times; /** New training time in three heart rate zones.
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_TIME_IN_THREE_HEART_RATE_ZONES_CHANGED. */
        ftms_five_zones_t five_zone_times;   /** New training time in five heart rate zones.
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_TIME_IN_FIVE_HEART_RATE_ZONES_CHANGED. */
        ftms_simulation_parameter_t simulation_parameter; /** New Indoor bike simulation parameters.
                                                            Parameter of OP code FTMS_FTMS_OPC_INDOOR_BIKE_SIMULATION_PARAMETERS_CHANGED. */
        uint16_t wheel_circumference;        /** New Wheel circumference value, in millimeters with resolution of 0.1 mm (M=1, d=-1, b=0).
                                               Parameter of OP code FTMS_FTMS_OPC_WHEEL_CIRCUMFERENCE_CHANGED. */
        uint8_t  spin_down_op;               /** ftms_spin_down_status_t. Parameter of OP code FTMS_FTMS_OPC_SPIN_DOWN_CONTROL_CHANGED. */
        uint16_t cadence;                    /** New cadence value, in 1/minute with a resolution of 0.5 1/minute (M=1, d=0, b=-1).
                                               Parameter of OP code FTMS_FTMS_OPC_TARGETED_CADENCE_CHANGED. */
    } parameter;                             /** OpCode specific parameter. */
} ftms_ftm_status_t;

typedef struct {
    peer_t inst;
    uint16_t report_states;
    uint8_t  ctl_flags;
} app_ftms_peer_t;

/*
 * FUNCTION DECLARATIONS
 *******************************************************************************
 */

/**
 * Initialize the fitness machine service (FTMS).
 *
 * @param[in] inf      Information of the FTMS. It SHALL NOT be NULL.
 * @param[in]  cbk     The callback function to report the operation requested
 *                     by the client. It SHALL NOT be NULL.
 * @param[in]  ctx     The costumized Context data for this task, will be returned
 *                     in the callback function.
 */
void ftms_start(ftms_inf_t *inf, ftms_cbk_t *cbk, void *ctx);

/**
 * Close the fitness machine service (FTMS).
 */
void ftms_stop(void);

/**
 * Send treadmill data record to the client. The data is dropped if there is
 * no connection between local device and the client.
 *
 * The application shall not send the treadmill data if the notification of
 * treadmill data is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_TREADMILL_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                    NOT be NULL.
 * @param[in] data    Treadmill data to send. It SHALL NOT be NULL.
 */
void ftms_send_treadmill_data(peer_t *client, ftms_treadmill_data_t *data);

/**
 * Send cross trainer data record to the client. The data is dropped if there
 * is no connection between local device and the client.
 *
 * The application shall not send the cross trainer data if the notification of
 * cross trainer data is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_CROSS_TRAINER_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                    NOT be NULL.
 * @param[in] data    Cross trainer data to send. It SHALL NOT be NULL.
 */
void ftms_send_cross_trainer_data(peer_t *client, ftms_cross_trainer_data_t *data);

/**
 * Send step climber data record to the client. The data is dropped if there
 * is no connection between local device and the client.
 *
 * The application shall not send the step climber data if the notification of
 * step climber data is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_STEP_CLIMBER_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                    NOT be NULL.
 * @param[in] data    Step climber data to send. It SHALL NOT be NULL.
 */
void ftms_send_step_climber_data(peer_t *client, ftms_step_climber_data_t *data);

/**
 * Send stair climber data record to the client. The data is dropped if there
 * is no connection between local device and the client.
 *
 * The application shall not send the stair climber data if the notification of
 * stair climber data is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_STAIR_CLIMBER_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                       NOT be NULL.
 * @param[in] data       Stair climber data to send. It SHALL NOT be NULL.
 */
void ftms_send_stair_climber_data(peer_t *client, ftms_stair_climber_data_t *data);

/**
 * Send rower data record to the client. The data is dropped if there is
 * no connection between local device and the client.
 * The application shall not send the rower data if the notification of
 * rower data is disabled. The notification is disabled for the first time
 * connection. The application shall watch the the state change event
 * FTMS_EV_ROWER_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                    NOT be NULL.
 * @param[in] data    rower data to send. It SHALL NOT be NULL.
 */
void ftms_send_rower_data(peer_t *client, ftms_rower_data_t *data);

/**
 * Send indoor bike data record to the client. The data is dropped if there
 * is no connection between local device and the client.
 *
 * The application shall not send the indoor bike data if the notification of
 * indoor bike data is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_INDOOR_BIKE_DATA_REPORT_STATE for the new notification state. The
 * notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * @param[in] client  Instance of the client to receive the data. It SHALL
 *                    NOT be NULL.
 * @param[in] data    Indoor bike data to send. It SHALL NOT be NULL.
 */
void ftms_send_indoor_bike_data(peer_t *client, ftms_indoor_bike_data_t *data);

/**
 * Send current training status to the client. The data is dropped if there
 * is no connection between local device and the client.
 *
 * The application shall not send the training status if the notification of
 * training status is disabled or there is no pending read request of current
 * training status.
 *
 * The notification is disabled for the first time connection. The application
 * shall watch the the state change event FTMS_EV_TRAINING_STATUS_REPORT_STATE
 * for the new notification state. The notification state shall kept unchanged
 * after reconnecting with a bonded client.
 *
 * The application shall reply to each FTMS_EV_READ_TRAINING_STATUS_IND.
 *
 * @param[in] client  Instance of the client to receive the status. It SHALL
 *                    NOT be NULL.
 * @param[in] status  Training status to send. It SHALL NOT be NULL. If status
 *                    string is present, the full string shall be provided,
 *                    and the lower layer will truncate the value according
 *                    to the transport MTU size and the operation context.
 *                    That is, FTMS_TS_FLAG_EXTENDED_STRING_PRESENT shall not
 *                    be set by the application and ignored otherwise.
 */
void ftms_send_training_status(peer_t *client, ftms_training_status_t *status);

/**
 * Send current fitness machine status to the client. The data is dropped
 * if there is no connection between local device and the client.
 *
 * The application shall not send the machine status if the notification of
 * machine status is disabled. The notification is disabled for the first
 * time connection. The application shall watch the the state change event
 * FTMS_EV_FITNESS_MACHINE_STATUS_REPORT_STATE for the new notification state.
 * The notification state shall kept unchanged after reconnecting with a bonded
 * client.
 *
 * If the status is updated by the user (e.g., the user increases the speed via
 * the UI of the Server), the application shall notify the machine status to all
 * connected Clients with the appropriate characteristic value.
 *
 * If the status is updated by a client (e.g., a Client increases the speed via
 * the Fitness Machine Control Point procedure, the application shall notify the
 * new machine status to the other connected Clients, if any.
 *
 * The application shall send the machine status notification with the op_code
 * set to FTMS_FTMS_OPC_CONTROL_PERMISSION_LOST to the client that has lost
 * control (e.g., control has been revoked by the user via the application UI
 * or if another connected client has requested control).
 *
 * @param[in] client  Instance of the client to receive the status. It SHALL
 *                    NOT be NULL.
 * @param[in] status  Fitness machine status to send. It SHALL NOT be NULL.
 */
void ftms_send_fitness_machine_status(peer_t *client, ftms_ftm_status_t *status);

/**
 * Restore the indication state (enabled) of Fitness Machine Control Point for
 * the specified client. The indication is disabled for the first time
 * connection. The application shall watch the the state change event
 * FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_REPORT_STATE for the new indication
 * state.
 *
 * The indication state shall kept unchanged for a bonded client even after
 * disconnection. The indication state shall be reset to disabled after
 * connection with a non-bonded client is disconnected.
 *
 * The application should store the indication state of a bonded client and
 * restore it each time this client is connected. Any write requests to the
 * control point are rejected directly if the indication enabled state is not
 * restored by the application or set by client for the current connection.
 *
 * @param[in] client  Instance of the client to restore enabled state. It SHALL
 *                    NOT be NULL.
 */
void ftms_control_indication_enabled(peer_t *client);

/**
 * Send indication with the result of control procedure. This is response to a
 * previous FTMS_EV_FITNESS_MACHINE_CONTROL_POINT_WRITE_IND request.
 *
 * @param[in] client  Instance of the client to receive the response. It SHALL
 *                    NOT be NULL.
 * @param[in] rsp_par Response parameter. It SHALL NOT be NULL.
 */
void ftms_control_response(peer_t *client, ftms_control_response_t *rsp_par);


void ble_ftms_init(void);

/// @} FTMS

#endif
