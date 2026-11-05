#pragma once

#ifndef ENVIRO_ENABLE_LORA
#define ENVIRO_ENABLE_LORA 0
#endif

#ifndef ENVIRO_ENABLE_LORA_TX
#define ENVIRO_ENABLE_LORA_TX 0
#endif

void beginSx127xDiagnostic();
void pollSx127xDiagnostic();
bool sx127xSpiReady();
