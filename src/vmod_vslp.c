/*-
 * Copyright 2009-2015 UPLEX - Nils Goroll Systemoptimierung
 * All rights reserved.
 *
 * Author: Julian Wiesener <jw@uplex.de>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <arpa/inet.h>

#include "cache/cache.h"
#if VMOD_ABI_VERSION == 40
#include "cache/cache_backend.h"
#else
#include "cache/cache_director.h"
#endif

#include "vrt.h"
#include "vbm.h"

#include "vcc_if.h"
#include "vslp_hash.h"
#include "vslp_dir.h"

struct vmod_vslp_vslp {
	unsigned		magic;
#define VMOD_VSLP_VSLP_MAGIC	0x6e63e1bf
	struct vslpdir		*vslpd;
};

VCL_VOID __match_proto__(td_vslp_vslp__init)
vmod_vslp__init(const struct vrt_ctx *ctx, struct vmod_vslp_vslp **vslpdp, const char *vcl_name)
{
	struct vmod_vslp_vslp *vslpd;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	AN(vslpdp);
	AZ(*vslpdp);
	ALLOC_OBJ(vslpd, VMOD_VSLP_VSLP_MAGIC);
	AN(vslpd);

	*vslpdp = vslpd;
	vslpdir_new(&vslpd->vslpd, vcl_name);
}

VCL_VOID __match_proto__(td_vslp_vslp__fini)
vmod_vslp__fini(struct vmod_vslp_vslp **vslpdp)
{
	struct vmod_vslp_vslp *vslpd;

	vslpd = *vslpdp;
	*vslpdp = NULL;
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);
	vslpdir_delete(&vslpd->vslpd);
	FREE_OBJ(vslpd);
}

VCL_VOID __match_proto__(td_vslp_vslp_add_backend)
vmod_vslp_add_backend(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_BACKEND be)
{
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);
	vslpdir_add_backend(vslpd->vslpd, be);
}

VCL_VOID __match_proto__(td_vslp_set_rampup_ratio)
vmod_vslp_set_rampup_ratio(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_REAL ratio)
{
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);
	vslpdir_set_rampup_ratio(vslpd->vslpd, ratio);
}

VCL_VOID __match_proto__(td_vslp_set_rampup_time)
vmod_vslp_set_rampup_time(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_DURATION duration)
{
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);
	vslpdir_set_rampup_time(vslpd->vslpd, duration);
}

VCL_VOID __match_proto__(td_vslp_vslp_set_hash)
vmod_vslp_set_hash(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_ENUM hash_m)
{
	hash_func hash_fp;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);

	hash_fp = vslp_get_hash_fp(hash_m);

	vslpdir_set_hash(vslpd->vslpd, hash_fp);
}

VCL_VOID __match_proto__(td_vslp_init_hashcircle)
vmod_vslp_init_hashcircle(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_INT replicas)
{
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);
	vslpdir_init_hashcircle(vslpd->vslpd, replicas);
}

VCL_INT __match_proto__(td_vslp_vslp_hash_string)
vmod_vslp_hash_string(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_STRING s, VCL_ENUM hash_m)
{
	uint32_t hash;
	hash_func hash_fp;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);

	hash_fp = vslp_get_hash_fp(hash_m);
	hash = hash_fp(s ? s : "");

	return (hash);
}

static VCL_BACKEND
_vmod_vslp_backend(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_INT n, VCL_BOOL altsrv_p, VCL_BOOL healthy,  VCL_INT i)
{
	uint32_t hash = (uint32_t) i;
	VCL_BACKEND be;

	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	CHECK_OBJ_NOTNULL(vslpd, VMOD_VSLP_VSLP_MAGIC);

	if(!hash) {
		/* client or backend context ? */
		struct http *http;
		if (ctx->http_req) {
			AN(http = ctx->http_req);
		} else {
			AN(ctx->http_bereq);
			AN(http = ctx->http_bereq);
		}
		hash = vslpd->vslpd->hash_fp(http->hd[HTTP_HDR_URL].b);
	}
	be = vslpdir_pick_be(vslpd->vslpd, ctx, hash, n, altsrv_p, healthy);

	return (be);
}

#if VMOD_ABI_VERSION == 40
VCL_BACKEND __match_proto__(td_vslp_vslp_backend)
vmod_vslp_backend(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd) {
	return _vmod_vslp_backend(ctx, vslpd, 0, 1, 1, 0);
}
VCL_BACKEND __match_proto__(td_vslp_vslp_backend_n)
vmod_vslp_backend_n(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_INT n, VCL_BOOL altsrv_p, VCL_BOOL healthy,  VCL_INT i) {
	return _vmod_vslp_backend(ctx, vslpd, n, altsrv_p, healthy, i);
}
VCL_BACKEND __match_proto__(td_vslp_vslp_backend_by_int)
vmod_vslp_backend_by_int(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_INT n) {
	return _vmod_vslp_backend(ctx, vslpd, 0, 1, 1, n);
}
#else
VCL_BACKEND __match_proto__(td_vslp_vslp_backend)
vmod_vslp_backend(const struct vrt_ctx *ctx, struct vmod_vslp_vslp *vslpd, VCL_INT n, VCL_BOOL altsrv_p, VCL_BOOL healthy,  VCL_INT i) {
	return _vmod_vslp_backend(ctx, vslpd, n, altsrv_p, healthy, i);
}
#endif
