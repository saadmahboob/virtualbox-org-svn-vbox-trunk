/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened main().
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/** @page pg_hardening      VirtualBox VM Process Hardening
 *
 * The VM process hardening is to prevent malicious software from using
 * VirtualBox as a vehicle to obtain kernel level access.
 *
 * The VirtualBox VMM requires supervisor (kernel) level access to the CPU.  For
 * both practical and historical reasons part of the VMM is partly realized in
 * ring-3 with a rich interface to the kernel part.  While the device emulations
 * can be run exclusively in ring-3, we have performance optimizations that
 * loads device emulation code into ring-0 and our special raw-mode execution
 * context (non-VT-x/AMD-V mode) for handling frequent operations a lot more
 * efficiently.  These share data between all three context (ring-3, ring-0 and
 * raw-mode).  All this poses a rather broad attack surface, which the hardening
 * protects.
 *
 * The hardening primarily focuses on restricting access to the support driver,
 * VBoxDrv or vboxdrv depending on the OS, as it is ultimately the link and
 * instigator of the communication between ring-3 and the ring-0 and raw-mode
 * contexts.  A secondary focus is to make sure malicious code cannot be loaded
 * and executed in the VM process.  Exactly how we go about this depends a lot
 * on the host OS.
 *
 *
 * @section sec_hardening_unix  Hardening on UNIX-like OSes
 *
 * On UNIX-like systems (Solaris, Linux, darwin, freebsd, ...) we put our trust
 * in root and the root knows what he/she/it does.
 *
 * We only allow root to get full unrestricted access to the support driver.
 * The device node corresponding to unrestricted access is own by root and has a
 * 0600 access mode (i.e. only accessible to the owner, root).  In addition to
 * this file system level restriction, the support driver also checks that the
 * effective user ID (EUID) is root when it is being opened.
 *
 * The VM processes temporarily assume root privileges using the set-uid-bit on
 * the executable with root as owner.  In fact, all the files and directories we
 * install are owned by root and the wheel (or equivalent gid = 0) group,
 * including extension pack files.
 *
 * The executable with the set-uid-to-root-bit set is a stub binary that has no
 * unnecessary library dependencies (only libc, pthreads, dynamic linker) and
 * simply calls #SUPR3HardenedMain.  It does the following:
 *      1. Validate installation (supR3HardenedVerifyAll):
 *          - Check that the executable file of the process is one of the known
 *            VirtualBox executables.
 *          - Check that all mandatory files are present.
 *          - Check that all installed files and directories (both optional and
 *            mandatory ones) are owned by root:wheel and are not writable by
 *            anyone except root.
 *          - Check that all the parent directories, all the way up to the root
 *            if possible, only permits root (or system admin) to change them.
 *            This is that to rule out unintentional rename races.
 *          - On systems where it is possible, we may also valiadate executable
 *            image signatures.
 *
 *      2. Open a file descriptor for the support device driver
 *         (supR3HardenedMainOpenDevice).
 *
 *      3. Grab ICMP capabilities, if needed (supR3HardenedMainGrabCapabilites).
 *
 *      4. Correctly drop the root privileges (supR3HardenedMainDropPrivileges).
 *
 *      5. Load the VBoxRT dynamic link library and hand over the file
 *         descriptor to the SUPLib code in it (supR3HardenedMainInitRuntime).
 *
 *      6. Load a dynamic library containing the actual VM frontend code and run
 *         it (tail of SUPR3HardenedMain).
 *
 *  The set-uid-to-root stub executable is paired with a dynamic link library
 *  which export one TrustedMain entrypoint (see FNSUPTRUSTEDMAIN) that we call.
 *  In case of error reporting, the library may also export a TrustedError
 *  function (FNSUPTRUSTEDERROR).
 *
 *  That the set-uid-to-root-bit modifies the dynamic linker behavior on all
 *  relevant systems, even after we've dropped back to the real UI, is something
 *  we take advantage of.  The dynamic linkers takes special care to prevent
 *  users from using clever tricks to inject their own code into set-uid
 *  processes and causing privilege escalation issues.  This is of course
 *  exactly the kind of behavior we're looking for.
 *
 *  In addition to what the dynamic linker does for us, we will not directly
 *  call either RTLdrLoad or dlopen to load dynamic link libraries into the
 *  process.  Instead we will call SUPR3HardenedLdrLoad,
 *  SUPR3HardenedLdrLoadAppPriv and SUPR3HardenedLdrLoadPlugIn to do the
 *  loading.  These functions will perform the validations on the file being
 *  loaded as SUPR3HardenedMain did in its validation step.  So, anything we
 *  load must be installed owned by root:wheel, the directory we load it from
 *  must also be owned by root:wheel and now allow for renaming the file.
 *  Similar ownership restricts applies to all the parent directories (except on
 *  darwin).
 *
 *  So, we place the responsibility of not installing malicious software on the
 *  root user on UNIX-like systems.  Which is fair enough, in our opinion.
 *
 *
 * @section sec_hardening_win   Hardening on Windows
 *
 * On Windows we cannot put the same level or trust in the Administrator users
 * (equivalent of root/wheel on unix) as on the UNIX-like systems, which
 * complicates things greatly.
 *
 * Some or the blame for this can be given to Windows being a
 * descendant/replacement for a set of single user systems: DOS, Windows
 * 1.0-3.11 Windows 95-ME, and OS/2.  Users of NT 3.51 and later was inclined to
 * want to always run it with full root/administrator privileges like they had
 * done on the predecessors, while Microsoft made doing this very simple and
 * didn't help with the alternatives.  Bad idea, security wise, which is good
 * for the security software industry.  For this reason using a set-uid-to-root
 * approach is pointless, even if windows had one, which is doesn't.
 *
 * So, in order to protect access to the support driver and protect the
 * VM process while it's running we have to do a lot more work.  A keystone in
 * the defences is code signing.  The short version is this:
 *      - Minimal stub executable, signed with the same certificate as the
 *        kernel driver.
 *
 *      - The stub executable respawns itself twice, hooking the NTDLL init
 *        routine to perform protection tasks as early as possible.  The parent
 *        stub helps keep in the child clean for verification as does the
 *        support driver.
 *
 *      - In order to protect against loading unwanted code into the process,
 *        the stub processes installs DLL load hooks with NTDLL as well as
 *        directly intercepting the LdrLoadDll and NtCreateSection APIs.
 *
 *      - The support driver will verify all but the initial process very
 *        thoroughly before allowing them protection and in the final case full
 *        unrestricted access.
 *
 * @subsection  sec_hardening_win_protsoft      3rd Party "Protection" Software
 *
 * What makes our life REALLY difficult on Windows is this 3rd party "security"
 * software which is more or less required to keep a Windows system safe for
 * normal users and all corporate IT departments rightly insists on installing.
 * After the kernel patching clampdown in Vista, AV software have to do a lot
 * more mucking about in user mode to get their job (kind of) done.  So, it is
 * common practice to patch a lot of NTDLL, KERNEL32, the executable import
 * table, load extra DLLs into the process, allocate executable memory in the
 * process (classic code injection) and more.
 *
 * The BIG problem with all this is that it is indistiguishable from what
 * malicious software would be doing in order to intercept process acctivity
 * (network sniffing, maybe password snooping) or gain a level of kernel access
 * via the the support driver.
 *
 *
 * @subsection  sec_hardening_win_1st_stub  The Initial Stub Process
 *
 * We share the stub executable approach with the UNIX-like systems, so there's
 * the SUPR3HardenedMain and a paired DLL with TrustedMain and TrustedError.
 * However, the stub executable is fatter and much more bare metal:
 *      - It has no CRT (libc) because we don't need one and we need full
 *        control over the code in the stub.
 *      - It does not statically import anything to avoid having a import table
 *        that can be patched or extended to either intercept our calls or load
 *        additional DLLs.
 *      - System calls normally going thru NTDLL are done directly because there
 *        is so much software out there which wants to patch known NTDLL entry
 *        points to control our software (either for good or malicious reasons).
 *
 * The initial stub process is not really to be trusted, though we try our best
 * to limit potential harm (user mode debugger checks, disable thread creation).
 * So, when it enters SUPR3HardenedMain we only call supR3HardenedVerifyAll to
 * verify the installation (known executables and DLLs, checking their code
 * signing signatures, keeping them all open to deny deletion and replacing) and
 * does a respawn via supR3HardenedWinReSpawn.
 *
 *
 * @subsection  sec_hardening_win_2nd_stub  The Second Stub Process
 *
 * The second stub process will be created in suspended state, i.e. the main
 * thread is suspended before it executes a single instruction, and with a less
 * generous ACLs associated with it (skin deep protection only).  In order for
 * SUPR3TrustedMain to figure that it is the second stub process, the zero'th
 * command line argument has been replaced by a known magic string (UUID).  Now,
 * before the process starts executing, the parent (initial stub) will patch the
 * LdrInitializeThunk entrypoint in NTDLL to call supR3HardenedEarlyProcessInit
 * via supR3HardenedEarlyProcessInitThunk.  The parent will also plant some
 * synchronization stuff via SUPR3WINPROCPARAMS (NTDLL location, inherited event
 * handles and associated ping-pong equipment).
 *
 * The LdrInitializeThunk entrypoint of NTDLL is where the kernel sets up
 * process execution to start executing (via a user alert, so not subject to
 * SetThreadContext).  LdrInitializeThunk performs process, NTDLL and
 * sub-system client (kernel32) initialization.  A lot of "protection" software
 * uses triggers in this initialization sequence (like the KERNEL32.DLL load
 * event), so we avoid quite a bit of problems by getting our stuff done early
 * on.
 *
 * However, there is also those that uses events that triggers immediately when
 * the process is created or/and starts executing the first instruction, we have
 * a well know process state we can restore.  The first thing that
 * supR3HardenedEarlyProcessInit does is to signal the parent to do perform a
 * child purification to exorcise potentially evil influences.
 *
 * What the parent does during the purification is very similar to what the
 * kernel driver will do later on when verifying the second stub and the VM
 * processes, except that instead of failing when encountering an shortcomming
 * it will take corrective actions:
 *      - Executable memory regions not belonging to a DLL mapping will be
 *        attempted freed, and we'll only fail if we can't evict it.
 *      - All pages in the executable images in the process (should be just the
 *        stub executable and NTDLL) will be compared to the pristine fixed-up
 *        copy prepared by the IPRT PE loader code, restoring any bytes which
 *        appears differently in the child.  (g_ProcParams (SUPR3WINPROCPARAMS)
 *        is exempted, LdrInitializeThunk is set to call NtTerminateThread.)
 *      - Unwanted DLLs will be unloaded (we have a set of DLLs we like).
 *
 * Before signalling the second stub process that it has been purified and should
 * get on with it, the parent will close all handles with unrestricted access to
 * the process and thread so that the initial stub process no longer can
 * influence the child in any really harmful way.  (The caller of CreateProcess
 * usually receives handles with unrestricted access to the child process and
 * main thread.  These could in theory be used with DuplicateHandle or
 * WriteProcessMemory to get at the VM process if we're not careful.)
 *
 * supR3HardenedEarlyProcessInit will continue with opening the log file
 * (require command line parsing).  It will continue to initialize a bunch of
 * globals, syscalls and trustworthy/harmless NTDLL imports.
 * supR3HardenedWinInit is then called to setup image verification, that is:
 *      - Hook (insert jump instruction) the NtCreateSection entrypoint in NTDLL
 *        so we can check all executable mappings before they're created and can
 *        be mapped.
 *      - Hook (ditto) the LdrLoadDll entrypoint in NTDLL so we can pre-validate
 *        all images that gets loaded the normal way (partly because the
 *        NtCreateSection context is restrictive because the NTDLL loader lock
 *        is usually held, which prevents us from safely calling WinVerityTrust).
 * The image/DLL verification hooks are at this point able to verify DLLs
 * containing code signing signatures, and will restrict the locations from
 * which DLLs will be loaded.  When SUPR3HardenedMain gets going later one, they
 * will start insisting on everything having valid signatures in the DLL or in an
 * installer catalog file.
 *
 * The function also irrevocably disables debug notifications related to the
 * current thread, just to make attaching a debugging that much more difficult.
 *
 * Now, the second stub process will open the so called stub device, that is a
 * special support driver device node that tells the support driver to:
 *      - Protect the process against the OpenProcess and OpenThread attack
 *        vectors by stripping risky access rights.
 *      - Check that the process isn't being debugged.
 *      - Check that the process contains exactly one thread.
 *      - Check that the process doesn't have any unknown DLLs loaded into it.
 *      - Check that the process doesn't have any executable memory (other than
 *        DLL sections) in it.
 *      - Check that the process executable is a known VBox executable which may
 *        access the support driver.
 *      - Check that the process executable is signed with the same code signing
 *        certificate as the driver and that the on disk image is valid
 *        according to its embedded signature.
 *      - Check all the signature of all DLLs in the process (NTDLL) if they are
 *        signed, and only accept unsigned ones in versions where they are known
 *        not to be signed.
 *
 *
 * @subsection  sec_hardening_win_3rd_stub  The Final Stub
 *
 * Yet to be written...
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#if defined(RT_OS_OS2)
# define INCL_BASE
# define INCL_ERRORS
# include <os2.h>
# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <unistd.h>

#elif RT_OS_WINDOWS
# include <iprt/nt/nt-and-windows.h>

#else /* UNIXes */
# include <iprt/types.h> /* stdint fun on darwin. */

