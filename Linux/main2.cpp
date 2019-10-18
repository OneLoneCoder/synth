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

main2.cpp 
This version expands on oscillators to include other waveforms 
and introduces envelopes
See Video: https://youtu.be/OSCzKOqtgcA

main1.cpp 
This is the first version of the software. It presents a simple 
keyboard and a sine wave oscillator.
See video: https://youtu.be/tgamhuQnOkM

*/

#include <iostream>
#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "SDLConsole.h"
#include "SDLAudioManager.h"

using namespace std;

// Converts frequency (Hz) to angular velocity
double w(double dHertz)
{
	return dHertz * 2.0 * PI;
}

// General purpose oscillator
#define OSC_SINE 0
#define OSC_SQUARE 1
#define OSC_TRIANGLE 2
#define OSC_SAW_ANA 3
#define OSC_SAW_DIG 4
#define OSC_NOISE 5

double osc(double dHertz, double dTime, int nType = OSC_SINE)
{
	switch (nType)
	{
	case OSC_SINE: // Sine wave bewteen -1 and +1
		return sin(w(dHertz) * dTime);

	case OSC_SQUARE: // Square wave between -1 and +1
		return sin(w(dHertz) * dTime) > 0 ? 1.0 : -1.0;

	case OSC_TRIANGLE: // Triangle wave between -1 and +1
		return asin(sin(w(dHertz) * dTime)) * (2.0 / PI);

	case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
	{
		double dOutput = 0.0;

		for (double n = 1.0; n < 40.0; n++)
			dOutput += (sin(n * w(dHertz) * dTime)) / n;

		return dOutput * (2.0 / PI);
	}

	case OSC_SAW_DIG: // Saw Wave (optimised / harsh / fast)
		return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));


	case OSC_NOISE: // Pseudorandom noise
		return 2.0 * ((double)rand() / (double)RAND_MAX) - 1.0;

	default:
		return 0.0;
	}
}

// Amplitude (Attack, Decay, Sustain, Release) Envelope
struct sEnvelopeADSR
{
	double dAttackTime;
	double dDecayTime;
	double dSustainAmplitude;
	double dReleaseTime;
	double dStartAmplitude;
	double dTriggerOffTime;
	double dTriggerOnTime;
	bool bNoteOn;

	sEnvelopeADSR()
	{
		dAttackTime = 0.10;
		dDecayTime = 0.01;
		dStartAmplitude = 1.0;
		dSustainAmplitude = 0.8;
		dReleaseTime = 0.20;
		bNoteOn = false;
		dTriggerOffTime = 0.0;
		dTriggerOnTime = 0.0;
	}

	// Call when key is pressed
	void NoteOn(double dTimeOn)
	{
		dTriggerOnTime = dTimeOn;
		bNoteOn = true;
	}

	// Call when key is released
	void NoteOff(double dTimeOff)
	{
		dTriggerOffTime = dTimeOff;
		bNoteOn = false;
	}

	// Get the correct amplitude at the requested point in time
	double GetAmplitude(double dTime)
	{
		double dAmplitude = 0.0;
		double dLifeTime = dTime - dTriggerOnTime;

		if (bNoteOn)
		{
			if (dLifeTime <= dAttackTime)
			{
				// In attack Phase - approach max amplitude
				dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;
			}

			if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
			{
				// In decay phase - reduce to sustained amplitude
				dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;
			}

			if (dLifeTime > (dAttackTime + dDecayTime))
			{
				// In sustain phase - dont change until note released
				dAmplitude = dSustainAmplitude;
			}
		}
		else
		{
			// Note has been released, so in release phase
			dAmplitude = ((dTime - dTriggerOffTime) / dReleaseTime) * (0.0 - dSustainAmplitude) + dSustainAmplitude;
		}

		// Amplitude should not be negative
		if (dAmplitude <= 0.0001)
			dAmplitude = 0.0;

		return dAmplitude;
	}
};




// Global synthesizer variables
atomic<double> dFrequencyOutput = 0.0;			// dominant output frequency of instrument, i.e. the note
sEnvelopeADSR envelope;							// amplitude modulation of output to give texture, i.e. the timbre
double dOctaveBaseFrequency = 110.0; // A2		// frequency of octave represented by keyboard
double d12thRootOf2 = pow(2.0, 1.0 / 12.0);		// assuming western 12 notes per ocatve

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
double MakeNoise(double dTime)
{	
	// Mix together a little sine and square waves
	double dOutput = envelope.GetAmplitude(dTime) *
		(
			+ 1.0 * osc(dFrequencyOutput * 0.5, dTime, OSC_SINE)
			+ 1.0 * osc(dFrequencyOutput, dTime, OSC_SAW_ANA)
		);
		
	return dOutput * 0.4; // Master Volume
}

int main()
{
	SDLAudioManager audio_manager;
	audio_manager.SetUserFunction(MakeNoise);
	SDLConsole console;

	// Shameless self-promotion
	console << "www.OneLoneCoder.com - Synthesizer Part 2" << SDLConsole::endl << "Multiple Oscillators with Single Amplitude Envelope, No Polyphony" << SDLConsole::endl << SDLConsole::endl;
	console << "Using the SDL2 Library" << SDLConsole::endl << SDLConsole::endl;

	// Display a keyboard
	console << SDLConsole::endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << SDLConsole::endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << SDLConsole::endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << SDLConsole::endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << SDLConsole::endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << SDLConsole::endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << SDLConsole::endl << SDLConsole::endl;
	console << "Key not pressed.";

	// Sit in loop, capturing keyboard state changes and modify
	// synthesizer output accordingly
	int nCurrentKey = -1;
	int keys[] = {SDLK_z, SDLK_s, SDLK_x, SDLK_c, SDLK_f, SDLK_v, SDLK_g, SDLK_b, SDLK_n, SDLK_j, SDLK_m, SDLK_k, SDLK_COMMA, SDLK_l, SDLK_PERIOD, SDLK_SLASH};
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		if (SDL_WaitEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				done = true;
				break;

			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
					done = true;

				for (int k = 0; k < 16; k++)
				{
					if (event.key.keysym.sym == keys[k])
					{
						if (nCurrentKey != k)
						{
							dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
							envelope.NoteOn(audio_manager.GetTime());
							console.m_ConsoleText.back() = "Note On : " + to_string(audio_manager.GetTime()) + "s " + to_string(dFrequencyOutput) + "Hz";
							nCurrentKey = k;
						}
					}
				}
				break;

			case SDL_KEYUP:
				if (nCurrentKey != -1)
				{
					console.m_ConsoleText.back() = "Note Off: " + to_string(audio_manager.GetTime()) + "s";
					envelope.NoteOff(audio_manager.GetTime());
					nCurrentKey = -1;
				}
				break;
			}
		}
		console.Render();
	}

	SDL_Quit();
	return 0;
}
