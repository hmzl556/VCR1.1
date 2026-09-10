/**
 * @file task.h
 * @author Barrot Firmware Team
 * @brief This file includes the definition of Task handler and timer functions.
 * @version 0.1
 * @date 2023-04-27
 * 
 * @copyright Copyright (c) 2023, Barrot Technology Co.,Ltd.. All rights reserved.
 * 
 */

#ifndef __TASK__H__
#define __TASK__H__

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "porting.h"

/*
 * DEFINES
 ****************************************************************************************
 */

///Defines the length of the timer name
#define MAX_TIMER_NAME_LEN        7

///Create task
#define TASK_CREATE(func_, arg_, ev_, act_, period_)   task_create((task_func *)(func_), (void *)(arg_), ev_, act_, period_)

///Create a one-time task and set it to active
#define TASK_CREATE_ONESHOT(func_, arg_, ev_)          task_create((task_func *)(func_), (void *)(arg_), ev_, true, false)

///Create a long-term task and set it to active
#define TASK_CREATE_LONGTERM(func_, arg_, ev_)         task_create((task_func *)(func_), (void *)(arg_), ev_, true, true) 

///Create a timer task, don't care handle
#define TIMER_CREATE(millisec, arg, func)              timer_create((tid_t *)NULL, millisec, false, (void *)arg, (task_func *)func)               

///Create a one-time timer task
#define TIMER_CREATE_ONESHOT(tid, millisec, arg, func) timer_create((tid_t *)tid, millisec, false, (void *)arg, (task_func *)func)

///Create a repeat timer task
#define TIMER_CREATE_REPEAT(tid, millisec, arg, func)  timer_create((tid_t *)tid, millisec, true, (void *)arg, (task_func *)func)

/*
 * TYPEDEF
 ****************************************************************************************
 */

///define the tid_t type
typedef unsigned short int  tid_t;

///define the task callback type
typedef void (task_func)(uint8_t ev, void *arg);

///define the task variable type
typedef struct{
    /// Task callback
    task_func* callback;    
    /// Arguments to the callback function
    void* arg;
    /// event type
    uint8_t event; 
    /// active state
    bool active;      
    /// Periodic task flag
    bool period; 
} task_handler_t;

///Timer structure for UI
typedef struct{
    /// Default Timer expire function
    task_func *func;    
    /// Argument for the event.        
    void *arg; 
    /// Timeout value   
    uint32_t millisec; 
    /// Event when time out
    uint8_t event;
    /// timer handle
    tid_t tid;
    /// timer handle pointer
    tid_t *tid_p;
    /// Repeated timing
    bool repetition;
#ifdef CONFIG_DBG_TIMER
    /// Name for the timer
    char name[MAX_TIMER_NAME_LEN];
#endif
}timer_t;

/*
 * GLOBAL FUNCTION DECLARATIONS
 ****************************************************************************************
 */

/**
 * @brief Create a task
 * 
 * @param func Task callback
 * @param arg Arguments to the callback function
 * @param event event type
 * @param active active state
 * @param period One-time task flag
 * @return task_handler_t* Task handle or NULL
 */
task_handler_t * task_create(task_func *func, void *arg, uint8_t event, bool active, bool period);

/**
 * @brief Delate a task
 * 
 * @param handle Task handle
 * @return bool operating result
 */
bool task_delete(task_handler_t *handle);

/**
 * @brief Set the task to active
 * 
 * @param handle  Task handle
 * @return bool  operating result
 */
bool task_set_active(task_handler_t *handle);

/**
 * @brief Create a scheduled task, timer tasks are one-time tasks
 * 
 * @param tid variable pointer that stores the timer handle    
 * @param millisec time-out period
 * @param arg arg for Callback function pointer
 * @param func Callback function pointer
 * @return tid_t Handle to the timer
 */
bool timer_create(tid_t *tid, uint32_t millisec, bool repetition, void *arg, task_func *func);

/**
 * @brief Delete a timer task
 * 
 * @param tid Timer handle pointer
 * @return bool operating result
 */
bool timer_delete(tid_t *tid);

/**
 * @brief Reset the time of a timer task
 * 
 * @param tid Timer handle pointer
 * @return bool operating result
 */
bool timer_restart(tid_t *tid);

/**
 * @brief Create the corresponding task after the timeout
 * 
 * @param tid handle of timer
 * @return bool Return is used to specify whether any timer is running
 */
bool timer_handle(tid_t tid);

/**
 * @brief Initializes the task module
 * 
 */
void task_init(void);

/**
 * @brief The main call function of the task module
 * 
 * @return uint8_t 
 */
uint8_t task_schedule(void);

#endif/* FSM_H */

