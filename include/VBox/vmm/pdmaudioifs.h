/** @file
 * PDM - Pluggable Device Manager, audio interfaces.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifndef ___VBox_vmm_pdmaudioifs_h
#define ___VBox_vmm_pdmaudioifs_h

#include <iprt/circbuf.h>
#include <iprt/list.h>

#include <VBox/types.h>
#ifdef VBOX_WITH_STATISTICS
# include <VBox/vmm/stam.h>
#endif

/** @defgroup grp_pdm_ifs_audio     PDM Audio Interfaces
 * @ingroup grp_pdm_interfaces
 * @{
 */

/** PDM audio driver instance flags. */
typedef uint32_t PDMAUDIODRVFLAGS;

/** No flags set. */
#define PDMAUDIODRVFLAGS_NONE       0
/** Marks a primary audio driver which is critical
 *  when running the VM. */
#define PDMAUDIODRVFLAGS_PRIMARY    RT_BIT(0)

/**
 * Audio format in signed or unsigned variants.
 */
typedef enum PDMAUDIOFMT
{
    /** Invalid format, do not use. */
    PDMAUDIOFMT_INVALID,
    /** 8-bit, unsigned. */
    PDMAUDIOFMT_U8,
    /** 8-bit, signed. */
    PDMAUDIOFMT_S8,
    /** 16-bit, unsigned. */
    PDMAUDIOFMT_U16,
    /** 16-bit, signed. */
    PDMAUDIOFMT_S16,
    /** 32-bit, unsigned. */
    PDMAUDIOFMT_U32,
    /** 32-bit, signed. */
    PDMAUDIOFMT_S32,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOFMT_32BIT_HACK = 0x7fffffff
} PDMAUDIOFMT;

/**
 * Audio direction.
 */
typedef enum PDMAUDIODIR
{
    /** Unknown direction. */
    PDMAUDIODIR_UNKNOWN = 0,
    /** Input. */
    PDMAUDIODIR_IN      = 1,
    /** Output. */
    PDMAUDIODIR_OUT     = 2,
    /** Duplex handling. */
    PDMAUDIODIR_ANY     = 3,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIODIR_32BIT_HACK = 0x7fffffff
} PDMAUDIODIR;

/** Device latency spec in milliseconds (ms). */
typedef uint32_t PDMAUDIODEVLATSPECMS;

/** Device latency spec in seconds (s). */
typedef uint32_t PDMAUDIODEVLATSPECSEC;

/** Audio device flags. Use with PDMAUDIODEV_FLAG_ flags. */
typedef uint32_t PDMAUDIODEVFLAG;

/** No flags set. */
#define PDMAUDIODEV_FLAGS_NONE            0
/** The device marks the default device within the host OS. */
#define PDMAUDIODEV_FLAGS_DEFAULT         RT_BIT(0)
/** The device can be removed at any time and we have to deal with it. */
#define PDMAUDIODEV_FLAGS_HOTPLUG         RT_BIT(1)
/** The device is known to be buggy and needs special treatment. */
#define PDMAUDIODEV_FLAGS_BUGGY           RT_BIT(2)
/** Ignore the device, no matter what. */
#define PDMAUDIODEV_FLAGS_IGNORE          RT_BIT(3)
/** The device is present but marked as locked by some other application. */
#define PDMAUDIODEV_FLAGS_LOCKED          RT_BIT(4)
/** The device is present but not in an alive state (dead). */
#define PDMAUDIODEV_FLAGS_DEAD            RT_BIT(5)

/**
 * Audio device type.
 */
typedef enum PDMAUDIODEVICETYPE
{
    /** Unknown device type. This is the default. */
    PDMAUDIODEVICETYPE_UNKNOWN    = 0,
    /** Dummy device; for backends which are not able to report
     *  actual device information (yet). */
    PDMAUDIODEVICETYPE_DUMMY,
    /** The device is built into the host (non-removable). */
    PDMAUDIODEVICETYPE_BUILTIN,
    /** The device is an (external) USB device. */
    PDMAUDIODEVICETYPE_USB,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIODEVICETYPE_32BIT_HACK = 0x7fffffff
} PDMAUDIODEVICETYPE;

/**
 * Audio device instance data.
 */
typedef struct PDMAUDIODEVICE
{
    /** List node. */
    RTLISTNODE         Node;
    /** Friendly name of the device, if any. */
    char               szName[64];
    /** The device type. */
    PDMAUDIODEVICETYPE enmType;
    /** Reference count indicating how many audio streams currently are relying on this device. */
    uint8_t            cRefCount;
    /** Usage of the device. */
    PDMAUDIODIR        enmUsage;
    /** Device flags. */
    PDMAUDIODEVFLAG    fFlags;
    /** Maximum number of input audio channels the device supports. */
    uint8_t            cMaxInputChannels;
    /** Maximum number of output audio channels the device supports. */
    uint8_t            cMaxOutputChannels;
    /** Additional data which might be relevant for the current context. */
    void              *pvData;
    /** Size of the additional data. */
    size_t             cbData;
    /** Device type union, based on enmType. */
    union
    {
        /** USB type specifics. */
        struct
        {
            /** Vendor ID. */
            int16_t    VID;
            /** Product ID. */
            int16_t    PID;
        } USB;
    } Type;
} PDMAUDIODEVICE, *PPDMAUDIODEVICE;

