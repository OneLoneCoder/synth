#ifndef NOISEMAKER_H_INCLUDED
#define NOISEMAKER_H_INCLUDED

#include <iostream>
#include <cmath>
#include <fstream>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <string>
#include <atomic>
#include <algorithm>

#include <locale>
#include <codecvt>
#include <type_traits>

// Linux-specific stuff begins
#include <alsa/asoundlib.h>

using namespace std;

template<class T>
class NoiseMaker
{
public:
    NoiseMaker(wstring sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 32, unsigned int nSamples = 2048)
    {
        Create(sOutputDevice, nSampleRate, nChannels, nBlocks, nSamples);
    }

    ~NoiseMaker()
    {
        Destroy();
    }

    bool Create(const wstring sOutputDevice, unsigned int nSampleRate = 44100, unsigned int nChannels = 1, unsigned int nBlocks = 32, unsigned int nSamples = 2048)
    {
        m_bReady = false;
        m_nSampleRate = nSampleRate;
        m_nChannels = nChannels;
        m_nBlockCount = nBlocks;
        m_nBlockSamples = nSamples;
        m_nBlockFree = m_nBlockCount;
        m_nBlockCurrent = 0;
        m_pBlockMemory = nullptr;

        m_userFunction = nullptr;

        // Validate device
        vector<wstring> devices = Enumerate();
        auto d = std::find(devices.begin(), devices.end(), sOutputDevice);

        if (d != devices.end())
        {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

            // Check and init device
            snd_pcm_hw_params_t *hw_params;

            // Open ALSA device by it's name in playback mode
            snd_pcm_open(&m_hwDevice, converter.to_bytes(sOutputDevice).c_str(), SND_PCM_STREAM_PLAYBACK, 0);
            // Allocate memory for the parameters and initialize it with default values
            snd_pcm_hw_params_malloc(&hw_params);
            snd_pcm_hw_params_any(m_hwDevice, hw_params);

            // Tell the ALSA we are going to use it with indirect buffered writes (calls to snd_pcm_writei)
            // in interleaved mode (samples for different channels are placed in the buffer next to each other).
            // The other options include indirect non-interleaved mode (samples for different channels placed
            // in differrent halves/quarters/etc of the buffer) and direct writes to memory shared with sound
            // hardware (also in interleaved / non-interleaved manner).
            // (Not that all that matters for us with our mono output)
            if (snd_pcm_hw_params_set_access(m_hwDevice, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
                cout << "Cannot set access type" << endl;
                return false;
            }

            // Specify asynchronous handler for audio events
            if (snd_async_add_pcm_handler(&m_aHandler, m_hwDevice, alsaProcWrap, this) < 0)
            {
                cout << "Unable to register async handler" << endl;
                return false;
            }

            // Determine and set samples format (16-bit shorts, 32-bit floats, etc)
            snd_pcm_format_t nFormat = GetAlsaPcmFormat();
            snd_pcm_hw_params_set_format(m_hwDevice, hw_params, nFormat);

            // Set the sample rate. Beware that ALSA might choose sample rate
            // slightly different (but close) to that we supplied (whence the "rate_near"),
            // so in real world it's good to check nSampleRate after function call
            snd_pcm_hw_params_set_rate_near(m_hwDevice, hw_params, &nSampleRate, 0);
            // Set the number of channels
            snd_pcm_hw_params_set_channels(m_hwDevice, hw_params, nChannels);
            // Pass the parameters to ALSA
            snd_pcm_hw_params(m_hwDevice, hw_params);
            // Dispose memory occupied by the parameters structure
            snd_pcm_hw_params_free(hw_params);
            // Prepare sound card for interaction
            if (snd_pcm_prepare(m_hwDevice) < 0)
            {
                cout << "Failed to prepare sound interface" << endl;
                return false;
            }
        }

        // Allocate block memory
        m_pBlockMemory = new T[m_nBlockCount * m_nBlockSamples];
        if (m_pBlockMemory == nullptr)
            return Destroy();
        bzero(m_pBlockMemory, m_nBlockCount * m_nBlockSamples * sizeof(T));

        // ALSA don't need Wave headers, so that's it

        m_bReady = true;

        m_thread = thread(&NoiseMaker::MainThread, this);

        unique_lock<mutex> lm(m_muxBlockNotZero);
        m_cvBlockNotZero.notify_one();

        return true;
    }

    bool Destroy()
    {
        return false;
    }

    bool IsOK()
    {
        return true;
    }

    void Start()
    {
    }

    void Stop()
    {
        m_bReady = false;
        m_thread.join();
    }

    double GetTime()
    {
        return m_dTime;
    }

    double mix(double dSample1, double dAmp1, double dSample2, double dAmp2)
    {
        return (dSample1*dAmp1) + (dSample2*dAmp2);
    }

    double clip(double dSample, double dMax)
    {
        if (dSample >= 0.0)
            return fmin(dSample, dMax);
        else
            return fmax(dSample, -dMax);
    }

    double amplify(double dSample, double dGain)
    {
        return dSample * dGain;
    }

    double sgn(double d)
    {
        return 2.0 * (0.0 < d) - 1.0;
    }

    double osc(double dTime, double dAmp, double dFreq, double dPhase = 0.0)
    {
        return dAmp * sin(dTime * 2.0 * M_PI * dFreq + dPhase);
    }

    double tri(double dTime, double dAmp, double dFreq, double dPhase = 0.0)
    {
        return ((dAmp * 2.0) / M_PI) * asin(osc(dTime, dAmp, dFreq, dPhase));
    }

    double sqr(double dTime, double dAmp, double dFreq, double dPhase = 0.0)
    {
        double d = osc(dTime, dAmp, dFreq, dPhase);
        return d * sgn(d);
    }

    double noise(double dTime = 0.0, double dAmp = 0.0, double dFreq = 0.0, double dPhase = 0.0)
    {
        return dAmp * ((double)rand() / (double)RAND_MAX);
    }

    // Override to process current sample
    virtual double UserProcess(int nChannel, double dTime)
    {
        double dWeird = sqr(dTime, 1.0, 0.3) * ((tri(dTime, 1.0, sqr(dTime, 4.0, 5.0) + 440.0, osc(dTime, 0 * M_PI, 0.5)) + osc(dTime, 1.0, 440.0)));
        WriteBuffer(0, dWeird);
        double dDelay = ReadBuffer(0);
        return mix(dWeird, 0.3, dDelay, 0.3);
    }

    // Override to create buffers, tools, other long-term fixtures
    virtual bool UserConstructBuffer()
    {
        ConstructBuffer(0, 0.05);
        return true;
    }

    struct NoiseBuffer
    {
        int id;
        double dLength;
        double dAttenuation;
        unsigned int nSamples;
        unsigned int nFront;
        unsigned int nBack;
        double* buffer;

        double read()
        {
            double d = buffer[nBack];
            nBack++;
            nBack %= nSamples;
            return d;
        }

        void write(double d)
        {
            buffer[nFront] = d;
            nFront++;
            nFront %= nSamples;
        }
    };

    void WriteBuffer(int id, double d)
    {
        m_buffers[id].write(d);
    }

    double ReadBuffer(int id)
    {
        return m_buffers[id].read();
    }

    bool ConstructBuffer(int id, double dLength, double dAttenuation = 1.0)
    {
        NoiseBuffer b;
        b.id = id;
        b.dLength = dLength;
        b.dAttenuation = dAttenuation;
        b.nSamples = (unsigned int)((double)m_nSampleRate * dLength);
        b.nFront = 0;
        b.nBack = 1;
        b.buffer = new double[b.nSamples];

        m_buffers.push_back(b);

        return true;
    }

    static vector<wstring> Enumerate()
    {
        vector<wstring> sDevices;

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter; // string <-> wide string

        // Linux-specific ALSA stuff

        // Device "default" is always there
        sDevices.push_back(L"default");

        char **hints;

        // Get list of all devices that is capable of playback
        int err = snd_device_name_hint(-1, "pcm", (void***)&hints);
        if (err == 0)
        {
            char** n = hints;
            while (*n != NULL)
            {
                // Get device name
                char *name = snd_device_name_get_hint(*n, "NAME");

                if (name != NULL)
                {
                    if (strncmp("null", name, 4) != 0)
                        sDevices.push_back(converter.from_bytes(name));
                    free(name);
                }
                n++;
            }
            snd_device_name_free_hint((void**)hints);
        }

        return sDevices;
    }

    void SetUserFunction(double (*func)(int, double))
    {
        m_userFunction = func;
    }

private:
    double (*m_userFunction)(int, double);

    unsigned int m_nSampleRate;
    unsigned int m_nChannels;
    unsigned int m_nBlockCount;
    unsigned int m_nBlockSamples;
    unsigned int m_nBlockCurrent;

    vector<NoiseBuffer> m_buffers;

    T* m_pBlockMemory;
    // waveHeaders
    snd_pcm_t* m_hwDevice;
    snd_async_handler_t* m_aHandler;

    thread m_thread;
    atomic<bool> m_bReady;
    atomic<unsigned int> m_nBlockFree;
    condition_variable m_cvBlockNotZero;
    mutex m_muxBlockNotZero;

    atomic<double> m_dTime;

    // A bit of juicy function what helps to determine correct PCM format for our buffer
    static snd_pcm_format_t GetAlsaPcmFormat()
    {
        if (std::is_signed<T>::value)
        {
            if (std::is_floating_point<T>::value)
            {
                switch (sizeof(T))
                {
                    case 4: // Float
                        return SND_PCM_FORMAT_FLOAT_LE;

                    case 8: // Double
                    default:
                        return SND_PCM_FORMAT_FLOAT64_LE;
                }
            } else {
                switch (sizeof(T))
                {
                    case 1: // Signed byte
                        return SND_PCM_FORMAT_S8;

                    case 2: // Signed short
                        return SND_PCM_FORMAT_S16_LE;

                    case 4: // Signed int
                    default:
                        return SND_PCM_FORMAT_S32_LE;
                }
            }
        } else {
            switch (sizeof(T))
            {
                case 1: // Unsigned byte
                    return SND_PCM_FORMAT_U8;

                case 2: // Unsigned short
                    return SND_PCM_FORMAT_U16_LE;

                case 4: // Unsigned int
                default:
                    return SND_PCM_FORMAT_U32_LE;
            }
        }
    }

    // Hander for soundcard events
    void alsaProc(/* No parameters */)
    {
        snd_pcm_state_t state = snd_pcm_state(m_hwDevice);
        // Handle underrun
        if (state == SND_PCM_STATE_XRUN)
        {
            int err = snd_pcm_prepare(m_hwDevice);
            if (err < 0)
            {
                // Couldn't recovery; exiting
                exit(1);
            }
        }

        m_nBlockFree++;
        unique_lock<mutex> lm(m_muxBlockNotZero);
        m_cvBlockNotZero.notify_one();
    }

    // Static wrapper for sound card handler
    static void alsaProcWrap(snd_async_handler_t* ahandler)
    {
        NoiseMaker<T>* data = (NoiseMaker<T>*)snd_async_handler_get_callback_private(ahandler);
        data->alsaProc();
    }

    void MainThread()
    {
        m_dTime = 0.0;
        double dTimeStep = 1.0 / (double)m_nSampleRate;

        T nMaxSample = (T)pow(2, (sizeof(T) * 8) - 1) - 1;

        double dMaxSample = (double)nMaxSample;

        UserConstructBuffer();

        T nPreviousSample = 0;

        while (m_bReady)
        {
            // Wait for the block to become available
            if (m_nBlockFree == 0)
            {
                unique_lock<mutex> lm(m_muxBlockNotZero);
                m_cvBlockNotZero.wait(lm);
            }

            // Block is here, use it
            m_nBlockFree--;

            // Process block
            int nCurrentBlock = m_nBlockCurrent * m_nBlockSamples;
            bool bGlitchDetected = false;
            T nGlitchThreshold = 400;
            T nNewSample = 0;
            int nSampleCount = 0;

            for (unsigned int n = 0; n < m_nBlockSamples; n += m_nChannels)
            {
                // Update buffers

                for (int c = 0; c < m_nChannels; c++)
                {
                    // User process
                    if (m_userFunction == nullptr)
                        nNewSample = (T)(clip(UserProcess(c, m_dTime), 1.0) * dMaxSample);
                    else
                        nNewSample = (T)(clip(m_userFunction(c, m_dTime), 1.0) * dMaxSample);

                    m_pBlockMemory[nCurrentBlock + n + c] = nNewSample;
                    nPreviousSample = nNewSample;
                }
                m_dTime = m_dTime + dTimeStep;
            }

            int err;
            // Send block to sound device
            // All the errors will be handled in async callback
            snd_pcm_writei(m_hwDevice, &m_pBlockMemory[nCurrentBlock], m_nBlockSamples);

            snd_pcm_start(m_hwDevice);
            m_nBlockCurrent++;
            m_nBlockCurrent %= m_nBlockCount;
        }
    }
};

#endif // NOISEMAKER_H_INCLUDED
