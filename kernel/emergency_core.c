/*
 * kernel/emergency_core.c
 * Emergency interaction core.
 *
 * Responsibilities:
 * - accept free-text descriptions
 * - classify emergency topic
 * - load relevant knowledge pack
 * - ask critical follow-up questions
 * - produce structured operational output
 */

#include <stdint.h>

typedef enum {
    EMERG_UNKNOWN = 0,
    EMERG_FIRST_AID,
    EMERG_LOST_PERSON,
    EMERG_WATER,
    EMERG_FOOD,
    EMERG_SHELTER,
    EMERG_WALK_TO_HELP
} emergency_topic_t;

emergency_topic_t emergency_classify_text(const char *text) {
    (void)text;
    /* TODO:
     * implement very small classifier for F1:
     * - keyword rules in es/en
     * - no heavy NLP required initially
     */
    return EMERG_UNKNOWN;
}

/* TODO:
 * - emergency_load_knowledge_pack()
 * - emergency_generate_questions()
 * - emergency_generate_actions()
 * - emergency_render_structured_response()
 * - audit every emergency session and proposal
 */
