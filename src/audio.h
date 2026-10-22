// Broyoo - tiny procedural synth: sound effects + military-suspense music. No SDL / no asset files.
#pragma once
#include <cstdint>

enum Sfx {
    SFX_SHOT, SFX_SHOTGUN, SFX_ROCKET, SFX_FLAME, SFX_RAY, SFX_SLASH, SFX_THROW, SFX_BOOM, SFX_BOOM_SMALL,
    SFX_DEATH, SFX_PDEATH, SFX_JUMP, SFX_ESHOT, SFX_PICKUP, SFX_CAGE, SFX_FLAG, SFX_FANFARE, SFX_MENU,
    SFX_SELECT, SFX_CRASH, SFX_LIGHTNING, SFX_HIT, SFX_SPECIAL, SFX_GASP, SFX_LAUGH, SFX_SCREAM, SFX_HELI,
    SFX_TICK, SFX_IGNITE, SFX_JET, SFX_BOOM_BIG, SFX_BARK, SFX_SILENCED, SFX_POWERSHOT, SFX_LASER, SFX_CHAINSAW, SFX_SPINUP, SFX_RELOAD, SFX_COUNT
};

// spoken phrases (formant synthesis); the game asks for them with sound id 1000 + phrase
enum Speech { SP_GOGOGO, SP_LETSGO, SP_HEREWEGO, SP_LOCKLOAD, SP_NOMERCY, SP_MISSIONFAILED, SP_RIP, SP_NOOO, SP_BRODOWN, SP_NEWBRO, SP_COUNT };
constexpr int SPEECH_BASE = 1000;

constexpr int MUSIC_TRACKS = 8; // 0..3 levels, 4 title / menus, 5 extraction helicopter, 6 boss, 7 credits

void audio_init(int sample_rate);
int audio_load_samples(const char *dir);        // sounds/*.wav (22050 Hz mono 16 bit); returns the file count. Missing ones fall back to the synth
void audio_trigger(int sfx, float vol);        // vol 0..1 (before the user's SFX volume)
void audio_set_volumes(float sfx, float music); // 0..1
void audio_speak(int phrase, float vol);        // formant-synth voice line
void audio_saw(float on);                       // chainsaw engine loop (same behaviour as the flame loop)
void audio_flame(float on);                     // flamethrower loop: starts from the top, runs while on, wraps at the end
void audio_heli(float vol);                     // helicopter rotor loop level 0..1 (smoothed)
int audio_load_music(const char *dir);             // music/{level,menu,heli,boss,credits}.wav (IMA ADPCM); returns how many loaded, 0 = synth music
void audio_music(int track);                    // -1 = stop
void audio_mix(int16_t *out, int frames);       // mono
