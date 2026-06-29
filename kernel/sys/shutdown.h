#ifndef SHUTDOWN_H
#define SHUTDOWN_H

/* Saves FS to disk, then powers off via ACPI/APM.
   If power-off fails it halts the CPU instead. */
void sys_shutdown(void);

/* Saves FS to disk, then issues a soft reboot (keyboard controller). */
void sys_reboot(void);

#endif