$(OBJS): Makefile
DATA = pg_htmldoc--1.0.sql
EXTENSION = pg_htmldoc
MODULE_big = $(EXTENSION)
OBJS = $(EXTENSION).o pg_whitelist/pg_whitelist.o
PG_CONFIG = pg_config
PG_CPPFLAGS = -Ipg_whitelist
REGRESS = $(patsubst sql/%.sql,%,$(TESTS))
TESTS = $(wildcard sql/*.sql)
PGXS = $(shell $(PG_CONFIG) --pgxs)
SHLIB_LINK = -lhtmldoc -ldl
include $(PGXS)
