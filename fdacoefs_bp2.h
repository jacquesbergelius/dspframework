/*
 * Filter Coefficients (C Source) generated by the Filter Design and Analysis Tool
 * Generated by MATLAB(R) 8.1 and the DSP System Toolbox 8.4.
 * Generated on: 21-Mar-2014 09:13:52
 */

/*
 * Discrete-Time FIR Filter (real)
 * -------------------------------
 * Filter Structure  : Direct-Form FIR
 * Filter Length     : 96
 * Stable            : Yes
 * Linear Phase      : Yes (Type 2)
 * Arithmetic        : fixed
 * Numerator         : s16,19 -> [-6.250000e-02 6.250000e-02)
 * Input             : s16,15 -> [-1 1)
 * Filter Internals  : Full Precision
 *   Output          : s36,34 -> [-2 2)  (auto determined)
 *   Product         : s31,34 -> [-6.250000e-02 6.250000e-02)  (auto determined)
 *   Accumulator     : s36,34 -> [-2 2)  (auto determined)
 *   Round Mode      : No rounding
 *   Overflow Mode   : No overflow
 */

/* General type conversion for MATLAB generated C-code  */
#include "tmwtypes.h"
/* 
 * Expected path to tmwtypes.h 
 * C:\Program Files\MATLAB\R2013a\extern\include\tmwtypes.h 
 */
/*
 * Warning - Filter coefficients were truncated to fit specified data type.  
 *   The resulting response may not match generated theoretical response.
 *   Use the Filter Design & Analysis Tool to design accurate
 *   int16 filter coefficients.
 */
//const int BL = 96;
const int16_T B2[96] = {
     3477,   -193,   -255,   -346,   -446,   -535,   -595,   -613,   -585,
     -516,   -419,   -317,   -231,   -184,   -191,   -255,   -368,   -508,
     -643,   -735,   -749,   -645,   -423,    -78,    364,    854,   1333,
     1728,   1970,   2002,   1789,   1325,    637,   -213,  -1138,  -2032,
    -2782,  -3287,  -3470,  -3286,  -2735,  -1865,   -755,    472,   1683,
     2740,   3522,   3937,   3937,   3522,   2740,   1683,    472,   -755,
    -1865,  -2735,  -3286,  -3470,  -3287,  -2782,  -2032,  -1138,   -213,
      637,   1325,   1789,   2002,   1970,   1728,   1333,    854,    364,
      -78,   -423,   -645,   -749,   -735,   -643,   -508,   -368,   -255,
     -191,   -184,   -231,   -317,   -419,   -516,   -585,   -613,   -595,
     -535,   -446,   -346,   -255,   -193,   3477
};
