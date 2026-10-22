#include "audio.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace {
const int MAXV = 28;
enum { K_SQ, K_NOISE, K_SINE, K_SAW, K_VOICE };
struct Voice {
    bool on = false;
    int kind = 0, shape = 0;
    float f0 = 0, f1 = 0, vol = 0, nk = 0.5f, ph = 0, lp = 0;
    float c1 = 0, c2 = 0, y11 = 0, y12 = 0, y21 = 0, y22 = 0; // two formant resonators (vocal tract)
    int n = 0, len = 0, delay = 0;
};
Voice voices[MAXV];
int g_rate = 22050;
float g_sfx = 0.75f, g_music = 0.5f;
uint32_t g_rng = 12345;
const float PI2 = 6.2831853f;
const float R_FORMANT = 0.955f;

float rnd1() {
    g_rng = g_rng * 1664525u + 1013904223u;
    return float((g_rng >> 9) & 0xFFFF) / 32768.0f - 1.0f;
}

// shape: 0 = sharp attack then decay, 1 = swell (breath in)
void voice(int kind, float f0, float f1, float dur, float vol, float nk = 0.5f, float delay = 0, float fm1 = 0, float fm2 = 0, int shape = 0) {
    int slot = -1, oldest = 0, oldn = -1;
    for (int i = 0; i < MAXV; i++) {
        if (!voices[i].on) { slot = i; break; }
        if (voices[i].n > oldn) { oldn = voices[i].n; oldest = i; }
    }
    if (slot < 0) slot = oldest;
    Voice &v = voices[slot];
    v = Voice();
    v.on = true; v.kind = kind; v.f0 = f0; v.f1 = f1; v.vol = vol; v.nk = nk; v.shape = shape;
    v.len = int(dur * float(g_rate)); v.delay = int(delay * float(g_rate));
    if (fm1 > 0) v.c1 = 2.0f * R_FORMANT * cosf(PI2 * fm1 / float(g_rate));
    if (fm2 > 0) v.c2 = 2.0f * R_FORMANT * cosf(PI2 * fm2 / float(g_rate));
}

// ---- music: military suspense. Staccato low strings, ticking side drum, heartbeat timpani, sparse brass ----
// Melody tokens: >=0 note (semitones above lead base), -1 sustain, -2 rest. 4 bars x 16 sixteenths.
struct Track {
    int bpm, root;                 // root = midi note of the bass key
    int8_t barRoot[4];             // chord root offset (semitones) per bar
    bool barMinor[4];
    int8_t lead[64];
    uint16_t timp, snare;          // 16-step masks (repeat every bar)
    uint8_t crash;                 // bar mask for cymbal swell on beat 1
    uint8_t bassStyle;             // 1: staccato eighths, 2: sparse, 3: nervous sixteenths
};
const Track TRACKS[MUSIC_TRACKS] = {
    // 0 jungle: slow-burn tension, D minor with a flat-two crawl (Dm Eb Bb A)
    {100, 38, {0, 1, -4, -5}, {true, false, false, false},
     {7,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1, 8,-1,-1,-1,   8,-1,-1,-1,-1,-1,-1,-1, 7,-1,-1,-1, 5,-1,-1,-1,
      10,-1,-1,-1,-1,-1,-1,-1, 8,-1,-1,-1, 7,-1,-1,-1,   9,-1,-1,-1,-1,-1,-1,-1, -1,-1,-1,-1, -2,-2,-2,-2},
     0x0101, 0xFFFF, 0x01, 1},
    // 1 desert: phrygian dread (Em F Em D)
    {92, 40, {0, 1, 0, -2}, {true, false, true, false},
     {7,-1,-1,-1, 8,-1,-1,-1, 7,-1,-1,-1, 3,-1,-1,-1,   8,-1,-1,-1,-1,-1,-1,-1, 5,-1,-1,-1, 3,-1,-1,-1,
      7,-1,-1,-1, 8,-1,-1,-1, 10,-1,-1,-1, 8,-1,-1,-1,   7,-1,-1,-1,-1,-1,-1,-1, 3,-1,-1,-1,-2,-2,-2,-2},
     0x0101, 0xAAAA, 0x01, 1},
    // 2 night base: cold and sparse, heartbeat timpani (Cm Cm Ab G)
    {84, 36, {0, 0, -4, -5}, {true, true, false, false},
     {-2,-2,-2,-2,-2,-2,-2,-2, 15,-1,-1,-1,-1,-1,-1,-1,   -2,-2,-2,-2,-2,-2,-2,-2, 14,-1,-1,-1,-1,-1,-1,-1,
      12,-1,-1,-1,-1,-1,-1,-1, 11,-1,-1,-1,-1,-1,-1,-1,   14,-1,-1,-1, 12,-1,-1,-1, 11,-1,-1,-1,-2,-2,-2,-2},
     0x0141, 0x0000, 0x01, 2},
    // 3 hell / boss: relentless attack (Bbm Gb Ab Bbm)
    {120, 34, {0, -4, -2, 0}, {true, false, false, true},
     {15,-1, 15, 15, -1,-1, 13,-1, 15,-1, 18,-1, 17,-1,-1,-1,   13,-1, 13, 13, -1,-1, 11,-1, 13,-1, 16,-1, 15,-1,-1,-1,
      11,-1, 11, 11, -1,-1, 10,-1, 11,-1, 15,-1, 13,-1,-1,-1,   15,-1, 18, 15, 17,-1, 18,-1, 20,-1, 18,-1, 15,-1,-1,-2},
     0x1111, 0xFFFF, 0x0F, 3},
    // 4 title: grave and serious, brass theme over a march (Dm Bb Gm A)
    {88, 38, {0, -4, -7, -5}, {true, false, true, false},
     {10,-1,-1,-1,-1,-1, 9,-1, 10,-1,-1,-1, 14,-1,-1,-1,   12,-1,-1,-1,-1,-1, 10,-1, 12,-1,-1,-1, 15,-1,-1,-1,
      14,-1,-1,-1,-1,-1, 12,-1, 14,-1,-1,-1, 17,-1,-1,-1,   16,-1,-1,-1,-1,-1, 14,-1, 12,-1,-1,-1,-2,-2,-2,-2},
     0x0101, 0x1414, 0x01, 1},
    // 5 extraction / victory: relief but still earnest (F Bb C F)
    {108, 41, {0, 5, 7, 0}, {false, false, false, false},
     {12,-1,-1,-1,-1,-1, 14,-1, 16,-1,-1,-1, 17,-1,-1,-1,   17,-1,-1,-1,-1,-1, 16,-1, 14,-1,-1,-1, 12,-1,-1,-1,
      14,-1,-1,-1,-1,-1, 16,-1, 17,-1,-1,-1, 19,-1,-1,-1,   21,-1,-1,-1,-1,-1, 19,-1, 17,-1,-1,-1, 17,-1,-1,-1},
     0x1111, 0x1414, 0x05, 1},
};

