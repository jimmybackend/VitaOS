/*
 * kernel/storage.c
 * Minimal F1A/F1B writable storage facade.
 *
 * Hosted:
 * - maps /vita/... to build/storage/vita/...
 * - writes plain text files with the host filesystem
 *
 * UEFI:
 * - uses UEFI Loaded Image + Simple File System + File Protocol
 * - writes plain text files into an already prepared FAT tree
 * - does not provide a general VFS, directory creation, SQLite, or security model yet
 */

#include <stdint.h>
#include <vita/console.h>
#include <vita/storage.h>

#ifdef VITA_HOSTED
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

#ifndef VITA_HOSTED
#define EFI_SUCCESS 0ULL
#define EFI_BUFFER_TOO_SMALL 5ULL
#define EFI_FILE_MODE_READ 0x0000000000000001ULL
#define EFI_FILE_MODE_WRITE 0x0000000000000002ULL
#define EFI_FILE_MODE_CREATE 0x8000000000000000ULL
#define EFI_FILE_DIRECTORY 0x0000000000000010ULL
#define EFI_LOCATE_BY_PROTOCOL 2ULL

typedef uint16_t char16_t;
typedef uint64_t efi_status_t;
typedef void *efi_handle_t;

typedef struct {
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} efi_guid_t;

typedef struct {
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} efi_table_header_t;

typedef struct efi_boot_services efi_boot_services_t;
typedef struct efi_system_table efi_system_table_t;
typedef struct efi_loaded_image_protocol efi_loaded_image_protocol_t;
typedef struct efi_simple_file_system_protocol efi_simple_file_system_protocol_t;
typedef struct efi_file_protocol efi_file_protocol_t;

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t pad1;
    uint32_t nanosecond;
    int16_t time_zone;
    uint8_t daylight;
    uint8_t pad2;
} efi_time_t;

typedef struct {
    uint64_t size;
    uint64_t file_size;
    uint64_t physical_size;
    efi_time_t create_time;
    efi_time_t last_access_time;
    efi_time_t modification_time;
    uint64_t attribute;
    char16_t file_name[1];
} efi_file_info_t;

struct efi_boot_services {
    efi_table_header_t hdr;

    void *raise_tpl;
    void *restore_tpl;
    void *allocate_pages;
    void *free_pages;
    void *get_memory_map;
    void *allocate_pool;
    void *free_pool;

    void *create_event;
    void *set_timer;
    void *wait_for_event;
    void *signal_event;
    void *close_event;
    void *check_event;

    void *install_protocol_interface;
    void *reinstall_protocol_interface;
    void *uninstall_protocol_interface;

    efi_status_t (*handle_protocol)(efi_handle_t handle,
                                    efi_guid_t *protocol,
                                    void **interface);

    void *reserved;
    void *register_protocol_notify;
    efi_status_t (*locate_handle)(uint64_t search_type,
                                  efi_guid_t *protocol,
                                  void *search_key,
                                  uint64_t *buffer_size,
                                  efi_handle_t *buffer);
    void *locate_device_path;
    void *install_configuration_table;

    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;

    void *get_next_monotonic_count;
    void *stall;
};

struct efi_system_table {
    efi_table_header_t hdr;
    char16_t *firmware_vendor;
    uint32_t firmware_revision;
    uint32_t _pad0;

    efi_handle_t console_in_handle;
    void *con_in;

    efi_handle_t console_out_handle;
    void *con_out;

    efi_handle_t standard_error_handle;
    void *std_err;

    void *runtime_services;
    efi_boot_services_t *boot_services;

    uint64_t number_of_table_entries;
    void *configuration_table;
};

struct efi_loaded_image_protocol {
    uint32_t revision;
    efi_handle_t parent_handle;
    efi_system_table_t *system_table;
    efi_handle_t device_handle;
    void *file_path;
    void *reserved;

    uint32_t load_options_size;
    void *load_options;

    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;

    efi_status_t (*unload)(efi_handle_t image_handle);
};

struct efi_simple_file_system_protocol {
    uint64_t revision;
    efi_status_t (*open_volume)(efi_simple_file_system_protocol_t *self,
                                efi_file_protocol_t **root);
};

struct efi_file_protocol {
    uint64_t revision;

    efi_status_t (*open)(efi_file_protocol_t *self,
                         efi_file_protocol_t **new_handle,
                         char16_t *file_name,
                         uint64_t open_mode,
                         uint64_t attributes);

    efi_status_t (*close)(efi_file_protocol_t *self);
    efi_status_t (*delete_file)(efi_file_protocol_t *self);

    efi_status_t (*read)(efi_file_protocol_t *self,
                         uint64_t *buffer_size,
                         void *buffer);

    efi_status_t (*write)(efi_file_protocol_t *self,
                          uint64_t *buffer_size,
                          void *buffer);

    efi_status_t (*get_position)(efi_file_protocol_t *self,
                                 uint64_t *position);

    efi_status_t (*set_position)(efi_file_protocol_t *self,
                                 uint64_t position);

