/*
 * dsp.cpp -- MyAudio dsp object
 *
 * Process an input stream and produces an output stream
 *
 * Written by Jarkko Vuori 2012, 2013, 2014
 */

#include <windows.h>
#include <audioclient.h>
#define _USE_MATH_DEFINES 
#include "dsp.h"
#include "fdacoefs.h"
#include "fdacoefs_bp1.h"
#include "fdacoefs_bp2.h"

#define DSPERROR	error_line = __LINE__

#define FS			44100	// sampling rate (Hz)
#define TIMINGS		100		// number of timing measurements taken


inline INT16 MyAudio::round(double x) {
   //assert(x >= INT16_MIN-0.5);
   //assert(x <= INT16_MAX+0.5);
   return (int) (x >= 0 ? (x+0.5) : (x-0.5));
}

inline INT16 MyAudio::sinewave() {
	// generate a new sample using IIR-resonator
	double y = 2.0*w*y1 - y2;
	y2 = y1; y1 = y;

	// convert it to the PCM format
	return round(32767*y);
}

MyAudio::MyAudio(): mode(filter_mode),
				    fir((void *)B, BL), fir1((void *)B1, BL12), fir2((void *)B2, BL12),
					comb1(5239, 1.0f), comb2(6544, 1.0f), comb3(7250, 1.0f), comb4(7708, 1.0f),
					ap1(220, 96.83e-3f), ap2(75, 32.92e-3f),
					chorus(1600, 2.0f, 0.9f),
					frame_cnt(0),
					frames(0), state(wait),
					wavfile(NULL),
					error_line(0) {
	w = cos(2.0*M_PI/40);	// f/fs = 40 (f = 1102,5 Hz when fs = 44100 Hz)

	y2 = sin(2.0*M_PI/40*0);
	y1 = sin(2.0*M_PI/40*1);
}

MyAudio::~MyAudio() {
}

HRESULT MyAudio::GetFormat(WAVEFORMATEX **pwfx) {
	WAVEFORMATEX *pwfx_l = (WAVEFORMATEX *)CoTaskMemAlloc(sizeof(WAVEFORMATEX));

	// stereo 16-bit, Fs = 44,1 kHz
	pwfx_l->wFormatTag      = WAVE_FORMAT_PCM;
	pwfx_l->nChannels       = 2;
	pwfx_l->nSamplesPerSec  = FS;
	pwfx_l->wBitsPerSample  = 16;
	pwfx_l->nBlockAlign     = (pwfx_l->nChannels * pwfx_l->wBitsPerSample) / 8;
	pwfx_l->nAvgBytesPerSec = pwfx_l->nSamplesPerSec * pwfx_l->nBlockAlign;
	pwfx_l->cbSize          = 0;

	*pwfx = pwfx_l;
	return S_OK;
}

HRESULT MyAudio::SetWavFileName(LPCWSTR name) {
	wavfile = new WavFileForIO(name);
	if (wavfile->read())
		return S_OK;
	else {
		wavfile->~WavFileForIO();
		wavfile = NULL;
		return E_FAIL;
	}
}

HRESULT MyAudio::ProcessData(UINT32 bufferFrameCount, BYTE *pCaptureData, DWORD *captureFlags, BYTE *pRenderData, DWORD *renderFlags) {
	pcm_frame *pInput  = (pcm_frame *)pCaptureData,
 		      *pOutput = (pcm_frame *)pRenderData;
	bool fSample = false;

	if (wavfile != NULL)
		wavfile->LoadData(bufferFrameCount, pCaptureData, captureFlags);

	switch (mode) {
	case filter_mode:
	case test_mode:
		*renderFlags = 0;

		// timing measurements
		switch (state) {
		case wait:
			// start measurements after cache has been settled
			if (frame_cnt++ == 10) {
				state = measureA;
				k = TIMINGS;
			}
			break;

		case measureA:
			// sample this period time
			period.Start();
			state = measureB;
			break;

		case measureB:
			// sample the next period time
			period.Stop();
			if (k--) {
				fSample = true;
			} else {
				state = sleep;
			}
			break;

		case sleep:
		default:
			break;
		}

		// then process all frames
		if (fSample) time.Start();
		if (mode == filter_mode) {
			float d = 0.0f;
			UINT32 i;

			fir1.process(pInput, pOutput, bufferFrameCount);
			for (i = 0; i < bufferFrameCount; i++)
				d += fabs((pOutput[i].left + pOutput[i].right) / 32768.0f);

			fir2.process(pInput, pOutput, bufferFrameCount);
			for (i = 0; i < bufferFrameCount; i++)
				d -= fabs((pOutput[i].left + pOutput[i].right) / 32768.0f);

			//printf("Value %f\n", fabs(d));
			if (fabs(d) > 24.0f)
				*renderFlags = AUDCLNT_BUFFERFLAGS_SILENT;
			//fir.process(pInput, pOutput, bufferFrameCount);
			//chorus.process(pInput, pOutput, bufferFrameCount);
	    } else {
			memset(pOutput, 0, bufferFrameCount*sizeof(pcm_frame));
			comb1.process(pInput, pOutput, bufferFrameCount);
			comb2.process(pInput, pOutput, bufferFrameCount);
			comb3.process(pInput, pOutput, bufferFrameCount);
			comb4.process(pInput, pOutput, bufferFrameCount);

			ap1.process(pOutput, pOutput, bufferFrameCount);
			ap2.process(pOutput, pOutput, bufferFrameCount);
		}
		if (fSample) {
			time.Stop();
			frames += bufferFrameCount;
			state = measureA;
		}
		break;

	case passthru_mode:
		// give all frames directly to the output
		for (UINT32 i = 0; i < bufferFrameCount; i++) {
			pOutput[i] = pInput[i];
		}
		break;

	case sinewave_mode:
		// give all frames directly to the output
		for (UINT32 i = 0; i < bufferFrameCount; i++) {
			pOutput[i].left = pOutput[i].right = sinewave();
		}
		break;

	default:
		*renderFlags = AUDCLNT_BUFFERFLAGS_SILENT;
		break;
	}

	return S_OK;
}

HRESULT MyAudio::SetMode(dsp_mode mode) {
	this->mode = mode;

	return S_OK;
}

HRESULT MyAudio::SetSineWaveFrequency(double frq) {
	w = cos(2.0*M_PI*(frq/FS));

	y2 = sin(2.0*M_PI*(frq/FS)*0);
	y1 = sin(2.0*M_PI*(frq/FS)*1);

	return S_OK;
}

HRESULT MyAudio::GetPerformance(double *period, double *dsptime, int *frames) {
	if (this->frames != 0) {
		*period  = this->period.Elapsed();
		*dsptime = time.Elapsed();
		*frames = this->frames / TIMINGS;

		return S_OK;
	} else
		return S_FALSE;
}

/* fStep false - impulse responce, true - step responce */
HRESULT MyAudio::SignalResponce(bool fStep, double *h, int *n) {
	double *p;
	int     len = fStep ? 2*BL : BL;

	pcm_frame *pInput  = new pcm_frame[len];
	pcm_frame *pOutput = new pcm_frame[len];
	for (int i = 0; i < len; i++) {
		pInput[i].left  = (i == 0 || fStep) ? 0x7fff : 0x0;
		pInput[i].right = 0;
	}

	fir.process(pInput, pOutput, len);

	p = h;
	for (int i = 0; i < len; i++) {
		*p++ = (double)pOutput[i].left/32768.0;
	}
	*n = len;

	return S_OK;
}