/**
 * Structure for keeping an audio device enumeration.
 */
typedef struct PDMAUDIODEVICEENUM
{
    /** Number of audio devices in the list. */
    uint16_t        cDevices;
    /** List of audio devices. */
    RTLISTANCHOR    lstDevices;
} PDMAUDIODEVICEENUM, *PPDMAUDIODEVICEENUM;

/**
 * Audio (static) configuration of an audio host backend.
 */
typedef struct PDMAUDIOBACKENDCFG
{
    /** Size (in bytes) of the host backend's audio output stream structure. */
    size_t   cbStreamOut;
    /** Size (in bytes) of the host backend's audio input stream structure. */
    size_t   cbStreamIn;
    /** Number of concurrent output streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t cMaxStreamsOut;
    /** Number of concurrent input streams supported on the host.
     *  UINT32_MAX for unlimited concurrent streams, 0 if no concurrent input streams are supported. */
    uint32_t cMaxStreamsIn;
} PDMAUDIOBACKENDCFG, *PPDMAUDIOBACKENDCFG;

/**
 * A single audio sample, representing left and right channels (stereo).
 */
typedef struct PDMAUDIOSAMPLE
{
    /** Left channel. */
    int64_t i64LSample;
    /** Right channel. */
    int64_t i64RSample;
} PDMAUDIOSAMPLE;
/** Pointer to a single (stereo) audio sample.   */
typedef PDMAUDIOSAMPLE *PPDMAUDIOSAMPLE;
/** Pointer to a const single (stereo) audio sample.   */
typedef PDMAUDIOSAMPLE const *PCPDMAUDIOSAMPLE;

typedef enum PDMAUDIOENDIANNESS
{
    /** The usual invalid endian. */
    PDMAUDIOENDIANNESS_INVALID,
    /** Little endian. */
    PDMAUDIOENDIANNESS_LITTLE,
    /** Bit endian. */
    PDMAUDIOENDIANNESS_BIG,
    /** Endianness doesn't have a meaning in the context. */
    PDMAUDIOENDIANNESS_NA,
    /** The end of the valid endian values (exclusive). */
    PDMAUDIOENDIANNESS_END,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOENDIANNESS_32BIT_HACK = 0x7fffffff
} PDMAUDIOENDIANNESS;

/**
 * Audio playback destinations.
 */
typedef enum PDMAUDIOPLAYBACKDEST
{
    /** Unknown destination. */
    PDMAUDIOPLAYBACKDEST_UNKNOWN = 0,
    /** Front channel. */
    PDMAUDIOPLAYBACKDEST_FRONT,
    /** Center / LFE (Subwoofer) channel. */
    PDMAUDIOPLAYBACKDEST_CENTER_LFE,
    /** Rear channel. */
    PDMAUDIOPLAYBACKDEST_REAR,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOPLAYBACKDEST_32BIT_HACK = 0x7fffffff
} PDMAUDIOPLAYBACKDEST;

/**
 * Audio recording sources.
 */
typedef enum PDMAUDIORECSOURCE
{
    /** Unknown recording source. */
    PDMAUDIORECSOURCE_UNKNOWN = 0,
    /** Microphone-In. */
    PDMAUDIORECSOURCE_MIC,
    /** CD. */
    PDMAUDIORECSOURCE_CD,
    /** Video-In. */
    PDMAUDIORECSOURCE_VIDEO,
    /** AUX. */
    PDMAUDIORECSOURCE_AUX,
    /** Line-In. */
    PDMAUDIORECSOURCE_LINE,
    /** Phone-In. */
    PDMAUDIORECSOURCE_PHONE,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIORECSOURCE_32BIT_HACK = 0x7fffffff
} PDMAUDIORECSOURCE;

/**
 * Audio stream (data) layout.
 */
