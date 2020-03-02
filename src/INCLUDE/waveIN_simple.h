#ifndef ___WAVEIN_SIMPLE_H_INCLUDED___
#define ___WAVEIN_SIMPLE_H_INCLUDED___

#include <windows.h>
#include <mmsystem.h>
#include <vector>

using namespace std;

#define WAIT_SIG 0
#define CONTINUE_SIG 1
#define EXIT_SIG 2

//---------------------------- CLASS -------------------------------------------------------------

// See CWaveINSimple::Start(IReceiver *pReceiver) below.
// Instances of any class extending "IReceiver" will be able to receive raw (PCM)
// sound from an instance of the CWaveINSimple and process sound via own
// implementation of the "ReceiveBuffer" method.

class IReceiver {
public:
	virtual void ReceiveBuffer(LPSTR lpData, DWORD dwBytesRecorded) = 0;
};

class CWaveINSimple;
class CMixer;
///////////////////////////////////////////////////////////////////////////
// Implementation of the Mixer's Line
class CMixerLine {
	friend class CMixer;
private:
	MIXERLINE m_MxLine;
	HMIXER m_MixerHandle;

	// Yep, constructor and destructor are declared private, since only
	// instances of CMixer can create CMixerLine objects (one CMixer can
	// have zero or more CMixerLine's, see CMixer::GetLines()).
	//
	// Constructor receives parameters exactly as they are passed by
	// the CMixer::InitLines(), which is called from CMixer::Open().
	// In fact, MixerHandle and pMxLine (pointer to MIXERLINE which
	// is filled in CMixer::InitLines()) is all we need to know about
	// Mixer's Line.
	CMixerLine(HMIXER MixerHandle, MIXERLINE *pMxLine);
	~CMixerLine() {};

public:

	// Raturns Line's name.
	const TCHAR *GetName() const { return this->m_MxLine.szName; };

	// Un-mute Mixer's Line. I would say this is a useless method, since
	// Mixer Lines for the WaveIN devices doesn't support Mute/Un-mute, at
	// least for the sounds cards I worked with. However, according to
	// http://support.microsoft.com/default.aspx?scid=kb%3Ben-us%3B159753,
	// there may be such sound cards. Anyway, method doesn't throw any
	// exceptions or return any errors if Mixer's Line doesn't support un-muting.
	void UnMute();

	// Set the Mixer's Line volume, nVolumeLevel should be within 0..100,
	// think of it like a percentage.
	void SetVolume(UINT nVolumeLevel);

	// Select the Mixer's Line. Regarding recording Lines, Method selects
	// Mixer's Line as a recording source (same as we would do this
	// manually via "sndvol32 /r").
	// Code for this method is almost an exact copy from the
	// http://support.microsoft.com/default.aspx?scid=kb%3Ben-us%3B159753.
	// I couldn't find a better one.
	void Select();
};
///////////////////////////////////////////////////////////////////////////
// Implementation of the Mixer. Via Mixer we have access to Mixer's Lines.
// Another particularity is that each Wave device (WaveIN in this case) has
// one Mixer.
class CMixer {
	friend class CWaveINSimple;

private:
	UINT m_nWaveDeviceID;

	// Handle to Mixer for WAVE Device
	HMIXER m_MixerHandle;
	QMutex m_qLocalMutex;

	// Here where Mixer keeps its Lines.
	vector<CMixerLine*> m_arrMixerLines;

	// Naturally, Mixer is a property of the Wave device (WaveIN in this case).
	// So, we can't just create (and destroy) a CMixer object from nothing (well,
	// we can but this design says so). See CWaveINSimple::OpenMixer() for more details.
	CMixer(UINT nWaveDeviceID);
	~CMixer();

	// This method initialize Mixer's Lines collection. It is
	// called within CMixer::Open().
	void InitLines();

public:
	// Opens the MIXER and call CMixer::InitLines().
	void Open();

	// Method closes the MIXER and clears the Lines collection.
	void Close();

