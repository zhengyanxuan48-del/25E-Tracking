#ifndef HARDWARE_PAN_TILT_DEBUG_H
#define HARDWARE_PAN_TILT_DEBUG_H

#include <stdint.h>

void PanTiltDebug_Init(uint8_t oled_enabled);
void PanTiltDebug_Update(uint32_t now_ms);

#endif /* HARDWARE_PAN_TILT_DEBUG_H */
