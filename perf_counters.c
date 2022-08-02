// New project to program and read all of the core and uncore counters
// John D. McCalpin, mccalpin@tacc.utexas.edu
static char const rcsid[] = "$Id: perf_counters.c,v 1.33 2018/05/02 17:29:20 mccalpin Exp mccalpin $";

// include files
#include <stdio.h>				// printf, etc
#include <stdint.h>				// standard integer types, e.g., uint32_t
#include <signal.h>				// for signal handler
#include <stdlib.h>				// exit() and EXIT_FAILURE
#include <string.h>				// strerror() function converts errno to a text string for printing
#include <fcntl.h>				// for open()
#include <errno.h>				// errno support
#include <assert.h>				// assert() function
#include <unistd.h>				// sysconf() function, sleep() function
#include <sys/mman.h>			// support for mmap() function
#include <math.h>				// for pow() function used in RAPL computations
#include <time.h>
#include <sys/time.h>			// for gettimeofday

#include "MSR_defs.h"		// Performance-Related MSR names for Xeon E5 v3
#include "low_overhead_timers.h"

// constant value defines
# define MAX_SAMPLES 10000			// 10,000 is enough for 1-second sampling for almost 3 hours.
# define NUM_SOCKETS 2				// 
# define NUM_IMC_CHANNELS 6			// includes channels on all IMCs in a socket
# define NUM_IMC_COUNTERS 5			// 0-3 are the 4 programmable counters, 4 is the fixed-function DCLK counter
# define NUM_CHA_BOXES 28			// in each socket for SKX
# define NUM_CHA_COUNTERS 4			// 4 counters for each CHA in each socket on SKX
# define NUM_CHA_CONTROLS 6			// 4 programmable counter controls plus 2 filters
# define NUM_LPROCS 112				// allows up to 2-socket 28-core Skylake Xeon with HyperThreading enabled
# define NUM_CORE_COUNTERS 4		// for Hikari, LS5, Wrangler, Stampede2 SKX with HyperThreading Enabled
# define NUM_HOME_AGENTS 2			// for Xeon E5 v3 processors with >8 cores
# define NUM_HA_COUNTERS 4			// for any Xeon E5 v1/v2/v3/v4 processor


// Global declarations
// 		lots of stuff is global here so the function call interfaces are easier
//		timeline arrays are statically allocated -- fixed limit to the number of samples
//			quit (call cleanup/analysis/output) after last sample if the code gets that far.
// NOTES ON ORGANIZATION!
//   The job of this program is not to think -- thinking will be done in post-processing.
//   The job of this program is to dump everything in a format that does not lose information
//  Core-related information will always be indexed by Logical Processor (LProc) *only*.
//  Uncore-related information will always be indexed by Socket and BoxNumber. 
//  	For the IMC, the MC and DDR channel will be combined into a single "Channel" index.
//  	All of the other uncore boxes naturally have 1D indexing within a socket.
//  	Any exceptions (such as Cluster-on-Die) will be separated out in POST-PROCESSING!!!!
// Bits of unimplemented features should be commented out or deleted, since they may not be 
//   consistent with the current implementation!!!!

// completed implementations
uint64_t tsc_start[MAX_SAMPLES];										// TSC measured on local core at beginning of "read_all_counters()" function
long walltime[2][MAX_SAMPLES];												// seconds and microseconds from gettimeofday()
uint64_t ubox_uclk[NUM_SOCKETS][MAX_SAMPLES];							// 1 UBox/socket, fixed-function counter increments at Uncore Clock Frequency when not in Package C3 or higher
uint64_t imc_counts[NUM_SOCKETS][NUM_IMC_CHANNELS][NUM_IMC_COUNTERS][MAX_SAMPLES];	// including the fixed-function (DCLK) counter as the final entry
char imc_event_name[NUM_SOCKETS][NUM_IMC_CHANNELS][NUM_IMC_COUNTERS][80];		// reserve 32 characters for the IMC event names for each socket, channel, counter
uint64_t core_counts[NUM_LPROCS][NUM_CORE_COUNTERS][MAX_SAMPLES];		// New storage/indexing approach.... 
char core_event_name[NUM_LPROCS][NUM_CORE_COUNTERS][80];		// reserve 80 characters for the core event names for each logical processor and counter
uint64_t core_fixed[NUM_LPROCS][3][MAX_SAMPLES];						// OK to use 3 since all systems have at most 3 fixed-function core counters with fixed names
#if 0
uint64_t ha_counts[NUM_SOCKETS][NUM_HOME_AGENTS][NUM_HA_COUNTERS][MAX_SAMPLES];		// 2 Home Agents: 4 programmable counters each
char ha_event_name[NUM_SOCKETS][NUM_HOME_AGENTS][NUM_HA_COUNTERS][80];			// reserve 32 characters for the HA event names for each socket, Home Agent, counter
#endif
#ifdef INFINIBAND
uint64_t ib_recv[MAX_SAMPLES],ib_xmit[MAX_SAMPLES];						// only one of these per node for TACC systems
#endif
uint64_t pkg_temperature[NUM_SOCKETS][MAX_SAMPLES];			        // Degrees C computed using degrees below PROCHOT
uint64_t rapl_pkg_energy[NUM_SOCKETS][MAX_SAMPLES];					// Unscaled values -- only low-order 32 bits will be set -- rolls after 2^32-1
uint64_t rapl_pkg_throttled[NUM_SOCKETS][MAX_SAMPLES];				// Unscaled values -- only low-order 32 bits will be set -- rolls after 2^32-1
uint64_t rapl_dram_energy[NUM_SOCKETS][MAX_SAMPLES];				// Unscaled values -- only low-order 32 bits will be set -- rolls after 2^32-1
uint64_t pcu_counts[NUM_SOCKETS][4][MAX_SAMPLES];						// 1 PCU: 4 programmable counters (maybe add residency counters later?)
char pcu_event_name[NUM_SOCKETS][4][80];								// reserve 80 characters for each PCI event name
uint64_t pkg_therm_status[NUM_SOCKETS][MAX_SAMPLES];					// IA32_PKG_THERM_STATUS (MSR 0x1b1) -- 13 fields packed into lower 22 bits, including temperature
uint64_t pkg_core_perf_limit_reasons[NUM_SOCKETS][MAX_SAMPLES];			// MSR_CORE_PERF_LIMIT_REASONS (MSR 0x64f) -- new for Skylake -- pkg scope reasons for core freq limits
uint64_t pkg_ring_perf_limit_reasons[NUM_SOCKETS][MAX_SAMPLES];			// MSR_RING_PERF_LIMIT_REASONS (MSR 0x6b1) -- new for Skylake -- pkg scope reasons for ring freq limits
uint64_t aperf[NUM_LPROCS][MAX_SAMPLES];								// 64-bit actual cycles not halted
uint64_t mperf[NUM_LPROCS][MAX_SAMPLES];								// 64-bit reference cycles not halted
uint64_t smi_count[NUM_SOCKETS][MAX_SAMPLES];							// 32-bit count of System Management Interrupts (SMIs) since last reset

#if 1
// 36-bit free-running IO data traffic counters -- count 4 Bytes per increment -- no setup required (or allowed)
uint64_t iio_CBDMA_port1_in[NUM_SOCKETS][MAX_SAMPLES];				// 0xb01 local filesystem in plus (test2 only) Ethernet in
uint64_t iio_CBDMA_port1_out[NUM_SOCKETS][MAX_SAMPLES];				// 0xb05 local filesystem out plus (test2 only) Ethernet out
uint64_t iio_PCIe0_port1_in[NUM_SOCKETS][MAX_SAMPLES];				// 0xb11 (test1 only) Ethernet in
uint64_t iio_PCIe0_port1_out[NUM_SOCKETS][MAX_SAMPLES];				// 0xb15 (test1 only) Ethernet out
uint64_t iio_PCIe2_port0_in[NUM_SOCKETS][MAX_SAMPLES];				// 0xb30 OPA inbound
uint64_t iio_PCIe2_port0_out[NUM_SOCKETS][MAX_SAMPLES];				// 0xb34 OPA outbound
#define CBDMA_p1_in  0xb01
#define CBDMA_p1_out 0xb05
#define PCIE0_p1_in  0xb11
#define PCIE0_p1_out 0xb15
#define PCIE2_p0_in  0xb30
#define PCIE2_p0_out 0xb34
#endif

// implementations waiting for a working program to test....

