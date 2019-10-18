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

	This is the first version of the software. It presents a simple keyboard and a sine
	wave oscillator.

	See video: https://youtu.be/tgamhuQnOkM

*/

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

// Global synthesizer variables
atomic<double> dFrequencyOutput = 0.0;			// dominant output frequency of instrument, i.e. the note
double dOctaveBaseFrequency = 110.0; // A2		// frequency of octave represented by keyboard
double d12thRootOf2 = pow(2.0, 1.0 / 12.0);		// assuming western 12 notes per ocatve

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
double MakeNoise(double dTime)
{
	double dOutput = sin(dFrequencyOutput * 2.0 * 3.14159 * dTime);
	return dOutput * 0.5; // Master Volume
}

int main()
{
	SDLAudioManager audio_manager;
	audio_manager.SetUserFunction(MakeNoise);
	SDLConsole console;

	// Shameless self-promotion
	console << "www.OneLoneCoder.com - Synthesizer Part 1" << SDLConsole::endl;
	console << "Single Sine Wave Oscillator, No Polyphony" << SDLConsole::endl << SDLConsole::endl;
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
					dFrequencyOutput = 0.0;
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
