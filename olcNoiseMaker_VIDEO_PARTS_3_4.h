/*
	OneLoneCoder.com - Simple Audio Noisy Thing
	"Allows you to simply listen to that waveform!" - @Javidx9

	License
	~~~~~~~
	Copyright (C) 2018  Javidx9
	This program comes with ABSOLUTELY NO WARRANTY.
	This is free software, and you are welcome to redistribute it
	under certain conditions; See license for details.
	Original works located at:
	https://www.github.com/onelonecoder
	https://www.onelonecoder.com
	https://www.youtube.com/javidx9

	GNU GPLv3
	https://github.com/OneLoneCoder/videos/blob/master/LICENSE

	From Javidx9 :)
	~~~~~~~~~~~~~~~
	Hello! Ultimately I don't care what you use this for. It's intended to be
	educational, and perhaps to the oddly minded - a little bit of fun.
	Please hack this, change it and use it in any way you see fit. You acknowledge
	that I am not responsible for anything bad that happens as a result of
	your actions. However this code is protected by GNU GPLv3, see the license in the
	github repo. This means you must attribute me if you use it. You can view this
	license here: https://github.com/OneLoneCoder/videos/blob/master/LICENSE
	Cheers!

	Author
	~~~~~~

	Twitter: @javidx9
	Blog: www.onelonecoder.com

	Versions
	~~~~~~~~

	1.0 - 14/01/17
	- Controls audio output hardware behind the scenes so you can just focus
	  on creating and listening to interesting waveforms.
	- Currently MS Windows only

	Documentation
	~~~~~~~~~~~~~

	See video: https://youtu.be/tgamhuQnOkM

	This will improve as it grows!

	UPDATED by IndustriousNomad - 13/1/2020 :
	     FIX   - corrected the DWORD TYPE Pointers in the waveOutProcWrap function
	     ADDED - #include <algorithm> for the FIND function to work in GCC
	     ADDED - Pragma lib Detection ( Codeblocks doesn't use this )
	     RESET - Previously, "using namespace std;" was used. But when using
	             GCC it caused some conflict naming errors. So std:: was manually
                     added where it needs to be
             FIX   - Commented out the "nPreviousSample" Variable since it wasn't being used

*/


#pragma once

#define UNICODE

#ifdef _MSC_VER
#pragma comment(lib, "winmm.lib")
#endif

#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <algorithm>

#include <Windows.h>

#ifndef FTYPE
#define FTYPE double
#endif

const double PI = 2.0 * acos(0.0);

template<class T>
class olcNoiseMaker
{
public:
	olcNoiseMaker(std::wstring sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512)
	{
		Create(sOutputDevice, nSampleRate, nChannels, nBlocks, nBlockSamples);
	}

	~olcNoiseMaker()
	{
		Destroy();
	}

	bool Create(std::wstring sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 8, unsigned int nBlockSamples = 512)
	{
		m_bReady = false;
		m_nSampleRate = nSampleRate;
		m_nChannels = nChannels;
		m_nBlockCount = nBlocks;
		m_nBlockSamples = nBlockSamples;
		m_nBlockFree = m_nBlockCount;
		m_nBlockCurrent = 0;
		m_pBlockMemory = nullptr;
		m_pWaveHeaders = nullptr;

		m_userFunction = nullptr;

		// Validate device
		std::vector<std::wstring> devices = Enumerate();
		auto d = std::find(devices.begin(), devices.end(), sOutputDevice);
		if (d != devices.end())
		{
			// Device is available
			int nDeviceID = distance(devices.begin(), d);
			WAVEFORMATEX waveFormat;
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nSamplesPerSec = m_nSampleRate;
			waveFormat.wBitsPerSample = sizeof(T) * 8;
			waveFormat.nChannels = m_nChannels;
			waveFormat.nBlockAlign = (waveFormat.wBitsPerSample / 8) * waveFormat.nChannels;
			waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
			waveFormat.cbSize = 0;

			// Open Device if valid
			if (waveOutOpen(&m_hwDevice, nDeviceID, &waveFormat, (DWORD_PTR)waveOutProcWrap, (DWORD_PTR)this, CALLBACK_FUNCTION) != S_OK)
				return Destroy();
		}

		// Allocate Wave|Block Memory
		m_pBlockMemory = new T[m_nBlockCount * m_nBlockSamples];
		if (m_pBlockMemory == nullptr)
			return Destroy();
		ZeroMemory(m_pBlockMemory, sizeof(T) * m_nBlockCount * m_nBlockSamples);

		m_pWaveHeaders = new WAVEHDR[m_nBlockCount];
		if (m_pWaveHeaders == nullptr)
			return Destroy();
		ZeroMemory(m_pWaveHeaders, sizeof(WAVEHDR) * m_nBlockCount);

		// Link headers to block memory
		for (unsigned int n = 0; n < m_nBlockCount; n++)
		{
			m_pWaveHeaders[n].dwBufferLength = m_nBlockSamples * sizeof(T);
			m_pWaveHeaders[n].lpData = (LPSTR)(m_pBlockMemory + (n * m_nBlockSamples));
		}

		m_bReady = true;

		m_thread = std::thread(&olcNoiseMaker::MainThread, this);

		// Start the ball rolling
		std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
		m_cvBlockNotZero.notify_one();

		return true;
	}

