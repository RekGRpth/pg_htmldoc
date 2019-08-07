-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_htmldoc" to load this file. \quit

CREATE OR REPLACE FUNCTION htmldoc(data TEXT) RETURNS TEXT AS 'MODULE_PATHNAME', 'pg_htmldoc' LANGUAGE 'c';