# include <stdio.h>
# include <stdlib.h>
# include <dlfcn.h>
# include <limits.h>
# include <errno.h>
# include <unistd.h>
# include <sys/stat.h>
# include <sys/time.h>
# include <sys/types.h>
# if defined(RT_OS_LINUX)
#  undef USE_LIB_PCAP /* don't depend on libcap as we had to depend on either
                         libcap1 or libcap2 */

#  undef _POSIX_SOURCE
#  include <linux/types.h> /* sys/capabilities from uek-headers require this */
#  include <sys/capability.h>
#  include <sys/prctl.h>
#  ifndef CAP_TO_MASK
#   define CAP_TO_MASK(cap) RT_BIT(cap)
#  endif
# elif defined(RT_OS_FREEBSD)
#  include <sys/param.h>
#  include <sys/sysctl.h>
# elif defined(RT_OS_SOLARIS)
#  include <priv.h>
# endif
# include <pwd.h>
# ifdef RT_OS_DARWIN
#  include <mach-o/dyld.h>
# endif

#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#ifdef RT_OS_WINDOWS
# include <VBox/version.h>
#endif
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>

#include "SUPLibInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def SUP_HARDENED_SUID
 * Whether we're employing set-user-ID-on-execute in the hardening.
 */
#if !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS) && !defined(RT_OS_L4)
# define SUP_HARDENED_SUID
#else
# undef  SUP_HARDENED_SUID
#endif

/** @def SUP_HARDENED_SYM
 * Decorate a symbol that's resolved dynamically.
 */
#ifdef RT_OS_OS2
# define SUP_HARDENED_SYM(sym)  "_" sym
#else
# define SUP_HARDENED_SYM(sym)  sym
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** @see RTR3InitEx */
typedef DECLCALLBACK(int) FNRTR3INITEX(uint32_t iVersion, uint32_t fFlags, int cArgs,
                                       char **papszArgs, const char *pszProgramPath);
typedef FNRTR3INITEX *PFNRTR3INITEX;

/** @see RTLogRelPrintf */
typedef DECLCALLBACK(void) FNRTLOGRELPRINTF(const char *pszFormat, ...);
typedef FNRTLOGRELPRINTF *PFNRTLOGRELPRINTF;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The pre-init data we pass on to SUPR3 (residing in VBoxRT). */
static SUPPREINITDATA   g_SupPreInitData;
/** The program executable path. */
#ifndef RT_OS_WINDOWS
static
#endif
char                    g_szSupLibHardenedExePath[RTPATH_MAX];
/** The application bin directory path. */
static char             g_szSupLibHardenedAppBinPath[RTPATH_MAX];

/** The program name. */
static const char      *g_pszSupLibHardenedProgName;
/** The flags passed to SUPR3HardenedMain. */
static uint32_t         g_fSupHardenedMain;

#ifdef SUP_HARDENED_SUID
/** The real UID at startup. */
static uid_t            g_uid;
/** The real GID at startup. */
static gid_t            g_gid;
# ifdef RT_OS_LINUX
static uint32_t         g_uCaps;
# endif
#endif

/** The startup log file. */
#ifdef RT_OS_WINDOWS
static HANDLE           g_hStartupLog = NULL;
#else
static int              g_hStartupLog = -1;
#endif
/** The number of bytes we've written to the startup log. */
static uint32_t volatile g_cbStartupLog = 0;

/** The current SUPR3HardenedMain state / location. */
SUPR3HARDENEDMAINSTATE  g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED;
AssertCompileSize(g_enmSupR3HardenedMainState, sizeof(uint32_t));

#ifdef RT_OS_WINDOWS
/** Pointer to VBoxRT's RTLogRelPrintf function so we can write errors to the
 * release log at runtime. */
static PFNRTLOGRELPRINTF g_pfnRTLogRelPrintf = NULL;
/** Log volume name (for attempting volume flush). */
static RTUTF16          g_wszStartupLogVol[16];
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef SUP_HARDENED_SUID
static void supR3HardenedMainDropPrivileges(void);
#endif
static PFNSUPTRUSTEDERROR supR3HardenedMainGetTrustedError(const char *pszProgName);


/**
 * Safely copy one or more strings into the given buffer.
 *
 * @returns VINF_SUCCESS or VERR_BUFFER_OVERFLOW.
 * @param   pszDst              The destionation buffer.
 * @param   cbDst               The size of the destination buffer.
 * @param   ...                 One or more zero terminated strings, ending with
 *                              a NULL.
 */
static int suplibHardenedStrCopyEx(char *pszDst, size_t cbDst, ...)
{
    int rc = VINF_SUCCESS;

    if (cbDst == 0)
        return VERR_BUFFER_OVERFLOW;

    va_list va;
    va_start(va, cbDst);
    for (;;)
    {
        const char *pszSrc = va_arg(va, const char *);
        if (!pszSrc)
            break;

        size_t cchSrc = suplibHardenedStrLen(pszSrc);
        if (cchSrc < cbDst)
        {
            suplibHardenedMemCopy(pszDst, pszSrc, cchSrc);
            pszDst += cchSrc;
            cbDst  -= cchSrc;
        }
        else
        {
            rc = VERR_BUFFER_OVERFLOW;
            if (cbDst > 1)
            {
                suplibHardenedMemCopy(pszDst, pszSrc, cbDst - 1);
                pszDst += cbDst - 1;
                cbDst   = 1;
            }
        }
        *pszDst = '\0';
    }
    va_end(va);

    return rc;
}


/**
 * Exit current process in the quickest possible fashion.
 *
 * @param   rcExit      The exit code.
 */
DECLNORETURN(void) suplibHardenedExit(RTEXITCODE rcExit)
{
    for (;;)
    {
#ifdef RT_OS_WINDOWS
        if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
            ExitProcess(rcExit);
        if (RtlExitUserProcess != NULL)
            RtlExitUserProcess(rcExit);
        NtTerminateProcess(NtCurrentProcess(), rcExit);
#else
        _Exit(rcExit);
#endif
    }
}


/**
 * Writes a substring to standard error.
 *
 * @param   pch                 The start of the substring.
 * @param   cch                 The length of the substring.
 */
