/*
 * Copyright (C) 2012 Marcin Kościelnicki <koriakin@0x04.net>
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

#include "nvhw/chipset.h"
#include "nvhw/vram.h"
#include <stdlib.h>
#include <string.h>

int comp_type(int chipset) {
	if (pfb_type(chipset) < PFB_NV20 || pfb_type(chipset) > PFB_NV41)
		return COMP_NONE;
	if (chipset == 0x20)
		return COMP_NV20;
	if (chipset < 0x30)
		return COMP_NV25;
	if (chipset < 0x35)
		return COMP_NV30;
	if (chipset < 0x36)
		return COMP_NV35;
	if (chipset < 0x40)
		return COMP_NV36;
	return COMP_NV40;
}

uint32_t tile_mmio_comp(int chipset) {
	switch (pfb_type(chipset)) {
		case PFB_NV20:
		case PFB_NV40:
			return 0x100300;
		case PFB_NV41:
			return 0x100700;
		default:
			return 0;
	}
}

int comp_format_type(int chipset, int format) {
	if (comp_type(chipset) == COMP_NV20) {
		return format & 1 ? COMP_FORMAT_Z24S8_GRAD : COMP_FORMAT_Z16_GRAD;
	} else if (comp_type(chipset) < COMP_NV36) {
		switch (format & 0xf) {
			case 1:
			case 3:
			case 5:
				return COMP_FORMAT_Z16_GRAD;
			case 2:
			case 4:
			case 6:
				return COMP_FORMAT_Z24S8_GRAD;
			case 7:
				if (comp_type(chipset) >= COMP_NV30)
					return COMP_FORMAT_A8R8G8B8_GRAD;
				return COMP_FORMAT_OFF;
			case 8:
				if (comp_type(chipset) >= COMP_NV30)
					return COMP_FORMAT_FLAT;
				return COMP_FORMAT_OFF;
			default:
				return COMP_FORMAT_OFF;

		}
	} else if (comp_type(chipset) == COMP_NV36) {
		switch (format & 7) {
			case 1:
			case 3:
				return COMP_FORMAT_Z16_GRAD;
			case 2:
			case 4:
				return COMP_FORMAT_Z24S8_GRAD;
			case 5:
				return COMP_FORMAT_A8R8G8B8_GRAD;
			case 6:
				return COMP_FORMAT_FLAT;
			default:
				return COMP_FORMAT_OFF;
		}
	} else {
		switch (format & 0xf) {
			case 7:
				return COMP_FORMAT_A8R8G8B8_INTERP;
			case 8:
				return COMP_FORMAT_FLAT;
			case 9:
				return COMP_FORMAT_Z24S8_SPLIT;
			case 10:
			case 11:
			case 12:
				return COMP_FORMAT_Z24S8_SPLIT_GRAD;
			default:
				return COMP_FORMAT_OFF;
		}
	}
}

int comp_format_endian(int chipset, int format) {
	if (comp_type(chipset) == COMP_NV20)
		return format >> 1 & 1;
	else if (comp_type(chipset) == COMP_NV36)
		return format >> 3 & 1;
	else
		return format >> 4 & 1;
}

int comp_format_ms(int chipset, int format) {
	if (comp_type(chipset) == COMP_NV20) {
		return 0;
	} else if (comp_type(chipset) < COMP_NV36) {
		switch (format & 0xf) {
			case 1:
			case 2:
				return 0;
			case 3:
			case 4:
				return 1;
			case 5:
			case 6:
				return 2;
			case 7:
				return 1;
			case 8:
				return 2;
			default:
				abort();
		}
	} else if (comp_type(chipset) == COMP_NV36) {
		switch (format & 7) {
			case 1:
			case 2:
				return 0;
			case 3:
			case 4:
				return 1;
			case 5:
				return 1;
			case 6:
				return 2;
			default:
				abort();
		}
	} else {
		switch (format & 0xf) {
			case 7:
				return 1;
			case 8:
				return 2;
			case 10:
				return 0;
			case 11:
				return 1;
			case 12:
				return 2;
			default:
				abort();
		}
	}
}

int comp_format_bpp(int chipset, int format) {
	if (comp_type(chipset) == COMP_NV20) {
		return format&1?32:16;
	} else if (comp_type(chipset) < COMP_NV36) {
		switch (format & 0xf) {
			case 1:
			case 3:
			case 5:
				return 16;
			case 2:
			case 4:
			case 6:
			case 7:
			case 8:
				return 32;
			default:
				abort();
		}
	} else if (comp_type(chipset) == COMP_NV36) {
		switch (format & 7) {
			case 1:
			case 3:
				return 16;
			case 2:
			case 4:
			case 5:
			case 6:
				return 32;
			default:
				abort();
		}
	} else {
		switch (format & 0xf) {
			case 7:
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
				return 32;
			default:
				abort();
		}
	}
}

static void comp_z16_grad_decompress(int chipset, int ms, uint64_t w0, uint64_t w1, int is_const, uint16_t od16[4][16]) {
	uint32_t rdelta[29];
	uint32_t base = w0 & 0xffff;
	uint32_t dx = w0 >> 16 & 0xfff;
	uint32_t dy = w0 >> 28 & 0xfff;
	int bpos = 40;
	int wpos = 0;
	int i, x, y;
	int x0, y0 = 0;
	if (comp_type(chipset) == COMP_NV20)
		x0 = 2;
	else
		x0 = 3;
	for (i = 0; i < 29; i++) {
		uint64_t w = wpos?w1:w0;
		if (!wpos && bpos+3 > 63) {
			w = w0 >> bpos;
			w &= (1 << (63-bpos)) - 1;
			w |= w1 << (63-bpos);
			w &= 7;
			wpos = 1;
			bpos -= 60;
		} else {
			w >>= bpos;
			bpos += 3;
			w &= 7;
		}
		if (w & 4)
			w |= 0xfff8;
		rdelta[i] = w;
	}
	if (dx & 0x800)
		dx |= 0xfffff000;
	if (dy & 0x800)
		dy |= 0xfffff000;
	int di = 0;
	int half_dx = comp_type(chipset) >= COMP_NV30;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 8; x++) {
			int need_delta = 1;
			uint16_t delta = 0;
			if (x == x0 && (y == y0 || y == y0 + 2))
				need_delta = 0;
			if (y == y0 && x == x0 + (half_dx ? 4 : 2))
				need_delta = 0;
			if (need_delta)
				delta = rdelta[di++];

			int dz = 6; /* for proper rounding */
			int rx = 2 * (x - x0);
			int ry = 2 * (y - y0);
			if (ms == 1)
				ry -= (x - x0) & 1;
			if (comp_type(chipset) < COMP_NV30)
				dz -= ry >= 0;
			dz += dx * rx * (2 - half_dx);
			dz += dy * ry * 2;
			dz >>= 3;

			if (is_const)
				od16[y][x] = base;
			else
				od16[y][x] = base + dz + delta;
		}
	}
}

