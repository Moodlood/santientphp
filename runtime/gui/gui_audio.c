// ============================================================================
// SantientPHP (SPH) - Native Zero-Dependency Chiptune Synthesizer & Audio Mixer
// Uses Windows Multimedia (winmm.dll) waveOut API
// 4-Channel Software Mixer: Square, Triangle, Sawtooth, LFSR White Noise
// ============================================================================

#include "gui.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

#define SAMPLE_RATE 44100
#define BUFFER_SAMPLES 1024
#define NUM_BUFFERS 3

// Waveform types
#define WAVE_SQUARE   0
#define WAVE_TRIANGLE 1
#define WAVE_SAWTOOTH 2
#define WAVE_NOISE    3

// SFX IDs
#define SFX_NONE      0
#define SFX_JUMP      1
#define SFX_COIN      2
#define SFX_STOMP     3
#define SFX_HURT      4
#define SFX_SPRING    5
#define SFX_VICTORY   6
#define SFX_LASER     7
#define SFX_BOSS_HIT  8
#define SFX_CRUMBLE   9

typedef struct SynthVoice {
    int waveform;
    float freq;
    float target_freq;
    float slide_rate;
    float phase;
    float volume;
    float decay_rate;
    float duty;
    uint32_t lfsr;
    int active;
} SynthVoice;

typedef struct BgmNote {
    int16_t ch0_note; // Square melody (MIDI note 0=rest, 36=C2..84=C6)
    int16_t ch1_note; // Triangle bass
    int16_t ch2_note; // Sawtooth chord / counter-melody
    int16_t ch3_drum; // 0=rest, 1=kick, 2=snare, 3=hihat
} BgmNote;

static HWAVEOUT g_hWaveOut = NULL;
static HANDLE g_hAudioEvent = NULL;
static HANDLE g_hAudioThread = NULL;
static volatile int g_audio_running = 0;
static CRITICAL_SECTION g_audio_cs;
static int g_audio_cs_init = 0;

static SynthVoice g_voices[4];
static float g_master_volume = 0.5f;
static int g_muted = 0;

// SFX state machine
static int g_cur_sfx = 0;
static float g_sfx_timer = 0.0f;
static int g_sfx_step = 0;

// BGM state machine
static int g_cur_track = 0;
static int g_bgm_step = 0;
static float g_bgm_tick_timer = 0.0f;
static float g_bgm_tempo = 135.0f; // BPM (4 ticks per beat = 16th notes)

// Note to Frequency lookup table (MIDI 0..95)
static float g_midi_freq[96];

static void init_freq_table(void) {
    static int initialized = 0;
    if (initialized) return;
    for (int i = 0; i < 96; ++i) {
        if (i == 0) {
            g_midi_freq[i] = 0.0f;
        } else {
            g_midi_freq[i] = 440.0f * powf(2.0f, (float)(i - 69) / 12.0f);
        }
    }
    initialized = 1;
}

// ----------------------------------------------------------------------------
// Built-in Music Tracks (Note / Duration Sequences)
// ----------------------------------------------------------------------------

// Track 1: Greenfield Meadows (Cheerful, heroic platformer theme in C Major)
static const BgmNote TRACK_GREENFIELD[] = {
    // Bar 1
    { 60, 36, 0, 1 }, { 0,  0,  0, 3 }, { 64, 43, 0, 3 }, { 0,  0,  0, 2 },
    { 67, 36, 0, 1 }, { 0,  0,  0, 3 }, { 72, 43, 0, 3 }, { 71, 0,  0, 2 },
    // Bar 2
    { 69, 41, 0, 1 }, { 0,  0,  0, 3 }, { 67, 48, 0, 3 }, { 0,  0,  0, 2 },
    { 65, 41, 0, 1 }, { 64, 0,  0, 3 }, { 62, 43, 0, 3 }, { 0,  0,  0, 2 },
    // Bar 3
    { 60, 36, 0, 1 }, { 0,  0,  0, 3 }, { 62, 43, 0, 3 }, { 64, 0,  0, 2 },
    { 65, 36, 0, 1 }, { 67, 0,  0, 3 }, { 69, 43, 0, 3 }, { 71, 0,  0, 2 },
    // Bar 4
    { 72, 48, 0, 1 }, { 71, 0,  0, 3 }, { 69, 45, 0, 3 }, { 67, 43, 0, 2 },
    { 65, 41, 0, 1 }, { 64, 0,  0, 3 }, { 62, 38, 0, 3 }, { 60, 36, 0, 2 },
};

