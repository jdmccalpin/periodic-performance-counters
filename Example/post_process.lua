-- first try at a lua post-processing program for the "perf_counters.c" 
-- whole-program performance monitoring utility....
--
-- Revised version updated to match revision 1.12 of perf_counters.c
-- which uses different indexing for core counters and different names
-- for many variables.
--
-- COMPLETED FIXING
-- 	core_fixed_counts
--  uncore_ha --> now ha_counts
--  uncore_imc --> now imc_counts
--
-- WORKING ON
--  core_counts
--
-- WAITING TO TEST
--
-- REMAINING TO DO

-- Global settings
-- "verbosity" ranges from 0 (only summary output) to 3 (output everything)
verbosity = 1
MAX_LPROC_NUM = 95


-- helper functions
--
-- Print statements controlled by "verbosity" variable
--  myprint(1,...) prints if the "verbosity" variable is >=1, etc...
function myprint (level, ...)
	if verbosity >= level then
		for i,v in ipairs(arg) do
			print(unpack(arg))
		end
    end
end


--
-- Corrected delta function for 48-bit counters with
-- possible wraparound
function corrected_delta48(after, before)
	if after >= before then
		return after-before
	else
		-- myprint(1,"DEBUG: wrap detected")
		return after-before+(2^48)
	end
end

--
-- Corrected delta function for 32-bit counters with
-- possible wraparound
function corrected_delta32(after, before)
	if after >= before then
		return after-before
	else
		-- myprint(1,"DEBUG: wrap detected")
		return after-before+(2^32)
	end
end


-- Look for input data file name on command line

-- by default, this program will process from the 
-- first to last provided data records.
-- Command-line options can override these
findMin = 1
findMax = 1

if #arg == 0 then
	print("WARNING: no input file name provided! Using default perfcounteroutput.lua")
	inputfile = "perfcounteroutput.lua"
elseif #arg == 1 then
	inputfile = arg[1]
	myprint(3,"INFO: input file requested is  ",inputfile)
elseif #arg == 2 then
	inputfile = arg[1]
	myprint(3,"INFO: input file requested is  ",inputfile)
	MinSample = tonumber(arg[2])
	findMin = 0
elseif #arg == 3 then
	inputfile = arg[1]
	myprint(3,"INFO: input file requested is  ",inputfile)
	MinSample = tonumber(arg[2])
	MaxSample = tonumber(arg[3])
	findMin = 0
	findMax = 0
else
	print("oops -- the number of arguments chosen is not yet supported!")
end

--[[
-- earlier version of argument parsing code -- keep it just in case....
if arg[1] then
	inputfile = arg[1]
	myprint(3,"INFO: input file requested is  ",inputfile)
else
	print("WARNING: no input file name provided! Using default perfcounteroutput.lua")
	inputfile = "perfcounteroutput.lua"
end
--]]




-- First step is to declare the multidimensional tables that will be used
-- by the data that I am going to import.
-- I thought this was going to be easy until I started into it --
-- it looks like it wants me to enumerate each value of each index
-- in order to create a sub-table.  This is easy for the Socket and
-- Box levels, but not for the Description (string) level.  
-- Do I have to parse the input files to get all of the Description
-- strings so that I can create the next level (sample number) of
-- the table?
-- Instead I will just make a list of the event names in the 
-- "*_event_names.lua" files that I will import below.

tsc = {}
walltime = {}
walltime[0] = {}
walltime[1] = {}
IA32_FIXED_CTR_CTRL = {}

ib_recv_bytes = {}
ib_xmit_bytes = {}

core_counts = {}
core_fixed_counts = {}
cum_core_counts = {}
cum_core_fixed_counts = {}
aperf = {}
mperf = {}

ubox_uclk = {}
imc_counts = {}
-- ha_counts = {}
cha_counts = {}
pcu_counts = {}
iio_CBDMA_port1_in_bytes = {}
iio_CBDMA_port1_out_bytes = {}
iio_PCIe0_port1_in_bytes = {}
iio_PCIe0_port1_out_bytes = {}
iio_PCIe2_port0_in_bytes = {}
iio_PCIe2_port0_out_bytes = {}

pkg_temperature = {}
rapl_pkg_energy = {}
rapl_dram_energy = {}
rapl_pkg_throttled = {}
pkg_therm_status = {}
smi_count = {}
pkg_core_perf_limit_reasons = {}
pkg_ring_perf_limit_reasons = {}

dofile("core_event_names.lua")
dofile("imc_event_names.lua")
--dofile("ha_event_names.lua")
dofile("cha_event_names.lua")
dofile("pcu_event_names.lua")

for socket=0,1 do
	ubox_uclk[socket] = {}
	imc_counts[socket] = {}
