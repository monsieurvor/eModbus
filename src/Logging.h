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

constexpr const char* str_end(const char *str) {
    return *str ? str_end(str + 1) : str;
}

constexpr bool str_slant(const char *str) {
    return ((*str == '/') || (*str == '\\')) ? true : (*str ? str_slant(str + 1) : false);
}

constexpr const char* r_slant(const char* str) {
    return ((*str == '/') || (*str == '\\')) ? (str + 1) : r_slant(str - 1);
}

constexpr const char* file_name(const char* str) {
    return str_slant(str) ? r_slant(str_end(str)) : str;
}