#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize OS helpers
 * Configure and start tick timer
 */
void furry_hal_os_init();

/* Advance OS tick counter
 */
void furry_hal_os_tick();

#ifdef __cplusplus
}
#endif