--	ha_counts[socket] = {}
	cha_counts[socket] = {}
	pcu_counts[socket] = {}
	pkg_temperature[socket] = {}
	rapl_pkg_energy[socket] = {}
	rapl_dram_energy[socket] = {}
	rapl_pkg_throttled[socket] = {}
	pkg_therm_status[socket] = {}
	pkg_core_perf_limit_reasons[socket] = {}
	pkg_ring_perf_limit_reasons[socket] = {}
	smi_count[socket] = {}
	iio_CBDMA_port1_in_bytes[socket] = {}
	iio_CBDMA_port1_out_bytes[socket] = {}
	iio_PCIe0_port1_in_bytes[socket] = {}
	iio_PCIe0_port1_out_bytes[socket] = {}
	iio_PCIe2_port0_in_bytes[socket] = {}
	iio_PCIe2_port0_out_bytes[socket] = {}

	for channel=0,5 do
		imc_counts[socket][channel] = {}
		for _, event in ipairs(imc_events) do
			imc_counts[socket][channel][event] = {}
		end
	end
--	for agent=0,1 do
--		ha_counts[socket][agent] = {}
--		for _, event in ipairs(ha_events) do
--			ha_counts[socket][agent][event] = {}
--		end
--	end
	for cha=0,27 do
		cha_counts[socket][cha] = {}
		for _, event in ipairs(cha_events) do
			cha_counts[socket][cha][event] = {}
		end
	end
	for _, event in ipairs(pcu_events) do
		pcu_counts[socket][event] = {}
	end
end

-- Notes added 2017-05-19:
-- use the MAX_LPROC_NUM for declaring the arrays, then use
-- "nr_cpus-1" (defined in the input file, included below)
-- for the maximum processor number for all of the actual
-- processing loops.  This is not yet fully operational 
-- for working with different processor counts, but it is
-- a start...
for lproc=0,MAX_LPROC_NUM do
	core_counts[lproc] = {}
	core_fixed_counts[lproc] = {}
	cum_core_counts[lproc] = {}
	cum_core_fixed_counts[lproc] = {}
	aperf[lproc] = {}
	mperf[lproc] = {}
	for _, event in ipairs(core_events[lproc]) do
		core_counts[lproc][event] = {}
		cum_core_counts[lproc][event] = 0
	end
	for _, event in ipairs(core_fixed_events) do
		core_fixed_counts[lproc][event] = {}
		cum_core_fixed_counts[lproc][event] = 0
	end

end

-- Load all of the data from the input data file
dofile(inputfile)

if findMax == 1 then
	myprint(0,"Looking through data to find the Maximum Sample number")
	MaxSample = table.maxn(imc_counts[0][0]["CAS_COUNT.READS"])
end
if findMin == 1 then
	myprint(0,"Assuming MinSample = 0")
	MinSample = 0
end
-- myprint(1,"Maximum sample number is ",maxsample)

myprint(0,"Sample range is ",MinSample," to ",MaxSample)

-- ========================= END SETUP -- BEGIN PROCESSING AFTER THIS ==================================

-- These derived metrics use the Fixed-Function Core Counters and so I can assume that the values will always be present

TSC_GHZ = TSC_ratio * 0.1

report_power = 0

if report_power > 0 then
	-- ========= PROCESS Temperature, Energy, and Throttling data by sample ================
	print("----------- Temperature, Power, Throttling by sample --------")
	print("Sample  Time    Socket  Package  Package  Package    DRAM    DRAM   Throttled  Fraction")
	print("number  (sec)   number  Temp(C)  Joules   Watts     Joules  Watts    seconds   Throttled")
	for sample=MinSample+1,MaxSample do
		time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
		for socket=0,1 do
			delta_time = (tsc[sample]-tsc[sample-1])/(TSC_GHZ*1.0e9)
			delta = corrected_delta32(rapl_pkg_energy[socket][sample],rapl_pkg_energy[socket][sample-1])
			pkg_joules = delta * RAPL_PKG_ENERGY_UNIT
			pkg_watts = pkg_joules / delta_time
			delta = corrected_delta32(rapl_dram_energy[socket][sample],rapl_dram_energy[socket][sample-1])
			dram_joules = delta * RAPL_DRAM_ENERGY_UNIT
			dram_watts = dram_joules / delta_time
			delta = corrected_delta32(rapl_pkg_throttled[socket][sample],rapl_pkg_throttled[socket][sample-1])
			throttled_seconds = delta * RAPL_TIME_UNIT
			fraction_throttled = throttled_seconds / delta_time
			io.write(string.format("%4d %8.3f %5d  %7d   %7.1f %7.1f   %7.1f %7.1f   %8.3f %8.3f\n", sample, time, socket, 
				pkg_temperature[socket][sample], pkg_joules, pkg_watts,
				dram_joules, dram_watts, throttled_seconds, fraction_throttled))
		end
	end
end


