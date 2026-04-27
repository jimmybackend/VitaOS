/*
 * kernel/pci_discovery.c
 * Minimal PCI discovery for F1A/F1B hardware inventory.
 *
 * Hosted build:
 * - reads /sys/bus/pci/devices when available
 *
 * UEFI/freestanding build:
 * - returns an empty snapshot until PCI config-space access is added
 */

#include <vita/pci.h>

#ifdef VITA_HOSTED
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#endif

static void mem_zero(void *ptr, unsigned long n) {
    unsigned char *p = (unsigned char *)ptr;
    for (unsigned long i = 0; i < n; ++i) {
        p[i] = 0;
    }
}

#ifdef VITA_HOSTED
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

static vita_pci_device_kind_t classify_pci_device(uint8_t class_code,
                                                  uint8_t subclass,
                                                  char class_name[VITA_PCI_CLASS_NAME_MAX],
                                                  char device_name[VITA_PCI_DEVICE_NAME_MAX]) {
    switch (class_code) {
        case 0x01:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "storage");
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "storage controller");
            return VITA_PCI_DEVICE_STORAGE;

        case 0x02:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "network");
            if (subclass == 0x00) {
                str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "ethernet controller");
                return VITA_PCI_DEVICE_ETHERNET;
            }
            if (subclass == 0x80) {
                str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "network controller");
                return VITA_PCI_DEVICE_WIFI;
            }
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "network controller");
            return VITA_PCI_DEVICE_ETHERNET;

        case 0x03:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "display");
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "display controller");
            return VITA_PCI_DEVICE_DISPLAY;

        case 0x04:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "multimedia");
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "audio/multimedia controller");
            return VITA_PCI_DEVICE_AUDIO;

        case 0x06:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "bridge");
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "bridge device");
            return VITA_PCI_DEVICE_BRIDGE;

        case 0x0c:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "serial-bus");
            if (subclass == 0x03) {
                str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "usb controller");
                return VITA_PCI_DEVICE_USB;
            }
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "serial bus controller");
            return VITA_PCI_DEVICE_UNKNOWN;

        default:
            str_copy(class_name, VITA_PCI_CLASS_NAME_MAX, "unknown");
            str_copy(device_name, VITA_PCI_DEVICE_NAME_MAX, "unknown pci device");
            return VITA_PCI_DEVICE_UNKNOWN;
    }
}

static void update_counts(vita_pci_snapshot_t *snapshot, vita_pci_device_kind_t kind) {
    if (!snapshot) {
        return;
    }

    switch (kind) {
        case VITA_PCI_DEVICE_DISPLAY:
            snapshot->display_count++;
            break;
        case VITA_PCI_DEVICE_ETHERNET:
            snapshot->ethernet_count++;
            break;
        case VITA_PCI_DEVICE_WIFI:
            snapshot->wifi_count++;
            break;
        case VITA_PCI_DEVICE_USB:
            snapshot->usb_controller_count++;
            break;
        case VITA_PCI_DEVICE_AUDIO:
            snapshot->audio_count++;
            break;
        case VITA_PCI_DEVICE_STORAGE:
            snapshot->storage_count++;
            break;
        case VITA_PCI_DEVICE_INPUT:
            snapshot->input_count++;
            break;
        default:
            break;
    }
}

static bool read_hex_u16_file(const char *path, uint16_t *out) {
    FILE *f;
    unsigned int value = 0;

    if (!path || !out) {
        return false;
    }

    f = fopen(path, "r");
    if (!f) {
        return false;
    }

    if (fscanf(f, "0x%x", &value) != 1) {
        fclose(f);
        return false;
    }

    fclose(f);
    *out = (uint16_t)value;
    return true;
}

static bool read_hex_u32_file(const char *path, uint32_t *out) {
    FILE *f;
    unsigned int value = 0;

    if (!path || !out) {
        return false;
    }

    f = fopen(path, "r");
    if (!f) {
        return false;
    }

    if (fscanf(f, "0x%x", &value) != 1) {
        fclose(f);
        return false;
    }

    fclose(f);
    *out = (uint32_t)value;
    return true;
}

static bool parse_bdf(const char *name, uint8_t *bus, uint8_t *slot, uint8_t *function) {
    unsigned int domain = 0;
    unsigned int b = 0;
    unsigned int s = 0;
    unsigned int f = 0;

    if (!name || !bus || !slot || !function) {
        return false;
    }

    if (sscanf(name, "%x:%x:%x.%x", &domain, &b, &s, &f) != 4) {
        return false;
    }

    (void)domain;
    *bus = (uint8_t)b;
    *slot = (uint8_t)s;
    *function = (uint8_t)f;
    return true;
}

static bool load_hosted_pci_device(const char *entry_name, vita_pci_device_t *out) {
    char path[256];
    uint32_t class_value = 0;

    if (!entry_name || !out) {
        return false;
    }

    mem_zero(out, sizeof(*out));

    if (!parse_bdf(entry_name, &out->bus, &out->slot, &out->function)) {
        return false;
    }

    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/vendor", entry_name);
    if (!read_hex_u16_file(path, &out->vendor_id)) {
        return false;
    }

    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/device", entry_name);
    if (!read_hex_u16_file(path, &out->device_id)) {
        return false;
    }

    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/class", entry_name);
    if (!read_hex_u32_file(path, &class_value)) {
        return false;
    }

    out->class_code = (uint8_t)((class_value >> 16) & 0xff);
    out->subclass = (uint8_t)((class_value >> 8) & 0xff);
    out->prog_if = (uint8_t)(class_value & 0xff);
    out->revision_id = 0;

    out->kind = classify_pci_device(out->class_code,
                                    out->subclass,
                                    out->class_name,
                                    out->device_name);

    return true;
}
#endif

bool pci_discovery_run(vita_pci_snapshot_t *out_snapshot) {
    if (!out_snapshot) {
        return false;
    }

    mem_zero(out_snapshot, sizeof(*out_snapshot));

#ifdef VITA_HOSTED
    {
        DIR *d;
        struct dirent *de;

        d = opendir("/sys/bus/pci/devices");
        if (!d) {
            return true;
        }

        while ((de = readdir(d)) != NULL) {
            vita_pci_device_t dev;

            if (de->d_name[0] == '.') {
                continue;
            }

            if (out_snapshot->count >= VITA_PCI_MAX_DEVICES) {
                break;
            }

            if (!load_hosted_pci_device(de->d_name, &dev)) {
                continue;
            }

            out_snapshot->devices[out_snapshot->count++] = dev;
            update_counts(out_snapshot, dev.kind);
        }

        closedir(d);
    }
#endif

    return true;
}
