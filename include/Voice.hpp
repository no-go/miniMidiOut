#ifndef _VOICE_HPP
#define _VOICE_HPP 1

#include <numbers> 
#include "Waveform.hpp"

#define SAMPLE_RATE 22050

class Voice
{
private:
    const double TWO_PI= 2.0 * std::numbers::pi;

    Waveform _waveform;
    double _mod;
    bool _modulationUp;
    float *_modFactor;

public:

    int _note;
    float _frequency;
    double _volume;
    double _phase;
    bool _isDeleted;
    bool _pendingNoteOff;

    unsigned _modFrequency;

    Voice (
        int note,
        float volume,
        Waveform waveform,
        float *modFactor,
        unsigned modFrequency = 5
    );
    float NextSample ();
    float MidiNoteToFrequency (int note);
    void Increment (float pitch = 1.0f);
};

#endif