// implementations being worked on now	
uint64_t cha_counts[NUM_SOCKETS][NUM_CHA_BOXES][NUM_CHA_COUNTERS][MAX_SAMPLES];		// SKX (and KNL) Coherence and Home Agent - used for both mesh and LLC events
char cha_event_name[NUM_SOCKETS][NUM_CHA_BOXES][NUM_CHA_CONTROLS][80];				// counters 0-3 are programmable counters, 4 and 5 are filters

// incomplete and/or untested implementations

uint64_t uncore_qpi[NUM_SOCKETS][2][4][MAX_SAMPLES];					// 2 QPI lanes: 4 programmable counters each
uint64_t uncore_cbo[NUM_SOCKETS][18][4][MAX_SAMPLES];					// up to 18 CBo's: 4 programmable counters each
uint64_t uncore_sbo[NUM_SOCKETS][4][4][MAX_SAMPLES];					// 4 SBo's: 4 programmable counters each --> must write control registers twice!
uint64_t uncore_r2pcie[NUM_SOCKETS][4][MAX_SAMPLES];					// 1 R2PCIe box: 4 programmable counters
uint64_t uncore_r3qpi[NUM_SOCKETS][3][MAX_SAMPLES];						// 1 R3QPI box: 3 programmable counters


int sample;							// number of samples processed (excludes initial performance counter reads)
int	valid;					// set to zero while counters are being read, set to 1 when complete -- use to detect interrupt during a counter read
int dummycounter[MAX_SAMPLES];

int TSC_ratio;
long nr_cpus;				// actual number of cores active -- must be less than or equal to NUM_LPROCS
#ifdef INFINIBAND
FILE *ib_recv_file;					// /sys/class/infiniband/hfi1_0/ports/1/hw_counters/RxWords
FILE *ib_xmit_file;					// /sys/class/infiniband/hfi1_0/ports/1/hw_counters/TxWords
#endif
FILE *log_file;					// output file for stdout and stderr
FILE *results_file;				// output file for lua-formatted counter values
int msr_fd[NUM_LPROCS];		// msr device driver files will be read from various functions, so make descriptors global
long proc_in_pkg[NUM_SOCKETS];		// gives a logical processor number in the socket corresponding to the index value
unsigned int *mmconfig_ptr;         // must be pointer to 32-bit int so compiler will generate 32-bit loads and stores

double power_unit,pkg_energy_unit,time_unit;
double dram_energy_unit, tmp;
int temp_target;

struct timeval tp;		// seconds and microseconds from gettimeofday
struct timezone tzp;	// required, but not used here.

#if 0
#include "Haswell_IMC_BusDeviceFunctionOffset.h"
#include "Haswell_HA_BusDeviceFunctionOffset.h"
#else
#include "SKX_IMC_BusDeviceFunctionOffset.h"
#endif
#include "topology.h"



// helper functions

// ==================================================================================================================
//		Final processing & output of results
void process_all_results()
{
	uint64_t tsc_before, tsc_after, delta_tsc, avg_tsc;
	uint32_t socket, imc, subchannel, channel, counter;
	uint32_t cha;
	uint32_t bus, device, function, offset, ctl_offset, ctr_offset, value, index;
	uint64_t count;
	int i,lproc;
	float microseconds;

	tsc_before = rdtscp();		// measure how long it takes to write out all of the output

	for (i=0; i<sample; i++) {
		// every output sample starts with the TSC value and then the corresponding wall-clock seconds and microseconds
		fprintf(results_file,"tsc[%d] = %lu\n",i, tsc_start[i]);
		fprintf(results_file,"walltime[0][%d] = %ld\n", i, walltime[0][i]);
		fprintf(results_file,"walltime[1][%d] = %ld\n", i, walltime[1][i]);

		// print temperature, PKG energy (unscaled), DRAM energy (unscaled), and PKG throttled time for each socket
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			fprintf(results_file,"pkg_temperature[%u][%d] = %ld\n",socket,i,pkg_temperature[socket][i]);
			fprintf(results_file,"rapl_pkg_energy[%u][%d] = %ld\n",socket,i,rapl_pkg_energy[socket][i]);
			fprintf(results_file,"rapl_dram_energy[%u][%d] = %ld\n",socket,i,rapl_dram_energy[socket][i]);
			fprintf(results_file,"rapl_pkg_throttled[%u][%d] = %ld\n",socket,i,rapl_pkg_throttled[socket][i]);
			fprintf(results_file,"pkg_therm_status[%u][%d] = 0x%lx\n",socket,i,pkg_therm_status[socket][i]);
			fprintf(results_file,"pkg_core_perf_limit_reasons[%u][%d] = 0x%lx\n",socket,i,pkg_core_perf_limit_reasons[socket][i]);
			fprintf(results_file,"pkg_ring_perf_limit_reasons[%u][%d] = 0x%lx\n",socket,i,pkg_ring_perf_limit_reasons[socket][i]);
			fprintf(results_file,"smi_count[%u][%d] = %lu\n",socket,i,smi_count[socket][i]);
		}

		// output the Uncore Cycle Counter in the UBox from each socket
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			fprintf(results_file,"ubox_uclk[%u][%d] = %lu\n",socket,i,ubox_uclk[socket][i]);
		}
		
		// print out fixed-function core counter results
		for (lproc=0; lproc<nr_cpus; lproc++) {
			count = core_fixed[lproc][0][i];
			fprintf(results_file,"core_fixed_counts[%d][\"Inst_Retired.Any\"][%d] = %lu\n",lproc,i,count);
			count = core_fixed[lproc][1][i];
			fprintf(results_file,"core_fixed_counts[%d][\"CPU_CLK_Unhalted.Core\"][%d] = %lu\n",lproc,i,count);
			count = core_fixed[lproc][2][i];
			fprintf(results_file,"core_fixed_counts[%d][\"CPU_CLK_Unhalted.Ref\"][%d] = %lu\n",lproc,i,count);
		}

		// print out programmable core counter results
		for (lproc=0; lproc<nr_cpus; lproc++) {
			for (counter=0; counter<4; counter++) {
				count = core_counts[lproc][counter][i];
						fprintf(results_file,"core_counts[%d][\"%s\"][%d] = %lu\n",lproc,
							core_event_name[lproc][counter],i,count);
			}
		}

		// print out extra MSR-based core counter results
		for (lproc=0; lproc<nr_cpus; lproc++) {
			fprintf(results_file,"aperf[%d][%d] = %lu\n",lproc,i,aperf[lproc][i]);
			fprintf(results_file,"mperf[%d][%d] = %lu\n",lproc,i,mperf[lproc][i]);
		}

		// print out CHA counter values
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			for (cha=0; cha<NUM_CHA_BOXES; cha++) {
				for (counter=0; counter<NUM_CHA_COUNTERS; counter++) {
					fprintf(results_file,"cha_counts[%u][%u][\"%s\"][%d] = %lu\n", socket, cha, 
							cha_event_name[socket][cha][counter], i,
							cha_counts[socket][cha][counter][i]);
				}
			}
		}

#if 0
		// print out HomeAgent counter results
		for (socket=0; socket<2; socket++) {
			for (ha=0; ha<2; ha++) {
				for (counter=0; counter<4; counter++) {
					fprintf(results_file,"ha_counts[%u][%u][\"%s\"][%d] = %lu\n", socket, ha, 
							ha_event_name[socket][ha][counter], i,
							ha_counts[socket][ha][counter][i]);
				}
			}
		}
#endif
		// print out IMC counter results
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			for (channel=0; channel<NUM_IMC_CHANNELS; channel++) {
				for (counter=0; counter<NUM_IMC_COUNTERS; counter++) {
					fprintf(results_file,"imc_counts[%u][%u][\"%s\"][%d] = %lu\n", socket, channel, 
						imc_event_name[socket][channel][counter], i,
						imc_counts[socket][channel][counter][i]);
				}
			}
		}
	#ifdef INFINIBAND
		// print out InfiniBand receive and transmit counts
		//    scale by 4 to get Bytes in the output file
		fprintf(results_file,"ib_recv_bytes[%d] = %lu\n", i, ib_recv[i]*4);
		fprintf(results_file,"ib_xmit_bytes[%d] = %lu\n", i, ib_xmit[i]*4);
	#endif

		// print out Free-Running IO counter results
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			fprintf(results_file,"iio_CBDMA_port1_in_bytes[%u][%d] = %lu\n", socket, i, iio_CBDMA_port1_in[socket][i]*4);
			fprintf(results_file,"iio_CBDMA_port1_out_bytes[%u][%d] = %lu\n", socket, i, iio_CBDMA_port1_out[socket][i]*4);
			fprintf(results_file,"iio_PCIe0_port1_in_bytes[%u][%d] = %lu\n", socket, i, iio_PCIe0_port1_in[socket][i]*4);
			fprintf(results_file,"iio_PCIe0_port1_out_bytes[%u][%d] = %lu\n", socket, i, iio_PCIe0_port1_out[socket][i]*4);
			fprintf(results_file,"iio_PCIe2_port0_in_bytes[%u][%d] = %lu\n", socket, i, iio_PCIe2_port0_in[socket][i]*4);
			fprintf(results_file,"iio_PCIe2_port0_out_bytes[%u][%d] = %lu\n", socket, i, iio_PCIe2_port0_out[socket][i]*4);
		}
		// print out PCU counter results
		for (socket=0; socket<NUM_SOCKETS; socket++) {
			for (counter=0; counter<4; counter++) {
				fprintf(results_file,"pcu_counts[%u][\"%s\"][%d] = %lu\n", socket, 
					pcu_event_name[socket][counter], i,
					pcu_counts[socket][counter][i]);
			}
		}
	}
	tsc_after = rdtscp();		// measure how long it takes to write out all of the output
	delta_tsc = tsc_after - tsc_before;
	microseconds = (float)(delta_tsc) / TSC_ratio / 100.0;			// 100 MHz reference clock
	fprintf(log_file,"OVERHEAD: writing all output took %lu TSC cycles %f microseconds\n",delta_tsc,microseconds);

	fflush(results_file);
	fclose(results_file);

	fflush(log_file);
	fclose(log_file);
}

