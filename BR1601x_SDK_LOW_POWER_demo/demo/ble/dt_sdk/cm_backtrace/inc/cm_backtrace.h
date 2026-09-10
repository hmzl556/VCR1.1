#ifndef _CORTEXM_BACKTRACE_H_
#define _CORTEXM_BACKTRACE_H_

#include "cmb_def.h"

#ifdef __cplusplus
extern "C" {
#endif

void cm_backtrace_init(const char *firmware_name, const char *hardware_ver, const char *software_ver);
void cm_backtrace_firmware_info(void);
size_t cm_backtrace_call_stack(uint32_t *buffer, size_t size, uint32_t sp);
void cm_backtrace_assert(uint32_t sp);
void cm_backtrace_fault(uint32_t fault_handler_lr, uint32_t fault_handler_sp);
__asm void cm_backtrace_ErrorDiagnosis(void);
#ifdef __cplusplus
}
#endif

#endif /* _CORTEXM_BACKTRACE_H_ */
