#ifndef ___MP3_SIMPLE_H_INCLUDED___
#define ___MP3_SIMPLE_H_INCLUDED___

#include <windows.h>
#include "INCLUDE/BladeMP3EncDLL.h"
#include "INCLUDE/sync_simple.h"

// Pointers to LAME API functions
BEINITSTREAM		beInitStream	=NULL;
BEENCODECHUNK		beEncodeChunk	=NULL;
BEDEINITSTREAM		beDeinitStream	=NULL;
BECLOSESTREAM		beCloseStream	=NULL;
BEVERSION		beVersion	=NULL;
BEWRITEVBRHEADER	beWriteVBRHeader=NULL;
BEWRITEINFOTAG		beWriteInfoTag	=NULL;

//---------------------------- CLASS -------------------------------------------------------------

// Class that implements basic MP3 encoding, by wrapping LAME API.
// This class can be extended to add extra functionality and customization features.
class CMP3Simple {
private:
	static QMutex m_qMutex;
	static bool m_isLibLoaded;

	BE_CONFIG	beConfig;
	HBE_STREAM	hbeStream;
	DWORD		dwMP3Buffer;
	DWORD		dwPCMBuffer;

public:
	// This static method performs LAME API initialization
	static void LoadLIBS();

	// Constructer of the class accepts only three parameters.
	// Feel free to add more constructors with different parameters, if a better
	// customization is necessary.
	//
	// nBitRate - says at what bitrate to encode the raw (PCM) sound
	// (e.g. 16, 32, 40, 48, ... 64, ... 96, ... 128, etc), see official LAME documentation
	// for accepted values.
	//
	// nInputSampleRate - expected input frequency of the raw (PCM) sound
	// (e.g. 44100, 32000, 22050, etc), see official LAME documentation
	// for accepted values.
	//
	// nOutSampleRate - requested frequency for the encoded/output (MP3) sound.
	// If equal with zero, then sound is not
	// re-sampled (nOutSampleRate = nInputSampleRate).
	CMP3Simple(unsigned int nBitRate, unsigned int nInputSampleRate = 44100,
		unsigned int nOutSampleRate = 0);


	~CMP3Simple();

	// This method performs encoding.
	//
	// pSamples - pointer to the buffer containing raw (PCM) sound to be encoded.
	// Mind that buffer must be an array of SHORT (16 bits PCM stereo sound, for
	// mono 8 bits PCM sound better to double every byte to obtain 16 bits).
	//
	// nSamples - number of elements in "pSamples" (SHORT). Not to be confused with
	// buffer size which represents (usually) volume in bytes. See also
	// "MaxInBufferSize" method.
	//
	// pOutput - pointer to the buffer that will receive encoded (MP3) sound, here
	// we have bytes already. LAME says that if pOutput is not cleaned before call,
	// data in pOutput will be mixed with incoming data from pSamples.
	//
	// pdwOutput - pointer to a variable that will receive the number of bytes written
	// to "pOutput". See also "MaxOutBufferSize" method.
	BE_ERR Encode(PSHORT pSamples, DWORD nSamples, PBYTE pOutput, PDWORD pdwOutput);

	// Returns maximum suggested number of elements (SHORT) to send to "Encode" method.
	// e.g. PSHORT pSamples = (PSHORT) malloc(sizeof(SHORT) * MaxInBufferSize())
	// or PSHORT pSamples = new SHORT[MaxInBufferSize()]
	DWORD MaxInBufferSize() const { return this->dwPCMBuffer; }

	// Returns minimum suggested buffer size (in bytes) for the buffer that will
	// receive encoded (MP3) sound, see "Encode" method for more details.
	DWORD MinOutBufferSize() const { return this->dwMP3Buffer; }

	// Returns requested bitrate for the MP3 sound.
	DWORD BitRate() const { return this->beConfig.format.LHV1.dwBitrate; }

	// Returns expected input frequency of the raw (PCM) sound
	DWORD InSampleRate() const { return this->beConfig.format.LHV1.dwSampleRate; }

	// Returns requested frequency for the encoded/output (MP3) sound.
	DWORD OutSampleRate() const {
		if (this->beConfig.format.LHV1.dwReSampleRate == 0) {
			// If equal with zero, then sound is not
			// re-sampled (nOutSampleRate = nInputSampleRate).
			return this->beConfig.format.LHV1.dwSampleRate;
		}

		return this->beConfig.format.LHV1.dwReSampleRate;
	}
};

//---------------------------- IMPLEMENTATION ----------------------------------------------------
bool CMP3Simple::m_isLibLoaded = false;
QMutex CMP3Simple::m_qMutex;

