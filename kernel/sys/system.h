#ifndef SYSTEM_H
#define SYSTEM_H
void show_cpu_info(void);
void show_ram_info(void);
void show_storage_info(void);
int  check_nvram_validity(void);
void save_system_to_nvram(void);
void load_system_from_nvram(void);
void sys_restart(void);
void sys_logout(void);
#endif