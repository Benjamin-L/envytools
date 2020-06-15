/*
 * Copyright (C) 2016 Marcelina Kościelnicka <mwk@0x04.net>
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

#include "hwtest.h"
#include "g80_gr.h"
#include "nva.h"
#include "util.h"
#include "nvhw/fp.h"

static int g80_int_prep(struct hwtest_ctx *ctx) {
	if (ctx->chipset.card_type != 0x50)
		return HWTEST_RES_NA;
	if (!(nva_rd32(ctx->cnum, 0x200) & 1 << 20)) {
		printf("Mem controller not up.\n");
		return HWTEST_RES_UNPREP;
	}
	if (g80_gr_prep(ctx))
		return HWTEST_RES_FAIL;
	if (g80_gr_compute_prep(ctx))
		return HWTEST_RES_FAIL;
	return HWTEST_RES_PASS;
}

static int fp_prep_code(struct hwtest_ctx *ctx, uint32_t op1, uint32_t op2) {
	uint32_t code[] = {
		0x30020001,
		0xc4100780,
		0x20008005,
		0x00000083,
		0x20008009,
		0x00000103,
		0x2000800d,
		0x00000183,
		0x100f8011,
		0x00000003,
		0x00000801,
		0xa0000790,
		0xd0000011,
		0x80c00780,
		0xd0000215,
		0x80c00780,
		0xd0000419,
		0x80c00780,
		0xd000061d,
		0x80c00780,
		0x00000e01,
		0xa0000780,
		op1,
		op2,
		(op1 & 1 && (op2 & 3) != 3) ? 0xd000001d : 0xd0000019,
		0xa0c00780,
		0x0000001d,
		0x20001780,
		0xd000021d,
		0xa0c00780,
		0xf0000001,
		0xe0000781,
	};
	unsigned i;
	/* Poke code and flush it. */
	nva_wr32(ctx->cnum, 0x1700, 0x100);
	for (i = 0; i < ARRAY_SIZE(code); i++)
		nva_wr32(ctx->cnum, 0x730000 + i * 4, code[i]);
	nva_wr32(ctx->cnum, 0x70000, 1);
	while (nva_rd32(ctx->cnum, 0x70000));
	return g80_gr_mthd(ctx, 3, 0x380, 0);
}

static int fp_prep_grid(struct hwtest_ctx *ctx, uint32_t xtra) {
	if (g80_gr_idle(ctx))
		return 1;
	uint32_t units = nva_rd32(ctx->cnum, 0x1540);
	int tpc;
	int mp;
	for (tpc = 0; tpc < 16; tpc++) if (units & 1 << tpc)
		for (mp = 0; mp < 4; mp++) if (units & 1 << (mp + 24)) {
			uint32_t base;
			if (ctx->chipset.chipset >= 0xa0) {
				base = 0x408100 + tpc * 0x800 + mp * 0x80;
			} else {
				base = 0x408200 + tpc * 0x1000 + mp * 0x80;
			}
			nva_wr32(ctx->cnum, base+0x60, xtra);
			nva_wr32(ctx->cnum, base+0x64, 0);
		}
	return
		/* CTA config. */
		g80_gr_mthd(ctx, 3, 0x3a8, 0x40) ||
		g80_gr_mthd(ctx, 3, 0x3ac, 0x00010200) ||
		g80_gr_mthd(ctx, 3, 0x3b0, 0x00000001) ||
		g80_gr_mthd(ctx, 3, 0x2c0, 0x00000008) || /* regs */
		g80_gr_mthd(ctx, 3, 0x2b4, 0x00010200) || /* threads & barriers */
		g80_gr_mthd(ctx, 3, 0x2b8, 0x00000001) ||
		g80_gr_mthd(ctx, 3, 0x3b8, 0x00000002) ||
		g80_gr_mthd(ctx, 3, 0x2f8, 0x00000001) || /* init */
		/* Grid config. */
		g80_gr_mthd(ctx, 3, 0x388, 0) ||
		g80_gr_mthd(ctx, 3, 0x3a4, 0x00010001) ||
		g80_gr_mthd(ctx, 3, 0x374, 0) ||
		g80_gr_mthd(ctx, 3, 0x384, 0x100);
}

