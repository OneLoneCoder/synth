class SDLConsole
{
	public:
		SDLConsole();
		~SDLConsole();
		void Render();

		std::vector<std::string> m_ConsoleText;
		static struct Endl{} endl;

		SDLConsole& operator<<(std::string);
		SDLConsole& operator<<(Endl);

	private:
		SDL_Renderer* m_renderer;
		TTF_Font* m_font;
		int m_font_height;
};
