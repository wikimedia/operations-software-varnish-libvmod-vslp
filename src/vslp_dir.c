/*-
 * Copyright (c) 2009-2013 UPLEX - Nils Goroll Systemoptimierung
 * All rights reserved.
 *
 * Authors: Nils Goroll <nils.goroll@uplex.de>
 *         Geoffrey Simmons <geoff.simmons@uplex.de>
 *         Julian Wiesener <jw@uplex.de>
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
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "cache/cache.h"
#include "cache/cache_backend.h"

#include "vrt.h"

#include "vslp_hash.h"
#include "vslp_dir.h"

typedef int (*compar)( const void*, const void* );

struct vslp_state {
	struct vslpdir *vslpd;
	uint64_t picklist;
	int idx;
	const struct vrt_ctx *ctx;
};

static int
hostnode_compare(struct vslp_hostnode *a, struct vslp_hostnode *b)
{
	return (a->point == b->point) ? 0 : ((a->point > b->point) ? 1 : -1);
}

static int
vslp_get_prefered_idx(const struct vslpdir *vslpd, const uint32_t key)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);

	const int n = vslpd->n_backend * vslpd->replicas;
	int idx = -1, high = n, low = 0, i;

        do {
            i = (high + low) >> 1;
            if (vslpd->hashcircle[i].point == key)
                idx = i;
            else if (i == n - 1)
                idx = n - 1;
            else if (vslpd->hashcircle[i].point < key && vslpd->hashcircle[i+1].point >= key)
                idx = i + 1;
            else if (vslpd->hashcircle[i].point > key)
                if (i == 0)
                    idx = 0;
                else
                    high = i;
            else
                low = i;
        } while (idx == -1);

	return idx;
}

static int //0=recoverey state, 1=healthy state
vslp_be_healthy(struct vslp_state *state, int chosen)
{
	int rval = 0;
	time_t now;

	AN(state);
	CHECK_OBJ_NOTNULL(state->vslpd, VSLPDIR_MAGIC);
	vslpdir_lock(state->vslpd);

	if(!state->vslpd->bstate[chosen].state)
	{
		now = time(NULL);
		if(state->vslpd->bstate[chosen].recover_time == 0)
			state->vslpd->bstate[chosen].recover_time = now;
		if (now >= (state->vslpd->bstate[chosen].recover_time + state->vslpd->rampup_time))
		{
			state->vslpd->bstate[chosen].state = 1;
			state->vslpd->bstate[chosen].recover_time = 0;
			rval = 1;
		}
	}
	else
		rval = 1;

	vslpdir_unlock(state->vslpd);
	return rval;
}

static void
vslp_be_unhealthy(struct vslp_state *state, int chosen)
{
	AN(state);
	CHECK_OBJ_NOTNULL(state->vslpd, VSLPDIR_MAGIC);
	vslpdir_lock(state->vslpd);

	if(state->vslpd->bstate[chosen].state)
	{
		state->vslpd->bstate[chosen].state = 0;
		state->vslpd->bstate[chosen].recover_time = 0;
	}
	vslpdir_unlock(state->vslpd);
}

static int
vslp_choose_next(struct vslp_state *state, uint32_t n_retry)
{
	int i, chosen = 0;

	AN(state);
	CHECK_OBJ_NOTNULL(state->vslpd, VSLPDIR_MAGIC);

	if(state->idx < 0 || n_retry >= state->vslpd->n_backend)
		return -1;

	do
	{
		i = state->idx + n_retry;
		if(i >= (state->vslpd->n_backend * state->vslpd->replicas))
		i = i - (state->vslpd->n_backend * state->vslpd->replicas);
	

		chosen = state->vslpd->hashcircle[i].host;
		n_retry++;
		if(n_retry > (state->vslpd->n_backend * state->vslpd->replicas) + state->vslpd->n_backend)
			return -1;
	} while (state->picklist & (1 << chosen));
	state->picklist |= 1 << chosen;

	return chosen;
}

static int
vslp_choose_next_healthy(struct vslp_state *state, uint32_t n_retry)
{
	int i, max_iter, chosen;
	VCL_BACKEND be;

	AN(state);
	CHECK_OBJ_NOTNULL(state->vslpd, VSLPDIR_MAGIC);

	max_iter = state->vslpd->n_backend - n_retry;
	for(i = 0; i < max_iter; i++, n_retry++)
	{
		chosen = vslp_choose_next(state, n_retry);
		if(chosen == -1)
			break;

		be = state->vslpd->backend[chosen];
		AN(be);

		if(be->healthy(be, NULL))
		{
			vslp_be_healthy(state, chosen);
			break;
		}
		vslp_be_unhealthy(state, chosen);
		chosen = -1;
	}

	return chosen;
}

void
vslpdir_new(struct vslpdir **vslpdp, const char *vcl_name)
{
	struct vslpdir *vslpd;

	AN(vcl_name);
	AN(vslpdp);
	AZ(*vslpdp);
	ALLOC_OBJ(vslpd, VSLPDIR_MAGIC);
	AN(vslpd);
	*vslpdp = vslpd;
	AZ(pthread_mutex_init(&vslpd->mtx, NULL));

	ALLOC_OBJ(vslpd->bstate, VSLP_BESTATE_MAGIC);
	vslpd->rampup_time = 60;
	vslpd->altsrv_p = 0.10;
	vslpd->hash_fp = vslp_hash_crc32;
}

void
vslpdir_delete(struct vslpdir **vslpdp)
{
	struct vslpdir *vslpd;

	AN(vslpdp);
	vslpd = *vslpdp;
	*vslpdp = NULL;

	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);

	free(vslpd->backend);
	AZ(pthread_mutex_destroy(&vslpd->mtx));
	FREE_OBJ(vslpd->bstate);
	free(vslpd->hashcircle);
	FREE_OBJ(vslpd);
}

void
vslpdir_add_backend(struct vslpdir *vslpd, VCL_BACKEND be)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	AN(be);
	vslpdir_lock(vslpd);
	if(vslpd->n_backend >= 64)
	{
		vslpdir_unlock(vslpd);
		WRONG("VSLP supports only up to 64 backends");
	}
	if(vslpd->n_backend >= vslpd->l_backend)
		vslpdir_expand(vslpd, vslpd->l_backend + 16);
	assert(vslpd->n_backend <= vslpd->l_backend);
	vslpd->backend[vslpd->n_backend] = be;
	vslpd->bstate[vslpd->n_backend].state = 1;
	vslpd->n_backend++;
	vslpdir_unlock(vslpd);
}

void
vslpdir_set_rampup_ratio(struct vslpdir *vslpd, VCL_REAL ratio)
{
	assert(ratio >= 0.0 && ratio < 1.0);
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	vslpdir_lock(vslpd);
	vslpd->altsrv_p = ratio; 
	vslpdir_unlock(vslpd);
}

void
vslpdir_set_rampup_time(struct vslpdir *vslpd, VCL_DURATION duration)
{
	assert(duration >= 0.0);
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);

	time_t t = (time_t) floor(duration);

	vslpdir_lock(vslpd);
	vslpd->rampup_time = t; 
	vslpdir_unlock(vslpd);
}

void
vslpdir_set_hash(struct vslpdir *vslpd, hash_func hash_m)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	AN(hash_m);
	vslpdir_lock(vslpd);
	vslpd->hash_fp = hash_m;
	vslpdir_unlock(vslpd);
}

void
vslpdir_init_hashcircle(struct vslpdir *vslpd, VCL_INT replicas)
{
	int i, j;

	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	AZ(vslpd->hashcircle);

	vslpdir_lock(vslpd);
	vslpd->hashcircle = calloc(vslpd->n_backend * replicas,
		sizeof(struct vslp_hostnode));
	AN(vslpd->hashcircle);
	AN(vslpd->backend);
	if(vslpd->backend[0] == NULL)
	{
		vslpdir_unlock(vslpd);
		WRONG("VSLP director doesn't have any backends");
	}
	vslpd->replicas = replicas;
		

        for (i = 0; i < vslpd->n_backend; i++) {
                for (j = 0; j < replicas; j++) {
                        int len = strlen(vslpd->backend[i]->vcl_name)
                            + (j == 0 ? 0 : log10(j)) + 2;
                        char s[len];

                        sprintf(s, "%s%d", vslpd->backend[i]->vcl_name, j);
                        vslpd->hashcircle[i * replicas + j].point =
                            vslp_hash_sha256(s);
                        vslpd->hashcircle[i * replicas + j].host = i;
                }
                vslpd->bstate[i].canon_point = vslpd->hashcircle[i * replicas].point;
        }
        qsort( (void *) vslpd->hashcircle, vslpd->n_backend * replicas,
            sizeof (struct vslp_hostnode), (compar) hostnode_compare);

        for (i = 0; i < vslpd->n_backend; i++)
	{
        	for (j = 0; j < replicas; j++)
		{
			VSL(SLT_Debug, 0, "VSLP hashcircle[%5ld] = {point = %8x, host = %2d}\n",
			    i * replicas + j,
			    vslpd->hashcircle[i * replicas + j].point,
			    vslpd->hashcircle[i * replicas + j].host);
		}
	}

	vslpdir_unlock(vslpd);
}

void
vslpdir_lock(struct vslpdir *vslpd)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	AZ(pthread_mutex_lock(&vslpd->mtx));
}

void
vslpdir_unlock(struct vslpdir *vslpd)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	AZ(pthread_mutex_unlock(&vslpd->mtx));
}

void vslpdir_expand(struct vslpdir *vslpd, unsigned n)
{
	CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);

	vslpd->backend = realloc(vslpd->backend, n * sizeof *vslpd->backend);
	AN(vslpd->backend);
	vslpd->bstate = realloc(vslpd->bstate, n * sizeof *vslpd->bstate);
	AN(vslpd->bstate);
	vslpd->l_backend = n;
}

unsigned
vslpdir_any_healthy(struct vslpdir *vslpd)
{
	unsigned retval = 0;
        VCL_BACKEND be;
        unsigned u;

        CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
        vslpdir_lock(vslpd);
        for (u = 0; u < vslpd->n_backend; u++) {
		be = vslpd->backend[u];
		CHECK_OBJ_NOTNULL(be, DIRECTOR_MAGIC);
		if (be->healthy(be, NULL)) {
			retval = 1;
			break;
		}
	}
        vslpdir_unlock(vslpd);

	return (retval);
}

VCL_BACKEND vslpdir_pick_be(struct vslpdir *vslpd, const struct vrt_ctx *ctx, uint32_t hash)
{
	VCL_BACKEND be;
	int chosen, be_choice, restarts_o, restarts, n_retry = 0;
	struct vslp_state state;

        CHECK_OBJ_NOTNULL(vslpd, VSLPDIR_MAGIC);
	CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
	AN(ctx->vsl);

	be_choice = (scalbn(random(), -31) > vslpd->altsrv_p);

	if (ctx->bo) {
		restarts = restarts_o = ctx->bo->retries;
	} else {
		AN(ctx->req);
		restarts = restarts_o = ctx->req->restarts;
	}

	state.picklist = 0;
	state.vslpd = vslpd;
	state.ctx = ctx;

	if(vslpd->hashcircle == NULL)
		WRONG("Incomplete VSLP hashcircle");

	state.idx = vslp_get_prefered_idx(vslpd, hash);
	chosen = vslp_choose_next(&state, n_retry);

	if(chosen >= 0) {
		VSLb(ctx->vsl, SLT_Debug,
		    "VSLP picked preferred backend %2i for key %8x",
		    chosen, hash);
		be = vslpd->backend[chosen];
		AN(be);
	} else {
		be = NULL;
	}
	
	while(restarts > 0)
	{
		n_retry++;

		chosen = vslp_choose_next_healthy(&state, n_retry);

		if(chosen < 0) {
			VSLb(ctx->vsl, SLT_Debug,
			    "VSLP failed to find other healthy backend for key %8x in restarts: %2i/%2i - using previous",
			    hash, restarts, restarts_o);
		} else {
			VSLb(ctx->vsl, SLT_Debug,
			    "VSLP picked backend %2i for key %8x in restarts: %2i/%2i",
			    chosen, hash, restarts, restarts_o);
			be = vslpd->backend[chosen];
			AN(be);
		}

		if(--restarts <= 0)
			return be;
	}

	if (! be) {
		VSLb(ctx->vsl, SLT_Debug, "VSLP no backend found");
		return NULL;
	}

	if (be->healthy(be, NULL))
	{
		if(!vslp_be_healthy(&state, chosen))
			 be_choice ^= be_choice;

		if(!be_choice)
		{
			chosen = vslp_choose_next_healthy(&state, n_retry);
			if(chosen < 0) {
				VSLb(ctx->vsl, SLT_Debug,
				    "VSLP found no alternative backend in healthy");
			} else {
				VSLb(ctx->vsl, SLT_Debug,
				    "VSLP picked alternative backend %2i for key %8x in healthy",
				    chosen, hash);
				be = vslpd->backend[chosen];
			}
		}
	}
	else
	{
		vslp_be_unhealthy(&state, chosen);
		chosen = vslp_choose_next_healthy(&state, n_retry);
		if(chosen < 0) {
			VSLb(ctx->vsl, SLT_Debug,
			    "VSLP found no alternative backend in unhealthy");
		} else {
			VSLb(ctx->vsl, SLT_Debug,
			    "VSLP picked alternative backend %2i for key %8x in unhealthy",
			    chosen, hash);
			be = vslpd->backend[chosen];
		}
	}

	AN(be);
	return (be);
}
