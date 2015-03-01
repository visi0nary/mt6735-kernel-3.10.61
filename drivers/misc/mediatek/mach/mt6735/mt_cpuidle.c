#include <linux/kernel.h>
#include <linux/init.h>
#if defined (__KERNEL__)  //|| !defined (__CTP__)
#include <linux/jiffies.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/cpuidle.h>
#include <linux/cpu_pm.h>
#include <linux/irqchip/arm-gic.h>

#include <asm/psci.h>
// #include <asm/smp_scu.h>
// #include <asm/cpuidle.h>
#endif //#if !defined (__CTP__)

#include <asm/proc-fns.h>
#include <asm/suspend.h>
#include <asm/tlbflush.h>
#include <asm/memory.h>
#include <asm/system.h>
#if !defined (__KERNEL__) //|| defined (__CTP__)
#include "reg_base.H"
#include "mt_dormant.h"
#include "mt_cpuidle.h"
#include "smp.h"
#include "mt_spm.h"
#include "irq.h"
#include "sync_write.h"
//#include "mt_dbg_v71.h"
#include "gic.h"
#else  //#if !defined (__KERNEL__) //|| defined(__CTP__)
// #include <asm/idmap.h>
#include <asm/irqflags.h>
#include <mach/mt_reg_base.h>
#include <mach/mt_dormant.h>
#include <mach/mt_cpuidle.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_idle.h>
// #include <mach/smp.h>
#include <mach/mt_irq.h>
#include <mach/sync_write.h>
// #include <mach/mt_dbg_v71.h>
#include <mach/mt_boot.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/mt_dbg.h>
#endif //#if !defined (__KERNEL__) //|| defined(__CTP__)

#if defined(MT_DORMANT_UT)
#include <mach/smp.h>
#endif // #if defined(MT_DORMANT_UT)

/*********************************
 * macro
 **********************************/

#define DBGAPB_CORE_OFFSET (0x00100000)

#if defined (__KERNEL__)
//#define CA15L_CONFIG_BASE 0xf0200200
#if defined (CONFIG_OF)
static unsigned long dbgapb_base;
static unsigned long mcucfg_base;
// static unsigned long cci400_base;
static unsigned long biu_base;
static unsigned long infracfg_ao_base;
static unsigned long gic_id_base;
static unsigned long gic_ci_base;

#define DBGAPB_NODE      "mediatek,DBG_DEBUG"
#define MCUCFG_NODE      "mediatek,MCUCFG"
// #define CCI400_NODE      "mediatek,CCI400"
#define BIU_NODE         "mediatek,MCU_BIU"
#define INFRACFG_AO_NODE "mediatek,INFRACFG_AO"
// #define GIC_NODE "mediatek,GIC"
#define GIC_NODE         "mtk,mt-gic"

#define DMT_MP0_DBGAPB_BASE       (dbgapb_base)
#define DMT_MP1_DBGAPB_BASE       (dbgapb_base + (DBGAPB_CORE_OFFSET*4))
#define DMT_MCUCFG_BASE           (mcucfg_base)      //0x1020_0000
// #define DMT_CCI400_BASE           (cci400_base)      //0x1039_0000
#define DMT_BIU_BASE              (biu_base)         //0x1030_0000
#define DMT_INFRACFG_AO_BASE      (infracfg_ao_base) //0x1000_1000
#define DMT_GIC_CPU_BASE          (gic_ci_base)
#define DMT_GIC_DIST_BASE         (gic_id_base)

#else // #if !defined (CONFIG_OF)
#define DMT_MP0_DBGAPB_BASE    (0xf0810000)
#define DMT_MP1_DBGAPB_BASE    (0xf0C10000)
#define DMT_MCUCFG_BASE        (0xf0200000)          //0x1020_0000
// #define DMT_CCI400_BASE        (0xf0390000)          //0x1039_0000
#define DMT_BIU_BASE           (0xf0300000)          //0x1030_0000
#define DMT_INFRACFG_AO_BASE   (0xf0001000)          //0x1000_1000
#define DMT_GIC_CPU_BASE       (0xf0220000 + 0x2000)
#define DMT_GIC_DIST_BASE      (0xf0220000 + 0x1000)
#endif //#if defined (CONFIG_OF)

#else //#if defined (__KERNEL__)
//#define CA15L_CONFIG_BASE 0x10200200
#define DMT_MP0_DBGAPB_BASE    (0x10810000)
#define DMT_MP1_DBGAPB_BASE    (0x10C10000)
#define DMT_MCUCFG_BASE        (0x10200000)        //0x1020_0000
// #define DMT_CCI400_BASE        (0x10390000)        //0x1039_0000
#define DMT_BIU_BASE           (0x10300000)        //0x1030_0000
#define DMT_INFRACFG_AO_BASE   (0x10001000)        //0x1000_1000

typedef unsigned long long u64;
#define ____cacheline_aligned __attribute__((aligned(8)))
#define __weak __attribute__((weak))
#define __naked __attribute__((naked))
typedef enum {
    CHIP_SW_VER_01 = 0x0000,
    CHIP_SW_VER_02 = 0x0001
} CHIP_SW_VER;
#define local_fiq_enable() do {} while(0)
#endif //#if defined (__KERNEL__)

#define MP0_CA7L_CACHE_CONFIG   (DMT_MCUCFG_BASE + 0)
#define MP1_CA7L_CACHE_CONFIG   (DMT_MCUCFG_BASE + 0x200)
#define L2RSTDISABLE 		(1 << 4)

#define MP0_AXI_CONFIG          (DMT_MCUCFG_BASE + 0x2C) 
#define MP1_AXI_CONFIG          (DMT_MCUCFG_BASE + 0x22C) 
#define ACINACTM                (1 << 4)


// #define CCI400_MP0_SNOOP_CTLR   (DMT_CCI400_BASE + 0x5000) //0x10395000
// #define CCI400_MP1_SNOOP_CTLR   (DMT_CCI400_BASE + 0x4000) //0x10394000
// #define CCI400_STATUS		(DMT_CCI400_BASE + 0x000C) //0x10390004
// #define CCI400_CONTROL		(DMT_CCI400_BASE + 0x0000) //0x10390000

#define BIU_CONTROL		(DMT_BIU_BASE + 0x0000)    //0x10300000
#define TLB_ULTRA_EN            (1 << 8)
#define DCM_EN                  (1 << 1)
#define CMD_QUEUE_EN            (1 << 0)


#ifdef CONFIG_MTK_RAM_CONSOLE
#define CPU_DORMANT_AEE_RR_REC 1
#else
#define CPU_DORMANT_AEE_RR_REC 0
#endif


#define reg_read(addr)          __raw_readl(IOMEM(addr))        //(*(volatile unsigned long *)(addr))
#define reg_write(addr, val)    mt_reg_sync_writel(val, addr)
#define _and(a, b) 	((a) & (b))
#define _or(a, b) 	((a) | (b))
#define _aor(a, b, c) 	_or(_and(a, b), (c))


#define read_cntpct()					\
	({						\
		unsigned long long cntpct;		\
		__asm__ __volatile__(			\
			"MRRC p15, 0, %Q0, %R0, c14\n"	\
			:"=r"(cntpct)			\
			:				\
			:"memory");			\
		cntpct;					\
	})


#define read_cntpctl()					\
	({						\
		unsigned int cntpctl;			\
		__asm__ __volatile__(			\
			"MRC p15, 0, %0, c14, c2, 1\n"  \
			:"=r"(cntpctl)			\
			:				\
			:"memory");			\
		cntpctl;				\
	})

#define write_cntpctl(cntpctl)				\
	do {						\
		__asm__ __volatile__(			\
			"MCR p15, 0, %0, c14, c2, 1\n"  \
			:				\
			:"r"(cntpctl));			\
	} while (0)