static void suplibHardenedPrintStrN(const char *pch, size_t cch)
{
#ifdef RT_OS_WINDOWS
    HANDLE hStdOut = NtCurrentPeb()->ProcessParameters->StandardOutput;
    if (hStdOut != NULL)
    {
        if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
        {
            DWORD cbWritten;
            WriteFile(hStdOut, pch, (DWORD)cch, &cbWritten, NULL);
        }
        /* Windows 7 and earlier uses fake handles, with the last two bits set ((hStdOut & 3) == 3). */
        else if (NtWriteFile != NULL && ((uintptr_t)hStdOut & 3) == 0)
        {
            IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
            NtWriteFile(hStdOut, NULL /*Event*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                        &Ios, (PVOID)pch, (ULONG)cch, NULL /*ByteOffset*/, NULL /*Key*/);
        }
    }
#else
    (void)write(2, pch, cch);
#endif
}


/**
 * Writes a string to standard error.
 *
 * @param   psz                 The string.
 */
static void suplibHardenedPrintStr(const char *psz)
{
    suplibHardenedPrintStrN(psz, suplibHardenedStrLen(psz));
}


/**
 * Writes a char to standard error.
 *
 * @param   ch                  The character value to write.
 */
static void suplibHardenedPrintChr(char ch)
{
    suplibHardenedPrintStrN(&ch, 1);
}


/**
 * Writes a decimal number to stdard error.
 *
 * @param   uValue              The value.
 */
static void suplibHardenedPrintDecimal(uint64_t uValue)
{
    char    szBuf[64];
    char   *pszEnd = &szBuf[sizeof(szBuf) - 1];
    char   *psz    = pszEnd;

    *psz-- = '\0';

    do
    {
        *psz-- = '0' + (uValue % 10);
        uValue /= 10;
    } while (uValue > 0);

    psz++;
    suplibHardenedPrintStrN(psz, pszEnd - psz);
}


/**
 * Writes a hexadecimal or octal number to standard error.
 *
 * @param   uValue              The value.
 * @param   uBase               The base (16 or 8).
 * @param   fFlags              Format flags.
 */
static void suplibHardenedPrintHexOctal(uint64_t uValue, unsigned uBase, uint32_t fFlags)
{
    static char const   s_achDigitsLower[17] = "0123456789abcdef";
    static char const   s_achDigitsUpper[17] = "0123456789ABCDEF";
    const char         *pchDigits   = !(fFlags & RTSTR_F_CAPITAL) ? s_achDigitsLower : s_achDigitsUpper;
    unsigned            cShift      = uBase == 16 ?   4 : 3;
    unsigned            fDigitMask  = uBase == 16 ? 0xf : 7;
    char                szBuf[64];
    char               *pszEnd = &szBuf[sizeof(szBuf) - 1];
    char               *psz    = pszEnd;

    *psz-- = '\0';

    do
    {
        *psz-- = pchDigits[uValue & fDigitMask];
        uValue >>= cShift;
    } while (uValue > 0);

    if ((fFlags & RTSTR_F_SPECIAL) && uBase == 16)
    {
        *psz-- = !(fFlags & RTSTR_F_CAPITAL) ? 'x' : 'X';
        *psz-- = '0';
    }

    psz++;
    suplibHardenedPrintStrN(psz, pszEnd - psz);
}


/**
 * Writes a wide character string to standard error.
 *
 * @param   pwsz                The string.
 */
static void suplibHardenedPrintWideStr(PCRTUTF16 pwsz)
{
    for (;;)
    {
        RTUTF16 wc = *pwsz++;
        if (!wc)
            return;
        if (   (wc < 0x7f && wc >= 0x20)
            || wc == '\n'
            || wc == '\r')
            suplibHardenedPrintChr((char)wc);
        else
        {
            suplibHardenedPrintStrN(RT_STR_TUPLE("\\x"));
            suplibHardenedPrintHexOctal(wc, 16, 0);
        }
    }
}

#ifdef IPRT_NO_CRT

/** Buffer structure used by suplibHardenedOutput. */
struct SUPLIBHARDENEDOUTPUTBUF
{
    size_t off;
    char   szBuf[2048];
};

/** Callback for RTStrFormatV, see FNRTSTROUTPUT. */
static DECLCALLBACK(size_t) suplibHardenedOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    SUPLIBHARDENEDOUTPUTBUF *pBuf = (SUPLIBHARDENEDOUTPUTBUF *)pvArg;
    size_t cbTodo = cbChars;
    for (;;)
    {
        size_t cbSpace = sizeof(pBuf->szBuf) - pBuf->off - 1;

        /* Flush the buffer? */
        if (   cbSpace == 0
            || (cbTodo == 0 && pBuf->off))
        {
            suplibHardenedPrintStrN(pBuf->szBuf, pBuf->off);
# ifdef RT_OS_WINDOWS
            if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
                OutputDebugString(pBuf->szBuf);
# endif
            pBuf->off = 0;
            cbSpace = sizeof(pBuf->szBuf) - 1;
        }

        /* Copy the string into the buffer. */
        if (cbTodo == 1)
        {
            pBuf->szBuf[pBuf->off++] = *pachChars;
            break;
        }
        if (cbSpace >= cbTodo)
        {
            memcpy(&pBuf->szBuf[pBuf->off], pachChars, cbTodo);
            pBuf->off += cbTodo;
            break;
        }
        memcpy(&pBuf->szBuf[pBuf->off], pachChars, cbSpace);
        pBuf->off += cbSpace;
        cbTodo -= cbSpace;
    }
    pBuf->szBuf[pBuf->off] = '\0';

    return cbChars;
}

#endif /* IPRT_NO_CRT */

/**
 * Simple printf to standard error.
 *
 * @param   pszFormat   The format string.
 * @param   va          Arguments to format.
 */
DECLHIDDEN(void) suplibHardenedPrintFV(const char *pszFormat, va_list va)
{
#ifdef IPRT_NO_CRT
    /*
     * Use buffered output here to avoid character mixing on the windows
     * console and to enable us to use OutputDebugString.
     */
    SUPLIBHARDENEDOUTPUTBUF Buf;
    Buf.off = 0;
    Buf.szBuf[0] = '\0';
    RTStrFormatV(suplibHardenedOutput, &Buf, NULL, NULL, pszFormat, va);

#else /* !IPRT_NO_CRT */
    /*
     * Format loop.
     */
    char ch;
    const char *pszLast = pszFormat;
    for (;;)
    {
        ch = *pszFormat;
        if (!ch)
            break;
        pszFormat++;

        if (ch == '%')
        {
            /*
             * Format argument.
             */

            /* Flush unwritten bits. */
            if (pszLast != pszFormat - 1)
                suplibHardenedPrintStrN(pszLast, pszFormat - pszLast - 1);
            pszLast = pszFormat;
            ch = *pszFormat++;

            /* flags. */
            uint32_t fFlags = 0;
            for (;;)
            {
                if (ch == '#')          fFlags |= RTSTR_F_SPECIAL;
                else if (ch == '-')     fFlags |= RTSTR_F_LEFT;
                else if (ch == '+')     fFlags |= RTSTR_F_PLUS;
                else if (ch == ' ')     fFlags |= RTSTR_F_BLANK;
                else if (ch == '0')     fFlags |= RTSTR_F_ZEROPAD;
                else if (ch == '\'')    fFlags |= RTSTR_F_THOUSAND_SEP;
                else                    break;
                ch = *pszFormat++;
            }

            /* Width and precision - ignored. */
            while (RT_C_IS_DIGIT(ch))
                ch = *pszFormat++;
            if (ch == '*')
                va_arg(va, int);
            if (ch == '.')
            {
                do ch = *pszFormat++;
                while (RT_C_IS_DIGIT(ch));
                if (ch == '*')
                    va_arg(va, int);
            }

            /* Size. */
            char chArgSize = 0;
            switch (ch)
            {
                case 'z':
                case 'L':
                case 'j':
                case 't':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    break;

                case 'l':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    if (ch == 'l')
                    {
                        chArgSize = 'L';
                        ch = *pszFormat++;
                    }
                    break;

                case 'h':
                    chArgSize = ch;
                    ch = *pszFormat++;
                    if (ch == 'h')
                    {
                        chArgSize = 'H';
                        ch = *pszFormat++;
                    }
                    break;
            }

            /*
             * Do type specific formatting.
             */
            switch (ch)
            {
                case 'c':
                    ch = (char)va_arg(va, int);
                    suplibHardenedPrintChr(ch);
                    break;

                case 's':
                    if (chArgSize == 'l')
                    {
                        PCRTUTF16 pwszStr = va_arg(va, PCRTUTF16 );
                        if (RT_VALID_PTR(pwszStr))
                            suplibHardenedPrintWideStr(pwszStr);
                        else
                            suplibHardenedPrintStr("<NULL>");
                    }
                    else
                    {
                        const char *pszStr = va_arg(va, const char *);
                        if (!RT_VALID_PTR(pszStr))
                            pszStr = "<NULL>";
                        suplibHardenedPrintStr(pszStr);
                    }
                    break;

                case 'd':
                case 'i':
                {
                    int64_t iValue;
                    if (chArgSize == 'L' || chArgSize == 'j')
                        iValue = va_arg(va, int64_t);
                    else if (chArgSize == 'l')
                        iValue = va_arg(va, signed long);
                    else if (chArgSize == 'z' || chArgSize == 't')
                        iValue = va_arg(va, intptr_t);
                    else
                        iValue = va_arg(va, signed int);
                    if (iValue < 0)
                    {
                        suplibHardenedPrintChr('-');
                        iValue = -iValue;
                    }
                    suplibHardenedPrintDecimal(iValue);
                    break;
                }

                case 'p':
                case 'x':
                case 'X':
                case 'u':
                case 'o':
                {
                    unsigned uBase = 10;
                    uint64_t uValue;

                    switch (ch)
                    {
                        case 'p':
                            fFlags |= RTSTR_F_ZEROPAD; /* Note not standard behaviour (but I like it this way!) */
                            uBase = 16;
                            break;
                        case 'X':
                            fFlags |= RTSTR_F_CAPITAL;
                        case 'x':
                            uBase = 16;
                            break;
                        case 'u':
                            uBase = 10;
                            break;
                        case 'o':
                            uBase = 8;
                            break;
                    }

                    if (ch == 'p' || chArgSize == 'z' || chArgSize == 't')
                        uValue = va_arg(va, uintptr_t);
                    else if (chArgSize == 'L' || chArgSize == 'j')
                        uValue = va_arg(va, uint64_t);
                    else if (chArgSize == 'l')
                        uValue = va_arg(va, unsigned long);
                    else
                        uValue = va_arg(va, unsigned int);

                    if (uBase == 10)
                        suplibHardenedPrintDecimal(uValue);
                    else
                        suplibHardenedPrintHexOctal(uValue, uBase, fFlags);
                    break;
                }

                case 'R':
                    if (pszFormat[0] == 'r' && pszFormat[1] == 'c')
                    {
                        int iValue = va_arg(va, int);
                        if (iValue < 0)
                        {
                            suplibHardenedPrintChr('-');
                            iValue = -iValue;
                        }
                        suplibHardenedPrintDecimal(iValue);
                        pszFormat += 2;
                        break;
                    }
                    /* fall thru */

                /*
                 * Custom format.
                 */
                default:
                    suplibHardenedPrintStr("[bad format: ");
                    suplibHardenedPrintStrN(pszLast, pszFormat - pszLast);
                    suplibHardenedPrintChr(']');
                    break;
            }

            /* continue */
            pszLast = pszFormat;
        }
    }

    /* Flush the last bits of the string. */
    if (pszLast != pszFormat)
        suplibHardenedPrintStrN(pszLast, pszFormat - pszLast);
#endif /* !IPRT_NO_CRT */
}


