#include <postgres.h>

#include <catalog/pg_type.h>
#include <utils/builtins.h>

#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

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
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!file")));
    htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)fileurl);
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(fileurl));
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
    if (!realname) ereport(ERROR, (errmsg("!realname")));
    if (!(in = fopen(realname, "rb"))) ereport(ERROR, (errmsg("!in")));
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
    if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!file")));
    htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"");
    htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
    if (!(in = fmemopen(html, len, "rb"))) ereport(ERROR, (errmsg("!in")));
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
    text *pdf;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    tree_t *document = NULL;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errmsg("data is null!")));
    if (!_htmlInitialized) htmlSetCharSet("utf-8");
    switch (input_type) {
        case INPUT_TYPE_FILE: switch (data_type) {
            case DATA_TYPE_TEXT: read_fileurl(TextDatumGetCString(PG_GETARG_DATUM(0)), &document, Path); break;
            case DATA_TYPE_ARRAY: {
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) ereport(ERROR, (errcode(ERRCODE_ARRAY_ELEMENT_ERROR), errmsg("array_contains_nulls")));
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
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) ereport(ERROR, (errcode(ERRCODE_ARRAY_ELEMENT_ERROR), errmsg("array_contains_nulls")));
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
                if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) ereport(ERROR, (errcode(ERRCODE_ARRAY_ELEMENT_ERROR), errmsg("array_contains_nulls")));
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
        case 1: if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errmsg("!out"))); break;
        default: {
            char *file;
            if (PG_ARGISNULL(1)) ereport(ERROR, (errmsg("our is null!")));
            file = TextDatumGetCString(PG_GETARG_DATUM(1));
            if (!(out = fopen(file, "wb"))) ereport(ERROR, (errmsg("!out")));
            pfree(file);
        } break;
    }
    pspdf_export_out(document, NULL, out);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    switch (PG_NARGS()) {
        case 1: {
            pdf = cstring_to_text_with_len(output_data, output_len);
            free(output_data);
            PG_RETURN_TEXT_P(pdf);
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
