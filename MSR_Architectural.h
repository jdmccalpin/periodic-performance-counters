// -----------------------------------------------------------------
// Part 2: Performance-related MSRs from "Architectural MSRs" 
//   (Volume 3B, Table 35-2) excludes those listed above in
//   "Architectural Performance Monitoring"
//
// Name, MSR_Address
#define IA32_TIME_STAMP_COUNTER 0x10L
#define MSR_SMI_COUNT 0x34L
#define IA32_MPERF 0xE7L
#define IA32_APERF 0xE8L
#define IA32_CLOCK_MODULATION 0x19AL
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
