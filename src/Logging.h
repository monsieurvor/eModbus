#if defined(ESP32)
#include <esp32-hal-log.h> // Use build flag `-D CORE_DEBUG_LEVEL` to set logging level
#else
#define log_v
#define log_buf_v
#define log_d
#define log_buf_d
#define log_i
#define log_buf_i
#define log_w
#define log_buf_w
#define log_e
#define log_buf_e
#endif