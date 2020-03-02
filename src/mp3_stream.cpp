// mp3_stream.cpp : Defines the entry point for the console application.
//

#pragma setlocale(".1251")

#include "stdafx.h"
#include "INCLUDE/mp3_simple.h"
#include "INCLUDE/waveIN_simple.h"
#include <conio.h>

// An example of the IReceiver implementation.
class mp3Writer: public IReceiver {
private:
	CMP3Simple	m_mp3Enc;
	FILE *f;

public:
	mp3Writer(unsigned int bitrate = 128, unsigned int finalSimpleRate = 0): 
			m_mp3Enc(bitrate, 44100, finalSimpleRate) {
		f = fopen("music.mp3", "wb");
		if (f == NULL) throw "Can't create MP3 file.";
	};

	~mp3Writer() {
		fclose(f);
	};

	virtual void ReceiveBuffer(LPSTR lpData, DWORD dwBytesRecorded) {
		BYTE	mp3Out[44100 * 4];
		DWORD	dwOut;
		m_mp3Enc.Encode((PSHORT) lpData, dwBytesRecorded/2, mp3Out, &dwOut);

		fwrite(mp3Out, dwOut, 1, f);
	};
};

// Prints the application's help.
void printHelp(char *progname) {
	printf("%s -devices\n\tWill list WaveIN devices.\n\n", progname);
	printf("%s -device=<device_name>\n\tWill list recording lines of the WaveIN <device_name> device.\n\n", progname);
	printf("%s -device=<device_name> -line=<line_name> [-v=<volume>] [-br=<bitrate>] [-sr=<samplerate>]\n", progname);
	printf("\tWill record from the <line_name> at the given voice <volume>, output <bitrate> (in Kbps)\n");
	printf("\tand output <samplerate> (in Hz).\n\n");
	printf("\t<volume>, <bitrate> and <samplerate> are optional parameters.\n");
	printf("\t<volume> - integer value between (0..100), defaults to 0 if not set.\n");
	printf("\t<bitrate> - integer value (16, 24, 32, .., 64, etc.), defaults to 128 if not set.\n");
	printf("\t<samplerate> - integer value (44100, 32000, 22050, etc.), defaults to 44100 if not set.\n");

}

// Lists WaveIN devices present in the system.
void printWaveINDevices() {
	const vector<CWaveINSimple*>& wInDevices = CWaveINSimple::GetDevices();
	UINT i;

	for (i = 0; i < wInDevices.size(); i++) {
		printf("%s\n", wInDevices[i]->GetName());
	}
}

// Prints WaveIN device's lines.
void printLines(CWaveINSimple& WaveInDevice) {
	CHAR szName[MIXER_LONG_NAME_CHARS];
	UINT j;

	try {
		CMixer& mixer = WaveInDevice.OpenMixer();
		const vector<CMixerLine*>& mLines = mixer.GetLines();

		for (j = 0; j < mLines.size(); j++) {
			::CharToOem(mLines[j]->GetName(), szName);
			printf("%s\n", szName);
		}

		mixer.Close();
	}
	catch (const char *err) {
		printf("%s\n",err);
	}
}

int main(int argc, char* argv[])
{
	mp3Writer *mp3Wr;

	char *strDeviceName = NULL;
	char *strLineName = NULL;
	char *strTemp = NULL;

	UINT nVolume = 0;
	UINT nBitRate = 128;
	UINT nFSimpleRate = 0;


	setlocale( LC_ALL, ".866");
	try {

		if (argc < 2) printHelp(argv[0]);
		else if (argc == 2) {
			if (::strcmp(argv[1],"-devices") == 0) printWaveINDevices();
			else if ((strTemp = ::strstr(argv[1],"-device=")) == argv[1]) {
				strDeviceName = &strTemp[8];
				CWaveINSimple& device = CWaveINSimple::GetDevice(strDeviceName);
				printLines(device);
			}
			else printHelp(argv[0]);
		}
		else {
			if ((strTemp = ::strstr(argv[1],"-device=")) == argv[1]) {
				strDeviceName = &strTemp[8];
			}

			if ((strTemp = ::strstr(argv[2],"-line=")) == argv[2]) {
				strLineName = &strTemp[6];
			}

			if ((strDeviceName == NULL) || (strLineName == NULL)) {
				printHelp(argv[0]);
				return 0;
			}

			for (int i = 3; i < argc; i ++) {
				if ((strTemp = ::strstr(argv[i],"-v=")) == argv[i]) {
					strTemp = &strTemp[3];
					nVolume = (UINT) atoi(strTemp);
				}
				else if ((strTemp = ::strstr(argv[i],"-br=")) == argv[i]) {
					strTemp = &strTemp[4];
					nBitRate = (UINT) atoi(strTemp);
				}
				else if ((strTemp = ::strstr(argv[i],"-sr=")) == argv[i]) {
					strTemp = &strTemp[4];
					nFSimpleRate = (UINT) atoi(strTemp);
				}
				else {
					printHelp(argv[0]);
					return 0;
				}
			}

			printf("\nRecording at %dKbps, ", nBitRate);
			if (nFSimpleRate == 0) printf("44100Hz\n");
			else printf("%dHz\n", nFSimpleRate);
			printf("from %s (%s).\n", strLineName, strDeviceName);
			printf("Volume %d%%.\n\n", nVolume);
			
			CWaveINSimple& device = CWaveINSimple::GetDevice(strDeviceName);
			CMixer& mixer = device.OpenMixer();
			CMixerLine& mixerline = mixer.GetLine(strLineName);

			mixerline.UnMute();
			mixerline.SetVolume(nVolume);
			mixerline.Select();
			mixer.Close();

			mp3Wr = new mp3Writer(nBitRate, nFSimpleRate);
			device.Start((IReceiver *) mp3Wr);
			printf("hit <ENTER> to stop ...\n");
			while( !_kbhit() ) ::Sleep(100);
		
			device.Stop();
			delete mp3Wr;
		}
	}
	catch (const char *err) {
		printf("%s\n",err);
	}

	CWaveINSimple::CleanUp();
	return 0;
}

