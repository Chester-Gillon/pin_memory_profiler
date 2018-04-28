/*
 * @file FFTW_example.c
 * @date 5 Sep 2014
 * @author Chester Gillon
 * @details
 *   Perform a 64K point complex double forward FFT, as a demonstration of profiling the memory usage
 *   of functions using the Intel Pin Instrumentation tool.
 */

#include <stdlib.h>
#include <complex.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <fftw3.h>

#define N 65536
static fftw_complex *in, *out;
static fftw_plan p;
static bool in_place;

static void fft_initialise (void)
{
    const size_t array_size = sizeof(fftw_complex) * N;
    in = (fftw_complex*) fftw_malloc(array_size);
    out = (fftw_complex*) fftw_malloc(array_size);
    p = fftw_plan_dft_1d(N, in_place ? out : in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    /* Display the pointers to the allocated data, to help analysing the memory profile.
     * fftw_plan is opaque type, so can't display any of its internals */
    printf (in_place ? "In place selected\n" : "Out of place selected\n");
    printf ("fftw_plan_dft_1d returned %p\n", p);
    printf ("in=%p[%lu] out=%p[%lu]\n", in, array_size, out, array_size);
}

static void set_fft_data (void)
{
    int index;

    /* For a test of the memory_profile in is initialised with an incrementing address and out with a decrementing address */
    for (index = 0; index < N; index++)
    {
        in[index] = (rand() - (RAND_MAX / 2.0)) + (rand() - (RAND_MAX / 2.0)) * I;
        out[N - index - 1] = 0.0 + 0.0 * I;
    }
}

static void copy_input_data (void)
{
    if (in_place)
    {
        /* For in place need to copy the input data to the in-place buffer before each execution */
        memcpy (out, in, sizeof(fftw_complex) * N);
    }
}

static void fft_execute (void)
{
    fftw_execute(p); /* repeat as needed */
}

static void fft_free (void)
{
    fftw_destroy_plan(p);
    fftw_free(in);
    fftw_free(out);
}

int main (int argc, char *argv[])
{
    int iteration;
    int index;
    char *temp;

    in_place = false;
    for (index = 1; index < argc; index++)
    {
        if (strcmp (argv[index], "-in_place") == 0)
        {
            in_place = true;
        }
    }

    /* @todo Perform a dummy allocation and free, so that the first fftw_malloc() call in fft_initialise()
     *       has its memory allocation traced. */
    temp = fftw_malloc (0x100000);
    fftw_free (temp);

    fft_initialise ();
    set_fft_data ();

    for (iteration = 0; iteration < 5; iteration++)
    {
        copy_input_data ();
        fft_execute ();
    }

    fft_free ();

    return 0;
}