    efi_status_t (*get_info)(efi_file_protocol_t *self,
                             efi_guid_t *information_type,
                             uint64_t *buffer_size,
                             void *buffer);

    efi_status_t (*set_info)(efi_file_protocol_t *self,
                             efi_guid_t *information_type,
                             uint64_t buffer_size,
                             void *buffer);

    efi_status_t (*flush)(efi_file_protocol_t *self);
};

static efi_guid_t g_loaded_image_protocol_guid = {
    0x5b1b31a1, 0x9562, 0x11d2,
    {0x8e, 0x3f, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};

static efi_guid_t g_simple_file_system_protocol_guid = {
    0x0964e5b22, 0x6459, 0x11d2,
    {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}
};
#endif

typedef struct {
    bool initialized;
    bool writable;
    bool bootstrap_attempted;
    bool bootstrap_verified;
    vita_storage_backend_t backend;
    char backend_name[32];
    char root_hint[64];
    char last_error[VITA_STORAGE_ERROR_MAX];
    const vita_handoff_t *handoff;
#ifndef VITA_HOSTED
    efi_handle_t uefi_fs_handle;
#endif
} storage_state_t;

static storage_state_t g_storage;

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    unsigned long i;

    if (!ptr) {
        return;
    }

    for (i = 0; i < n; ++i) {
        p[i] = 0;
    }
}

static unsigned long str_len(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;

    if (!a || !b) {
        return false;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }

    return a[i] == b[i];
}

const char *storage_last_error(void) {
    return g_storage.last_error[0] ? g_storage.last_error : "none";
}

static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;

    if (!s || !prefix) {
        return false;
    }

    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_spaces(const char *s) {
    if (!s) {
        return "";
    }

    while (*s && is_space_char(*s)) {
        s++;
    }

    return s;
}

