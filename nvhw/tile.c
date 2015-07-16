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

int has_large_tile(int chipset) {
	return pfb_type(chipset) == PFB_NV40 || pfb_type(chipset) == PFB_NV41;
}

int has_tile_factor_13(int chipset) {
	return pfb_type(chipset) >= PFB_NV20 && chipset != 0x20;
}

int tile_pitch_valid(int chipset, uint32_t pitch, int *pshift, int *pfactor) {
	if (pshift)
		*pshift = 0;
	if (pfactor)
		*pfactor = 0;
	if (pitch & ~0x1ff00)
		return 0;
	pitch >>= 8;
	if (!pitch)
		return 0;
	if (pitch == 1)
		return 0;
	if (pitch & 1 && (pfb_type(chipset) == PFB_NV10 || pfb_type(chipset) == PFB_NV44))
		return 0;
	if (pitch > 0x100)
		return 0;
	int shift = 0;
	while (!(pitch & 1))
		pitch >>= 1, shift++;
	if (shift >= 8 && !has_large_tile(chipset))
		return 0;
	int factor;
	switch (pitch) {
		case 1:
			factor = 0;
			break;
		case 3:
			factor = 1;
			break;
		case 5:
			factor = 2;
			break;
		case 7:
			factor = 3;
			break;
		case 13:
			if (!has_tile_factor_13(chipset))
				return 0;
			factor = 4;
			break;
		default:
			return 0;
	}
	if (pshift)
		*pshift = shift;
	if (pfactor)
		*pfactor = factor;
	return 1;
}

int tile_bankoff_bits(int chipset) {
	if (pfb_type(chipset) < PFB_NV20)
		return 0;
	if (pfb_type(chipset) == PFB_NV44)
		return chipset != 0x44 && chipset != 0x4a;
	if (chipset < 0x30)
		return 1;
	return 2;
}

int num_tile_regions(int chipset) {
	if (pfb_type(chipset) < PFB_NV10 || pfb_type(chipset) > PFB_NV44)
		return 0;
	if (pfb_type(chipset) < PFB_NV41)
		return 8;
	return is_g7x(chipset) ? 15 : 12;
}

