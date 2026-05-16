#include "tx.h"
#include "lsp.h"

#include <limits.h>
#include <stdlib.h>

static char *json_escape(const char *s) {
    if (!s) return strdup("");

    size_t in_len = strlen(s);
    char  *out    = malloc(in_len * 6 + 1);
    if (!out) return NULL;

    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  out[j++] = '\\'; out[j++] = '"';  break;
        case '\\': out[j++] = '\\'; out[j++] = '\\'; break;
        case '\b': out[j++] = '\\'; out[j++] = 'b';  break;
        case '\f': out[j++] = '\\'; out[j++] = 'f';  break;
        case '\n': out[j++] = '\\'; out[j++] = 'n';  break;
        case '\r': out[j++] = '\\'; out[j++] = 'r';  break;
        case '\t': out[j++] = '\\'; out[j++] = 't';  break;
        default:
            if (c < 0x20)
                j += (size_t)sprintf(out + j, "\\u%04x", c);
            else
                out[j++] = (char)c;
        }
    }
    out[j] = '\0';
    return out;
}

char *lspBuildFileUri(const char *filename) {
    if (!filename || !*filename) return NULL;

    char abs[PATH_MAX];
    if (filename[0] == '/') {
        snprintf(abs, sizeof(abs), "%s", filename);
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) return NULL;
        snprintf(abs, sizeof(abs), "%s/%s", cwd, filename);
    }

    /* Canonicalize if we can — but never fail just because the file
     * doesn't exist on disk yet. */
    char canon[PATH_MAX];
    if (realpath(abs, canon))
        snprintf(abs, sizeof(abs), "%s", canon);

    char  *esc = json_escape(abs);
    if (!esc) return NULL;
    size_t cap = strlen(esc) + sizeof("file://");
    char  *uri = malloc(cap);
    if (!uri) { free(esc); return NULL; }
    snprintf(uri, cap, "file://%s", esc);
    free(esc);
    return uri;
}

char *lspBuildInitialize(int id, const char *root_uri) {
    const char *fmt =
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"initialize\","
        "\"params\":{\"processId\":null,"
        "\"rootUri\":%s%s%s,"
        "\"capabilities\":{\"textDocument\":{"
        "\"synchronization\":{\"didSave\":true},"
        "\"publishDiagnostics\":{\"relatedInformation\":false},"
        "\"hover\":{\"contentFormat\":[\"plaintext\",\"markdown\"]}}}}}";

    const char *root_open  = root_uri ? "\"" : "";
    const char *root_value = root_uri ? root_uri : "null";
    const char *root_close = root_uri ? "\"" : "";

    size_t cap = strlen(fmt) + strlen(root_value) + 64;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    snprintf(buf, cap, fmt, id, root_open, root_value, root_close);
    return buf;
}

char *lspBuildInitialized(void) {
    return strdup(
        "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}");
}

char *lspBuildDidOpen(const char *uri, const char *language_id, int version,
                      const char *text) {
    char *text_esc = json_escape(text);
    if (!text_esc) return NULL;

    size_t cap = strlen(text_esc) + strlen(uri) + strlen(language_id) + 256;
    char  *buf = malloc(cap);
    if (!buf) { free(text_esc); return NULL; }
    snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\","
        "\"params\":{\"textDocument\":{"
        "\"uri\":\"%s\",\"languageId\":\"%s\",\"version\":%d,\"text\":\"%s\"}}}",
        uri, language_id, version, text_esc);
    free(text_esc);
    return buf;
}

char *lspBuildDidChange(const char *uri, int version, const char *text) {
    char *text_esc = json_escape(text);
    if (!text_esc) return NULL;

    size_t cap = strlen(text_esc) + strlen(uri) + 256;
    char  *buf = malloc(cap);
    if (!buf) { free(text_esc); return NULL; }
    snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\",\"version\":%d},"
        "\"contentChanges\":[{\"text\":\"%s\"}]}}",
        uri, version, text_esc);
    free(text_esc);
    return buf;
}

char *lspBuildDidSave(const char *uri) {
    size_t cap = strlen(uri) + 128;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didSave\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"}}}",
        uri);
    return buf;
}

char *lspBuildDidClose(const char *uri) {
    size_t cap = strlen(uri) + 128;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didClose\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"}}}",
        uri);
    return buf;
}

char *lspBuildHover(int id, const char *uri, int line, int character) {
    size_t cap = strlen(uri) + 256;
    char  *buf = malloc(cap);
    if (!buf) return NULL;
    snprintf(buf, cap,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"textDocument/hover\","
        "\"params\":{\"textDocument\":{\"uri\":\"%s\"},"
        "\"position\":{\"line\":%d,\"character\":%d}}}",
        id, uri, line, character);
    return buf;
}

char *lspBuildShutdown(int id) {
    char *buf = malloc(96);
    if (!buf) return NULL;
    snprintf(buf, 96,
        "{\"jsonrpc\":\"2.0\",\"id\":%d,\"method\":\"shutdown\"}", id);
    return buf;
}

char *lspBuildExit(void) {
    return strdup("{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}");
}
