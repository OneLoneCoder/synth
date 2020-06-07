#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <cmath>
#include <atomic>
#include <mutex>

#include <ncursesw/curses.h>

using namespace std;

#include "NoiseMaker.h"
#include "LinuxInput.h"

#define FTYPE double

namespace synth
{
    // Utilities

    // Forward declaration
    struct instrument_base;

    // Converts frequency (hertz) to angular velocity
    FTYPE w(const FTYPE dHertz)
    {
        return dHertz * 2.0 * M_PI;
    }

    // A basic note
    struct note
    {
        int id; // Position in scale
        FTYPE on; // Time note was activated
        FTYPE off; // Time note was deactivated
        bool active;
        instrument_base* channel;

        note()
        {
            id = 0;
            channel = 0;
            active = false;
            on = 0.0;
            off = 0.0;
        }
    };

    // General purpose oscillator
    enum OSC_TYPE
    {
        OSC_SINE = 0,
        OSC_SQUARE,
        OSC_TRIANGLE,
        OSC_SAW_ANA,
        OSC_SAW_DIG,
        OSC_NOISE,
    };

    FTYPE osc(FTYPE dHertz, FTYPE dTime, OSC_TYPE nType = OSC_SINE, FTYPE dLFOHertz = 0.0, FTYPE dLFOAmplitude = 0.0, FTYPE dCustom = 50.0)
    {
        FTYPE dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * sin(w(dLFOHertz) * dTime);

        switch(nType)
        {
            case OSC_SINE: // Sine wave
                return sin(dFreq);

            case OSC_SQUARE: // Square wave
                return sin(dFreq) > 0.0 ? 1.0 : -1.0;

            case OSC_TRIANGLE: // Triangle wave
                return asin(sin(dFreq)) * 2.0 / M_PI;

            case OSC_SAW_ANA: // Saw wave (analogue)
                {
                    FTYPE dOutput = 0.0;
                    for (int n = 1; n < 100; n++)
                        dOutput += sin(n * dFreq) / (FTYPE)n;
                    return dOutput * (2.0 / M_PI);
                }

            case OSC_SAW_DIG: // Saw wave (digital)
                return (2.0 / M_PI) * (dHertz * M_PI * fmod(dTime, 1.0 / dHertz) - (M_PI / 2.0));

            case OSC_NOISE: // Pseudo random noise
                return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;

            default:
                return 0.0;
        }
    }

    // Scale to frequency conversion
    enum SCALE_TYPE
    {
        SCALE_DEFAULT = 0,
    };

    FTYPE scale(const int nNoteID, const SCALE_TYPE nScaleID = SCALE_DEFAULT)
    {
        return 256 * pow(1.0594630943592952645618252949463, nNoteID);
    }

