/* mga_ucode.h -- Matrox G200/G400 WARP engine microcode -*- linux-c -*-
 * Created: Thu Jan 11 21:20:43 2001 by gareth@valinux.com
 *
 * Copyright 1999 Matrox Graphics Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * MATROX GRAPHICS INC., OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Kernel-based WARP engine management:
 *    Gareth Hughes <gareth@valinux.com>
 */

/*
 * WARP pipes are named according to the functions they perform, where:
 *
 *   - T stands for computation of texture stage 0
 *   - T2 stands for computation of both texture stage 0 and texture stage 1
 *   - G stands for computation of triangle intensity (Gouraud interpolation)
 *   - Z stands for computation of Z buffer interpolation
 *   - S stands for computation of specular highlight
 *   - A stands for computation of the alpha channel
 *   - F stands for computation of vertex fog interpolation
 */

/*(DEBLOBBED)*/
