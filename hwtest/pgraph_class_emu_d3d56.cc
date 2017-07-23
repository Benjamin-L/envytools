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

class MthdEmuD3D56TexColorKey : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_color_key[0] = val;
			pgraph_celsius_icmd(&exp, 0x28, val, true);
		}
		insrt(exp.valid[1], 12, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TexOffset : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= ~0xff;
			val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		return (val & 0xff) == 0 || cls == 0x48;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		for (int j = 0; j < 2; j++) {
			if (which & (1 << j)) {
				if (!extr(exp.nsource, 1, 1)) {
					exp.bundle_tex_offset[j] = val;
					pgraph_celsius_icmd(&exp, j, val, true);
				}
				insrt(exp.valid[1], j ? 22 : 14, 1, 1);
			}
		}
		if (chipset.chipset != 0x10 && which == 3) {
			exp.celsius_pipe_junk[0] = 4;
		}
	}
public:
	MthdEmuD3D56TexOffset(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdEmuD3D56TexFormat : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 0, 2, 1);
			if (rnd() & 3)
				insrt(val, 2, 2, 0);
			if (rnd() & 3)
				insrt(val, 4, 2, 1);
			if (rnd() & 3)
				insrt(val, 6, 2, 1);
			if (rnd() & 3)
				insrt(val, 11, 1, 0);
			if (rnd() & 3)
				insrt(val, 24, 3, 1);
			if (rnd() & 3)
				insrt(val, 28, 3, 1);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (!extr(val, 0, 2) || extr(val, 0, 2) == 3)
			return false;
		if (!extr(val, 4, 2) || extr(val, 4, 2) == 3)
			return false;
		if (!extr(val, 6, 2) || extr(val, 6, 2) == 3)
			return false;
		if (!extr(val, 8, 3))
			return false;
		if (extr(val, 11, 1))
			return false;
		if (!extr(val, 12, 4))
			return false;
		if (extr(val, 16, 4) > 0xb)
			return false;
		if (extr(val, 20, 4) > 0xb)
			return false;
		if (extr(val, 24, 3) < 1 || extr(val, 24, 3) > 4)
			return false;
		if (extr(val, 28, 3) < 1 || extr(val, 28, 3) > 4)
			return false;
		if (cls == 0x54 || cls == 0x94) {
			if (extr(val, 2, 2) > 1)
				return false;
		} else {
			if (extr(val, 2, 2))
				return false;;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t rval = val & 0xfffff002;
		int omips = extr(val, 12, 4);
		int sfmt = extr(val, 8, 4);
		int fmt = sfmt;
		int mips = omips;
		int su = extr(val, 16, 4);
		int sv = extr(val, 20, 4);
		int wrapu = extr(val, 24, 3);
		int wrapv = extr(val, 28, 3);
		if (wrapu == 2)
			insrt(rval, 24, 4, 2);
		if (wrapu != 1)
			insrt(rval, 27, 1, 0);
		if (wrapu == 0)
			insrt(rval, 24, 3, nv04_pgraph_is_nv15p(&chipset) ? 2 : 3);
		if (wrapu == 4)
			insrt(rval, 24, 3, 3);
		if (wrapu == 5)
			insrt(rval, 24, 3, nv04_pgraph_is_nv15p(&chipset) ? 1 : 0);
		if (wrapu > 5)
			insrt(rval, 24, 3, 0);
		if (wrapv != 1)
			insrt(rval, 31, 1, 0);
		if (wrapv == 0)
			insrt(rval, 28, 3, nv04_pgraph_is_nv15p(&chipset) ? 0 : 2);
		if (wrapv == 7)
			insrt(rval, 28, 3, 0);
		if (wrapv == 4)
			insrt(rval, 28, 3, 3);
		if (wrapv == 5)
			insrt(rval, 28, 3, 1);
		if (wrapv == 6)
			insrt(rval, 28, 3, nv04_pgraph_is_nv15p(&chipset) ? 2 : 0);
		if (mips > su && mips > sv)
			mips = (su > sv ? su : sv) + 1;
		if (cls == 0x54 || cls == 0x94) {
			if (sfmt == 1)
				fmt = 0;
		}
		insrt(rval, 4, 1, extr(val, 5, 1));
		insrt(rval, 6, 1, extr(val, 7, 1));
		insrt(rval, 7, 5, fmt);
		insrt(rval, 12, 4, mips);
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 2; i++) {
				if (which & 1 << i) {
					exp.bundle_tex_format[i] = rval | (exp.bundle_tex_format[i] & 8);
				}
			}
			if (cls == 0x94 || cls == 0x54) {
				exp.bundle_tex_control[0] = 0x4003ffc0 | extr(val, 2, 2);
				exp.bundle_tex_control[1] = 0x3ffc0 | extr(val, 2, 2);
				insrt(exp.celsius_xf_misc_b, 2, 1,
					extr(exp.bundle_config_b, 6, 1) &&
					!extr(rval, 27, 1) && !extr(rval, 31, 1));
				insrt(exp.celsius_xf_misc_b, 16, 1, 0);
				pgraph_celsius_icmd(&exp, 4, exp.bundle_tex_format[0], false);
				pgraph_celsius_icmd(&exp, 6, exp.bundle_tex_control[0], false);
				pgraph_celsius_icmd(&exp, 7, exp.bundle_tex_control[1], true);
			} else {
				if (which == 2) {
					insrt(exp.celsius_xf_misc_b, 16, 1,
						extr(exp.bundle_config_b, 6, 1) &&
						!extr(rval, 27, 1) && !extr(rval, 31, 1));
				}
				pgraph_celsius_icmd(&exp, 4 + which - 1, exp.bundle_tex_format[which - 1], true);
			}
			if (which & 1) {
				insrt(exp.celsius_xf_misc_b, 2, 1,
					extr(exp.bundle_config_b, 6, 1) &&
					!extr(rval, 27, 1) && !extr(rval, 31, 1));
			}
			insrt(exp.celsius_xf_misc_a, 28, 1, 1);
			exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
			exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
		}
		if (which & 1)
			insrt(exp.valid[1], 15, 1, 1);
		if (which & 2)
			insrt(exp.valid[1], 21, 1, 1);
	}
