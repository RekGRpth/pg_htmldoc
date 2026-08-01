$(OBJS): Makefile
DATA = pg_htmldoc--1.0.sql
EXTENSION = pg_htmldoc
MODULE_big = $(EXTENSION)
OBJS = $(EXTENSION).o
PG_CONFIG = pg_config
REGRESS = $(patsubst sql/%.sql,%,$(TESTS))
TESTS = $(wildcard sql/*.sql)
PGXS = $(shell $(PG_CONFIG) --pgxs)
SHLIB_LINK = -lhtmldoc
include $(PGXS)
