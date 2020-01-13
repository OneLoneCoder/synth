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

	UPDATED by IndustriousNomad :
	     FIX   - using atomic for "double dFrequencyOutput = 0.0;" is not needed
	             GCC wouldn't compile with it in this manner anyhow. Visual Studio
	             should have caught this, but didn't. :/
	     RESET - Previously, "using namespace std;" was used. But when using
	             GCC it caused some conflict naming errors. So std:: was manually
                     added where it needs to be.
             RESET - To future proof this, the endl functions were replaced with \n.
                     Reasoning behind this is for speed and less code.

         SIDE NOTE FOR CODEBLOCKS USERS -- Make sure you add UNICODE in the DEFINES.
         I just add #define UNICODE to the olcNoiseMaker.h header.
*/

#include <iostream>
#include "olcNoiseMaker.h"

//// Global synthesizer variables
double dFrequencyOutput = 0.0;			// dominant output frequency of instrument, i.e. the note
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
	// Shameless self-promotion
	std::wcout << "www.OneLoneCoder.com - Synthesizer Part 1\nSingle Sine Wave Oscillator, No Polyphony\n\n";

	// Get all sound hardware
	std::vector<std::wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Display findings
	for (auto d : devices) std::wcout << "Found Output Device: " << d << "\n";
	std::wcout << "Using Device: " << devices[0] << "\n";

	// Display a keyboard
	std::wcout << "\n" <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |\n" <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |\n" <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__\n" <<
		"|     |     |     |     |     |     |     |     |     |     |\n" <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |\n" <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|\n\n";

	// Create sound machine!!
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 512);

	// Link noise function with sound machine
	sound.SetUserFunction(MakeNoise);

	// Sit in loop, capturing keyboard state changes and modify
	// synthesizer output accordingly
	int nCurrentKey = -1;
	bool bKeyPressed = false;
	while (1)
	{
		bKeyPressed = false;
		for (int k = 0; k < 16; k++)
		{
			if (GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k])) & 0x8000)
			{
				if (nCurrentKey != k)
				{
					dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
					std::wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";
					nCurrentKey = k;
				}

				bKeyPressed = true;
			}
		}

		if (!bKeyPressed)
		{
			if (nCurrentKey != -1)
			{
				std::wcout << "\rNote Off: " << sound.GetTime() << "s                        ";
				nCurrentKey = -1;
			}

			dFrequencyOutput = 0.0;
		}
	}

	return 0;
}
