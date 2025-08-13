#ifndef MY_DB_H 
#define MY_DB_H

int init_db(const char *path);
void save_user_profile(const char* user_id, int ac, int window, int wiper, const char* color);
int load_user_profile(const char* user_id, int* ac, int* window, int* wiper, char* color_buf, size_t buf_size);
void get_user_list();

#endif