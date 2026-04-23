/*
 * kernel/hardware_discovery.c
 * Minimal F1A hardware discovery for the current slice.
 */

#include <vita/hw.h>

#ifdef VITA_HOSTED
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#endif

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) {
        p[i] = 0;
    }
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

    while (src[i] != '\0' && (i + 1) < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

#ifdef VITA_HOSTED
static uint64_t unix_now(void) {
    return (uint64_t)time(NULL);
}

static bool filter_not_dot(const struct dirent *de) {
    return strcmp(de->d_name, ".") != 0 && strcmp(de->d_name, "..") != 0;
}

static bool filter_net(const struct dirent *de) {
    return filter_not_dot(de) && strcmp(de->d_name, "lo") != 0;
}

static bool filter_storage(const struct dirent *de) {
    return filter_not_dot(de) &&
           strncmp(de->d_name, "loop", 4) != 0 &&
           strncmp(de->d_name, "ram", 3) != 0;
}

static bool filter_usb(const struct dirent *de) {
    return filter_not_dot(de) && strchr(de->d_name, '-') != NULL;
}

static int count_dir_entries(const char *path, bool (*filter)(const struct dirent *)) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir(path);
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (filter && !filter(de)) {
            continue;
        }
        count++;
    }

    closedir(d);
    return count;
}

static int count_wifi_ifaces(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;
    char path[256];

    d = opendir("/sys/class/net");
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        DIR *w;

        if (!filter_net(de)) {
            continue;
        }

        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", de->d_name);
        w = opendir(path);
        if (w) {
            count++;
            closedir(w);
        }
    }

    closedir(d);
    return count;
}

static void read_cpu_model(char out[128]) {
    FILE *f;
    char line[256];

    f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        out[0] = '\0';
        return;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                size_t n;

                colon += 2;
                snprintf(out, 128, "%s", colon);
                n = strlen(out);

                while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
                    out[--n] = '\0';
                }

                fclose(f);
                return;
            }
        }
    }

    fclose(f);
    out[0] = '\0';
}

static uint64_t read_ram_bytes(void) {
    FILE *f;
    char line[256];

    f = fopen("/proc/meminfo", "r");
    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            unsigned long long kb = 0;
            if (sscanf(line, "MemTotal: %llu kB", &kb) == 1) {
                fclose(f);
                return kb * 1024ULL;
            }
        }
    }

    fclose(f);
    return 0;
}
#endif

bool hw_discovery_run(const vita_handoff_t *handoff, vita_hw_snapshot_t *out_snapshot) {
    if (!out_snapshot) {
        return false;
    }

    mem_zero(out_snapshot, sizeof(*out_snapshot));

#ifdef VITA_HOSTED
    {
        struct utsname un;

        mem_zero(&un, sizeof(un));
        if (uname(&un) == 0) {
            str_copy(out_snapshot->cpu_arch, sizeof(out_snapshot->cpu_arch), un.machine);
        } else {
            str_copy(out_snapshot->cpu_arch,
                     sizeof(out_snapshot->cpu_arch),
                     (handoff && handoff->arch_name) ? handoff->arch_name : "unknown");
        }
    }

    read_cpu_model(out_snapshot->cpu_model);
    out_snapshot->ram_bytes = read_ram_bytes();

    str_copy(out_snapshot->firmware_type,
             sizeof(out_snapshot->firmware_type),
             (handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED) ? "hosted" : "uefi");

    str_copy(out_snapshot->console_type,
             sizeof(out_snapshot->console_type),
             (handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED) ? "stdio" : "uefi_text");

    out_snapshot->net_count = count_dir_entries("/sys/class/net", filter_net);
    out_snapshot->storage_count = count_dir_entries("/sys/block", filter_storage);
    out_snapshot->usb_count = count_dir_entries("/sys/bus/usb/devices", filter_usb);
    out_snapshot->wifi_count = count_wifi_ifaces();
    out_snapshot->detected_at_unix = unix_now();
#else
    str_copy(out_snapshot->cpu_arch,
             sizeof(out_snapshot->cpu_arch),
             (handoff && handoff->arch_name) ? handoff->arch_name : "unknown");
    str_copy(out_snapshot->firmware_type, sizeof(out_snapshot->firmware_type), "uefi");
    str_copy(out_snapshot->console_type, sizeof(out_snapshot->console_type), "uefi_text");
    out_snapshot->detected_at_unix = 0;
#endif

    return true;
}
