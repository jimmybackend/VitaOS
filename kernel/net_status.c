/*
 * kernel/net_status.c
 * F1A/F1B network readiness reporting.
 *
 * This module intentionally does not connect to the network yet.
 * It only reports hosted/UEFI readiness signals so the next patches can
 * decide whether Ethernet DHCP or a remote assistant gateway is realistic.
 */

#include <vita/console.h>
#include <vita/net_status.h>

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

typedef struct {
    efi_table_header_t hdr;
    uint16_t *firmware_vendor;
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
};

/* EFI_SIMPLE_NETWORK_PROTOCOL_GUID */
static efi_guid_t g_simple_network_protocol_guid = {
    0xA19832B9U, 0xAC25U, 0x11D3U,
    {0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}
};

/* EFI_IP4_SERVICE_BINDING_PROTOCOL_GUID */
static efi_guid_t g_ip4_service_binding_protocol_guid = {
    0xC51711E7U, 0xB4BFU, 0x404AU,
    {0xBF, 0xB8, 0x0A, 0x04, 0x8E, 0xF1, 0xFF, 0xE4}
};

/* EFI_DHCP4_SERVICE_BINDING_PROTOCOL_GUID */
static efi_guid_t g_dhcp4_service_binding_protocol_guid = {
    0x9D9A39D8U, 0xBD42U, 0x4A73U,
    {0xA4, 0xD5, 0x8E, 0xE9, 0x4B, 0xE1, 0x13, 0x80}
};

/* EFI_TCP4_SERVICE_BINDING_PROTOCOL_GUID */
static efi_guid_t g_tcp4_service_binding_protocol_guid = {
    0x00720665U, 0x67EBU, 0x4A99U,
    {0xBA, 0xF7, 0xD3, 0xC3, 0x3A, 0x1C, 0x7C, 0xC9}
};

/* EFI_UDP4_SERVICE_BINDING_PROTOCOL_GUID */
static efi_guid_t g_udp4_service_binding_protocol_guid = {
    0x83F01464U, 0x99BDU, 0x45E5U,
    {0xB3, 0x83, 0xAF, 0x63, 0x05, 0xD8, 0xE9, 0xE6}
};

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

static void u32_to_dec(uint32_t v, char out[16]) {
    char tmp[16];
    int i = 0;
    int j = 0;

    if (!out) {
        return;
    }

    if (v == 0) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }

    while (v > 0 && i < (int)(sizeof(tmp) - 1)) {
        tmp[i++] = (char)('0' + (v % 10U));
        v /= 10U;
    }

    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
}

static void write_i32(const char *label, int value) {
    char num[16];

    console_write_line(label);
    u32_to_dec((uint32_t)((value < 0) ? 0 : value), num);
    console_write_line(num);
}

static void write_bool(const char *label, bool value) {
    console_write_line(label);
    console_write_line(value ? "yes" : "no");
}

static bool uefi_protocol_present(const vita_handoff_t *handoff, efi_guid_t *guid) {
    efi_system_table_t *st;
    void *iface = 0;

    if (!handoff || handoff->firmware_type != VITA_FIRMWARE_UEFI || !handoff->uefi_system_table || !guid) {
        return false;
    }

    st = (efi_system_table_t *)handoff->uefi_system_table;
    if (!st || !st->boot_services || !st->boot_services->locate_protocol) {
        return false;
    }

    return st->boot_services->locate_protocol(guid, 0, &iface) == EFI_SUCCESS && iface != 0;
}

void net_status_probe(const vita_handoff_t *handoff,
                      const vita_hw_snapshot_t *hw,
                      bool hw_ready,
                      vita_net_status_t *out_status) {
    if (!out_status) {
        return;
    }

    mem_zero(out_status, sizeof(*out_status));

    out_status->hw_ready = hw_ready;
    out_status->hosted_mode = handoff && handoff->firmware_type == VITA_FIRMWARE_HOSTED;
    out_status->uefi_mode = handoff && handoff->firmware_type == VITA_FIRMWARE_UEFI;

    if (hw_ready && hw) {
        out_status->net_count = hw->net_count;
        out_status->ethernet_count = hw->ethernet_count;
        out_status->wifi_count = hw->wifi_count;
        out_status->usb_controller_count = hw->usb_controller_count;
    }

    if (out_status->uefi_mode) {
        out_status->uefi_simple_network_present = uefi_protocol_present(handoff, &g_simple_network_protocol_guid);
        out_status->uefi_ip4_service_present = uefi_protocol_present(handoff, &g_ip4_service_binding_protocol_guid);
        out_status->uefi_dhcp4_service_present = uefi_protocol_present(handoff, &g_dhcp4_service_binding_protocol_guid);
        out_status->uefi_tcp4_service_present = uefi_protocol_present(handoff, &g_tcp4_service_binding_protocol_guid);
        out_status->uefi_udp4_service_present = uefi_protocol_present(handoff, &g_udp4_service_binding_protocol_guid);
    }

    out_status->can_attempt_ethernet_auto_later =
        out_status->hosted_mode ||
        (out_status->uefi_simple_network_present &&
         (out_status->uefi_ip4_service_present || out_status->uefi_dhcp4_service_present));

    out_status->can_attempt_remote_ai_later =
        out_status->hosted_mode ||
        (out_status->uefi_simple_network_present &&
         (out_status->uefi_tcp4_service_present || out_status->uefi_udp4_service_present));
}

