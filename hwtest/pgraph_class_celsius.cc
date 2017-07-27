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
#include "pgraph_mthd.h"
#include "pgraph_class.h"
#include "nva.h"

namespace hwtest {
namespace pgraph {

static void adjust_orig_icmd(struct pgraph_state *state) {
	if (state->celsius_pipe_begin_end == 3 || state->celsius_pipe_begin_end == 0xa)
		insrt(state->celsius_pipe_vtx_state, 28, 3, 0);
}

class MthdDmaVtx : public SingleMthdTest {
	bool takes_dma() override { return true; }
	void emulate_mthd() override {
		uint32_t rval = val & 0xffff;
		int dcls = extr(pobj[0], 0, 12);
		if (dcls == 0x30)
			rval = 0;
		bool bad = false;
		if (dcls != 0x30 && dcls != 0x3d && dcls != 2)
			bad = true;
		if (bad && extr(exp.debug[3], 23, 1))
			nv04_pgraph_blowup(&exp, 2);
		bool prot_err = false;
		if (dcls != 0x30) {
			if (extr(pobj[1], 0, 8) != 0xff)
				prot_err = true;
			if (extr(pobj[0], 20, 8))
				prot_err = true;
			if (!extr(pobj[0], 12, 2)) {
				exp.intr |= 0x400;
				exp.fifo_enable = 0;
			}
		}
#if 0
		// XXX: figure this out
		if (dcls == 0x30 && nv04_pgraph_is_nv15p(&chipset)) {
			exp.intr |= 0x400;
			exp.fifo_enable = 0;
		}
#endif
		if (prot_err)
			nv04_pgraph_blowup(&exp, 4);
		insrt(exp.celsius_dma, 0, 16, rval);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdDmaState : public SingleMthdTest {
	bool takes_dma() override { return true; }
	void emulate_mthd() override {
		uint32_t rval = val & 0xffff;
		int dcls = extr(pobj[0], 0, 12);
		if (dcls == 0x30)
			rval = 0;
		bool bad = false;
		if (dcls != 0x30 && dcls != 0x3d && dcls != 3)
			bad = true;
		if (bad && extr(exp.debug[3], 23, 1))
			nv04_pgraph_blowup(&exp, 2);
		insrt(exp.celsius_dma, 16, 16, rval);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexOffset : public SingleMthdTest {
	void adjust_orig_mthd() override {
		val &= ~0x7f;
		if (rnd() & 1) {
			val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & 0x7f);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_offset[idx] = val;
			pgraph_celsius_icmd(&exp, idx, val, true);
		}
		insrt(exp.valid[1], idx ? 22 : 14, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexFormat : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 0, 2, 1);
			if (rnd() & 3)
				insrt(val, 3, 2, 1);
			if (rnd() & 3)
				insrt(val, 5, 2, 1);
			if (!(rnd() & 3))
				insrt(val, 7, 5, 0x19 + rnd() % 4);
			if (rnd() & 1)
				insrt(val, 20, 4, extr(val, 16, 4));
			if (rnd() & 3)
				insrt(val, 24, 3, 3);
			if (rnd() & 3)
				insrt(val, 28, 3, 3);
			if (rnd() & 1) {
				if (rnd() & 3)
					insrt(val, 2, 1, 0);
				if (rnd() & 3)
					insrt(val, 12, 4, 1);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		int mips = extr(val, 12, 4);
		int su = extr(val, 16, 4);
		int sv = extr(val, 20, 4);
		int fmt = extr(val, 7, 5);
		if (!extr(val, 0, 2) || extr(val, 0, 2) == 3)
			return false;
		if (extr(val, 2, 1)) {
			if (su != sv)
				return false;
			if (su >= 0xa && (fmt == 6 || fmt == 7 || fmt == 0xb || fmt == 0xe || fmt == 0xf))
				return false;
			if (su >= 0xb)
				return false;
		}
		if (!extr(val, 3, 2) || extr(val, 3, 2) == 3)
			return false;
		if (!extr(val, 5, 2) || extr(val, 5, 2) == 3)
			return false;
		if (fmt == 0xd)
			return false;
		if (fmt >= 0x1d)
			return false;
		if (mips > 0xc || mips == 0)
			return false;
		if (fmt >= 0x19) {
			if (cls != 0x99)
				return false;
		}
		if (fmt >= 0x10) {
			if (extr(val, 2, 1))
				return false;
			if (extr(val, 24, 3) != 3)
				return false;
			if (extr(val, 28, 3) != 3)
				return false;
			if (mips != 1)
				return false;
		}
		if (su > 0xb || sv > 0xb)
			return false;
		if (extr(val, 24, 3) < 1 || extr(val, 24, 3) > 5)
			return false;
		if (extr(val, 28, 3) < 1 || extr(val, 28, 3) > 5)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t rval = val & 0xffffffd6;
		int mips = extr(val, 12, 4);
		int su = extr(val, 16, 4);
		int sv = extr(val, 20, 4);
		if (mips > su && mips > sv)
			mips = (su > sv ? su : sv) + 1;
		insrt(rval, 12, 4, mips);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_format[idx] = rval | (exp.bundle_tex_format[idx] & 8);
			pgraph_celsius_icmd(&exp, 4 + idx, exp.bundle_tex_format[idx], true);
		}
		insrt(exp.valid[1], idx ? 21 : 15, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexControl : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 31, 1))
			return false;
		if (extr(val, 5, 1))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_control[idx] = val & 0x7fffffff;
			insrt(exp.celsius_xf_misc_b, idx ? 14 : 0, 1, extr(val, 30, 1));
			pgraph_celsius_icmd(&exp, 6 + idx, exp.bundle_tex_control[idx], true);
			if (!extr(exp.debug[3], 28, 1)) {
				exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
				exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexPitch : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= ~0xffff;
			if (!(rnd() & 3)) {
				val &= 0xe00f0000;
			}
			if (!(rnd() & 3))
				val = 0;
			if (rnd() & 1) {
				val ^= 1 << (rnd() & 0x1f);
				val ^= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & 0xffff) && !!(val & 0x3ff80000);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_pitch[idx] = val & 0xffff0000;
			pgraph_celsius_icmd(&exp, 8 + idx, exp.bundle_tex_pitch[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexUnk238 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_unk238[idx] = val;
			pgraph_celsius_icmd(&exp, 0xa + idx, exp.bundle_tex_unk238[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexRect : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				val &= ~0xf000f800;
			}
			if (rnd() & 1) {
				if (rnd() & 1) {
					val &= ~0xffff;
				} else {
					val &= ~0xffff0000;
				}
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 16, 1))
			return false;
		if (!extr(val, 0, 16) || extr(val, 0, 16) >= 0x800)
			return false;
		if (!extr(val, 17, 15) || extr(val, 17, 15) >= 0x800)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_rect[idx] = val & 0x07ff07ff;
			pgraph_celsius_icmd(&exp, 0xc + idx, exp.bundle_tex_rect[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexFilter : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 13, 11, 0);
			if (rnd() & 3)
				insrt(val, 24, 4, 1);
			if (rnd() & 3)
				insrt(val, 28, 4, 1);
			if (rnd() & 1) {
				val ^= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val ^= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 13, 11))
			return false;
		if (extr(val, 24, 4) < 1 || extr(val, 24, 4) > 6)
			return false;
		if (extr(val, 28, 4) < 1 || extr(val, 28, 4) > 2)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_filter[idx] = val & 0x77001fff;
			pgraph_celsius_icmd(&exp, 0xe + idx, exp.bundle_tex_filter[idx], true);
		}
		insrt(exp.valid[1], idx ? 23 : 16, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexPalette : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= ~0x3c;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & 0x3e);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_palette[idx] = val & 0xffffffc1;
			pgraph_celsius_icmd(&exp, 2 + idx, exp.bundle_tex_palette[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcInAlpha : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				insrt(val, 0, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 8, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 16, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 24, 4, 0xd);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		for (int j = 0; j < 4; j++) {
			int reg = extr(val, 8 * j, 4);
			if (reg == 6 || reg == 7 || reg == 0xa || reg == 0xb || reg >= 0xe)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_in_alpha[idx] = val;
			pgraph_celsius_icmd(&exp, 0x10 + idx, exp.bundle_rc_in_alpha[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcInColor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				insrt(val, 0, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 8, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 16, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 24, 4, 0xd);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		for (int j = 0; j < 4; j++) {
			int reg = extr(val, 8 * j, 4);
			if (reg == 6 || reg == 7 || reg == 0xa || reg == 0xb || reg >= 0xe)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_in_color[idx] = val;
			pgraph_celsius_icmd(&exp, 0x12 + idx, exp.bundle_rc_in_color[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcFactor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			if (idx == 0)
				exp.bundle_rc_factor_0[0] = val;
			else
				exp.bundle_rc_factor_1[0] = val;
			pgraph_celsius_icmd(&exp, 0x14 + idx, val, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcOutAlpha : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				insrt(val, 18, 14, 0);
			}
			if (rnd() & 1) {
				insrt(val, 28, 2, 1);
			}
			if (rnd() & 1) {
				insrt(val, 12, 2, 0);
			}
			if (rnd() & 1) {
				insrt(val, 0, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 4, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 8, 4, 0xd);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		int op = extr(val, 15, 3);
		if (op == 5 || op == 7)
			return false;
		if (extr(val, 12, 2))
			return false;
		for (int j = 0; j < 3; j++) {
			int reg = extr(val, 4 * j, 4);
			if (reg != 0 && reg != 4 && reg != 5 && reg != 8 && reg != 9 && reg != 0xc && reg != 0xd)
				return false;
		}
		if (extr(val, 18, 14))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_out_alpha[idx] = val & 0x3cfff;
			pgraph_celsius_icmd(&exp, 0x16 + idx, exp.bundle_rc_out_alpha[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcOutColor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				insrt(val, 18, 14, 0);
			}
			if (rnd() & 1) {
				insrt(val, 28, 2, 1);
			}
			if (rnd() & 1) {
				insrt(val, 12, 2, 0);
			}
			if (rnd() & 1) {
				insrt(val, 0, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 4, 4, 0xd);
			}
			if (rnd() & 1) {
				insrt(val, 8, 4, 0xd);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		int op = extr(val, 15, 3);
		if (op == 5 || op == 7)
			return false;
		if (!idx) {
			if (extr(val, 27, 3))
				return false;
		} else {
			int cnt = extr(val, 28, 2);
			if (cnt == 0 || cnt == 3)
				return false;
		}
		for (int j = 0; j < 3; j++) {
			int reg = extr(val, 4 * j, 4);
			if (reg != 0 && reg != 4 && reg != 5 && reg != 8 && reg != 9 && reg != 0xc && reg != 0xd)
				return false;
		}
		if (extr(val, 18, 9))
			return false;
		if (extr(val, 30, 2))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t rval;
		if (!idx) {
			rval = val & 0x3ffff;
		} else {
			rval = val & 0x3803ffff;
		}
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_out_color[idx] = rval;
			pgraph_celsius_icmd(&exp, 0x18 + idx, exp.bundle_rc_out_color[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcFinalA : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x3f3f3f3f;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (val & 0xc0c0c0c0)
			return false;
		for (int j = 0; j < 4; j++) {
			int reg = extr(val, 8 * j, 4);
			if (reg == 6 || reg == 7 || reg == 0xa || reg == 0xb)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_final_a = val & 0x3f3f3f3f;
			pgraph_celsius_icmd(&exp, 0x1a, exp.bundle_rc_final_a, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusRcFinalB : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x3f3f3fe0;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (val & 0xc0c0c01f)
			return false;
		for (int j = 1; j < 4; j++) {
			int reg = extr(val, 8 * j, 4);
			if (reg == 6 || reg == 7 || reg == 0xa || reg == 0xb || reg >= 0xe)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_final_b = val & 0x3f3f3fe0;
			pgraph_celsius_icmd(&exp, 0x1b, exp.bundle_rc_final_b, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusConfig : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x31111101;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (val & 0xceeeeefe)
			return false;
		if (extr(val, 28, 2) && cls != 0x99)
			return false;
		if (extr(val, 8, 1) && !extr(val, 16, 1))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		insrt(exp.valid[1], 17, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_config_a, 25, 1, extr(val, 0, 1));
			insrt(exp.bundle_config_a, 23, 1, extr(val, 16, 1));
			insrt(exp.bundle_config_b, 2, 1, extr(val, 24, 1));
			insrt(exp.bundle_config_b, 6, 1, extr(val, 20, 1));
			insrt(exp.bundle_config_b, 10, 4, extr(val, 8, 4));
			if (nv04_pgraph_is_nv17p(&chipset))
				insrt(exp.bundle_config_b, 14, 2, extr(val, 28, 2));
			pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, false);
			pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, false);
			insrt(exp.bundle_raster, 29, 1, extr(val, 12, 1));
			pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLightModel : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x00010007;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
	}
	bool is_valid_val() override {
		return !(val & 0xfffefff8);
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.celsius_xf_misc_a, 18, 1, extr(val, 2, 1));
			insrt(exp.celsius_xf_misc_a, 19, 1, extr(val, 0, 1));
			insrt(exp.celsius_xf_misc_a, 20, 1, extr(val, 1, 1));
			insrt(exp.celsius_xf_misc_b, 28, 1, extr(val, 16, 1));
			if (!extr(exp.debug[3], 28, 1)) {
				exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
				exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLightMaterial : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		if (val & ~0xf) {
			warn(1);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_a, 21, 4, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFogMode : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
				if (rnd() & 1) {
					val |= (rnd() & 1 ? 0x800 : 0x2600);
				}
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		uint32_t rv = 0;
		if (val == 0x800) {
			rv = 1;
		} else if (val == 0x801) {
			rv = 3;
		} else if (val == 0x802) {
			rv = 5;
		} else if (val == 0x803) {
			rv = 7;
		} else if (val == 0x2601) {
			rv = 0;
		} else {
			err |= 1;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_config_c, 0, 3, rv);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFogCoord : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 3)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_a, 16, 2, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFogEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_b, 8, 1, val);
				pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, true);
				insrt(exp.celsius_xf_misc_b, 31, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFogColor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (extr(exp.celsius_config_c, 8, 1)) {
			warn(4);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_fog_color, 0, 8, extr(val, 16, 8));
				insrt(exp.bundle_fog_color, 8, 8, extr(val, 8, 8));
				insrt(exp.bundle_fog_color, 16, 8, extr(val, 0, 8));
				insrt(exp.bundle_fog_color, 24, 8, extr(val, 24, 8));
				pgraph_celsius_icmd(&exp, 0x23, exp.bundle_fog_color, true);
			}
		}
		insrt(exp.valid[1], 13, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexColorKey : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (extr(exp.celsius_config_c, 8, 1)) {
			warn(4);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_tex_color_key[idx] = val;
				pgraph_celsius_icmd(&exp, 0x28 + idx, exp.bundle_tex_color_key[idx], true);
			}
		}
		if (idx == 0)
			insrt(exp.valid[1], 12, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusClipRectMode : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool is_valid_val() override {
		return val < 2;
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.celsius_config_c, 4, 1, val);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusClipRectHoriz : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			val *= 0x00010001;
		}
		if (rnd() & 1) {
			val &= 0x0fff0fff;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool is_valid_val() override {
		return !(val & 0xf000f000) && extrs(val, 0, 12) <= extrs(val, 16, 12);
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = idx; i < 8; i++) {
				exp.celsius_clip_rect_horiz[i] = val & 0x0fff0fff;
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusClipRectVert : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			val *= 0x00010001;
		}
		if (rnd() & 1) {
			val &= 0x0fff0fff;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool is_valid_val() override {
		return !(val & 0xf000f000) && extrs(val, 0, 12) <= extrs(val, 16, 12);
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = idx; i < 8; i++) {
				exp.celsius_clip_rect_vert[i] = val & 0x0fff0fff;
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusAlphaFuncEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 12, 1, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusBlendFuncEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_blend, 3, 1, val);
				pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusCullFaceEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 28, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusDepthTestEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 14, 1, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusDitherEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 22, 1, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLightingEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_b, 29, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPointParamsEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_b, 9, 1, val);
				pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, true);
				insrt(exp.celsius_xf_misc_a, 25, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPointSmoothEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 9, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLineSmoothEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 10, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonSmoothEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 11, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusWeightEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_a, 27, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusStencilEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_stencil_a, 0, 1, val);
				pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonOffsetPointEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 6, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonOffsetLineEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 7, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonOffsetFillEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 8, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusAlphaFuncFunc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 0x200;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val >= 0x200 && val < 0x208) {
		} else {
			err |= 1;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 8, 4, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusAlphaFuncRef : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xff;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val & ~0xff)
			err |= 2;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 0, 8, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusBlendFunc : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= (rnd() & 1 ? 0x8000 : 0x300);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		uint32_t rv = 0;
		switch (val) {
			case 0:
				rv = 0;
				break;
			case 1:
				rv = 1;
				break;
			case 0x300:
				rv = 2;
				break;
			case 0x301:
				rv = 3;
				break;
			case 0x302:
				rv = 4;
				break;
			case 0x303:
				rv = 5;
				break;
			case 0x304:
				rv = 6;
				break;
			case 0x305:
				rv = 7;
				break;
			case 0x306:
				rv = 8;
				break;
			case 0x307:
				rv = 9;
				break;
			case 0x308:
				rv = 0xa;
				break;
			case 0x8001:
				rv = 0xc;
				break;
			case 0x8002:
				rv = 0xd;
				break;
			case 0x8003:
				rv = 0xe;
				break;
			case 0x8004:
				rv = 0xf;
				break;
			default:
				err |= 1;
				break;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_blend, 4 + which * 4, 4, rv);
				pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			}
		}
	}
public:
	MthdCelsiusBlendFunc(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusBlendColor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (extr(exp.celsius_config_c, 8, 1)) {
			warn(4);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_blend_color = val;
				pgraph_celsius_icmd(&exp, 0x21, exp.bundle_blend_color, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusBlendEquation : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= (rnd() & 1 ? 0x8000 : 0x300);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		uint32_t rv = 0;
		switch (val) {
			case 0x8006:
				rv = 0x2;
				break;
			case 0x8007:
				rv = 0x3;
				break;
			case 0x8008:
				rv = 0x4;
				break;
			case 0x800a:
				rv = 0x0;
				break;
			case 0x800b:
				rv = 0x1;
				break;
			default:
				err |= 1;
				break;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_blend, 0, 3, rv);
				pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusDepthFunc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 0x200;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val >= 0x200 && val < 0x208) {
		} else {
			err |= 1;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 16, 4, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusColorMask : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x1010101;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val & ~0x1010101)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 29, 1, extr(val, 0, 1));
				insrt(exp.bundle_config_a, 28, 1, extr(val, 8, 1));
				insrt(exp.bundle_config_a, 27, 1, extr(val, 16, 1));
				insrt(exp.bundle_config_a, 26, 1, extr(val, 24, 1));
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusDepthWriteEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_a, 24, 1, val);
				pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusStencilVal : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xff;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (which == 0 && val & ~0xff)
			err |= 2;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_stencil_a, 8 + 8 * which, 8, val);
				pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_a, true);
			}
		}
	}
public:
	MthdCelsiusStencilVal(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusStencilFunc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 0x200;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val >= 0x200 && val < 0x208) {
		} else {
			err |= 1;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_stencil_a, 4, 4, val);
				pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_a, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusStencilOp : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xff0f;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= (rnd() & 1 ? 0x1500 : rnd() & 1 ? 0x8500 : 0x1e00);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		uint32_t rv = 0;
		switch (val) {
			case 0:
				rv = 2;
				break;
			case 0x150a:
				rv = 6;
				break;
			case 0x1e00:
				rv = 1;
				break;
			case 0x1e01:
				rv = 3;
				break;
			case 0x1e02:
				rv = 4;
				break;
			case 0x1e03:
				rv = 5;
				break;
			case 0x8507:
				rv = 7;
				break;
			case 0x8508:
				rv = 8;
				break;
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_stencil_b, 4 * which, 4, rv);
				pgraph_celsius_icmd(&exp, 0x1e, exp.bundle_stencil_b, true);
			}
		}
	}
public:
	MthdCelsiusStencilOp(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusShadeMode : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xf0ff;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 0x1d00;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		switch (val) {
			case 0x1d00:
				break;
			case 0x1d01:
				break;
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_b, 7, 1, val);
				pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLineWidth : public SingleMthdTest {
	void adjust_orig_mthd() override {
		val = sext(val, rnd() & 0x1f);
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (val >= 0x200)
			err |= 2;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				uint32_t rv = val & 0x1ff;
				if (chipset.chipset == 0x10)
					rv &= 0xff;
				insrt(exp.bundle_raster, 12, 9, rv);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonOffsetFactor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_polygon_offset_factor = val;
				pgraph_celsius_icmd(&exp, 0x24, exp.bundle_polygon_offset_factor, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonOffsetUnits : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_polygon_offset_units = val;
				pgraph_celsius_icmd(&exp, 0x25, exp.bundle_polygon_offset_units, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPolygonMode : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xff0f;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 0x1b00;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		uint32_t rv = 0;
		switch (val) {
			case 0x1b00:
				rv = 1;
				break;
			case 0x1b01:
				rv = 2;
				break;
			case 0x1b02:
				rv = 0;
				break;
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, which * 2, 2, rv);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
public:
	MthdCelsiusPolygonMode(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusDepthRangeNear : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_depth_range_near = val;
				pgraph_celsius_icmd(&exp, 0x26, exp.bundle_depth_range_near, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusDepthRangeFar : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_depth_range_far = val;
				pgraph_celsius_icmd(&exp, 0x27, exp.bundle_depth_range_far, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusCullFace : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xff0f;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 0x400;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		uint32_t rv = 0;
		switch (val) {
			case 0x404:
				rv = 1;
				break;
			case 0x405:
				rv = 2;
				break;
			case 0x408:
				rv = 3;
				break;
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 21, 2, rv);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFrontFace : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xff0f;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 0x900;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		switch (val) {
			case 0x900:
				break;
			case 0x901:
				break;
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 23, 1, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusNormalizeEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_b, 30, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusSpecularEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_b, 5, 1, val);
				pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusLightEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool is_valid_val() override {
		if (val & ~0xffff)
			return false;
		bool off = false;
		for (int i = 0; i < 8; i++) {
			int mode = extr(val, i * 2, 2);
			if (off && mode != 0)
				return false;
			if (mode == 0)
				off = true;
		}
		return true;
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_a, 0, 16, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexGenMode : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1)
				val &= 0xff0f;
		} else if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= (rnd() & 1 ? 0x8550 : rnd() & 1 ? 0x8510 : 0x2400);
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		uint32_t rv = 0;
		switch (val) {
			case 0:
				rv = 0;
				break;
			case 0x2400:
				rv = 1;
				break;
			case 0x2401:
				rv = 2;
				break;
			case 0x8511:
				if (which < 3) {
					rv = 4;
					break;
				}
			case 0x8512:
				if (which < 3) {
					rv = 5;
					break;
				}
			case 0x855f:
				if (which < 3) {
					rv = 6;
					break;
				}
			case 0x2402:
				if (which < 2) {
					rv = 3;
					break;
				}
			default:
				err |= 1;
		}
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_b, 3 + idx * 14 + which * 3, which == 3 ? 2 : 3, rv);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
public:
	MthdCelsiusTexGenMode(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusTexMatrixEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_b, 1 + idx * 14, 1, val);
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTlMode : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (val & ~7)
			err |= 1;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.celsius_xf_misc_a, 28, 1, extr(val, 0, 1));
				insrt(exp.celsius_xf_misc_b, 2, 1, extr(val, 1, 1));
				insrt(exp.celsius_xf_misc_b, 16, 1, extr(val, 2, 1));
				if (!extr(exp.debug[3], 28, 1)) {
					exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
					exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
				}
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusPointSize : public SingleMthdTest {
	void adjust_orig_mthd() override {
		val = sext(val, rnd() & 0x1f);
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (val >= 0x200)
			err |= 2;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.bundle_point_size = val & 0x1ff;
				pgraph_celsius_icmd(&exp, 0x2a, exp.bundle_point_size, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusUnk3f0 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
		}
		if (rnd() & 1) {
			val |= 1 << (rnd() & 0x1f);
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 4;
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 26, 2, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusFlatshadeFirst : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
		}
		if (rnd() & 1) {
			val |= 1 << (rnd() & 0x1f);
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 2;
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_config_b, 0, 1, val);
				pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusMatrix : public SingleMthdTest {
	int which;
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[idx & 3] = val;
				if ((idx & 3) == 3) {
					for (int i = 0; i < 4; i++)
						exp.celsius_pipe_xfrm[which * 4 + (idx >> 2)][i] = exp.celsius_pipe_junk[i];
				}
			}
		}
	}
public:
	MthdCelsiusMatrix(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusXfrm : public SingleMthdTest {
	int which;
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[idx] = val;
				if (idx == 3) {
					for (int i = 0; i < 4; i++)
						exp.celsius_pipe_xfrm[which][i] = exp.celsius_pipe_junk[i];
				}
			}
		}
	}
public:
	MthdCelsiusXfrm(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusXfrm3 : public SingleMthdTest {
	int which;
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[idx] = val;
				if (idx == 2) {
					for (int i = 0; i < 4; i++)
						exp.celsius_pipe_xfrm[which][i] = exp.celsius_pipe_junk[i];
				}
			}
		}
	}
public:
	MthdCelsiusXfrm3(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusXfrmFree : public SingleMthdTest {
	int which;
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[idx] = val;
			if (idx == 3) {
				for (int i = 0; i < 4; i++)
					exp.celsius_pipe_xfrm[which][i] = exp.celsius_pipe_junk[i];
			}
		}
	}
public:
	MthdCelsiusXfrmFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusLightV : public SingleMthdTest {
	int which;
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[idx] = val;
				if (idx == 2) {
					for (int i = 0; i < 3; i++)
						exp.celsius_pipe_light_v[which][i] = pgraph_celsius_convert_light_v(exp.celsius_pipe_junk[i]);
				}
			}
		}
	}
public:
	MthdCelsiusLightV(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusLightVFree : public SingleMthdTest {
	int which;
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[idx] = val;
			if (idx == 2) {
				for (int i = 0; i < 3; i++)
					exp.celsius_pipe_light_v[which][i] = pgraph_celsius_convert_light_v(exp.celsius_pipe_junk[i]);
			}
		}
	}
public:
	MthdCelsiusLightVFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusLightSAFree : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[0] = val;
			exp.celsius_pipe_light_sa[which] = pgraph_celsius_convert_light_sx(val);
		}
	}
public:
	MthdCelsiusLightSAFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSB : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[0] = val;
				exp.celsius_pipe_light_sb[which] = pgraph_celsius_convert_light_sx(val);
			}
		}
	}
public:
	MthdCelsiusLightSB(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSBFree : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[0] = val;
			exp.celsius_pipe_light_sb[which] = pgraph_celsius_convert_light_sx(val);
		}
	}
public:
	MthdCelsiusLightSBFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSC : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[0] = val;
				exp.celsius_pipe_light_sc[which] = pgraph_celsius_convert_light_sx(val);
			}
		}
	}
