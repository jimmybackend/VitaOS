#!/usr/bin/env python3
from pathlib import Path


def read(path: str) -> str:
    return Path(path).read_text()


def write(path: str, text: str) -> None:
    Path(path).write_text(text)


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise SystemExit(f"patch failed: could not find {label}")
    return text.replace(old, new, 1)


def insert_before(text: str, marker: str, insert: str, label: str) -> str:
    if insert.strip() in text:
        return text
    if marker not in text:
        raise SystemExit(f"patch failed: could not find marker for {label}")
    return text.replace(marker, insert + marker, 1)


def patch_storage_h() -> None:
    path = Path("include/vita/storage.h")
    s = read(str(path))
    if "bool storage_delete_path(" not in s:
        old = "bool storage_read_text(const char *path, char *out, unsigned long out_cap);\n"
        new = old + "bool storage_delete_path(const char *path);\nbool storage_rename_path(const char *old_path, const char *new_path);\n"
        s = replace_once(s, old, new, "storage prototypes")
    write(str(path), s)


def patch_storage_c() -> None:
    path = Path("kernel/storage.c")
    s = read(str(path))

    protected_fn = r'''
static bool path_is_protected_root(const char *path) {
    if (!path) {
        return true;
    }

    if (str_eq(path, "/vita") || str_eq(path, "/vita/")) {
        return true;
    }

    if (str_eq(path, "/vita/config") || str_eq(path, "/vita/config/")) {
        return true;
    }
    if (str_eq(path, "/vita/audit") || str_eq(path, "/vita/audit/")) {
        return true;
    }
    if (str_eq(path, "/vita/notes") || str_eq(path, "/vita/notes/")) {
        return true;
    }
    if (str_eq(path, "/vita/messages") || str_eq(path, "/vita/messages/")) {
        return true;
    }
    if (str_eq(path, "/vita/messages/inbox") || str_eq(path, "/vita/messages/inbox/")) {
        return true;
    }
    if (str_eq(path, "/vita/messages/outbox") || str_eq(path, "/vita/messages/outbox/")) {
        return true;
    }
    if (str_eq(path, "/vita/messages/drafts") || str_eq(path, "/vita/messages/drafts/")) {
        return true;
    }
    if (str_eq(path, "/vita/emergency") || str_eq(path, "/vita/emergency/")) {
        return true;
    }
    if (str_eq(path, "/vita/emergency/reports") || str_eq(path, "/vita/emergency/reports/")) {
        return true;
    }
    if (str_eq(path, "/vita/emergency/checklists") || str_eq(path, "/vita/emergency/checklists/")) {
        return true;
    }
    if (str_eq(path, "/vita/ai") || str_eq(path, "/vita/ai/")) {
        return true;
    }
    if (str_eq(path, "/vita/ai/prompts") || str_eq(path, "/vita/ai/prompts/")) {
        return true;
    }
    if (str_eq(path, "/vita/ai/responses") || str_eq(path, "/vita/ai/responses/")) {
        return true;
    }
    if (str_eq(path, "/vita/ai/sessions") || str_eq(path, "/vita/ai/sessions/")) {
        return true;
    }
    if (str_eq(path, "/vita/net") || str_eq(path, "/vita/net/")) {
        return true;
    }
    if (str_eq(path, "/vita/net/profiles") || str_eq(path, "/vita/net/profiles/")) {
        return true;
    }
    if (str_eq(path, "/vita/export") || str_eq(path, "/vita/export/")) {
        return true;
    }
    if (str_eq(path, "/vita/export/audit") || str_eq(path, "/vita/export/audit/")) {
        return true;
    }
    if (str_eq(path, "/vita/export/notes") || str_eq(path, "/vita/export/notes/")) {
        return true;
    }
    if (str_eq(path, "/vita/export/reports") || str_eq(path, "/vita/export/reports/")) {
        return true;
    }
    if (str_eq(path, "/vita/tmp") || str_eq(path, "/vita/tmp/")) {
        return true;
    }

    return false;
}

'''
    if "path_is_protected_root" not in s:
        marker = "#ifdef VITA_HOSTED\n"
        s = insert_before(s, marker, protected_fn, "protected storage roots")

    if "bool storage_delete_path(const char *path)" not in s:
        public_funcs = r'''
bool storage_delete_path(const char *path) {
    if (!g_storage.initialized || !g_storage.writable) {
        set_error("storage not initialized or not writable");
        return false;
    }

    if (!path_allowed(path)) {
        set_error("path must start with /vita/ and must not contain '..' or ':'");
        return false;
    }

    if (path_is_protected_root(path)) {
        set_error("refusing to delete protected VitaOS storage root");
        return false;
    }

#ifdef VITA_HOSTED
    {
        char host_path[256];
        if (!hosted_map_path(path, host_path, sizeof(host_path))) {
            set_error("invalid hosted storage path");
            return false;
        }
        if (remove(host_path) == 0) {
            set_error("ok");
            return true;
        }
        set_error("hosted remove failed");
        return false;
    }
#else
    {
        efi_file_protocol_t *root = 0;
        efi_file_protocol_t *file = 0;
        char16_t path16[VITA_STORAGE_PATH_MAX];
        bool ok = false;

        if (!uefi_path_to_char16(path, path16, sizeof(path16) / sizeof(path16[0]))) {
            set_error("invalid UEFI storage path");
            return false;
        }

        if (!uefi_open_root(&root)) {
            return false;
        }

        if (!root->open || root->open(root,
                                      &file,
                                      path16,
                                      EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE,
                                      0) != EFI_SUCCESS || !file) {
            set_error("UEFI file open delete failed");
            if (root->close) {
                root->close(root);
            }
            return false;
        }

        if (file->delete_file && file->delete_file(file) == EFI_SUCCESS) {
            ok = true;
            set_error("ok");
        } else {
            set_error("UEFI file delete failed");
            if (file->close) {
                file->close(file);
            }
        }

        if (root->close) {
            root->close(root);
        }

        return ok;
    }
#endif
}

bool storage_rename_path(const char *old_path, const char *new_path) {
    static char rename_buffer[VITA_STORAGE_READ_MAX];

    if (!old_path || !new_path) {
        set_error("rename requires old and new paths");
        return false;
    }

    if (!path_allowed(old_path) || !path_allowed(new_path)) {
        set_error("rename paths must stay under /vita/ and must not contain '..' or ':'");
        return false;
    }

    if (path_is_protected_root(old_path) || path_is_protected_root(new_path)) {
        set_error("refusing to rename protected VitaOS storage root");
        return false;
    }

    if (!storage_read_text(old_path, rename_buffer, sizeof(rename_buffer))) {
        return false;
    }

    if (!storage_write_text(new_path, rename_buffer)) {
        return false;
    }

    if (!storage_delete_path(old_path)) {
        set_error("rename copied new file but could not delete old file");
        return false;
    }

    set_error("ok");
    return true;
}

'''
        marker = "bool storage_list_notes(void)"
        s = insert_before(s, marker, public_funcs, "storage delete/rename public functions")

    # Add command text to status output.
    if "storage delete /vita/notes/test.txt" not in s:
        old = "    console_write_line(\"notes list\");\n"
        new = old + "    console_write_line(\"storage delete /vita/notes/test.txt\");\n    console_write_line(\"storage rename /vita/notes/old.txt /vita/notes/new.txt\");\n"
        s = replace_once(s, old, new, "storage status command list")

    # Insert helper command functions before storage_handle_command.
    if "static bool storage_command_delete" not in s:
        helper_funcs = r'''
static bool storage_parse_two_paths(const char *args,
                                    char first[VITA_STORAGE_PATH_MAX],
                                    char second[VITA_STORAGE_PATH_MAX]) {
    const char *p;
    unsigned long i = 0;
    unsigned long j = 0;

    if (!first || !second) {
        return false;
    }

    first[0] = '\0';
    second[0] = '\0';

    p = skip_spaces(args);
    if (!p[0]) {
        return false;
    }

    while (p[i] && !is_space_char(p[i]) && (i + 1) < VITA_STORAGE_PATH_MAX) {
        first[i] = p[i];
        i++;
    }
    first[i] = '\0';

    p = skip_spaces(p + i);
    if (!p[0]) {
        return false;
    }

    while (p[j] && !is_space_char(p[j]) && (j + 1) < VITA_STORAGE_PATH_MAX) {
        second[j] = p[j];
        j++;
    }
    second[j] = '\0';

    return first[0] != '\0' && second[0] != '\0';
}

static bool storage_command_delete(const char *args) {
    char path[VITA_STORAGE_PATH_MAX];
    const char *p;
    unsigned long i = 0;

    p = skip_spaces(args);
    if (!p[0]) {
        console_write_line("usage: storage delete /vita/notes/test.txt");
        return false;
    }

    while (p[i] && !is_space_char(p[i]) && (i + 1) < sizeof(path)) {
        path[i] = p[i];
        i++;
    }
    path[i] = '\0';

    if (storage_delete_path(path)) {
        console_write_line("storage delete: removed");
        console_write_line(path);
        return true;
    }

    console_write_line("storage delete: failed");
    console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
    return false;
}

static bool storage_command_rename(const char *args) {
    char old_path[VITA_STORAGE_PATH_MAX];
    char new_path[VITA_STORAGE_PATH_MAX];

    if (!storage_parse_two_paths(args, old_path, new_path)) {
        console_write_line("usage: storage rename /vita/notes/old.txt /vita/notes/new.txt");
        return false;
    }

    if (storage_rename_path(old_path, new_path)) {
        console_write_line("storage rename: moved");
        console_write_line(old_path);
        console_write_line(new_path);
        return true;
    }

    console_write_line("storage rename: failed");
    console_write_line(g_storage.last_error[0] ? g_storage.last_error : "unknown");
    return false;
}

'''
        marker = "bool storage_handle_command(const char *cmd)"
        s = insert_before(s, marker, helper_funcs, "storage delete/rename command helpers")

    # Insert command branches inside storage_handle_command before fallback return false.
    if "storage rename " not in s[s.find("bool storage_handle_command"):]:
        branches = r'''
    if (starts_with(cmd, "storage delete ")) {
        return storage_command_delete(cmd + 15);
    }

    if (starts_with(cmd, "storage rm ")) {
        return storage_command_delete(cmd + 11);
    }

    if (starts_with(cmd, "storage rename ")) {
        return storage_command_rename(cmd + 15);
    }

    if (starts_with(cmd, "storage mv ")) {
        return storage_command_rename(cmd + 11);
    }

    if (starts_with(cmd, "delete ")) {
        return storage_command_delete(cmd + 7);
    }

    if (starts_with(cmd, "rm ")) {
        return storage_command_delete(cmd + 3);
    }

    if (starts_with(cmd, "rename ")) {
        return storage_command_rename(cmd + 7);
    }

    if (starts_with(cmd, "mv ")) {
        return storage_command_rename(cmd + 3);
    }

'''
        idx = s.find("bool storage_handle_command")
        if idx < 0:
            raise SystemExit("patch failed: storage_handle_command not found")
        tail = s[idx:]
        marker = "    return false;\n}"
        if marker not in tail:
            raise SystemExit("patch failed: final return false in storage_handle_command not found")
        tail = tail.replace(marker, branches + marker, 1)
        s = s[:idx] + tail

    write(str(path), s)