/**
 * Prints to standard error.
 *
 * @param   pszFormat   The format string.
 * @param   ...         Arguments to format.
 */
DECLHIDDEN(void) suplibHardenedPrintF(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    suplibHardenedPrintFV(pszFormat, va);
    va_end(va);
}


/**
 * @copydoc RTPathStripFilename
 */
static void suplibHardenedPathStripFilename(char *pszPath)
{
    char *psz = pszPath;
    char *pszLastSep = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastSep = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastSep = psz;
                break;

            /* the end */
            case '\0':
                if (pszLastSep == pszPath)
                    *pszLastSep++ = '.';
                *pszLastSep = '\0';
                return;
        }
    }
    /* will never get here */
}


/**
 * @copydoc RTPathFilename
 */
DECLHIDDEN(char *) supR3HardenedPathFilename(const char *pszPath)
{
    const char *psz = pszPath;
    const char *pszLastComp = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastComp = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastComp = psz + 1;
                break;

            /* the end */
            case '\0':
                if (*pszLastComp)
                    return (char *)(void *)pszLastComp;
                return NULL;
        }
    }

    /* will never get here */
    return NULL;
}


/**
 * @copydoc RTPathAppPrivateNoArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateNoArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE)
    const char *pszSrcPath = RTPATH_APP_PRIVATE;
    size_t cchPathPrivateNoArch = suplibHardenedStrLen(pszSrcPath);
    if (cchPathPrivateNoArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateNoArch: Buffer overflow, %zu >= %zu\n", cchPathPrivateNoArch, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathPrivateNoArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathAppBin(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppPrivateArch
 */
DECLHIDDEN(int) supR3HardenedPathAppPrivateArch(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE_ARCH)
    const char *pszSrcPath = RTPATH_APP_PRIVATE_ARCH;
    size_t cchPathPrivateArch = suplibHardenedStrLen(pszSrcPath);
    if (cchPathPrivateArch >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppPrivateArch: Buffer overflow, %zu >= %zu\n", cchPathPrivateArch, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathPrivateArch + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathAppBin(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathSharedLibs
 */
DECLHIDDEN(int) supR3HardenedPathAppSharedLibs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_SHARED_LIBS)
    const char *pszSrcPath = RTPATH_SHARED_LIBS;
    size_t cchPathSharedLibs = suplibHardenedStrLen(pszSrcPath);
    if (cchPathSharedLibs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppSharedLibs: Buffer overflow, %zu >= %zu\n", cchPathSharedLibs, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathSharedLibs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathAppBin(pszPath, cchPath);
#endif
}


/**
 * @copydoc RTPathAppDocs
 */
DECLHIDDEN(int) supR3HardenedPathAppDocs(char *pszPath, size_t cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_DOCS)
    const char *pszSrcPath = RTPATH_APP_DOCS;
    size_t cchPathAppDocs = suplibHardenedStrLen(pszSrcPath);
    if (cchPathAppDocs >= cchPath)
        supR3HardenedFatal("supR3HardenedPathAppDocs: Buffer overflow, %zu >= %zu\n", cchPathAppDocs, cchPath);
    suplibHardenedMemCopy(pszPath, pszSrcPath, cchPathAppDocs + 1);
    return VINF_SUCCESS;

#else
    return supR3HardenedPathAppBin(pszPath, cchPath);
#endif
}


/**
 * Returns the full path to the executable in g_szSupLibHardenedExePath.
 *
 * @returns IPRT status code.
 */
static void supR3HardenedGetFullExePath(void)
{
    /*
     * Get the program filename.
     *
     * Most UNIXes have no API for obtaining the executable path, but provides a symbolic
     * link in the proc file system that tells who was exec'ed. The bad thing about this
     * is that we have to use readlink, one of the weirder UNIX APIs.
     *
     * Darwin, OS/2 and Windows all have proper APIs for getting the program file name.
     */
#if defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
# ifdef RT_OS_LINUX
    int cchLink = readlink("/proc/self/exe", &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);

# elif defined(RT_OS_SOLARIS)
    char szFileBuf[PATH_MAX + 1];
    sprintf(szFileBuf, "/proc/%ld/path/a.out", (long)getpid());
    int cchLink = readlink(szFileBuf, &g_szSupLibHardenedExePath[0], sizeof(g_szSupLibHardenedExePath) - 1);

# else /* RT_OS_FREEBSD */
    int aiName[4];
    aiName[0] = CTL_KERN;
    aiName[1] = KERN_PROC;
    aiName[2] = KERN_PROC_PATHNAME;
    aiName[3] = getpid();

    size_t cbPath = sizeof(g_szSupLibHardenedExePath);
    if (sysctl(aiName, RT_ELEMENTS(aiName), g_szSupLibHardenedExePath, &cbPath, NULL, 0) < 0)
        supR3HardenedFatal("supR3HardenedExecDir: sysctl failed\n");
    g_szSupLibHardenedExePath[sizeof(g_szSupLibHardenedExePath) - 1] = '\0';
    int cchLink = suplibHardenedStrLen(g_szSupLibHardenedExePath); /* paranoid? can't we use cbPath? */

# endif
    if (cchLink < 0 || cchLink == sizeof(g_szSupLibHardenedExePath) - 1)
        supR3HardenedFatal("supR3HardenedExecDir: couldn't read \"%s\", errno=%d cchLink=%d\n",
                            g_szSupLibHardenedExePath, errno, cchLink);
    g_szSupLibHardenedExePath[cchLink] = '\0';

#elif defined(RT_OS_OS2) || defined(RT_OS_L4)
    _execname(g_szSupLibHardenedExePath, sizeof(g_szSupLibHardenedExePath));

#elif defined(RT_OS_DARWIN)
    const char *pszImageName = _dyld_get_image_name(0);
    if (!pszImageName)
        supR3HardenedFatal("supR3HardenedExecDir: _dyld_get_image_name(0) failed\n");
    size_t cchImageName = suplibHardenedStrLen(pszImageName);
    if (!cchImageName || cchImageName >= sizeof(g_szSupLibHardenedExePath))
        supR3HardenedFatal("supR3HardenedExecDir: _dyld_get_image_name(0) failed, cchImageName=%d\n", cchImageName);
    suplibHardenedMemCopy(g_szSupLibHardenedExePath, pszImageName, cchImageName + 1);

#elif defined(RT_OS_WINDOWS)
    char *pszDst = g_szSupLibHardenedExePath;
    int rc = RTUtf16ToUtf8Ex(g_wszSupLibHardenedExePath, RTSTR_MAX, &pszDst, sizeof(g_szSupLibHardenedExePath), NULL);
    if (RT_FAILURE(rc))
        supR3HardenedFatal("supR3HardenedExecDir: RTUtf16ToUtf8Ex failed, rc=%Rrc\n", rc);
#else
# error needs porting.
#endif

    /*
     * Determine the application binary directory location.
     */
    suplibHardenedStrCopy(g_szSupLibHardenedAppBinPath, g_szSupLibHardenedExePath);
    suplibHardenedPathStripFilename(g_szSupLibHardenedAppBinPath);

    if (g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_HARDENED_MAIN_CALLED)
        supR3HardenedFatal("supR3HardenedExecDir: Called before SUPR3HardenedMain! (%d)\n", g_enmSupR3HardenedMainState);
    switch (g_fSupHardenedMain & SUPSECMAIN_FLAGS_LOC_MASK)
    {
        case SUPSECMAIN_FLAGS_LOC_APP_BIN:
            break;
        case SUPSECMAIN_FLAGS_LOC_TESTCASE:
            suplibHardenedPathStripFilename(g_szSupLibHardenedAppBinPath);
            break;
        default:
            supR3HardenedFatal("supR3HardenedExecDir: Unknown program binary location: %#x\n", g_fSupHardenedMain);
    }
}


#ifdef RT_OS_LINUX
/**
 * Checks if we can read /proc/self/exe.
 *
 * This is used on linux to see if we have to call init
 * with program path or not.
 *
 * @returns true / false.
 */
static bool supR3HardenedMainIsProcSelfExeAccssible(void)
{
    char szPath[RTPATH_MAX];
    int cchLink = readlink("/proc/self/exe", szPath, sizeof(szPath));
    return cchLink != -1;
}
#endif /* RT_OS_LINUX */