public:
	MthdCelsiusLightSC(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSCFree : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[0] = val;
			exp.celsius_pipe_light_sc[which] = pgraph_celsius_convert_light_sx(val);
		}
	}
public:
	MthdCelsiusLightSCFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSD : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_junk[0] = val;
				exp.celsius_pipe_light_sd[which] = pgraph_celsius_convert_light_sx(val);
			}
		}
	}
public:
	MthdCelsiusLightSD(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusLightSDFree : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 23, 8, rnd() & 1 ? 0 : 0xff);
			if (rnd() & 1) {
				val &= 0xfff0000f;
			}
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[0] = val;
			exp.celsius_pipe_light_sd[which] = pgraph_celsius_convert_light_sx(val);
		}
	}
public:
	MthdCelsiusLightSDFree(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusVtxAttrFloat : public SingleMthdTest {
	int which;
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			if (which == 1 || which == 3 || which == 4) {
				if (idx < 1 && chipset.chipset != 0x10)
					exp.celsius_pipe_vtx[which * 4 + 1] = 0;
				if (idx < 2) {
					exp.celsius_pipe_vtx[which * 4 + 2] = 0;
					exp.celsius_pipe_vtx[which * 4 + 3] = 0x3f800000;
				}
			}
			exp.celsius_pipe_vtx[which == 7 ? 11 : which * 4 + idx] = val;
		}
	}
