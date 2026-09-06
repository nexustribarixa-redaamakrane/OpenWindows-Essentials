/*
 * Copyright © 2026 Trung Lê
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
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* The POWER8 implementation is the VMX/AltiVec implementation compiled
 * a second time with -mcpu=power8, which turns on the VSX unaligned
 * vector loads and stores and better code generation throughout via
 * the __VSX__ conditionals in pixman-vmx.c.  Everything in that file
 * is static except for the entry point, which is renamed here.
 */

#ifndef __VSX__
#error "pixman-power8.c must be compiled with VSX enabled (e.g. -mcpu=power8)"
#endif

#define _pixman_implementation_create_vmx _pixman_implementation_create_power8

#include "pixman-vmx.c"
