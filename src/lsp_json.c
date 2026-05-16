#include "tx.h"
#include "lsp_json.h"

const char *jsonSkipWS(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        p++;
    return p;
}

static const char *skip_string(const char *p, const char *end) {
    if (p >= end || *p != '"') return NULL;
    p++;
    while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) p += 2;
        else p++;
    }
    return (p < end) ? p + 1 : NULL;
}

const char *jsonSkipValue(const char *p, const char *end) {
    p = jsonSkipWS(p, end);
    if (p >= end) return p;
    if (*p == '"') {
        const char *r = skip_string(p, end);
        return r ? r : end;
    }
    if (*p == '{' || *p == '[') {
        char open = *p, close = (open == '{') ? '}' : ']';
        int depth = 1;
        p++;
        while (p < end && depth > 0) {
            if (*p == '"') {
                const char *r = skip_string(p, end);
                if (!r) return end;
                p = r;
                continue;
            }
            if (*p == open)  depth++;
            else if (*p == close) depth--;
            p++;
        }
        return p;
    }
    while (p < end && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
        p++;
    return p;
}

const char *jsonFindKey(const char *start, const char *end, const char *key) {
    const char *p = jsonSkipWS(start, end);
    if (p >= end) return NULL;
    if (*p == '{') p++;

    size_t klen = strlen(key);
    while (p < end) {
        p = jsonSkipWS(p, end);
        if (p >= end || *p == '}') return NULL;
        if (*p != '"') return NULL;

        const char *k_after = skip_string(p, end);
        if (!k_after) return NULL;
        size_t this_klen = (size_t)(k_after - p - 2);

        const char *p2 = jsonSkipWS(k_after, end);
        if (p2 >= end || *p2 != ':') return NULL;
        p2 = jsonSkipWS(p2 + 1, end);

        if (this_klen == klen && memcmp(p + 1, key, klen) == 0)
            return p2;

        p = jsonSkipWS(jsonSkipValue(p2, end), end);
        if (p < end && *p == ',') p++;
    }
    return NULL;
}

char *jsonParseString(const char *p, const char *end, const char **after) {
    if (p >= end || *p != '"') return NULL;
    p++;

    size_t cap = (size_t)(end - p) + 1;
    char  *out = malloc(cap);
    if (!out) return NULL;

    size_t j = 0;
    while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
            switch (p[1]) {
            case '"':  out[j++] = '"';  break;
            case '\\': out[j++] = '\\'; break;
            case '/':  out[j++] = '/';  break;
            case 'b':  out[j++] = '\b'; break;
            case 'f':  out[j++] = '\f'; break;
            case 'n':  out[j++] = '\n'; break;
            case 'r':  out[j++] = '\r'; break;
            case 't':  out[j++] = '\t'; break;
            case 'u':
                if (p + 5 < end) {
                    char hex[5] = { p[2], p[3], p[4], p[5], '\0' };
                    long cp = strtol(hex, NULL, 16);
                    if (cp < 0x80) {
                        out[j++] = (char)cp;
                    } else if (cp < 0x800) {
                        out[j++] = (char)(0xC0 | (cp >> 6));
                        out[j++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        out[j++] = (char)(0xE0 | (cp >> 12));
                        out[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[j++] = (char)(0x80 | (cp & 0x3F));
                    }
                    p += 6;
                    continue;
                }
                break;
            default:
                out[j++] = p[1];
            }
            p += 2;
        } else {
            out[j++] = *p++;
        }
    }
    if (p >= end) { free(out); return NULL; }
    out[j] = '\0';
    if (after) *after = p + 1;
    return out;
}

int jsonParseInt(const char *p, const char *end, long *out, const char **after) {
    char  buf[32];
    size_t avail = (size_t)(end - p);
    if (avail > sizeof(buf) - 1) avail = sizeof(buf) - 1;
    memcpy(buf, p, avail);
    buf[avail] = '\0';

    char *e;
    long  v = strtol(buf, &e, 10);
    if (e == buf) return -1;
    if (after) *after = p + (e - buf);
    *out = v;
    return 0;
}

int jsonForEachArrayElem(const char *p, const char *end,
                         jsonArrayCb cb, void *ud) {
    p = jsonSkipWS(p, end);
    if (p >= end || *p != '[') return -1;
    p++;
    while (p < end) {
        p = jsonSkipWS(p, end);
        if (p >= end) return -1;
        if (*p == ']') return 0;

        const char *e_start = p;
        const char *e_end   = jsonSkipValue(p, end);
        int rc = cb(e_start, e_end, ud);
        if (rc != 0) return rc;

        p = jsonSkipWS(e_end, end);
        if (p < end && *p == ',') p++;
    }
    return -1;
}
