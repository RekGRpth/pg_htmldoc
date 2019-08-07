#include <postgres.h>
#include <utils/builtins.h>
#include "htmldoc.h"

#define EXTENSION(function) Datum (function)(PG_FUNCTION_ARGS); PG_FUNCTION_INFO_V1(function); Datum (function)(PG_FUNCTION_ARGS)

PG_MODULE_MAGIC;

EXTENSION(pg_htmldoc) {
    text *html, *pdf;
    char *output_data = NULL;
    size_t output_len = 0;
    FILE *in, *out;
    tree_t *document;
    if (PG_ARGISNULL(0)) ereport(ERROR, (errmsg("html is null!")));
    html = DatumGetTextP(PG_GETARG_DATUM(0));
    if (!(in = fmemopen(VARDATA_ANY(html), VARSIZE_ANY_EXHDR(html), "rb"))) ereport(ERROR, (errmsg("!in")));
    if (!(out = open_memstream(&output_data, &output_len))) ereport(ERROR, (errmsg("!out")));
    set_out(out);
    htmlSetCharSet("utf-8");
    if (!(document = htmlAddTree(NULL, MARKUP_FILE, NULL))) ereport(ERROR, (errmsg("!document")));
    htmlSetVariable(document, (uchar *)"_HD_FILENAME", (uchar *)"");
    htmlSetVariable(document, (uchar *)"_HD_BASE", (uchar *)".");
    htmlReadFile2(document, in, ".");
    htmlFixLinks(document, document, 0);
    pspdf_export(document, NULL);
    htmlDeleteTree(document);
    file_cleanup();
    image_flush_cache();
    fclose(in);
    pfree(html);
    pdf = cstring_to_text_with_len(output_data, output_len);
    free(output_data);
    PG_RETURN_TEXT_P(pdf);
}
