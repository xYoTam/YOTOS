#ifndef STRING_H
#define STRING_H

#include <stdint.h>
#include <stddef.h>

void* memset(void* dest, uint8_t value, size_t size);
size_t strlen(const char* str);
int strcmp(const char *s1, const char *s2);
int strcasecmp(const char *a, const char *b);
size_t strlcpy(char *dst, const char *src, size_t size);

#endif
