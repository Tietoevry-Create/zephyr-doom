
#include <stdint.h>
#include <stdio.h>

//  Retargeting (for printf etc)
// ------------------------------------

#ifdef __PRINTF_TAG_PTR_DEFINED

int __getchar(__printf_tag_ptr p_file) {
    uint8_t input = 0;
    // TODO?
    return input;
}

int __putchar(int ch, __printf_tag_ptr p_file) {
    // UNUSED_PARAMETER(p_file);
    return ch;
}
#else
int __getchar(FILE* p_file) {
    uint8_t input = 0;
    // TODO?
    return input;
}

int __putchar(int ch, FILE* p_file) {
    // UNUSED_PARAMETER(p_file);
    return ch;
}
#endif

int _write(int file, const char* p_char, int len) {
    int i;

    // UNUSED_PARAMETER(file);
    for (i = 0; i < len; i++) {
    }
    return len;
}

int _read(int file, char* p_char, int len) {
    // UNUSED_PARAMETER(file);
    // TODO?
    return 1;
}