	// Returns MIXER's WAVEIN lines,
	// call Open() before requesting lines (except cases when
	// Mixer is returned by CWaveINSimple::OpenMixer()).
	const vector<CMixerLine*>& GetLines() { return this->m_arrMixerLines; };

	// Returns a MIXER's WAVEIN line by line's name,
	// call Open() before requesting the line (except cases when
	// Mixer is returned by CWaveINSimple::OpenMixer()).
	CMixerLine& GetLine(const TCHAR *pLineName);
};
///////////////////////////////////////////////////////////////////////////
// Via implementation of the CWaveINSimple we get access to the WaveIN
// devices, or, better saying, to the Wave input devices (capable of
// recording sound).
class CWaveINSimple {
private:
	// Static collection where all WaveIN devices, present in the system,
	// are saved. See CWaveINSimple::GetDevices()
	static vector<CWaveINSimple*> m_arrWaveINDevices;
	static QMutex m_qGlobalMutex;
	static volatile bool m_isDeviceListLoaded;

	// This is thread's routine procedure to which the Windows Low Level
	// WAVE API passes messages regarding digital audio recording (such
	// as MM_WIM_DATA, MM_WIM_OPEN, and MM_WIM_CLOSE). Mind that "waveInOpen"
	// (in CWaveINSimple::_Start()) is called with CALLBACK_THREAD.
	// Thread is created inside the CWaveINSimple::_Start() call.
	static DWORD WINAPI waveInProc(LPVOID arg);

	// WaveIN Device's ID. It is used as input parameter for the CMixer
	// constructor. With this ID we can access Mixer without actually opening
	// the WaveIN device.
	UINT m_nWaveDeviceID;

	// Structure to keep basic details about WaveIN device.
	WAVEINCAPS m_wic;

	// Handle to the WaveIN Device.
	HWAVEIN	m_WaveInHandle;

	// Structure to keep sound's quality settings. Also used when opening WaveIN
	// device, see CWaveINSimple::_Start().
	WAVEFORMATEX m_waveFormat;

	// Two WAVEHDR's are used for recording (ie, double-buffering).
	WAVEHDR	m_WaveHeader[2];

	// Pointer to a IReceiver object, passed via 
	// CWaveINSimple::Start(IReceiver *pReceiver), that will be responsible for 
	// further processing of the sound data.
	IReceiver *m_Receiver;
	CMixer m_Mixer;
	QMutex m_qLocalMutex;

	// These class' attributes are used for communication with thread's routine.
	volatile int m_SIG;
	volatile unsigned char m_BuffersDone;

	// Constructor and destructor are declared private (due design). So, there 
	// is no way to instantiate CWaveINSimple objects directly. To obtain a 
	// CWaveINSimple object, use CWaveINSimple::GetDevices() or CWaveINSimple::GetDevice() 
	// (see below). CWaveINSimple objects are destroyed via CWaveINSimple::CleanUp().
	CWaveINSimple(UINT nWaveDeviceID, WAVEINCAPS *pWIC);
	~CWaveINSimple();

	void Close(int iLevel);

	// This method starts recording sound from the WaveIN device. Passed object (derivate from 
	// IReceiver) will be responsible for further processing of the sound data.
	void _Start(IReceiver *pReceiver);

	// This method stops recording.
	void _Stop();

public:
	// This static method returns a collection of the WaveIN devices (capable of recording), 
	// present in the system.
	static const vector<CWaveINSimple*>& GetDevices();

	// This static method returns an instance of the WaveIN device (capable of recording), 
	// by device's name.
	static CWaveINSimple& GetDevice(const TCHAR *pDeviceName);

	// This static method performs general clean-up, once everything is finished and 
	// no more recording is needed.
	static void CleanUp();

	// Wrapper of the _Start() method, for the multithreading version.
	// This is the actual starter.
	void Start(IReceiver *pReceiver);

	// Wrapper of the _Stop() method, for the multithreading version
	// This is the actual stopper.
	void Stop();

	// Returns name of the Device
	const TCHAR *GetName() const { return this->m_wic.szPname; };

