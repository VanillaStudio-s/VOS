#include "kernel.h"
#include "drivers/screen.h"
#include "sys/user.h"

char user_database[5][16] = {"live","merdo","","",""};
char pass_database[5][16] = {"","1234","","",""};
int  active_user_idx = 0;
int  is_retype_pw = 0;
int  is_locked = 0;
int  is_changing_pw = 0;
int  is_setting_initial_pw = 0;
char pw_first_input[16];

void handle_user_system(const char* cmd) {
    if (cmd[0]=='u'&&cmd[1]=='s'&&cmd[2]=='e'&&cmd[3]=='r'&&cmd[4]==' ') {
        char name[16]; int i=5,j=0;
        while(cmd[i]&&j<15) { name[j]=cmd[i]; j++; i++; } name[j]='\0';
        for (int idx=0;idx<5;idx++)
            if (my_strcmp(user_database[idx],name)==0) {
                active_user_idx=idx;
                if (!pass_database[idx][0]) { is_locked=0; my_puts("Switched.\n"); }
                else { is_locked=1; my_puts("Verification required.\n"); }
                return;
            }
        for (int idx=0;idx<5;idx++)
            if (!user_database[idx][0]) {
                int n=0; while(name[n]&&n<15) { user_database[idx][n]=name[n]; n++; }
                user_database[idx][n]='\0'; pass_database[idx][0]='\0';
                active_user_idx=idx;
                my_puts_color("Profile created!\n",0x0A);
                my_puts_color("Set password: ",0x0E);
                is_setting_initial_pw=1; is_changing_pw=1; return;
            }
        my_puts("Database full!\n");
    }
}

void list_user_system(void) {
    my_puts_color("Users:\n",0x0B);
    for (int i=0;i<5;i++) {
        if (!user_database[i][0]) continue;
        if (i==active_user_idx) my_puts_color(" -> ",0x0A); else my_puts("    ");
        my_puts_color(user_database[i],0x0E);
        if (i==0) my_puts_color(" [live]",0x07);
        if (i==active_user_idx) my_puts_color(" [active]",0x0A);
        my_puts("\n");
    }
}

void delete_user_system(const char* u) {
    if (my_strcmp(u,"live")==0) { my_puts_color("Cannot delete live.\n",0x0C); return; }
    for (int i=1;i<5;i++)
        if (my_strcmp(user_database[i],u)==0) {
            if (active_user_idx!=0&&i!=active_user_idx) { my_puts_color("Only delete yourself.\n",0x0C); return; }
            if (i==active_user_idx&&active_user_idx!=0) { my_puts_color("Logout first.\n",0x0C); return; }
            for (int j=0;j<16;j++) { user_database[i][j]='\0'; pass_database[i][j]='\0'; }
            my_puts_color("Deleted.\n",0x0A); return;
        }
    my_puts_color("User not found.\n",0x0C);
}

void process_pw_input(const char* in) {
    if (is_changing_pw) {
        int i=0; while(in[i]&&i<15) { pw_first_input[i]=in[i]; i++; } pw_first_input[i]='\0';
        is_changing_pw=0; is_retype_pw=1; my_puts_color("Retype: ",0x0E);
    } else if (is_retype_pw) {
        if (my_strcmp(in,pw_first_input)==0) {
            int i=0; while(in[i]&&i<15) { pass_database[active_user_idx][i]=in[i]; i++; }
            pass_database[active_user_idx][i]='\0';
            is_setting_initial_pw=0; my_puts_color("Password set!\n",0x0A);
        } else {
            my_puts_color("Mismatch!\n",0x0C);
            if (is_setting_initial_pw) { my_puts_color("Set password: ",0x0E); is_changing_pw=1; }
        }
        is_retype_pw=0;
    }
}

void handle_pw_system(void) {
    if (active_user_idx==0) { my_puts_color("Cannot change live session pw.\n",0x0C); return; }
    my_puts_color("New password: ",0x0E); is_changing_pw=1;
}
