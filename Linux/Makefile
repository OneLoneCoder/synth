synth: synth1 synth2

synth1: main1.cpp SDLConsole.o SDLAudioManager.o
	g++ main1.cpp SDLConsole.o SDLAudioManager.o -g -std=c++17 -o synth1 -lSDL2 -lSDL2_ttf

synth2: main2.cpp SDLConsole.o SDLAudioManager.o
	g++ main2.cpp SDLConsole.o SDLAudioManager.o -g -std=c++17 -o synth2 -lSDL2 -lSDL2_ttf

SDLConsole.o: SDLConsole.cpp SDLConsole.h
	g++ SDLConsole.cpp -c -g -std=c++17 -o SDLConsole.o -lSDL2 -lSDL2_ttf

SDLAudioManager.o: SDLAudioManager.cpp SDLAudioManager.h
	g++ SDLAudioManager.cpp -c -g -std=c++17 -o SDLAudioManager.o -lSDL2

clean:
	rm *.o synth1 synth2
