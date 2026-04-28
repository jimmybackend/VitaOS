#!/usr/bin/env bash
set -euo pipefail

# VitaOS patch v22: storage USB tree validator and repair command.
# Bash/perl only. No Python, no malloc, no GUI/window manager.

ROOT=$(pwd)
STORAGE_C="$ROOT/kernel/storage.c"
STORAGE_H="$ROOT/include/vita/storage.h"
DOC_DIR="$ROOT/docs/storage"
DOC="$DOC_DIR/tree-validator.md"

if [[ ! -f "$ROOT/Makefile" || ! -f "$STORAGE_C" || ! -f "$STORAGE_H" ]]; then
  echo "[v22] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

mkdir -p "$DOC_DIR" "$ROOT/tools/patches"

# Header prototypes.
if ! grep -q 'storage_show_tree' "$STORAGE_H"; then
  perl -0pi -e 's/(bool storage_list_notes\(void\);\n)/$1void storage_show_tree(void);\nbool storage_check_tree(bool verbose);\nbool storage_repair_tree(bool verbose);\n/' "$STORAGE_H"
fi

# Add status help lines if the current storage status command list is present.
if ! grep -q 'storage tree' "$STORAGE_C"; then
  perl -0pi -e 's/(    console_write_line\("notes list"\);\n)/$1    console_write_line("storage tree");\n    console_write_line("storage check");\n    console_write_line("storage repair");\n/' "$STORAGE_C"
fi

# Add tree validator/repair implementation before storage_handle_command().
if ! grep -q 'VITA_STORAGE_TREE_V22_START' "$STORAGE_C"; then
  if ! grep -q 'bool storage_handle_command(const char \*cmd)' "$STORAGE_C"; then
    echo "[v22] ERROR: storage_handle_command() marker not found" >&2
    exit 1
  fi

  cat > /tmp/vitaos_storage_tree_v22_block.c <<'CBLOCK'

/* VITA_STORAGE_TREE_V22_START */
typedef struct {
    const char *path;
    const char *label;
} vita_storage_tree_dir_t;

static const vita_storage_tree_dir_t g_vita_tree_dirs[] = {
    {"/vita/", "root"},
    {"/vita/config/", "config"},
    {"/vita/audit/", "audit"},
    {"/vita/notes/", "notes"},
    {"/vita/messages/", "messages"},
    {"/vita/messages/inbox/", "messages inbox"},
    {"/vita/messages/outbox/", "messages outbox"},
    {"/vita/messages/drafts/", "messages drafts"},
    {"/vita/emergency/", "emergency"},
    {"/vita/emergency/reports/", "emergency reports"},
    {"/vita/emergency/checklists/", "emergency checklists"},
    {"/vita/ai/", "ai"},
    {"/vita/ai/prompts/", "ai prompts"},
    {"/vita/ai/responses/", "ai responses"},
    {"/vita/ai/sessions/", "ai sessions"},
    {"/vita/net/", "net"},
    {"/vita/net/profiles/", "net profiles"},
    {"/vita/export/", "export"},
    {"/vita/export/audit/", "export audit"},
    {"/vita/export/notes/", "export notes"},
    {"/vita/export/reports/", "export reports"},
    {"/vita/tmp/", "tmp"},
};

#define VITA_STORAGE_TREE_COUNT ((unsigned long)(sizeof(g_vita_tree_dirs) / sizeof(g_vita_tree_dirs[0])))

#ifdef VITA_HOSTED
static void hosted_trim_trailing_slash(char *path) {
    unsigned long n;

    if (!path) {
        return;
    }

    n = str_len(path);
    while (n > 1U && path[n - 1U] == '/') {
        path[n - 1U] = '\0';
        n--;
    }
}

static bool hosted_dir_exists(const char *path) {
    char host_path[256];
    struct stat st;

    if (!hosted_map_path(path, host_path, sizeof(host_path))) {
        set_error("invalid hosted tree path");
        return false;
    }

    hosted_trim_trailing_slash(host_path);

    if (stat(host_path, &st) != 0) {
        set_error("hosted directory missing");
        return false;
    }

    if (!S_ISDIR(st.st_mode)) {
        set_error("hosted path is not a directory");
        return false;
    }

    set_error("ok");
    return true;
}