static void str_copy(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;

    if (!dst || cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1) < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

 #ifdef VITA_HOSTED
static void str_append(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;
    unsigned long j = 0;

    if (!dst || cap == 0 || !src) {
        return;
    }

    while (i < cap && dst[i]) {
        i++;
    }

    if (i >= cap) {
        return;
    }

    while (src[j] && (i + 1) < cap) {
        dst[i++] = src[j++];
    }

    dst[i] = '\0';
}
#endif

static void set_error(const char *text) {
    str_copy(g_storage.last_error, sizeof(g_storage.last_error), text ? text : "unknown");
}

static bool path_allowed(const char *path) {
    unsigned long i;

    if (!path) {
        return false;
    }

    if (!(str_eq(path, "/vita") || starts_with(path, "/vita/"))) {
        return false;
    }

    for (i = 0; path[i]; ++i) {
        char c = path[i];

        if (c == ':') {
            return false;
        }

        if (c == '.' && path[i + 1] == '.') {
            return false;
        }

        if ((unsigned char)c < 32U) {
            return false;
        }
    }

    return true;
}

static const char *g_storage_expected_dirs[] = {
    "/vita",
    "/vita/config",
    "/vita/audit",
    "/vita/notes",
    "/vita/messages",
    "/vita/messages/inbox",
    "/vita/messages/outbox",
    "/vita/messages/drafts",
    "/vita/emergency",
    "/vita/emergency/reports",
    "/vita/emergency/checklists",
    "/vita/ai",
    "/vita/ai/prompts",
    "/vita/ai/responses",
    "/vita/ai/sessions",
    "/vita/net",
    "/vita/net/profiles",
    "/vita/export",
    "/vita/export/audit",
    "/vita/export/notes",
    "/vita/export/reports",
    "/vita/tmp"
};

#define STORAGE_EXPECTED_DIR_COUNT (sizeof(g_storage_expected_dirs) / sizeof(g_storage_expected_dirs[0]))

static bool storage_command_probe(void);
static bool storage_tree_repair(void);
static bool storage_tree_check(void);

#ifdef VITA_HOSTED
static bool hosted_ensure_parent_dirs(char *host_path);

static bool hosted_ensure_dir_path(const char *path) {
    char host_path[256];

    if (!path_allowed(path)) {
        return false;
    }

    host_path[0] = '\0';
    str_copy(host_path, sizeof(host_path), "build/storage");
    str_append(host_path, sizeof(host_path), path);
    hosted_ensure_parent_dirs(host_path);
    (void)mkdir(host_path, 0775);
    return true;
}

static bool hosted_path_exists(const char *path) {
    char host_path[256];
    struct stat st;

    if (!path_allowed(path)) {
        return false;
    }

    host_path[0] = '\0';
    str_copy(host_path, sizeof(host_path), "build/storage");
    str_append(host_path, sizeof(host_path), path);

    return stat(host_path, &st) == 0;
}

static bool hosted_ensure_parent_dirs(char *host_path) {
    unsigned long i;

    if (!host_path) {
        return false;
    }

    for (i = 1; host_path[i]; ++i) {
        if (host_path[i] == '/') {
            host_path[i] = '\0';
            (void)mkdir(host_path, 0775);
            host_path[i] = '/';
        }
    }

    return true;
}

static bool hosted_map_path(const char *path, char *out, unsigned long out_cap) {
    if (!path_allowed(path) || !out || out_cap == 0) {
        return false;
    }

    out[0] = '\0';
    str_copy(out, out_cap, "build/storage");
    str_append(out, out_cap, path);

    return out[0] != '\0';
}

static bool hosted_write_text(const char *path, const char *text) {
    char host_path[256];
    FILE *f;

    if (!hosted_map_path(path, host_path, sizeof(host_path))) {
        set_error("invalid hosted storage path");
        return false;
    }

    hosted_ensure_parent_dirs(host_path);

    f = fopen(host_path, "wb");
    if (!f) {
        set_error("hosted fopen failed");
        return false;
    }

    if (text && text[0]) {
        if (fwrite(text, 1, str_len(text), f) != str_len(text)) {
            fclose(f);
            set_error("hosted fwrite failed");
            return false;
        }
    }

    fclose(f);
    set_error("ok");
    return true;
}


static bool hosted_read_text(const char *path, char *out, unsigned long out_cap) {
    char host_path[256];
    FILE *f;
    unsigned long n;

    if (!out || out_cap == 0) {
        set_error("invalid read buffer");
        return false;
    }
    out[0] = '\0';

    if (!hosted_map_path(path, host_path, sizeof(host_path))) {
        set_error("invalid hosted storage path");
        return false;
    }

    f = fopen(host_path, "rb");
    if (!f) {
        set_error("hosted fopen read failed");
        return false;
    }

    n = (unsigned long)fread(out, 1, out_cap - 1, f);
    out[n] = '\0';
    fclose(f);
    set_error("ok");
    return true;
}

static bool hosted_list_notes(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir("build/storage/vita/notes");
    if (!d) {
        console_write_line("notes: no hosted notes directory yet");
        console_write_line("Run: note first.txt, then .save");
        set_error("hosted notes directory missing");
        return false;
    }

    console_write_line("Notes / Notas:");
    while ((de = readdir(d)) != 0) {
        if (str_eq(de->d_name, ".") || str_eq(de->d_name, "..")) {
            continue;
        }
        console_write_line(de->d_name);
        count++;
    }

    closedir(d);

    if (count == 0) {
        console_write_line("(empty / vacio)");
    }

    set_error("ok");
    return true;
}
#else
static bool uefi_path_to_char16(const char *path, char16_t *out, unsigned long out_cap) {
    unsigned long i;
    unsigned long j = 0;

    if (!path_allowed(path) || !out || out_cap == 0) {
        return false;
    }

    out[j++] = '\\';

    i = (path[0] == '/') ? 1U : 0U;
    while (path[i] && (j + 1) < out_cap) {
        out[j++] = (path[i] == '/') ? (char16_t)'\\' : (char16_t)(uint8_t)path[i];
        i++;
    }

    out[j] = 0;
    return true;
}

static bool uefi_open_root(efi_file_protocol_t **out_root) {
    efi_system_table_t *st;
    efi_boot_services_t *bs;
    void *fs_iface = 0;
    efi_handle_t fs_handle = 0;
    efi_simple_file_system_protocol_t *fs;

    if (!out_root) {
        return false;
    }

    *out_root = 0;

    if (!g_storage.handoff || !g_storage.handoff->uefi_image_handle || !g_storage.handoff->uefi_system_table) {
        set_error("missing UEFI handoff");
        return false;
    }

    st = (efi_system_table_t *)g_storage.handoff->uefi_system_table;
    if (!st || !st->boot_services || !st->boot_services->handle_protocol) {
        set_error("missing UEFI boot services");
        return false;
    }

    bs = st->boot_services;

    fs_handle = g_storage.uefi_fs_handle;
    if (!fs_handle) {
        void *loaded_iface = 0;
        efi_loaded_image_protocol_t *loaded;

        if (bs->handle_protocol((efi_handle_t)g_storage.handoff->uefi_image_handle,
                                &g_loaded_image_protocol_guid,
                                &loaded_iface) != EFI_SUCCESS) {
            set_error("LoadedImage protocol unavailable");
            return false;
        }

        loaded = (efi_loaded_image_protocol_t *)loaded_iface;
        if (!loaded || !loaded->device_handle) {
            set_error("LoadedImage device unavailable");
            return false;
        }

        fs_handle = loaded->device_handle;
    }

    if (bs->handle_protocol(fs_handle,
                            &g_simple_file_system_protocol_guid,
                            &fs_iface) != EFI_SUCCESS) {
        set_error("SimpleFileSystem protocol unavailable");
        return false;
    }

    fs = (efi_simple_file_system_protocol_t *)fs_iface;
    if (!fs || !fs->open_volume || fs->open_volume(fs, out_root) != EFI_SUCCESS || !*out_root) {
        set_error("open_volume failed");
        return false;
    }

    return true;
}

static bool uefi_try_select_writable_fs(void) {
    static const char *probe_path = "/vita/tmp/boot-storage-fs-probe.txt";
    static const char *probe_text = "vita_storage_uefi_fs_probe_v1\n";
    static char readback[VITA_STORAGE_READ_MAX];
    efi_system_table_t *st;
    efi_boot_services_t *bs;
    efi_handle_t handles[64];
    uint64_t size = sizeof(handles);
    uint64_t i;

    if (!g_storage.handoff || !g_storage.handoff->uefi_system_table) {
        set_error("missing UEFI handoff");
        return false;
    }

    st = (efi_system_table_t *)g_storage.handoff->uefi_system_table;
    if (!st || !st->boot_services || !st->boot_services->locate_handle || !st->boot_services->handle_protocol) {
        set_error("missing UEFI boot services");
        return false;
    }
    bs = st->boot_services;

    if (bs->locate_handle(EFI_LOCATE_BY_PROTOCOL,
                          &g_simple_file_system_protocol_guid,
                          0,
                          &size,
                          handles) != EFI_SUCCESS) {
        set_error("UEFI locate_handle(SimpleFileSystem) failed");
        return false;
    }

    for (i = 0; i < (size / sizeof(efi_handle_t)); ++i) {
        g_storage.uefi_fs_handle = handles[i];

        if (!storage_tree_repair()) {
            continue;
        }

        if (!storage_write_text(probe_path, probe_text)) {
            continue;
        }

        if (!storage_read_text(probe_path, readback, sizeof(readback))) {
            continue;
        }

        if (!str_eq(readback, probe_text)) {
            continue;
        }

        set_error("ok");
        return true;
    }

    g_storage.uefi_fs_handle = 0;
    set_error("UEFI no writable SimpleFileSystem candidate");
    return false;
}

static bool uefi_open_path(efi_file_protocol_t *root,
                           const char *path,
                           uint64_t mode,
                           uint64_t attrs,
                           efi_file_protocol_t **out_file) {
    char16_t path16[VITA_STORAGE_PATH_MAX];

    if (!root || !out_file) {
        return false;
    }
    *out_file = 0;

    if (!uefi_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI storage path");
        return false;
    }

    if (!root->open || root->open(root, out_file, path16, mode, attrs) != EFI_SUCCESS || !*out_file) {
        return false;
    }

    return true;
}

static bool uefi_path_exists(const char *path) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *f = 0;
    bool exists = false;

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (uefi_open_path(root, path, EFI_FILE_MODE_READ, 0, &f)) {
        exists = true;
        if (f->close) {
            f->close(f);
        }
    }

    if (root->close) {
        root->close(root);
    }

    return exists;
}