/*********************************
 * macro for log
 **********************************/
#define CPU_DORMANT_LOG_WITH_NONE                           0
#define CPU_DORMANT_LOG_WITH_XLOG                           1
#define CPU_DORMANT_LOG_WITH_PRINTK                         2

#define CPU_DORMANT_LOG_PRINT CPU_DORMANT_LOG_WITH_NONE

#if (CPU_DORMANT_LOG_PRINT == CPU_DORMANT_LOG_WITH_NONE)
#define CPU_DORMANT_INFO(fmt, args...)          do { } while(0)
#elif (CPU_DORMANT_LOG_PRINT == CPU_DORMANT_LOG_WITH_XLOG)
#define CPU_DORMANT_INFO(fmt, args...)		do { xlog_printk(ANDROID_LOG_INFO, "Power/cpu_dormant", fmt, ##args); } while(0)
#elif (CPU_DORMANT_LOG_PRINT == CPU_DORMANT_LOG_WITH_PRINTK)
#define CPU_DORMANT_INFO(fmt, args...)		do { printk("[Power/cpu_dormant] "fmt, ##args); } while(0)
#endif

#define zlog(fmt, args...)		xlog_printk(ANDROID_LOG_INFO, "Power/cpu_dormant", fmt, ##args)

#define MT_DORMANT_DEBUG 

#if defined (MT_DORMANT_DEBUG)
#define SENTINEL_CHECK(data, p) BUG_ON((unsigned long)(p) > ((unsigned long)(&data) + sizeof(data)))
#else //#if defined (MT_DORMANT_DEBGU)
#define SENTINEL_CHECK(a, b) do{} while(0)
#endif //#if defined (MT_DORMANT_DEBGU)


/* debug facility */
#define DEBUG_DORMANT_BYPASS (1==0)

// #define TSLOG_ENABLE
#if defined (TSLOG_ENABLE)
#define TSLOG(a, b) do { (a) = (b); } while(0)
#else 
#define TSLOG(a, b) do {} while(0)
#endif

/*********************************
 * struct
 **********************************/
typedef struct cpu_context {
	unsigned int banked_regs[32];
	unsigned int pmu_data[20];
	unsigned int vfp_data[32*2+8]; 
	unsigned int timer_data[8]; /* Global timers if the NS world has access to them */
	volatile u64 timestamp[5];
        unsigned int count, rst, abt, brk;
} core_context;

#define MAX_CORES (4)   //core num per cluster

typedef struct cluster_context {
	core_context core[MAX_CORES] ____cacheline_aligned;
	unsigned int dbg_data[40]; 
	int l2rstdisable;
	int l2rstdisable_rfcnt;
} cluster_context;


#define MAX_CLUSTER (2)
/*
 * Top level structure to hold the complete context of a multi cluster system
 */
typedef struct system_context {
	cluster_context cluster[2];
        struct _data_poc {
                void (*cpu_resume_phys)(void); // this is referenced by cpu_resume_wrapper
                unsigned long l2ectlr, l2actlr;
                CHIP_SW_VER chip_ver;
                unsigned long *cpu_dormant_aee_rr_rec;
        } poc  ____cacheline_aligned; 
} system_context;


#if CPU_DORMANT_AEE_RR_REC
phys_addr_t sleep_aee_rec_cpu_dormant;
unsigned long *sleep_aee_rec_cpu_dormant_va;
extern unsigned long *aee_rr_rec_cpu_dormant(void);
#endif


/*********************************
 * extern
 **********************************/
void __disable_dcache__inner_flush_dcache_L1__inner_clean_dcache_L2(void);
void __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);
void __disable_dcache__inner_flush_dcache_L1(void);

extern unsigned int *mt_save_banked_registers(unsigned int *container);
extern void mt_restore_banked_registers(unsigned int *container);

extern void cpu_wake_up(void);
extern void __disable_dcache(void);
extern void __enable_dcache(void);
extern void __disable_icache(void);
extern void __enable_icache(void);
extern void v7_flush_kern_dcache_louis(void);
extern void v7_flush_kern_dcache_all(void);
extern void cpu_resume(void);
extern void trace_start_dormant(void);

//check COREn IRQ	
//check COREn FIQ	
#define SPM_CORE_ID() core_idx()
#define SPM_IS_CPU_IRQ_OCCUR(core_id)                                   \
        ({                                                              \
                (!!(spm_read(SPM_SLEEP_WAKEUP_MISC) & ((0x101<<(core_id))))); \
        })

#if 1
#define PCLOG(cid) do {                                                 \
                __asm__ __volatile__ (                                  \
                        "str pc, [%0, %1] \n\t"                         \
                        ::"r"(dormant_data[0].poc.cpu_dormant_aee_rr_rec), "r"(cid*4) \
                        );                                              \
        } while(0)

#else
#define PCLOG(cid) do { } while(0)

#endif

/*********************************
 * glabal variable
 **********************************/
/*
 * Top level structure which encapsulates the context of the entire
 * Kingfisher system
 */

system_context dormant_data[1];
volatile int debug_dormant_bypass=0;
static int mt_dormant_initialized = 0;


/*********************************
 * function
 **********************************/

#define read_isr()							\
	({								\
		register unsigned int ret;				\
		__asm__ __volatile__ ("mrc   p15, 0, %0, c12, c1, 0 \n\t" \
				      :"=r"(ret));			\
		ret;							\
	})

#define read_mpidr()							\
	({								\
		register unsigned int ret;				\
		__asm__ __volatile__ ("mrc   p15, 0, %0, c0, c0, 5 \n\t" \
				      :"=r"(ret));			\
		ret;							\
	})

//inline unsigned read_midr(void)
#define read_midr()							\
	({								\
		register unsigned int ret;				\
		__asm__ __volatile__ ("mrc   p15, 0, %0, c0, c0, 0 \n\t" \
				      :"=r"(ret));			\
		ret;							\
	})


#define read_scuctlr()							\
	({								\
		register unsigned int ret;				\
		__asm__ __volatile__ ("mrc   p15, 1, %0, c9, c0, 4 \n\t" \
				      :"=r"(ret));			\
		ret;							\
	})


#define CA12_TYPEID     0x410FC0D0
#define CA17_TYPEID     0x410FC0E0
#define CA7_TYPEID      0x410FC070
#define CA7L_TYPEID     0x410FD030
#define CA53_TYPEID     0x410FD030
#define CPU_TYPEID_MASK 0xfffffff0

//inline int is_cpu_type(int type) 
#define is_cpu_type(type)						\
	({								\
		((read_midr() & CPU_TYPEID_MASK) == type) ? 1 : 0;	\
	})
	
//inline int boot_cpu(void)
#define boot_cpu()					\
	({						\
		((read_mpidr() & 0xf0f) == 0) ? 1 : 0;	\
	})

//inline int cpu_id(void)
#define cpu_id()				\
	({					\
		(read_mpidr() & 0x03);		\
	})

//inline int cluster_id(void)
#define cluster_id()				\
	({					\
		((read_mpidr() >> 8) & 0x0f);	\
	})

#define core_idx()                                                      \
	({                                                              \
                int mpidr = read_mpidr();                               \
		((( mpidr & (0x0f << 8)) >> 6) | (mpidr & 0x03));	\
	})

inline int read_id(int *cpu_id, int *cluster_id)
{
	int mpidr = read_mpidr();
	
	*cpu_id = mpidr & 0x0f;
	*cluster_id = (mpidr >> 8) & 0x0f;

	return mpidr;
}

#define system_cluster(system, clusterid)	(&((system_context *)system)->cluster[clusterid])
#define cluster_core(cluster, cpuid)	(&((cluster_context *)cluster)->core[cpuid])