	// This method returns and opens Mixer associated with the Device.
	CMixer& OpenMixer();
};

//---------------------------- IMPLEMENTATION ----------------------------------------------------

vector<CWaveINSimple*> CWaveINSimple::m_arrWaveINDevices;
QMutex CWaveINSimple::m_qGlobalMutex;
volatile bool CWaveINSimple::m_isDeviceListLoaded = false;

///////////////////////////////////////////////////////////////////////////
CMixerLine::CMixerLine(HMIXER MixerHandle, MIXERLINE *pMxLine) {
	this->m_MixerHandle = MixerHandle;
	memcpy(&this->m_MxLine, pMxLine, sizeof(MIXERLINE));
}

void CMixerLine::Select() {
	DWORD i;
	MIXERLINE mxl;
	BOOL bOneItemOnly = FALSE;

	// Get the line info for the WaveIN destination line
	mxl.cbStruct = sizeof(MIXERLINE);
	mxl.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;
	mixerGetLineInfo((HMIXEROBJ) this->m_MixerHandle, &mxl, MIXER_GETLINEINFOF_COMPONENTTYPE);

	// Find a LIST control, if any, for the WaveIN line
	LPMIXERCONTROL pmxctrl = (LPMIXERCONTROL) malloc(mxl.cControls * sizeof(MIXERCONTROL));
	if (pmxctrl != NULL) {
		MIXERLINECONTROLS mxlctrl = {sizeof(mxlctrl), mxl.dwLineID, 0, mxl.cControls,
			sizeof(MIXERCONTROL), pmxctrl};
		mixerGetLineControls((HMIXEROBJ) this->m_MixerHandle, &mxlctrl, MIXER_GETLINECONTROLSF_ALL);

		// Now walk through each control to find a type of LIST control. This
		// can be either Mux, Single-select, Mixer or Multiple-select.
		for(i=0; i < mxl.cControls; i++) {
			if (MIXERCONTROL_CT_CLASS_LIST == (pmxctrl[i].dwControlType & MIXERCONTROL_CT_CLASS_MASK)) {
				break;
			}
		}

		if (i < mxl.cControls) {
			// Found a LIST control
			// Check if the LIST control is a Mux or Single-select type
			switch (pmxctrl[i].dwControlType) {
				case MIXERCONTROL_CONTROLTYPE_MUX:
				case MIXERCONTROL_CONTROLTYPE_SINGLESELECT:
					bOneItemOnly = TRUE;
			}

			DWORD cChannels = mxl.cChannels, cMultipleItems = 0;
			if (MIXERCONTROL_CONTROLF_UNIFORM & pmxctrl[i].fdwControl) cChannels = 1;
			if (MIXERCONTROL_CONTROLF_MULTIPLE & pmxctrl[i].fdwControl) {
				cMultipleItems = pmxctrl[i].cMultipleItems;
			}

			// Get the text description of each item
			LPMIXERCONTROLDETAILS_LISTTEXT plisttext = (LPMIXERCONTROLDETAILS_LISTTEXT)
				malloc(cChannels * cMultipleItems * sizeof(MIXERCONTROLDETAILS_LISTTEXT));

			if (plisttext != NULL) {
				MIXERCONTROLDETAILS mxcd = {sizeof(mxcd), pmxctrl[i].dwControlID,
				cChannels, (HWND)cMultipleItems, sizeof(MIXERCONTROLDETAILS_LISTTEXT), (LPVOID) plisttext};
				mixerGetControlDetails((HMIXEROBJ) this->m_MixerHandle, &mxcd, MIXER_GETCONTROLDETAILSF_LISTTEXT);

				// Now get the value for each item
				LPMIXERCONTROLDETAILS_BOOLEAN plistbool = (LPMIXERCONTROLDETAILS_BOOLEAN)
					malloc(cChannels * cMultipleItems * sizeof(MIXERCONTROLDETAILS_BOOLEAN));

				if (plistbool != NULL) {

					mxcd.cbDetails = sizeof MIXERCONTROLDETAILS_BOOLEAN;
					mxcd.paDetails = plistbool;
					mixerGetControlDetails((HMIXEROBJ) this->m_MixerHandle, &mxcd, MIXER_GETCONTROLDETAILSF_VALUE);

					// Select the class' line item
					for (DWORD j=0; j<cMultipleItems; j = j + cChannels) {
						if (0 == lstrcmp(plisttext[j].szName, this->GetName())) {
							// Select it for both left and right channels
							plistbool[j].fValue = plistbool[j+ cChannels - 1].fValue = 1;
						}
						else if (bOneItemOnly) {
							// Mux or Single-select allows only one item to be selected
							// so clear other items as necessary
							plistbool[j].fValue = plistbool[j+ cChannels - 1].fValue = 0;
						}
					}
					// Now actually set the new values in
					mixerSetControlDetails((HMIXEROBJ) this->m_MixerHandle, &mxcd, MIXER_SETCONTROLDETAILSF_VALUE);

					free(plistbool);
				}

				free(plisttext);
			}
		}

		free(pmxctrl);
	}
}