static bool uefi_create_directory(const char *path) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *dir = 0;
    bool ok = false;

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (uefi_open_path(root,
                       path,
                       EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                       EFI_FILE_DIRECTORY,
                       &dir)) {
        ok = true;
        if (dir->close) {
            dir->close(dir);
        }
    }

    if (root->close) {
        root->close(root);
    }

    if (!ok) {
        set_error("UEFI directory create failed");
    } else {
        set_error("ok");
    }

    return ok;
}

static bool uefi_ensure_directory(const char *path) {
    if (uefi_path_exists(path)) {
        set_error("ok");
        return true;
    }

    return uefi_create_directory(path);
}

static bool uefi_ensure_parent_dirs(const char *file_path) {
    char dir[VITA_STORAGE_PATH_MAX];
    unsigned long i = 0;
    unsigned long last_slash = 0;

    if (!file_path || !starts_with(file_path, "/vita/")) {
        return false;
    }

    while (file_path[i] && (i + 1) < sizeof(dir)) {
        dir[i] = file_path[i];
        if (file_path[i] == '/') {
            last_slash = i;
        }
        i++;
    }
    dir[i] = '\0';

    if (last_slash == 0) {
        return true;
    }

    for (i = 1; dir[i]; ++i) {
        if (dir[i] == '/') {
            char keep = dir[i];
            dir[i] = '\0';
            if (!uefi_ensure_directory(dir)) {
                dir[i] = keep;
                return false;
            }
            dir[i] = keep;
        }
    }

    dir[last_slash] = '\0';
    if (dir[0]) {
        return uefi_ensure_directory(dir);
    }

    return true;
}

static void uefi_delete_if_exists(efi_file_protocol_t *root, char16_t *path16) {
    efi_file_protocol_t *file = 0;

    if (!root || !root->open || !path16) {
        return;
    }

    if (root->open(root,
                   &file,
                   path16,
                   EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                   0) == EFI_SUCCESS && file) {
        if (file->delete_file) {
            (void)file->delete_file(file);
        } else if (file->close) {
            file->close(file);
        }
    }
}

static bool uefi_write_text(const char *path, const char *text) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *file = 0;
    char16_t path16[VITA_STORAGE_PATH_MAX];
    uint64_t size;
    bool ok = false;

    if (!uefi_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI storage path");
        return false;
    }

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (!uefi_ensure_parent_dirs(path)) {
        if (root->close) {
            root->close(root);
        }
        set_error("UEFI ensure parent dirs failed");
        return false;
    }

    uefi_delete_if_exists(root, path16);

    if (!root->open || root->open(root,
                                  &file,
                                  path16,
                                  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                  0) != EFI_SUCCESS || !file) {
        set_error("UEFI file open/create failed; parent directory may be missing");
        if (root->close) {
            root->close(root);
        }
        return false;
    }

    size = text ? str_len(text) : 0;
    if (file->write && file->write(file, &size, (void *)(text ? text : "")) == EFI_SUCCESS) {
        ok = true;
        set_error("ok");
    } else {
        set_error("UEFI file write failed");
    }

    if (file->close) {
        file->close(file);
    }
    if (root->close) {
        root->close(root);
    }

    return ok;
}

