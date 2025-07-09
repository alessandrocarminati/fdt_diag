#ifndef UNIQUE_PRINTF_H
#define UNIQUE_PRINTF_H

#define PRINTF_BUFFER_MD5_POS 1024
#define UNIQUE_PRINTF(...) unique_printf_impl(__VA_ARGS__)

int is_md5_seen(const unsigned char* md5);
void add_md5(const unsigned char* md5);
int unique_printf_impl(const char *format, ...);

#endif
