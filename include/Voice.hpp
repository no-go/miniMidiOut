#ifndef _VOICE_HPP
#define _VOICE_HPP 1

#include <numbers> 
#include "Waveform.hpp"

#define SAMPLE_RATE 22050

class Voice
{
private:
    const double TWO_PI = 2.0 * std::numbers::pi;
    Waveform _waveform;
    double _mod;
    bool _modulationUp;

    double Increment ();

public:
    int _note;
    float _orgFrequency;
    float _frequency;
    double _volume;
    double _phase;
    bool _isDeleted;
    bool _pendingNoteOff;

    float _modFactor;
    unsigned _modFrequency;

    Voice (
        int note,
        float volume,
        Waveform waveform = Waveform::Sin,
        float modFactor = 0.00f,
        unsigned modFrequency = 5
    );
    float NextSample ();
    float MidiNoteToFrequency (int note);
};

#endif