static void char16_to_ascii(const char16_t *src, char *dst, unsigned long dst_cap) {
    unsigned long i = 0;

    if (!dst || dst_cap == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    while (src[i] && (i + 1) < dst_cap) {
        char16_t ch = src[i];
        dst[i] = (ch < 128U) ? (char)ch : '?';
        i++;
    }

    dst[i] = '\0';
}

static bool uefi_read_text(const char *path, char *out, unsigned long out_cap) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *file = 0;
    char16_t path16[VITA_STORAGE_PATH_MAX];
    uint64_t size;
    bool ok = false;

    if (!out || out_cap == 0) {
        set_error("invalid read buffer");
        return false;
    }
    out[0] = '\0';

    if (!uefi_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI storage path");
        return false;
    }

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (!root->open || root->open(root, &file, path16, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS || !file) {
        set_error("UEFI file open read failed");
        if (root->close) {
            root->close(root);
        }
        return false;
    }

    size = (uint64_t)(out_cap - 1U);
    if (file->read && file->read(file, &size, out) == EFI_SUCCESS) {
        out[(unsigned long)size] = '\0';
        ok = true;
        set_error("ok");
    } else {
        set_error("UEFI file read failed");
    }

    if (file->close) {
        file->close(file);
    }
    if (root->close) {
        root->close(root);
    }

    return ok;
}

static bool uefi_open_path_read(const char *path, efi_file_protocol_t **out_file) {
    efi_file_protocol_t *root = 0;
    char16_t path16[VITA_STORAGE_PATH_MAX];

    if (!out_file) {
        return false;
    }
    *out_file = 0;

    if (!uefi_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI storage path");
        return false;
    }

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (!root->open || root->open(root, out_file, path16, EFI_FILE_MODE_READ, 0) != EFI_SUCCESS || !*out_file) {
        set_error("UEFI directory open failed");
        if (root->close) {
            root->close(root);
        }
        return false;
    }

    if (root->close) {
        root->close(root);
    }
    return true;
}

static bool uefi_list_notes(void) {
    static uint8_t info_buffer[1024];
    efi_file_protocol_t *dir = 0;
    int count = 0;

    if (!uefi_open_path_read("/vita/notes", &dir)) {
        console_write_line("notes: /vita/notes unavailable");
        console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
        return false;
    }

    console_write_line("Notes / Notas:");

    for (;;) {
        uint64_t size = sizeof(info_buffer);
        efi_file_info_t *info;
        char name[96];

        mem_zero(info_buffer, sizeof(info_buffer));

        if (!dir->read || dir->read(dir, &size, info_buffer) != EFI_SUCCESS) {
            set_error("UEFI directory read failed");
            if (dir->close) {
                dir->close(dir);
            }
            return false;
        }

        if (size == 0) {
            break;
        }

        info = (efi_file_info_t *)info_buffer;
        char16_to_ascii(info->file_name, name, sizeof(name));

        if (!name[0] || str_eq(name, ".") || str_eq(name, "..")) {
            continue;
        }

        if ((info->attribute & EFI_FILE_DIRECTORY) != 0ULL) {
            continue;
        }

        console_write_line(name);
        count++;
    }

    if (dir->close) {
        dir->close(dir);
    }

    if (count == 0) {
        console_write_line("(empty / vacio)");
    }

    set_error("ok");
    return true;
}

#endif

static bool storage_ensure_directory(const char *path) {
#ifdef VITA_HOSTED
    if (!hosted_ensure_dir_path(path)) {
        set_error("hosted ensure directory failed");
        return false;
    }
    set_error("ok");
    return true;
#else
    return uefi_ensure_directory(path);
#endif
}

static bool storage_path_exists(const char *path) {
#ifdef VITA_HOSTED
    return hosted_path_exists(path);
#else
    return uefi_path_exists(path);
#endif
}

bool storage_init(const vita_handoff_t *handoff) {
    mem_zero(&g_storage, sizeof(g_storage));
    g_storage.handoff = handoff;

#ifdef VITA_HOSTED
    g_storage.initialized = true;
    g_storage.writable = true;
    g_storage.backend = VITA_STORAGE_BACKEND_HOSTED;
    str_copy(g_storage.backend_name, sizeof(g_storage.backend_name), "hosted_fs");
    str_copy(g_storage.root_hint, sizeof(g_storage.root_hint), "build/storage/vita");
    set_error("ok");
    return true;
#else
    if (!handoff || handoff->firmware_type != VITA_FIRMWARE_UEFI || !handoff->uefi_image_handle || !handoff->uefi_system_table) {
        g_storage.initialized = false;
        g_storage.writable = false;
        g_storage.backend = VITA_STORAGE_BACKEND_NONE;
        str_copy(g_storage.backend_name, sizeof(g_storage.backend_name), "none");
        str_copy(g_storage.root_hint, sizeof(g_storage.root_hint), "/vita");
        set_error("UEFI storage handoff unavailable");
        return false;
    }

    g_storage.initialized = true;
    g_storage.writable = true;
    g_storage.backend = VITA_STORAGE_BACKEND_UEFI_SIMPLE_FS;
    str_copy(g_storage.backend_name, sizeof(g_storage.backend_name), "uefi_simple_fs");
    str_copy(g_storage.root_hint, sizeof(g_storage.root_hint), "/vita");
    set_error("ok");
    return true;
#endif
}

bool storage_is_ready(void) {
    return g_storage.initialized && g_storage.writable;
}

bool storage_is_bootstrap_verified(void) {
    return g_storage.bootstrap_attempted && g_storage.bootstrap_verified;
}

void storage_get_status(vita_storage_status_t *out_status) {
    unsigned long i;

    if (!out_status) {
        return;
    }

    out_status->initialized = g_storage.initialized;
    out_status->writable = g_storage.writable;
    out_status->bootstrap_attempted = g_storage.bootstrap_attempted;
    out_status->bootstrap_verified = g_storage.bootstrap_verified;
    out_status->backend = g_storage.backend;

    for (i = 0; i < sizeof(out_status->backend_name); ++i) {
        out_status->backend_name[i] = g_storage.backend_name[i];
    }
    for (i = 0; i < sizeof(out_status->root_hint); ++i) {
        out_status->root_hint[i] = g_storage.root_hint[i];
    }
    for (i = 0; i < sizeof(out_status->last_error); ++i) {
        out_status->last_error[i] = g_storage.last_error[i];
    }
}

void storage_show_status(void) {
    console_write_line("Storage / Almacenamiento:");
    console_write_line(g_storage.initialized ? "initialized: yes" : "initialized: no");
    console_write_line(g_storage.writable ? "writable: yes" : "writable: no");
    console_write_line(g_storage.bootstrap_attempted ? "bootstrap_attempted: yes" : "bootstrap_attempted: no");
    console_write_line(g_storage.bootstrap_verified ? "storage: verified writable" : "storage: unavailable or unverified");
    console_write_line(g_storage.bootstrap_verified ? "storage bootstrap: verified" : "storage bootstrap: failed");
    console_write_line("backend:");
    console_write_line(g_storage.backend_name[0] ? g_storage.backend_name : "none");
    console_write_line("root_hint:");
    console_write_line(g_storage.root_hint[0] ? g_storage.root_hint : "/vita");
    console_write_line("last_error:");
    console_write_line(g_storage.last_error[0] ? g_storage.last_error : "none");
    console_write_line("Commands / Comandos:");
    console_write_line("storage status");
    console_write_line("storage test");
    console_write_line("storage write /vita/notes/test.txt hello from VitaOS");
    console_write_line("storage read /vita/notes/test.txt");
    console_write_line("cat /vita/notes/test.txt");
    console_write_line("storage repair | storage check | storage probe");
    console_write_line("storage last-error");
    console_write_line("notes list");
}

bool storage_write_text(const char *path, const char *text) {
    if (!g_storage.initialized || !g_storage.writable) {
        set_error("storage not initialized or not writable");
        return false;
    }

    if (!path_allowed(path)) {
        set_error("path must be /vita or /vita/... and must not contain '..' or ':'");
        return false;
    }

#ifdef VITA_HOSTED
    return hosted_write_text(path, text ? text : "");
#else
    return uefi_write_text(path, text ? text : "");
#endif
}

bool storage_read_text(const char *path, char *out, unsigned long out_cap) {
    if (!g_storage.initialized) {
        set_error("storage not initialized");
        return false;
    }

    if (!out || out_cap == 0) {
        set_error("invalid read buffer");
        return false;
    }
    out[0] = '\0';

    if (!path_allowed(path)) {
        set_error("path must be /vita or /vita/... and must not contain '..' or ':'");
        return false;
    }

#ifdef VITA_HOSTED
    return hosted_read_text(path, out, out_cap);
#else
    return uefi_read_text(path, out, out_cap);
#endif
}

bool storage_list_notes(void) {
    if (!g_storage.initialized) {
        set_error("storage not initialized");
        console_write_line("notes: storage not initialized");
        return false;
    }

#ifdef VITA_HOSTED
    return hosted_list_notes();
#else
    return uefi_list_notes();
#endif
}

bool storage_bootstrap_persistent_tree(void) {
    static const char *verify_path = "/vita/tmp/boot-storage-verify.txt";
    static const char *verify_text = "vita_storage_bootstrap_verify_v1\n";
    char readback[VITA_STORAGE_READ_MAX];

    g_storage.bootstrap_attempted = true;
    g_storage.bootstrap_verified = false;

    if (!g_storage.initialized || !g_storage.writable) {
        set_error("bootstrap failed: storage unavailable");
        return false;
    }

#ifndef VITA_HOSTED
    if (!uefi_try_select_writable_fs()) {
        set_error("bootstrap failed: no writable UEFI FAT filesystem");
        return false;
    }
#endif

    if (!storage_tree_repair()) {
        set_error("bootstrap failed: storage_tree_repair");
        return false;
    }

    if (!storage_tree_check()) {
        set_error("bootstrap failed: storage_tree_check");
        return false;
    }

    if (!storage_write_text(verify_path, verify_text)) {
        set_error("bootstrap failed: verify marker write");
        return false;
    }

    if (!storage_read_text(verify_path, readback, sizeof(readback))) {
        set_error("bootstrap failed: verify marker read");
        return false;
    }

    if (!str_eq(readback, verify_text)) {
        set_error("bootstrap failed: verify marker compare");
        return false;
    }

    g_storage.bootstrap_verified = true;
    set_error("ok");
    return true;
}

bool storage_bootstrap_persistent_tree(void) {
    static const char *verify_path = "/vita/tmp/boot-storage-verify.txt";
    static const char *verify_text = "vita_storage_bootstrap_verify_v1\n";
    char readback[VITA_STORAGE_READ_MAX];

    g_storage.bootstrap_attempted = true;
    g_storage.bootstrap_verified = false;

    if (!g_storage.initialized || !g_storage.writable) {
        set_error("bootstrap failed: storage unavailable");
        return false;
    }

    if (!storage_tree_repair()) {
        set_error("bootstrap failed: storage_tree_repair");
        return false;
    }

    if (!storage_tree_check()) {
        set_error("bootstrap failed: storage_tree_check");
        return false;
    }

    if (!storage_write_text(verify_path, verify_text)) {
        set_error("bootstrap failed: verify marker write");
        return false;
    }

    if (!storage_read_text(verify_path, readback, sizeof(readback))) {
        set_error("bootstrap failed: verify marker read");
        return false;
    }

    if (!str_eq(readback, verify_text)) {
        set_error("bootstrap failed: verify marker compare");
        return false;
    }

    g_storage.bootstrap_verified = true;
    set_error("ok");
    return true;
}

static bool storage_command_test(void) {
    static const char *path = "/vita/tmp/storage-test.txt";
    static const char *text = "vita_storage_test_v1\n";
    char readback[VITA_STORAGE_READ_MAX];

    if (!storage_bootstrap_persistent_tree()) {
        console_write_line("storage test: bootstrap failed");
        console_write_line(storage_last_error());
        return false;
    }

    if (!storage_write_text(path, text)) {
        console_write_line("storage test: write failed");
        console_write_line(storage_last_error());
        return false;
    }

    if (!storage_read_text(path, readback, sizeof(readback))) {
        console_write_line("storage test: read failed");
        console_write_line(storage_last_error());
        return false;
    }

    if (!str_eq(readback, text)) {
        console_write_line("storage test: verify failed");
        console_write_line("write/read mismatch");
        return false;
    }

    console_write_line("storage test: VERIFIED");
    console_write_line(path);
    return true;
}

static bool storage_command_write(const char *args) {
    char path[VITA_STORAGE_PATH_MAX];
    char text[VITA_STORAGE_TEXT_MAX];
    unsigned long i = 0;
    unsigned long j = 0;
    const char *p;

    p = skip_spaces(args);
    if (!p[0]) {
        console_write_line("usage: storage write /vita/notes/test.txt text");
        return false;
    }

    while (p[i] && !is_space_char(p[i]) && (i + 1) < sizeof(path)) {
        path[i] = p[i];
        i++;
    }
    path[i] = '\0';

    p = skip_spaces(p + i);
    if (!p[0]) {
        console_write_line("storage write: missing text");
        return false;
    }

    while (p[j] && (j + 2) < sizeof(text)) {
        text[j] = p[j];
        j++;
    }
    text[j++] = '\n';
    text[j] = '\0';

    if (storage_write_text(path, text)) {
        console_write_line("storage write: written");
        console_write_line(path);
        return true;
    }

    console_write_line("storage write: failed");
    console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
    return false;
}

static bool storage_command_read(const char *args) {
    char path[VITA_STORAGE_PATH_MAX];
    char text[VITA_STORAGE_READ_MAX];
    unsigned long i = 0;
    const char *p;

    p = skip_spaces(args);
    if (!p[0]) {
        console_write_line("usage: storage read /vita/notes/test.txt");
        return false;
    }

    while (p[i] && !is_space_char(p[i]) && (i + 1) < sizeof(path)) {
        path[i] = p[i];
        i++;
    }
    path[i] = '\0';

    if (!storage_read_text(path, text, sizeof(text))) {
        console_write_line("storage read: failed");
        console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
        return false;
    }

    console_write_line("--- file / archivo ---");
    console_write_line(text[0] ? text : "(empty / vacio)");
    console_write_line("--- end / fin ---");
    return true;
}

static bool storage_command_list_notes(void) {
    return storage_list_notes();
}

static bool storage_tree_repair(void) {
    unsigned long i;
    bool ok = true;

    for (i = 0; i < STORAGE_EXPECTED_DIR_COUNT; ++i) {
        if (!storage_ensure_directory(g_storage_expected_dirs[i])) {
            ok = false;
            break;
        }
    }

    return ok;
}

static bool storage_tree_check(void) {
    unsigned long i;
    bool ok = true;

    for (i = 0; i < STORAGE_EXPECTED_DIR_COUNT; ++i) {
        if (!storage_path_exists(g_storage_expected_dirs[i])) {
            set_error("storage tree missing expected directory");
            console_write_line("missing:");
            console_write_line(g_storage_expected_dirs[i]);
            ok = false;
            break;
        }
    }

    if (ok) {
        set_error("ok");
    }

    return ok;
}

static bool storage_command_probe(void) {
    static const char *path = "/vita/tmp/storage-probe.txt";
    static const char *text = "vita_storage_probe_v1\n";
    char readback[VITA_STORAGE_READ_MAX];

    console_write_line("storage probe: status/backend");
    storage_show_status();

    console_write_line("storage probe: repair/check");
    if (!storage_tree_repair()) {
        console_write_line("storage probe: repair failed");
        console_write_line(storage_last_error());
        return false;
    }
    if (!storage_tree_check()) {
        console_write_line("storage probe: check failed");
        console_write_line(storage_last_error());
        return false;
    }

    console_write_line("storage probe: create/write");
    if (!storage_write_text(path, text)) {
        console_write_line("storage probe: write failed");
        console_write_line(storage_last_error());
        return false;
    }

    console_write_line("storage probe: read");
    if (!storage_read_text(path, readback, sizeof(readback))) {
        console_write_line("storage probe: read failed");
        console_write_line(storage_last_error());
        return false;
    }

    console_write_line("storage probe: compare");
    if (!str_eq(readback, text)) {
        console_write_line("storage probe: compare failed");
        console_write_line("write/read mismatch");
        set_error("write/read verify failed");
        console_write_line(storage_last_error());
        return false;
    }

    console_write_line("storage probe: ok");
    console_write_line(path);
    set_error("ok");
    return true;
}


bool storage_handle_command(const char *cmd) {
    const char *args;
    bool storage_family;

    if (!cmd) {
        return false;
    }

    storage_family =
        starts_with(cmd, "storage") ||
        starts_with(cmd, "cat ") ||
        str_eq(cmd, "notes list");

    if (str_eq(cmd, "storage") || str_eq(cmd, "storage status")) {
        storage_show_status();
        return true;
    }

    if (str_eq(cmd, "storage test")) {
        return storage_command_test();
    }

    if (str_eq(cmd, "storage probe")) {
        return storage_command_probe();
    }

    if (starts_with(cmd, "storage write ")) {
        args = cmd + str_len("storage write ");
        return storage_command_write(args);
    }

    if (starts_with(cmd, "storage read ")) {
        args = cmd + str_len("storage read ");
        return storage_command_read(args);
    }

    if (starts_with(cmd, "storage cat ")) {
        args = cmd + str_len("storage cat ");
        return storage_command_read(args);
    }

    if (starts_with(cmd, "cat ")) {
        args = cmd + str_len("cat ");
        return storage_command_read(args);
    }

    if (str_eq(cmd, "storage notes") || str_eq(cmd, "storage notes list") || str_eq(cmd, "notes list")) {
        return storage_command_list_notes();
    }

    if (str_eq(cmd, "storage tree")) {
        unsigned long i;
        console_write_line("/vita tree expected:");
        for (i = 0; i < STORAGE_EXPECTED_DIR_COUNT; ++i) {
            console_write_line(g_storage_expected_dirs[i]);
        }
        return true;
    }

    if (str_eq(cmd, "storage last-error")) {
        console_write_line(storage_last_error());
        return true;
    }

    if (str_eq(cmd, "storage repair") || str_eq(cmd, "storage init-tree") || str_eq(cmd, "storage mkdirs")) {
        if (storage_tree_repair()) {
            console_write_line("storage repair: ok");
            return true;
        }
        console_write_line("storage repair: failed");
        return false;
    }

    if (str_eq(cmd, "storage check") || str_eq(cmd, "storage validate") || str_eq(cmd, "storage verify")) {
        if (storage_tree_check()) {
            console_write_line("storage check: ok");
            return true;
        }
        console_write_line("storage check: missing tree entries (run storage repair)");
        return false;
    }

    if (str_eq(cmd, "storage bootstrap")) {
        if (storage_bootstrap_persistent_tree()) {
            console_write_line("storage bootstrap: verified");
            return true;
        }
        console_write_line("storage bootstrap: failed");
        console_write_line(storage_last_error());
        return false;
    }

    if (!storage_family) {
        return false;
    }

    console_write_line("Unknown storage command / Comando storage desconocido");
    console_write_line("Use: storage, storage status, storage test, storage write /vita/notes/test.txt text");
    console_write_line("     storage read /vita/notes/test.txt, cat /vita/notes/test.txt, notes list");
    return false;
}
