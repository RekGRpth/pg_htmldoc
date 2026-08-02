CREATE EXTENSION pg_htmldoc;

-- Two roles: one with neither predefined role pg_htmldoc's C code
-- checks for, one privileged enough to satisfy require_role() on
-- whatever server this test runs against. Role names can't start with
-- "pg_" (reserved), so use htmldoc_test_*.
--
-- require_role() only consults the pg_read_server_files/
-- pg_execute_server_program/pg_write_server_files predefined roles from
-- PG 11 onward -- they don't exist before that, and the C code falls
-- back to a plain superuser() check instead (see the PG_VERSION_NUM
-- guard around PGHTMLDOC_ROLE_* in pg_htmldoc.c). So grant the roles
-- when they exist, and make htmldoc_test_full a superuser when they
-- don't -- either way it ends up satisfying whichever check
-- require_role() actually performs here.
CREATE ROLE htmldoc_test_none;
CREATE ROLE htmldoc_test_full;
DO $$
BEGIN
    IF current_setting('server_version_num')::int >= 110000 THEN
        EXECUTE 'GRANT pg_read_server_files TO htmldoc_test_full';
        EXECUTE 'GRANT pg_execute_server_program TO htmldoc_test_full';
        EXECUTE 'GRANT pg_write_server_files TO htmldoc_test_full';
    ELSE
        EXECUTE 'ALTER ROLE htmldoc_test_full SUPERUSER';
    END IF;
END
$$;

--
-- No document queued: convert2pdf()/convert2pdf(file)/convert2ps()/
-- convert2ps(file) all check for a queued document before anything
-- else -- before even require_role() or the NULL-argument check --
-- so calling any of them with nothing queued yet must fail with the
-- same "!document" internal error regardless of call form or role.
-- Use the privileged role so the error can't be mistaken for a
-- permission denial.
--
SET ROLE htmldoc_test_full;
SELECT convert2pdf();
SELECT convert2pdf('/tmp/pg_htmldoc_test_no_document.pdf');
SELECT convert2ps();
SELECT convert2ps('/tmp/pg_htmldoc_test_no_document.ps');
RESET ROLE;

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
-- convert2ps(file text) -- the file-output variant of convert2ps(),
-- as opposed to the bytea-returning convert2ps() exercised below --
-- was never exercised at all, unlike its convert2pdf(file) sibling
-- above. Mirror the same denied/success pair for it.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addhtml('<html><body>ps file test</body></html>');
RESET ROLE;

SET ROLE htmldoc_test_none;
SELECT convert2ps('/tmp/pg_htmldoc_test_denied.ps');
RESET ROLE;

SET ROLE htmldoc_test_full;
SELECT convert2ps('/tmp/pg_htmldoc_test_out.ps');
RESET ROLE;

--
-- htmldoc_addfile() was, until now, only ever exercised for its denial
-- (via the htmldoc_test_none block above) -- its success path for the
-- privileged role was never actually run. Confirm it end-to-end via
-- the in-memory (bytea) output path, same as the addurl/addhtml
-- checks below.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addfile('/etc/hostname');
SELECT octet_length(convert2pdf()) > 100 AS addfile_pdf_nonempty;
RESET ROLE;

--
-- htmldoc_addfile() with a path that resolves locally (no "http:"/
-- "https:"/"//" prefix) but doesn't exist on disk: file_find() (in
-- the vendored htmldoc library) returns NULL, and read_fileurl()
-- surfaces that as its own internal error rather than silently doing
-- nothing.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addfile('/nonexistent/pg_htmldoc_test_missing_file');
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

--
-- Multiple add* calls before a single convert* chain their documents
-- together (via document->next/->prev, set up identically in
-- read_fileurl()/read_html()) rather than each new add* replacing the
-- previous one. htmldoc flows chained documents continuously (no
-- forced page break between them), so output size isn't a reliable
-- signal -- a second copy of the same filler content mostly just
-- fills out remaining whitespace on the last page. Count PDF page
-- objects instead (a direct, unambiguous signal of how much content
-- actually got rendered) and confirm a second add* produces more
-- pages than a single one. Counting is done on the raw bytea (via a
-- manual position()/substring() scan) rather than by casting to text,
-- since compressed PDF content can contain embedded NUL bytes that
-- text values can't hold.
--
SET ROLE htmldoc_test_full;
DO $$
DECLARE
    needle bytea := convert_to('/Type/Page', 'LATIN1');
    single_pdf bytea;
    combined_pdf bytea;
    single_pages integer;
    combined_pages integer;
    pos integer;
