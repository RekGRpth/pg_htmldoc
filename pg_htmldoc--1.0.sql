-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_htmldoc" to load this file. \quit

CREATE FUNCTION htmldoc_addfile(file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'htmldoc_addfile' LANGUAGE 'c';
CREATE FUNCTION htmldoc_addhtml(html TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'htmldoc_addhtml' LANGUAGE 'c';
CREATE FUNCTION htmldoc_addurl(url TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'htmldoc_addurl' LANGUAGE 'c';

CREATE FUNCTION convert2pdf() RETURNS BYTEA AS 'MODULE_PATHNAME', 'convert2pdf' LANGUAGE 'c';
CREATE FUNCTION convert2ps() RETURNS BYTEA AS 'MODULE_PATHNAME', 'convert2ps' LANGUAGE 'c';

CREATE FUNCTION convert2pdf(file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'convert2pdf' LANGUAGE 'c';
CREATE FUNCTION convert2ps(file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'convert2ps' LANGUAGE 'c';
