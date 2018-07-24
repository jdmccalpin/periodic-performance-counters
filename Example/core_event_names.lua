core_fixed_events = {"Inst_Retired.Any","CPU_CLK_Unhalted.Core","CPU_CLK_Unhalted.Ref"}

core_events = {}
for lproc=0,95 do
	core_events[lproc] = {"CPU_CLK_UNHALTED.KERNEL","CPU_CLK_UNHALTED.REF_XCLK","INST_RETIRED.KERNEL","OFFCORE_REQUESTS.L3_MISS_DEMAND_DATA_READ"}
end