void CMixerLine::SetVolume(UINT nVolumeLevel) {
	MIXERCONTROL MxCtrl;
	MIXERLINECONTROLS MxLCtrl;
	MIXERCONTROLDETAILS_UNSIGNED uValue[2];
	MIXERCONTROLDETAILS MxControlDetails;
	UINT nVolume;

	if (nVolumeLevel > 100) nVolume = 100;
	else nVolume = nVolumeLevel;

	// Find volume control, if any, of the line
	MxLCtrl.cbStruct = sizeof(MIXERLINECONTROLS);
	MxLCtrl.dwLineID = this->m_MxLine.dwLineID;
	MxLCtrl.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	MxLCtrl.cControls = 1;
	MxLCtrl.cbmxctrl = sizeof(MIXERCONTROL);
	MxLCtrl.pamxctrl = &MxCtrl;

	if (!mixerGetLineControls((HMIXEROBJ) this->m_MixerHandle, &MxLCtrl, MIXER_GETLINECONTROLSF_ONEBYTYPE)) {
		// Found, so proceed
		MxControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
		MxControlDetails.dwControlID = MxCtrl.dwControlID;
		MxControlDetails.cChannels = this->m_MxLine.cChannels;

		if (MxControlDetails.cChannels > 2) MxControlDetails.cChannels = 2;
		if (MIXERCONTROL_CONTROLF_UNIFORM &  MxCtrl.fdwControl) MxControlDetails.cChannels = 1;

		MxControlDetails.cMultipleItems = 0;
		MxControlDetails.hwndOwner = (HWND) 0;
		MxControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_UNSIGNED);
		MxControlDetails.paDetails = uValue;

		nVolume = MxCtrl.Bounds.dwMinimum + (UINT) (((MxCtrl.Bounds.dwMaximum - MxCtrl.Bounds.dwMinimum) * nVolume)/100);
		// Set volume (for both channels)
		uValue[0].dwValue = uValue[1].dwValue = nVolume;
		mixerSetControlDetails((HMIXEROBJ) this->m_MixerHandle, &MxControlDetails, MIXER_SETCONTROLDETAILSF_VALUE);
	}
}

