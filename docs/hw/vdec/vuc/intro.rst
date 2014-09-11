====================================
Overview of VP2/VP3/VP4 vµc hardware
====================================

.. contents::


Introduction
============

vµc is a microprocessor unit used as the second stage of the VP2 [in H.264
mode only], VP3 and VP4 video decoding pipelines. The same name is also used
to refer to the instruction set of this microprocessor. vµc's task is to read
decoded bitstream data written by VLD into the MBRING structure, do any
required calculations on this data, then construct instructions for the VP
stage regarding processing of the incoming macroblocks. The work required of
vµc is dependent on the codec and may include eg. motion vector derivation,
calculating quantization parameters, converting macroblock type to prediction
modes, etc.

On VP2, the vµc is located inside the PBSP engine [see vdec/vp2/pbsp.txt]. On
VP3 and VP4, it is located inside the PPDEC engine [see vdec/vp3/ppdec.txt].

The vµc unit is made of the following subunits:

- the vµc microcprocessor - oversees everything and does the calculations
  that are not performance-sensitive enough to be done in hardware
- MBRING input and parsing circuitry - reads bitstream data parsed by the VLD
- MVSURF input and output circuitry - the MVSURF is a storage buffer attached
  to all reference pictures in H.264 and to P pictures in VC-1, MPEG-4. It
  stores the motion vectors and other data used for direct prediction in B
  pictures. There are two MVSURFs that can be used: the output MVSURF that
  will store the data of the current picture, and the input MVSURF that
  should store the data for the first picture in L1 list [H.264] or the
  last P picture [other codecs]
- VPRINGs output circuitry [VP2 only] - the VPRINGs are ring buffers filled
  by vµc with instructions for various VP subunits. There are three VPRINGs:
  VPRING_DEBLOCK used for deblocking commands, VPRIND_RESIDUAL used for the
  residual transform coefficients, and VPRINT_CTRL used for the motion
  vectors and other control data.
- direct VP connection [VP3, VP4 only] - the VP3+ vµc is directly connected
  to the VP engine, instead of relying on ring buffers in memory.


.. _pbsp-vuc-mmio:

The MMIO registers - VP2
========================

The vµc registers are located in PBSP XLMI space at addresses 0x08000:0x10000
[BAR0 addresses 0x103200:0x103400]. They are:

08000:0a000/103200:103280: DATA - vµc microprocessor data space
							[vdec/vuc/isa.txt]
0a000/103280: ICNT - executed instructions counter, aliased to vµc special
              register $sr15 [$icnt]
0a100/103284: WDCNT - watchdog count - when ICNT reaches WDCNT value and WDCNT
              is not equal to 0xffff, a watchdog interrupt is raised
0a200/103288: CODE_CONTROL - code execution control	[vdec/vuc/isa.txt]
0a300/10328c: CODE_WINDOW - code access window		[vdec/vuc/isa.txt]
0a400/103290: H2V - host to vµc scratch register	[vdec/vuc/isa.txt]
0a500/103294: V2H - vµc to host scratch register	[vdec/vuc/isa.txt]
0a600/103298: PARM - sequence/picture/slice parameters required by vµc
              hardware, aliased to vµc special register $sr7 [$parm]
