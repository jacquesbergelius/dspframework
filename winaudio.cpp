/*
 * winaudio.cpp -- Plays and Records MyAudio object on the default Windows audio device
 *
 * Plays and records simultaneously an exclusive-mode streams on the default
 * audio rendering and capture devices. Uses event-driven buffering and MMCSS to play the stream
 * at the minimum latency supported by the device.
 *
 * Written by Jarkko Vuori 2012, 2013
 */

#include <windows.h>
#include <stdio.h>
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <FunctionDiscoveryKeys.h>
#pragma comment(lib, "Avrt.lib")
#include "dsp.h"
#include "winaudio.h"


// REFERENCE_TIME time units per second and per millisecond
#define REFTIMES_PER_SEC	  10000000
#define REFTIMES_PER_MILLISEC 10000

#define EXIT_ON_ERROR(hres)  \
              if (FAILED(hres)) { printf("Error@'%s'(%d)\n", __FILE__, __LINE__); goto Exit; }
#define SAFE_RELEASE(punk)  \
              if ((punk) != NULL)  \
                { (punk)->Release(); (punk) = NULL; }

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);


/* identifies the given audio devices */
void identifyAudioDeviceType(IMMDevice *pDevice) {
	IPropertyStore *pProps = NULL;
	PROPVARIANT     varName;
    HRESULT         hr;

    hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    EXIT_ON_ERROR(hr)

    // Initialize container for property value
    PropVariantInit(&varName);

    // Get the endpoint's friendly-name property
    hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
    EXIT_ON_ERROR(hr)

    // Print endpoint friendly name and endpoint ID
    printf("%S\n", varName.pwszVal);

    PropVariantClear(&varName);
Exit:
    SAFE_RELEASE(pProps)
}


