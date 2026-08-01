CREATE EXTENSION pg_htmldoc;

-- Two roles: one with neither predefined role pg_htmldoc's C code
-- checks for, one with both (mirrors what COPY checks for file access).
-- Role names can't start with "pg_" (reserved), so use htmldoc_test_*.
CREATE ROLE htmldoc_test_none;
CREATE ROLE htmldoc_test_full;
GRANT pg_read_server_files TO htmldoc_test_full;
GRANT pg_execute_server_program TO htmldoc_test_full;
GRANT pg_write_server_files TO htmldoc_test_full;

--
-- Without either predefined role, every entry point must be denied: the
-- render pipeline can resolve <img>/<body background>/<embed> references
-- (and htmldoc_addfile/htmldoc_addurl arguments) to either a local file
-- or a URL fetch, regardless of which function was called.
--
SET ROLE htmldoc_test_none;
SELECT htmldoc_addhtml('<html><body>no privilege</body></html>');
SELECT htmldoc_addfile('/etc/hostname');
SELECT htmldoc_addurl('/etc/hostname');
RESET ROLE;

--
-- Build a real document as the fully-privileged role, then confirm the
-- unprivileged role still can't write it to a server file, and that the
-- privileged role can.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addhtml('<html><body><h1>pg_htmldoc regression test</h1></body></html>');
RESET ROLE;

SET ROLE htmldoc_test_none;
SELECT convert2pdf('/tmp/pg_htmldoc_test_denied.pdf');
RESET ROLE;

SET ROLE htmldoc_test_full;
SELECT convert2pdf('/tmp/pg_htmldoc_test_out.pdf');
RESET ROLE;

--
-- htmldoc_addurl() resolves a plain local path exactly the way
-- htmldoc_addfile() does (no "http:"/"https:"/"//" prefix), so it needs
-- both roles too; confirm success end-to-end via the in-memory (bytea)
-- output path, for both PDF and PS.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addurl('/etc/hostname');
SELECT octet_length(convert2pdf()) > 100 AS pdf_nonempty;
SELECT htmldoc_addhtml('<html><body>ps test</body></html>');
SELECT octet_length(convert2ps()) > 100 AS ps_nonempty;
RESET ROLE;

DROP ROLE htmldoc_test_none;
DROP ROLE htmldoc_test_full;
DROP EXTENSION pg_htmldoc;
