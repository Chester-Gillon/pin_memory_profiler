pin_memory_profiler
===================

Test of writing a Pin tool to profile the memory usage of a 3rd party algorithm

Uses fftw-3.3.7 compiled from source for AVX support, for both single and double precision, using:
./configure --enable-avx --enable-float
make
make check
sudo make install
./configure --enable-avx
make
make check
sudo make install


The memory_profile project was compiled with gcc 5.4.0 and pin-3.6-97554-gcc-linux, and run under Ubuntu 16.04.LTS with a 4.4 series Kernel.

memory_profile is an Eclipse project, and the compiler and linker options were set to match those used in the pin makefiles.
The complete options are:

make all 
Building file: ../memory_profile.cpp
Invoking: GCC C++ Compiler
g++ -DBIGARRAY_MULTIPLIER=1 -DTARGET_IA32E -DHOST_IA32E -DTARGET_LINUX -D_GLIBCXX_USE_CXX11_ABI=0 -DPIN_CRT=1 -D__PIN__=1 -I"/home/mr_halfword/pin-3.6-97554-gcc-linux/source/include/pin" -I"/home/mr_halfword/pin-3.6-97554-gcc-linux/source/include/pin/gen" -I"/home/mr_halfword/pin-3.6-97554-gcc-linux/extras/components/include" -I"/home/mr_halfword/pin-3.6-97554-gcc-linux/extras/xed-intel64/include/xed" -I"/home/mr_halfword/pin-3.6-97554-gcc-linux/source/tools/InstLib" -O3 -Wall -fno-stack-protector -fno-exceptions -funwind-tables -fasynchronous-unwind-tables -fno-rtti -c -fmessage-length=0 -fPIC -fabi-version=2 -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/stlport/include -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/libstdc++/include -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/crt/include -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/crt/include/arch-x86_64 -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/crt/include/kernel/uapi -isystem /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/crt/include/kernel/uapi/asm-x86 -MMD -MP -MF"memory_profile.d" -MT"memory_profile.o" -o "memory_profile.o" "../memory_profile.cpp"
Finished building: ../memory_profile.cpp
 
Building target: libmemory_profile.so
Invoking: GCC C++ Linker
g++ -nostdlib -L"/home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt" -L"/home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/lib" -L"/home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/lib-ext" -L"/home/mr_halfword/pin-3.6-97554-gcc-linux/extras/xed-intel64/lib" -Xlinker --hash-style=sysv -Xlinker --version-script="/home/mr_halfword/pin-3.6-97554-gcc-linux/source/include/pin/pintool.ver" -Xlinker -Bsymbolic -shared -o "libmemory_profile.so"  ./memory_profile.o  /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/crtbeginS.o /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/crtendS.o /home/mr_halfword/pin-3.6-97554-gcc-linux/source/tools/InstLib/obj-intel64/controller.a -lpin -lxed -lpin3dwarf -ldl-dynamic -lstlport-dynamic -lm-dynamic -lc-dynamic -lunwind-dynamic
Finished building target: libmemory_profile.so

The generated libmemory_profile.so pin tool only has dependencies on the Pin runtime, i.e. doesn't use any standard system libraries:
(PIN_ROOT=~/pin-3.6-97554-gcc-linux; export LD_LIBRARY_PATH=${PIN_ROOT}/extras/xed-intel64/lib:${PIN_ROOT}/intel64/lib-ext:${PIN_ROOT}/intel64/runtime/pincrt; ldd memory_profile/Release/libmemory_profile.so)
	linux-vdso.so.1 =>  (0x00007ffdd41e8000)
	libxed.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/extras/xed-intel64/lib/libxed.so (0x00007fb1cf84b000)
	libpin3dwarf.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/lib-ext/libpin3dwarf.so (0x00007fb1cf362000)
	libdl-dynamic.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/libdl-dynamic.so (0x00007fb1d0336000)
	libstlport-dynamic.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/libstlport-dynamic.so (0x00007fb1d0271000)
	libc-dynamic.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/libc-dynamic.so (0x00007fb1d01dc000)
	libunwind-dynamic.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/libunwind-dynamic.so (0x00007fb1d01c7000)
	libm-dynamic.so => /home/mr_halfword/pin-3.6-97554-gcc-linux/intel64/runtime/pincrt/libm-dynamic.so (0x00007fb1d0197000)


