#ifndef HARDWARE_KEY_H_
#define HARDWARE_KEY_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEY_ID_1 = 0,
    KEY_ID_2,
    KEY_ID_COUNT
} KeyId;

void Key_Init(void);
void Key_Update(uint32_t now_ms);
uint8_t Key_WasPressed(KeyId id);

#ifdef __cplusplus
}
#endif

#endif /* HARDWARE_KEY_H_ */
