#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <link.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <ucontext.h>
#include "Backtrace.h"

#  define ALWAYS_INLINE	inline __attribute__((always_inline))
#define UNW_LOCAL_ONLY
#define PROTECTED static
#define HIDDEN static
#define Debug(...)
#define Dprintf(...)
#define SIGPROCMASK(...)
#  define unlikely(x)   __builtin_expect ((x), 0)
#ifndef offsetof
#  define offsetof(type, member)  __builtin_offsetof (type, member)
#endif // offsetof
#ifndef PT_GNU_EH_FRAME
#define PT_GNU_EH_FRAME 0x6474e550
#endif // PT_GNU_EH_FRAME
#define ARRAY_SIZE(a)	(sizeof (a) / sizeof ((a)[0]))
typedef ucontext_t unw_tdep_context_t;

#ifndef dwarf_to_unw_regnum
/* REG is evaluated multiple times; it better be side-effects free!  */
# define dwarf_to_unw_regnum(reg)					  \
  (((reg) <= DWARF_REGNUM_MAP_LENGTH) ? dwarf_to_unw_regnum_map[reg] : 0)
#endif


typedef sigset_t intrmask_t;
typedef uint32_t unw_word_t;
typedef int unw_regnum_t;

#define DWARF_LOG_UNW_CACHE_SIZE	7
#define DW_EH_VERSION       1   /* The version we're implementing */

#define DWARF_UNW_CACHE_SIZE	(1 << DWARF_LOG_UNW_CACHE_SIZE)

#define DWARF_LOG_UNW_HASH_SIZE	(DWARF_LOG_UNW_CACHE_SIZE + 1)

#if defined(__i386__)
# define tdep_find_proc_info(c,ip,n)				\
	dwarf_find_proc_info((ip), &(c)->pi, (n))
# define dwarf_addr_size() (4)
#define Elf_W(type) Elf32_##type
#define UNW_TDEP_NUM_EH_REGS    2   /* eax and edx are exception args */
#define DWARF_REGNUM_MAP_LENGTH		19
#define DWARF_NUM_PRESERVED_REGS	17
#define unw_is_signal_frame(...) 0

#define DWARF_GET_LOC(l)	((l).val)
typedef struct
  {
    /* no x86-specific auxiliary proc-info */
    /* ANDROID support update. */
    char __reserved;
    /* End of ANDROID update. */
  }
unw_tdep_proc_info_t;


typedef struct dwarf_loc
  {
    unw_word_t val;
#ifndef UNW_LOCAL_ONLY
    unw_word_t type;		/* see X86_LOC_TYPE_* macros.  */
#endif
  }
dwarf_loc_t;

typedef uint32_t unw_word_t;
typedef int32_t unw_sword_t;

#define EAX	0
#define ECX	1
#define EDX	2
#define EBX	3
#define ESP	4
#define EBP	5
#define ESI	6
#define EDI	7
#define EIP	8
#define EFLAGS	9
#define TRAPNO	10
#define ST0	11
#define UNW_TDEP_CURSOR_LEN 127


#define tdep_fetch_frame(c,ip,n)	do {} while(0)
#define dwarf_to_cursor(c)  ((unw_cursor_t *) (c))
#define dwarf_is_big_endian() (0)
#define tdep_stash_frame(c,rs)      do {} while(0)

typedef enum
  {
    /* Note: general registers are expected to start with index 0.
       This convention facilitates architecture-independent
       implementation of the C++ exception handling ABI.  See
       _Unwind_SetGR() and _Unwind_GetGR() for details.

       The described register usage convention is based on "System V
       Application Binary Interface, Intel386 Architecture Processor
       Supplement, Fourth Edition" at

         http://www.linuxbase.org/spec/refspecs/elf/abi386-4.pdf

       It would have been nice to use the same register numbering as
       DWARF, but that doesn't work because the libunwind requires
       that the exception argument registers be consecutive, which the
       wouldn't be with the DWARF numbering.  */
    UNW_X86_EAX,	/* scratch (exception argument 1) */
    UNW_X86_EDX,	/* scratch (exception argument 2) */
    UNW_X86_ECX,	/* scratch */
    UNW_X86_EBX,	/* preserved */
    UNW_X86_ESI,	/* preserved */
    UNW_X86_EDI,	/* preserved */
    UNW_X86_EBP,	/* (optional) frame-register */
    UNW_X86_ESP,	/* (optional) frame-register */
    UNW_X86_EIP,	/* frame-register */
    UNW_X86_EFLAGS,	/* scratch (except for "direction", which is fixed */
    UNW_X86_TRAPNO,	/* scratch */

    /* MMX/stacked-fp registers */
    UNW_X86_ST0,	/* fp return value */
    UNW_X86_ST1,	/* scratch */
    UNW_X86_ST2,	/* scratch */
    UNW_X86_ST3,	/* scratch */
    UNW_X86_ST4,	/* scratch */
    UNW_X86_ST5,	/* scratch */
    UNW_X86_ST6,	/* scratch */
    UNW_X86_ST7,	/* scratch */

    UNW_X86_FCW,	/* scratch */
    UNW_X86_FSW,	/* scratch */
    UNW_X86_FTW,	/* scratch */
    UNW_X86_FOP,	/* scratch */
    UNW_X86_FCS,	/* scratch */
    UNW_X86_FIP,	/* scratch */
    UNW_X86_FEA,	/* scratch */
    UNW_X86_FDS,	/* scratch */

    /* SSE registers */
    UNW_X86_XMM0_lo,	/* scratch */
    UNW_X86_XMM0_hi,	/* scratch */
    UNW_X86_XMM1_lo,	/* scratch */
    UNW_X86_XMM1_hi,	/* scratch */
    UNW_X86_XMM2_lo,	/* scratch */
    UNW_X86_XMM2_hi,	/* scratch */
    UNW_X86_XMM3_lo,	/* scratch */
    UNW_X86_XMM3_hi,	/* scratch */
    UNW_X86_XMM4_lo,	/* scratch */
    UNW_X86_XMM4_hi,	/* scratch */
    UNW_X86_XMM5_lo,	/* scratch */
    UNW_X86_XMM5_hi,	/* scratch */
    UNW_X86_XMM6_lo,	/* scratch */
    UNW_X86_XMM6_hi,	/* scratch */
    UNW_X86_XMM7_lo,	/* scratch */
    UNW_X86_XMM7_hi,	/* scratch */

    UNW_X86_MXCSR,	/* scratch */

    /* segment registers */
    UNW_X86_GS,		/* special */
    UNW_X86_FS,		/* special */
    UNW_X86_ES,		/* special */
    UNW_X86_DS,		/* special */
    UNW_X86_SS,		/* special */
    UNW_X86_CS,		/* special */
    UNW_X86_TSS,	/* special */
    UNW_X86_LDT,	/* special */

    /* frame info (read-only) */
    UNW_X86_CFA,

    UNW_X86_XMM0,	/* scratch */
    UNW_X86_XMM1,	/* scratch */
    UNW_X86_XMM2,	/* scratch */
    UNW_X86_XMM3,	/* scratch */
    UNW_X86_XMM4,	/* scratch */
    UNW_X86_XMM5,	/* scratch */
    UNW_X86_XMM6,	/* scratch */
    UNW_X86_XMM7,	/* scratch */

    UNW_TDEP_LAST_REG = UNW_X86_XMM7,

    UNW_TDEP_IP = UNW_X86_EIP,
    UNW_TDEP_SP = UNW_X86_ESP,
    UNW_TDEP_EH = UNW_X86_EAX
  }
x86_regnum_t;

HIDDEN const uint8_t dwarf_to_unw_regnum_map[19] =
  {
    UNW_X86_EAX, UNW_X86_ECX, UNW_X86_EDX, UNW_X86_EBX,
    UNW_X86_ESP, UNW_X86_EBP, UNW_X86_ESI, UNW_X86_EDI,
    UNW_X86_EIP, UNW_X86_EFLAGS, UNW_X86_TRAPNO,
    UNW_X86_ST0, UNW_X86_ST1, UNW_X86_ST2, UNW_X86_ST3,
    UNW_X86_ST4, UNW_X86_ST5, UNW_X86_ST6, UNW_X86_ST7
  };
#define tdep_get_ip(c)          ((c)->dwarf.ip)

# define DWARF_NULL_LOC		DWARF_LOC (0, 0)
# define DWARF_IS_NULL_LOC(l)	(DWARF_GET_LOC (l) == 0)
# define DWARF_LOC(r, t)	((dwarf_loc_t) { .val = (r) })
# define DWARF_IS_REG_LOC(l)	0
# define DWARF_REG_LOC(c,r)	(DWARF_LOC((unw_word_t)			     \
				 tdep_uc_addr(dwarf_get_uc(c), (r)), 0))
# define DWARF_MEM_LOC(c,m)	DWARF_LOC ((m), 0)
# define DWARF_FPREG_LOC(c,r)	(DWARF_LOC((unw_word_t)			     \
				 tdep_uc_addr(dwarf_get_uc(c), (r)), 0))


HIDDEN void *
x86_r_uc_addr (ucontext_t *uc, int reg)
{
  void *addr;

  switch (reg)
    {
    case UNW_X86_GS:  addr = &uc->uc_mcontext.gregs[REG_GS]; break;
    case UNW_X86_FS:  addr = &uc->uc_mcontext.gregs[REG_FS]; break;
    case UNW_X86_ES:  addr = &uc->uc_mcontext.gregs[REG_ES]; break;
    case UNW_X86_DS:  addr = &uc->uc_mcontext.gregs[REG_DS]; break;
    case UNW_X86_EAX: addr = &uc->uc_mcontext.gregs[REG_EAX]; break;
    case UNW_X86_EBX: addr = &uc->uc_mcontext.gregs[REG_EBX]; break;
    case UNW_X86_ECX: addr = &uc->uc_mcontext.gregs[REG_ECX]; break;
    case UNW_X86_EDX: addr = &uc->uc_mcontext.gregs[REG_EDX]; break;
    case UNW_X86_ESI: addr = &uc->uc_mcontext.gregs[REG_ESI]; break;
    case UNW_X86_EDI: addr = &uc->uc_mcontext.gregs[REG_EDI]; break;
    case UNW_X86_EBP: addr = &uc->uc_mcontext.gregs[REG_EBP]; break;
    case UNW_X86_EIP: addr = &uc->uc_mcontext.gregs[REG_EIP]; break;
    case UNW_X86_ESP: addr = &uc->uc_mcontext.gregs[REG_ESP]; break;
    case UNW_X86_TRAPNO:  addr = &uc->uc_mcontext.gregs[REG_TRAPNO]; break;
    case UNW_X86_CS:  addr = &uc->uc_mcontext.gregs[REG_CS]; break;
    case UNW_X86_EFLAGS:  addr = &uc->uc_mcontext.gregs[REG_EFL]; break;
    case UNW_X86_SS:  addr = &uc->uc_mcontext.gregs[REG_SS]; break;

    default:
      addr = NULL;
    }
  return addr;
}

HIDDEN void *
tdep_uc_addr (ucontext_t *uc, int reg)
{
  return x86_r_uc_addr (uc, reg);
}

#endif

#define DWARF_UNW_HASH_SIZE	(1 << DWARF_LOG_UNW_HASH_SIZE)

#define DW_EH_PE_FORMAT_MASK	0x0f	/* format of the encoded value */
#define DW_EH_PE_APPL_MASK	0x70	/* how the value is to be applied */
/* Flag bit.  If set, the resulting pointer is the address of the word
   that contains the final address.  */
#define DW_EH_PE_indirect	0x80
#define DW_EH_PE_omit		0xff
#define DW_EH_PE_ptr		0x00	/* pointer-sized unsigned value */
#define DW_EH_PE_uleb128	0x01	/* unsigned LE base-128 value */
#define DW_EH_PE_udata2		0x02	/* unsigned 16-bit value */
#define DW_EH_PE_udata4		0x03	/* unsigned 32-bit value */
#define DW_EH_PE_udata8		0x04	/* unsigned 64-bit value */
#define DW_EH_PE_sleb128	0x09	/* signed LE base-128 value */
#define DW_EH_PE_sdata2		0x0a	/* signed 16-bit value */
#define DW_EH_PE_sdata4		0x0b	/* signed 32-bit value */
#define DW_EH_PE_sdata8		0x0c	/* signed 64-bit value */

/* Pointer-encoding application: */
#define DW_EH_PE_absptr		0x00	/* absolute value */
#define DW_EH_PE_pcrel		0x10	/* rel. to addr. of encoded value */
#define DW_EH_PE_textrel	0x20	/* text-relative (GCC-specific???) */
#define DW_EH_PE_datarel	0x30	/* data-relative */
/* The following are not documented by LSB v1.3, yet they are used by
   GCC, presumably they aren't documented by LSB since they aren't
   used on Linux:  */
#define DW_EH_PE_funcrel	0x40	/* start-of-procedure-relative */
#define DW_EH_PE_aligned	0x50	/* aligned pointer */

/* Each target may define it's own set of flags, but bits 0-15 are
   reserved for general libunwind-use.  */
#define UNW_PI_FLAG_FIRST_TDEP_BIT	16
/* The information comes from a .debug_frame section.  */
#define UNW_PI_FLAG_DEBUG_FRAME	32

// typedef uint32_t __u32;
// typedef uint16_t __u16;
// typedef int32_t __s32;
// typedef int16_t __s16;
// typedef int64_t __s64;
// typedef uint64_t __u64;
// 
// typedef __u32 Elf32_Addr;
// typedef __u16 Elf32_Half;
// typedef __u32 Elf32_Off;
// typedef __s32 Elf32_Sword;
// typedef __u32 Elf32_Word;
// 
// typedef __u64 Elf64_Addr;
// typedef __u16 Elf64_Half;
// typedef __s16 Elf64_SHalf;
// typedef __u64 Elf64_Off;
// typedef __s32 Elf64_Sword;
// typedef __u32 Elf64_Word;
// typedef __u64 Elf64_Xword;
// typedef __s64 Elf64_Sxword;

// typedef struct elf32_phdr{
//  Elf32_Word p_type;
//  Elf32_Off p_offset;
//  Elf32_Addr p_vaddr;
//  Elf32_Addr p_paddr;
//  Elf32_Word p_filesz;
//  Elf32_Word p_memsz;
//  Elf32_Word p_flags;
//  Elf32_Word p_align;
// } Elf32_Phdr;
// 
// typedef struct elf64_phdr {
//  Elf64_Word p_type;
//  Elf64_Word p_flags;
//  Elf64_Off p_offset;
//  Elf64_Addr p_vaddr;
//  Elf64_Addr p_paddr;
//  Elf64_Xword p_filesz;
//  Elf64_Xword p_memsz;
//  Elf64_Xword p_align;
// } Elf64_Phdr;