/**
 * @copydoc RTPathExecDir
 * @remarks not quite like RTPathExecDir actually...
 */
DECLHIDDEN(int) supR3HardenedPathAppBin(char *pszPath, size_t cchPath)
{
    /*
     * Lazy init (probably not required).
     */
    if (!g_szSupLibHardenedAppBinPath[0])
        supR3HardenedGetFullExePath();

    /*
     * Calc the length and check if there is space before copying.
     */
    size_t cch = suplibHardenedStrLen(g_szSupLibHardenedAppBinPath) + 1;
    if (cch <= cchPath)
    {
        suplibHardenedMemCopy(pszPath, g_szSupLibHardenedAppBinPath, cch + 1);
        return VINF_SUCCESS;
    }

    supR3HardenedFatal("supR3HardenedPathAppBin: Buffer too small (%u < %u)\n", cchPath, cch);
    return VERR_BUFFER_OVERFLOW;
}


#ifdef RT_OS_WINDOWS
extern "C" uint32_t g_uNtVerCombined;
#endif

DECLHIDDEN(void) supR3HardenedOpenLog(int *pcArgs, char **papszArgs)
{
    static const char s_szLogOption[] = "--sup-hardening-log=";

    /*
     * Scan the argument vector.
     */
    int cArgs = *pcArgs;
    for (int iArg = 1; iArg < cArgs; iArg++)
        if (strncmp(papszArgs[iArg], s_szLogOption, sizeof(s_szLogOption) - 1) == 0)
        {
            const char *pszLogFile = &papszArgs[iArg][sizeof(s_szLogOption) - 1];

            /*
             * Drop the argument from the vector (has trailing NULL entry).
             */
            memmove(&papszArgs[iArg], &papszArgs[iArg + 1], (cArgs - iArg) * sizeof(papszArgs[0]));
            *pcArgs -= 1;
            cArgs   -= 1;

            /*
             * Open the log file, unless we've already opened one.
             * First argument takes precedence
             */
#ifdef RT_OS_WINDOWS
            if (g_hStartupLog == NULL)
            {
                int rc = RTNtPathOpen(pszLogFile,
                                      GENERIC_WRITE | SYNCHRONIZE,
                                      FILE_ATTRIBUTE_NORMAL,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      FILE_OPEN_IF,
                                      FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                      OBJ_CASE_INSENSITIVE,
                                      &g_hStartupLog,
                                      NULL);
                if (RT_SUCCESS(rc))
                {
                    SUP_DPRINTF(("Log file opened: " VBOX_VERSION_STRING "r%u g_hStartupLog=%p g_uNtVerCombined=%#x\n",
                                 VBOX_SVN_REV, g_hStartupLog, g_uNtVerCombined));

                    /*
                     * If the path contains a drive volume, save it so we can
                     * use it to flush the volume containing the log file.
                     */
                    if (RT_C_IS_ALPHA(pszLogFile[0]) && pszLogFile[1] == ':')
                    {
                        RTUtf16CopyAscii(g_wszStartupLogVol, RT_ELEMENTS(g_wszStartupLogVol), "\\??\\");
                        g_wszStartupLogVol[sizeof("\\??\\") - 1] = RT_C_TO_UPPER(pszLogFile[0]);
                        g_wszStartupLogVol[sizeof("\\??\\") + 0] = ':';
                        g_wszStartupLogVol[sizeof("\\??\\") + 1] = '\0';
                    }
                }
                else
                    g_hStartupLog = NULL;
            }
#else
            //g_hStartupLog = open()
#endif
        }
}


DECLHIDDEN(void) supR3HardenedLogV(const char *pszFormat, va_list va)
{
#ifdef RT_OS_WINDOWS
    if (   g_hStartupLog != NULL
        && g_cbStartupLog < 16*_1M)
    {
        char szBuf[5120];
        PCLIENT_ID pSelfId = &((PTEB)NtCurrentTeb())->ClientId;
        size_t cchPrefix = RTStrPrintf(szBuf, sizeof(szBuf), "%x.%x: ", pSelfId->UniqueProcess, pSelfId->UniqueThread);
        size_t cch = RTStrPrintfV(&szBuf[cchPrefix], sizeof(szBuf) - cchPrefix, pszFormat, va) + cchPrefix;

        if ((size_t)cch >= sizeof(szBuf))
            cch = sizeof(szBuf) - 1;

        if (!cch || szBuf[cch - 1] != '\n')
            szBuf[cch++] = '\n';

        ASMAtomicAddU32(&g_cbStartupLog, (uint32_t)cch);

        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        LARGE_INTEGER   Offset;
        Offset.QuadPart = -1; /* Write to end of file. */
        NtWriteFile(g_hStartupLog, NULL /*Event*/, NULL /*ApcRoutine*/, NULL /*ApcContext*/,
                    &Ios, szBuf, (ULONG)cch, &Offset, NULL /*Key*/);
    }
#else
    /* later */
#endif
}


DECLHIDDEN(void) supR3HardenedLog(const char *pszFormat,  ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedLogV(pszFormat, va);
    va_end(va);
}


DECLHIDDEN(void) supR3HardenedLogFlush(void)
{
#ifdef RT_OS_WINDOWS
    if (   g_hStartupLog != NULL
        && g_cbStartupLog < 16*_1M)
    {
        IO_STATUS_BLOCK Ios = RTNT_IO_STATUS_BLOCK_INITIALIZER;
        NTSTATUS rcNt = NtFlushBuffersFile(g_hStartupLog, &Ios);

        /*
         * Try flush the volume containing the log file too.
         */
        if (g_wszStartupLogVol[0])
        {
            HANDLE              hLogVol = RTNT_INVALID_HANDLE_VALUE;
            UNICODE_STRING      NtName;
            NtName.Buffer        = g_wszStartupLogVol;
            NtName.Length        = (USHORT)(RTUtf16Len(g_wszStartupLogVol) * sizeof(RTUTF16));
            NtName.MaximumLength = NtName.Length + 1;
            OBJECT_ATTRIBUTES   ObjAttr;
            InitializeObjectAttributes(&ObjAttr, &NtName, OBJ_CASE_INSENSITIVE, NULL /*hRootDir*/, NULL /*pSecDesc*/);
            RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
            rcNt = NtCreateFile(&hLogVol,
                                GENERIC_WRITE | GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                                &ObjAttr,
                                &Ios,
                                NULL /* Allocation Size*/,
                                0 /*FileAttributes*/,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                FILE_OPEN,
                                FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                NULL /*EaBuffer*/,
                                0 /*EaLength*/);
            if (NT_SUCCESS(rcNt))
                rcNt = Ios.Status;
            if (NT_SUCCESS(rcNt))
            {
                RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
                rcNt = NtFlushBuffersFile(hLogVol, &Ios);
                NtClose(hLogVol);
            }
            else
            {
                /* This may have sideeffects similar to what we want... */
                hLogVol = RTNT_INVALID_HANDLE_VALUE;
                RTNT_IO_STATUS_BLOCK_REINIT(&Ios);
                rcNt = NtCreateFile(&hLogVol,
                                    GENERIC_READ | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                                    &ObjAttr,
                                    &Ios,
                                    NULL /* Allocation Size*/,
                                    0 /*FileAttributes*/,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    FILE_OPEN,
                                    FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                                    NULL /*EaBuffer*/,
                                    0 /*EaLength*/);
                if (NT_SUCCESS(rcNt) && NT_SUCCESS(Ios.Status))
                    NtClose(hLogVol);
            }
        }
    }
#else
    /* later */
#endif
}


/**
 * Prints the message prefix.
 */
static void suplibHardenedPrintPrefix(void)
{
    if (g_pszSupLibHardenedProgName)
        suplibHardenedPrintStr(g_pszSupLibHardenedProgName);
    suplibHardenedPrintStr(": ");
}


DECLHIDDEN(void)   supR3HardenedFatalMsgV(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, va_list va)
{
    /*
     * First to the log.
     */
    supR3HardenedLog("Error %d in %s! (enmWhat=%d)\n", rc, pszWhere, enmWhat);
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszMsgFmt, vaCopy);
    va_end(vaCopy);

#ifdef RT_OS_WINDOWS
    /*
     * The release log.
     */
    if (g_pfnRTLogRelPrintf)
    {
        va_copy(vaCopy, va);
        g_pfnRTLogRelPrintf("supR3HardenedFatalMsgV: %s enmWhat=%d rc=%Rrc (%#x)\n", pszWhere, enmWhat, rc);
        g_pfnRTLogRelPrintf("supR3HardenedFatalMsgV: %N\n", pszMsgFmt, &vaCopy);
        va_end(vaCopy);
    }
#endif

    /*
     * Then to the console.
     */
    suplibHardenedPrintPrefix();
    suplibHardenedPrintF("Error %d in %s!\n", rc, pszWhere);

    suplibHardenedPrintPrefix();
    va_copy(vaCopy, va);
    suplibHardenedPrintFV(pszMsgFmt, vaCopy);
    va_end(vaCopy);
    suplibHardenedPrintChr('\n');

    switch (enmWhat)
    {
        case kSupInitOp_Driver:
            suplibHardenedPrintChr('\n');
            suplibHardenedPrintPrefix();
            suplibHardenedPrintStr("Tip! Make sure the kernel module is loaded. It may also help to reinstall VirtualBox.\n");
            break;

        case kSupInitOp_Misc:
        case kSupInitOp_IPRT:
        case kSupInitOp_Integrity:
        case kSupInitOp_RootCheck:
            suplibHardenedPrintChr('\n');
            suplibHardenedPrintPrefix();
            suplibHardenedPrintStr("Tip! It may help to reinstall VirtualBox.\n");
            break;

        default:
            /* no hints here */
            break;
    }

    /*
     * Finally, TrustedError if appropriate.
     */
    if (g_enmSupR3HardenedMainState >= SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED)
    {
#ifdef SUP_HARDENED_SUID
        /*
         * Drop any root privileges we might be holding, this won't return
         * if it fails but end up calling supR3HardenedFatal[V].
         */
        supR3HardenedMainDropPrivileges();
#endif

        /*
         * Now try resolve and call the TrustedError entry point if we can
         * find it.  We'll fork before we attempt this because that way the
         * session management in main will see us exiting immediately (if
         * it's involved with us).
         */
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_OS2)
        int pid = fork();
        if (pid <= 0)
