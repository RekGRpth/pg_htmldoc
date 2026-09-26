#include <postgres.h>

#include <catalog/pg_type.h>
#include <dlfcn.h>
#include <miscadmin.h>
#include <unistd.h>
#include <utils/builtins.h>
#if PG_VERSION_NUM >= 160000
#include <varatt.h>
#endif

#include "htmldoc.h"
#include "pg_whitelist.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

PG_MODULE_MAGIC;

/* pg_whitelist's "privileged" caller is a superuser; anyone else must be
 * granted access explicitly via pg_htmldoc.whitelist. Writing htmldoc
 * output to a server file, and htmldoc_addhtml() (whose markup comes straight
 * from the caller, with no file/URL for pg_whitelist to grant), have no
 * whitelist alternative and so require superuser unconditionally. */
static void require_superuser(const char *action) {
    if (!superuser()) ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE), errmsg("permission denied to %s", action), errdetail("Only superuser may %s.", action)));
}

static bool cleanup = false;
/* Set by fileCallbackFunction() to whatever it last refused; empty when it has
 * refused nothing since read_fileurl() cleared it. Sized like the URL and
 * filename buffers libhtmldoc itself uses, so a longer one was already
 * truncated before it got here. */
static char denied_url[1024] = "";
static bool privileged = false;
static tree_t *document = NULL;

/* libhtmldoc's htmlReadFile(tree_t *, FILE *, const char *) collides by name
 * with libxml2's unrelated, ABI-incompatible htmlReadFile(const char *, const
 * char *, int). libxml2 is already loaded into every backend for the built-in
 * xml type, before CREATE EXTENSION dlopens libhtmldoc, so plain (non-weak,
 * global-scope) symbol resolution binds calls to libxml2's version instead of
 * libhtmldoc's. It fails silently -- returns NULL, which every caller here
 * already ignores -- rather than crashing, so htmldoc_addfile/addhtml/addurl
 * report success while silently queuing an empty document: convert2pdf/ps
 * then produce a page-less, near-empty PDF/PS with no error at all.
 *
 * dlsym() on a handle scoped to libhtmldoc itself bypasses the ambiguous
 * global scope and always resolves libhtmldoc's own definition, regardless of
 * what else happens to be loaded into the backend or in what order. Resolved
 * once in _PG_init(); html.h deliberately does not declare htmlReadFile(), so
 * accidentally calling it directly (and reintroducing this bug) is a build
 * error rather than a silent miscompile. */
typedef tree_t *(*htmlReadFile_fn)(tree_t *parent, FILE *fp, const char *base);
static htmlReadFile_fn real_htmlReadFile = NULL;

/* read_fileurl()'s whitelist checks only cover the one file/URL named in the
 * function argument. Everything a document goes on to reference -- <img>,
 * <body background>, <embed>, and every hop of an HTTP redirect -- is resolved
 * inside libhtmldoc's file_find(), which read_fileurl() never sees, and image
 * fetching happens later still, during convert2pdf/convert2ps. file_callback()
 * reports each of those accesses instead, so the whitelist covers them too.
 *
 * The verdict is returned rather than raised: ereport(ERROR) from here would
 * longjmp out of libhtmldoc's C++ call stack, skipping destructors and leaving
 * its HTTP connection and temporary files behind. Returning 0 makes
 * file_find() fail the access cleanly and report it itself. */
static int fileCallbackFunction(void *data, hd_file_event_t event, const char *url, const char *localname, int status) {
    bool allowed;
    switch (event) {
        /* A "data:" URI is content inlined in the document that referenced it:
         * no file is read and no host is contacted. */
        case HD_FILE_DATA: allowed = true; break;
        /* The request that produced this was already vetted below. */
        case HD_FILE_RESULT: allowed = true; break;
        /* url is reassembled from the host httpSeparateURI() actually parsed
         * out, so the userinfo tricks pg_whitelist_url_prefix() guards against
         * can't reach it, and each redirect hop comes back here as a fresh
         * HD_FILE_REQUEST before anything is sent. */
        case HD_FILE_REQUEST: case HD_FILE_REDIRECT: allowed = pg_whitelist_allows_url(url, privileged); break;
        /* Either a candidate local path (url == localname), or a URL already
         * in the web cache (localname is its temporary file). Each check
         * passes what isn't its kind, so the pair dispatches on url. */
        case HD_FILE_LOCAL: case HD_FILE_CACHE: allowed = pg_whitelist_allows_url(url, privileged) && pg_whitelist_allows_local(url, localname ? localname : url, privileged); break;
        default: allowed = false; break;
    }
    /* Record what was refused, so read_fileurl() can name it rather than the
     * argument it started from -- which, after a redirect, is a URL that is
     * itself permitted. Only ever set here; read_fileurl() clears it. */
    if (!allowed) strlcpy(denied_url, url, sizeof(denied_url));
    return allowed ? 1 : 0;
}

