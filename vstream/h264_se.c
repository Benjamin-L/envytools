/*
 * Copyright (C) 2011 Marcelina Kościelnicka <mwk@0x04.net>
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

#include "h264.h"
#include "h264_cabac.h"
#include <stdio.h>
#include <stdlib.h>

int h264_mb_skip_flag(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *binVal) {
	if (!cabac) {
		fprintf (stderr, "mb_skip_flag used in CAVLC mode\n");
		return 1;
	}
	int ctxIdxOffset, ctxIdxInc;
	if (cabac->slice->slice_type == H264_SLICE_TYPE_P || cabac->slice->slice_type == H264_SLICE_TYPE_SP) {
		ctxIdxOffset = H264_CABAC_CTXIDX_MB_SKIP_FLAG_P;
	} else if (cabac->slice->slice_type == H264_SLICE_TYPE_B) {
		ctxIdxOffset = H264_CABAC_CTXIDX_MB_SKIP_FLAG_B;
	} else {
		fprintf (stderr, "mb_skip_flag used in I/SI slice\n");
		return 1;
	}
	const struct h264_macroblock *mbA = h264_mb_nb(cabac->slice, H264_MB_A, 0);
	const struct h264_macroblock *mbB = h264_mb_nb(cabac->slice, H264_MB_B, 0);
	int condTermFlagA = (mbA->mb_type != H264_MB_TYPE_UNAVAIL && !h264_is_skip_mb_type(mbA->mb_type));
	int condTermFlagB = (mbB->mb_type != H264_MB_TYPE_UNAVAIL && !h264_is_skip_mb_type(mbB->mb_type));
	ctxIdxInc = condTermFlagA + condTermFlagB;
	return h264_cabac_decision(str, cabac, ctxIdxOffset+ctxIdxInc, binVal);
}

int h264_mb_field_decoding_flag(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *binVal) {
	if (!cabac)
		return vs_u(str, binVal, 1);
	int ctxIdxOffset = H264_CABAC_CTXIDX_MB_FIELD_DECODING_FLAG, ctxIdxInc;
	int condTermFlagA = h264_mb_nb_p(cabac->slice, H264_MB_A, 0)->mb_field_decoding_flag;
	int condTermFlagB = h264_mb_nb_p(cabac->slice, H264_MB_B, 0)->mb_field_decoding_flag;
	ctxIdxInc = condTermFlagA + condTermFlagB;
	return h264_cabac_decision(str, cabac, ctxIdxOffset+ctxIdxInc, binVal);
}

static const struct h264_cabac_se_val mb_type_i[] = {
	{ H264_MB_TYPE_I_NXN, 0, 1, { {0,0} } },
	{ H264_MB_TYPE_I_16X16_0_0_0, 0, 6, { {0,1}, {1,0}, {2,0}, {3,0}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_0_0, 0, 6, { {0,1}, {1,0}, {2,0}, {3,0}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_0_0, 0, 6, { {0,1}, {1,0}, {2,0}, {3,0}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_0_0, 0, 6, { {0,1}, {1,0}, {2,0}, {3,0}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_0_1_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,0}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_1_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,0}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_1_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,0}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_1_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,0}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_0_2_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,1}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_2_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,1}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_2_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,1}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_2_0, 0, 7, { {0,1}, {1,0}, {2,0}, {3,1}, {4,1}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_0_0_1, 0, 6, { {0,1}, {1,0}, {2,1}, {3,0}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_0_1, 0, 6, { {0,1}, {1,0}, {2,1}, {3,0}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_0_1, 0, 6, { {0,1}, {1,0}, {2,1}, {3,0}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_0_1, 0, 6, { {0,1}, {1,0}, {2,1}, {3,0}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_0_1_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,0}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_1_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,0}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_1_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,0}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_1_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,0}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_0_2_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,1}, {5,0}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_1_2_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,1}, {5,0}, {6,1} } },
	{ H264_MB_TYPE_I_16X16_2_2_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,1}, {5,1}, {6,0} } },
	{ H264_MB_TYPE_I_16X16_3_2_1, 0, 7, { {0,1}, {1,0}, {2,1}, {3,1}, {4,1}, {5,1}, {6,1} } },
	{ H264_MB_TYPE_I_PCM, 0, 2, { {0,1}, {1,1} } },
	{ 0 },
};

static const struct h264_cabac_se_val mb_type_si[] = {
	{ H264_MB_TYPE_SI, 0, 1, { {7,0} } },
	{ 0, mb_type_i,  1, { {7,1} } },
	{ 0 },
};

static const struct h264_cabac_se_val mb_type_p[] = {
	{ H264_MB_TYPE_P_L0_16X16, 0, 3, { {7,0}, {8,0}, {9,0} } },
	{ H264_MB_TYPE_P_8X8, 0, 3, { {7,0}, {8,0}, {9,1} } },
	{ H264_MB_TYPE_P_L0_L0_8X16, 0, 3, { {7,0}, {8,1}, {10,0} } },
	{ H264_MB_TYPE_P_L0_L0_16X8, 0, 3, { {7,0}, {8,1}, {10,1} } },
	{ 0, mb_type_i,  1, { {7,1} } },
	{ 0 },
};

static const struct h264_cabac_se_val mb_type_b[] = {
	{ H264_MB_TYPE_B_DIRECT_16X16, 0, 1, { {7,0} } },
	{ H264_MB_TYPE_B_L0_16X16, 0, 3, { {7,1}, {8,0}, {10,0} } },
	{ H264_MB_TYPE_B_L1_16X16, 0, 3, { {7,1}, {8,0}, {10,1} } },
	{ H264_MB_TYPE_B_BI_16X16, 0, 6, { {7,1}, {8,1}, {9,0}, {10,0}, {10,0}, {10,0} } },
	{ H264_MB_TYPE_B_L0_L0_16X8, 0, 6, { {7,1}, {8,1}, {9,0}, {10,0}, {10,0}, {10,1} } },
	{ H264_MB_TYPE_B_L0_L0_8X16, 0, 6, { {7,1}, {8,1}, {9,0}, {10,0}, {10,1}, {10,0} } },
	{ H264_MB_TYPE_B_L1_L1_16X8, 0, 6, { {7,1}, {8,1}, {9,0}, {10,0}, {10,1}, {10,1} } },
	{ H264_MB_TYPE_B_L1_L1_8X16, 0, 6, { {7,1}, {8,1}, {9,0}, {10,1}, {10,0}, {10,0} } },
	{ H264_MB_TYPE_B_L0_L1_16X8, 0, 6, { {7,1}, {8,1}, {9,0}, {10,1}, {10,0}, {10,1} } },
	{ H264_MB_TYPE_B_L0_L1_8X16, 0, 6, { {7,1}, {8,1}, {9,0}, {10,1}, {10,1}, {10,0} } },
	{ H264_MB_TYPE_B_L1_L0_16X8, 0, 6, { {7,1}, {8,1}, {9,0}, {10,1}, {10,1}, {10,1} } },
	{ H264_MB_TYPE_B_L0_BI_16X8, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,0}, {10,0}, {10,0} } },
	{ H264_MB_TYPE_B_L0_BI_8X16, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,0}, {10,0}, {10,1} } },
	{ H264_MB_TYPE_B_L1_BI_16X8, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,0}, {10,1}, {10,0} } },
	{ H264_MB_TYPE_B_L1_BI_8X16, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,0}, {10,1}, {10,1} } },
	{ H264_MB_TYPE_B_BI_L0_16X8, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,1}, {10,0}, {10,0} } },
	{ H264_MB_TYPE_B_BI_L0_8X16, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,1}, {10,0}, {10,1} } },
	{ H264_MB_TYPE_B_BI_L1_16X8, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,1}, {10,1}, {10,0} } },
	{ H264_MB_TYPE_B_BI_L1_8X16, 0, 7, { {7,1}, {8,1}, {9,1}, {10,0}, {10,1}, {10,1}, {10,1} } },
	{ H264_MB_TYPE_B_BI_BI_16X8, 0, 7, { {7,1}, {8,1}, {9,1}, {10,1}, {10,0}, {10,0}, {10,0} } },
	{ H264_MB_TYPE_B_BI_BI_8X16, 0, 7, { {7,1}, {8,1}, {9,1}, {10,1}, {10,0}, {10,0}, {10,1} } },
	{ 0, mb_type_i, 6, { {7,1}, {8,1}, {9,1}, {10,1}, {10,0}, {10,1} } },
	{ H264_MB_TYPE_B_L1_L0_8X16, 0, 6, { {7,1}, {8,1}, {9,1}, {10,1}, {10,1}, {10,0} } },
	{ H264_MB_TYPE_B_8X8, 0, 6, { {7,1}, {8,1}, {9,1}, {10,1}, {10,1}, {10,1} } },
	{ 0 },
};

static const struct h264_cabac_se_val sub_mb_type_p[] = {
	{ H264_SUB_MB_TYPE_P_L0_8X4, 0, 2, { {0,0}, {1,0} } },
	{ H264_SUB_MB_TYPE_P_L0_4X4, 0, 3, { {0,0}, {1,1}, {2,0} } },
	{ H264_SUB_MB_TYPE_P_L0_4X8, 0, 3, { {0,0}, {1,1}, {2,1} } },
	{ H264_SUB_MB_TYPE_P_L0_8X8, 0, 1, { {0,1} } },
	{ 0 },
};

static const struct h264_cabac_se_val sub_mb_type_b[] = {
	{ H264_SUB_MB_TYPE_B_DIRECT_8X8, 0, 1, { {0,0} } },
	{ H264_SUB_MB_TYPE_B_L0_8X8, 0, 3, { {0,1}, {1,0}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_L1_8X8, 0, 3, { {0,1}, {1,0}, {3,1} } },
	{ H264_SUB_MB_TYPE_B_BI_8X8, 0, 5, { {0,1}, {1,1}, {2,0}, {3,0}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_L0_8X4, 0, 5, { {0,1}, {1,1}, {2,0}, {3,0}, {3,1} } },
	{ H264_SUB_MB_TYPE_B_L0_4X8, 0, 5, { {0,1}, {1,1}, {2,0}, {3,1}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_L1_8X4, 0, 5, { {0,1}, {1,1}, {2,0}, {3,1}, {3,1} } },
	{ H264_SUB_MB_TYPE_B_L1_4X8, 0, 6, { {0,1}, {1,1}, {2,1}, {3,0}, {3,0}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_BI_8X4, 0, 6, { {0,1}, {1,1}, {2,1}, {3,0}, {3,0}, {3,1} } },
	{ H264_SUB_MB_TYPE_B_BI_4X8, 0, 6, { {0,1}, {1,1}, {2,1}, {3,0}, {3,1}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_L0_4X4, 0, 6, { {0,1}, {1,1}, {2,1}, {3,0}, {3,1}, {3,1} } },
	{ H264_SUB_MB_TYPE_B_L1_4X4, 0, 5, { {0,1}, {1,1}, {2,1}, {3,1}, {3,0} } },
	{ H264_SUB_MB_TYPE_B_BI_4X4, 0, 5, { {0,1}, {1,1}, {2,1}, {3,1}, {3,1} } },
	{ 0 },
};

int h264_mb_type(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t slice_type, uint32_t *val) {
	if (!cabac) {
		uint32_t rval;
		int rstart;
		int rend;
		switch (slice_type) {
			case H264_SLICE_TYPE_I:
				rstart = 0;
				rend = 0;
				break;
			case H264_SLICE_TYPE_SI:
				rstart = H264_MB_TYPE_SI_BASE;
				rend = H264_MB_TYPE_SI_END;
				break;
			case H264_SLICE_TYPE_P:
			case H264_SLICE_TYPE_SP:
				rstart = H264_MB_TYPE_P_BASE;
				rend = H264_MB_TYPE_P_SKIP;
				break;
			case H264_SLICE_TYPE_B:
				rstart = H264_MB_TYPE_B_BASE;
				rend = H264_MB_TYPE_B_SKIP;
				break;
			default:
				abort();
		}
		if (str->dir == VS_ENCODE) {
			if (*val >= rstart && *val < rend) {
				rval = *val - rstart;
			} else if (*val < H264_MB_TYPE_I_END) {
				rval = *val + rend - rstart;
			} else {
				fprintf (stderr, "Invalid mb_type for this slice_type\n");
				return 1;
			}
		}
		if (vs_ue(str, &rval))
			return 1;
		if (str->dir == VS_DECODE) {
			if (rval < rend - rstart) {
				*val = rval + rstart;
			} else if (rval < rend - rstart + H264_MB_TYPE_I_END) {
				*val = rval - (rend - rstart);
			} else {
				fprintf (stderr, "Invalid mb_type for this slice_type\n");
				return 1;
			}
		}
	} else {
		int bidx[11];
		uint32_t mbtA = h264_mb_nb(cabac->slice, H264_MB_A, 0)->mb_type;
		uint32_t mbtB = h264_mb_nb(cabac->slice, H264_MB_B, 0)->mb_type;
		int condTermFlagA = mbtA != H264_MB_TYPE_UNAVAIL;
		int condTermFlagB = mbtB != H264_MB_TYPE_UNAVAIL;
		switch (slice_type) {
			case H264_SLICE_TYPE_SI:
				condTermFlagA = condTermFlagA && mbtA != H264_MB_TYPE_SI;
				condTermFlagB = condTermFlagB && mbtB != H264_MB_TYPE_SI;
				bidx[7] = H264_CABAC_CTXIDX_MB_TYPE_SI_PRE + condTermFlagA + condTermFlagB;
				condTermFlagA = condTermFlagA && mbtA != H264_MB_TYPE_I_NXN;
				condTermFlagB = condTermFlagB && mbtB != H264_MB_TYPE_I_NXN;
				bidx[0] = H264_CABAC_CTXIDX_MB_TYPE_I + condTermFlagA + condTermFlagB;
				bidx[1] = H264_CABAC_CTXIDX_TERMINATE;
				bidx[2] = H264_CABAC_CTXIDX_MB_TYPE_I + 3;
				bidx[3] = H264_CABAC_CTXIDX_MB_TYPE_I + 4;
				bidx[4] = H264_CABAC_CTXIDX_MB_TYPE_I + 5;
				bidx[5] = H264_CABAC_CTXIDX_MB_TYPE_I + 6;
				bidx[6] = H264_CABAC_CTXIDX_MB_TYPE_I + 7;
				h264_cabac_se(str, cabac, mb_type_si, bidx, val);
				break;
			case H264_SLICE_TYPE_I:
				condTermFlagA = condTermFlagA && mbtA != H264_MB_TYPE_I_NXN;
				condTermFlagB = condTermFlagB && mbtB != H264_MB_TYPE_I_NXN;
				bidx[0] = H264_CABAC_CTXIDX_MB_TYPE_I + condTermFlagA + condTermFlagB;
				bidx[1] = H264_CABAC_CTXIDX_TERMINATE;
				bidx[2] = H264_CABAC_CTXIDX_MB_TYPE_I + 3;
				bidx[3] = H264_CABAC_CTXIDX_MB_TYPE_I + 4;
				bidx[4] = H264_CABAC_CTXIDX_MB_TYPE_I + 5;
				bidx[5] = H264_CABAC_CTXIDX_MB_TYPE_I + 6;
				bidx[6] = H264_CABAC_CTXIDX_MB_TYPE_I + 7;
				h264_cabac_se(str, cabac, mb_type_i, bidx, val);
				break;
			case H264_SLICE_TYPE_P:
			case H264_SLICE_TYPE_SP:
				bidx[7] = H264_CABAC_CTXIDX_MB_TYPE_P_PRE + 0;
				bidx[8] = H264_CABAC_CTXIDX_MB_TYPE_P_PRE + 1;
				bidx[9] = H264_CABAC_CTXIDX_MB_TYPE_P_PRE + 2;
				bidx[10] = H264_CABAC_CTXIDX_MB_TYPE_P_PRE + 3;
				bidx[0] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 0;
				bidx[1] = H264_CABAC_CTXIDX_TERMINATE;
				bidx[2] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 1;
				bidx[3] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 2;
				bidx[4] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 2;
				bidx[5] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 3;
				bidx[6] = H264_CABAC_CTXIDX_MB_TYPE_P_SUF + 3;
				h264_cabac_se(str, cabac, mb_type_p, bidx, val);
				break;
			case H264_SLICE_TYPE_B:
				condTermFlagA = condTermFlagA && mbtA != H264_MB_TYPE_B_SKIP && mbtA != H264_MB_TYPE_B_DIRECT_16X16;
				condTermFlagB = condTermFlagB && mbtB != H264_MB_TYPE_B_SKIP && mbtB != H264_MB_TYPE_B_DIRECT_16X16;
				bidx[7] = H264_CABAC_CTXIDX_MB_TYPE_B_PRE + condTermFlagA + condTermFlagB;
				bidx[8] = H264_CABAC_CTXIDX_MB_TYPE_B_PRE + 3;
				bidx[9] = H264_CABAC_CTXIDX_MB_TYPE_B_PRE + 4;
				bidx[10] = H264_CABAC_CTXIDX_MB_TYPE_B_PRE + 5;
				bidx[0] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 0;
				bidx[1] = H264_CABAC_CTXIDX_TERMINATE;
				bidx[2] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 1;
				bidx[3] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 2;
				bidx[4] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 2;
				bidx[5] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 3;
				bidx[6] = H264_CABAC_CTXIDX_MB_TYPE_B_SUF + 3;
				h264_cabac_se(str, cabac, mb_type_b, bidx, val);
				break;
		}
	}
	return 0;
}

int h264_sub_mb_type(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t slice_type, uint32_t *val) {
	if (!cabac) {
		int rbase = 0, rend = 0;
		switch (slice_type) {
			case H264_SLICE_TYPE_P:
			case H264_SLICE_TYPE_SP:
				rbase = H264_SUB_MB_TYPE_P_BASE;
				rend = H264_SUB_MB_TYPE_P_END;
				break;
			case H264_SLICE_TYPE_B:
				rbase = H264_SUB_MB_TYPE_B_BASE;
				rend = H264_SUB_MB_TYPE_B_END;
				break;
			default:
				fprintf(stderr, "sub_mb_type requested for invalid slice type\n");
				return 1;
		}
		uint32_t tmp = *val - rbase;
		if (vs_ue(str, &tmp))
			return 1;
		*val = tmp + rbase;
		if (*val < rbase || *val >= rend) {
			fprintf(stderr, "Invalid sub_mb_type for this slice_type\n");
			return 1;
		}
		return 0;
	} else {
		if (slice_type == H264_SLICE_TYPE_P || slice_type == H264_SLICE_TYPE_SP) {
			int bidx[3] = {
				H264_CABAC_CTXIDX_SUB_MB_TYPE_P + 0,
				H264_CABAC_CTXIDX_SUB_MB_TYPE_P + 1,
				H264_CABAC_CTXIDX_SUB_MB_TYPE_P + 2,
			};
			return h264_cabac_se(str, cabac, sub_mb_type_p, bidx, val);
		} else if (slice_type == H264_SLICE_TYPE_B) {
			int bidx[4] = {
				H264_CABAC_CTXIDX_SUB_MB_TYPE_B + 0,
				H264_CABAC_CTXIDX_SUB_MB_TYPE_B + 1,
				H264_CABAC_CTXIDX_SUB_MB_TYPE_B + 2,
				H264_CABAC_CTXIDX_SUB_MB_TYPE_B + 3,
			};
			return h264_cabac_se(str, cabac, sub_mb_type_b, bidx, val);
		} else {
			fprintf(stderr, "sub_mb_type requested for invalid slice type\n");
			return 1;
		}
	}
}

static const int cbplc[48][2] = {
	47, 0,
	31, 16,
	15, 1,
	0, 2,
	23, 4,
	27, 8,
	29, 32,
	30, 3,
	7, 5,
	11, 10,
	13, 12,
	14, 15,
	39, 47,
	43, 7,
	45, 11,
	46, 13,
	16, 14,
	3, 6,
	5, 9,
	10, 31,
	12, 35,
	19, 37,
	21, 42,
	26, 44,
	28, 33,
	35, 34,
	37, 36,
	42, 40,
	44, 39,
	1, 43,
	2, 45,
	4, 46,
	8, 17,
	17, 18,
	18, 20,
	20, 24,
	24, 19,
	6, 21,
	9, 26,
	22, 28,
	25, 23,
	32, 27,
	33, 29,
	34, 30,
	36, 22,
	40, 25,
	38, 38,
	41, 41,
};

static const int cbpl[16][2] = {
	15, 0,
	0, 1,
	7, 2,
	11, 4,
	13, 8,
	14, 3,
	3, 5,
	5, 10,
	10, 12,
	12, 15,
	1, 7,
	2, 11,
	4, 13,
	8, 14,
	6, 6,
	9, 9,
};

int h264_coded_block_pattern(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t mb_type, int has_chroma, uint32_t *val) {
	if (!cabac) {
		uint32_t tmp = -1;
		const int (*tab)[2];
		int maxval;
		int which;
		if (has_chroma) {
			tab = cbplc;
			maxval = 48;
		} else {
			tab = cbpl;
			maxval = 16;
		}
		if (mb_type <= H264_MB_TYPE_SI)
			which = 0;
		else
			which = 1;
		if (str->dir == VS_ENCODE) {
			if (*val >= maxval) {
				fprintf(stderr, "coded_block_pattern too large\n");
				return 1;
			}
			int i;
			for (i = 0; i < maxval; i++) {
				if (tab[i][which] == *val)
					tmp = i;
			}
		}
		if (vs_ue(str, &tmp))
			return 1;
		if (str->dir == VS_DECODE) {
			if (tmp >= maxval) {
				fprintf(stderr, "coded_block_pattern too large\n");
				return 1;
			}
			*val = tab[tmp][which];
		}
		return 0;
	} else {
		int i;
		uint32_t bit[6] = { 0 };
		int ctxIdx;
		const struct h264_macroblock *mbT = h264_mb_nb(cabac->slice, H264_MB_THIS, 0);
		const struct h264_macroblock *mbA;
		const struct h264_macroblock *mbB;
		int idxA;
		int idxB;
		mbA = h264_mb_nb(cabac->slice, H264_MB_A, 0);
		mbB = h264_mb_nb(cabac->slice, H264_MB_B, 0);
		if (str->dir == VS_ENCODE) {
			if (*val >= (has_chroma?48:16)) {
				fprintf(stderr, "coded_block_pattern too large\n");
				return 1;
			}
			bit[0] = *val >> 0 & 1;
			bit[1] = *val >> 1 & 1;
			bit[2] = *val >> 2 & 1;
			bit[3] = *val >> 3 & 1;
			bit[4] = *val >> 4 > 0;
			bit[5] = *val >> 4 > 1;
		}
		for (i = 0; i < 4; i++) {
			mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_8X8, 0, i, &idxA);
			mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_8X8, 0, i, &idxB);
			int condTermFlagA = !(mbA == mbT ? bit[idxA] : mbA->coded_block_pattern >> idxA & 1);
			int condTermFlagB = !(mbB == mbT ? bit[idxB] : mbB->coded_block_pattern >> idxB & 1);
			ctxIdx = H264_CABAC_CTXIDX_CODED_BLOCK_PATTERN_LUMA + condTermFlagA + condTermFlagB * 2;
			if (h264_cabac_decision(str, cabac, ctxIdx, &bit[i])) return 1;
		}
		if (has_chroma) {
			mbA = h264_mb_nb(cabac->slice, H264_MB_A, 0);
			mbB = h264_mb_nb(cabac->slice, H264_MB_B, 0);
			int condTermFlagA = (mbA->coded_block_pattern >> 4) > 0;
			int condTermFlagB = (mbB->coded_block_pattern >> 4) > 0;
			ctxIdx = H264_CABAC_CTXIDX_CODED_BLOCK_PATTERN_CHROMA + condTermFlagA + condTermFlagB * 2;
			if (h264_cabac_decision(str, cabac, ctxIdx, &bit[4])) return 1;
			if (bit[4]) {
				int condTermFlagA = (mbA->coded_block_pattern >> 4) > 1;
				int condTermFlagB = (mbB->coded_block_pattern >> 4) > 1;
				ctxIdx = H264_CABAC_CTXIDX_CODED_BLOCK_PATTERN_CHROMA + condTermFlagA + condTermFlagB * 2 + 4;
				if (h264_cabac_decision(str, cabac, ctxIdx, &bit[5])) return 1;
			}
		}
		if (str->dir == VS_DECODE) {
			*val = bit[0] | bit[1] << 1 | bit[2] << 2 | bit[3] << 3 | bit[4] << (4+bit[5]);
		}
		return 0;
	}
}

int h264_transform_size_8x8_flag(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *binVal) {
	if (!cabac)
		return vs_u(str, binVal, 1);
	int ctxIdxOffset = H264_CABAC_CTXIDX_TRANSFORM_SIZE_8X8_FLAG, ctxIdxInc;
	int condTermFlagA = h264_mb_nb(cabac->slice, H264_MB_A, 0)->transform_size_8x8_flag;
	int condTermFlagB = h264_mb_nb(cabac->slice, H264_MB_B, 0)->transform_size_8x8_flag;
	ctxIdxInc = condTermFlagA + condTermFlagB;
	return h264_cabac_decision(str, cabac, ctxIdxOffset+ctxIdxInc, binVal);
}

int h264_mb_qp_delta(struct bitstream *str, struct h264_cabac_context *cabac, int32_t *val) {
	if (!cabac)
		return vs_se(str, val);
	int ctxIdx[3];
	if (cabac->slice->prev_mb_addr != (uint32_t)-1 && cabac->slice->mbs[cabac->slice->prev_mb_addr].mb_qp_delta)
		ctxIdx[0] = H264_CABAC_CTXIDX_MB_QP_DELTA + 1;
	else
		ctxIdx[0] = H264_CABAC_CTXIDX_MB_QP_DELTA + 0;
	ctxIdx[1] = H264_CABAC_CTXIDX_MB_QP_DELTA + 2;
	ctxIdx[2] = H264_CABAC_CTXIDX_MB_QP_DELTA + 3;
	uint32_t tmp;
	if (str->dir == VS_ENCODE) {
		if (*val > 0) {
			tmp = *val * 2 - 1;
		} else {
			tmp = -*val * 2;
		}
	}
	if (h264_cabac_tu(str, cabac, ctxIdx, 3, -1, &tmp)) return 1;
	if (str->dir == VS_DECODE) {
		if (tmp & 1) {
			*val = (tmp + 1) >> 1;
		} else {
			*val = -(tmp >> 1);
		}
	}
	return 0;
}

int h264_prev_intra_pred_mode_flag(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *val) {
	if (!cabac)
		return vs_u(str, val, 1);
	else
		return h264_cabac_decision(str, cabac, H264_CABAC_CTXIDX_PREV_INTRA_PRED_MODE_FLAG, val);
}

int h264_rem_intra_pred_mode(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *val) {
	if (!cabac)
		return vs_u(str, val, 3);
	uint32_t bit[3];
	int i;
	for (i = 0; i < 3; i++) {
		bit[i] = *val >> i & 1;
		if (h264_cabac_decision(str, cabac, H264_CABAC_CTXIDX_REM_INTRA_PRED_MODE, &bit[i])) return 1;
	}
	if (str->dir == VS_DECODE) {
		*val = bit[0] | bit[1] << 1 | bit[2] << 2;
	}
	return 0;
}

int h264_intra_chroma_pred_mode(struct bitstream *str, struct h264_cabac_context *cabac, uint32_t *val) {
	if (!cabac)
		return vs_ue(str, val);
	int ctxIdx[2];
	int condTermFlagA = h264_mb_nb(cabac->slice, H264_MB_A, 0)->intra_chroma_pred_mode != 0;
	int condTermFlagB = h264_mb_nb(cabac->slice, H264_MB_B, 0)->intra_chroma_pred_mode != 0;
	ctxIdx[0] = H264_CABAC_CTXIDX_INTRA_CHROMA_PRED_MODE + condTermFlagA + condTermFlagB;
	ctxIdx[1] = H264_CABAC_CTXIDX_INTRA_CHROMA_PRED_MODE + 3;
	return h264_cabac_tu(str, cabac, ctxIdx, 2, 3, val);
}

int h264_ref_idx(struct bitstream *str, struct h264_cabac_context *cabac, int idx, int which, int max, uint32_t *val) {
	if (!max)
		return vs_infer(str, val, 0);
	if (!cabac) {
		if (max == 1) {
			uint32_t tmp = 1 - *val;
			if (vs_u(str, val, 1)) return 1;
			*val = 1 - tmp;
			return 0;
		} else {
			return vs_ue(str, val);
		}
	}
	int idxA, idxB;
	const struct h264_macroblock *mbT = h264_mb_nb(cabac->slice, H264_MB_THIS, 0);
	const struct h264_macroblock *mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_8X8, 0, idx, &idxA);
	const struct h264_macroblock *mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_8X8, 0, idx, &idxB);
	int thrA, thrB;
	thrA = (!mbT->mb_field_decoding_flag && mbA->mb_field_decoding_flag);
	thrB = (!mbT->mb_field_decoding_flag && mbB->mb_field_decoding_flag);
	int condTermFlagA = mbA->ref_idx[which][idxA] > thrA;
	int condTermFlagB = mbB->ref_idx[which][idxB] > thrB;
	int ctxIdx[3];
	ctxIdx[0] = H264_CABAC_CTXIDX_REF_IDX + condTermFlagA + 2 * condTermFlagB;
	ctxIdx[1] = H264_CABAC_CTXIDX_REF_IDX + 4;
	ctxIdx[2] = H264_CABAC_CTXIDX_REF_IDX + 5;
	return h264_cabac_tu(str, cabac, ctxIdx, 3, -1, val);
}

int h264_mvd(struct bitstream *str, struct h264_cabac_context *cabac, int idx, int comp, int which, int32_t *val) {
	if (!cabac)
		return vs_se(str, val);
	int baseidx = (comp ? H264_CABAC_CTXIDX_MVD_Y : H264_CABAC_CTXIDX_MVD_X);
	int idxA, idxB;
	const struct h264_macroblock *mbT = h264_mb_nb(cabac->slice, H264_MB_THIS, 0);
	const struct h264_macroblock *mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_4X4, 0, idx, &idxA);
	const struct h264_macroblock *mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_4X4, 0, idx, &idxB);
	int absMvdCompA = abs(mbA->mvd[which][idxA][comp]);
	int absMvdCompB = abs(mbB->mvd[which][idxB][comp]);
	if (comp) {
		if (mbT->mb_field_decoding_flag && !mbA->mb_field_decoding_flag)
			absMvdCompA /= 2;
		if (!mbT->mb_field_decoding_flag && mbA->mb_field_decoding_flag)
			absMvdCompA *= 2;
		if (mbT->mb_field_decoding_flag && !mbB->mb_field_decoding_flag)
			absMvdCompB /= 2;
		if (!mbT->mb_field_decoding_flag && mbB->mb_field_decoding_flag)
			absMvdCompB *= 2;
	}
	int sum = absMvdCompA + absMvdCompB;
	int inc;
	if (sum < 3)
		inc = 0;
	else if (sum <= 32)
		inc = 1;
	else
		inc = 2;
	int ctxIdx[5];
	ctxIdx[0] = baseidx + inc;
	ctxIdx[1] = baseidx + 3;
	ctxIdx[2] = baseidx + 4;
	ctxIdx[3] = baseidx + 5;
	ctxIdx[4] = baseidx + 6;
	return h264_cabac_ueg(str, cabac, ctxIdx, 5, 3, 1, 9, val);
}

int h264_coded_block_flag(struct bitstream *str, struct h264_cabac_context *cabac, int cat, int idx, uint32_t *val) {
	static const int basectx[14] = {
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT0,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT1,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT2,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT3,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT4,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT5,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT6,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT7,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT8,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT9,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT10,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT11,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT12,
		H264_CABAC_CTXIDX_CODED_BLOCK_FLAG_CAT13,
	};
	const struct h264_macroblock *mbT = h264_mb_nb_p(cabac->slice, H264_MB_THIS, 0);
	int inter = h264_is_inter_mb_type(mbT->mb_type);
	int which;
	switch (cat) {
		case H264_CTXBLOCKCAT_LUMA_DC:
		case H264_CTXBLOCKCAT_LUMA_AC:
		case H264_CTXBLOCKCAT_LUMA_4X4:
		case H264_CTXBLOCKCAT_LUMA_8X8:
			which = 0;
			break;
		case H264_CTXBLOCKCAT_CB_DC:
		case H264_CTXBLOCKCAT_CB_AC:
		case H264_CTXBLOCKCAT_CB_4X4:
		case H264_CTXBLOCKCAT_CB_8X8:
			which = 1;
			break;
		case H264_CTXBLOCKCAT_CR_DC:
		case H264_CTXBLOCKCAT_CR_AC:
		case H264_CTXBLOCKCAT_CR_4X4:
		case H264_CTXBLOCKCAT_CR_8X8:
			which = 2;
			break;
		case H264_CTXBLOCKCAT_CHROMA_DC:
			which = idx + 1;
			break;
		case H264_CTXBLOCKCAT_CHROMA_AC:
			which = (idx >> 3) + 1;
			idx &= 7;
			break;
		default:
			abort();
	}
	const struct h264_macroblock *mbA;
	const struct h264_macroblock *mbB;
	int idxA;
	int idxB;
	switch (cat) {
		case H264_CTXBLOCKCAT_LUMA_DC:
		case H264_CTXBLOCKCAT_CB_DC:
		case H264_CTXBLOCKCAT_CR_DC:
		case H264_CTXBLOCKCAT_CHROMA_DC:
			mbA = h264_mb_nb(cabac->slice, H264_MB_A, inter);
			mbB = h264_mb_nb(cabac->slice, H264_MB_B, inter);
			idxA = 16;
			idxB = 16;
			break;
		case H264_CTXBLOCKCAT_LUMA_AC:
		case H264_CTXBLOCKCAT_LUMA_4X4:
		case H264_CTXBLOCKCAT_CB_AC:
		case H264_CTXBLOCKCAT_CB_4X4:
		case H264_CTXBLOCKCAT_CR_AC:
		case H264_CTXBLOCKCAT_CR_4X4:
			mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_4X4, inter, idx, &idxA);
			mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_4X4, inter, idx, &idxB);
			break;
		case H264_CTXBLOCKCAT_LUMA_8X8:
		case H264_CTXBLOCKCAT_CB_8X8:
		case H264_CTXBLOCKCAT_CR_8X8:
			mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_8X8, inter, idx, &idxA);
			mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_8X8, inter, idx, &idxB);
			idxA *= 4;
			idxB *= 4;
			if (!mbA->transform_size_8x8_flag && mbA->mb_type != H264_MB_TYPE_I_PCM && mbA->mb_type != H264_MB_TYPE_UNAVAIL)
				mbA = h264_mb_unavail(1);;
			if (!mbB->transform_size_8x8_flag && mbB->mb_type != H264_MB_TYPE_I_PCM && mbB->mb_type != H264_MB_TYPE_UNAVAIL)
				mbB = h264_mb_unavail(1);;
			break;
		case H264_CTXBLOCKCAT_CHROMA_AC:
			mbA = h264_mb_nb_b(cabac->slice, H264_MB_A, H264_BLOCK_CHROMA, inter, idx, &idxA);
			mbB = h264_mb_nb_b(cabac->slice, H264_MB_B, H264_BLOCK_CHROMA, inter, idx, &idxB);
			break;
		default:
			abort();
	}
	mbA = h264_inter_filter(cabac->slice, mbA, inter);
	mbB = h264_inter_filter(cabac->slice, mbB, inter);
	int condTermFlagA = mbA->coded_block_flag[which][idxA];
	int condTermFlagB = mbB->coded_block_flag[which][idxB];
	int ctxIdx = basectx[cat] + condTermFlagA + condTermFlagB * 2;
	return h264_cabac_decision(str, cabac, ctxIdx, val);
}

int h264_significant_coeff_flag(struct bitstream *str, struct h264_cabac_context *cabac, int field, int cat, int idx, int last, uint32_t *val) {
	static const int basectx[2][2][14] = {
		{
			{
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT0,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT1,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT2,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT3,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT4,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT5,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT6,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT7,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT8,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT9,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT10,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT11,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT12,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FRAME_CAT13,
			},  {
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT0,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT1,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT2,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT3,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT4,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT5,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT6,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT7,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT8,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT9,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT10,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT11,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT12,
				H264_CABAC_CTXIDX_SIGNIFICANT_COEFF_FLAG_FIELD_CAT13,
			},
		}, {
			{
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT0,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT1,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT2,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT3,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT4,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT5,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT6,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT7,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT8,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT9,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT10,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT11,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT12,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FRAME_CAT13,
			},  {
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT0,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT1,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT2,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT3,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT4,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT5,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT6,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT7,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT8,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT9,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT10,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT11,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT12,
				H264_CABAC_CTXIDX_LAST_SIGNIFICANT_COEFF_FLAG_FIELD_CAT13,
			},
		},
	};
	static const int tab8x8[63][3] = {
		{  0,  0, 0 }, /*  0 */
		{  1,  1, 1 }, /*  1 */
		{  2,  1, 1 }, /*  2 */
		{  3,  2, 1 }, /*  3 */
		{  4,  2, 1 }, /*  4 */
		{  5,  3, 1 }, /*  5 */
		{  5,  3, 1 }, /*  6 */
		{  4,  4, 1 }, /*  7 */
		{  4,  5, 1 }, /*  8 */
		{  3,  6, 1 }, /*  9 */
		{  3,  7, 1 }, /* 10 */
		{  4,  7, 1 }, /* 11 */
		{  4,  7, 1 }, /* 12 */
		{  4,  8, 1 }, /* 13 */
		{  5,  4, 1 }, /* 14 */
		{  5,  5, 1 }, /* 15 */
		{  4,  6, 2 }, /* 16 */
		{  4,  9, 2 }, /* 17 */
		{  4, 10, 2 }, /* 18 */
		{  4, 10, 2 }, /* 19 */
		{  3,  8, 2 }, /* 20 */
		{  3, 11, 2 }, /* 21 */
		{  6, 12, 2 }, /* 22 */
		{  7, 11, 2 }, /* 23 */
		{  7,  9, 2 }, /* 24 */
		{  7,  9, 2 }, /* 25 */
		{  8, 10, 2 }, /* 26 */
		{  9, 10, 2 }, /* 27 */
		{ 10,  8, 2 }, /* 28 */
		{  9, 11, 2 }, /* 29 */
		{  8, 12, 2 }, /* 30 */
		{  7, 11, 2 }, /* 31 */
		{  7,  9, 3 }, /* 32 */
		{  6,  9, 3 }, /* 33 */
		{ 11, 10, 3 }, /* 34 */
		{ 12, 10, 3 }, /* 35 */
		{ 13,  8, 3 }, /* 36 */
		{ 11, 11, 3 }, /* 37 */
		{  6, 12, 3 }, /* 38 */
		{  7, 11, 3 }, /* 39 */
		{  8,  9, 4 }, /* 40 */
		{  9,  9, 4 }, /* 41 */
		{ 14, 10, 4 }, /* 42 */
		{ 10, 10, 4 }, /* 43 */
		{  9,  8, 4 }, /* 44 */
		{  8, 13, 4 }, /* 45 */
		{  6, 13, 4 }, /* 46 */
		{ 11,  9, 4 }, /* 47 */
		{ 12,  9, 5 }, /* 48 */
		{ 13, 10, 5 }, /* 49 */
		{ 11, 10, 5 }, /* 50 */
		{  6,  8, 5 }, /* 51 */
		{  9, 13, 6 }, /* 52 */
		{ 14, 13, 6 }, /* 53 */
		{ 10,  9, 6 }, /* 54 */
		{  9,  9, 6 }, /* 55 */
		{ 11, 10, 7 }, /* 56 */
		{ 12, 10, 7 }, /* 57 */
		{ 13, 14, 7 }, /* 58 */
		{ 11, 14, 7 }, /* 59 */
		{ 14, 14, 8 }, /* 60 */
		{ 10, 14, 8 }, /* 61 */
		{ 12, 14, 8 }, /* 62 */
	};
	int ctxInc;
	switch (cat) {
		case H264_CTXBLOCKCAT_LUMA_DC:
		case H264_CTXBLOCKCAT_LUMA_AC:
		case H264_CTXBLOCKCAT_LUMA_4X4:
		case H264_CTXBLOCKCAT_CB_DC:
		case H264_CTXBLOCKCAT_CB_AC:
		case H264_CTXBLOCKCAT_CB_4X4:
		case H264_CTXBLOCKCAT_CR_DC:
		case H264_CTXBLOCKCAT_CR_AC:
		case H264_CTXBLOCKCAT_CR_4X4:
		case H264_CTXBLOCKCAT_CHROMA_AC:
			ctxInc = idx;
			break;
		case H264_CTXBLOCKCAT_CHROMA_DC:
			ctxInc = idx / cabac->slice->chroma_array_type;
			if (ctxInc > 2)
				ctxInc = 2;
			break;
		case H264_CTXBLOCKCAT_LUMA_8X8:
		case H264_CTXBLOCKCAT_CB_8X8:
		case H264_CTXBLOCKCAT_CR_8X8:
			if (last)
				ctxInc = tab8x8[idx][2];
			else
				ctxInc = tab8x8[idx][field];
			break;
		default:
			abort();
	}
	int ctxIdx = basectx[last][field][cat] + ctxInc;
	return h264_cabac_decision(str, cabac, ctxIdx, val);
}

int h264_coeff_abs_level_minus1(struct bitstream *str, struct h264_cabac_context *cabac, int cat, int num1, int numgt1, int32_t *val) {
	static const int basectx[14] = {
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT0,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT1,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT2,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT3,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT4,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT5,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT6,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT7,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT8,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT9,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT10,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT11,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT12,
		H264_CABAC_CTXIDX_COEFF_ABS_LEVEL_MINUS1_PRE_CAT13,
	};
	int ctxIdx[2];
	ctxIdx[0] = basectx[cat] + (numgt1 ? 0 : (num1 >= 4 ? 4 : num1 + 1));
	int clamp = (cat == H264_CTXBLOCKCAT_CHROMA_DC ? 3 : 4);
	ctxIdx[1] = basectx[cat] + 5 + (numgt1 > clamp ? clamp : numgt1);
	return h264_cabac_ueg(str, cabac, ctxIdx, 2, 0, 0, 14, val);
}
