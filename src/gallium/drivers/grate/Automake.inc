if HAVE_GALLIUM_GRATE

TARGET_DRIVERS += grate
TARGET_CPPFLAGS += -DGALLIUM_GRATE
TARGET_LIB_DEPS += \
	$(top_builddir)/src/gallium/winsys/grate/drm/libtegradrm.la \
	$(top_builddir)/src/gallium/drivers/grate/libtegra.la \
	$(GRATE_LIBS) \
	$(LIBDRM_LIBS)

endif
