#ifndef VITA_PCI_H
#define VITA_PCI_H

#include <stdbool.h>
#include <stdint.h>

#define VITA_PCI_MAX_DEVICES 64
#define VITA_PCI_CLASS_NAME_MAX 32
#define VITA_PCI_DEVICE_NAME_MAX 64

typedef enum {
    VITA_PCI_DEVICE_UNKNOWN = 0,
    VITA_PCI_DEVICE_DISPLAY,
    VITA_PCI_DEVICE_ETHERNET,
    VITA_PCI_DEVICE_WIFI,
    VITA_PCI_DEVICE_USB,
    VITA_PCI_DEVICE_AUDIO,
    VITA_PCI_DEVICE_STORAGE,
    VITA_PCI_DEVICE_BRIDGE,
    VITA_PCI_DEVICE_INPUT,
} vita_pci_device_kind_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;

    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision_id;

    vita_pci_device_kind_t kind;

    char class_name[VITA_PCI_CLASS_NAME_MAX];
    char device_name[VITA_PCI_DEVICE_NAME_MAX];
} vita_pci_device_t;

typedef struct {
    vita_pci_device_t devices[VITA_PCI_MAX_DEVICES];
    uint32_t count;

    uint32_t display_count;
    uint32_t ethernet_count;
    uint32_t wifi_count;
    uint32_t usb_controller_count;
    uint32_t audio_count;
    uint32_t storage_count;
    uint32_t input_count;
} vita_pci_snapshot_t;

bool pci_discovery_run(vita_pci_snapshot_t *out_snapshot);

#endif