-- ========================= BEGIN PROCESSING FIXED-FUNCTION CORE COUNTERS ============================
-- I won't print these very often, but these loops compute cumulative values, so the code is always executed
myprint(3,"=========== Fixed-Function Core Counter deltas by step =============")
cum_tsc = 0
for lproc=0,nr_cpus-1 do
	for sample=MinSample+1,MaxSample do
		delta_tsc = tsc[sample] - tsc[sample-1]
		cum_tsc = cum_tsc + delta_tsc
	end
	for _, event in ipairs(core_fixed_events) do
		for sample=MinSample+1,MaxSample do
			-- print("DEBUG: lproc ",lproc," event ",event," sample ",sample)
			delta_tsc = tsc[sample] - tsc[sample-1]
			delta_core_fixed_count = corrected_delta48(core_fixed_counts[lproc][event][sample], core_fixed_counts[lproc][event][sample-1])
			cum_core_fixed_counts[lproc][event] = cum_core_fixed_counts[lproc][event] + delta_core_fixed_count
			if verbosity >= 3 then
				io.write(string.format("lproc %d Backwards_TSC %d Event %s Sample %d BackwardsDelta  %d\n",
								lproc,delta_tsc,event,sample,delta_core_fixed_count))
			end
		end
	end
end

if verbosity > 2 then
	print("======================================================")
	print("======= LOTS OF WORK NEEDED HERE ==================================")
	print("======= Updated 2017-11-01 for SKX Skylake Xeon processors in Stampede2 Dell SKX nodes  ==================================")
	for sample=MinSample+1,MaxSample do
		time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
		for socket=0,1 do
			for thread=0,1 do
				io.write(string.format("%d %8.3f %d %d  ",sample,time,socket,thread))
				for localcore=0,23 do
					lproc = 2*localcore+socket+48*thread
					delta_CPU_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][sample], 
														core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][sample-1])
					delta_REF_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][sample], 
														core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][sample-1])
					delta_INST_RET = corrected_delta48(core_fixed_counts[lproc]["Inst_Retired.Any"][sample], 
														core_fixed_counts[lproc]["Inst_Retired.Any"][sample-1])
					delta_tsc = tsc[sample] - tsc[sample-1]
					AvgCoreGHz = delta_CPU_CLK/delta_REF_CLK * TSC_GHZ
					FractionStalled = 1.0 - delta_REF_CLK/delta_tsc
					IPC = delta_INST_RET / delta_CPU_CLK
					if FractionStalled < 0.0 then FractionStalled = 0.0 end
					-- io.write(string.format("LocalCore %d sample %d AvgGHz %f FractionStalled %f\n",lproc,sample,AvgCoreGHz,FractionStalled))
					io.write(string.format("%.3f/%.3f/%.3f ",AvgCoreGHz,FractionStalled,IPC))
				end
				io.write(string.format("\n"))
			end
		end
	end
	print("======================================================")
end

-- Total cycle counts for all cores/threads
print("======================================================")
print("Total Fixed Function Counts by LPROC from sample ",MinSample," to sample ",MaxSample)
print("socket thread lproc AvgGHz FracStalled IPC")
for socket=0,1 do
	for thread=0,1 do
		for localcore=0,23 do
			lproc = 2*localcore+socket+48*thread
			io.write(string.format("%d %d %d ",socket,thread,lproc))
			delta_CPU_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][MaxSample], 
												core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][MinSample])
			delta_REF_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][MaxSample], 
												core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][MinSample])
			delta_INST_RET = corrected_delta48(core_fixed_counts[lproc]["Inst_Retired.Any"][MaxSample], 
												core_fixed_counts[lproc]["Inst_Retired.Any"][MinSample])
			delta_tsc = tsc[MaxSample] - tsc[MinSample]
			AvgCoreGHz = delta_CPU_CLK/delta_REF_CLK * TSC_GHZ
			FractionStalled = 1.0 - delta_REF_CLK/delta_tsc
			IPC = delta_INST_RET / delta_CPU_CLK
			if FractionStalled < 0.0 then FractionStalled = 0.0 end
			io.write(string.format("%.3f %.3f %.3f\n",AvgCoreGHz,FractionStalled,IPC))
		end
	end
end

-- Split per-thread unhalted reference clock cycle counts into
-- 		"idle","thread 0 only", "thread 1 only", "both threads"
-- and reports results by physical core
-- To split the results into the 4 categories above, I need 4 counter values:
--      "TSC", "REF_CLK.Thread0", "REF_CLK.Thread1", "REF_CLK.AnyThread"
-- The TSC is from the TSC, the REF_CLK values for each thread context come
-- from the fixed-function counters, and the REF_CLK.AnyThread comes from a 
-- programmable counter with the AnyThread bit set.  This last could be run
-- on either thread of a core (since the values should be the same), but for 
-- the moment I am running it on both for convenience.
--
-- The transformation of counter values to the desired usage values is
--         idle = TSC - REF_CLK.AnyThread * TSC_ratio
--         thread0_only = REF_CLK.AnyThread * TSC_ratio - REF_CLK.Thread1
--         thread1_only = REF_CLK.AnyThread * TSC_ratio - REF_CLK.Thread0
--         both = REF_CLK.Thread0 + REF_CLK.Thread1 - REF_CLK.AnyThread * TSC_ratio
-- Note that I have to scale the REF_CLK.AnyThread by the TSC ratio because the
-- programmable event only counts the 100 MHz reference clock and does not scale
-- the result by the TSC_ratio like the fixed-function counters.