1) out-of-place complex double forward FFT run, where FFTW_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTW_example
Out of place selected
fftw_plan_dft_1d returned 0x982040
in=0x761040[1048576] out=0x8610a0[1048576]
	Command being timed: "setarch x86_64 -R /home/mr_halfword/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTW_example"
	User time (seconds): 1.80
	System time (seconds): 0.19
	Percent of CPU this job got: 95%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:02.10
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 35248
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 15
	Minor (reclaiming a frame) page faults: 82773
	Voluntary context switches: 47
	Involuntary context switches: 8
	Swaps: 0
	File system inputs: 5584
	File system outputs: 1096
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/out_of_place_memory_profile.csv 

The profile is for a 64K point double complex forward FFT, performed out of place.

The 1M input buffer was at address 0x761040 for the run. Looking at the various stages in the memory profile::
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x761040,caller=fft_initialise

b) Initialised with random values with C code one double at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x761040,end_addr=0x86103f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

) Read during each FFT execution. Read one double complex at time, with mainly incrementing addresses. Each double is read once, using 16 byte accesses so may be using SSE vectors:
fft_execute,memory read,start_addr=0x761040,end_addr=0x86103f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3800,cache_line_decrements=0x7ff,16 byte accesses=0x10000

d) Freed during termination:
fft_free,free,data_ptr=0x761040,size=0x100000,caller=fft_free


The 1M output buffer was at address 0x8610a0 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x8610a0,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x100000,cache_line_decrements=0x4000,8 byte accesses=0x20000

c) *read* 2 times during each FFT execution, with incrementing addresses. Uses 32 byte accesses so using AVX vectors:
fft_execute,memory read,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x200000,cache_line_increments=0x4000,32 byte accesses=0x10000

*written* 3 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be one write with 16 byte SSE vectors and two writes with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x300000,cache_line_increments=0x1000,cache_line_decrements=0x3000,16 byte accesses=0x10000,32 byte accesses=0x10000

i.e. the output buffer from the FFT is read/written multiple times during the FFT execution. All accesses are 16 or 32 bytes so a sign of SSE and AVX of vector instructions.

d) Freed during termination:
fft_free,free,data_ptr=0x8610a0,size=0x100000,caller=fft_free


Each FFT execution is shown to read a 128K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x98fa20,end_addr=0x9afa1f,size=0x20000,total_bytes_accessed=0x20000,cache_line_increments=0x400,cache_line_decrements=0x400,32 byte accesses=0x1000

And a 4K region is read 32 times, using 32 byte access so AVX vectors:
fft_execute,memory read,start_addr=0x98e9c0,end_addr=0x98f9bf,size=0x1000,total_bytes_accessed=0x20000,cache_line_increments=0x1f,cache_line_decrements=0x20,32 byte accesses=0x1000

Believe that these regions contain the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x20000,data_ptr=0x98fa20,caller=fftw_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x1000,data_ptr=0x98e9c0,caller=fftw_malloc_plain

And freed from:
fft_free,free,data_ptr=0x98fa20,size=0x20000,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x98e9c0,size=0x1000,caller=fftw_twiddle_awake


2) in-place complex double forward FFT run, where FFTW_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTW_example -in_placeIn place selected
fftw_plan_dft_1d returned 0x983680
in=0x761040[1048576] out=0x8610a0[1048576]
	Command being timed: "setarch x86_64 -R /home/mr_halfword/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTW_example -in_place"
	User time (seconds): 2.28
	System time (seconds): 0.26
	Percent of CPU this job got: 100%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:02.53
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 37480
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 96272
	Voluntary context switches: 22
	Involuntary context switches: 13
	Swaps: 0
	File system inputs: 0
	File system outputs: 2216
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/in_place_memory_profile.csv 

The profile is for a 64K point double complex forward FFT, performed in place.

