#pragma once
#include "Bmp180Compensation.h"

void beginWeatherLoRaTx();
void pollWeatherLoRaTx();
bool weatherLoRaTxReady();
void publishWeatherLoRaTx(const bmp180::Measurement &measurement);
