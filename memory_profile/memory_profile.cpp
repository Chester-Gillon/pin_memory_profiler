/*
 * @file memory_profile.cpp
 * @date 5 Sep 2014
 * @author Chester Gillon
 * @details
 *  A demonstration of a Pin tool which instruments a program to determine the memory profile usage
 *  of a fixed set of "top-level" functions in the FFTW_example program. The information obtained is:
 *  1) When memory allocations and frees occur. Currently, only malloc(), memalign() and free() are instrumented
 *     as they are the memory allocation functions used by the FFTW_example program.
 *
 *  2) The unique regions of memory which are read/written by each top level function. For each region
 *     which is read or written the following is collected:
 *     a) The total number of bytes accessed in the region.
 *        If the total number of bytes accessed is greater than the region size, then same region was
 *        read or written multiple times.
 *
 *     b) A histogram of how many accessed to the memory region were made with different sized accesses.
 *        This can show if vector instructions are being used to access the memory region.
 *
 *     c) Counts of how many times the memory region was expanded with incrementing or decrementing cache lines:
 *        - If only cache_line_increments is non-zero then the region was accessed with increasing addresses.
 *        - If only cache_line_decrements is non-zero then the region was accessed with decreasing addresses.
 *        - If both cache_line_increments and cache_line_decrements the region was accessed with non-uniform
 *          address sequence.
 *
 *  The FFTW_example is single threaded so the memory profile is maintained for the whole process.
 *  This program could be expanded with Pin thread-local-storage to maintain the memory profile for
 *  individual threads as required.
 */

#include <unistd.h>
#include <string.h>

#include "pin.H"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>

/** Command line options */
KNOB<string> trace_filename(KNOB_MODE_WRITEONCE, "pintool",
    "o", "memory_profile.csv", "specify trace file name");

/** Text .csv file the trace is written to */
std::ofstream trace_file;

/** The top-level functions in the FFTW_example program which have memory profiles maintained */
static std::vector<std::string> top_level_func_names;

/* The index into top_level_func_names[] for the top level function which is currently executing,
 * or -1 if no top level function is currently executing.
 * Memory is only profiled when a top level function is executing
 */
static INT32 active_top_level_func_index = -1;

/** The allocation size requested at entry to malloc, to correlate with the allocated address at exit from malloc */
static ADDRINT malloc_requested_size = 0;

/** Return Instruction Pointer at entry to malloc */
static ADDRINT malloc_return_ip;

/** The alignment requested at entry to memalign, to correlate with the allocated address at exit from memalign */
static ADDRINT memalign_boundary = 0;

/** The allocation size requested at entry to memalign, to correlate with the allocated address at exit from memalign */
static ADDRINT memalign_requested_size = 0;

/** Return Instruction Pointer at entry to memalign */
static ADDRINT memalign_return_ip;

/** Records memory allocations, which are then removed when freed.
 *  Used to report memory which is allocated but not freed upon program completion.
 *  Key is address of allocation, data is size
 */
static std::map<ADDRINT,ADDRINT> outstanding_allocations;

/** Used to record the memory profile for either read or writes */
class memory_regions_usage
{
public:
    void clear (void);
    void display (const std::string &prefix);
    void record_access (ADDRINT memory_addr, UINT32 bytes_accessed);

    memory_regions_usage() : cache_line_size(sysconf (_SC_LEVEL1_DCACHE_LINESIZE)) {};
private:
    static const UINT32 max_mem_access_size = 64;

    /** The information maintained for each non-consecutive memory region */
    struct region_info
    {
        /** The end address of the region */
        UINT64 region_end_addr;
        /** The total number of bytes which have been accessed in the region */
        UINT64 total_bytes;
        /** Count of the number of times the region has been extended to cover a incrementing cache line */
        UINT32 cache_line_increments;
        /** Count of the number of times the region has been extended to cover a decrementing cache line */
        UINT32 cache_line_decrements;
        /** Count of total instruction memory accesses to the region, indexed by the number of bytes in each access.
         *  Index zero is used for sizes outside of the expected range. */
        UINT64 mem_access_size_counts[max_mem_access_size + 1];
    };

