.. _graph-2d-pattern:

==========
2D pattern
==========

.. contents::


Introduction
============

One of the configurable inputs to the bitwise operation and, on NV1:NV4,
the blending operation is the pattern. A pattern is an infinitely repeating
8x8, 64x1, or 1x64 image. There are two types of patterns:

- bitmap pattern: an arbitrary 2-color 8x8, 64x1, or 1x64 2-color image
- color pattern: an aribtrary 8x8 R8G8B8 image [NV4-]

The pattern can be set through the NV1-style \*_PATTERN context objects, or
through the G80-style unified 2d objects. For details on how and when the
pattern is used, see :ref:`graph-2d-pattern`.

The graph context used for pattern storage is made of:

- pattern type selection: bitmap or color [NV4-]
- bitmap pattern state:

  - shape selection: 8x8, 1x64, or 64x1
  - the bitmap: 2 32-bit words
  - 2 colors: A8R10G10B10 format [NV1:NV4]
  - 2 colors: 32-bit word + format selector each [NV4:G80]
  - 2 colors: 32-bit word each [G80-]
  - color format selection [G80-]
  - bitmap format selection [G80-]

- color pattern state [NV4-]:

  - 64 colors: R8G8B8 format

- pattern offset: 2 6-bit numbers [G80-]


.. _obj-pattern:

PATTERN objects
===============

The PATTERN object family deals with setting up the pattern. The objects in
this family are:

- objtype 0x06: NV1_PATTERN [NV1:NV4]
- class 0x0018: NV1_PATTERN [NV4:G80]
- class 0x0044: NV4_PATTERN [NV4:G84]

The methods for this family are:

0100   NOP [NV4-]              [graph/intro.txt]
0104   NOTIFY                   [graph/intro.txt]
0110   WAIT_FOR_IDLE [G80-]            [graph/intro.txt]
0140   PM_TRIGGER [NV40-?] [XXX]        [graph/intro.txt]
0180 N DMA_NOTIFY [NV4-]           [graph/intro.txt]
0200 O PATCH_IMAGE_OUTPUT [NV4:NV20]       [see below]
0300   COLOR_FORMAT [NV4-]         [see below]
0304   BITMAP_FORMAT [NV4-]            [see below]
0308   BITMAP_SHAPE             [see below]
030c   TYPE [NV4_PATTERN]          [see below]
0310+i*4, i<2   BITMAP_COLOR            [see below]
0318+i*4, i<2   BITMAP              [see below]
0400+i*4, i<16  COLOR_Y8 [NV4_PATTERN]     [see below]
0500+i*4, i<32  COLOR_R5G6B5 [NV4_PATTERN] [see below]
0600+i*4, i<32  COLOR_X1R5G5B5 [NV4_PATTERN]   [see below]
0700+i*4, i<64  COLOR_X8R8G8B8 [NV4_PATTERN]   [see below]

mthd 0x200: PATCH_IMAGE_OUTPUT [\*_PATTERN] [NV4:NV20]
  Reserved for plugging an image patchcord to output the pattern into.
Operation:
    throw(UNIMPLEMENTED_MTHD);


Pattern selection
=================

With the \*_PATTERN objects, the pattern type is selected using the TYPE and
BITMAP_SHAPE methods:

mthd 0x030c: TYPE [NV4_PATTERN]
  Sets the pattern type. One of:
    1: BITMAP
    2: COLOR
Operation::
    if (NV4:G80) {
        PATTERN_TYPE = param;
    } else {
        SHADOW_COMP2D.PATTERN_TYPE = param;
        if (SHADOW_COMP2D.PATTERN_TYPE == COLOR)
            PATTERN_SELECT = COLOR;
        else
            PATTERN_SELECT = SHADOW_COMP2D.PATTERN_BITMAP_SHAPE;
    }

mthd 0x308: BITMAP_SHAPE [\*_PATTERN]
  Sets the pattern shape. One of:
    0: 8x8
    1: 64x1
    2: 1x64
  On unified 2d objects, use the PATTERN_SELECT method instead.
Operation::
    if (param > 2)
        throw(INVALID_ENUM);
    if (NV1:G80) {
        PATTERN_BITMAP_SHAPE = param;
    } else {
        SHADOW_COMP2D.PATTERN_BITMAP_SHAPE = param;
        if (SHADOW_COMP2D.PATTERN_TYPE == COLOR)
            PATTERN_SELECT = COLOR;
        else
            PATTERN_SELECT = SHADOW_COMP2D.PATTERN_BITMAP_SHAPE;
    }

With the unified 2d objects, the pattern type is selected along with the
bitmap shape using the PATTERN_SELECT method:

mthd 0x02bc: PATTERN_SELECT [\*_2D]
  Sets the pattern type and shape. One of:
    0: BITMAP_8X8
    1: BITMAP_64X1
    2: BITMAP_1X64
    3: COLOR
Operation::
    if (param < 4)
        PATTERN_SELECT = SHADOW_2D.PATTERN_SELECT = param;
    else
        throw(INVALID_ENUM);