#endif
        {
            static volatile bool s_fRecursive = false; /* Loader hooks may cause recursion. */
            if (!s_fRecursive)
            {
                s_fRecursive = true;

                PFNSUPTRUSTEDERROR pfnTrustedError = supR3HardenedMainGetTrustedError(g_pszSupLibHardenedProgName);
                if (pfnTrustedError)
                    pfnTrustedError(pszWhere, enmWhat, rc, pszMsgFmt, va);

                s_fRecursive = false;
            }
        }
    }
#if defined(RT_OS_WINDOWS)
    /*
     * Report the error to the parent if this happens during early VM init.
     */
    else if (   g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED
             && g_enmSupR3HardenedMainState != SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED)
        supR3HardenedWinReportErrorToParent(pszWhere, enmWhat, rc, pszMsgFmt, va);
#endif

    /*
     * Quit
     */
    suplibHardenedExit(RTEXITCODE_FAILURE);
}


DECLHIDDEN(void)   supR3HardenedFatalMsg(const char *pszWhere, SUPINITOP enmWhat, int rc, const char *pszMsgFmt, ...)
{
    va_list va;
    va_start(va, pszMsgFmt);
    supR3HardenedFatalMsgV(pszWhere, enmWhat, rc, pszMsgFmt, va);
    va_end(va);
}


DECLHIDDEN(void) supR3HardenedFatalV(const char *pszFormat, va_list va)
{
    supR3HardenedLog("Fatal error:\n");
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszFormat, vaCopy);
    va_end(vaCopy);

#if defined(RT_OS_WINDOWS)
    /*
     * Report the error to the parent if this happens during early VM init.
     */
    if (   g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_WIN_IMPORTS_RESOLVED
        && g_enmSupR3HardenedMainState != SUPR3HARDENEDMAINSTATE_NOT_YET_CALLED)
        supR3HardenedWinReportErrorToParent(NULL, kSupInitOp_Invalid, VERR_INTERNAL_ERROR, pszFormat, va);
    else
#endif
    {
#ifdef RT_OS_WINDOWS
        if (g_pfnRTLogRelPrintf)
        {
            va_copy(vaCopy, va);
            g_pfnRTLogRelPrintf("supR3HardenedFatalV: %N", pszFormat, &vaCopy);
            va_end(vaCopy);
        }
#endif

        suplibHardenedPrintPrefix();
        suplibHardenedPrintFV(pszFormat, va);
    }

    suplibHardenedExit(RTEXITCODE_FAILURE);
}


DECLHIDDEN(void) supR3HardenedFatal(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedFatalV(pszFormat, va);
    va_end(va);
}


DECLHIDDEN(int) supR3HardenedErrorV(int rc, bool fFatal, const char *pszFormat, va_list va)
{
    if (fFatal)
        supR3HardenedFatalV(pszFormat, va);

    supR3HardenedLog("Error (rc=%d):\n", rc);
    va_list vaCopy;
    va_copy(vaCopy, va);
    supR3HardenedLogV(pszFormat, vaCopy);
    va_end(vaCopy);

#ifdef RT_OS_WINDOWS
    if (g_pfnRTLogRelPrintf)
    {
        va_copy(vaCopy, va);
        g_pfnRTLogRelPrintf("supR3HardenedErrorV: %N", pszFormat, &vaCopy);
        va_end(vaCopy);
    }
#endif

    suplibHardenedPrintPrefix();
    suplibHardenedPrintFV(pszFormat, va);

    return rc;
}


DECLHIDDEN(int) supR3HardenedError(int rc, bool fFatal, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    supR3HardenedErrorV(rc, fFatal, pszFormat, va);
    va_end(va);
    return rc;
}



/**
 * Attempts to open /dev/vboxdrv (or equvivalent).
 *
 * @remarks This function will not return on failure.
 */
DECLHIDDEN(void) supR3HardenedMainOpenDevice(void)
{
    RTERRINFOSTATIC ErrInfo;
    SUPINITOP       enmWhat = kSupInitOp_Driver;
    int rc = suplibOsInit(&g_SupPreInitData.Data, false /*fPreInit*/, true /*fUnrestricted*/,
                          &enmWhat, RTErrInfoInitStatic(&ErrInfo));
    if (RT_SUCCESS(rc))
        return;

    if (RTErrInfoIsSet(&ErrInfo.Core))
        supR3HardenedFatalMsg("suplibOsInit", enmWhat, rc, "%s", ErrInfo.szMsg);

    switch (rc)
    {
        /** @todo better messages! */
        case VERR_VM_DRIVER_NOT_INSTALLED:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "Kernel driver not installed");
        case VERR_VM_DRIVER_NOT_ACCESSIBLE:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "Kernel driver not accessible");
        case VERR_VM_DRIVER_LOAD_ERROR:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_VM_DRIVER_LOAD_ERROR");
        case VERR_VM_DRIVER_OPEN_ERROR:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_VM_DRIVER_OPEN_ERROR");
        case VERR_VM_DRIVER_VERSION_MISMATCH:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "Kernel driver version mismatch");
        case VERR_ACCESS_DENIED:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "VERR_ACCESS_DENIED");
        case VERR_NO_MEMORY:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "Kernel memory allocation/mapping failed");
        case VERR_SUPDRV_HARDENING_EVIL_HANDLE:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Integrity, rc, "VERR_SUPDRV_HARDENING_EVIL_HANDLE");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_0:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Integrity, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_0");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_1:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Integrity, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_1");
        case VERR_SUPLIB_NT_PROCESS_UNTRUSTED_2:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Integrity, rc, "VERR_SUPLIB_NT_PROCESS_UNTRUSTED_2");
        default:
            supR3HardenedFatalMsg("suplibOsInit", kSupInitOp_Driver, rc, "Unknown rc=%d (%Rrc)", rc, rc);
    }
}


#ifdef SUP_HARDENED_SUID

/**
 * Grabs extra non-root capabilities / privileges that we might require.
 *
 * This is currently only used for being able to do ICMP from the NAT engine.
 *
 * @note We still have root privileges at the time of this call.
 */
static void supR3HardenedMainGrabCapabilites(void)
{
# if defined(RT_OS_LINUX)
    /*
     * We are about to drop all our privileges. Remove all capabilities but
     * keep the cap_net_raw capability for ICMP sockets for the NAT stack.
     */
    if (g_uCaps != 0)
    {
#  ifdef USE_LIB_PCAP
        /* XXX cap_net_bind_service */
        if (!cap_set_proc(cap_from_text("all-eip cap_net_raw+ep")))
            prctl(PR_SET_KEEPCAPS, 1 /*keep=*/, 0, 0, 0);
        prctl(PR_SET_DUMPABLE, 1 /*dump*/, 0, 0, 0);
#  else
        cap_user_header_t hdr = (cap_user_header_t)alloca(sizeof(*hdr));
        cap_user_data_t   cap = (cap_user_data_t)alloca(sizeof(*cap));
        memset(hdr, 0, sizeof(*hdr));
        hdr->version = _LINUX_CAPABILITY_VERSION;
        memset(cap, 0, sizeof(*cap));
        cap->effective = g_uCaps;
        cap->permitted = g_uCaps;
        if (!capset(hdr, cap))
            prctl(PR_SET_KEEPCAPS, 1 /*keep*/, 0, 0, 0);
        prctl(PR_SET_DUMPABLE, 1 /*dump*/, 0, 0, 0);
#  endif /* !USE_LIB_PCAP */
    }

# elif defined(RT_OS_SOLARIS)
    /*
     * Add net_icmpaccess privilege to effective privileges and limit
     * permitted privileges before completely dropping root privileges.
     * This requires dropping root privileges temporarily to get the normal
     * user's privileges.
     */
    seteuid(g_uid);
    priv_set_t *pPrivEffective = priv_allocset();
    priv_set_t *pPrivNew = priv_allocset();
    if (pPrivEffective && pPrivNew)
    {
        int rc = getppriv(PRIV_EFFECTIVE, pPrivEffective);
        seteuid(0);
        if (!rc)
        {
            priv_copyset(pPrivEffective, pPrivNew);
            rc = priv_addset(pPrivNew, PRIV_NET_ICMPACCESS);
            if (!rc)
            {
                /* Order is important, as one can't set a privilege which is
                 * not in the permitted privilege set. */
                rc = setppriv(PRIV_SET, PRIV_EFFECTIVE, pPrivNew);
                if (rc)
                    supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to set effective privilege set.\n");
                rc = setppriv(PRIV_SET, PRIV_PERMITTED, pPrivNew);
                if (rc)
                    supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to set permitted privilege set.\n");
            }
            else
                supR3HardenedError(rc, false, "SUPR3HardenedMain: failed to add NET_ICMPACCESS privilege.\n");
        }
    }
    else
    {
        /* for memory allocation failures just continue */
        seteuid(0);
    }

    if (pPrivEffective)
        priv_freeset(pPrivEffective);
    if (pPrivNew)
        priv_freeset(pPrivNew);
# endif
}

/*
 * Look at the environment for some special options.
 */
