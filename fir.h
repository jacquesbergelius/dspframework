#pragma once
#include <windows.h>
#include <emmintrin.h>
#include <tmmintrin.h>
#include "wavIO.h"
#include "cirbuffer.h"

using namespace std;


class Fir: private CircularBuffer {
public:
	Fir(void *pCoeffs, size_t capacity): pC(pCoeffs), pC_len(capacity), CircularBuffer(capacity) {
	}

#if 0
	void process(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			write(input[i].left);										// add sample to the delay line

			double  acc = 0;
			INT16 *index = getPtr();
			for (double *h = (double *)BR; h < &BR[BL]; h++) {
				acc += (/*100.0**/(double)*index++/32768.0) * *h;		// MAC
				ptrCheck(index);										// wrap around circular buffer end
			};

			output[i].left = output[i].right = saturate(32768.0*acc);	// double -> Q15 format conversion with saturate and round
		}
	}
#elif 1
	void process(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			write(input[i].left);									// add sample to the delay line

			INT32  acc = 0x4000;									// Q30 -> Q15 rounding constant
			INT16 *index = getPtr();
			for (INT16 *h = (INT16 *)pC; h < &((INT16 *)pC)[pC_len]; h++) {
				acc += (INT32)*index++ * *h;						// Q15*Q15->Q30 MAC
				ptrCheck(index);									// wrap around circular buffer end
			};

			output[i].left = output[i].right = saturate(acc >> 15);	// Q30 -> Q15 format conversion with rounding and saturation
		}
	}
#elif 0
	void process(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			write(input[i].left);																		// add sample to the delay line

			__m128i acc    = _mm_set_epi32(0x0, 0x0, 0x0, 0x4000);										// Q30 -> Q15 rounding constant
			__m128i *h     = (__m128i *)pC;
			__m128i *index = (__m128i *)getPtr();
			for (unsigned j = 0; j < (pC_len+7)/8; j++) {
				acc = _mm_add_epi32(acc, _mm_madd_epi16(_mm_loadu_si128(index++), h[j]));				// Q15*Q15->Q30 MAC
				ptrCheck(index);																		// wrap around circular buffer end
			}

			acc = _mm_hadd_epi32(acc, acc);																// accumulate four partial values to one result
			acc = _mm_hadd_epi32(acc, acc);
			output[i].left = output[i].right = saturate(_mm_cvtsi128_si32(_mm_srai_epi32(acc, 15)));	// Q30 -> Q15 format conversion
		}
	}
#endif

	void test(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			write(input[i].left);
			INT16 *tmp = getPtr();

			output[i].left = output[i].right = *tmp;
		}
	}

private:
	void  *pC;
	size_t pC_len;
};