typedef enum PDMAUDIOSTREAMLAYOUT
{
    /** Unknown access type; do not use. */
    PDMAUDIOSTREAMLAYOUT_UNKNOWN = 0,
    /** Non-interleaved access, that is, consecutive
     *  access to the data. */
    PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED,
    /** Interleaved access, where the data can be
     *  mixed together with data of other audio streams. */
    PDMAUDIOSTREAMLAYOUT_INTERLEAVED,
    /** Complex layout, which does not fit into the
     *  interleaved / non-interleaved layouts. */
    PDMAUDIOSTREAMLAYOUT_COMPLEX,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMLAYOUT_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMLAYOUT, *PPDMAUDIOSTREAMLAYOUT;

/** No stream channel data flags defined. */
#define PDMAUDIOSTREAMCHANNELDATA_FLAG_NONE      0

/**
 * Structure for keeping a stream channel data block around.
 */
typedef struct PDMAUDIOSTREAMCHANNELDATA
{
    /** Circular buffer for the channel data. */
    PRTCIRCBUF pCircBuf;
    size_t     cbAcq;
    /** Channel data flags. */
    uint32_t   fFlags;
} PDMAUDIOSTREAMCHANNELDATA, *PPDMAUDIOSTREAMCHANNELDATA;

/**
 * Structure for a single channel of an audio stream.
 * An audio stream consists of one or multiple channels,
 * depending on the configuration.
 */
typedef struct PDMAUDIOSTREAMCHANNEL
{
    /** Channel ID. */
    uint8_t                   uChannel;
    /** Step size (in bytes) to the channel's next frame. */
    size_t                    cbStep;
    /** Frame size (in bytes) of this channel. */
    size_t                    cbFrame;
    /** Offset (in bytes) to first sample in the data block. */
    size_t                    cbFirst;
    /** Currente offset (in bytes) in the data stream. */
    size_t                    cbOff;
    /** Associated data buffer. */
    PDMAUDIOSTREAMCHANNELDATA Data;
} PDMAUDIOSTREAMCHANNEL, *PPDMAUDIOSTREAMCHANNEL;

/**
 * Union for keeping an audio stream destination or source.
 */
typedef union PDMAUDIODESTSOURCE
{
    /** Desired playback destination (for an output stream). */
    PDMAUDIOPLAYBACKDEST Dest;
    /** Desired recording source (for an input stream). */
    PDMAUDIORECSOURCE    Source;
} PDMAUDIODESTSOURCE, *PPDMAUDIODESTSOURCE;

/**
 * Structure for keeping an audio stream configuration.
 */
typedef struct PDMAUDIOSTREAMCFG
{
    /** Friendly name of the stream. */
    char                     szName[64];
    /** Direction of the stream. */
    PDMAUDIODIR              enmDir;
    /** Destination / source indicator, depending on enmDir. */
    PDMAUDIODESTSOURCE       DestSource;
    /** Frequency in Hertz (Hz). */
    uint32_t                 uHz;
    /** Number of audio channels (2 for stereo, 1 for mono). */
    uint8_t                  cChannels;
    /** Audio format. */
    PDMAUDIOFMT              enmFormat;
    /** @todo Use RT_LE2H_*? */
    PDMAUDIOENDIANNESS       enmEndianness;
    /** Hint about the optimal sample buffer size (in audio samples).
     *  0 if no hint is given. */
    uint32_t                 cSampleBufferSize;
} PDMAUDIOSTREAMCFG, *PPDMAUDIOSTREAMCFG;

#if defined(RT_LITTLE_ENDIAN)
# define PDMAUDIOHOSTENDIANNESS PDMAUDIOENDIANNESS_LITTLE
#elif defined(RT_BIG_ENDIAN)
# define PDMAUDIOHOSTENDIANNESS PDMAUDIOENDIANNESS_BIG
#else
# error "Port me!"
#endif

/**
 * Audio mixer controls.
 */
typedef enum PDMAUDIOMIXERCTL
{
    /** Unknown mixer control. */
    PDMAUDIOMIXERCTL_UNKNOWN = 0,
    /** Master volume. */
    PDMAUDIOMIXERCTL_VOLUME_MASTER,
    /** Front. */
    PDMAUDIOMIXERCTL_FRONT,
    /** Center / LFE (Subwoofer). */
    PDMAUDIOMIXERCTL_CENTER_LFE,
    /** Rear. */
    PDMAUDIOMIXERCTL_REAR,
    /** Line-In. */
    PDMAUDIOMIXERCTL_LINE_IN,
    /** Microphone-In. */
    PDMAUDIOMIXERCTL_MIC_IN,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOMIXERCTL_32BIT_HACK = 0x7fffffff
} PDMAUDIOMIXERCTL;

/**
 * Audio stream commands. Used in the audio connector
 * as well as in the actual host backends.
 */
typedef enum PDMAUDIOSTREAMCMD
{
    /** Unknown command, do not use. */
    PDMAUDIOSTREAMCMD_UNKNOWN = 0,
    /** Enables the stream. */
    PDMAUDIOSTREAMCMD_ENABLE,
    /** Disables the stream. */
    PDMAUDIOSTREAMCMD_DISABLE,
    /** Pauses the stream. */
    PDMAUDIOSTREAMCMD_PAUSE,
    /** Resumes the stream. */
    PDMAUDIOSTREAMCMD_RESUME,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMCMD_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMCMD;

/**
 * Properties of audio streams for host/guest
 * for in or out directions.
 */
typedef struct PDMAUDIOPCMPROPS
{
    /** Sample width. Bits per sample. */
    uint8_t     cBits;
    /** Signed or unsigned sample. */
    bool        fSigned;
    /** Shift count used for faster calculation of various
     *  values, such as the alignment, bytes to samples and so on.
     *  Depends on number of stream channels and the stream format
     *  being used.
     *
     ** @todo Use some RTAsmXXX functions instead?
     */
    uint8_t     cShift;
    /** Number of audio channels. */
    uint8_t     cChannels;
    /** Alignment mask. */
    uint32_t    uAlign;
    /** Sample frequency in Hertz (Hz). */
    uint32_t    uHz;
    /** Bitrate (in bytes/s). */
    uint32_t    cbBitrate;
    /** Whether the endianness is swapped or not. */
    bool        fSwapEndian;
} PDMAUDIOPCMPROPS, *PPDMAUDIOPCMPROPS;

/** Converts (audio) samples to bytes. */
#define PDMAUDIOPCMPROPS_S2B(pProps, samples) ((samples) << (pProps)->cShift)
/** Converts bytes to (audio) samples. */
#define PDMAUDIOPCMPROPS_B2S(pProps, cb)  (cb >> (pProps)->cShift)

/**
 * Audio volume parameters.
 */
typedef struct PDMAUDIOVOLUME
{
    /** Set to @c true if this stream is muted, @c false if not. */
    bool    fMuted;
    /** Left channel volume.
     *  Range is from [0 ... 255], whereas 0 specifies
     *  the most silent and 255 the loudest value. */
    uint8_t uLeft;
    /** Right channel volume.
     *  Range is from [0 ... 255], whereas 0 specifies
     *  the most silent and 255 the loudest value. */
    uint8_t uRight;
} PDMAUDIOVOLUME, *PPDMAUDIOVOLUME;

/** Defines the minimum volume allowed. */
#define PDMAUDIO_VOLUME_MIN     (0)
/** Defines the maximum volume allowed. */
#define PDMAUDIO_VOLUME_MAX     (255)

/**
 * Structure for holding rate processing information
 * of a source + destination audio stream. This is needed
 * because both streams can differ regarding their rates
 * and therefore need to be treated accordingly.
 */
typedef struct PDMAUDIOSTRMRATE
{
    /** Current (absolute) offset in the output
     *  (destination) stream. */
    uint64_t       dstOffset;
    /** Increment for moving dstOffset for the
     *  destination stream. This is needed because the
     *  source <-> destination rate might be different. */
    uint64_t       dstInc;
    /** Current (absolute) offset in the input
     *  stream. */
    uint32_t       srcOffset;
    /** Last processed sample of the input stream.
     *  Needed for interpolation. */
    PDMAUDIOSAMPLE srcSampleLast;
} PDMAUDIOSTRMRATE, *PPDMAUDIOSTRMRATE;

/**
 * Structure for holding mixing buffer volume parameters.
 * The volume values are in fixed point style and must
 * be converted to/from before using with e.g. PDMAUDIOVOLUME.
 */
typedef struct PDMAUDMIXBUFVOL
{
    /** Set to @c true if this stream is muted, @c false if not. */
    bool    fMuted;
    /** Left volume to apply during conversion. Pass 0
     *  to convert the original values. May not apply to
     *  all conversion functions. */
    uint32_t uLeft;
    /** Right volume to apply during conversion. Pass 0
     *  to convert the original values. May not apply to
     *  all conversion functions. */
    uint32_t uRight;
} PDMAUDMIXBUFVOL, *PPDMAUDMIXBUFVOL;

/**
 * Structure for holding sample conversion parameters for
 * the audioMixBufConvFromXXX / audioMixBufConvToXXX macros.
 */
typedef struct PDMAUDMIXBUFCONVOPTS
{
    /** Number of audio samples to convert. */
    uint32_t        cSamples;
    union
    {
        struct
        {
            /** Volume to use for conversion. */
            PDMAUDMIXBUFVOL Volume;
        } From;
    };
} PDMAUDMIXBUFCONVOPTS;
/** Pointer to conversion parameters for the audio mixer.   */
typedef PDMAUDMIXBUFCONVOPTS *PPDMAUDMIXBUFCONVOPTS;
/** Pointer to const conversion parameters for the audio mixer.   */
typedef PDMAUDMIXBUFCONVOPTS const *PCPDMAUDMIXBUFCONVOPTS;

/**
 * Note: All internal handling is done in samples,
 *       not in bytes!
 */
typedef uint32_t PDMAUDIOMIXBUFFMT;
typedef PDMAUDIOMIXBUFFMT *PPDMAUDIOMIXBUFFMT;

/**
 * Convertion-from function used by the PDM audio buffer mixer.
 *
 * @returns Number of samples returned.
 * @param   paDst           Where to return the converted samples.
 * @param   pvSrc           The source samples bytes.
 * @param   cbSrc           Number of bytes to convert.
 * @param   pOpts           Conversion options.
 */
typedef DECLCALLBACK(uint32_t) FNPDMAUDIOMIXBUFCONVFROM(PPDMAUDIOSAMPLE paDst, const void *pvSrc, uint32_t cbSrc,
                                                        PCPDMAUDMIXBUFCONVOPTS pOpts);
/** Pointer to a convertion-from function used by the PDM audio buffer mixer. */
typedef FNPDMAUDIOMIXBUFCONVFROM *PFNPDMAUDIOMIXBUFCONVFROM;

/**
 * Convertion-to function used by the PDM audio buffer mixer.
 *
 * @param   pvDst           Output buffer.
 * @param   paSrc           The input samples.
 * @param   pOpts           Conversion options.
 */
typedef DECLCALLBACK(void) FNPDMAUDIOMIXBUFCONVTO(void *pvDst, PCPDMAUDIOSAMPLE paSrc, PCPDMAUDMIXBUFCONVOPTS pOpts);
/** Pointer to a convertion-to function used by the PDM audio buffer mixer. */
typedef FNPDMAUDIOMIXBUFCONVTO *PFNPDMAUDIOMIXBUFCONVTO;

typedef struct PDMAUDIOMIXBUF *PPDMAUDIOMIXBUF;
typedef struct PDMAUDIOMIXBUF
{
    RTLISTNODE                Node;
    /** Name of the buffer. */
    char                     *pszName;
    /** Sample buffer. */
    PPDMAUDIOSAMPLE           pSamples;
    /** Size of the sample buffer (in samples). */
    uint32_t                  cSamples;
    /** The current read position (in samples). */
    uint32_t                  offRead;
    /** The current write position (in samples). */
    uint32_t                  offWrite;
    /**
     * Total samples already mixed down to the parent buffer (if any). Always starting at
     * the parent's offRead position.
     *
     * Note: Count always is specified in parent samples, as the sample count can differ between parent
     *       and child.
     */
    uint32_t                  cMixed;
    /** How much audio samples are currently being used
     *  in this buffer.
     *  Note: This also is known as the distance in ring buffer terms. */
    uint32_t                  cUsed;
    /** Pointer to parent buffer (if any). */
    PPDMAUDIOMIXBUF           pParent;
    /** List of children mix buffers to keep in sync with (if being a parent buffer). */
    RTLISTANCHOR              lstChildren;
    /** Intermediate structure for buffer conversion tasks. */
    PPDMAUDIOSTRMRATE         pRate;
    /** Internal representation of current volume used for mixing. */
    PDMAUDMIXBUFVOL           Volume;
    /** This buffer's audio format. */
    PDMAUDIOMIXBUFFMT         AudioFmt;
    /** Standard conversion-to function for set AudioFmt. */
    PFNPDMAUDIOMIXBUFCONVTO   pfnConvTo;
    /** Standard conversion-from function for set AudioFmt. */
    PFNPDMAUDIOMIXBUFCONVFROM pfnConvFrom;
    /**
     * Ratio of the associated parent stream's frequency by this stream's
     * frequency (1<<32), represented as a signed 64 bit integer.
     *
     * For example, if the parent stream has a frequency of 44 khZ, and this
     * stream has a frequency of 11 kHz, the ration then would be
     * (44/11 * (1 << 32)).
     *
     * Currently this does not get changed once assigned.
     */
    int64_t                   iFreqRatio;
    /** For quickly converting samples <-> bytes and vice versa. */
    uint8_t                   cShift;
} PDMAUDIOMIXBUF;

typedef uint32_t PDMAUDIOFILEFLAGS;

/* No flags defined. */
#define PDMAUDIOFILEFLAG_NONE            0

/**
 * Audio file types.
 */
typedef enum PDMAUDIOFILETYPE
{
    /** Unknown type, do not use. */
    PDMAUDIOFILETYPE_UNKNOWN = 0,
    /** Wave (.WAV) file. */
    PDMAUDIOFILETYPE_WAV,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOFILETYPE_32BIT_HACK = 0x7fffffff
} PDMAUDIOFILETYPE;

/**
 * Structure for an audio file handle.
 */
typedef struct PDMAUDIOFILE
{
    /** Type of the audio file. */
    PDMAUDIOFILETYPE enmType;
    /** File name. */
    char             szName[255];
    /** Actual file handle. */
    RTFILE           hFile;
    /** Data needed for the specific audio file type implemented.
     *  Optional, can be NULL. */
    void            *pvData;
    /** Data size (in bytes). */
    size_t           cbData;
} PDMAUDIOFILE, *PPDMAUDIOFILE;

/** Stream status flag. To be used with PDMAUDIOSTRMSTS_FLAG_ flags. */
typedef uint32_t PDMAUDIOSTRMSTS;

/** No flags being set. */
#define PDMAUDIOSTRMSTS_FLAG_NONE            0
/** Whether this stream has been initialized by the
 *  backend or not. */
#define PDMAUDIOSTRMSTS_FLAG_INITIALIZED     RT_BIT_32(0)
/** Whether this stream is enabled or disabled. */
#define PDMAUDIOSTRMSTS_FLAG_ENABLED         RT_BIT_32(1)
/** Whether this stream has been paused or not. This also implies
 *  that this is an enabled stream! */
#define PDMAUDIOSTRMSTS_FLAG_PAUSED          RT_BIT_32(2)
/** Whether this stream was marked as being disabled
 *  but there are still associated guest output streams
 *  which rely on its data. */
#define PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE RT_BIT_32(3)
/** Data can be read from the stream. */
#define PDMAUDIOSTRMSTS_FLAG_DATA_READABLE   RT_BIT_32(4)
/** Data can be written to the stream. */
#define PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE   RT_BIT_32(5)
/** Whether this stream is in re-initialization phase.
 *  All other bits remain untouched to be able to restore
 *  the stream's state after the re-initialization bas been
 *  finished. */
#define PDMAUDIOSTRMSTS_FLAG_PENDING_REINIT  RT_BIT_32(6)
/** Validation mask. */
#define PDMAUDIOSTRMSTS_VALID_MASK           UINT32_C(0x0000007F)

/**
 * Enumeration presenting a backend's current status.
 */
typedef enum PDMAUDIOBACKENDSTS
{
    /** Unknown/invalid status. */
    PDMAUDIOBACKENDSTS_UNKNOWN = 0,
    /** The backend is in its initialization phase.
     *  Not all backends support this status. */
    PDMAUDIOBACKENDSTS_INITIALIZING,
    /** The backend has stopped its operation. */
    PDMAUDIOBACKENDSTS_STOPPED,
    /** The backend is up and running. */
    PDMAUDIOBACKENDSTS_RUNNING,
    /** The backend ran into an error and is unable to recover.
     *  A manual re-initialization might help. */
    PDMAUDIOBACKENDSTS_ERROR,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOBACKENDSTS_32BIT_HACK = 0x7fffffff
} PDMAUDIOBACKENDSTS;

/**
 * Audio stream context.
 */
typedef enum PDMAUDIOSTREAMCTX
{
    /** No context set / invalid. */
    PDMAUDIOSTREAMCTX_UNKNOWN = 0,
    /** Host stream, connected to a backend. */
    PDMAUDIOSTREAMCTX_HOST,
    /** Guest stream, connected to the device emulation. */
    PDMAUDIOSTREAMCTX_GUEST,
    /** Hack to blow the type up to 32-bit. */
    PDMAUDIOSTREAMCTX_32BIT_HACK = 0x7fffffff
} PDMAUDIOSTREAMCTX;

/**
 * Structure for keeping audio input stream specifics.
 * Do not use directly. Instead, use PDMAUDIOSTREAM.
 */
typedef struct PDMAUDIOSTREAMIN
{
    /** Timestamp (in ms) since last read. */
    uint64_t tsLastReadMS;
#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER StatBytesElapsed;
    STAMCOUNTER StatBytesTotalRead;
    STAMCOUNTER StatSamplesCaptured;
#endif
} PDMAUDIOSTREAMIN, *PPDMAUDIOSTREAMIN;

/**
 * Structure for keeping audio output stream specifics.
 * Do not use directly. Instead, use PDMAUDIOSTREAM.
 */
typedef struct PDMAUDIOSTREAMOUT
{
    /** Timestamp (in ms) since last write. */
    uint64_t    tsLastWriteMS;
#ifdef VBOX_WITH_STATISTICS
    STAMCOUNTER StatBytesElapsed;
    STAMCOUNTER StatBytesTotalWritten;
    STAMCOUNTER StatSamplesPlayed;
#endif
} PDMAUDIOSTREAMOUT, *PPDMAUDIOSTREAMOUT;

struct PDMAUDIOSTREAM;
typedef PDMAUDIOSTREAM *PPDMAUDIOSTREAM;

/**
 * Structure for maintaining an nput/output audio stream.
 */
typedef struct PDMAUDIOSTREAM
{
    /** List node. */
    RTLISTNODE             Node;
    /** Pointer to the other pair of this stream.
     *  This might be the host or guest side. */
    PPDMAUDIOSTREAM        pPair;
    /** Name of this stream. */
    char                   szName[64];
    /** Number of references to this stream. Only can be
     *  destroyed if the reference count is reaching 0. */
    uint32_t               cRefs;
    /** The stream's audio configuration. */
    PDMAUDIOSTREAMCFG      Cfg;
    /** Stream status flag. */
    PDMAUDIOSTRMSTS        fStatus;
    /** This stream's mixing buffer. */
    PDMAUDIOMIXBUF         MixBuf;
    /** Audio direction of this stream. */
    PDMAUDIODIR            enmDir;
    /** Context of this stream. */
    PDMAUDIOSTREAMCTX      enmCtx;
    /** Timestamp (in ms) since last iteration. */
    uint64_t               tsLastIterateMS;
    /** Union for input/output specifics. */
    union
    {
        PDMAUDIOSTREAMIN   In;
        PDMAUDIOSTREAMOUT  Out;
    };
} PDMAUDIOSTREAM, *PPDMAUDIOSTREAM;

/** Pointer to a audio connector interface. */
typedef struct PDMIAUDIOCONNECTOR *PPDMIAUDIOCONNECTOR;

/**
 * Audio callback types.
 * Those callbacks are being sent from the backends to the audio connector.
 */
typedef enum PDMAUDIOCBTYPE
{
    /** Invalid, do not use. */
    PDMAUDIOCBTYPE_INVALID = 0,
    /** The backend's status has changed. */
    PDMAUDIOCBTYPE_STATUS,
    /** One or more host audio devices have changed. */
    PDMAUDIOCBTYPE_DEVICES_CHANGED,
    /** Data is availabe as input for passing to the device emulation. */
    PDMAUDIOCBTYPE_DATA_INPUT,
    /** Free data for the device emulation to write to the backend. */
    PDMAUDIOCBTYPE_DATA_OUTPUT
} PDMAUDIOCBTYPE;

/**
 * Callback data for audio input.
 */
typedef struct PDMAUDIOCBDATA_DATA_INPUT
{
    /** Input: How many bytes are availabe as input for passing
     *         to the device emulation. */
    uint32_t cbInAvail;
    /** Output: How many bytes have been read. */
    uint32_t cbOutRead;
} PDMAUDIOCBDATA_DATA_INPUT, *PPDMAUDIOCBDATA_DATA_INPUT;

/**
 * Callback data for audio output.
 */
typedef struct PDMAUDIOCBDATA_DATA_OUTPUT
{
    /** Input:  How many bytes are free for the device emulation to write. */
    uint32_t cbInFree;
    /** Output: How many bytes were written by the device emulation. */
    uint32_t cbOutWritten;
} PDMAUDIOCBDATA_DATA_OUTPUT, *PPDMAUDIOCBDATA_DATA_OUTPUT;

/** Pointer to a host audio interface. */
typedef struct PDMIHOSTAUDIO *PPDMIHOSTAUDIO;

/**
 * Host audio (backend) callback function.
 *
 * @returns IPRT status code.
 * @param   pDrvIns             Pointer to driver instance which called us.
 * @param   enmType             Callback type.
 * @param   pvUser              User argument.
 * @param   cbUser              Size (in bytes) of user argument.
 */
typedef DECLCALLBACK(int) FNPDMHOSTAUDIOCALLBACK(PPDMDRVINS pDrvIns, PDMAUDIOCBTYPE enmType, void *pvUser, size_t cbUser);
/** Pointer to a FNPDMHOSTAUDIOCALLBACK(). */
typedef FNPDMHOSTAUDIOCALLBACK *PFNPDMHOSTAUDIOCALLBACK;

#ifdef VBOX_WITH_AUDIO_DEVICE_CALLBACKS
/**
 * Structure for keeping a registered audio callback around.
 */
typedef struct PDMAUDIOCALLBACK
{
    /** List node. */
    RTLISTANCHOR        Node;
    /** Callback type. */
    PDMAUDIOCBTYPE      enmType;
    /** Pointer to context data. Optional. */
    void               *pvCtx;
    /** Size (in bytes) of context data.
     *  Must be 0 if pvCtx is NULL. */
    size_t              cbCtx;
    /** Actual callback function to call. */
    PFNPDMAUDIOCALLBACK pFn;
} PDMAUDIOCALLBACK, *PPDMAUDIOCALLBACK;
#endif /* VBOX_WITH_AUDIO_DEVICE_CALLBACKS */

/**
 * Audio connector interface (up).
 */
typedef struct PDMIAUDIOCONNECTOR
{
    /**
     * Retrieves the current configuration of the host audio backend.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pCfg            Where to store the host audio backend configuration data.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConfig, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOBACKENDCFG pCfg));

    /**
     * Retrieves the current status of the host audio backend.
     *
     * @returns Status of the host audio backend.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   enmDir          Audio direction to check host audio backend for. Specify PDMAUDIODIR_ANY for the overall
     *                          backend status.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOBACKENDSTS, pfnGetStatus, (PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir));

    /**
     * Creates an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pCfgHost             Stream configuration for host side.
     * @param   pCfgGuest            Stream configuration for guest side.
     * @param   ppStream             Pointer where to return the created audio stream on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCreate, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest, PPDMAUDIOSTREAM *ppStream));

    /**
     * Destroys an audio stream.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Adds a reference to the specified audio stream.
     *
     * @returns New reference count. UINT32_MAX on error.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream adding the reference to.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamRetain, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Releases a reference from the specified stream.
     *
     * @returns New reference count. UINT32_MAX on error.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream releasing a reference from.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamRelease, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Reads PCM audio data from the host (input).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream to write to.
     * @param   pvBuf           Where to store the read data.
     * @param   cbBuf           Number of bytes to read.
     * @param   pcbRead         Bytes of audio data read. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamRead, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead));

    /**
     * Writes PCM audio data to the host (output).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream to read from.
     * @param   pvBuf           Audio data to be written.
     * @param   cbBuf           Number of bytes to be written.
     * @param   pcbWritten      Bytes of audio data written. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamWrite, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten));

    /**
     * Controls a specific audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     * @param   enmStreamCmd    The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamControl, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Processes stream data.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamIterate, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the number of readable data (in bytes) of a specific audio input stream.
     *
     * @returns Number of readable data (in bytes).
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetReadable, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the number of writable data (in bytes) of a specific audio output stream.
     *
     * @returns Number of writable data (in bytes).
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnStreamGetWritable, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Returns the status of a specific audio stream.
     *
     * @returns Audio stream status
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOSTRMSTS, pfnStreamGetStatus, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Sets the audio volume of a specific audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pStream         Pointer to audio stream.
     * @param   pVol            Pointer to audio volume structure to set the stream's audio volume to.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamSetVolume, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOVOLUME pVol));

    /**
     * Plays (transfers) available audio samples via the host backend. Only works with output streams.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pStream              Pointer to audio stream.
     * @param   pcSamplesPlayed      Number of samples played. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamPlay, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcSamplesPlayed));

    /**
     * Captures (transfers) available audio samples from the host backend. Only works with input streams.
     *
     * @returns VBox status code.
     * @param   pInterface           Pointer to the interface structure containing the called function pointer.
     * @param   pStream              Pointer to audio stream.
     * @param   pcSamplesCaptured    Number of samples captured. Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCapture, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcSamplesCaptured));

#ifdef VBOX_WITH_AUDIO_DEVICE_CALLBACKS
    DECLR3CALLBACKMEMBER(int, pfnRegisterCallbacks, (PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOCALLBACK paCallbacks, size_t cCallbacks));
    DECLR3CALLBACKMEMBER(int, pfnCallback, (PPDMIAUDIOCONNECTOR pInterface, PDMAUDIOCBTYPE enmType, void *pvUser, size_t cbUser));
#endif

} PDMIAUDIOCONNECTOR;

/** PDMIAUDIOCONNECTOR interface ID. */
#define PDMIAUDIOCONNECTOR_IID                  "FF2044D1-F8D9-4F42-BE9E-0E9AD14F4552"

/**
 * Assigns all needed interface callbacks for an audio backend.
 *
 * @param   a_Prefix        The function name prefix.
 */
#define PDMAUDIO_IHOSTAUDIO_CALLBACKS(a_Prefix) \
    do { \
        pThis->IHostAudio.pfnInit            = RT_CONCAT(a_Prefix,Init); \
        pThis->IHostAudio.pfnShutdown        = RT_CONCAT(a_Prefix,Shutdown); \
        pThis->IHostAudio.pfnGetConfig       = RT_CONCAT(a_Prefix,GetConfig); \
        /** @todo Add pfnGetDevices here as soon as supported by all backends. */ \
        pThis->IHostAudio.pfnGetStatus       = RT_CONCAT(a_Prefix,GetStatus); \
        /** @todo Ditto for pfnSetCallback. */ \
        pThis->IHostAudio.pfnStreamCreate    = RT_CONCAT(a_Prefix,StreamCreate); \
        pThis->IHostAudio.pfnStreamDestroy   = RT_CONCAT(a_Prefix,StreamDestroy); \
        pThis->IHostAudio.pfnStreamControl   = RT_CONCAT(a_Prefix,StreamControl); \
        pThis->IHostAudio.pfnStreamGetStatus = RT_CONCAT(a_Prefix,StreamGetStatus); \
        pThis->IHostAudio.pfnStreamIterate   = RT_CONCAT(a_Prefix,StreamIterate); \
        pThis->IHostAudio.pfnStreamPlay      = RT_CONCAT(a_Prefix,StreamPlay); \
        pThis->IHostAudio.pfnStreamCapture   = RT_CONCAT(a_Prefix,StreamCapture); \
    } while (0)

/**
 * PDM host audio interface.
 */
typedef struct PDMIHOSTAUDIO
{
    /**
     * Initializes the host backend (driver).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(int, pfnInit, (PPDMIHOSTAUDIO pInterface));

    /**
     * Shuts down the host backend (driver).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     */
    DECLR3CALLBACKMEMBER(void, pfnShutdown, (PPDMIHOSTAUDIO pInterface));

    /**
     * Returns the host backend's configuration (backend).
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pBackendCfg         Where to store the backend audio configuration to.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetConfig, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg));

    /**
     * Returns (enumerates) host audio device information.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pDeviceEnum         Where to return the enumerated audio devices.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetDevices, (PPDMIHOSTAUDIO pInterface, PPDMAUDIODEVICEENUM pDeviceEnum));

    /**
     * Returns the current status from the audio backend.
     *
     * @returns PDMAUDIOBACKENDSTS enum.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   enmDir              Audio direction to get status for. Pass PDMAUDIODIR_ANY for overall status.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOBACKENDSTS, pfnGetStatus, (PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir));

    /**
     * Sets a callback the audio backend can call. Optional.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pfnCallback         The callback function to use, or NULL when unregistering.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetCallback, (PPDMIHOSTAUDIO pInterface, PFNPDMHOSTAUDIOCALLBACK pfnCallback));

    /**
     * Creates an audio stream using the requested stream configuration.
     * If a backend is not able to create this configuration, it will return its best match in the acquired configuration
     * structure on success.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   pCfgReq             Pointer to requested stream configuration.
     * @param   pCfgAcq             Pointer to acquired stream configuration.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCreate, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq));

    /**
     * Destroys an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamDestroy, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Controls an audio stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   enmStreamCmd        The stream command to issue.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamControl, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd));

    /**
     * Returns whether the specified audio direction in the backend is enabled or not.
     *
     * @returns PDMAUDIOSTRMSTS
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(PDMAUDIOSTRMSTS, pfnStreamGetStatus, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Gives the host backend the chance to do some (necessary) iteration work.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamIterate, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream));

    /**
     * Plays (writes to) an audio (output) stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   pvBuf               Pointer to audio data buffer to play.
     * @param   cbBuf               Size (in bytes) of audio data buffer.
     * @param   pcbWritten          Returns number of bytes written.  Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamPlay, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten));

    /**
     * Captures (reads from) an audio (input) stream.
     *
     * @returns VBox status code.
     * @param   pInterface          Pointer to the interface structure containing the called function pointer.
     * @param   pStream             Pointer to audio stream.
     * @param   pvBuf               Buffer where to store read audio data.
     * @param   cbBuf               Size (in bytes) of buffer.
     * @param   pcbRead             Returns number of bytes read.  Optional.
     */
    DECLR3CALLBACKMEMBER(int, pfnStreamCapture, (PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead));

} PDMIHOSTAUDIO;

/** PDMIHOSTAUDIO interface ID. */
#define PDMIHOSTAUDIO_IID                           "C45550DE-03C0-4A45-9A96-C5EB956F806D"

/** @} */

#endif /* !___VBox_vmm_pdmaudioifs_h */