public:
	MthdCelsiusVtxAttrFloat(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusVtxAttrUByte : public SingleMthdTest {
	int which;
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 4; i++) {
				uint32_t v = extr(val, i * 8, 8);
				exp.celsius_pipe_vtx[which * 4 + i] = pgraph_idx_ubyte_to_float(v);
			}
		}
	}
public:
	MthdCelsiusVtxAttrUByte(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusVtxAttrShort : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, rnd() & 1 ? 0 : 16, 16, rnd() & 1 ? 0xffff : 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 2; i++) {
				int16_t v = extrs(val, i * 16, 16);
				exp.celsius_pipe_vtx[which * 4 + idx * 2 + i] = pgraph_idx_short_to_float(&exp, v);
			}
			if (idx == 0) {
				exp.celsius_pipe_vtx[which * 4 + 2] = 0;
				exp.celsius_pipe_vtx[which * 4 + 3] = 0x3f800000;
			}
		}
	}
public:
	MthdCelsiusVtxAttrShort(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusVtxAttrNShort : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, rnd() & 1 ? 0 : 16, 16, rnd() & 1 ? 0xffff : 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 2; i++) {
				int16_t v = extrs(val, i * 16, 16);
				if (idx == 0 || i == 0)
					exp.celsius_pipe_vtx[which * 4 + idx * 2 + i] = pgraph_idx_nshort_to_float(v);
			}
		}
	}
