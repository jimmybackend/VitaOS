SHELL := /bin/bash

BUILD_DIR := build

EFI_DIR := $(BUILD_DIR)/efi
EFI_BOOT_DIR := $(EFI_DIR)/EFI/BOOT
EFI_APP := $(EFI_BOOT_DIR)/BOOTX64.EFI
EFI_SPLASH_DIR := $(EFI_DIR)/img
EFI_SPLASH_ASSETS := \
	img/1.bmp \
	img/2.bmp \
	img/3.bmp \
	img/4.bmp

HOSTED_DIR := $(BUILD_DIR)/hosted
HOSTED_BIN := $(HOSTED_DIR)/vitaos-hosted
AUDIT_DIR := $(BUILD_DIR)/audit
ISO_IMAGE := $(BUILD_DIR)/iso/vitaos-uefi-live.iso

CC := clang

COMMON_CFLAGS := -Wall -Wextra -Werror -Iinclude

EFI_CFLAGS := -target x86_64-unknown-windows -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin $(COMMON_CFLAGS)
EFI_LDFLAGS := -target x86_64-unknown-windows -fuse-ld=lld -nostdlib -Wl,/subsystem:efi_application -Wl,/entry:efi_main -Wl,/nodefaultlib

HOSTED_CFLAGS := $(COMMON_CFLAGS) -DVITA_HOSTED
HOSTED_LDFLAGS := -lsqlite3

COMMON_KERNEL := \
	kernel/kmain.c \
	kernel/console.c \
	kernel/command_core.c \
	kernel/panic.c \
	kernel/audit_core.c \
	kernel/hardware_discovery.c \
	kernel/pci_discovery.c \
	kernel/proposal.c \
	kernel/node_core.c \
	kernel/ai_gateway.c \
	kernel/storage.c \
	kernel/editor.c \
	kernel/session_export.c \
	kernel/session_jsonl_export.c \
	kernel/session_journal.c \
	kernel/session_transcript.c \
	kernel/power.c \
	kernel/net_status.c \
	kernel/net_connect.c

EFI_SOURCES := \
	arch/x86_64/boot/uefi_entry.c \
	arch/x86_64/boot/uefi_neon_frame.c \
	arch/x86_64/boot/uefi_neon_text.c \
	arch/x86_64/boot/uefi_splash.c \
	$(COMMON_KERNEL)

HOSTED_SOURCES := \
	arch/x86_64/boot/host_entry.c \
	$(COMMON_KERNEL)

.DEFAULT_GOAL := all

all: $(EFI_APP)

efi: $(EFI_APP)

hosted: $(HOSTED_BIN)

iso: $(EFI_APP)
	BUILD_EFI=0 ./tools/image/make-uefi-iso.sh $(ISO_IMAGE)

$(EFI_APP): $(EFI_SOURCES) $(EFI_SPLASH_ASSETS)
	mkdir -p $(EFI_BOOT_DIR) $(EFI_SPLASH_DIR)
	$(CC) $(EFI_CFLAGS) $(EFI_LDFLAGS) -o $@ $(EFI_SOURCES)
	cp $(EFI_SPLASH_ASSETS) $(EFI_SPLASH_DIR)/

$(HOSTED_BIN): $(HOSTED_SOURCES)
	mkdir -p $(HOSTED_DIR) $(AUDIT_DIR)
	$(CC) $(HOSTED_CFLAGS) -o $@ $(HOSTED_SOURCES) $(HOSTED_LDFLAGS)

run: $(EFI_APP)
	./tools/build/qemu-x86_64.sh

smoke: $(EFI_APP)
	./tools/test/smoke-boot.sh

smoke-audit: $(HOSTED_BIN)
	./tools/test/smoke-audit.sh

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all efi hosted iso run smoke smoke-audit clean