void *_get_data(int core_or_cluster)
{
	int cpuid, clusterid;
	cluster_context *cluster;
	core_context *core;
					  
	read_id(&cpuid, &clusterid);
	
	cluster = system_cluster(dormant_data, clusterid);
	if (core_or_cluster == 1) 
		return (void *)cluster;

	core = cluster_core(cluster, cpuid);
	return (void *)core;
}

#define GET_CORE_DATA() ((core_context *)_get_data(0))
#define GET_CLUSTER_DATA() ((cluster_context *)_get_data(1))
#define GET_SYSTEM_DATA() ((system_context *)dormant_data)


/********************/
/* .global save_vfp */
/********************/
unsigned *save_vfp(unsigned int *container)
{
	__asm__ __volatile__ (
		".fpu neon \n\t"
		"@ FPU state save/restore.	   \n\t"
		"@    Save configuration registers and enable. \n\t"
                
                "@ CPACR allows CP10 and CP11 access \n\t"
                "mrc    p15,0,r4,c1,c0,2    \n\t"
                "ORR    r2,r4,#0xF00000 \n\t"
                "mcr    p15,0,r2,c1,c0,2 \n\t"
                "isb    \n\t"
                "mrc    p15,0,r2,c1,c0,2 \n\t"
                "and    r2,r2,#0xF00000 \n\t"
                "cmp    r2,#0xF00000 \n\t"
                "beq    0f \n\t"
                "movs   r2, #0 \n\t"
                "b      2f \n\t"

                "0: \n\t"
                "@ Enable FPU access to save/restore the other registers. \n\t"
		"FMRX   r1,FPEXC          @ vmrs   r1,FPEXC \n\t"
		"ldr    r2,=0x40000000  \n\t"
		"orr    r2, r1         \n\t"
		"FMXR   FPEXC,r2           @ vmsr   FPEXC,r2 \n\t"

		"@ Store the VFP-D16 registers. \n\t"
		"vstm   %0!, {D0-D15} \n\t"

		"@ Check for Advanced SIMD/VFP-D32 support \n\t"
		"FMRX   r2,MVFR0           @ vmrs   r2,MVFR0 \n\t"
		"and    r2,r2,#0xF         @ extract the A_SIMD bitfield \n\t"
		"cmp    r2, #0x2 \n\t"

		"@ Store the Advanced SIMD/VFP-D32 additional registers. \n\t"
		"vstmiaeq   %0!, {D16-D31} \n\t"
                "addne  %0, %0, #128 \n\t"

		"FMRX   r2,FPSCR           @ vmrs   r2,FPSCR \n\t"

                "tst	r1, #0x80000000    @#FPEXC_EX  \n\t"
                "beq	3f \n\t"
                "FMRX	r3, FPINST	   @ FPINST (only if FPEXC.EX is set) \n\t"
                
                "tst	r1, #0x10000000    @FPEXC_FP2V, is there an FPINST2 to read? \n\t"
                "beq	3f \n\t"
                "FMRX	ip, FPINST2		@ FPINST2 if needed (and present) \n\t"
                
                "3: \n\t"
                "@ IMPLEMENTATION DEFINED: save any subarchitecture defined state \n\t"
		"@ NOTE: Dont change the order of the FPEXC and CPACR restores \n\t"
                "stm	%0!, {r1, r2, r3, ip} \n\t"

		"@ Restore the original En bit of FPU. \n\t"
		"FMXR   FPEXC, r1          @ vmsr   FPEXC,r1           \n\t"
        
                "@ Restore the original CPACR value. \n\t"
                "2:     \n\t"
                "mcr    p15,0,r4,c1,c0,2   \n\t"

		:"+r"(container)
                :
                :"r1", "r2", "r3", "r4", "ip");

	return container;
}


/***********************/
/* .global restore_vfp */
/***********************/
void restore_vfp(int *container)
{
	__asm__ __volatile__ (
		".fpu neon \n\t"
		"@ FPU state save/restore. Obviously FPSID,MVFR0 and MVFR1 dont get \n\t"
		"@ serialized (RO). \n\t"
		"@ Modify CPACR to allow CP10 and CP11 access \n\t"
                "mrc    p15,0,r4,c1,c0,2 \n\t"
                "ORR    r2,r4,#0x00F00000  \n\t"
		"mcr    p15,0,r2,c1,c0,2 \n\t"
                "isb    \n\t"

		"@ Enable FPU access to save/restore the rest of registers. \n\t"
		"FMRX   r1,FPEXC          @ vmrs   r1,FPEXC \n\t"

		"ldr    r2,=0x40000000 \n\t"
                "orr    r2, r1 \n\t"
		"FMXR   FPEXC, r2        @ vmsr   FPEXC, r2 \n\t"

		"@ Restore the VFP-D16 registers. \n\t"
		"vldm   %0!, {D0-D15} \n\t"

		"@ Check for Advanced SIMD/VFP-D32 support \n\t"
		"FMRX   r2, MVFR0        @ vmrs   r2, MVFR0 \n\t"
		"and    r2,r2,#0xF       @ extract the A_SIMD bitfield \n\t"
		"cmp    r2, #0x2 \n\t"
        
		"@ Store the Advanced SIMD/VFP-D32 additional registers. \n\t"
		"vldmiaeq    %0!, {D16-D31} \n\t"
                "addne  %0, %0, #128 \n\t"

                "ldm	%0, {r1, r2, r3, ip} \n\t"

                "tst	r1, #0x80000000   @#FPEXC_EX  \n\t"
                "beq	3f \n\t"
                "FMXR	FPINST, r3	   @ FPINST (only if FPEXC.EX is set) \n\t"
                
                "tst	r1, #0x10000000  @ FPEXC_FP2V, is there an FPINST2 to read? \n\t"
                "beq	3f \n\t"
                "FMXR	FPINST2, ip		@ FPINST2 if needed (and present) \n\t"
                
                "3: \n\t"
                
                "@ Restore configuration registers and enable. \n\t"
                "@ Restore FPSCR _before_ FPEXC since FPEXC could disable FPU \n\t"
                "@ and make setting FPSCR unpredictable. \n\t"
		"FMXR    FPSCR,r2       @ vmsr    FPSCR,r2 \n\t"

		"FMXR    FPEXC,r1        @ vmsr    FPEXC,r1                 \n\t"

                "mcr     p15,0,r4,c1,c0,2 \n\t"

		:
		:"r"(container)
                :"r1", "r2", "r3", "r4", "ip" );

	return;
}


/*************************************/
/* .global save_performance_monitors */
/*************************************/
unsigned *save_pmu_context(unsigned *container)
{
	__asm__ __volatile__ (
		"mrc    p15,0,r8,c9,c12,0    @ PMon: Control Register \n\t"
		"bic    r1,r8,#1 \n\t"
		"mcr    p15,0,r1,c9,c12,0    @ disable counter updates from here \n\t"
		"isb                         @ 0b0 => PMCR<0> \n\t"
		"mrc    p15,0,r9,c9,c12,5    @ PMon: Event Counter Selection Register \n\t"
		"mrc    p15,0,r10,c9,c12,1   @ PMon: Count Enable Set Reg \n\t"
		"stm    %0!, {r8-r10} \n\t"
		"mrc    p15,0,r8,c9,c12,2    @ PMon: Count Enable Clear Register \n\t"
		"mrc    p15,0,r9,c9,c13,0    @ PMon: Cycle Counter Register \n\t"
		"mrc    p15,0,r10,c9,c12,3   @ PMon: Overflow flag Status Register \n\t"
		"stm    %0!, {r8-r10} \n\t"
		"mrc    p15,0,r8,c9,c14,1    @ PMon: Interrupt Enable Set Registern \n\t"
		"mrc    p15,0,r9,c9,c14,2    @ PMon: Interrupt Enable Clear Register \n\t"
		"stm    %0!, {r8-r9} \n\t"
		"mrc    p15,0,r8,c9,c12,0    @ Read PMon Control Register \n\t"
		"ubfx   r9,r8,#11,#5         @ extract # of event counters, N \n\t"
		"tst    r9, r9 \n\t"
		"beq    1f \n\t"
        
		"mov    r8,#0 \n\t"
		"0:         \n\t"
		"mcr    p15,0,r8,c9,c12,5    @ PMon: select CounterN \n\t"
		"isb \n\t"
		"mrc    p15,0,r3,c9,c13,1    @ PMon: save Event Type Register \n\t"
		"mrc    p15,0,r4,c9,c13,2    @ PMon: save Event Counter Register \n\t"
		"stm    %0!, {r3,r4} \n\t"
		"add    r8,r8,#1             @ increment index \n\t"
		"cmp    r8,r9 \n\t"
		"bne    0b \n\t"
		"1: \n\t"
		: "+r"(container)
		:
		:"r1", "r3", "r4", "r8", "9", "r10");

	return container;
}