struct Music {
    int track = -1, step = 0, stepSamples = 0, stepCount = 0;
    float padPh[3] = {0, 0, 0}, padF[3] = {0, 0, 0}, padEnv = 0, padLp = 0;
    float bassPh = 0, bassF = 0, bassEnv = 0, bassLp = 0, bassDecay = 0.9990f;
    float leadPh[2] = {0, 0}, leadF = 0, leadEnv = 0, leadAge = 0, leadLp = 0;
    bool leadOn = false;
    float timpPh = 0, timpEnv = 0, snareEnv = 0, cymEnv = 0, snareLp = 0, cymLp = 0;
} M;

float midi_f(int m) { return 440.0f * powf(2.0f, float(m - 69) / 12.0f); }

void music_step() {
    const Track &t = TRACKS[M.track];
    int s = M.step & 15, bar = (M.step >> 4) & 3, l = M.step & 63;
    int rootNote = t.root + t.barRoot[bar];
    if (s == 0) { // low string pad swells in on each bar
        M.padF[0] = midi_f(rootNote + 24);
        M.padF[1] = midi_f(rootNote + 24 + (t.barMinor[bar] ? 3 : 4));
        M.padF[2] = midi_f(rootNote + 24 + 7);
        M.padEnv = 0.0001f;
    }
    int8_t b = -1;
    if (t.bassStyle == 1) { if (s % 2 == 0) b = (s == 14) ? 1 : 0; }              // staccato eighths with a minor-second sting
    else if (t.bassStyle == 2) { if (s == 0 || s == 10) b = 0; }
    else if (t.bassStyle == 3) { if (s != 3 && s != 11) b = (s == 7 || s == 15) ? 1 : 0; } // nervous sixteenths
    if (b >= 0) { M.bassF = midi_f(rootNote + b); M.bassEnv = (s % 4 == 0) ? 1.0f : 0.7f; }
    M.bassDecay = t.bassStyle == 2 ? 0.99985f : 0.9990f;
    int8_t n = t.lead[l];
    if (n >= 0) { M.leadF = midi_f(t.root + 24 + n); M.leadEnv = 0.0001f; M.leadAge = 0; M.leadOn = true; }
    else if (n == -2) M.leadOn = false;
    if (t.timp & (1u << s)) M.timpEnv = 1.0f;
    if (t.snare & (1u << s)) M.snareEnv = (s % 4 == 0) ? 1.0f : 0.33f; // ticking side drum, accented on the beat
    if (s == 0 && (t.crash & (1u << bar))) M.cymEnv = 1.0f;
    M.step++;
}

float synth_sample() {
    if (M.track < 0) return 0;
    if (M.stepCount-- <= 0) { M.stepCount = M.stepSamples; music_step(); }
    float out = 0;
    M.padEnv += (0.8f - M.padEnv) * 0.00012f; // slow swell
    float pad = 0;
    for (int i = 0; i < 3; i++) {
        M.padPh[i] += M.padF[i] * (1.0f + 0.003f * float(i - 1)) / float(g_rate);
        if (M.padPh[i] > 1) M.padPh[i] -= 1;
        pad += M.padPh[i] * 2.0f - 1.0f;
    }
    M.padLp += (pad - M.padLp) * 0.07f;
    out += M.padLp * M.padEnv * 0.13f;
    M.bassPh += M.bassF / float(g_rate);
    if (M.bassPh > 1) M.bassPh -= 1;
    M.bassLp += ((M.bassPh * 2.0f - 1.0f) - M.bassLp) * 0.10f;
    out += M.bassLp * M.bassEnv * 0.40f;
    M.bassEnv *= M.bassDecay;
    if (M.leadOn) {
        M.leadAge += 1.0f / float(g_rate);
        M.leadEnv += (0.9f - M.leadEnv) * 0.0009f;
    } else M.leadEnv *= 0.9985f;
    float vib = 1.0f + (M.leadAge > 0.3f ? 0.005f * sinf(M.leadAge * 28.0f) : 0.0f);
    M.leadPh[0] += M.leadF * vib / float(g_rate);
    M.leadPh[1] += M.leadF * vib * 1.003f / float(g_rate);
    for (float &p : M.leadPh) if (p > 1) p -= 1;
    float brass = (M.leadPh[0] * 2.0f - 1.0f) + (M.leadPh[1] * 2.0f - 1.0f);
    M.leadLp += (brass - M.leadLp) * (0.07f + 0.08f * M.leadEnv); // dark horn
    out += M.leadLp * M.leadEnv * 0.24f;
    M.timpPh += (48.0f + 36.0f * M.timpEnv * M.timpEnv) / float(g_rate);
    if (M.timpPh > 1) M.timpPh -= 1;
    out += sinf(M.timpPh * PI2) * M.timpEnv * 0.8f;
    M.timpEnv *= 0.99950f;
    M.snareLp += (rnd1() - M.snareLp) * 0.6f;
    out += M.snareLp * M.snareEnv * 0.26f;
    M.snareEnv *= 0.9955f;
    M.cymLp += (rnd1() - M.cymLp) * 0.9f;
    out += M.cymLp * M.cymEnv * 0.12f;
    M.cymEnv *= 0.99988f;
    return out;
}

