#ifndef ESP32_FIX_H
#define ESP32_FIX_H

#include <Arduino.h>

#ifndef digitalPinToInterrupt
#define digitalPinToInterrupt(p) (p)
#endif

#endif // ESP32_FIX_H