The 1M input buffer was at address 0x761040 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x761040,caller=fft_initialise

b) Initialised with random values with C code one double at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x761040,end_addr=0x86103f,size=0x100000,total_bytes_accessed=0x100000,cache_line_increments=0x3fff,8 byte accesses=0x20000

c) Read during copy_input_data with memcpy one double complex at a time, mostly with an incrementing address and one decrementing address:
copy_input_data,memory read,start_addr=0x761040,end_addr=0x86103f,size=0x100000,total_bytes_accessed=0x100040,cache_line_increments=0x3ffe,cache_line_decrements=0x1,16 byte accesses=0x10004

d) Freed during termination:
fft_free,free,data_ptr=0x761040,size=0x100000,caller=fft_free


The 1M output buffer, used for in-place execution, was at address 0x8610a0 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x100000,data_ptr=0x8610a0,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x100000,cache_line_decrements=0x4000,8 byte accesses=0x20000

c) Write to with a copy of the input data using memcpy() one double complex at a time, mostly with an incrementing address and one decrementing address:
copy_input_data,memory write,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x100040,cache_line_increments=0x3fff,cache_line_decrements=0x1,16 byte accesses=0x10004

d) *read* 5 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be 4 reads with 16 byte SSE vectors, and 1 read with 32 byte AVX vectors:
fft_execute,memory read,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x500000,cache_line_increments=0x4000,16 byte accesses=0x40000,32 byte accesses=0x8000

*written* 5 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be 4 writes with 16 byte SSE vectors, and 1 write with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x8610a0,end_addr=0x96109f,size=0x100000,total_bytes_accessed=0x500000,cache_line_decrements=0x4000,16 byte accesses=0x40000,32 byte accesses=0x8000

i.e. the in-place buffer from the FFT is read/written multiple times during the FFT execution. All accesses are 16 or 32 bytes so a sign of SSE and AVX of vector instructions.

e) Freed during termination:
fft_free,free,data_ptr=0x8610a0,size=0x100000,caller=fft_free


Each FFT execution is shown to read a 384K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x9b44a0,end_addr=0xa1449f,size=0x60000,total_bytes_accessed=0x60000,cache_line_increments=0x1000,cache_line_decrements=0x800,32 byte accesses=0x3000

And a 14K region 64 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x9933a0,end_addr=0x996b9f,size=0x3800,total_bytes_accessed=0xe0000,cache_line_increments=0x80,cache_line_decrements=0x60,32 byte accesses=0x7000

And a 112K region 8 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x998460,end_addr=0x9b445f,size=0x1c000,total_bytes_accessed=0xe0000,cache_line_increments=0x400,cache_line_decrements=0x300,32 byte accesses=0x7000

Believe that these regions contain the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x60000,data_ptr=0x9b44a0,caller=fftw_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x3800,data_ptr=0x9933a0,caller=fftw_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x1c000,data_ptr=0x998460,caller=fftw_malloc_plain

And freed from:
fft_free,free,data_ptr=0x9b44a0,size=0x60000,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x9933a0,size=0x3800,caller=fftw_twiddle_awake
fft_free,free,data_ptr=0x998460,size=0x1c000,caller=fftw_twiddle_awake


3) out-of-place complex float forward FFT run, where FFTWf_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTWf_exampleOut of place selected
fftwf_plan_dft_1d returned 0x88e080
in=0x76c040[524288] out=0x7ec0a0[524288]
	Command being timed: "setarch x86_64 -R /home/mr_halfword/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o out_of_place_memory_profile.csv -- Debug/FFTWf_example"
	User time (seconds): 1.64
	System time (seconds): 0.26
	Percent of CPU this job got: 98%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:01.92
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 34280
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 3
	Minor (reclaiming a frame) page faults: 83018
	Voluntary context switches: 36
	Involuntary context switches: 9
	Swaps: 0
	File system inputs: 3176
	File system outputs: 1096
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

The profile is for a 64K point float complex forward FFT, performed out of place.

The 512K input buffer was at address 0x76c040 for the run. Looking at the various stages in the memory profile::
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x80000,data_ptr=0x76c040,caller=fft_initialise