void CMixerLine::UnMute() {
	MIXERCONTROL MxCtrl;
	MIXERLINECONTROLS MxLCtrl;
	MIXERCONTROLDETAILS_BOOLEAN bValue[2];
	MIXERCONTROLDETAILS MxControlDetails;

	// Find mute control, if any, of the line
	MxLCtrl.cbStruct = sizeof(MIXERLINECONTROLS);
	MxLCtrl.dwLineID = this->m_MxLine.dwLineID;
	MxLCtrl.dwControlType = MIXERCONTROL_CONTROLTYPE_MUTE;
	MxLCtrl.cControls = 1;
	MxLCtrl.cbmxctrl = sizeof(MIXERCONTROL);
	MxLCtrl.pamxctrl = &MxCtrl;

	if (!mixerGetLineControls((HMIXEROBJ) this->m_MixerHandle, &MxLCtrl, MIXER_GETLINECONTROLSF_ONEBYTYPE)) {
		// Found, so proceed
		MxControlDetails.cbStruct = sizeof(MIXERCONTROLDETAILS);
		MxControlDetails.dwControlID = MxCtrl.dwControlID;
		MxControlDetails.cChannels = this->m_MxLine.cChannels;

		if (MxControlDetails.cChannels > 2) MxControlDetails.cChannels = 2;
		if (MIXERCONTROL_CONTROLF_UNIFORM &  MxCtrl.fdwControl) MxControlDetails.cChannels = 1;

		MxControlDetails.cMultipleItems = 0;
		MxControlDetails.hwndOwner = (HWND) 0;
		MxControlDetails.cbDetails = sizeof(MIXERCONTROLDETAILS_BOOLEAN);
		MxControlDetails.paDetails = bValue;

		// Unmute the line (for both channels)
		bValue[0].fValue = bValue[1].fValue = 0;
		mixerSetControlDetails((HMIXEROBJ) this->m_MixerHandle, &MxControlDetails, MIXER_SETCONTROLDETAILSF_VALUE);
	}
}
///////////////////////////////////////////////////////////////////////////
CMixerLine& CMixer::GetLine(const TCHAR *pLineName) {
	vector<CMixerLine*>::iterator itPos = this->m_arrMixerLines.begin();
	CMixerLine* pMixerLine = NULL;

	if (pLineName != NULL) {
		this->m_qLocalMutex.Lock();
		for (; itPos < this->m_arrMixerLines.end(); itPos++) {
			if (::lstrcmp(((CMixerLine*) *itPos)->GetName(), pLineName) == 0) {
				pMixerLine = *itPos;
				break;
			}
		}
		this->m_qLocalMutex.Unlock();
	}

	if (pMixerLine == NULL) throw "Mixer's line not found.";
	return *pMixerLine;
}

