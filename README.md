pin_memory_profiler
===================

Test of writing a Pin tool to profile the memory usage of a 3rd party algorithm

Example run command, where FFT_example was the current working directory:
setarch x86_64 -R ~/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -- Debug/FFTW_example
in=0x7fffe505d010[1048576] out=0x7fffe4e7c010[1048576]

Resulting memory_profile is in FFTW_example/memory_profile.csv 