/****************************************/
/* .global restore_performance_monitors */
/****************************************/
void restore_pmu_context(int *container)
{
	__asm__ __volatile__ (
		"@ NOTE: all counters disabled by PMCR<0> == 0 on reset \n\t"
        
		"ldr    r8,[%0]                  @ r8 = PMCR \n\t"
		"add    r1,%0,#20                @ r1 now points to saved PMOVSR \n\t"
		"ldr    r9,[r1]                  @ r9 = PMOVSR \n\t"
        
		"mvn    r2,#0                    @ generate Register of all 1s \n\t"
		"mcr    p15,0,r2,c9,c14,2        @ disable all counter related interrupts \n\t"
		"mcr    p15,0,r2,c9,c12,3        @ clear all overflow flags \n\t"
		"isb \n\t"
        
		"ubfx   r12,r8,#11,#5            @ extract # of event counters, N (0-31) \n\t"
		"tst    r12, r12 \n\t"
		"beq    20f \n\t"
        
		"add    r1,%0,#32                @ r1 now points to the 1st saved event counter \n\t"

		"@@ Restore counters \n\t"
		"mov    r6,#0 \n\t"
		"10:     \n\t"
		"mcr    p15,0,r6,c9,c12,5        @ PMon: select CounterN \n\t"
		"isb \n\t"
		"ldm    r1!, {r3,r4}             @ Read saved data \n\t"
		"mcr    p15,0,r3,c9,c13,1        @ PMon: restore Event Type Register \n\t"
		"mcr    p15,0,r4,c9,c13,2        @ PMon: restore Event Counter Register \n\t"
		"add    r6,r6,#1                 @ increment index \n\t"
		"cmp    r6,r12 \n\t"
		"bne    10b \n\t"
        
		"20:     \n\t"
		"tst    r9, #0x80000000          @ check for cycle count overflow flag \n\t"
		"beq    40f \n\t"
		"mcr    p15,0,r2,c9,c13,0        @ set Cycle Counter to all 1s \n\t"
		"isb \n\t"
		"mov    r3, #1 \n\t"
		"mcr    p15,0,r3,c9,c12,0        @ set the PMCR global enable bit \n\t"
		"mov    r3, #0x80000000 \n\t"
		"mcr    p15,0,r3,c9,c12,1        @ enable the Cycle Counter \n\t"
		"isb \n\t"
        
		"30:     \n\t"
		"mrc    p15,0,r4,c9,c12,3        @ check cycle count overflow now set \n\t"
		"movs   r4,r4                    @ test bit<31> \n\t"
		"bpl    30b \n\t"
		"mcr    p15,0,r3,c9,c12,2        @ disable the Cycle Counter \n\t"
        
		"40:     \n\t"
		"mov    r1, #0 \n\t"
		"mcr    p15,0,r1,c9,c12,0        @ clear the PMCR global enable bit \n\t"
		"isb \n\t"
        
		"@@ Restore left regs but PMCR \n\t"
		"add    r1,%0,#4                 @ r1 now points to the PMSELR \n\t"
		"ldm    r1!,{r3,r4} \n\t"
		"mcr    p15,0,r3,c9,c12,5        @ PMon: Event Counter Selection Reg \n\t"
		"mcr    p15,0,r4,c9,c12,1        @ PMon: Count Enable Set Reg \n\t"
		"ldm    r1!, {r3,r4} \n\t"
		"mcr    p15,0,r4,c9,c13,0        @ PMon: Cycle Counter Register \n\t"
		"ldm    r1!,{r3,r4} \n\t"
		"mcr    p15,0,r3,c9,c14,2        @ PMon: Interrupt Enable Clear Reg \n\t"
		"mcr    p15,0,r4,c9,c14,1        @ PMon: Interrupt Enable Set Reg \n\t"
		"ldr    r3,[r1] \n\t"
		"isb \n\t"
		"ldr    %0,[%0] \n\t"
		"mcr    p15,0,%0,c9,c12,0        @ restore the PM Control Register \n\t"
		"isb \n\t"
		:
		: "r"(container)
		: "r1", "r2", "r3", "r4", "r5", "r6", "r8", "r9", "r10", "r12", "lr");

	return;
}

/***********************************************************************************/
/* @ If r1 is 0, we assume that the OS is not using the Virtualization extensions, */
/* @ and that the warm boot code will set up CNTHCTL correctly. If r1 is non-zero  */
/* @ then CNTHCTL is saved and restored						   */
/* @ CNTP_CVAL will be preserved as it is in the always-on domain.		   */
/***********************************************************************************/

unsigned *mt_save_generic_timer(unsigned int *container, int sw)
{
	__asm__ __volatile__ (
		" cmp    %1, #0  		  @ non-secure? \n\t"
		/* " mrcne  p15, 4, %1, c14, c1, 0        @ read CNTHCTL \n\t" */
		/* " strne  %1, [%0], #4 \n\t" */
		" mrc    p15,0,r2,c14,c2,1        @ read CNTP_CTL \n\t"
		" mrc    p15,0,r3,c14,c2,0        @ read CNTP_TVAL \n\t"
		" mrc    p15,0,r12,c14,c1,0       @ read CNTKCTL \n\t"
		" stm    %0!, {r2, r3, r12}  \n\t"
		: "+r"(container)
		: "r"(sw)
		:"r2", "r3", "r12");

	return container;
}

void mt_restore_generic_timer(int *container, int sw)
{
	__asm__ __volatile__ (
		" cmp    %1, #0 \n\t"
		/* " ldrne  %1, [r0], #4 \n\t" */
		/* " mcrne  p15, 4, %1, c14, c1, 0        @ write CNTHCTL \n\t" */
		" ldm    %0!, {r2, r3, r12} \n\t"
		" mcr    p15,0,r3,c14,c2,0        @ write CNTP_TVAL \n\t"
		" mcr    p15,0,r12,c14,c1,0       @ write CNTKCTL \n\t"
		" mcr    p15,0,r2,c14,c2,1        @ write CNTP_CTL \n\t"
		:
		:"r"(container), "r"(sw)
		:"r2", "r3", "r12");
	
	return;
}

void stop_generic_timer(void)
{
	/*
	 * Disable the timer and mask the irq to prevent
	 * suprious interrupts on this cpu interface. It
	 * will bite us when we come back if we don't. It
	 * will be replayed on the inbound cluster.
	 */
	write_cntpctl(read_cntpctl() & ~1);
	return;
}

void start_generic_timer(void)
{
	write_cntpctl(read_cntpctl() | 1);
	return;
}


