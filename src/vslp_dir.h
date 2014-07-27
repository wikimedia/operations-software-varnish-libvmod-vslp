/*-
 * Copyright (c) 2009-2013 UPLEX - Nils Goroll Systemoptimierung
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

struct vbitmap;

struct be_state {
	unsigned				magic;
#define VSLP_BESTATE_MAGIC			0x14e33bd2
	time_t					recover_time;
	uint32_t				state;
	uint32_t				canon_point;
};

struct vslp_hostnode {
	uint32_t		point;
	unsigned int		host;
};

struct vslp_host {
	VCL_BACKEND		*backend;
	uint32_t		canon_point;
};

struct vslpdir {
	unsigned				magic;
#define VSLPDIR_MAGIC				0xdbb7d59f
	pthread_mutex_t				mtx;
	unsigned				n_backend;
	unsigned				l_backend;
	VCL_BACKEND				*backend;
	struct be_state				*bstate;
	time_t					rampup_time;
	double					altsrv_p;
	hash_func				hash_fp;
	struct vslp_hostnode			*hashcircle;
	VCL_INT					replicas;
};

void vslpdir_new(struct vslpdir **vslpdp, const char *vcl_name);
void vslpdir_delete(struct vslpdir **vslpdp);
void vslpdir_add_backend(struct vslpdir *vslpd, VCL_BACKEND be);
void vslpdir_set_rampup_ratio(struct vslpdir *vslpd, VCL_REAL ratio);
void vslpdir_set_rampup_time(struct vslpdir *vslpd, VCL_DURATION duration);
void vslpdir_set_hash(struct vslpdir *vslpd, hash_func hash_m);
void vslpdir_init_hashcircle(struct vslpdir *vslpd, VCL_INT replicas);
void vslpdir_lock(struct vslpdir *vslpd);
void vslpdir_unlock(struct vslpdir *vslpd);
void vslpdir_expand(struct vslpdir *vslpd, unsigned n);
unsigned vslpdir_any_healthy(struct vslpdir *vslpd);
VCL_BACKEND vslpdir_pick_be(struct vslpdir *vslpd, const struct vrt_ctx *ctx, uint32_t hash,
	       VCL_INT n_retry, VCL_BOOL altsrv_p, VCL_BOOL healthy);
