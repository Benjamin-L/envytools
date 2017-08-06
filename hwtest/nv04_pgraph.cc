/*
 * Copyright (C) 2016 Marcin Kościelnicki <koriakin@0x04.net>
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

#include "pgraph.h"
#include "hwtest.h"
#include "old.h"
#include "nvhw/pgraph.h"
#include "nva.h"
#include <initializer_list>

static void nv04_pgraph_prep_mthd(struct hwtest_ctx *ctx, uint32_t grobj[4], struct pgraph_state *state, uint32_t cls, uint32_t addr, uint32_t val, bool fix_cls = true) {
	if (extr(state->debug_d, 16, 1) && ctx->chipset.card_type >= 0x10 && fix_cls) {
		if (cls == 0x56)
			cls = 0x85;
		if (cls == 0x62)
			cls = 0x82;
		if (cls == 0x63)
			cls = 0x67;
		if (cls == 0x89)
			cls = 0x87;
		if (cls == 0x7b)
			cls = 0x79;
	}
	int chid = extr(state->ctx_user, 24, 7);
	state->fifo_ptr = 0;
	int subc = extr(addr, 13, 3);
	uint32_t mthd = extr(addr, 2, 11);
	if (state->chipset.card_type < 0x10) {
		state->fifo_mthd_st2 = chid << 15 | addr >> 1 | 1;
	} else {
		state->fifo_mthd_st2 = chid << 20 | subc << 16 | mthd << 2 | 1 << 26;
		insrt(state->ctx_switch_a, 23, 1, 0);
		// XXX: figure this out
		if (mthd != 0x104)
			insrt(state->debug_d, 12, 1, 0);
		uint32_t save = state->ctx_switch_a;
		state->ctx_switch_a = cls;
		if (nv04_pgraph_is_3d_class(state))
			insrt(state->debug_d, 27, 1, 0);
		state->ctx_switch_a = save;
		if (nv04_pgraph_is_nv15p(&ctx->chipset)) {
			insrt(state->ctx_user, 31, 1, 0);
			insrt(state->debug_d, 8, 1, 0);
		}
	}
	state->fifo_data_st2[0] = val;
	state->fifo_enable = 1;
	state->ctx_control |= 1 << 16;
	insrt(state->ctx_user, 13, 3, extr(addr, 13, 3));
	int old_subc = extr(state->ctx_user, 13, 3);
	for (int i = 0; i < 4; i++)
		grobj[i] = jrand48(ctx->rand48);
	uint32_t inst;
	if (extr(addr, 2, 11) == 0) {
		insrt(grobj[0], 0, 8, cls);
		if (state->chipset.card_type >= 0x10) {
			insrt(grobj[0], 23, 1, 0);
		}
		inst = val & 0xffff;
	} else if (old_subc != subc && extr(state->debug_b, 20, 1)) {
		bool reload = false;
		if (state->chipset.card_type < 0x10)
			reload = extr(state->debug_b, 15, 1);
		else
			reload = extr(state->debug_d, 14, 1);
		if (reload) {
			insrt(grobj[0], 0, 8, cls);
			if (state->chipset.card_type >= 0x10) {
				insrt(grobj[0], 23, 1, 0);
			}
			if (jrand48(ctx->rand48) & 3)
				grobj[3] = 0;
		} else {
			insrt(state->ctx_cache_a[subc], 0, 8, cls);
			if (nv04_pgraph_is_nv15p(&ctx->chipset)) {
				insrt(state->ctx_cache_a[subc], 23, 1, 0);
			}
			if (jrand48(ctx->rand48) & 3)
				state->ctx_cache_t[subc] = 0;
		}
		inst = state->ctx_cache_i[subc];
	} else {
		insrt(state->ctx_switch_a, 0, 8, cls);
		inst = state->ctx_switch_i;
		if (jrand48(ctx->rand48) & 3)
			state->ctx_switch_t = 0;
	}
	for (int i = 0; i < 4; i++)
		nva_wr32(ctx->cnum, 0x700000 | inst << 4 | i << 2, grobj[i]);
}

static void nv04_pgraph_mthd(struct pgraph_state *state, uint32_t grobj[4], int trapbit = -1) {
	int subc, old_subc = extr(state->ctx_user, 13, 3);
	bool ctxsw;
	if (state->chipset.card_type < 0x10) {
		subc = extr(state->fifo_mthd_st2, 12, 3);
		state->fifo_mthd_st2 &= ~1;
		ctxsw = extr(state->fifo_mthd_st2, 1, 11) == 0;
	} else {
		subc = extr(state->fifo_mthd_st2, 16, 3);
		state->fifo_mthd_st2 &= ~(1 << 26);
		ctxsw = extr(state->fifo_mthd_st2, 2, 11) == 0;
	}
	if (old_subc != subc || ctxsw) {
		if (ctxsw) {
			state->ctx_cache_i[subc] = state->fifo_data_st2[0] & 0xffff;
		}
		uint32_t ctx_mask;
		if (state->chipset.chipset < 5)
			ctx_mask = 0x0303f0ff;
		else if (state->chipset.card_type < 0x10)
			ctx_mask = 0x7f73f0ff;
		else
			ctx_mask = 0x7f33f0ff;
		bool reload = false;
		if (state->chipset.card_type < 0x10)
			reload = extr(state->debug_b, 15, 1);
		else
			reload = extr(state->debug_d, 14, 1);
		if (reload || ctxsw) {
			state->ctx_cache_a[subc] = grobj[0] & ctx_mask;
			state->ctx_cache_b[subc] = grobj[1] & 0xffff3f03;
			state->ctx_cache_c[subc] = grobj[2];
			state->ctx_cache_t[subc] = grobj[3];
		}
		bool reset = extr(state->debug_c, 28, 1);
		if (state->chipset.card_type >= 0x10)
			reset = extr(state->debug_d, 19, 1);
		insrt(state->debug_b, 0, 1, reset);
		if (reset)
			pgraph_volatile_reset(state);
		insrt(state->ctx_user, 13, 3, subc);
		if (extr(state->debug_b, 20, 1)) {
			for (int i = 0; i < 5; i++) {
				state->ctx_switch_a = state->ctx_cache_a[subc];
				state->ctx_switch_b = state->ctx_cache_b[subc];
				state->ctx_switch_c = state->ctx_cache_c[subc];
				state->ctx_switch_d = state->ctx_cache_d[subc];
				state->ctx_switch_i = state->ctx_cache_i[subc];
				state->ctx_switch_t = state->ctx_cache_t[subc];
			}
		}
	}
	if (trapbit >= 0 && extr(state->ctx_switch_t, trapbit, 1) && state->chipset.card_type >= 0x10) {
		state->intr |= 0x10;
		state->fifo_enable = 0;
	} else {
		if (extr(state->debug_d, 20, 2) == 3 && !ctxsw)
			nv04_pgraph_blowup(state, 2);
	}
	if (state->chipset.card_type < 0x10) {
		insrt(state->trap_addr, 2, 14, extr(state->fifo_mthd_st2, 1, 14));
		insrt(state->trap_addr, 24, 4, extr(state->fifo_mthd_st2, 15, 4));
	} else {
		insrt(state->trap_addr, 2, 11, extr(state->fifo_mthd_st2, 2, 11));
		insrt(state->trap_addr, 16, 3, extr(state->fifo_mthd_st2, 16, 3));
		insrt(state->trap_addr, 20, 5, extr(state->fifo_mthd_st2, 20, 5));
	}
	state->trap_data[0] = state->fifo_data_st2[0];
	state->trap_data[1] = state->fifo_data_st2[1];
}

static int test_invalid_class(struct hwtest_ctx *ctx) {
	int i;
	for (int cls = 0; cls < 0x100; cls++) {
		switch (cls) {
			case 0x10:
			case 0x11:
			case 0x13:
			case 0x15:
				if (ctx->chipset.chipset == 4)
					continue;
				break;
			case 0x67:
				if (ctx->chipset.chipset == 4 || ctx->chipset.card_type >= 0x10)
					continue;
				break;
			case 0x64:
			case 0x65:
			case 0x66:
			case 0x12:
			case 0x72:
			case 0x43:
			case 0x19:
			case 0x17:
			case 0x57:
			case 0x18:
			case 0x44:
			case 0x42:
			case 0x52:
			case 0x53:
			case 0x58:
			case 0x59:
			case 0x5a:
			case 0x5b:
			case 0x38:
			case 0x39:
			case 0x1c:
			case 0x1d:
			case 0x1e:
			case 0x1f:
			case 0x21:
			case 0x36:
			case 0x37:
			case 0x5c:
			case 0x5d:
			case 0x5e:
			case 0x5f:
			case 0x60:
			case 0x61:
			case 0x76:
			case 0x77:
			case 0x4a:
			case 0x4b:
			case 0x54:
			case 0x55:
				continue;
			case 0x48:
				if (!nv04_pgraph_is_nv15p(&ctx->chipset))
					continue;
				break;
			case 0x56:
			case 0x62:
			case 0x63:
			case 0x79:
			case 0x7b:
			case 0x82:
			case 0x85:
			case 0x87:
			case 0x88:
			case 0x89:
			case 0x8a:
			case 0x93:
			case 0x94:
			case 0x95:
				if (ctx->chipset.card_type >= 0x10)
					continue;
				break;
			case 0x96:
			case 0x9e:
			case 0x9f:
				if (nv04_pgraph_is_nv15p(&ctx->chipset))
					continue;
			case 0x98:
			case 0x99:
				if (nv04_pgraph_is_nv17p(&ctx->chipset))
					continue;
				break;
		}
		for (i = 0; i < 20; i++) {
			uint32_t val = jrand48(ctx->rand48);
			uint32_t mthd = jrand48(ctx->rand48) & 0x1ffc;
			if (mthd == 0 || mthd == 0x100 || i < 10)
				mthd = 0x104;
			uint32_t addr = (jrand48(ctx->rand48) & 0xe000) | mthd;
			struct pgraph_state orig, exp, real;
			std::mt19937 rnd(jrand48(ctx->rand48));
			pgraph_gen_state(ctx->cnum, rnd, &orig);
			orig.notify &= ~0x10000;
			uint32_t grobj[4];
			nv04_pgraph_prep_mthd(ctx, grobj, &orig, cls, addr, val);
			pgraph_load_state(ctx->cnum, &orig);
			exp = orig;
			nv04_pgraph_mthd(&exp, grobj);
			nv04_pgraph_blowup(&exp, 0x0040);
			pgraph_calc_state(&exp);
			pgraph_dump_state(ctx->cnum, &real);
			if (pgraph_cmp_state(&orig, &exp, &real, false)) {
				printf("Iter %d mthd %02x.%04x %08x\n", i, cls, addr, val);
				return HWTEST_RES_FAIL;
			}
		}
	}
	return HWTEST_RES_PASS;
}

static int nv04_pgraph_prep(struct hwtest_ctx *ctx) {
	if (ctx->chipset.card_type < 0x04 || ctx->chipset.card_type > 0x10)
		return HWTEST_RES_NA;
	if (!(nva_rd32(ctx->cnum, 0x200) & 1 << 24)) {
		printf("Mem controller not up.\n");
		return HWTEST_RES_UNPREP;
	}
	nva_wr32(ctx->cnum, 0x200, 0xffffffff);
	return HWTEST_RES_PASS;
}

HWTEST_DEF_GROUP(nv04_pgraph,
	HWTEST_TEST(test_invalid_class, 0),
)