struct set_and_clear_regs
{
	volatile unsigned int set[32], clear[32];
};

typedef struct
{
	volatile unsigned int control;                      /* 0x000 */
	const unsigned int controller_type;
	const unsigned int implementer;
	const char padding1[116];
	volatile unsigned int security[32];                 /* 0x080 */
	struct set_and_clear_regs enable;                   /* 0x100 */
	struct set_and_clear_regs pending;                  /* 0x200 */
	struct set_and_clear_regs active;                   /* 0x300 */
	volatile unsigned int priority[256];                /* 0x400 */
	volatile unsigned int target[256];                  /* 0x800 */
	volatile unsigned int configuration[64];            /* 0xC00 */
	const char padding3[256];                           /* 0xD00 */
	volatile unsigned int non_security_access_control[64];       /* 0xE00 */
	volatile unsigned int software_interrupt;           /* 0xF00 */
	volatile unsigned int sgi_clr_pending[4];           /* 0xF10 */
	volatile unsigned int sgi_set_pending[4];           /* 0xF20 */
	const char padding4[176];
	unsigned const int peripheral_id[4];                /* 0xFE0 */
	unsigned const int primecell_id[4];                 /* 0xFF0 */
} interrupt_distributor;


typedef struct
{
	volatile unsigned int control;                      /* 0x00 */
	volatile unsigned int priority_mask;                /* 0x04 */
	volatile unsigned int binary_point;                 /* 0x08 */
	volatile unsigned const int interrupt_ack;          /* 0x0c */
	volatile unsigned int end_of_interrupt;             /* 0x10 */
	volatile unsigned const int running_priority;       /* 0x14 */
	volatile unsigned const int highest_pending;        /* 0x18 */
	volatile unsigned int aliased_binary_point;         /* 0x1c */
	volatile unsigned const int aliased_interrupt_ack;  /* 0x20 */
	volatile unsigned int alias_end_of_interrupt;       /* 0x24 */
	volatile unsigned int aliased_highest_pending;      /* 0x28 */
} cpu_interface;

extern unsigned *copy_words(volatile unsigned *destination, volatile unsigned *source, unsigned num_words);

typedef struct ns_gic_cpu_context {
	unsigned int gic_cpu_if_regs[32];   /* GIC context local to the CPU */
	unsigned int gic_dist_if_pvt_regs[32];  /* GIC SGI/PPI context local to the CPU */
	unsigned int gic_dist_if_regs[512]; /* GIC distributor context to be saved by the last cpu. */
} gic_cpu_context;

gic_cpu_context gic_data[1];
#define gic_data_base() ((gic_cpu_context *)&gic_data[0])

/*
 * Saves the GIC CPU interface context
 * Requires 3 words of memory
 */
static void save_gic_interface(u32 *pointer, unsigned gic_interface_address)
{
	cpu_interface *ci = (cpu_interface *)gic_interface_address;

	pointer[0] = ci->control;
	pointer[1] = ci->priority_mask;
	pointer[2] = ci->binary_point;
	pointer[3] = ci->aliased_binary_point;

	pointer[4] = ci->aliased_highest_pending;

	/* TODO: add nonsecure stuff */

}

/*
 * Saves this CPU's banked parts of the distributor
 * Returns non-zero if an SGI/PPI interrupt is pending (after saving all required context)
 * Requires 19 words of memory
 */
static void save_gic_distributor_private(u32 *pointer, unsigned gic_distributor_address)
{
	interrupt_distributor *id = (interrupt_distributor *)gic_distributor_address;
	unsigned int *ptr = 0x0;

	/*  Save SGI,PPI enable status*/
	*pointer = id->enable.set[0];
	++pointer;
	/*  Save SGI,PPI priority status*/
	pointer = copy_words(pointer, id->priority, 8);
	/*  Save SGI,PPI target status*/
	pointer = copy_words(pointer, id->target, 8);
	/*  Save just the PPI configurations (SGIs are not configurable) */
	*pointer = id->configuration[1];
	++pointer;
	/*  Save SGI,PPI security status*/
	*pointer = id->security[0];
	++pointer;

	/*  Save SGI Non-security status (PPI is read-only)*/
	*pointer = id->non_security_access_control[0] & 0x0ffff;
	++pointer;
#if 0
	/*
	 * Private peripheral interrupts need to be replayed on
	 * the destination cpu interface for consistency. This
	 * is the responsibility of the peripheral driver. When
	 * it sees a pending interrupt while saving its context
	 * it should record enough information to recreate the
	 * interrupt while restoring.
	 * We don't save the Pending/Active status and clear it
	 * so that it does not interfere when we are back.
	 */
	/*  Clear PPI pending status*/
	id->pending.clear[0] = 0xffffffff;
	id->active.clear[0] = 0xffffffff;
#endif
#if 1 
	/*  Save SGI,PPI pending status*/
	*pointer = id->pending.set[0];
	++pointer;
#endif
	/*
	 * IPIs are different and can be replayed just by saving
	 * and restoring the set/clear pending registers
	 */
	ptr = pointer;
	copy_words(pointer, id->sgi_set_pending, 4);
	pointer += 8; 

	/*
	 * Clear the pending SGIs on this cpuif so that they don't
	 * interfere with the wfi later on.
	 */
	copy_words(id->sgi_clr_pending, ptr, 4);
    
}

/*
 * Saves the shared parts of the distributor
 * Requires 1 word of memory, plus 20 words for each block of 32 SPIs (max 641 words)
 * Returns non-zero if an SPI interrupt is pending (after saving all required context)
 */
static void save_gic_distributor_shared(u32 *pointer, unsigned gic_distributor_address)
{
	interrupt_distributor *id = (interrupt_distributor *)gic_distributor_address;
	unsigned num_spis, *saved_pending;

	/* Calculate how many SPIs the GIC supports */
	num_spis = 32 * (id->controller_type & 0x1f);

	/* TODO: add nonsecure stuff */

	/* Save rest of GIC configuration */
	if (num_spis) {
		pointer = copy_words(pointer, id->enable.set + 1, num_spis / 32);
		pointer = copy_words(pointer, id->priority + 8, num_spis / 4);
		pointer = copy_words(pointer, id->target + 8, num_spis / 4);
		pointer = copy_words(pointer, id->configuration + 2, num_spis / 16);
		pointer = copy_words(pointer, id->security + 1, num_spis / 32);
		saved_pending = pointer;
		pointer = copy_words(pointer, id->pending.set + 1, num_spis / 32);

		pointer = copy_words(pointer, id->non_security_access_control+1, num_spis / 16);
	}

	/* Save control register */
	*pointer = id->control;
}
static void restore_gic_interface(u32 *pointer, unsigned gic_interface_address)
{
	cpu_interface *ci = (cpu_interface *)gic_interface_address;

	/* TODO: add nonsecure stuff */

	ci->priority_mask = pointer[1];
	ci->binary_point = pointer[2];
	ci->aliased_binary_point = pointer[3];

	ci->aliased_highest_pending = pointer[4];

	/* Restore control register last */
	ci->control = pointer[0];
}
static void restore_gic_distributor_private(u32 *pointer, unsigned gic_distributor_address)
{
	interrupt_distributor *id = (interrupt_distributor *)gic_distributor_address;
	unsigned tmp;
	//unsigned ctr, prev_val = 0, prev_ctr = 0;

	/* First disable the distributor so we can write to its config registers */
	tmp = id->control;
	id->control = 0;
	/* Restore SGI,PPI enable status*/
	id->enable.set[0] = *pointer;
	++pointer;
	/* Restore SGI,PPI priority  status*/
	copy_words(id->priority, pointer, 8);
	pointer += 8;
	/* Restore SGI,PPI target status*/
	copy_words(id->target, pointer, 8);
	pointer += 8;
	/* Restore just the PPI configurations (SGIs are not configurable) */
	id->configuration[1] = *pointer;
	++pointer;
	/* Restore SGI,PPI security status*/
	id->security[0] = *pointer;
	++pointer;

	/* restore SGI Non-security status (PPI is read-only)*/
	id->non_security_access_control[0] = 
                (id->non_security_access_control[0] & 0x0ffff0000) | (*pointer) ;
	++pointer;

#if 0
	/*
	 * Clear active and  pending PPIs as they will be recreated by the
	 * peripiherals
	 */
	id->active.clear[0] = 0xffffffff;
	id->pending.clear[0] = 0xffffffff;
#endif
#if 1
	/*  Restore SGI,PPI pending status*/
	id->pending.set[0] = *pointer;
	++pointer;
#endif
	/*
	 * Restore pending SGIs
	 */
	copy_words(id->sgi_set_pending, pointer, 4);
	pointer += 4;

	id->control = tmp;
}


