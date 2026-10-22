// Broyoo - SDL 1.2 front-end for the Miyoo Mini+ (Onion OS)
#include "game.h"
#include "audio.h"
#include <SDL/SDL.h>
#include <csignal>
#include <ctime>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

// ---- rumble motor (GPIO 48, active low: "0" = on, "1" = off; same as Onion's rumble.h) ----
static FILE *g_rumble = nullptr;
static bool g_rumble_on = false;

static bool write_file(const char *path, const char *s) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    size_t n = fwrite(s, 1, strlen(s), f);
    fclose(f);
    return n == strlen(s);
}

static void rumble_init() {
    bool ex = write_file("/sys/class/gpio/export", "48"); // fails harmlessly if already exported
    bool dir = write_file("/sys/class/gpio/gpio48/direction", "out");
    g_rumble = fopen("/sys/class/gpio/gpio48/value", "w");
    std::cerr << "rumble: export=" << ex << " direction=" << dir << " value_fd=" << (g_rumble ? "open" : "FAILED") << std::endl;
    if (g_rumble) { fputs("1", g_rumble); fflush(g_rumble); }
}

static void rumble_set(bool on) {
    if (!g_rumble || on == g_rumble_on) return;
    g_rumble_on = on;
    fputs(on ? "0" : "1", g_rumble);
    fflush(g_rumble);
}

static void rumble_off_now() {
    if (g_rumble) { fputs("1", g_rumble); fflush(g_rumble); }
}

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return double(ts.tv_sec) * 1000.0 + double(ts.tv_nsec) / 1e6;
}

static void audio_cb(void *, Uint8 *stream, int len) {
    audio_mix(reinterpret_cast<int16_t *>(stream), len / 2);
}

static void on_signal(int) {
    rumble_off_now(); // never leave the motor running if we get killed
    _exit(0);
}

// ---- settings file ----
static void load_settings(Game &g) {
    FILE *f = fopen("broforcelite.cfg", "r");
    if (!f) f = fopen("broyoo.cfg", "r"); // settings of the old name
    if (!f) return;
    char k[32];
    int v, ver = 1;
    while (fscanf(f, "%31[^=]=%d\n", k, &v) == 2) {
        if (!strcmp(k, "shake")) g.st.shake = (v / 25) * 25 < 0 ? 0 : (v / 25) * 25 > 100 ? 100 : (v / 25) * 25;
        else if (!strcmp(k, "vib")) g.st.vib = (v / 25) * 25 < 0 ? 0 : (v / 25) * 25 > 100 ? 100 : (v / 25) * 25;
        else if (!strcmp(k, "sfx")) g.st.sfx = (v / 25) * 25 < 0 ? 0 : (v / 25) * 25 > 100 ? 100 : (v / 25) * 25;
        else if (!strcmp(k, "music")) g.st.music = (v / 25) * 25 < 0 ? 0 : (v / 25) * 25 > 100 ? 100 : (v / 25) * 25;
        else if (!strcmp(k, "flip")) g.st.flip = v != 0;
        else if (!strcmp(k, "ver")) ver = v;
        else if (!strcmp(k, "unlocked")) g.set_progress(v); // old single-save files
        else if (!strcmp(k, "slot")) g.st.lastSlot = v < 0 ? 0 : v > 3 ? 3 : v;
        else if (k[0] == 's' && k[1] >= '0' && k[1] <= '3' && k[2] == '_') {
            SaveSlot &sl = g.st.slots[k[1] - '0'];
            if (!strcmp(k + 3, "used")) sl.used = v;
            else if (!strcmp(k + 3, "mask")) sl.mask = v;
            else if (!strcmp(k + 3, "kills")) sl.kills = v;
            else if (!strcmp(k + 3, "sec")) sl.seconds = v;
            else if (!strcmp(k + 3, "bros")) sl.broMask = unsigned(v) | 1u;
        }
    }
    fclose(f);
    if (ver < 2) // 6 old levels -> 6 worlds of 4 levels: a cleared old level means that whole world is cleared
        for (SaveSlot &sl : g.st.slots) {
            int m = 0;
            for (int i = 0; i < 6; i++) if (sl.mask & (1 << i)) m |= 0xF << (4 * i);
            sl.mask = m;
        }
}

