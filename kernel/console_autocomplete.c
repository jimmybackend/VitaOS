#include <vita/console.h>
#include <vita/console_autocomplete.h>
#include <vita/storage.h>

#ifdef VITA_HOSTED
#include <dirent.h>
#endif

#define VITA_AC_MATCH_MAX 32U
#define VITA_AC_PATH_MAX 64U

static const char *g_commands[] = {
    "status", "hw", "audit", "audit status", "audit report", "audit gate", "audit readiness",
    "audit verify", "audit verify-chain", "audit hash", "audit hash-chain",
    "audit export", "audit verify export", "audit export verify", "export audit verify",
    "audit events", "audit export events", "export audit events",
    "audit sqlite", "audit sqlite summary", "audit db", "audit db summary",
    "peers", "proposals", "list", "emergency", "helpme", "help", "menu", "clear", "cls",
    "note", "append", "edit", "notes list",
    "journal status", "journal summary",
    "storage status", "storage check", "storage last-error", "storage read", "storage notes", "storage notes list",
    "export session", "export jsonl", "diagnostic", "diag", "export diagnostic",
    "export index", "export manifest", "export list", "exports", "storage export-index",
    "selftest", "self-test", "boot selftest", "boot self-test", "checkup",
    "shutdown", "exit"
};

static const char *g_known_paths[] = {
    "/vita/", "/vita/notes/", "/vita/audit/", "/vita/audit/sessions/", "/vita/export/",
    "/vita/export/reports/", "/vita/tmp/", "/vita/notes/usb-test.txt", "/vita/audit/session-journal.txt",
    "/vita/audit/session-journal.jsonl", "/vita/export/reports/last-session.txt",
    "/vita/export/reports/last-session.jsonl", "/vita/export/reports/diagnostic-bundle.txt",
    "/vita/export/reports/self-test.txt"
};

static unsigned long str_len(const char *s){unsigned long n=0;while(s&&s[n])n++;return n;}
static bool starts_with(const char *s,const char *p){unsigned long i=0;while(s&&p&&p[i]){if(s[i]!=p[i])return false;i++;}return true;}
static void str_copy(char *d,unsigned long c,const char *s){unsigned long i=0;if(!d||!c)return; if(!s){d[0]='\0';return;} while(s[i]&&(i+1)<c){d[i]=s[i];i++;} d[i]='\0';}

static void add_match(const char *candidate, const char **matches, unsigned long *count) {
    if (*count < VITA_AC_MATCH_MAX) {
        matches[*count] = candidate;
        (*count)++;
    }
}

static void gather_command_matches(const char *prefix, const char **matches, unsigned long *count) {
    unsigned long i;
    for (i = 0; i < sizeof(g_commands)/sizeof(g_commands[0]); ++i) {
        if (starts_with(g_commands[i], prefix)) {
            add_match(g_commands[i], matches, count);
        }
    }
}

#ifdef VITA_HOSTED
static void gather_hosted_note_paths(const char *prefix, char out[VITA_AC_PATH_MAX][VITA_STORAGE_PATH_MAX], unsigned long *out_count) {
    DIR *d = opendir("build/storage/vita/notes");
    struct dirent *ent;
    unsigned long i;
    if (!d) return;
    while ((ent = readdir(d)) != 0) {
        unsigned long n = 0;
        if (ent->d_name[0] == '.') continue;
        str_copy(out[*out_count], VITA_STORAGE_PATH_MAX, "/vita/notes/");
        n = str_len(out[*out_count]);
        str_copy(out[*out_count] + n, VITA_STORAGE_PATH_MAX - n, ent->d_name);
        if (starts_with(out[*out_count], prefix) && *out_count < VITA_AC_PATH_MAX) {
            (*out_count)++;
        }
    }
    closedir(d);
    for (i = 0; i < *out_count; ++i) {
        (void)i;
    }
}
#endif

static void gather_path_matches(const char *prefix, const char **matches, unsigned long *count) {
    unsigned long i;
#ifdef VITA_HOSTED
    static char hosted_paths[VITA_AC_PATH_MAX][VITA_STORAGE_PATH_MAX];
    unsigned long hosted_count = 0;
#endif
    for (i = 0; i < sizeof(g_known_paths)/sizeof(g_known_paths[0]); ++i) {
        if (starts_with(g_known_paths[i], prefix)) {
            add_match(g_known_paths[i], matches, count);
        }
    }
#ifdef VITA_HOSTED
    gather_hosted_note_paths(prefix, hosted_paths, &hosted_count);
    for (i = 0; i < hosted_count; ++i) {
        add_match(hosted_paths[i], matches, count);
    }
#endif
}

bool console_autocomplete_line(char *line, unsigned long cap) {
    const char *matches[VITA_AC_MATCH_MAX];
    unsigned long count = 0;
    unsigned long i;
    char prefix[VITA_CONSOLE_LINE_MAX];
    char *tab;

    if (!line || cap == 0) return false;
    tab = 0;
    for (i = 0; line[i]; ++i) if (line[i] == '\t') { tab = &line[i]; break; }
    if (!tab) return false;

    *tab = '\0';
    str_copy(prefix, sizeof(prefix), line);

    if (prefix[0] == '\0') {
        console_write_line("Autocomplete:");
        gather_command_matches("", matches, &count);
    } else if (starts_with(prefix, "edit ") || starts_with(prefix, "append ") || starts_with(prefix, "storage read ")) {
        const char *path_prefix = prefix + (starts_with(prefix, "storage read ") ? 13 : 5);
        gather_path_matches(path_prefix, matches, &count);
    } else {
        gather_command_matches(prefix, matches, &count);
    }

    if (count == 0) {
        console_write_line("sin coincidencias");
        console_write_line("Path autocomplete currently uses known VitaOS paths unless storage directory listing is available.");
        return false;
    }

    if (count == 1) {
        str_copy(line, cap, matches[0]);
        if (starts_with(matches[0], "storage") || starts_with(matches[0], "audit") || starts_with(matches[0], "journal") || starts_with(matches[0], "export")) {
            unsigned long n = str_len(line);
            if (n > 0 && n + 1 < cap && line[n - 1] != ' ') {
                line[n] = ' ';
                line[n + 1] = '\0';
            }
        }
        return true;
    }

    console_write_line("Autocomplete options:");
    for (i = 0; i < count; ++i) {
        console_write_line(matches[i]);
    }
    return false;
}
