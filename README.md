pin_memory_profiler
===================

Test of writing a Pin tool to profile the memory usage of a 3rd party algorithm

Uses fftw-3.3.4 compiled from source for AVX support, for both single and double precision, using:
./configure --enable-avx --enable-float
make
make check
sudo make install
./configure --enable-avx
make
make check
sudo make install


1) out-of-place complex double forward FFT run, where FFT_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTW_example
Out of place selected
fftw_plan_dft_1d returned 0x77df80
in=0x7fffe4b95040[1048576] out=0x7fffe4a8b040[1048576]
	Command being timed: "setarch x86_64 -R /home/Mr_Halfword/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTW_example"
	User time (seconds): 1.81
	System time (seconds): 0.18
	Percent of CPU this job got: 100%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:01.99
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 137664
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 58313
	Voluntary context switches: 23
	Involuntary context switches: 214
	Swaps: 0
	File system inputs: 0
	File system outputs: 2864
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/out_of_place_memory_profile.csv 

The profile is for a 64K point double complex forward FFT, performed out of place.

The 1M input buffer was at address 0x7fffe4b95040 for the run. Looking at the various stages in the memory profile::
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x7fffe4b95040,caller=fft_initialise

b) Initialised with random values with C code one double at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x7fffe4b95040,end_addr=0x7fffe4c9503f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

c) Read during each FFT execution. Read one double complex, with mainly incrementing addresses. Each double is read once, using 16 byte accesses so may be using SSE vectors:
fft_execute,memory read,start_addr=0x7fffe4b95040,end_addr=0x7fffe4c9503f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3800,cache_line_decrements=0x7ff,16 byte accesses=0x10000

d) Freed during termination:
fft_free,free,data_ptr=0x7fffe4b95040,size=0x100000,caller=fft_free


The 1M output buffer was at address 0x7fffe4a8b040 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x7fffe4a8b040,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x7fffe4a8b040,end_addr=0x7fffe4b8b03f,size=0x100000,total_bytes_accessed=0x100000,cache_line_decrements=0x3fff,8 byte accesses=0x20000

c) *read* 2 times during each FFT execution, with a mix of incrementing and decrementing addresses. Uses 32 byte accesses so using AVX vectors:
fft_execute,memory read,start_addr=0x7fffe4a8b040,end_addr=0x7fffe4b8b03f,size=0x100000,total_bytes_accessed=0x200000,cache_line_increments=0x3c1f,cache_line_decrements=0x3e0,32 byte accesses=0x10000

*written* 3 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be one write with 16 byte SSE vectors and two writes with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x7fffe4a8b040,end_addr=0x7fffe4b8b03f,size=0x100000,total_bytes_accessed=0x300000,cache_line_increments=0x1ff,cache_line_decrements=0x3e00,16 byte accesses=0x10000,32 byte accesses=0x10000

i.e. the output buffer from the FFT is read/written multiple times during the FFT execution. All accesses are 16 or 32 bytes so a sign of SSE and AVXn of vector instructions.

d) Freed during termination:
fft_free,free,data_ptr=0x7fffe4a8b040,size=0x100000,caller=fft_free


Each FFT execution is shown to read a 128K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x7fffe4809040,end_addr=0x7fffe482903f,size=0x20000,total_bytes_accessed=0x20000,cache_line_increments=0x7ff,32 byte accesses=0x1000

And a 4K region is read 32 times, using 32 byte access so AVX vectors:
fft_execute,memory read,start_addr=0x78a900,end_addr=0x78b8ff,size=0x1000,total_bytes_accessed=0x20000,cache_line_increments=0x3f,32 byte accesses=0x1000

Believe that these regions contains the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x20000,data_ptr=0x7fffe4809040,caller=fftw_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x1000,data_ptr=0x78a900,caller=fftw_malloc_plain

And freed from:
fft_free,free,data_ptr=0x7fffe4809040,size=0x20000,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x78a900,size=0x1000,caller=fftw_twiddle_awake