    /** Which memory regions have been accessed.
     *  Key is the start address, data is the region information
     */
    std::map<ADDRINT,region_info> memory_regions;
    typedef std::map<ADDRINT,region_info>::iterator region_iter;

    const ADDRINT cache_line_size;

    /**
     * @details
     *  Called when an instruction memory access extends the upper address of an existing region,
     *  to check when the region extends to a new cache line at a higher address.
     * @param[in,out] it Iterator referencing the region being extended
     * @param[in] access_end_addr The end address of the memory access
     */
    inline void update_addr_inc_cache_line_counts (region_iter &it, const ADDRINT access_end_addr)
    {
        const ADDRINT previous_end_cache_line = it->second.region_end_addr / cache_line_size;
        const ADDRINT access_end_cache_line = access_end_addr / cache_line_size;

        if (access_end_cache_line > previous_end_cache_line)
        {
            it->second.cache_line_increments++;
        }
    }

    /**
     * @details
     *  Called when an instruction memory access extends the lower address of an existing region,
     *  to check when the region extends to a new cache line at a lower address.
     * @param[in,out] it Iterator referencing the region being extended
     * @param[in] access_start_addr The start address of the memory access
     */
    inline void update_addr_dec_cache_line_counts (region_iter &it, const ADDRINT access_start_addr)
    {
        const ADDRINT previous_start_cache_line = it->first / cache_line_size;
        const ADDRINT access_start_cache_line = access_start_addr / cache_line_size;

        if (access_start_cache_line < previous_start_cache_line)
        {
            it->second.cache_line_decrements++;
        }
    }

    /**
     * @brief Called after each instruction memory access to update the count of memory accesses
     * @param[in,out] region The region to update the counter for
     * @param[in] bytes_accessed How many bytes were accessed by the instruction
     */
    inline void update_access_counts (region_info &region, const UINT32 bytes_accessed)
    {
        region.total_bytes += bytes_accessed;
        if (bytes_accessed <= max_mem_access_size)
        {
            region.mem_access_size_counts[bytes_accessed]++;
        }
        else
        {
            region.mem_access_size_counts[0]++;
        }
    }
};

/** Used to record the memory regions read/written by the current active top level function */
static memory_regions_usage read_memory_regions;
static memory_regions_usage write_memory_regions;

/**
 * @brief Clear the memory profile
 */
void memory_regions_usage::clear(void)
{
    memory_regions.clear();
}

/**
 * @brief Called when an instruction reads or write memory to update the memory profile
 * @param[in] access_start_address Start address read or written
 * @param[in] bytes_accessed The number of bytes read or written by the instruction
 */
