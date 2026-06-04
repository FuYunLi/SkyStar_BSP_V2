#ifndef __PORT_DWT_H
#define __PORT_DWT_H

#include "bsp_board.h"

#ifdef __cplusplus
extern "C" {
#endif

bsp_status_t port_dwt_init(void);
uint32_t     port_dwt_get_cycles(void);
void         port_dwt_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* __PORT_DWT_H */
