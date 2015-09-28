===============
vmod_vslp
===============

------------------------------------------
Varnish Consistent Hashing Director Module
------------------------------------------

:Author: Julian Wiesener
:Date: 2014-07-16
:Version: 1.1
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
               set req.backend_hint = vd.backend();
        }

	# and/or

	sub vcl_backend_fetch {
               set bereq.backend = vd.backend();
        }


DESCRIPTION
===========

VSLP Director - Varnish StateLess Persistence Director

The purpose of this director is to provide stateless persistence (or
persistency if you prefer) for backend requests based upon this simple
concept:

* Generate a load balancing key, which will be used to select the backend
  The key values should be as uniformly distributed as possible.
  For all requests which need to hit the same backend server, the same
  key must be generated.
  For strings, a hash function can be used to generate the key.

* Select the preferred backend server using an implementation of consistent
  hashing (cf. Karger et al, references below), which ensures that the same
  hosts are always chosen for every key (for instance hash of incoming URL)
  in the same order (i.e. if the preferred host is down, then alternative
  hosts are always chosen in a fixed order).

* Initialize a circular data structure consisting of the hash values of
  "hostname%d" for each host and for a running number from 1 to n (n is
  the number of "replicas").

* For the load balancing key, find the smallest hash value in the circle
  that is larger than the key (searching clockwise and wrapping around
  as necessary).

* If the host thus selected is down, choose alternative hosts by
  continuing to search clockwise in the circle.

On consistent hashing see:

* http://www8.org/w8-papers/2a-webserver/caching/paper2.html
* http://www.audioscrobbler.net/development/ketama/
* svn://svn.audioscrobbler.net/misc/ketama
* http://en.wikipedia.org/wiki/Consistent_hashing

This technique allowes to implement persistence without keeping any state,
and, in particular, without the need to synchronize state between nodes of a
cluster of Varnish servers.

Still, all members of a cluster will, if otherwise configured equally,
implement the same persistence, in other words, requests sharing a common
persistence criterium will be balanced onto the same backend server no matter
which Varnish server handles the request.

The drawbacks are:

* the distribution of requests depends on the number of requests per key and
  the uniformity of the distribution of key values. In short, this technique
  will generally lead to less good load balancing compared to stateful
  techniques.

* When a backend server becomes unavailable, every persistence technique has
  to reselect a new backend server, but this technique will also switch back
  to the preferred server once it becomes healthy again, so the persistence
  is generally less stable compared to stateful techniques (which would
  continue to use a selected server for as long as possible (or dictated by a
  TTL)).

The name VSLP was chosen to accompany VTLA as another VETR (Varnish
Exception To the Rule) ;-) http://varnish-cache.org/wiki/VTLA



FUNCTIONS
=========

_Note_ The vmod functions differ varnish 4.0 and 4.1. This is the
documentation for the varnish 4.1 interface.

VSLP needs to be configured before it can hand out backends. The functions not
returning a backend should only be called in vcl_init, as we can safely skip
locking for most parts of the code, if we do not modify the hash ring after
initialization.

VOID .add_backend(BACKEND)
--------------------------
Adds a backend to the Director, must be called in before init_hashcircle().

VOID .set_rampup_ratio(REAL)
----------------------------

Sets the ratio of requests (0.0 to 1.0) that goes to the next alternate backend
to warm it up when the preferd backend is healthy. If the preferred backend just
recovered from being unhealthy, the ratio will be used to warm the preferred
backend. Defaults to 0.1 (10%).

VOID .set_rampup_time(DURATION)
-------------------------------

Sets the duration that will be used to determine if a backend will be counted
as warmed after recovering from an unhealthy state. During the warm-up period,
only the rampup-ratio of requests will go to the recoverd backend. Defaults
to 60 seconds.

VOID .set_hash(ENUM { CRC32, SHA256, RS })
------------------------------------------

Sets the default hash algorithm used to choose a backend by backend(). Defaults
to CRC32.

VOID .init_hashcircle(INT)
--------------------------

Initializes the hash ring. This function must be called after all backends are
added. The argument is the numbers of replicas the hash ring contains for each
backend.

INT .hash_string(STRING string, ENUM { CRC32, SHA256, RS } alg)
---------------------------------------------------------------

Returns the hash of its first argument using the hash
algorithm defined, defaults to CRC32.


BACKEND .backend(INT nte, BOOL altsrv_p, BOOL healthy, INT hash)
----------------------------------------------------------------

Returns the nth backend with respect of altsrv_p  and respect of its healthy
state for the given hash. All parameters are optional, the defaults are:

 nte=0 will pick the first backend under respect of VSLP rules
 altsrv_p=ture
 healthy=true
 hash=0 will use a CRC32 of the request URL


LIMITATIONS
===========

* The number of backends per director is limited to 64.
* Adding backends after initializing the hash ring is possible, but invalid, as
  such backends will not be choosen. Calling init_hashcircle() twice on the same
  VSLP instance is invalid.


INSTALLATION
============

The source tree is based on autotools to configure the building, and
does also have the necessary bits in place to do functional unit tests
using the ``varnishtest`` tool.

Building requires the Varnish header files and uses pkg-config to find
the necessary paths.

Usage::

 ./autogen.sh
 ./configure

If you have installed Varnish to a non-standard directory, call
``autogen.sh`` and ``configure`` with ``PKG_CONFIG_PATH`` pointing to
the appropriate path. For example, when varnishd configure was called
with ``--prefix=$PREFIX``, use

 PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
 export PKG_CONFIG_PATH

Make targets:

* make - builds the vmod.
* make install - installs your vmod.
* make check - runs the unit tests in ``src/tests/*.vtc``
* make distcheck - run check and prepare a tarball of the vmod.


MISSING
=======
* Documentation of interactions with restarts/retries

HISTORY
=======

Version 1.0: Initial version.

ACKNOWLEDGEMENTS
================

Development of this module was partly sponsored by Deutsche Telekom AG
– Products & Innovation


COPYRIGHT
=========

This document is licensed under the same license as the
libvmod-vslp project. See LICENSE for details.

Copyright 2013-2015 UPLEX Nils Goroll Systemoptimierung. All rights reserved.