Pattern coordinates
===================

The pattern pixel is selected according to pattern coordinates: px, py. On
NV1:G80, the pattern coordinates are equal to absolute [ie. not
canvas-relative] coordinates in the destination surface. On G80+, an offset
can be added to the coordinates. The offset is set by the PATTERN_OFFSET
method:

mthd 0x02b0: PATTERN_OFFSET [\*_2D]
  Sets the pattern offset.
  bits 0-5: X offset
  bits 8-13: Y offset
Operation:
    PATTERN_OFFSET = param;

The offset values are added to the destination surface X, Y coordinates to
obtain px, py coordinates.


Bitmap pattern
==============

The bitmap pattern is made of three parts:

- two-color palette
- 64 bits of pattern: each bit describes one pixel of the pattern and selects
  which color to use
- shape selector: determines whether the bitmap is 8x8, 64x1, or 1x64

The color to use for given pattern coordinates is selected as follows::

    b6 bit;
    if (shape == 8x8)
        bit = (py&7) << 3 | (px&7);
    else if (shape == 64x1)
        bit = px & 0x3f;
    else if (shape == 1x64)
        bit = py & 0x3f;
    b1 pixel = PATTERN_BITMAP[bit[5]][bit[0:4]];
    color = PATTERN_BITMAP_COLOR[pixel];

On NV1:NV4, the color is internally stored in A8R10G10B10 format and
upconverted from the source format when submitted. On NV4:G80, it's stored
in the original format it was submitted with, and is annotated with the format
information as of the submission. On G80+, it's also stored as it was
submitted, but is not annotated with format information - the format used to
interpret it is the most recent pattern color format submitted.

On NV1:G80, the color and bitmap formats are stored in graph options for the
PATTERN object. On G80+, they're part of main graph state instead.

The methods dealing with bitmap patterns are:

mthd 0x300: COLOR_FORMAT [NV1_PATTERN] [NV4-]
  Sets the color format used for subsequent bitmap pattern colors. One of:
    1: X16A8Y8
    2: X16A1R5G5B5
    3: A8R8G8B8
Operation::
    switch (param) {
        case 1: cur_grobj.color_format = X16A8Y8; break;
        case 2: cur_grobj.color_format = X16A1R5G5B5; break;
        case 3: cur_grobj.color_format = A8R8G8B8; break;
        default: throw(INVALID_ENUM);
    }

mthd 0x300: COLOR_FORMAT [NV4_PATTERN]
  Sets the color format used for subsequent bitmap pattern colors. One of:
    1: A16R5G6B5
    2: X16A1R5G5B5
    3: A8R8G8B8
Operation::
    if (NV1:NV4) {
        switch (param) {
            case 1: cur_grobj.color_format = A16R5G6B5; break;
            case 2: cur_grobj.color_format = X16A1R5G5B5; break;
            case 3: cur_grobj.color_format = A8R8G8B8; break;
            default: throw(INVALID_ENUM);
        }
    } else {
        SHADOW_COMP2D.PATTERN_COLOR_FORMAT = param;
        switch (param) {
            case 1: PATTERN_COLOR_FORMAT = A16R5G6B5; break;
            case 2: PATTERN_COLOR_FORMAT = X16A1R5G5B5; break;
            case 3: PATTERN_COLOR_FORMAT = A8R8G8B8; break;
            default: throw(INVALID_ENUM);
        }
    }

mthd 0x2e8: PATTERN_COLOR_FORMAT [G80_2D]
  Sets the color format used for bitmap pattern colors. One of:
    0: A16R5G6B5
    1: X16A1R5G5B5
    2: A8R8G8B8
    3: X16A8Y8
    4: ??? [XXX]
    5: ??? [XXX]
Operation::
    if (param < 6)
        PATTERN_COLOR_FORMAT = SHADOW_2D.PATTERN_COLOR_FORMAT = param;
    else
        throw(INVALID_ENUM);

mthd 0x304: BITMAP_FORMAT [\*_PATTERN] [NV4-]
  Sets the bitmap format used for subsequent pattern bitmaps. One of:
    1: LE
    2: CGA6
Operation::
    if (NV4:G80) {
        switch (param) {
            case 1: cur_grobj.bitmap_format = LE; break;
            case 2: cur_grobj.bitmap_format = CGA6; break;
            default: throw(INVALID_ENUM);
        }
    } else {
        switch (param) {
            case 1: PATTERN_BITMAP_FORMAT = LE; break;
            case 2: PATTERN_BITMAP_FORMAT = CGA6; break;
            default: throw(INVALID_ENUM);
        }
    }

mthd 0x2ec: PATTERN_BITMAP_FORMAT [\*_PATTERN]
  Sets the bitmap format used for pattern bitmaps. One of:
    0: LE
    1: CGA6
Operation::
    if (param < 2)
        PATTERN_BITMAP_FORMAT = param;
    else
        throw(INVALID_ENUM);

