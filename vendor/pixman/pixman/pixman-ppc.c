/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <pixman-config.h>
#endif

#include "pixman-private.h"

#ifdef USE_VMX

/* The CPU detection code needs to be in a file not compiled with
 * "-maltivec -mabi=altivec", as gcc would try to save vector register
 * across function calls causing SIGILL on cpus without Altivec/vmx.
 */
#ifdef __APPLE__
#include <sys/sysctl.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    int error, have_vmx;
    size_t length = sizeof(have_vmx);

    error = sysctlbyname ("hw.optional.altivec", &have_vmx, &length, NULL, 0);

    if (error)
	return FALSE;

    return have_vmx;
}

#elif defined (HAVE_ELF_AUX_INFO)

#include <sys/auxv.h>
#ifdef __FreeBSD__
#include <machine/cpu.h>
#endif

static pixman_bool_t
pixman_have_vmx (void)
{
    unsigned long cpufeatures = 0;
    int error, have_vmx;

    error = elf_aux_info (AT_HWCAP, &cpufeatures, sizeof(cpufeatures));

    if (error != 0)
        return FALSE;

    have_vmx = cpufeatures & PPC_FEATURE_HAS_ALTIVEC;
    return have_vmx;
}

#elif defined (__OpenBSD__)

#include <sys/param.h>
#include <sys/sysctl.h>
#include <machine/cpu.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    int error, have_vmx;
    int mib[2] = { CTL_MACHDEP, CPU_ALTIVEC };
    size_t length = sizeof(have_vmx);

    error = sysctl (mib, 2, &have_vmx, &length, NULL, 0);

    if (error != 0)
	return FALSE;

    return have_vmx;
}

#elif defined (HAVE_GETAUXVAL)

#include <sys/auxv.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    unsigned long cpufeatures;
    int have_vmx;

    cpufeatures = getauxval (AT_HWCAP);

    have_vmx = cpufeatures & PPC_FEATURE_HAS_ALTIVEC;
    return have_vmx;
}

#elif defined (__linux__)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/auxvec.h>
#include <asm/cputable.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    int have_vmx = FALSE;
    int fd;
    struct
    {
	unsigned long type;
	unsigned long value;
    } aux;

    fd = open ("/proc/self/auxv", O_RDONLY);
    if (fd >= 0)
    {
	while (read (fd, &aux, sizeof (aux)) == sizeof (aux))
	{
	    if (aux.type == AT_HWCAP && (aux.value & PPC_FEATURE_HAS_ALTIVEC))
	    {
		have_vmx = TRUE;
		break;
	    }
	}

	close (fd);
    }

    return have_vmx;
}

#else /* !__APPLE__ && !__OpenBSD__ && !__linux__ */
#include <signal.h>
#include <setjmp.h>

static jmp_buf jump_env;

static void
vmx_test (int        sig,
	  siginfo_t *si,
	  void *     unused)
{
    longjmp (jump_env, 1);
}

static pixman_bool_t
pixman_have_vmx (void)
{
    struct sigaction sa, osa;
    int jmp_result;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset (&sa.sa_mask);
    sa.sa_sigaction = vmx_test;
    sigaction (SIGILL, &sa, &osa);
    jmp_result = setjmp (jump_env);
    if (jmp_result == 0)
    {
	asm volatile ( "vor 0, 0, 0" );
    }
    sigaction (SIGILL, &osa, NULL);
    return (jmp_result == 0);
}

#endif /* __APPLE__ */
#endif /* USE_VMX */

#if defined(USE_POWER8) || defined(USE_POWER9)
#define USE_PPC_HWCAP2
#endif

#ifdef USE_PPC_HWCAP2

/* Define the PPC AT_HWCAP2 feature flags in case the libc or kernel
 * headers do not.
 */
#ifndef PPC_FEATURE2_ARCH_2_07
#define PPC_FEATURE2_ARCH_2_07 0x80000000
#endif

#ifndef PPC_FEATURE2_ARCH_3_00
#define PPC_FEATURE2_ARCH_3_00 0x00800000
#endif

#ifndef AT_HWCAP2
#define AT_HWCAP2 26
#endif

#if defined (HAVE_ELF_AUX_INFO)

#include <sys/auxv.h>
#ifdef __FreeBSD__
#include <machine/cpu.h>
#endif

static unsigned long
pixman_hwcap2 (void)
{
    unsigned long cpufeatures = 0;

    if (elf_aux_info (AT_HWCAP2, &cpufeatures, sizeof(cpufeatures)) != 0)
	return 0;

    return cpufeatures;
}

#elif defined (HAVE_GETAUXVAL)

#include <sys/auxv.h>

static unsigned long
pixman_hwcap2 (void)
{
    return getauxval (AT_HWCAP2);
}

#elif defined (__linux__)

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/auxvec.h>

static unsigned long
pixman_hwcap2 (void)
{
    unsigned long hwcap2 = 0;
    int fd;
    struct
    {
	unsigned long type;
	unsigned long value;
    } aux;

    fd = open ("/proc/self/auxv", O_RDONLY);
    if (fd >= 0)
    {
	while (read (fd, &aux, sizeof (aux)) == sizeof (aux))
	{
	    if (aux.type == AT_HWCAP2)
	    {
		hwcap2 = aux.value;
		break;
	    }
	}

	close (fd);
    }

    return hwcap2;
}

#else /* !HAVE_ELF_AUX_INFO && !HAVE_GETAUXVAL && !__linux__ */

/* No machine running Mac OS X has ISA 2.07, and there is no reliable
 * way to read AT_HWCAP2 elsewhere without the ELF auxiliary vector,
 * so conservatively report no features.
 */
static unsigned long
pixman_hwcap2 (void)
{
    return 0;
}

#endif
#endif /* USE_PPC_HWCAP2 */

pixman_implementation_t *
_pixman_ppc_get_implementations (pixman_implementation_t *imp)
{
    /* The POWER9 and POWER8 implementations are recompilations of the
     * VMX one, so whichever is selected completely replaces it, and
     * disabling "vmx" disables them as well.
     *
     * The VMX implementation must always remain as the last resort,
     * even where a variant covers the same ISA level: pixman_have_vmx ()
     * always produces an answer, falling back to a SIGILL probe, while
     * pixman_hwcap2 () reports no features at all on a platform that has
     * neither getauxval () nor elf_aux_info () and is not Linux.  Were
     * the variants to replace it outright, such a platform would end up
     * with no vector code on hardware that provably has VSX.
     */
#ifdef USE_PPC_HWCAP2
    unsigned long hwcap2 = pixman_hwcap2 ();

#ifdef USE_POWER9
    if (!_pixman_disabled ("power9") && !_pixman_disabled ("vmx") &&
	(hwcap2 & PPC_FEATURE2_ARCH_3_00))
	return _pixman_implementation_create_power9 (imp);
#endif

#ifdef USE_POWER8
    if (!_pixman_disabled ("power8") && !_pixman_disabled ("vmx") &&
	(hwcap2 & PPC_FEATURE2_ARCH_2_07))
	return _pixman_implementation_create_power8 (imp);
#endif
#endif /* USE_PPC_HWCAP2 */

#ifdef USE_VMX
    if (!_pixman_disabled ("vmx") && pixman_have_vmx ())
	imp = _pixman_implementation_create_vmx (imp);
#endif

    return imp;
}