def patch_console_c() -> None:
    path = Path("kernel/console.c")
    s = read(str(path))
    if "- rm / delete <path>" not in s:
        old = "    console_write_line(\"- clear\");\n"
        new = old + "    console_write_line(\"- rm / delete <path>\");\n    console_write_line(\"- mv / rename <old> <new>\");\n"
        if old in s:
            s = s.replace(old, new, 1)
    if "rm PATH     -> delete a file under /vita" not in s:
        old = "    console_write_line(\"clear       -> clear screen and redraw guided header\");\n"
        new = old + "    console_write_line(\"rm PATH     -> delete a file under /vita\");\n    console_write_line(\"mv OLD NEW  -> rename a small text file under /vita\");\n"
        if old in s:
            s = s.replace(old, new, 1)
    write(str(path), s)


def patch_command_core_c() -> None:
    path = Path("kernel/command_core.c")
    s = read(str(path))
    if "#include <vita/storage.h>" not in s:
        s = s.replace("#include <vita/proposal.h>\n", "#include <vita/proposal.h>\n#include <vita/storage.h>\n", 1)

    if "COMMAND_STORAGE_MUTATE" not in s:
        branch = r'''
    if (starts_with(cmd, "rm ") || starts_with(cmd, "delete ") ||
        starts_with(cmd, "mv ") || starts_with(cmd, "rename ")) {
        audit_emit_boot_event("COMMAND_STORAGE_MUTATE", cmd);
        if (!storage_handle_command(cmd)) {
            console_write_line("storage command failed / comando de almacenamiento no ejecutado");
        }
        return VITA_COMMAND_CONTINUE;
    }

'''
        marker = "    if (starts_with(cmd, \"approve \") || starts_with(cmd, \"reject \")) {\n"
        if marker not in s:
            # Fallback: insert before shutdown branch.
            marker = "    if (str_eq(cmd, \"shutdown\") || str_eq(cmd, \"exit\")) {\n"
        s = insert_before(s, marker, branch, "storage mutate command routing")
    write(str(path), s)


def main() -> None:
    patch_storage_h()
    patch_storage_c()
    patch_console_c()
    patch_command_core_c()
    print("storage delete/rename v10 patch applied")


if __name__ == "__main__":
    main()