BE_ERR CMP3Simple::Encode(PSHORT pSamples, DWORD nSamples, PBYTE pOutput, PDWORD pdwOutput) {
	return beEncodeChunk(this->hbeStream, nSamples, pSamples, pOutput, pdwOutput);
}

CMP3Simple::CMP3Simple(unsigned int nBitRate, unsigned int nInputSampleRate,
						   unsigned int nOutSampleRate) {
	BE_ERR		err = 0;

	CMP3Simple::LoadLIBS();

	this->dwMP3Buffer = 0;
	this->dwPCMBuffer = 0;
	this->hbeStream = 0;
	memset(&this->beConfig, 0, sizeof(BE_CONFIG));

	this->beConfig.dwConfig = BE_CONFIG_LAME;
	this->beConfig.format.LHV1.dwStructVersion = 1;
	this->beConfig.format.LHV1.dwStructSize = sizeof(BE_CONFIG);

	// OUTPUT IN STREO
	this->beConfig.format.LHV1.nMode = BE_MP3_MODE_JSTEREO;	

	// QUALITY PRESET SETTING, CBR = Constant Bit Rate
	this->beConfig.format.LHV1.nPreset = LQP_CBR;

	// USE DEFAULT PSYCHOACOUSTIC MODEL
	this->beConfig.format.LHV1.dwPsyModel = 0;

	// NO EMPHASIS TURNED ON
	this->beConfig.format.LHV1.dwEmphasis = 0;

	// SET ORIGINAL FLAG
	this->beConfig.format.LHV1.bOriginal = TRUE;

	// Write INFO tag
	this->beConfig.format.LHV1.bWriteVBRHeader = FALSE;

	// No Bit resorvoir
	this->beConfig.format.LHV1.bNoRes = TRUE;		

	this->beConfig.format.LHV1.bPrivate = TRUE;
	this->beConfig.format.LHV1.bCopyright = TRUE;

	// MINIMUM BIT RATE
	this->beConfig.format.LHV1.dwBitrate = nBitRate;

	// MPEG VERSION (I or II)
	this->beConfig.format.LHV1.dwMpegVersion = (nBitRate > 32)?MPEG1:MPEG2;

	// INPUT FREQUENCY
	this->beConfig.format.LHV1.dwSampleRate	= nInputSampleRate;

	// OUTPUT FREQUENCY, IF == 0 THEN DON'T RESAMPLE
	this->beConfig.format.LHV1.dwReSampleRate = nOutSampleRate;

  	err = beInitStream(&this->beConfig, &this->dwPCMBuffer, &this->dwMP3Buffer, &this->hbeStream);
	if(err != BE_ERR_SUCCESSFUL) throw "ERRORR in beInitStream.";
}

CMP3Simple::~CMP3Simple() {

	beCloseStream(this->hbeStream);
}

void CMP3Simple::LoadLIBS() {
	HINSTANCE  hDLLlame = NULL;

	m_qMutex.Lock();
	if (!m_isLibLoaded) {
		// LAME API wasn't loaded yet, so load it

		hDLLlame = ::LoadLibrary("lame_enc.dll");

		if( hDLLlame == NULL ) {
			m_qMutex.Unlock();
			throw "Error loading lame_enc.DLL";
		}

		beInitStream	= (BEINITSTREAM) GetProcAddress(hDLLlame, TEXT_BEINITSTREAM);
		beEncodeChunk	= (BEENCODECHUNK) GetProcAddress(hDLLlame, TEXT_BEENCODECHUNK);
		beDeinitStream	= (BEDEINITSTREAM) GetProcAddress(hDLLlame, TEXT_BEDEINITSTREAM);
		beCloseStream	= (BECLOSESTREAM) GetProcAddress(hDLLlame, TEXT_BECLOSESTREAM);
		beVersion      	= (BEVERSION) GetProcAddress(hDLLlame, TEXT_BEVERSION);
		beWriteVBRHeader= (BEWRITEVBRHEADER) GetProcAddress(hDLLlame, TEXT_BEWRITEVBRHEADER);
		beWriteInfoTag  = (BEWRITEINFOTAG) GetProcAddress(hDLLlame, TEXT_BEWRITEINFOTAG);

		if(!beInitStream || !beEncodeChunk || !beDeinitStream ||
			!beCloseStream || !beVersion || !beWriteVBRHeader) {

			::FreeLibrary(hDLLlame);
			m_qMutex.Unlock();
			throw "Unable to get LAME interfaces";
		}

		m_isLibLoaded = true;
	}

	m_qMutex.Unlock();
}

#endif