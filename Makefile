SHELL := /bin/bash

BUILD_DIR := build
EFI_DIR := $(BUILD_DIR)/efi
EFI_BOOT_DIR := $(EFI_DIR)/EFI/BOOT
EFI_APP := $(EFI_BOOT_DIR)/BOOTX64.EFI
HOSTED_BIN := $(BUILD_DIR)/hosted/vitaos-hosted

CC := clang
EFI_CFLAGS := -target x86_64-unknown-windows -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin -Wall -Wextra -Werror -Iinclude
EFI_LDFLAGS := -target x86_64-unknown-windows -fuse-ld=lld -nostdlib -Wl,/subsystem:efi_application -Wl,/entry:efi_main -Wl,/nodefaultlib

HOSTED_CFLAGS := -Wall -Wextra -Werror -Iinclude -DVITA_HOSTED
HOSTED_LDFLAGS := -lsqlite3

COMMON_KERNEL := kernel/kmain.c kernel/console.c kernel/panic.c kernel/audit_core.c kernel/hardware_discovery.c
EFI_SOURCES := arch/x86_64/boot/uefi_entry.c $(COMMON_KERNEL)
HOSTED_SOURCES := arch/x86_64/boot/host_entry.c $(COMMON_KERNEL)

all: $(EFI_APP)

$(EFI_APP): $(EFI_SOURCES)
	mkdir -p $(EFI_BOOT_DIR)
	$(CC) $(EFI_CFLAGS) $(EFI_LDFLAGS) -o $(EFI_APP) $(EFI_SOURCES)

$(HOSTED_BIN): $(HOSTED_SOURCES)
	mkdir -p $(BUILD_DIR)/hosted $(BUILD_DIR)/audit
	$(CC) $(HOSTED_CFLAGS) -o $(HOSTED_BIN) $(HOSTED_SOURCES) $(HOSTED_LDFLAGS)

hosted: $(HOSTED_BIN)

run: all
	./tools/build/qemu-x86_64.sh

smoke: all
	./tools/test/smoke-boot.sh

smoke-audit: hosted
	./tools/test/smoke-audit.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all hosted clean run smoke smoke-audit
