#ifndef _SYNTH_APP_HPP
#define _SYNTH_APP_HPP 1

#include <portaudio.h>

#include "Waveform.hpp"
#include "Voice.hpp"
#include "RecycleList.hpp"

#define FRAMES_PER_BUFFER 128

class SynthApp
{

private:
    static const double _fade;
    static RecycleList<Voice> Voices;
    static float _pitch;

    int _selectedMidiIn;
    float _modulationFactor = 0.00f;
    int _joystickFd;
    PaStream *_output;
    
    Waveform _currentWaveform;
    bool _sustainPedal;
    int _sustainCount;

    void NoteOn (int noteNumber, int velocity);
    void NoteOff (int noteNumber);
    void CalcPitch (double rel);
    bool MidiMessageReceived ();
    void JoystickMessageReceived ();

public:
    SynthApp (char *midiDev, Waveform waveform, char *joyDev = nullptr);
    virtual ~SynthApp ();
    
    void Run (bool &keepRunning);

    static int Read (
        const void *inputBuffer,
        void *outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo *timeInfo,
        PaStreamCallbackFlags statusFlags,
        void *userData
    );
};

#endif
