/*
 * main.cpp -- Framework to interface user's DSP object to the Windows audio input and output devices
 *
 * Plays and records simultaneously with a very low latency (256 samples, abt. 5.8 ms) on the default
 * audio rendering device using WASAPI. Is also able to use WAV-file (stereo, 44.1 Khz, 16-bit) as an input
 * instead of the default audio (microphone) input.
 *
 * Dec 2012		development starts
 * Jan 2013		Steinberg VST plug-ins like programming interface for DSP objects
 * Feb 2013		event based audio processing for the audio capturing to decrease latencies
 * Apr 2014		Program termination possible also from the signal processing module (new thread for the UI)
 *
 * Written by Jarkko Vuori 2012, 2013, 2014
 */

#include <windows.h>
#include <comdef.h>
#include <stdio.h>
#include <conio.h>
#include <fstream>
#include "dsp.h"
#include "winaudio.h"


/* evaluates impulse responce of the system and stores it to the given file */
int testImpulseResponce(MyAudio *pAudio, LPCWSTR szFilename) {
	double *h = new double[10000];
	int     n, result = 0;
	FILE   *fp;

	if (_wfopen_s(&fp, szFilename, L"w") == 0) {
		if (pAudio->SignalResponce(false, h, &n) == S_OK && n < 10000) {
			printf("Impulse responce:\n");
			for (int i = 0; i < n; i++) {
				printf("%+10.7lf\n", h[i]);
				fprintf(fp, "%4d: %+10.7lf\n", i, h[i]);
			}
		} else
			result = -__LINE__;

		fclose(fp);
	} else
		result = -__LINE__;

	delete [] h;
	return result;
}

/* for the internal test only: generates signal blocks to the system and observes the result */
int internalTest(MyAudio *pAudio) {
	UINT32  numFramesAvailable = 441;
	BYTE   *pInputData  = new BYTE[numFramesAvailable*sizeof(pcm_frame)]; 
	BYTE   *pOutputData = new BYTE[numFramesAvailable*sizeof(pcm_frame)]; 
	DWORD   captureFlags = 0, renderFlags;
	HRESULT hr;
	int     result = 0;

	for (int i = 0; i < 10; i++) {
		pcm_frame *p;

		memset(pInputData, 0, numFramesAvailable*sizeof(pcm_frame));
		hr = pAudio->ProcessData(numFramesAvailable, pInputData, &captureFlags, pOutputData, &renderFlags);

		printf("FRAME: ");
		p = (pcm_frame *)pOutputData;
		for (UINT32 i = 0; i < numFramesAvailable; i++)
			printf("%d ", p++->left);
	}

	delete pInputData;
	delete pOutputData;
	return result;
}

void usage(LPCWSTR exe) {
    wprintf(
		L"usage:\n"
        L"  %ls -?\n"
        L"  %ls --sine\n"
        L"  %ls --impulse <filename>\n"
		L"  %ls --file <wavefilename>\n"
		L"  %ls --test\n"
        L"\n",
		exe, exe, exe, exe, exe
    );
}

/* parses user command line arguments */
class CPrefs {
public:
    LPCWSTR szImpulseFilename, szWaveFilename;
	int     Hz;
	bool    fTest;

    // set hr to S_FALSE to abort but return success
    CPrefs(int argc, LPCWSTR argv[], HRESULT &hr);
    ~CPrefs();

};

CPrefs::CPrefs(int argc, LPCWSTR argv[], HRESULT &hr)
: Hz(0)
, fTest(false)
, szImpulseFilename(NULL)
, szWaveFilename(NULL) {
    switch (argc) {
        case 2:
            if (0 == _wcsicmp(argv[1], L"-?") || 0 == _wcsicmp(argv[1], L"/?")) {
                // print usage but don't actually anything
                hr = S_FALSE;
                usage(argv[0]);
                return;
            } else if (0 == _wcsicmp(argv[1], L"--test")) {
                // activate an internal test
                fTest = true;
                return;
            }

        // intentional fallthrough
        default:
            // loop through arguments and parse them
            for (int i = 1; i < argc; i++) {
                
                // --sine
                if (0 == _wcsicmp(argv[i], L"--sine")) {
                    if (0 != Hz) {
                        printf("Only one --sine switch is allowed\n");
                        hr = E_INVALIDARG;
                        return;
                    }

                    if (i++ == argc) {
                        printf("--sine switch requires an argument\n");
                        hr = E_INVALIDARG;
                        return;
                    }

					Hz = _wtoi(argv[i]);
					if (Hz < 0 || Hz > 22050) {
						hr = S_FALSE;
						return;
					}
					
                    continue;
                }

                // --impulse
                if (0 == _wcsicmp(argv[i], L"--impulse")) {
                    if (NULL != szImpulseFilename) {
                        printf("Only one --impulse switch is allowed\n");
                        hr = E_INVALIDARG;
                        return;
                    }

                    if (i++ == argc) {
                        printf("--impulse switch requires an argument\n");
                        hr = E_INVALIDARG;
                        return;
                    }

                    szImpulseFilename = argv[i];
                    continue;
                }

                // --file
                if (0 == _wcsicmp(argv[i], L"--file")) {
                    if (NULL != szWaveFilename) {
                        printf("Only one --file switch is allowed\n");
                        hr = E_INVALIDARG;
                        return;
                    }

                    if (i++ == argc) {
                        printf("--file switch requires an argument\n");
                        hr = E_INVALIDARG;
                        return;
                    }

                    szWaveFilename = argv[i];
                    continue;
                }

                printf("Invalid argument '%ls'\n", argv[i]);
                hr = E_INVALIDARG;
                return;
            }
    }
}

