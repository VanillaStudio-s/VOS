#ifndef NET_PANEL_H
#define NET_PANEL_H

void net_tray_draw(int x, int y);
void net_tray_click(void);
int  net_panel_is_open(void);
void net_panel_close(void);
void net_panel_render(void);
int  net_panel_handle_click(int mx, int my);
void net_panel_init(void);

#endif
