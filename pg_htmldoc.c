#include <postgres.h>
#include <utils/builtins.h>
#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)
#define DATUM(function) Datum (function)(PG_FUNCTION_ARGS); Datum (function)(PG_FUNCTION_ARGS)

enum {
    INPUT_TYPE_HTML = 0,
    INPUT_TYPE_URL
} input_type;

enum {
    OUTPUT_TYPE_PDF = 0,
    OUTPUT_TYPE_PS
} output_type;

PG_MODULE_MAGIC;

EXTENSION(htmldoc) {
    text *pdf;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *out;
    tree_t *document;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errmsg("data is null!")));
    _htmlPPI = 72.0f * _htmlBrowserWidth / (PageWidth - PageLeft - PageRight);
    htmlSetCharSet("utf-8");
    if (!(document = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!document")));
    if (input_type == INPUT_TYPE_HTML) {
        FILE *in;
        text *html = DatumGetTextP(PG_GETARG_DATUM(0));
        htmlSetVariable(document, (uchar *)"_HD_FILENAME", (uchar *)"");
        htmlSetVariable(document, (uchar *)"_HD_BASE", (uchar *)".");
        if (!(in = fmemopen(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), "rb"))) ereport(ERROR, (errmsg("!in")));
        htmlReadFile2(document, in, ".");
        fclose(in);
        pfree(html);
    } else if (input_type == INPUT_TYPE_URL) {
        const char *realname;
        FILE *fp;
        char *url = TextDatumGetCString(PG_GETARG_DATUM(0));
        char *base = pstrdup(file_directory(url));
        if (!(realname = file_find(NULL, url))) ereport(ERROR, (errmsg("!realname")));
        if (!(fp = fopen(realname, "rb"))) ereport(ERROR, (errmsg("!fp")));
        htmlSetVariable(document, (uchar *)"_HD_URL", (uchar *)url);
        htmlSetVariable(document, (uchar *)"_HD_FILENAME", (uchar *)file_basename(url));
        htmlSetVariable(document, (uchar *)"_HD_BASE", (uchar *)base);
        htmlReadFile2(document, fp, base);
        pfree(url);
        pfree(base);
    }
    htmlFixLinks(document, document, 0);
    if (output_type == OUTPUT_TYPE_PDF) {
        PSLevel = 0;
    } else if (output_type == OUTPUT_TYPE_PS) {
        PSLevel = 3;
    }
    if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errmsg("!out")));
    pspdf_export_out(document, NULL, out);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    pdf = cstring_to_text_with_len(output_data, output_len);
    free(output_data);
    PG_RETURN_TEXT_P(pdf);
}

EXTENSION(html2pdf) {
    input_type = INPUT_TYPE_HTML;
    output_type = OUTPUT_TYPE_PDF;
    return htmldoc(fcinfo);
}

EXTENSION(html2ps) {
    input_type = INPUT_TYPE_HTML;
    output_type = OUTPUT_TYPE_PS;
    return htmldoc(fcinfo);
}

EXTENSION(url2pdf) {
    input_type = INPUT_TYPE_URL;
    output_type = OUTPUT_TYPE_PDF;
    return htmldoc(fcinfo);
}

EXTENSION(url2ps) {
    input_type = INPUT_TYPE_URL;
    output_type = OUTPUT_TYPE_PS;
    return htmldoc(fcinfo);
}
