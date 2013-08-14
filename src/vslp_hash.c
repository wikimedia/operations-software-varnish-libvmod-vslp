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

#include <stdint.h>
#include <string.h>

#include "cache/cache.h"

#include "vrt.h"
#include "vgz.h"
#include "vsha256.h"

#include "vslp_hash.h"

uint32_t vslp_hash_crc32(VCL_STRING s)
{
	uint32_t crc;
	crc = crc32(~0U, (const unsigned char*)s, strlen(s));
	crc ^= ~0U;
	crc++;

	return (crc ? crc : 1);
}

uint32_t vslp_hash_sha256(VCL_STRING s)
{
	uint32_t res = 0;
	struct SHA256Context sha256;
	union {
		unsigned char digest[32];
		uint32_t uint32_digest[8];
	} sha256_digest;

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, s, strlen(s));
	SHA256_Final(sha256_digest.digest, &sha256);

	/*
	* use low 32 bits only
	* XXX: Are these the best bits to pick?
	*/
	res = sha256_digest.uint32_digest[7];
	res++;

	return (res ? res : 1);
}

uint32_t vslp_hash_rs(VCL_STRING s)
{
	uint32_t res = 0;
	/* hash function from Robert Sedgwicks 'Algorithms in C' book */
	const uint32_t b    = 378551;
	uint32_t a          = 63689;

	while (*s) {
		res = res * a + (*s++);
		a *= b;
	}
	res++;

	return (res ? res : 1);
}

hash_func vslp_get_hash_fp(VCL_ENUM hash_m)
{
	if (!strcmp(hash_m, "CRC32"))
		return vslp_hash_crc32;
	if (!strcmp(hash_m, "SHA256"))
		return vslp_hash_sha256;
	if (!strcmp(hash_m, "RS"))
		return vslp_hash_rs;
	WRONG("Illegal VMOD enum");
}
