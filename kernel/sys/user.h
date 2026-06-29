#ifndef USER_H
#define USER_H

extern char user_database[5][16];
extern char pass_database[5][16];
extern int  active_user_idx;
extern int  is_retype_pw;
extern char pw_first_input[16];
extern int  is_setting_initial_pw;

void handle_user_system(const char* cmd);
void list_user_system(void);
void delete_user_system(const char* user);
void handle_pw_system(void);
void process_pw_input(const char* input);
#endif
