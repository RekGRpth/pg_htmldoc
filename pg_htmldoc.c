#include <postgres.h>

#include <catalog/pg_authid.h>
#include <catalog/pg_type.h>
#include <miscadmin.h>
#include <utils/acl.h>
#include <utils/builtins.h>
#if PG_VERSION_NUM >= 160000
#include <varatt.h>
#endif

#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

PG_MODULE_MAGIC;

/* Mirrors the pg_read_server_files/pg_write_server_files/pg_execute_server_program
 * checks that COPY performs in DoCopy() (src/backend/commands/copy.c): access to the
 * server filesystem or network is gated on role membership rather than EXECUTE grants,
 * so it can't be bypassed by granting EXECUTE on these functions to PUBLIC. */
#if PG_VERSION_NUM >= 110000
#define PGHTMLDOC_ROLE_READ_SERVER_FILES      ROLE_PG_READ_SERVER_FILES
#define PGHTMLDOC_ROLE_WRITE_SERVER_FILES     ROLE_PG_WRITE_SERVER_FILES
#define PGHTMLDOC_ROLE_EXECUTE_SERVER_PROGRAM ROLE_PG_EXECUTE_SERVER_PROGRAM
#else
#define PGHTMLDOC_ROLE_READ_SERVER_FILES      InvalidOid
#define PGHTMLDOC_ROLE_WRITE_SERVER_FILES     InvalidOid
#define PGHTMLDOC_ROLE_EXECUTE_SERVER_PROGRAM InvalidOid
#endif

static void require_role(Oid role, const char *rolename, const char *action) {
#if PG_VERSION_NUM >= 110000
    if (!has_privs_of_role(GetUserId(), role))
#else
    (void)role;
    if (!superuser())
#endif
        ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to %s", action), errdetail("Only roles with privileges of the \"%s\" role may %s.", rolename, action)));
}

static bool cleanup = false;
#if PG_VERSION_NUM >= 90500
static struct MemoryContextCallback documentMemoryContextCallback = {0};
#endif
static tree_t *document = NULL;

void _PG_init(void); void _PG_init(void) {
    if (!_htmlInitialized) htmlSetCharSet("utf-8");
}

#if PG_VERSION_NUM >= 90500
static void documentMemoryContextCallbackFunction(void *arg) {
    if (!cleanup) return;
    if (document) htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    document = NULL;
    cleanup = false;
}
#endif

static void read_fileurl(tree_t **document, const char *fileurl, const char *path) {
    const char *base = file_directory(fileurl);
    const char *realname = file_find(path, fileurl);
    FILE *in;
    tree_t *file;
    if (!base) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!file_directory(\"%s\")", fileurl)));
    if (!realname) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!file_find(\"%s\", \"%s\")", path, fileurl)));
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!htmlAddTree")));
    if (!*document) *document = file; else {
        while ((*document)->next) *document = (*document)->next;
        (*document)->next = file;
        file->prev = *document;
    }
#if PG_VERSION_NUM >= 90500
    if (!documentMemoryContextCallback.func) {
        documentMemoryContextCallback.func = documentMemoryContextCallbackFunction;
        MemoryContextRegisterResetCallback(CurrentMemoryContext, &documentMemoryContextCallback);
    }
#endif
    htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)fileurl);
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(fileurl));
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
    if (!(in = fopen(realname, "rb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fopen(\"%s\")", realname)));
    htmlReadFile(file, in, base);
    fclose(in);
}

static void read_html(tree_t **document, const char *html, size_t len) {
    FILE *in;
    tree_t *file;
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!htmlAddTree")));
    if (!*document) *document = file; else {
        while ((*document)->next) *document = (*document)->next;
        (*document)->next = file;
        file->prev = *document;
    }
#if PG_VERSION_NUM >= 90500
    if (!documentMemoryContextCallback.func) {
        documentMemoryContextCallback.func = documentMemoryContextCallbackFunction;
        MemoryContextRegisterResetCallback(CurrentMemoryContext, &documentMemoryContextCallback);
    }
#endif
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"html");
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
    if (!(in = fmemopen((void *)html, len, "rb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fmemopen")));
    htmlReadFile(file, in, ".");
    fclose(in);
}

static Datum htmldoc(PG_FUNCTION_ARGS) {
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    cleanup = true;
    if (!document) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!document")));
    while (document && document->prev) document = document->prev;
    htmlFixLinks(document, document, 0);
    switch (PG_NARGS()) {
        case 0: if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!open_memstream"))); break;
        default: {
            char *file;
            if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc requires argument file")));
            require_role(PGHTMLDOC_ROLE_WRITE_SERVER_FILES, "pg_write_server_files", "write htmldoc output to a server file");
            file = TextDatumGetCString(PG_GETARG_DATUM(0));
            if (!(out = fopen(file, "wb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fopen(\"%s\")", file)));
            pfree(file);
        } break;
    }
    pspdf_export_out(document, NULL, out);
    switch (PG_NARGS()) {
        case 0: {
            bytea *pdf = cstring_to_text_with_len(output_data, output_len);
            free(output_data);
            PG_RETURN_BYTEA_P(pdf);
        } break;
        default: PG_RETURN_BOOL(true); break;
    }
}

EXTENSION(htmldoc_addfile) {
    char *file;
    cleanup = true;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addfile requires argument file")));
    require_role(PGHTMLDOC_ROLE_READ_SERVER_FILES, "pg_read_server_files", "read a server file with htmldoc_addfile");
    file = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(&document, file, Path);
    pfree(file);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addhtml) {
    text *html;
    cleanup = true;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addhtml requires argument html")));
    html = PG_GETARG_TEXT_PP(0);
    read_html(&document, VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html));
    PG_FREE_IF_COPY(html, 0);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addurl) {
    char *url;
    cleanup = true;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addurl requires argument url")));
    require_role(PGHTMLDOC_ROLE_EXECUTE_SERVER_PROGRAM, "pg_execute_server_program", "fetch a URL with htmldoc_addurl");
    url = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(&document, url, NULL);
    pfree(url);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(convert2pdf) { PSLevel = 0; return htmldoc(fcinfo); }
EXTENSION(convert2ps) { PSLevel = 3; return htmldoc(fcinfo); }
