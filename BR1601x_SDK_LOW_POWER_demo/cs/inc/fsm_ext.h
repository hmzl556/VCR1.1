/*---------------------------------------------------------------------------
*
* Copyright (c) 2017-2026 BRT Corporation
*
* All rights reserved.
*
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
Module Name:
    fsm_ext.h
Abstract:
    This file includes the definition of finite state machine functions.
Author:
    Tian Leixn
Revision History:
    1. 2025-12-4   copy from fsm.h
---------------------------------------------------------------------------*/
#ifndef FSM_EXT_H
#define FSM_EXT_H

#include "stdint.h"

typedef void (task_func_t) (uint8_t, void *);

/* Timer structure for UI */
struct FsmTimer {/* a number, TIMER_ID, user visit as void *, this is forbidden to visit */
  task_func_t *func;            /* Default Timer expire function */
  void *arg;                /* Argument for the event. */
  uint32_t millisec;        /* Timeout value */
  uint8_t event;            /* Event when time out. */
  int tid;
};

void FSM_Write(uint8_t slot, task_func_t *func, void *arg, uint8_t event);

/* Timer functions */
struct FsmTimer *Timer_Restart(struct FsmTimer *ft, uint32_t millisec, uint8_t event, task_func_t *func, void *arg, void *name/* DBG */, uint32_t u_id);
struct FsmTimer *Timer_Add(uint32_t millisec, uint8_t event, task_func_t *func, void *arg, const char *name, uint32_t u_id);
uint8_t *Timer_DelByP(struct FsmTimer *ft);
void Timer_DelByPFree(struct FsmTimer **ft);

#define TASK_ADD2(func, event, arg)          FSM_Write(1, func, arg, event)//full arguments
#define TASK_ADD1(func,event)                FSM_Write(1, func, NULL, event)//no context
#define TASK_ADD(func)                       FSM_Write(1, func, NULL, 0)//no context and event
#define TIMER_ADD2(millisec,func,event,arg)  Timer_Add(millisec,event,func,arg,NULL,0);//name,u_id not care
#define TIMER_ADD1(millisec,func,event)      Timer_Add(millisec,event,func,NULL,NULL,0);//name,u_id not care
#define TIMER_ADD(millisec,func)             Timer_Add(millisec,0,func,NULL,NULL,0);//name,u_id not care

#endif/* FSM_EXT_H */
