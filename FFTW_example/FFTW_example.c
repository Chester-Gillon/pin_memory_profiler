/*
 * FFTW_example.c
 *
 *  Created on: 5 Sep 2014
 *      Author: Mr_Halfword
 */

#include <stdlib.h>
#include <complex.h>
#include <fftw3.h>

#define N 65536
static fftw_complex *in, *out;
static fftw_plan p;

static void fft_initialise (void)
{
    const size_t array_size = sizeof(fftw_complex) * N;
    in = (fftw_complex*) fftw_malloc(array_size);
    out = (fftw_complex*) fftw_malloc(array_size);
    p = fftw_plan_dft_1d(N, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    printf ("in=%p[%lu] out=%p[%lu]\n", in, array_size, out, array_size);
}

static void set_fft_data (void)
{
    int index;

    for (index = 0; index < N; index++)
    {
        in[index] = (rand() - (RAND_MAX / 2.0)) + (rand() - (RAND_MAX / 2.0)) * I;
        out[index] = 0.0 + 0.0 * I;
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

    fft_initialise ();
    set_fft_data ();

    for (iteration = 0; iteration < 5; iteration++)
    {
        fft_execute ();
    }

    fft_free ();

    return 0;
}