CPrefs::~CPrefs() {
	// close open handles here
}

DWORD WINAPI UserInterfaceFunction(LPVOID pContext) {
	AudioThreadArgs *pArgs = (AudioThreadArgs*)pContext;

	// wait for the user command
	printf("\nPress\n"
		"  'X' to stop processing\n"
		"  'F' to filter\n"
		"  'D' to direct output without any processing\n"
		"  'S' to generate sinusoidal signal\n"
		"  'T' to test special signal processing block\n"
		);
	wchar_t ch;
	do {
		ch = toupper(_getwch());

		switch (ch) {
		case L'S':
			pArgs->audioSource->SetMode(sinewave_mode);
			break;

		case L'D':
			pArgs->audioSource->SetMode(passthru_mode);
			break;

		case L'F':
			pArgs->audioSource->SetMode(filter_mode);
			break;

		case L'T':
			pArgs->audioSource->SetMode(test_mode);
			break;

		case L'X':
			pArgs->audioSource->SetMode(stop_mode);
			break;
		}
	} while (ch != L'X');
	printf("\n");

	return S_OK;
}

int _cdecl wmain(int argc, LPCWSTR argv[]) {
	AudioThreadArgs pta = { NULL, true, E_UNEXPECTED };
	MyAudio         audioSource;
	HRESULT         hr = S_OK;
	int             result = 0;

	printf("Audio front end for DSP objects (%s)\n\n", __DATE__);

	// parse command line
	CPrefs prefs(argc, argv, hr);
	if (E_INVALIDARG == hr || S_FALSE == hr) {
		// nothing to do
		goto wmerr;
	}
	else if (FAILED(hr)) {
		printf("CPrefs::CPrefs constructor failed: hr = 0x%08x\n", hr);
		result = -__LINE__;
		goto wmerr;
	}

	// special internal test
	if (prefs.fTest) {
		result = internalTest(&audioSource);
		goto wmerr;
	}

	// impulse responce
	if (prefs.szImpulseFilename != NULL) {
		if (testImpulseResponce(&audioSource, prefs.szImpulseFilename) < 0) {
			printf("testImpulseResponce failed\n");
			result = -__LINE__;
		}
		else
			result = 0;
		goto wmerr;
	}

	// wav file
	if (prefs.szWaveFilename != NULL) {
		if (audioSource.SetWavFileName(prefs.szWaveFilename) != S_OK)
			result = -__LINE__;
		else {
			pta.fInputEna = false;	// if we read samples from the file, audio input is not needed
			result = 0;
		}
	}

	// sinewave generation
	if (prefs.Hz) {
		audioSource.SetSineWaveFrequency(prefs.Hz);
	}

	hr = CoInitialize(NULL);
	if (FAILED(hr)) {
		printf("CoInitialize failed: hr = 0x%08x", hr);
		result = -__LINE__;
		goto wmerr;
	}

	// start the signal processing and user interface threads
	pta.audioSource = &audioSource;
	PrintAudioDeviceNames(&pta);
	HANDLE hThreads[2];
	hThreads[0] = CreateThread(
		NULL,
		0,
		AudioThreadFunction,
		&pta,
		0,
		NULL
		);
	if (hThreads[0] == NULL) {
		printf("CreateThread failed: GetLastError = %u\n", GetLastError());
		result = -__LINE__;
		goto wmerri;
	}

	hThreads[1] = CreateThread(
		NULL,
		0,
		UserInterfaceFunction,
		&pta,
		0,
		NULL
		);
	if (hThreads[1] == NULL) {
		printf("CreateThread failed: GetLastError = %u\n", GetLastError());
		result = -__LINE__;
		goto wmerri;
	}

	// stop if one of threads has been ended
	WaitForMultipleObjects(2, hThreads, FALSE, INFINITE);
	if (FAILED(pta.hr)) {
		_com_error err(pta.hr);
		LPCTSTR errMsg = err.ErrorMessage();
		printf("Play Thread returned failing HRESULT 0x%08x: %ls\n", pta.hr, errMsg);
		CloseHandle(hThreads[0]);
	}

	// give a statement about the performance
	double cycle, process_time;
	int    frames;
	if (audioSource.GetPerformance(&cycle, &process_time, &frames) == S_OK) {
		printf("Cycle time %.2lf ms, processing time %.2lf ms (load %.1lf%%)\n", cycle, process_time, cycle > 0 ? process_time/cycle*100 : 0.0);
		printf("%d frames per one buffer\n", frames);
	}
	if (audioSource.error() != 0)
		printf("There was an error on the dsp object at line %d\n", audioSource.error());

wmerri:
    CoUninitialize();
wmerr:
#ifdef DEBUG
	system("PAUSE");	// so that the user can see the results before windows close
#endif
    return result;
}




