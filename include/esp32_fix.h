#ifndef ESP32_FIX_H
#define ESP32_FIX_H

#ifdef __cplusplus
#include <Arduino.h>
#endif

#ifndef digitalPinToInterrupt
#define digitalPinToInterrupt(p) (p)
#endif

#endif // ESP32_FIX_H