static void comp_z24s8_grad_decompress(int chipset, int ms, uint64_t w0, uint64_t w1, int is_const, uint32_t od32[4][8]) {
	uint32_t rdelta[13];
	uint8_t stencil = w0 & 0xff;
	uint32_t base = w0 >> 8 & 0xffffff;
	uint32_t dx = w0 >> 32 & 0x7fff;
	uint32_t dy = w0 >> 47 & 0x7fff;
	if (dx & 0x4000)
		dx |= 0xffff8000;
	if (dy & 0x4000)
		dy |= 0xffff8000;
	int wpos = 0;
	int bpos = 62;
	int x0 = 1, y0 = 1;
	int i, x, y;
	for (i = 0; i < 13; i++) {
		uint64_t w = wpos?w1:w0;
		if (!wpos && bpos+5 > 63) {
			w = w0 >> bpos;
			w &= (1 << (63-bpos)) - 1;
			w |= w1 << (63-bpos);
			wpos = 1;
			bpos -= 58;
		} else {
			w >>= bpos;
			bpos += 5;
		}
		w &= 0x1f;
		if (w & 0x10)
			w |= 0xffffffe0;
		rdelta[i] = w;
	}
	int di = 0;
	int half_dx = ms == 1;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			int need_delta = 1;
			if (x == x0 && y == y0)
				need_delta = 0;
			if (x == x0 && y == y0 + 1)
				need_delta = 0;
			if (x == x0 + 1 + half_dx && y == y0)
				need_delta = 0;
			uint32_t delta = 0;
			if (need_delta)
				delta = rdelta[di++];

			int dz = 1;
			int rx = x - x0;
			int ry = 2 * (y - y0);
			if (ms == 1)
				ry -= rx & 1;
			else
				rx <<= 1;
			dz += dx * rx;
			dz += dy * ry;
			dz >>= 1;

			if (is_const)
				od32[y][x] = base << 8 | stencil;
			else
				od32[y][x] = (base + dz + delta) << 8 | stencil;
		}
	}
}

