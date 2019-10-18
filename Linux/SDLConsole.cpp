/*
SDLConsole.cpp
Copyright 2019 David V. Makray

This file is dual licensed under both the GPL3 and MIT licenses.
*/

#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include "SDLConsole.h"

SDLConsole::SDLConsole()
{
	//Initialize SDL video
	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		std::cerr << "Unable to init SDL: " << SDL_GetError() << std::endl;
		exit(1);
	}

	if (TTF_Init() != 0)
	{
		std::cerr << "TTF_Init failed." << std::endl;
		SDL_Quit();
		exit(1);
	}

	m_font = TTF_OpenFont("/usr/share/fonts/truetype/freefont/FreeMono.ttf", 16);
	if (m_font == nullptr)
	{
		std::cerr << "TTF_OpenFont failed." << std::endl;
		SDL_Quit();
		exit(1);
	}
	m_font_height = TTF_FontHeight(m_font);

	//Make sure SDL cleans up prior to exit
	atexit(SDL_Quit);

	//Create a new window
	SDL_Window* window = SDL_CreateWindow(NULL, 0, 0, 800, 600, SDL_WINDOW_SHOWN);
	if (window == nullptr)
	{
		std::cerr << "CreateWindow failed." << std::endl;
		SDL_Quit();
		exit(1);
	}

	m_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (m_renderer == nullptr)
	{
		std::cerr << "CreateRenderer failed." << std::endl;
		SDL_DestroyWindow(window);
		SDL_Quit();
		exit(1);
	}
}

SDLConsole::~SDLConsole()
{
	TTF_CloseFont(m_font);
	SDL_Quit();
}

SDLConsole& SDLConsole::operator<<(std::string stream_text)
{
	if (m_ConsoleText.size() == 0)
		m_ConsoleText.push_back(stream_text);
	else
		m_ConsoleText.back() += stream_text;
	return *this;
}

SDLConsole& SDLConsole::operator<<(Endl)
{
	m_ConsoleText.push_back("");
	return *this;
}

void SDLConsole::Render()
{
	SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
	SDL_RenderClear(m_renderer);

	const SDL_Color white = { 255, 255, 255, 255 };
	int cumlative_font_offset = 0;

	for (auto current_line : m_ConsoleText)
	{
		if (current_line == "")
		{
			cumlative_font_offset += m_font_height + 1;
			continue;
		}

		//Render a line of text into a surface
		SDL_Surface* surface = TTF_RenderText_Blended(m_font, current_line.c_str(), white);
		if (surface == nullptr)
		{
			std::cerr << "TTF_RenderText failed." << std::endl;
			SDL_Quit();
			exit(1);
		}

		//Make surface into a texture
		SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
		SDL_FreeSurface(surface);
		if (texture == nullptr)
		{
			std::cerr << "CreateTexture failed." << std::endl;
			SDL_Quit();
			exit(1);
		}
		SDL_Rect dest = {0, cumlative_font_offset, 0, 0};
		SDL_QueryTexture(texture, NULL, NULL, &dest.w, &dest.h);
		SDL_RenderCopy(m_renderer, texture, NULL, &dest);
		cumlative_font_offset += m_font_height + 1;
	}
	SDL_RenderPresent(m_renderer);
}