uint32_t tile_translate_addr(int chipset, uint32_t pitch, uint32_t address, int mode, int bankoff, const struct mc_config *mcc, int *ppart, int *ptag) {
	int bankshift = mcc->mcbits + mcc->partbits + mcc->colbits_lo;
	int is_vram = mode == 1 || mode == 4;
	if (is_igp(chipset))
		is_vram = 0;
	if (!is_vram)
		bankshift = 12;
	int shift, factor;
	if (!tile_pitch_valid(chipset, pitch, &shift, &factor))
		abort();
	uint32_t x = address % pitch;
	uint32_t y = address / pitch;
	uint32_t ix = x & 0xff;
	uint32_t iy = y & ((1 << (bankshift - 8)) - 1);
	x >>= 8;
	y >>= bankshift - 8;
	uint32_t iaddr = 0;
	uint32_t baddr = y * (pitch >> 8) + x;
	int part = 0, tag = 0;
	switch (pfb_type(chipset)) {
		case PFB_NV10:
		{
			iy = address >> (shift + 8) & ((1 << (bankshift - 8)) - 1);
			iaddr = ix | iy << 8;
			if (y & 1)
				baddr ^= 1;
			if (chipset > 0x10 && mcc->mcbits + mcc->partbits + mcc->burstbits > 4 && address & 0x100)
				iaddr ^= 0x10;
			if (ppart || ptag)
				abort();
		} break;
		case PFB_NV20:
		{
			uint32_t x1 = ix & 0xf;
			ix >>= 4;
			part = ix & ((1 << mcc->partbits) - 1);
			ix >>= mcc->partbits;
			uint32_t x2 = ix;
			uint32_t y1 = iy & 3;
			iy >>= 2;
			uint32_t y2 = iy;
			if (y2 & 1 && mcc->partbits >= 1)
				part += 1 << (mcc->partbits - 1);
			if (y2 & 2 && mcc->partbits >= 2)
				part += 1 << (mcc->partbits - 2);
			part &= (1 << mcc->partbits) - 1;
			if (chipset >= 0x30) {
				int bank = baddr & 3;
				if (shift >= 2)
					bank ^= y << 1 & 2;
				if (shift >= 1)
					bank += y >> 1 & 1;
				bank ^= bankoff;
				bank &= 3;
				baddr = (baddr & ~3) | bank;
			} else {
				baddr ^= bankoff;
				if (y & 1 && shift)
					baddr ^= 1;
			}
			iaddr = y2;
			iaddr <<= 4 - mcc->partbits, iaddr |= x2;
			iaddr <<= 2, iaddr |= y1;
			iaddr <<= mcc->partbits, iaddr |= part;
			iaddr <<= 4, iaddr |= x1;
			tag = x + y * (pitch >> 8);
			tag <<= bankshift - 10, tag |= iy;
			tag <<= (4 - mcc->partbits), tag |= x2;
		} break;
		case PFB_NV40:
		case PFB_NV41:
		{
			iaddr = iy >> (2 + mcc->partbits);
			iaddr <<= 2, iaddr |= ix >> 6 & 3;
			if (mcc->partbits == 0) {
				part = 0;
			} else if (mcc->partbits == 1) {
				part = iy >> 2;
				part ^= ix >> 7;
			} else if (pfb_type(chipset) < PFB_NV41) {
				part = (iy >> 3 & 1) | (iy >> 2 & 1) << 1;
				part += x << 1 | ix >> 7;
			} else {
				part = -(iy >> 2) ^ (x << 1 | ix >> 7);
			}
			if (shift == 0)
				part += y << 1;
			part &= (1 << mcc->partbits) - 1;
			if (mcc->partshift == 8) {
				iaddr <<= mcc->partbits, iaddr |= part;
				iaddr <<= 1, iaddr |= ix >> 5 & 1;
			} else {
				iaddr <<= 1, iaddr |= ix >> 5 & 1;
				iaddr <<= mcc->partbits, iaddr |= part;
			}
			iaddr <<= 2, iaddr |= iy & 3;
			iaddr <<= 1, iaddr |= (ix >> 4 ^ ix >> 5 ^ iy) & 1;
			iaddr <<= 4, iaddr |= ix & 0xf;
			int bank = baddr & 3;
			if (shift >= 2) {
				bank = (x >> 1 & 1) | (x << 1 & 2);
				bank += y;
			} else if (shift == 1) {
				bank = (x >> 1 & 1) | (x << 1 & 2);
				bank += y;
			}
			bank += bankoff;
			baddr = (baddr & ~3) | (bank & 3);
			if (mcc->partbits == 2) {
				/* undo linear partition transformation */
				uint32_t addr = iaddr | baddr << bankshift;
				int px0 = addr >> 11 ^ addr >> 12;
				int px1 = addr >> 10 ^ addr >> 11 ^ addr >> 13 ^ addr >> 14;
				int px = (px0 & 1) | (px1 & 1) << 1;
				iaddr ^= px << mcc->partshift;
			}
			if (mcc->partbits == 1 && chipset >= 0x43) {
				/* undo linear partition transformation */
				uint32_t addr = iaddr | baddr << bankshift;
				int px = addr >> 9 ^ addr >> 10 ^ addr >> 11 ^ addr >> 12 ^ addr >> 13;
				if (mcc->partshift == 7)
					px ^= addr >> 8;
				iaddr ^= (px & 1) << mcc->partshift;
			}
			tag = x + y * (pitch >> 8);
			tag <<= bankshift - mcc->partbits - 10, tag |= (iy >> (mcc->partbits + 2));
			tag <<= 3, tag |= ix >> 5;
		} break;
		case PFB_NV44:
		{
			iaddr = ix | iy << 8;
			if (chipset != 0x4e)
				baddr ^= y&1;
			if (is_igp(chipset))
				baddr ^= bankoff;
			else
				baddr ^= bankoff << 1;
			if (mcc->mcbits + mcc->partbits + mcc->burstbits > 4 && is_vram && iaddr & 0x100)
				iaddr ^= 0x10;
			if (ppart || ptag)
				abort();
		} break;
		default:
			abort();
	}
	if (ptag)
		*ptag = tag;
	if (ppart)
		*ppart = part;
	return iaddr | baddr << bankshift;
}

uint32_t tile_mmio_region(int chipset) {
	switch (pfb_type(chipset)) {
		case PFB_NV10:
		case PFB_NV20:
		case PFB_NV40:
			return 0x100240;
		case PFB_NV41:
		case PFB_NV44:
			return 0x100600;
		default:
			return 0;
	}
}
