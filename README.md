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

