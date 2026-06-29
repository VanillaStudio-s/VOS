#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#define CLIPBOARD_SIZE 1024

void  clipboard_copy(const char* text);
const char* clipboard_paste(void);
void  clipboard_clear(void);
int   clipboard_empty(void);

#endif