static void restore_gic_spm_irq(unsigned gic_distributor_address)
{
	interrupt_distributor *id = (interrupt_distributor *)gic_distributor_address;
	unsigned int backup;
	int i, j;

	/* First disable the distributor so we can write to its config registers */
        
	backup = id->control;
        id->control = 0;

        
        /* Set the pending bit for spm wakeup source that is edge triggerd */ 
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_LTE_PTP) {
		// i = KP_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                // j = KP_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                // id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_KP) {
                i = KP_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                j = KP_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_CONN_WDT) {
                // i = KP_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                // j = KP_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                // id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_LOW_BAT) {
                i = LOWBATTERY_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                j = LOWBATTERY_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_USB_CD) {
                // i = USB_CD_BIT_ID / GIC_PRIVATE_SIGNALS;
                // j = USB_CD_BIT_ID % GIC_PRIVATE_SIGNALS;
                // id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_MD_WDT) {
                i = MD_WDT_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                j = MD_WDT_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_C2K_WDT) {
                // i = MD_WDT_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                // j = MD_WDT_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                // id->pending.set[i] |= (1 << j);
        }
        if (reg_read(SPM_SLEEP_ISR_RAW_STA) & WAKE_SRC_CLDMA_MD) {
                // i = WDT_IRQ_BIT_ID / GIC_PRIVATE_SIGNALS;
                // j = WDT_IRQ_BIT_ID % GIC_PRIVATE_SIGNALS;
                // id->pending.set[i] |= (1 << j);
        }


	/* We assume the I and F bits are set in the CPSR so that we will not respond to interrupts! */
	/* Restore control register */
	id->control = backup;
}


static void restore_gic_distributor_shared(u32 *pointer, unsigned gic_distributor_address)
{
	interrupt_distributor *id = (interrupt_distributor *)gic_distributor_address;
	unsigned num_spis;

	/* First disable the distributor so we can write to its config registers */
	id->control = 0;

	/* Calculate how many SPIs the GIC supports */
	num_spis = 32 * ((id->controller_type) & 0x1f);

	/* TODO: add nonsecure stuff */

	/* Restore rest of GIC configuration */
	if (num_spis) {
		copy_words(id->enable.set + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;
		copy_words(id->priority + 8, pointer, num_spis / 4);
		pointer += num_spis / 4;
		copy_words(id->target + 8, pointer, num_spis / 4);
		pointer += num_spis / 4;
		copy_words(id->configuration + 2, pointer, num_spis / 16);
		pointer += num_spis / 16;
		copy_words(id->security + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;
		copy_words(id->pending.set + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;

		copy_words(id->non_security_access_control + 1, pointer, num_spis / 16);
		pointer += num_spis / 16;


                restore_gic_spm_irq(gic_distributor_address);

	}

	/* We assume the I and F bits are set in the CPSR so that we will not respond to interrupts! */
	/* Restore control register */
	id->control = *pointer;
}


static void gic_cpu_save(void)
{
	save_gic_interface(gic_data_base()->gic_cpu_if_regs, DMT_GIC_CPU_BASE);
	/*
	 * TODO:
	 * Is it safe for the secondary cpu to save its context
	 * while the GIC distributor is on. Should be as its
	 * banked context and the cpu itself is the only one
	 * who can change it. Still have to consider cases e.g
	 * SGIs/Localtimers becoming pending.
	 */
	/* Save distributoer interface private context */
	save_gic_distributor_private(gic_data_base()->gic_dist_if_pvt_regs, DMT_GIC_DIST_BASE);
}

static void gic_dist_save(void)
{
	/* Save distributoer interface global context */
	save_gic_distributor_shared(gic_data_base()->gic_dist_if_regs, DMT_GIC_DIST_BASE); 
}

static void gic_dist_restore(void)
{
	/*restores the global context  */
	restore_gic_distributor_shared(gic_data_base()->gic_dist_if_regs, DMT_GIC_DIST_BASE);
}

void gic_cpu_restore(void)
{
	/*restores the private context  */
	restore_gic_distributor_private(gic_data_base()->gic_dist_if_pvt_regs, DMT_GIC_DIST_BASE);
	/* Restore GIC context */
	restore_gic_interface(gic_data_base()->gic_cpu_if_regs, DMT_GIC_CPU_BASE);
}


static inline void biu_reconfig(void)
{
	int val;
	val = reg_read(BIU_CONTROL);
	val = _or(val, TLB_ULTRA_EN);
	val = _or(val, DCM_EN);
	val = _or(val, CMD_QUEUE_EN);
	reg_write(BIU_CONTROL, val);
}


static inline void mp0_l2rstdisable(int flags) {} 
static inline void mp1_l2rstdisable(int flags) {}
static inline void mp0_l2rstdisable_restore(int flags) {} 
static inline void mp1_l2rstdisable_restore(int flags) {}


/* cluster_save_context: */
static void mt_cluster_save(int flags)
{
	/***************************************/
        /* int cpuid, clusterid;	       */
	/* read_id(&cpuid, &clusterid);	       */
	/* BUG_ON(cpuid != 0);		       */
        /***************************************/

	if (cluster_id() == 0) {
		mp0_l2rstdisable(flags);
	} 
        else {
		mp1_l2rstdisable(flags);
	}
}

/* cluster_save_context: */
static void mt_cluster_restore(int flags)
{
	int cpuid, clusterid;


	/*************************************/
        /* if (flag != SHUTDOWN_MODE)	     */
	/* 	return;			     */
        /*************************************/

	read_id(&cpuid, &clusterid);


	if (cluster_id() == 0) {
		mp0_l2rstdisable_restore(flags);
	} 
        else {
		mp1_l2rstdisable_restore(flags);
	}
	
	biu_reconfig();
}

__weak unsigned int *mt_save_dbg_regs(unsigned int *p, unsigned int cpuid)
{
        return p;
}

__weak void mt_restore_dbg_regs(unsigned int *p, unsigned int cpuid)
{
        return;
}

__weak void mt_copy_dbg_regs(int to, int from)
{
        return;
}

void mt_cpu_save(void)
{	
	core_context *core;
        cluster_context *cluster;
	unsigned int *ret;
        unsigned long dbg_base;
        unsigned int sleep_sta;
        int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	core = GET_CORE_DATA();

	ret = mt_save_generic_timer(core->timer_data, 0x0); 
	stop_generic_timer(); //disable timer irq, and upper layer should enable again.
	
	SENTINEL_CHECK(core->timer_data, ret);

	ret = save_pmu_context(core->pmu_data);
	SENTINEL_CHECK(core->pmu_data, ret);


	ret = save_vfp(core->vfp_data);
	SENTINEL_CHECK(core->vfp_data, ret);

	/** FIXME,
         * To restore preious backup context makeing MCDIed core geting inconsistent.
         * But to do a copy is not 100% right for Multi-core debuging or non-attached cores, 
         * which need to backup/restore itself.
         * However, SW is not able to aware of this 2 conditions.
         *
         * Right now, copy is prefered for internal debug usage.
         * And, save/restore is by cluster.
         **/
        if (clusterid==0) {
                sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 16) & 0x0f;
                dbg_base = DMT_MP0_DBGAPB_BASE;
        } else { 
                sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 20) & 0x0f;
                dbg_base = DMT_MP1_DBGAPB_BASE;
        }
         
        if ((sleep_sta | (1<<cpuid)) == 0x0f) { // last core
                cluster = GET_CLUSTER_DATA();
                ret = mt_save_dbg_regs(cluster->dbg_data, cpuid + (clusterid*4));
                SENTINEL_CHECK(cluster->dbg_data, ret);
        }
        else {
                /** do nothing **/
        }

        ret = mt_save_banked_registers(core->banked_regs);
	SENTINEL_CHECK(core->banked_regs, ret);
}

void mt_cpu_restore(void)
{
	core_context *core;
        cluster_context *cluster;
        unsigned long dbg_base;
        unsigned int sleep_sta;
        int cpuid, clusterid;

	read_id(&cpuid, &clusterid);
	

	core = GET_CORE_DATA();

        mt_restore_banked_registers(core->banked_regs);

	/** FIXME,
         * To restore preious backup context makeing MCDIed core geting inconsistent.
         * But to do a copy is not 100% right for Multi-core debuging or non-attached cores, 
         * which need to backup/restore itself.
         * However, SW is not able to aware of this 2 conditions.
         *
         * Right now, copy is prefered for internal debug usage.
         * And, save/restore is by cluster.
         **/
        if (clusterid == 0) {
                sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 16) & 0x0f;
                dbg_base = DMT_MP0_DBGAPB_BASE;
        } else { 
                sleep_sta = (spm_read(SPM_SLEEP_TIMER_STA) >> 20) & 0x0f;
                dbg_base = DMT_MP1_DBGAPB_BASE;
        }
         
        sleep_sta = (sleep_sta | (1<<cpuid)); 

        if (sleep_sta == 0x0f) { // first core
                cluster = GET_CLUSTER_DATA();
                mt_restore_dbg_regs(cluster->dbg_data, cpuid + (clusterid*4));
        }
        else {  //otherwise, do copy from anyone
                int any = __builtin_ffs(~sleep_sta) -1;
                mt_copy_dbg_regs(cpuid + (clusterid*4), any + (clusterid*4));
        }

        
	restore_vfp(core->vfp_data);
	
	restore_pmu_context(core->pmu_data);

	mt_restore_generic_timer(core->timer_data, 0x0);

}