// ---- recorded music: mono IMA-ADPCM wav files kept in memory, decoded block by block and resampled ----
// track ids: 0..3 levels, 4 title / menus, 5 extraction helicopter, 6 boss, 7 credits
const char *const MUSIC_FILES[5] = {"level", "menu", "heli", "boss", "credits"};
int music_file_of(int track) { return track >= 0 && track <= 3 ? 0 : track == 4 ? 1 : track == 5 ? 2 : track == 6 ? 3 : track == 7 ? 4 : -1; }
int synth_of(int track) { return track == 6 ? 3 : track == 7 ? 5 : track; }

struct MusFile {
    std::vector<uint8_t> d;
    size_t dataOff = 0, dataLen = 0;
    int rate = 22050, blockAlign = 0, spb = 0, block = 0, bufPos = 0, bufN = 0;
    int16_t buf[4096];
    float prev = 0, cur = 0, frac = 0;
    bool ok = false;
} MF[5];
MusFile *curFile = nullptr;
const int IMA_STEP[89] = {7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767};
const int IMA_IDX[16] = {-1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8};

bool load_music_file(const char *path, MusFile &m) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz < 64) { fclose(f); return false; }
    m.d.resize(size_t(sz));
    size_t got = fread(m.d.data(), 1, size_t(sz), f);
    fclose(f);
    if (got != size_t(sz) || memcmp(m.d.data(), "RIFF", 4) || memcmp(m.d.data() + 8, "WAVE", 4)) return false;
    auto u16 = [&](size_t o) { return int(m.d[o]) | (int(m.d[o + 1]) << 8); };
    auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
    bool gotFmt = false;
    for (size_t o = 12; o + 8 <= size_t(sz);) {
        size_t len = size_t(u32(o + 4));
        if (!memcmp(&m.d[o], "fmt ", 4) && o + 8 + 20 <= size_t(sz)) {
            if (u16(o + 8) != 0x11 || u16(o + 10) != 1) return false; // IMA ADPCM, mono only
            m.rate = u32(o + 12); m.blockAlign = u16(o + 20); m.spb = u16(o + 26); gotFmt = true;
        } else if (!memcmp(&m.d[o], "data", 4)) {
            m.dataOff = o + 8; m.dataLen = std::min(len, size_t(sz) - (o + 8));
        }
        o += 8 + len + (len & 1);
    }
    if (!gotFmt || !m.dataOff || m.blockAlign < 8 || m.spb < 1 || m.spb > 4096 || m.rate < 4000) return false;
    m.ok = true;
    return true;
}

void music_decode_block(MusFile &m) {
    size_t nb = m.dataLen / size_t(m.blockAlign);
    if (nb == 0) { m.bufN = 0; return; }
    if (size_t(m.block) >= nb) m.block = 0; // loop
    const uint8_t *p = &m.d[m.dataOff + size_t(m.block) * size_t(m.blockAlign)];
    m.block++;
    int pred = int16_t(p[0] | (p[1] << 8)), idx = std::min(88, int(p[2]));
    int n = 0;
    m.buf[n++] = int16_t(pred);
    for (int i = 4; i < m.blockAlign && n < m.spb; i++)
        for (int h = 0; h < 2 && n < m.spb; h++) {
            int nib = h ? (p[i] >> 4) : (p[i] & 15);
            int step = IMA_STEP[idx], diff = step >> 3;
            if (nib & 1) diff += step >> 2;
            if (nib & 2) diff += step >> 1;
            if (nib & 4) diff += step;
            pred += (nib & 8) ? -diff : diff;
            pred = pred > 32767 ? 32767 : pred < -32768 ? -32768 : pred;
            idx += IMA_IDX[nib]; idx = idx < 0 ? 0 : idx > 88 ? 88 : idx;
            m.buf[n++] = int16_t(pred);
        }
    m.bufN = n; m.bufPos = 0;
}

float music_file_sample(MusFile &m) {
    m.frac += float(m.rate) / float(g_rate);
    while (m.frac >= 1.0f) {
        m.frac -= 1.0f;
        if (m.bufPos >= m.bufN) music_decode_block(m);
        m.prev = m.cur;
        m.cur = m.bufPos < m.bufN ? float(m.buf[m.bufPos++]) * (1.0f / 32768.0f) : 0.0f;
    }
    return m.prev + (m.cur - m.prev) * m.frac;
}

// one track is playing (g_want = the one the game asks for); a change fades the old one out, then starts the new one in
int g_want = -1;
float g_fade = 0;
float music_sample() {
    if (g_want != M.track) {
        if (M.track < 0 || g_fade <= 0.001f) {
            M = Music();
            curFile = nullptr;
            g_fade = 0;
            if (g_want >= 0 && g_want < MUSIC_TRACKS) {
                int fi = music_file_of(g_want);
                if (fi >= 0 && MF[fi].ok) { curFile = &MF[fi]; curFile->block = 0; curFile->bufPos = curFile->bufN = 0; curFile->frac = 1.0f; curFile->prev = curFile->cur = 0; }
                int st = synth_of(g_want);
                M.stepSamples = int(float(g_rate) * 60.0f / float(TRACKS[st].bpm) / 4.0f);
                M.track = g_want;
            }
        } else g_fade -= 1.0f / (float(g_rate) * (g_want == 5 ? 1.0f : 0.3f)); // heli: one second fade-out of the level music
    } else if (g_fade < 1.0f) g_fade = std::min(1.0f, g_fade + 1.0f / (float(g_rate) * 0.3f));
    if (M.track < 0) return 0;
    if (curFile) return music_file_sample(*curFile) * g_fade * 2.2f;
    return synth_sample() * g_fade;
}