static void save_settings(const Game &g) {
    FILE *f = fopen("broforcelite.cfg", "w");
    if (f) fprintf(f, "ver=2\n");
    if (!f) return;
    fprintf(f, "shake=%d\nvib=%d\nsfx=%d\nmusic=%d\nflip=%d\nslot=%d\n", g.st.shake, g.st.vib, g.st.sfx, g.st.music, g.st.flip ? 1 : 0, g.st.lastSlot);
    for (int i = 0; i < 4; i++) {
        const SaveSlot &sl = g.st.slots[i];
        fprintf(f, "s%d_used=%d\ns%d_mask=%d\ns%d_kills=%d\ns%d_sec=%d\ns%d_bros=%d\n", i, sl.used, i, sl.mask, i, sl.kills, i, sl.seconds, i, int(sl.broMask));
    }
    fclose(f);
}

int main() {
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);
    atexit(rumble_off_now);

    bool haveAudio = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0;
    if (!haveAudio && SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    SDL_ShowCursor(SDL_DISABLE);
    const int PW = 640, PH = 480;
    // Same video path as the working pixel-reader app: hardware video surface, software scratch buffer
    // blitted onto it (blitting a hardware surface directly applies an unwanted flip on this device).
    SDL_Surface *video = SDL_SetVideoMode(PW, PH, 32, SDL_HWSURFACE);
    SDL_Surface *scratch = SDL_CreateRGBSurface(SDL_SWSURFACE, PW, PH, 32, 0, 0, 0, 0);
    if (!video || !scratch) {
        std::cerr << "video init failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    // Detect the scratch surface's channel order so colors are right whatever the SDL defaults are.
    uint32_t red = SDL_MapRGB(scratch->format, 255, 0, 0);
    bool swap_rb = red != 0x00FF0000u;
    std::cerr << "scratch pixel format: Rmask=" << std::hex << scratch->format->Rmask << " Gmask=" << scratch->format->Gmask
              << " Bmask=" << scratch->format->Bmask << " map(red)=" << red << std::dec << " bpp=" << int(scratch->format->BitsPerPixel)
              << " pitch=" << scratch->pitch << " swap_rb=" << swap_rb << std::endl;

    rumble_init();

    if (haveAudio) {
        SDL_AudioSpec want, got;
        memset(&want, 0, sizeof(want));
        want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024; want.callback = audio_cb;
        if (SDL_OpenAudio(&want, &got) == 0) {
            audio_init(got.freq);
            int nmus = audio_load_music("music");
            std::cerr << "music files loaded: " << nmus << "/5" << std::endl;
            int nsamp = audio_load_samples("sounds");
            std::cerr << "audio: " << nsamp << " sample files loaded" << std::endl;
            SDL_PauseAudio(0);
            std::cerr << "audio: opened " << got.freq << " Hz, " << int(got.channels) << " ch, format " << got.format << ", buffer " << got.samples << std::endl;
        } else {
            haveAudio = false;
            std::cerr << "audio: SDL_OpenAudio failed: " << SDL_GetError() << " (continuing silently)" << std::endl;
        }
    } else std::cerr << "audio: SDL audio init failed (continuing silently)" << std::endl;

    Game *game = new Game();
    load_settings(*game);
    if (FILE *df = fopen("debug.txt", "r")) { fclose(df); game->set_debug(true); std::cerr << "DEBUG MODE (debug.txt found)" << std::endl; }
    game->seed_random(SDL_GetTicks());
    Gfx *gfx = new Gfx();
    std::cerr << "settings: shake=" << game->st.shake << " vib=" << game->st.vib << " flip=" << game->st.flip << std::endl;

    bool keys[SDLK_LAST];
    memset(keys, 0, sizeof(keys));
    bool running = true;
    const double STEP_MS = 1000.0 / 62.5; // simulation rate
    double acc = 0;
    double last = now_ms();
    double fpsT = last;
    int frames = 0, updates = 0;
    struct Stat { double sum = 0, mx = 0; void add(double v) { sum += v; if (v > mx) mx = v; } void reset() { sum = mx = 0; } };
    Stat sUpd, sDraw, sPres, sAud, sFrame;
    float duty = 0;
    static uint32_t row[PW];
    double lastFrame = last;

    while (running && !game->want_quit) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                if (e.key.keysym.sym < SDLK_LAST) keys[e.key.keysym.sym] = e.type == SDL_KEYDOWN;
            }
        }
        Input in;
        in.left = keys[SDLK_LEFT]; in.right = keys[SDLK_RIGHT]; in.up = keys[SDLK_UP]; in.down = keys[SDLK_DOWN];
        in.jump = keys[SDLK_SPACE];                         // A
        in.fire = keys[SDLK_LCTRL] || keys[SDLK_t];         // B or R1
        in.special = keys[SDLK_LSHIFT] || keys[SDLK_e];     // X or L1
        in.melee = keys[SDLK_LALT];                         // Y
        in.start = keys[SDLK_RETURN] || keys[SDLK_ESCAPE];  // START or MENU
        in.select = keys[SDLK_RCTRL];

        double now = now_ms();
        acc += now - last;
        last = now;
        if (acc < STEP_MS) { // nothing new to show yet: sleep instead of redrawing an identical frame
            double wait = STEP_MS - acc;
            if (wait >= 1.0) SDL_Delay(Uint32(wait));
            continue;
        }
        if (acc > 250) acc = 250; // don't spiral if the device stalls
        double t0 = now_ms();
        while (acc >= STEP_MS) {
            game->update(in);
            acc -= STEP_MS;
            updates++;
            // sigma-delta duty cycle: the motor is on/off only, so intensity = fraction of frames on
            duty += game->rumble_level();
            if (duty >= 1.0f) { duty -= 1.0f; rumble_set(true); } else rumble_set(false);
        }
        double t1 = now_ms();
        if (haveAudio) {
            SDL_LockAudio();
            audio_set_volumes(float(game->st.sfx) / 100.0f, float(game->st.music) / 100.0f);
            audio_music(game->music_track());
            audio_heli(game->heli_volume());
            for (const SndEvt &se : game->sounds) {
                if (se.id >= SPEECH_BASE) audio_speak(se.id - SPEECH_BASE, se.vol);
                else audio_trigger(se.id, se.vol);
            }
            SDL_UnlockAudio();
        }
        game->sounds.clear();
        double t2 = now_ms();
        game->render(*gfx);
        double t3 = now_ms();
        // 2x nearest-neighbour upscale into the scratch surface, optional 180 degree flip
        uint32_t *dst = static_cast<uint32_t *>(scratch->pixels);
        int pitch = scratch->pitch / 4;
        bool flip = game->st.flip;
        for (int y = 0; y < VH; y++) {
            const u32 *src = &gfx->px[y * VW];
            for (int x = 0; x < VW; x++) {
                uint32_t c = src[x] & 0x00FFFFFFu;
                if (swap_rb) c = ((c & 0xFF) << 16) | (c & 0xFF00) | ((c >> 16) & 0xFF);
                if (flip) { row[PW - 1 - 2 * x] = c; row[PW - 2 - 2 * x] = c; }
                else { row[2 * x] = c; row[2 * x + 1] = c; }
            }
            int y0 = flip ? PH - 1 - 2 * y : 2 * y, y1 = flip ? PH - 2 - 2 * y : 2 * y + 1;
            memcpy(dst + y0 * pitch, row, sizeof(row));
            memcpy(dst + y1 * pitch, row, sizeof(row));
        }
        SDL_BlitSurface(scratch, nullptr, video, nullptr);
        SDL_Flip(video);
        double t4 = now_ms();
        sUpd.add(t1 - t0); sAud.add(t2 - t1); sDraw.add(t3 - t2); sPres.add(t4 - t3); sFrame.add(t4 - lastFrame);
        lastFrame = t4;
        frames++;

        if (game->settings_dirty) { save_settings(*game); game->settings_dirty = false; }
        if (t4 - fpsT >= 5000) {
            double secs = (t4 - fpsT) / 1000.0;
            double nf = frames ? frames : 1;
            fprintf(stderr, "perf: %.0f fps %.0f ups | ms avg/max: update %.2f/%.1f audio-sync %.2f/%.1f draw %.2f/%.1f present %.2f/%.1f frame-gap %.1f/%.1f\n",
                    frames / secs, updates / secs, sUpd.sum / nf, sUpd.mx, sAud.sum / nf, sAud.mx, sDraw.sum / nf, sDraw.mx,
                    sPres.sum / nf, sPres.mx, sFrame.sum / nf, sFrame.mx);
            fpsT = t4; frames = 0; updates = 0;
            sUpd.reset(); sDraw.reset(); sPres.reset(); sAud.reset(); sFrame.reset();
        }
    }
    rumble_off_now();
    if (haveAudio) SDL_CloseAudio();
    save_settings(*game);
    SDL_Quit();
    return 0;
}
