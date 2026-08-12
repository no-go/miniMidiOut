#include <Arduino.h>
#include <stdint.h>
#include "voice.hpp"
#include "sustain.hpp"
#include "pitchbend.hpp"
#include "modulation.hpp"
#include "octave.hpp"

Voice voices[VOICE_MAX];
volatile int voice_active_value;
volatile uint32_t sample_counter = 0;

Voice *voice_new () {
    int8_t isFound = -1;
    uint8_t oldest_idx = 0;
    uint32_t oldest_time = voices[oldest_idx].started_at;
    Voice *v;
    for (uint8_t i = 0; i < VOICE_MAX; ++i) {
        v = &voices[i];
        if (v->state == VOICE_OFF) {
            isFound = i;
        } else {
            v->volume = ((float)v->volume) * 0.85f;
            if (v->started_at < oldest_time) {
                oldest_time = v->started_at;
                oldest_idx = i;
            }
        }
    }
    if (isFound != -1) return &voices[isFound];
    // reuse oldest active voice and make ++ invalid
    voice_active_value--;
    return &voices[oldest_idx];
}

void voice_off (const uint8_t note) {
    int i;
    for (i = 0; i < VOICE_MAX; i++) {
        if ((voices[i].state != VOICE_OFF) && voices[i].note == note) {
            noInterrupts();
            voices[i].hold = 0;
            voices[i].release = sustain_on? VOICE_SUSTAIN_RELEASE : VOICE_FAST_RELEASE;
            interrupts();
        }
    }
}

void voice_release_refresh (Voice *v) {
    // Q8 scaling: low notes (small incr) release faster, notes >= C5 keep rate
    uint32_t s;
    if (v->incr == 0) s = 1u<<8;
    else s = (uint32_t)(((uint64_t)RELEASE_REF_INCR << 8) / v->incr);
    if (s < (1u<<8)) s = 1u<<8;
    if (s > (RELEASE_MAX_SCALE<<8)) s = RELEASE_MAX_SCALE<<8;
    v->release_scale = s;
}

void voice_init (Voice *v, uint8_t note, uint8_t velocity) {
    v->state = VOICE_OFF;
    v->freqX100 = voice_get_freq(note + octave_pitch);
    v->incr = pitchbend_incr(v->freqX100);
    voice_release_refresh(v);
    v->note = note;
    v->phase = 0;
    v->modulation = 0;
    v->mod_up = true;
    v->hold = 4*SAMPLE_RATE;
    v->volume = ((uint32_t)velocity)<<10;
    v->release = VOICE_SUSTAIN_RELEASE;
    v->started_at = sample_counter;
}

uint32_t voice_get_freq (uint8_t note) {
    if (note > 132) note = 132;
    return pgm_read_dword(&voice_midiFreq[note]);
}