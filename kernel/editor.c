/*
 * kernel/editor.c
 * Minimal line-oriented VitaOS note editor for F1A/F1B.
 *
 * Scope:
 * - text-only, console-driven editor
 * - no malloc
 * - writes through storage_write_text()
 * - designed for UEFI Simple Text Input and hosted stdin
 * - not a full nano clone yet
 */

#include <vita/console.h>
#include <vita/editor.h>
#include <vita/storage.h>

#define VITA_EDITOR_TEXT_MAX 1536U
#define VITA_EDITOR_LINE_MAX VITA_CONSOLE_LINE_MAX

typedef enum {
    EDITOR_SAVE = 0,
    EDITOR_CANCEL = 1,
    EDITOR_ERROR = 2
} editor_result_t;

static char g_editor_text[VITA_EDITOR_TEXT_MAX];
static char g_editor_path[VITA_STORAGE_PATH_MAX];

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

static unsigned long str_len(const char *s) {
    unsigned long n = 0;

    if (!s) {
        return 0;
    }

    while (s[n]) {
        n++;
    }

    return n;
}

static bool str_eq(const char *a, const char *b) {
    unsigned long i = 0;

    if (!a || !b) {
        return false;
    }

    while (a[i] && b[i]) {
        if (a[i] != b[i]) {
            return false;
        }
        i++;
    }

    return a[i] == b[i];
}

static bool starts_with(const char *s, const char *prefix) {
    unsigned long i = 0;

    if (!s || !prefix) {
        return false;
    }

    while (prefix[i]) {
        if (s[i] != prefix[i]) {
            return false;
        }
        i++;
    }

    return true;
}

