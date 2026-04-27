#ifndef VITA_NET_STATUS_H
#define VITA_NET_STATUS_H

#include <stdbool.h>
#include <stdint.h>

#include <vita/boot.h>
#include <vita/hw.h>

typedef struct {
    bool hw_ready;
    bool hosted_mode;
    bool uefi_mode;

    int net_count;
    int ethernet_count;
    int wifi_count;
    int usb_controller_count;

    bool uefi_simple_network_present;
    bool uefi_ip4_service_present;
    bool uefi_dhcp4_service_present;
    bool uefi_tcp4_service_present;
    bool uefi_udp4_service_present;

    bool can_attempt_ethernet_auto_later;
    bool can_attempt_remote_ai_later;
} vita_net_status_t;

void net_status_probe(const vita_handoff_t *handoff,
                      const vita_hw_snapshot_t *hw,
                      bool hw_ready,
                      vita_net_status_t *out_status);

void net_status_show(const vita_handoff_t *handoff,
                     const vita_hw_snapshot_t *hw,
                     bool hw_ready);

#endif