void CMixer::InitLines() {
	DWORD dwMixLines, i;
	CMixerLine *pMixerLine;
	MIXERLINE mxLine;
	MMRESULT err;

	mxLine.cbStruct = sizeof(MIXERLINE);
	mxLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_WAVEIN;

	err = mixerGetLineInfo((HMIXEROBJ) this->m_MixerHandle, &mxLine, MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (err) {
		// Device doesn't have a WAVE recording control
		return;
	}

	dwMixLines = mxLine.cConnections;

	for (i = 0; i < dwMixLines; i++) {
		mxLine.cbStruct = sizeof(MIXERLINE);
		mxLine.dwSource = i;

		err = mixerGetLineInfo((HMIXEROBJ) this->m_MixerHandle, &mxLine, MIXER_GETLINEINFOF_SOURCE);
		if (!err) {
			// Not interested in MIDI IN lines
			if (mxLine.dwComponentType != MIXERLINE_COMPONENTTYPE_SRC_SYNTHESIZER) {
				// Create Mixer's Line object and add to the Lines Collection.
				pMixerLine = new CMixerLine(this->m_MixerHandle, &mxLine);
				this->m_arrMixerLines.push_back(pMixerLine);
			}
		}
	}

}

void CMixer::Open() {
	MMRESULT err;

	this->m_qLocalMutex.Lock();

	if (this->m_MixerHandle == NULL) {
		err = mixerOpen(&this->m_MixerHandle, this->m_nWaveDeviceID, 0, 0, MIXER_OBJECTF_WAVEIN);
		if (err) {
			this->m_MixerHandle = NULL;
			this->m_qLocalMutex.Unlock();
			throw "Can't open Mixer Device.";
		}
		this->InitLines();
	}

	this->m_qLocalMutex.Unlock();
}

void CMixer::Close() {
	this->m_qLocalMutex.Lock();

	if (this->m_MixerHandle != NULL) {

		vector<CMixerLine*>::iterator itPos = this->m_arrMixerLines.begin();
		for (; itPos < this->m_arrMixerLines.end(); itPos++) {
			delete *itPos;
		}

		this->m_arrMixerLines.clear();
		mixerClose(this->m_MixerHandle);
		this->m_MixerHandle = NULL;
	}

	this->m_qLocalMutex.Unlock();
}

CMixer::~CMixer() {
	this->Close();
}

CMixer::CMixer(UINT nWaveDeviceID): m_qLocalMutex() {
	this->m_nWaveDeviceID = nWaveDeviceID;
	this->m_MixerHandle = NULL;
}
///////////////////////////////////////////////////////////////////////////
CMixer& CWaveINSimple::OpenMixer() {
	this->m_Mixer.Open();
	return this->m_Mixer;
}

CWaveINSimple& CWaveINSimple::GetDevice(const TCHAR *pDeviceName) {
	CWaveINSimple* wINSimple = NULL;
	UINT i;

	GetDevices();

	if (pDeviceName != NULL) {
		m_qGlobalMutex.Lock();
		for (i = 0; i < m_arrWaveINDevices.size(); i++) {
			if ((::lstrcmp(m_arrWaveINDevices[i]->GetName(), pDeviceName)) == 0) {
				wINSimple = m_arrWaveINDevices[i];
				break;
			}
		}
		m_qGlobalMutex.Unlock();
	}

	if (wINSimple == NULL) throw "Device not found.";
	return *wINSimple;
}

CWaveINSimple::~CWaveINSimple() {
	this->m_qLocalMutex.Lock();
	this->_Stop();
	if (this->m_WaveHeader[0].lpData != NULL) VirtualFree(this->m_WaveHeader[0].lpData, 0, MEM_RELEASE);
	this->m_qLocalMutex.Unlock();
}

void CWaveINSimple::Close(int iLevel) {
	switch(iLevel) {
	case 1:
		waveInUnprepareHeader(this->m_WaveInHandle, &this->m_WaveHeader[1], sizeof(WAVEHDR));
	case 2:
		waveInUnprepareHeader(this->m_WaveInHandle, &this->m_WaveHeader[0], sizeof(WAVEHDR));
	case 3:
		// Close the WaveIN device.
		while ((waveInClose(this->m_WaveInHandle)) != MMSYSERR_NOERROR) ::Sleep(1);
	case 4:
		this->m_WaveInHandle = NULL;
		this->m_Receiver = NULL;
	}
}

// Wrapper for the multithreading version
void CWaveINSimple::Stop() {
	this->m_qLocalMutex.Lock();
	this->_Stop();
	this->m_qLocalMutex.Unlock();
}

void CWaveINSimple::_Stop() {
	if (this->m_WaveInHandle != NULL) {
		// Say to Thread that stop recording is requested.
		this->m_SIG = EXIT_SIG;

		// Stop recording and tell the driver to unqueue/return all of
		// our WAVEHDRs (via MM_WIM_DONE). The driver will return any
		// partially filled buffer that was currently recording.
		waveInReset(this->m_WaveInHandle);

		// Wait for the recording Thread to receive the MM_WIM_DONE for
		// each queued WAVEHDRs.
		while (this->m_BuffersDone < 2) ::Sleep(1);
		this->Close(1);

		this->m_WaveHeader[1].dwFlags = this->m_WaveHeader[0].dwFlags = 0;
	}
}

// Wrapper for the multithreading version
void CWaveINSimple::Start(IReceiver *pReceiver) {
	const char *message = NULL;

	this->m_qLocalMutex.Lock();

	try {
		this->_Start(pReceiver);
	}
	catch (const char *msg) {
		message = msg;
	}

	this->m_qLocalMutex.Unlock();
	if (message != NULL) throw message;
}

void CWaveINSimple::_Start(IReceiver *pReceiver) {
	HANDLE	waveInThread;
	LPTHREAD_START_ROUTINE pStartRoutine = &CWaveINSimple::waveInProc;
	DWORD	dwThreadID;
	MMRESULT	err;

	if (this->m_WaveInHandle == NULL) {
		this->m_Receiver = pReceiver;
		this->m_SIG = WAIT_SIG;

		// Create the Thread that will receive incoming "blocks" of digital audio data
		// (sent from the driver). The main procedure of this thread is
		// CWaveINSimple::waveInProc(). We need to get the threadID and pass that
		// to waveInOpen().
		waveInThread = CreateThread(NULL, 0, pStartRoutine, (PVOID) this, 0, &dwThreadID);
		if (!waveInThread) {
			this->m_Receiver = NULL;
			throw "Can't create WAVE recording thread.";
		}
		CloseHandle(waveInThread);

		// Open the WaveIN Device, specifying Thread's ID as a callback.
		err = waveInOpen(&this->m_WaveInHandle, this->m_nWaveDeviceID, &this->m_waveFormat, dwThreadID, 0, CALLBACK_THREAD);
		if (err) {
			// Open failed, say to Thread to stop.
			this->m_SIG = EXIT_SIG;
			this->Close(4);
			throw "Can't open WaveIN Device.";
		}
		this->m_SIG = CONTINUE_SIG;

		// Allocate, prepare, and queue two buffers that the driver can
		// use to record blocks of audio data. Alloc buffers only one
		// time and use them until object is deleted.
		// Double-buffering is used.
		if (this->m_WaveHeader[0].lpData == NULL) {
			this->m_WaveHeader[0].lpData = (char *)VirtualAlloc(0, this->m_WaveHeader[0].dwBufferLength * 2, MEM_COMMIT, PAGE_READWRITE);
			if (this->m_WaveHeader[0].lpData == NULL) {
				this->Close(3);
				throw "Can't allocate memory for WAVE buffer.";
			}
		}
		this->m_WaveHeader[1].lpData = this->m_WaveHeader[0].lpData + this->m_WaveHeader[0].dwBufferLength;

		err = waveInPrepareHeader(this->m_WaveInHandle, &this->m_WaveHeader[0], sizeof(WAVEHDR));
		if (err) {
			this->Close(3);
			throw "Error preparing WAVEHDR 1.";
		}

		err = waveInPrepareHeader(this->m_WaveInHandle, &this->m_WaveHeader[1], sizeof(WAVEHDR));
		if (err) {
			this->Close(2);
			throw "Error preparing WAVEHDR 2.";
		}

		err = waveInAddBuffer(this->m_WaveInHandle, &this->m_WaveHeader[0], sizeof(WAVEHDR));
		if (err) {
			this->Close(1);
			throw "Error queueing WAVEHDR 1.";
		}

		err = waveInAddBuffer(this->m_WaveInHandle, &this->m_WaveHeader[1], sizeof(WAVEHDR));
		if (err) {
			this->m_BuffersDone = 1;
			this->Stop();
			throw "Error queueing WAVEHDR 2.";
		}

		// Start recording. Thread will now be receiving audio data.
		err = waveInStart(this->m_WaveInHandle);
		if (err) {
			this->Stop();
			throw "Error starting record.";
		}
	}
}

CWaveINSimple::CWaveINSimple(UINT nWaveDeviceID, WAVEINCAPS *pWIC): m_Mixer(nWaveDeviceID), m_qLocalMutex() {
	this->m_nWaveDeviceID = nWaveDeviceID;
	memcpy(&this->m_wic, pWIC, sizeof(WAVEINCAPS));
	this->m_WaveInHandle = NULL;
	this->m_Receiver = NULL;

	//Initialize the WAVEFORMATEX for 16-bit, 44KHz, stereo.
	ZeroMemory(&this->m_waveFormat, sizeof(WAVEFORMATEX));
	this->m_waveFormat.wFormatTag = WAVE_FORMAT_PCM;
	this->m_waveFormat.nChannels = 2;
	this->m_waveFormat.nSamplesPerSec = 44100;
	this->m_waveFormat.wBitsPerSample = 16;
	this->m_waveFormat.nBlockAlign = this->m_waveFormat.nChannels * (this->m_waveFormat.wBitsPerSample/8);
	this->m_waveFormat.nAvgBytesPerSec = this->m_waveFormat.nSamplesPerSec * this->m_waveFormat.nBlockAlign;
	this->m_waveFormat.cbSize = 0;

	//Initialize the sound buffers, but don't allocate memory yet.
	ZeroMemory(this->m_WaveHeader, sizeof(WAVEHDR) * 2);
	this->m_WaveHeader[1].dwBufferLength = this->m_WaveHeader[0].dwBufferLength = this->m_waveFormat.nAvgBytesPerSec << 1;
}

DWORD WINAPI CWaveINSimple::waveInProc(LPVOID arg) {
	MSG		msg;
	CWaveINSimple *_this = (CWaveINSimple *) arg;

	if (_this == NULL) return(0);

	while (_this->m_SIG == WAIT_SIG) ::Sleep(1);
	if (_this->m_SIG == EXIT_SIG) {
		// Call to waveInOpen failed.
		return(0);
	}

	while (GetMessage(&msg, 0, 0, 0) == 1) 	{
		switch (msg.message) {
			// A buffer has been filled by the driver
			case MM_WIM_DATA:
				// msg.lParam contains a pointer to the WAVEHDR structure
				// for the filled buffer.
				if ((((WAVEHDR *)msg.lParam)->dwBytesRecorded) && (_this->m_Receiver)) {

					// Send buffer to the m_Receiver (instance of the IReceiver)
					// for further processing
					_this->m_Receiver->ReceiveBuffer(((WAVEHDR *)msg.lParam)->lpData,
						((WAVEHDR *)msg.lParam)->dwBytesRecorded);
				}

				// Still recording?
				if (_this->m_SIG != EXIT_SIG) {
					// Yes. Then requeue this buffer so the driver can
					// use it for another block of audio data.
					waveInAddBuffer(_this->m_WaveInHandle, (WAVEHDR *)msg.lParam, sizeof(WAVEHDR));
				}
				else {
					// No, so another WAVEHDR has been returned after
					// recording has stopped. When we get all of them back,
					// m_BuffersDone will be equal to how many WAVEHDRs
					// we queued.
					++_this->m_BuffersDone;
				}
				break;

			// Main thread is opening the WAVE device.
			case MM_WIM_OPEN:
				_this->m_BuffersDone = 0;
				break;

			// Main thread is closing the WAVE device.
			case MM_WIM_CLOSE:
				return(0);
				break;
		}
	}
	return(0);
}

const vector<CWaveINSimple*>& CWaveINSimple::GetDevices() {
	UINT nInDev, i;
	WAVEINCAPS wic;
	CWaveINSimple *pWaveIn;

	m_qGlobalMutex.Lock();

	if (!m_isDeviceListLoaded) {
		nInDev = waveInGetNumDevs();

		for (i = 0; i < nInDev; i++) {
			if (!waveInGetDevCaps(i, &wic, sizeof(WAVEINCAPS))) {

				// We are only interested in devices supporting
				// 44.1 kHz, stereo, 16-bit, stereo input
				if (wic.dwFormats & WAVE_FORMAT_4S16) {
					pWaveIn = new CWaveINSimple(i, &wic);
					m_arrWaveINDevices.push_back(pWaveIn);
				}
			}
		}

		m_isDeviceListLoaded = true;
	}

	m_qGlobalMutex.Unlock();

	return m_arrWaveINDevices;
}

void CWaveINSimple::CleanUp() {
	m_qGlobalMutex.Lock();

	vector<CWaveINSimple*>::iterator itPos = m_arrWaveINDevices.begin();
	for (; itPos < m_arrWaveINDevices.end(); itPos++) {
		delete *itPos;
	}

	m_arrWaveINDevices.clear();
	m_isDeviceListLoaded = false;

	m_qGlobalMutex.Unlock();
}

#endif