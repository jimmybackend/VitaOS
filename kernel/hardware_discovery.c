/*
 * kernel/hardware_discovery.c
 * Minimal F1A/F1B hardware discovery for the current slice.
 *
 * Scope note:
 * - Hosted builds use the host OS (/proc and /sys) for validation.
 * - UEFI builds report firmware/PCI-visible capabilities only.
 * - Mouse/network counts in UEFI mean "protocol/device presence detected";
 *   they do not imply a VitaOS mouse driver or network stack is active yet.
 */

#include <vita/hw.h>
#include <vita/pci.h>

#ifdef VITA_HOSTED
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>
#else
#define EFI_SUCCESS 0ULL

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
    void *handle_protocol;
    void *reserved;
    void *register_protocol_notify;
    void *locate_handle;
    void *locate_device_path;
    void *install_configuration_table;

    void *load_image;
    void *start_image;
    void *exit;
    void *unload_image;
    void *exit_boot_services;

    void *get_next_monotonic_count;
    void *stall;
    void *set_watchdog_timer;

    void *connect_controller;
    void *disconnect_controller;

    void *open_protocol;
    void *close_protocol;
    void *open_protocol_information;
    void *protocols_per_handle;
    void *locate_handle_buffer;

    efi_status_t (*locate_protocol)(efi_guid_t *protocol,
                                    void *registration,
                                    void **interface);

    void *install_multiple_protocol_interfaces;
    void *uninstall_multiple_protocol_interfaces;
    void *calculate_crc32;
    void *copy_mem;
    void *set_mem;
    void *create_event_ex;
};

typedef struct {
    efi_table_header_t hdr;
    void *firmware_vendor;
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
} efi_system_table_t;

static efi_guid_t g_simple_pointer_protocol_guid = {
    0x31878c87U, 0x0b75U, 0x11d5U,
    {0x9aU, 0x4fU, 0x00U, 0x90U, 0x27U, 0x3fU, 0xc1U, 0x4dU}
};

static efi_guid_t g_absolute_pointer_protocol_guid = {
    0x8d59d32bU, 0xc655U, 0x4ae9U,
    {0x9bU, 0x15U, 0xf2U, 0x59U, 0x04U, 0x99U, 0x2aU, 0x43U}
};

static efi_guid_t g_simple_network_protocol_guid = {
    0xa19832b9U, 0xac25U, 0x11d3U,
    {0x9aU, 0x2dU, 0x00U, 0x90U, 0x27U, 0x3fU, 0xc1U, 0x4dU}
};
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

static int int_max(int a, int b) {
    return (a > b) ? a : b;
}

#ifndef VITA_HOSTED
static bool uefi_protocol_present(const vita_handoff_t *handoff, efi_guid_t *protocol) {
    efi_system_table_t *st;
    void *interface = 0;

    if (!handoff || !protocol || handoff->firmware_type != VITA_FIRMWARE_UEFI) {
        return false;
    }

    st = (efi_system_table_t *)handoff->uefi_system_table;
    if (!st || !st->boot_services || !st->boot_services->locate_protocol) {
        return false;
    }

    return st->boot_services->locate_protocol(protocol, 0, &interface) == EFI_SUCCESS && interface != 0;
}

static void apply_uefi_protocol_snapshot(const vita_handoff_t *handoff,
                                         vita_hw_snapshot_t *out_snapshot) {
    bool pointer_present;
    bool network_present;

    if (!out_snapshot) {
        return;
    }

    /*
     * Protocol presence only. This is not a VitaOS input driver yet, but it
     * tells the guided console that firmware exposes a pointer-capable path.
     */
    pointer_present = uefi_protocol_present(handoff, &g_simple_pointer_protocol_guid) ||
                      uefi_protocol_present(handoff, &g_absolute_pointer_protocol_guid);
    if (pointer_present) {
        out_snapshot->mouse_count = int_max(out_snapshot->mouse_count, 1);
    }

    /*
     * Protocol presence only. This does not mean DHCP, TCP/IP, Wi-Fi auth, or
     * a VitaOS network stack is active. It is a safe F1B readiness signal.
     */
    network_present = uefi_protocol_present(handoff, &g_simple_network_protocol_guid);
    if (network_present) {
        out_snapshot->net_count = int_max(out_snapshot->net_count, 1);
    }
}
#endif

/*
 * Keep the PCI snapshot out of the UEFI stack.
 * The Windows/UEFI target emits __chkstk for large stack frames,
 * but VitaOS is freestanding and does not link a host runtime.
 */
static vita_pci_snapshot_t g_pci_snapshot;

