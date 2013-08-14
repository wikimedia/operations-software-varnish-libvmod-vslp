
AM_CPPFLAGS = \
	-I$(VARNISHSRC)/include \
	-I$(VARNISHSRC)/bin/varnishd \
	-I$(VARNISHSRC)/lib/libvgz

AM_LDFLAGS  = $(AM_LT_LDFLAGS)

vmoddir = $(VMODDIR)
vmod_LTLIBRARIES = libvmod_vslp.la
vmod_srcdir = $(top_srcdir)/src
vmodtool = $(VARNISHSRC)/lib/libvcl/vmodtool.py

libvmod_vslp_la_LDFLAGS = $(AM_LDFLAGS) -module -export-dynamic -avoid-version -shared

libvmod_vslp_la_SOURCES = \
	vmod_vslp.c \
	vslp_dir.c \
	vslp_hash.c

nodist_libvmod_vslp_la_SOURCES = \
	vcc_if.c \
	vcc_if.h

# BUILT_SOURCES is only a hack and dependency tracking does not help for the first build
vmod_vslp.lo: vcc_if.h

vcc_if.c vcc_if.h: $(vmodtool) $(vmod_srcdir)/vmod_vslp.vcc
	@PYTHON@ $(vmodtool) $(vmod_srcdir)/vmod_vslp.vcc

VMOD_TESTS = tests/*.vtc


check: $(VARNISHSRC)/bin/varnishtest/varnishtest
	$(VARNISHSRC)/bin/varnishtest/varnishtest \
		-Dvarnishd=$(VARNISHSRC)/bin/varnishd/varnishd \
		-Dvmod_topbuild=$(abs_top_builddir) \
		tests/*.vtc

EXTRA_DIST = \
	vmod_vslp.vcc	\
	$(VMOD_TESTS)

CLEANFILES = $(builddir)/vcc_if.c $(builddir)/vcc_if.h