public:
	MthdEmuD3D56TexFormat(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdEmuD3D56TexFilter : public SingleMthdTest {
	int which;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 5, 3, 0);
			if (rnd() & 3)
				insrt(val, 13, 2, 0);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 13, 2))
			return false;
		if (extr(val, 5, 3))
			return false;
		if (extr(val, 24, 3) == 0 || extr(val, 24, 3) > 6)
			return false;
		if (extr(val, 28, 3) == 0 || extr(val, 28, 3) > 6)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t rval = val & 0x77000000;
		int fmag = extr(val, 28, 3);
		if (fmag & 1)
			insrt(rval, 28, 3, 1);
		else
			insrt(rval, 28, 3, 2);
		insrt(rval, 5, 8, extr(val, 16, 8));
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_tex_filter[which == 2] = rval;
			pgraph_celsius_icmd(&exp, 0xe + (which == 2), rval, true);
		}
		insrt(exp.valid[1], which == 2 ? 23 : 16, 1, 1);
	}
public:
	MthdEmuD3D56TexFilter(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which) {}
};

class MthdEmuD3D6CombineControl : public SingleMthdTest {
	int which, ac;
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 1) {
				if (rnd() & 7)
					insrt(val, 0, 5, 0x10);
				if (rnd() & 7)
					insrt(val, 8, 5, 0x05);
				if (rnd() & 7)
					insrt(val, 16, 5, 0x04);
				if (rnd() & 7)
					insrt(val, 24, 5, 0x04);
			}
			val &= ~0x00e0e0e0;
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
		if (val & 0x00e0e0e0)
			return false;
		if (!extr(val, 2, 3))
			return false;
		if (!extr(val, 10, 3))
			return false;
		if (!extr(val, 18, 3))
			return false;
		if (!extr(val, 26, 3))
			return false;
		if (!extr(val, 29, 3))
			return false;
		if (which || cls != 0x55) {
			if (extr(val, 2, 3) == 7)
				return false;
			if (extr(val, 10, 3) == 7)
				return false;
			if (extr(val, 18, 3) == 7)
				return false;
			if (extr(val, 26, 3) == 7)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		uint32_t rc_in = 0, rc_out = 0;
		bool t1 = false;
		bool comp = false;
		int sop = extr(val, 29, 3);
		for (int j = 0; j < 4; j++) {
			int sreg = extr(val, 8 * j + 2, 3);
			bool neg = extr(val, 8 * j, 1);
			bool alpha = extr(val, 1 + 8 * j, 1) || !ac;
			int reg = 0;
			if (sreg == 1) {
				reg = 0;
			} else if (sreg == 2) {
				reg = 2;
			} else if (sreg == 3) {
				reg = 4;
			} else if (sreg == 4) {
				reg = which ? 0xc : 8;
			} else if (sreg == 5) {
				reg = 8;
			} else if (sreg == 6) {
				reg = 9;
				t1 = true;
			}
			insrt(rc_in, 5 + (3 - j) * 8, 1, neg);
			insrt(rc_in, 4 + (3 - j) * 8, 1, alpha);
			insrt(rc_in, 0 + (3 - j) * 8, 4, reg);
		}
		rc_out |= 0xc00;
		int op = 3;
		bool mux = false;
		if (sop == 1)
			op = 0;
		else if (sop == 2)
			op = 2;
		else if (sop == 3)
			op = 4;
		else if (sop == 4)
			op = 1;
		else if (sop == 5) {
			op = 0;
			mux = true;
		} else if (sop == 6) {
			op = 0;
			comp = true;
		} else if (sop == 7) {
			op = 3;
		}
		insrt(rc_out, 14, 1, mux);
		insrt(rc_out, 15, 3, op);
		insrt(exp.valid[1], 28 - which * 2 - ac, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			if (which == 0) {
				insrt(exp.celsius_config_c, 16 + ac, 1, !t1);
			} else {
				bool bypass = (val & 0x1f1f1f1f) == 0x04040510;
				insrt(exp.celsius_config_c, 18 + ac, 1, bypass);
			}
			insrt(exp.celsius_config_c, 20 + ac + which * 2, 1, comp);
			if (which == 1 && ac == 1) {
				exp.bundle_tex_control[0] = 0x4003ffc0;
				pgraph_celsius_icmd(&exp, 6, exp.bundle_tex_control[0], false);
			}
			exp.bundle_tex_control[1] = 0x4003ffc0;
			pgraph_celsius_icmd(&exp, 7, exp.bundle_tex_control[1], false);
			if (ac)
				exp.bundle_rc_in_color[which] = rc_in;
			else
				exp.bundle_rc_in_alpha[which] = rc_in;
			pgraph_celsius_icmd(&exp, 0x10 + ac * 2 + which, rc_in, false);
			if (which == 0)
				pgraph_celsius_icmd(&exp, 0x11 + ac * 2, (ac ? exp.bundle_rc_in_color : exp.bundle_rc_in_alpha)[1], false);
			if (ac)
				insrt(exp.bundle_rc_out_color[which], 0, 18, rc_out);
			else
				insrt(exp.bundle_rc_out_alpha[which], 0, 18, rc_out);
			if (which == 1) {
				insrt(exp.bundle_rc_out_color[1], 28, 2, extr(exp.celsius_config_c, 18, 2) == 3 ? 1 : 2);
			}
			pgraph_celsius_icmd(&exp, 0x16 + ac * 2 + which, (ac ? exp.bundle_rc_out_color : exp.bundle_rc_out_alpha)[which], false);
			if (which == 1 && !ac)
				pgraph_celsius_icmd(&exp, 0x19, exp.bundle_rc_out_color[1], false);
			pgraph_celsius_icmd(&exp, 0x1b, exp.bundle_rc_final_b, true);
		}
	}
