/*
	OneLoneCoder.com - Simple Audio Noisy Thing
	"Allows you to simply listen to that waveform!" - @Javidx9

	Disclaimer
	~~~~~~~~~~

	I don't care what you use this for. It's intended to be educational, and perhaps
	to the oddly minded - a little bit of fun. Please hack this, change it and use it
	in any way you see fit. BUT, you acknowledge that I am not responsible for anything
	bad that happens as a result of your actions. However, if good stuff happens, I
	would appreciate a shout out, or at least give the blog some publicity for me.
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
using namespace std;

#include "olcNoiseMaker.h"

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
	// Shameless self-promotion
	wcout << "www.OneLoneCoder.com - Synthesizer Part 1" << endl << "Single Sine Wave Oscillator, No Polyphony" << endl << endl;

	// Get all sound hardware
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Display findings
	for (auto d : devices) wcout << "Found Output Device: " << d << endl;
	wcout << "Using Device: " << devices[0] << endl;

	// Display a keyboard
	wcout << endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;

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
					wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";					
					nCurrentKey = k;
				}

				bKeyPressed = true;
			}
		}

		if (!bKeyPressed)
		{	
			if (nCurrentKey != -1)
			{
				wcout << "\rNote Off: " << sound.GetTime() << "s                        ";
				nCurrentKey = -1;
			}

			dFrequencyOutput = 0.0;
		}
	}

	return 0;
}
