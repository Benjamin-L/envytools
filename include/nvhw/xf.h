/*
 * Copyright (C) 2018 Marcin Kościelnicki <koriakin@0x04.net>
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef XF_H
#define XF_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t xf_s2lt(uint32_t x);
void xf_v2lt(uint32_t dst[3], const uint32_t src[3]);
uint32_t xf_sum(const uint32_t *v, int n, int version);
uint32_t xf_min(uint32_t a, uint32_t b);
uint32_t xf_max(uint32_t a, uint32_t b);
bool xf_islt(uint32_t a, uint32_t b);
uint32_t xf_rcp(uint32_t x, bool rcc, bool v2);
uint32_t xf_rsq(uint32_t x, bool v2);
uint32_t xf_exp_flr(uint32_t x);
uint32_t xf_exp_frc(uint32_t x);
uint32_t xf_exp(uint32_t x);
uint32_t xf_log_e(uint32_t x);
uint32_t xf_log_f(uint32_t x);
uint32_t xf_log(uint32_t x);
int32_t xf_pre_exp(uint32_t x);
void xf_lit(uint32_t dst[4], uint32_t src[4]);

extern const uint8_t xf_rcp_lut_v1[0x40];
extern const uint8_t xf_rcp_lut_v2[0x40];
extern const uint8_t xf_rsq_lut_v1[0x80];
extern const uint8_t xf_rsq_lut_v2[0x80];

static inline uint32_t xf_add(uint32_t a, uint32_t b, int version) {
	uint32_t v[2] = {a, b};
	return xf_sum(v, 2, version);
}

#define xf_sum3(v, r) xf_sum(v, 3, r)
#define xf_sum4(v, r) xf_sum(v, 4, r)

#ifdef __cplusplus
}
#endif

#endif
