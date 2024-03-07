#pragma once
#include <windows.h>
#include <math.h>
#include "wavIO.h"
#include "cirbuffer.h"

using namespace std;

#define TDLY	0.025f	// average chorus delay (in s)
#define TCH		0.005f	// chorus delay change (in s)


class Chorus: private CircularBuffer {
public:
	Chorus(size_t capacity, float lfo, float g): CircularBuffer(capacity) {
		this->g = g;
		step_   = 2.0f * lfo / (FS*TCH);
		minSweepSamples_ = TDLY*FS - TCH*FS;
		maxSweepSamples_ = TDLY*FS + TCH*FS;
		sweep_ = (minSweepSamples_ + maxSweepSamples_) / 2.0f;
	}

	void process(const pcm_frame *input, pcm_frame *output, const UINT32 samples) {
		for (UINT32 i = 0; i < samples; i++) {
			// assemble mono input value and store it in circular buffer
			write(input[i].left);

			// build the two read pointers and do linear interpolation
			int   ep1, ep2;
			float w1, w2, outval;
			w2 = modf(sweep_, &outval); ep1 = (int)outval;
			ep2 = ep1 + 1;
			w1  = 1.0f - w2;
			outval = w1*(float)readpos(ep1) + w2*(float)readpos(ep2);

			// develop output mix
			output[i].left  = saturate((float)input[i].left + g*outval);
			output[i].right = saturate((float)input[i].left + g*outval);

			// increment the sweep
			sweep_ += step_;
			if (sweep_ >= maxSweepSamples_ || sweep_ <= minSweepSamples_)
				step_ = -step_;
		}
	}

private:
	float g;
	float sweep_, step_;
	float maxSweepSamples_, minSweepSamples_;
};