// Track 2: Spikefall Cavern (Moody, atmospheric cavern theme in E Minor)
static const BgmNote TRACK_CAVERN[] = {
    // Bar 1
    { 64, 28, 52, 1 }, { 0,  0,  0, 3 }, { 67, 28, 0,  0 }, { 0,  35, 0, 2 },
    { 71, 28, 55, 0 }, { 0,  0,  0, 3 }, { 70, 28, 0,  0 }, { 0,  34, 0, 2 },
    // Bar 2
    { 67, 28, 52, 1 }, { 0,  0,  0, 3 }, { 64, 28, 0,  0 }, { 0,  35, 0, 2 },
    { 63, 27, 51, 0 }, { 0,  0,  0, 3 }, { 66, 27, 0,  0 }, { 69, 39, 0, 2 },
    // Bar 3
    { 64, 28, 52, 1 }, { 0,  0,  0, 3 }, { 76, 28, 0,  3 }, { 0,  35, 0, 2 },
    { 74, 28, 55, 0 }, { 0,  0,  0, 3 }, { 72, 28, 0,  0 }, { 71, 35, 0, 2 },
    // Bar 4
    { 69, 26, 50, 1 }, { 67, 0,  0, 3 }, { 66, 26, 0,  3 }, { 64, 38, 0, 2 },
    { 63, 27, 49, 1 }, { 64, 0,  0, 3 }, { 66, 27, 0,  3 }, { 67, 39, 0, 2 },
};

// Track 3: Castle of the Sky (Fast-tempo heroic baroque theme in D Minor)
static const BgmNote TRACK_CASTLE[] = {
    // Bar 1
    { 62, 38, 0, 1 }, { 65, 38, 0, 3 }, { 69, 38, 0, 2 }, { 74, 38, 0, 3 },
    { 73, 38, 0, 1 }, { 69, 38, 0, 3 }, { 73, 38, 0, 2 }, { 76, 38, 0, 3 },
    // Bar 2
    { 77, 41, 0, 1 }, { 74, 41, 0, 3 }, { 77, 41, 0, 2 }, { 81, 41, 0, 3 },
    { 80, 40, 0, 1 }, { 76, 40, 0, 3 }, { 80, 40, 0, 2 }, { 84, 40, 0, 3 },
    // Bar 3
    { 85, 43, 0, 1 }, { 81, 43, 0, 3 }, { 77, 43, 0, 2 }, { 74, 43, 0, 3 },
    { 76, 37, 0, 1 }, { 73, 37, 0, 3 }, { 69, 37, 0, 2 }, { 64, 37, 0, 3 },
    // Bar 4
    { 65, 38, 0, 1 }, { 69, 38, 0, 3 }, { 74, 38, 0, 2 }, { 77, 38, 0, 3 },
    { 76, 33, 0, 1 }, { 74, 33, 0, 3 }, { 73, 33, 0, 2 }, { 74, 38, 0, 3 },
};

// Track 4: Sky Knight Boss Battle (Aggressive, relentless minor groove)
static const BgmNote TRACK_BOSS[] = {
    // Bar 1
    { 70, 34, 58, 1 }, { 70, 34, 58, 3 }, { 71, 35, 59, 2 }, { 70, 34, 58, 3 },
    { 67, 31, 55, 1 }, { 67, 31, 55, 3 }, { 68, 32, 56, 2 }, { 67, 31, 55, 3 },
    // Bar 2
    { 70, 34, 58, 1 }, { 73, 37, 61, 3 }, { 76, 40, 64, 2 }, { 75, 39, 63, 3 },
    { 74, 38, 62, 1 }, { 70, 34, 58, 3 }, { 67, 31, 55, 2 }, { 66, 30, 54, 3 },
};

