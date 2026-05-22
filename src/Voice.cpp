#include <cmath>
#include "Waveform.hpp"
#include "Voice.hpp"

void Voice::Increment (float pitch)
{   
    if (*_modFactor < 0.0f) {
        if (_modulationUp)
        {
            _mod -= 0.5 * _modFrequency / SAMPLE_RATE;
        }
        else
        {
            _mod += 0.5 * _modFrequency / SAMPLE_RATE;
        }

        if (_mod > (-1.0 * *_modFactor)) _modulationUp = true;
        if (_mod < *_modFactor) _modulationUp = false;
    }
    else
    {
        if (_modulationUp)
        {
            _mod += 0.5 * _modFrequency / SAMPLE_RATE;
        }
        else
        {
            _mod -= 0.5 * _modFrequency / SAMPLE_RATE;
        }

        if (_mod < (-1.0 * *_modFactor)) _modulationUp = true;
        if (_mod > *_modFactor) _modulationUp = false;
    }

    _phase += TWO_PI * (pitch * _frequency * (1.0 + _mod)) / SAMPLE_RATE;
    if (_phase >= TWO_PI) _phase -= TWO_PI;
}

Voice::Voice (int note, float volume, Waveform waveform, float *modFactor, unsigned modFrequency)
{
    _waveform = waveform;
    _note = note;
    _phase = 0.0;
    _volume = volume;
    _isDeleted = false;
    _pendingNoteOff = false;

    _modFactor = modFactor;
    _modFrequency = modFrequency;
    _mod = 0.0;
    _modulationUp = true;

    _frequency = MidiNoteToFrequency(note);
}

float Voice::NextSample ()
{
    float sample = 0.0f;
    double phaseNorm = _phase / TWO_PI; // 0..1

    switch (_waveform)
    {
        case Waveform::Saw:
            sample = (2.0 * phaseNorm - 1.0) * _volume;
            break;

        case Waveform::Square:
            sample = (phaseNorm < 0.5) ? _volume : -_volume;
            break;

        case Waveform::Triangle:
            sample = (1.0 - 4.0 * std::abs(phaseNorm - 0.5)) * _volume;
            break;

        case Waveform::Sin:
        default:
            sample = std::sinf(_phase) * _volume;
            break;
    }

    return sample;
}

float Voice::MidiNoteToFrequency (int note)
{
    return 440.0f * std::powf(2.0, (note - 69) / 12.0);
}


