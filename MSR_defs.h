//-------------------------------------------------
// MSR list for performance monitoring utility for Xeon E5 v3 (Haswell EP, 06_3F, Hikari/Wrangler/Lonestar5)
// Revision 0.2, 2017-03-02
// John D. McCalpin, mccalpin@tacc.utexas.edu
//-------------------------------------------------
// This is a shortened version of Xeon_E5_v3_Perf_MSRs.txt that just includes
// the MSR names and numbers in cpp #define format...
// Since I use the MSR numbers primarily in "pread()" and "pwrite()" calls,
// I will define all of these as signed long (64-bit) integers.
// https://www.gnu.org/software/libc/manual/html_node/File-Position-Primitive.html
//-------------------------------------------------
// Intel Arch SW Developer's Manual, Volume 3, document 325384-060, September 2016
//-------------------------------------------------
// Part 1: Architectural performance monitoring version 3, Volume 3B, section 18.2 and Section 35.1
//-------------------------------------------------
#include "MSR_ArchPerfMon_v3.h"

// -----------------------------------------------------------------
// Part 2: Performance-related MSRs from "Architectural MSRs" 
//   (Volume 3B, Table 35-2) excludes those listed above in
//   "Architectural Performance Monitoring"
//
#include "MSR_Architectural.h"

// -----------------------------------------------------------------
// Part 3: MSRs from Volume 3B, Tables 34-5 & 34-6 for Nehalem & Westmere, plus Table 34-8 for Xeon 5600
// Skipped here....

// -----------------------------------------------------------------
// Part 4: MSRs from Volume 3B, Table 34-10, Sandy Bridge 06_2Ah and 06_2Dh
// All Architectural MSRs are included (including Architectural Perf Monitoring, v3)
//   06_2Dh is Xeon E5 (Stampede)
//   06_2Ah is Xeon E3 (Scorpion)
// Stampede:
//   DisplayFamily_DisplayModel 06_2Dh
//   Family 06, ExtendedFamily 00, Model 13, ExtendedModel 02
//   cpuinfo model 45 (decimal)

// Update 2012-10-02: Downloaded new revision of Volume 3C (326019-044, August 2012)
// and reviewed Sandy Bridge MSRs in Section 35.7, Table 35-11

// Note: IA32_PERF_STATUS and MSR_PERF_STATUS are both 0x198, but the former only includes P-state, while the latter includes both P-state and voltage info
// Note: All versions of Volume 3B screw up 0x1AD, calling it decimal 428, when it is actually 429.
//       Nehalem/Westmere have both 0x1AC and 0x1AD, while Sandy Bridge only defines one of the two

// Name, MSR, Access, (~WriteMask), Notes

// Same on Nehalem/Westmere as on Sandy Bridge
#define MSR_PLATFORM_INFO 0xCEL
#define MSR_PKG_CST_CONFIG_CONTROL 0xE2L
#define MSR_TEMPERATURE_TARGET 0x1A2L
#define MSR_MISC_FEATURE_CONTROL 0x1A4L
#define MSR_OFFCORE_RSP_0 0x1A6L
#define MSR_OFFCORE_RSP_1 0x1A7L
#define MSR_MISC_PWR_MGMT 0x1AAL
#define MSR_TURBO_POWER_CURRENT_LIMIT 0x1ACL
#define MSR_TURBO_RATIO_LIMIT 0x1ADL
#define MSR_POWER_CTL 0x1AFL
#define MSR_PEBS_LD_LAT 0x3F6L
#define MSR_PKG_C3_RESIDENCY 0x3F8L
#define MSR_PKG_C6_RESIDENCY 0x3F9L
#define MSR_PKG_C7_RESIDENCY 0x3FAL
#define MSR_CORE_C3_RESIDENCY 0x3FCL
#define MSR_CORE_C6_RESIDENCY 0x3FDL

// Changes between Westmere and Sandy Bridge -- same register has additional info fields in SNB
#define MSR_PERF_STATUS 0x198L

// New features in Sandy Bridge
#define MSR_CORE_C7_RESIDENCY 0x3FEL
#define MSR_RAPL_POWER_UNIT 0x606L
#define MSR_PKG_C2_RESIDENCY 0x60DL
#define MSR_PKG_POWER_LIMIT 0x610L
#define MSR_PKG_ENERGY_STATUS 0x611L
#define MSR_PKG_POWER_INFO 0x614L
#define MSR_PP0_POWER_LIMIT 0x638L
#define MSR_PP0_ENERGY_STATUS 0x639L
#define MSR_PP0_POLICY 0x63AL
#define MSR_PP0_PERF_STATUS 0x63BL

// -----------------------------------------------------------------
// Part 5: MSRs from August 2012 Volume 3C, Table 35-12, Extra MSRs for Sandy Bridge 06_2Ah 
// Skipped here....

// -----------------------------------------------------------------
// Part 6: MSRs from August 2012 Volume 3C, Table 35-12, Extra MSRs for Sandy Bridge 06_2Dh 
// All Architectural MSRs are included (including Architectural Perf Monitoring, v3)
//   06_2Dh is Xeon E5 (Stampede)

// Name, MSR, Access, (~WriteMask), Notes
#define MSR_PKG_PERF_STATUS 0x613L
#define MSR_RAPL_PERF_STATUS 0x614L
#define MSR_DRAM_POWER_LIMIT 0x618L
#define MSR_DRAM_ENERGY_STATUS 0x619L
#define MSR_DRAM_PERF_STATUS 0x61BL
#define MSR_DRAM_POWER_INFO 0x61CL

#define U_MSR_PMON_FIXED_CTL 0x703L
#define U_MSR_PMON_FIXED_CTR 0x704L

// New package-scope performance limit MSRs for Goldmont and Skylake (or newer) processors
#define MSR_CORE_PERF_LIMIT_REASONS 0x64f
#define MSR_RING_PERF_LIMIT_REASONS 0x6b1


// -----------------------------------------------------------------
// Part 7: MSRs from SKX uncore
// PCU counters are MSR-based and occupy 4 consecutive MSR addresses (for both control and counts)

#define PCU_MSR_PMON_CTL 0x711L
#define PCU_MSR_PMON_CTR 0x717L

// CHA counters are MSR-based.  
//   The starting MSR address is 0x0E00 + 0x10*CHA
//   	Offset 0 is Unit Control -- mostly un-needed
//   	Offsets 1-4 are the Counter PerfEvtSel registers
//   	Offset 5 is Filter0	-- selects state for LLC lookup event (and TID, if enabled by bit 19 of PerfEvtSel)
//   	Offset 6 is Filter1 -- lots of filter bits, including opcode -- default if unused should be 0x03b, or 0x------33 if using opcode matching
//   	Offset 7 is Unit Status
//   	Offsets 8,9,A,B are the Counter count registers
#define CHA_MSR_PMON_BASE 0x0E00L
#define CHA_MSR_PMON_CTL_BASE 0x0E01L
#define CHA_MSR_PMON_FILTER0_BASE 0x0E05L
#define CHA_MSR_PMON_FILTER1_BASE 0x0E06L
#define CHA_MSR_PMON_STATUS_BASE 0x0E07L
#define CHA_MSR_PMON_CTR_BASE 0x0E08L