static bool hosted_create_dir_path(const char *path) {
    char host_path[256];

    if (!hosted_map_path(path, host_path, sizeof(host_path))) {
        set_error("invalid hosted tree path");
        return false;
    }

    hosted_ensure_parent_dirs(host_path);
    hosted_trim_trailing_slash(host_path);

    if (mkdir(host_path, 0775) != 0 && !hosted_dir_exists(path)) {
        set_error("hosted mkdir failed");
        return false;
    }

    set_error("ok");
    return true;
}
#else
static bool uefi_dir_path_to_char16(const char *path, char16_t *out, unsigned long out_cap) {
    unsigned long i;
    unsigned long j = 0;
    unsigned long n;

    if (!path_allowed(path) || !out || out_cap == 0) {
        return false;
    }

    n = str_len(path);
    while (n > 1U && path[n - 1U] == '/') {
        n--;
    }

    out[j++] = (char16_t)'\\';

    i = (path[0] == '/') ? 1U : 0U;
    while (i < n && path[i] && (j + 1U) < out_cap) {
        out[j++] = (path[i] == '/') ? (char16_t)'\\' : (char16_t)(uint8_t)path[i];
        i++;
    }

    out[j] = 0;
    return true;
}

static bool uefi_dir_exists(const char *path) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *dir = 0;
    char16_t path16[VITA_STORAGE_PATH_MAX];
    bool ok = false;

    if (!uefi_dir_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI tree path");
        return false;
    }

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (root->open && root->open(root, &dir, path16, EFI_FILE_MODE_READ, 0) == EFI_SUCCESS && dir) {
        ok = true;
        set_error("ok");
        if (dir->close) {
            dir->close(dir);
        }
    } else {
        set_error("UEFI directory missing");
    }

    if (root->close) {
        root->close(root);
    }

    return ok;
}

static bool uefi_create_dir_path(const char *path) {
    efi_file_protocol_t *root = 0;
    efi_file_protocol_t *dir = 0;
    char16_t path16[VITA_STORAGE_PATH_MAX];
    bool ok = false;

    if (!uefi_dir_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
        set_error("invalid UEFI tree path");
        return false;
    }

    if (!uefi_open_root(&root)) {
        return false;
    }

    if (root->open && root->open(root,
                                  &dir,
                                  path16,
                                  EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE,
                                  EFI_FILE_DIRECTORY) == EFI_SUCCESS && dir) {
        ok = true;
        set_error("ok");
        if (dir->close) {
            dir->close(dir);
        }
    } else {
        set_error("UEFI directory create/open failed");
    }

    if (root->close) {
        root->close(root);
    }

    return ok;
}
#endif

static bool storage_tree_dir_exists(const char *path) {
    if (!g_storage.initialized) {
        set_error("storage not initialized");
        return false;
    }

#ifdef VITA_HOSTED
    return hosted_dir_exists(path);
#else
    return uefi_dir_exists(path);
#endif
}

static bool storage_tree_create_dir(const char *path) {
    if (!g_storage.initialized || !g_storage.writable) {
        set_error("storage not initialized or not writable");
        return false;
    }

#ifdef VITA_HOSTED
    return hosted_create_dir_path(path);
#else
    return uefi_create_dir_path(path);
#endif
}

static void storage_print_tree_line(const char *prefix, const char *path, const char *label) {
    char line[256];

    line[0] = '\0';
    str_copy(line, sizeof(line), prefix ? prefix : "");
    str_append(line, sizeof(line), " ");
    str_append(line, sizeof(line), path ? path : "");
    if (label && label[0]) {
        str_append(line, sizeof(line), "  # ");
        str_append(line, sizeof(line), label);
    }
    console_write_line(line);
}

void storage_show_tree(void) {
    unsigned long i;

    console_write_line("VitaOS storage tree / Arbol de almacenamiento:");
    for (i = 0; i < VITA_STORAGE_TREE_COUNT; ++i) {
        storage_print_tree_line("-", g_vita_tree_dirs[i].path, g_vita_tree_dirs[i].label);
    }
}