2) in-place complex double forward FFT run, where FFT_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTW_example -in_place
In place selected
fftw_plan_dft_1d returned 0x78a5e0
in=0x7fffe4b92040[1048576] out=0x7fffe4a8e040[1048576]
	Command being timed: "setarch x86_64 -R /home/Mr_Halfword/pin-2.14-67254-gcc.4.4.7-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTW_example -in_place"
	User time (seconds): 2.75
	System time (seconds): 0.21
	Percent of CPU this job got: 100%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:02.96
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 144576
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 67317
	Voluntary context switches: 23
	Involuntary context switches: 317
	Swaps: 0
	File system inputs: 0
	File system outputs: 4008
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/in_place_memory_profile.csv 

The profile is for a 64K point double complex forward FFT, performed in place.

The 1M input buffer was at address 0x7fffe4b92040 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x7fffe4b92040,caller=fft_initialise

b) Initialised with random values with C code one double at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x7fffe4b92040,end_addr=0x7fffe4c9203f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

c) Read during copy_input_data with memcpy one double at a time, with an incrementing address:
copy_input_data,memory read,start_addr=0x7fffe4b92040,end_addr=0x7fffe4c9203f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

d) Freed during termination:
fft_free,free,data_ptr=0x7fffe4b92040,size=0x100000,caller=fft_free

The 1M output buffer, used for in-place execution, was at address 0x7fffe4a8e040 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x7fffe4a8e040,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x7fffe4a8e040,end_addr=0x7fffe4b8e03f,size=0x100000,total_bytes_accessed=0x100000,cache_line_decrements=0x3fff,8 byte accesses=0x20000

c) Write to with a copy of the input data using memcpy() one double at a time, with an incrementing address:
copy_input_data,memory write,start_addr=0x7fffe4a8e040,end_addr=0x7fffe4b8e03f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

d) *read* 5 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be 4 reads with 16 byte SSE vectors, and 1 read with 32 byte AVX vectors:
fft_execute,memory read,start_addr=0x7fffe4a8e040,end_addr=0x7fffe4b8e03f,size=0x100000,total_bytes_accessed=0x500000,cache_line_increments=0x2ff8,cache_line_decrements=0x1007,16 byte accesses=0x40000,32 byte accesses=0x8000

d) *written* 5 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be 4 writes with 16 byte SSE vectors, and 1 write with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x7fffe4a8e040,end_addr=0x7fffe4b8e03f,size=0x100000,total_bytes_accessed=0x500000,cache_line_increments=0xff8,cache_line_decrements=0x3007,16 byte accesses=0x40000,32 byte accesses=0x8000

e) Freed during termination:
fft_free,free,data_ptr=0x7fffe4a8e040,size=0x100000,caller=fft_free


Each FFT execution is shown to read a 384K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x7fffe46e0040,end_addr=0x7fffe474003f,size=0x60000,total_bytes_accessed=0x60000,cache_line_increments=0xfff,cache_line_decrements=0x800,32 byte accesses=0x3000

And a 14K region 64 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x793460,end_addr=0x796c5f,size=0x3800,total_bytes_accessed=0xe0000,cache_line_increments=0x80,cache_line_decrements=0x60,32 byte accesses=0x7000

And a 112K region 8 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x796ca0,end_addr=0x7b2c9f,size=0x1c000,total_bytes_accessed=0xe0000,cache_line_increments=0x400,cache_line_decrements=0x300,32 byte accesses=0x7000

Believe that these regions contains the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x60000,data_ptr=0x7fffe46e0040,caller=fftw_malloc_plain
fft_execute,memory read,start_addr=0x793460,end_addr=0x796c5f,size=0x3800,total_bytes_accessed=0xe0000,cache_line_increments=0x80,cache_line_decrements=0x60,32 byte accesses=0x7000
fft_execute,memory read,start_addr=0x796ca0,end_addr=0x7b2c9f,size=0x1c000,total_bytes_accessed=0xe0000,cache_line_increments=0x400,cache_line_decrements=0x300,32 byte accesses=0x7000

And freed from:
fft_free,free,data_ptr=0x7fffe46e0040,size=0x60000,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x793460,size=0x3800,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x796ca0,size=0x1c000,caller=fftw_twiddle_awake