// Convert PCI(bus:device.function,offset) to uint32_t array index
uint32_t PCI_cfg_index(unsigned int Bus, unsigned int Device, unsigned int Function, unsigned int Offset)
{
    uint32_t byteaddress;
    uint32_t index;
    // assert (Bus == BUS);
    assert (Device >= 0);
    assert (Function >= 0);
    assert (Offset >= 0);
    assert (Device < (1<<5));
    assert (Function < (1<<3));
    assert (Offset < (1<<12));
#ifdef DEBUG
    fprintf(log_file,"Bus,(Bus<<20)=%x\n",Bus,(Bus<<20));
    fprintf(log_file,"Device,(Device<<15)=%x\n",Device,(Device<<15));
    fprintf(log_file,"Function,(Function<<12)=%x\n",Function,(Function<<12));
    fprintf(log_file,"Offset,(Offset)=%x\n",Offset,Offset);
#endif
    byteaddress = (Bus<<20) | (Device<<15) | (Function<<12) | Offset;
    index = byteaddress / 4;
    return ( index );
}

// ==========================================================================================================
// Read all the performance counters for this node
//		currently contains writes to log files -- not sure if I need to kill these
//
void read_all_counters()
{
	uint32_t low, high;
	uint32_t socket, imc, subchannel, channel, counter;
	uint32_t cha;
	uint32_t bus, device, function, offset, ctl_offset, ctr_offset, value, index;
	uint64_t count;
	uint64_t tsc_before, tsc_after, delta_tsc, avg_tsc;
	uint64_t msr_num, msr_val;
	ssize_t rc64;
	char filename[100];
	int reads_performed;
	int core,lproc;
	int temp_below;
	int i;

	// Grab a TSC value to use as the node-local timeline value for this set of samples
	// Call gettimeofday() to get the wall clock time for cross-node timing alignment
	tsc_start[sample] = rdtscp();
    i = gettimeofday(&tp,&tzp);
	walltime[0][sample] = tp.tv_sec;
	walltime[1][sample] = tp.tv_usec;

	// Read socket-scope MSRs in each socket.
	// This includes temperature, pkg energy use, dram energy use, pkg power throttled time, 
	// SMI interrupts, and uncore clock counts.
	// NOTE: Temperature values are in degrees C
	//  Energy and Throttle time values are unscaled 32-bit counts (to make it easier to
    //		correct for wrap-around in post-processing).
	fprintf(log_file,"VERBOSE: Reading Socket-Scope MSR counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		core = proc_in_pkg[socket];
		msr_num = IA32_PACKAGE_THERM_STATUS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		pkg_therm_status[socket][sample] = msr_val;
		temp_below  = (msr_val & 0x007F0000)>>16;      // 7 bit field for degrees C below PROCHOT temperature
		pkg_temperature[socket][sample] = temp_target - temp_below;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_CORE_PERF_LIMIT_REASONS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		pkg_core_perf_limit_reasons[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_RING_PERF_LIMIT_REASONS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		pkg_ring_perf_limit_reasons[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_PKG_ENERGY_STATUS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		rapl_pkg_energy[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_DRAM_ENERGY_STATUS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		rapl_dram_energy[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_PKG_PERF_STATUS;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
		rapl_pkg_throttled[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = MSR_SMI_COUNT;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, lproc);
			exit(-1);
		}
		smi_count[socket][sample] = msr_val;
		reads_performed++;

		core = proc_in_pkg[socket];
		msr_num = U_MSR_PMON_FIXED_CTR;
		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d",
						counter, msr_num, core);
			exit(-1);
		}
		ubox_uclk[socket][sample] = msr_val;
		reads_performed++;
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d socket-scope-MSR-counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);

	// Read the programmable core counters in each logical processor
	fprintf(log_file,"VERBOSE: Reading Programmable Core counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (lproc=0; lproc<nr_cpus; lproc++) {
		for (counter=0; counter<NUM_CORE_COUNTERS; counter++) {
			msr_num = 0xC1 + counter;
			rc64 = pread(msr_fd[lproc],&msr_val,sizeof(msr_val),msr_num);
			if (rc64 != sizeof(msr_val)) {
				fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d",
					counter, msr_num, lproc);
				exit(-1);
			}
			core_counts[lproc][counter][sample] = msr_val;
			reads_performed++;
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d programmable_core_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);

	// Read the fixed-function core counters in each logical processor
	fprintf(log_file,"VERBOSE: Reading Fixed-Function Core counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (lproc=0; lproc<nr_cpus; lproc++) {
		for (counter=0; counter<3; counter++) {
			msr_num = 0x309 + counter;
			rc64 = pread(msr_fd[lproc],&msr_val,sizeof(msr_val),msr_num);
			if (rc64 != sizeof(msr_val)) {
				fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d",
					counter, msr_num, lproc);
				exit(-1);
			}
			core_fixed[lproc][counter][sample] = msr_val;
			reads_performed++;
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d fixed-function_core_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);

	// Read additional MSR-based counters in each logical processor
	fprintf(log_file,"VERBOSE: Reading additional MSR Core counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (lproc=0; lproc<nr_cpus; lproc++) {
		msr_num = IA32_APERF;
		rc64 = pread(msr_fd[lproc],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read APERF MSR %x on Logical Processor %d", msr_num, lproc);
			exit(-1);
		}
		aperf[lproc][sample] = msr_val;
		reads_performed++;
		msr_num = IA32_MPERF;
		rc64 = pread(msr_fd[lproc],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read MPERF MSR %x on Logical Processor %d", msr_num, lproc);
			exit(-1);
		}
		mperf[lproc][sample] = msr_val;
		reads_performed++;

	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d extra_MSR_core_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);


	// NEW -- read CHA counters for SKX
	fprintf(log_file,"VERBOSE: Reading CHA counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<2; socket++) {
		core = proc_in_pkg[socket];
		for (cha=0; cha<NUM_CHA_BOXES; cha++) {
			for (counter=0; counter<NUM_CHA_COUNTERS; counter++) {
				msr_num = CHA_MSR_PMON_CTR_BASE + (0x10 * cha) + counter;
				// fprintf(log_file,"DEBUG: socket %u core %u cha %u counter %u msr_num %lx msr_val %lu\n", socket, core, cha, counter, msr_num, msr_val);
				rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
				if (rc64 != sizeof(msr_val)) {
					fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
					exit(-1);
				}
				cha_counts[socket][cha][counter][sample] = msr_val;
				reads_performed++;
			}
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d CHA_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);


#if 0
	fprintf(log_file,"VERBOSE: Reading HomeAgent counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<2; socket++) {
		bus = HA_BUS_Socket[socket];
		for (ha=0; ha<2; ha++) {
			device = HA_Device_Agent[ha];
			function = HA_Function_Agent[ha];
			for (counter=0; counter<4; counter++) {
				offset = HA_PmonCtr_Offset[counter];
				index = PCI_cfg_index(bus, device, function, offset);
				low = mmconfig_ptr[index];
				high = mmconfig_ptr[index+1];
				count = ((uint64_t) high) << 32 | (uint64_t) low;
				// fprintf(log_file,"DEBUG: low %x high %x count %lx (count %ld)\n",low,high,count,count);
				ha_counts[socket][ha][counter][sample] = count;
				reads_performed++;
			}
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d HomeAgent_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);
#endif



	fprintf(log_file,"VERBOSE: Reading IMC counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		bus = IMC_BUS_Socket[socket];
		for (channel=0; channel<NUM_IMC_CHANNELS; channel++) {
			device = IMC_Device_Channel[channel];
			function = IMC_Function_Channel[channel];
			for (counter=0; counter<NUM_IMC_COUNTERS; counter++) {
				offset = IMC_PmonCtr_Offset[counter];
				index = PCI_cfg_index(bus, device, function, offset);
				low = mmconfig_ptr[index];
				high = mmconfig_ptr[index+1];
				count = ((uint64_t) high) << 32 | (uint64_t) low;
				imc_counts[socket][channel][counter][sample] = count;
				reads_performed++;
			}
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	// NOTE: some quick tests on a Hikari node showed 30k-36k TSC cycles (11-14 microseconds) to read the 4 programmable IMC counters for each socket/imc/channel.
	//   2 sockets * 4 channels/socket * 4 IMCs * 2 reads/counter = 64 reads --> 450-550 TSC cycles/read
	// fprintf(log_file,"DEBUG: reading IMC counters took %lu TSC cycles\n",delta_tsc);
	fprintf(log_file,"OVERHEAD: reading %d IMC_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);


#ifdef INFINIBAND
	fprintf(log_file,"VERBOSE: Reading InfiniBand counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();

	sprintf(filename,"/sys/class/infiniband/hfi1_0/ports/1/hw_counters/RxWords");
	ib_recv_file = fopen(filename,"r");
	i = fscanf(ib_recv_file,"%ld",&ib_recv[sample]);
	reads_performed++;
	fclose(ib_recv_file);

	sprintf(filename,"/sys/class/infiniband/hfi1_0/ports/1/hw_counters/TxWords");
	ib_xmit_file = fopen(filename,"r");
	i = fscanf(ib_xmit_file,"%ld",&ib_xmit[sample]);
	fclose(ib_xmit_file);
	reads_performed++;

	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d InfiniBand_Counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);
#endif

	fprintf(log_file,"VERBOSE: Reading Free-running IO counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		core = proc_in_pkg[socket];

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),CBDMA_p1_in);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, CBDMA_p1_in, core);
			exit(-1);
		}
		iio_CBDMA_port1_in[socket][sample] = msr_val;

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),CBDMA_p1_out);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, CBDMA_p1_out, core);
			exit(-1);
		}
		iio_CBDMA_port1_out[socket][sample] = msr_val;

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),PCIE0_p1_in);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, PCIE0_p1_in, core);
			exit(-1);
		}
		iio_PCIe0_port1_in[socket][sample] = msr_val;

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),PCIE0_p1_out);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, PCIE0_p1_out, core);
			exit(-1);
		}
		iio_PCIe0_port1_out[socket][sample] = msr_val;

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),PCIE2_p0_in);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, PCIE2_p0_in, core);
			exit(-1);
		}
		iio_PCIe2_port0_in[socket][sample] = msr_val;

		rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),PCIE2_p0_out);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to read performance counter number %d MSR %x on Logical Processor %d", counter, PCIE2_p0_out, core);
			exit(-1);
		}
		iio_PCIe2_port0_out[socket][sample] = msr_val;

		reads_performed += 6;
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d Free-Running_IIO_Counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);


	// read PCU counters in each socket
	fprintf(log_file,"VERBOSE: Reading PCU counters....\n");
	reads_performed = 0;
	tsc_before = rdtscp();
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		core = proc_in_pkg[socket];
		for (counter=0; counter<4; counter++) {
			msr_num = PCU_MSR_PMON_CTR + counter;
			rc64 = pread(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
			if (rc64 != sizeof(msr_val)) {
				fprintf(log_file,"ERROR: failed to read MSR %x on Logical Processor %d", msr_num, core);
				exit(-1);
			}
			pcu_counts[socket][counter][sample] = msr_val;
			reads_performed++;
		}
	}
	tsc_after = rdtscp();
	delta_tsc = tsc_after - tsc_before;
	avg_tsc = delta_tsc / reads_performed;
	fprintf(log_file,"OVERHEAD: reading %d PCU_counters %lu total TSC cycles, %lu average TSC cycles\n",reads_performed,delta_tsc,avg_tsc);



	sample++;
}