public:
	MthdCelsiusVtxAttrNShort(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which(which) {}
};

class MthdCelsiusEdgeFlag : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_edge_flag = val & 1;
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusUnkcf0 : public SingleMthdTest {
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusUnkcf4 : public SingleMthdTest {
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusXfNop : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[2] = val;
			exp.celsius_pipe_junk[3] = val;
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusXfSync : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_junk[2] = val;
			exp.celsius_pipe_junk[3] = val;
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusVtxbufOffset : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x0ffffffc;
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		return !(val & ~0x0ffffffc);
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtxbuf_offset[idx] = val & 0x0ffffffc;
			insrt(exp.celsius_pipe_vtx_state, 8, 5, 0);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusVtxbufFormat : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x0100fc77;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
			if (rnd() & 3)
				insrt(val, 0, 3, 6);
			if (rnd() & 3)
				insrt(val, 4, 3, idx < 6 ? 3 : 1);
		}
	}
	bool can_warn() override {
		return true;
	}
	bool is_valid_val() override {
		if (idx == 0) {
			int comp = extr(val, 4, 3);
			if (!extr(val, 24, 1)) {
				if (comp == 4)
					return false;
			} else {
				if (comp == 2)
					return false;
			}
		}
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		int comp = extr(val, 4, 4);
		if (idx == 0) {
			if (extr(val, 24, 8) > 1)
				err |= 1;
			if (comp != 2 && comp != 3 && comp != 4)
				err |= 1;
		} else if (idx == 1 || idx == 2) {
			if (comp != 0 && comp != 3 && comp != 4)
				err |= 1;
		} else if (idx == 3 || idx == 4) {
			if (comp > 4)
				err |= 1;
		} else if (idx == 5) {
			if (comp != 0 && comp != 3)
				err |= 1;
		} else if (idx == 6 || idx == 7) {
			if (comp != 0 && comp != 1)
				err |= 1;
		}
		int fmt = extr(val, 0, 4);
		if (idx == 1 || idx == 2) {
			if (fmt != 0 && fmt != 2 && fmt != 4)
				err |= 1;
		} else {
			if (fmt != 1 && fmt != 2)
				err |= 1;
		}
		if (val & 0xff000000 && idx != 0)
			err |= 2;
		if (val & 0x00ff0300)
			err |= 2;
		if (extr(exp.celsius_config_c, 8, 1) && (cls == 0x56 || cls == 0x85))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				exp.celsius_pipe_vtxbuf_format[idx] = pgraph_celsius_fixup_vtxbuf_format(&exp, idx, val);
				insrt(exp.celsius_pipe_vtx_state, 8, 5, 0);
				for (int i = 0; i < 8; i++) {
					insrt(exp.celsius_pipe_vtxbuf_format[i], 24, 1, extr(exp.celsius_pipe_vtxbuf_format[0], 24, 1));
				}
				if (chipset.chipset == 0x10) {
					if (extr(exp.celsius_pipe_vtxbuf_format[1], 0, 3) == 0)
						insrt(exp.celsius_pipe_vtxbuf_format[2], 2, 1, 0);
					else
						insrt(exp.celsius_pipe_vtxbuf_format[2], 2, 1, 1);
				}
				int last = 7;
				while (last > 0 && extr(exp.celsius_pipe_vtxbuf_format[last], 4, 3) == 0)
					last--;
				insrt(exp.celsius_pipe_vtx_state, 0, 5, last);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusBeginEnd : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		if (orig.celsius_pipe_begin_end == 3 || orig.celsius_pipe_begin_end == 0xa) {
			insrt(orig.celsius_pipe_vtx_state, 28, 3, 0);
		}
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		uint32_t err = 0;
		if (val > 0xa)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1) && (val & 0xf) != 0)
			err |= 4;
		warn(err);
		if (!err && !extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_begin_end = val & 0xf;
			insrt(exp.celsius_config_c, 8, 1, (val & 0xf) != 0);
			insrt(exp.celsius_pipe_vtx_state, 28, 3, 0);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusOldUnk3f8 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
		}
		if (rnd() & 1) {
			val |= 1 << (rnd() & 0x1f);
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 3)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_raster, 30, 2, val);
				pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusColorLogicOpEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
		}
		if (rnd() & 1) {
			val |= 1 << (rnd() & 0x1f);
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val > 1)
			err |= 1;
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_blend, 16, 1, val);
				pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusColorLogicOpOp : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			if (rnd() & 1) {
				val &= 0xf;
			}
			if (rnd() & 1) {
				val |= 0x1500;
			}
			if (rnd() & 1) {
				val |= 1 << (rnd() & 0x1f);
				if (rnd() & 1) {
					val |= 1 << (rnd() & 0x1f);
				}
			}
		}
		adjust_orig_icmd(&orig);
	}
	bool can_warn() override {
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t err = 0;
		if (val >= 0x1500 && val < 0x1510) {
		} else {
			err |= 1;
		}
		if (extr(exp.celsius_config_c, 8, 1))
			err |= 4;
		if (err) {
			warn(err);
		} else {
			if (!extr(exp.nsource, 1, 1)) {
				insrt(exp.bundle_blend, 12, 4, val);
				pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdDmaClipid : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool takes_dma() override { return true; }
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t offset_mask = pgraph_offset_mask(&chipset) | 0xf;
		uint32_t base = (pobj[2] & ~0xfff) | extr(pobj[0], 20, 12);
		uint32_t limit = pobj[1];
		uint32_t dcls = extr(pobj[0], 0, 12);
		exp.bundle_surf_limit_clipid = (limit & offset_mask) | (dcls != 0x30) << 31;
		exp.bundle_surf_base_clipid = base & offset_mask;
		pgraph_celsius_icmd(&exp, 0x31, exp.bundle_surf_base_clipid, false);
		pgraph_celsius_icmd(&exp, 0x32, exp.bundle_surf_limit_clipid, true);
		bool bad = true;
		if (dcls == 0x30 || dcls == 0x3d)
			bad = false;
		if (dcls == 3 && (cls == 0x38 || cls == 0x88))
			bad = false;
		if (extr(exp.debug[3], 23, 1) && bad) {
			nv04_pgraph_blowup(&exp, 0x2);
		}
		bool prot_err = false;
		if (extr(base, 0, 6))
			prot_err = true;
		if (extr(pobj[0], 16, 2))
			prot_err = true;
		if (dcls == 0x30)
			prot_err = false;
		if (prot_err)
			nv04_pgraph_blowup(&exp, 4);
		insrt(exp.ctx_valid, 28, 1, dcls != 0x30 && !(bad && extr(exp.debug[3], 23, 1)));
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdDmaZcull : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool takes_dma() override { return true; }
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t offset_mask = 0x3fffffff;
		uint32_t base = (pobj[2] & ~0xfff) | extr(pobj[0], 20, 12);
		uint32_t limit = pobj[1];
		uint32_t dcls = extr(pobj[0], 0, 12);
		exp.bundle_surf_limit_zcull = (limit & offset_mask) | (dcls != 0x30) << 31;
		exp.bundle_surf_base_zcull = base & offset_mask;
		pgraph_celsius_icmd(&exp, 0x2d, exp.bundle_surf_base_zcull, false);
		pgraph_celsius_icmd(&exp, 0x2e, exp.bundle_surf_limit_zcull, true);
		bool bad = true;
		if (dcls == 0x30 || dcls == 0x3d)
			bad = false;
		if (dcls == 3 && (cls == 0x38 || cls == 0x88))
			bad = false;
		if (extr(exp.debug[3], 23, 1) && bad) {
			nv04_pgraph_blowup(&exp, 0x2);
		}
		bool prot_err = false;
		if (extr(base, 0, 6))
			prot_err = true;
		if (extr(pobj[0], 16, 2))
			prot_err = true;
		if (dcls == 0x30)
			prot_err = false;
		if (prot_err)
			nv04_pgraph_blowup(&exp, 4);
		insrt(exp.ctx_valid, 29, 1, dcls != 0x30 && !(bad && extr(exp.debug[3], 23, 1)));
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurfPitchClipid : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xff80;
			val ^= 1 << (rnd() & 0x1f);
		}
		if (!(rnd() & 3)) {
			val = (rnd() & 1) << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0xff80) && !!val;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_surf_pitch_clipid = val & 0xffff;
			pgraph_celsius_icmd(&exp, 0x34, exp.bundle_surf_pitch_clipid, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurfOffsetClipid : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffffffc0;
			val ^= 1 << (rnd() & 0x1f);
		}
		if (!(rnd() & 3)) {
			val = (rnd() & 1) << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0xffffffc0);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_surf_offset_clipid = val & 0x07ffffff;
			pgraph_celsius_icmd(&exp, 0x33, exp.bundle_surf_offset_clipid, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurfPitchZcull : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xff80;
			val ^= 1 << (rnd() & 0x1f);
		}
		if (!(rnd() & 3)) {
			val = (rnd() & 1) << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0xff80) && !!val;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_surf_pitch_zcull = val & 0xffff;
			pgraph_celsius_icmd(&exp, 0x30, exp.bundle_surf_pitch_zcull, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdSurfOffsetZcull : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffffffc0;
			val ^= 1 << (rnd() & 0x1f);
		}
		if (!(rnd() & 3)) {
			val = (rnd() & 1) << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0xffffffc0);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_surf_offset_zcull = val & 0x3fffffff;
			pgraph_celsius_icmd(&exp, 0x2f, exp.bundle_surf_offset_zcull, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdInvalidateZcull : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 4 || val == 6;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		// XXX completely broken
		skip = true;
		if (!extr(exp.nsource, 1, 1)) {
			if (val & 1) {
				exp.zcull_unka00[0] = 0;
				exp.zcull_unka00[1] = 0;
			} else {
				insrt(exp.zcull_unka00[0], 0, 13, 0x1000);
				insrt(exp.zcull_unka00[1], 0, 13, 0x1000);
				insrt(exp.zcull_unka00[0], 16, 13, extr(exp.bundle_clear_hv[0], 16, 16) + 1);
				insrt(exp.zcull_unka00[1], 16, 13, extr(exp.bundle_clear_hv[1], 16, 16) + 1);
			}
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdClearZeta : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !extr(exp.celsius_config_c, 8, 1);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_clear_zeta = val;
			pgraph_celsius_icmd(&exp, 0x37, exp.bundle_clear_zeta, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdClipidEnable : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 2 && !extr(exp.celsius_config_c, 8, 1);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_z_config, 6, 1, val);
			pgraph_celsius_icmd(&exp, 0x36, exp.bundle_z_config, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdClipidId : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0xf) && !extr(exp.celsius_config_c, 8, 1);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_clipid_id = val & 0xf;
			pgraph_celsius_icmd(&exp, 0x35, exp.bundle_clipid_id, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdClearHv : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xffff;
			val *= 0x10001;
		}
		if (rnd() & 1) {
			val &= 0x0fff0fff;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0x0fff0fff) && !extr(exp.celsius_config_c, 8, 1) &&
			extr(val, 0, 16) <= extr(val, 16, 16);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_clear_hv[which] = val & 0x0fff0fff;
			pgraph_celsius_icmd(&exp, 0x2b + which, exp.bundle_clear_hv[which], true);
		}
	}
public:
	MthdClearHv(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdCelsiusUnkd84 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0x8000000f;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return !(val & ~0x80000003) && !extr(exp.celsius_config_c, 8, 1);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_z_config, 1, 2, val);
			insrt(exp.bundle_z_config, 31, 1, extr(val, 31, 1));
			pgraph_celsius_icmd(&exp, 0x36, exp.bundle_z_config, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusTexUnk258 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 2;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_tex_format[idx], 3, 1, val);
			pgraph_celsius_icmd(&exp, 4 + idx, exp.bundle_tex_format[idx], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusSurfUnk2b8 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (extr(val, 4, 28))
			return false;
		if (extr(val, 0, 2) > 2)
			return false;
		if (extr(val, 2, 2) > 2)
			return false;
		return true;
	}
	void emulate_mthd() override {
		insrt(exp.surf_type, 2, 2, extr(val, 0, 2));
		insrt(exp.surf_type, 6, 2, extr(val, 2, 2));
		insrt(exp.unka10, 29, 1, extr(exp.debug[4], 2, 1) && !!extr(exp.surf_type, 2, 2));
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusSurfUnk2bc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		return val < 2;
	}
	void emulate_mthd() override {
		insrt(exp.surf_type, 31, 1, val);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusUnk3f8 : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xf;
			if (rnd() & 1)
				val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return val < 2;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_config_a, 31, 1, val);
			pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdCelsiusUnk3fc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_mthd_unk3fc = val;
			pgraph_celsius_icmd(&exp, 0x38, exp.celsius_mthd_unk3fc, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

std::vector<SingleMthdTest *> Celsius::mthds() {
	std::vector<SingleMthdTest *> res = {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdWarning(opt, rnd(), "warning", 2, cls, 0x108),
		new MthdState(opt, rnd(), "state", -1, cls, 0x10c),
		new MthdSync(opt, rnd(), "sync", 1, cls, 0x110),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 3, cls, 0x180),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_a", 4, cls, 0x184, 0, DMA_R | DMA_ALIGN),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_b", 5, cls, 0x188, 1, DMA_R | DMA_ALIGN),
		new MthdDmaVtx(opt, rnd(), "dma_vtx", 6, cls, 0x18c),
		new MthdDmaState(opt, rnd(), "dma_state", 7, cls, 0x190),
		new MthdDmaSurf(opt, rnd(), "dma_surf_color", 8, cls, 0x194, 2, SURF_NV10),
		new MthdDmaSurf(opt, rnd(), "dma_surf_zeta", 9, cls, 0x198, 3, SURF_NV10),
		new MthdClipHv(opt, rnd(), "clip_h", 10, cls, 0x200, 2, 0),
		new MthdClipHv(opt, rnd(), "clip_v", 10, cls, 0x204, 2, 1),
		new MthdSurf3DFormat(opt, rnd(), "surf_format", 11, cls, 0x208, true),
		new MthdSurfPitch2(opt, rnd(), "surf_pitch_2", 12, cls, 0x20c, 2, 3, SURF_NV10),
		new MthdSurfOffset(opt, rnd(), "color_offset", 13, cls, 0x210, 2, SURF_NV10),
		new MthdSurfOffset(opt, rnd(), "zeta_offset", 14, cls, 0x214, 3, SURF_NV10),
		new MthdCelsiusTexOffset(opt, rnd(), "tex_offset", 15, cls, 0x218, 2),
		new MthdCelsiusTexFormat(opt, rnd(), "tex_format", 16, cls, 0x220, 2),
		new MthdCelsiusTexControl(opt, rnd(), "tex_control", 17, cls, 0x228, 2),
		new MthdCelsiusTexPitch(opt, rnd(), "tex_pitch", 18, cls, 0x230, 2),
		new MthdCelsiusTexUnk238(opt, rnd(), "tex_unk238", 19, cls, 0x238, 2),
		new MthdCelsiusTexRect(opt, rnd(), "tex_rect", 20, cls, 0x240, 2),
		new MthdCelsiusTexFilter(opt, rnd(), "tex_filter", 21, cls, 0x248, 2),
		new MthdCelsiusTexPalette(opt, rnd(), "tex_palette", 22, cls, 0x250, 2),
		new MthdCelsiusRcInAlpha(opt, rnd(), "rc_in_alpha", 23, cls, 0x260, 2),
		new MthdCelsiusRcInColor(opt, rnd(), "rc_in_color", 24, cls, 0x268, 2),
		new MthdCelsiusRcFactor(opt, rnd(), "rc_factor", 25, cls, 0x270, 2),
		new MthdCelsiusRcOutAlpha(opt, rnd(), "rc_out_alpha", 26, cls, 0x278, 2),
		new MthdCelsiusRcOutColor(opt, rnd(), "rc_out_color", 27, cls, 0x280, 2),
		new MthdCelsiusRcFinalA(opt, rnd(), "rc_final_a", 28, cls, 0x288),
		new MthdCelsiusRcFinalB(opt, rnd(), "rc_final_b", 29, cls, 0x28c),
		new MthdCelsiusConfig(opt, rnd(), "config", 30, cls, 0x290),
		new MthdCelsiusLightModel(opt, rnd(), "light_model", 31, cls, 0x294),
		new MthdCelsiusLightMaterial(opt, rnd(), "light_material", -1, cls, 0x298),
		new MthdCelsiusFogMode(opt, rnd(), "fog_mode", -1, cls, 0x29c),
		new MthdCelsiusFogCoord(opt, rnd(), "fog_coord", -1, cls, 0x2a0),
		new MthdCelsiusFogEnable(opt, rnd(), "fog_enable", -1, cls, 0x2a4),
		new MthdCelsiusFogColor(opt, rnd(), "fog_color", -1, cls, 0x2a8),
		new MthdCelsiusTexColorKey(opt, rnd(), "tex_color_key", -1, cls, 0x2ac, 2),
		new MthdCelsiusClipRectMode(opt, rnd(), "clip_rect_mode", -1, cls, 0x2b4),
		new MthdCelsiusClipRectHoriz(opt, rnd(), "clip_rect_horiz", -1, cls, 0x2c0, 8),
		new MthdCelsiusClipRectVert(opt, rnd(), "clip_rect_vert", -1, cls, 0x2e0, 8),
		new MthdCelsiusAlphaFuncEnable(opt, rnd(), "alpha_func_enable", -1, cls, 0x300),
		new MthdCelsiusBlendFuncEnable(opt, rnd(), "blend_func_enable", -1, cls, 0x304),
		new MthdCelsiusCullFaceEnable(opt, rnd(), "cull_face_enable", -1, cls, 0x308),
		new MthdCelsiusDepthTestEnable(opt, rnd(), "depth_test_enable", -1, cls, 0x30c),
		new MthdCelsiusDitherEnable(opt, rnd(), "dither_enable", -1, cls, 0x310),
		new MthdCelsiusLightingEnable(opt, rnd(), "lighting_enable", -1, cls, 0x314),
		new MthdCelsiusPointParamsEnable(opt, rnd(), "point_params_enable", -1, cls, 0x318),
		new MthdCelsiusPointSmoothEnable(opt, rnd(), "point_smooth_enable", -1, cls, 0x31c),
		new MthdCelsiusLineSmoothEnable(opt, rnd(), "line_smooth_enable", -1, cls, 0x320),
		new MthdCelsiusPolygonSmoothEnable(opt, rnd(), "polygon_smooth_enable", -1, cls, 0x324),
		new MthdCelsiusWeightEnable(opt, rnd(), "weight_enable", -1, cls, 0x328),
		new MthdCelsiusStencilEnable(opt, rnd(), "stencil_enable", -1, cls, 0x32c),
		new MthdCelsiusPolygonOffsetPointEnable(opt, rnd(), "polygon_offset_point_enable", -1, cls, 0x330),
		new MthdCelsiusPolygonOffsetLineEnable(opt, rnd(), "polygon_offset_line_enable", -1, cls, 0x334),
		new MthdCelsiusPolygonOffsetFillEnable(opt, rnd(), "polygon_offset_fill_enable", -1, cls, 0x338),
		new MthdCelsiusAlphaFuncFunc(opt, rnd(), "alpha_func_func", -1, cls, 0x33c),
		new MthdCelsiusAlphaFuncRef(opt, rnd(), "alpha_func_ref", -1, cls, 0x340),
		new MthdCelsiusBlendFunc(opt, rnd(), "blend_func_src", -1, cls, 0x344, 0),
		new MthdCelsiusBlendFunc(opt, rnd(), "blend_func_dst", -1, cls, 0x348, 1),
		new MthdCelsiusBlendColor(opt, rnd(), "blend_color", -1, cls, 0x34c),
		new MthdCelsiusBlendEquation(opt, rnd(), "blend_equation", -1, cls, 0x350),
		new MthdCelsiusDepthFunc(opt, rnd(), "depth_func", -1, cls, 0x354),
		new MthdCelsiusColorMask(opt, rnd(), "color_mask", -1, cls, 0x358),
		new MthdCelsiusDepthWriteEnable(opt, rnd(), "depth_write_enable", -1, cls, 0x35c),
		new MthdCelsiusStencilVal(opt, rnd(), "stencil_mask", -1, cls, 0x360, 2),
		new MthdCelsiusStencilFunc(opt, rnd(), "stencil_func", -1, cls, 0x364),
		new MthdCelsiusStencilVal(opt, rnd(), "stencil_func_ref", -1, cls, 0x368, 0),
		new MthdCelsiusStencilVal(opt, rnd(), "stencil_func_mask", -1, cls, 0x36c, 1),
		new MthdCelsiusStencilOp(opt, rnd(), "stencil_op_fail", -1, cls, 0x370, 0),
		new MthdCelsiusStencilOp(opt, rnd(), "stencil_op_zfail", -1, cls, 0x374, 1),
		new MthdCelsiusStencilOp(opt, rnd(), "stencil_op_zpass", -1, cls, 0x378, 2),
		new MthdCelsiusShadeMode(opt, rnd(), "shade_mode", -1, cls, 0x37c),
		new MthdCelsiusLineWidth(opt, rnd(), "line_width", -1, cls, 0x380),
		new MthdCelsiusPolygonOffsetFactor(opt, rnd(), "polygon_offset_factor", -1, cls, 0x384),
		new MthdCelsiusPolygonOffsetUnits(opt, rnd(), "polygon_offset_units", -1, cls, 0x388),
		new MthdCelsiusPolygonMode(opt, rnd(), "polygon_mode_front", -1, cls, 0x38c, 0),
		new MthdCelsiusPolygonMode(opt, rnd(), "polygon_mode_back", -1, cls, 0x390, 1),
		new MthdCelsiusDepthRangeNear(opt, rnd(), "depth_range_near", -1, cls, 0x394),
		new MthdCelsiusDepthRangeFar(opt, rnd(), "depth_range_far", -1, cls, 0x398),
		new MthdCelsiusCullFace(opt, rnd(), "cull_face", -1, cls, 0x39c),
		new MthdCelsiusFrontFace(opt, rnd(), "front_face", -1, cls, 0x3a0),
		new MthdCelsiusNormalizeEnable(opt, rnd(), "normalize_enable", -1, cls, 0x3a4),
		new MthdCelsiusLightSDFree(opt, rnd(), "material_factor_a", -1, cls, 0x3b4, 11),
		new MthdCelsiusSpecularEnable(opt, rnd(), "specular_enable", -1, cls, 0x3b8),
		new MthdCelsiusLightEnable(opt, rnd(), "light_enable", -1, cls, 0x3bc),
		new MthdCelsiusTexGenMode(opt, rnd(), "tex_gen_mode_s", -1, cls, 0x3c0, 2, 0x10, 0),
		new MthdCelsiusTexGenMode(opt, rnd(), "tex_gen_mode_t", -1, cls, 0x3c4, 2, 0x10, 1),
		new MthdCelsiusTexGenMode(opt, rnd(), "tex_gen_mode_r", -1, cls, 0x3c8, 2, 0x10, 2),
		new MthdCelsiusTexGenMode(opt, rnd(), "tex_gen_mode_q", -1, cls, 0x3cc, 2, 0x10, 3),
		new MthdCelsiusTexMatrixEnable(opt, rnd(), "tex_matrix_enable", -1, cls, 0x3e0, 2),
		new MthdCelsiusTlMode(opt, rnd(), "tl_mode", -1, cls, 0x3e8),
		new MthdCelsiusPointSize(opt, rnd(), "point_size", -1, cls, 0x3ec),
		new MthdCelsiusUnk3f0(opt, rnd(), "unk3f0", -1, cls, 0x3f0),
		new MthdCelsiusFlatshadeFirst(opt, rnd(), "flatshade_first", -1, cls, 0x3f4),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_mv0", -1, cls, 0x400, 0x10, 4, 0),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_mv1", -1, cls, 0x440, 0x10, 4, 3),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_imv0", -1, cls, 0x480, 0x10, 4, 1),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_imv1", -1, cls, 0x4c0, 0x10, 4, 4),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_proj", -1, cls, 0x500, 0x10, 4, 2),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_tx0", -1, cls, 0x540, 0x10, 4, 6),
		new MthdCelsiusMatrix(opt, rnd(), "matrix_tx1", -1, cls, 0x580, 0x10, 4, 8),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_0_s_plane", -1, cls, 0x600, 4, 4, 20),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_0_t_plane", -1, cls, 0x610, 4, 4, 21),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_0_r_plane", -1, cls, 0x620, 4, 4, 22),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_0_q_plane", -1, cls, 0x630, 4, 4, 23),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_1_s_plane", -1, cls, 0x640, 4, 4, 28),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_1_t_plane", -1, cls, 0x650, 4, 4, 29),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_1_r_plane", -1, cls, 0x660, 4, 4, 30),
		new MthdCelsiusXfrm(opt, rnd(), "tex_gen_1_q_plane", -1, cls, 0x670, 4, 4, 31),
		new MthdCelsiusLightV(opt, rnd(), "fog_coeff", -1, cls, 0x680, 3, 4, 43),
		new MthdCelsiusXfrmFree(opt, rnd(), "fog_plane", -1, cls, 0x68c, 4, 4, 56),
		new MthdCelsiusLightSBFree(opt, rnd(), "material_shininess_0", -1, cls, 0x6a0, 1),
		new MthdCelsiusLightSCFree(opt, rnd(), "material_shininess_1", -1, cls, 0x6a4, 1),
		new MthdCelsiusLightSDFree(opt, rnd(), "material_shininess_2", -1, cls, 0x6a8, 2),
		new MthdCelsiusLightSAFree(opt, rnd(), "material_shininess_3", -1, cls, 0x6ac, 2),
		new MthdCelsiusLightSCFree(opt, rnd(), "material_shininess_4", -1, cls, 0x6b0, 2),
		new MthdCelsiusLightSCFree(opt, rnd(), "material_shininess_5", -1, cls, 0x6b4, 3),
		new MthdCelsiusLightVFree(opt, rnd(), "light_model_ambient_color", -1, cls, 0x6c4, 3, 4, 41),
		new MthdCelsiusXfrm(opt, rnd(), "viewport_translate", -1, cls, 0x6e8, 4, 4, 57),
		new MthdCelsiusLightV(opt, rnd(), "point_params_012", -1, cls, 0x6f8, 3, 4, 45),
		new MthdCelsiusLightV(opt, rnd(), "point_params_345", -1, cls, 0x704, 3, 4, 46),
		new MthdCelsiusLightSB(opt, rnd(), "point_params_6", -1, cls, 0x710, 2),
		new MthdCelsiusLightSD(opt, rnd(), "point_params_7", -1, cls, 0x714, 1),
		new MthdCelsiusXfrm(opt, rnd(), "light_eye_position", -1, cls, 0x718, 4, 4, 52),
		new MthdCelsiusLightVFree(opt, rnd(), "light_0_ambient_color", -1, cls, 0x800, 3, 4, 0),
		new MthdCelsiusLightVFree(opt, rnd(), "light_0_diffuse_color", -1, cls, 0x80c, 3, 4, 1),
		new MthdCelsiusLightVFree(opt, rnd(), "light_0_specular_color", -1, cls, 0x818, 3, 4, 2),
		new MthdCelsiusLightSB(opt, rnd(), "light_0_local_range", -1, cls, 0x824, 3),
		new MthdCelsiusLightV(opt, rnd(), "light_0_half_vector", -1, cls, 0x828, 3, 4, 3),
		new MthdCelsiusLightV(opt, rnd(), "light_0_direction", -1, cls, 0x834, 3, 4, 4),
		new MthdCelsiusLightSB(opt, rnd(), "light_0_spot_cutoff_0", -1, cls, 0x840, 11),
		new MthdCelsiusLightSC(opt, rnd(), "light_0_spot_cutoff_1", -1, cls, 0x844, 4),
		new MthdCelsiusLightSD(opt, rnd(), "light_0_spot_cutoff_2", -1, cls, 0x848, 3),
		new MthdCelsiusXfrm(opt, rnd(), "light_0_spot_direction", -1, cls, 0x84c, 4, 4, 44),
		new MthdCelsiusXfrm3(opt, rnd(), "light_0_position", -1, cls, 0x85c, 3, 4, 36),
		new MthdCelsiusLightV(opt, rnd(), "light_0_attenuation", -1, cls, 0x868, 3, 4, 3),
		new MthdCelsiusLightVFree(opt, rnd(), "light_1_ambient_color", -1, cls, 0x880, 3, 4, 5),
		new MthdCelsiusLightVFree(opt, rnd(), "light_1_diffuse_color", -1, cls, 0x88c, 3, 4, 6),
		new MthdCelsiusLightVFree(opt, rnd(), "light_1_specular_color", -1, cls, 0x898, 3, 4, 7),
		new MthdCelsiusLightSB(opt, rnd(), "light_1_local_range", -1, cls, 0x8a4, 4),
		new MthdCelsiusLightV(opt, rnd(), "light_1_half_vector", -1, cls, 0x8a8, 3, 4, 8),
		new MthdCelsiusLightV(opt, rnd(), "light_1_direction", -1, cls, 0x8b4, 3, 4, 9),
		new MthdCelsiusLightSB(opt, rnd(), "light_1_spot_cutoff_0", -1, cls, 0x8c0, 12),
		new MthdCelsiusLightSC(opt, rnd(), "light_1_spot_cutoff_1", -1, cls, 0x8c4, 5),
		new MthdCelsiusLightSD(opt, rnd(), "light_1_spot_cutoff_2", -1, cls, 0x8c8, 4),
		new MthdCelsiusXfrm(opt, rnd(), "light_1_spot_direction", -1, cls, 0x8cc, 4, 4, 45),
		new MthdCelsiusXfrm3(opt, rnd(), "light_1_position", -1, cls, 0x8dc, 3, 4, 37),
		new MthdCelsiusLightV(opt, rnd(), "light_1_attenuation", -1, cls, 0x8e8, 3, 4, 8),
		new MthdCelsiusLightVFree(opt, rnd(), "light_2_ambient_color", -1, cls, 0x900, 3, 4, 10),
		new MthdCelsiusLightVFree(opt, rnd(), "light_2_diffuse_color", -1, cls, 0x90c, 3, 4, 11),
		new MthdCelsiusLightVFree(opt, rnd(), "light_2_specular_color", -1, cls, 0x918, 3, 4, 12),
		new MthdCelsiusLightSB(opt, rnd(), "light_2_local_range", -1, cls, 0x924, 5),
		new MthdCelsiusLightV(opt, rnd(), "light_2_half_vector", -1, cls, 0x928, 3, 4, 13),
		new MthdCelsiusLightV(opt, rnd(), "light_2_direction", -1, cls, 0x934, 3, 4, 14),
		new MthdCelsiusLightSB(opt, rnd(), "light_2_spot_cutoff_0", -1, cls, 0x940, 13),
		new MthdCelsiusLightSC(opt, rnd(), "light_2_spot_cutoff_1", -1, cls, 0x944, 6),
		new MthdCelsiusLightSD(opt, rnd(), "light_2_spot_cutoff_2", -1, cls, 0x948, 5),
		new MthdCelsiusXfrm(opt, rnd(), "light_2_spot_direction", -1, cls, 0x94c, 4, 4, 46),
		new MthdCelsiusXfrm3(opt, rnd(), "light_2_position", -1, cls, 0x95c, 3, 4, 38),
		new MthdCelsiusLightV(opt, rnd(), "light_2_attenuation", -1, cls, 0x968, 3, 4, 13),
		new MthdCelsiusLightVFree(opt, rnd(), "light_3_ambient_color", -1, cls, 0x980, 3, 4, 15),
		new MthdCelsiusLightVFree(opt, rnd(), "light_3_diffuse_color", -1, cls, 0x98c, 3, 4, 16),
		new MthdCelsiusLightVFree(opt, rnd(), "light_3_specular_color", -1, cls, 0x998, 3, 4, 17),
		new MthdCelsiusLightSB(opt, rnd(), "light_3_local_range", -1, cls, 0x9a4, 6),
		new MthdCelsiusLightV(opt, rnd(), "light_3_half_vector", -1, cls, 0x9a8, 3, 4, 18),
		new MthdCelsiusLightV(opt, rnd(), "light_3_direction", -1, cls, 0x9b4, 3, 4, 19),
		new MthdCelsiusLightSB(opt, rnd(), "light_3_spot_cutoff_0", -1, cls, 0x9c0, 14),
		new MthdCelsiusLightSC(opt, rnd(), "light_3_spot_cutoff_1", -1, cls, 0x9c4, 7),
		new MthdCelsiusLightSD(opt, rnd(), "light_3_spot_cutoff_2", -1, cls, 0x9c8, 6),
		new MthdCelsiusXfrm(opt, rnd(), "light_3_spot_direction", -1, cls, 0x9cc, 4, 4, 47),
		new MthdCelsiusXfrm3(opt, rnd(), "light_3_position", -1, cls, 0x9dc, 3, 4, 39),
		new MthdCelsiusLightV(opt, rnd(), "light_3_attenuation", -1, cls, 0x9e8, 3, 4, 18),
		new MthdCelsiusLightVFree(opt, rnd(), "light_4_ambient_color", -1, cls, 0xa00, 3, 4, 20),
		new MthdCelsiusLightVFree(opt, rnd(), "light_4_diffuse_color", -1, cls, 0xa0c, 3, 4, 21),
		new MthdCelsiusLightVFree(opt, rnd(), "light_4_specular_color", -1, cls, 0xa18, 3, 4, 22),
		new MthdCelsiusLightSB(opt, rnd(), "light_4_local_range", -1, cls, 0xa24, 7),
		new MthdCelsiusLightV(opt, rnd(), "light_4_half_vector", -1, cls, 0xa28, 3, 4, 23),
		new MthdCelsiusLightV(opt, rnd(), "light_4_direction", -1, cls, 0xa34, 3, 4, 24),
		new MthdCelsiusLightSB(opt, rnd(), "light_4_spot_cutoff_0", -1, cls, 0xa40, 15),
		new MthdCelsiusLightSC(opt, rnd(), "light_4_spot_cutoff_1", -1, cls, 0xa44, 8),
		new MthdCelsiusLightSD(opt, rnd(), "light_4_spot_cutoff_2", -1, cls, 0xa48, 7),
		new MthdCelsiusXfrm(opt, rnd(), "light_4_spot_direction", -1, cls, 0xa4c, 4, 4, 48),
		new MthdCelsiusXfrm3(opt, rnd(), "light_4_position", -1, cls, 0xa5c, 3, 4, 40),
		new MthdCelsiusLightV(opt, rnd(), "light_4_attenuation", -1, cls, 0xa68, 3, 4, 23),
		new MthdCelsiusLightVFree(opt, rnd(), "light_5_ambient_color", -1, cls, 0xa80, 3, 4, 25),
		new MthdCelsiusLightVFree(opt, rnd(), "light_5_diffuse_color", -1, cls, 0xa8c, 3, 4, 26),
		new MthdCelsiusLightVFree(opt, rnd(), "light_5_specular_color", -1, cls, 0xa98, 3, 4, 27),
		new MthdCelsiusLightSB(opt, rnd(), "light_5_local_range", -1, cls, 0xaa4, 8),
		new MthdCelsiusLightV(opt, rnd(), "light_5_half_vector", -1, cls, 0xaa8, 3, 4, 28),
		new MthdCelsiusLightV(opt, rnd(), "light_5_direction", -1, cls, 0xab4, 3, 4, 29),
		new MthdCelsiusLightSB(opt, rnd(), "light_5_spot_cutoff_0", -1, cls, 0xac0, 16),
		new MthdCelsiusLightSC(opt, rnd(), "light_5_spot_cutoff_1", -1, cls, 0xac4, 9),
		new MthdCelsiusLightSD(opt, rnd(), "light_5_spot_cutoff_2", -1, cls, 0xac8, 8),
		new MthdCelsiusXfrm(opt, rnd(), "light_5_spot_direction", -1, cls, 0xacc, 4, 4, 49),
		new MthdCelsiusXfrm3(opt, rnd(), "light_5_position", -1, cls, 0xadc, 3, 4, 41),
		new MthdCelsiusLightV(opt, rnd(), "light_5_attenuation", -1, cls, 0xae8, 3, 4, 28),
		new MthdCelsiusLightVFree(opt, rnd(), "light_6_ambient_color", -1, cls, 0xb00, 3, 4, 30),
		new MthdCelsiusLightVFree(opt, rnd(), "light_6_diffuse_color", -1, cls, 0xb0c, 3, 4, 31),
		new MthdCelsiusLightVFree(opt, rnd(), "light_6_specular_color", -1, cls, 0xb18, 3, 4, 32),
		new MthdCelsiusLightSB(opt, rnd(), "light_6_local_range", -1, cls, 0xb24, 9),
		new MthdCelsiusLightV(opt, rnd(), "light_6_half_vector", -1, cls, 0xb28, 3, 4, 33),
		new MthdCelsiusLightV(opt, rnd(), "light_6_direction", -1, cls, 0xb34, 3, 4, 34),
		new MthdCelsiusLightSB(opt, rnd(), "light_6_spot_cutoff_0", -1, cls, 0xb40, 17),
		new MthdCelsiusLightSC(opt, rnd(), "light_6_spot_cutoff_1", -1, cls, 0xb44, 10),
		new MthdCelsiusLightSD(opt, rnd(), "light_6_spot_cutoff_2", -1, cls, 0xb48, 9),
		new MthdCelsiusXfrm(opt, rnd(), "light_6_spot_direction", -1, cls, 0xb4c, 4, 4, 50),
		new MthdCelsiusXfrm3(opt, rnd(), "light_6_position", -1, cls, 0xb5c, 3, 4, 42),
		new MthdCelsiusLightV(opt, rnd(), "light_6_attenuation", -1, cls, 0xb68, 3, 4, 33),
		new MthdCelsiusLightVFree(opt, rnd(), "light_7_ambient_color", -1, cls, 0xb80, 3, 4, 35),
		new MthdCelsiusLightVFree(opt, rnd(), "light_7_diffuse_color", -1, cls, 0xb8c, 3, 4, 36),
		new MthdCelsiusLightVFree(opt, rnd(), "light_7_specular_color", -1, cls, 0xb98, 3, 4, 37),
		new MthdCelsiusLightSB(opt, rnd(), "light_7_local_range", -1, cls, 0xba4, 10),
		new MthdCelsiusLightV(opt, rnd(), "light_7_half_vector", -1, cls, 0xba8, 3, 4, 38),
		new MthdCelsiusLightV(opt, rnd(), "light_7_direction", -1, cls, 0xbb4, 3, 4, 39),
		new MthdCelsiusLightSB(opt, rnd(), "light_7_spot_cutoff_0", -1, cls, 0xbc0, 18),
		new MthdCelsiusLightSC(opt, rnd(), "light_7_spot_cutoff_1", -1, cls, 0xbc4, 11),
		new MthdCelsiusLightSD(opt, rnd(), "light_7_spot_cutoff_2", -1, cls, 0xbc8, 10),
		new MthdCelsiusXfrm(opt, rnd(), "light_7_spot_direction", -1, cls, 0xbcc, 4, 4, 51),
		new MthdCelsiusXfrm3(opt, rnd(), "light_7_position", -1, cls, 0xbdc, 3, 4, 43),
		new MthdCelsiusLightV(opt, rnd(), "light_7_attenuation", -1, cls, 0xbe8, 3, 4, 38),
		new UntestedMthd(opt, rnd(), "vtx_pos_3f", -1, cls, 0xc00, 3), // XXX
		new UntestedMthd(opt, rnd(), "vtx_pos_3s", -1, cls, 0xc10, 2), // XXX
		new UntestedMthd(opt, rnd(), "vtx_pos_4f", -1, cls, 0xc18, 4), // XXX
		new UntestedMthd(opt, rnd(), "vtx_pos_4s", -1, cls, 0xc28, 2), // XXX
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_nrm_3f", -1, cls, 0xc30, 3, 4, 5),
		new MthdCelsiusVtxAttrNShort(opt, rnd(), "vtx_nrm_3s", -1, cls, 0xc40, 2, 4, 5),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_col0_4f", -1, cls, 0xc50, 4, 4, 1),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_col0_3f", -1, cls, 0xc60, 3, 4, 1),
		new MthdCelsiusVtxAttrUByte(opt, rnd(), "vtx_col0_4ub", -1, cls, 0xc6c, 1),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_col1_4f", -1, cls, 0xc70, 4, 4, 2),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_col1_3f", -1, cls, 0xc80, 3, 4, 2),
		new MthdCelsiusVtxAttrUByte(opt, rnd(), "vtx_col1_4ub", -1, cls, 0xc8c, 2),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_txc0_2f", -1, cls, 0xc90, 2, 4, 3),
		new MthdCelsiusVtxAttrShort(opt, rnd(), "vtx_txc0_2s", -1, cls, 0xc98, 1, 4, 3),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_txc0_4f", -1, cls, 0xca0, 4, 4, 3),
		new MthdCelsiusVtxAttrShort(opt, rnd(), "vtx_txc0_4s", -1, cls, 0xcb0, 2, 4, 3),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_txc1_2f", -1, cls, 0xcb8, 2, 4, 4),
		new MthdCelsiusVtxAttrShort(opt, rnd(), "vtx_txc1_2s", -1, cls, 0xcc0, 1, 4, 4),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_txc1_4f", -1, cls, 0xcc8, 4, 4, 4),
		new MthdCelsiusVtxAttrShort(opt, rnd(), "vtx_txc1_4s", -1, cls, 0xcd8, 2, 4, 4),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_fog_1f", -1, cls, 0xce0, 1, 4, 7),
		new MthdCelsiusVtxAttrFloat(opt, rnd(), "vtx_wei_1f", -1, cls, 0xce4, 1, 4, 6),
		new MthdCelsiusEdgeFlag(opt, rnd(), "vtx_edge_flag", -1, cls, 0xcec),
		new MthdCelsiusUnkcf0(opt, rnd(), "unkcf0", -1, cls, 0xcf0), // XXX
		new MthdCelsiusUnkcf4(opt, rnd(), "unkcf4", -1, cls, 0xcf4), // XXX
		new MthdCelsiusXfNop(opt, rnd(), "xf_nop", -1, cls, 0xcf8),
		new MthdCelsiusXfSync(opt, rnd(), "xf_sync", -1, cls, 0xcfc),
		new MthdCelsiusVtxbufOffset(opt, rnd(), "vtxbuf_offset", -1, cls, 0xd00, 8, 8),
		new MthdCelsiusVtxbufFormat(opt, rnd(), "vtxbuf_format", -1, cls, 0xd04, 8, 8),
		new MthdCelsiusBeginEnd(opt, rnd(), "draw_idx16.begin", -1, cls, 0xdfc),
		new UntestedMthd(opt, rnd(), "draw_idx16.data", -1, cls, 0xe00, 0x80), // XXX
		new MthdCelsiusBeginEnd(opt, rnd(), "draw_idx32.begin", -1, cls, 0x10fc),
		new UntestedMthd(opt, rnd(), "draw_idx32.data", -1, cls, 0x1100, 0x40), // XXX
		new MthdCelsiusBeginEnd(opt, rnd(), "draw_arrays.begin", -1, cls, 0x13fc),
		new UntestedMthd(opt, rnd(), "draw_arrays.data", -1, cls, 0x1400, 0x80), // XXX
		new MthdCelsiusBeginEnd(opt, rnd(), "draw_inline.begin", -1, cls, 0x17fc),
		new UntestedMthd(opt, rnd(), "draw_inline.data", -1, cls, 0x1800, 0x200), // XXX
	};
	if (cls == 0x56) {
		res.insert(res.end(), {
			new MthdCelsiusLightV(opt, rnd(), "material_factor_rgb", -1, cls, 0x3a8, 3, 4, 42),
		});
	} else {
		res.insert(res.end(), {
			new MthdCelsiusLightVFree(opt, rnd(), "material_factor_rgb", -1, cls, 0x3a8, 3, 4, 42),
			new UntestedMthd(opt, rnd(), "unk114", -1, cls, 0x114), // XXX
			new MthdFlipSet(opt, rnd(), "flip_write", -1, cls, 0x120, 1, 1),
			new MthdFlipSet(opt, rnd(), "flip_read", -1, cls, 0x124, 1, 0),
			new MthdFlipSet(opt, rnd(), "flip_modulo", -1, cls, 0x128, 1, 2),
			new MthdFlipBumpWrite(opt, rnd(), "flip_bump_write", -1, cls, 0x12c, 1),
			new UntestedMthd(opt, rnd(), "flip_unk130", -1, cls, 0x130),
			new MthdCelsiusColorLogicOpEnable(opt, rnd(), "color_logic_op_enable", -1, cls, 0xd40),
			new MthdCelsiusColorLogicOpOp(opt, rnd(), "color_logic_op_op", -1, cls, 0xd44),
		});
	}
	if (cls == 0x99) {
		res.insert(res.end(), {
			new MthdDmaClipid(opt, rnd(), "dma_clipid", -1, cls, 0x1ac),
			new MthdDmaZcull(opt, rnd(), "dma_zcull", -1, cls, 0x1b0),
			new MthdSurfPitchClipid(opt, rnd(), "surf_pitch_clipid", -1, cls, 0xd54),
			new MthdSurfOffsetClipid(opt, rnd(), "surf_offset_clipid", -1, cls, 0xd58),
			new MthdSurfPitchZcull(opt, rnd(), "surf_pitch_zcull", -1, cls, 0xd5c),
			new MthdSurfOffsetZcull(opt, rnd(), "surf_offset_zcull", -1, cls, 0xd60),
			new MthdInvalidateZcull(opt, rnd(), "invalidate_zcull", -1, cls, 0xd64),
			new MthdClearZeta(opt, rnd(), "clear_zeta", -1, cls, 0xd68),
			new UntestedMthd(opt, rnd(), "clear_zeta_trigger", -1, cls, 0xd6c), // XXX
			new UntestedMthd(opt, rnd(), "clear_clipid_trigger", -1, cls, 0xd70), // XXX
			new MthdClipidEnable(opt, rnd(), "clipid_enable", -1, cls, 0xd74),
			new MthdClipidId(opt, rnd(), "clipid_id", -1, cls, 0xd78),
			new MthdClearHv(opt, rnd(), "clear_h", -1, cls, 0xd7c, 0),
			new MthdClearHv(opt, rnd(), "clear_v", -1, cls, 0xd80, 1),
			new MthdCelsiusUnkd84(opt, rnd(), "unkd84", -1, cls, 0xd84),
			new MthdCelsiusTexUnk258(opt, rnd(), "tex_unk258", -1, cls, 0x258, 2),
			new MthdCelsiusSurfUnk2b8(opt, rnd(), "surf_unk2b8", -1, cls, 0x2b8),
			new MthdCelsiusSurfUnk2bc(opt, rnd(), "surf_unk2bc", -1, cls, 0x2bc),
			new MthdCelsiusUnk3f8(opt, rnd(), "unk3f8", -1, cls, 0x3f8),
			new MthdCelsiusUnk3fc(opt, rnd(), "unk3fc", -1, cls, 0x3fc),
		});
	} else {
		res.insert(res.end(), {
			new MthdCelsiusOldUnk3f8(opt, rnd(), "old_unk3f8", -1, cls, 0x3f8),
		});
	}
	return res;
}

}
}