    // Envelope
    struct envelope
    {
        virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff) = 0;
    };

    struct envelope_adsr : public envelope
    {
        FTYPE dAttackTime;
        FTYPE dDecayTime;
        FTYPE dReleaseTime;

        FTYPE dSustainAmplitude;
        FTYPE dStartAmplitude;

        envelope_adsr()
        {
            dAttackTime = 0.1;
            dDecayTime = 0.1;
            dStartAmplitude = 1.0;
            dSustainAmplitude = 1.0;
            dReleaseTime = 0.2;
        }

        virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff)
        {
            FTYPE dAmplitude = 0.0;
            FTYPE dReleaseAmplitude = 0.0;

            if (dTimeOn > dTimeOff) // Note is on
            {
                // ADS
                FTYPE dLifeTime = dTime - dTimeOn;

                // Attack phase
                if (dLifeTime <= dAttackTime)
                    dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

                // Decay phase
                if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
                    dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

                // Sustain phase
                if (dLifeTime > (dAttackTime + dDecayTime))
                    dAmplitude = dSustainAmplitude;
            } else {
                // R
                FTYPE dLifeTime = dTimeOff - dTimeOn;

                // Release
                if (dLifeTime <= dAttackTime)
                    dReleaseAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

                if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
                    dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

                // Sustain phase
                if (dLifeTime > (dAttackTime + dDecayTime))
                    dReleaseAmplitude = dSustainAmplitude;

                dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
            }

            if (dAmplitude <= 0.0001)
                dAmplitude = 0.0;

            return dAmplitude;
        }
    };

    FTYPE env(const FTYPE dTime, envelope& envlpe, const FTYPE dTimeOn, const FTYPE dTimeOff)
    {
        return envlpe.amplitude(dTime, dTimeOn, dTimeOff);
    }

    struct instrument_base
    {
        FTYPE dVolume;
        synth::envelope_adsr env;
        FTYPE fMaxLifeTime;
        wstring name;

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished) = 0;
    };

    struct instrument_bell : public instrument_base
    {
        instrument_bell()
        {
            env.dAttackTime = 0.01;
            env.dDecayTime = 1.0;
            env.dSustainAmplitude = 0.0;
            env.dReleaseTime = 1.0;
            fMaxLifeTime = 3.0;
            name = L"Bell";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (dAmplitude <= 0.0)
                bNoteFinished = true;

            FTYPE dSound =
                + 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SINE, 5.0, 0.001)
                + 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 24))
                + 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 36));

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_bell8 : public instrument_base
    {
        instrument_bell8()
        {
            env.dAttackTime = 0.01;
            env.dDecayTime = 0.5;
            env.dSustainAmplitude = 0.8;
            env.dReleaseTime = 1.0;
            fMaxLifeTime = 3.0;
            name = L"8-bit Bell";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (dAmplitude <= 0.0)
                bNoteFinished = true;

            FTYPE dSound =
                + 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + 0 ), synth::OSC_SQUARE, 5.0, 0.001)
                + 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 12))
                + 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 24));

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_harmonica : public instrument_base
    {
        instrument_harmonica()
        {
            env.dAttackTime = 0.05;
            env.dDecayTime = 1.0;
            env.dSustainAmplitude = 0.95;
            env.dReleaseTime = 0.1;
            fMaxLifeTime = -1.0;
            name = L"Harmonica";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (dAmplitude <= 0.0)
                bNoteFinished = true;

            FTYPE dSound =
                + 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + 0 ), synth::OSC_SQUARE, 5.0, 0.001)
                + 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SQUARE)
                + 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 24), synth::OSC_SQUARE);

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_drumkick : public instrument_base
    {
        instrument_drumkick()
        {
            env.dAttackTime = 0.01;
            env.dDecayTime = 0.15;
            env.dSustainAmplitude = 0.0;
            env.dReleaseTime = 0.0;
            fMaxLifeTime = 1.5;
            name = L"Kick Drum";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)
                bNoteFinished = true;

            FTYPE dSound =
                + 0.99 * synth::osc(dTime - n.on, synth::scale(n.id - 36), synth::OSC_SINE, 1.0, 1.0)
                + 0.01 * synth::osc(0.0, 0, synth::OSC_NOISE);

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_drumsnare : public instrument_base
    {
        instrument_drumsnare()
        {
            env.dAttackTime = 0.0;
            env.dDecayTime = 0.2;
            env.dSustainAmplitude = 0.0;
            env.dReleaseTime = 0.0;
            fMaxLifeTime = 1.0;
            name = L"Snare Drum";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)
                bNoteFinished = true;

            FTYPE dSound =
                + 0.50 * synth::osc(dTime - n.on, synth::scale(n.id - 24), synth::OSC_SINE, 0.5, 1.0)
                + 0.50 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

            return dAmplitude * dSound * dVolume;
        }
    };

    struct instrument_drumhihat : public instrument_base
    {
        instrument_drumhihat()
        {
            env.dAttackTime = 0.01;
            env.dDecayTime = 0.05;
            env.dSustainAmplitude = 0.0;
            env.dReleaseTime = 0.0;
            fMaxLifeTime = 1.0;
            name = L"HiHat Drum";

            dVolume = 1.0;
        }

        virtual FTYPE sound(const FTYPE dTime, synth::note n, bool& bNoteFinished)
        {
            FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
            if (fMaxLifeTime > 0.0 && dTime - n.on >= fMaxLifeTime)
                bNoteFinished = true;

            FTYPE dSound =
                + 0.1 * synth::osc(dTime - n.on, synth::scale(n.id - 12), synth::OSC_SQUARE, 1.5, 1.0)
                + 0.9 * synth::osc(0.0, 0, synth::OSC_NOISE);

            return dAmplitude * dSound * dVolume;
        }
    };

    struct sequencer
    {
    public:
        struct channel
        {
            instrument_base* instrument;
            wstring sBeat;
        };

        sequencer(float tempo = 120.0f, int beats = 4, int subbeats = 4)
        {
            nBeats = beats;
            nSubBeats = subbeats;
            fTempo = tempo;
            nCurrentBeat = 0;
            fBeatTime = (60.0f / fTempo) / (float)nSubBeats;
            nTotalBeats = nBeats*nSubBeats;
            fAccumulate = 0.0f;
        }

        int Update(FTYPE fElapsedTime)
        {
            vecNotes.clear();

            fAccumulate += fElapsedTime;

            while (fAccumulate >= fBeatTime)
            {
                fAccumulate -= fBeatTime;
                nCurrentBeat++;

                if (nCurrentBeat >= nTotalBeats)
                    nCurrentBeat = 0;

                int c = 0;

                for (auto& v : vecChannel)
                {
                    if (v.sBeat[nCurrentBeat] == L'X')
                    {
                        note n;

                        n.channel = vecChannel[c].instrument;
                        n.active = true;
                        n.id = 64;

                        vecNotes.push_back(n);
                    }
                    c++;
                }
            }

            return vecNotes.size();
        }

        void AddInstrument(instrument_base* inst)
        {
            channel c;
            c.instrument = inst;
            vecChannel.push_back(c);
        }

    public:
        int nBeats;
        int nSubBeats;
        float fTempo;
        float fBeatTime;
        int nCurrentBeat;
        int nTotalBeats;
        float fAccumulate;
        vector<note> vecNotes;
        vector<channel> vecChannel;
    };
}