b) Initialised with random values with C code one float at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x76c040,end_addr=0x7ec03f,size=0x80000,total_bytes_accessed=0x80000,cache_line_increments=0x1fff,4 byte accesses=0x20000

c) Read during each FFT execution. Read one float complex at a time, with mainly incrementing addresses. Each float complex is read once, using 8 byte accesses:
fft_execute,memory read,start_addr=0x76c040,end_addr=0x7ec03f,size=0x80000,total_bytes_accessed=0x80000,cache_line_increments=0x1800,cache_line_decrements=0x7ff,8 byte accesses=0x10000

d) Freed during termination:
fft_free,free,data_ptr=0x76c040,size=0x80000,caller=fft_free


The 512K output buffer was at address 0x7ec0a0 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x80000,data_ptr=0x7ec0a0,caller=fft_initialise

b) Initialised with zero values with C code one double at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x80000,cache_line_decrements=0x2000,4 byte accesses=0x20000

c) *read* 2 times during each FFT execution, with a mix of incrementing and decrementing addresses. Uses 32 byte accesses so using AVX vectors:
fft_execute,memory read,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x100000,cache_line_increments=0x2000,32 byte accesses=0x8000

*written* 3 times during each FFT execution, with a mix of incrementing and decrementing addresses. May be one write with 8 byte writes and two writes with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x180000,cache_line_decrements=0x2000,8 byte accesses=0x10000,32 byte accesses=0x8000

i.e. the output buffer from the FFT is read/written multiple times during the FFT execution. Some accesses are 16 or 32 bytes so a sign of SSE and AVX of vector instructions.

d) Freed during termination:
fft_free,free,data_ptr=0x7ec0a0,size=0x80000,caller=fft_free


Each FFT execution is shown to read a 2K region 32 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x88ff00,end_addr=0x8906ff,size=0x800,total_bytes_accessed=0x10000,cache_line_increments=0xf,cache_line_decrements=0x10,32 byte accesses=0x800

Each FFT execution is shown to read a 64K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x89bc20,end_addr=0x8abc1f,size=0x10000,total_bytes_accessed=0x10000,cache_line_increments=0x200,cache_line_decrements=0x200,32 byte accesses=0x800

Believe that these regions contain the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x800,data_ptr=0x88ff00,caller=fftwf_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x10000,data_ptr=0x89bc20,caller=fftwf_malloc_plain

And freed from:
fft_free,free,data_ptr=0x88ff00,size=0x800,caller=fftwf_twiddle_awake
fft_free,free,data_ptr=0x89bc20,size=0x10000,caller=fftwf_twiddle_awake


4) in-place complex float forward FFT run, where FFTWf_example was the current working directory:
/usr/bin/time -v setarch x86_64 -R ~/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTWf_example -in_place 
In place selected
fftwf_plan_dft_1d returned 0x88d240
in=0x76c040[524288] out=0x7ec0a0[524288]
	Command being timed: "setarch x86_64 -R /home/mr_halfword/pin-3.6-97554-gcc-linux/pin -t ../memory_profile/Release/libmemory_profile.so -o in_place_memory_profile.csv -- Debug/FFTWf_example -in_place"
	User time (seconds): 1.99
	System time (seconds): 0.24
	Percent of CPU this job got: 100%
	Elapsed (wall clock) time (h:mm:ss or m:ss): 0:02.22
	Average shared text size (kbytes): 0
	Average unshared data size (kbytes): 0
	Average stack size (kbytes): 0
	Average total size (kbytes): 0
	Maximum resident set size (kbytes): 35856
	Average resident set size (kbytes): 0
	Major (requiring I/O) page faults: 0
	Minor (reclaiming a frame) page faults: 94168
	Voluntary context switches: 22
	Involuntary context switches: 9
	Swaps: 0
	File system inputs: 8
	File system outputs: 1448
	Socket messages sent: 0
	Socket messages received: 0
	Signals delivered: 0
	Page size (bytes): 4096
	Exit status: 0

Resulting memory_profile is in FFTW_example/in_place_memory_profile.csv 