static bool is_space_char(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_spaces(const char *s) {
    if (!s) {
        return "";
    }

    while (*s && is_space_char(*s)) {
        s++;
    }

    return s;
}

static void trim_right_in_place(char *s) {
    unsigned long n;

    if (!s) {
        return;
    }

    n = str_len(s);
    while (n > 0 && is_space_char(s[n - 1])) {
        s[--n] = '\0';
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

    while (src[i] && (i + 1) < cap) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static void str_append(char *dst, unsigned long cap, const char *src) {
    unsigned long i = 0;
    unsigned long j = 0;

    if (!dst || cap == 0 || !src) {
        return;
    }

    while (i < cap && dst[i]) {
        i++;
    }

    if (i >= cap) {
        return;
    }

    while (src[j] && (i + 1) < cap) {
        dst[i++] = src[j++];
    }

    dst[i] = '\0';
}

static bool path_allowed(const char *path) {
    unsigned long i;

    if (!path || !starts_with(path, "/vita/")) {
        return false;
    }

    for (i = 0; path[i]; ++i) {
        char c = path[i];

        if (c == ':' || (c == '.' && path[i + 1] == '.')) {
            return false;
        }

        if ((unsigned char)c < 32U) {
            return false;
        }
    }

    return true;
}

static bool safe_note_name_char(char c) {
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    return c == '-' || c == '_' || c == '.';
}

static bool build_note_path(const char *name, char *out, unsigned long out_cap) {
    unsigned long i = 0;
    bool has_dot = false;

    if (!out || out_cap == 0) {
        return false;
    }

    out[0] = '\0';

    if (!name || !name[0]) {
        str_copy(out, out_cap, "/vita/notes/note.txt");
        return true;
    }

    if (name[0] == '/') {
        if (!path_allowed(name)) {
            return false;
        }
        str_copy(out, out_cap, name);
        return out[0] != '\0';
    }

    str_copy(out, out_cap, "/vita/notes/");

    while (name[i] && !is_space_char(name[i])) {
        if (!safe_note_name_char(name[i])) {
            return false;
        }
        if (name[i] == '.') {
            has_dot = true;
        }
        i++;
    }

    if (i == 0) {
        return false;
    }

    {
        char name_copy[64];
        unsigned long j;

        for (j = 0; j < i && (j + 1) < sizeof(name_copy); ++j) {
            name_copy[j] = name[j];
        }
        name_copy[j] = '\0';

        str_append(out, out_cap, name_copy);
    }

    if (!has_dot) {
        str_append(out, out_cap, ".txt");
    }

    return path_allowed(out);
}

static void editor_show_help(void) {
    console_write_line("VitaOS editor / Editor VitaOS:");
    console_write_line("Write one line at a time / Escribe una linea a la vez");
    console_write_line(".save   -> save file / guardar archivo");
    console_write_line(".cancel -> cancel / cancelar");
    console_write_line(".show   -> show current text / mostrar texto actual");
    console_write_line(".clear  -> clear current text / limpiar texto actual");
    console_write_line(".help   -> show editor help / mostrar ayuda");
    console_write_line("Limit: 1536 bytes per note in this first editor slice.");
}

static void editor_show_current_text(void) {
    console_write_line("--- note text / texto actual ---");
    if (g_editor_text[0]) {
        console_write_line(g_editor_text);
    } else {
        console_write_line("(empty / vacio)");
    }
    console_write_line("--- end / fin ---");
}

static bool editor_append_line(const char *line) {
    unsigned long used = str_len(g_editor_text);
    unsigned long add = str_len(line);

    if (used + add + 2 >= sizeof(g_editor_text)) {
        console_write_line("editor: text buffer full / buffer de texto lleno");
        return false;
    }

    str_append(g_editor_text, sizeof(g_editor_text), line ? line : "");
    str_append(g_editor_text, sizeof(g_editor_text), "\n");
    return true;
}

static editor_result_t editor_run(const char *path) {
    char line[VITA_EDITOR_LINE_MAX];

    if (!path_allowed(path)) {
        console_write_line("editor: path must be inside /vita/ and cannot contain '..' or ':'");
        return EDITOR_ERROR;
    }

    if (!storage_is_ready()) {
        console_write_line("editor: storage is not writable / almacenamiento no escribible");
        storage_show_status();
        return EDITOR_ERROR;
    }

    mem_zero(g_editor_text, sizeof(g_editor_text));
    str_copy(g_editor_path, sizeof(g_editor_path), path);

    console_pager_end();
    console_write_line("VitaOS note editor / Editor de notas VitaOS");
    console_write_line("Target / Destino:");
    console_write_line(g_editor_path);
    editor_show_help();

    for (;;) {
        console_write_raw("edit> ");

        if (!console_read_line(line, sizeof(line))) {
            console_write_line("editor: input closed / entrada cerrada");
            return EDITOR_ERROR;
        }

        trim_right_in_place(line);

        if (str_eq(line, ".save")) {
            if (storage_write_text(g_editor_path, g_editor_text)) {
                console_write_line("editor: saved / guardado");
                console_write_line(g_editor_path);
                return EDITOR_SAVE;
            }

            console_write_line("editor: save failed / fallo al guardar");
            storage_show_status();
            return EDITOR_ERROR;
        }

        if (str_eq(line, ".cancel")) {
            console_write_line("editor: cancelled / cancelado");
            return EDITOR_CANCEL;
        }

        if (str_eq(line, ".help")) {
            editor_show_help();
            continue;
        }

        if (str_eq(line, ".show")) {
            editor_show_current_text();
            continue;
        }

        if (str_eq(line, ".clear")) {
            mem_zero(g_editor_text, sizeof(g_editor_text));
            console_write_line("editor: current text cleared / texto actual borrado");
            continue;
        }

        (void)editor_append_line(line);
    }
}

static void editor_show_notes_help(void) {
    console_write_line("Notes / Notas:");
    console_write_line("note                 -> edit /vita/notes/note.txt");
    console_write_line("note field.txt       -> edit /vita/notes/field.txt");
    console_write_line("note report          -> edit /vita/notes/report.txt");
    console_write_line("edit /vita/notes/a.txt -> edit an explicit /vita path");
    console_write_line("Inside editor use .save, .cancel, .show, .clear, .help");
}

bool editor_handle_command(const char *cmd) {
    const char *args;
    char path[VITA_STORAGE_PATH_MAX];
    editor_result_t result;

    if (!cmd) {
        return false;
    }

    if (str_eq(cmd, "notes")) {
        editor_show_notes_help();
        return true;
    }

    if (str_eq(cmd, "note") || starts_with(cmd, "note ")) {
        args = str_eq(cmd, "note") ? "" : skip_spaces(cmd + str_len("note "));
        if (!build_note_path(args, path, sizeof(path))) {
            console_write_line("note: invalid note name or path");
            console_write_line("Use: note, note file.txt, or edit /vita/notes/file.txt");
            return false;
        }

        result = editor_run(path);
        return result == EDITOR_SAVE || result == EDITOR_CANCEL;
    }

    if (starts_with(cmd, "edit ")) {
        args = skip_spaces(cmd + str_len("edit "));
        if (!args[0]) {
            console_write_line("usage: edit /vita/notes/file.txt");
            return false;
        }

        str_copy(path, sizeof(path), args);
        trim_right_in_place(path);

        if (!path_allowed(path)) {
            console_write_line("edit: path must be inside /vita/ and cannot contain '..' or ':'");
            return false;
        }

        result = editor_run(path);
        return result == EDITOR_SAVE || result == EDITOR_CANCEL;
    }

    return false;
}
