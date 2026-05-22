#include <cmath>
#include <portaudio.h>

#include <cstdio>

/* open */
#include <fcntl.h>
/* close */
#include <unistd.h>

#include <sys/epoll.h>
#include <linux/hidraw.h>

#include "Waveform.hpp"
#include "Voice.hpp"
#include "RecycleList.hpp"
#include "SynthApp.hpp"

const double SynthApp::_fade = 0.000001;
RecycleList<Voice> SynthApp::Voices;
float SynthApp::_pitch = 1.0f;

void SynthApp::CalcPitch (double rel)
{
    // semitone offset -4 .. +4
    double semitones = rel * 4.0;

    // Frequency factor and new frequency
    _pitch = std::powf(2.0, semitones / 12.0);
}

void SynthApp::NoteOn (int noteNumber, int velocity)
{
    Voices.Add(noteNumber, velocity / 127.0f * 0.4f, _currentWaveform, &_modulationFactor);
    _sustainCount = 0;
}

void SynthApp::NoteOff (int noteNumber)
{
    auto view = Voices.All([noteNumber](const std::unique_ptr<Voice> &v) {
        return v->_note == noteNumber;
    });
    
    for (const auto &v : view)
    {
        if (_sustainPedal)
        {
            v->_pendingNoteOff = true;
        }
        else
        {
            v->_isDeleted = true;
        }
    }
}
void SynthApp::JoystickMessageReceived ()
{
    static int val1 = 0;
    static int val2 = 0;
    static unsigned char btn = 0;
    static int calibrate1 = -999;
    static int calibrate2 = -999;
    unsigned char buffer[8];
    
    int n = read(_joystickFd, buffer, sizeof(buffer));

    if (n > 3) {
        int z = buffer[0];
        int x = buffer[1];
        int y = buffer[2];
        //std::printf("decoded: X=%d Y=%d Z=%d\n", x, y, z);
        if (calibrate1 == -999) {
            calibrate1 = x;
            val1 = x;
        }

        if (calibrate2 == -999) {
            calibrate2 = z;
            val2 = z;
        }

        if (val1 != x) {
            CalcPitch((float)(val1-calibrate1)/calibrate1);
            std::printf("pitch: %f\n", _pitch);
            val1 = x;
        }

        if (val2 != z) {
            _modulationFactor = 0.6 * (float)(val2-calibrate2)/calibrate2;
            std::printf("modulation: %f\n", _modulationFactor);
            val2 = z;
        }

        if ((btn & 0x01) == 0 && (buffer[3] & 0x01) == 1) _sustainPedal = !_sustainPedal;

        if ((btn & 0x02) == 0 && (buffer[3] & 0x02) == 2) _currentWaveform++;
        btn = buffer[3];
    }
}

bool SynthApp::MidiMessageReceived ()
{
    bool isNewVoice = false;
    unsigned char buffer[3];
    
    int n = read(_selectedMidiIn, buffer, sizeof(buffer));

    if (n > 0) {
        unsigned char status   = buffer[0];
        unsigned char note     = buffer[1];
        unsigned char velocity = buffer[2];
        
        if (status == 0x90 && velocity > 0)
        {
            NoteOn(note, velocity);
            isNewVoice = true;
        }
        else if (status == 0x80 || (status == 0x90 && velocity == 0))
        {
            NoteOff(note);
        }
        else if (status == 0xB0 && note == 0x40)
        {
            bool newState = velocity >= 64;
            if (newState != _sustainPedal)
            {
                _sustainPedal = newState;

                if (_sustainPedal == false)
                {
                    auto view = Voices.All([](const std::unique_ptr<Voice> &v) {
                        return v->_pendingNoteOff == true;
                    });
                    for (const auto &v : view)
                    {
                        v->_isDeleted = true;
                        v->_pendingNoteOff = false;
                    }
                } else {
                    if (Voices.IsEmpty())
                    {
                        _sustainCount++;
                        if (_sustainCount > 2)
                        {
                            // on silence -> 3x sustain = cycles through waveforms
                            _currentWaveform++;
                            _sustainCount = 0;
                        }
                    } else {
                        _sustainCount = 0;
                    }
                }
            }
        }
        else if (status == 0xE0)
        {
            // relative -1..+1 (center -> 0)
            double rel = (((velocity << 7) | note) - 8192) / (double)8192.0;
            CalcPitch(rel);
        }
    }
    return isNewVoice;
}

SynthApp::SynthApp (char *midiDev, Waveform waveform, char *joyDev)
{
    _sustainCount = 0;
    _sustainPedal = false;
    _currentWaveform = waveform;
    _selectedMidiIn = open(midiDev, O_RDONLY | O_NONBLOCK);
    _joystickFd = -1;
    if (joyDev != nullptr) {
        _joystickFd = open(joyDev, O_RDONLY | O_NONBLOCK);
    }

    Pa_Initialize();
    Pa_OpenDefaultStream(
        &_output,
        0,
        2,
        paFloat32,
        SAMPLE_RATE,
        FRAMES_PER_BUFFER,
        SynthApp::Read,
        NULL
    );
}

SynthApp::~SynthApp ()
{
    if (Pa_IsStreamActive(_output)) {
        Pa_StopStream(_output);
    }
    Pa_CloseStream(_output);
    Pa_Terminate();
    
    close(_selectedMidiIn);
    if (_joystickFd != -1) close(_joystickFd);
}

void SynthApp::Run (bool &keepRunning)
{
    int epoll_fd;
    struct epoll_event *events;

    int maxElements = 1;
    if (_joystickFd != -1) maxElements++;
    
    epoll_fd = epoll_create1(0);
    // 1 element = index 0 = midi
    // 1 element = index 1 = joystick
    events = (struct epoll_event *)calloc(maxElements, sizeof(struct epoll_event));
    events[0].events = EPOLLIN;
    events[0].data.fd = _selectedMidiIn;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _selectedMidiIn, &events[0]);

    if (_joystickFd != -1) {
        events[1].events = EPOLLIN;
        events[1].data.fd = _joystickFd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _joystickFd, &events[1]);
    }

    Pa_StartStream(_output);

    while (keepRunning)
    {
        int n = epoll_wait(epoll_fd, events, maxElements, 200);
        for (int i = 0; i < n; ++i) {
            if (events[i].data.fd == _selectedMidiIn) {
                MidiMessageReceived();
            }
            if (events[i].data.fd == _joystickFd) {
                JoystickMessageReceived();
            }
        }
    }
    
    Pa_StopStream(_output);
    
    free(events);
    close(epoll_fd);
}

int SynthApp::Read (
    const void *inputBuffer,
    void *outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo *timeInfo,
    PaStreamCallbackFlags statusFlags,
    void *userData
){
    float *out = (float *)outputBuffer;
    
    for (long unsigned int j = 0; j < framesPerBuffer; ++j) {
        float mixedSample = 0.0f;

        for (const auto &v : Voices)
        {
            if (v->_isDeleted == false && v->_volume > 0.00001)
            {
                v->_volume -= _fade;
                mixedSample += v->NextSample();
                v->Increment(_pitch);
            }
        }

        if (mixedSample > 1) mixedSample = 1.0f;
        else if (mixedSample < -1) mixedSample = -1.0f;

        *out++ = mixedSample; /* left */
        *out++ = mixedSample; /* right */
    }

    return paContinue;
}
