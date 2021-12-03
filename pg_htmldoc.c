#include <postgres.h>

#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

#define D1(...) ereport(DEBUG1, (errmsg(__VA_ARGS__)))
#define D2(...) ereport(DEBUG2, (errmsg(__VA_ARGS__)))
#define D3(...) ereport(DEBUG3, (errmsg(__VA_ARGS__)))
#define D4(...) ereport(DEBUG4, (errmsg(__VA_ARGS__)))
#define D5(...) ereport(DEBUG5, (errmsg(__VA_ARGS__)))
#define E(...) ereport(ERROR, (errmsg(__VA_ARGS__)))
#define F(...) ereport(FATAL, (errmsg(__VA_ARGS__)))
#define I(...) ereport(INFO, (errmsg(__VA_ARGS__)))
#define L(...) ereport(LOG, (errmsg(__VA_ARGS__)))
#define N(...) ereport(NOTICE, (errmsg(__VA_ARGS__)))
#define W(...) ereport(WARNING, (errmsg(__VA_ARGS__)))

PG_MODULE_MAGIC;

tree_t *document = NULL;

void _PG_init(void); void _PG_init(void) {
    if (!_htmlInitialized) htmlSetCharSet("utf-8");
}

static void read_fileurl(const char *fileurl) {
    tree_t *file;
    FILE *in;
    const char *realname = file_find(Path, fileurl);
    const char *base = file_directory(fileurl);
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) E("!htmlAddTree");
    htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)fileurl);
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(fileurl));
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
    if (!realname) E("!file_find(\"%s\", \"%s\")", Path, fileurl);
    if (!(in = fopen(realname, "rb"))) E("!fopen(\"%s\")", realname);
    htmlReadFile2(file, in, base);
    fclose(in);
    if (document == NULL) document = file; else {
        while (document->next != NULL) document = document->next;
        document->next = file;
        file->prev = document;
    }
}

static void read_html(char *html, size_t len) {
    tree_t *file;
    FILE *in;
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) E("!htmlAddTree");
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"html");
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
    if (!(in = fmemopen(html, len, "rb"))) E("!fmemopen");
    htmlReadFile2(file, in, ".");
    fclose(in);
    if (document == NULL) document = file; else {
        while (document->next != NULL) document = document->next;
        document->next = file;
        file->prev = document;
    }
}

static Datum htmldoc(PG_FUNCTION_ARGS) {
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    while (document && document->prev) document = document->prev;
    htmlFixLinks(document, document, 0);
    switch (PG_NARGS()) {
        case 0: if (!(out = open_memstream(&output_data, &output_len))) E("!open_memstream"); break;
        default: {
            char *file;
            if (PG_ARGISNULL(0)) E("file is null!");
            file = TextDatumGetCString(PG_GETARG_DATUM(0));
            if (!(out = fopen(file, "wb"))) E("!fopen(\"%s\")", file);
            pfree(file);
        } break;
    }
    pspdf_export_out(document, NULL, out);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    document = NULL;
    switch (PG_NARGS()) {
        case 0: {
            text *pdf = cstring_to_text_with_len(output_data, output_len);
            free(output_data);
            PG_RETURN_BYTEA_P(pdf);
        } break;
        default: PG_RETURN_BOOL(true); break;
    }
}

EXTENSION(htmldoc_addfile) {
    char *file;
    if (PG_ARGISNULL(0)) E("file is null!");
    file = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(file);
    pfree(file);
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addhtml) {
    text *html;
    if (PG_ARGISNULL(0)) E("html is null!");
    html = DatumGetTextP(PG_GETARG_DATUM(0));
    read_html(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html));
    PG_RETURN_BOOL(true);
}

EXTENSION(htmldoc_addurl) {
    char *url;
    if (PG_ARGISNULL(0)) E("url is null!");
    url = TextDatumGetCString(PG_GETARG_DATUM(0));
    read_fileurl(url);
    pfree(url);
    PG_RETURN_BOOL(true);
}

EXTENSION(convert2pdf) { PSLevel = 0; return htmldoc(fcinfo); }
EXTENSION(convert2ps) { PSLevel = 3; return htmldoc(fcinfo); }