void mt_platform_save_context(int flags)
{
	/* mcusys_save_context: */
	mt_cpu_save();
	mt_cluster_save(flags);

	if (IS_DORMANT_GIC_OFF(flags)) {
		//mt_gic_save_contex;
		gic_cpu_save();
		gic_dist_save();
	}

	/* infrasys_save_context: */
	/* misc_save_context; */

}


void mt_platform_restore_context(int flags)
{
	/* misc_restore_context: */
	/* infrasys_restore_context: */

	/* mcusys_restore_context: */
	mt_cluster_restore(flags);
	mt_cpu_restore();

	if (IS_DORMANT_GIC_OFF(flags)) {
		gic_dist_restore();
		gic_cpu_restore();
	}
}



#define _get_sp() ({ register void *SP asm("sp"); SP; })


int mt_cpu_dormant_psci(unsigned long flags)
{
        int ret = 1;
        struct psci_power_state pps = {
                .type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
                .affinity_level = 2,
        };

	int cpuid, clusterid;
	read_id(&cpuid, &clusterid);

        if (psci_ops.cpu_suspend) {

                PCLOG(clusterid*4 + cpuid);
                ret = psci_ops.cpu_suspend(pps, virt_to_phys(cpu_resume));
                PCLOG(clusterid*4 + cpuid);

                if (!ret) { //fixme
                        ret = 1;
                }
        }

        return ret;
}


static int mt_cpu_dormant_abort(unsigned long flags)
{
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);


	/* restore l2rstdisable setting */
	if (cluster_id() == 0) {
		mp0_l2rstdisable_restore(flags);
	} 
        else {
		mp1_l2rstdisable_restore(flags);
	}

	biu_reconfig();

	// enable generic timer
	start_generic_timer();

        /* // unlock dbg oslock/dlock	     */
	/* write_dbgoslar(0);		     */
        /* isb(); */
	/* write_dbgosdlr(0);		     */
		
	return 0; 
}

/***************************************************************************/
/* /\*********************************					   */
/*  * cpuidle support 							   */
/*  **********************************\/				   */
/* static DEFINE_PER_CPU(struct cpuidle_device, mt_cpuidle_device);	   */
/* 									   */
/* static int mt_cpu_idle(struct cpuidle_device *dev,			   */
/* 		       struct cpuidle_driver *drv,			   */
/* 		       int index);					   */
/***************************************************************************/
#if !defined (__KERNEL__)
int cpu_pm_resume(void)
{
	return 0;
}

int cpu_pm_suspend(void)
{
	return 0;

}
#else //#if !defined (__KERNEL__)
int cpu_pm_resume(void) {  /* cpu_pm_exit() */;  return 0; }
int cpu_pm_suspend(void) {  /* cpu_pm_enter() */;  return 0; }

#endif //#if !defined (__KERNEL__)

#define NOMMU(a) (((a) & ~0xf0000000) | 0x10000000)
#define get_data_nommu(va)  \
        ({                                              \
                register int data;                      \
                register unsigned long pva = (unsigned long)(void *)(&(va)); \
                __asm__ __volatile__ (                  \
                        "adr r1, 1f 	\n\t"           \
                        "ldr r2, [r1]   \n\t"           \
                        "sub r2, %1, r2 \n\t"           \
                        "add r3, r1, r2	\n\t"           \
                        "ldr %0, [r3]  	\n\t"           \
                        "b 3f \n\t"                     \
                        "1: .long . \n\t"               \
                        "3: \n\t"                       \
                        :"=r"(data)                     \
                        : "r"(pva)                      \
                        : "r1", "r2", "r3");            \
                data;                                   \
        })


