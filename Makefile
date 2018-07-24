CC = icc
CFLAGS = -g  -DINFINIBAND
SRCS = perf_counters.c low_overhead_timers.c 
OBJS = perf_counters.o low_overhead_timers.o 

INCLUDES = SKX_IMC_BusDeviceFunctionOffset.h  SKX_UPI_BusDeviceFunctionOffset.h MSR_defs.h low_overhead_timers.h topology.h MSR_ArchPerfMon_v3.h MSR_Architectural.h

perf_counters: $(OBJS) $(INCLUDES)
	$(CC) $(CFLAGS) $(OBJS) -o perf_counters -lm

clean:
	rm -f perf_counters $(OBJS)
