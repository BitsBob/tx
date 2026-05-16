#ifndef LSP_JSON_H
#define LSP_JSON_H

#include <stddef.h>

/* Tiny JSON helpers operating on [start, end) byte spans. Tolerant only of
 * well-formed JSON — sufficient for parsing the message shapes LSP servers
 * emit. */

const char *jsonSkipWS(const char *p, const char *end);
const char *jsonSkipValue(const char *p, const char *end);

/* Find a key at the current object depth. `start` should point at '{' or
 * just after one. Returns a pointer to the value (whitespace skipped), or
 * NULL if the key isn't present. */
const char *jsonFindKey(const char *start, const char *end, const char *key);

/* Parse a JSON string value (must point at opening quote). Returns a
 * malloc'd null-terminated buffer with escapes decoded; caller frees.
 * `*after` (if non-NULL) gets the position past the closing quote. */
char *jsonParseString(const char *p, const char *end, const char **after);

/* Parse a JSON integer. Stores into *out. `*after` (if non-NULL) gets the
 * position past the number. Returns 0 on success, -1 on failure. */
int jsonParseInt(const char *p, const char *end, long *out, const char **after);

/* Iterate elements of a JSON array (`p` at '['). For each element the
 * callback receives [e_start, e_end). Return non-zero from `cb` to stop. */
typedef int (*jsonArrayCb)(const char *e_start, const char *e_end, void *ud);
int jsonForEachArrayElem(const char *p, const char *end,
                         jsonArrayCb cb, void *ud);

#endif
