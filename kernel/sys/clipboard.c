#include "kernel.h"
#include "sys/clipboard.h"

static char _buf[CLIPBOARD_SIZE];
static int  _len = 0;

static int my_strnlen(const char* s, int max) {
    int i = 0;
    while (i < max && s[i]) i++;
    return i;
}

void clipboard_copy(const char* text) {
    if (!text) { clipboard_clear(); return; }
    int n = my_strnlen(text, CLIPBOARD_SIZE - 1);
    for (int i = 0; i < n; i++) _buf[i] = text[i];
    _buf[n] = '\0';
    _len = n;
}

const char* clipboard_paste(void) {
    return _buf;
}

void clipboard_clear(void) {
    _buf[0] = '\0';
    _len = 0;
}

int clipboard_empty(void) {
    return _len == 0;
}
