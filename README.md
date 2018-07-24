# periodic-performance-counters

This project contains the bits and pieces that I use to periodically collect hardware performance counters on Intel Skylake Xeon systems.

The goal is to collect as many counters as possible with very low overhead.  The current implementation collects over 1100 performance counter values on a 2-socket Xeon Platinum 8160 system (24 cores/48 threads per socket), with a runtime overhead of a bit over 1% of one logical processor at a sample interval of one second.

The main program is launched in the background, where it reads its input configuration files, programs the performance counters, then goes into a loop of reading the performance counters then sleeping for a command-line selectable interval (default 1 second).  This repeats until the code receives a SIGCONT signal, or reaches its static array limit (default 10,000 samples).  Upon receiving the signal, the code does a final read of the performance counters, then writes all the collected counter values into a text output file.  For multi-node runs, the program is run separately on each node, and the use of the host name as part of the output file name keeps the data separate for each node.

The output file consists of assignment statements, compatible with lua or python, that can be imported into a post-processing script, or processed with standard tools such as awk, grep, sed, etc.

The code was originally developed using Intel Xeon E5-2690 v3 processors (Haswell EP), and some of that code is still present, but is almost certainly not functional.

## Contents and Structure

The main program is almost completely self-contained in `perf_counters.c`, with a few utility functions in `low_overhead_timers.c`.

A number of header files are also used.  These are mostly definitions of Machine-Specific-Registers related to the performance counters, but there are a few header files with PCI bus/device/function information for the uncore performance counters that are accessed via PCI configuration space, and there is a file containing the mapping of logical processor numbers to package and physical core numbers (`topology.h`).  

There are a set of performance counter control files with the file extension `.input`.   These are text files that are read at runtime by `perf_counters` and used to define the specific performance counter events to be collected.   The interpretation of the input file syntax should be easy to follow from the source code (look for the input file name -- it occurs in an `sprintf` statement immediately before the section of code that opens and reads each file).

## Post-Processing (in Examples subdirectory)

The lua program `post_process.lua` provides a way to post-process the output files.  It uses the lua `dofile()` function to import a set of lua files containing the performance counter event names.  The files `*_event_names.lua` should be modified so the counter names match the names in the `*.input` files.   The internal structure of `post_process.lua` is a horrible mess, but the first ~250 lines are setup and array definition/instantiation that are likely to be useful.
The remaining 500 lines contain post-processing blocks for the various performance counters, computing sample-to-sample deltas for each performance counter (correcting for overflow/wraparound), computing sums for physical cores, sockets, etc, and computing time-averaged values such as average processor utilization, average frequency, average instructions per cycle, etc.

An important caveat that I discovered about lua -- the interpreter does not allow more than 256k variables to be defined in a function.  When you are working with output files that are longer than 256k lines, the script `Chunkify_Lua_files.sh` uses the `split` utility to break up the output file into as many functions as required, with each function not exceeding 100,000 statements.   "Chunkifying" the output files in this way allows lua programs to import arbitrarily large files, and does not interfere with ad hoc processing using awk/grep/sed, etc.

### Example post-processing

The file `Example/c591-803.perfcounts.lua` contains output for approximately 59 seconds.  A multithreaded copy of the STREAM benchmark was started at about 18 seconds into the sampling, and ended at about 53 seconds into the sampling.  

The command `lua post_process.lua c591-803.perfcounts.lua 19 51 > output_samples_19-51.txt` runs the sample post-processing on the "active" period of this short data set. The top section (lines 7-103) shows the average frequency, utilization, and instructions per cycle for the 96 logical processors. 

An example of "ad hoc" post-processing can be seen in the file `imc_0_0_deltas.txt`.   This was produced using a simple grep and awk pipe to extract the memory controller CAS_COUNT.READS event for socket 0, channel 0, and print the deltas:
`grep "^imc_counts\[0\]\[0\]\[\"CAS_COUNT.READS\"\]"  c591-803.perfcounts.lua  | awk '{print $3}' | awk '{print $1-s; s=$1}' > imc_0_0_deltas.txt`
The values of around 152,386,045 during the STREAM run correspond to 152 million cache lines per second, or about 9.79 GB/s read bandwidth on this channel.  The other channels give almost identical results, for a net of 58.7 GB/s read bandwidth on socket 0.  STREAM has 4 writes for every 6 reads, for a socket read+write bandwidth of 97.9 GB/s and a two-socket bandwidth of 195.8 GB/s.  This is very close to the bandwidth reported by STREAM of 190 GB/s to 200 GB/s for this test.

## Porting Notes -- preliminary

The core performance counter infrastructure is the same across almost all Intel processors, so this will require minimal intervention.   The default code configuration statically defines the maximum number of logical processors as 112, which is enough for a two-socket 28-core Xeon Scalable Processor with HyperThreading enabled.  For larger systems, the preprocessor definitions starting at line 24 of `perf_counters.c` will need review and possible modification.

The code opens the `/dev/cpu/*/msr` device driver on each logical processor and leaves that driver open for the duration of the run.  This requires root privileges on most systems.  The MSR device drivers allow the code to enable, program, and read the core performance counters on each core, as well as to read a large number of additional configuration, status, and power (RAPL) registers in each socket.  Many of the "uncore" performance counters are also programmed and accessed via MSRs -- the "Caching and Home Agent" (CHA) counters, and "Power Control Unit" (PCU) counters are currently implemented.  The QPI/UPI counters are also controlled and accessed via MSRs, but have not yet been implemented.

The "Integrated Memory Controller" (IMC) counters are programmed and accessed via PCI configuration space.   Although there are device drivers in Linux to read/write this space, the `perf_counters` code uses memory-mapped accesses as a lower-overhead alternative.  The code opens `/dev/mem` and performs an `mmap()` at the base of PCI configuration space.  This 256 MiB region is often located at address 0x80000000 -- this value is hard-coded into the variable `mmconfig_base` and should be reviewed when porting to any other system.  (The code checks the Skylake Xeon Vendor ID (VID) and Device ID (DID) for bus 0, device 5, function 0, and will abort if the expected value is not found.)