BEGIN
    PERFORM htmldoc_addhtml('<html><body><h1>multi-document test</h1><p>' || repeat('filler text. ', 400) || '</p></body></html>');
    single_pdf := convert2pdf();

    PERFORM htmldoc_addhtml('<html><body><h1>multi-document test, part one</h1><p>' || repeat('filler text. ', 400) || '</p></body></html>');
    PERFORM htmldoc_addhtml('<html><body><h1>multi-document test, part two</h1><p>' || repeat('filler text. ', 400) || '</p></body></html>');
    combined_pdf := convert2pdf();

    single_pages := 0;
    LOOP
        pos := position(needle in single_pdf);
        EXIT WHEN pos = 0;
        single_pages := single_pages + 1;
        single_pdf := substring(single_pdf from pos + length(needle));
    END LOOP;

    combined_pages := 0;
    LOOP
        pos := position(needle in combined_pdf);
        EXIT WHEN pos = 0;
        combined_pages := combined_pages + 1;
        combined_pdf := substring(combined_pdf from pos + length(needle));
    END LOOP;

    IF combined_pages <= single_pages THEN
        RAISE EXCEPTION 'combined two-document render (% pages) has no more pages than a single document (% pages) -- second add* may not have been included', combined_pages, single_pages;
    END IF;
END
$$;
RESET ROLE;

--
-- Cleanup on rollback: an add* call that fails *after* a document has
-- already been chained onto `document` and its MemoryContextCallback
-- registered -- unlike the NULL-argument and missing-file checks
-- above, which both fail before either happens -- must still leave
-- things clean once the aborted statement unwinds:
-- documentMemoryContextCallbackFunction() tears the tree down and
-- resets `document` to NULL (PG >= 9.5 only -- see the
-- PG_VERSION_NUM guard in pg_htmldoc.c; older versions have no such
-- callback, so the check below is skipped there). htmldoc_addfile()
-- on a file that exists (so file_find() succeeds and the tree/
-- callback get set up) but isn't readable by this role reaches
-- exactly that path: fopen() in read_fileurl() is the only failure
-- point left after the tree is chained in.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addfile('/etc/shadow');
RESET ROLE;

SET ROLE htmldoc_test_full;
DO $$
BEGIN
    IF current_setting('server_version_num')::int >= 90500 THEN
        BEGIN
            PERFORM convert2pdf();
            RAISE EXCEPTION 'convert2pdf() succeeded after a failed htmldoc_addfile() -- the half-built document from the failed call was not cleaned up';
        EXCEPTION WHEN OTHERS THEN
            IF SQLERRM IS DISTINCT FROM '!document' THEN
                RAISE;
            END IF;
        END;
    END IF;
END
$$;
RESET ROLE;

--
-- Required arguments: htmldoc_addhtml/addfile/addurl and the
-- file-output convert2pdf(file)/convert2ps(file) all reject a NULL
-- argument outright. The PG_ARGISNULL(0) check runs before
-- require_role() in every one of them, so a privileged role sees the
-- same ERRCODE_NULL_VALUE_NOT_ALLOWED error an unprivileged one
-- would -- this isolates the NULL check from the permission check.
-- convert2pdf/convert2ps need a document already queued to reach
-- their own NULL check instead of the (separate, untested) "no
-- document queued" error, hence the addhtml() in between.
--
SET ROLE htmldoc_test_full;
SELECT htmldoc_addhtml(NULL);
SELECT htmldoc_addfile(NULL);
SELECT htmldoc_addurl(NULL);
SELECT htmldoc_addhtml('<html><body>null file arg test</body></html>');
SELECT convert2pdf(NULL);
SELECT convert2ps(NULL);
RESET ROLE;

DROP ROLE htmldoc_test_none;
DROP ROLE htmldoc_test_full;
DROP EXTENSION pg_htmldoc;
