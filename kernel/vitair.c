#include <vita/vitair.h>

bool vita_tri_is_valid(vita_tri_t state) {
    return state == VITA_TRI_NO || state == VITA_TRI_UNKNOWN || state == VITA_TRI_YES;
}

bool vita_ir_severity_is_valid(vita_ir_severity_t severity) {
    return severity == VITA_IR_SEVERITY_INFO ||
           severity == VITA_IR_SEVERITY_WARN ||
           severity == VITA_IR_SEVERITY_ERROR ||
           severity == VITA_IR_SEVERITY_CRITICAL;
}

const char *vita_tri_to_symbol(vita_tri_t state) {
    switch (state) {
        case VITA_TRI_YES:
            return "+1";
        case VITA_TRI_UNKNOWN:
            return "0";
        case VITA_TRI_NO:
            return "-1";
        default:
            return "0";
    }
}

const char *vita_tri_to_json_number(vita_tri_t state) {
    switch (state) {
        case VITA_TRI_YES:
            return "1";
        case VITA_TRI_UNKNOWN:
            return "0";
        case VITA_TRI_NO:
            return "-1";
        default:
            return "0";
    }
}

const char *vita_tri_to_word(vita_tri_t state) {
    switch (state) {
        case VITA_TRI_YES:
            return "yes";
        case VITA_TRI_UNKNOWN:
            return "unknown";
        case VITA_TRI_NO:
            return "no";
        default:
            return "unknown";
    }
}

const char *vita_ir_severity_to_string(vita_ir_severity_t severity) {
    switch (severity) {
        case VITA_IR_SEVERITY_INFO:
            return "info";
        case VITA_IR_SEVERITY_WARN:
            return "warn";
        case VITA_IR_SEVERITY_ERROR:
            return "error";
        case VITA_IR_SEVERITY_CRITICAL:
            return "critical";
        default:
            return "unknown";
    }
}