// 		signal handler for SIGCONT (optional)
// 			this function calls cleanup and analysis/output code
static void catch_function(int signal) {
	fprintf(log_file,"Caught SIGCONT. Shutting down...\n");
	if (valid == 0) {
		fprintf(log_file,"DEBUG: Interrupt received while counters were being read -- discarding final sample %d\n",sample);
		sample--;
		fprintf(log_file,"DEBUG: This leaves %d deltas to be processed\n",sample);
	} else {
		fprintf(log_file,"DEBUG: %d samples read after initial read, %d deltas to be processed\n",sample,sample);
	}
	read_all_counters();
	process_all_results();
	exit(0);
}

static char* acpi_mcfg_paths[] = {
    "/sys/firmware/acpi/tables/MCFG",
    "/sys/firmware/acpi/tables/MCFG1",
    NULL,
};

int PCI_get_base(unsigned long* base) {
    int ret = -1;
    unsigned long tmp = 0x0;
    int pid = 0;
    do {
        if (!access(acpi_mcfg_paths[pid], R_OK))
        {
            int fp = open(acpi_mcfg_paths[pid], O_RDONLY);
            if (fp >= 0) {
                ret = pread(fp, &tmp, sizeof(unsigned long), 44);
                close(fp);
                if (ret == sizeof(unsigned long)) {
                    ret = 0;
                    break;
                }
            }
        }
        pid++;
    } while (acpi_mcfg_paths[pid]);
    if (ret == 0)
        *base = tmp;
    return ret;
}