// ---- speech: a tiny formant synthesizer, fed with phoneme strings ---------------------------
// Deliberately low, raspy and mumbled: it is meant to sound like a tough action hero muttering.
struct Ph { const char *n; int kind; float dur, f1, f2, f3, g1, g2, g3, nf, amp; };
// kind: 0 vowel, 1 glide/liquid, 2 nasal, 3 fricative, 4 voiced stop, 5 unvoiced stop, 6 pause
const Ph PH[] = {
    {"aa", 0, 130, 730, 1090, 2440, 730, 1090, 2440, 0, 1.0f},
    {"ae", 0, 130, 660, 1720, 2410, 660, 1720, 2410, 0, 1.0f},
    {"eh", 0, 110, 530, 1840, 2480, 530, 1840, 2480, 0, 1.0f},
    {"ih", 0, 90, 390, 1990, 2550, 390, 1990, 2550, 0, 1.0f},
    {"iy", 0, 130, 270, 2290, 3010, 270, 2290, 3010, 0, 1.0f},
    {"ah", 0, 100, 640, 1190, 2390, 640, 1190, 2390, 0, 1.0f},
    {"uw", 0, 130, 300, 870, 2240, 300, 870, 2240, 0, 1.0f},
    {"ow", 0, 190, 570, 840, 2410, 400, 760, 2300, 0, 1.0f},
    {"ey", 0, 170, 480, 1900, 2500, 300, 2100, 2900, 0, 1.0f},
    {"ay", 0, 200, 730, 1090, 2440, 400, 1900, 2600, 0, 1.0f},
    {"aw", 0, 190, 730, 1090, 2440, 450, 900, 2300, 0, 1.0f},
    {"er", 0, 120, 490, 1350, 1690, 490, 1350, 1690, 0, 1.0f},
    {"w", 1, 60, 290, 610, 2150, 430, 900, 2300, 0, 0.9f},
    {"r", 1, 70, 310, 1060, 1380, 350, 1200, 1600, 0, 0.9f},
    {"l", 1, 70, 360, 1300, 2700, 400, 1400, 2600, 0, 0.9f},
    {"y", 1, 55, 260, 2070, 3020, 300, 2000, 2800, 0, 0.9f},
    {"m", 2, 75, 250, 1100, 2100, 250, 1100, 2100, 0, 0.5f},
    {"n", 2, 70, 250, 1500, 2300, 250, 1500, 2300, 0, 0.5f},
    {"s", 3, 100, 0, 0, 0, 0, 0, 0, 5500, 0.5f},
    {"sh", 3, 110, 0, 0, 0, 0, 0, 0, 3000, 0.6f},
    {"f", 3, 70, 0, 0, 0, 0, 0, 0, 3800, 0.18f},
    {"h", 3, 60, 500, 1500, 2500, 500, 1500, 2500, 0, 0.5f},
    {"g", 4, 50, 0, 0, 0, 0, 0, 0, 1800, 0.45f},
    {"b", 4, 50, 0, 0, 0, 0, 0, 0, 800, 0.45f},
    {"d", 4, 50, 0, 0, 0, 0, 0, 0, 3000, 0.45f},
    {"p", 5, 60, 0, 0, 0, 0, 0, 0, 900, 0.5f},
    {"t", 5, 60, 0, 0, 0, 0, 0, 0, 3500, 0.5f},
    {"k", 5, 65, 0, 0, 0, 0, 0, 0, 2000, 0.5f},
    {".", 6, 120, 0, 0, 0, 0, 0, 0, 0, 0},
};
const int NPH = int(sizeof(PH) / sizeof(PH[0]));

struct Seg { int n; float f1a, f2a, f3a, f1b, f2b, f3b, voice, breath, fric, nf, f0a, f0b; };
const int MAXSEG = 200;
struct SpeechState {
    bool on = false;
    Seg segs[MAXSEG];
    int nseg = 0, seg = 0, n = 0, cnt = 0;
    float ph = 0, vol = 1;
    float f[3] = {500, 1500, 2500}, c[3] = {0, 0, 0}, y1[3] = {0, 0, 0}, y2[3] = {0, 0, 0};
    float v = 0, br = 0, fr = 0, nfS = 3000, cn = 0, yn1 = 0, yn2 = 0;
} S;

const char *PHRASES[SP_COUNT] = {
    "^g ow . ^g ow . ^g ow",
    "^l eh t s . g ow . ^b r ow",
    "h iy r . w iy . ^g ow",
    "^l aa k . ae n d . ^l ow d",
    "^n ow . ^m er s iy",
    "^m ih sh ah n . ^f ey l d",
    "^r eh s t . ih n . ^p iy s . ^b r ow",
    "^n ow*",
    "^b r ow . ^d aw n",
    "^n y uw . ^b r ow",
};

void speech_add(const Seg &s) { if (S.nseg < MAXSEG) S.segs[S.nseg++] = s; }

