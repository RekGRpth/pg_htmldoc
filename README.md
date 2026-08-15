PostgreSQL implementation of HTMLDOC conversion software. It is a program that reads HTML and Markdown source files or web pages and generates corresponding EPUB, HTML, PostScript, or PDF files with an optional table of contents.

### Requirements

- PostgreSQL with the `pgxs` build infrastructure (`pg_config` on `PATH`).
- [HTMLDOC](https://www.msweet.org/htmldoc/) built and installed as a shared library (`libhtmldoc.so`), headers included.
- The bundled [pg_whitelist](https://github.com/RekGRpth/pg_whitelist) git submodule (initialize it with `git submodule update --init`).

### Installation

```sh
git clone --recurse-submodules https://github.com/RekGRpth/pg_htmldoc.git
cd pg_htmldoc
make
make install
```

Then, in the target database:

```sql
create extension pg_htmldoc;
```

Run the regression tests with `make installcheck` (requires a running server and superuser access).

### Use of the extension

`pg_htmldoc` builds up a single in-memory document per session by queuing one or more files/URLs/HTML fragments with the `htmldoc_add*()` functions, then rendering everything queued so far -- and clearing it -- with `convert2pdf()`/`convert2ps()`.

```sql
select htmldoc_addurl('https://github.com');
copy (
    select convert2pdf()
) to '/var/lib/postgresql/htmldoc.pdf' WITH (FORMAT binary, HEADER false)
```

Multiple `add*` calls in a row are concatenated into one document:

```sql
select htmldoc_addfile('/srv/reports/cover.html');
select htmldoc_addfile('/srv/reports/body.html');
select convert2pdf('/srv/reports/report.pdf');
```

### Functions

| Function | Returns | Description |
| --- | --- | --- |
| `htmldoc_addfile(file text)` | `bool` | Queue a local file (HTML or Markdown), resolved relative to the server's working directory. |
| `htmldoc_addurl(url text)` | `bool` | Queue a web page, fetched over `http://`/`https://`. |
| `htmldoc_addhtml(html text)` | `bool` | Queue an in-memory HTML/Markdown fragment. Always requires superuser -- any local files or URLs referenced from its markup (`img`/`body`/`embed`) are resolved deep inside HTMLDOC's rendering pipeline, where there's no single file/URL `pg_htmldoc.whitelist` can check against. |
| `convert2pdf()` / `convert2ps()` | `bytea` | Render everything queued so far as PDF/PostScript and return it directly. |
| `convert2pdf(file text)` / `convert2ps(file text)` | `bool` | Render and write the result to `file` on the server instead of returning it. Always requires superuser, since it writes to the server's filesystem. |

Every `add*` call requires its argument to be non-`NULL`, and every `convert2*` call requires a document to already be queued (via a prior `add*` call) -- both raise an error otherwise. A successful `convert2*` call clears the queued document, so it must be rebuilt with `add*` before the next `convert2*` call.

### Permissions

| Caller | `htmldoc_addfile()` / `htmldoc_addurl()` | `htmldoc_addhtml()` | `convert2pdf()` / `convert2ps()` (bytea) | `convert2pdf(file)` / `convert2ps(file)` |
| --- | --- | --- | --- | --- |
| Superuser | Allowed (narrowed by `pg_htmldoc.whitelist` if set) | Allowed | Allowed | Allowed |
| Non-superuser | Allowed only if `pg_htmldoc.whitelist` explicitly grants the specific file/URL | Denied | Allowed (no filesystem access involved) | Denied |

`pg_htmldoc.whitelist` is a `PGC_SUSET` GUC -- settable only by a superuser, including via `ALTER ROLE ... SET`, so a role can never loosen its own scope with a plain `SET` -- holding a comma-separated list of `file://` and `http(s)://` prefixes:

```sql
alter role reporting set pg_htmldoc.whitelist = 'file:///srv/reports/,https://example.com/';
```

- For a superuser, a non-empty whitelist *narrows* access: only matching files/URLs are allowed.
- For a non-superuser, a non-empty whitelist is their *sole* grant: only matching files/URLs are allowed, everything else is denied.
- An empty/unset whitelist means unrestricted access for a superuser, but denies everything for a non-superuser.

### License

[MIT](LICENSE)