// ===========================================================================================================================================================================
int main(int argc, char *argv[])
{
	// local declarations
	struct timespec duration, remainder;				// structures for nanosleep function -- remainder is not used here
	int cpuid_return[4];
	int i;
	int rc;
	ssize_t rc64;
	char description[100];
	size_t len;
	uint64_t msr_num, msr_val;
	uint32_t bus, device, function, offset, ctl_offset, ctr_offset, value, index;
	uint32_t dummy32u;
	uint32_t socket, imc, channel, subchannel, counter;
	int cha;
	uint32_t ha;
	char filename[100];
	int save_errno;
	int core, lproc;
	int mem_fd;
    // Read mmconfig base address later from ACPI tables
    unsigned long mmconfig_base=0x0;
    unsigned long mmconfig_size=0x10000000;
	long long result;

	int pkg;
	FILE *input_file;


	// get the host name, assume that it is of the TACC standard form, and use this as part
	// of the log file name....  Standard form is "c263-109.hikari.tacc.utexas.edu", so
	// truncating at the first "." is done by writing \0 to character #8.
	len = 100;	
	rc = gethostname(description, len);
	if (rc != 0) {
		fprintf(stderr,"ERROR when trying to get hostname\n");
		exit(-1);
	}
	description[8] = 0;		// assume hostname of the form c263-109.hikari.tacc.utexas.edu -- truncate after first period

	sprintf(filename,"log.%s.perf_counters",description);
	// sprintf(filename,"log.perf_counters");
	log_file = fopen(filename,"w+");
	if (log_file == 0) {
		fprintf(stderr,"ERROR %s when trying to open log file %s\n",strerror(errno),filename);
		exit(-1);
	}
	uid_t my_uid;
	gid_t my_gid;
	my_uid = getuid();
	my_gid = getgid();
	rc = chown(filename,my_uid,my_gid);
	if (rc == 0) {
		fprintf(log_file,"DEBUG: Successfully changed ownership of log file to uid %d gid %d\n",my_uid,my_gid);
	} else {
		fprintf(stderr,"ERROR: Attempt to change ownership of log file to uid %d gid %d failed -- bailing out\n",my_uid,my_gid);
		exit(-1);
	}


	// initial checks
	// 		is this a supported core?  (CPUID Family/Model)
	// 		The compiler cpuid intrinsics are not documented by Intel -- they use the Microsoft format
	// 			described at https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
	// 			__cpuid(array to hold eax,ebx,ecx,edx outputs, initial eax value)
	// 			__cpuidex(array to hold eax,ebx,ecx,edx outputs, initial eax value, initial ecx value)
	//      CPUID function 0x01 returns the model info in eax.
	//      		27:20 ExtFamily	-- expect 0x00
	//      		19:16 ExtModel	-- expect 0x3 for HSW, 0x5 for SKX
	//      		11:8  Family	-- expect 0x6
	//      		7:4   Model		-- expect 0xf for HSW, 0x5 for SKX
	//      Every processor that I am going to see will be Family 0x06 (no ExtFamily needed).
	//      The DisplayModel field is (ExtModel<<4)+Model and should be 0x3F for all Xeon E5 v3 systems
	__cpuid(&cpuid_return[0], 1);
	uint32_t ModelInfo = cpuid_return[0] & 0x0fff0ff0;	// mask out the reserved and "stepping" fields, leaving only the based and extended Family/Model fields
#if 0
	if (ModelInfo != 0x000306f0) {				// expected values for Haswell EP
		fprintf(log_file,"ERROR -- this does not appear to be the correct processor type!!!\n");
		fprintf(log_file,"ERROR -- Expected CPUID(0x01) Family/Model bits = 0x%x, but found 0x%x\n",0x000306f0,ModelInfo);
		exit(1);
	} else {
		fprintf(log_file,"DEBUG: Well Done! You are running on a Xeon E5 v3 processor! CPUID signature 0x%x\n",ModelInfo);
	}
#else
	if (ModelInfo != 0x00050650) {				// expected values for Haswell EP
		fprintf(log_file,"ERROR -- this does not appear to be the correct processor type!!!\n");
		fprintf(log_file,"ERROR -- Expected CPUID(0x01) Family/Model bits = 0x%x, but found 0x%x\n",0x00050650,ModelInfo);
		exit(1);
	} else {
		fprintf(log_file,"DEBUG: Well Done! You are running on a Xeon Scalable (Skylake Xeon) processor! CPUID signature 0x%x\n",ModelInfo);
	}
#endif

	// check command-line arguments
	// 		details TBD, but should include
	//			sleeptime (optional)	
	//				-- if sleeptime is not specified, output will be reported as a single interval
	//			input counter file (optional)
	//				-- if not specified, use a default name?

	if (argc == 1) {
		fprintf(log_file, "INFO: No command-line arguments provided -- assuming 1 second sampling rate\n");
		duration.tv_sec = 1;
		duration.tv_nsec = 0;
	} else if (argc == 2) {
		i = atoi(argv[1]);
		if (i >= 1000000000) {
			fprintf(log_file, "ERROR: sampling interval in ns cannot exceed 1,000,000,000\n");
			exit(1);
		}
		duration.tv_sec = 0;
		duration.tv_nsec = i;		// 1,000,000 ns = 1 millisecond
		fprintf(log_file, "INFO: sampling uses %d nanosecond sleep \n",i);
	} else if (argc == 3) {
		i = atoi(argv[1]);
		duration.tv_sec = i;
		i = atoi(argv[2]);
		if (i >= 1000000000) {
			fprintf(log_file, "ERROR: sampling interval in ns cannot exceed 1,000,000,000\n");
			exit(1);
		}
		duration.tv_nsec = i;		// 1,000,000 ns = 1 millisecond
		fprintf(log_file, "INFO: sampling uses %ld second plus %ld nanosecond sleep \n",duration.tv_sec,duration.tv_nsec);
	} else {
		fprintf(log_file, "ERROR: Expected one or two numeric arguments\n");
		exit(1);
	}


	// register signal handler to receive SIGCONT
	if (signal(SIGCONT, catch_function) == SIG_ERR) {
		fprintf(log_file, "An error occurred while setting the signal handler.\n");
		return EXIT_FAILURE;
	}

	// initialize the dummycounter array....
	// TEMPORARY HACK
	for (i=0; i<MAX_SAMPLES; i++) dummycounter[i] = -1;

	// initialize the tsc_start and walltime arrays
	for (sample=0; sample < MAX_SAMPLES; sample++) {
		tsc_start[sample] = 0;
		walltime[0][sample] = 0;
		walltime[1][sample] = 0;
#ifdef INFINIBAND
		ib_recv[sample] = 0;
		ib_xmit[sample] = 0;
#endif
	}

	// initialize ubox_uclk cycle count array
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		for (sample=0; sample < MAX_SAMPLES; sample++) {
			ubox_uclk[socket][sample] = 0;
		}
	}

	// initialize uncore_imc performance count array
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		for (channel=0; channel<NUM_IMC_CHANNELS; channel++) {
			for (counter=0; counter<NUM_IMC_COUNTERS; counter++) {
				for (sample=0; sample < MAX_SAMPLES; sample++) {
					imc_counts[socket][channel][counter][sample] = 0;
				}
			}
		}
	}
	// initialize array to hold core programmable counter results
	for (lproc=0; lproc<NUM_LPROCS; lproc++) {
		for (counter=0; counter<NUM_CORE_COUNTERS; counter++) {
			for (sample=0; sample < MAX_SAMPLES; sample++) {
				core_counts[lproc][counter][sample] = 0;
			}
		}
	}
	// initialize array to hold core fixed counter results
	for (lproc=0; lproc<NUM_LPROCS; lproc++) {
		for (counter=0; counter<3; counter++) {
			for (sample=0; sample < MAX_SAMPLES; sample++) {
				core_fixed[lproc][counter][sample] = 0;
			}
		}
	}

	// initialize PCU count array
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		for (counter=0; counter<4; counter++) {
			for (sample=0; sample < MAX_SAMPLES; sample++) {
				pcu_counts[socket][counter][sample] = 0;
			}
		}
	}



	// For Xeon systems (max of 2 threads per core), I can check the AnyThread bit, and if it is set, then
	// I can merge the counts for logical processors 0..N/2-1 with the corresponding logical processor in the
	// upper half of the range (i.e., merge counts for "i" and "i + N/2").
	// 		Be sure that the "AnyThread bit is set in the counter for both thread contexts before merging.

	// I can also use Logical Processors 0 and N-1 to get uncore counts from the two sockets.
	nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	assert (nr_cpus <= NUM_LPROCS);
	proc_in_pkg[0] = 0;					// logical processor 0 is in socket 0 in all TACC systems
	proc_in_pkg[1] = nr_cpus-1;			// logical processor N-1 is in socket 1 in all TACC 2-socket systems

#ifdef INFINIBAND
	// ---- does not require root permission -----
	// ---- only needs one per node on TACC systems ----
		sprintf(filename,"/sys/class/infiniband/hfi1_0/ports/1/hw_counters/RxWords");
		ib_recv_file = fopen(filename,"r");
		if (ib_recv_file == 0) {
			fprintf(log_file,"ERROR %s when trying to open IB receive data counter file %s\n",strerror(errno),filename);
			exit(-1);
		}
		sprintf(filename,"/sys/class/infiniband/hfi1_0/ports/1/hw_counters/TxWords");
		ib_xmit_file = fopen(filename,"r");
		if (ib_xmit_file == 0) {
			fprintf(log_file,"ERROR %s when trying to open IB transmit data counter file %s\n",strerror(errno),filename);
			exit(-1);
		}
#endif

	// ------------------ REQUIRES ROOT PERMISSIONS ------------------
	// open all /dev/cpu/*/msr files
	fprintf(log_file,"opening all /dev/cpu/*/msr files\n");
	for (i=0; i<nr_cpus; i++) {
		sprintf(filename,"/dev/cpu/%d/msr",i);
		// printf("opening %s\n",filename);
		msr_fd[i] = open(filename, O_RDWR);
		// printf("   open command returns %d\n",msr_fd[i]);
		if (msr_fd[i] == -1) {
			fprintf(log_file,"ERROR %s when trying to open %s\n",strerror(errno),filename);
			exit(-1);
		}
#if 0
		// Simple test to see if these file opens are working correctly....
		pread(msr_fd[i],&msr_val,sizeof(msr_val),IA32_TIME_STAMP_COUNTER);
		fprintf(log_file,"DEBUG: TSC on core %d is %ld\n",i,msr_val);
#endif
	}


	// ------------------ REQUIRES ROOT PERMISSIONS ------------------
	// open /dev/mem for PCI device access and mmap() a pointer to the beginning
	// of the 256 MiB PCI Configuration Space.
	// 		check VID/DID for uncore bus:device:function combinations
	//   Note that using /dev/mem for PCI configuration space access is required for some devices on KNL.
	//   It is not required on other systems, but it is not particularly inconvenient either.
	if (PCI_get_base(&mmconfig_base) != 0) {
		fprintf(log_file,"ERROR %s when trying to get mmconfig base address from ACPI tables\n",strerror(errno));
		exit(-1);
	}
	sprintf(filename,"/dev/mem");
	fprintf(log_file,"opening %s\n",filename);
	mem_fd = open(filename, O_RDWR);
	// fprintf(log_file,"   open command returns %d\n",mem_fd);
	if (mem_fd == -1) {
		fprintf(log_file,"ERROR %s when trying to open %s\n",strerror(errno),filename);
		exit(-1);
	}
	int map_prot = PROT_READ | PROT_WRITE;
	mmconfig_ptr = mmap(NULL, mmconfig_size, map_prot, MAP_SHARED, mem_fd, mmconfig_base);
    if (mmconfig_ptr == MAP_FAILED) {
        fprintf(log_file,"cannot mmap base of PCI configuration space from /dev/mem: address %lx\n", mmconfig_base);
        exit(2);
    } else {
		fprintf(log_file,"Successful mmap of base of PCI configuration space from /dev/mem at address %lx\n", mmconfig_base);
	}
    close(mem_fd);      // OK to close file after mmap() -- the mapping persists until unmap() or program exit