/* render event based audio playback */
HRESULT PlayAudioStream(MyAudio *pMyAudio) {
    HRESULT hr;
    REFERENCE_TIME hnsRequestedDuration = 0;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pRenderDevice = NULL;
    IAudioClient *pAudioRenderClient = NULL;
    IAudioRenderClient *pRenderClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    HANDLE hEvent = NULL;
    HANDLE hTask = NULL;
    UINT32 bufferFrameCount;
    BYTE *pCaptureData = NULL, *pRenderData = NULL;
    DWORD captureFlags = 0, renderFlags = 0;
	UINT32 frameCounter = 0;

    DWORD taskIndex = 0;

	// First find out the default audio render and capture devices
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDevice);
    EXIT_ON_ERROR(hr)

    hr = pRenderDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioRenderClient);
    EXIT_ON_ERROR(hr)

    // Check the source's audio stream format
    hr = pMyAudio->GetFormat(&pwfx);
    EXIT_ON_ERROR(hr)

    // Initialize the stream to play at the default device period
	// (there is no need to obtain minumum latency here, because the input comes from the audio file)
    hr = pAudioRenderClient->GetDevicePeriod(&hnsRequestedDuration, NULL);
    EXIT_ON_ERROR(hr)

    hr = pAudioRenderClient->Initialize(
                         AUDCLNT_SHAREMODE_EXCLUSIVE,
                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                         hnsRequestedDuration,
                         hnsRequestedDuration,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Create an event handle and register it for
    // buffer-event notifications
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL) {
        hr = E_FAIL;
        goto Exit;
    }
    hr = pAudioRenderClient->SetEventHandle(hEvent);
    EXIT_ON_ERROR(hr);

    // Get the actual size of the two allocated buffers
    hr = pAudioRenderClient->GetBufferSize(&bufferFrameCount);
    EXIT_ON_ERROR(hr)

    hr = pAudioRenderClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
    EXIT_ON_ERROR(hr)

	// create empty capture buffer (when microphone is not used)
	pCaptureData = new BYTE[bufferFrameCount*sizeof(pcm_frame)];
	memset(pCaptureData, 0, bufferFrameCount*sizeof(pcm_frame));

    // to reduce latency, load the first buffer with data
    // from the audio source before starting the stream
    hr = pRenderClient->GetBuffer(bufferFrameCount, &pRenderData);
    EXIT_ON_ERROR(hr)

	hr = pMyAudio->ProcessData(bufferFrameCount, pCaptureData, &captureFlags, pRenderData, &renderFlags);
    EXIT_ON_ERROR(hr)
    
	hr = pRenderClient->ReleaseBuffer(bufferFrameCount, renderFlags);
    EXIT_ON_ERROR(hr)

    // ask MMCSS to temporarily boost the thread priority
    // to reduce glitches while the low-latency stream plays
    hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    if (hTask == NULL) {
        hr = E_FAIL;
        EXIT_ON_ERROR(hr)
    }

	// start recording and playing
    hr = pAudioRenderClient->Start();
    EXIT_ON_ERROR(hr)

    // each loop fills one of the two buffers
    while (renderFlags != AUDCLNT_BUFFERFLAGS_SILENT) {
        // wait for next buffer event to be signaled
        DWORD retval = WaitForSingleObject(hEvent, 2000);
        if (retval != WAIT_OBJECT_0) {
            // event handle timed out after a 2-second wait
            pAudioRenderClient->Stop();
            hr = ERROR_TIMEOUT;
            goto Exit;
        }

		// grab the next empty buffers from the audio device
		hr = pRenderClient->GetBuffer(bufferFrameCount, &pRenderData);
		EXIT_ON_ERROR(hr)

		// process them
		hr = pMyAudio->ProcessData(bufferFrameCount, pCaptureData, &captureFlags, pRenderData, &renderFlags);
		memset(pCaptureData, 0, bufferFrameCount*sizeof(pcm_frame));
		EXIT_ON_ERROR(hr)

		// give processed buffers back to the audio device
		hr = pRenderClient->ReleaseBuffer(bufferFrameCount, renderFlags);
		EXIT_ON_ERROR(hr) 
    }

	// Stop playing and recording
	Sleep((DWORD)(hnsRequestedDuration/REFTIMES_PER_MILLISEC)); // Wait for the last buffer to play before stopping
    hr = pAudioRenderClient->Stop();
    EXIT_ON_ERROR(hr)

Exit:
	if (pCaptureData != NULL)
		delete[] pCaptureData;
    if (hEvent != NULL) {
        CloseHandle(hEvent);
    }
    if (hTask != NULL) {
        AvRevertMmThreadCharacteristics(hTask);
    }
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pRenderDevice)
    SAFE_RELEASE(pAudioRenderClient)
    SAFE_RELEASE(pRenderClient)

    return hr;
}