static void trigger_voice_note(int v_idx, int midi_note, float vol, float decay) {
    if (v_idx < 0 || v_idx >= 4) return;
    SynthVoice* v = &g_voices[v_idx];
    if (midi_note <= 0) {
        v->active = 0;
        return;
    }
    v->freq = g_midi_freq[midi_note];
    v->target_freq = v->freq;
    v->slide_rate = 0.0f;
    v->volume = vol;
    v->decay_rate = decay;
    v->active = 1;
}

static void trigger_drum(int drum_type) {
    if (drum_type == 1) { // Kick (Low triangle pitch drop + noise click)
        SynthVoice* v = &g_voices[1];
        v->waveform = WAVE_TRIANGLE;
        v->freq = 150.0f;
        v->target_freq = 40.0f;
        v->slide_rate = 800.0f;
        v->volume = 0.9f;
        v->decay_rate = 6.0f;
        v->active = 1;
    } else if (drum_type == 2) { // Snare (Noise burst + mid tone)
        SynthVoice* v = &g_voices[3];
        v->waveform = WAVE_NOISE;
        v->volume = 0.65f;
        v->decay_rate = 14.0f;
        v->lfsr = 0xACE1u;
        v->active = 1;
    } else if (drum_type == 3) { // Hi-hat (Very fast noise ping)
        SynthVoice* v = &g_voices[3];
        v->waveform = WAVE_NOISE;
        v->volume = 0.25f;
        v->decay_rate = 45.0f;
        v->lfsr = 0x55AAu;
        v->active = 1;
    }
}

// ----------------------------------------------------------------------------
// Audio Render Loop
// ----------------------------------------------------------------------------