The profile is for a 64K point float complex forward FFT, performed in place.

The 512K input buffer was at address 0x76c040 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x80000,data_ptr=0x76c040,caller=fft_initialise

b) Initialised with random values with C code one float at a time, with an incrementing address:
set_fft_data,memory write,start_addr=0x76c040,end_addr=0x7ec03f,size=0x80000,total_bytes_accessed=0x80000,cache_line_increments=0x1fff,4 byte accesses=0x20000

c) Read during copy_input_data with memcpy two complex floats at a time, mostly with an incrementing address and one decrementing address:
copy_input_data,memory read,start_addr=0x76c040,end_addr=0x7ec03f,size=0x80000,total_bytes_accessed=0x80040,cache_line_increments=0x1ffe,cache_line_decrements=0x1,16 byte accesses=0x8004

d) Freed during termination:
fft_free,free,data_ptr=0x76c040,size=0x80000,caller=fft_free


The 512K output buffer, used for in-place execution, was at address 0x7ec0a0 for the run. Looking at the various stages in the memory profile:
a) Allocated during initialisation:
fft_initialise,memalign,boundary=0x20,size=0x80000,data_ptr=0x7ec0a0,caller=fft_initialise

b) Initialised with zero values with C code one float at a time, with a decrementing address (decrementing address chosen to demonstrate shown in the memory profile):
set_fft_data,memory write,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x80000,cache_line_decrements=0x2000,4 byte accesses=0x20000

c) Write to with a copy of the input data using memcpy() two complex floats at a time, mostly with an incrementing address and one decrementing address:
copy_input_data,memory write,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x80040,cache_line_increments=0x1fff,cache_line_decrements=0x1,16 byte accesses=0x8004

d) *read* twice during each FFT execution, with a mix of incrementing and decrementing addresses. May be 1 read with using 8 byte accesses, and 1 read with 32 byte AVX vectors:
fft_execute,memory read,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x100000,cache_line_increments=0x2000,8 byte accesses=0x10000,32 byte accesses=0x4000

*written* twice during each FFT execution, with a mix of incrementing and decrementing addresses. May be 1 write with 16 byte SSE vectors, and 1 write with 32 byte AVX vectors:
fft_execute,memory write,start_addr=0x7ec0a0,end_addr=0x86c09f,size=0x80000,total_bytes_accessed=0x100000,cache_line_increments=0x2000,16 byte accesses=0x8000,32 byte accesses=0x4000

e) Freed during termination:
fft_free,free,data_ptr=0x7ec0a0,size=0x80000,caller=fft_free


Each FFT execution is shown to read a 2K region 32 times, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x8993c0,end_addr=0x899bbf,size=0x800,total_bytes_accessed=0x10000,cache_line_increments=0xf,cache_line_decrements=0x10,32 byte accesses=0x800

Each FFT execution is shown to read a 64K region once, using 32 byte accesses so AVX vectors:
fft_execute,memory read,start_addr=0x89c4e0,end_addr=0x8ac4df,size=0x10000,total_bytes_accessed=0x10000,cache_line_increments=0x200,cache_line_decrements=0x200,32 byte accesses=0x800

Believe that these regions contain the FFT "twiddle factors". Allocated from:
fft_initialise,memalign,boundary=0x20,size=0x80600,data_ptr=0x8993c0,caller=fftwf_malloc_plain
fft_initialise,memalign,boundary=0x20,size=0x10000,data_ptr=0x89c4e0,caller=fftwf_malloc_plain

And freed from:
fft_free,free,data_ptr=0x8993c0,size=0x800,caller=fftwf_twiddle_awake
fft_free,free,data_ptr=0x89c4e0,size=0x10000,caller=fftwf_twiddle_awake

Each fft_execute call dynamically allocates a 513.5K buffer, which is read/written during each FFT execution so is probably a working buffer:
fft_execute,memalign,boundary=0x20,size=0x80600,data_ptr=0x8ac540,caller=fftwf_malloc_plain
fft_execute,free,data_ptr=0x8ac540,size=0x80600,caller=apply

(The working buffer is accessed as 32 32K chunks)
