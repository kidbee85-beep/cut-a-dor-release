/* GPL-3.0-only: bounded audio deadline guard, independent of flash update code. */
#pragma once
#include <stdint.h>
static inline uint32_t wolf_deadline_next(uint32_t streak, uint32_t elapsed, uint32_t budget)
{
    if (streak >= 8u) return 8u; /* latched until reboot */
    return elapsed >= budget ? streak + 1u : 0u;
}
