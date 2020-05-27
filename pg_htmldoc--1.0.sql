-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_htmldoc" to load this file. \quit

CREATE OR REPLACE FUNCTION file2pdf(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'file2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2ps(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'file2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2pdf(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'file2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2ps(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'file2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2pdf(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'file2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2ps(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'file2ps_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2pdf(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'file2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION file2ps(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'file2ps_array' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION html2pdf(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'html2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2ps(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'html2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2pdf(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'html2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2ps(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'html2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2pdf(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'html2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2ps(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'html2ps_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2pdf(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'html2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION html2ps(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'html2ps_array' LANGUAGE 'c';

CREATE OR REPLACE FUNCTION url2pdf(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'url2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2ps(data TEXT) RETURNS BYTEA AS 'MODULE_PATHNAME', 'url2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2pdf(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'url2pdf' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2ps(data TEXT, file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'url2ps' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2pdf(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'url2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2ps(data TEXT[]) RETURNS BYTEA AS 'MODULE_PATHNAME', 'url2ps_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2pdf(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'url2pdf_array' LANGUAGE 'c';
CREATE OR REPLACE FUNCTION url2ps(data TEXT[], file TEXT) RETURNS BOOL AS 'MODULE_PATHNAME', 'url2ps_array' LANGUAGE 'c';
