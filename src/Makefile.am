AM_CPPFLAGS = @VMOD_INCLUDES@ -Wall -Werror

vmoddir = @VMOD_DIR@
vmod_LTLIBRARIES = libvmod_vslp.la

libvmod_vslp_la_LDFLAGS = -module -export-dynamic -avoid-version -shared

libvmod_vslp_la_SOURCES = \
	vmod_vslp.c \
	vslp_dir.c \
	vslp_dir.h \
	vslp_hash.c \
	vslp_hash.h \
	crc32.c \
	crc32.h

nodist_libvmod_vslp_la_SOURCES = \
	vcc_if.c \
	vcc_if.h

vmod_vslp.lo: vcc_if.c vcc_if.h

vcc_if.c: vcc_if.h

if HAVE_VMOD_ABI_4_1
vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_vslp.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_vslp.vcc
else
vcc_if.h: @VMODTOOL@ $(top_srcdir)/src/vmod_vslp.40.vcc
	@VMODTOOL@ $(top_srcdir)/src/vmod_vslp.40.vcc
endif

VMOD_TESTS = $(top_srcdir)/src/tests/*.vtc

if HAVE_VMOD_ABI_4_1
VMOD_TESTS += $(top_srcdir)/src/tests/v41/*.vtc
else
VMOD_TESTS += $(top_srcdir)/src/tests/v40/*.vtc
endif

.PHONY: $(VMOD_TESTS)

$(top_srcdir)/src/tests/*.vtc $(top_srcdir)/src/tests/*/*.vtc: libvmod_vslp.la
	@VARNISHTEST@ -Dvarnishd=@VARNISHD@ -Dvmod_topbuild=$(abs_top_builddir) $@

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_vslp.vcc	\
	$(VMOD_TESTS)

CLEANFILES = \
	$(builddir)/vcc_if.c \
	$(builddir)/vcc_if.h \
	$(builddir)/vmod_vslp.rst \
	$(builddir)/vmod_vslp.man.rst