/* capture event based simultaneous audio playback and record */
HRESULT PlayAndRecordAudioStream(MyAudio *pMyAudio) {
    HRESULT hr;
    REFERENCE_TIME hnsMinPeriod = 0, hnsAlignedBufferDuration;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pRenderDevice = NULL; IMMDevice *pCaptureDevice = NULL;
    IAudioClient *pAudioRenderClient = NULL; IAudioClient *pAudioCaptureClient = NULL;
    IAudioRenderClient *pRenderClient = NULL; IAudioCaptureClient *pCaptureClient = NULL;
    WAVEFORMATEX *pwfx = NULL;
    HANDLE hEvent = NULL;
    HANDLE hTask = NULL;
    UINT32 captureBufferFrameCount, renderBufferFrameCount, numFramesAvailable;
    BYTE *pCaptureData, *pRenderData;
    DWORD captureFlags = 0, renderFlags = 0;
	UINT32 frameCounter = 0;

    DWORD taskIndex = 0;
    int cnt = 0;
    bool fStartRendering = true;
    UINT32 buffersize = 128;

	// First find out the default audio render and capture devices
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureDevice);
	EXIT_ON_ERROR(hr)
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDevice);
    EXIT_ON_ERROR(hr)

	hr = pCaptureDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioCaptureClient);
	EXIT_ON_ERROR(hr)
    hr = pRenderDevice->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioRenderClient);
    EXIT_ON_ERROR(hr)

    // Check the source's audio stream format
    hr = pMyAudio->GetFormat(&pwfx);
    EXIT_ON_ERROR(hr)

    // Initialize the stream to play at the minimum latency (stream buffer must be 128 bytes aligned for some audio drivers)
	// (NOTE!: USB A/D & D/A converters needs a little longer period than the given minimum)
    hr = pAudioRenderClient->GetDevicePeriod(NULL, &hnsMinPeriod);
    EXIT_ON_ERROR(hr)
	do {	// find out buffer period which is 128 byte aligned and larger than the minum device period
		buffersize               += 128;
		hnsAlignedBufferDuration  = (UINT32)(buffersize * 10e6 / pwfx->nSamplesPerSec + 0.5);	// convert to 100ns time units
	} while (hnsAlignedBufferDuration < hnsMinPeriod);

	hr = pAudioCaptureClient->Initialize(
							AUDCLNT_SHAREMODE_EXCLUSIVE,
							AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
							hnsAlignedBufferDuration,
							hnsAlignedBufferDuration,
							pwfx,
							NULL);
	EXIT_ON_ERROR(hr)
    hr = pAudioRenderClient->Initialize(
                         AUDCLNT_SHAREMODE_EXCLUSIVE,
                         0,
                         hnsAlignedBufferDuration,
                         hnsAlignedBufferDuration,
                         pwfx,
                         NULL);
    EXIT_ON_ERROR(hr)

    // Create an event handle and register it for
    // buffer-event notifications
    hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (hEvent == NULL) {
        hr = E_FAIL;
        goto Exit;
    }
    hr = pAudioCaptureClient->SetEventHandle(hEvent);
    EXIT_ON_ERROR(hr);

    // Get the actual size of the two allocated buffers
    hr = pAudioCaptureClient->GetBufferSize(&captureBufferFrameCount);
    EXIT_ON_ERROR(hr)
	hr = pAudioRenderClient->GetBufferSize(&renderBufferFrameCount);
	EXIT_ON_ERROR(hr)

	hr = pAudioCaptureClient->GetService(IID_IAudioCaptureClient, (void**)&pCaptureClient);
	EXIT_ON_ERROR(hr)
    hr = pAudioRenderClient->GetService(IID_IAudioRenderClient, (void**)&pRenderClient);
    EXIT_ON_ERROR(hr)

    // ask MMCSS to temporarily boost the thread priority
    // to reduce glitches while the low-latency stream plays
    hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
    if (hTask == NULL) {
        hr = E_FAIL;
        EXIT_ON_ERROR(hr)
    }

	// start recording
	hr = pAudioCaptureClient->Start();
	EXIT_ON_ERROR(hr)

    // each loop fills one of the two buffers
    while (renderFlags != AUDCLNT_BUFFERFLAGS_SILENT) {
        // wait for next buffer event to be signaled
        DWORD retval = WaitForSingleObject(hEvent, 2000);
        if (retval != WAIT_OBJECT_0) {
            // event handle timed out after a 2-second wait
            pAudioCaptureClient->Stop();
            hr = ERROR_TIMEOUT;
            goto Exit;
        }

		// grab the next capture buffer from the audio device
		hr = pCaptureClient->GetBuffer(&pCaptureData, &numFramesAvailable, &captureFlags, NULL, NULL); //printf("C%p ", pCaptureData);
		EXIT_ON_ERROR(hr)

		// first check that we have space for the rendering (under the heavy system load, it is possible to ran out of rendering buffer space)
		UINT32 alreadyUsed;
		pAudioRenderClient->GetCurrentPadding(&alreadyUsed);
		if (renderBufferFrameCount-alreadyUsed >= numFramesAvailable) { 
			// grab the next rendering buffer from the audio device
			hr = pRenderClient->GetBuffer(numFramesAvailable, &pRenderData); //printf("R%p ", pRenderData);
			EXIT_ON_ERROR(hr)

			// process them
			hr = pMyAudio->ProcessData(numFramesAvailable, pCaptureData, &captureFlags, pRenderData, &renderFlags);
			EXIT_ON_ERROR(hr)

			// give processed buffer back to the audio device
			hr = pRenderClient->ReleaseBuffer(numFramesAvailable, renderFlags);
			EXIT_ON_ERROR(hr)

			// start playing only after first captured frame is encountered
			// (otherwise the system does not start properly in some environments)
			if (fStartRendering) {
				hr = pAudioRenderClient->Start();
				EXIT_ON_ERROR(hr)
				fStartRendering = false;
			}

			// capture buffer processed, release it back to the audio device
			hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			EXIT_ON_ERROR(hr)
		} else {
			// rendering stream lagging, first release capture buffer back to the audio device
			hr = pCaptureClient->ReleaseBuffer(numFramesAvailable);
			EXIT_ON_ERROR(hr)

			// then clear the whole capture buffer, because render buffer is full
			printf("[%d](%d,%d)", cnt, numFramesAvailable,alreadyUsed);
			hr = pAudioCaptureClient->Stop();
			EXIT_ON_ERROR(hr)
			hr = pAudioCaptureClient->Reset();
			EXIT_ON_ERROR(hr)
			hr = pAudioCaptureClient->Start();
			EXIT_ON_ERROR(hr)
		}

		cnt++;
    }

	hr = pAudioCaptureClient->Stop();
    EXIT_ON_ERROR(hr)
	Sleep((DWORD)(hnsAlignedBufferDuration/REFTIMES_PER_MILLISEC)); // Wait for the last buffer to play before stopping
    hr = pAudioRenderClient->Stop();
    EXIT_ON_ERROR(hr)