threadsplitting = 0
if threadsplitting > 0 then
	print("======================================================")
	print("Thread Utilization Ratios by Physical Core from sample ",MinSample," to sample ",MaxSample)
	-- print("Socket  LocalCore  TSC  RefClk0 RefClk1  RefClkAny  idle  t0_only t1_only both")
	print("Socket  Core  either idle  t0_only  t1_only  both")
	for socket=0,1 do
		for localcore=0,23 do
			lproc0 = 2*localcore+socket
			lproc1 = 2*localcore+socket+48
			io.write(string.format("%3d %3d ",socket,localcore))
			Ref_Clk_Unhalted_0 = corrected_delta48(core_fixed_counts[lproc0]["CPU_CLK_Unhalted.Ref"][MaxSample], 
												   core_fixed_counts[lproc0]["CPU_CLK_Unhalted.Ref"][MinSample])
			Ref_Clk_Unhalted_1 = corrected_delta48(core_fixed_counts[lproc1]["CPU_CLK_Unhalted.Ref"][MaxSample], 
												   core_fixed_counts[lproc1]["CPU_CLK_Unhalted.Ref"][MinSample])
			Ref_Clk_Unhalted_Any0 = TSC_ratio * corrected_delta48(core_counts[lproc0]["CPU_CLK_UNHALTED.REF_XCLK"][MaxSample],
																 core_counts[lproc0]["CPU_CLK_UNHALTED.REF_XCLK"][MinSample])
			Ref_Clk_Unhalted_Any1 = TSC_ratio * corrected_delta48(core_counts[lproc1]["CPU_CLK_UNHALTED.REF_XCLK"][MaxSample],
																 core_counts[lproc1]["CPU_CLK_UNHALTED.REF_XCLK"][MinSample])
			if Ref_Clk_Unhalted_Any0 > Ref_Clk_Unhalted_Any1 then
				Ref_Clk_Unhalted_Any = Ref_Clk_Unhalted_Any0
			else
				Ref_Clk_Unhalted_Any = Ref_Clk_Unhalted_Any1
			end
			-- print("Ref_Clk_Unhalted 0, 1, diff: ",Ref_Clk_Unhalted_Any0,Ref_Clk_Unhalted_Any1,Ref_Clk_Unhalted_Any0-Ref_Clk_Unhalted_Any1)
			delta_tsc = tsc[MaxSample] - tsc[MinSample]
			-- io.write(string.format("%d %d %d %d ",delta_tsc,Ref_Clk_Unhalted_0,Ref_Clk_Unhalted_1,Ref_Clk_Unhalted_Any))
			idle = (delta_tsc - Ref_Clk_Unhalted_Any) / delta_tsc
			t0_only = (Ref_Clk_Unhalted_Any - Ref_Clk_Unhalted_1) / delta_tsc
			t1_only = (Ref_Clk_Unhalted_Any - Ref_Clk_Unhalted_0) / delta_tsc
			either = t0_only + t1_only
			both = (Ref_Clk_Unhalted_0 + Ref_Clk_Unhalted_1 - Ref_Clk_Unhalted_Any) / delta_tsc
			io.write(string.format("%6.3f %6.3f %6.3f %6.3f %6.3f\n",either,idle,t0_only,t1_only,both))
		end
	end
end

print("======================================================")
	
-- [Optional] Print Fixed-Function Core Counter values for each socket and core accumulated over all steps
if verbosity >= 1 then
	print("=========== Fixed-Function Core Counter Cumulative Deltas for samples ",MinSample," to ",MaxSample,"=============")
	for lproc=0,nr_cpus-1 do
		for _, event in ipairs(core_fixed_events) do
			io.write(string.format("Lproc %d Elapsed_TSC %d Event %s TotalDelta %d\n",lproc,cum_tsc,event,cum_core_fixed_counts[lproc][event]))
		end
	end
	print("=========== Fixed-Function Core Counter Per Socket Cumulative Deltas =============")
end

-- [Optional] Print Fixed-Function Core Counter values for each socket accumulated over all cores and all steps
if verbosity >= 1 then
	print("=========== Fixed-Function Core Counter Cumulative Deltas for samples ",MinSample," to ",MaxSample,"=============")
	for _, event in ipairs(core_fixed_events) do
		sum = 0
		for lproc=0,nr_cpus-1 do
			sum = sum + cum_core_fixed_counts[lproc][event]
		end
		io.write(string.format("Elapsed_TSC %d Event %s AllCoreDelta %d\n",cum_tsc,event,sum))
	end
end

export = 0

