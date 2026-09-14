#include "app_internal.h"

const char *json_key(const char *json, const char *key)
{
    char needle[96];
    const char *value;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    value = strstr(json, needle);
    if (value == NULL) return NULL;
    value = strchr(value + strlen(needle), ':');
    if (value == NULL) return NULL;
    do { value++; } while (*value == ' ' || *value == '\t');
    return value;
}

int json_string(const char *json, const char *key, char *output, size_t size)
{
    const char *value = json_key(json, key);
    size_t used = 0;
    if (value == NULL || *value != '"' || size == 0) return 0;
    value++;
    while (*value != '\0' && *value != '"' && used + 1 < size) {
        if (*value == '\\' && value[1] != '\0') {
            value++;
            if (*value == 'n') output[used++] = ' ';
            else if (*value == 't') output[used++] = ' ';
            else output[used++] = *value;
        } else output[used++] = *value;
        value++;
    }
    output[used] = '\0';
    return 1;
}

int json_number(const char *json, const char *key, double *output)
{
    const char *value = json_key(json, key);
    char *end;
    if (value == NULL) return 0;
    *output = strtod(value, &end);
    return end != value;
}

int json_boolean(const char *json, const char *key, int *output)
{
    const char *value = json_key(json, key);
    if (value == NULL) return 0;
    if (strncmp(value, "true", 4) == 0) { *output = 1; return 1; }
    if (strncmp(value, "false", 5) == 0) { *output = 0; return 1; }
    return 0;
}
