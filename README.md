PostgreSQL implementation of HTMLDOC conversion software. It is a program that reads HTML and Markdown source files or web pages and generates corresponding EPUB, HTML, PostScript, or PDF files with an optional table of contents.

### [Use of the extension](#use-of-the-extension)

```sql
select htmldoc_addurl('https://github.com');
copy (
    select convert2pdf()
) to '/var/lib/postgresql/htmldoc.pdf' WITH (FORMAT binary, HEADER false)
```