static void render_audio_block(int16_t* buffer, int num_samples) {
    EnterCriticalSection(&g_audio_cs);

    float tick_duration = 60.0f / (g_bgm_tempo * 4.0f); // seconds per 16th note tick
    float dt = 1.0f / (float)SAMPLE_RATE;

    const BgmNote* track_notes = NULL;
    int track_len = 0;
    if (g_cur_track == 1) {
        track_notes = TRACK_GREENFIELD;
        track_len = sizeof(TRACK_GREENFIELD) / sizeof(TRACK_GREENFIELD[0]);
    } else if (g_cur_track == 2) {
        track_notes = TRACK_CAVERN;
        track_len = sizeof(TRACK_CAVERN) / sizeof(TRACK_CAVERN[0]);
    } else if (g_cur_track == 3) {
        track_notes = TRACK_CASTLE;
        track_len = sizeof(TRACK_CASTLE) / sizeof(TRACK_CASTLE[0]);
    } else if (g_cur_track == 4) {
        track_notes = TRACK_BOSS;
        track_len = sizeof(TRACK_BOSS) / sizeof(TRACK_BOSS[0]);
    }

    for (int i = 0; i < num_samples; ++i) {
        // Advance BGM sequencer
        if (track_notes && track_len > 0) {
            g_bgm_tick_timer += dt;
            if (g_bgm_tick_timer >= tick_duration) {
                g_bgm_tick_timer -= tick_duration;
                const BgmNote* note = &track_notes[g_bgm_step];
                if (note->ch0_note > 0 && g_cur_sfx != SFX_JUMP && g_cur_sfx != SFX_COIN) {
                    trigger_voice_note(0, note->ch0_note, 0.45f, 3.5f);
                    g_voices[0].waveform = WAVE_SQUARE;
                    g_voices[0].duty = 0.5f;
                }
                if (note->ch1_note > 0 && g_cur_sfx != SFX_SPRING && g_cur_sfx != SFX_STOMP) {
                    trigger_voice_note(1, note->ch1_note, 0.55f, 2.5f);
                    g_voices[1].waveform = WAVE_TRIANGLE;
                }
                if (note->ch2_note > 0 && g_cur_sfx != SFX_HURT && g_cur_sfx != SFX_LASER) {
                    trigger_voice_note(2, note->ch2_note, 0.35f, 4.0f);
                    g_voices[2].waveform = WAVE_SAWTOOTH;
                }
                if (note->ch3_drum > 0 && g_cur_sfx != SFX_STOMP && g_cur_sfx != SFX_BOSS_HIT) {
                    trigger_drum(note->ch3_drum);
                }

                g_bgm_step = (g_bgm_step + 1) % track_len;
            }
        }

        // SFX Progression
        if (g_cur_sfx != SFX_NONE) {
            g_sfx_timer += dt;
            if (g_cur_sfx == SFX_COIN) { // Two-note chime
                if (g_sfx_step == 0 && g_sfx_timer >= 0.045f) {
                    g_sfx_step = 1;
                    trigger_voice_note(0, 76, 0.65f, 12.0f); // High E5
                    g_voices[0].waveform = WAVE_SQUARE;
                    g_voices[0].duty = 0.25f;
                }
                if (g_sfx_timer >= 0.18f) g_cur_sfx = SFX_NONE;
            } else if (g_cur_sfx == SFX_VICTORY) { // Fanfare
                if (g_sfx_step == 0 && g_sfx_timer >= 0.08f) {
                    g_sfx_step = 1; trigger_voice_note(0, 64, 0.6f, 5.0f); // E4
                } else if (g_sfx_step == 1 && g_sfx_timer >= 0.16f) {
                    g_sfx_step = 2; trigger_voice_note(0, 67, 0.6f, 5.0f); // G4
                } else if (g_sfx_step == 2 && g_sfx_timer >= 0.24f) {
                    g_sfx_step = 3; trigger_voice_note(0, 72, 0.7f, 3.0f); // C5
                }
                if (g_sfx_timer >= 0.6f) g_cur_sfx = SFX_NONE;
            } else if (g_sfx_timer >= 0.35f) {
                g_cur_sfx = SFX_NONE;
            }
        }

        // Synthesize Channels
        float mixed = 0.0f;
        if (!g_muted) {
            for (int v = 0; v < 4; ++v) {
                SynthVoice* voice = &g_voices[v];
                if (!voice->active || voice->volume <= 0.001f) continue;

                // Frequency slide
                if (voice->slide_rate != 0.0f) {
                    if (voice->freq < voice->target_freq) {
                        voice->freq += voice->slide_rate * dt;
                        if (voice->freq > voice->target_freq) voice->freq = voice->target_freq;
                    } else if (voice->freq > voice->target_freq) {
                        voice->freq -= voice->slide_rate * dt;
                        if (voice->freq < voice->target_freq) voice->freq = voice->target_freq;
                    }
                }

                // Waveform generation
                float s = 0.0f;
                if (voice->waveform == WAVE_SQUARE) {
                    voice->phase += voice->freq * dt;
                    if (voice->phase >= 1.0f) voice->phase -= 1.0f;
                    s = (voice->phase < voice->duty) ? 1.0f : -1.0f;
                } else if (voice->waveform == WAVE_TRIANGLE) {
                    voice->phase += voice->freq * dt;
                    if (voice->phase >= 1.0f) voice->phase -= 1.0f;
                    s = (voice->phase < 0.5f) ? (4.0f * voice->phase - 1.0f) : (3.0f - 4.0f * voice->phase);
                } else if (voice->waveform == WAVE_SAWTOOTH) {
                    voice->phase += voice->freq * dt;
                    if (voice->phase >= 1.0f) voice->phase -= 1.0f;
                    s = 2.0f * voice->phase - 1.0f;
                } else if (voice->waveform == WAVE_NOISE) {
                    // 16-bit Galois LFSR
                    uint32_t bit = ((voice->lfsr >> 0) ^ (voice->lfsr >> 2) ^ (voice->lfsr >> 3) ^ (voice->lfsr >> 5)) & 1;
                    voice->lfsr = (voice->lfsr >> 1) | (bit << 15);
                    s = (voice->lfsr & 1) ? 1.0f : -1.0f;
                }

                // Volume decay envelope
                voice->volume -= voice->volume * voice->decay_rate * dt;
                if (voice->volume < 0.005f) {
                    voice->volume = 0.0f;
                    voice->active = 0;
                }

                mixed += s * voice->volume;
            }
        }

        // Master scaling and soft-clipping saturation
        float out_f = mixed * g_master_volume;
        if (out_f > 1.0f) out_f = 1.0f;
        if (out_f < -1.0f) out_f = -1.0f;

        buffer[i] = (int16_t)(out_f * 30000.0f);
    }

    LeaveCriticalSection(&g_audio_cs);
}