static void apply_pci_snapshot(vita_hw_snapshot_t *out_snapshot) {
    vita_pci_snapshot_t *pci = &g_pci_snapshot;
    int pci_net_count;

    if (!out_snapshot) {
        return;
    }

    mem_zero(pci, sizeof(*pci));

    if (!pci_discovery_run(pci)) {
        return;
    }

    /*
     * PCI discovery provides controller/class visibility. Do not add these
     * blindly to hosted OS-visible counts, otherwise net/storage/display can
     * be double-counted. Use PCI to fill typed/controller counters and as a
     * lower bound for aggregate counters when the hosted view is unavailable.
     */
    out_snapshot->display_count = int_max(out_snapshot->display_count, (int)pci->display_count);
    out_snapshot->ethernet_count = int_max(out_snapshot->ethernet_count, (int)pci->ethernet_count);
    out_snapshot->wifi_count = int_max(out_snapshot->wifi_count, (int)pci->wifi_count);
    out_snapshot->usb_controller_count = int_max(out_snapshot->usb_controller_count,
                                                 (int)pci->usb_controller_count);
    out_snapshot->audio_count = int_max(out_snapshot->audio_count, (int)pci->audio_count);

    if (out_snapshot->storage_count == 0) {
        out_snapshot->storage_count = (int)pci->storage_count;
    }

    pci_net_count = (int)(pci->ethernet_count + pci->wifi_count);
    out_snapshot->net_count = int_max(out_snapshot->net_count, pci_net_count);

    /*
     * F1A/F1B limitation:
     * PCI can identify audio controllers, but microphone presence is not
     * reliable without codec/USB/audio-class enumeration. Keep this as 0.
     */
    out_snapshot->microphone_count = 0;
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

static bool filter_input_event(const struct dirent *de) {
    return filter_not_dot(de) && strncmp(de->d_name, "event", 5) == 0;
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

static bool net_iface_has_wireless_dir(const char *iface_name) {
    DIR *w;
    char path[256];

    if (!iface_name) {
        return false;
    }

    snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", iface_name);
    w = opendir(path);
    if (!w) {
        return false;
    }

    closedir(w);
    return true;
}

static int count_wifi_ifaces(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir("/sys/class/net");
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (!filter_net(de)) {
            continue;
        }

        if (net_iface_has_wireless_dir(de->d_name)) {
            count++;
        }
    }

    closedir(d);
    return count;
}

static int count_ethernet_ifaces(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir("/sys/class/net");
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (!filter_net(de)) {
            continue;
        }

        if (!net_iface_has_wireless_dir(de->d_name)) {
            count++;
        }
    }

    closedir(d);
    return count;
}

static bool input_device_has_token(const char *event_name, const char *token) {
    FILE *f;
    char path[256];
    char line[256];
    bool found = false;

    if (!event_name || !token) {
        return false;
    }

    snprintf(path, sizeof(path), "/sys/class/input/%s/device/name", event_name);
    f = fopen(path, "r");
    if (!f) {
        return false;
    }

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, token)) {
            found = true;
            break;
        }
    }

    fclose(f);
    return found;
}

static int count_keyboards(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir("/sys/class/input");
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (!filter_input_event(de)) {
            continue;
        }

        if (input_device_has_token(de->d_name, "keyboard") ||
            input_device_has_token(de->d_name, "Keyboard")) {
            count++;
        }
    }

    closedir(d);
    return count;
}

static int count_mice(void) {
    DIR *d;
    struct dirent *de;
    int count = 0;

    d = opendir("/sys/class/input");
    if (!d) {
        return 0;
    }

    while ((de = readdir(d)) != NULL) {
        if (!filter_input_event(de)) {
            continue;
        }

        if (input_device_has_token(de->d_name, "mouse") ||
            input_device_has_token(de->d_name, "Mouse") ||
            input_device_has_token(de->d_name, "touchpad") ||
            input_device_has_token(de->d_name, "Touchpad")) {
            count++;
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
    out_snapshot->ethernet_count = count_ethernet_ifaces();
    out_snapshot->wifi_count = count_wifi_ifaces();
    out_snapshot->storage_count = count_dir_entries("/sys/block", filter_storage);
    out_snapshot->usb_count = count_dir_entries("/sys/bus/usb/devices", filter_usb);
    out_snapshot->keyboard_count = count_keyboards();
    out_snapshot->mouse_count = count_mice();
    out_snapshot->display_count = 1;
    out_snapshot->detected_at_unix = unix_now();
#else
    str_copy(out_snapshot->cpu_arch,
             sizeof(out_snapshot->cpu_arch),
             (handoff && handoff->arch_name) ? handoff->arch_name : "unknown");
    str_copy(out_snapshot->firmware_type, sizeof(out_snapshot->firmware_type), "uefi");
    str_copy(out_snapshot->console_type, sizeof(out_snapshot->console_type), "uefi_text");

    /*
     * UEFI console implies at least one display path and keyboard input path
     * when firmware exposes Simple Text Output/Input to the boot stage.
     */
    out_snapshot->display_count = 1;
    out_snapshot->keyboard_count = 1;
    out_snapshot->detected_at_unix = 0;

    apply_uefi_protocol_snapshot(handoff, out_snapshot);
#endif

    apply_pci_snapshot(out_snapshot);

    return true;
}