void memory_regions_usage::record_access (const ADDRINT access_start_addr, const UINT32 bytes_accessed)
{
    const ADDRINT access_end_addr = access_start_addr + bytes_accessed - 1;
    region_info new_region;
    region_iter begin_it, end_it, current_it, next_it;
    bool region_processed = false;
    bool region_addrs_changed = false;
    bool region_merge_complete;
    ADDRINT modified_start_addr = access_start_addr;
    ADDRINT modified_end_addr = access_end_addr;
    UINT32 mem_access_size;

    if (!memory_regions.empty())
    {
        /* Determine if the memory access overlaps any existing region */
        begin_it = memory_regions.lower_bound (access_start_addr);
        if (begin_it != memory_regions.begin())
        {
            --begin_it;
        }
        end_it = memory_regions.upper_bound(access_end_addr + 1);
        for (current_it = begin_it; !region_processed && (current_it != end_it); ++current_it)
        {
            if ((access_start_addr < current_it->first) && (access_end_addr >= current_it->first))
            {
                /* The memory access overlaps the beginning of an existing region */
                update_addr_dec_cache_line_counts (current_it, access_start_addr);
                new_region = current_it->second;
                update_access_counts (new_region, bytes_accessed);
                if (access_end_addr > new_region.region_end_addr)
                {
                    new_region.region_end_addr = access_end_addr;
                }
                memory_regions.erase (current_it);
                memory_regions[access_start_addr] = new_region;
                end_it = memory_regions.upper_bound(access_end_addr + 1);
                region_processed = true;
                region_addrs_changed = true;
                modified_end_addr = new_region.region_end_addr;
            }
            else if ((access_start_addr >= current_it->first) && (access_end_addr <= current_it->second.region_end_addr))
            {
                /* The memory access is entirely within an existing region */
                update_access_counts (current_it->second, bytes_accessed);
                region_processed = true;
            }
            else if ((access_start_addr <= current_it->second.region_end_addr) &&
                     (access_end_addr > current_it->second.region_end_addr))
            {
                /* The memory access overlaps the end of an existing region */
                update_addr_inc_cache_line_counts (current_it, access_end_addr);
                current_it->second.region_end_addr = access_end_addr;
                update_access_counts (current_it->second, bytes_accessed);
                region_processed = true;
                region_addrs_changed = true;
                modified_start_addr = current_it->first;
            }
            else
            {
                /* Update cache line counts for a memory access which will be combined with an adjacent region */
                if (access_start_addr == (current_it->second.region_end_addr + 1))
                {
                    update_addr_inc_cache_line_counts (current_it, access_end_addr);
                }
                if ((access_end_addr + 1) == current_it->first)
                {
                    update_addr_dec_cache_line_counts (current_it, access_start_addr);
                }
            }
        }
    }

    if (!region_processed)
    {
        /* Insert as a new region */
        new_region.region_end_addr = access_end_addr;
        new_region.total_bytes = 0;
        new_region.cache_line_increments = 0;
        new_region.cache_line_decrements = 0;
        memset (&new_region.mem_access_size_counts, 0, sizeof (new_region.mem_access_size_counts));
        update_access_counts (new_region, bytes_accessed);
        memory_regions[access_start_addr] = new_region;
        region_addrs_changed = true;
    }

    if (region_addrs_changed)
    {
        /* Combine adjacent regions */
        begin_it = memory_regions.lower_bound (modified_start_addr);
        if (begin_it != memory_regions.begin())
        {
            --begin_it;
        }

        current_it = begin_it;
        next_it = current_it;
        if (next_it != memory_regions.end())
        {
            ++next_it;
        }
        region_merge_complete = false;
        while ((next_it != memory_regions.end()) && !region_merge_complete)
        {
            while ((next_it != memory_regions.end()) && ((current_it->second.region_end_addr + 1) >= next_it->first))
            {
                /* Regions are adjacent - so combine */
                current_it->second.region_end_addr = next_it->second.region_end_addr;
                current_it->second.total_bytes += next_it->second.total_bytes;
                current_it->second.cache_line_increments += next_it->second.cache_line_increments;
                current_it->second.cache_line_decrements += next_it->second.cache_line_decrements;
                for (mem_access_size = 0; mem_access_size <= max_mem_access_size; mem_access_size++)
                {
                    current_it->second.mem_access_size_counts[mem_access_size] +=
                            next_it->second.mem_access_size_counts[mem_access_size];
                }
                memory_regions.erase (next_it);
                next_it = current_it;
                ++next_it;
            }

            ++current_it;
            next_it = current_it;
            ++next_it;
            if (current_it != memory_regions.end())
            {
                region_merge_complete = current_it->first > modified_end_addr;
            }
        }
    }
}

/**
 * @brief Output the the trace file the read or write memory profile
 * @param[in] prefix Output at the start of each line of trace output to identify the top-level function and if read or write
 */