struct unw_cursor_t {
};

typedef struct unw_proc_info
  {
    unw_word_t start_ip;	/* first IP covered by this procedure */
    unw_word_t end_ip;		/* first IP NOT covered by this procedure */
    unw_word_t lsda;		/* address of lang.-spec. data area (if any) */
    unw_word_t handler;		/* optional personality routine */
    unw_word_t gp;		/* global-pointer value for this procedure */
    unw_word_t flags;		/* misc. flags */

    int format;			/* unwind-info format (arch-specific) */
    int unwind_info_size;	/* size of the information (if applicable) */
    void *unwind_info;		/* unwind-info (arch-specific) */
    unw_tdep_proc_info_t extra;	/* target-dependent auxiliary proc-info */
  }
unw_proc_info_t;


struct dwarf_cursor
  {
    void *as_arg;		/* argument to address-space callbacks */

    unw_word_t cfa;	/* canonical frame address; aka frame-/stack-pointer */
    unw_word_t ip;		/* instruction pointer */
    unw_word_t args_size;	/* size of arguments */
    unw_word_t ret_addr_column;	/* column for return-address */
    unw_word_t eh_args[UNW_TDEP_NUM_EH_REGS];
    unsigned int eh_valid_mask;
    /* ANDROID support update. */
    unsigned int frame;
    /* End of ANDROID update. */

    dwarf_loc_t loc[DWARF_NUM_PRESERVED_REGS];

    unsigned int stash_frames :1; /* stash frames for fast lookup */
    unsigned int use_prev_instr :1; /* use previous (= call) or current (= signal) instruction? */
    unsigned int pi_valid :1;	/* is proc_info valid? */
    unsigned int pi_is_dynamic :1; /* proc_info found via dynamic proc info? */
    unw_proc_info_t pi;		/* info about current procedure */

    short hint; /* faster lookup of the rs cache */
    short prev_rs;
  };

struct cursor {
    struct dwarf_cursor dwarf;      /* must be first */
    ucontext_t *uc;
};


typedef enum
  {
    DWARF_WHERE_UNDEF,		/* register isn't saved at all */
    DWARF_WHERE_SAME,		/* register has same value as in prev. frame */
    DWARF_WHERE_CFAREL,		/* register saved at CFA-relative address */
    DWARF_WHERE_REG,		/* register saved in another register */
    DWARF_WHERE_EXPR,		/* register saved */
  }
dwarf_where_t;

typedef enum
  {
    UNW_INFO_FORMAT_DYNAMIC,		/* unw_dyn_proc_info_t */
    UNW_INFO_FORMAT_TABLE,		/* unw_dyn_table_t */
    UNW_INFO_FORMAT_REMOTE_TABLE,	/* unw_dyn_remote_table_t */
    UNW_INFO_FORMAT_ARM_EXIDX		/* ARM specific unwind info */
  }
unw_dyn_info_format_t;

typedef enum
  {
    UNW_ESUCCESS = 0,		/* no error */
    UNW_EUNSPEC,		/* unspecified (general) error */
    UNW_ENOMEM,			/* out of memory */
    UNW_EBADREG,		/* bad register number */
    UNW_EREADONLYREG,		/* attempt to write read-only register */
    UNW_ESTOPUNWIND,		/* stop unwinding */
    UNW_EINVALIDIP,		/* invalid IP */
    UNW_EBADFRAME,		/* bad frame */
    UNW_EINVAL,			/* unsupported operation or bad value */
    UNW_EBADVERSION,		/* unwind info has unsupported version */
    UNW_ENOINFO			/* no unwind info found */
  }
unw_error_t;

#define DWARF_CFA_OPCODE_MASK	0xc0
#define DWARF_CFA_OPERAND_MASK	0x3f

typedef enum
  {
    DW_CFA_advance_loc		= 0x40,
    DW_CFA_offset		= 0x80,
    DW_CFA_restore		= 0xc0,
    DW_CFA_nop			= 0x00,
    DW_CFA_set_loc		= 0x01,
    DW_CFA_advance_loc1		= 0x02,
    DW_CFA_advance_loc2		= 0x03,
    DW_CFA_advance_loc4		= 0x04,
    DW_CFA_offset_extended	= 0x05,
    DW_CFA_restore_extended	= 0x06,
    DW_CFA_undefined		= 0x07,
    DW_CFA_same_value		= 0x08,
    DW_CFA_register		= 0x09,
    DW_CFA_remember_state	= 0x0a,
    DW_CFA_restore_state	= 0x0b,
    DW_CFA_def_cfa		= 0x0c,
    DW_CFA_def_cfa_register	= 0x0d,
    DW_CFA_def_cfa_offset	= 0x0e,
    DW_CFA_def_cfa_expression	= 0x0f,
    DW_CFA_expression		= 0x10,
    DW_CFA_offset_extended_sf	= 0x11,
    DW_CFA_def_cfa_sf		= 0x12,
    DW_CFA_def_cfa_offset_sf	= 0x13,
    DW_CFA_lo_user		= 0x1c,
    DW_CFA_MIPS_advance_loc8	= 0x1d,
    DW_CFA_GNU_window_save	= 0x2d,
    DW_CFA_GNU_args_size	= 0x2e,
    DW_CFA_GNU_negative_offset_extended	= 0x2f,
    DW_CFA_hi_user		= 0x3c
  }
dwarf_cfa_t;

/* The following enum defines the indices for a couple of
   (pseudo-)registers which have the same meaning across all
   platforms.  (RO) means read-only.  (RW) means read-write.  General
   registers (aka "integer registers") are expected to start with
   index 0.  The number of such registers is architecture-dependent.
   The remaining indices can be used as an architecture sees fit.  The
   last valid register index is given by UNW_REG_LAST.  */
typedef enum
  {
    UNW_REG_IP = UNW_TDEP_IP,		/* (rw) instruction pointer (pc) */
    UNW_REG_SP = UNW_TDEP_SP,		/* (ro) stack pointer */
    UNW_REG_EH = UNW_TDEP_EH,		/* (rw) exception-handling reg base */
    UNW_REG_LAST = UNW_TDEP_LAST_REG
  }
unw_frame_regnum_t;

#define MAX_EXPR_STACK_SIZE	64

#define NUM_OPERANDS(signature)	(((signature) >> 6) & 0x3)
#define OPND1_TYPE(signature)	(((signature) >> 3) & 0x7)
#define OPND2_TYPE(signature)	(((signature) >> 0) & 0x7)

#define OPND_SIGNATURE(n, t1, t2) (((n) << 6) | ((t1) << 3) | ((t2) << 0))
#define OPND1(t1)		OPND_SIGNATURE(1, t1, 0)
#define OPND2(t1, t2)		OPND_SIGNATURE(2, t1, t2)

#define VAL8	0x0
#define VAL16	0x1
#define VAL32	0x2
#define VAL64	0x3
#define ULEB128	0x4
#define SLEB128	0x5
#define OFFSET	0x6	/* 32-bit offset for 32-bit DWARF, 64-bit otherwise */
#define ADDR	0x7	/* Machine address.  */

typedef enum
  {
    DW_OP_addr			= 0x03,
    DW_OP_deref			= 0x06,
    DW_OP_const1u		= 0x08,
    DW_OP_const1s		= 0x09,
    DW_OP_const2u		= 0x0a,
    DW_OP_const2s		= 0x0b,
    DW_OP_const4u		= 0x0c,
    DW_OP_const4s		= 0x0d,
    DW_OP_const8u		= 0x0e,
    DW_OP_const8s		= 0x0f,
    DW_OP_constu		= 0x10,
    DW_OP_consts		= 0x11,
    DW_OP_dup			= 0x12,
    DW_OP_drop			= 0x13,
    DW_OP_over			= 0x14,
    DW_OP_pick			= 0x15,
    DW_OP_swap			= 0x16,
    DW_OP_rot			= 0x17,
    DW_OP_xderef		= 0x18,
    DW_OP_abs			= 0x19,
    DW_OP_and			= 0x1a,
    DW_OP_div			= 0x1b,
    DW_OP_minus			= 0x1c,
    DW_OP_mod			= 0x1d,
    DW_OP_mul			= 0x1e,
    DW_OP_neg			= 0x1f,
    DW_OP_not			= 0x20,
    DW_OP_or			= 0x21,
    DW_OP_plus			= 0x22,
    DW_OP_plus_uconst		= 0x23,
    DW_OP_shl			= 0x24,
    DW_OP_shr			= 0x25,
    DW_OP_shra			= 0x26,
    DW_OP_xor			= 0x27,
    DW_OP_skip			= 0x2f,
    DW_OP_bra			= 0x28,
    DW_OP_eq			= 0x29,
    DW_OP_ge			= 0x2a,
    DW_OP_gt			= 0x2b,
    DW_OP_le			= 0x2c,
    DW_OP_lt			= 0x2d,
    DW_OP_ne			= 0x2e,
    DW_OP_lit0			= 0x30,
    DW_OP_lit1,  DW_OP_lit2,  DW_OP_lit3,  DW_OP_lit4,  DW_OP_lit5,
    DW_OP_lit6,  DW_OP_lit7,  DW_OP_lit8,  DW_OP_lit9,  DW_OP_lit10,
    DW_OP_lit11, DW_OP_lit12, DW_OP_lit13, DW_OP_lit14, DW_OP_lit15,
    DW_OP_lit16, DW_OP_lit17, DW_OP_lit18, DW_OP_lit19, DW_OP_lit20,
    DW_OP_lit21, DW_OP_lit22, DW_OP_lit23, DW_OP_lit24, DW_OP_lit25,
    DW_OP_lit26, DW_OP_lit27, DW_OP_lit28, DW_OP_lit29, DW_OP_lit30,
    DW_OP_lit31,
    DW_OP_reg0			= 0x50,
    DW_OP_reg1,  DW_OP_reg2,  DW_OP_reg3,  DW_OP_reg4,  DW_OP_reg5,
    DW_OP_reg6,  DW_OP_reg7,  DW_OP_reg8,  DW_OP_reg9,  DW_OP_reg10,
    DW_OP_reg11, DW_OP_reg12, DW_OP_reg13, DW_OP_reg14, DW_OP_reg15,
    DW_OP_reg16, DW_OP_reg17, DW_OP_reg18, DW_OP_reg19, DW_OP_reg20,
    DW_OP_reg21, DW_OP_reg22, DW_OP_reg23, DW_OP_reg24, DW_OP_reg25,
    DW_OP_reg26, DW_OP_reg27, DW_OP_reg28, DW_OP_reg29, DW_OP_reg30,
    DW_OP_reg31,
    DW_OP_breg0			= 0x70,
    DW_OP_breg1,  DW_OP_breg2,  DW_OP_breg3,  DW_OP_breg4,  DW_OP_breg5,
    DW_OP_breg6,  DW_OP_breg7,  DW_OP_breg8,  DW_OP_breg9,  DW_OP_breg10,
    DW_OP_breg11, DW_OP_breg12, DW_OP_breg13, DW_OP_breg14, DW_OP_breg15,
    DW_OP_breg16, DW_OP_breg17, DW_OP_breg18, DW_OP_breg19, DW_OP_breg20,
    DW_OP_breg21, DW_OP_breg22, DW_OP_breg23, DW_OP_breg24, DW_OP_breg25,
    DW_OP_breg26, DW_OP_breg27, DW_OP_breg28, DW_OP_breg29, DW_OP_breg30,
    DW_OP_breg31,
    DW_OP_regx			= 0x90,
    DW_OP_fbreg			= 0x91,
    DW_OP_bregx			= 0x92,
    DW_OP_piece			= 0x93,
    DW_OP_deref_size		= 0x94,
    DW_OP_xderef_size		= 0x95,
    DW_OP_nop			= 0x96,
    DW_OP_push_object_address	= 0x97,
    DW_OP_call2			= 0x98,
    DW_OP_call4			= 0x99,
    DW_OP_call_ref		= 0x9a,
    DW_OP_lo_user		= 0xe0,
    DW_OP_hi_user		= 0xff
  }
dwarf_expr_op_t;

typedef struct
  {
    dwarf_where_t where;	/* how is the register saved? */
    unw_word_t val;		/* where it's saved */
  }
dwarf_save_loc_t;

#define DWARF_CFA_REG_COLUMN	DWARF_NUM_PRESERVED_REGS
#define DWARF_CFA_OFF_COLUMN	(DWARF_NUM_PRESERVED_REGS + 1)

typedef struct dwarf_reg_state
  {
    struct dwarf_reg_state *next;	/* for rs_stack */
    dwarf_save_loc_t reg[DWARF_NUM_PRESERVED_REGS + 2];
    unw_word_t ip;		          /* ip this rs is for */
    unw_word_t ret_addr_column;           /* indicates which column in the rule table represents return address */
    unsigned short lru_chain;	  /* used for least-recently-used chain */
    unsigned short coll_chain;	/* used for hash collisions */
    unsigned short hint;	      /* hint for next rs to try (or -1) */
    unsigned short valid : 1;         /* optional machine-dependent signal info */
    unsigned short signal_frame : 1;  /* optional machine-dependent signal info */
  }
dwarf_reg_state_t;

struct dwarf_rs_cache
  {
    unsigned short lru_head;	/* index of lead-recently used rs */
    unsigned short lru_tail;	/* index of most-recently used rs */

    /* hash table that maps instruction pointer to rs index: */
    unsigned short hash[DWARF_UNW_HASH_SIZE];

    uint32_t generation;	/* generation number */

    /* rs cache: */
    dwarf_reg_state_t buckets[DWARF_UNW_CACHE_SIZE];
  };

typedef struct dwarf_state_record
  {
    unsigned char fde_encoding;
    unw_word_t args_size;

    dwarf_reg_state_t rs_initial;	/* reg-state after CIE instructions */
    dwarf_reg_state_t rs_current;	/* current reg-state */
  }
dwarf_state_record_t;

typedef struct unw_dyn_op
  {
    int8_t tag;				/* what operation? */
    int8_t qp;				/* qualifying predicate register */
    int16_t reg;			/* what register */
    int32_t when;			/* when does it take effect? */
    unw_word_t val;			/* auxiliary value */
  }
unw_dyn_op_t;


typedef struct unw_dyn_region_info
  {
    struct unw_dyn_region_info *next;	/* linked list of regions */
    int32_t insn_count;			/* region length (# of instructions) */
    uint32_t op_count;			/* length of op-array */
    unw_dyn_op_t op[1];			/* variable-length op-array */
  }
unw_dyn_region_info_t;


typedef struct unw_dyn_proc_info
  {
    unw_word_t name_ptr;	/* address of human-readable procedure name */
    unw_word_t handler;		/* address of personality routine */
    uint32_t flags;
    int32_t pad0;
    unw_dyn_region_info_t *regions;
  }
