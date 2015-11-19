# contrib/pg_sulog/Makefile

MODULE_big = pg_sulog
OBJS = pg_sulog.o $(WIN32RES)

EXTENSION = pg_sulog
DATA = pg_sulog--1.0.sql
PGFILEDESC = "pg_sulog - PostgreSQL superuser force logging."

REGRESS = pg_sulog
REGRESS_OPTS = --temp-config=$(top_srcdir)/contrib/pg_sulog/pg_sulog.conf

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/pg_sulog
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