void speech_build(const char *text) {
    S.nseg = 0;
    // count tokens for the pitch declination
    int total = 0;
    for (const char *p = text; *p; p++) if (*p == ' ') total++;
    total++;
    int idx = 0;
    char tok[16];
    const char *p = text;
    while (*p) {
        while (*p == ' ') p++;
        int k = 0;
        while (*p && *p != ' ' && k < 15) tok[k++] = *p++;
        tok[k] = 0;
        if (!k) break;
        bool stress = false, stretch = false;
        char *name = tok;
        if (*name == '^') { stress = true; name++; }
        int len = int(strlen(name));
        if (len && name[len - 1] == '*') { stretch = true; name[len - 1] = 0; }
        const Ph *ph = nullptr;
        for (int i = 0; i < NPH; i++) if (!strcmp(PH[i].n, name)) { ph = &PH[i]; break; }
        float pos = float(idx) / float(total > 1 ? total - 1 : 1);
        idx++;
        if (!ph) continue;
        float f0 = 122.0f - 24.0f * pos + (stress ? 11.0f : 0.0f);
        float dur = ph->dur * (stress ? 1.15f : 1.0f) * (stretch ? 4.5f : 1.0f) * 0.95f;
        Seg s = {};
        s.nf = ph->nf;
        s.f0a = f0; s.f0b = stretch ? f0 * 0.6f : f0 * 0.96f;
        auto samples = [](float ms) { return int(ms / 1000.0f * float(g_rate)); };
        if (ph->kind <= 2) {
            s.n = samples(dur);
            s.f1a = ph->f1; s.f2a = ph->f2; s.f3a = ph->f3; s.f1b = ph->g1; s.f2b = ph->g2; s.f3b = ph->g3;
            s.voice = ph->amp; s.breath = ph->kind == 0 ? 0.14f : 0.08f;
            speech_add(s);
        } else if (ph->kind == 3) {
            s.n = samples(dur);
            s.f1a = s.f1b = ph->f1; s.f2a = s.f2b = ph->f2; s.f3a = s.f3b = ph->f3;
            if (ph->f1 > 0) { s.breath = ph->amp; } else { s.fric = ph->amp; }
            speech_add(s);
        } else if (ph->kind == 4 || ph->kind == 5) {
            Seg c = s; // closure
            c.n = samples(dur * 0.65f);
            c.f1a = c.f1b = 250; c.f2a = c.f2b = 900; c.f3a = c.f3b = 2200;
            c.voice = ph->kind == 4 ? 0.22f : 0.0f;
            speech_add(c);
            Seg b = s; // burst
            b.n = samples(14);
            b.f1a = b.f1b = 500; b.f2a = b.f2b = 1500; b.f3a = b.f3b = 2500;
            b.fric = ph->amp; b.voice = ph->kind == 4 ? 0.3f : 0.0f;
            speech_add(b);
            if (ph->kind == 5) { // aspiration after unvoiced stops
                Seg a = b;
                a.n = samples(22); a.fric = 0; a.breath = 0.35f; a.voice = 0.0f;
                speech_add(a);
            }
        } else {
            s.n = samples(dur);
            speech_add(s);
        }
    }
}

float speech_sample() {
    if (!S.on) return 0;
    const Seg &sg = S.segs[S.seg];
    float t = float(S.n) / float(sg.n > 0 ? sg.n : 1);
    float tf[3] = {sg.f1a + (sg.f1b - sg.f1a) * t, sg.f2a + (sg.f2b - sg.f2a) * t, sg.f3a + (sg.f3b - sg.f3a) * t};
    float f0 = sg.f0a + (sg.f0b - sg.f0a) * t;
    for (int i = 0; i < 3; i++) if (tf[i] > 0) S.f[i] += (tf[i] - S.f[i]) * 0.004f; // coarticulation: glide between targets
    S.v += (sg.voice - S.v) * 0.02f;
    S.br += (sg.breath - S.br) * 0.03f;
    S.fr += (sg.fric - S.fr) * 0.06f;
    if (sg.nf > 0) S.nfS += (sg.nf - S.nfS) * 0.05f;
    if ((S.cnt++ & 15) == 0) {
        const float rr[3] = {0.955f, 0.955f, 0.93f};
        for (int i = 0; i < 3; i++) S.c[i] = 2.0f * rr[i] * cosf(PI2 * S.f[i] / float(g_rate));
        S.cn = 2.0f * 0.9f * cosf(PI2 * S.nfS / float(g_rate));
    }
    float fj = f0 * (1.0f + 0.012f * sinf(float(S.cnt) * 0.0021f) + 0.012f * rnd1()); // hoarse jitter
    S.ph += fj / float(g_rate);
    if (S.ph > 1) S.ph -= 1;
    float pulse = S.ph < 0.1f ? 1.0f : 0.0f;
    float exc = pulse * S.v * 1.6f + rnd1() * (S.br + 0.22f * S.v) * 0.9f; // + raspiness
    const float rr[3] = {0.955f, 0.955f, 0.93f};
    float out = 0, wgt[3] = {1.0f, 0.65f, 0.3f};
    for (int i = 0; i < 3; i++) {
        float y = (1.0f - rr[i]) * exc + S.c[i] * S.y1[i] - rr[i] * rr[i] * S.y2[i];
        S.y2[i] = S.y1[i]; S.y1[i] = y;
        out += y * wgt[i];
    }
    float nz = rnd1() * S.fr;
    float yn = (1.0f - 0.9f) * nz + S.cn * S.yn1 - 0.81f * S.yn2;
    S.yn2 = S.yn1; S.yn1 = yn;
    out = out * 0.30f + yn * 0.55f;
    if (++S.n >= sg.n) {
        S.n = 0;
        if (++S.seg >= S.nseg) { S.on = false; }
    }
    return tanhf(out * 1.3f) * 0.8f * S.vol; // soft limiter: glottal spikes never clip
}

// ---- sample bank: recorded sounds (CC0) played on top of / instead of the synth ----
struct Sample { int16_t *d = nullptr; int n = 0; };
const int MAXVAR = 6, MAXSV = 14;
const char *SFX_NAMES[SFX_COUNT] = {
    "shot", "shotgun", "rocket", "flame", "ray", "slash", "throw", "boom", "boom_small",
    "death", "pdeath", "jump", "eshot", "pickup", "cage", "flag", "fanfare", "menu",
    "select", "crash", "lightning", "hit", "special", "gasp", "laugh", "scream", "heli",
    "tick", "ignite", "jet", "boom_big", "bark", "silenced", "powershot", "laser", "chainsaw", "spinup", "reload"};
const char *SP_NAMES[SP_COUNT] = {"gogogo", "letsgo", "herewego", "lockload", "nomercy", "failed", "rip", "nooo", "brodown", "newbro"};
// per-sound gain after the files' own normalisation (frequent sounds quieter, booms and voices louder)
const float SFX_GAIN[SFX_COUNT] = {
    0.55f, 0.85f, 1.4f, 1.1f, 1, 0.6f, 0.5f, 1.0f, 0.9f,
    0.8f, 0.95f, 0.35f, 0.5f, 0.6f, 0.7f, 0.6f, 1, 0.5f,
    0.6f, 0.9f, 0.7f, 0.4f, 1, 0.8f, 0.85f, 0.9f, 1.0f,
    1, 0.6f, 0.5f, 1.0f, 0.8f, 0.45f, 0.6f, 0.8f, 0.5f, 0.8f, 0.8f};
