#ifndef SHELL_H
#define SHELL_H

typedef void (*output_func)(const char*);

void process_command(const char* cmd, output_func out);
void neofetch_command(const char* cmd, output_func out);
void shell_prompt(void);

#endif