static DWORD WINAPI audio_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    WAVEHDR headers[NUM_BUFFERS];
    int16_t buffers[NUM_BUFFERS][BUFFER_SAMPLES];

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        memset(&headers[i], 0, sizeof(WAVEHDR));
        headers[i].lpData = (LPSTR)buffers[i];
        headers[i].dwBufferLength = BUFFER_SAMPLES * sizeof(int16_t);
        headers[i].dwFlags = 0;
        waveOutPrepareHeader(g_hWaveOut, &headers[i], sizeof(WAVEHDR));

        render_audio_block(buffers[i], BUFFER_SAMPLES);
        waveOutWrite(g_hWaveOut, &headers[i], sizeof(WAVEHDR));
    }

    while (g_audio_running) {
        WaitForSingleObject(g_hAudioEvent, 50);
        if (!g_audio_running) break;

        for (int i = 0; i < NUM_BUFFERS; ++i) {
            if (headers[i].dwFlags & WHDR_DONE) {
                render_audio_block(buffers[i], BUFFER_SAMPLES);
                waveOutWrite(g_hWaveOut, &headers[i], sizeof(WAVEHDR));
            }
        }
    }

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        waveOutUnprepareHeader(g_hWaveOut, &headers[i], sizeof(WAVEHDR));
    }

    return 0;
}

#endif // _WIN32

// ============================================================================
// Public C API
// ============================================================================

void sph_audio_init(void) {
#if defined(_WIN32)
    if (g_audio_running) return;

    if (!g_audio_cs_init) {
        InitializeCriticalSection(&g_audio_cs);
        g_audio_cs_init = 1;
    }

    init_freq_table();

    memset(g_voices, 0, sizeof(g_voices));
    g_voices[0].waveform = WAVE_SQUARE;
    g_voices[0].duty = 0.5f;
    g_voices[1].waveform = WAVE_TRIANGLE;
    g_voices[2].waveform = WAVE_SAWTOOTH;
    g_voices[3].waveform = WAVE_NOISE;
    g_voices[3].lfsr = 0xACE1u;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1; // Mono
    wfx.nSamplesPerSec = SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    g_hAudioEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!g_hAudioEvent) return;

    MMRESULT res = waveOutOpen(&g_hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)g_hAudioEvent, 0, CALLBACK_EVENT);
    if (res != MMSYSERR_NOERROR) {
        CloseHandle(g_hAudioEvent);
        g_hAudioEvent = NULL;
        return;
    }

    g_audio_running = 1;
    g_hAudioThread = CreateThread(NULL, 0, audio_thread_proc, NULL, 0, NULL);
#endif
}

void sph_audio_shutdown(void) {
#if defined(_WIN32)
    if (!g_audio_running) return;
    g_audio_running = 0;
    if (g_hAudioEvent) SetEvent(g_hAudioEvent);
    if (g_hAudioThread) {
        WaitForSingleObject(g_hAudioThread, 500);
        CloseHandle(g_hAudioThread);
        g_hAudioThread = NULL;
    }
    if (g_hWaveOut) {
        waveOutReset(g_hWaveOut);
        waveOutClose(g_hWaveOut);
        g_hWaveOut = NULL;
    }
    if (g_hAudioEvent) {
        CloseHandle(g_hAudioEvent);
        g_hAudioEvent = NULL;
    }
#endif
}