Sample bank[SFX_COUNT][MAXVAR];
int bankN[SFX_COUNT];
Sample spBank[SP_COUNT];
struct SVoice { const Sample *s = nullptr; int pos = 0; float vol = 0; };
SVoice sv[MAXSV];
Sample heliLoop;
float heliTarget = 0, heliCur = 0, heliPh = 0;
int heliPos = 0;
float flameTarget = 0, flameCur = 0;
float sawTarget = 0, sawCur = 0;
int sawPos = 0;
float sawPh = 0;
int flamePos = 0;

void sv_play(const Sample *s, float vol) {
    int slot = -1, best = -1;
    for (int i = 0; i < MAXSV; i++) {
        if (!sv[i].s) { slot = i; break; }
        if (sv[i].pos > best) { best = sv[i].pos; slot = i; }
    }
    sv[slot].s = s; sv[slot].pos = 0; sv[slot].vol = vol;
}

bool load_wav(const char *path, Sample &out) {
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 44 || sz > 8 * 1024 * 1024) { fclose(f); return false; }
    unsigned char *b = (unsigned char *)malloc(size_t(sz));
    if (!b || fread(b, 1, size_t(sz), f) != size_t(sz)) { free(b); fclose(f); return false; }
    fclose(f);
    bool ok = false;
    if (!memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4)) {
        long p = 12;
        int fmt = 0, ch = 0, bits = 0;
        while (p + 8 <= sz) {
            long cl = b[p + 4] | (b[p + 5] << 8) | (b[p + 6] << 16) | (long(b[p + 7]) << 24);
            if (!memcmp(b + p, "fmt ", 4) && cl >= 16) { fmt = b[p + 8] | (b[p + 9] << 8); ch = b[p + 10] | (b[p + 11] << 8); bits = b[p + 22] | (b[p + 23] << 8); }
            else if (!memcmp(b + p, "data", 4) && fmt == 1 && ch == 1 && bits == 16) {
                long n = cl;
                if (p + 8 + n > sz) n = sz - p - 8;
                out.n = int(n / 2);
                out.d = (int16_t *)malloc(size_t(out.n) * 2);
                if (out.d) { memcpy(out.d, b + p + 8, size_t(out.n) * 2); ok = true; }
                break;
            }
            p += 8 + cl + (cl & 1);
        }
    }
    free(b);
    return ok;
}
} // namespace

int audio_load_samples(const char *dir) {
    int count = 0;
    char path[512];
    for (int s = 0; s < SFX_COUNT; s++) {
        bankN[s] = 0;
        for (int k = 1; k <= MAXVAR; k++) {
            snprintf(path, sizeof(path), "%s/%s_%d.wav", dir, SFX_NAMES[s], k);
            if (load_wav(path, bank[s][bankN[s]])) { bankN[s]++; count++; }
        }
    }
    snprintf(path, sizeof(path), "%s/heli_loop.wav", dir);
    if (load_wav(path, heliLoop)) count++;
    for (int s = 0; s < SP_COUNT; s++) {
        snprintf(path, sizeof(path), "%s/sp_%s.wav", dir, SP_NAMES[s]);
        if (load_wav(path, spBank[s])) count++;
    }
    return count;
}

void audio_init(int rate) { g_rate = rate; }
void audio_saw(float on) {
    if (on > 0 && sawTarget <= 0 && sawCur < 0.02f) sawPos = 0;
    sawTarget = on > 0 ? 1.0f : 0.0f;
}
void audio_flame(float on) {
    if (on > 0 && flameTarget <= 0 && flameCur < 0.02f) flamePos = 0; // a new burst starts at the beginning of the recording
    flameTarget = on > 0 ? 1.0f : 0.0f;
}
void audio_heli(float vol) { heliTarget = vol < 0 ? 0 : vol > 1 ? 1 : vol; }
void audio_set_volumes(float sfx, float music) { g_sfx = sfx; g_music = music; }

void audio_music(int track) { g_want = track; }

int audio_load_music(const char *dir) {
    int n = 0;
    char path[256];
    for (int i = 0; i < 5; i++) {
        snprintf(path, sizeof(path), "%s/%s.wav", dir, MUSIC_FILES[i]);
        if (load_music_file(path, MF[i])) n++;
    }
    return n;
}

void audio_speak(int phrase, float vol) {
    if (phrase < 0 || phrase >= SP_COUNT) return;
    if (spBank[phrase].d) { // recorded line
        sv_play(&spBank[phrase], vol > 1 ? 1.0f : vol);
        return;
    }
    S = SpeechState();
    speech_build(PHRASES[phrase]);
    S.vol = vol > 1 ? 1 : vol;
    S.seg = 0; S.n = 0;
    S.on = S.nseg > 0;
}

