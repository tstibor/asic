SUBDIRS = lib

bin_PROGRAMS = asic
asic_CFLAGS = -I@ASI_SDK_DIR@/include -I$(top_srcdir)/src/lib
asic_SOURCES = asic.c
asic_LDADD = $(INTI_LIBS) $(top_srcdir)/src/lib/libasi_util.a