void memory_regions_usage::display (const std::string &prefix)
{
    std::map<ADDRINT,region_info>::const_iterator it;
    ADDRINT previous_end_addr = 0;
    bool first_region = true;
    UINT32 mem_access_size;

    for (it = memory_regions.begin(); it != memory_regions.end(); ++it)
    {
        trace_file << prefix << ",start_addr=" << it->first << ",end_addr=" << it->second.region_end_addr
                << ",size=" << (it->second.region_end_addr - it->first + 1)
                << ",total_bytes_accessed=" << it->second.total_bytes;
        if (it->second.cache_line_increments > 0)
        {
            trace_file << ",cache_line_increments=" << it->second.cache_line_increments;
        }
        if (it->second.cache_line_decrements > 0)
        {
            trace_file << ",cache_line_decrements=" << it->second.cache_line_decrements;
        }
        if (it->second.mem_access_size_counts[0] > 0)
        {
            trace_file << ",unknown size accesses=" << it->second.mem_access_size_counts[0];
        }
        for (mem_access_size = 1; mem_access_size <= max_mem_access_size; mem_access_size++)
        {
            if (it->second.mem_access_size_counts[mem_access_size] > 0)
            {
                trace_file << "," << dec << mem_access_size << hex << " byte accesses="
                        << it->second.mem_access_size_counts[mem_access_size];
            }
        }
        trace_file << endl;
        if (first_region)
        {
            first_region = false;
        }
        else
        {
            if ((previous_end_addr + 1) == it->first)
            {
                trace_file << prefix << ",**ERROR** merge of adjacent regions failed" << endl;
            }
        }
        previous_end_addr = it->second.region_end_addr;
    }
}

/**
 * @brief Analysis function called when an instruction reads or writes memory
 * @details When a top-level function is active updates the memory profile
 * @param[in,out] memory_regions The read or write memory regions to update
 * @param[in] memory_addr The memory address being read or written
 * @param[in] bytes_accessed The number of bytes read or written by the instruction
 */
static void memory_access_analysis (memory_regions_usage *const memory_regions, ADDRINT memory_addr, UINT32 bytes_accessed)
{
    if (active_top_level_func_index != -1)
    {
        memory_regions->record_access (memory_addr, bytes_accessed);
    }
}

/**
 * @brief Is called for every instruction and instruments memory reads and writes
 * @details When a top-level function is active updates the memory read / write profiles for the top-level function.
 * @param[in] arg Instrumentation context - not used
 */
static void instrument_memory_access (INS ins, void *arg)
{
    /* Instruments memory accesses using a predicated call, i.e.
       the instrumentation is called if the instruction will actually be executed.

       On the IA-32 and Intel(R) 64 architectures conditional moves and REP
       prefixed instructions appear as predicated instructions in Pin. */
    UINT32 mem_operands = INS_MemoryOperandCount(ins);

    /* Iterate over each memory operand of the instruction. */
    for (UINT32 mem_op = 0; mem_op < mem_operands; mem_op++)
    {
        if (INS_MemoryOperandIsRead (ins, mem_op))
        {
            INS_InsertPredicatedCall (ins, IPOINT_BEFORE, (AFUNPTR) memory_access_analysis,
                                      IARG_PTR, &read_memory_regions,
                                      IARG_MEMORYOP_EA, mem_op,
                                      IARG_MEMORYREAD_SIZE,
                                      IARG_END);
        }

        /* Note that in some architectures a single memory operand can be
           both read and written (for instance incl (%eax) on IA-32)
           In that case we instrument it once for read and once for write. */
        if (INS_MemoryOperandIsWritten (ins, mem_op))
        {
            INS_InsertPredicatedCall (ins, IPOINT_BEFORE, (AFUNPTR) memory_access_analysis,
                                      IARG_PTR, &write_memory_regions,
                                      IARG_MEMORYOP_EA, mem_op,
                                      IARG_MEMORYWRITE_SIZE,
                                      IARG_END);
        }
    }
}

/**
 * @brief Instrumentation function called before entry to a top-level function.
 * @details Traces entry to the top-level function and re-initialises the memory profile to empty.
 * @param[in] func_index Index into top_level_func_names[] for the top-level function
 */
static void before_top_level_function (ADDRINT func_index)
{
   if (active_top_level_func_index == -1)
   {
       trace_file << top_level_func_names[func_index] << ",enter" << endl;
       read_memory_regions.clear();
       write_memory_regions.clear();
       active_top_level_func_index = func_index;
   }
}

/**
 * @brief Instrumentation function called after exit from a top-level function.
 * @details Outputs the memory profile for the top-level function to the trace file.
 * @param[in] func_index Index into top_level_func_names[] for the top-level function
 */
static void after_top_level_function (ADDRINT func_index)
{
    if (active_top_level_func_index == (INT32) func_index)
    {
        trace_file << top_level_func_names[func_index] << ",exit" << endl;
        read_memory_regions.display(top_level_func_names[func_index] + ",memory read");
        write_memory_regions.display(top_level_func_names[func_index] + ",memory write");
        active_top_level_func_index = -1;
    }
}

