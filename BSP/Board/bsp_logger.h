#ifndef __BSP_LOGGER_H__
#define __BSP_LOGGER_H__

/* 包含 elog.h：使调用方通过 #include "bsp_logger.h" 即可使用 log_x() 快捷宏，
 * 无需在每个业务文件里再单独包含 elog.h。 */
#include "elog.h"

void bsp_logger_init(void);

#endif