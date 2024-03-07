#pragma once
#include <windows.h>
#include <math.h>
#include "wavIO.h"
#include "cirbuffer.h"

using namespace std;


class Allpass: private CircularBuffer {
public:
	Allpass(size_t capacity, float rvt): CircularBuffer(capacity) {
		g = (INT16)(pow(0.001f, ((float)capacity/FS) / rvt) * 32767.0f);
	}

	void process(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			INT16 delayedInput, out;

			delayedInput = read();
			out = saturate(input[i].left + mpy(delayedInput, -g));
			write(out);

			out = saturate(mpy(out, g) + delayedInput);

			output[i].left  = out;
			output[i].right = out;
		}
	}

private:
	INT16 g;
};