mthd 0x310+i*4, i<2: BITMAP_COLOR [\*_PATTERN]
mthd 0x2f0+i*4, i<2: PATTERN_BITMAP_COLOR [\*_2D]
  Sets the colors used for bitmap pattern. i=0 sets the color used for pixels
  corresponding to '0' bits in the pattern, i=1 sets the color used for '1'.
Operation::
    if (NV1:NV4) {
        PATTERN_BITMAP_COLOR[i].B = get_color_b10(cur_grobj, param);
        PATTERN_BITMAP_COLOR[i].G = get_color_b10(cur_grobj, param);
        PATTERN_BITMAP_COLOR[i].R = get_color_b10(cur_grobj, param);
        PATTERN_BITMAP_COLOR[i].A = get_color_b8(cur_grobj, param);
    } else if (NV4:G80) {
        PATTERN_BITMAP_COLOR[i] = param;
        /* XXX: details */
        CONTEXT_FORMAT.PATTERN_BITMAP_COLOR[i] = cur_grobj.color_format;
    } else {
        PATTERN_BITMAP_COLOR[i] = param;
    }

mthd 0x318+i*4, i<2: BITMAP [\*_PATTERN]
mthd 0x2f8+i*4, i<2: PATTERN_BITMAP [\*_2D]
  Sets the pattern bitmap. i=0 sets bits 0-31, i=1 sets bits 32-63.
Operation::
    tmp = param;
    if (cur_grobj.BITMAP_FORMAT == CGA6 && NV1:G80) { /* XXX: check if also NV4+ */
        /* pattern stored internally in LE format - for CGA6, reverse
           bits in all bytes */
        tmp = (tmp & 0xaaaaaaaa) >> 1 | (tmp & 0x55555555) << 1;
        tmp = (tmp & 0xcccccccc) >> 2 | (tmp & 0x33333333) << 2;
        tmp = (tmp & 0xf0f0f0f0) >> 4 | (tmp & 0x0f0f0f0f) << 4;
    }
    PATTERN_BITMAP[i] = tmp;


Color pattern
=============

The color pattern is always an 8x8 array of R8G8B8 colors. It is stored and
uploaded as an array of 64 cells in raster scan - the color for pattern
coordinates (px, py) is taken from PATTERN_COLOR[(py&7) << 3 | (px&7)].
There are 4 sets of methods that set the pattern, corresponding to various
color formats. Each set of methods updates the same state internally and
converts the written values to R8G8B8 if necessary. Color pattern is available
on NV4+ only.

mthd 0x400+i*4, i<16: COLOR_Y8 [NV4_PATTERN]
mthd 0x500+i*4, i<16: PATTERN_COLOR_Y8 [\*_2D]
  Sets 4 color pattern cells, from Y8 source.
  bits 0-7: color for pattern cell i*4+0
  bits 8-15: color for pattern cell i*4+1
  bits 16-23: color for pattern cell i*4+2
  bits 24-31: color for pattern cell i*4+3
Operation::
    PATTERN_COLOR[4*i] = Y8_to_R8G8B8(param[0:7]);
    PATTERN_COLOR[4*i+1] = Y8_to_R8G8B8(param[8:15]);
    PATTERN_COLOR[4*i+2] = Y8_to_R8G8B8(param[16:23]);
    PATTERN_COLOR[4*i+3] = Y8_to_R8G8B8(param[24:31]);

mthd 0x500+i*4, i<32: COLOR_R5G6B5 [NV4_PATTERN]
mthd 0x400+i*4, i<32: PATTERN_COLOR_R5G6B5 [\*_2D]
  Sets 2 color pattern cells, from R5G6B5 source.
  bits 0-15: color for pattern cell i*2+0
  bits 16-31: color for pattern cell i*2+1
Operation::
    PATTERN_COLOR[2*i] = R5G6B5_to_R8G8B8(param[0:15]);
    PATTERN_COLOR[2*i+1] = R5G6B5_to_R8G8B8(param[16:31]);

mthd 0x600+i*4, i<32: COLOR_X1R5G5B5 [NV4_PATTERN]
mthd 0x480+i*4, i<32: PATTERN_COLOR_X1R5G5B5 [\*_2D]
  Sets 2 color pattern cells, from X1R5G5B5 source.
  bits 0-15: color for pattern cell i*2+0
  bits 16-31: color for pattern cell i*2+1
Operation::
    PATTERN_COLOR[2*i] = X1R5G5B5_to_R8G8B8(param[0:15]);
    PATTERN_COLOR[2*i+1] = X1R5G5B5_to_R8G8B8(param[16:31]);

mthd 0x700+i*4, i<64: COLOR_X8R8G8B8 [NV4_PATTERN]
mthd 0x300+i*4, i<64: PATTERN_COLOR_X8R8G8B8 [\*_2D]
  Sets a color pattern cell, from X8R8G8B8 source.
Operation::
    PATTERN_COLOR[i] = param[0:23];

.. todo:: precise upconversion formulas