static void supR3GrabOptions(void)
{
    const char *pszOpt;

# ifdef RT_OS_LINUX
    g_uCaps = 0;

    /*
     * Do _not_ perform any capability-related system calls for root processes
     * (leaving g_uCaps at 0).
     * (Hint: getuid gets the real user id, not the effective.)
     */
    if (getuid() != 0)
    {
        /*
         * CAP_NET_RAW.
         * Default: enabled.
         * Can be disabled with 'export VBOX_HARD_CAP_NET_RAW=0'.
         */
        pszOpt = getenv("VBOX_HARD_CAP_NET_RAW");
        if (   !pszOpt
            || memcmp(pszOpt, "0", sizeof("0")) != 0)
            g_uCaps = CAP_TO_MASK(CAP_NET_RAW);

        /*
         * CAP_NET_BIND_SERVICE.
         * Default: disabled.
         * Can be enabled with 'export VBOX_HARD_CAP_NET_BIND_SERVICE=1'.
         */
        pszOpt = getenv("VBOX_HARD_CAP_NET_BIND_SERVICE");
        if (   pszOpt
            && memcmp(pszOpt, "0", sizeof("0")) != 0)
            g_uCaps |= CAP_TO_MASK(CAP_NET_BIND_SERVICE);
    }
# endif
}

/**
 * Drop any root privileges we might be holding.
 */
static void supR3HardenedMainDropPrivileges(void)
{
    /*
     * Try use setre[ug]id since this will clear the save uid/gid and thus
     * leave fewer traces behind that libs like GTK+ may pick up.
     */
    uid_t euid, ruid, suid;
    gid_t egid, rgid, sgid;
# if defined(RT_OS_DARWIN)
    /* The really great thing here is that setreuid isn't available on
       OS X 10.4, libc emulates it. While 10.4 have a slightly different and
       non-standard setuid implementation compared to 10.5, the following
       works the same way with both version since we're super user (10.5 req).
       The following will set all three variants of the group and user IDs. */
    setgid(g_gid);
    setuid(g_uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# elif defined(RT_OS_SOLARIS)
    /* Solaris doesn't have setresuid, but the setreuid interface is BSD
       compatible and will set the saved uid to euid when we pass it a ruid
       that isn't -1 (which we do). */
    setregid(g_gid, g_gid);
    setreuid(g_uid, g_uid);
    euid = geteuid();
    ruid = suid = getuid();
    egid = getegid();
    rgid = sgid = getgid();

# else
    /* This is the preferred one, full control no questions about semantics.
       PORTME: If this isn't work, try join one of two other gangs above. */
    setresgid(g_gid, g_gid, g_gid);
    setresuid(g_uid, g_uid, g_uid);
    if (getresuid(&ruid, &euid, &suid) != 0)
    {
        euid = geteuid();
        ruid = suid = getuid();
    }
    if (getresgid(&rgid, &egid, &sgid) != 0)
    {
        egid = getegid();
        rgid = sgid = getgid();
    }
# endif


    /* Check that it worked out all right. */
    if (    euid != g_uid
        ||  ruid != g_uid
        ||  suid != g_uid
        ||  egid != g_gid
        ||  rgid != g_gid
        ||  sgid != g_gid)
        supR3HardenedFatal("SUPR3HardenedMain: failed to drop root privileges!"
                           " (euid=%d ruid=%d suid=%d  egid=%d rgid=%d sgid=%d; wanted uid=%d and gid=%d)\n",
                           euid, ruid, suid, egid, rgid, sgid, g_uid, g_gid);

# if RT_OS_LINUX
    /*
     * Re-enable the cap_net_raw capability which was disabled during setresuid.
     */
    if (g_uCaps != 0)
    {
#  ifdef USE_LIB_PCAP
        /** @todo Warn if that does not work? */
        /* XXX cap_net_bind_service */
        cap_set_proc(cap_from_text("cap_net_raw+ep"));
#  else
        cap_user_header_t hdr = (cap_user_header_t)alloca(sizeof(*hdr));
        cap_user_data_t   cap = (cap_user_data_t)alloca(sizeof(*cap));
        memset(hdr, 0, sizeof(*hdr));
        hdr->version = _LINUX_CAPABILITY_VERSION;
        memset(cap, 0, sizeof(*cap));
        cap->effective = g_uCaps;
        cap->permitted = g_uCaps;
        /** @todo Warn if that does not work? */
        capset(hdr, cap);
#  endif /* !USE_LIB_PCAP */
    }
# endif
}

#endif /* SUP_HARDENED_SUID */

/**
 * Loads the VBoxRT DLL/SO/DYLIB, hands it the open driver,
 * and calls RTR3InitEx.
 *
 * @param   fFlags      The SUPR3HardenedMain fFlags argument, passed to supR3PreInit.
 *
 * @remarks VBoxRT contains both IPRT and SUPR3.
 * @remarks This function will not return on failure.
 */
static void supR3HardenedMainInitRuntime(uint32_t fFlags)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedPathAppSharedLibs(szPath, sizeof(szPath) - sizeof("/VBoxRT" SUPLIB_DLL_SUFF));
    suplibHardenedStrCat(szPath, "/VBoxRT" SUPLIB_DLL_SUFF);

    /*
     * Open it and resolve the symbols.
     */
#if defined(RT_OS_WINDOWS)
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/, g_fSupHardenedMain);
    if (!hMod)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_MODULE_NOT_FOUND,
                              "LoadLibrary \"%s\" failed (rc=%d)",
                              szPath, RtlGetLastWin32Error());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)GetProcAddress(hMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"RTR3InitEx\" not found in \"%s\" (rc=%d)",
                              szPath, RtlGetLastWin32Error());

    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)GetProcAddress(hMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"supR3PreInit\" not found in \"%s\" (rc=%d)",
                              szPath, RtlGetLastWin32Error());

    g_pfnRTLogRelPrintf = (PFNRTLOGRELPRINTF)GetProcAddress(hMod, SUP_HARDENED_SYM("RTLogRelPrintf"));
    Assert(g_pfnRTLogRelPrintf);  /* Not fatal in non-strict builds. */

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_MODULE_NOT_FOUND,
                              "dlopen(\"%s\",) failed: %s",
                              szPath, dlerror());
    PFNRTR3INITEX pfnRTInitEx = (PFNRTR3INITEX)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("RTR3InitEx"));
    if (!pfnRTInitEx)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"RTR3InitEx\" not found in \"%s\"!\ndlerror: %s",
                              szPath, dlerror());
    PFNSUPR3PREINIT pfnSUPPreInit = (PFNSUPR3PREINIT)(uintptr_t)dlsym(pvMod, SUP_HARDENED_SYM("supR3PreInit"));
    if (!pfnSUPPreInit)
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, VERR_SYMBOL_NOT_FOUND,
                              "Entrypoint \"supR3PreInit\" not found in \"%s\"!\ndlerror: %s",
                              szPath, dlerror());
#endif

    /*
     * Make the calls.
     */
    supR3HardenedGetPreInitData(&g_SupPreInitData);
    int rc = pfnSUPPreInit(&g_SupPreInitData, fFlags);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, rc,
                              "supR3PreInit failed with rc=%d", rc);
    const char *pszExePath = NULL;
#ifdef RT_OS_LINUX
    if (!supR3HardenedMainIsProcSelfExeAccssible())
        pszExePath = g_szSupLibHardenedExePath;
#endif
    rc = pfnRTInitEx(RTR3INIT_VER_1,
                     fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV ? 0 : RTR3INIT_FLAGS_SUPLIB,
                     0 /*cArgs*/, NULL /*papszArgs*/, pszExePath);
    if (RT_FAILURE(rc))
        supR3HardenedFatalMsg("supR3HardenedMainInitRuntime", kSupInitOp_IPRT, rc,
                              "RTR3InitEx failed with rc=%d", rc);

#if defined(RT_OS_WINDOWS)
    /*
     * Windows: Create thread that terminates the process when the parent stub
     *          process terminates (VBoxNetDHCP, Ctrl-C, etc).
     */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
        supR3HardenedWinCreateParentWatcherThread(hMod);
#endif
}


/**
 * Construct the path to the DLL/SO/DYLIB containing the actual program.
 *
 * @returns VBox status code.
 * @param   pszProgName     The program name.
 * @param   fMainFlags      The flags passed to SUPR3HardenedMain.
 * @param   pszPath         The output buffer.
 * @param   cbPath          The size of the output buffer, in bytes.  Must be at
 *                          least 128 bytes!
 */
static int supR3HardenedMainGetTrustedLib(const char *pszProgName, uint32_t fMainFlags, char *pszPath, size_t cbPath)
{
    supR3HardenedPathAppPrivateArch(pszPath, sizeof(cbPath) - 10);
    const char *pszSubDirSlash;
    switch (g_fSupHardenedMain & SUPSECMAIN_FLAGS_LOC_MASK)
    {
        case SUPSECMAIN_FLAGS_LOC_APP_BIN:
            pszSubDirSlash = "/";
            break;
        case SUPSECMAIN_FLAGS_LOC_TESTCASE:
            pszSubDirSlash = "/testcase/";
            break;
        default:
            pszSubDirSlash = "/";
            supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Unknown program binary location: %#x\n", g_fSupHardenedMain);
    }
#ifdef RT_OS_DARWIN
    if (fMainFlags & SUPSECMAIN_FLAGS_OSX_VM_APP)
        pszProgName = "VirtualBox";
#endif
    size_t cch = suplibHardenedStrLen(pszPath);
    return suplibHardenedStrCopyEx(&pszPath[cch], cbPath - cch, pszSubDirSlash, pszProgName, SUPLIB_DLL_SUFF, NULL);
}


/**
 * Loads the DLL/SO/DYLIB containing the actual program and
 * resolves the TrustedError symbol.
 *
 * This is very similar to supR3HardenedMainGetTrustedMain().
 *
 * @returns Pointer to the trusted error symbol if it is exported, NULL
 *          and no error messages otherwise.
 * @param   pszProgName     The program name.
 */