if export > 0 then
	print("======================================================")
	print("======= CPU_CLK_Unhalted.Core for import into Excel ============")
	for sample=MinSample+1,MaxSample do
		time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
		io.write(string.format("%d %8.3f ",sample,time))
		for lproc=0,nr_cpus-1 do
			delta_CPU_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][sample], 
											  core_fixed_counts[lproc]["CPU_CLK_Unhalted.Core"][sample-1])
			io.write(string.format("%d ",delta_CPU_CLK))
		end
		io.write(string.format("\n"))
	end
	print("======= CPU_CLK_Unhalted.Ref for import into Excel ============")
	for sample=MinSample+1,MaxSample do
		time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
		io.write(string.format("%d %8.3f ",sample,time))
		for lproc=0,nr_cpus-1 do
			delta_REF_CLK = corrected_delta48(core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][sample], 
											  core_fixed_counts[lproc]["CPU_CLK_Unhalted.Ref"][sample-1])
			io.write(string.format("%d ",delta_REF_CLK))
		end
		io.write(string.format("\n"))
	end
	print("======= Inst_Retired.Any for import into Excel ============")
	for sample=MinSample+1,MaxSample do
		time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
		io.write(string.format("%d %8.3f ",sample,time))
		for lproc=0,nr_cpus-1 do
			delta_INST_RET = corrected_delta48(core_fixed_counts[lproc]["Inst_Retired.Any"][sample], 
											   core_fixed_counts[lproc]["Inst_Retired.Any"][sample-1])
			io.write(string.format("%d ",delta_INST_RET))
		end
		io.write(string.format("\n"))
	end
	print("======================================================")
end

-- ========================= END PROCESSING FIXED-FUNCTION CORE COUNTERS ============================


-- ========================= BEGIN PROCESSING PROGRAMMABLE CORE COUNTERS ============================
-- I won't print these very often, but these loops compute cumulative values, so the code is always executed

skip_core_counts = 0
for _, event in ipairs(core_events[0]) do
	print("Event from core_events[0] is ",event)
	if (core_counts[0][0] == nil) then
		skip_core_counts = 1
		break
	end
end
skip_core_counts = 0
if (skip_core_counts == 0) then
	myprint(3,"=========== Programmable Core Counter deltas by step =============")
	cum_tsc = 0
	for lproc=0,nr_cpus-1 do
		for sample=MinSample+1,MaxSample do
			delta_tsc = tsc[sample] - tsc[sample-1]
			cum_tsc = cum_tsc + delta_tsc
		end
		for _, event in ipairs(core_events[lproc]) do
			for sample=MinSample+1,MaxSample do
				-- print("DEBUG: lproc ",lproc," event ",event," sample ",sample)
				delta_tsc = tsc[sample] - tsc[sample-1]
				delta_core_count = corrected_delta48(core_counts[lproc][event][sample], core_counts[lproc][event][sample-1])
				cum_core_counts[lproc][event] = cum_core_counts[lproc][event] + delta_core_count
				if verbosity >= 3 then
					io.write(string.format("LogicalProcessor %d Backwards_TSC %d Event %s Sample %d BackwardsDelta  %d\n",
								lproc,delta_tsc,event,sample,delta_core_count))
				end
			end
		end
	end

-- [Optional] Print Programmable Core Counter values for each socket and core accumulated over all steps
	if verbosity >=1 then
		print("=========== Programmable Core Counter Cumulative Deltas =============")
		for lproc=0,nr_cpus-1 do
			for _, event in ipairs(core_events[lproc]) do
				io.write(string.format("LogicalProcessor %d Elapsed_TSC %d Event %s TotalDelta %d\n",lproc,cum_tsc,event,cum_core_counts[lproc][event]))
			end
		end
	end
end

-- [Optional] Print Programmable Core Counter values for each socket accumulated over all cores and all steps
--if verbosity >=1 then
--	print("=========== Programmable Core Counter Per Socket Cumulative Deltas =============")
--	for _, event in ipairs(core_events[lproc]) do
--		sum = 0
--		for lproc=0,nr_cpus-1 do
--			sum = sum + cum_core_counts[lproc][event]
--		end
--		io.write(string.format("Elapsed_TSC %d Event %s AllCoreDelta %d\n",cum_tsc,event,sum))
--	end
--end
--
-- TO DO: ADDITIONAL PROGRAMMABLE CORE COUNTER PROCESSING DEPENDS ON WHAT EVENTS ARE PROGRAMMED.
--        I CAN EASILY TEST TO SEE IF VALUES ARE AVAILABLE, SO THIS COULD BE A GROWING LIST OF 
--        DERIVED METRICS THAT ARE ONLY COMPUTED IF THE REQUIRED INPUT VALUES ARE AVAILABLE.

--print("DEBUG: Testing to see what core events are present in the input file.....")
--for _, event in ipairs(core_events[lproc]) do
--	if core_counts[0][0][event] then
--		print("The ",event," programmable core event is available")
--	else
--		print("The ",event," programmable core event is NOT available")
--	end
--end
--event = "FOO.BAR"
--if core_counts[0][0][event] then
--	print("The ",event," programmable core event is available")
--else
--	print("The ",event," programmable core event is NOT available")
--end