static void fp_write_data(struct hwtest_ctx *ctx, const uint32_t *src1, const uint32_t *src2, const uint32_t *src3, uint8_t *cc) {
	int i;
	nva_wr32(ctx->cnum, 0x1700, 0x200);
	for (i = 0; i < 0x200; i++) {
		nva_wr32(ctx->cnum, 0x700000 + i * 4, src1[i]);
		nva_wr32(ctx->cnum, 0x700800 + i * 4, src2[i]);
		nva_wr32(ctx->cnum, 0x701000 + i * 4, src3[i]);
		nva_wr32(ctx->cnum, 0x701800 + i * 4, cc[i]);
	}
	nva_wr32(ctx->cnum, 0x70000, 1);
	while (nva_rd32(ctx->cnum, 0x70000));
}

static int fp_run(struct hwtest_ctx *ctx) {
	return g80_gr_mthd(ctx, 3, 0x368, 0) || g80_gr_idle(ctx);
}

static uint32_t g80_add(bool b32, int sop, bool sat, uint32_t s1, uint32_t s3, uint8_t scc, uint8_t *pecc) {
	bool xbit;
	if (sop == 0) {
		/* add */
		xbit = false;
	} else if (sop == 1) {
		/* sub */
		s3 = ~s3;
		xbit = true;
	} else if (sop == 2) {
		/* subr */
		s1 = ~s1;
		xbit = true;
	} else if (sop == 3) {
		/* addc */
		xbit = (scc >> 2 & 1);
	} else {
		abort();
	}
	if (!b32) {
		s1 &= 0xffff;
		s3 &= 0xffff;
	}
	uint32_t exp = s1 + s3 + xbit;
	uint8_t ecc = 0;
	if (!b32) {
		if (exp & 0x10000)
			ecc |= 4;
		exp &= 0xffff;
	} else {
		if (exp <= s1 && exp <= s3 && (s1 || s3))
			ecc |= 4;
	}
	int sbit = b32 ? 31 : 15;
	if (s1 >> sbit == s3 >> sbit && s1 >> sbit != exp >> sbit) {
		ecc |= 8;
	}
	if (sat && ecc & 8) {
		/* saturate */
		if (s1 >> sbit & 1)
			exp = 1 << sbit;
		else
			exp = (1 << sbit) - 1;
	}
	if (exp >> sbit)
		ecc |= 2;
	if (!exp)
		ecc |= 1;
	if (pecc)
		*pecc = ecc;
	return exp;
}

static uint32_t g80_mul16(uint32_t s1, uint32_t s2, bool sign1, bool sign2) {
	if (sign1)
		s1 = (int16_t)s1;
	else
		s1 = (uint16_t)s1;
	if (sign2)
		s2 = (int16_t)s2;
	else
		s2 = (uint16_t)s2;
	return s1 * s2;
}

static uint32_t g80_mul24(uint32_t s1, uint32_t s2, bool sign, bool high) {
	s1 <<= 8;
	s2 <<= 8;
	uint64_t res = sign ? (int64_t)(int32_t)s1 * (int32_t)s2 : (uint64_t)s1 * s2;
	return res >> (high ? 32 : 16);
}

