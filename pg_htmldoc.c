#include <postgres.h>

#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

#define FORMAT_0(fmt, ...) "%s(%s:%d): %s", __func__, __FILE__, __LINE__, fmt
#define FORMAT_1(fmt, ...) "%s(%s:%d): " fmt,  __func__, __FILE__, __LINE__
#define GET_FORMAT(fmt, ...) GET_FORMAT_PRIVATE(fmt, 0, ##__VA_ARGS__, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
    1, 1, 1, 1, 1, 1, 1, 1, 1, 0)
#define GET_FORMAT_PRIVATE(fmt, \
      _0,  _1,  _2,  _3,  _4,  _5,  _6,  _7,  _8,  _9, \
     _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, \
     _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, \
     _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, \
     _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, \
     _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, \
     _60, _61, _62, _63, _64, _65, _66, _67, _68, _69, \
     _70, format, ...) FORMAT_ ## format(fmt)

#define D1(fmt, ...) ereport(DEBUG1, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D2(fmt, ...) ereport(DEBUG2, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D3(fmt, ...) ereport(DEBUG3, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D4(fmt, ...) ereport(DEBUG4, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define D5(fmt, ...) ereport(DEBUG5, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define E(fmt, ...) ereport(ERROR, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define F(fmt, ...) ereport(FATAL, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define I(fmt, ...) ereport(INFO, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define L(fmt, ...) ereport(LOG, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define N(fmt, ...) ereport(NOTICE, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))
#define W(fmt, ...) ereport(WARNING, (errmsg(GET_FORMAT(fmt, ##__VA_ARGS__), ##__VA_ARGS__)))

typedef enum {
    DATA_TYPE_TEXT = 0,
    DATA_TYPE_ARRAY
} data_type_t;

typedef enum {
    INPUT_TYPE_FILE = 0,
    INPUT_TYPE_HTML,
    INPUT_TYPE_URL
} input_type_t;

typedef enum {
    OUTPUT_TYPE_PDF = 0,
    OUTPUT_TYPE_PS
} output_type_t;

PG_MODULE_MAGIC;

static void read_fileurl(const char *fileurl, tree_t **document, const char *path) {
    tree_t *file;
    FILE *in;
    const char *realname = file_find(path, fileurl);
    const char *base = file_directory(fileurl);
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) E("!htmlAddTree");
    htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)fileurl);
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(fileurl));
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
    if (!realname) E("!file_find(\"%s\", \"%s\")", path, fileurl);
    if (!(in = fopen(realname, "rb"))) E("!fopen(\"%s\")", realname);
    htmlReadFile2(file, in, base);
    fclose(in);
    if (*document == NULL) *document = file; else {
        while ((*document)->next != NULL) *document = (*document)->next;
        (*document)->next = file;
        file->prev = *document;
    }
}

static void read_html(char *html, size_t len, tree_t **document) {
    tree_t *file;
    FILE *in;
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) E("!htmlAddTree");
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"");
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
    if (!(in = fmemopen(html, len, "rb"))) E("!fmemopen");
    htmlReadFile2(file, in, ".");
    fclose(in);
    if (*document == NULL) *document = file; else {
        while ((*document)->next != NULL) *document = (*document)->next;
        (*document)->next = file;
        file->prev = *document;
    }
}

