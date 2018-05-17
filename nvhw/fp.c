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

#include "nvhw/fp.h"

#include <stdlib.h>

uint32_t fp32_sat(uint32_t x, bool fnz) {
	if (FP32_ISNAN(x))
		return fnz ? 0 : FP32_CNAN;
	if (FP32_SIGN(x))
		return 0;
	else if (x >= 0x3f800000)
		return 0x3f800000;
	else
		return x;
}

uint32_t fp32_rint(uint32_t x, enum fp_rm rm) {
	bool sx;
	int ex;
	uint32_t fx;
	if (FP32_ISNAN(x))
		return FP32_CNAN;
	if (FP32_ISINF(x))
		return x;
	fp32_parsefin(x, &sx, &ex, &fx, true);
	/* For ex larger than that, result is already an integer,
	   and our shift would overflow.  */
	if (ex < FP32_MIDE + 23) {
		int shift = (FP32_MIDE + 23) - ex;
		fx = shr32(fx, shift, fp_adjust_rm(rm, sx));
		if (fx == 0) {
			ex = 0;
		} else {
			ex = FP32_MIDE + 23;
			fx = norm32(fx, &ex, 23);
		}
	}
	return fp32_mkfin(sx, ex, fx, rm | FP_FTZ);
}

uint32_t fp32_minmax(uint32_t a, uint32_t b, bool min, bool ftz) {
	if (FP32_ISNAN(a)) {
		if (FP32_ISNAN(b))
			return FP32_CNAN;
		a = b;
	} else if (FP32_ISNAN(b)) {
		b = a;
	}
	bool sa, sb;
	int ea, eb;
	uint32_t fa, fb;
	/* Flush denormals.  */
	fp32_parsefin(a, &sa, &ea, &fa, ftz);
	a = fp32_mkfin(sa, ea, fa, FP_RN | (ftz ? FP_FTZ : 0));
	fp32_parsefin(b, &sb, &eb, &fb, ftz);
	b = fp32_mkfin(sb, eb, fb, FP_RN | (ftz ? FP_FTZ : 0));
	bool flip = min;
	if (sa != sb) {
		/* Different signs, pick the positive one */
		if (sa)
			flip = !flip;
		return flip ? b : a;
	} else {
		/* Same signs, compare exp & fract */
		if (sa)
			flip = !flip;
		if (a < b)
			flip = !flip;
		return flip ? b : a;
	}
}

enum fp_cmp fp32_cmp(uint32_t a, uint32_t b, bool ftz) {
	if (FP32_ISNAN(a) || FP32_ISNAN(b))
		return FP_UN;
	bool sa, sb;
	int ea, eb;
	uint32_t fa, fb;
	fp32_parsefin(a, &sa, &ea, &fa, ftz);
	fp32_parsefin(b, &sb, &eb, &fb, ftz);
	if (fa == 0 && fb == 0) {
		/* Zeros - equal regardless of sign. */
		return FP_EQ;
	} else if (sa != sb) {
		/* Different signs, both finite or infinity. */
		return sa ? FP_LT : FP_GT;
	} else if (ea == eb && fa == fb) {
		/* Get the equality case out of the way. */
		return FP_EQ;
	} else {
		/* Same signs, both finite or infinity. */
		if ((a < b) ^ sa)
			return FP_LT;
		else
			return FP_GT;
	}
}