int mt_cpu_dormant(unsigned long flags)
{
	int ret;
	int cpuid, clusterid;
        static unsigned int dormant_count;
	core_context *core = GET_CORE_DATA();

#if defined(MT_DORMANT_UT)
	extern int mt_irq_mask_all(struct mtk_irq_mask *mask);	
	extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
	struct mtk_irq_mask mask;
#endif //#if defined(MT_DORMANT_UT)

        if (mt_dormant_initialized == 0)
                return MT_CPU_DORMANT_BYPASS;

	read_id(&cpuid, &clusterid);

        PCLOG(clusterid*4 + cpuid);

        dormant_count++;
#if defined(TSLOG_ENABLE)
        if ((core->count & 0x01ff) == 0) 
                CPU_DORMANT_INFO("dormant(%d) flags:%x start (cluster/cpu:%d/%d) !\n", 
                                 dormant_count, flags, clusterid, cpuid);
#endif

	// debug purpose, just bypass
	if (DEBUG_DORMANT_BYPASS || debug_dormant_bypass == 1)
		return MT_CPU_DORMANT_BYPASS;

        TSLOG(core->timestamp[0], read_cntpct());
        core->count++;

	BUG_ON(!irqs_disabled());

	// for cpu_pm callback
	if (cpu_pm_suspend()) {
		ret = MT_CPU_DORMANT_BREAK_V(CPU_PM_BREAK);
		goto _back;
	}

	// dormant break
	if (IS_DORMANT_BREAK_CHECK(flags) && 
	    SPM_IS_CPU_IRQ_OCCUR(SPM_CORE_ID())) {
		ret = MT_CPU_DORMANT_BREAK_V(IRQ_PENDING_1);
		goto _back;
	}

        PCLOG(clusterid*4 + cpuid);

#if defined(MT_DORMANT_UT)
	mt_irq_mask_all(&mask);
#endif //#if defined(MT_DORMANT_UT)

	//
	mt_platform_save_context(flags);

        PCLOG(clusterid*4 + cpuid);

	// dormant break
	if (IS_DORMANT_BREAK_CHECK(flags) && 
	    SPM_IS_CPU_IRQ_OCCUR(SPM_CORE_ID())) {
		mt_cpu_dormant_abort(flags);
		ret = MT_CPU_DORMANT_BREAK_V(IRQ_PENDING_2);
		goto _back;
	}

	ret = cpu_suspend(flags, mt_cpu_dormant_psci);

        PCLOG(clusterid*4 + cpuid);

        TSLOG(core->timestamp[3], read_cntpct());
		
	switch (ret) {
	case 0: // back from dormant reset
		mt_platform_restore_context(flags);
#ifdef CONFIG_MTK_ETM
                trace_start_dormant();
#endif    
                core->rst++;
		ret = MT_CPU_DORMANT_RESET;
		break;

	case 1: // back from dormant abort,
		mt_cpu_dormant_abort(flags);
                core->abt++;
		ret = MT_CPU_DORMANT_ABORT;
		break;
		
	default: // back from dormant break, do nothing for return
		zlog("EOPNOTSUPP \n");
	case 2:
		mt_cpu_dormant_abort(flags);
                core->brk++;
		ret = MT_CPU_DORMANT_BREAK_V(IRQ_PENDING_3);
		break;
	}

        PCLOG(clusterid*4 + cpuid);

        cpu_pm_resume();
        local_fiq_enable();  /** cpu mask F-bit at reset, but nobody clear that **/

#if defined(MT_DORMANT_UT)
	mt_irq_mask_restore(&mask);
#endif //#if defined(MT_DORMANT_UT)
	
_back:

        PCLOG(clusterid*4 + cpuid);

        TSLOG(core->timestamp[4], read_cntpct());

#if defined(TSLOG_ENABLE)
        if (MT_CPU_DORMANT_BREAK & ret)
                CPU_DORMANT_INFO("dormant BREAK(%x) !! \n\t", ret);
        if (MT_CPU_DORMANT_ABORT & ret)
                CPU_DORMANT_INFO("dormant ABORT(%x) !! \n\t", ret);

        CPU_DORMANT_INFO("dormant(flags:%x) (ret:%x) (core:%d/%d) cnt:%d, rst:%d, abt:%d, brk:%d\n",
			 flags, ret, clusterid, cpuid, 
                                 core->count, core->rst, core->abt, core->brk);
	CPU_DORMANT_INFO("dormant timing: %llu, %llu, %llu, %llu, %llu\n",
                                 core->timestamp[0],
                                 core->timestamp[1],
                                 core->timestamp[2],
                                 core->timestamp[3],
                                 core->timestamp[4]);
#endif

        PCLOG(clusterid*4 + cpuid);

	return ret & 0x0ff;

}

#if defined (CONFIG_OF)
static int mt_dormant_dts_map(void)
{
        struct device_node *node;

        /* dbgapb */
        node = of_find_compatible_node(NULL, NULL, DBGAPB_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " DBGAPB_NODE); 
                BUG();
        }
        dbgapb_base = (unsigned long) of_iomap(node, 0);
        if(!dbgapb_base) {
                CPU_DORMANT_INFO("error: cannot iomap " DBGAPB_NODE);
                BUG();
        }
        of_node_put(node);

        /* mcucfg */
        node = of_find_compatible_node(NULL, NULL, MCUCFG_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " MCUCFG_NODE); 
                BUG();
        }
        mcucfg_base = (unsigned long) of_iomap(node, 0);
        if(!mcucfg_base) {
                CPU_DORMANT_INFO("error: cannot iomap " MCUCFG_NODE);
                BUG();
        }
        of_node_put(node);
        
#if 0 // cci400
        /* cci400 */
        node = of_find_compatible_node(NULL, NULL, CCI400_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " CCI400_NODE); 
                BUG();
        }
        cci400_base = (unsigned long) of_iomap(node, 0);
        if(!cci400_base) {
                CPU_DORMANT_INFO("error: cannot iomap " CCI400_NODE);
                BUG();
        }
        of_node_put(node);
 #else // cci400
        /* BIU */
        node = of_find_compatible_node(NULL, NULL, BIU_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " BIU_NODE); 
                BUG();
        }
        biu_base = (unsigned long) of_iomap(node, 0);
        if(!biu_base) {
                CPU_DORMANT_INFO("error: cannot iomap " BIU_NODE);
                BUG();
        }
        of_node_put(node);
#endif // cci400

        /* infracfg_ao */
        node = of_find_compatible_node(NULL, NULL, INFRACFG_AO_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " INFRACFG_AO_NODE); 
                BUG();
        }
        infracfg_ao_base = (unsigned long) of_iomap(node, 0);
        if(!infracfg_ao_base) {
                CPU_DORMANT_INFO("error: cannot iomap " INFRACFG_AO_NODE);
                BUG();
        }
        of_node_put(node);

        /* gic */
        node = of_find_compatible_node(NULL, NULL, GIC_NODE);
        if (!node) 
        {
                CPU_DORMANT_INFO("error: cannot find node " GIC_NODE); 
                BUG();
        }
        gic_id_base = (unsigned long) of_iomap(node, 0);
        gic_ci_base = (unsigned long) of_iomap(node, 1);
        if(!gic_id_base || !gic_ci_base) {
                CPU_DORMANT_INFO("error: cannot iomap " GIC_NODE);
                BUG();
        }
        of_node_put(node);

        return 0;
}
#else //#if definded(CONFIG_OF)
static int mt_dormant_dts_map(void) { return 0; }
#endif //#if definded(CONFIG_OF)

int mt_cpu_dormant_init(void)
{
	int cpuid, clusterid;
	read_id(&cpuid, &clusterid);
        
        if (mt_dormant_initialized == 1)
                return 0;

	// map base address
	mt_dormant_dts_map();

#if CPU_DORMANT_AEE_RR_REC
        sleep_aee_rec_cpu_dormant_va = dormant_data[0].poc.cpu_dormant_aee_rr_rec = aee_rr_rec_cpu_dormant();
        sleep_aee_rec_cpu_dormant = virt_to_phys(sleep_aee_rec_cpu_dormant_va);

        CPU_DORMANT_INFO("dormant init aee_rec_cpu_dormant: va:%lx pa:%lx\n",
	     sleep_aee_rec_cpu_dormant_va, sleep_aee_rec_cpu_dormant);

        mt_dormant_initialized = 1;
#endif

#if defined (MT_DORMANT_UT)
        {
#include <asm/system_misc.h>
                int mt_cpu_dormant_test(void);
                arm_pm_idle = mt_cpu_dormant_test;
        }
#endif

	return 0;
}


// move to mt_pm_init to resolve dependency with others.
//late_initcall(mt_cpu_dormant_init);

#if defined (MT_DORMANT_UT)
volatile int mt_cpu_dormant_test_mode = (
	CPU_SUSPEND_MODE | 
	0
	);


int mt_cpu_dormant_test(void)
{
	return mt_cpu_dormant(mt_cpu_dormant_test_mode);
}

#endif //#if defined (MT_DORMANT_UT)

