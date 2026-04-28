#ifndef VITA_STORAGE_H
#define VITA_STORAGE_H

#include <stdbool.h>

#include <vita/boot.h>

#define VITA_STORAGE_PATH_MAX 192U
#define VITA_STORAGE_ERROR_MAX 128U
#define VITA_STORAGE_TEXT_MAX 512U
#define VITA_STORAGE_READ_MAX 2048U

typedef enum {
    VITA_STORAGE_BACKEND_NONE = 0,
    VITA_STORAGE_BACKEND_HOSTED = 1,
    VITA_STORAGE_BACKEND_UEFI_SIMPLE_FS = 2,
} vita_storage_backend_t;

typedef struct {
    bool initialized;
    bool writable;
    bool bootstrap_attempted;
    bool bootstrap_verified;
    vita_storage_backend_t backend;
    char backend_name[32];
    char root_hint[64];
    char last_error[VITA_STORAGE_ERROR_MAX];
} vita_storage_status_t;

bool storage_init(const vita_handoff_t *handoff);
bool storage_is_ready(void);
bool storage_bootstrap_persistent_tree(void);
bool storage_is_bootstrap_verified(void);
void storage_get_status(vita_storage_status_t *out_status);
void storage_show_status(void);
bool storage_write_text(const char *path, const char *text);
bool storage_read_text(const char *path, char *out, unsigned long out_cap);
bool storage_list_notes(void);
const char *storage_last_error(void);
bool storage_handle_command(const char *cmd);

#endif
