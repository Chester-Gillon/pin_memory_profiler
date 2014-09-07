pin_memory_profiler
===================

Test of writing a Pin tool to profile the memory usage of a 3rd party algorithm

Example run command, where FFT_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -- Debug/FFTW_example
fftw_plan_dft_1d returned 0x60bfe0
in=0x7fffe4b91010[1048576] out=0x7fffe4a89010[1048576]
	Command being timed: "setarch x86_64 -R /home/Mr_Halfword/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -- Debug/FFTW_example"
	User time (seconds): 3.40
	System time (seconds): 0.19
	Percent of CPU this job got: 98%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:03.64
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 134560
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 4
	Minor (reclaiming a frame) page faults: 63366
	Voluntary context switches: 28
	Involuntary context switches: 379
	Swaps: 0
	File system inputs: 520
	File system outputs: 2640
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/memory_profile.csv 

The profile is for a 64K point double complex forward FFT, performed out of place.

The 1M input buffer was at address 0x7fffe4b91010 for the run. Looking at the various stages in the memory profile::
a) Allocated during initialisation:
fft_initialise,malloc,size=0x100000,data_ptr=0x7fffe4b91010,caller=fft_initialise

b) Initialised with random values with C code one double at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x7fffe4b91010,end_addr=0x7fffe4c9100f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x4000,8 byte accesses=0x20000

c) Read during each FFT execution. Read one double at a time with increnting addresses. Each double is read once, using 8 byte accesses so no sign of vector instructions:
fft_execute,memory read,start_addr=0x7fffe4b91010,end_addr=0x7fffe4c9100f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x4000,8 byte accesses=0x20000

d) Freed during termination:
fft_free,free,data_ptr=0x7fffe4b91010,size=0x100000,caller=fft_free


The 1M output buffer was at address 0x7fffe4a89010 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,malloc,size=0x100000,data_ptr=0x7fffe4a89010,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x7fffe4a89010,end_addr=0x7fffe4b8900f,size=0x100000,total_bytes_accessed=0x100000,cache_line_decrements=0x4000,8 byte accesses=0x20000

c) *read* 2.15625 times during each FFT execution:
fft_execute,memory read,start_addr=0x7fffe4a89010,end_addr=0x7fffe4b8900f,size=0x100000,total_bytes_accessed=0x228000,cache_line_increments=0x4000,8 byte accesses=0x45000

*written* 3 times during each FFT execution:
fft_execute,memory write,start_addr=0x7fffe4a89010,end_addr=0x7fffe4b8900f,size=0x100000,total_bytes_accessed=0x300000,cache_line_increments=0x4000,8 byte accesses=0x60000

i.e. the output buffer from the FFT is read/written multiple times during the FFT execution. All accesses are individual doubles so no sign of vector instructions.

d) Freed during termination:
fft_free,free,data_ptr=0x7fffe4a89010,size=0x100000,caller=fft_free



Each FFT execution is shown to read a 128K region 2.75 times:
fft_execute,memory read,start_addr=0x7fffe48ba010,end_addr=0x7fffe48da00f,size=0x20000,total_bytes_accessed=0x58000,cache_line_increments=0x800,8 byte accesses=0xb000

Believe that this region at 0x7fffe48ba010 contains the FFT "twiddle factors". Allocated from:
fft_initialise,malloc,size=0x20000,data_ptr=0x7fffe48ba010,caller=fftw_malloc_plain

And freed from:
fft_free,free,data_ptr=0x7fffe48ba010,size=0x20000,caller=fftw_twiddle_awake