uint32_t fp32_add(uint32_t a, uint32_t b, int flags) {
	bool ftz = !!(flags & FP_FTZ);
	int rm = flags & FP_ROUND_MASK;
	if (FP32_ISNAN(a) || FP32_ISNAN(b))
		return FP32_CNAN;
	if (FP32_ISINF(a)) {
		if (FP32_ISINF(b) && a != b) {
			/* Inf-Inf */
			return FP32_CNAN;
		}
		return a;
	}
	if (FP32_ISINF(b))
		return b;
	/* Two honest real numbers involved. */
	bool sa, sb, sr;
	int ea, eb, er;
	uint32_t fa, fb;
	fp32_parsefin(a, &sa, &ea, &fa, ftz);
	fp32_parsefin(b, &sb, &eb, &fb, ftz);
	er = (ea > eb ? ea : eb) + 1;
	int32_t res = 0;
	fa = shr32(fa, er - ea - 4, FP_RT);
	fb = shr32(fb, er - eb - 4, FP_RT);
	res += sa ? -fa : fa;
	res += sb ? -fb : fb;
	if (res == 0) {
		/* Got a proper 0. */
		er = 0;
		/* Make a -0 if both inputs were negative (ie. both -0). */
		sr = sa && sb;
	} else {
		/* Compute sign, make sure accumulator is positive. */
		sr = res < 0;
		if (sr)
			res = -res;
		/* 23 bits of input
		   +1 bit of implicit 0
		   +3 bits for proper rounding
		   +1 bit because we add two numbers
		  ---
		   28 bit accumulator.
		   Make sure bit 27 is actually set - normalize the number. */
		res = norm32(res, &er, 27);
		/* Round it. */
		res = shr32(res, 4, fp_adjust_rm(rm, sr));
	}
	return fp32_mkfin(sr, er, res, flags);
}

uint32_t fp32_mul(uint32_t a, uint32_t b, int flags) {
	bool sa, sb, sr;
	int ea, eb, er;
	uint32_t fa, fb;
	bool ftz = !!(flags & FP_FTZ);
	bool zero_wins = !!(flags & FP_ZERO_WINS);
	int rm = flags & FP_ROUND_MASK;
	if (!ftz)
		abort();
	fp32_parsefin(a, &sa, &ea, &fa, ftz);
	fp32_parsefin(b, &sb, &eb, &fb, ftz);
	if (zero_wins && (fa == 0 || fb == 0))
		return 0;
	if (FP32_ISNAN(a) || FP32_ISNAN(b))
		return FP32_CNAN;
	sr = sa ^ sb;
	if (FP32_ISINF(a) || FP32_ISINF(b)) {
		if (fa == 0 || fb == 0) {
			/* Inf*0 is NaN */
			return FP32_CNAN;
		} else {
			/* Inf*finite and Inf*Inf are +-Inf */
			return FP32_INF(sr);
		}
	}
	er = ea + eb - FP32_MIDE + 1;
	uint64_t res = (uint64_t)fa * fb;
	if (res == 0) {
		er = 0;
	} else {
		/* We multiplied two 24-bit numbers starting with 1s.
		   The first 1 has to be at either 46th or 47th
		   position.  Make it 47. */
		res = norm64(res, &er, 47);
		res = shr64(res, 24, fp_adjust_rm(rm, sr));
	}
	return fp32_mkfin(sr, er, res, flags);
}

uint32_t fp32_mad(uint32_t a, uint32_t b, uint32_t c, int flags) {
	bool sa, sb, sc, ss, sr;
	int ea, eb, ec, es, er;
	uint32_t fa, fb, fc;
	bool ftz = !!(flags & FP_FTZ);
	bool zero_wins = !!(flags & FP_ZERO_WINS);
	int rm = flags & FP_ROUND_MASK;
	fp32_parsefin(a, &sa, &ea, &fa, ftz);
	fp32_parsefin(b, &sb, &eb, &fb, ftz);
	fp32_parsefin(c, &sc, &ec, &fc, ftz);
	if (zero_wins && (fa == 0 || fb == 0))
		return fp32_add(c, 0, flags);
	ss = sa ^ sb;
	if (FP32_ISNAN(a) || FP32_ISNAN(b) || FP32_ISNAN(c))
		return FP32_CNAN;
	if (FP32_ISINF(a) || FP32_ISINF(b)) {
		if (fa == 0 || fb == 0) {
			/* 0*inf */
			return FP32_CNAN;
		} else if (FP32_ISINF(c) && sc != ss) {
			/* inf*inf - inf */
			return FP32_CNAN;
		} else {
			return FP32_INF(ss);
		}
	}
	if (FP32_ISINF(c))
		return FP32_INF(sc);
	if (fa == 0 || fb == 0 || fc == 0)
		return fp32_add(fp32_mul(a, b, flags), c, flags);
	es = ea + eb - FP32_MIDE + 1;
	int64_t res = (uint64_t)fa * fb;
	/* We multiplied two 24-bit numbers starting with 1s.
	   The first 1 has to be at either 46th or 47th
	   position.  Make it 47. */
	res = norm64(res, &es, 47);
	res = shr64(res, 24, fc ? FP_RZ : rm);
	/* Two honest real numbers involved. */
	er = (es > ec ? es : ec) + 1;
	res = shr32(res, er - es - 4, FP_RT);
	if (ss)
		res = -res;
	fc = shr32(fc, er - ec - 4, FP_RT);
	res += sc ? -(int64_t)fc : fc;
	if (!res) {
		/* Got a proper 0. */
		er = 0;
		/* Make a -0 if both inputs were negative (ie. both -0). */
		sr = ss && sc;
	} else {
		/* Compute sign, make sure accumulator is positive. */
		sr = res < 0;
		if (sr)
			res = -res;
		/* 23 bits of input
		   +1 bit of implicit 0
		   +3 bits for proper rounding
		   +1 bit because we add two numbers
		  ---
		   28 bit accumulator.
		   Make sure bit 27 is actually set - normalize the number. */
		res = norm32(res, &er, 27);
		if (er < 1) {
			res >>= 1-er;
			er = 1;
		}
		/* Round it. */
		res = shr32(res, 4, rm);
	}
	return fp32_mkfin(sr, er, res, flags);
}

