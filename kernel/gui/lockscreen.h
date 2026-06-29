#ifndef LOCKSCREEN_H
#define LOCKSCREEN_H

void lockscreen_render(void);
int  lockscreen_input(char c);   /* returns 1 when unlocked */
void lockscreen_reset(void);

#endif
