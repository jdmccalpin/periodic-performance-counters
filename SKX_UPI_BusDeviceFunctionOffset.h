// ================ Machine-Dependent Uncore Performance Monitor Locations =================
// These are the most common bus/device/function locations for the UPI link-layer counters on 
// Xeon Platinum 8160 (Skylake, SKX)
// These are set up to work on the TACC Stampede2 SKX nodes....
//
// Note that the PmonCtl offsets are for programmable counters 0-3.
// Note that the PmonCtr offsets are for the bottom 32 bits of a 48 bit counter in a
//     64-bit field.  The four offsets are for the programmable counters 0-3.

int UPI_BUS_Socket[2] = {0x5d, 0xd7};
int UPI_Device_Channel[3] = {0x0e, 0x0f, 0x10};
int UPI_Function_Channel[3] = {0x0, 0x0, 0x0};
int UPI_PmonCtl_Offset[4] = {0x350, 0x358, 0x360, 0x368}; 
int UPI_PmonCtr_Offset[4] = {0x318, 0x320, 0x328, 0x330};

// ================ End of Machine-Dependent Uncore Performance Monitor Locations =================