bool storage_check_tree(bool verbose) {
    unsigned long i;
    unsigned long missing = 0;

    if (!g_storage.initialized) {
        console_write_line("storage check: storage not initialized");
        set_error("storage not initialized");
        return false;
    }

    if (verbose) {
        console_write_line("Storage tree check / Revision del arbol:");
    }

    for (i = 0; i < VITA_STORAGE_TREE_COUNT; ++i) {
        if (storage_tree_dir_exists(g_vita_tree_dirs[i].path)) {
            if (verbose) {
                storage_print_tree_line("ok", g_vita_tree_dirs[i].path, g_vita_tree_dirs[i].label);
            }
        } else {
            missing++;
            if (verbose) {
                storage_print_tree_line("missing", g_vita_tree_dirs[i].path, g_vita_tree_dirs[i].label);
            }
        }
    }

    if (missing == 0U) {
        console_write_line("storage check: OK");
        set_error("ok");
        return true;
    }

    console_write_line("storage check: missing directories; run storage repair");
    set_error("storage tree incomplete");
    return false;
}

bool storage_repair_tree(bool verbose) {
    unsigned long i;
    unsigned long failed = 0;
    unsigned long created = 0;

    if (!g_storage.initialized || !g_storage.writable) {
        console_write_line("storage repair: storage not initialized or not writable");
        set_error("storage not initialized or not writable");
        return false;
    }

    if (verbose) {
        console_write_line("Storage tree repair / Reparacion del arbol:");
    }

    for (i = 0; i < VITA_STORAGE_TREE_COUNT; ++i) {
        const char *path = g_vita_tree_dirs[i].path;

        if (storage_tree_dir_exists(path)) {
            if (verbose) {
                storage_print_tree_line("ok", path, g_vita_tree_dirs[i].label);
            }
            continue;
        }

        if (storage_tree_create_dir(path)) {
            created++;
            if (verbose) {
                storage_print_tree_line("created", path, g_vita_tree_dirs[i].label);
            }
        } else {
            failed++;
            if (verbose) {
                storage_print_tree_line("failed", path, g_vita_tree_dirs[i].label);
                console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
            }
        }
    }

    if (failed == 0U) {
        if (created == 0U) {
            console_write_line("storage repair: OK, tree already complete");
        } else {
            console_write_line("storage repair: OK, missing directories created");
        }
        set_error("ok");
        return true;
    }

    console_write_line("storage repair: failed; check writable USB/FAT tree");
    set_error("storage tree repair failed");
    return false;
}
/* VITA_STORAGE_TREE_V22_END */

CBLOCK

  perl -0pi -e 'BEGIN { local $/; open my $fh, "<", "/tmp/vitaos_storage_tree_v22_block.c" or die $!; our $block = <$fh>; } s/\nbool storage_handle_command\(const char \*cmd\) \{/\n$block\nbool storage_handle_command(const char *cmd) {/' "$STORAGE_C"
fi

# Add command cases near the top of storage_handle_command().
if ! grep -q 'storage_check_tree(true)' "$STORAGE_C"; then
  perl -0pi -e 's/(bool storage_handle_command\(const char \*cmd\) \{\n)/$1    if (cmd && (str_eq(cmd, "storage tree") || str_eq(cmd, "storage paths"))) {\n        storage_show_tree();\n        return true;\n    }\n\n    if (cmd && (str_eq(cmd, "storage check") || str_eq(cmd, "storage validate") || str_eq(cmd, "storage verify"))) {\n        (void)storage_check_tree(true);\n        return true;\n    }\n\n    if (cmd && (str_eq(cmd, "storage repair") || str_eq(cmd, "storage init-tree") || str_eq(cmd, "storage mkdirs"))) {\n        (void)storage_repair_tree(true);\n        return true;\n    }\n\n/' "$STORAGE_C"
fi

cat > "$DOC" <<'DOC'
# Storage tree validator and repair command

Patch: `storage: add USB tree validator and repair command`

## Purpose

VitaOS F1A/F1B uses a constrained writable `/vita/` tree for journal, notes, messages, emergency reports, AI sessions, network profiles, exports and temporary files.

This patch adds commands to make that tree visible and repairable without adding a general-purpose VFS, GUI, malloc, Python or new OS dependencies.