unw_dyn_proc_info_t;

typedef struct unw_dyn_table_info
  {
    unw_word_t name_ptr;	/* addr. of table name (e.g., library name) */
    unw_word_t segbase;		/* segment base */
    unw_word_t table_len;	/* must be a multiple of sizeof(unw_word_t)! */
    unw_word_t *table_data;
  }
unw_dyn_table_info_t;

typedef struct unw_dyn_remote_table_info
  {
    unw_word_t name_ptr;	/* addr. of table name (e.g., library name) */
    unw_word_t segbase;		/* segment base */
    unw_word_t table_len;	/* must be a multiple of sizeof(unw_word_t)! */
    unw_word_t table_data;
  }
unw_dyn_remote_table_info_t;

typedef struct unw_dyn_info
  {
    /* doubly-linked list of dyn-info structures: */
    struct unw_dyn_info *next;
    struct unw_dyn_info *prev;
    unw_word_t start_ip;	/* first IP covered by this entry */
    unw_word_t end_ip;		/* first IP NOT covered by this entry */
    unw_word_t gp;		/* global-pointer in effect for this entry */
    int32_t format;		/* real type: unw_dyn_info_format_t */
    int32_t pad;
    union
      {
	unw_dyn_proc_info_t pi;
	unw_dyn_table_info_t ti;
	unw_dyn_remote_table_info_t rti;
      }
    u;
  }
unw_dyn_info_t;

struct dwarf_callback_data
  {
    /* in: */
    unw_word_t ip;		/* instruction-pointer we're looking for */
    unw_proc_info_t *pi;	/* proc-info pointer */
    int need_unwind_info;
    /* out: */
    int single_fde;		/* did we find a single FDE? (vs. a table) */
    unw_dyn_info_t di;		/* table info (if single_fde is false) */
    unw_dyn_info_t di_debug;	/* additional table info for .debug_frame */
  };

struct dwarf_eh_frame_hdr
  {
    unsigned char version;
    unsigned char eh_frame_ptr_enc;
    unsigned char fde_count_enc;
    unsigned char table_enc;
    /* The rest of the header is variable-length and consists of the
       following members:

	encoded_t eh_frame_ptr;
	encoded_t fde_count;
	struct
	  {
	    encoded_t start_ip;	// first address covered by this FDE
	    encoded_t fde_addr;	// address of the FDE
	  }
	binary_search_table[fde_count];  */
  };

typedef union __attribute__ ((packed))
  {
    int8_t s8;
    int16_t s16;
    int32_t s32;
    int64_t s64;
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    void *ptr;
  }
dwarf_misaligned_value_t;

typedef struct dwarf_cie_info
  {
    unw_word_t cie_instr_start;	/* start addr. of CIE "initial_instructions" */
    unw_word_t cie_instr_end;	/* end addr. of CIE "initial_instructions" */
    unw_word_t fde_instr_start;	/* start addr. of FDE "instructions" */
    unw_word_t fde_instr_end;	/* end addr. of FDE "instructions" */
    unw_word_t code_align;	/* code-alignment factor */
    unw_word_t data_align;	/* data-alignment factor */
    unw_word_t ret_addr_column;	/* column of return-address register */
    unw_word_t handler;		/* address of personality-routine */
    uint16_t abi;
    uint16_t tag;
    uint8_t fde_encoding;
    uint8_t lsda_encoding;
    unsigned int sized_augmentation : 1;
    unsigned int have_abi_marker : 1;
    unsigned int signal_frame : 1;
  }
dwarf_cie_info_t;

struct table_entry
  {
    int32_t start_ip_offset;
    int32_t fde_offset;
  };

struct unw_debug_frame_list
  {
    /* The start (inclusive) and end (exclusive) of the described region.  */
    unw_word_t start;
    unw_word_t end;
    /* The debug frame itself.  */
    char *debug_frame;
    size_t debug_frame_size;
    /* Relocation amount since debug_frame was compressed. */
    unw_word_t segbase_bias;
    /* Index (for binary search).  */
    struct table_entry *index;
    size_t index_size;
    /* Pointer to next descriptor.  */
    struct unw_debug_frame_list *next;
  };

typedef struct unw_cursor
  {
    unw_word_t opaque[UNW_TDEP_CURSOR_LEN];
  }
unw_cursor_t;
PROTECTED int
unw_get_reg (unw_cursor_t *cursor, int regnum, unw_word_t *valp);

static const uint8_t operands[256] =
  {
    [DW_OP_addr] =		OPND1 (ADDR),
    [DW_OP_const1u] =		OPND1 (VAL8),
    [DW_OP_const1s] =		OPND1 (VAL8),
    [DW_OP_const2u] =		OPND1 (VAL16),
    [DW_OP_const2s] =		OPND1 (VAL16),
    [DW_OP_const4u] =		OPND1 (VAL32),
    [DW_OP_const4s] =		OPND1 (VAL32),
    [DW_OP_const8u] =		OPND1 (VAL64),
    [DW_OP_const8s] =		OPND1 (VAL64),
    [DW_OP_pick] =		OPND1 (VAL8),
    [DW_OP_plus_uconst] =	OPND1 (ULEB128),
    [DW_OP_skip] =		OPND1 (VAL16),
    [DW_OP_bra] =		OPND1 (VAL16),
    [DW_OP_breg0 +  0] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  1] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  2] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  3] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  4] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  5] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  6] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  7] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  8] =	OPND1 (SLEB128),
    [DW_OP_breg0 +  9] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 10] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 11] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 12] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 13] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 14] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 15] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 16] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 17] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 18] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 19] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 20] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 21] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 22] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 23] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 24] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 25] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 26] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 27] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 28] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 29] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 30] =	OPND1 (SLEB128),
    [DW_OP_breg0 + 31] =	OPND1 (SLEB128),
    [DW_OP_regx] =		OPND1 (ULEB128),
    [DW_OP_fbreg] =		OPND1 (SLEB128),
    [DW_OP_bregx] =		OPND2 (ULEB128, SLEB128),
    [DW_OP_piece] =		OPND1 (ULEB128),
    [DW_OP_deref_size] =	OPND1 (VAL8),
    [DW_OP_xderef_size] =	OPND1 (VAL8),
    [DW_OP_call2] =		OPND1 (VAL16),
    [DW_OP_call4] =		OPND1 (VAL32),
    [DW_OP_call_ref] =		OPND1 (OFFSET)
  };

#define alloc_reg_state()	(malloc (sizeof(dwarf_reg_state_t)))
#define free_reg_state(rs)	(free (rs))

static inline int
dwarf_reads8 (unw_word_t *addr,
	      int8_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->s8;
  *addr += sizeof (mvp->s8);
  return 0;
}

static inline int
dwarf_reads16 (unw_word_t *addr,
	       int16_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->s16;
  *addr += sizeof (mvp->s16);
  return 0;
}

static inline ucontext_t *
dwarf_get_uc(const struct dwarf_cursor *cursor)
{
  const struct cursor *c = (struct cursor *) cursor->as_arg;
  return c->uc;
}

static inline int
dwarf_reads32 (unw_word_t *addr,
	       int32_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->s32;
  *addr += sizeof (mvp->s32);
  return 0;
}

static inline int
dwarf_reads64 (unw_word_t *addr,
	       int64_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->s64;
  *addr += sizeof (mvp->s64);
  return 0;
}

static inline int
dwarf_readu8 (unw_word_t *addr,
	      uint8_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->u8;
  *addr += sizeof (mvp->u8);
  return 0;
}

static inline int
dwarf_readu16 (unw_word_t *addr,
	       uint16_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->u16;
  *addr += sizeof (mvp->u16);
  return 0;
}

static inline int
dwarf_readu32 (unw_word_t *addr,
	       uint32_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->u32;
  *addr += sizeof (mvp->u32);
  return 0;
}

static inline int
dwarf_readu64 (unw_word_t *addr,
	       uint64_t *val)
{
  dwarf_misaligned_value_t *mvp = (void *) (uintptr_t) *addr;

  *val = mvp->u64;
  *addr += sizeof (mvp->u64);
  return 0;
}

static inline int
dwarf_readw (unw_word_t *addr,
	     unw_word_t *val)
{
  uint32_t u32;
  uint64_t u64;
  int ret;

  switch (dwarf_addr_size ())
    {
    case 4:
      ret = dwarf_readu32 (addr, &u32);
      if (ret < 0)
	return ret;
      *val = u32;
      return ret;

    case 8:
      ret = dwarf_readu64 (addr, &u64);
      if (ret < 0)
	return ret;
      *val = u64;
      return ret;

    default:
      abort ();
    }
}

static inline int
dwarf_read_uleb128 (unw_word_t *addr,
		    unw_word_t *valp)
{
  unw_word_t val = 0, shift = 0;
  unsigned char byte;
  int ret;

  do
    {
      if ((ret = dwarf_readu8 (addr, &byte)) < 0)
	return ret;

      val |= ((unw_word_t) byte & 0x7f) << shift;
      shift += 7;
    }
  while (byte & 0x80);

  *valp = val;
  return 0;
}

static inline int
dwarf_read_sleb128 (unw_word_t *addr, unw_word_t *valp)
{
  unw_word_t val = 0, shift = 0;
  unsigned char byte;
  int ret;

  do
    {
      if ((ret = dwarf_readu8 (addr, &byte)) < 0)
	return ret;

      val |= ((unw_word_t) byte & 0x7f) << shift;
      shift += 7;
    }
  while (byte & 0x80);

  if (shift < 8 * sizeof (unw_word_t) && (byte & 0x40) != 0)
    /* sign-extend negative value */
    val |= ((unw_word_t) -1) << shift;

  *valp = val;
  return 0;
}

HIDDEN int
dwarf_read_encoded_pointer (unw_word_t *addr, unsigned char encoding,
			    const unw_proc_info_t *pi,
			    unw_word_t *valp);

static inline unw_sword_t
sword (unw_word_t val)
{
  switch (dwarf_addr_size ())
    {
    case 1: return (int8_t) val;
    case 2: return (int16_t) val;
    case 4: return (int32_t) val;
    case 8: return (int64_t) val;
    default: abort ();
    }
}

static inline unw_word_t
read_operand (unw_word_t *addr, int operand_type, unw_word_t *val)
{
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int ret;

  if (operand_type == ADDR)
    switch (dwarf_addr_size ())
      {
      case 1: operand_type = VAL8; break;
      case 2: operand_type = VAL16; break;
      case 4: operand_type = VAL32; break;
      case 8: operand_type = VAL64; break;
      default: abort ();
      }

  switch (operand_type)
    {
    case VAL8:
      ret = dwarf_readu8 (addr, &u8);
      if (ret < 0)
	return ret;
      *val = u8;
      break;

    case VAL16:
      ret = dwarf_readu16 (addr, &u16);
      if (ret < 0)
	return ret;
      *val = u16;
      break;

    case VAL32:
      ret = dwarf_readu32 (addr, &u32);
      if (ret < 0)
	return ret;
      *val = u32;
      break;

    case VAL64:
      ret = dwarf_readu64 (addr, &u64);
      if (ret < 0)
	return ret;
      *val = u64;
      break;

    case ULEB128:
      ret = dwarf_read_uleb128 (addr, val);
      break;

    case SLEB128:
      ret = dwarf_read_sleb128 (addr, val);
      break;

    case OFFSET: /* only used by DW_OP_call_ref, which we don't implement */
    default:
      Debug (1, "Unexpected operand type %d\n", operand_type);
      ret = -UNW_EINVAL;
    }
  return ret;
}