static int fp_check_data(struct hwtest_ctx *ctx, uint32_t op1, uint32_t op2, const uint32_t *src1, const uint32_t *src2, const uint32_t *src3, uint8_t *cc, uint32_t xtra) {
	int i;
	for (i = 0; i < 0x200; i++) {
		uint32_t real = nva_rd32(ctx->cnum, 0x700000 + i * 4);
		uint32_t rcc = nva_rd32(ctx->cnum, 0x700800 + i * 4);
		uint32_t exp;
		uint8_t ecc = 0xf;
		uint8_t op = (op1 >> 28) << 4;
		uint32_t s1 = src1[i];
		uint32_t s2 = src2[i];
		uint32_t s3 = src3[i];
		uint8_t scc = cc[i] & 0xf;
		if (!(op1 & 1)) {
			op |= 8;
		} else if ((op2 & 3) == 3) {
			op |= 8;
			s2 = (op1 >> 16 & 0x3f) | (op2 << 4 & 0xffffffc0);
		} else {
			op |= op2 >> 29;
		}
		int sop;
		switch (op) {
			case 0x20: /* iadd */
			case 0x30: /* iadd */
				sop = (op1 >> 22 & 1) | (op1 >> 27 & 2);
				exp = g80_add(op2 >> 26 & 1, sop, op2 >> 27 & 1, s1, s3, scc, &ecc);
				if (!(op2 >> 26 & 1))
					real &= 0xffff;
				break;
			case 0x28: /* iadd */
			case 0x38: /* iadd */
				sop = (op1 >> 22 & 1) | (op1 >> 27 & 2);
				exp = g80_add(op1 >> 15 & 1, sop, op1 >> 8 & 1, s1, s2, scc, 0);
				if (!(op1 >> 15 & 1))
					real &= 0xffff;
				break;
			case 0x33: /* icmp */
				if (!(op2 >> 26 & 1)) {
					s1 <<= 16;
					s2 <<= 16;
				}
				if (op2 >> 27 & 1) {
					s1 ^= 1 << 31;
					s2 ^= 1 << 31;
				}
				exp = (s1 < s2 ? 0 : s1 == s2 ? 1 : 2);
				exp = (op2 >> (14 + exp) & 1) ? -1 : 0;
				ecc = exp ? 2 : 1;
				if (!(op2 >> 26 & 1)) {
					real &= 0xffff;
					exp &= 0xffff;
				}
				break;
			case 0x34: /* imax */
			case 0x35: /* imin */
				if (!(op2 >> 26 & 1)) {
					s1 <<= 16;
					s2 <<= 16;
				}
				if (op2 >> 27 & 1) {
					s1 ^= 1 << 31;
					s2 ^= 1 << 31;
				}
				exp = ((s1 > s2) ^ (op2 >> 29 & 1)) ? s1 : s2;
				if (op2 >> 27 & 1) {
					exp ^= 1 << 31;
				}
				ecc = exp ? exp >> 31 ? 2 : 0 : 1;
				if (!(op2 >> 26 & 1)) {
					exp >>= 16;
					real &= 0xffff;
				}
				break;
			case 0x36: /* shl */
			case 0x37: /* shr */
				{
					uint32_t bits = (op2 >> 26 & 1) ? 32 : 16;
					if (op2 >> 20 & 1) {
						/* const shift count */
						s2 = op1 >> 16 & 0x7f;
					}
					if (!(op2 >> 26 & 1)) {
						s1 &= 0xffff;
						s2 &= 0xffff;
					}
					if (!(op2 >> 29 & 1)) {
						/* shl */
						if (s2 >= 32)
							exp = 0;
						else
							exp = s1 << s2;
						ecc = (s2 && s2 < bits && s1 >> (bits - s2) & 1) ? 4 : 0;
					} else {
						/* shr */
						if (op2 >> 27 & 1 && s1 >> (bits - 1)) {
							/* signed */
							if (s2 >= 32)
								exp = -1;
							else if (bits == 16)
								exp = (int16_t)s1 >> s2;
							else
								exp = (int32_t)s1 >> s2;
						} else {
							if (s2 >= 32)
								exp = 0;
							else
								exp = s1 >> s2;
						}
						ecc = (s2 && s2 < bits && s1 >> (s2 - 1) & 1) ? 4 : 0;
					}
					if (!(op2 >> 26 & 1)) {
						exp &= 0xffff;
						real &= 0xffff;
						ecc |= exp ? exp >> 15 ? 2 : 0 : 1;
					} else {
						ecc |= exp ? exp >> 31 ? 2 : 0 : 1;
					}
					if (exp >> (bits - 1) != s1 >> (bits - 1) && s2 == 1)
						ecc |= 8;
					break;
				}
			case 0x40:
				if (op2 >> 16 & 1)
					exp = g80_mul24(s1, s2, op2 >> 15 & 1, op2 >> 14 & 1);
				else
					exp = g80_mul16(s1, s2, op2 >> 15 & 1, op2 >> 14 & 1);
				ecc = exp ? exp >> 31 ? 2 : 0 : 1;
				break;
			case 0x48:
				if (op1 >> 22 & 1)
					exp = g80_mul24(s1, s2, op1 >> 15 & 1, op1 >> 8 & 1);
				else
					exp = g80_mul16(s1, s2, op1 >> 15 & 1, op1 >> 8 & 1);
				break;
			case 0x50:
				if (!(op2 >> 26 & 1)) {
					s1 <<= 16;
					s2 <<= 16;
				}
				if (op2 >> 27 & 1) {
					s1 ^= 1 << 31;
					s2 ^= 1 << 31;
				}
				s1 = s1 > s2 ? s1 - s2 : s2 - s1;
				if (!(op2 >> 26 & 1)) {
					s1 >>= 16;
				}
				exp = g80_add(true, 0, false, s1, s3, 0, &ecc);
				break;
			case 0x58:
				if (!(op1 >> 15 & 1)) {
					s1 <<= 16;
					s2 <<= 16;
				}
				if (op1 >> 8 & 1) {
					s1 ^= 1 << 31;
					s2 ^= 1 << 31;
				}
				s1 = s1 > s2 ? s1 - s2 : s2 - s1;
				if (!(op1 >> 15 & 1)) {
					s1 >>= 16;
				}
				exp = g80_add(true, 0, false, s1, s3, 0, 0);
				break;
			case 0x60: /* imad u16 */
				exp = g80_mul16(s1, s2, false, false);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x61: /* imad s16 */
				exp = g80_mul16(s1, s2, true, true);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x62: /* imad sat s16 */
				exp = g80_mul16(s1, s2, true, true);
				exp = g80_add(true, op2 >> 26 & 3, true, exp, s3, scc, &ecc);
				break;
			case 0x63: /* imad u24 */
			case 0x71: /* imad */
			case 0x72: /* imad */
			case 0x73: /* imad */
			case 0x74: /* imad */
			case 0x75: /* imad */
			case 0x76: /* imad */
			case 0x77: /* imad */
				exp = g80_mul24(s1, s2, false, false);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x64: /* imad s24 */
				exp = g80_mul24(s1, s2, true, false);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x65: /* imad sat s24 */
				exp = g80_mul24(s1, s2, true, false);
				exp = g80_add(true, op2 >> 26 & 3, true, exp, s3, scc, &ecc);
				break;
			case 0x66: /* imad high u24 */
				exp = g80_mul24(s1, s2, false, true);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x67: /* imad high s24 */
				exp = g80_mul24(s1, s2, true, true);
				exp = g80_add(true, op2 >> 26 & 3, false, exp, s3, scc, &ecc);
				break;
			case 0x70: /* imad sat high s24 */
				exp = g80_mul24(s1, s2, true, true);
				exp = g80_add(true, op2 >> 26 & 3, true, exp, s3, scc, &ecc);
				break;
			case 0x68:
			case 0x78:
				sop = (op1 >> 14 & 2) | (op1 >> 8 & 1);
				if (sop == 3)
					exp = g80_mul24(s1, s2, false, false);
				else
					exp = g80_mul16(s1, s2, sop > 0, sop > 0);
				exp = g80_add(true, (op1 >> 22 & 1) | (op1 >> 27 & 2), sop == 2, exp, s3, scc, 0);
				break;
			case 0xd0: /* logop */
				if (op2 >> 16 & 1)
					s1 = ~s1;
				if (op2 >> 17 & 1)
					s2 = ~s2;
				sop = op2 >> 14 & 3;
				if (sop == 0) {
					exp = s1 & s2;
				} else if (sop == 1) {
					exp = s1 | s2;
				} else if (sop == 2) {
					exp = s1 ^ s2;
				} else if (sop == 3) {
					exp = s2;
				} else {
					abort();
				}
				if (op2 >> 26 & 1) {
					ecc = exp ? exp >> 31 ? 2 : 0 : 1;
				} else {
					exp &= 0xffff;
					real &= 0xffff;
					ecc = exp ? exp >> 15 ? 2 : 0 : 1;
				}
				break;
			case 0xd8: /* logop */
				if (op1 >> 22 & 1)
					s1 = ~s1;
				sop = (op1 >> 8 & 1) | (op1 >> 14 & 2);
				if (sop == 0) {
					exp = s1 & s2;
				} else if (sop == 1) {
					exp = s1 | s2;
				} else if (sop == 2) {
					exp = s1 ^ s2;
				} else if (sop == 3) {
					exp = s2;
				} else {
					abort();
				}
				break;
			default:
				abort();
		}
		if (real != exp || rcc != ecc) {
			printf("fp %08x %08x (%08x %08x %08x %x): got %08x.%x expected %08x.%x diff %d\n", op1, op2, src1[i], src2[i], src3[i], scc, real, rcc, exp, ecc, real-exp);
			return 1;
		}
	}
	return 0;
}

