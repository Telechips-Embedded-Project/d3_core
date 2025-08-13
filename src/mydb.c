#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include "../include/mydb.h"

static sqlite3 *db;


// [DB �ʱ�ȭ]
int init_db(const char *path) {
    char *err = NULL;
    int rc = sqlite3_open("settings.db", &db);
    if (rc != SQLITE_OK) {
        printf("DB open error\n");
        return -1;
    }

    const char *sql =
        "CREATE TABLE IF NOT EXISTS user_profiles ("
        "user_id TEXT PRIMARY KEY, "  // ID: string : �ߺ��Ұ�
        "auto_ac INTEGER, "  // off/on 0,1
        "auto_window INTEGER, "  // 0,1
        "auto_wiper INTEGER, "  // 0,1
        "ambient_color TEXT);";  // Color: string

    rc = sqlite3_exec(db, sql, 0, 0, &err);  
    if (rc != SQLITE_OK) {
        printf("DB init error: %s\n", err);
        sqlite3_free(err);
        sqlite3_close(db);
        return 0;
    }
    return 1;
}

// [����ڰ� ���� ��ư Ŭ�� �� �ش� �Լ� ȣ��]
void save_user_profile(const char* user_id, int ac, int window, int wiper, const char* color) {
    sqlite3 *db;
    sqlite3_open("settings.db", &db);

    const char *sql =
        "INSERT OR REPLACE INTO user_profiles "
        "(user_id, auto_ac, auto_window, auto_wiper, ambient_color) "
        "VALUES (?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL); //SQL ������ -> stmt ��ü ����
  
    // ?�� ���� ������ ġȯ
    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, ac);
    sqlite3_bind_int(stmt, 3, window);
    sqlite3_bind_int(stmt, 4, wiper);
    sqlite3_bind_text(stmt, 5, color, -1, SQLITE_STATIC);

    sqlite3_step(stmt); // SQL (insert or update) ����
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

// [ ����� ��ȯ(������ Ŭ����) ]
int load_user_profile(const char* user_id, int* ac, int* window, int* wiper, char* color_buf, size_t buf_size) {
    sqlite3 *db;
    sqlite3_open("settings.db", &db);

    const char *sql =
        "SELECT auto_ac, auto_window, auto_wiper, ambient_color "
        "FROM user_profiles WHERE user_id = ?;";

    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_bind_text(stmt, 1, user_id, -1, SQLITE_STATIC);

    int found = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *ac     = sqlite3_column_int(stmt, 0);
        *window = sqlite3_column_int(stmt, 1);
        *wiper  = sqlite3_column_int(stmt, 2);
        const unsigned char* color = sqlite3_column_text(stmt, 3);
        strncpy(color_buf, (const char*)color, buf_size);
        found = 1;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return found;
}

// [ ����� ������ list show]
void get_user_list() {
    sqlite3 *db;
    sqlite3_open("settings.db", &db);

    const char *sql = "SELECT user_id FROM user_profiles;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    printf("Available profiles:\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* user = sqlite3_column_text(stmt, 0);
        printf(" - %s\n", user);
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}