void sph_audio_play_sfx(int32_t sfx_id) {
#if defined(_WIN32)
    if (!g_audio_running) return;
    EnterCriticalSection(&g_audio_cs);

    g_cur_sfx = sfx_id;
    g_sfx_timer = 0.0f;
    g_sfx_step = 0;

    if (sfx_id == SFX_JUMP) { // Rapid upward slide on Square
        SynthVoice* v = &g_voices[0];
        v->waveform = WAVE_SQUARE;
        v->duty = 0.5f;
        v->freq = 280.0f;
        v->target_freq = 780.0f;
        v->slide_rate = 3200.0f;
        v->volume = 0.65f;
        v->decay_rate = 7.0f;
        v->active = 1;
    } else if (sfx_id == SFX_COIN) { // High chime (B5 -> E6)
        SynthVoice* v = &g_voices[0];
        v->waveform = WAVE_SQUARE;
        v->duty = 0.25f;
        v->freq = 987.0f; // B5
        v->target_freq = 987.0f;
        v->slide_rate = 0.0f;
        v->volume = 0.6f;
        v->decay_rate = 6.0f;
        v->active = 1;
    } else if (sfx_id == SFX_STOMP) { // Satisfying crunch
        SynthVoice* v0 = &g_voices[1]; // Low thud
        v0->waveform = WAVE_TRIANGLE;
        v0->freq = 180.0f;
        v0->target_freq = 40.0f;
        v0->slide_rate = 900.0f;
        v0->volume = 0.85f;
        v0->decay_rate = 9.0f;
        v0->active = 1;

        SynthVoice* v1 = &g_voices[3]; // Noise crack
        v1->waveform = WAVE_NOISE;
        v1->volume = 0.75f;
        v1->decay_rate = 18.0f;
        v1->lfsr = 0xACE1u;
        v1->active = 1;
    } else if (sfx_id == SFX_HURT) { // Downward warning buzz
        SynthVoice* v = &g_voices[2];
        v->waveform = WAVE_SAWTOOTH;
        v->freq = 380.0f;
        v->target_freq = 90.0f;
        v->slide_rate = 1200.0f;
        v->volume = 0.75f;
        v->decay_rate = 6.0f;
        v->active = 1;
    } else if (sfx_id == SFX_SPRING) { // Ascending sweep
        SynthVoice* v = &g_voices[1];
        v->waveform = WAVE_TRIANGLE;
        v->freq = 220.0f;
        v->target_freq = 1100.0f;
        v->slide_rate = 4200.0f;
        v->volume = 0.8f;
        v->decay_rate = 4.5f;
        v->active = 1;
    } else if (sfx_id == SFX_VICTORY) { // Fanfare
        SynthVoice* v = &g_voices[0];
        v->waveform = WAVE_SQUARE;
        v->duty = 0.5f;
        v->freq = 523.0f; // C4
        v->target_freq = 523.0f;
        v->slide_rate = 0.0f;
        v->volume = 0.6f;
        v->decay_rate = 5.0f;
        v->active = 1;
    } else if (sfx_id == SFX_LASER) { // Rapid pew-pew
        SynthVoice* v = &g_voices[2];
        v->waveform = WAVE_SAWTOOTH;
        v->freq = 1200.0f;
        v->target_freq = 150.0f;
        v->slide_rate = 8000.0f;
        v->volume = 0.6f;
        v->decay_rate = 15.0f;
        v->active = 1;
    } else if (sfx_id == SFX_BOSS_HIT) { // Heavy hit
        SynthVoice* v = &g_voices[3];
        v->waveform = WAVE_NOISE;
        v->volume = 0.9f;
        v->decay_rate = 8.0f;
        v->lfsr = 0xBEEFu;
        v->active = 1;
    } else if (sfx_id == SFX_CRUMBLE) { // Stone crumble
        SynthVoice* v = &g_voices[3];
        v->waveform = WAVE_NOISE;
        v->volume = 0.45f;
        v->decay_rate = 22.0f;
        v->lfsr = 0x7654u;
        v->active = 1;
    }

    LeaveCriticalSection(&g_audio_cs);
#else
    (void)sfx_id;
#endif
}

void sph_audio_set_bgm(int32_t track_id) {
#if defined(_WIN32)
    if (!g_audio_running) return;
    EnterCriticalSection(&g_audio_cs);
    g_cur_track = track_id;
    g_bgm_step = 0;
    g_bgm_tick_timer = 0.0f;
    if (track_id == 1) g_bgm_tempo = 132.0f;
    else if (track_id == 2) g_bgm_tempo = 110.0f;
    else if (track_id == 3) g_bgm_tempo = 145.0f;
    else if (track_id == 4) g_bgm_tempo = 155.0f;
    LeaveCriticalSection(&g_audio_cs);
#else
    (void)track_id;
#endif
}

void sph_audio_set_volume(float vol) {
#if defined(_WIN32)
    EnterCriticalSection(&g_audio_cs);
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    g_master_volume = vol;
    LeaveCriticalSection(&g_audio_cs);
#else
    (void)vol;
#endif
}

void sph_audio_toggle_mute(void) {
#if defined(_WIN32)
    EnterCriticalSection(&g_audio_cs);
    g_muted = !g_muted;
    LeaveCriticalSection(&g_audio_cs);
#endif
}
