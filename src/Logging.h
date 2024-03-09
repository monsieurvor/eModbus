#if defined(ESP32)
#include <esp32-hal-log.h>
#ifdef MODBUS_DEBUG 
#define mb_log_v log_v
#define mb_log_buf_v log_buf_v
#define mb_log_d log_d
#define mb_log_buf_d log_buf_d
#define mb_log_i log_i
#define mb_log_buf_i log_buf_i
#define mb_log_w log_w
#define mb_log_buf_w log_buf_w 
#define mb_log_e log_e
#define mb_log_buf_e log_buf_e
#else
#define mb_log_v
#define mb_log_buf_v
#define mb_log_d
#define mb_log_buf_d
#define mb_log_i mb_log_i
#define mb_log_buf_i
#define mb_log_w
#define mb_log_buf_w
#define mb_log_e
#define mb_log_buf_e
#endif
#else
#define mb_log_v
#define mb_log_buf_v
#define mb_log_d
#define mb_log_buf_d
#define mb_log_i mb_log_i
#define mb_log_buf_i
#define mb_log_w
#define mb_log_buf_w
#define mb_log_e
#define mb_log_buf_e
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