-- ========================= END PROCESSING PROGRAMMABLE CORE COUNTERS ============================

-- ------- Uncore counters: CHA
print("Cumulative CHA counts from sample ",MinSample," to sample ",MaxSample)
for socket=0,1 do
	for cha=0,23 do
		for _, event in ipairs(cha_events) do
			-- print ("debug: socket ",socket," cha ",cha," event ",event)
			delta_cha_count = corrected_delta48(cha_counts[socket][cha][event][MaxSample], cha_counts[socket][cha][event][MinSample])
			io.write(string.format("Socket %d cha %d Event %s TotalDelta %d\n",socket,cha,event,delta_cha_count))
		end
	end
end
		



-- ------- Uncore counters: IMC and HA -------------
global_imc_reads = 0
global_imc_writes = 0
global_imc_activates = 0
global_imc_conflicts = 0
socket_imc_reads = {}
socket_imc_writes = {}
socket_imc_activates = {}
socket_imc_conflicts = {}
--global_ha_local_reads = 0
--global_ha_remote_reads = 0
--global_ha_local_writes = 0
--global_ha_remote_writes = 0
--socket_ha_local_reads = {}
--socket_ha_remote_reads = {}
--socket_ha_total_reads = {}
--socket_ha_local_writes = {}
--socket_ha_remote_writes = {}
--socket_ha_total_writes = {}
for socket=0,1 do
	socket_imc_reads[socket] = 0
	socket_imc_writes[socket] = 0
	socket_imc_activates[socket] = 0
	socket_imc_conflicts[socket] = 0
	for channel=0,5 do
		for sample=MinSample+1,MaxSample do
			-- accumulate reads by socket
			delta_imc_reads = corrected_delta48(imc_counts[socket][channel]["CAS_COUNT.READS"][sample], imc_counts[socket][channel]["CAS_COUNT.READS"][sample-1])
			socket_imc_reads[socket] = socket_imc_reads[socket] + delta_imc_reads
			global_imc_reads = global_imc_reads + delta_imc_reads
			-- accumulate writes by socket
			delta_imc_writes = corrected_delta48(imc_counts[socket][channel]["CAS_COUNT.WRITES"][sample], imc_counts[socket][channel]["CAS_COUNT.WRITES"][sample-1])
			socket_imc_writes[socket] = socket_imc_writes[socket] + delta_imc_writes
			global_imc_writes = global_imc_writes + delta_imc_writes
			-- accumulate activates by socket
			delta_imc_activates = corrected_delta48(imc_counts[socket][channel]["ACT.ALL"][sample], imc_counts[socket][channel]["ACT.ALL"][sample-1])
			socket_imc_activates[socket] = socket_imc_activates[socket] + delta_imc_activates
			global_imc_activates = global_imc_activates + delta_imc_activates
			-- accumulate conflicts by socket
			delta_imc_conflicts = corrected_delta48(imc_counts[socket][channel]["PRE_COUNT.MISS"][sample], imc_counts[socket][channel]["PRE_COUNT.MISS"][sample-1])
			socket_imc_conflicts[socket] = socket_imc_conflicts[socket] + delta_imc_conflicts
			global_imc_conflicts = global_imc_conflicts + delta_imc_conflicts
		end
	end
end

showbandwidth = 0
if showbandwidth > 0 then
	print("======================================================")
	print("Time Series of Memory BW (GB/s) from IMC and HA units.")
	for socket=0,1 do
		io.write(string.format("--- Socket %d:\n",socket))
		print("#   Time(s)        IMC RD/WR GB/s  (%Hit/%Miss/%Conf)           HA RD/WR GB/s    (%LocRD/%LocWR)")
		
		for sample=MinSample+1,MaxSample do
			time = (tsc[sample]-tsc[0])/(TSC_GHZ*1.0e9)
			imc_read_bytes = 0
			imc_write_bytes = 0
			imc_ACTIVATE = 0
			imc_PAGE_CONFLICT = 0
			io.write(string.format("%d %8.3f       ",sample,time))
			for channel=0,5 do
				delta_imc_reads = corrected_delta48(imc_counts[socket][channel]["CAS_COUNT.READS"][sample], imc_counts[socket][channel]["CAS_COUNT.READS"][sample-1])
				imc_read_bytes = imc_read_bytes + delta_imc_reads*64
				delta_imc_writes = corrected_delta48(imc_counts[socket][channel]["CAS_COUNT.WRITES"][sample], imc_counts[socket][channel]["CAS_COUNT.WRITES"][sample-1])
				imc_write_bytes = imc_write_bytes + delta_imc_writes*64
				delta = corrected_delta48(imc_counts[socket][channel]["ACT.ALL"][sample], imc_counts[socket][channel]["ACT.ALL"][sample-1])
				imc_ACTIVATE = imc_ACTIVATE + delta
				delta = corrected_delta48(imc_counts[socket][channel]["PRE_COUNT.MISS"][sample], imc_counts[socket][channel]["PRE_COUNT.MISS"][sample-1])
				imc_PAGE_CONFLICT = imc_PAGE_CONFLICT + delta
			end
			delta_time = (tsc[sample]-tsc[sample-1])/(TSC_GHZ*1.0e9)
			imc_read_BW = imc_read_bytes / delta_time / 1e9;
			imc_write_BW = imc_write_bytes / delta_time / 1e9;
			cas_count = (imc_read_bytes + imc_write_bytes)/64			-- easier than accumulating it separately
			page_conflict_rate = imc_PAGE_CONFLICT / cas_count
			page_miss_rate = (imc_ACTIVATE - imc_PAGE_CONFLICT) / cas_count
			page_hit_rate = 1.0 - page_miss_rate - page_conflict_rate

			io.write(string.format("%7.2f %7.2f   (%5.1f/%5.1f/%5.1f) ",imc_read_BW,imc_write_BW,100.0*page_hit_rate,100.0*page_miss_rate,100.0*page_conflict_rate))
			io.write(string.format("        "))
			

