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
    INPUT_TYPE_HTML = 0,
    INPUT_TYPE_URL
} input_type_t;

typedef enum {
    OUTPUT_TYPE_PDF = 0,
    OUTPUT_TYPE_PS
} output_type_t;

PG_MODULE_MAGIC;

static Datum htmldoc(PG_FUNCTION_ARGS, data_type_t data_type, input_type_t input_type, output_type_t output_type) {
    Datum *elemsp;
    bool *nullsp;
    int nelemsp;
    text *pdf;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *in, *out;
    tree_t *document = NULL, *file;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errmsg("data is null!")));
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    htmlSetCharSet("utf-8");
    if (input_type == INPUT_TYPE_HTML) {
        if (data_type == DATA_TYPE_ARRAY) {
            if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) ereport(ERROR, (errcode(ERRCODE_ARRAY_ELEMENT_ERROR), errmsg("array_contains_nulls")));
            deconstruct_array(DatumGetArrayTypeP(PG_GETARG_DATUM(0)), TEXTOID, -1, false, 'i', &elemsp, &nullsp, &nelemsp);
            for (int i = 0; i < nelemsp; i++) {
                text *html = DatumGetTextP(elemsp[i]);
                if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!file")));
                htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)"");
                htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)".");
                if (!(in = fmemopen(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), "rb"))) ereport(ERROR, (errmsg("!in")));
                htmlReadFile2(file, in, ".");
                if (document == NULL) document = file; else {
                    while (document->next != NULL) document = document->next;
                    document->next = file;
                    file->prev = document;
                }
                fclose(in);
                //pfree(html);
            }
        } else {
            text *html = DatumGetTextP(PG_GETARG_DATUM(0));
            if (!(document = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!document")));
            htmlSetVariable(document, (uchar *)"_HD_FILENAME", (uchar *)"");
            htmlSetVariable(document, (uchar *)"_HD_BASE", (uchar *)".");
            if (!(in = fmemopen(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), "rb"))) ereport(ERROR, (errmsg("!in")));
            htmlReadFile2(document, in, ".");
            fclose(in);
            pfree(html);
        }
    } else if (input_type == INPUT_TYPE_URL) {
        if (data_type == DATA_TYPE_ARRAY) {
            if (array_contains_nulls(DatumGetArrayTypeP(PG_GETARG_DATUM(0)))) ereport(ERROR, (errcode(ERRCODE_ARRAY_ELEMENT_ERROR), errmsg("array_contains_nulls")));
            deconstruct_array(DatumGetArrayTypeP(PG_GETARG_DATUM(0)), TEXTOID, -1, false, 'i', &elemsp, &nullsp, &nelemsp);
            for (int i = 0; i < nelemsp; i++) {
                char *url = TextDatumGetCString(elemsp[i]);
                const char *realname = file_find(NULL, url);
                const char *base = file_directory(url);
                if (!realname) ereport(ERROR, (errmsg("!realname")));
                if (!(in = fopen(realname, "rb"))) ereport(ERROR, (errmsg("!in")));
                if (!(file = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!file")));
                htmlSetVariable(file, (uchar *)"_HD_URL", (uchar *)url);
                htmlSetVariable(file, (uchar *)"_HD_FILENAME", (uchar *)file_basename(url));
                htmlSetVariable(file, (uchar *)"_HD_BASE", (uchar *)base);
                htmlReadFile2(file, in, base);
                if (document == NULL) document = file; else {
                    while (document->next != NULL) document = document->next;
                    document->next = file;
                    file->prev = document;
                }
                fclose(in);
                //pfree(url);
            }
        } else {
            char *url = TextDatumGetCString(PG_GETARG_DATUM(0));
            const char *realname = file_find(NULL, url);
            const char *base = file_directory(url);
            if (!realname) ereport(ERROR, (errmsg("!realname")));
            if (!(in = fopen(realname, "rb"))) ereport(ERROR, (errmsg("!in")));
            if (!(document = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!document")));
            htmlSetVariable(document, (uchar *)"_HD_URL", (uchar *)url);
            htmlSetVariable(document, (uchar *)"_HD_FILENAME", (uchar *)file_basename(url));
            htmlSetVariable(document, (uchar *)"_HD_BASE", (uchar *)base);
            htmlReadFile2(document, in, base);
            fclose(in);
            pfree(url);
        }
    }
    htmlFixLinks(document, document, 0);
    if (output_type == OUTPUT_TYPE_PDF) {
        PSLevel = 0;
    } else if (output_type == OUTPUT_TYPE_PS) {
        PSLevel = 3;
    }
    if (PG_NARGS() == 1) {
        if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errmsg("!out")));
    } else {
        char *file;
        if (PG_ARGISNULL(1)) ereport(ERROR, (errmsg("our is null!")));
        file = TextDatumGetCString(PG_GETARG_DATUM(1));
        if (!(out = fopen(file, "wb"))) ereport(ERROR, (errmsg("!out")));
        pfree(file);
    }
    pspdf_export_out(document, NULL, out);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    if (PG_NARGS() == 1) {
        pdf = cstring_to_text_with_len(output_data, output_len);
        free(output_data);
        PG_RETURN_TEXT_P(pdf);
    } else {
        PG_RETURN_BOOL(true);
    }
}

EXTENSION(html2pdf) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_HTML, OUTPUT_TYPE_PDF); }
EXTENSION(html2ps) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_HTML, OUTPUT_TYPE_PS); }
EXTENSION(url2pdf) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_URL, OUTPUT_TYPE_PDF); }
EXTENSION(url2ps) { return htmldoc(fcinfo, DATA_TYPE_TEXT, INPUT_TYPE_URL, OUTPUT_TYPE_PS); }

EXTENSION(html2pdf_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_HTML, OUTPUT_TYPE_PDF); }
EXTENSION(html2ps_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_HTML, OUTPUT_TYPE_PS); }
EXTENSION(url2pdf_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_URL, OUTPUT_TYPE_PDF); }
EXTENSION(url2ps_array) { return htmldoc(fcinfo, DATA_TYPE_ARRAY, INPUT_TYPE_URL, OUTPUT_TYPE_PS); }
