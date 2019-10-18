const double PI = 2.0 * acos(0.0);

static void AudioCallbackWrap(void*, unsigned char*, int);

struct TimeStruct
{
	double audio_time = 0.0;
};

class SDLAudioManager
{
	public:
		SDLAudioManager();
		~SDLAudioManager();
		static SDLAudioManager* s_Instance;
		void AudioCallback(void*, unsigned char*, int);
		double GetTime();
		void SetUserFunction(double(*func)(double));

	private:
		TimeStruct time_struct;
		double(*m_userFunction)(double);
		SDL_AudioSpec m_audio_req;
		SDL_AudioDeviceID m_dev;
};
