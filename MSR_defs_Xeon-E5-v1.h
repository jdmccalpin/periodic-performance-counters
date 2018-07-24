//-------------------------------------------------
// MSR list for performance monitoring utility for Xeon E5 v3 (Haswell EP, 06_3F, Hikari/Wrangler/Lonestar5)
// Revision 0,1, 2017-02-17
// John D. McCalpin, mccalpin@tacc.utexas.edu
//-------------------------------------------------
// This is a shortened version of Xeon_E5_v3_Perf_MSRs.txt that just includes
// the MSR names and numbers in cpp #define format...
// Since I use the MSR numbers primarily in "pread()" and "pwrite()" calls,
// I will define all of these as signed long (64-bit) integers.
// https://www.gnu.org/software/libc/manual/html_node/File-Position-Primitive.html
//-------------------------------------------------
// Part 1: Architectural performance monitoring version 3, Volume 3B, section 18.2
//-------------------------------------------------
// Name, MSR, Access, (~WriteMask), Notes
#define IA32_PMC0 0xC1L
#define IA32_PMC1 0xC2L
#define IA32_PMC2 0xC3L
#define IA32_PMC3 0xC4L
#define IA32_PMC4 0xC5L
#define IA32_PMC5 0xC6L
#define IA32_PMC6 0xC7L
#define IA32_PMC7 0xC8L
#define IA32_PERFEVTSEL0 0x186L
#define IA32_PERFEVTSEL1 0x187L
#define IA32_PERFEVTSEL2 0x188L
#define IA32_PERFEVTSEL3 0x189L
#define IA32_PERFEVTSEL4 0x18AL
#define IA32_PERFEVTSEL5 0x18BL
#define IA32_PERFEVTSEL6 0x18CL
#define IA32_PERFEVTSEL7 0x18DL
#define IA32_PERF_STATUS 0x198L
#define IA32_THERM_STATUS 0x19CL
#define IA32_PERF_CTL 0x199L
#define IA32_MISC_ENABLE 0x1A0L
#define IA32_FIXED_CTR0 0x309L
#define IA32_FIXED_CTR1 0x30AL
#define IA32_FIXED_CTR2 0x30BL
#define IA32_FIXED_CTR_CTRL 0x38DL
#define IA32_PERF_GLOBAL_STATUS 0x38EL
#define IA32_PERF_GLOBAL_CTRL 0x38FL
#define IA32_PERF_GLOBAL_OVF_CTRL 0x390L


// -----------------------------------------------------------------
// Part 2: Performance-related MSRs from "Architectural MSRs" 
//   (Volume 3B, Table 34-2; or Table 35-2 in August 2012 revision)
//   excludes those listed above in "Architectural Performance Monitoring"
//
// Name, MSR, Access, (~WriteMask), Notes
#define IA32_TIME_STAMP_COUNTER 0x10L
#define IA32_MPERF 0xE7L
#define IA32_APERF 0xE8L
#define IA32_ENERGY_PERF_BIAS 0x1B0L
#define IA32_PACKAGE_THERM_STATUS 0x1B1L
#define IA32_DEBUGCTL 0x1D9L
#define IA32_PLATFORM_DCA_CAP 0x1F8L
#define IA32_CPU_DCA_CAP 0x1F9L
#define IA32_DCA_0_CAP 0x1FAL
#define IA32_PERF_CAPABILITIES 0x345L
#define IA32_PEBS_ENABLE 0x3F1L
#define IA32_A_PMC0 0x4C1L
#define IA32_A_PMC1 0x4C2L
#define IA32_A_PMC2 0x4C2L
#define IA32_A_PMC3 0x4C3L
#define IA32_A_PMC4 0x4C4L
#define IA32_A_PMC5 0x4C5L
#define IA32_A_PMC6 0x4C6L
#define IA32_A_PMC7 0x4C7L
#define IA32_TSC_AUX 0xC0000203L


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
#define MSR_RAPL_PERF_STATUS 0x614L
#define MSR_DRAM_POWER_LIMIT 0x618L
#define MSR_DRAM_ENERGY_STATUS 0x619L
#define MSR_DRAM_PERF_STATUS 0x61BL
#define MSR_DRAM_POWER_INFO 0x61CL


// Part 7: MSRs from Xeon E5-2600 Uncore Performance Monitoring Guide, document 327043-001
// Applies to Xeon E5 (06_2D)
// Overview:
// -------------------------------------------------------------------------------------------
// Box         #Boxes        #Counters           #Queues      Bus      Packet Match/   Bit
//                            per Box            enabled     Lock?     Mask Filters?   Width
// -- MSR Access -----------------------------------------------------------------------------
// U-Box         1               2 (+1)             0         N/A           N           44
// C-Box         8               4                  1          N            Y           44
// PCU           1               4 (+2)             4          N            N           48
// -- PCI Config Space Access ----------------------------------------------------------------
// HA            1               4                  4          Y            Y           48
// iMC           1 (4 chan)      4 (+1) per chan    4          N            N           48
// QPI           1 (2 ports)     4 per port         4          N            Y?          48
// R2PCIe        1               4                  1          N            N           44
// R3QPI         1               3                  1          N            N           44
// -------------------------------------------------------------------------------------------

// Name, MSR, Access, (~WriteMask), Notes
#define U_MSR_PMON_UCLK_FIXED_CTL 0xC08L
#define U_MSR_PMON_UCLK_FIXED_CTR 0xC09L
#define U_MSR_PMON_CTL0 0xC10L
#define U_MSR_PMON_CTL1 0xC11L
#define U_MSR_PMON_CTR0 0xC16L
#define U_MSR_PMON_CTR1 0xC17L

