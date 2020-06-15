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

#include "pgraph.h"
#include "pgraph_mthd.h"
#include "pgraph_class.h"
#include "nva.h"

namespace hwtest {
namespace pgraph {

namespace {

class MthdSurfFormat : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x01010003;
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		return val == 1 || val == 0x01000000 || val == 0x01010000 || val == 0x01010001;
	}
	void emulate_mthd() override {
		if (chipset.card_type < 4) {
			int which = extr(exp.ctx_switch_a, 16, 2);
			int f = 1;
			if (extr(val, 0, 1))
				f = 0;
			if (!extr(val, 16, 1))
				f = 2;
			if (!extr(val, 24, 1))
				f = 3;
			insrt(exp.surf_format, 4*which, 3, f | 4);
		} else {
			int which = cls & 3;
			int fmt = 0;
			if (val == 1) {
				if (which == 3 && chipset.card_type >= 0x20)
					fmt = 2;
				else
					fmt = 7;
			} else if (val == 0x01010000) {
				fmt = 1;
			} else if (val == 0x01000000) {
				if (which == 3 && chipset.card_type >= 0x20)
					fmt = 1;
				else
					fmt = 2;
			} else if (val == 0x01010001) {
				if (which == 3 && chipset.card_type >= 0x20)
					fmt = 1;
				else
					fmt = 6;
			}
			pgraph_set_surf_format(&exp, which, fmt);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurf2DFormat : public SingleMthdTest {
	bool is_valid_val() override {
		if (cls & 0xff00)
			return val != 0 && val < 0xf;
		return val != 0 && val < 0xe;
	}
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		int fmt = 0;
		switch (val & 0xf) {
			case 0x01:
				fmt = 1;
				break;
			case 0x02:
				fmt = 2;
				break;
			case 0x03:
				fmt = 3;
				break;
			case 0x04:
				fmt = 5;
				break;
			case 0x05:
				fmt = 6;
				break;
			case 0x06:
				fmt = 7;
				break;
			case 0x07:
				fmt = 0xb;
				break;
			case 0x08:
				fmt = 0x9;
				break;
			case 0x09:
				fmt = 0xa;
				break;
			case 0x0a:
				fmt = 0xc;
				break;
			case 0x0b:
				fmt = 0xd;
				break;
			case 0x0c:
				if (cls & 0xff00)
					fmt = 0x13;
				break;
			case 0x0d:
				if (cls & 0xff00)
					fmt = 0x14;
				break;
			case 0x0e:
				if (cls & 0xff00)
					fmt = 0x15;
				break;
		}
		pgraph_set_surf_format(&exp, 0, fmt);
		pgraph_set_surf_format(&exp, 1, fmt);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurf2DFormatAlt : public MthdSurf2DFormat {
	// yeah... no idea.
	bool supported() override { return chipset.card_type >= 0x20; }
	using MthdSurf2DFormat::MthdSurf2DFormat;
};

class MthdSurfSwzFormat : public SingleMthdTest {
	bool is_valid_val() override {
		int fmt = extr(val, 0, 16);
		if (fmt == 0 || fmt > (cls & 0xff00 ? 0xe : 0xd))
			return false;
		int swzx = extr(val, 16, 8);
		int swzy = extr(val, 24, 8);
		if (swzx > 0xb)
			return false;
		if (swzy > 0xb)
			return false;
		if (swzx == 0)
			return false;
		if (swzy == 0 && cls == 0x52)
			return false;
		return true;
	}
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x0f0f000f;
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		int fmt = 0;
		switch (val & 0xf) {
			case 0x01:
				fmt = 1;
				break;
			case 0x02:
				fmt = 2;
				break;
			case 0x03:
				fmt = 3;
				break;
			case 0x04:
				fmt = 5;
				break;
			case 0x05:
				fmt = 6;
				break;
			case 0x06:
				fmt = 7;
				break;
			case 0x07:
				fmt = 0xb;
				break;
			case 0x08:
				fmt = 0x9;
				break;
			case 0x09:
				fmt = 0xa;
				break;
			case 0x0a:
				fmt = 0xc;
				break;
			case 0x0b:
				fmt = 0xd;
				break;
			case 0x0c:
				if (cls & 0xff00)
					fmt = 0x13;
				break;
			case 0x0d:
				if (cls & 0xff00)
					fmt = 0x14;
				break;
			case 0x0e:
				if (cls & 0xff00)
					fmt = 0x15;
				break;
		}
		int swzx = extr(val, 16, 4);
		int swzy = extr(val, 24, 4);
		if (cls == 0x9e && chipset.card_type < 0x20) {
			fmt = 0;
		}
		pgraph_set_surf_format(&exp, 5, fmt);
		insrt(exp.surf_swizzle[1], 16, 4, swzx);
		insrt(exp.surf_swizzle[1], 24, 4, swzy);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdClipSize : public SingleMthdTest {
	void emulate_mthd() override {
		int vidx = chipset.card_type < 0x10 ? 13 : 9;
		exp.vtx_xy[vidx][0] = extr(val, 0, 16);
		exp.vtx_xy[vidx][1] = extr(val, 16, 16);
		int xcstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[vidx][0], 0);
		int ycstat = nv04_pgraph_clip_status(&exp, exp.vtx_xy[vidx][1], 1);
		pgraph_set_xy_d(&exp, 0, 1, 1, false, false, false, xcstat);
		pgraph_set_xy_d(&exp, 1, 1, 1, false, false, false, ycstat);
		exp.uclip_min[0][0] = 0;
		exp.uclip_min[0][1] = 0;
		exp.uclip_max[0][0] = exp.vtx_xy[vidx][0];
		exp.uclip_max[0][1] = exp.vtx_xy[vidx][1];
		insrt(exp.valid[0], 28, 1, 0);
		insrt(exp.valid[0], 30, 1, 0);
		insrt(exp.xy_misc_1[0], 4, 2, 0);
		insrt(exp.xy_misc_1[0], 12, 1, 0);
		insrt(exp.xy_misc_1[0], 16, 1, 0);
		insrt(exp.xy_misc_1[0], 20, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurf2DPitch : public SingleMthdTest {
	int kind;
	bool is_valid_val() override {
		if (!extr(val, 0, 16))
			return false;
		if (!extr(val, 16, 16))
			return false;
		if (kind == SURF_NV4)
			if (chipset.card_type >= 0x20 && extr(exp.debug_d, 6, 1))
				return !(val & 0xe03fe03f);
			else
				return !(val & 0xe01fe01f);
		else {
			return !(val & 0x3f003f);
		}
	}
	void emulate_mthd() override {
		uint32_t pitch_mask = pgraph_pitch_mask(&chipset);
		if (chipset.card_type == 0x40)
			exp.src2d_pitch = val & 0xffff;
		else
			exp.surf_pitch[SURF_SRC] = val & pitch_mask & 0xffff;
		exp.surf_pitch[SURF_DST] = val >> 16 & pitch_mask & 0xffff;
		exp.valid[0] |= 4;
		insrt(exp.ctx_valid, 8, 1, !extr(exp.nsource, 1, 1));
		insrt(exp.ctx_valid, 9, 1, !extr(exp.nsource, 1, 1));
	}
public:
	MthdSurf2DPitch(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int flags)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), kind(flags) {}
};

}

void MthdDmaSurf::emulate_mthd_pre() {
	if (which == 1 && chipset.card_type == 0x40 && cls == 0x59) {
		// huh.
		exp.src2d_dma = 0;
	}
}

void MthdDmaSurf::emulate_mthd() {
	uint32_t offset_mask = pgraph_offset_mask(&chipset);
	uint32_t base = (pobj[2] & ~0xfff) | extr(pobj[0], 20, 12);
	uint32_t limit = pobj[1];
	uint32_t dcls = extr(pobj[0], 0, 12);
	bool bad = true;
	if (dcls == 0x30 || dcls == 0x3d)
		bad = false;
	if (dcls == 3 && (cls == 0x38 || cls == 0x88))
		bad = false;
	if (dcls == 2 && cls != 0x59 && which == 1)
		bad = false;
	if (extr(exp.debug_d, 23, 1) && bad) {
		nv04_pgraph_blowup(&exp, 0x2);
	}
	bool prot_err = false;
	int ekind = kind;
	if (chipset.card_type >= 0x20 && kind == SURF_NV4 && extr(exp.debug_d, 6, 1))
		ekind = SURF_NV10;
	if (extr(base, 0, 4 + ekind))
		prot_err = true;
	int mem = extr(pobj[0], 16, 2);
	if (mem > (chipset.card_type < 0x20 ? 0 : 1))
		prot_err = true;
	if (chipset.chipset >= 5 && ((bad && chipset.card_type < 0x10) || dcls == 0x30))
		prot_err = false;
	if (prot_err)
		nv04_pgraph_blowup(&exp, 4);
	int bit = which;
	if (which == 8)
		bit = 18;
	insrt(exp.ctx_valid, bit, 1, dcls != 0x30 && !(bad && extr(exp.debug_d, 23, 1)));
	if (which == 1 && chipset.card_type == 0x40) {
		// Handled in _pre.
		if (cls != 0x59) {
			exp.src2d_dma = val & 0xffffff;
		}
	} else {
		exp.surf_limit[which] = (limit & offset_mask) | ((chipset.card_type < 0x20 || chipset.chipset == 0x34) ? 0xf : 0x3f) | (dcls == 0x30) << 31;
		insrt(exp.surf_base[which], 0, 30, base & offset_mask);
		if (chipset.chipset == 0x34) {
			insrt(exp.surf_base[which], 30, 1, extr(pobj[0], 16, 2) == 1);
			insrt(exp.surf_base[which], 31, 1, dcls == 0x30);
		}
		if (exp.chipset.card_type >= 0x20 && !extr(exp.surf_unk880, 20, 1)) {
			insrt(exp.surf_unk888, which & ~1, 1, 0);
			insrt(exp.surf_unk888, which | 1, 1, 0);
			if (nv04_pgraph_is_nv25p(&exp.chipset)) {
				insrt(exp.surf_unk89c, (which & ~1) * 4, 4, 0);
				insrt(exp.surf_unk89c, (which | 1) * 4, 4, 0);
			}
		}
	}
}

bool MthdSurfPitch::is_valid_val() {
	if (chipset.card_type < 4)
		return true;
	return val && !(val & ~0x1ff0);
}

void MthdSurfPitch::emulate_mthd() {
	int surf;
	if (chipset.card_type < 4) {
		surf = extr(exp.ctx_switch_a, 16, 2);
	} else {
		surf = which;
		insrt(exp.valid[0], 2, 1, 1);
		int bit = which + 8;
		if (which >= 6)
			bit = which + 7;
		insrt(exp.ctx_valid, bit, 1, !extr(exp.nsource, 1, 1));
	}
	if (surf == 1 && chipset.card_type >= 0x40) {
		exp.src2d_pitch = val & 0xffff;
	} else {
		exp.surf_pitch[surf] = val & pgraph_pitch_mask(&chipset);
	}
}

bool MthdSurfOffset::is_valid_val() {
	if (chipset.card_type < 4)
		return true;
	int ekind = kind;
	if (chipset.card_type < 0x20 && cls == 0x88)
		ekind = SURF_NV4;
	if (chipset.card_type >= 0x20 && ekind == SURF_NV4 && extr(exp.debug_d, 6, 1))
		ekind = SURF_NV10;
	if (ekind == SURF_NV3)
		return !(val & 0xf);
	else if (ekind == SURF_NV4)
		return !(val & 0x1f);
	else
		return !(val & 0x3f);
}

void MthdSurfOffset::emulate_mthd() {
	int surf;
	if (chipset.card_type < 4) {
		surf = extr(exp.ctx_switch_a, 16, 2);
	} else {
		surf = which;
		insrt(exp.valid[0], 3, 1, 1);
	}
	if (surf == 1 && chipset.card_type >= 0x40) {
		exp.src2d_offset = val;
	} else {
		exp.surf_offset[surf] = val & pgraph_offset_mask(&chipset);
		if (exp.chipset.card_type >= 0x20 && !extr(exp.surf_unk880, 20, 1)) {
			insrt(exp.surf_unk888, which & ~1, 1, 0);
			insrt(exp.surf_unk888, which | 1, 1, 0);
			if (nv04_pgraph_is_nv25p(&exp.chipset)) {
				insrt(exp.surf_unk89c, (which & ~1) * 4, 4, 0);
				insrt(exp.surf_unk89c, (which | 1) * 4, 4, 0);
			}
		}
	}
}

bool MthdSurf3DPitch::is_valid_val() {
	if (!extr(val, 0, 16))
		return false;
	if (!extr(val, 16, 16))
		return false;
	if (kind == SURF_NV4)
		if (chipset.card_type >= 0x20 && extr(exp.debug_d, 6, 1))
			return !(val & 0xe03fe03f);
		else
			return !(val & 0xe01fe01f);
	else {
		return !(val & 0x3f003f);
	}
}

void MthdSurf3DPitch::emulate_mthd() {
	uint32_t pitch_mask = pgraph_pitch_mask(&chipset);
	if (chipset.card_type < 0x40 || !exp.nsource) {
		exp.surf_pitch[2] = val & pitch_mask & 0xffff;
		exp.surf_pitch[3] = val >> 16 & pitch_mask & 0xffff;
	} else {
		exp.surf_pitch[2] = val & pitch_mask;
	}
	exp.valid[0] |= 4;
	insrt(exp.ctx_valid, 10, 1, !extr(exp.nsource, 1, 1));
	insrt(exp.ctx_valid, 11, 1, !extr(exp.nsource, 1, 1));
}

void MthdSurf3DFormat::adjust_orig_mthd() {
	if (orig.chipset.card_type >= 0x20)
		orig.surf_unk800 = 0;
	if (rnd() & 1) {
		val &= 0x0f0f737f;
		if (rnd() & 3)
			insrt(val, 8, 4, rnd() & 1 ? 2 : 1);
		if (rnd() & 1)
			insrt(val, 12, 4, 0);
		if (rnd() & 3)
			insrt(val, 16, 4, rnd() % 0xd);
		if (rnd() & 3)
			insrt(val, 20, 4, rnd() % 0xd);
		if (rnd() & 1)
			val ^= 1 << (rnd() & 0x1f);
	}
}

bool MthdSurf3DFormat::is_valid_val() {
	int fmt = extr(val, 0, 4);
	int zfmt = extr(val, 4, 4);
	int mode = extr(val, 8, 4);
	bool considered_new = false;
	if ((cls == 0x53 || cls == 0x93 || cls == 0x96) && pgraph_grobj_get_new(&exp))
		considered_new = true;
	if ((cls == 0x56 || cls == 0x85) && pgraph_grobj_get_new(&exp) && chipset.card_type >= 0x20)
		considered_new = true;
	if (cls == 0x98 || cls == 0x99)
		considered_new = true;
	if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE) {
		fmt = extr(val, 0, 5);
		zfmt = extr(val, 5, 3);
	}
	if (fmt == 0)
		return false;
	if (pgraph_3d_class(&exp) < PGRAPH_3D_CELSIUS) {
		if (fmt > 8)
			return false;
	} else if (pgraph_3d_class(&exp) < PGRAPH_3D_RANKINE) {
		if (fmt > 0xa)
			return false;
	} else {
		if (fmt > 0x10)
			return false;
	}
	unsigned max_swz;
	if (pgraph_3d_class(&exp) < PGRAPH_3D_KELVIN) {
		if (considered_new) {
			if (zfmt > 1)
				return false;
		} else {
			if (zfmt)
				return false;
		}
		if (zfmt == 1 && mode == 2 && cls != 0x53 && cls != 0x93)
			return false;
		if (zfmt == 1 && (fmt == 9 || fmt == 0xa))
			return false;
		if (cls == 0x99) {
			int v = extr(val, 12, 4);
			if (v != 0 && v != 3)
				return false;
		} else {
			if (extr(val, 12, 4))
				return false;
		}
		max_swz = 0xb;
	} else {
		if (zfmt != 1 && zfmt != 2)
			return false;
		int v = extr(val, 12, 4);
		if (cls == 0x97) {
			if (v > 2)
				return false;
		} else if (pgraph_3d_class(&exp) < PGRAPH_3D_CURIE) {
			if (v > 4)
				return false;
		} else {
			if (v > 5)
				return false;
		}
		if (v && mode == 2)
			return false;
		if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE) {
			if (v == 1 || v == 2)
				return false;
		}
		int expz;
		if (fmt < 4)
			expz = 1;
		else if (fmt < 9)
			expz = 2;
		else if (fmt >= 0xe)
			expz = 2;
		else if (fmt >= 0xb && pgraph_3d_class(&exp) >= PGRAPH_3D_CURIE)
			expz = 2;
		else
			expz = 0;
		if (mode == 2 && zfmt != expz && expz != 0)
			return false;
		if (pgraph_3d_class(&exp) >= PGRAPH_3D_CURIE) {
			if ((fmt == 0xb || fmt == 0xc || fmt == 0xd) && zfmt == 1)
				return false;
		}
		if (!extr(val, 16, 8) && extr(val, 24, 8))
			return false;
		if (cls != 0x97 && (fmt == 6 || fmt == 7))
			return false;
		max_swz = 0xc;
	}
	if (mode == 0 || mode > 2)
		return false;
	if (extr(val, 16, 8) > max_swz)
		return false;
	if (extr(val, 24, 8) > max_swz)
		return false;
	return true;
}

void MthdSurf3DFormat::emulate_mthd() {
	int fmt = 0;
	int sfmt = val & 0xf;
	if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
		sfmt = val & 0x1f;
	switch (sfmt) {
		case 1:
			fmt = 0x2;
			break;
		case 2:
			fmt = 0x3;
			break;
		case 3:
			fmt = 0x5;
			break;
		case 4:
			fmt = 0x7;
			break;
		case 5:
			fmt = 0xb;
			break;
		case 6:
			if (pgraph_3d_class(&exp) < PGRAPH_3D_RANKINE)
				fmt = 0x9;
			break;
		case 7:
			if (pgraph_3d_class(&exp) < PGRAPH_3D_RANKINE)
				fmt = 0xa;
			break;
		case 8:
			fmt = 0xc;
			break;
		case 9:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_CELSIUS)
				fmt = 0x1;
			break;
		case 0xa:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_CELSIUS)
				fmt = 0x6;
			break;
		case 0xb:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x10;
			break;
		case 0xc:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x11;
			break;
		case 0xd:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x12;
			break;
		case 0xe:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x13;
			break;
		case 0xf:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x14;
			break;
		case 0x10:
			if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE)
				fmt = 0x15;
			break;
	}
	pgraph_set_surf_format(&exp, 2, fmt);
	insrt(exp.surf_type, 0, 2, extr(val, 8, 2));
	if (chipset.card_type < 0x20) {
		if (nv04_pgraph_is_nv11p(&chipset))
			insrt(exp.surf_type, 4, 1, extr(val, 4, 1));
		if (nv04_pgraph_is_nv17p(&chipset))
			insrt(exp.surf_type, 5, 1, extr(val, 12, 1));
	} else {
		if (nv04_pgraph_is_nv25p(&chipset))
			insrt(exp.surf_type, 4, 3, extr(val, 12, 3));
		else
			insrt(exp.surf_type, 4, 2, extr(val, 12, 2));
		if (pgraph_3d_class(&exp) >= PGRAPH_3D_RANKINE) {
			int zfmt = extr(val, 5, 3);
			if (zfmt > 2)
				zfmt = 0;
			pgraph_set_surf_format(&exp, 3, zfmt);
		} else if (pgraph_3d_class(&exp) >= PGRAPH_3D_KELVIN) {
			int zfmt = extr(val, 4, 4);
			if (zfmt > 2)
				zfmt = 0;
			pgraph_set_surf_format(&exp, 3, zfmt);
		} else {
			int zfmt = extr(val, 4, 1) ? 1 : 2;
			if (sfmt == 1 || sfmt == 2 || sfmt == 3 || sfmt == 9 || sfmt == 0xa)
				zfmt = 1;
			if (!fmt)
				zfmt = 0;
			pgraph_set_surf_format(&exp, 3, zfmt);
		}
	}
	insrt(exp.surf_swizzle[0], 16, 4, extr(val, 16, 4));
	insrt(exp.surf_swizzle[0], 24, 4, extr(val, 24, 4));
}

std::vector<SingleMthdTest *> Surf::mthds() {
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaSurf(opt, rnd(), "dma_surf", 2, cls, 0x184, cls & 3, SURF_NV3),
		new MthdMissing(opt, rnd(), "missing", -1, cls, 0x200, 2),
		new MthdSurfFormat(opt, rnd(), "format", 3, cls, 0x300),
		new MthdSurfPitch(opt, rnd(), "pitch", 4, cls, 0x308, cls & 3),
		new MthdSurfOffset(opt, rnd(), "offset", 5, cls, 0x30c, cls & 3, SURF_NV3),
	};
}