// Global synthesizer variables
synth::instrument_base* voice = nullptr;
mutex muxNotes;
vector<synth::note> vecNotes;

synth::instrument_bell instBell;
synth::instrument_harmonica instHarm;
synth::instrument_drumkick instKick;
synth::instrument_drumsnare instSnare;
synth::instrument_drumhihat instHiHat;

typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T& v, lambda f)
{
    auto n = v.begin();

    while (n != v.end())
    {
        if (!f(*n))
            n = v.erase(n);
        else
            ++n;
    }
}

FTYPE MakeNoise(int nChannel, FTYPE dTime)
{
    unique_lock<mutex> lm(muxNotes);
    FTYPE dMixedOutput = 0.0;

    for (auto& n : vecNotes)
    {
        bool bNoteFinished = false;
        FTYPE dSound = 0.0;

        if (n.channel != nullptr)
            dSound = n.channel->sound(dTime, n, bNoteFinished);

        dMixedOutput += dSound;

        if (bNoteFinished)
            n.active = false;
    }

    safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item){return item.active;});

    return dMixedOutput * 0.1;
}

int main()
{
    WINDOW *w = initscr();
    cbreak();
    nodelay(w, TRUE);
    noecho();

    vector<wstring> devices = NoiseMaker<short>::Enumerate();

    for (auto& d : devices)
        printw("Found Output Device: %ls\n", d.c_str());
    wrefresh(w);

    NoiseMaker<short> sound(devices[0], 44100, 1, 8, 1024);

    voice = new synth::instrument_harmonica();//new bell();

    sound.SetUserFunction(MakeNoise);

    auto draw = [&w](int x, int y, wstring ws)
    {
        using convert_type = std::codecvt_utf8<wchar_t>;
        std::wstring_convert<convert_type, wchar_t> converter;

        std::string s = converter.to_bytes( ws );
        mvwaddstr(w, y, x, s.c_str());
    };

    int keys[16] = {
        KEY_Z,
        KEY_S,
        KEY_X,
        KEY_C,
        KEY_F,
        KEY_V,
        KEY_G,
        KEY_B,
        KEY_N,
        KEY_J,
        KEY_M,
        KEY_K,
        KEY_COMMA,
        KEY_L,
        KEY_DOT,
        KEY_SLASH,
    };

    FILE* input = start_input();

    char* all_keys = get_all_keys(input);

    auto clock_old_time = chrono::high_resolution_clock::now();
    auto clock_real_time = chrono::high_resolution_clock::now();
    double dElapsedTime = 0.0;
    double dWallTime = 0.0;

    // Establish sequencer
    synth::sequencer seq(90.0);

    seq.AddInstrument(&instKick);
    seq.AddInstrument(&instSnare);
    seq.AddInstrument(&instHiHat);

    seq.vecChannel.at(0).sBeat = L"X...X...X..X.X..";
    seq.vecChannel.at(1).sBeat = L"..X...X...X...X.";
    seq.vecChannel.at(2).sBeat = L"X.X.X.X.X.X.X.XX";

    while (1)
    {
        // Update timings
        clock_real_time = chrono::high_resolution_clock::now();
        auto time_last_loop = clock_real_time - clock_old_time;
        clock_old_time = clock_real_time;
        dElapsedTime = chrono::duration<FTYPE>(time_last_loop).count();
        dWallTime += dElapsedTime;
        FTYPE dTimeNow = sound.GetTime();

        // Sequencer
        int newNotes = seq.Update(dElapsedTime);
        muxNotes.lock();
        for (int a = 0; a < newNotes; a++)
        {
            seq.vecNotes[a].on = dTimeNow;
            vecNotes.emplace_back(seq.vecNotes[a]);
        }
        muxNotes.unlock();

        read_keys(input, all_keys);
        for (int k = 0; k < 16; k++)
        {
            int nKeyState = check_key_state(all_keys, keys[k]);
            double dTimeNow = sound.GetTime();
            muxNotes.lock();
            auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item){return item.id == k;});
            if (noteFound == vecNotes.end())
            {
                // Note not found in vector
                if (nKeyState)
                {
                    // Key pressed so create a new note
                    synth::note n;
                    n.id = k;
                    n.on = dTimeNow;
                    //n.channel = voice;
                    n.channel = &instHarm;
                    n.active = true;

                    vecNotes.emplace_back(n);
                } else {
                    // Note not in vector, but key hasn't been pressed
                    // Nothing to do
                }
            } else {
                // Note exists in the vector
                if (nKeyState)
                {
                    if (noteFound->off > noteFound->on)
                    {
                        noteFound->on = dTimeNow;
                        noteFound->active = true;
                    }
                } else {
                    if (noteFound->off < noteFound->on)
                    {
                        noteFound->off = dTimeNow;
                    }
                }
            }
            muxNotes.unlock();
        }

        // VISUAL
        // Clear Background
		werase(w);

		// Draw Sequencer
		draw(2, 2, L"SEQUENCER:");
		for (int beats = 0; beats < seq.nBeats; beats++)
		{
			draw(beats*seq.nSubBeats + 20, 2, L"O");
			for (int subbeats = 1; subbeats < seq.nSubBeats; subbeats++)
				draw(beats*seq.nSubBeats + subbeats + 20, 2, L".");
		}

		// Draw Sequences
		int n = 0;
		for (auto v : seq.vecChannel)
		{
			draw(2, 3 + n, v.instrument->name);
			draw(20, 3 + n, v.sBeat);
			n++;
		}

		// Draw Beat Cursor
		draw(20 + seq.nCurrentBeat, 1, L"|");

		// Draw Keyboard
		draw(2, 8,  L"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |  ");
		draw(2, 9,  L"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |  ");
		draw(2, 10, L"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__");
		draw(2, 11, L"|     |     |     |     |     |     |     |     |     |     |");
		draw(2, 12, L"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |");
		draw(2, 13, L"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|");

		// Draw Stats
		wstring stats =  L"Notes: " + to_wstring(vecNotes.size()) + L" Wall Time: " + to_wstring(dWallTime) + L" CPU Time: " + to_wstring(dTimeNow) + L" Latency: " + to_wstring(dWallTime - dTimeNow) ;
		draw(2, 15, stats);

		wrefresh(w);
        // /VISUAL

        if (get_key_state(input, KEY_Q))
            break;
    }

    stop_input(input);
    free(all_keys);

    sound.Stop();
    delete voice;
    // Little hack: read all the characters that was produced on stdin so they won't appear in commamd prompt
    while (getch() != ERR);
    endwin();

    return 0;
}
