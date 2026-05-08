#include "string.h"
#include <stdint.h>
#include <stddef.h>


// Fill value for size bytes where dest points at 
void* memset(void* dest, uint8_t value, size_t size) {
	uint8_t* pointer = (uint8_t*)dest;
	while (size--) {
		*pointer++ = value; 
	}
	return dest;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len])
		len++;
	return len;
}


int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}


size_t strlcpy(char *dst, const char *src, size_t size) {
    size_t i = 0;
    if (size > 0) {
        while (i < size - 1 && src[i]) { dst[i] = src[i]; i++; }
        dst[i] = '\0';
    }
    while (src[i]) i++;
    return i;
}


int strcasecmp(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return (unsigned char)ca - (unsigned char)cb;
        a++; b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}