void audio_trigger(int s, float vol) {
    if (vol <= 0.01f) return;
    float v = vol > 1 ? 1 : vol;
    if (s >= 0 && s < SFX_COUNT && bankN[s] > 0) {
        if (s == SFX_IGNITE) for (const SVoice &c : sv) if (c.s && c.s == &bank[s][0] && c.pos < c.s->n / 2) return; // one crackle at a time
        g_rng = g_rng * 1664525u + 1013904223u;
        sv_play(&bank[s][(g_rng >> 16) % unsigned(bankN[s])], v * SFX_GAIN[s]);
        return;
    }
    switch (s) {
    case SFX_SHOT: voice(K_NOISE, 0, 0, 0.06f, 0.45f * v, 0.55f); voice(K_SQ, 1000, 220, 0.05f, 0.22f * v); break;
    case SFX_SHOTGUN: voice(K_NOISE, 0, 0, 0.24f, 0.9f * v, 0.35f); voice(K_SQ, 220, 60, 0.15f, 0.4f * v); break;
    case SFX_ROCKET: voice(K_NOISE, 0, 0, 0.35f, 0.5f * v, 0.15f); voice(K_SQ, 320, 110, 0.3f, 0.22f * v); break;
    case SFX_FLAME: voice(K_NOISE, 0, 0, 0.1f, 0.3f * v, 0.3f); break;
    case SFX_RAY: voice(K_SQ, 1700, 300, 0.16f, 0.3f * v); voice(K_SINE, 700, 200, 0.2f, 0.3f * v); break;
    case SFX_SLASH: voice(K_NOISE, 0, 0, 0.12f, 0.4f * v, 0.9f); break;
    case SFX_THROW: voice(K_SQ, 300, 650, 0.08f, 0.2f * v); break;
    case SFX_BOOM: voice(K_NOISE, 0, 0, 0.7f, 1.0f * v, 0.1f); voice(K_SINE, 100, 28, 0.55f, 1.0f * v); break;
    case SFX_BOOM_SMALL: voice(K_NOISE, 0, 0, 0.3f, 0.8f * v, 0.2f); voice(K_SINE, 130, 40, 0.25f, 0.8f * v); break;
    case SFX_BOOM_BIG: // gas bottle: deeper, longer, with a rumble tail
        voice(K_NOISE, 0, 0, 1.1f, 1.0f * v, 0.07f); voice(K_SINE, 85, 22, 0.9f, 1.0f * v); voice(K_NOISE, 0, 0, 0.35f, 0.7f * v, 0.4f);
        break;
    case SFX_DEATH: voice(K_VOICE, 230, 110, 0.22f, 0.9f * v, 0.15f, 0, 560, 920); break; // "ugh"
    case SFX_PDEATH: // "aaargh"
        voice(K_VOICE, 340, 85, 0.55f, 0.95f * v, 0.12f, 0, 720, 1100);
        voice(K_NOISE, 0, 0, 0.18f, 0.35f * v, 0.3f);
        break;
    case SFX_JUMP: voice(K_SQ, 240, 520, 0.1f, 0.18f * v); break;
    case SFX_ESHOT: voice(K_SQ, 520, 240, 0.08f, 0.18f * v); voice(K_NOISE, 0, 0, 0.04f, 0.15f * v, 0.5f); break;
    case SFX_PICKUP: voice(K_SQ, 600, 1200, 0.12f, 0.3f * v); break;
    case SFX_CAGE: voice(K_SQ, 820, 700, 0.12f, 0.3f * v); voice(K_NOISE, 0, 0, 0.12f, 0.4f * v, 0.6f); break;
    case SFX_FLAG: voice(K_SQ, 523, 523, 0.1f, 0.25f * v); voice(K_SQ, 784, 784, 0.18f, 0.25f * v, 0.5f, 0.1f); break;
    case SFX_FANFARE:
        voice(K_SAW, 523, 523, 0.14f, 0.3f * v, 0.25f); voice(K_SAW, 659, 659, 0.14f, 0.3f * v, 0.25f, 0.15f);
        voice(K_SAW, 784, 784, 0.14f, 0.3f * v, 0.25f, 0.3f); voice(K_SAW, 1046, 1046, 0.4f, 0.32f * v, 0.25f, 0.45f);
        break;
    case SFX_MENU: voice(K_SQ, 700, 700, 0.04f, 0.2f * v); break;
    case SFX_SELECT: voice(K_SQ, 500, 900, 0.09f, 0.25f * v); break;
    case SFX_CRASH: voice(K_NOISE, 0, 0, 0.5f, 0.7f * v, 0.15f); voice(K_SINE, 80, 30, 0.3f, 0.6f * v); break;
    case SFX_LIGHTNING: voice(K_NOISE, 0, 0, 0.35f, 0.7f * v, 0.85f); voice(K_SQ, 2200, 90, 0.3f, 0.3f * v); break;
    case SFX_HIT: voice(K_SQ, 220, 110, 0.04f, 0.2f * v); break;
    case SFX_SPECIAL: voice(K_SQ, 400, 900, 0.14f, 0.25f * v); break;
    case SFX_GASP: // breath in: noisy vowel that swells
        voice(K_VOICE, 170, 210, 0.3f, 1.0f * v, 1.0f, 0, 640, 1500, 1);
        break;
    case SFX_LAUGH: // "ha ha ha ha": a deep voiced pulse train through an /a/ vocal tract, pitch falling
        for (int i = 0; i < 4; i++) {
            float d = 0.18f * float(i), f = 175.0f - 14.0f * float(i);
            voice(K_VOICE, f * 1.12f, f * 0.9f, 0.13f, (1.0f - 0.1f * float(i)) * v, 0.22f, d, 760, 1180);
        }
        voice(K_VOICE, 120, 95, 0.22f, 0.8f * v, 0.2f, 0.74f, 700, 1050); // closing "haaa"
        break;
    case SFX_SCREAM: // suicide bomber battle cry
        voice(K_VOICE, 430, 740, 0.6f, 0.9f * v, 0.14f, 0, 800, 1250);
        voice(K_VOICE, 436, 750, 0.6f, 0.6f * v, 0.14f, 0, 780, 1300);
        break;
    case SFX_HELI: voice(K_NOISE, 0, 0, 0.1f, 0.5f * v, 0.1f); voice(K_SINE, 70, 45, 0.1f, 0.5f * v); break;
    case SFX_TICK: voice(K_SQ, 900, 1300, 0.03f, 0.15f * v); break;
    case SFX_IGNITE: voice(K_NOISE, 0, 0, 0.4f, 0.5f * v, 0.3f, 0, 0, 0, 1); break;
    case SFX_JET: voice(K_NOISE, 0, 0, 0.14f, 0.3f * v, 0.4f); break;
    case SFX_POWERSHOT: voice(K_NOISE, 0, 0, 0.2f, 0.7f * v, 0.3f); voice(K_SQ, 700, 140, 0.12f, 0.3f * v); voice(K_SINE, 160, 50, 0.16f, 0.5f * v); break; // heavy-calibre crack
    case SFX_LASER: voice(K_SQ, 1800, 500, 0.12f, 0.25f * v); voice(K_SINE, 900, 300, 0.14f, 0.25f * v); break; // zap (fallback)
    case SFX_SILENCED: voice(K_NOISE, 0, 0, 0.07f, 0.18f * v, 0.2f); voice(K_SINE, 120, 50, 0.05f, 0.14f * v); break; // muffled 'pfft'
    case SFX_BARK: // two gruff barks
        voice(K_VOICE, 330, 190, 0.12f, 1.0f * v, 0.3f, 0, 650, 1100);
        voice(K_VOICE, 320, 180, 0.12f, 0.9f * v, 0.3f, 0.2f, 650, 1100);
        break;
    default: break;
    }
}

