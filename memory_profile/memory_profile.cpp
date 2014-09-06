/*
 * memory_profile.cpp
 *
 *  Created on: 5 Sep 2014
 *      Author: Mr_Halfword
 */

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

/** Records memory allocations, which are then removed when freed.
 *  Used to report memory which is allocated but not freed upon program completion.
 *  Key is address of allocation, data is size
 */
static std::map<ADDRINT,ADDRINT> outstanding_allocations;

class memory_regions_usage
{
public:
    void clear (void);
    void display (const std::string &prefix);
    void record_access (ADDRINT memory_addr, UINT32 bytes_accessed);

private:
    /** The information maintained for each non-consecutive memory region */
    struct region_info
    {
        /** The end address of the region */
        UINT64 region_end_addr;
        /** The total number of bytes which have been accessed in the region */
        UINT64 total_bytes;
    };

    /** Which memory regions have been accessed.
     *  Key is the start address, data is the region information
     */
    std::map<ADDRINT,region_info> memory_regions;
};

/** Used to record the memory regions read/written by the current active top level function */
static memory_regions_usage read_memory_regions;
static memory_regions_usage write_memory_regions;

void memory_regions_usage::clear(void)
{
    memory_regions.clear();
}

void memory_regions_usage::record_access (const ADDRINT access_start_addr, const UINT32 bytes_accessed)
{
    const ADDRINT access_end_addr = access_start_addr + bytes_accessed - 1;
    region_info new_region;
    std::map<ADDRINT,region_info>::iterator begin_it, end_it, current_it, next_it;
    bool region_processed = false;
    bool region_addrs_changed = false;
    ADDRINT modified_start_addr = access_start_addr;

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
                new_region = current_it->second;
                new_region.total_bytes += bytes_accessed;
                if (access_end_addr > new_region.region_end_addr)
                {
                    new_region.region_end_addr = access_end_addr;
                }
                memory_regions.erase (current_it);
                memory_regions[access_start_addr] = new_region;
                end_it = memory_regions.upper_bound(access_end_addr + 1);
                region_processed = true;
                region_addrs_changed = true;
                modified_start_addr = access_start_addr;
            }
            else if ((access_start_addr >= current_it->first) && (access_end_addr <= current_it->second.region_end_addr))
            {
                /* The memory access is entirely within an existing region */
                current_it->second.total_bytes += bytes_accessed;
                region_processed = true;
            }
            else if ((access_start_addr <= current_it->second.region_end_addr) &&
                     (access_end_addr > current_it->second.region_end_addr))
            {
                /* The memory access overlaps the end of an existing region */
                current_it->second.region_end_addr = access_end_addr;
                current_it->second.total_bytes += bytes_accessed;
                region_processed = true;
                region_addrs_changed = true;
                modified_start_addr = current_it->first;
            }
        }
    }

    if (!region_processed)
    {
        /* Insert as a new region */
        new_region.region_end_addr = access_end_addr;
        new_region.total_bytes = bytes_accessed;
        memory_regions[access_start_addr] = new_region;
        region_addrs_changed = true;
        modified_start_addr = access_start_addr;
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
        while (next_it != memory_regions.end())
        {
            while ((next_it != memory_regions.end()) && ((current_it->second.region_end_addr + 1) >= next_it->first))
            {
                /* Regions are adjacent - so combine */
                current_it->second.region_end_addr = next_it->second.region_end_addr;
                current_it->second.total_bytes += next_it->second.total_bytes;
                memory_regions.erase (next_it);
                next_it = current_it;
                ++next_it;
            }

            ++current_it;
            next_it = current_it;
            ++next_it;
        }
    }
}

void memory_regions_usage::display (const std::string &prefix)
{
    std::map<ADDRINT,region_info>::const_iterator it;

    for (it = memory_regions.begin(); it != memory_regions.end(); ++it)
    {
        trace_file << prefix << ",start_addr=" << it->first << ",end_addr=" << it->second.region_end_addr
                << ",size=" << (it->second.region_end_addr - it->first + 1)
                << ",total_bytes_accessed=" << it->second.total_bytes << endl;
    }
}

static void memory_access_analysis (memory_regions_usage *const memory_regions, ADDRINT memory_addr, UINT32 bytes_accessed)
{
    if (active_top_level_func_index != -1)
    {
        memory_regions->record_access (memory_addr, bytes_accessed);
    }
}

/**
 * @brief Is called for every instruction and instruments memory reads and writes
 */
static void instrument_memory_access (INS ins, void *v)
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

static void before_malloc (ADDRINT size, ADDRINT return_ip)
{
    malloc_requested_size = size;
    malloc_return_ip = return_ip;
}

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

static void image_insert_calls (IMG image, void *arg)
{
    hook_top_level_function (image, "fft_initialise");
    hook_top_level_function (image, "set_fft_data");
    hook_top_level_function (image, "fft_execute");
    hook_top_level_function (image, "fft_free");
    hook_memory_allocation (image);
}

static void display_outstanding_allocations (INT32 code, void *v)
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