public:
	MthdEmuD3D6CombineControl(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, int which, int ac)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd), which(which), ac(ac) {}
};

class MthdEmuD3D6CombineFactor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_factor_0[0] = val;
			exp.bundle_rc_factor_1[0] = val;
			pgraph_celsius_icmd(&exp, 0x14, val, false);
			pgraph_celsius_icmd(&exp, 0x15, val, true);
		}
		insrt(exp.valid[1], 24, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56Blend : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 9, 3, 0);
			if (rnd() & 3)
				insrt(val, 13, 3, 0);
			if (rnd() & 3)
				insrt(val, 17, 3, 0);
			if (rnd() & 3)
				insrt(val, 21, 3, 0);
			if (rnd() & 1)
				insrt(val, 0, 4, 0);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (cls == 0x54 || cls == 0x94) {
			if (extr(val, 0, 4) < 1 || extr(val, 0, 4) > 8)
				return false;
		} else {
			if (extr(val, 0, 4))
				return false;
		}
		if (extr(val, 4, 2) < 1 || extr(val, 4, 2) > 2)
			return false;
		if (!extr(val, 6, 2))
			return false;
		if (extr(val, 9, 3))
			return false;
		if (extr(val, 13, 3))
			return false;
		if (extr(val, 17, 3))
			return false;
		if (extr(val, 21, 3))
			return false;
		if (extr(val, 24, 4) < 1 || extr(val, 24, 4) > 0xb)
			return false;
		if (extr(val, 28, 4) < 1 || extr(val, 28, 4) > 0xb)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		int colreg = extr(val, 12, 1) ? 0xe : 0xc;
		int op = extr(val, 0, 4);
		bool is_d3d6 = cls == 0x55 || cls == 0x95;
		if (is_d3d6)
			op = 0;
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_rc_final_a = extr(val, 16, 1) ? 0x13000300 | colreg << 16 : colreg;
			exp.bundle_rc_final_b = 0x1c80;
			insrt(exp.bundle_blend, 0, 3, 2);
			insrt(exp.bundle_blend, 3, 1, extr(val, 20, 1));
			insrt(exp.bundle_blend, 4, 4, extr(val, 24, 4) - 1);
			insrt(exp.bundle_blend, 8, 4, extr(val, 28, 4) - 1);
			insrt(exp.bundle_blend, 12, 5, 0);
			insrt(exp.bundle_config_b, 0, 5, 0);
			insrt(exp.bundle_config_b, 5, 1, extr(val, 12, 1));
			insrt(exp.bundle_config_b, 6, 1, extr(val, 8, 1));
			insrt(exp.bundle_config_b, 7, 1, extr(val, 7, 1));
			insrt(exp.bundle_config_b, 8, 1, extr(val, 16, 1));
			insrt(exp.celsius_xf_misc_a, 28, 1, 1);
			insrt(exp.celsius_xf_misc_b, 2, 1, 0);
			insrt(exp.bundle_rc_out_color[1], 27, 1, extr(val, 5, 1));
			if (cls == 0x55 || cls == 0x95) {
				insrt(exp.bundle_rc_out_color[1], 28, 2, extr(exp.celsius_config_c, 18, 2) == 3 ? 1 : 2);
			} else {
				insrt(exp.bundle_rc_out_color[1], 28, 2, 1);
			}
			insrt(exp.celsius_xf_misc_b, 2, 1,
				extr(exp.bundle_config_b, 6, 1) &&
				!extr(exp.bundle_tex_format[0], 27, 1) &&
				!extr(exp.bundle_tex_format[0], 31, 1));
			insrt(exp.celsius_xf_misc_b, 16, 1,
				is_d3d6 && extr(exp.bundle_config_b, 6, 1) &&
				!extr(exp.bundle_tex_format[1], 27, 1) &&
				!extr(exp.bundle_tex_format[1], 31, 1));
			if (op == 1 || op == 7) {
				exp.bundle_rc_in_alpha[0] = 0x18200000;
				exp.bundle_rc_in_color[0] = 0x08200000;
				exp.bundle_rc_out_alpha[0] = 0x00c00;
				exp.bundle_rc_out_color[0] = 0x00c00;
			} else if (op == 2) {
				exp.bundle_rc_in_alpha[0] = 0x18200000;
				exp.bundle_rc_in_color[0] = 0x08040000;
				exp.bundle_rc_out_alpha[0] = 0x00c00;
				exp.bundle_rc_out_color[0] = 0x00c00;
			} else if (op == 3) {
				exp.bundle_rc_in_alpha[0] = 0x14200000;
				exp.bundle_rc_in_color[0] = 0x08180438;
				exp.bundle_rc_out_alpha[0] = 0x00c00;
				exp.bundle_rc_out_color[0] = 0x00c00;
			} else if (op == 4) {
				exp.bundle_rc_in_alpha[0] = 0x18140000;
				exp.bundle_rc_in_color[0] = 0x08040000;
				exp.bundle_rc_out_alpha[0] = 0x00c00;
				exp.bundle_rc_out_color[0] = 0x00c00;
			} else if (op == 5) {
				exp.bundle_rc_in_alpha[0] = 0x14201420;
				exp.bundle_rc_in_color[0] = 0x04200820;
				exp.bundle_rc_out_alpha[0] = 0x04c00;
				exp.bundle_rc_out_color[0] = 0x04c00;
			} else if (op == 6) {
				exp.bundle_rc_in_alpha[0] = 0x14201420;
				exp.bundle_rc_in_color[0] = 0x04200408;
				exp.bundle_rc_out_alpha[0] = 0x04c00;
				exp.bundle_rc_out_color[0] = 0x04c00;
			} else if (op == 8) {
				exp.bundle_rc_in_alpha[0] = 0x14200000;
				exp.bundle_rc_in_color[0] = 0x08200420;
				exp.bundle_rc_out_alpha[0] = 0x00c00;
				exp.bundle_rc_out_color[0] = 0x00c00;
			}
			if (cls == 0x54 || cls == 0x94) {
				pgraph_celsius_icmd(&exp, 0x10, exp.bundle_rc_in_alpha[0], false);
				pgraph_celsius_icmd(&exp, 0x12, exp.bundle_rc_in_color[0], false);
				pgraph_celsius_icmd(&exp, 0x16, exp.bundle_rc_out_alpha[0], false);
				pgraph_celsius_icmd(&exp, 0x18, exp.bundle_rc_out_color[0], false);
			}
			pgraph_celsius_icmd(&exp, 0x19, exp.bundle_rc_out_color[1], false);
			pgraph_celsius_icmd(&exp, 0x1a, exp.bundle_rc_final_a, false);
			pgraph_celsius_icmd(&exp, 0x1b, exp.bundle_rc_final_b, false);
			pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, false);
			pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, true);
			exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
			exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
		}
		insrt(exp.valid[1], 19, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56Config : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 8, 4, 1);
			if (rnd() & 3)
				insrt(val, 15, 1, 0);
			if (rnd() & 3)
				insrt(val, 16, 4, 1);
			if (rnd() & 3)
				insrt(val, 25, 5, 0);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 8, 4) < 1 || extr(val, 8, 4) > 8)
			return false;
		if (extr(val, 15, 1))
			return false;
		if (extr(val, 16, 4) < 1 || extr(val, 16, 4) > 8)
			return false;
		if (extr(val, 20, 2) < 1)
			return false;
		if (extr(val, 25, 5) && (cls == 0x54 || cls == 0x94))
			return false;
		if (extr(val, 30, 2) < 1 || extr(val, 30, 2) > 2)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		bool is_d3d6 = cls == 0x55 || cls == 0x95;
		if (!extr(exp.nsource, 1, 1)) {
			uint32_t rval = val & 0x3fcf5fff;
			insrt(rval, 16, 4, extr(val, 16, 4) - 1);
			insrt(rval, 8, 4, extr(val, 8, 4) - 1);
			if (!is_d3d6)
				insrt(rval, 25, 5, 0x1e);
			int scull = extr(val, 20, 2);
			insrt(exp.bundle_config_a, 0, 30, rval);
			pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, false);
			insrt(exp.bundle_raster, 0, 21, 0);
			insrt(exp.bundle_raster, 21, 2, scull == 2 ? 1 : 2);
			insrt(exp.bundle_raster, 23, 5, 8);
			insrt(exp.bundle_raster, 28, 2, scull != 1);
			insrt(exp.bundle_raster, 29, 1, extr(val, 31, 1));
			insrt(exp.celsius_xf_misc_a, 28, 1, 1);
			insrt(exp.celsius_xf_misc_a, 29, 1, !extr(val, 13, 1));
			insrt(exp.celsius_xf_misc_b, 0, 1, 0);
			insrt(exp.celsius_xf_misc_b, 14, 1, 0);
			if (!is_d3d6) {
				exp.bundle_stencil_func = 0x70;
				pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_func, false);
			}
			pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
			exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
		}
		insrt(exp.valid[1], 17, 1, 1);
		if (!is_d3d6)
			insrt(exp.valid[1], 18, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D6StencilFunc : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1)
			val &= ~0xe;
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (extr(val, 1, 3))
			return false;
		if (extr(val, 4, 4) < 1 || extr(val, 4, 4) > 8)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		insrt(exp.valid[1], 18, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			uint32_t rval = val & 0xfffffff1;
			insrt(rval, 4, 4, extr(val, 4, 4) - 1);
			exp.bundle_stencil_func = rval;
			pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_func, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D6StencilOp : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= ~0xfffff000;
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
		if (extr(val, 0, 4) < 1 || extr(val, 0, 4) > 8)
			return false;
		if (extr(val, 4, 4) < 1 || extr(val, 4, 4) > 8)
			return false;
		if (extr(val, 8, 4) < 1 || extr(val, 8, 4) > 8)
			return false;
		if (extr(val, 12, 20))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		insrt(exp.valid[1], 20, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_stencil_op = val & 0xfff;
			pgraph_celsius_icmd(&exp, 0x1e, exp.bundle_stencil_op, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56FogColor : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		insrt(exp.valid[1], 13, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			exp.bundle_fog_color = val;
			pgraph_celsius_icmd(&exp, 0x23, val, true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuEmuD3D0TexFormat : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			val &= 0xff31ffff;
			val ^= 1 << (rnd() & 0x1f);
		}
		adjust_orig_icmd(&orig);
	}
	bool is_valid_val() override {
		if (val & ~0xff31ffff)
			return false;
		int max_l = extr(val, 28, 4);
		int min_l = extr(val, 24, 4);
		if (min_l < 2)
			return false;
		if (min_l > max_l)
			return false;
		if (max_l > 0xb)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		int fmt = 5;
		int sfmt = extr(val, 20, 4);
		if (sfmt == 0)
			fmt = 2;
		else if (sfmt == 1)
			fmt = 3;
		else if (sfmt == 2)
			fmt = 4;
		else if (sfmt == 3)
			fmt = 5;
		int max_l = extr(val, 28, 4);
		int min_l = extr(val, 24, 4);
		insrt(exp.valid[1], 21, 1, 1);
		insrt(exp.valid[1], 15, 1, 1);
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 2; i++) {
				insrt(exp.bundle_tex_format[i], 1, 1, 0);
				insrt(exp.bundle_tex_format[i], 2, 1, 0);
				insrt(exp.bundle_tex_format[i], 6, 1, 0);
				insrt(exp.bundle_tex_format[i], 7, 5, fmt);
				insrt(exp.bundle_tex_format[i], 12, 4, max_l - min_l + 1);
				insrt(exp.bundle_tex_format[i], 16, 4, max_l);
				insrt(exp.bundle_tex_format[i], 20, 4, max_l);
				pgraph_celsius_icmd(&exp, 4 + i, exp.bundle_tex_format[i], false);
			}
			exp.bundle_tex_control[0] = 0x4003ffc0 | extr(val, 16, 2);
			exp.bundle_tex_control[1] = 0x3ffc0 | extr(val, 16, 2);
			pgraph_celsius_icmd(&exp, 6, exp.bundle_tex_control[0], false);
			pgraph_celsius_icmd(&exp, 7, exp.bundle_tex_control[1], true);
		}
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuEmuD3D0TexFilter : public SingleMthdTest {
	void adjust_orig_mthd() override {
		adjust_orig_icmd(&orig);
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_tex_filter[0], 0, 13, extrs(val, 16, 8) << 4);
			insrt(exp.bundle_tex_filter[1], 0, 13, extrs(val, 16, 8) << 4);
			pgraph_celsius_icmd(&exp, 0xe, exp.bundle_tex_filter[0], false);
			pgraph_celsius_icmd(&exp, 0xf, exp.bundle_tex_filter[1], true);
		}
		insrt(exp.valid[1], 23, 1, 1);
		insrt(exp.valid[1], 16, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuEmuD3D0Config : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 0, 4, 1);
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
		if (extr(val, 9, 1))
			return false;
		if (!extr(val, 12, 2))
			return false;
		if (extr(val, 14, 1))
			return false;
		if (extr(val, 16, 4) < 1 || extr(val, 16, 4) > 8)
			return false;
		if (extr(val, 20, 4) > 4)
			return false;
		if (extr(val, 24, 4) > 4)
			return false;
		if (extr(val, 0, 4) > 2)
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		int filt = 1;
		int origin = 0;
		int sinterp = extr(val, 0, 4);
		if (sinterp == 0)
			origin = 1;
		if (sinterp == 2)
			filt = 2;
		int wrapu = extr(val, 4, 2);
		if (wrapu == 0)
			wrapu = 9;
		int wrapv = extr(val, 6, 2);
		if (wrapv == 0)
			wrapv = 9;
		uint32_t rc_in_alpha_a = 0x18;;
		uint32_t rc_in_alpha_b = 0x14;;
		if (!extr(val, 8, 1))
			rc_in_alpha_b = 0x20;
		uint32_t rc_final_b = 0x1c80;
		int srcmode = extr(val, 10, 2);
		if (srcmode == 1)
			rc_final_b |= 0x2020;
		if (srcmode == 2)
			rc_final_b |= 0x2000;
		if (srcmode == 3) {
			rc_in_alpha_a = 0x20;
			rc_in_alpha_b = 0x20;
		}
		int scull = extr(val, 12, 2);
		int sblend = 4, dblend = 5;
		if (extr(val, 29, 1)) {
			dblend = 9;
			sblend = 8;
		}
		if (extr(val, 28, 1))
			sblend = dblend = 1;
		if (extr(val, 30, 1))
			dblend = 0;
		if (extr(val, 31, 1))
			sblend = 0;
		if (!extr(exp.nsource, 1, 1)) {
			for (int i = 0; i < 2; i++) {
				insrt(exp.bundle_tex_format[i], 4, 1, origin);
				insrt(exp.bundle_tex_format[i], 24, 4, wrapu);
				insrt(exp.bundle_tex_format[i], 28, 4, wrapv);
				pgraph_celsius_icmd(&exp, 4 + i, exp.bundle_tex_format[i], false);
			}
			for (int i = 0; i < 2; i++) {
				insrt(exp.bundle_tex_filter[i], 24, 3, filt);
				insrt(exp.bundle_tex_filter[i], 28, 3, filt);
				pgraph_celsius_icmd(&exp, 0xe + i, exp.bundle_tex_filter[i], false);
			}
			exp.bundle_rc_in_alpha[0] = rc_in_alpha_a << 24 | rc_in_alpha_b << 16;
			pgraph_celsius_icmd(&exp, 0x10, exp.bundle_rc_in_alpha[0], false);
			exp.bundle_rc_in_color[0] = 0x08040000;
			pgraph_celsius_icmd(&exp, 0x12, exp.bundle_rc_in_color[0], false);
			exp.bundle_rc_factor_0[0] = val;
			pgraph_celsius_icmd(&exp, 0x14, exp.bundle_rc_factor_0[0], false);
			exp.bundle_rc_factor_1[0] = val;
			pgraph_celsius_icmd(&exp, 0x15, exp.bundle_rc_factor_1[0], false);
			exp.bundle_rc_out_alpha[0] = 0xc00;
			pgraph_celsius_icmd(&exp, 0x16, exp.bundle_rc_out_alpha[0], false);
			exp.bundle_rc_out_color[0] = 0xc00;
			pgraph_celsius_icmd(&exp, 0x18, exp.bundle_rc_out_color[0], false);
			insrt(exp.bundle_rc_out_color[1], 27, 3, 2);
			pgraph_celsius_icmd(&exp, 0x19, exp.bundle_rc_out_color[1], false);
			exp.bundle_rc_final_a = 0xc;
			pgraph_celsius_icmd(&exp, 0x1a, exp.bundle_rc_final_a, false);
			exp.bundle_rc_final_b = rc_final_b;
			pgraph_celsius_icmd(&exp, 0x1b, exp.bundle_rc_final_b, false);
			insrt(exp.bundle_config_a, 14, 1, !!extr(val, 20, 3));
			insrt(exp.bundle_config_a, 16, 4, extr(val, 16, 4) - 1);
			insrt(exp.bundle_config_a, 22, 1, 1);
			insrt(exp.bundle_config_a, 23, 1, extr(val, 15, 1));
			insrt(exp.bundle_config_a, 24, 1, !!extr(val, 20, 3));
			insrt(exp.bundle_config_a, 25, 1, 0);
			insrt(exp.bundle_config_a, 26, 1, 0);
			insrt(exp.bundle_config_a, 27, 3, extr(val, 24, 3) ? 0x7 : 0);
			pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, false);
			exp.bundle_stencil_func = 0x70;
			pgraph_celsius_icmd(&exp, 0x1d, exp.bundle_stencil_func, false);
			exp.bundle_stencil_op = 0x222;
			pgraph_celsius_icmd(&exp, 0x1e, exp.bundle_stencil_op, false);
			insrt(exp.bundle_config_b, 0, 9, 0x1c0);
			pgraph_celsius_icmd(&exp, 0x1f, exp.bundle_config_b, false);
			exp.bundle_blend = 0xa | sblend << 4 | dblend << 8;
			pgraph_celsius_icmd(&exp, 0x20, exp.bundle_blend, false);
			insrt(exp.bundle_raster, 0, 21, 0);
			insrt(exp.bundle_raster, 21, 2, scull == 3 ? 1 : 2);
			insrt(exp.bundle_raster, 23, 5, 8);
			insrt(exp.bundle_raster, 28, 2, scull != 1);
			pgraph_celsius_icmd(&exp, 0x22, exp.bundle_raster, true);
			exp.celsius_pipe_junk[2] = exp.celsius_xf_misc_a;
			exp.celsius_pipe_junk[3] = exp.celsius_xf_misc_b;
		}
		insrt(exp.valid[1], 17, 1, 1);
		insrt(exp.valid[1], 24, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuEmuD3D0Alpha : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			if (rnd() & 3)
				insrt(val, 8, 4, 1);
			val &= ~0xfffff000;
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
		if (extr(val, 8, 4) < 1 || extr(val, 8, 4) > 8)
			return false;
		if (extr(val, 12, 20))
			return false;
		return true;
	}
	void emulate_mthd() override {
		pgraph_celsius_pre_icmd(&exp);
		if (!extr(exp.nsource, 1, 1)) {
			insrt(exp.bundle_config_a, 0, 8, val);
			insrt(exp.bundle_config_a, 8, 4, extr(val, 8, 4) - 1);
			insrt(exp.bundle_config_a, 12, 1, 1);
			pgraph_celsius_icmd(&exp, 0x1c, exp.bundle_config_a, true);
		}
		insrt(exp.valid[1], 18, 1, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuEmuD3D0TlvFogTri : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[8] = pgraph_celsius_ub_to_float(extr(val, 16, 8));
			exp.celsius_pipe_vtx[9] = pgraph_celsius_ub_to_float(extr(val, 8, 8));
			exp.celsius_pipe_vtx[10] = pgraph_celsius_ub_to_float(extr(val, 0, 8));
			exp.celsius_pipe_vtx[11] = pgraph_celsius_ub_to_float(extr(val, 24, 8));
		}
		exp.misc24[2] = val & 0xffffff;
		insrt(exp.valid[1], 0, 7, 1);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvColor : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[4] = pgraph_celsius_ub_to_float(extr(val, 16, 8));
			exp.celsius_pipe_vtx[5] = pgraph_celsius_ub_to_float(extr(val, 8, 8));
			exp.celsius_pipe_vtx[6] = pgraph_celsius_ub_to_float(extr(val, 0, 8));
			exp.celsius_pipe_vtx[7] = pgraph_celsius_ub_to_float(extr(val, 24, 8));
		}
		insrt(exp.valid[1], 1, 1, 1);
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		insrt(exp.valid[0], vidx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvFogCol1 : public SingleMthdTest {
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[8] = pgraph_celsius_ub_to_float(extr(val, 16, 8));
			exp.celsius_pipe_vtx[9] = pgraph_celsius_ub_to_float(extr(val, 8, 8));
			exp.celsius_pipe_vtx[10] = pgraph_celsius_ub_to_float(extr(val, 0, 8));
			exp.celsius_pipe_vtx[11] = pgraph_celsius_ub_to_float(extr(val, 24, 8));
		}
		insrt(exp.valid[1], 0, 1, 1);
		insrt(exp.valid[0], idx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvX : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 0, 23, 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (cls == 0x48) {
			int e = extr(val, 23, 8);
			if (e > 0x7f + 10)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[0] = val;
			if (chipset.chipset != 0x10)
				exp.celsius_pipe_vtx[1] = 0;
			exp.celsius_pipe_vtx[2] = 0;
			exp.celsius_pipe_vtx[3] = 0x3f800000;
		}
		insrt(exp.valid[1], 5, 1, 1);
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		insrt(exp.valid[0], vidx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvY : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 0, 23, 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (cls == 0x48) {
			int e = extr(val, 23, 8);
			if (e > 0x7f + 10)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[1] = val;
			exp.celsius_pipe_vtx[2] = 0;
			exp.celsius_pipe_vtx[3] = 0x3f800000;
		}
		insrt(exp.valid[1], 4, 1, 1);
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		insrt(exp.valid[0], vidx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvZ : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 0, 23, 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (cls == 0x48) {
			if (extr(val, 31, 1))
				return false;
			int e = extr(val, 23, 8);
			if (e >= 0x7f)
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[2] = val;
		}
		insrt(exp.valid[1], 3, 1, 1);
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		insrt(exp.valid[0], vidx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvW : public SingleMthdTest {
	void adjust_orig_mthd() override {
		if (rnd() & 1) {
			insrt(val, 0, 23, 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (cls == 0x48) {
			if (extr(val, 31, 1))
				return false;
		}
		return true;
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			exp.celsius_pipe_vtx[3] = val;
		}
		insrt(exp.valid[1], 2, 1, 1);
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		insrt(exp.valid[0], vidx, 1, 0);
	}
	using SingleMthdTest::SingleMthdTest;
};

class MthdEmuD3D56TlvUv : public SingleMthdTest {
	int which_uv, which_vtx;
	bool fin;
	void adjust_orig_mthd() override {
		if (fin) {
			insrt(orig.notify, 0, 1, 0);
			adjust_orig_icmd(&orig);
		}
		if (cls == 0x48 && fin) {
			// XXX: test me
			exp.valid[0] = 0;
			exp.valid[1] = 0;
		}
		if (rnd() & 1) {
			insrt(val, 0, 23, 0);
			val ^= 1 << (rnd() & 0x1f);
		}
	}
	bool is_valid_val() override {
		if (cls == 0x48) {
			int e = extr(val, 23, 8);
			if (e > 0x7f + 10)
				return false;
		}
		return true;
	}
	void emulate_mthd_pre() override {
		if (cls == 0x48 && fin) {
			exp.misc24[1] = exp.misc24[2];
		}
	}
	void emulate_mthd() override {
		if (!extr(exp.nsource, 1, 1)) {
			if (chipset.chipset != 0x10)
				exp.celsius_pipe_vtx[(3 + which_vtx) * 4 + 1] = 0;
			exp.celsius_pipe_vtx[(3 + which_vtx) * 4 + 2] = 0;
			exp.celsius_pipe_vtx[(3 + which_vtx) * 4 + 3] = 0x3f800000;
			exp.celsius_pipe_vtx[(3 + which_vtx) * 4 + which_uv] = val;
		}
		int vidx = (cls == 0x48 ? extr(exp.misc24[2], 0, 4) : idx);
		if (!which_uv || !which_vtx)
			insrt(exp.valid[1], 6 + which_vtx * 2 + which_uv, 1, 1);
		if (fin) {
			// XXX: test this thing
			skip = true;
			uint32_t msk = (cls == 0x55 || cls == 0x95 ? 0x1ff : 0x7f);
			int ovidx = extr(exp.valid[0], 16, 4);
			if ((exp.valid[1] & msk) == msk) {
				insrt(exp.valid[0], vidx, 1, 1);
				if (cls != 0x48)
					insrt(exp.valid[0], 16, 4, vidx);
				exp.valid[1] &= ~msk;
			} else if (!extr(exp.valid[0], ovidx, 1)) {
				nv04_pgraph_blowup(&exp, 0x4000);
			}
			// sounds buggy...
			if (extr(exp.debug[3], 5, 1) && cls != 0x48)
				vidx = extr(exp.valid[0], 16, 4);
			if (cls == 0x55) {
				vidx &= 7;
				// ...
			} else {
				// ...
			}
			if (cls == 0x48) {
				// XXX
				skip = true;
			}
			if (!extr(exp.surf_format, 8, 4) && extr(exp.debug[3], 22, 1))
				nv04_pgraph_blowup(&exp, 0x0200);
		} else {
			insrt(exp.valid[0], vidx, 1, 0);
		}
	}
public:
	MthdEmuD3D56TlvUv(hwtest::TestOptions &opt, uint32_t seed, const std::string &name, int trapbit, uint32_t cls, uint32_t mthd, uint32_t num, uint32_t stride, int which_vtx, int which_uv, bool fin)
	: SingleMthdTest(opt, seed, name, trapbit, cls, mthd, num, stride), which_uv(which_uv), which_vtx(which_vtx), fin(fin) {}
};

std::vector<SingleMthdTest *> EmuEmuD3D0::mthds() {
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaGrobj(opt, rnd(), "dma_tex", 2, cls, 0x184, 0, DMA_R | DMA_ALIGN),
		new MthdCtxClip(opt, rnd(), "ctx_clip", 3, cls, 0x188),
		new MthdCtxSurf(opt, rnd(), "ctx_color", 4, cls, 0x18c, 2),
		new MthdCtxSurf(opt, rnd(), "ctx_zeta", 5, cls, 0x190, 3),
		new MthdMissing(opt, rnd(), "missing", -1, cls, 0x200, 4),
		new MthdEmuD3D56TexOffset(opt, rnd(), "tex_offset", 6, cls, 0x304, 3),
		new MthdEmuEmuD3D0TexFormat(opt, rnd(), "tex_format", 7, cls, 0x308),
		new MthdEmuEmuD3D0TexFilter(opt, rnd(), "tex_filter", 8, cls, 0x30c),
		new MthdEmuD3D56FogColor(opt, rnd(), "fog_color", 9, cls, 0x310),
		new MthdEmuEmuD3D0Config(opt, rnd(), "config", 10, cls, 0x314),
		new MthdEmuEmuD3D0Alpha(opt, rnd(), "alpha", 11, cls, 0x318),
		new MthdEmuEmuD3D0TlvFogTri(opt, rnd(), "tlv_fog_tri", 12, cls, 0x1000, 0x80, 0x20),
		new MthdEmuD3D56TlvColor(opt, rnd(), "tlv_color", 13, cls, 0x1004, 0x80, 0x20),
		new MthdEmuD3D56TlvX(opt, rnd(), "tlv_x", 14, cls, 0x1008, 0x80, 0x20),
		new MthdEmuD3D56TlvY(opt, rnd(), "tlv_y", 15, cls, 0x100c, 0x80, 0x20),
		new MthdEmuD3D56TlvZ(opt, rnd(), "tlv_z", 16, cls, 0x1010, 0x80, 0x20),
		new MthdEmuD3D56TlvW(opt, rnd(), "tlv_rhw", 17, cls, 0x1014, 0x80, 0x20),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_u", 18, cls, 0x1018, 0x80, 0x20, 0, 0, false),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_v", 19, cls, 0x101c, 0x80, 0x20, 0, 1, true),
	};
}

std::vector<SingleMthdTest *> EmuD3D5::mthds() {
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_a", 2, cls, 0x184, 0, DMA_R | DMA_ALIGN),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_b", 3, cls, 0x188, 1, DMA_R | DMA_ALIGN),
		new MthdCtxSurf3D(opt, rnd(), "ctx_surf3d", 4, cls, 0x18c, cls == 0x54 ? SURF2D_NV4 : SURF2D_NV10),
		new MthdEmuD3D56TexColorKey(opt, rnd(), "tex_color_key", 5, cls, 0x300),
		new MthdEmuD3D56TexOffset(opt, rnd(), "tex_offset", 6, cls, 0x304, 1),
		new MthdEmuD3D56TexFormat(opt, rnd(), "tex_format", 7, cls, 0x308, 3),
		new MthdEmuD3D56TexFilter(opt, rnd(), "tex_filter", 8, cls, 0x30c, 3),
		new MthdEmuD3D56Blend(opt, rnd(), "blend", 9, cls, 0x310),
		new MthdEmuD3D56Config(opt, rnd(), "config", 10, cls, 0x314),
		new MthdEmuD3D56FogColor(opt, rnd(), "fog_color", 11, cls, 0x318),
		new MthdEmuD3D56TlvX(opt, rnd(), "tlv_x", 12, cls, 0x400, 0x10, 0x20),
		new MthdEmuD3D56TlvY(opt, rnd(), "tlv_y", 13, cls, 0x404, 0x10, 0x20),
		new MthdEmuD3D56TlvZ(opt, rnd(), "tlv_z", 14, cls, 0x408, 0x10, 0x20),
		new MthdEmuD3D56TlvW(opt, rnd(), "tlv_rhw", 15, cls, 0x40c, 0x10, 0x20),
		new MthdEmuD3D56TlvColor(opt, rnd(), "tlv_color", 16, cls, 0x410, 0x10, 0x20),
		new MthdEmuD3D56TlvFogCol1(opt, rnd(), "tlv_fog_col1", 17, cls, 0x414, 0x10, 0x20),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_u", 18, cls, 0x418, 0x10, 0x20, 0, 0, false),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_v", 19, cls, 0x41c, 0x10, 0x20, 0, 1, true),
		new UntestedMthd(opt, rnd(), "draw", 20, cls, 0x600, 0x40),
	};
}

std::vector<SingleMthdTest *> EmuD3D6::mthds() {
	return {
		new MthdNop(opt, rnd(), "nop", -1, cls, 0x100),
		new MthdNotify(opt, rnd(), "notify", 0, cls, 0x104),
		new MthdPmTrigger(opt, rnd(), "pm_trigger", -1, cls, 0x140),
		new MthdDmaNotify(opt, rnd(), "dma_notify", 1, cls, 0x180),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_a", 2, cls, 0x184, 0, DMA_R | DMA_ALIGN),
		new MthdDmaGrobj(opt, rnd(), "dma_tex_b", 3, cls, 0x188, 1, DMA_R | DMA_ALIGN),
		new MthdCtxSurf3D(opt, rnd(), "ctx_surf3d", 4, cls, 0x18c, cls == 0x55 ? SURF2D_NV4 : SURF2D_NV10),
		new MthdEmuD3D56TexOffset(opt, rnd(), "tex_0_offset", 5, cls, 0x308, 1),
		new MthdEmuD3D56TexOffset(opt, rnd(), "tex_1_offset", 6, cls, 0x30c, 2),
		new MthdEmuD3D56TexFormat(opt, rnd(), "tex_0_format", 7, cls, 0x310, 1),
		new MthdEmuD3D56TexFormat(opt, rnd(), "tex_1_format", 8, cls, 0x314, 2),
		new MthdEmuD3D56TexFilter(opt, rnd(), "tex_0_filter", 9, cls, 0x318, 1),
		new MthdEmuD3D56TexFilter(opt, rnd(), "tex_1_filter", 10, cls, 0x31c, 2),
		new MthdEmuD3D6CombineControl(opt, rnd(), "combine_0_control_alpha", 11, cls, 0x320, 0, 0),
		new MthdEmuD3D6CombineControl(opt, rnd(), "combine_0_control_color", 12, cls, 0x324, 0, 1),
		new MthdEmuD3D6CombineControl(opt, rnd(), "combine_1_control_alpha", 13, cls, 0x32c, 1, 0),
		new MthdEmuD3D6CombineControl(opt, rnd(), "combine_1_control_color", 14, cls, 0x330, 1, 1),
		new MthdEmuD3D6CombineFactor(opt, rnd(), "combine_factor", 15, cls, 0x334),
		new MthdEmuD3D56Blend(opt, rnd(), "blend", 16, cls, 0x338),
		new MthdEmuD3D56Config(opt, rnd(), "config", 17, cls, 0x33c),
		new MthdEmuD3D6StencilFunc(opt, rnd(), "stencil_func", 18, cls, 0x340),
		new MthdEmuD3D6StencilOp(opt, rnd(), "stencil_op", 19, cls, 0x344),
		new MthdEmuD3D56FogColor(opt, rnd(), "fog_color", 20, cls, 0x348),
		new MthdEmuD3D56TlvX(opt, rnd(), "tlv_x", 21, cls, 0x400, 8, 0x28),
		new MthdEmuD3D56TlvY(opt, rnd(), "tlv_y", 22, cls, 0x404, 8, 0x28),
		new MthdEmuD3D56TlvZ(opt, rnd(), "tlv_z", 23, cls, 0x408, 8, 0x28),
		new MthdEmuD3D56TlvW(opt, rnd(), "tlv_rhw", 24, cls, 0x40c, 8, 0x28),
		new MthdEmuD3D56TlvColor(opt, rnd(), "tlv_color", 25, cls, 0x410, 8, 0x28),
		new MthdEmuD3D56TlvFogCol1(opt, rnd(), "tlv_fog_col1", 26, cls, 0x414, 8, 0x28),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_u_0", 27, cls, 0x418, 8, 0x28, 0, 0, false),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_v_0", 28, cls, 0x41c, 8, 0x28, 0, 1, false),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_u_1", 29, cls, 0x420, 8, 0x28, 1, 0, false),
		new MthdEmuD3D56TlvUv(opt, rnd(), "tlv_v_1", 30, cls, 0x424, 8, 0x28, 1, 1, true),
		new UntestedMthd(opt, rnd(), "draw", 31, cls, 0x540, 0x30),
	};
}

}
}