#if 0
	// simple test -- should return "2f328086" on Haswell EP -- DID 0x2f32, VID 0x8086
	bus = 0x7f;
	device = 0x8;
	function = 0x2;
	offset = 0x0;
	index = PCI_cfg_index(bus, device, function, offset);
    value = mmconfig_ptr[index];
	if (value == 0x2f328086) {
		fprintf(log_file,"DEBUG: Well done! Bus %x device %x function %x offset %x returns expected value of %x\n",bus,device,function,offset,value);
	} else {
		fprintf(log_file,"DEBUG: ERROR: Bus %x device %x function %x offset %x expected %x, found %x\n",bus,device,function,offset,0x2f328086,value);
		exit(3);
	}
#elif 0
	// New simple test -- the old one failed on nodes with QPI Link Layer counters disabled.
	// This happens on Hikari when a node with a bad CMOS batter loses power and reverts to
	// the default.  The node needs to have its BIOS settings fixed after this, but since I
	// am not currently using the QPI link-layer counters, I can just check a different PCIe
	// device -- Home Agent 0 should be safe.
	//
	// simple test -- should return "2f308086" on Haswell EP -- DID 0x2f30, VID 0x8086
	bus = 0x7f;
	device = 0x12;
	function = 0x1;
	offset = 0x0;
	index = PCI_cfg_index(bus, device, function, offset);
    value = mmconfig_ptr[index];
	if (value == 0x2f308086) {
		fprintf(log_file,"DEBUG: Well done! Bus %x device %x function %x offset %x returns expected value of %x\n",bus,device,function,offset,value);
	} else {
		fprintf(log_file,"DEBUG: ERROR: Bus %x device %x function %x offset %x expected %x, found %x\n",bus,device,function,offset,0x2f308086,value);
		exit(3);
	}
#else 
	// New simple test that does not need to know the uncore bus numbers here...
	// Skylake bus 0, Function 5, offset 0 -- Sky Lake-E MM/Vt-d Configuration Registers
	//
	// simple test -- should return "20248086" on Skylake Xeon EP -- DID 0x2024, VID 0x8086
	bus = 0x00;
	device = 0x5;
	function = 0x0;
	offset = 0x0;
	index = PCI_cfg_index(bus, device, function, offset);
    value = mmconfig_ptr[index];
	if (value == 0x20248086) {
		fprintf(log_file,"DEBUG: Well done! Bus %x device %x function %x offset %x returns expected value of %x\n",bus,device,function,offset,value);
	} else {
		fprintf(log_file,"DEBUG: ERROR: Bus %x device %x function %x offset %x expected %x, found %x\n",bus,device,function,offset,0x20248086,value);
		exit(3);
	}
