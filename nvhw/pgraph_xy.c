/*
 * Copyright (C) 2012-2016 Marcin Kościelnicki <koriakin@0x04.net>
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

#include "nvhw/pgraph.h"
#include "util.h"
#include <stdlib.h>

void nv01_pgraph_clip_bounds(struct nv01_pgraph_state *state, int32_t min[2], int32_t max[2]) {
	int i;
	for (i = 0; i < 2; i++) {
		min[i] = extrs(state->canvas_min, i*16, 16);
		max[i] = extr(state->canvas_max, i*16, 12);
		int sel = extr(state->xy_misc_1, 12+i*4, 3);
		bool uce = extr(state->ctx_switch, 7, 1);
		if (sel & 1 && uce)
			min[i] = state->uclip_min[i];
		if (sel & 2 && uce)
			max[i] = state->uclip_max[i];
		if (sel & 4)
			max[i] = state->iclip[i];
		if (min[i] < 0)
			min[i] = 0;
		min[i] &= 0xfff;
		max[i] &= 0xfff;
	}
}

int nv01_pgraph_clip_status(struct nv01_pgraph_state *state, int32_t coord, int xy, int is_tex_class) {
	int32_t clip_min[2], clip_max[2];
	nv01_pgraph_clip_bounds(state, clip_min, clip_max);
	int cstat = 0;
	if (is_tex_class) {
		coord = extrs(coord, 15, 16);
		if (extr(state->xy_misc_1, 25, 1))
			coord >>= 4;
	} else {
		coord = extrs(coord, 0, 18);
	}
	if (coord < clip_min[xy])
		cstat |= 1;
	if (coord == clip_min[xy])
		cstat |= 2;
	if (coord > clip_max[xy])
		cstat |= 4;
	if (coord == clip_max[xy])
		cstat |= 8;
	return cstat;
}

int nv01_pgraph_use_v16(struct nv01_pgraph_state *state) {
	uint32_t class = extr(state->access, 12, 5);
	int d0_24 = extr(state->debug[0], 24, 1);
	int d1_8 = extr(state->debug[1], 8, 1);
	int d1_24 = extr(state->debug[1], 24, 1);
	int j;
	int sdl = 1;
	for (j = 0; j < 4; j++)
		if (extr(state->subdivide, (j/2)*4, 4) > extr(state->subdivide, 16+j*4, 4))
			sdl = 0;
	int biopt = d1_8 && !extr(state->xy_misc_2[0], 28, 2) && !extr(state->xy_misc_2[1], 28, 2);
	switch (class) {
		case 0xd:
		case 0x1d:
			return d1_24 && (!d0_24 || sdl) && !biopt;
		case 0xe:
		case 0x1e:
			return d1_24 && (!d0_24 || sdl);
		default:
			return 0;
	}
}

void nv01_pgraph_set_xym2(struct nv01_pgraph_state *state, int xy, int idx, int sid, bool carry, bool oob, int cstat) {
	uint32_t class = extr(state->access, 12, 5);
	insrt(state->xy_misc_2[xy], sid, 1, carry);
	insrt(state->xy_misc_2[xy], sid+4, 1, oob);
	if (class == 8) {
		insrt(state->xy_misc_2[xy], 8, 4, cstat);
		insrt(state->xy_misc_2[xy], 12, 4, cstat);
	} else if (idx < 16 || nv01_pgraph_use_v16(state)) {
		insrt(state->xy_misc_2[xy], 8 + sid * 4, 4, cstat);
	}
}

void nv01_pgraph_vtx_fixup(struct nv01_pgraph_state *state, int xy, int idx, int32_t coord, int rel, int ridx, int sid) {
	uint32_t class = extr(state->access, 12, 5);
	bool is_tex_class = nv01_pgraph_is_tex_class(class);
	int32_t cbase = extrs(state->canvas_min, 16 * xy, 16);
	if (ridx != -1)
		cbase = (xy ? state->vtx_y : state->vtx_x)[ridx];
	if (is_tex_class && state->xy_misc_1 & 1 << 25 && ridx == -1)
		cbase <<= 4;
	if (rel)
		coord += cbase;
	if (xy == 0)
		state->vtx_x[idx] = coord;
	else
		state->vtx_y[idx] = coord;
	int oob = !is_tex_class && (coord >= 0x8000 || coord < -0x8000);
	int cstat = nv01_pgraph_clip_status(state, coord, xy, is_tex_class);
	int carry = rel && (uint32_t)coord < (uint32_t)cbase;
	nv01_pgraph_set_xym2(state, xy, idx, sid, carry, oob, cstat);
	if (idx >= 16) {
		insrt(state->edgefill, 16 + xy * 4 + (idx - 16) * 8, 4, cstat);
	}
}

void nv01_pgraph_iclip_fixup(struct nv01_pgraph_state *state, int xy, int32_t coord, int rel) {
	uint32_t class = extr(state->access, 12, 5);
	int is_tex_class = nv01_pgraph_is_tex_class(class);
	int max = extr(state->canvas_max, xy * 16, 12);
	if (state->xy_misc_1 & 0x2000 << (xy * 4) && state->ctx_switch & 0x80)
		max = state->uclip_max[xy] & 0xfff;
	int32_t cbase = extrs(state->canvas_min, 16 * xy, 16);
	int32_t cmin = cbase;
	if (cmin < 0)
		cmin = 0;
	if (state->ctx_switch & 0x80 && state->xy_misc_1 & 0x1000 << xy * 4)
		cmin = state->uclip_min[xy];
	cmin &= 0xfff;
	if (is_tex_class && state->xy_misc_1 & 1 << 25)
		cbase <<= 4;
	if (rel)
		coord += cbase;
	state->iclip[xy] = coord & 0x3ffff;
	if (is_tex_class) {
		coord = extrs(coord, 15, 16);
		if (state->xy_misc_1 & 0x02000000)
			coord >>= 4;
	} else {
		coord = extrs(coord, 0, 18);
	}
	state->xy_misc_1 &= ~0x144000;
	insrt(state->xy_misc_1, 14+xy*4, 1, coord <= max);
	if (!xy) {
		insrt(state->xy_misc_1, 20, 1, coord <= max);
		insrt(state->xy_misc_1, 4, 1, coord < cmin);
	} else {
		insrt(state->xy_misc_1, 5, 1, coord < cmin);
	}
}

void nv01_pgraph_uclip_fixup(struct nv01_pgraph_state *state, int xy, int idx, int32_t coord, int rel) {
	uint32_t class = extr(state->access, 12, 5);
	int is_tex_class = nv01_pgraph_is_tex_class(class);
	int32_t cbase = extrs(state->canvas_min, 16 * xy, 16);
	if (rel)
		coord += cbase;
	state->uclip_min[xy] = state->uclip_max[xy];
	state->uclip_max[xy] = coord & 0x3ffff;
	if (!xy)
		state->vtx_x[15] = coord;
	else
		state->vtx_y[15] = coord;
	state->xy_misc_1 &= ~0x0177000;
	if (!idx) {
		insrt(state->xy_misc_1, 24, 1, 0);
	} else {
		int32_t cmin = extr(state->canvas_min, 16 * xy, 12);
		if (extr(state->canvas_min, 16 * xy + 15, 1))
			cmin = 0;
		int32_t cmax = extr(state->canvas_max, 16 * xy, 12);
		int32_t ucmin = extrs(state->uclip_min[xy], 0, 18);
		int32_t ucmax = extrs(state->uclip_max[xy], 0, 18);
		if (is_tex_class && extr(state->xy_misc_1, 25, 1))
			ucmin >>= 4, ucmax >>= 4;
		insrt(state->xy_misc_1, 4 + xy, 1, 0);
		insrt(state->xy_misc_1, 8 + xy, 1,
			ucmax < cmin || (extr(state->xy_misc_2[xy], 10, 1) && ucmax >= 0));
		insrt(state->xy_misc_1, 12 + xy * 4, 1, !extr(state->xy_misc_2[xy], 8, 1));
		insrt(state->xy_misc_1, 13 + xy * 4, 1, ucmax <= cmax);
	}
}

void nv01_pgraph_set_clip(struct nv01_pgraph_state *state, int is_size, uint32_t val) {
	uint32_t class = extr(state->access, 12, 5);
	int is_tex_class = nv01_pgraph_is_tex_class(class);
	int is_tex = is_tex_class && !is_size;
	int xy;
	if (is_size) {
		int n = state->valid >> 24 & 1;
		state->valid &= ~0x11000000;
		if (!n)
			state->valid |= 1 << 28;
		state->xy_misc_1 &= 0x03000001;
	} else {
		if (is_tex_class && (state->xy_misc_1 >> 24 & 3) == 3) {
			state->valid = 0;
			state->xy_misc_1 &= ~0x02000000;
		}
		state->valid &= ~0x11000000;
		state->valid |= 1 << 24;
		state->xy_misc_1 &= 0x00000330;
		state->xy_misc_1 |= 0x01000000;
	}
	for (xy = 0; xy < 2; xy++) {
		int32_t new = (uint16_t)(val >> xy * 16);
		int32_t cbase = (int16_t)(state->canvas_min >> 16 * xy);
		int svidx = is_tex ? 4 : 15;
		int dvidx = svidx;
		int mvidx = is_size;
		if ((class == 0xc || class == 0x11 || class == 0x12 || class == 0x13) && is_size) {
			svidx = 0;
			dvidx = extr(state->xy_misc_0, 28, 4);
			mvidx = dvidx%4;
		}
		if (class == 0x14 && is_size) {
			svidx = 0;
			dvidx = 2;
			mvidx = dvidx%4;
			state->vtx_x[3] = val & 0xffff;
			state->vtx_y[3] = val >> 16 & 0xffff;
			state->xy_misc_0 &= ~0x1000;
		}
		if (is_tex_class && state->xy_misc_1 & 1 << 25)
			cbase <<= 4;
		if (is_size) {
			new += xy ? state->vtx_y[svidx] : state->vtx_x[svidx];
		} else {
			new = (int16_t)new;
			new += cbase;
		}
		uint32_t m2 = state->xy_misc_2[xy];
		int32_t vcoord = new;
		if (is_tex)
			vcoord <<= 15, vcoord |= 1 << 14;
		if (!xy)
			state->vtx_x[dvidx] = vcoord;
		else
			state->vtx_y[dvidx] = vcoord;
		state->uclip_min[xy] = state->uclip_max[xy];
		state->uclip_max[xy] = new & 0x3ffff;
		int cstat = nv01_pgraph_clip_status(state, new, xy, is_tex_class && is_size);
		int oob = (new >= 0x8000 || new < -0x8000);
		if (is_tex_class) {
			if (is_size) {
				oob = 0;
			} else {
				oob |= m2 >> (mvidx + 4) & 1;
			}
		}
		int carry = !is_size && ((uint32_t)new < (uint32_t)cbase);
		nv01_pgraph_set_xym2(state, xy, 0, mvidx, carry, oob, cstat);
		if (is_size) {
			int32_t cmin = state->canvas_min >> 16 * xy & 0x8fff;
			if (cmin & 0x8000)
				cmin = 0;
			int32_t cmax = state->canvas_max >> 16 * xy & 0x0fff;
			int32_t ucmax = sext(state->uclip_max[xy], 17);
			if (is_tex_class)
				ucmax = extrs(vcoord, 15, 16);
			if (is_tex_class && state->xy_misc_1 & 1 << 25)
				ucmax >>= 4;
			state->xy_misc_1 |= (ucmax < cmin) << (8 + xy);
			if (m2 >> 10 & 1)
				state->xy_misc_1 |= (ucmax >= 0) << (8 + xy);
			if (!(m2 >> 8 & 1))
				state->xy_misc_1 |= 1 << (12 + xy * 4);
			state->xy_misc_1 |= (ucmax <= cmax) << (13 + xy * 4);
		}
	}
}

void nv01_pgraph_set_vtx(struct nv01_pgraph_state *state, int xy, int idx, int32_t coord, bool is32) {
	uint32_t class = extr(state->access, 12, 5);
	int is_tex_class = nv01_pgraph_is_tex_class(class);
	int sid = idx & 3;
	int32_t cbase = extrs(state->canvas_min, 16 * xy, 16);
	int fract = is_tex_class && extr(state->xy_misc_1, 25, 1);
	if (fract)
		cbase <<= 4;
	coord += cbase;
	int carry = (uint32_t)coord < (uint32_t)cbase;
	int oob = (coord >= 0x8000 || coord < -0x8000);
	if (is_tex_class)
		oob |= extr(state->xy_misc_2[xy], sid+4, 1);
	uint32_t val = (is_tex_class && !is32) ? coord << 15 | 0x4000 : coord;
	if (xy == 0)
		state->vtx_x[idx] = val;
	else
		state->vtx_y[idx] = val;
	int cstat = nv01_pgraph_clip_status(state, is32 ? coord : extrs(coord, fract * 4, 18 - fract * 4), xy, is_tex_class && is32);
	nv01_pgraph_set_xym2(state, xy, idx, sid, carry, oob, cstat);
	if (idx >= 16) {
		insrt(state->edgefill, 16 + xy * 4 + (idx - 16) * 8, 4, cstat);
	}
}

void nv01_pgraph_bump_vtxid(struct nv01_pgraph_state *state) {
	uint32_t class = extr(state->access, 12, 5);
	if (class < 8 || class == 0x0f || (class > 0x14 && class < 0x1d) || class == 0x1f)
		return;
	int vtxid = extr(state->xy_misc_0, 28, 4);
	vtxid++;
	if (class == 0x0b) {
		if (vtxid == 3)
			vtxid = 0;
	} else if (class == 0x10 || class == 0x14) {
		if (vtxid == 4)
			vtxid = 0;
	} else if (nv01_pgraph_is_tex_class(class)) {
		if (vtxid == 3 + nv01_pgraph_use_v16(state))
			vtxid = 0;
	} else {
		vtxid &= 1;
	}
	insrt(state->xy_misc_0, 28, 4, vtxid);
}

void nv01_pgraph_prep_draw(struct nv01_pgraph_state *state, bool poly) {
	uint32_t class = extr(state->access, 12, 5);
	if (!nv01_pgraph_is_drawable_class(class))
		return;
	if (class == 0x08) {
		if ((state->valid & 0x1001) != 0x1001)
			state->intr |= 1 << 16;
		state->valid &= ~0xffffff;
	} else if (class == 0x0c) {
		if ((state->valid & 0x3103) != 0x3103)
			state->intr |= 1 << 16;
		state->valid &= ~0xffffff;
	} else if (class == 0x10) {
		if ((state->valid & 0xf10f) != 0xf10f)
			state->intr |= 1 << 16;
		state->valid &= ~0xffffff;
	} else if (class >= 0x09 && class <= 0x0a) {
		if (!poly) {
			if ((state->valid & 0x3103) != 0x3103)
				state->intr |= 1 << 16;
			state->valid &= ~0x00f00f;
		} else {
			if ((state->valid & 0x3f13f) != 0x30130)
				state->intr |= 1 << 16;
			state->valid &= ~(0x10010 << (extr(state->xy_misc_0, 28, 2)));
			if (state->valid & 0xf00f)
				state->valid &= ~0x100;
		}
	} else if (class == 0x0b) {
		if (!poly) {
			if ((state->valid & 0x7107) != 0x7107)
				state->intr |= 1 << 16;
			state->valid &= ~0x00f00f;
		} else {
			if ((state->valid & 0x7f17f) != 0x70170)
				state->intr |= 1 << 16;
			state->valid &= ~(0x10010 << (extr(state->xy_misc_0, 28, 2)));
			if (state->valid & 0xf00f)
				state->valid &= ~0x100;
		}
	} else if (class == 0x11 || class == 0x12) {
		if ((state->valid & 0x38038) != 0x38038)
			state->intr |= 1 << 16;
		/* XXX: this steps the IFC machine */
	} else if (class == 0x13) {
		/* XXX: this steps the IFM machine */
	}
	if (state->xy_misc_2[0] & 0xf0)
		state->intr |= 1 << 12;
	if (state->xy_misc_2[1] & 0xf0)
		state->intr |= 1 << 12;
	if (state->valid & 0x11000000 && state->ctx_switch & 0x80)
		state->intr |= 1 << 16;
	if (extr(state->canvas_config, 24, 1))
		state->intr |= 1 << 20;
	if (extr(state->cliprect_ctrl, 8, 1))
		state->intr |= 1 << 24;
	if (state->intr)
		state->access &= ~0x101;
}