void audio_mix(int16_t *out, int frames) {
    for (int i = 0; i < frames; i++) {
        float m = 0;
        for (Voice &v : voices) {
            if (!v.on) continue;
            if (v.delay > 0) { v.delay--; continue; }
            float t = float(v.n) / float(v.len > 0 ? v.len : 1);
            float env = v.shape == 1 ? sinf(3.14159f * t) : (1.0f - t) * (1.0f - t);
            float s;
            if (v.kind == K_NOISE) { v.lp += (rnd1() - v.lp) * v.nk; s = v.lp * 1.6f; }
            else if (v.kind == K_VOICE) {
                float f = (v.f0 + (v.f1 - v.f0) * t) * (1.0f + 0.025f * sinf(float(v.n) * 0.0016f * 22050.0f / float(g_rate) * 9.0f));
                v.ph += f / float(g_rate);
                if (v.ph > 1) v.ph -= 1;
                float pulse = v.ph < 0.09f ? 1.0f : 0.0f;                  // glottal pulse train
                float exc = pulse * (1.0f - v.nk) * 1.5f + rnd1() * v.nk * 0.9f; // nk = breathiness
                float y1 = (1.0f - R_FORMANT) * exc + v.c1 * v.y11 - R_FORMANT * R_FORMANT * v.y12;
                v.y12 = v.y11; v.y11 = y1;
                float y2 = (1.0f - R_FORMANT) * exc + v.c2 * v.y21 - R_FORMANT * R_FORMANT * v.y22;
                v.y22 = v.y21; v.y21 = y2;
                s = (y1 + 0.7f * y2) * 0.35f;
                if (t < 0.06f) env *= t / 0.06f; // avoid a click on the attack
            } else {
                float f = v.f0 + (v.f1 - v.f0) * t;
                v.ph += f / float(g_rate);
                if (v.ph > 1) v.ph -= 1;
                if (v.kind == K_SQ) s = (v.ph < 0.5f ? 1.0f : -1.0f) * 0.6f;
                else if (v.kind == K_SAW) { v.lp += ((v.ph * 2.0f - 1.0f) - v.lp) * v.nk; s = v.lp * 1.4f; }
                else s = sinf(v.ph * PI2);
            }
            m += s * env * v.vol;
            if (++v.n >= v.len) v.on = false;
        }
        m += speech_sample();
        for (SVoice &c : sv) {
            if (!c.s) continue;
            m += float(c.s->d[c.pos]) * (1.0f / 32768.0f) * c.vol;
            if (++c.pos >= c.s->n) c.s = nullptr;
        }
        flameCur += (flameTarget - flameCur) * 0.004f; // ~10 ms ramps: no clicks
        if (flameCur > 0.003f) {
            if (bankN[SFX_FLAME] > 0) {
                const Sample &fs = bank[SFX_FLAME][0];
                m += float(fs.d[flamePos]) * (1.0f / 32768.0f) * flameCur * SFX_GAIN[SFX_FLAME];
                if (++flamePos >= fs.n) flamePos = 0;
            } else m += rnd1() * 0.12f * flameCur; // synth fallback: a hiss
        }
        sawCur += (sawTarget - sawCur) * 0.004f;
        if (sawCur > 0.003f) {
            if (bankN[SFX_CHAINSAW] > 0) {
                const Sample &ss = bank[SFX_CHAINSAW][0];
                m += float(ss.d[sawPos]) * (1.0f / 32768.0f) * sawCur * SFX_GAIN[SFX_CHAINSAW];
                if (++sawPos >= ss.n) sawPos = 0;
            } else { sawPh += 95.0f / float(g_rate); if (sawPh > 1) sawPh -= 1; m += ((sawPh * 2.0f - 1.0f) * 0.25f + rnd1() * 0.1f) * sawCur; } // synth fallback: a buzz
        }
        heliCur += (heliTarget - heliCur) * 0.0008f; // ~60 ms smoothing: no clicks when the level changes
        if (heliCur > 0.003f) {
            if (heliLoop.d) {
                m += float(heliLoop.d[heliPos]) * (1.0f / 32768.0f) * heliCur * 1.15f;
                if (++heliPos >= heliLoop.n) heliPos = 0;
            } else { // fallback without the recording: a synthetic rotor thump
                heliPh += 8.0f / float(g_rate);
                if (heliPh > 1) heliPh -= 1;
                float e = (1.0f - heliPh) * (1.0f - heliPh) * (1.0f - heliPh);
                m += (sinf(heliPh * PI2 * 6.0f) * 0.7f + rnd1() * 0.3f) * e * heliCur * 0.7f;
            }
        }
        m = tanhf(m * 0.9f); // soft limiter: stacked explosions and voices never hard-clip
        m *= g_sfx;
        if (g_music > 0) m += music_sample() * g_music * 0.55f;
        if (m > 1) m = 1;
        if (m < -1) m = -1;
        out[i] = int16_t(m * 26000.0f);
    }
}
