#pragma once

#include "Bmp180Compensation.h"
#include "I2cBusRouter.h"

#ifndef ENVIRO_ENABLE_LCD
#define ENVIRO_ENABLE_LCD 1
#endif

// Restitue toujours le controleur au BMP180 avant de retourner.
void updateLcd1602(I2cBusRouter &bus, const bmp180::Measurement &measurement);