--			ha_local_read_bytes = 0
--			ha_local_write_bytes = 0
--			ha_remote_read_bytes = 0
--			ha_remote_write_bytes = 0
--			ha_total_read_bytes = 0
--			ha_total_write_bytes = 0
--			for ha=0,1 do
--				delta = corrected_delta48(ha_counts[socket][ha]["REQUESTS.READS_LOCAL"][sample], ha_counts[socket][ha]["REQUESTS.READS_LOCAL"][sample-1])
--				ha_local_read_bytes = ha_local_read_bytes + delta*64
--				delta = corrected_delta48(ha_counts[socket][ha]["REQUESTS.WRITES_LOCAL"][sample], ha_counts[socket][ha]["REQUESTS.WRITES_LOCAL"][sample-1])
--				ha_local_write_bytes = ha_local_write_bytes + delta*64
--				delta = corrected_delta48(ha_counts[socket][ha]["REQUESTS.READS_REMOTE"][sample], ha_counts[socket][ha]["REQUESTS.READS_REMOTE"][sample-1])
--				ha_remote_read_bytes = ha_remote_read_bytes + delta*64
--				delta = corrected_delta48(ha_counts[socket][ha]["REQUESTS.WRITES_REMOTE"][sample], ha_counts[socket][ha]["REQUESTS.WRITES_REMOTE"][sample-1])
--				ha_remote_write_bytes = ha_remote_write_bytes + delta*64
--			end
--			delta_time = (tsc[sample]-tsc[sample-1])/(TSC_GHZ*1.0e9)
--			ha_total_read_bytes = ha_local_read_bytes + ha_remote_read_bytes
--			ha_total_write_bytes = ha_local_write_bytes + ha_remote_write_bytes
--			ha_local_read_BW = ha_local_read_bytes / delta_time / 1e9
--			ha_local_write_BW = ha_local_write_bytes / delta_time / 1e9
--			ha_remote_read_BW = ha_remote_read_bytes / delta_time / 1e9
--			ha_remote_write_BW = ha_remote_write_bytes / delta_time / 1e9
--			ha_total_read_BW = ha_local_read_BW + ha_remote_read_BW
--			ha_total_write_BW = ha_local_write_BW + ha_remote_write_BW
--			fraction_local_read = ha_local_read_bytes / ha_total_read_bytes
--			fraction_local_write = ha_local_write_bytes / ha_total_write_bytes
--			io.write(string.format("%7.2f %7.2f   (%5.1f/%5.1f)",ha_total_read_BW,ha_total_write_BW,100.0*fraction_local_read,100.0*fraction_local_write))
--			io.write(string.format("\n"))
		end
	end
	print("======================================================")
end

-- global DRAM page hit/miss/conflict by socket
print("======================================================")
print("Cumulative DRAM Stats from sample ",MinSample," to sample ",MaxSample)
global_cas_count = global_imc_reads + global_imc_writes
global_page_conflict_rate = global_imc_conflicts / global_cas_count
global_page_miss_rate = (global_imc_activates - global_imc_conflicts) / global_cas_count
global_page_hit_rate = 1.0 - global_page_miss_rate - global_page_conflict_rate
socket_cas_count = {}
socket_page_conflict_rate = {}
socket_page_miss_rate = {}
socket_page_hit_rate = {}
for socket=0,1 do
	socket_cas_count[socket] = socket_imc_reads[socket] + socket_imc_writes[socket]
	socket_page_conflict_rate[socket] = socket_imc_conflicts[socket] / socket_cas_count[socket]
	socket_page_miss_rate[socket] = (socket_imc_activates[socket] - socket_imc_conflicts[socket]) / socket_cas_count[socket]
	socket_page_hit_rate[socket] = 1.0 - socket_page_miss_rate[socket] - socket_page_conflict_rate[socket]