static uint32_t sext(uint32_t a, int b) {
	uint32_t res = a & ((1 << b) - 1);
	if (res & 1 << (b-1))
		res |= -(1 << b);
	return res;
}

static void comp_a8r8g8b8_grad_decompress(int chipset, uint64_t w0, uint64_t w1, int is_const, uint32_t od32[4][8]) {
	uint8_t base[4];
	int8_t dx[3], dy[3];
	int8_t delta[5][3];
	int i;
	for (i = 0; i < 4; i++)
		base[i] = w0 >> 8 * i;
	int amode = w0 >> 62 & 1;
	if (amode == 0 || is_const) {
		dx[0] = sext(w0 >> 32, 5);
		dx[1] = sext(w0 >> 37, 5);
		dx[2] = sext(w0 >> 42, 6);
		dy[0] = sext(w0 >> 48, 6);
		dy[1] = sext(w0 >> 54, 6);
		dy[2] = sext(w0 >> 60 | w1 << 2, 6);;
		for (i = 0; i < 15; i++)
			delta[i/3][i%3] = sext(w1 >> (4 + i * 4), 4);
	} else {
		base[3] = sext(w0 >> 24, 1);
		dx[0] = sext(w0 >> 25, 4);
		dx[1] = sext(w0 >> 29, 4);
		dx[2] = sext(w0 >> 33, 4);
		dy[0] = sext(w0 >> 37, 4);
		dy[1] = sext(w0 >> 41, 5);
		dy[2] = sext(w0 >> 46, 5);
		delta[0][0] = sext(w0 >> 51, 5);
		delta[0][1] = sext(w0 >> 56, 5);
		delta[0][2] = sext((w0 >> 61 & 1) | w1 << 1, 5);
		for (i = 0; i < 12; i++)
			delta[1+i/3][i%3] = sext(w1 >> (4 + i * 5), 5);
	}
	int x, y;
	int x0 = 0;
	int y0 = 1;
	int twidth = comp_type(chipset) >= COMP_NV40 ? 8 : 4;
	int theight = 4;
	int di = 0;
	for (y = 0; y < theight; y++) {
		for (x = 0; x < twidth/2; x++) {
			int need_delta = 1;
			if (y == y0)
				need_delta = 0;
			if (x == x0 && y == y0 + 2)
				need_delta = 0;
			uint8_t comp[4];
			uint32_t res = 0;
			for (i = 0; i < 4; i++)
				comp[i] = base[i];
			if (!is_const) {
				int rx = x - x0;
				int ry = y - y0;
				for (i = 0; i < 3; i++) {
					comp[i] += rx * dx[i];
					comp[i] += (ry * dy[i] + 1) >> 1;
					if (need_delta)
						comp[i] += delta[di][i];
				}
			}
			if (need_delta)
				di++;
			for (i = 0; i < 4; i++)
				res |= comp[i] << 8*i;
			od32[y][2*x] = res;
			od32[y][2*x+1] = res;
		}
	}
}

static int rbits(char *bit, int *ppos, int n) {
	int i;
	int res = 0;
	for (i = 0; i < n; i++)
		res |= bit[(*ppos)++] << i;
	if (res & 1 << (n-1))
		res |= -1 << n;
	return res;
}