void _PG_init(void); void _PG_init(void) {
    void *handle;
    if (!(handle = dlopen("libhtmldoc.so", RTLD_NOW | RTLD_NOLOAD))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!dlopen(\"libhtmldoc.so\"): %s", dlerror())));
    if (!(real_htmlReadFile = (htmlReadFile_fn)dlsym(handle, "htmlReadFile"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!dlsym(\"htmlReadFile\"): %s", dlerror())));
    if (!_htmlInitialized) htmlSetCharSet("utf-8");
    file_callback(fileCallbackFunction, NULL);
    pg_whitelist_init("pg_htmldoc.whitelist");
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
    const char *base;
    const char *realname;
    FILE *in;
    tree_t *file;
    pg_whitelist_check_url(fileurl, privileged);
    base = file_directory(fileurl);
    denied_url[0] = '\0';
    realname = file_find(path, fileurl);
    if (!base) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!file_directory(\"%s\")", fileurl)));
    /* file_find() reports a refusal from fileCallbackFunction() the same way it
     * reports a missing file, and the refusal is the more useful of the two. */
    if (!realname && denied_url[0]) pg_whitelist_deny(denied_url);
    if (!realname) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!file_find(\"%s\", \"%s\")", path, fileurl)));
    pg_whitelist_check_local(fileurl, realname, privileged);
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!htmlAddTree")));
    if (!*document) *document = file; else {
        tree_t *last = *document;
        while (last->next) last = last->next;
        last->next = file;
        file->prev = last;
    }
#if PG_VERSION_NUM >= 90500
    {
        MemoryContextCallback *cb = palloc0(sizeof(MemoryContextCallback));
        cb->func = documentMemoryContextCallbackFunction;
        MemoryContextRegisterResetCallback(CurrentMemoryContext, cb);
    }
#endif
    htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)fileurl);
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(fileurl));
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
    if (!(in = fopen(realname, "rb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fopen(\"%s\")", realname)));
    real_htmlReadFile(file, in, base);
    fclose(in);
}

static void read_html(tree_t **document, const char *html, size_t len) {
    FILE *in;
    tree_t *file;
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!htmlAddTree")));
    if (!*document) *document = file; else {
        tree_t *last = *document;
        while (last->next) last = last->next;
        last->next = file;
        file->prev = last;
    }
#if PG_VERSION_NUM >= 90500
    {
        MemoryContextCallback *cb = palloc0(sizeof(MemoryContextCallback));
        cb->func = documentMemoryContextCallbackFunction;
        MemoryContextRegisterResetCallback(CurrentMemoryContext, cb);
    }
#endif
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"html");
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
    if (!(in = fmemopen((void *)html, len, "rb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fmemopen")));
    real_htmlReadFile(file, in, ".");
    fclose(in);
}

static Datum htmldoc(PG_FUNCTION_ARGS) {
    char *file = NULL;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    cleanup = true;
    privileged = superuser();
    if (!document) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!document")));
    while (document && document->prev) document = document->prev;
    htmlFixLinks(document, document, 0);
    switch (PG_NARGS()) {
        case 0: if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!open_memstream"))); break;
        default: {
            if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc requires argument file")));
            require_superuser("write htmldoc output to a server file");
            file = TextDatumGetCString(PG_GETARG_DATUM(0));
            if (!(out = fopen(file, "wb"))) ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("!fopen(\"%s\")", file)));
        } break;
    }
    if (pspdf_export_out(document, NULL, out)) {
        /* pspdf_export_out() only closes out once it has written the document;
         * its error returns happen before that, so out is still open here --
         * and nothing has been written to it, so don't leave the empty file
         * fopen() created behind either. */
        fclose(out);
        free(output_data);
        if (file) unlink(file);
        ereport(ERROR, (errcode(ERRCODE_INTERNAL_ERROR), errmsg("pspdf_export_out failed")));
    }
    if (file) pfree(file);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    document = NULL;
    cleanup = false;
    switch (PG_NARGS()) {
        case 0: {
            bytea *pdf = cstring_to_text_with_len(output_data, output_len);
            free(output_data);
            PG_RETURN_BYTEA_P(pdf);
        } break;
        default: PG_RETURN_BOOL(true); break;
    }
}

/* htmldoc_addfile()/htmldoc_addurl() name a single concrete file/URL up front,
 * so a non-superuser caller isn't refused outright: read_fileurl() ->
 * pg_whitelist_check_url()/check_local() still admit it if
 * pg_htmldoc.whitelist explicitly grants that specific file/URL, treating the
 * whitelist as an alternative grant rather than only a narrowing of an
 * already-privileged caller. Whatever the document then references is covered
 * by fileCallbackFunction() above. htmldoc_addhtml() takes its markup straight
 * from the caller with no file/URL to grant at all, so superuser stays
 * mandatory for it. */
EXTENSION(htmldoc_addfile) {
    char *file;
    cleanup = true;
    privileged = superuser();
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addfile requires argument file")));
    file = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(&document, file, Path);
    pfree(file);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addhtml) {
    text *html;
    cleanup = true;
    privileged = superuser();
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addhtml requires argument html")));
    require_superuser("use htmldoc_addhtml (HTML may reference a local file or URL via img/body/embed)");
    html = PG_GETARG_TEXT_PP(0);
    read_html(&document, VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html));
    PG_FREE_IF_COPY(html, 0);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addurl) {
    char *url;
    cleanup = true;
    privileged = superuser();
    if (PG_ARGISNULL(0)) ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("htmldoc_addurl requires argument url")));
    url = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(&document, url, NULL);
    pfree(url);
    cleanup = false;
    PG_RETURN_BOOL(true);
}

EXTENSION(convert2pdf) { PSLevel = 0; return htmldoc(fcinfo); }
EXTENSION(convert2ps) { PSLevel = 3; return htmldoc(fcinfo); }
