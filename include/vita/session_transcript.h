#ifndef VITA_SESSION_TRANSCRIPT_H
#define VITA_SESSION_TRANSCRIPT_H

#include <stdbool.h>
#include <vita/boot.h>

bool session_transcript_init(const vita_handoff_t *handoff, const char *arch_name);
void session_transcript_shutdown(void);
bool session_transcript_is_active(void);
void session_transcript_log_user_input(const char *text);
void session_transcript_log_command_executed(const char *text);
void session_transcript_log_editor_event(const char *event_type, const char *text);
void session_transcript_log_storage_status(const char *text);
void session_transcript_log_journal_status(const char *text);
void session_transcript_log_system_output(const char *text, bool from_raw);
const char *session_transcript_txt_path(void);
const char *session_transcript_jsonl_path(void);
const char *session_transcript_state(void);

#endif