0a700/10329c: PC - program counter			[vdec/vuc/isa.txt]
0a800/1032a0: VPRING_RESIDUAL.OFFSET - the VPRING_RESIDUAL offset
0a900/1032a4: VPRING_RESIDUAL.HALT_POS - the VPRING_RESIDUAL halt position
0aa00/1032a8: VPRING_RESIDUAL.WRITE_POS - the VPRING_RESIDUAL write position
0ab00/1032ac: VPRING_RESIDUAL.SIZE - the VPRING_RESIDUAL size
0ac00/1032b0: VPRING_CTRL.OFFSET - the VPRING_CTRL offset
0ad00/1032b4: VPRING_CTRL.HALT_POS - the VPRING_CTRL halt position
0ae00/1032b8: VPRING_CTRL.WRITE_POS - the VPRING_CTRL write position
0af00/1032bc: VPRING_CTRL.SIZE - the VPRING_CTRL size
0b000/1032c0: VPRING_DEBLOCK.OFFSET - the VPRING_DEBLOCK offset
0b100/1032c4: VPRING_DEBLOCK.HALT_POS - the VPRING_DEBLOCK halt position
0b200/1032c8: VPRING_DEBLOCK.WRITE_POS - the VPRING_DEBLOCK write position
0b300/1032cc: VPRING_DEBLOCK.SIZE - the VPRING_DEBLOCK size
0b400/1032d0: VPRING_TRIGGER - flush/resume triggers the for VPRINGs
0b500/1032d4: INTR - interrupt status
0b600/1032d8: INTR_EN - interrupt enable mask
0b700/1032dc: VPRING_ENABLE - enables VPRING access
0b800/1032e0: MVSURF_IN_OFFSET - MVSURF_IN offset	[vdec/vuc/mvsurf.txt]
0b900/1032e4: MVSURF_IN_PARM - MVSURF_IN parameters	[vdec/vuc/mvsurf.txt]
0ba00/1032e8: MVSURF_IN_LEFT - MVSURF_IN data left	[vdec/vuc/mvsurf.txt]
0bb00/1032ec: MVSURF_IN_POS - MVSURF_IN position	[vdec/vuc/mvsurf.txt]
0bc00/1032f0: MVSURF_OUT_OFFSET - MVSURF_OUT offset	[vdec/vuc/mvsurf.txt]
0bd00/1032f4: MVSURF_OUT_PARM - MVSURF_OUT parameters	[vdec/vuc/mvsurf.txt]
0be00/1032f8: MVSURF_OUT_LEFT - MVSURF_OUT space left	[vdec/vuc/mvsurf.txt]
0bf00/1032fc: MVSURF_OUT_POS - MVSURF_OUT position	[vdec/vuc/mvsurf.txt]
0c000/103300: MBRING_OFFSET - the MBRING offset
0c100/103304: MBRING_SIZE - the MBRING size
0c200/103308: MBRING_READ_POS - the MBRING read position
0c300/10330c: MBRING_READ_AVAIL - the bytes left to read in MBRING


.. _ppdec-io-vuc:

The MMIO registers - VP3/VP4
============================

The vµc registers are located in PPDEC falcon IO space at addresses 0x10000:0x14000
[BAR0 addresses 0x085400:0x085500]. They are:

10000:11000/085400:085440: DATA - vµc microprocessor data space
							[vdec/vuc/isa.txt]
11000/085440: CODE_CONTROL - code execution control	[vdec/vuc/isa.txt]
11100/085444: CODE_WINDOW - code access window		[vdec/vuc/isa.txt]
11200/085448: ICNT - executed instructions counter, aliased to vµc special
              register $sr15 [$icnt]
11300/08544c: WDCNT - watchdog count - when ICNT reaches WDCNT value and WDCNT
              is not equal to 0xffff, a watchdog interrupt is raised
11400/085450: H2V - host to vµc scratch register	[vdec/vuc/isa.txt]
11500/085454: V2H - vµc to host scratch register	[vdec/vuc/isa.txt]
11600/085458: PARM - sequence/picture/slice parameters required by vµc
              hardware, aliased to vµc special register $sr7 [$parm]
11700/08545c: PC - program counter			[vdec/vuc/isa.txt]
11800/085460: RPITAB - the address of refidx -> RPI translation table
11900/085464: REFTAB - the address of RPI -> VM address translation table
11a00/085468: BUSY - a status reg showing which subunits of vµc are busy
11c00/085470: INTR - interrupt status
11d00/085474: INTR_EN - interrupt enable mask
12000/085480: MVSURF_IN_ADDR - MVSURF_IN address	[vdec/vuc/mvsurf.txt]
12100/085484: MVSURF_IN_PARM - MVSURF_IN parameters	[vdec/vuc/mvsurf.txt]
12200/085488: MVSURF_IN_LEFT - MVSURF_IN data left	[vdec/vuc/mvsurf.txt]
12300/08548c: MVSURF_IN_POS - MVSURF_IN position	[vdec/vuc/mvsurf.txt]
12400/085490: MVSURF_OUT_ADDR - MVSURF_OUT address	[vdec/vuc/mvsurf.txt]
12500/085494: MVSURF_OUT_PARM - MVSURF_OUT parameters	[vdec/vuc/mvsurf.txt]
12600/085498: MVSURF_OUT_LEFT - MVSURF_OUT space left	[vdec/vuc/mvsurf.txt]
12700/08549c: MVSURF_OUT_POS - MVSURF_OUT position	[vdec/vuc/mvsurf.txt]
12800/0854a0: MBRING_OFFSET - the MBRING offset
12900/0854a4: MBRING_SIZE - the MBRING size
12a00/0854a8: MBRING_READ_POS - the MBRING read position
12b00/0854ac: MBRING_READ_AVAIL - the bytes left to read in MBRING
12c00/0854b0: ??? [XXX]
12d00/0854b4: ??? [XXX]
12e00/0854b8: ??? [XXX]
12f00/0854bc: STAT - control/status register		[vdec/vuc/isa.txt]
13000/0854c0: ??? [XXX]
13100/0854c4: ??? [XXX]


.. _ppdec-intr-vuc:

Interrupts
==========

.. todo:: write me