## Commands

```text
storage tree
storage check
storage repair
```

Aliases:

```text
storage paths
storage validate
storage verify
storage init-tree
storage mkdirs
```

## Expected tree

```text
/vita/
/vita/config/
/vita/audit/
/vita/notes/
/vita/messages/
/vita/messages/inbox/
/vita/messages/outbox/
/vita/messages/drafts/
/vita/emergency/
/vita/emergency/reports/
/vita/emergency/checklists/
/vita/ai/
/vita/ai/prompts/
/vita/ai/responses/
/vita/ai/sessions/
/vita/net/
/vita/net/profiles/
/vita/export/
/vita/export/audit/
/vita/export/notes/
/vita/export/reports/
/vita/tmp/
```

## Scope

Included:

- hosted directory check/create under `build/storage/vita/`;
- UEFI Simple File System directory check/create;
- no heap allocation;
- no Python;
- no GUI/window manager;
- no broad filesystem or recursive VFS claims.

Not included:

- recursive delete;
- full filesystem browser;
- FAT formatting;
- SQLite UEFI persistence;
- encryption;
- remote sync;
- network/Wi-Fi expansion.
DOC

cat > "$ROOT/README-vitaos-patch22-storage-tree.md" <<'DOC'
# VitaOS patch v22 — storage tree validator and repair

Commit title:

```text
storage: add USB tree validator and repair command
```

## Apply

```bash
bash tools/patches/apply-storage-tree-validator-v22.sh
```

## Validate

```bash
bash tools/patches/verify-storage-tree-v22.sh
make clean
make hosted
make smoke-audit
make
make smoke
make iso
```

## Manual test

Hosted:

```bash
./build/hosted/vitaos-hosted
```

Inside VitaOS:

```text
storage tree
storage check
storage repair
storage check
storage status
note test.txt
.save
notes list
export session
journal status
shutdown
```

UEFI:

```text
storage tree
storage check
storage repair
storage test
journal flush
export session
shutdown
```

## Scope

No Python runtime/build dependency, no malloc, no GUI/window manager, no broad VFS, and no network/Wi-Fi expansion.
DOC

cat > "$ROOT/tools/patches/verify-storage-tree-v22.sh" <<'VERIFY'
#!/usr/bin/env bash
set -euo pipefail

ROOT=$(pwd)
STORAGE_C="$ROOT/kernel/storage.c"
STORAGE_H="$ROOT/include/vita/storage.h"
MAKEFILE="$ROOT/Makefile"

if [[ ! -f "$MAKEFILE" || ! -f "$STORAGE_C" || ! -f "$STORAGE_H" ]]; then
  echo "[verify-v22] ERROR: run this from the VitaOS repository root" >&2
  exit 1
fi

required=(
  'VITA_STORAGE_TREE_V22_START'
  'storage_show_tree'
  'storage_check_tree'
  'storage_repair_tree'
  'storage tree'
  'storage check'
  'storage repair'
  'uefi_create_dir_path'
  'hosted_create_dir_path'
)

for sym in "${required[@]}"; do
  if ! grep -q "$sym" "$STORAGE_C" "$STORAGE_H" 2>/dev/null; then
    echo "[verify-v22] ERROR: missing symbol/text: $sym" >&2
    exit 1
  fi
done

if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$MAKEFILE" "$STORAGE_C" "$STORAGE_H" 2>/dev/null; then
  echo "[verify-v22] ERROR: unexpected Python reference in runtime/build files" >&2
  exit 1
fi

echo "[verify-v22] OK: storage tree validator/repair patch is present"
VERIFY
chmod +x "$ROOT/tools/patches/verify-storage-tree-v22.sh"

# Basic sanity: no Python references in patched runtime/build files.
if grep -RInE '(^#!.*python|python3|[^A-Za-z0-9_]python[^A-Za-z0-9_])' "$STORAGE_C" "$STORAGE_H" "$ROOT/Makefile" 2>/dev/null; then
  echo "[v22] ERROR: unexpected Python reference in runtime/build files" >&2
  exit 1
fi

bash "$ROOT/tools/patches/verify-storage-tree-v22.sh"
echo "[v22] storage USB tree validator and repair command patch applied"
