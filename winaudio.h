#include <windows.h>
#include "dsp.h"


// pass an address to this structure to AudioThreadFunction
struct AudioThreadArgs {
	MyAudio *audioSource;
	bool     fInputEna;
    HRESULT hr;
};

DWORD WINAPI AudioThreadFunction(LPVOID pContext);
HRESULT      PrintAudioDeviceNames(AudioThreadArgs *pArgs);