uint32_t fp16_to_fp32(uint16_t x) {
	bool sx;
	int ex;
	uint16_t sfx;
	uint32_t fx;
	fp16_parsefin(x, &sx, &ex, &sfx, false);
	if (FP16_ISNAN(x))
		return FP32_CNAN;
	if (FP16_ISINF(x))
		return FP32_INF(sx);
	ex += FP32_MIDE - FP16_MIDE;
	fx = sfx << 13;
	fx = norm32(fx, &ex, 23);
	return fp32_mkfin(sx, ex, fx, FP_RN | FP_FTZ);
}

uint16_t fp32_to_fp16(uint32_t x, enum fp_rm rm, bool rint) {
	bool sx;
	int ex;
	uint32_t fx;
	fp32_parsefin(x, &sx, &ex, &fx, true);
	if (FP32_ISNAN(x))
		return FP16_NAN;
	if (FP32_ISINF(x))
		return FP16_INF(sx);
	if (ex <= 0x5d)
		/* 0, rounding be damned */
		return FP16_MAKE(sx, 0, 0);
	ex += FP16_MIDE - FP32_MIDE;
	int shift = 13;
	if (ex <= 0) {
		/* Going to be a denormal... */
		shift -= ex - 1;
		ex = 1;
	}
	fx = shr32(fx, shift, rint ? FP_RZ : fp_adjust_rm(rm, sx));
	return fp16_mkfin(sx, ex, fx, rm);
}

uint32_t fp32_from_u64(uint64_t x, enum fp_rm rm) {
	if (!x)
		return 0;
	int ex = FP32_MIDE + 63;
	uint32_t fx;
	/* first, shift it until MSB is lit */
	x = norm64(x, &ex, 63);
	fx = shr64(x, 40, rm);
	return fp32_mkfin(false, ex, fx, rm | FP_FTZ);
}

uint64_t fp32_to_u64(uint32_t x, enum fp_rm rm, bool ftz) {
	bool sx;
	int ex;
	uint32_t fx;
	fp32_parsefin(x, &sx, &ex, &fx, ftz);
	if (FP32_ISNAN(x) || sx)
		return 0;
	if (FP32_ISINF(x))
		return -1ull;
	ex -= FP32_MIDE + 23;
	if (ex > 40)
		return -1ull;
	return shr64(fx, -ex, rm);
}

enum fp_cmp fp64_cmp(uint64_t a, uint64_t b) {
	if (FP64_ISNAN(a) || FP64_ISNAN(b))
		return FP_UN;
	bool sa, sb;
	int ea, eb;
	uint64_t fa, fb;
	fp64_parsefin(a, &sa, &ea, &fa);
	fp64_parsefin(b, &sb, &eb, &fb);
	if (fa == 0 && fb == 0) {
		/* Zeros - equal regardless of sign. */
		return FP_EQ;
	} else if (sa != sb) {
		/* Different signs, both finite or infinity. */
		return sa ? FP_LT : FP_GT;
	} else if (ea == eb && fa == fb) {
		/* Get the equality case out of the way. */
		return FP_EQ;
	} else {
		/* Same signs, both finite or infinity. */
		if ((a < b) ^ sa)
			return FP_LT;
		else
			return FP_GT;
	}
}