HIDDEN int
dwarf_eval_expr (struct dwarf_cursor *c, unw_word_t *addr, unw_word_t len,
		 unw_word_t *valp, int *is_register)
{
  unw_word_t operand1 = 0, operand2 = 0, tmp1, tmp2, tmp3, end_addr;
  uint8_t opcode, operands_signature, u8;
  unw_word_t stack[MAX_EXPR_STACK_SIZE];
  unsigned int tos = 0;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int ret;
# define pop()					\
({						\
  if ((tos - 1) >= MAX_EXPR_STACK_SIZE)		\
    {						\
      Debug (1, "Stack underflow\n");		\
      return -UNW_EINVAL;			\
    }						\
  stack[--tos];					\
})
# define push(x)				\
do {						\
  if (tos >= MAX_EXPR_STACK_SIZE)		\
    {						\
      Debug (1, "Stack overflow\n");		\
      return -UNW_EINVAL;			\
    }						\
  stack[tos++] = (x);				\
} while (0)
# define pick(n)				\
({						\
  unsigned int _index = tos - 1 - (n);		\
  if (_index >= MAX_EXPR_STACK_SIZE)		\
    {						\
      Debug (1, "Out-of-stack pick\n");		\
      return -UNW_EINVAL;			\
    }						\
  stack[_index];				\
})

  end_addr = *addr + len;
  *is_register = 0;

  Debug (14, "len=%lu, pushing cfa=0x%lx\n",
	 (unsigned long) len, (unsigned long) c->cfa);

  push (c->cfa);	/* push current CFA as required by DWARF spec */

  while (*addr < end_addr)
    {
      if ((ret = dwarf_readu8 (addr, &opcode)) < 0)
	return ret;

      operands_signature = operands[opcode];

      if (unlikely (NUM_OPERANDS (operands_signature) > 0))
	{
	  if ((ret = read_operand (addr,
				   OPND1_TYPE (operands_signature),
				   &operand1)) < 0)
	    return ret;
	  if (NUM_OPERANDS (operands_signature) > 1)
	    if ((ret = read_operand (addr,
				     OPND2_TYPE (operands_signature),
				     &operand2)) < 0)
	      return ret;
	}

      switch ((dwarf_expr_op_t) opcode)
	{
	case DW_OP_lit0:  case DW_OP_lit1:  case DW_OP_lit2:
	case DW_OP_lit3:  case DW_OP_lit4:  case DW_OP_lit5:
	case DW_OP_lit6:  case DW_OP_lit7:  case DW_OP_lit8:
	case DW_OP_lit9:  case DW_OP_lit10: case DW_OP_lit11:
	case DW_OP_lit12: case DW_OP_lit13: case DW_OP_lit14:
	case DW_OP_lit15: case DW_OP_lit16: case DW_OP_lit17:
	case DW_OP_lit18: case DW_OP_lit19: case DW_OP_lit20:
	case DW_OP_lit21: case DW_OP_lit22: case DW_OP_lit23:
	case DW_OP_lit24: case DW_OP_lit25: case DW_OP_lit26:
	case DW_OP_lit27: case DW_OP_lit28: case DW_OP_lit29:
	case DW_OP_lit30: case DW_OP_lit31:
	  Debug (15, "OP_lit(%d)\n", (int) opcode - DW_OP_lit0);
	  push (opcode - DW_OP_lit0);
	  break;

	case DW_OP_breg0:  case DW_OP_breg1:  case DW_OP_breg2:
	case DW_OP_breg3:  case DW_OP_breg4:  case DW_OP_breg5:
	case DW_OP_breg6:  case DW_OP_breg7:  case DW_OP_breg8:
	case DW_OP_breg9:  case DW_OP_breg10: case DW_OP_breg11:
	case DW_OP_breg12: case DW_OP_breg13: case DW_OP_breg14:
	case DW_OP_breg15: case DW_OP_breg16: case DW_OP_breg17:
	case DW_OP_breg18: case DW_OP_breg19: case DW_OP_breg20:
	case DW_OP_breg21: case DW_OP_breg22: case DW_OP_breg23:
	case DW_OP_breg24: case DW_OP_breg25: case DW_OP_breg26:
	case DW_OP_breg27: case DW_OP_breg28: case DW_OP_breg29:
	case DW_OP_breg30: case DW_OP_breg31:
	  Debug (15, "OP_breg(r%d,0x%lx)\n",
		 (int) opcode - DW_OP_breg0, (unsigned long) operand1);
	  if ((ret = unw_get_reg (dwarf_to_cursor (c),
				  dwarf_to_unw_regnum (opcode - DW_OP_breg0),
				  &tmp1)) < 0)
	    return ret;
	  push (tmp1 + operand1);
	  break;

	case DW_OP_bregx:
	  Debug (15, "OP_bregx(r%d,0x%lx)\n",
		 (int) operand1, (unsigned long) operand2);
	  if ((ret = unw_get_reg (dwarf_to_cursor (c),
				  dwarf_to_unw_regnum (operand1), &tmp1)) < 0)
	    return ret;
	  push (tmp1 + operand2);
	  break;

	case DW_OP_reg0:  case DW_OP_reg1:  case DW_OP_reg2:
	case DW_OP_reg3:  case DW_OP_reg4:  case DW_OP_reg5:
	case DW_OP_reg6:  case DW_OP_reg7:  case DW_OP_reg8:
	case DW_OP_reg9:  case DW_OP_reg10: case DW_OP_reg11:
	case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14:
	case DW_OP_reg15: case DW_OP_reg16: case DW_OP_reg17:
	case DW_OP_reg18: case DW_OP_reg19: case DW_OP_reg20:
	case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
	case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26:
	case DW_OP_reg27: case DW_OP_reg28: case DW_OP_reg29:
	case DW_OP_reg30: case DW_OP_reg31:
	  Debug (15, "OP_reg(r%d)\n", (int) opcode - DW_OP_reg0);
	  *valp = dwarf_to_unw_regnum (opcode - DW_OP_reg0);
	  *is_register = 1;
	  return 0;

	case DW_OP_regx:
	  Debug (15, "OP_regx(r%d)\n", (int) operand1);
	  *valp = dwarf_to_unw_regnum (operand1);
	  *is_register = 1;
	  return 0;

	case DW_OP_addr:
	case DW_OP_const1u:
	case DW_OP_const2u:
	case DW_OP_const4u:
	case DW_OP_const8u:
	case DW_OP_constu:
	case DW_OP_const8s:
	case DW_OP_consts:
	  Debug (15, "OP_const(0x%lx)\n", (unsigned long) operand1);
	  push (operand1);
	  break;

	case DW_OP_const1s:
	  if (operand1 & 0x80)
	    operand1 |= ((unw_word_t) -1) << 8;
	  Debug (15, "OP_const1s(%ld)\n", (long) operand1);
	  push (operand1);
	  break;

	case DW_OP_const2s:
	  if (operand1 & 0x8000)
	    operand1 |= ((unw_word_t) -1) << 16;
	  Debug (15, "OP_const2s(%ld)\n", (long) operand1);
	  push (operand1);
	  break;

	case DW_OP_const4s:
	  if (operand1 & 0x80000000)
	    operand1 |= (((unw_word_t) -1) << 16) << 16;
	  Debug (15, "OP_const4s(%ld)\n", (long) operand1);
	  push (operand1);
	  break;

	case DW_OP_deref:
	  Debug (15, "OP_deref\n");
	  tmp1 = pop ();
	  if ((ret = dwarf_readw (&tmp1, &tmp2)) < 0)
	    return ret;
	  push (tmp2);
	  break;

	case DW_OP_deref_size:
	  Debug (15, "OP_deref_size(%d)\n", (int) operand1);
	  tmp1 = pop ();
	  switch (operand1)
	    {
	    default:
	      Debug (1, "Unexpected DW_OP_deref_size size %d\n",
		     (int) operand1);
	      return -UNW_EINVAL;

	    case 1:
	      if ((ret = dwarf_readu8 (&tmp1, &u8)) < 0)
		return ret;
	      tmp2 = u8;
	      break;

	    case 2:
	      if ((ret = dwarf_readu16 (&tmp1, &u16)) < 0)
		return ret;
	      tmp2 = u16;
	      break;

	    case 3:
	    case 4:
	      if ((ret = dwarf_readu32 (&tmp1, &u32)) < 0)
		return ret;
	      tmp2 = u32;
	      if (operand1 == 3)
		{
		  if (dwarf_is_big_endian ())
		    tmp2 >>= 8;
		  else
		    tmp2 &= 0xffffff;
		}
	      break;
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	      if ((ret = dwarf_readu64 (&tmp1, &u64)) < 0)
		return ret;
	      tmp2 = u64;
	      if (operand1 != 8)
		{
		  if (dwarf_is_big_endian ())
		    tmp2 >>= 64 - 8 * operand1;
		  else
		    tmp2 &= (~ (unw_word_t) 0) << (8 * operand1);
		}
	      break;
	    }
	  push (tmp2);
	  break;

	case DW_OP_dup:
	  Debug (15, "OP_dup\n");
	  push (pick (0));
	  break;

	case DW_OP_drop:
	  Debug (15, "OP_drop\n");
	  (void) pop ();
	  break;

	case DW_OP_pick:
	  Debug (15, "OP_pick(%d)\n", (int) operand1);
	  push (pick (operand1));
	  break;

	case DW_OP_over:
	  Debug (15, "OP_over\n");
	  push (pick (1));
	  break;

	case DW_OP_swap:
	  Debug (15, "OP_swap\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp1);
	  push (tmp2);
	  break;

	case DW_OP_rot:
	  Debug (15, "OP_rot\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  tmp3 = pop ();
	  push (tmp1);
	  push (tmp3);
	  push (tmp2);
	  break;

	case DW_OP_abs:
	  Debug (15, "OP_abs\n");
	  tmp1 = pop ();
	  if (tmp1 & ((unw_word_t) 1 << (8 * dwarf_addr_size () - 1)))
	    tmp1 = -tmp1;
	  push (tmp1);
	  break;

	case DW_OP_and:
	  Debug (15, "OP_and\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp1 & tmp2);
	  break;

	case DW_OP_div:
	  Debug (15, "OP_div\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  if (tmp1)
	    tmp1 = sword (tmp2) / sword (tmp1);
	  push (tmp1);
	  break;

	case DW_OP_minus:
	  Debug (15, "OP_minus\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  tmp1 = tmp2 - tmp1;
	  push (tmp1);
	  break;

	case DW_OP_mod:
	  Debug (15, "OP_mod\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  if (tmp1)
	    tmp1 = tmp2 % tmp1;
	  push (tmp1);
	  break;

	case DW_OP_mul:
	  Debug (15, "OP_mul\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  if (tmp1)
	    tmp1 = tmp2 * tmp1;
	  push (tmp1);
	  break;

	case DW_OP_neg:
	  Debug (15, "OP_neg\n");
	  push (-pop ());
	  break;

	case DW_OP_not:
	  Debug (15, "OP_not\n");
	  push (~pop ());
	  break;

	case DW_OP_or:
	  Debug (15, "OP_or\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp1 | tmp2);
	  break;

	case DW_OP_plus:
	  Debug (15, "OP_plus\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp1 + tmp2);
	  break;

	case DW_OP_plus_uconst:
	  Debug (15, "OP_plus_uconst(%lu)\n", (unsigned long) operand1);
	  tmp1 = pop ();
	  push (tmp1 + operand1);
	  break;

	case DW_OP_shl:
	  Debug (15, "OP_shl\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp2 << tmp1);
	  break;

	case DW_OP_shr:
	  Debug (15, "OP_shr\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp2 >> tmp1);
	  break;

	case DW_OP_shra:
	  Debug (15, "OP_shra\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) >> tmp1);
	  break;

	case DW_OP_xor:
	  Debug (15, "OP_xor\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (tmp1 ^ tmp2);
	  break;

	case DW_OP_le:
	  Debug (15, "OP_le\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) <= sword (tmp1));
	  break;

	case DW_OP_ge:
	  Debug (15, "OP_ge\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) >= sword (tmp1));
	  break;

	case DW_OP_eq:
	  Debug (15, "OP_eq\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) == sword (tmp1));
	  break;

	case DW_OP_lt:
	  Debug (15, "OP_lt\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) < sword (tmp1));
	  break;

	case DW_OP_gt:
	  Debug (15, "OP_gt\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) > sword (tmp1));
	  break;

	case DW_OP_ne:
	  Debug (15, "OP_ne\n");
	  tmp1 = pop ();
	  tmp2 = pop ();
	  push (sword (tmp2) != sword (tmp1));
	  break;

	case DW_OP_skip:
	  Debug (15, "OP_skip(%d)\n", (int16_t) operand1);
	  *addr += (int16_t) operand1;
	  break;

	case DW_OP_bra:
	  Debug (15, "OP_skip(%d)\n", (int16_t) operand1);
	  tmp1 = pop ();
	  if (tmp1)
	    *addr += (int16_t) operand1;
	  break;

	case DW_OP_nop:
	  Debug (15, "OP_nop\n");
	  break;

	case DW_OP_call2:
	case DW_OP_call4:
	case DW_OP_call_ref:
	case DW_OP_fbreg:
	case DW_OP_piece:
	case DW_OP_push_object_address:
	case DW_OP_xderef:
	case DW_OP_xderef_size:
	default:
	  Debug (1, "Unexpected opcode 0x%x\n", opcode);
	  return -UNW_EINVAL;
	}
    }
  *valp = pop ();
  Debug (14, "final value = 0x%lx\n", (unsigned long) *valp);
  return 0;
}

static inline int
eval_location_expr (struct dwarf_cursor *c, unw_word_t addr,
		    dwarf_loc_t *locp)
{
  int ret, is_register;
  unw_word_t len, val;

  /* read the length of the expression: */
  if ((ret = dwarf_read_uleb128 (&addr, &len)) < 0)
    return ret;

  /* evaluate the expression: */
  if ((ret = dwarf_eval_expr (c, &addr, len, &val, &is_register)) < 0)
    return ret;

  if (is_register)
    *locp = DWARF_REG_LOC (c, dwarf_to_unw_regnum (val));
  else
    *locp = DWARF_MEM_LOC (c, val);

  return 0;
}

static inline int
parse_cie (unw_word_t addr,
	   const unw_proc_info_t *pi, struct dwarf_cie_info *dci,
	   unw_word_t base)
{
  uint8_t version, ch, augstr[5], fde_encoding, handler_encoding;
  unw_word_t len, cie_end_addr, aug_size;
  uint32_t u32val;
  uint64_t u64val;
  size_t i;
  int ret;
# define STR2(x)	#x
# define STR(x)		STR2(x)

  /* Pick appropriate default for FDE-encoding.  DWARF spec says
     start-IP (initial_location) and the code-size (address_range) are
     "address-unit sized constants".  The `R' augmentation can be used
     to override this, but by default, we pick an address-sized unit
     for fde_encoding.  */
  switch (dwarf_addr_size ())
    {
    case 4:	fde_encoding = DW_EH_PE_udata4; break;
    case 8:	fde_encoding = DW_EH_PE_udata8; break;
    default:	fde_encoding = DW_EH_PE_omit; break;
    }

  dci->lsda_encoding = DW_EH_PE_omit;
  dci->handler = 0;

  if ((ret = dwarf_readu32 (&addr, &u32val)) < 0)
    return ret;

  if (u32val != 0xffffffff)
    {
      /* the CIE is in the 32-bit DWARF format */
      uint32_t cie_id;
      /* DWARF says CIE id should be 0xffffffff, but in .eh_frame, it's 0 */
      const uint32_t expected_id = (base) ? 0xffffffff : 0;

      len = u32val;
      cie_end_addr = addr + len;
      if ((ret = dwarf_readu32 (&addr, &cie_id)) < 0)
	return ret;
      if (cie_id != expected_id)
	{
	  Debug (1, "Unexpected CIE id %x\n", cie_id);
	  return -UNW_EINVAL;
	}
    }
  else
    {
      /* the CIE is in the 64-bit DWARF format */
      uint64_t cie_id;
      /* DWARF says CIE id should be 0xffffffffffffffff, but in
	 .eh_frame, it's 0 */
      const uint64_t expected_id = (base) ? 0xffffffffffffffffull : 0;

      if ((ret = dwarf_readu64 (&addr, &u64val)) < 0)
	return ret;
      len = u64val;
      cie_end_addr = addr + len;
      if ((ret = dwarf_readu64 (&addr, &cie_id)) < 0)
	return ret;
      if (cie_id != expected_id)
	{
	  Debug (1, "Unexpected CIE id %llx\n", (long long) cie_id);
	  return -UNW_EINVAL;
	}
    }
  dci->cie_instr_end = cie_end_addr;

  if ((ret = dwarf_readu8 (&addr, &version)) < 0)
    return ret;

  if (version != 1 && version != 3 && version != 4)
    {
      Debug (1, "Got CIE version %u, expected version 1, 3 or 4\n", version);
      return -UNW_EBADVERSION;
    }

  /* read and parse the augmentation string: */
  memset (augstr, 0, sizeof (augstr));
  for (i = 0;;)
    {
      if ((ret = dwarf_readu8 (&addr, &ch)) < 0)
	return ret;

      if (!ch)
	break;	/* end of augmentation string */

      if (i < sizeof (augstr) - 1)
	augstr[i++] = ch;
    }

  if (version == 4) {
    uint8_t address_size;
    if ((ret = dwarf_readu8(&addr, &address_size)) < 0) {
      return ret;
    }
    if (address_size != sizeof(unw_word_t)) {
      return -UNW_EBADVERSION;
    }
    uint8_t segment_size;
    if ((ret = dwarf_readu8(&addr, &segment_size)) < 0) {
      return ret;
    }
    // We don't support non-zero segment size.
    if (segment_size != 0) {
      return -UNW_EBADVERSION;
    }
  }
  if ((ret = dwarf_read_uleb128 (&addr, &dci->code_align)) < 0
      || (ret = dwarf_read_sleb128 (&addr, &dci->data_align)) < 0)
    return ret;

  /* Read the return-address column either as a u8 or as a uleb128.  */
  if (version == 1)
    {
      if ((ret = dwarf_readu8 (&addr, &ch)) < 0)
	return ret;
      dci->ret_addr_column = ch;
    }
  else if ((ret = dwarf_read_uleb128 (&addr, &dci->ret_addr_column)) < 0)
    return ret;

  i = 0;
  if (augstr[0] == 'z')
    {
      dci->sized_augmentation = 1;
      if ((ret = dwarf_read_uleb128 (&addr, &aug_size)) < 0)
	return ret;
      i++;
    }

  for (; i < sizeof (augstr) && augstr[i]; ++i)
    switch (augstr[i])
      {
      case 'L':
	/* read the LSDA pointer-encoding format.  */
	if ((ret = dwarf_readu8 (&addr, &ch)) < 0)
	  return ret;
	dci->lsda_encoding = ch;
	break;

      case 'R':
	/* read the FDE pointer-encoding format.  */
	if ((ret = dwarf_readu8 (&addr, &fde_encoding)) < 0)
	  return ret;
	break;

      case 'P':
	/* read the personality-routine pointer-encoding format.  */
	if ((ret = dwarf_readu8 (&addr, &handler_encoding)) < 0)
	  return ret;
	if ((ret = dwarf_read_encoded_pointer (&addr, handler_encoding,
					       pi, &dci->handler)) < 0)
	  return ret;
	break;

      case 'S':
	/* This is a signal frame. */
	dci->signal_frame = 1;

	/* Temporarily set it to one so dwarf_parse_fde() knows that
	   it should fetch the actual ABI/TAG pair from the FDE.  */
	dci->have_abi_marker = 1;
	break;

      default:
	Debug (1, "Unexpected augmentation string `%s'\n", augstr);
	if (dci->sized_augmentation)
	  /* If we have the size of the augmentation body, we can skip
	     over the parts that we don't understand, so we're OK. */
	  goto done;
	else
	  return -UNW_EINVAL;
      }
 done:
  dci->fde_encoding = fde_encoding;
  dci->cie_instr_start = addr;
  Debug (15, "CIE parsed OK, augmentation = \"%s\", handler=0x%lx\n",
	 augstr, (long) dci->handler);
  return 0;
}

static inline int
is_cie_id (unw_word_t val, int is_debug_frame)
{
  /* The CIE ID is normally 0xffffffff (for 32-bit ELF) or
     0xffffffffffffffff (for 64-bit ELF).  However, .eh_frame
     uses 0.  */
  if (is_debug_frame)
    /* ANDROID support update. */
    return (val == (uint32_t) -1 || val == (unw_word_t) (uint64_t) -1);
    /* End of ANDROID update. */
  else
    return (val == 0);
}

HIDDEN int
dwarf_extract_proc_info_from_fde (unw_word_t *addrp, unw_proc_info_t *pi,
				  int need_unwind_info, unw_word_t base)
{
  unw_word_t fde_end_addr, cie_addr, cie_offset_addr, aug_end_addr = 0;
  unw_word_t start_ip, ip_range, aug_size, addr = *addrp;
  int ret, ip_range_encoding;
  struct dwarf_cie_info dci;
  uint64_t u64val;
  uint32_t u32val;

  Debug (12, "FDE @ 0x%lx\n", (long) addr);

  memset (&dci, 0, sizeof (dci));

  if ((ret = dwarf_readu32 (&addr, &u32val)) < 0)
    return ret;

  if (u32val != 0xffffffff)
    {
      int32_t cie_offset;

      /* In some configurations, an FDE with a 0 length indicates the
	 end of the FDE-table.  */
      if (u32val == 0)
	return -UNW_ENOINFO;

      /* the FDE is in the 32-bit DWARF format */

      *addrp = fde_end_addr = addr + u32val;
      cie_offset_addr = addr;

      if ((ret = dwarf_reads32 (&addr, &cie_offset)) < 0)
	return ret;

      if (is_cie_id (cie_offset, base != 0))
	/* ignore CIEs (happens during linear searches) */
	return 0;

      if (base != 0)
        cie_addr = base + cie_offset;
      else
	/* DWARF says that the CIE_pointer in the FDE is a
	   .debug_frame-relative offset, but the GCC-generated .eh_frame
	   sections instead store a "pcrelative" offset, which is just
	   as fine as it's self-contained.  */
	cie_addr = cie_offset_addr - cie_offset;
    }
  else
    {
      int64_t cie_offset;

      /* the FDE is in the 64-bit DWARF format */

      if ((ret = dwarf_readu64 (&addr, &u64val)) < 0)
	return ret;

      *addrp = fde_end_addr = addr + u64val;
      cie_offset_addr = addr;

      if ((ret = dwarf_reads64 (&addr, &cie_offset)) < 0)
	return ret;

      if (is_cie_id (cie_offset, base != 0))
	/* ignore CIEs (happens during linear searches) */
	return 0;

      if (base != 0)
	cie_addr = base + cie_offset;
      else
	/* DWARF says that the CIE_pointer in the FDE is a
	   .debug_frame-relative offset, but the GCC-generated .eh_frame
	   sections instead store a "pcrelative" offset, which is just
	   as fine as it's self-contained.  */
	cie_addr = (unw_word_t) ((uint64_t) cie_offset_addr - cie_offset);
    }

  Debug (15, "looking for CIE at address %lx\n", (long) cie_addr);

  if ((ret = parse_cie (cie_addr, pi, &dci, base)) < 0)
    return ret;

  /* IP-range has same encoding as FDE pointers, except that it's
     always an absolute value: */
  ip_range_encoding = dci.fde_encoding & DW_EH_PE_FORMAT_MASK;

  if ((ret = dwarf_read_encoded_pointer (&addr, dci.fde_encoding,
					 pi, &start_ip)) < 0
      || (ret = dwarf_read_encoded_pointer (&addr, ip_range_encoding,
					    pi, &ip_range)) < 0)
    return ret;
  pi->start_ip = start_ip;
  pi->end_ip = start_ip + ip_range;
  pi->handler = dci.handler;

  if (dci.sized_augmentation)
    {
      if ((ret = dwarf_read_uleb128 (&addr, &aug_size)) < 0)
	return ret;
      aug_end_addr = addr + aug_size;
    }

  if ((ret = dwarf_read_encoded_pointer (&addr, dci.lsda_encoding,
					 pi, &pi->lsda)) < 0)
    return ret;

  Debug (15, "FDE covers IP 0x%lx-0x%lx, LSDA=0x%lx\n",
	 (long) pi->start_ip, (long) pi->end_ip, (long) pi->lsda);

  if (need_unwind_info)
    {
      pi->format = UNW_INFO_FORMAT_TABLE;
      pi->unwind_info_size = sizeof (dci);
      // pi->unwind_info = mempool_alloc (&dwarf_cie_info_pool);
      pi->unwind_info = malloc(sizeof(dci));
      if (!pi->unwind_info)
	return -UNW_ENOMEM;

      if (dci.have_abi_marker)
	{
	  if ((ret = dwarf_readu16 (&addr, &dci.abi)) < 0
	      || (ret = dwarf_readu16 (&addr, &dci.tag)) < 0)
	    return ret;
	  Debug (13, "Found ABI marker = (abi=%u, tag=%u)\n",
		 dci.abi, dci.tag);
	}

      if (dci.sized_augmentation)
	dci.fde_instr_start = aug_end_addr;
      else
	dci.fde_instr_start = addr;
      dci.fde_instr_end = fde_end_addr;

      memcpy (pi->unwind_info, &dci, sizeof (dci));
    }
  return 0;
}

static inline const struct table_entry *
lookup (const struct table_entry *table, size_t table_size, int32_t rel_ip)
{
  unsigned long table_len = table_size / sizeof (struct table_entry);
  const struct table_entry *e = NULL;
  unsigned long lo, hi, mid;

  /* do a binary search for right entry: */
  for (lo = 0, hi = table_len; lo < hi;)
    {
      mid = (lo + hi) / 2;
      e = table + mid;
      Debug (15, "e->start_ip_offset = %lx\n", (long) e->start_ip_offset);
      if (rel_ip < e->start_ip_offset)
	hi = mid;
      else
	lo = mid + 1;
    }
  if (hi <= 0)
	return NULL;
  e = table + hi - 1;
  return e;
}

PROTECTED int
dwarf_search_unwind_table (unw_word_t ip,
			   unw_dyn_info_t *di, unw_proc_info_t *pi,
			   int need_unwind_info)
{
  const struct table_entry *e = NULL, *table;
  unw_word_t segbase = 0, fde_addr;
#ifndef UNW_LOCAL_ONLY
  struct table_entry ent;
#endif
  int ret;
  unw_word_t debug_frame_base;
  size_t table_len;

#ifdef UNW_REMOTE_ONLY
  assert (di->format == UNW_INFO_FORMAT_REMOTE_TABLE);
#else
  assert (di->format == UNW_INFO_FORMAT_REMOTE_TABLE
	  || di->format == UNW_INFO_FORMAT_TABLE);
#endif
  assert (ip >= di->start_ip && ip < di->end_ip);

  if (di->format == UNW_INFO_FORMAT_REMOTE_TABLE)
    {
      table = (const struct table_entry *) (uintptr_t) di->u.rti.table_data;
      table_len = di->u.rti.table_len * sizeof (unw_word_t);
      debug_frame_base = 0;
    }
  else
    {
#ifndef UNW_REMOTE_ONLY
      struct unw_debug_frame_list *fdesc = (void *) di->u.ti.table_data;

      /* UNW_INFO_FORMAT_TABLE (i.e. .debug_frame) is read from local address
         space.  Both the index and the unwind tables live in local memory, but
         the address space to check for properties like the address size and
         endianness is the target one.  */
      table = fdesc->index;
      table_len = fdesc->index_size * sizeof (struct table_entry);
      debug_frame_base = (uintptr_t) fdesc->debug_frame;
#endif
    }


#ifndef UNW_REMOTE_ONLY
    {
      segbase = di->u.rti.segbase;
      e = lookup (table, table_len, ip - segbase);
    }
#endif
  if (!e)
    {
      Debug (1, "IP %lx inside range %lx-%lx, but no explicit unwind info found\n",
	     (long) ip, (long) di->start_ip, (long) di->end_ip);
      /* IP is inside this table's range, but there is no explicit
	 unwind info.  */
      return -UNW_ENOINFO;
    }
  Debug (15, "ip=0x%lx, start_ip=0x%lx\n",
	 (long) ip, (long) (e->start_ip_offset));
  if (debug_frame_base)
    fde_addr = e->fde_offset + debug_frame_base;
  else
    fde_addr = e->fde_offset + segbase;
  Debug (1, "e->fde_offset = %lx, segbase = %lx, debug_frame_base = %lx, "
	    "fde_addr = %lx\n", (long) e->fde_offset, (long) segbase,
	    (long) debug_frame_base, (long) fde_addr);
  if ((ret = dwarf_extract_proc_info_from_fde (&fde_addr, pi,
					       need_unwind_info,
					       debug_frame_base)) < 0)
    return ret;

  /* .debug_frame uses an absolute encoding that does not know about any
     shared library relocation.  */
  if (di->format == UNW_INFO_FORMAT_TABLE)
    {
      pi->start_ip += segbase;
      pi->end_ip += segbase;
      pi->flags = UNW_PI_FLAG_DEBUG_FRAME;
    }

  if (ip < pi->start_ip || ip >= pi->end_ip)
    {
      /* ANDROID support update. */
      if (need_unwind_info && pi->unwind_info && pi->format == UNW_INFO_FORMAT_TABLE)
        {
          /* Free the memory used if the call fails. Otherwise, when there
           * is a mix of dwarf and other unwind data, the memory allocated
           * will be leaked.
           */
          // mempool_free (&dwarf_cie_info_pool, pi->unwind_info);
          free(pi->unwind_info);
          pi->unwind_info = NULL;
        }
      /* End of ANDROID support update. */
      return -UNW_ENOINFO;
    }

  return 0;
}

static ALWAYS_INLINE int
dwarf_read_encoded_pointer_inlined (unw_word_t *addr, unsigned char encoding,
				    const unw_proc_info_t *pi,
				    unw_word_t *valp)
{
  unw_word_t val, initial_addr = *addr;
  uint16_t uval16;
  uint32_t uval32;
  uint64_t uval64;
  int16_t sval16;
  int32_t sval32;
  int64_t sval64;
  int ret;

  /* DW_EH_PE_omit and DW_EH_PE_aligned don't follow the normal
     format/application encoding.  Handle them first.  */
  if (encoding == DW_EH_PE_omit)
    {
      *valp = 0;
      return 0;
    }
  else if (encoding == DW_EH_PE_aligned)
    {
      int size = dwarf_addr_size ();
      *addr = (initial_addr + size - 1) & -size;
      return dwarf_readw (addr, valp);
    }

  switch (encoding & DW_EH_PE_FORMAT_MASK)
    {
    case DW_EH_PE_ptr:
      if ((ret = dwarf_readw (addr, &val) < 0))
	return ret;
      break;

    case DW_EH_PE_uleb128:
      if ((ret = dwarf_read_uleb128 (addr, &val)) < 0)
	return ret;
      break;

    case DW_EH_PE_udata2:
      if ((ret = dwarf_readu16 (addr, &uval16)) < 0)
	return ret;
      val = uval16;
      break;

    case DW_EH_PE_udata4:
      if ((ret = dwarf_readu32 (addr, &uval32)) < 0)
	return ret;
      val = uval32;
      break;

    case DW_EH_PE_udata8:
      if ((ret = dwarf_readu64 (addr, &uval64)) < 0)
	return ret;
      val = uval64;
      break;

    case DW_EH_PE_sleb128:
      if ((ret = dwarf_read_uleb128 (addr, &val)) < 0)
	return ret;
      break;

    case DW_EH_PE_sdata2:
      if ((ret = dwarf_reads16 (addr, &sval16)) < 0)
	return ret;
      val = sval16;
      break;

    case DW_EH_PE_sdata4:
      if ((ret = dwarf_reads32 (addr, &sval32)) < 0)
	return ret;
      val = sval32;
      break;

    case DW_EH_PE_sdata8:
      if ((ret = dwarf_reads64 (addr, &sval64)) < 0)
	return ret;
      val = sval64;
      break;

    default:
      Debug (1, "unexpected encoding format 0x%x\n",
	     encoding & DW_EH_PE_FORMAT_MASK);
      return -UNW_EINVAL;
    }

  if (val == 0)
    {
      /* 0 is a special value and always absolute.  */
      *valp = 0;
      return 0;
    }

  switch (encoding & DW_EH_PE_APPL_MASK)
    {
    case DW_EH_PE_absptr:
      break;

    case DW_EH_PE_pcrel:
      val += initial_addr;
      break;

    case DW_EH_PE_datarel:
      /* XXX For now, assume that data-relative addresses are relative
         to the global pointer.  */
      val += pi->gp;
      break;

    case DW_EH_PE_funcrel:
      val += pi->start_ip;
      break;

    case DW_EH_PE_textrel:
      /* XXX For now we don't support text-rel values.  If there is a
         platform which needs this, we probably would have to add a
         "segbase" member to unw_proc_info_t.  */
    default:
      Debug (1, "unexpected application type 0x%x\n",
	     encoding & DW_EH_PE_APPL_MASK);
      return -UNW_EINVAL;
    }

  /* Trim off any extra bits.  Assume that sign extension isn't
     required; the only place it is needed is MIPS kernel space
     addresses.  */
  if (sizeof (val) > dwarf_addr_size ())
    {
      assert (dwarf_addr_size () == 4);
      val = (uint32_t) val;
    }

  if (encoding & DW_EH_PE_indirect)
    {
      unw_word_t indirect_addr = val;

      if ((ret = dwarf_readw (&indirect_addr, &val)) < 0)
	return ret;
    }

  *valp = val;
  return 0;
}

HIDDEN int
dwarf_read_encoded_pointer (unw_word_t *addr, unsigned char encoding,
			    const unw_proc_info_t *pi,
			    unw_word_t *valp)
{
  return dwarf_read_encoded_pointer_inlined (addr, encoding,
					     pi, valp);
}

static int
linear_search (unw_word_t ip,
	       unw_word_t eh_frame_start, unw_word_t eh_frame_end,
	       unw_word_t fde_count,
	       unw_proc_info_t *pi, int need_unwind_info)
{
  unw_word_t i = 0, fde_addr, addr = eh_frame_start;
  int ret;

  while (i++ < fde_count && addr < eh_frame_end)
    {
      fde_addr = addr;
      if ((ret = dwarf_extract_proc_info_from_fde (&addr, pi, 0, 0))
	  < 0)
	return ret;

      if (ip >= pi->start_ip && ip < pi->end_ip)
	{
	  if (!need_unwind_info)
	    return 1;
	  addr = fde_addr;
	  if ((ret = dwarf_extract_proc_info_from_fde (&addr, pi,
						       need_unwind_info, 0))
	      < 0)
	    return ret;
	  return 1;
	}
    }
  return -UNW_ENOINFO;
}

HIDDEN int
dwarf_callback (struct dl_phdr_info *info, size_t size, void *ptr)
{
  struct dwarf_callback_data *cb_data = ptr;
  unw_dyn_info_t *di = &cb_data->di;
  const Elf_W(Phdr) *phdr, *p_eh_hdr, *p_dynamic, *p_text;
  unw_word_t addr, eh_frame_start, eh_frame_end, fde_count, ip;
  Elf_W(Addr) load_base, max_load_addr = 0;
  int ret, need_unwind_info = cb_data->need_unwind_info;
  unw_proc_info_t *pi = cb_data->pi;
  struct dwarf_eh_frame_hdr *hdr;
  long n;
  int found = 0;
#ifdef CONFIG_DEBUG_FRAME
  unw_word_t start, end;
#endif /* CONFIG_DEBUG_FRAME*/

  ip = cb_data->ip;

  /* Make sure struct dl_phdr_info is at least as big as we need.  */
  if (size < offsetof (struct dl_phdr_info, dlpi_phnum)
	     + sizeof (info->dlpi_phnum))
    return -1;

  Debug (15, "checking %s, base=0x%lx)\n",
	 info->dlpi_name, (long) info->dlpi_addr);

  phdr = info->dlpi_phdr;
  load_base = info->dlpi_addr;
  p_text = NULL;
  p_eh_hdr = NULL;
  p_dynamic = NULL;

  /* See if PC falls into one of the loaded segments.  Find the
     eh-header segment at the same time.  */
  for (n = info->dlpi_phnum; --n >= 0; phdr++)
    {
      if (phdr->p_type == PT_LOAD)
	{
	  Elf_W(Addr) vaddr = phdr->p_vaddr + load_base;

	  if (ip >= vaddr && ip < vaddr + phdr->p_memsz)
	    p_text = phdr;

	  if (vaddr + phdr->p_filesz > max_load_addr)
	    max_load_addr = vaddr + phdr->p_filesz;
	}
      else if (phdr->p_type == PT_GNU_EH_FRAME)
	p_eh_hdr = phdr;
      else if (phdr->p_type == PT_DYNAMIC)
	p_dynamic = phdr;
    }

  if (!p_text)
    return 0;

  if (p_eh_hdr)
    {
      if (p_dynamic)
	{
	  /* For dynamicly linked executables and shared libraries,
	     DT_PLTGOT is the value that data-relative addresses are
	     relative to for that object.  We call this the "gp".  */
	  Elf_W(Dyn) *dyn = (Elf_W(Dyn) *)(p_dynamic->p_vaddr + load_base);
	  for (; dyn->d_tag != DT_NULL; ++dyn)
	    if (dyn->d_tag == DT_PLTGOT)
	      {
		/* Assume that _DYNAMIC is writable and GLIBC has
		   relocated it (true for x86 at least).  */
		di->gp = dyn->d_un.d_ptr;
		break;
	      }
	}
      else
	/* Otherwise this is a static executable with no _DYNAMIC.  Assume
	   that data-relative addresses are relative to 0, i.e.,
	   absolute.  */
	di->gp = 0;
      pi->gp = di->gp;

      hdr = (struct dwarf_eh_frame_hdr *) (p_eh_hdr->p_vaddr + load_base);
      if (hdr->version != DW_EH_VERSION)
	{
	  Debug (1, "table `%s' has unexpected version %d\n",
		 info->dlpi_name, hdr->version);
	  return 0;
	}

      addr = (unw_word_t) (uintptr_t) (hdr + 1);

      /* (Optionally) read eh_frame_ptr: */
      if ((ret = dwarf_read_encoded_pointer (&addr, hdr->eh_frame_ptr_enc, pi,
					     &eh_frame_start)) < 0)
	return ret;

      /* (Optionally) read fde_count: */
      if ((ret = dwarf_read_encoded_pointer (&addr, hdr->fde_count_enc, pi,
					     &fde_count)) < 0)
	return ret;

      if (hdr->table_enc != (DW_EH_PE_datarel | DW_EH_PE_sdata4))
	{
	  /* If there is no search table or it has an unsupported
	     encoding, fall back on linear search.  */
	  if (hdr->table_enc == DW_EH_PE_omit)
            /* ANDROID support update. */
	    {
            /* End of ANDROID update. */
	      Debug (4, "table `%s' lacks search table; doing linear search\n",
		     info->dlpi_name);
            /* ANDROID support update. */
	    }
            /* End of ANDROID update. */
	  else
            /* ANDROID support update. */
	    {
            /* End of ANDROID update. */
	      Debug (4, "table `%s' has encoding 0x%x; doing linear search\n",
		     info->dlpi_name, hdr->table_enc);
            /* ANDROID support update. */
	    }
            /* End of ANDROID update. */

	  eh_frame_end = max_load_addr;	/* XXX can we do better? */

	  if (hdr->fde_count_enc == DW_EH_PE_omit)
	    fde_count = ~0UL;
	  if (hdr->eh_frame_ptr_enc == DW_EH_PE_omit)
	    abort ();

	  /* XXX we know how to build a local binary search table for
	     .debug_frame, so we could do that here too.  */
	  cb_data->single_fde = 1;
	  found = linear_search (ip,
				 eh_frame_start, eh_frame_end, fde_count,
				 pi, need_unwind_info);
	  if (found != 1)
	    found = 0;
	}
      else
	{
	  di->format = UNW_INFO_FORMAT_REMOTE_TABLE;
	  di->start_ip = p_text->p_vaddr + load_base;
	  di->end_ip = p_text->p_vaddr + load_base + p_text->p_memsz;
	  di->u.rti.name_ptr = (unw_word_t) (uintptr_t) info->dlpi_name;
	  di->u.rti.table_data = addr;
	  assert (sizeof (struct table_entry) % sizeof (unw_word_t) == 0);
	  di->u.rti.table_len = (fde_count * sizeof (struct table_entry)
				 / sizeof (unw_word_t));
	  /* For the binary-search table in the eh_frame_hdr, data-relative
	     means relative to the start of that section... */
	  di->u.rti.segbase = (unw_word_t) (uintptr_t) hdr;

	  found = 1;
	  Debug (15, "found table `%s': segbase=0x%lx, len=%lu, gp=0x%lx, "
		 "table_data=0x%lx\n", (char *) (uintptr_t) di->u.rti.name_ptr,
		 (long) di->u.rti.segbase, (long) di->u.rti.table_len,
		 (long) di->gp, (long) di->u.rti.table_data);
	}
    }

#ifdef CONFIG_DEBUG_FRAME
  /* Find the start/end of the described region by parsing the phdr_info
     structure.  */
  start = (unw_word_t) -1;
  end = 0;

  for (n = 0; n < info->dlpi_phnum; n++)
    {
      if (info->dlpi_phdr[n].p_type == PT_LOAD)
        {
	  unw_word_t seg_start = info->dlpi_addr + info->dlpi_phdr[n].p_vaddr;
          unw_word_t seg_end = seg_start + info->dlpi_phdr[n].p_memsz;

	  if (seg_start < start)
	    start = seg_start;

	  if (seg_end > end)
	    end = seg_end;
	}
    }

  found = dwarf_find_debug_frame (found, &cb_data->di_debug, ip,
				  info->dlpi_addr, info->dlpi_name, start,
				  end);
#endif  /* CONFIG_DEBUG_FRAME */

  return found;
}

HIDDEN int
dwarf_find_proc_info (unw_word_t ip,
		      unw_proc_info_t *pi, int need_unwind_info)
{
  struct dwarf_callback_data cb_data;
  int ret;

  Debug (14, "looking for IP=0x%lx\n", (long) ip);

  memset (&cb_data, 0, sizeof (cb_data));
  cb_data.ip = ip;
  cb_data.pi = pi;
  cb_data.need_unwind_info = need_unwind_info;
  cb_data.di.format = -1;
  cb_data.di_debug.format = -1;

  SIGPROCMASK (SIG_SETMASK, &unwi_full_mask, &saved_mask);
  ret = dl_iterate_phdr (dwarf_callback, &cb_data);
  SIGPROCMASK (SIG_SETMASK, &saved_mask, NULL);

  if (ret <= 0)
    {
      Debug (14, "IP=0x%lx not found\n", (long) ip);
      return -UNW_ENOINFO;
    }

  if (cb_data.single_fde)
    /* already got the result in *pi */
    return 0;

  /* search the table: */
  if (cb_data.di.format != -1)
    ret = dwarf_search_unwind_table (ip, &cb_data.di,
				      pi, need_unwind_info);
  else
    ret = -UNW_ENOINFO;

  if (ret == -UNW_ENOINFO && cb_data.di_debug.format != -1)
    ret = dwarf_search_unwind_table (ip, &cb_data.di_debug, pi,
				     need_unwind_info);
  return ret;
}

static inline int
read_regnum (unw_word_t *addr,
	     unw_word_t *valp)
{
  int ret;

  if ((ret = dwarf_read_uleb128 (addr, valp)) < 0)
    return ret;

  if (*valp >= DWARF_NUM_PRESERVED_REGS)
    {
      Debug (1, "Invalid register number %u\n", (unsigned int) *valp);
      return -UNW_EBADREG;
    }
  return 0;
}

static inline void
set_reg (dwarf_state_record_t *sr, unw_word_t regnum, dwarf_where_t where,
	 unw_word_t val)
{
  sr->rs_current.reg[regnum].where = where;
  sr->rs_current.reg[regnum].val = val;
}

/* Run a CFI program to update the register state.  */
static int
run_cfi_program (struct dwarf_cursor *c, dwarf_state_record_t *sr,
		 unw_word_t ip, unw_word_t *addr, unw_word_t end_addr,
		 struct dwarf_cie_info *dci)
{
  unw_word_t curr_ip, operand = 0, regnum, val, len, fde_encoding;
  dwarf_reg_state_t *rs_stack = NULL, *new_rs, *old_rs;
  uint8_t u8, op;
  uint16_t u16;
  uint32_t u32;
  int ret;

  curr_ip = c->pi.start_ip;

  /* Process everything up to and including the current 'ip',
     including all the DW_CFA_advance_loc instructions.  See
     'c->use_prev_instr' use in 'fetch_proc_info' for details. */
  while (curr_ip <= ip && *addr < end_addr)
    {
      if ((ret = dwarf_readu8 (addr, &op)) < 0)
	return ret;

      if (op & DWARF_CFA_OPCODE_MASK)
	{
	  operand = op & DWARF_CFA_OPERAND_MASK;
	  op &= ~DWARF_CFA_OPERAND_MASK;
	}
      switch ((dwarf_cfa_t) op)
	{
	case DW_CFA_advance_loc:
	  curr_ip += operand * dci->code_align;
	  Debug (15, "CFA_advance_loc to 0x%lx\n", (long) curr_ip);
	  break;

	case DW_CFA_advance_loc1:
	  if ((ret = dwarf_readu8 (addr, &u8)) < 0)
	    goto fail;
	  curr_ip += u8 * dci->code_align;
	  Debug (15, "CFA_advance_loc1 to 0x%lx\n", (long) curr_ip);
	  break;

	case DW_CFA_advance_loc2:
	  if ((ret = dwarf_readu16 (addr, &u16)) < 0)
	    goto fail;
	  curr_ip += u16 * dci->code_align;
	  Debug (15, "CFA_advance_loc2 to 0x%lx\n", (long) curr_ip);
	  break;

	case DW_CFA_advance_loc4:
	  if ((ret = dwarf_readu32 (addr, &u32)) < 0)
	    goto fail;
	  curr_ip += u32 * dci->code_align;
	  Debug (15, "CFA_advance_loc4 to 0x%lx\n", (long) curr_ip);
	  break;

	case DW_CFA_MIPS_advance_loc8:
#ifdef UNW_TARGET_MIPS
	  {
	    uint64_t u64;

	    if ((ret = dwarf_readu64 (addr, &u64)) < 0)
	      goto fail;
	    curr_ip += u64 * dci->code_align;
	    Debug (15, "CFA_MIPS_advance_loc8\n");
	    break;
	  }
#else
	  Debug (1, "DW_CFA_MIPS_advance_loc8 on non-MIPS target\n");
	  ret = -UNW_EINVAL;
	  goto fail;
#endif

	case DW_CFA_offset:
	  regnum = operand;
	  if (regnum >= DWARF_NUM_PRESERVED_REGS)
	    {
	      Debug (1, "Invalid register number %u in DW_cfa_OFFSET\n",
		     (unsigned int) regnum);
	      ret = -UNW_EBADREG;
	      goto fail;
	    }
	  if ((ret = dwarf_read_uleb128 (addr, &val)) < 0)
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_CFAREL, val * dci->data_align);
	  Debug (15, "CFA_offset r%lu at cfa+0x%lx\n",
		 (long) regnum, (long) (val * dci->data_align));
	  break;

	case DW_CFA_offset_extended:
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_uleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_CFAREL, val * dci->data_align);
	  Debug (15, "CFA_offset_extended r%lu at cf+0x%lx\n",
		 (long) regnum, (long) (val * dci->data_align));
	  break;

	case DW_CFA_offset_extended_sf:
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_sleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_CFAREL, val * dci->data_align);
	  Debug (15, "CFA_offset_extended_sf r%lu at cf+0x%lx\n",
		 (long) regnum, (long) (val * dci->data_align));
	  break;

	case DW_CFA_restore:
	  regnum = operand;
	  if (regnum >= DWARF_NUM_PRESERVED_REGS)
	    {
	      Debug (1, "Invalid register number %u in DW_CFA_restore\n",
		     (unsigned int) regnum);
	      ret = -UNW_EINVAL;
	      goto fail;
	    }
	  sr->rs_current.reg[regnum] = sr->rs_initial.reg[regnum];
	  Debug (15, "CFA_restore r%lu\n", (long) regnum);
	  break;

	case DW_CFA_restore_extended:
	  if ((ret = dwarf_read_uleb128 (addr, &regnum)) < 0)
	    goto fail;
	  if (regnum >= DWARF_NUM_PRESERVED_REGS)
	    {
	      Debug (1, "Invalid register number %u in "
		     "DW_CFA_restore_extended\n", (unsigned int) regnum);
	      ret = -UNW_EINVAL;
	      goto fail;
	    }
	  sr->rs_current.reg[regnum] = sr->rs_initial.reg[regnum];
	  Debug (15, "CFA_restore_extended r%lu\n", (long) regnum);
	  break;

	case DW_CFA_nop:
	  break;

	case DW_CFA_set_loc:
	  fde_encoding = dci->fde_encoding;
	  if ((ret = dwarf_read_encoded_pointer (addr, fde_encoding,
						 &c->pi, &curr_ip)) < 0)
	    goto fail;
	  Debug (15, "CFA_set_loc to 0x%lx\n", (long) curr_ip);
	  break;

	case DW_CFA_undefined:
	  if ((ret = read_regnum (addr, &regnum)) < 0)
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_UNDEF, 0);
	  Debug (15, "CFA_undefined r%lu\n", (long) regnum);
	  break;

	case DW_CFA_same_value:
	  if ((ret = read_regnum (addr, &regnum)) < 0)
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_SAME, 0);
	  Debug (15, "CFA_same_value r%lu\n", (long) regnum);
	  break;

	case DW_CFA_register:
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_uleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_REG, val);
	  Debug (15, "CFA_register r%lu to r%lu\n", (long) regnum, (long) val);
	  break;

	case DW_CFA_remember_state:
	  new_rs = alloc_reg_state ();
	  if (!new_rs)
	    {
	      Debug (1, "Out of memory in DW_CFA_remember_state\n");
	      ret = -UNW_ENOMEM;
	      goto fail;
	    }

	  memcpy (new_rs->reg, sr->rs_current.reg, sizeof (new_rs->reg));
	  new_rs->next = rs_stack;
	  rs_stack = new_rs;
	  Debug (15, "CFA_remember_state\n");
	  break;

	case DW_CFA_restore_state:
	  if (!rs_stack)
	    {
	      Debug (1, "register-state stack underflow\n");
	      ret = -UNW_EINVAL;
	      goto fail;
	    }
	  memcpy (&sr->rs_current.reg, &rs_stack->reg, sizeof (rs_stack->reg));
	  old_rs = rs_stack;
	  rs_stack = rs_stack->next;
	  free_reg_state (old_rs);
	  Debug (15, "CFA_restore_state\n");
	  break;

	case DW_CFA_def_cfa:
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_uleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, DWARF_CFA_REG_COLUMN, DWARF_WHERE_REG, regnum);
	  set_reg (sr, DWARF_CFA_OFF_COLUMN, 0, val);	/* NOT factored! */
	  Debug (15, "CFA_def_cfa r%lu+0x%lx\n", (long) regnum, (long) val);
	  break;

	case DW_CFA_def_cfa_sf:
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_sleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, DWARF_CFA_REG_COLUMN, DWARF_WHERE_REG, regnum);
	  set_reg (sr, DWARF_CFA_OFF_COLUMN, 0,
		   val * dci->data_align);		/* factored! */
	  Debug (15, "CFA_def_cfa_sf r%lu+0x%lx\n",
		 (long) regnum, (long) (val * dci->data_align));
	  break;

	case DW_CFA_def_cfa_register:
	  if ((ret = read_regnum (addr, &regnum)) < 0)
	    goto fail;
	  set_reg (sr, DWARF_CFA_REG_COLUMN, DWARF_WHERE_REG, regnum);
	  Debug (15, "CFA_def_cfa_register r%lu\n", (long) regnum);
	  break;

	case DW_CFA_def_cfa_offset:
	  if ((ret = dwarf_read_uleb128 (addr, &val)) < 0)
	    goto fail;
	  set_reg (sr, DWARF_CFA_OFF_COLUMN, 0, val);	/* NOT factored! */
	  Debug (15, "CFA_def_cfa_offset 0x%lx\n", (long) val);
	  break;

	case DW_CFA_def_cfa_offset_sf:
	  if ((ret = dwarf_read_sleb128 (addr, &val)) < 0)
	    goto fail;
	  set_reg (sr, DWARF_CFA_OFF_COLUMN, 0,
		   val * dci->data_align);	/* factored! */
	  Debug (15, "CFA_def_cfa_offset_sf 0x%lx\n",
		 (long) (val * dci->data_align));
	  break;

	case DW_CFA_def_cfa_expression:
	  /* Save the address of the DW_FORM_block for later evaluation. */
	  set_reg (sr, DWARF_CFA_REG_COLUMN, DWARF_WHERE_EXPR, *addr);

	  if ((ret = dwarf_read_uleb128 (addr, &len)) < 0)
	    goto fail;

	  Debug (15, "CFA_def_cfa_expr @ 0x%lx [%lu bytes]\n",
		 (long) *addr, (long) len);
	  *addr += len;
	  break;

	case DW_CFA_expression:
	  if ((ret = read_regnum (addr, &regnum)) < 0)
	    goto fail;

	  /* Save the address of the DW_FORM_block for later evaluation. */
	  set_reg (sr, regnum, DWARF_WHERE_EXPR, *addr);

	  if ((ret = dwarf_read_uleb128 (addr, &len)) < 0)
	    goto fail;

	  Debug (15, "CFA_expression r%lu @ 0x%lx [%lu bytes]\n",
		 (long) regnum, (long) addr, (long) len);
	  *addr += len;
	  break;

	case DW_CFA_GNU_args_size:
	  if ((ret = dwarf_read_uleb128 (addr, &val)) < 0)
	    goto fail;
	  sr->args_size = val;
	  Debug (15, "CFA_GNU_args_size %lu\n", (long) val);
	  break;

	case DW_CFA_GNU_negative_offset_extended:
	  /* A comment in GCC says that this is obsoleted by
	     DW_CFA_offset_extended_sf, but that it's used by older
	     PowerPC code.  */
	  if (((ret = read_regnum (addr, &regnum)) < 0)
	      || ((ret = dwarf_read_uleb128 (addr, &val)) < 0))
	    goto fail;
	  set_reg (sr, regnum, DWARF_WHERE_CFAREL, -(val * dci->data_align));
	  Debug (15, "CFA_GNU_negative_offset_extended cfa+0x%lx\n",
		 (long) -(val * dci->data_align));
	  break;

	case DW_CFA_GNU_window_save:
#ifdef UNW_TARGET_SPARC
	  /* This is a special CFA to handle all 16 windowed registers
	     on SPARC.  */
	  for (regnum = 16; regnum < 32; ++regnum)
	    set_reg (sr, regnum, DWARF_WHERE_CFAREL,
		     (regnum - 16) * sizeof (unw_word_t));
	  Debug (15, "CFA_GNU_window_save\n");
	  break;
#else
	  /* FALL THROUGH */
#endif
	case DW_CFA_lo_user:
	case DW_CFA_hi_user:
	  Debug (1, "Unexpected CFA opcode 0x%x\n", op);
	  ret = -UNW_EINVAL;
	  goto fail;
	}
    }
  ret = 0;

 fail:
  /* Free the register-state stack, if not empty already.  */
  while (rs_stack)
    {
      old_rs = rs_stack;
      rs_stack = rs_stack->next;
      free_reg_state (old_rs);
    }
  return ret;
}

static int
fetch_proc_info (struct dwarf_cursor *c, unw_word_t ip, int need_unwind_info)
{
  int ret, dynamic = 1;

  /* The 'ip' can point either to the previous or next instruction
     depending on what type of frame we have: normal call or a place
     to resume execution (e.g. after signal frame).

     For a normal call frame we need to back up so we point within the
     call itself; this is important because a) the call might be the
     very last instruction of the function and the edge of the FDE,
     and b) so that run_cfi_program() runs locations up to the call
     but not more.

     For execution resume, we need to do the exact opposite and look
     up using the current 'ip' value.  That is where execution will
     continue, and it's important we get this right, as 'ip' could be
     right at the function entry and hence FDE edge, or at instruction
     that manipulates CFA (push/pop). */
  if (c->use_prev_instr)
    --ip;

  if (c->pi_valid && !need_unwind_info)
    return 0;

  memset (&c->pi, 0, sizeof (c->pi));

  /* check dynamic info first --- it overrides everything else */
    {
      dynamic = 0;
      if ((ret = tdep_find_proc_info (c, ip, need_unwind_info)) < 0)
	return ret;
    }

  if (c->pi.format != UNW_INFO_FORMAT_DYNAMIC
      && c->pi.format != UNW_INFO_FORMAT_TABLE
      && c->pi.format != UNW_INFO_FORMAT_REMOTE_TABLE)
    return -UNW_ENOINFO;

  c->pi_valid = 1;
  c->pi_is_dynamic = dynamic;

  /* Let system/machine-dependent code determine frame-specific attributes. */
  if (ret >= 0)
    tdep_fetch_frame (c, ip, need_unwind_info);

  /* Update use_prev_instr for the next frame. */
  if (need_unwind_info)
  {
    assert(c->pi.unwind_info);
    struct dwarf_cie_info *dci = c->pi.unwind_info;
    c->use_prev_instr = ! dci->signal_frame;
  }

  return ret;
}

HIDDEN void
unwi_put_dynamic_unwind_info (unw_proc_info_t *pi)
{
  switch (pi->format)
    {
    case UNW_INFO_FORMAT_DYNAMIC:
#ifndef UNW_LOCAL_ONLY
# ifdef UNW_REMOTE_ONLY
      unwi_dyn_remote_put_unwind_info (as, pi, arg);
# else
      if (as != unw_local_addr_space)
	unwi_dyn_remote_put_unwind_info (as, pi, arg);
# endif
#endif
      break;

    case UNW_INFO_FORMAT_TABLE:
    case UNW_INFO_FORMAT_REMOTE_TABLE:
#ifdef tdep_put_unwind_info
      tdep_put_unwind_info (as, pi, arg);
      break;
#endif
      /* fall through */
    default:
      break;
    }
}

static inline void
put_unwind_info (struct dwarf_cursor *c, unw_proc_info_t *pi)
{
  if (c->pi_is_dynamic)
    unwi_put_dynamic_unwind_info (pi);
  else if (pi->unwind_info && pi->format == UNW_INFO_FORMAT_TABLE)
    {
      // mempool_free (&dwarf_cie_info_pool, pi->unwind_info);
      free(pi->unwind_info);
      pi->unwind_info = NULL;
    }
}

static inline int
parse_fde (struct dwarf_cursor *c, unw_word_t ip, dwarf_state_record_t *sr)
{
  struct dwarf_cie_info *dci;
  unw_word_t addr;
  int ret;

  dci = c->pi.unwind_info;
  c->ret_addr_column = dci->ret_addr_column;

  addr = dci->cie_instr_start;
  if ((ret = run_cfi_program (c, sr, ~(unw_word_t) 0, &addr,
			      dci->cie_instr_end, dci)) < 0)
    return ret;

  memcpy (&sr->rs_initial, &sr->rs_current, sizeof (sr->rs_initial));

  addr = dci->fde_instr_start;
  if ((ret = run_cfi_program (c, sr, ip, &addr, dci->fde_instr_end, dci)) < 0)
    return ret;

  return 0;
}

static int
parse_dynamic (struct dwarf_cursor *c, unw_word_t ip, dwarf_state_record_t *sr)
{
  Debug (1, "Not yet implemented\n");
#if 0
  /* Don't forget to set the ret_addr_column!  */
  c->ret_addr_column = XXX;
#endif
  return -UNW_ENOINFO;
}

static int
create_state_record_for (struct dwarf_cursor *c, dwarf_state_record_t *sr,
			 unw_word_t ip)
{
  int i, ret;

  assert (c->pi_valid);

  memset (sr, 0, sizeof (*sr));
  for (i = 0; i < DWARF_NUM_PRESERVED_REGS + 2; ++i)
    set_reg (sr, i, DWARF_WHERE_SAME, 0);

  switch (c->pi.format)
    {
    case UNW_INFO_FORMAT_TABLE:
    case UNW_INFO_FORMAT_REMOTE_TABLE:
      ret = parse_fde (c, ip, sr);
      break;

    case UNW_INFO_FORMAT_DYNAMIC:
      ret = parse_dynamic (c, ip, sr);
      break;

    default:
      Debug (1, "Unexpected unwind-info format %d\n", c->pi.format);
      ret = -UNW_EINVAL;
    }
  return ret;
}

HIDDEN dwarf_loc_t
x86_scratch_loc (struct cursor *c, unw_regnum_t reg)
{
  return DWARF_REG_LOC (&c->dwarf, reg);
}

static inline int
dwarf_get (struct dwarf_cursor *c, dwarf_loc_t loc, unw_word_t *val)
{
  if (!DWARF_GET_LOC (loc))
    return -1;
  unw_word_t* _loc = (unw_word_t*)DWARF_GET_LOC (loc);
  *val = *_loc;
  return 0;
}


static inline int
dwarf_put (struct dwarf_cursor *c, dwarf_loc_t loc, unw_word_t val)
{
  if (!DWARF_GET_LOC (loc))
    return -1;
  unw_word_t* _loc = (unw_word_t*)DWARF_GET_LOC (loc);
  *_loc = val;
  return 0;
}

HIDDEN int
tdep_access_reg (struct cursor *c, unw_regnum_t reg, unw_word_t *valp,
		 int write)
{
  dwarf_loc_t loc = DWARF_NULL_LOC;
  unsigned int mask;
  int arg_num;

  switch (reg)
    {

    case UNW_X86_EIP:
      if (write)
	c->dwarf.ip = *valp;		/* also update the EIP cache */
      loc = c->dwarf.loc[EIP];
      break;

    case UNW_X86_CFA:
    case UNW_X86_ESP:
      if (write)
	return -UNW_EREADONLYREG;
      *valp = c->dwarf.cfa;
      return 0;

    case UNW_X86_EAX:
    case UNW_X86_EDX:
      arg_num = reg - UNW_X86_EAX;
      mask = (1 << arg_num);
      if (write)
	{
	  c->dwarf.eh_args[arg_num] = *valp;
	  c->dwarf.eh_valid_mask |= mask;
	  return 0;
	}
      else if ((c->dwarf.eh_valid_mask & mask) != 0)
	{
	  *valp = c->dwarf.eh_args[arg_num];
	  return 0;
	}
      else
	loc = c->dwarf.loc[(reg == UNW_X86_EAX) ? EAX : EDX];
      break;

    case UNW_X86_ECX: loc = c->dwarf.loc[ECX]; break;
    case UNW_X86_EBX: loc = c->dwarf.loc[EBX]; break;

    case UNW_X86_EBP: loc = c->dwarf.loc[EBP]; break;
    case UNW_X86_ESI: loc = c->dwarf.loc[ESI]; break;
    case UNW_X86_EDI: loc = c->dwarf.loc[EDI]; break;
    case UNW_X86_EFLAGS: loc = c->dwarf.loc[EFLAGS]; break;
    case UNW_X86_TRAPNO: loc = c->dwarf.loc[TRAPNO]; break;

    case UNW_X86_FCW:
    case UNW_X86_FSW:
    case UNW_X86_FTW:
    case UNW_X86_FOP:
    case UNW_X86_FCS:
    case UNW_X86_FIP:
    case UNW_X86_FEA:
    case UNW_X86_FDS:
    case UNW_X86_MXCSR:
    case UNW_X86_GS:
    case UNW_X86_FS:
    case UNW_X86_ES:
    case UNW_X86_DS:
    case UNW_X86_SS:
    case UNW_X86_CS:
    case UNW_X86_TSS:
    case UNW_X86_LDT:
      loc = x86_scratch_loc (c, reg);
      break;

    default:
      Debug (1, "bad register number %u\n", reg);
      return -UNW_EBADREG;
    }

  if (write)
    return dwarf_put (&c->dwarf, loc, *valp);
  else
    return dwarf_get (&c->dwarf, loc, valp);
}

PROTECTED int
unw_get_reg (unw_cursor_t *cursor, int regnum, unw_word_t *valp)
{
  struct cursor *c = (struct cursor *) cursor;

  // We can get the IP value directly without needing a lookup.
  if (regnum == UNW_REG_IP)
    {
      *valp = tdep_get_ip (c);
      return 0;
    }

  return tdep_access_reg (c, regnum, valp, 0);
}

static int
apply_reg_state (struct dwarf_cursor *c, struct dwarf_reg_state *rs)
{
  unw_word_t regnum, addr, cfa, ip;
  unw_word_t prev_ip, prev_cfa;
  dwarf_loc_t cfa_loc;
  int i, ret;

  prev_ip = c->ip;
  prev_cfa = c->cfa;

  /* Evaluate the CFA first, because it may be referred to by other
     expressions.  */

  if (rs->reg[DWARF_CFA_REG_COLUMN].where == DWARF_WHERE_REG)
    {
      /* CFA is equal to [reg] + offset: */

      /* As a special-case, if the stack-pointer is the CFA and the
	 stack-pointer wasn't saved, popping the CFA implicitly pops
	 the stack-pointer as well.  */
      if ((rs->reg[DWARF_CFA_REG_COLUMN].val == UNW_TDEP_SP)
          && (UNW_TDEP_SP < ARRAY_SIZE(rs->reg))
	  && (rs->reg[UNW_TDEP_SP].where == DWARF_WHERE_SAME))
	  cfa = c->cfa;
      else
	{
	  regnum = dwarf_to_unw_regnum (rs->reg[DWARF_CFA_REG_COLUMN].val);
	  if ((ret = unw_get_reg ((unw_cursor_t *) c, regnum, &cfa)) < 0)
	    return ret;
	}
      cfa += rs->reg[DWARF_CFA_OFF_COLUMN].val;
    }
  else
    {
      /* CFA is equal to EXPR: */

      assert (rs->reg[DWARF_CFA_REG_COLUMN].where == DWARF_WHERE_EXPR);

      addr = rs->reg[DWARF_CFA_REG_COLUMN].val;
      if ((ret = eval_location_expr (c, addr, &cfa_loc)) < 0)
	return ret;
      /* the returned location better be a memory location... */
      if (DWARF_IS_REG_LOC (cfa_loc))
	return -UNW_EBADFRAME;
      cfa = DWARF_GET_LOC (cfa_loc);
    }

  for (i = 0; i < DWARF_NUM_PRESERVED_REGS; ++i)
    {
      switch ((dwarf_where_t) rs->reg[i].where)
	{
	case DWARF_WHERE_UNDEF:
	  c->loc[i] = DWARF_NULL_LOC;
	  break;

	case DWARF_WHERE_SAME:
	  break;

	case DWARF_WHERE_CFAREL:
	  c->loc[i] = DWARF_MEM_LOC (c, cfa + rs->reg[i].val);
	  break;

	case DWARF_WHERE_REG:
	  c->loc[i] = DWARF_REG_LOC (c, dwarf_to_unw_regnum (rs->reg[i].val));
	  break;

	case DWARF_WHERE_EXPR:
	  addr = rs->reg[i].val;
	  if ((ret = eval_location_expr (c, addr, c->loc + i)) < 0)
	    return ret;
	  break;
	}
    }

  c->cfa = cfa;
  /* DWARF spec says undefined return address location means end of stack. */
  if (DWARF_IS_NULL_LOC (c->loc[c->ret_addr_column]))
    c->ip = 0;
  else
  {
    ret = dwarf_get (c, c->loc[c->ret_addr_column], &ip);
    if (ret < 0)
      return ret;
    c->ip = ip;
  }

  /* XXX: check for ip to be code_aligned */
  if (c->ip == prev_ip && c->cfa == prev_cfa)
    {
      Dprintf ("%s: ip and cfa unchanged; stopping here (ip=0x%lx)\n",
	       __FUNCTION__, (long) c->ip);
      return -UNW_EBADFRAME;
    }

  if (c->stash_frames)
    tdep_stash_frame (c, rs);

  return 0;
}

static int
uncached_dwarf_find_save_locs (struct dwarf_cursor *c)
{
  dwarf_state_record_t sr;
  int ret;

  if ((ret = fetch_proc_info (c, c->ip, 1)) < 0)
    {
      put_unwind_info (c, &c->pi);
      return ret;
    }

  if ((ret = create_state_record_for (c, &sr, c->ip)) < 0)
    {
      /* ANDROID support update. */
      put_unwind_info (c, &c->pi);
      /* End of ANDROID update. */
      return ret;
    }

  if ((ret = apply_reg_state (c, &sr.rs_current)) < 0)
    {
      /* ANDROID support update. */
      put_unwind_info (c, &c->pi);
      /* End of ANDROID update. */
      return ret;
    }

  put_unwind_info (c, &c->pi);
  return 0;
}

HIDDEN int
dwarf_find_save_locs (struct dwarf_cursor *c)
{
#if defined(CONSERVE_STACK)
  dwarf_reg_state_t *rs_copy;
#else
#endif

  return uncached_dwarf_find_save_locs (c);
}

HIDDEN int
dwarf_step (struct dwarf_cursor *c)
{
  int ret;

  if ((ret = dwarf_find_save_locs (c)) >= 0) {
    c->pi_valid = 0;
    ret = 1;
  }

  Debug (15, "returning %d\n", ret);
  return ret;
}

#if defined(__i386__)
#if 0
PROTECTED int
unw_step (unw_cursor_t *cursor)
{
  struct cursor *c = (struct cursor *) cursor;
  int ret, i;

  Debug (1, "(cursor=%p, ip=0x%08x)\n", c, (unsigned) c->dwarf.ip);

  /* ANDROID support update. */
  /* Save the current ip/cfa to prevent looping if the decode yields
     the same ip/cfa as before. */
  unw_word_t old_ip = c->dwarf.ip;
  unw_word_t old_cfa = c->dwarf.cfa;
  /* End of ANDROID update. */

  /* Try DWARF-based unwinding... */
  ret = dwarf_step (&c->dwarf);

#if !defined(UNW_LOCAL_ONLY)
  /* Do not use this method on a local unwind. There is a very high
   * probability this method will try to access unmapped memory, which
   * will crash the process. Since this almost never actually works,
   * it should be okay to skip.
   */
  if (ret < 0)
    {
      /* DWARF failed, let's see if we can follow the frame-chain
	 or skip over the signal trampoline.  */
      struct dwarf_loc ebp_loc, eip_loc;

      /* We could get here because of missing/bad unwind information.
         Validate all addresses before dereferencing. */
      c->validate = 1;

      Debug (13, "dwarf_step() failed (ret=%d), trying frame-chain\n", ret);

      if (unw_is_signal_frame (cursor))
        {
          ret = unw_handle_signal_frame(cursor);
	  if (ret < 0)
	    {
	      Debug (2, "returning 0\n");
	      return 0;
	    }
        }
      else
	{
	  ret = dwarf_get (&c->dwarf, c->dwarf.loc[EBP], &c->dwarf.cfa);
	  if (ret < 0)
	    {
	      Debug (2, "returning %d\n", ret);
	      return ret;
	    }

	  Debug (13, "[EBP=0x%x] = 0x%x\n", DWARF_GET_LOC (c->dwarf.loc[EBP]),
		 c->dwarf.cfa);

	  ebp_loc = DWARF_LOC (c->dwarf.cfa, 0);
	  eip_loc = DWARF_LOC (c->dwarf.cfa + 4, 0);
	  c->dwarf.cfa += 8;

	  /* Mark all registers unsaved, since we don't know where
	     they are saved (if at all), except for the EBP and
	     EIP.  */
	  for (i = 0; i < DWARF_NUM_PRESERVED_REGS; ++i)
	    c->dwarf.loc[i] = DWARF_NULL_LOC;

          c->dwarf.loc[EBP] = ebp_loc;
          c->dwarf.loc[EIP] = eip_loc;
	}
      c->dwarf.ret_addr_column = EIP;

      if (!DWARF_IS_NULL_LOC (c->dwarf.loc[EBP]))
	{
	  ret = dwarf_get (&c->dwarf, c->dwarf.loc[EIP], &c->dwarf.ip);
	  if (ret < 0)
	    {
	      Debug (13, "dwarf_get([EIP=0x%x]) failed\n", DWARF_GET_LOC (c->dwarf.loc[EIP]));
	      Debug (2, "returning %d\n", ret);
	      return ret;
	    }
	  else
	    {
	      Debug (13, "[EIP=0x%x] = 0x%x\n", DWARF_GET_LOC (c->dwarf.loc[EIP]),
		c->dwarf.ip);
	    }
	}
      else
	c->dwarf.ip = 0;
    }
#endif

  /* ANDROID support update. */
  if (ret >= 0)
    {
      if (c->dwarf.ip)
        {
          /* Adjust the pc to the instruction before. */
          c->dwarf.ip--;
        }
      /* If the decode yields the exact same ip/cfa as before, then indicate
         the unwind is complete. */
      if (old_ip == c->dwarf.ip && old_cfa == c->dwarf.cfa)
        {
          Dprintf ("%s: ip and cfa unchanged; stopping here (ip=0x%lx)\n",
                   __FUNCTION__, (long) c->dwarf.ip);
          return -UNW_EBADFRAME;
        }
      c->dwarf.frame++;
    }
  /* End of ANDROID update. */
  if (unlikely (ret <= 0))
    return 0;

  return (c->dwarf.ip == 0) ? 0 : 1;
}
#endif

static int
init_dwarf (struct cursor *c, ucontext_t *uc)
{
  int ret, i;
  c->uc = uc;
  c->dwarf.as_arg = c;
  c->dwarf.loc[EAX] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EAX);
  c->dwarf.loc[ECX] = DWARF_REG_LOC (&c->dwarf, UNW_X86_ECX);
  c->dwarf.loc[EDX] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EDX);
  c->dwarf.loc[EBX] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EBX);
  c->dwarf.loc[ESP] = DWARF_REG_LOC (&c->dwarf, UNW_X86_ESP);
  c->dwarf.loc[EBP] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EBP);
  c->dwarf.loc[ESI] = DWARF_REG_LOC (&c->dwarf, UNW_X86_ESI);
  c->dwarf.loc[EDI] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EDI);
  c->dwarf.loc[EIP] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EIP);
  c->dwarf.loc[EFLAGS] = DWARF_REG_LOC (&c->dwarf, UNW_X86_EFLAGS);
  c->dwarf.loc[TRAPNO] = DWARF_REG_LOC (&c->dwarf, UNW_X86_TRAPNO);
  c->dwarf.loc[ST0] = DWARF_REG_LOC (&c->dwarf, UNW_X86_ST0);
  for (i = ST0 + 1; i < DWARF_NUM_PRESERVED_REGS; ++i)
    c->dwarf.loc[i] = DWARF_NULL_LOC;

  ret = dwarf_get (&c->dwarf, c->dwarf.loc[EIP], &c->dwarf.ip);
  if (ret < 0)
    return ret;

  ret = dwarf_get (&c->dwarf, DWARF_REG_LOC (&c->dwarf, UNW_X86_ESP),
		   &c->dwarf.cfa);
  if (ret < 0)
    return ret;
  return 0;
}

int
mybacktrace (backtrace_callback callback, void *args, void* uc_mcontext)
{
  struct cursor c;
  ucontext_t uc;
  memset (&c, 0, sizeof (c));
  memset (&uc, 0, sizeof (uc));
  memcpy(&uc.uc_mcontext, uc_mcontext, sizeof(mcontext_t));
  if (init_dwarf (&c, &uc) < 0)
    {
      return 1;
    }
  while (1)
    {
      if (dwarf_step (&c.dwarf) < 0)
        {
          return 0;
        }
      if (BACKTRACE_CONTINUE != callback (c.dwarf.ip, args))
        {
          return 0;
        }
    }
  return 1;
}

#endif