#define C0_MSR_PMON_BOX_CTL 0xD04L
#define C0_MSR_PMON_CTL0 0xD10L
#define C0_MSR_PMON_CTL1 0xD11L
#define C0_MSR_PMON_CTL2 0xD12L
#define C0_MSR_PMON_CTL3 0xD13L
#define C0_MSR_PMON_BOX_FILTER 0xD14L
#define C0_MSR_PMON_CTR0 0xD16L
#define C0_MSR_PMON_CTR1 0xD17L
#define C0_MSR_PMON_CTR2 0xD18L
#define C0_MSR_PMON_CTR3 0xD19L

#define C1_MSR_PMON_BOX_CTL 0xD24L
#define C1_MSR_PMON_CTL0 0xD30L
#define C1_MSR_PMON_CTL1 0xD31L
#define C1_MSR_PMON_CTL2 0xD32L
#define C1_MSR_PMON_CTL3 0xD33L
#define C1_MSR_PMON_BOX_FILTER 0xD34L
#define C1_MSR_PMON_CTR0 0xD36L
#define C1_MSR_PMON_CTR1 0xD37L
#define C1_MSR_PMON_CTR2 0xD38L
#define C1_MSR_PMON_CTR3 0xD39L

#define C2_MSR_PMON_BOX_CTL 0xD44L
#define C2_MSR_PMON_CTL0 0xD50L
#define C2_MSR_PMON_CTL1 0xD51L
#define C2_MSR_PMON_CTL2 0xD52L
#define C2_MSR_PMON_CTL3 0xD53L
#define C2_MSR_PMON_BOX_FILTER 0xD54L
#define C2_MSR_PMON_CTR0 0xD56L
#define C2_MSR_PMON_CTR1 0xD57L
#define C2_MSR_PMON_CTR2 0xD58L
#define C2_MSR_PMON_CTR3 0xD59L

#define C3_MSR_PMON_BOX_CTL 0xD64L
#define C3_MSR_PMON_CTL0 0xD70L
#define C3_MSR_PMON_CTL1 0xD71L
#define C3_MSR_PMON_CTL2 0xD72L
#define C3_MSR_PMON_CTL3 0xD73L
#define C3_MSR_PMON_BOX_FILTER 0xD74L
#define C3_MSR_PMON_CTR0 0xD76L
#define C3_MSR_PMON_CTR1 0xD77L
#define C3_MSR_PMON_CTR2 0xD78L
#define C3_MSR_PMON_CTR3 0xD79L

#define C4_MSR_PMON_BOX_CTL 0xD84L
#define C4_MSR_PMON_CTL0 0xD90L
#define C4_MSR_PMON_CTL1 0xD91L
#define C4_MSR_PMON_CTL2 0xD92L
#define C4_MSR_PMON_CTL3 0xD93L
#define C4_MSR_PMON_BOX_FILTER 0xD94L
#define C4_MSR_PMON_CTR0 0xD96L
#define C4_MSR_PMON_CTR1 0xD97L
#define C4_MSR_PMON_CTR2 0xD98L
#define C4_MSR_PMON_CTR3 0xD99L

#define C5_MSR_PMON_BOX_CTL 0xDA4L
#define C5_MSR_PMON_CTL0 0xDB0L
#define C5_MSR_PMON_CTL1 0xDB1L
#define C5_MSR_PMON_CTL2 0xDB2L
#define C5_MSR_PMON_CTL3 0xDB3L
#define C5_MSR_PMON_BOX_FILTER 0xDB4L
#define C5_MSR_PMON_CTR0 0xDB6L
#define C5_MSR_PMON_CTR1 0xDB7L
#define C5_MSR_PMON_CTR2 0xDB8L
#define C5_MSR_PMON_CTR3 0xDB9L

#define C6_MSR_PMON_BOX_CTL 0xDC4L
#define C6_MSR_PMON_CTL0 0xDD0L
#define C6_MSR_PMON_CTL1 0xDD1L
#define C6_MSR_PMON_CTL2 0xDD2L
#define C6_MSR_PMON_CTL3 0xDD3L
#define C6_MSR_PMON_BOX_FILTER 0xDD4L
#define C6_MSR_PMON_CTR0 0xDD6L
#define C6_MSR_PMON_CTR1 0xDD7L
#define C6_MSR_PMON_CTR2 0xDD8L
#define C6_MSR_PMON_CTR3 0xDD9L

#define C7_MSR_PMON_BOX_CTL 0xDE4L
#define C7_MSR_PMON_CTL0 0xDF0L
#define C7_MSR_PMON_CTL1 0xDF1L
#define C7_MSR_PMON_CTL2 0xDF2L
#define C7_MSR_PMON_CTL3 0xDF3L
#define C7_MSR_PMON_BOX_FILTER 0xDF4L
#define C7_MSR_PMON_CTR0 0xDF6L
#define C7_MSR_PMON_CTR1 0xDF7L
#define C7_MSR_PMON_CTR2 0xDF8L
#define C7_MSR_PMON_CTR3 0xDF9L

#define PCU_MSR_PMON_BOX_CTL 0xC24L
#define PCU_MSR_PMON_CTL0 0xC30L
#define PCU_MSR_PMON_CTL1 0xC31L
#define PCU_MSR_PMON_CTL2 0xC32L
#define PCU_MSR_PMON_CTL3 0xC33L
#define PCU_MSR_PMON_BOX_FILTER 0xC34L

#define PCU_MSR_PMON_CTR0 0xC36L
#define PCU_MSR_PMON_CTR1 0xC37L
#define PCU_MSR_PMON_CTR2 0xC38L
#define PCU_MSR_PMON_CTR3 0xC39L

#define PCU_MSR_CORE_C3_CTR 0x3FCL
#define PCU_MSR_CORE_C6_CTR 0x3FDL
