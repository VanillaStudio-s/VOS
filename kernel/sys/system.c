#include "kernel.h"
#include "drivers/screen.h"
#include "mem/heap.h"
#include "fs/fs.h"
#include "sys/user.h"
#include "sys/system.h"

void show_cpu_info(void) {
    unsigned int r[4]; char brand[49]; brand[48]='\0';
    __asm__ volatile("cpuid":"=a"(r[0]):"a"(0x80000000));
    if (r[0] < 0x80000004) { my_puts("CPU: Generic x86\n"); return; }
    for (int i=0;i<3;i++) {
        __asm__ volatile("cpuid":"=a"(r[0]),"=b"(r[1]),"=c"(r[2]),"=d"(r[3]):"a"(0x80000002+i));
        for (int j=0;j<4;j++) ((unsigned int*)brand)[i*4+j]=r[j];
    }
    my_puts("CPU: "); my_puts(brand); my_putchar('\n');
}

void show_ram_info(void) {
    unsigned short lm = *(volatile unsigned short*)0x413;
    my_puts("RAM: ~"); my_print_int((int)lm); my_puts(" KB conventional\n");
}

void show_storage_info(void) {
    my_puts("STORAGE: ");
    if (inb(0x1F7)==0xFF) my_puts("No ATA drive\n");
    else my_puts("ATA Primary drive present\n");
}

int check_nvram_validity(void) {
    return (*(volatile unsigned int*)0x50000 == NVRAM_MAGIC);
}

/*
 * NVRAM layout (at 0x50000):
 *   [4]  magic
 *   [80] user_database
 *   [80] pass_database
 *   [4]  fs_node_count
 *   [4]  current_dir_idx
 *   per node (fs_node_count entries):
 *     [32] name
 *     [4]  type
 *     [4]  parent_idx
 *     [4]  content_len
 *     [content_len] content bytes (no null terminator stored)
 */
void save_system_to_nvram(void) {
    char* p = (char*)0x50000;
    *((unsigned int*)p) = NVRAM_MAGIC; p += 4;
    for (int i=0;i<5*16;i++) { p[i]=((char*)user_database)[i]; } p+=80;
    for (int i=0;i<5*16;i++) { p[i]=((char*)pass_database)[i]; } p+=80;
    *((int*)p) = fs_node_count; p+=4;
    *((int*)p) = current_dir_idx; p+=4;
    for (int i=0;i<fs_node_count;i++) {
        for (int j=0;j<32;j++) { p[j]=fs_tree[i].name[j]; } p+=32;
        *((int*)p)=fs_tree[i].type; p+=4;
        *((int*)p)=fs_tree[i].parent_idx; p+=4;
        int clen=fs_tree[i].content_len;
        *((int*)p)=clen; p+=4;
        if (clen>0 && fs_tree[i].content) {
            for (int j=0;j<clen;j++) p[j]=fs_tree[i].content[j];
            p+=clen;
        }
    }
}

void load_system_from_nvram(void) {
    char* p = (char*)0x50000 + 4; /* skip magic */
    for (int i=0;i<5*16;i++) { ((char*)user_database)[i]=p[i]; } p+=80;
    for (int i=0;i<5*16;i++) { ((char*)pass_database)[i]=p[i]; } p+=80;
    int nc=*((int*)p); p+=4;
    int cdi=*((int*)p); p+=4;
    /* free existing heap content */
    for (int i=0;i<fs_node_count;i++)
        if (fs_tree[i].content) { kfree(fs_tree[i].content); fs_tree[i].content=(char*)0; fs_tree[i].content_len=0; }
    fs_node_count=nc;
    current_dir_idx=cdi;
    for (int i=0;i<nc;i++) {
        for (int j=0;j<32;j++) { fs_tree[i].name[j]=p[j]; } p+=32;
        fs_tree[i].type=*((int*)p); p+=4;
        fs_tree[i].parent_idx=*((int*)p); p+=4;
        int clen=*((int*)p); p+=4;
        fs_tree[i].content=(char*)0; fs_tree[i].content_len=0;
        if (clen>0 && clen<FS_CONTENT_MAX) {
            fs_tree[i].content=(char*)kmalloc(clen+1);
            if (fs_tree[i].content) {
                for (int j=0;j<clen;j++) fs_tree[i].content[j]=p[j];
                fs_tree[i].content[clen]='\0';
                fs_tree[i].content_len=clen;
            }
        }
        p+=clen;
    }
}


void sys_restart(void) {
    save_system_to_nvram();
    __asm__ volatile("cli");
    outb(0xCF9, 0x06);
    unsigned char s = 0x02;
    while (s & 0x02) s = inb(0x64);
    outb(0x64, 0xFE);
    __asm__ volatile("lidt 0");
    __asm__ volatile("int $3");
    __asm__ volatile("hlt");
    while(1){}
}

void sys_logout(void) {
    is_locked = 1;
    my_puts_color("[LOCKED] Enter password: ", 0x0C);
}