end
io.write(string.format("      Global                 Socket 0                Socket 1\n"))
io.write(string.format(" Hits  Misses Conflicts   Hits Misses Conflicts   Hits Misses Conflicts\n"))
io.write(string.format("%6.3f %6.3f %6.3f     ",global_page_hit_rate,global_page_miss_rate,global_page_conflict_rate))
for socket=0,1 do
	io.write(string.format("%6.3f %6.3f %6.3f    ",socket_page_hit_rate[socket],socket_page_miss_rate[socket],socket_page_conflict_rate[socket]))
end
io.write(string.format("\n"))
print("======================================================")

-- homeagentexists = 0
-- if homeagentexists > 0 then
-- 
-- 	for socket=0,1 do
-- 		socket_ha_local_reads[socket] = 0
-- 		socket_ha_remote_reads[socket] = 0
-- 		socket_ha_total_reads[socket] = 0
-- 		for ha=0,1 do
-- 			for sample=MinSample+1,MaxSample do
-- 				delta_ha_local_reads = corrected_delta48(ha_counts[socket][ha]["REQUESTS.READS_LOCAL"][sample], ha_counts[socket][ha]["REQUESTS.READS_LOCAL"][sample-1])
-- 				delta_ha_remote_reads = corrected_delta48(ha_counts[socket][ha]["REQUESTS.READS_REMOTE"][sample], ha_counts[socket][ha]["REQUESTS.READS_REMOTE"][sample-1])
-- 				global_ha_local_reads = global_ha_local_reads + delta_ha_local_reads
-- 				global_ha_remote_reads = global_ha_remote_reads + delta_ha_remote_reads
-- 				socket_ha_local_reads[socket] = socket_ha_local_reads[socket] + delta_ha_local_reads
-- 				socket_ha_remote_reads[socket] = socket_ha_remote_reads[socket] + delta_ha_remote_reads
-- 				socket_ha_total_reads[socket] = socket_ha_total_reads[socket] + delta_ha_local_reads + delta_ha_remote_reads
-- 			end
-- 		end
-- 	end
-- 	global_ha_reads = global_ha_local_reads + global_ha_remote_reads
-- end


for socket=0,1 do
	io.write(string.format("Socket %d IMC reads       %d\n",socket,socket_imc_reads[socket]))
	-- io.write(string.format("Socket %d HA local reads  %d\n",socket,socket_ha_local_reads[socket]))
	-- io.write(string.format("Socket %d HA remote reads %d\n",socket,socket_ha_remote_reads[socket]))
	-- io.write(string.format("Socket %d HA total reads  %d\n",socket,socket_ha_total_reads[socket]))
end
-- io.write(string.format("Global IMC Reads %d, Global HA Reads %d\n",global_imc_reads,global_ha_reads))
io.write(string.format("Global IMC Reads %d\n",global_imc_reads))

--if homeagentexists > 0 then
--	for socket=0,1 do
--		socket_ha_local_writes[socket] = 0
--		socket_ha_remote_writes[socket] = 0
--		socket_ha_total_writes[socket] = 0
--		for ha=0,1 do
--			for sample=MinSample+1,MaxSample do
--				delta_ha_local_writes = corrected_delta48(ha_counts[socket][ha]["REQUESTS.WRITES_LOCAL"][sample], ha_counts[socket][ha]["REQUESTS.WRITES_LOCAL"][sample-1])
--				delta_ha_remote_writes = corrected_delta48(ha_counts[socket][ha]["REQUESTS.WRITES_REMOTE"][sample], ha_counts[socket][ha]["REQUESTS.WRITES_REMOTE"][sample-1])
--				global_ha_local_writes = global_ha_local_writes + delta_ha_local_writes
--				global_ha_remote_writes = global_ha_remote_writes + delta_ha_remote_writes
--				socket_ha_local_writes[socket] = socket_ha_local_writes[socket] + delta_ha_local_writes
--				socket_ha_remote_writes[socket] = socket_ha_remote_writes[socket] + delta_ha_remote_writes
--				socket_ha_total_writes[socket] = socket_ha_total_writes[socket] + delta_ha_local_writes + delta_ha_remote_writes
--			end
--		end
--	end
--	global_ha_writes = global_ha_local_writes + global_ha_remote_writes
--end


for socket=0,1 do
	io.write(string.format("Socket %d IMC writes       %d\n",socket,socket_imc_writes[socket]))
	-- io.write(string.format("Socket %d HA local writes  %d\n",socket,socket_ha_local_writes[socket]))
	-- io.write(string.format("Socket %d HA remote writes %d\n",socket,socket_ha_remote_writes[socket]))
	-- io.write(string.format("Socket %d HA total writes  %d\n",socket,socket_ha_total_writes[socket]))
end
-- io.write(string.format("Global IMC writes %d, Global HA writes %d\n",global_imc_writes,global_ha_writes))
io.write(string.format("Global IMC writes %d\n",global_imc_writes))
