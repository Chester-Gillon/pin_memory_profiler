pin_memory_profiler
===================

Test of writing a Pin tool to profile the memory usage of a 3rd party algorithm

Example run command, where FFT_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -- Debug/FFTW_example
in=0x7fffe505d010[1048576] out=0x7fffe4e7c010[1048576]
	Command being timed: "setarch x86_64 -R /home/Mr_Halfword/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -- Debug/FFTW_example"
	User time (seconds): 28.09
	System time (seconds): 0.21
	Percent of CPU this job got: 99%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:28.30
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 114080
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 61967
	Voluntary context switches: 24
	Involuntary context switches: 2969
	Swaps: 0
	File system inputs: 0
	File system outputs: 2136
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/memory_profile.csv 