static void comp_a8r8g8b8_interp_decompress(int chipset, uint32_t cd[8], uint32_t od32[4][8]) {
	/* fuck performance. */
	char bit[255];
	int mode;
	if (cd[3] & 1 << 31) {
		if (cd[3] & 1 << 30)
			mode = 2;
		else
			mode = 1;
	} else {
		mode = 0;
	}
	int i, k;
	for (i = 0; i < 128-mode-1; i++)
		bit[i] = cd[i/32] >> (i%32) & 1;
	for (i = 128; i < 256; i++)
		bit[i-mode-1] = cd[i/32] >> (i%32) & 1;
	int pos = 0;
	uint16_t raw[16][3];
	uint8_t alpha = 0;
	for (i = 0; i < (mode == 2 ? 1 : 16); i++) {
		int rbsize, gsize, is_l;
		if (i < 4)
			rbsize = gsize = 8, is_l = 0;
		else if (!mode) {
			if (i < 11)
				rbsize = 4, gsize = 5, is_l = 1;
			else
				rbsize = gsize = 4, is_l = 1;
		} else {
			if (i < 5)
				rbsize = 4, gsize = 6, is_l = 1;
			else
				rbsize = 4, gsize = 5, is_l = 1;
		}
		int b = rbits(bit, &pos, rbsize);
		int g = rbits(bit, &pos, gsize);
		int r = rbits(bit, &pos, rbsize);
		if (is_l)
			r += g, b += g;
		else
			r &= 0xff, g &= 0xff, b &= 0xff;
		raw[i][0] = b;
		raw[i][1] = g;
		raw[i][2] = r;
		if (i == 0)
			alpha = rbits(bit, &pos, mode == 1 ? 1 : 8);
	}
	static const int8_t weight[16][4][4] = {
		{
			{ 3, 4, 2, 1 },
			{ 2, 3, 1, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 2, 3 },
			{ 0, 0, 3, 4 },
			{ 0, 0, 1, 2 },
			{ 0, 0, 0, 1 },
		},
		{
			{ 1, 0, 0, 0 },
			{ 2, 1, 0, 0 },
			{ 4, 3, 0, 0 },
			{ 3, 2, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 1, 3, 2 },
			{ 1, 2, 4, 3 },
		},
		{
			{ 0, 0, 4, 2 },
			{ 0, 0, 2, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 2, 0, 0, 0 },
			{ 4, 2, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 2, 4 },
			{ 0, 0, 0, 2 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 2, 0, 0 },
			{ 2, 4, 0, 0 },
		},
		{
			{ 4, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 4 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 4, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 4, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 4, 0, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 4, 0 },
			{ 0, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 4, 0, 0, 0 },
		},
		{
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 0 },
			{ 0, 0, 0, 4 },
		},
	};
	int y, x;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 4; x++) {
			uint32_t res = alpha << 24;
			for (k = 0; k < 3; k++) {
				int pixel = 3;
				if (mode == 2) {
					pixel = raw[0][k];
				} else {
					for (i = 0; i < 16; i++)
						pixel += raw[i][k] * weight[i][y][x];
					pixel >>= 2;
				}
				res |= (pixel & 0xff) << 8 * k;
			}
			od32[y][2*x] = res;
			od32[y][2*x+1] = res;
		}
	}
}

static void comp_z24_grad_decompress(int chipset, int ms, uint32_t cd[8], uint32_t od32[4][8]) {
	int is_const = cd[3] >> 31 & 1;
	uint32_t base = cd[0] & 0xffffff;
	uint32_t dxp = sext(cd[0] >> 24 | cd[1] << 8, 17);
	uint32_t dxn = sext(cd[1] >> 9, 17);
	uint32_t dy = sext(cd[1] >> 26 | cd[2] << 6, 17);
	uint32_t delta[4][8];
	delta[0][0] = sext(cd[2] >> 11, 7);
	delta[0][1] = 0;
	delta[0][2] = sext(cd[2] >> 18, 6);
	delta[0][3] = 0;
	delta[0][4] = sext(cd[2] >> 24, 6);
	delta[0][5] = sext(cd[2] >> 30 | cd[3] << 2, 7);
	delta[0][6] = sext(cd[3] >> 5, 7);
	delta[0][7] = 0;
	delta[1][0] = sext(cd[3] >> 12, 7);
	delta[1][1] = sext(cd[3] >> 19, 6);
	delta[1][2] = sext(cd[3] >> 25, 6);
	delta[1][3] = sext(cd[4], 6);
	delta[1][4] = sext(cd[4] >> 6, 6);
	delta[1][5] = sext(cd[4] >> 12, 7);
	delta[1][6] = sext(cd[4] >> 19, 7);
	delta[1][7] = sext(cd[4] >> 26, 6);
	delta[2][0] = sext(cd[5], 7);
	delta[2][1] = sext(cd[5] >> 7, 6);
	delta[2][2] = sext(cd[5] >> 13, 6);
	delta[2][3] = 0;
	delta[2][4] = sext(cd[5] >> 19, 6);
	delta[2][5] = sext(cd[5] >> 25, 7);
	delta[2][6] = sext(cd[6], 7);
	delta[2][7] = sext(cd[6] >> 7, 6);
	delta[3][0] = sext(cd[6] >> 13, 7);
	delta[3][1] = sext(cd[6] >> 20, 6);
	delta[3][2] = sext(cd[6] >> 26, 6);
	delta[3][3] = sext(cd[7], 6);
	delta[3][4] = sext(cd[7] >> 6, 6);
	delta[3][5] = sext(cd[7] >> 12, 7);
	delta[3][6] = sext(cd[7] >> 19, 7);
	delta[3][7] = sext(cd[7] >> 26, 6);
	int y, x;
	int y0 = 0, x0 = 3;
	for (y = 0; y < 4; y++) {
		for (x = 0; x < 8; x++) {
			uint32_t val = base;
			if (!is_const) {
				int32_t dz = 7;
				int rx = 2 * (x - x0);
				int ry = 2 * (y - y0);
				if (ms == 1) {
					ry -= ((x - x0) & 1);
				} else if (ms == 2) {
					ry -= ((x - x0) & 1);
					rx -= ((y - y0) & 1);
				}

				if (rx > 0)
					dz += rx * dxp;
				else
					dz -= 2 * rx * dxn;
				dz += 2 * ry * dy;
				dz >>= 3;
				val += dz + delta[y][x];
			}
			od32[y][x] = val;
		}
	}
}