#endif

	// Open and read performance counter event files
	//   Input Files are split by "box" to make subsequent parsing easier....
	//
	//	#1. Core MSR control -- logical processor scope
	//	#2. Core MSR PerfEvtSel -- logical processor scope
	//	#3. Uncore MSR control -- Package scope:  UBox, CBo, SBo, PCU
	//	#4a. Uncore MSR PerfEvtSel -- Ubox	--- NOT IMPLEMENTED -- NO INTERESTING EVENTS HERE
	//	#4b. Uncore MSR PerfEvtSel -- CBo
	//	#4c. Uncore MSR PerfEvtSel -- SBo
	//	#4d. Uncore MSR PerfEvtSel -- PCU
	//	#4e. Uncore MSR PerfEvtSel -- CHA -- KNL and SKX
	//	#5. Uncore PCIConfig control -- HA, IMC, QPI, R2PCIe, R3QPI
	//	#6a. Uncore PCIConfig PerfEvtSel -- HA
	//	#6b. Uncore PCIConfig PerfEvtSel -- IMC
	//	#6c. Uncore PCIConfig PerfEvtSel -- QPI
	//	#6d. Uncore PCIConfig PerfEvtSel -- R2PCIe
	//	#6e. Uncore PCIConfig PerfEvtSel -- R3QPI
	//
	// 		These need to be set up carefully to ensure that they are correct for the system under test.
	// 		I expect to make minimal sanity checks on the input....
	// 	Do I want to split these by box, or should this be part of the event name?
	// 		I think the latter....
	//
	// Input File #1: Core MSRs Control/Config (i.e., not PerfEvtSel) 
	// 		Contains one line per MSR, each line contains 5 fields: CoreMin, CoreMax, MSR, value, description
	sprintf(filename,"core_msr_control.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open MSR input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	int core_min,core_max;
	while (1) {
		rc = fscanf(input_file,"%d %d %lx %lx %s",&core_min,&core_max,&msr_num,&msr_val,&description);
		if (rc == EOF) break;
		i++;
		// fprintf(log_file,"DEBUG: Core MSR control input file contains %d %d 0x%0lx 0x%#0x %s\n",core_min, core_max, msr_num, msr_val, description);
		for (core=core_min; core<=core_max; core++) {
			// fprintf(log_file,"pwrite(msr_fd[%d],0x%lx,%ld,0x%lx)\n",core,msr_val,sizeof(msr_val),msr_num);
			rc64 = pwrite(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
			if (rc64 != 8) {
				fprintf(log_file,"ERROR writing to MSR device on core %d, write %ld bytes\n",core,rc64);
				exit(-1);
			}
		}
		// fprintf(log_file,"DEBUG: completed writing Core MSR input value to all core MSR device drivers\n");
	}
	// fprintf(log_file,"DEBUG: Core MSR control input file contained %d values\n",i);
	fclose(input_file);


	// Input File #2: Core MSRs PerfEvtSel
	// 		Contains one line per MSR, each line contains 5 fields: CoreMin, CoreMax, MSR, value, description
	// Note that I don't specify the MSR numbers for the core performance counter count registers --
	// they are assumed to be in the standard locations:
	//   TSC 0x10
	//   IA32_PMC0 0xC1
	//   IA32_PMC1 0xC2
	//   IA32_PMC2 0xC3
	//   IA32_PMC3 0xC4
	//   (plus the next 4, if HT is disabled)
	//   IA32_FIXED_CTR0 0x309 INSTR_RETIRED.ANY
	//   IA32_FIXED_CTR1 0x30a CPU_CLK_UNHALTED.CORE
	//   IA32_FIXED_CTR1 0x30b CPU_CLK_UNHALTED.REF
	sprintf(filename,"core_msr_perfevtsel.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open MSR input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	while (1) {
		rc = fscanf(input_file,"%d %d %lx %u %lx %s",&core_min,&core_max,&msr_num,&counter,&msr_val,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: Core MSR perfevtsel input file contains %d %d 0x%lx %u 0x%lx %s\n",core_min, core_max, msr_num, counter, msr_val, description);
		for (lproc=core_min; lproc<=core_max; lproc++) {
			// fprintf(log_file,"pwrite(msr_fd[%d],0x%0.8x,%ld,0x%lx)\n",core,msr_val,sizeof(msr_val),msr_num);
			rc64 = pwrite(msr_fd[lproc],&msr_val,sizeof(msr_val),msr_num);
			if (rc64 != 8) {
				fprintf(log_file,"ERROR writing to MSR device on lproc %d, write %ld bytes\n",lproc,rc64);
				exit(-1);
			}
			// use the lproc number to look up the socket (package) number
			// from the arrays defined in the topology.h file
			socket = Package_by_LProc[lproc];
			strncpy(core_event_name[lproc][counter],description,80);
			fprintf(log_file,"DEBUG: socket %u lproc %d counter %d description %s\n",socket,lproc,counter,core_event_name[lproc][counter]);
		}
		// fprintf(log_file,"DEBUG: completed writing Core MSR input value to all core MSR device drivers\n");
	}
	// fprintf(log_file,"DEBUG: Core MSR perfevtsel input file contained %d values\n",i);
	fclose(input_file);



	// Input File #3: Uncore MSRs Control/Config (i.e., not PerfEvtSel) 
	// 		Contains one line per MSR, each line contains 5 fields: CoreMin, CoreMax, MSR, value, description
	fprintf(log_file,"------------------- Input File #3 --- Uncore MSR Control --- TBD -------------\n");
	fprintf(log_file,"------------------- Uncore MSR Control currently done in Setup_SKX_Node.sh script  -------------\n");
	fprintf(log_file,"-------------------    Repeat enabling UBOX Fixed Counter here on each socket -------------\n");
	for (socket=0; socket<NUM_SOCKETS; socket++) {
		core = proc_in_pkg[socket];
		msr_num = U_MSR_PMON_FIXED_CTL;
		msr_val = 0x00400000UL;
		rc64 = pwrite(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != sizeof(msr_val)) {
			fprintf(log_file,"ERROR: failed to write MSR %x on Logical Processor %d", msr_num, core);
			exit(-1);
		}
	}

	fprintf(log_file,"------------------- Input File #4d --- Uncore PCU PerfEvtSel via MSRs -------------\n");

	// Input File #4: Uncore MSRs PerfEvtSel
	// 		Contains one line per MSR, each line contains 5 fields: CoreMin, CoreMax, MSR, value, description

	sprintf(filename,"pcu_perfevtsel.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open MSR input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	while (1) {
		rc = fscanf(input_file,"%d %d %lx %s",&socket,&counter,&msr_val,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: PCU MSR perfevtsel input file contains %d %d 0x%lx %s\n",socket, counter, msr_val, description);
		assert (socket >= 0);
		assert (socket < 2);
		assert (counter >= 0); 
		assert (counter < 4); 
		core = proc_in_pkg[socket];
		msr_num = PCU_MSR_PMON_CTL + counter;
		rc64 = pwrite(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != 8) {
			fprintf(log_file,"ERROR writing to MSR device on core %d, write %ld bytes\n",core,rc64);
			exit(-1);
		}
		strncpy(pcu_event_name[socket][counter],description,80);
	}
	fprintf(log_file,"DEBUG: PCU MSR perfevtsel input file contained %d values\n",i);
	fclose(input_file);

	fprintf(log_file,"------------------- Input File #4e --- Uncore CHA PerfEvtSel via MSRs -------------\n");
	sprintf(filename,"cha_perfevtsel.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open MSR input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	// ugly hack here -- allow input to define 6 counters control inputs -- 0-3 are performance counters, 4-5 are filters.
	// But I only read 4 counters laters, since the filters are input-only.
	// TO DO:  make sure that the output file contains the filter data as well as the counter data.
	while (1) {
		rc = fscanf(input_file,"%d %d %d %lx %s",&socket,&cha,&counter,&msr_val,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: CHA MSR perfevtsel input file contains %d %d %d 0x%lx %s\n",socket, cha, counter, msr_val, description);
		assert (socket >= 0);
		assert (socket < 2);
		assert (cha >= 0);
		assert (cha < NUM_CHA_BOXES);
		assert (counter >= 0); 
		assert (counter < NUM_CHA_CONTROLS); 		// address filter0 and filter1 as counters 4-5
		core = proc_in_pkg[socket];
		msr_num = CHA_MSR_PMON_CTL_BASE + (0x10 * cha) + counter;
		rc64 = pwrite(msr_fd[core],&msr_val,sizeof(msr_val),msr_num);
		if (rc64 != 8) {
			fprintf(log_file,"ERROR writing to MSR device on core %d, write %ld bytes\n",core,rc64);
			exit(-1);
		}
		strncpy(cha_event_name[socket][cha][counter],description,80);
	}
	fprintf(log_file,"DEBUG: CHA MSR perfevtsel input file contained %d values\n",i);
	fclose(input_file);



	fprintf(log_file,"------------------- Input File #4 --- other Uncore MSR PerfEvtSel --- TBD -------------\n");

#if 0
	// Input File #5: PCI Configuration space Uncore Control/Config values
	//        one line per value, each line contains 6 fields: bus, device, function, offset, value, description
	//
	// NOTE: I am not using this file now -- I just need to make sure that the FRZ bit in each of the box control
	//       registers is clear (e.g., bit 8 at offset 0xF4 in the IMC performance counter devices), and I can
	//       handle that in the Setup_*_Node.sh script if I run across problems.
	//       I don't want to mess with it here because there are some bits in the IMC PMON_BOX_CTL that read as 0,
	//       but which the Uncore Performance Monitoring Guide says must be written as 1.  (e.g., page 109).
	//
	sprintf(filename,"uncore_pci_control.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open Uncore PCI Control/Config file %s\n",strerror(errno),filename);
		exit(-1);
	}
	int pkg_min,pkg_max;
	i = 0;
	while (1) {
		rc = fscanf(input_file,"%x %x %x %x %x %s",&bus,&device,&function,&offset,&value,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: Uncore PCI Control/Config input file contains %#x %#x %#x %#x %#x %s\n",bus,device,function,offset,value,description);
		index = PCI_cfg_index(bus, device, function, offset);
		mmconfig_ptr[index] = value;
	}
	fprintf(log_file,"DEBUG: Uncore PCI Control/Config input file contained %d values\n",i);
	fclose(input_file);
#endif









	// Input File #6a: PCI Configuration space Uncore HA PerfEvtSel selections
	//        one line per value, each line contains 6 fields: bus, device, function, ctl_offset, ctr_offset, value, description
	// NOTE!!! Not used on SKX -- HA functionality has been merged with LLC functionality into CHA boxes

#if 0
	sprintf(filename,"ha_perfevtsel.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open Uncore PCI PerfEvtSel input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	while (i<100) {
		rc = fscanf(input_file,"%d %d %d %x %s",&socket,&ha,&counter,&value,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: Uncore HA PerfEvtSel input file contains %d %d %d %#x %s\n",socket,ha,counter,value,description);
		bus = HA_BUS_Socket[socket];
		device = HA_Device_Agent[ha];
		function = HA_Function_Agent[ha];
#if 1
		// before using these values, check the DID/VID field at the zero offset on the 
		// page to make sure this is the correct device address
		offset = 0;
		index = PCI_cfg_index(bus, device, function, offset);
		dummy32u = mmconfig_ptr[index];
		if ( (ha == 0) && (dummy32u != 0x2f308086) ) {
			fprintf(log_file,"ERROR: HA0 DID/VID expected 0x2f308086, but found = %x\n",dummy32u);
			exit(-1);
		} else if ( (ha == 1) && (dummy32u != 0x2f388086) ) {
			fprintf(log_file,"ERROR: HA1 DID/VID expected 0x2f388086, but found = %x\n",dummy32u);
			exit(-1);
		}
#endif
		offset = HA_PmonCtl_Offset[counter];
		// fprintf(log_file,"DEBUG: translated bus/device/function/offset values %#x %#x %#x %#x\n",bus,device,function,offset);
		index = PCI_cfg_index(bus, device, function, offset);
		mmconfig_ptr[index] = value;
		// fprintf(log_file,"DEBUG: HA mmconfig_ptr_index = %d\n",index);
		strncpy(ha_event_name[socket][ha][counter],description,32);
	}
	fprintf(log_file,"DEBUG: Uncore PCI PerfEvtSel input file contained %d values\n",i);
	fclose(input_file);
#endif

	// Input File #6b: PCI Configuration space Uncore IMC PerfEvtSel selections
	//        one line per value, each line contains 6 fields: bus, device, function, ctl_offset, ctr_offset, value, description

	sprintf(filename,"imc_perfevtsel.input");
	input_file = fopen(filename,"r");
	if (input_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open Uncore PCI PerfEvtSel input file %s\n",strerror(errno),filename);
		exit(-1);
	}
	i = 0;
	while (i<100) {
		rc = fscanf(input_file,"%d %d %d %d %x %s",&socket,&imc,&subchannel,&counter,&value,&description);
		if (rc == EOF) break;
		i++;
		fprintf(log_file,"DEBUG: Uncore IMC PerfEvtSel input file contains %d %d %d %d 0x%x %s\n",socket,imc,subchannel,counter,value,description);
		channel = 3*imc + subchannel;				// PCI device/function is indexed by channel here (0-5)
		bus = IMC_BUS_Socket[socket];
		device = IMC_Device_Channel[channel];
		function = IMC_Function_Channel[channel];
		offset = IMC_PmonCtl_Offset[counter];
		// fprintf(log_file,"DEBUG: translated bus/device/function/offset values %#x %#x %#x %#x\n",bus,device,function,offset);
		index = PCI_cfg_index(bus, device, function, offset);
		mmconfig_ptr[index] = value;
		strncpy(imc_event_name[socket][channel][counter],description,32);
	}
	fprintf(log_file,"DEBUG: Uncore PCI PerfEvtSel input file contained %d values\n",i);
	fclose(input_file);



	// Get the host name (again), assume that it is of the TACC standard form, and use this as part
	//    of the results output file name so every node can write into the same directory.
	// Standard form is "c591-101.stampede2.tacc.utexas.edu", so truncating at the
	//     first "." is done by writing \0 to character #8.
	len = 100;	
	rc = gethostname(description, len);
	if (rc != 0) {
		fprintf(log_file,"ERROR when trying to get hostname\n");
		exit(-1);
	}
	fprintf(log_file,"HOSTNAME: %s\n",description);
	description[8] = 0;		// assume hostname of the form c581-101.stampede2.tacc.utexas.edu -- truncate after first period

	// NOTE that root (or setuid root) will not be able to write to filesystems with "root-squashing" enabled.

	sprintf(filename,"%s.perfcounts.lua",description);
	results_file = fopen(filename,"w+");
	if (results_file == 0) {
		fprintf(log_file,"ERROR %s when trying to open output file %s\n",strerror(errno),filename);
		exit(-1);
	}
	rc = chown(filename,my_uid,my_gid);
	if (rc == 0) {
		fprintf(log_file,"DEBUG: Successfully changed ownership of output file to uid %d gid %d\n",my_uid,my_gid);
	} else {
		fprintf(stderr,"ERROR: Attempt to change ownership of output file to uid %d gid %d failed -- bailing out\n",my_uid,my_gid);
		exit(-1);
	}
	// put the TSC ratio at the top of the output file -- this won't need to be repeated
	// for each sample
	rc64 = pread(msr_fd[0],&msr_val,sizeof(msr_val),MSR_PLATFORM_INFO);
	TSC_ratio = (msr_val & 0x000000000000ff00L) >> 8;
	fprintf(results_file,"TSC_ratio = %d\n", TSC_ratio);

	// include the number of active cores
	fprintf(results_file,"nr_cpus = %d\n", nr_cpus);


	// get the TSC and gettimeofday once on each node so I can convert TSC to 
	// synchronized wall-clock time.
	// assume both components of tp struct are longs.
	rc64 = rdtscp();
    i = gettimeofday(&tp,&tzp);
	fprintf(results_file,"Reference_TSC = %ld\n", rc64);
	fprintf(results_file,"Reference_WallTime = %ld.%06ld\n", tp.tv_sec,tp.tv_usec);

	// for reference, write the initial contents of IA32_FIXED_CTR_CTRL to see if the
	// external environment has set the AnyThread bit for the Core Fixed-Function Counters
	for (lproc=0; lproc<nr_cpus; lproc++) {
		rc64 = pread(msr_fd[lproc],&msr_val,sizeof(msr_val),IA32_FIXED_CTR_CTRL);
		fprintf(results_file,"IA32_FIXED_CTR_CTRL[%d] = 0x%lx\n", lproc, msr_val);
	}

	// --------------------- SETUP CODE FOR TEMPERATURE, POWER, and THROTTLING ------------------------------------
	// Read the RAPL configuration MSRs and compute the energy, time, and power units.
	// Remember to override the energy_unit for the DRAM domain since
	// this is a Xeon E5 v3...
	//
	// This code is copied from "haswell_monitor.c" with as few changes as 
	// I can get away with....
	//
	// Step 1 -- open the MSR files -- already done above
	//
    // 2. Read the one-time values needed for calculations
    // 2a. Read the "unique" (global) MSR_TEMPERATURE_TARGET
	// assume both sockets are the same, so just read on socket 0
	msr_num = MSR_TEMPERATURE_TARGET;
	if (pread(msr_fd[proc_in_pkg[0]], &msr_val, sizeof msr_val, msr_num) != sizeof (msr_val)) {
		fprintf(log_file,"ERROR: Failed to read MSR_TEMPERATURE_TARGET for core %d\n",proc_in_pkg[socket]);
		exit(-3);
	}
	temp_target = (msr_val & 0x00FF0000)>>16;     // 8 bit field for PROCHOT in degrees C
	fprintf(log_file,"INFO: Package 0 PROCHOT Temperature = %d\n",temp_target);
	// Write the PROCHOT value to the lua file for use in post-processing
    fprintf(results_file,"PROCHOT = %d\n",temp_target);

    // 2b.  Read the RAPL configuration MSRs
    /* Calculate the units used -- safe to assume both sockets are the same!! */
	msr_num = MSR_RAPL_POWER_UNIT;
    if (pread(msr_fd[proc_in_pkg[0]],&result, sizeof(result), msr_num) != sizeof(result)) {
		fprintf(log_file,"ERROR: Failed to read MSR_TEMPERATURE_TARGET for core %d\n",proc_in_pkg[socket]);
		exit(-3);
	}
	// fprintf(log_file,"DEBUG_RAPL: MSR_RAPL_POWER_UNIT (MSR %lx) contains %lx\n",msr_num,result);

    power_unit=pow((double)0.5,(double)(result&0xf));
    pkg_energy_unit=pow((double)0.5,(double)((result>>8)&0x1f));
    time_unit=pow((double)0.5,(double)((result>>16)&0xf));
	dram_energy_unit = 1.0/65536.0;		// This is a constant, not necessarily a particular ratio to the pkg_energy unit
	

    fprintf(log_file,"==================================\n");
    fprintf(log_file,"RAPL: Power unit = %.9fW\n",power_unit);
    fprintf(log_file,"RAPL: Energy unit = %.9fJ\n",pkg_energy_unit);
    tmp = pkg_energy_unit * 4294967296.0;
    fprintf(log_file,"RAPL:    Energy values wrap at %g Joules\n",tmp);
    fprintf(log_file,"RAPL: Time unit = %.9fs\n",time_unit);
    tmp = time_unit * 4294967296.0;
    fprintf(log_file,"RAPL:    Time values wrap at %g seconds\n",tmp);
    fprintf(log_file,"RAPL: NOTE: DRAM Energy Unit manually overridden to 15.3 uJ (1/65536 J)\n");

	msr_num = MSR_PKG_POWER_INFO;
    if (pread(msr_fd[proc_in_pkg[0]],&msr_val, sizeof(msr_val), msr_num) != sizeof(msr_val)) {
		fprintf(log_file,"ERROR: Failed to read PKG_POWER_INFO for core %d\n",proc_in_pkg[socket]);
		exit(-3);
	}
	// fprintf(log_file,"DEBUG_RAPL: MSR_PKG_POWER_INFO (MSR %lx) contains %lx\n",msr_num,msr_val);
    double thermal_spec_power=power_unit*(double)(msr_val&0x7fff);

	// For energy use, I can write out either the low-level counts or the scaled values to the lua
	// results_file.  Handling wrapping is easier if I write out the unmodified counts, so the
	// results_file needs to get the units defined.
    fprintf(results_file,"RAPL_POWER_UNIT = %.9f\n",power_unit);
    fprintf(results_file,"RAPL_PKG_ENERGY_UNIT = %.9f\n",pkg_energy_unit);
    fprintf(results_file,"RAPL_DRAM_ENERGY_UNIT = %.9f\n",dram_energy_unit);
    fprintf(results_file,"RAPL_TIME_UNIT = %.9f\n",time_unit);
    fprintf(results_file,"PACKAGE_TDP = %.6f\n",thermal_spec_power);

	// That is all I need here -- the data reads and writes will go in the read_data routine
	// --------------------- END OF RAPL SETUP CODE FOR POWER & THROTTLING ------------------------------------

	// Duration is now set earlier in main using command-line parameters if present
	//duration.tv_sec = 0;
	//duration.tv_nsec = 100*1000*1000;		// 1,000,000 ns = 1 millisecond
	remainder.tv_sec = 0;				// not used
	remainder.tv_nsec = 0;				// not used
	// fprintf(log_file,"DEBUG: About to call nanosleep with duration of %ld seconds plus %ld nanoseconds\n",duration.tv_sec,duration.tv_nsec);

	sample = 0;
	read_all_counters();
	while (sample < MAX_SAMPLES) {
		nanosleep(&duration,&remainder);
		dummycounter[sample]=dummycounter[sample-1]+10;
		valid=0;				// try to catch cases where the interrupt happens in the middle of the counter reads
		read_all_counters();
		valid=1;
	}
	// Fall-through -- maximum number of samples reached 
	// Process and output all results
	process_all_results();
	exit(0);
}