/**
 * @brief Instrumentation function called before malloc() to save parameters used in after_malloc()
 * @param[in] size malloc() parameter for the requested size to be allocated
 * @param[in] return_ip Return IP for malloc() call, which is traced
 */
static void before_malloc (ADDRINT size, ADDRINT return_ip)
{
    malloc_requested_size = size;
    malloc_return_ip = return_ip;
}

/**
 * @brief Instrumentation function called after malloc()
 * @details Traces the memory allocation, and records the allocation as outstanding.
 *          Only takes action when a top-level function is active.
 * @param[in] data_ptr Return value from malloc(), i.e. if non-zero the allocated memory pointer
 */
static void after_malloc (ADDRINT data_ptr)
{
    if ((active_top_level_func_index != -1) && (data_ptr != 0))
    {
        outstanding_allocations[data_ptr] = malloc_requested_size;
        trace_file << top_level_func_names[active_top_level_func_index] << ",malloc,size=" << malloc_requested_size
                << ",data_ptr=" << data_ptr << ",caller=" << RTN_FindNameByAddress (malloc_return_ip) << endl;
    }
    malloc_requested_size = 0;
    malloc_return_ip = 0;
}

/**
 * @brief Instrumention function called before memalign() to save parameters used in after_malloc()
 * @param[in] boundary memalign() parameter for the alignment
 * @param[in] size memalign() parameter for the allocation size
 * @param[in] return_ip Return IP for memalign() call, which is traced
 */
static void before_memalign (ADDRINT boundary, ADDRINT size, ADDRINT return_ip)
{
    memalign_boundary = boundary;
    memalign_requested_size = size;
    memalign_return_ip = return_ip;
}

/**
 * @brief Instrumentation function called after memalign()
 * @details Traces the memory allocation, and records the allocation as outstanding.
 *          Only takes action when a top-level function is active.
 * @param[in] data_ptr Return value from memalign(), i.e. if non-zero the allocated memory pointer
 */
static void after_memalign (ADDRINT data_ptr)
{
    if ((active_top_level_func_index != -1) && (data_ptr != 0))
    {
        outstanding_allocations[data_ptr] = memalign_requested_size;
        trace_file << top_level_func_names[active_top_level_func_index] << ",memalign,boundary=" << memalign_boundary
                << ",size=" << memalign_requested_size
                << ",data_ptr=" << data_ptr << ",caller=" << RTN_FindNameByAddress (memalign_return_ip) << endl;
    }

    memalign_boundary = 0;
    memalign_requested_size = 0;
    memalign_return_ip = 0;
}

/**
 * @brief Instrumentation function called before free()
 * @details Traces the buffer which is being freed, and removes the buffer from the outstanding allocations.
 *          if the buffer is on the list of outstanding allocations, also traces the size of the allocation being freed.
 *          Only takes action when a top-level function is active.
 * @param[in] data_ptr Data buffer being freed
 * @param[in] return_ip Return IP for free() call, which is traced
 */
static void before_free (ADDRINT data_ptr, ADDRINT return_ip)
{
    if (active_top_level_func_index != -1)
    {
        std::map<ADDRINT,ADDRINT>::iterator it;
        trace_file << top_level_func_names[active_top_level_func_index] << ",free,data_ptr=" << data_ptr << ",size=";
        it = outstanding_allocations.find (data_ptr);
        if (it != outstanding_allocations.end())
        {
            trace_file << it->second;
            outstanding_allocations.erase (it);
        }
        else
        {
            trace_file << "???";
        }
        trace_file << ",caller=" << RTN_FindNameByAddress (return_ip) << endl;
    }
}

/**
 * @brief Called at image load to insert instrumentation for a "top level" function in the program under test
 * @details The memory profile is gather for each "top level" function, where the top level functions are assumed
 *          to be called in series, i.e. not nested.
 * @param[in] image The image being loaded
 */
