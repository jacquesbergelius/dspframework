/*
 * wavIO.h -- Wav file input and output
 *
 * Reads and writes WAV files. 
 * Based on code by Evan Merz.
 *
 * Written by Jarkko Vuori 2012, 2013
 */


#pragma once
#include <windows.h>
#include <audioclient.h>
#include <fstream>
#include <iostream>
#include <string>

using namespace std;


struct pcm_frame {
	INT16 left;
	INT16 right;
};

class WavFileForIO {
/*
     WAV File Specification
     FROM http://ccrma.stanford.edu/courses/422/projects/WaveFormat/
    The canonical WAVE format starts with the RIFF header:
    0         4   ChunkID          Contains the letters "RIFF" in ASCII form
                                   (0x52494646 big-endian form).
    4         4   ChunkSize        36 + SubChunk2Size, or more precisely:
                                   4 + (8 + SubChunk1Size) + (8 + SubChunk2Size)
                                   This is the size of the rest of the chunk 
                                   following this number.  This is the size of the 
                                   entire file in bytes minus 8 bytes for the
                                   two fields not included in this count:
                                   ChunkID and ChunkSize.
    8         4   Format           Contains the letters "WAVE"
                                   (0x57415645 big-endian form).

    The "WAVE" format consists of two subchunks: "fmt " and "data":
    The "fmt " subchunk describes the sound data's format:
    12        4   Subchunk1ID      Contains the letters "fmt "
                                   (0x666d7420 big-endian form).
    16        4   Subchunk1Size    16 for PCM.  This is the size of the
                                   rest of the Subchunk which follows this number.
    20        2   AudioFormat      PCM = 1 (i.e. Linear quantization)
                                   Values other than 1 indicate some 
                                   form of compression.
    22        2   NumChannels      Mono = 1, Stereo = 2, etc.
    24        4   SampleRate       8000, 44100, etc.
    28        4   ByteRate         == SampleRate * NumChannels * BitsPerSample/8
    32        2   BlockAlign       == NumChannels * BitsPerSample/8
                                   The number of bytes for one sample including
                                   all channels. I wonder what happens when
                                   this number isn't an integer?
    34        2   BitsPerSample    8 bits = 8, 16 bits = 16, etc.
	36		  2	  ExtraParamSize   0 if PCM (may exists or not)

    The "data" subchunk contains the size of the data and the actual sound:
    38        4   Subchunk2ID      Contains the letters "data"
                                   (0x64617461 big-endian form).
    42        4   Subchunk2Size    == NumSamples * NumChannels * BitsPerSample/8
                                   This is the number of bytes in the data.
                                   You can also think of this as the size
                                   of the read of the subchunk following this 
                                   number.
    46        *   Data             The actual sound data.
*/


private:
	LPCWSTR	myPath;
	int 	myChunkSize;
	int	    mySubChunk1Size;
   	short 	myFormat;
	short 	myChannels;
	int   	mySampleRate;
	int   	myByteRate;
	short 	myBlockAlign;
	short 	myBitsPerSample;
	int	    myDataSize;
		
	BYTE 	*myData;
	BYTE    *pRead;

public:
	// get/set for the Path property
	LPCWSTR getPath() {
		return myPath;
	}

	void setPath(LPCWSTR newPath) {
		myPath = newPath;
	}

	~WavFileForIO() {
		myChunkSize = NULL;
		mySubChunk1Size = NULL;
	    myFormat = NULL;
		myChannels = NULL;
		mySampleRate = NULL;
		myByteRate = NULL;
		myBlockAlign = NULL;
		myBitsPerSample = NULL;
		myDataSize = NULL;
	}

	// empty constructor
	WavFileForIO() {
    }

	// constructor takes a wav path
	WavFileForIO(LPCWSTR tmpPath) {
		myPath = tmpPath;
    }

