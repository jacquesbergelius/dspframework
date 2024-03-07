#ifndef _DSP_H
#define _DSP_H
#include <windows.h>
#include "fir.h"
#include "comb.h"
#include "allpass.h"
#include "chorus.h"
#include "wavIO.h"
#include "timer.h"

using namespace std;


enum dsp_mode {passthru_mode, filter_mode, sinewave_mode, test_mode, stop_mode};

class MyAudio {
public:
	MyAudio();
	~MyAudio();

	HRESULT SetWavFileName(LPCWSTR name);
	HRESULT GetFormat(WAVEFORMATEX **pwfx);
	HRESULT ProcessData(UINT32 bufferFrameCount, BYTE *pCaptureData, DWORD *captureFlags, BYTE *pRenderData, DWORD *renderFlags);
	HRESULT SetMode(dsp_mode mode);
	HRESULT SignalResponce(bool fStep, double *h, int *n);
	HRESULT SetSineWaveFrequency(double frq);
	HRESULT GetPerformance(double *period, double *dsptime, int *frames);

	int error() const { return error_line; }

private:
	inline INT16 round(double x);
	inline INT16 sinewave();

	volatile dsp_mode  mode;
	WavFileForIO      *wavfile;
	Fir                fir, fir1, fir2;
	Comb			   comb1, comb2, comb3, comb4;
	Allpass			   ap1, ap2;
	Chorus             chorus;

	Timer									 period, time;
	UINT64                                   frame_cnt;
	enum { wait, measureA, measureB, sleep } state;
	int                                      k, frames;

	double w;
	double y1, y2;
	
	int    error_line;
};
#endif