static void hook_top_level_function (IMG image, const char *func_name)
{
    const size_t func_index = top_level_func_names.size();
    RTN routine = RTN_FindByName (image, func_name);

    if (RTN_Valid (routine))
    {
        RTN_Open (routine);
        RTN_InsertCall (routine, IPOINT_BEFORE, (AFUNPTR) before_top_level_function,
                        IARG_ADDRINT, func_index,
                        IARG_END);
        RTN_InsertCall (routine, IPOINT_AFTER, (AFUNPTR) after_top_level_function,
                        IARG_ADDRINT, func_index,
                        IARG_END);
        RTN_Close (routine);

        top_level_func_names.push_back (func_name);
    }
}

/**
 * @brief Called at image load to insert instrumentation for the malloc() and free() memory allocation functions
 */
static void hook_memory_allocation (IMG image)
{
    RTN malloc_rtn = RTN_FindByName (image, "malloc");
    if (RTN_Valid (malloc_rtn))
    {
        RTN_Open (malloc_rtn);
        RTN_InsertCall (malloc_rtn, IPOINT_BEFORE, (AFUNPTR) before_malloc,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                        IARG_RETURN_IP,
                        IARG_END);
        RTN_InsertCall (malloc_rtn, IPOINT_AFTER, (AFUNPTR) after_malloc,
                        IARG_FUNCRET_EXITPOINT_VALUE,
                        IARG_END);
        RTN_Close (malloc_rtn);
    }

    RTN memalign_rtn = RTN_FindByName (image, "memalign");
    if (RTN_Valid (memalign_rtn))
    {
        RTN_Open (memalign_rtn);
        RTN_InsertCall (memalign_rtn, IPOINT_BEFORE, (AFUNPTR) before_memalign,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                        IARG_RETURN_IP,
                        IARG_END);
        RTN_InsertCall (memalign_rtn, IPOINT_AFTER, (AFUNPTR) after_memalign,
                        IARG_FUNCRET_EXITPOINT_VALUE,
                        IARG_END);
        RTN_Close (memalign_rtn);
    }

    RTN free_rtn = RTN_FindByName (image, "free");
    if (RTN_Valid (free_rtn))
    {
        RTN_Open (free_rtn);
        RTN_InsertCall (free_rtn, IPOINT_BEFORE, (AFUNPTR) before_free,
                        IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                        IARG_RETURN_IP,
                        IARG_END);
        RTN_Close (free_rtn);
    }
}

/**
 * @brief Called on a image load to instrument fixed functions
 * @param[in] image The image being loaded
 * @param[in] arg Instrumentation context - not used
 */
static void image_insert_calls (IMG image, void *arg)
{
    hook_top_level_function (image, "fft_initialise");
    hook_top_level_function (image, "set_fft_data");
    hook_top_level_function (image, "fft_execute");
    hook_top_level_function (image, "fft_free");
    hook_memory_allocation (image);
}

/**
 * @brief Called at program exit to display memory allocations which have not been explicitly freed.
 * @param[in] code Exit status from program - not used
 * @param[in] arg Instrumentation context - not used
 */
static void display_outstanding_allocations (INT32 code, void *arg)
{
    std::map<ADDRINT,ADDRINT>::const_iterator it;

    trace_file << "N/A,outstanding_allocations";
    for (it = outstanding_allocations.begin(); it != outstanding_allocations.end(); ++it)
    {
        trace_file << "," << it->first << "(" << it->second << ")";
    }
    trace_file << endl;
}

/**
 * @brief Display help usage
 */
static INT32 Usage()
{
    cerr << "This tool produces profiles the memory usage of the FFTW_example program." << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/**
 * @brief Pin tool entry point
 */
int main(int argc, char *argv[])
{
    /* Initialise pin & symbol manager */
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    /* Create trace file */
    trace_file.open(trace_filename.Value().c_str());
    trace_file << hex;
    trace_file.setf(ios::showbase);

    /* Set functions to install instrumentation */
    IMG_AddInstrumentFunction (image_insert_calls, NULL);
    INS_AddInstrumentFunction (instrument_memory_access, NULL);
    PIN_AddFiniFunction (display_outstanding_allocations, 0);

    /* Never returns */
    PIN_StartProgram();

    return 0;
}