static int fp_test(struct hwtest_ctx *ctx, uint32_t op1, uint32_t op2, const uint32_t *src1, const uint32_t *src2, const uint32_t *src3, uint8_t *cc, uint32_t xtra) {
	fp_write_data(ctx, src1, src2, src3, cc);
	if (fp_run(ctx))
		return 1;
	if (fp_check_data(ctx, op1, op2, src1, src2, src3, cc, xtra))
		return 1;
	return 0;
}

static void fp_gen(struct hwtest_ctx *ctx, uint32_t *src1, uint32_t *src2, uint32_t *src3, uint8_t *cc) {
	int i;
	int mode = jrand48(ctx->rand48);
	for (i = 0; i < 0x200; i++) {
		src1[i] = jrand48(ctx->rand48);
		src2[i] = jrand48(ctx->rand48);
		src3[i] = jrand48(ctx->rand48);
		if (mode & 0x1)
			src1[i] = (int8_t)src1[i];
		if (mode & 0x2)
			src2[i] = (int8_t)src2[i];
		if (mode & 0x4)
			src3[i] = (int8_t)src3[i];
		cc[i] = jrand48(ctx->rand48);
	}
}

static int test_iadd(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x20000001 | (jrand48(ctx->rand48) & 0x1e7f0000);
		uint32_t op2 = 0x000007d0 | (jrand48(ctx->rand48) & 0x1fc00004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			op1 |= 0x81c;
			op2 |= 0x18000;
		} else {
			/* 16-bit */
			op1 |= 0x1038;
			op2 |= 0x30000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_icmp(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x30000001 | (jrand48(ctx->rand48) & 0x0e000000);
		uint32_t op2 = 0x600007d0 | (jrand48(ctx->rand48) & 0x1fdfc004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			op1 |= 0x5081c;
		} else {
			/* 16-bit */
			op1 |= 0xa1038;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_iminmax(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x30000001 | (jrand48(ctx->rand48) & 0x0e000000);
		uint32_t op2 = 0x800007d0 | (jrand48(ctx->rand48) & 0x3fdfc004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			op1 |= 0x5081c;
		} else {
			/* 16-bit */
			op1 |= 0xa1038;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_shift(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x30000001 | (jrand48(ctx->rand48) & 0x0e7f0000);
		uint32_t op2 = 0xc00007d0 | (jrand48(ctx->rand48) & 0x3fdfc004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			if (!(op2 >> 20 & 1)) {
				op1 &= ~0x7f0000;
				op1 |= 0x50000;
			}
			op1 |= 0x081c;
		} else {
			/* 16-bit */
			if (!(op2 >> 20 & 1)) {
				op1 &= ~0x7f0000;
				op1 |= 0xa0000;
			}
			op1 |= 0x1038;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imul(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x4000001d | (jrand48(ctx->rand48) & 0x0e000000);
		uint32_t op2 = 0x000007d0 | (jrand48(ctx->rand48) & 0x1fdfc004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 16 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_isad(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x5000001d | (jrand48(ctx->rand48) & 0x0e000000);
		uint32_t op2 = 0x000187d0 | (jrand48(ctx->rand48) & 0x1fc00004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imad(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x6000001d | (jrand48(ctx->rand48) & 0x1e000000);
		uint32_t op2 = 0x000187d0 | (jrand48(ctx->rand48) & 0xffc00004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >= 0x60000000 || op1 >> 28 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_logop(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0xd0000001 | (jrand48(ctx->rand48) & 0x0e000000);
		uint32_t op2 = 0x000007d0 | (jrand48(ctx->rand48) & 0x1fdfc004);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op2 >> 26 & 1) {
			/* 32-bit */
			op1 |= 0x5081c;
		} else {
			/* 16-bit */
			op1 |= 0xa1038;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_iadd_s(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x20000000 | (jrand48(ctx->rand48) & 0x1e408100);
		uint32_t op2 = 0x10000000;
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 15 & 1) {
			/* 32-bit */
			op1 |= 0x50818;
		} else {
			/* 16-bit */
			op1 |= 0xa1030;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imul_s(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x40000018 | (jrand48(ctx->rand48) & 0x0e408100);
		uint32_t op2 = 0x10000000;
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 22 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_isad_s(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x50000018 | (jrand48(ctx->rand48) & 0x0e408100);
		uint32_t op2 = 0x10000000;
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 15 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imad_s(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x60000018 | (jrand48(ctx->rand48) & 0x1e408100);
		uint32_t op2 = 0x10000000;
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 15 & 1 && op1 >> 8 & 1) {
			/* 32-bit */
			op1 |= 0x50800;
		} else {
			/* 16-bit */
			op1 |= 0xa1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_iadd_i(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x20000001 | (jrand48(ctx->rand48) & 0x1e7f8100);
		uint32_t op2 = 0x00000003 | (jrand48(ctx->rand48) & 0x1ffffffc);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 15 & 1) {
			/* 32-bit */
			op1 |= 0x0818;
		} else {
			/* 16-bit */
			op1 |= 0x1030;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imul_i(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x40000019 | (jrand48(ctx->rand48) & 0x0e7f8100);
		uint32_t op2 = 0x00000003 | (jrand48(ctx->rand48) & 0x1ffffffc);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 22 & 1) {
			/* 32-bit */
			op1 |= 0x0800;
		} else {
			/* 16-bit */
			op1 |= 0x1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_imad_i(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0x60000019 | (jrand48(ctx->rand48) & 0x1e7f8100);
		uint32_t op2 = 0x00000003 | (jrand48(ctx->rand48) & 0x1ffffffc);
		uint32_t xtra = jrand48(ctx->rand48);
		if (op1 >> 15 & 1 && op1 >> 8 & 1) {
			/* 32-bit */
			op1 |= 0x0800;
		} else {
			/* 16-bit */
			op1 |= 0x1000;
		}
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

static int test_logop_i(struct hwtest_ctx *ctx) {
	int i;
	for (i = 0; i < 100000; i++) {
		uint32_t op1 = 0xd0000001 | (jrand48(ctx->rand48) & 0x0e7f8100);
		uint32_t op2 = 0x00000003 | (jrand48(ctx->rand48) & 0x1ffffffc);
		uint32_t xtra = jrand48(ctx->rand48);
		/* 32-bit */
		op1 |= 0x0818;
		if (fp_prep_grid(ctx, xtra))
			return HWTEST_RES_FAIL;
		if (fp_prep_code(ctx, op1, op2))
				return HWTEST_RES_FAIL;
		uint32_t src1[0x200], src2[0x200], src3[0x200];
		uint8_t cc[0x200];
		fp_gen(ctx, src1, src2, src3, cc);
		if (fp_test(ctx, op1, op2, src1, src2, src3, cc, xtra))
			return HWTEST_RES_FAIL;
	}
	return HWTEST_RES_PASS;
}

HWTEST_DEF_GROUP(g80_int,
	HWTEST_TEST(test_iadd, 0),
	HWTEST_TEST(test_icmp, 0),
	HWTEST_TEST(test_iminmax, 0),
	HWTEST_TEST(test_shift, 0),
	HWTEST_TEST(test_imul, 0),
	HWTEST_TEST(test_isad, 0),
	HWTEST_TEST(test_imad, 0),
	HWTEST_TEST(test_logop, 0),
	HWTEST_TEST(test_iadd_s, 0),
	HWTEST_TEST(test_imul_s, 0),
	HWTEST_TEST(test_isad_s, 0),
	HWTEST_TEST(test_imad_s, 0),
	HWTEST_TEST(test_iadd_i, 0),
	HWTEST_TEST(test_imul_i, 0),
	HWTEST_TEST(test_imad_i, 0),
	HWTEST_TEST(test_logop_i, 0),
)
