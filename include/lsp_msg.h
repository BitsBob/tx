#ifndef LSP_MSG_H
#define LSP_MSG_H

/* Build LSP JSON-RPC messages. All functions return a malloc'd string
 * the caller must free, or NULL on allocation failure. */

char *lspBuildFileUri(const char *filename);
char *lspBuildInitialize(int id, const char *root_uri);
char *lspBuildInitialized(void);
char *lspBuildDidOpen(const char *uri, const char *language_id,
                      int version, const char *text);
char *lspBuildDidChange(const char *uri, int version, const char *text);
char *lspBuildDidSave(const char *uri);
char *lspBuildDidClose(const char *uri);
char *lspBuildHover(int id, const char *uri, int line, int character);
char *lspBuildShutdown(int id);
char *lspBuildExit(void);

#endif