uint64_t fp64_rint(uint64_t x, enum fp_rm rm) {
	bool sx;
	int ex;
	uint64_t fx;
	if (FP64_ISNAN(x))
		return x;
	if (FP64_ISINF(x))
		return x;
	fp64_parsefin(x, &sx, &ex, &fx);
	/* For ex larger than that, result is already an integer,
	   and our shift would overflow.  */
	if (ex < FP64_MIDE + 52) {
		int shift = (FP64_MIDE + 52) - ex;
		fx = shr64(fx, shift, fp_adjust_rm(rm, sx));
		if (fx == 0) {
			ex = 0;
		} else {
			ex = FP64_MIDE + 52;
			fx = norm64(fx, &ex, 52);
		}
	}
	return fp64_mkfin(sx, ex, fx, rm);
}

uint64_t fp64_from_u64(uint64_t x, enum fp_rm rm) {
	if (!x)
		return 0;
	int ex = FP64_MIDE + 63;
	uint64_t fx;
	/* first, shift it until MSB is lit */
	x = norm64(x, &ex, 63);
	fx = shr64(x, 11, rm);
	return fp64_mkfin(false, ex, fx, rm);
}

uint64_t fp64_to_u64(uint64_t x, enum fp_rm rm) {
	bool sx;
	int ex;
	uint64_t fx;
	fp64_parsefin(x, &sx, &ex, &fx);
	if (FP64_ISNAN(x) || sx)
		return 0;
	if (FP64_ISINF(x))
		return -1ull;
	ex -= FP64_MIDE + 52;
	if (ex > 11)
		return -1ull;
	return shr64(fx, -ex, rm);
}

uint64_t fp32_to_fp64(uint32_t x) {
	bool sx;
	int ex;
	uint32_t sfx;
	uint64_t fx;
	fp32_parsefin(x, &sx, &ex, &sfx, false);
	if (FP32_ISNAN(x))
		return FP64_NAN(sx, (uint64_t)(((sfx - FP32_IONE) | 1 << 22)) << (52 - 23));
	if (FP32_ISINF(x))
		return FP64_INF(sx);
	ex += FP64_MIDE - FP32_MIDE;
	fx = (uint64_t)sfx << (52 - 23);
	fx = norm64(fx, &ex, 52);
	return fp64_mkfin(sx, ex, fx, FP_RN);
}

uint32_t fp64_to_fp32(uint64_t x, enum fp_rm rm, bool rint) {
	bool sx;
	int ex;
	uint64_t fx;
	fp64_parsefin(x, &sx, &ex, &fx);
	if (FP64_ISNAN(x))
		return FP32_NAN(sx, (fx - FP64_IONE) >> (52 - 23) | (1 << 22));
	if (FP64_ISINF(x))
		return FP32_INF(sx);
	ex += FP32_MIDE - FP64_MIDE;
	int shift = 52 - 23;
	if (ex <= 0) {
		/* Going to be a denormal... */
		shift -= ex - 1;
		ex = 1;
	}
	fx = shr64(fx, shift, rint ? FP_RZ : fp_adjust_rm(rm, sx));
	return fp32_mkfin(sx, ex, fx, rm);
}

uint64_t fp64_minmax(uint64_t a, uint64_t b, bool min) {
	if (FP64_ISNAN(a)) {
		if (FP64_ISNAN(b))
			return b;
		a = b;
	} else if (FP64_ISNAN(b)) {
		b = a;
	}
	bool sa, sb;
	int ea, eb;
	uint64_t fa, fb;
	fp64_parsefin(a, &sa, &ea, &fa);
	fp64_parsefin(b, &sb, &eb, &fb);
	bool flip = min;
	if (sa != sb) {
		/* Different signs, pick the positive one */
		if (sa)
			flip = !flip;
		return flip ? b : a;
	} else {
		/* Same signs, compare exp & fract */
		if (sa)
			flip = !flip;
		if (a < b)
			flip = !flip;
		return flip ? b : a;
	}
}