static Datum htmldoc(PG_FUNCTION_ARGS, data_type_t data_type, input_type_t input_type, output_type_t output_type) {
    Datum *elemsp;
    bool *nullsp;
    int nelemsp;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    tree_t *document = NULL;
    if (PG_ARGISNULL(0)) E("data is null!");
    if (!_htmlInitialized) htmlSetCharSet("utf-8");
    switch (input_type) {
        case INPUT_TYPE_FILE: switch (data_type) {
            case DATA_TYPE_TEXT: read_fileurl(TextDatumGetCString(PG_GETARG_DATUM(0)), &document, Path); break;
            case DATA_TYPE_ARRAY: {
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) E("array_contains_nulls");
                deconstruct_array(DatumGetArrayTypeP(PG_GETARG_DATUM(0)), TEXTOID, -1, false, 'i', &elemsp, &nullsp, &nelemsp);
                for (int i = 0; i < nelemsp; i++) read_fileurl(TextDatumGetCString(elemsp[i]), &document, Path);
            } break;
        } break;
        case INPUT_TYPE_HTML: switch (data_type) {
            case DATA_TYPE_TEXT: {
                text *html = DatumGetTextP(PG_GETARG_DATUM(0));
                read_html(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), &document);
            } break;
            case DATA_TYPE_ARRAY: {
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) E("array_contains_nulls");
                deconstruct_array(DatumGetArrayTypeP(PG_GETARG_DATUM(0)), TEXTOID, -1, false, 'i', &elemsp, &nullsp, &nelemsp);
                for (int i = 0; i < nelemsp; i++) {
                    text *html = DatumGetTextP(elemsp[i]);
                    read_html(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), &document);
                }
            } break;
        } break;
        case INPUT_TYPE_URL: switch (data_type) {
            case DATA_TYPE_TEXT: read_fileurl(TextDatumGetCString(PG_GETARG_DATUM(0)), &document, NULL); break;
            case DATA_TYPE_ARRAY: {
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) E("array_contains_nulls");
                deconstruct_array(DatumGetArrayTypeP(PG_GETARG_DATUM(0)), TEXTOID, -1, false, 'i', &elemsp, &nullsp, &nelemsp);
                for (int i = 0; i < nelemsp; i++) read_fileurl(TextDatumGetCString(elemsp[i]), &document, NULL);
            } break;
        } break;
    }
    while (document && document->prev) document = document->prev;
    htmlFixLinks(document, document, 0);
    switch (output_type) {
        case OUTPUT_TYPE_PDF: PSLevel = 0; break;
        case OUTPUT_TYPE_PS: PSLevel = 3; break;
    }
    switch (PG_NARGS()) {
        case 1: if (!(out = open_memstream(&output_data, &output_len))) E("!open_memstream"); break;
        default: {
            char *file;
            if (PG_ARGISNULL(1)) E("our is null!");
            file = TextDatumGetCString(PG_GETARG_DATUM(1));
            if (!(out = fopen(file, "wb"))) E("!fopen(\"%s\")", file);
            pfree(file);
        } break;
    }
    pspdf_export_out(document, NULL, out);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    switch (PG_NARGS()) {
        case 1: {
            text *pdf = cstring_to_text_with_len(output_data, output_len);
            free(output_data);
            PG_RETURN_BYTEA_P(pdf);
        } break;
        default: PG_RETURN_BOOL(true); break;
    }
}

EXTENSION(file2pdf) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_FILE, OUTPUT_TYPE_PDF); }
EXTENSION(file2ps) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_FILE, OUTPUT_TYPE_PS); }
EXTENSION(file2pdf_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_FILE, OUTPUT_TYPE_PDF); }
EXTENSION(file2ps_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_FILE, OUTPUT_TYPE_PS); }

EXTENSION(html2pdf) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_HTML, OUTPUT_TYPE_PDF); }
EXTENSION(html2ps) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_HTML, OUTPUT_TYPE_PS); }
EXTENSION(html2pdf_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_HTML, OUTPUT_TYPE_PDF); }
EXTENSION(html2ps_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_HTML, OUTPUT_TYPE_PS); }

EXTENSION(url2pdf) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_URL, OUTPUT_TYPE_PDF); }
EXTENSION(url2ps) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_URL, OUTPUT_TYPE_PS); }
EXTENSION(url2pdf_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_URL, OUTPUT_TYPE_PDF); }
EXTENSION(url2ps_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_URL, OUTPUT_TYPE_PS); }
