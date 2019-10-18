/*
SDLAudioManager.cpp
Copyright 2019 David V. Makray

This file is dual licensed under both the GPL3 and MIT licenses.
*/

#include <iostream>
#include <SDL2/SDL.h>
#include "SDLAudioManager.h"

SDLAudioManager* SDLAudioManager::s_Instance = nullptr;

SDLAudioManager::SDLAudioManager()
{
	s_Instance = this;

	if(SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		std::cerr << "SDL2 didn't initialize." << std::endl;
		exit(1);
	}
}

SDLAudioManager::~SDLAudioManager()
{
	SDL_CloseAudio();
}

static void AudioCallbackWrap(void* userdata, unsigned char* stream, int streamLength)
{
	SDLAudioManager::s_Instance->AudioCallback(userdata, stream, streamLength);
}

void SDLAudioManager::AudioCallback(void* userdata, unsigned char* stream, int streamLength)
{
	TimeStruct* time_struct = (TimeStruct*)userdata;
	if (streamLength == 0)
		return;

	SDL_memset(stream, 0, streamLength);
	float* stream_float = (float*)stream;
	int audio_index;
	for (audio_index = 0; audio_index < (streamLength/sizeof(float)); audio_index++)
	{
		stream_float[audio_index] = (float)(m_userFunction(time_struct->audio_time + (double)audio_index / (double)44100));
	}
	time_struct->audio_time += (double)audio_index / 44100.0;
}

double SDLAudioManager::GetTime()
{
	return time_struct.audio_time;
}

void SDLAudioManager::SetUserFunction(double(*func)(double))
{
	m_userFunction = func;
	
	//Setup the parameters of the audio stream
	SDL_memset(&m_audio_req, 0, sizeof(m_audio_req));
	m_audio_req.freq = 48000;
	m_audio_req.format = AUDIO_F32;
	m_audio_req.channels = 1;
	m_audio_req.samples = 1024;
	m_audio_req.callback = AudioCallbackWrap;
	m_audio_req.userdata = &time_struct;

	m_dev = SDL_OpenAudioDevice(NULL, 0, &m_audio_req, NULL, 0);
	if(m_dev == 0)
	{
		std::cerr << "Error: " << SDL_GetError() << std::endl;
		SDL_Quit();
		exit(1);
	}

	SDL_PauseAudioDevice(m_dev, 0);	//Play audio
}
