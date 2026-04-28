#ifndef VITA_SESSION_JOURNAL_H
#define VITA_SESSION_JOURNAL_H

#include <stdbool.h>

bool session_journal_init(void);
bool session_journal_is_active(void);
void session_journal_log_command(const char *command);
void session_journal_log_system(const char *event_type, const char *summary);
void session_journal_show_status(void);
bool session_journal_flush(void);
bool session_journal_handle_command(const char *cmd);

#endif
