include $(top_srcdir)/src/gallium/Automake.inc

noinst_LTLIBRARIES = libtegra.la

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/gallium/drivers/tegra/include \
	-I$(top_srcdir)/src/gallium/drivers \
	-I$(top_srcdir)/include

AM_CFLAGS = \
	$(GALLIUM_CFLAGS) \
	$(LIBDRM_CFLAGS)

libtegra_la_SOURCES = \
	class_ids.h \
	fp_ir.c \
	fp_ir.h \
	host1x01_hardware.h \
	hw_host1x01_uclass.h \
	tegra_common.h \
	tegra_compiler_fp.c \
	tegra_compiler_vpe.c \
	tegra_compiler.h \
	tegra_context.c \
	tegra_context.h \
	tegra_draw.c \
	tegra_draw.h \
	tegra_fence.h \
	tegra_program.c \
	tegra_program.h \
	tegra_resource.c \
	tegra_resource.h \
	tegra_screen.c \
	tegra_screen.h \
	tegra_state.c \
	tegra_state.h \
	tegra_stream.c \
	tegra_stream.h \
	tegra_surface.c \
	tegra_surface.h \
	tegra_texture.c \
	tegra_texture.h \
	vpe_ir.c \
	vpe_ir.h

noinst_PROGRAMS = tegra_compiler

tegra_compiler_SOURCES = \
	fp_ir.c \
	fp_ir.h \
	tegra_compiler.c \
	tegra_compiler.h \
	tegra_compiler_cmdline.c \
	vpe_ir.c \
	vpe_ir.h

tegra_compiler_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/src/gallium/auxiliary \
	-I$(top_srcdir)/src/gallium/include \
	-I$(top_srcdir)/src/mapi \
	-I$(top_srcdir)/src/mesa

tegra_compiler_LDADD = \
	$(top_builddir)/src/gallium/auxiliary/libgallium.la \
	$(top_builddir)/src/compiler/nir/libnir.la \
	$(top_builddir)/src/compiler/glsl/libstandalone.la \
	$(top_builddir)/src/util/libmesautil.la \
	$(top_builddir)/src/mesa/libmesagallium.la \
	$(GALLIUM_COMMON_LIB_DEPS)
