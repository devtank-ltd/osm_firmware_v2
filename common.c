#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <math.h>


#include "uart_rings.h"
#include "measurements.h"


void tight_loop_contents(void)
{
    // Should be included in while loop when expected to last > 50ms
    uart_rings_in_drain();
    uart_rings_out_drain();
    measurements_loop_iteration();
}


void loose_loop_contents(void)
{
    // Should be looped about once a second
    lw_loop_iteration();
}



// Maths Functions

float Q_rsqrt( float number )
{
    long i;
    float x2, y;
    const float threehalfs = 1.5F;

    x2 = number * 0.5F;
    y  = number;
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wstrict-aliasing"
    i  = * ( long * ) &y;                       // evil floating point bit level hacking
    i  = 0x5f3759df - ( i >> 1 );               // what the fuck? 
    y  = * ( float * ) &i;
    #pragma GCC diagnostic pop
    y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//  y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

    return y;
}