	// read a wav file into this class
	bool read() {
		class myexception: public exception {
		  virtual const char* what() const throw() {
			return "Incorrect WAV file";
		  }
		} noWAVexecption;
		ifstream inFile;
		UINT32   id;
		bool     fResult = true;

		inFile.exceptions(ifstream::failbit | ifstream::badbit | ifstream::eofbit);
		try {
			inFile.open(myPath, ios::in | ios::binary);

			inFile.read((char*) &id, 4);	// check the chunkid (RIFF)
			if (id != 0x46464952) throw noWAVexecption;

			inFile.read((char*) &myChunkSize, 4); // read the ChunkSize

			inFile.read((char*) &id, 4);	// check the format (WAVE)
			if (id != 0x45564157) throw noWAVexecption;

			inFile.read((char*) &id, 4);	// check the chunkid (fmt)
			if (id != 0x20746d66) throw noWAVexecption;

			//inFile.seekg(16, ios::beg);
			inFile.read((char*) &mySubChunk1Size, 4); // read the SubChunk1Size

			//inFile.seekg(20, ios::beg);
			inFile.read((char*) &myFormat, sizeof(short)); // read the file format.  This should be 1 for PCM
			if (myFormat != 1) throw noWAVexecption;

			//inFile.seekg(22, ios::beg);
			inFile.read((char*) &myChannels, sizeof(short)); // read the # of channels (1 or 2)
			if (myChannels != 2) throw noWAVexecption;

			//inFile.seekg(24, ios::beg);
			inFile.read((char*) &mySampleRate, sizeof(int)); // read the samplerate
			if (mySampleRate != 44100) throw noWAVexecption;

			//inFile.seekg(28, ios::beg);
			inFile.read((char*) &myByteRate, sizeof(int)); // read the byterate

			//inFile.seekg(32, ios::beg);
			inFile.read((char*) &myBlockAlign, sizeof(short)); // read the blockalign

			//inFile.seekg(34, ios::beg);
			inFile.read((char*) &myBitsPerSample, sizeof(short)); // read the bitspersample

			inFile.seekg(36, ios::beg);
			inFile.read((char*) &id, 4);	// check the chunkid (data)
			if (id != 0x61746164) {			// check if it exists in other position (after optional ExtraParamSize)
				inFile.seekg(38, ios::beg);
				inFile.read((char*) &id, 4);
				if (id != 0x61746164) throw noWAVexecption;
			}

			//inFile.seekg(40 or 42, ios::beg);
			inFile.read((char*) &myDataSize, sizeof(int)); // read the size of the data

			// read the data chunk
			myData = new BYTE[myDataSize];
			inFile.read((char *)myData, myDataSize);
			inFile.close(); // close the input file
		} catch (exception &e) {
			cout << "Problem opening/reading the file (" << e.what() << ")\n";
			myData = new BYTE[sizeof(pcm_frame)];
			myDataSize = 0;
			fResult = false;
		}

		pRead = myData;
		return fResult;
	}

	// write out the wav file
	bool save() {
		fstream myFile (myPath, ios::out | ios::binary);
		short extraParamSize = 0;

		// write the wav file per the wav file format
		myFile.seekp (0, ios::beg); 
		myFile.write ("RIFF", 4);
		myFile.write ((char*) &myChunkSize, 4);
		myFile.write ("WAVE", 4);
		myFile.write ("fmt ", 4);
		myFile.write ((char*) &mySubChunk1Size, 4);
		myFile.write ((char*) &myFormat, 2);
		myFile.write ((char*) &myChannels, 2);
		myFile.write ((char*) &mySampleRate, 4);
		myFile.write ((char*) &myByteRate, 4);
		myFile.write ((char*) &myBlockAlign, 2);
		myFile.write ((char*) &myBitsPerSample, 2);
		myFile.write ((char*) &extraParamSize, 2);
		myFile.write ("data", 4);
		myFile.write ((char*) &myDataSize, 4);
		myFile.write ((char *)myData, myDataSize);

		myFile.close();
		return true;
	}

	// return a printable summary of the wav file
	char *getSummary() {
		char *summary = new char[250];
		sprintf_s(summary, 250, " Format: %d\n Channels: %d\n SampleRate: %d\n ByteRate: %d\n BlockAlign: %d\n BitsPerSample: %d\n DataSize: %d\n", myFormat, myChannels, mySampleRate, myByteRate, myBlockAlign, myBitsPerSample, myDataSize);
		return summary;
	}

	// read next buffer
	bool LoadData(UINT32 bufferFrameCount, BYTE *pData, DWORD *flags) {
		//*flags = 0;
		if (pRead+bufferFrameCount*sizeof(pcm_frame) < myData+myDataSize) {
			// there is enough frames in the buffer
			memcpy(pData, pRead, bufferFrameCount*sizeof(pcm_frame));
			pRead += bufferFrameCount*sizeof(pcm_frame);
		} else {
			int n = (myData+myDataSize)-pRead;
			if (n > 0) {
				// not enough frames in the buffer, fill remaining requested data with zeroes
				memcpy(pData, pRead, n);
				memset(pData+n, 0, bufferFrameCount*sizeof(pcm_frame) - n);
				pRead = myData+myDataSize;
			} else {
				// nothing in the buffer, fill whole requested data with zeroes
				memset(pData, 0, bufferFrameCount*sizeof(pcm_frame));

				pRead = myData; 
			}
			//*flags = AUDCLNT_BUFFERFLAGS_SILENT;
		}

		return true;
	}
};