Exit:
    if (hEvent != NULL) {
        CloseHandle(hEvent);
    }
    if (hTask != NULL) {
        AvRevertMmThreadCharacteristics(hTask);
    }
    CoTaskMemFree(pwfx);
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pRenderDevice)       SAFE_RELEASE(pCaptureDevice)
    SAFE_RELEASE(pAudioRenderClient)  SAFE_RELEASE(pAudioCaptureClient)
    SAFE_RELEASE(pRenderClient)		  SAFE_RELEASE(pCaptureClient)

    return hr;
}


/*
 * prints the name of the audio devices
 */
HRESULT PrintAudioDeviceNames(AudioThreadArgs *pArgs) {
	HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pRenderDevice = NULL; IMMDevice *pCaptureDevice = NULL;

	// First find out the default audio render and capture devices
    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**)&pEnumerator);
    EXIT_ON_ERROR(hr)

	if (pArgs->fInputEna) {
		hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pCaptureDevice);
		EXIT_ON_ERROR(hr)
		printf("Input: "); identifyAudioDeviceType(pCaptureDevice);
	}
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pRenderDevice);
    EXIT_ON_ERROR(hr)
	printf("Output: "); identifyAudioDeviceType(pRenderDevice);

Exit:
    SAFE_RELEASE(pEnumerator)
    SAFE_RELEASE(pRenderDevice)
	if (pArgs->fInputEna)
		SAFE_RELEASE(pCaptureDevice)

	return hr;
}


/*
 * records and plays audio streams
 * sets hr according to success or failure
 */
DWORD WINAPI AudioThreadFunction(LPVOID pContext) {
    AudioThreadArgs *pArgs = (AudioThreadArgs*)pContext;

    pArgs->hr = CoInitialize(NULL);
    if (FAILED(pArgs->hr)) {
        return 0;
    }

	// open only audio output device if input is not needed (playing from the audio file)
	if (pArgs->fInputEna)
		pArgs->hr = PlayAndRecordAudioStream(pArgs->audioSource);
	else
		pArgs->hr = PlayAudioStream(pArgs->audioSource);

    CoUninitialize();
    return 0;
}
