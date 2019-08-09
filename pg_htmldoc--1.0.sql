-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_htmldoc" to load this file. \quit

CREATE OR REPLACE FUNCTION html2pdf(data TEXT) RETURNS TEXT AS 'MODULE_PATHNAME', 'html2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2ps(data TEXT) RETURNS TEXT AS 'MODULE_PATHNAME', 'html2ps' LANGUAGE 'c';