uint64_t fp64_add(uint64_t a, uint64_t b, enum fp_rm rm) {
	if (FP64_ISNAN(b))
		return b;
	if (FP64_ISNAN(a))
		return a;
	if (FP64_ISINF(a)) {
		if (FP64_ISINF(b) && a != b) {
			/* Inf-Inf */
			return FP64_NAN(true, 1ull << 51);
		}
		return a;
	}
	if (FP64_ISINF(b))
		return b;
	/* Two honest real numbers involved. */
	bool sa, sb, sr;
	int ea, eb, er;
	uint64_t fa, fb;
	fp64_parsefin(a, &sa, &ea, &fa);
	fp64_parsefin(b, &sb, &eb, &fb);
	er = (ea > eb ? ea : eb) + 1;
	int64_t res = 0;
	fa = shr64(fa, er - ea - 4, FP_RT);
	fb = shr64(fb, er - eb - 4, FP_RT);
	res += sa ? -fa : fa;
	res += sb ? -fb : fb;
	if (res == 0) {
		/* Got a proper 0. */
		/* Make a -0 if both inputs were negative (ie. both -0). */
		if (sa == sb)
			sr = sa;
		else
			sr = (rm == FP_RM);
	} else {
		/* Compute sign, make sure accumulator is positive. */
		sr = res < 0;
		if (sr)
			res = -res;
		res = norm64(res, &er, 56);
		if (er < 1) {
			res >>= 1 - er;
			er = 1;
		}
		/* Round it. */
		res = shr64(res, 4, fp_adjust_rm(rm, sr));
	}
	return fp64_mkfin(sr, er, res, rm);
}

uint64_t fp64_fma(uint64_t a, uint64_t b, uint64_t c, enum fp_rm rm) {
	bool sa, sb, sc, ss, sr;
	int ea, eb, ec, es, er;
	uint64_t fa, fb, fc;
	fp64_parsefin(a, &sa, &ea, &fa);
	fp64_parsefin(b, &sb, &eb, &fb);
	fp64_parsefin(c, &sc, &ec, &fc);
	if (FP64_ISNAN(b))
		return b;
	if (FP64_ISNAN(c))
		return c;
	if (FP64_ISNAN(a))
		return a;
	ss = sa ^ sb;
	if (FP64_ISINF(a) || FP64_ISINF(b)) {
		if (fa == 0 || fb == 0) {
			/* Inf*0 is NaN */
			return FP64_NAN(true, 1ull << 51);
		} else {
			/* Inf*finite and Inf*Inf are +-Inf */
			if (FP64_ISINF(c) && sc != ss) {
				/* Inf-Inf */
				return FP64_NAN(true, 1ull << 51);
			} else {
				return FP64_INF(ss);
			}
		}
	} else if (FP64_ISINF(c)) {
		return c;
	}
	es = ea + eb - FP64_MIDE + 1;
	struct uint128 res = mul128(fa, fb * 8);
	res = norm128(res, &es, 108);
	if (!res.lo && !res.hi)
		es = ec;
	er = es;
	if (er < ec)
		er = ec;
	er++;
	if (er < 1)
		er = 1;
	res = shr128(res, er - 1 - es, FP_RT);
	if (ss)
		res = neg128(res);
	int sh = er - 4 - ec - 53;
	if (sh > 0) {
		fc = shr64(fc, sh, FP_RT);
		sh = 0;
	}
	if (sc)
		res = sub128(res, fc, -sh);
	else
		res = add128(res, fc, -sh);
	if (res.hi == 0 && res.lo == 0) {
		/* Got a proper 0. */
		/* Make a -0 if both inputs were negative (ie. both -0). */
		if (ss == sc)
			sr = ss;
		else
			sr = (rm == FP_RM);
	} else {
		sr = res.hi >> 63 & 1;
		if (sr)
			res = neg128(res);
		res = norm128(res, &er, 109);
		if (er < 1) {
			res = shr128(res, 1-er, FP_RZ);
			er = 1;
		}
		res = shr128(res, 57, fp_adjust_rm(rm, sr));
	}
	return fp64_mkfin(sr, er, res.lo, rm);
}

uint64_t fp64_mul(uint64_t a, uint64_t b, enum fp_rm rm) {
	return fp64_fma(a, b, rm == FP_RM ? 0 : 1ull << 63, rm);
}
