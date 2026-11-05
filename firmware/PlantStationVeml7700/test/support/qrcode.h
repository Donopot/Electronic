#pragma once
#include <string>
// This fake verifies the display contract only. It does not prove QR encoding/scanning.
using esp_qrcode_handle_t = const unsigned char*;
struct esp_qrcode_config_t {void (*display_func)(esp_qrcode_handle_t);int max_qrcode_version;int qrcode_ecc_level;};
#define ESP_QRCODE_CONFIG_DEFAULT() esp_qrcode_config_t{nullptr,10,0}
namespace host { inline std::string& qrPayload(){static std::string value;return value;} }
inline int esp_qrcode_get_size(esp_qrcode_handle_t){return 21;}
inline bool esp_qrcode_get_module(esp_qrcode_handle_t,int x,int y){return (x+y)%2==0;}
inline int esp_qrcode_generate(esp_qrcode_config_t* config,const char* text){host::qrPayload()=text;config->display_func(nullptr);return 0;}
