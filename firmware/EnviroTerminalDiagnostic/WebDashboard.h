#pragma once
#include <Arduino.h>
#include "Bmp180Compensation.h"
#ifndef ENVIRO_ENABLE_WEB
#define ENVIRO_ENABLE_WEB 0
#endif
#ifndef ENVIRO_REMOTE_SENSOR
#define ENVIRO_REMOTE_SENSOR 0
#endif
#ifndef ENVIRO_SAMPLE_MAX_AGE_MS
#define ENVIRO_SAMPLE_MAX_AGE_MS 7000
#endif
void beginWebDashboard();
void pollWebDashboard();
void publishWebMeasurement(const bmp180::Measurement &measurement);
void publishWebRadioSignal(int rssi, float snr);
using WebRadioDiagnosticProvider = String (*)();
void setWebRadioDiagnosticProvider(WebRadioDiagnosticProvider provider);