std::vector<SingleMthdTest *> Surf2D::mthds() {
	int kind = cls == 0x42 ? SURF_NV4 : SURF_NV10;
	std::vector<SingleMthdTest *> res = {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaSurf(opt, rnd(), "dma_surf_src", 2, cls, 0x184, SURF_SRC, kind),
		new MthdDmaSurf(opt, rnd(), "dma_surf_dst", 3, cls, 0x188, SURF_DST, kind),
		new MthdSurf2DFormatAlt(opt, rnd(), "format_alt", 4, cls, 0x200),
		new MthdMissing(opt, rnd(), "missing", -1, cls, 0x200, 0x40),
		new MthdSurf2DFormat(opt, rnd(), "format", 4, cls, 0x300),
		new MthdSurf2DPitch(opt, rnd(), "pitch", 5, cls, 0x304, kind),
		new MthdSurfOffset(opt, rnd(), "src_offset", 6, cls, 0x308, SURF_SRC, kind),
		new MthdSurfOffset(opt, rnd(), "dst_offset", 7, cls, 0x30c, SURF_DST, kind),
	};
	if (cls != 0x42) {
		res.insert(res.begin(), {
			new MthdSync(opt, rnd(), "sync", -1, cls, 0x108),
		});
	}
	return res;
}

std::vector<SingleMthdTest *> SurfSwz::mthds() {
	int kind = cls == 0x52 ? SURF_NV4 : SURF_NV10;
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaSurf(opt, rnd(), "dma_surf_swz", 2, cls, 0x184, SURF_SWZ, kind),
		new MthdMissing(opt, rnd(), "missing", -1, cls, 0x200, 0x40),
		new MthdSurfSwzFormat(opt, rnd(), "format", 3, cls, 0x300),
		new MthdSurfOffset(opt, rnd(), "offset", 4, cls, 0x304, SURF_SWZ, kind),
	};
}

std::vector<SingleMthdTest *> Surf3D::mthds() {
	int kind = cls == 0x53 ? SURF_NV4 : SURF_NV10;
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaSurf(opt, rnd(), "dma_surf_color", 2, cls, 0x184, SURF_COLOR_A, kind),
		new MthdDmaSurf(opt, rnd(), "dma_surf_zeta", 3, cls, 0x188, SURF_ZETA, kind),
		new MthdClipHv(opt, rnd(), "clip_h", 4, cls, 0x2f8, 0, 0),
		new MthdClipHv(opt, rnd(), "clip_v", 5, cls, 0x2fc, 0, 1),
		new MthdSurf3DFormat(opt, rnd(), "format", 6, cls, 0x300, false),
		new MthdClipSize(opt, rnd(), "clip_size", 7, cls, 0x304),
		new MthdSurf3DPitch(opt, rnd(), "pitch", 8, cls, 0x308, kind),
		new MthdSurfOffset(opt, rnd(), "color_offset", 9, cls, 0x30c, SURF_COLOR_A, kind),
		new MthdSurfOffset(opt, rnd(), "zeta_offset", 10, cls, 0x310, SURF_ZETA, kind),
	};
}

}
}