void net_status_show(const vita_handoff_t *handoff,
                     const vita_hw_snapshot_t *hw,
                     bool hw_ready) {
    vita_net_status_t st;

    net_status_probe(handoff, hw, hw_ready, &st);

    console_write_line("Network detailed status / Estado detallado de red:");

    if (!st.hw_ready) {
        console_write_line("No hardware snapshot available / No hay resumen de hardware disponible");
        console_write_line("Network readiness cannot be evaluated yet.");
        console_write_line("La disponibilidad de red aun no se puede evaluar.");
        return;
    }

    console_write_line("Hardware counters / Conteos de hardware:");
    write_i32("net_count:", st.net_count);
    write_i32("ethernet_count:", st.ethernet_count);
    write_i32("wifi_count:", st.wifi_count);
    write_i32("usb_controller_count:", st.usb_controller_count);

    if (st.uefi_mode) {
        console_write_line("UEFI protocol readiness / Preparacion de protocolos UEFI:");
        write_bool("simple_network_protocol:", st.uefi_simple_network_present);
        write_bool("ip4_service_binding:", st.uefi_ip4_service_present);
        write_bool("dhcp4_service_binding:", st.uefi_dhcp4_service_present);
        write_bool("tcp4_service_binding:", st.uefi_tcp4_service_present);
        write_bool("udp4_service_binding:", st.uefi_udp4_service_present);

        if (st.uefi_simple_network_present) {
            console_write_line("- UEFI exposes a basic network protocol candidate.");
            console_write_line("- UEFI expone un candidato basico de protocolo de red.");
        } else {
            console_write_line("- UEFI Simple Network Protocol was not exposed in this boot.");
            console_write_line("- UEFI no expuso Simple Network Protocol en este arranque.");
        }

        if (st.can_attempt_ethernet_auto_later) {
            console_write_line("- Future Ethernet auto/DHCP attempt looks possible on this firmware path.");
            console_write_line("- Un intento futuro de Ethernet auto/DHCP parece posible en esta ruta de firmware.");
        } else {
            console_write_line("- Future Ethernet auto/DHCP needs more firmware protocol support or a driver.");
            console_write_line("- Ethernet auto/DHCP futuro requiere mas soporte UEFI o un driver.");
        }
    } else if (st.hosted_mode) {
        console_write_line("Hosted network readiness / Preparacion de red hosted:");
        console_write_line("- Hosted mode can use the host OS sockets for validation and remote AI experiments.");
        console_write_line("- El modo hosted puede usar sockets del sistema anfitrion para validacion e IA remota experimental.");
    } else {
        console_write_line("Unknown boot mode / Modo de arranque desconocido");
    }

    console_write_line("Wi-Fi note / Nota Wi-Fi:");
    if (st.wifi_count > 0) {
        console_write_line("- Wi-Fi hardware is visible, but SSID/password association is not implemented yet.");
        console_write_line("- Wi-Fi es visible, pero asociacion SSID/password aun no esta implementada.");
    } else {
        console_write_line("- Wi-Fi was not visible in this snapshot.");
        console_write_line("- Wi-Fi no fue visible en este resumen.");
    }

    console_write_line("Remote AI readiness / Preparacion para IA remota:");
    if (st.can_attempt_remote_ai_later) {
        console_write_line("- A future remote assistant gateway can be attempted after network bring-up exists.");
        console_write_line("- Se podra intentar un gateway de IA remota cuando exista activacion de red.");
    } else {
        console_write_line("- Remote assistant gateway needs Ethernet/IP transport first.");
        console_write_line("- El gateway de IA remota necesita transporte Ethernet/IP primero.");
    }

    console_write_line("Current scope / Alcance actual:");
    console_write_line("- Status only: no DHCP, TCP connect, DNS, TLS, Wi-Fi login, or remote AI call is executed yet.");
    console_write_line("- Solo estado: aun no ejecuta DHCP, conexion TCP, DNS, TLS, login Wi-Fi ni llamada de IA remota.");
}
