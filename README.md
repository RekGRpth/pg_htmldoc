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
| `htmldoc_addhtml(html text)` | `bool` | Queue an in-memory HTML/Markdown fragment. Always requires superuser -- its markup comes straight from the caller, with no file/URL for `pg_htmldoc.whitelist` to grant. |
| `convert2pdf()` / `convert2ps()` | `bytea` | Render everything queued so far as PDF/PostScript and return it directly. |
| `convert2pdf(file text)` / `convert2ps(file text)` | `bool` | Render and write the result to `file` on the server instead of returning it. Always requires superuser, since it writes to the server's filesystem. |

Every `add*` call requires its argument to be non-`NULL`, and every `convert2*` call requires a document to already be queued (via a prior `add*` call) -- both raise an error otherwise. A successful `convert2*` call clears the queued document, so it must be rebuilt with `add*` before the next `convert2*` call.

### Permissions

| Caller | `htmldoc_addfile()` / `htmldoc_addurl()` | `htmldoc_addhtml()` | `convert2pdf()` / `convert2ps()` (bytea) | `convert2pdf(file)` / `convert2ps(file)` |
| --- | --- | --- | --- | --- |
| Superuser | Allowed (narrowed by `pg_htmldoc.whitelist` if set) | Allowed | Allowed | Allowed |
| Non-superuser | Allowed only if `pg_htmldoc.whitelist` explicitly grants the specific file/URL | Denied | Allowed (the result is returned, not written to the server) | Denied |

`pg_htmldoc.whitelist` is a `PGC_SUSET` GUC -- settable only by a superuser, including via `ALTER ROLE ... SET`, so a role can never loosen its own scope with a plain `SET` -- holding a comma-separated list of `file://` and `http(s)://` prefixes:

```sql
alter role reporting set pg_htmldoc.whitelist = 'file:///srv/reports/,https://example.com/';
```

- For a superuser, a non-empty whitelist *narrows* access: only matching files/URLs are allowed.
- For a non-superuser, a non-empty whitelist is their *sole* grant: only matching files/URLs are allowed, everything else is denied.
- An empty/unset whitelist means unrestricted access for a superuser, but denies everything for a non-superuser.

Entries are separated by commas, with surrounding whitespace ignored, and matched as follows:

- `file:///dir/` (trailing slash) allows anything under that directory; `file:///dir/file` allows only that one file. The path being accessed is resolved with `realpath()` first, so `..` or a symlink can't lead out of an allowed directory. A directory entry itself is compared as written, so give its real path rather than one through a symlink.
- `http://` and `https://` entries allow any URL starting with them. Without a trailing slash, the match must end at `/`, `?`, `#` or the end of the URL: `https://example.com` doesn't allow `https://example.com.evil.net/`, and `https://example.com/api` doesn't allow `https://example.com/api2`. A URL whose path has a `.` or `..` segment, percent-encoded or not, never matches an entry with a path below the root, since a server resolving it could land outside that path; `https://example.com/reports/` doesn't allow `https://example.com/reports/../secret.html`. Paths are compared percent-decoded, the way they're actually sent, so `%7E` and `~` -- or `%2F` and `/` -- are the same on either side.
- The scheme and port must match: an `https://` entry doesn't allow `http://`, and a URL on a non-default port needs an entry naming that port. A default port (`:443` for `https`, `:80` for `http`) may be written or left out on either side.
- A user name and password -- anything before an `@` that comes ahead of the first `/` -- aren't part of the host and are ignored on both sides: `https://user:secret@example.com/` is compared as `https://example.com/`, and `https://example.com?@evil.net/` as `https://evil.net/`, the host it would actually connect to.
- Hosts are compared exactly as written, so spell them the same way the documents do (normally lowercase).
- A scheme-relative `//host/...` URL never matches, since every entry names its scheme.

The whitelist covers everything a document goes on to reference, not just the file/URL passed to `htmldoc_addfile()`/`htmldoc_addurl()`: images, `<body background>` and any other file or URL HTMLDOC loads, and every hop of an HTTP redirect, are checked the same way, before any file is opened or any host is contacted. This holds for `htmldoc_addhtml()` markup too, and for images fetched later, during `convert2pdf()`/`convert2ps()` -- judged by the role calling that function. A refusal while resolving the argument itself, including any redirect it leads to, raises `permission denied` naming the file/URL that was actually refused; a refused image is silently left out of the output.

### License

[MIT](LICENSE)