static PFNSUPTRUSTEDERROR supR3HardenedMainGetTrustedError(const char *pszProgName)
{
    /*
     * Don't bother if the main() function didn't advertise any TrustedError
     * export.  It's both a waste of time and may trigger additional problems,
     * confusing or obscuring the original issue.
     */
    if (!(g_fSupHardenedMain & SUPSECMAIN_FLAGS_TRUSTED_ERROR))
        return NULL;

    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedMainGetTrustedLib(pszProgName, g_fSupHardenedMain, szPath, sizeof(szPath));

    /*
     * Open it and resolve the symbol.
     */
#if defined(RT_OS_WINDOWS)
    supR3HardenedWinEnableThreadCreation();
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/, 0 /*fMainFlags*/);
    if (!hMod)
        return NULL;
    FARPROC pfn = GetProcAddress(hMod, SUP_HARDENED_SYM("TrustedError"));
    if (!pfn)
        return NULL;
    return (PFNSUPTRUSTEDERROR)pfn;

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        return NULL;
    void *pvSym = dlsym(pvMod, SUP_HARDENED_SYM("TrustedError"));
    if (!pvSym)
        return NULL;
    return (PFNSUPTRUSTEDERROR)(uintptr_t)pvSym;
#endif
}


/**
 * Loads the DLL/SO/DYLIB containing the actual program and
 * resolves the TrustedMain symbol.
 *
 * @returns Pointer to the trusted main of the actual program.
 * @param   pszProgName     The program name.
 * @param   fMainFlags      The flags passed to SUPR3HardenedMain.
 * @remarks This function will not return on failure.
 */
static PFNSUPTRUSTEDMAIN supR3HardenedMainGetTrustedMain(const char *pszProgName, uint32_t fMainFlags)
{
    /*
     * Construct the name.
     */
    char szPath[RTPATH_MAX];
    supR3HardenedMainGetTrustedLib(pszProgName, fMainFlags, szPath, sizeof(szPath));

    /*
     * Open it and resolve the symbol.
     */
#if defined(RT_OS_WINDOWS)
    HMODULE hMod = (HMODULE)supR3HardenedWinLoadLibrary(szPath, false /*fSystem32Only*/, 0 /*fMainFlags*/);
    if (!hMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: LoadLibrary \"%s\" failed, rc=%d\n",
                           szPath, RtlGetLastWin32Error());
    FARPROC pfn = GetProcAddress(hMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pfn)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\" (rc=%d)\n",
                           szPath, RtlGetLastWin32Error());
    return (PFNSUPTRUSTEDMAIN)pfn;

#else
    /* the dlopen crowd */
    void *pvMod = dlopen(szPath, RTLD_NOW | RTLD_GLOBAL);
    if (!pvMod)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: dlopen(\"%s\",) failed: %s\n",
                           szPath, dlerror());
    void *pvSym = dlsym(pvMod, SUP_HARDENED_SYM("TrustedMain"));
    if (!pvSym)
        supR3HardenedFatal("supR3HardenedMainGetTrustedMain: Entrypoint \"TrustedMain\" not found in \"%s\"!\ndlerror: %s\n",
                           szPath, dlerror());
    return (PFNSUPTRUSTEDMAIN)(uintptr_t)pvSym;
#endif
}


/**
 * Secure main.
 *
 * This is used for the set-user-ID-on-execute binaries on unixy systems
 * and when using the open-vboxdrv-via-root-service setup on Windows.
 *
 * This function will perform the integrity checks of the VirtualBox
 * installation, open the support driver, open the root service (later),
 * and load the DLL corresponding to \a pszProgName and execute its main
 * function.
 *
 * @returns Return code appropriate for main().
 *
 * @param   pszProgName     The program name. This will be used to figure out which
 *                          DLL/SO/DYLIB to load and execute.
 * @param   fFlags          Flags.
 * @param   argc            The argument count.
 * @param   argv            The argument vector.
 * @param   envp            The environment vector.
 */
DECLHIDDEN(int) SUPR3HardenedMain(const char *pszProgName, uint32_t fFlags, int argc, char **argv, char **envp)
{
    SUP_DPRINTF(("SUPR3HardenedMain: pszProgName=%s fFlags=%#x\n", pszProgName, fFlags));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_HARDENED_MAIN_CALLED;

    /*
     * Note! At this point there is no IPRT, so we will have to stick
     * to basic CRT functions that everyone agree upon.
     */
    g_pszSupLibHardenedProgName   = pszProgName;
    g_fSupHardenedMain            = fFlags;
    g_SupPreInitData.u32Magic     = SUPPREINITDATA_MAGIC;
    g_SupPreInitData.u32EndMagic  = SUPPREINITDATA_MAGIC;
#ifdef RT_OS_WINDOWS
    if (!g_fSupEarlyProcessInit)
#endif
        g_SupPreInitData.Data.hDevice = SUP_HDEVICE_NIL;

    /*
     * Determine the full exe path as we'll be needing it for the verify all
     * call(s) below.  (We have to do this early on Linux because we * *might*
     * not be able to access /proc/self/exe after the seteuid call.)
     */
    supR3HardenedGetFullExePath();
#ifdef RT_OS_WINDOWS
    supR3HardenedWinInitAppBin(fFlags);
#endif

#ifdef SUP_HARDENED_SUID
    /*
     * Grab any options from the environment.
     */
    supR3GrabOptions();

    /*
     * Check that we're root, if we aren't then the installation is butchered.
     */
    g_uid = getuid();
    g_gid = getgid();
    if (geteuid() != 0 /* root */)
        supR3HardenedFatalMsg("SUPR3HardenedMain", kSupInitOp_RootCheck, VERR_PERMISSION_DENIED,
                              "Effective UID is not root (euid=%d egid=%d uid=%d gid=%d)",
                              geteuid(), getegid(), g_uid, g_gid);
#endif /* SUP_HARDENED_SUID */

#ifdef RT_OS_WINDOWS
    /*
     * Windows: First respawn. On Windows we will respawn the process twice to establish
     * something we can put some kind of reliable trust in.  The first respawning aims
     * at dropping compatibility layers and process "security" solutions.
     */
    if (   !g_fSupEarlyProcessInit
        && !(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV)
        && supR3HardenedWinIsReSpawnNeeded(1 /*iWhich*/, argc, argv))
    {
        SUP_DPRINTF(("SUPR3HardenedMain: Respawn #1\n"));
        supR3HardenedWinInit(SUPSECMAIN_FLAGS_DONT_OPEN_DEV, false /*fAvastKludge*/);
        supR3HardenedVerifyAll(true /* fFatal */, pszProgName, g_szSupLibHardenedExePath, fFlags);
        return supR3HardenedWinReSpawn(1 /*iWhich*/);
    }

    /*
     * Windows: Initialize the image verification global data so we can verify the
     * signature of the process image and hook the core of the DLL loader API so we
     * can check the signature of all DLLs mapped into the process.  (Already done
     * by early VM process init.)
     */
    if (!g_fSupEarlyProcessInit)
        supR3HardenedWinInit(fFlags, true /*fAvastKludge*/);
#endif /* RT_OS_WINDOWS */

    /*
     * Validate the installation.
     */
    supR3HardenedVerifyAll(true /* fFatal */, pszProgName, g_szSupLibHardenedExePath, fFlags);

    /*
     * The next steps are only taken if we actually need to access the support
     * driver.  (Already done by early process init.)
     */
    if (!(fFlags & SUPSECMAIN_FLAGS_DONT_OPEN_DEV))
    {
#ifdef RT_OS_WINDOWS
        /*
         * Windows: Must have done early process init if we get here.
         */
        if (!g_fSupEarlyProcessInit)
            supR3HardenedFatalMsg("SUPR3HardenedMain", kSupInitOp_Integrity, VERR_WRONG_ORDER,
                                  "Early process init was somehow skipped.");

        /*
         * Windows: The second respawn.  This time we make a special arrangement
         * with vboxdrv to monitor access to the new process from its inception.
         */
        if (supR3HardenedWinIsReSpawnNeeded(2 /* iWhich*/, argc, argv))
        {
            SUP_DPRINTF(("SUPR3HardenedMain: Respawn #2\n"));
            return supR3HardenedWinReSpawn(2 /* iWhich*/);
        }
        SUP_DPRINTF(("SUPR3HardenedMain: Final process, opening VBoxDrv...\n"));
        supR3HardenedWinFlushLoaderCache();

#else
        /*
         * Open the vboxdrv device.
         */
        supR3HardenedMainOpenDevice();
#endif /* !RT_OS_WINDOWS */
    }

#ifdef RT_OS_WINDOWS
    /*
     * Windows: Enable the use of windows APIs to verify images at load time.
     */
    supR3HardenedWinEnableThreadCreation();
    supR3HardenedWinFlushLoaderCache();
    supR3HardenedWinResolveVerifyTrustApiAndHookThreadCreation(g_pszSupLibHardenedProgName);
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_WIN_VERIFY_TRUST_READY;
#endif

#ifdef SUP_HARDENED_SUID
    /*
     * Grab additional capabilities / privileges.
     */
    supR3HardenedMainGrabCapabilites();

    /*
     * Drop any root privileges we might be holding (won't return on failure)
     */
    supR3HardenedMainDropPrivileges();
#endif

    /*
     * Load the IPRT, hand the SUPLib part the open driver and
     * call RTR3InitEx.
     */
    SUP_DPRINTF(("SUPR3HardenedMain: Load Runtime...\n"));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_INIT_RUNTIME;
    supR3HardenedMainInitRuntime(fFlags);
#ifdef RT_OS_WINDOWS
    supR3HardenedWinModifyDllSearchPath(fFlags, g_szSupLibHardenedAppBinPath);
#endif

    /*
     * Load the DLL/SO/DYLIB containing the actual program
     * and pass control to it.
     */
    SUP_DPRINTF(("SUPR3HardenedMain: Load TrustedMain...\n"));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_GET_TRUSTED_MAIN;
    PFNSUPTRUSTEDMAIN pfnTrustedMain = supR3HardenedMainGetTrustedMain(pszProgName, fFlags);

    SUP_DPRINTF(("SUPR3HardenedMain: Calling TrustedMain (%p)...\n", pfnTrustedMain));
    g_enmSupR3HardenedMainState = SUPR3HARDENEDMAINSTATE_CALLED_TRUSTED_MAIN;
    return pfnTrustedMain(argc, argv, envp);
}

