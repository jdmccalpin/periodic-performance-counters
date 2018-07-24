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
