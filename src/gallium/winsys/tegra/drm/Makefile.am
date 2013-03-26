include Makefile.sources
include $(top_srcdir)/src/gallium/Automake.inc

AM_CFLAGS = \
	-I$(top_srcdir)/src/gallium/drivers \
	$(GALLIUM_WINSYS_CFLAGS)

if HAVE_GALLIUM_TEGRA
AM_CFLAGS += -DGALLIUM_TEGRA
endif

if HAVE_GALLIUM_GRATE
AM_CFLAGS += -DGALLIUM_GRATE \
	$(LIBDRM_CFLAGS)
endif

noinst_LTLIBRARIES = libtegradrm.la

libtegradrm_la_SOURCES = $(C_SOURCES)

EXTRA_DIST = \
	meson.build