	bool Destroy()
	{
		return false;
	}

	void Stop()
	{
		m_bReady = false;
		m_thread.join();
	}

	// Override to process current sample
	virtual FTYPE UserProcess(int nChannel, FTYPE dTime)
	{
		return 0.0;
	}

	FTYPE GetTime()
	{
		return m_dGlobalTime;
	}



public:
	static std::vector<std::wstring> Enumerate()
	{
		int nDeviceCount = waveOutGetNumDevs();
		std::vector<std::wstring> sDevices;
		WAVEOUTCAPS woc;
		for (int n = 0; n < nDeviceCount; n++)
			if (waveOutGetDevCaps(n, &woc, sizeof(WAVEOUTCAPS)) == S_OK)
				sDevices.push_back(woc.szPname);
		return sDevices;
	}

	void SetUserFunction(FTYPE(*func)(int, FTYPE))
	{
		m_userFunction = func;
	}

	FTYPE clip(FTYPE dSample, FTYPE dMax)
	{
		if (dSample >= 0.0)
			return fmin(dSample, dMax);
		else
			return fmax(dSample, -dMax);
	}


private:
	FTYPE(*m_userFunction)(int, FTYPE);

	unsigned int m_nSampleRate;
	unsigned int m_nChannels;
	unsigned int m_nBlockCount;
	unsigned int m_nBlockSamples;
	unsigned int m_nBlockCurrent;

	T* m_pBlockMemory;
	WAVEHDR *m_pWaveHeaders;
	HWAVEOUT m_hwDevice;

	std::thread m_thread;
	std::atomic<bool> m_bReady;
	std::atomic<unsigned int> m_nBlockFree;
	std::condition_variable m_cvBlockNotZero;
	std::mutex m_muxBlockNotZero;

	std::atomic<FTYPE> m_dGlobalTime;

	// Handler for soundcard request for more data
	void waveOutProc(HWAVEOUT hWaveOut, UINT uMsg, DWORD dwParam1, DWORD dwParam2)
	{
		if (uMsg != WOM_DONE) return;

		m_nBlockFree++;
		std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
		m_cvBlockNotZero.notify_one();
	}

	// Static wrapper for sound card handler
	static void CALLBACK waveOutProcWrap(HWAVEOUT hWaveOut, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2)
	{
		((olcNoiseMaker*)dwInstance)->waveOutProc(hWaveOut, uMsg, dwParam1, dwParam2);
	}

	// Main thread. This loop responds to requests from the soundcard to fill 'blocks'
	// with audio data. If no requests are available it goes dormant until the sound
	// card is ready for more data. The block is fille by the "user" in some manner
	// and then issued to the soundcard.
	void MainThread()
	{
		m_dGlobalTime = 0.0;
		FTYPE dTimeStep = 1.0 / (FTYPE)m_nSampleRate;

		// Goofy hack to get maximum integer for a type at run-time
		T nMaxSample = (T)pow(2, (sizeof(T) * 8) - 1) - 1;
		FTYPE dMaxSample = (FTYPE)nMaxSample;
//		T nPreviousSample = 0;

		while (m_bReady)
		{
			// Wait for block to become available
			if (m_nBlockFree == 0)
			{
				std::unique_lock<std::mutex> lm(m_muxBlockNotZero);
				while(m_nBlockFree == 0) // sometimes, Windows signals incorrectly
					m_cvBlockNotZero.wait(lm);
			}

			// Block is here, so use it
			m_nBlockFree--;

			// Prepare block for processing
			if (m_pWaveHeaders[m_nBlockCurrent].dwFlags & WHDR_PREPARED)
				waveOutUnprepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));

			T nNewSample = 0;
			int nCurrentBlock = m_nBlockCurrent * m_nBlockSamples;

			for (unsigned int n = 0; n < m_nBlockSamples; n+=m_nChannels)
			{
				// User Process
				for (unsigned int c = 0; c < m_nChannels; c++)
				{
					if (m_userFunction == nullptr)
						nNewSample = (T)(clip(UserProcess(c, m_dGlobalTime), 1.0) * dMaxSample);
					else
						nNewSample = (T)(clip(m_userFunction(c, m_dGlobalTime), 1.0) * dMaxSample);

					m_pBlockMemory[nCurrentBlock + n + c] = nNewSample;
//					nPreviousSample = nNewSample;
				}

				m_dGlobalTime = m_dGlobalTime + dTimeStep;
			}

			// Send block to sound device
			waveOutPrepareHeader(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
			waveOutWrite(m_hwDevice, &m_pWaveHeaders[m_nBlockCurrent], sizeof(WAVEHDR));
			m_nBlockCurrent++;
			m_nBlockCurrent %= m_nBlockCount;
		}
	}
};
