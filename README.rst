===============
vmod_vslp
===============

------------------------------------------
Varnish Consistent Hashing Director Module
------------------------------------------

:Author: Julian Wiesener
:Date: 2013-08-14
:Version: 1.0
:Manual section: 3

.. _synopsis:

SYNOPSIS
========

::

	import vslp;

        sub vcl_init {
                new vd = vslp.vslp();
                vd.add_backend(be1);
                vd.add_backend(be2);
                vd.add_backend(be3);
                vd.init_hashcircle(150); #number of replicas
        }

	sub vcl_recv {
	    	set req.backend = vd.backend();
        }


DESCRIPTION
===========

FUNCTIONS
=========

LIMITATIONS
===========

INSTALLATION
============

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the varnishtest tool.

Usage::

 ./configure VARNISHSRC=DIR [VMODDIR=DIR]

`VARNISHSRC` is the directory of the Varnish source tree for which to
compile your vmod.

Optionally you can also set the vmod install directory by adding
`VMODDIR=DIR` (defaults to the pkg-config discovered directory from your
Varnish installation).

Make targets:

* make - builds the vmod
* make install - installs your vmod in `VMODDIR`
* make check - runs the unit tests in ``src/tests/*.vtc``


HISTORY
=======

Version 1.0: Initial version.

COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-vslp project. See LICENSE for details.

Copyright (c) 2013 UPLEX Nils Goroll Systemoptimierung. All rights
reserved.