void comp_decompress(int chipset, int format, uint8_t *data, int tag) {
	int ftype = comp_format_type(chipset, format);
	uint32_t cd[8];
	uint32_t od32[4][8];
	uint16_t od16[4][16];
	int i, x, y;
	int twidth = comp_type(chipset) >= COMP_NV40 ? 0x20 : 0x10;
	int theight = 4;
	for (i = 0; i < 8; i++) {
		int dp;
		if (comp_type(chipset) < COMP_NV40) {
			dp = 4*i;
		} else {
			dp = 4 * (4 + (i & 3) + (~i >> 2 & 1) * 8);
		}
		cd[i] = data[dp] | data[dp+1] << 8 | data[dp+2] << 16 | data[dp+3] << 24;
	}
	switch (ftype) {
		case COMP_FORMAT_OFF: {
			return;
		}
		case COMP_FORMAT_FLAT: {
			if (!tag)
				return;
			for (x = 0; x < twidth/4; x++)
				for (y = 0; y < theight; y++)
					od32[y][x] = cd[(x >> 1) + (y >> 1) * (twidth >> 3)];
			break;
		}
		case COMP_FORMAT_Z16_GRAD:
		case COMP_FORMAT_Z24S8_GRAD:
		case COMP_FORMAT_A8R8G8B8_GRAD: {
			if (!tag)
				return;
			uint64_t w0 = cd[0] | (uint64_t)cd[1] << 32;
			uint64_t w1 = cd[2] | (uint64_t)cd[3] << 32;
			int is_const = 0;
			int ms = comp_format_ms(chipset, format);
			if (w0 & (uint64_t)1 << 63) {
				if (comp_type(chipset) < COMP_NV30)
					w1 = 0;
				else
					is_const = 1;
			}
			if (ftype == COMP_FORMAT_Z16_GRAD) {
				comp_z16_grad_decompress(chipset, ms, w0, w1, is_const, od16);
			} else if (ftype == COMP_FORMAT_Z24S8_GRAD) {
				comp_z24s8_grad_decompress(chipset, ms, w0, w1, is_const, od32);
			} else {
				comp_a8r8g8b8_grad_decompress(chipset, w0, w1, is_const, od32);
			}
			break;
		}
		case COMP_FORMAT_A8R8G8B8_INTERP: {
			if (!tag)
				return;
			comp_a8r8g8b8_interp_decompress(chipset, cd, od32);
			break;
		}
		case COMP_FORMAT_Z24S8_SPLIT_GRAD: {
			if (tag) {
				comp_z24_grad_decompress(chipset, comp_format_ms(chipset, format), cd, od32);
			} else {
				uint8_t rbuf[0x60];
				case COMP_FORMAT_Z24S8_SPLIT:
				for (x = 0; x < 6; x++)
				       memcpy(rbuf + x*0x10, data + "\x10\x30\x50\x40\x60\x70"[x], 0x10);
				for (x = 0; x < twidth/4; x++)
					for (y = 0; y < theight; y++) {
						int di = 3*((x&4) << 2 | y << 2 | (x&3));
						od32[y][x] = rbuf[di] | rbuf[di+1] << 8 | rbuf[di+2] << 16;
					}
			}
			for (x = 0; x < twidth/4; x++)
				for (y = 0; y < theight; y++) {
					uint8_t stencil = data[y*4+(x&3)+(x&4)*8];
					od32[y][x] <<= 8;
					od32[y][x] |= stencil;
				}
			break;
		}
		default:
			abort();
	}
	int is_be = comp_format_endian(chipset, format);
	int bpp = comp_format_bpp(chipset, format);
	for (x = 0; x < twidth; x++)
		for (y = 0; y < theight; y++)
			if (bpp == 16)
				data[x+y*twidth] = od16[y][x>>1] >> ((x & 1) ^ is_be) * 8;
			else if (bpp == 32)
				data[x+y*twidth] = od32[y][x>>2] >> ((x & 3) ^ (is_be | is_be << 1)) * 8;
			else
				abort();
}
