// Broyoo - headless test driver: runs the game logic without SDL, dumps PNG screenshots, checks rules.
#include "game.h"
#include "audio.h"
#include "phrases.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int g_fail = 0;
#define CHECK(cond, msg) do { if (cond) printf("PASS  %s\n", msg); else { printf("FAIL  %s\n", msg); g_fail++; } } while (0)

static Gfx *gfx = new Gfx();
static std::string outdir = ".";

static void shot(Game &g, const char *name, int scale = 2) {
    g.render(*gfx);
    std::string p = outdir + "/" + name + ".png";
    if (scale == 1) { write_png(p.c_str(), gfx->px, VW, VH); return; }
    static u32 big[VW * 2 * VH * 2];
    for (int y = 0; y < VH * 2; y++) for (int x = 0; x < VW * 2; x++) big[y * VW * 2 + x] = gfx->px[(y / 2) * VW + x / 2];
    write_png(p.c_str(), big, VW * 2, VH * 2);
}

static void run(Game &g, int frames, const Input &in) { for (int i = 0; i < frames; i++) g.update(in); }

// simple autopilot: run right, jump when blocked, shoot
static Input autopilot(Game &g, int t) {
    Input in;
    in.right = true;
    in.fire = true;
    in.jump = (t / 20) % 3 == 0;
    (void)g;
    return in;
}

int main(int argc, char **argv) {
    if (argc > 2 && !strcmp(argv[1], "--icon")) { // render the Onion app icon (80x80)
        static u32 icon[80 * 80];
        for (int y = 0; y < 80; y++)
            for (int x = 0; x < 80; x++) {
                float dx = float(x - 40), dy = float(y - 40), d = dx * dx + dy * dy;
                u32 c = RGB(40 + y / 3, 20 + y / 5, 60 - y / 3);
                if (d < 34 * 34) c = RGB(255, 140, 30);
                if (d < 26 * 26) c = RGB(255, 205, 60);
                if (d < 14 * 14) c = RGB(255, 250, 200);
                icon[y * 80 + x] = c;
            }
        memcpy(gfx->px, icon, 0); // (gfx unused for the icon geometry)
        Gfx *ig = gfx;
        ig->clear(0);
        for (int i = 0; i < 80 * 80; i++) ig->px[i] = icon[i];
        // Gfx rows are VW wide, so draw into a 320-wide scratch and copy out
        for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++) ig->px[y * VW + x] = icon[y * 80 + x];
        struct L { const char *t; int sc, y; u32 col; } lines[3] = {{"BRO", 5, 6, RGB(255, 255, 255)}, {"FORCE", 4, 30, RGB(255, 215, 60)}, {"LITE", 4, 54, RGB(220, 40, 40)}};
        for (const L &l : lines) {
            int sc = l.sc; while (sc > 1 && Gfx::text_w(l.t, sc) > 74) sc--;
            int x = (80 - Gfx::text_w(l.t, sc)) / 2;
            ig->text(x + 2, l.y + 2, l.t, RGB(0, 0, 0), sc);
            ig->text(x, l.y, l.t, l.col, sc);
        }
        for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++) icon[y * 80 + x] = ig->px[y * VW + x];
        return write_png(argv[2], icon, 80, 80) ? 0 : 1;
    }
    if (argc > 1) outdir = argv[1];
    Game g;
    g.set_progress(3);

    // 1. title screen
    for (int i = 0; i < 90; i++) g.update(Input());
    shot(g, "01_title");

    // 2. every level boots and renders
    for (int lvl = 0; lvl < NUM_LEVELS; lvl++) {
        g.debug_start_level(lvl, 0);
        run(g, 10 + INTRO_HOLD, Input());
        char n[32]; snprintf(n, sizeof(n), "02_level%d_start", lvl + 1);
        shot(g, n);
        printf("level %d: width=%d tiles, enemies=%d\n", lvl + 1, g.level_w(), g.enemies_alive());
    }

    // 3. destructible terrain
    g.debug_start_level(0, 0);
    run(g, 30, Input());
    int tx = 30;
    int gr = g.ground_row(tx);
    uint8_t before = g.debug_tile(tx, gr);
    g.debug_explode(float(tx * TS + 4), float(gr * TS), 26, 14);
    CHECK(before != T_AIR && g.debug_tile(tx, gr) == T_AIR, "explosion destroys dirt terrain");

    // 4. one bullet = death
    g.debug_start_level(0, 0);
    run(g, 5, Input());
    g.player().invuln = 0;
    {
        Player &p = g.player();
        CHECK(p.alive, "player alive after spawn");
        // enemy bullet fired straight at the player
        g.debug_explode(p.a.x + 40, p.a.y, 4, 1); // harmless far explosion
        CHECK(p.alive, "far explosion does not kill");
        g.debug_explode(p.a.x + 6, p.a.y - 5, 20, 14, false);
        CHECK(!p.alive, "close explosion kills the bro");
    }

    // 5. falling out of the world kills
    g.debug_start_level(0, 0);
    run(g, 5, Input());
    g.player().invuln = 0;
    // drop the bro into a freshly emptied column so he falls through the world naturally
    for (int y = 0; y < 60; y++) for (int dx = -2; dx <= 2; dx++) g.debug_damage_tile(int(g.player().a.x / TS) + dx, y, 99); // (a blast would now throw him out of the shaft)
    run(g, 400, Input());
    run(g, 5, Input());
    CHECK(!g.player().alive, "falling below the world kills the bro");

    // 6. screen shake + rumble scale with distance and settings
    g.st.shake = 100; g.st.vib = 100;
    g.debug_start_level(0, 0);
    run(g, 100, Input());
    float px = g.player().a.x, py = g.player().a.y;
    g.debug_explode(px + 120, py, 26, 14);
    float far_t = g.get_trauma(), far_r = g.rumble_level();
    run(g, 120, Input());
    g.debug_explode(px + 20, py, 26, 14);
    float near_t = g.get_trauma(), near_r = g.rumble_level();
    printf("trauma far=%.2f near=%.2f rumble far=%.2f near=%.2f\n", far_t, near_t, far_r, near_r);
    CHECK(near_t > far_t, "closer explosion -> stronger screen shake");
    CHECK(near_r > far_r, "closer explosion -> stronger vibration");
    int maxshake = 0;
    for (int i = 0; i < 30; i++) { g.update(Input()); if (std::abs(g.shake_x()) + std::abs(g.shake_y()) > maxshake) maxshake = std::abs(g.shake_x()) + std::abs(g.shake_y()); }
    CHECK(maxshake > 0, "screen shake offsets are applied at 100%");
    for (int k = 0; k <= 4; k++) {
        g.st.shake = k * 25; g.st.vib = k * 25;
        g.debug_start_level(0, 0);
        run(g, 100, Input());
        g.debug_explode(g.player().a.x + 12, g.player().a.y, 26, 14);
        int m = 0;
        float r = 0;
        for (int i = 0; i < 20; i++) { g.update(Input()); m = std::max(m, std::abs(g.shake_x()) + std::abs(g.shake_y())); r = std::max(r, g.rumble_level()); }
        printf("setting %3d%%: max shake offset=%d px, max rumble=%.2f\n", k * 25, m, r);
        if (k == 0) CHECK(m == 0 && r == 0, "0% setting disables shake and vibration");
    }
    g.st.shake = 100; g.st.vib = 100;

    // 7. every bro: fire, special, melee for a while without crashing; screenshot
    for (int b = 0; b < NUM_BROS; b++) {
        g.debug_start_level(0, b);
        g.player().invuln = 100000;
        run(g, 20, Input());
        Input in;
        in.fire = true; in.right = true;
        for (int i = 0; i < 90; i++) { in.special = i == 10 || i == 50; in.melee = i % 30 == 5; in.jump = i % 40 == 0; g.update(in); }
        char n[48]; snprintf(n, sizeof(n), "03_bro%02d", b);
        if (b == 0 || b == 1 || b == 2 || b == 7 || b == 11 || b == 13) shot(g, n);
        CHECK(g.player().alive, (std::string("bro ") + BROS[b].name + " survives a firing session").c_str());
    }

    // 8. autopilot run through each level, screenshots along the way
    for (int lvl = 0; lvl < 3; lvl++) {
        g.debug_start_level(lvl, 0);
        g.player().invuln = 100000;
        int best = 0;
        for (int t = 0; t < 3600; t++) {
            Input in = autopilot(g, t);
            in.special = t % 200 == 0;
            g.update(in);
            if (t % 600 == 300) { char n[48]; snprintf(n, sizeof(n), "04_level%d_t%d", lvl + 1, t); shot(g, n); }
            best = std::max(best, int(g.player().a.x));
            if (g.state() != GS_PLAY) break;
        }
        printf("level %d autopilot: reached x=%d of %d, state=%d, kills=%d\n", lvl + 1, best, g.level_w() * TS, int(g.state()), g.stat_kills);
    }

    // 9. boss level: stand under the helicopter and shoot up
    g.debug_start_level(LAST_LEVEL, 0);
    g.player().invuln = 100000;
    g.debug_teleport(60.0f * TS, 39.0f * TS);
    run(g, 5, Input());
    {
        float bx = 0, by = 0; int bt = 0;
        for (int i = 0; i < g.debug_enemy_count(); i++) { g.debug_enemy(i, bx, by, bt); if (bt == E_BOSS) break; }
        g.debug_teleport(std::min(std::max(bx, 80.0f), 1500.0f), 40.0f * TS);
        Input fu; fu.up = true; fu.fire = true;
        for (int t = 0; t < 400; t++) {
            g.debug_enemy(0, bx, by, bt);
            for (int i = 0; i < g.debug_enemy_count(); i++) { g.debug_enemy(i, bx, by, bt); if (bt == E_BOSS) break; }
            g.player().a.x = bx; g.player().a.y = 40.0f * TS;
            g.update(fu);
        }
        shot(g, "05_boss");
        printf("boss hp after shooting up for ~6s: %d/70\n", g.boss_hp());
        CHECK(g.boss_hp() < 70, "boss takes damage from the player's bullets");
    }

    // 9b. wall jump: climb a 12-tile metal wall by pushing against it and jumping
    {
        g.debug_start_level(0, 0);
        g.player().invuln = 100000;
        run(g, 5, Input());
        int wx = int(g.player().a.x / TS) + 4, gr = g.ground_row(wx);
        for (int y = gr - 12; y < gr; y++) g.debug_set_tile(wx, y, T_METAL);
        float top = float((gr - 12) * TS), startY = g.player().a.y;
        float best = startY;
        Input in; in.right = true;
        int hold = 0, rest = 0;
        for (int t = 0; t < 600; t++) {
            // a human presses jump when the bro touches the wall, holds it for the whole jump, then lets go
            if (hold > 0) { in.jump = true; hold--; if (hold == 0) rest = 2; }
            else if (rest > 0) { in.jump = false; rest--; }
            else if (g.player().wallT > 0 || g.player().a.ground) { in.jump = true; hold = 12; }
            else in.jump = false;
            g.update(in);
            best = std::min(best, g.player().a.y);
        }
        printf("wall jump: start y=%.0f, highest y=%.0f, wall top y=%.0f\n", startY, best, top);
        CHECK(best < top + 4, "wall jump lets the bro climb a tall wall");
    }

    // 10. enemy AI kills an exposed bro (one hit = death)
    {
        bool killed = false;
        for (int idx = 0; idx < 12 && !killed; idx++) {
            g.debug_start_level(0, 0);
            g.player().invuln = 0;
            float ex, ey; int et;
            if (!g.debug_enemy(idx, ex, ey, et) || et != E_MOOK) continue;
            run(g, 3, Input());
            g.debug_teleport(ex - 70, ey);
            g.player().a.face = 1;
            g.player().invuln = 0;
            run(g, 500, Input());
            killed = !g.player().alive;
        }
        CHECK(killed, "an enemy soldier shoots the bro dead (one hit = death)");
    }

    // 11. prisoner cage rescue: +1 life and a new bro
    {
        g.debug_start_level(0, 0);
        float cx, cy;
        bool has = g.debug_prop(PR_CAGE, cx, cy);
        CHECK(has, "level contains a prisoner cage");
        if (has) {
            g.debug_teleport(cx - 30, cy);
            g.player().invuln = 100000;
            int lives0 = g.player().lives, bro0 = g.player().bro;
            g.debug_explode(cx, cy - 6, 10, 10);
            run(g, 20, Input());
            float fx, fy;
            bool freed = g.debug_prop(PR_FREED, fx, fy);
            CHECK(freed, "cage opens and frees the prisoner");
            if (freed) {
                g.debug_teleport(fx, fy);
                run(g, 3, Input());
                CHECK(g.player().lives == lives0 + 1, "rescued prisoner gives an extra life");
                printf("bro changed: %s -> %s\n", BROS[bro0].name, BROS[g.player().bro].name);
            }
        }
    }

    // 12. enemy officer: killing him calls the helicopter; grab the rope ladder; results panel; next level
    {
        g.debug_start_level(0, 0);
        run(g, 3, Input());
        float ox = 0, oy = 0; int ot = 0, found = -1;
        for (int i = 0; i < g.debug_enemy_count(); i++) { g.debug_enemy(i, ox, oy, ot); if (ot == g.officer_type()) { found = i; break; } }
        CHECK(found >= 0, "level ends with an enemy officer");
        g.debug_teleport(ox - 60, oy);
        g.player().invuln = 100000;
        CHECK(g.exfil_phase() == 0, "no helicopter before the officer dies");
        g.debug_explode(ox, oy - 7, 12, 10); // a blast kills the (unarmed) officer
        run(g, 2, Input());
        CHECK(g.exfil_phase() >= 1, "killing the officer calls the helicopter");
        run(g, 400, Input());
        CHECK(g.exfil_phase() == 2, "helicopter hovers with its ladder down");
        CHECK(g.heli_volume() > 0.3f, "the rotor loop is audible while the helicopter hovers nearby");
        shot(g, "06_ladder");
        float lx, ly;
        g.debug_ladder(lx, ly);
        g.debug_teleport(lx, ly - 24); // grab the ladder well above its end
        run(g, 3, Input());
        CHECK(g.exfil_phase() == 3, "touching the ladder starts the rescue");
        run(g, 120, Input());
        {
            float lx2, ly2;
            g.debug_ladder(lx2, ly2);
            float off = ly2 - g.player().a.y;
            printf("grabbed %.0f px above the ladder end, now %.1f px above it\n", 24.0f, off);
            CHECK(std::fabs(off - 24.0f) < 5.0f, "the bro stays on the ladder at the height he grabbed it");
        }
        float v1 = g.heli_volume();
        shot(g, "06_rescue");
        run(g, 700, Input());
        CHECK(v1 > 0.0f && g.heli_volume() < 0.01f, "the rotor sound dies away once the helicopter has left");
        shot(g, "06_results");
        CHECK(g.exfil_done(), "results panel finishes stacking the kills");
        printf("kills recorded: %d\n", g.kill_log_size());
        Input a; a.jump = true;
        g.update(a);
        CHECK(g.state() == GS_PLAY && g.level_w() > 0 && g.exfil_phase() == 0, "pressing A moves on to the next level");
    }

    // 12b. your own explosions never kill you, enemy ones do
    {
        g.debug_start_level(0, 0);
        run(g, 5, Input());
        g.player().invuln = 0;
        g.debug_explode(g.player().a.x, g.player().a.y - 7, 30, 16, true);
        CHECK(g.player().alive, "own explosion at point-blank does not kill the bro");
        g.debug_explode(g.player().a.x, g.player().a.y - 7, 30, 16, false);
        CHECK(!g.player().alive, "an enemy explosion still kills the bro");
    }

    // 12c. the killer laughs after the bro dies
    {
        bool laughed = false;
        for (int idx = 0; idx < 12 && !laughed; idx++) {
            g.debug_start_level(0, 0);
            float ex_, ey_; int et;
            if (!g.debug_enemy(idx, ex_, ey_, et) || et != E_MOOK) continue;
            run(g, 3, Input());
            g.debug_clear_enemies();
            for (int k = 0; k < 8; k++) g.debug_add_enemy(E_MOOK, ex_ + float(k) * 3.0f, ey_, -1); // soldiers facing the bro: they have him in sight
            g.debug_teleport(ex_ - 40, ey_);
            g.player().invuln = 100000;
            for (int i = 0; i < 20; i++) { g.update(Input()); g.sounds.clear(); }
            g.player().invuln = 0;
            g.debug_explode(g.player().a.x, g.player().a.y - 7, 20, 16, false);
            int seen = 0;
            for (int i = 0; i < 140; i++) { g.update(Input()); if (g.laughing_count() > 0) seen++; }
            laughed = seen > 20;
        }
        CHECK(laughed, "after the bro dies, a nearby soldier laughs at him");
        shot(g, "09_laugh");
        g.debug_start_level(0, 0);
        run(g, 3, Input());
        float bx, by; int bt;
        for (int i = 0; i < g.debug_enemy_count(); i++) {
            g.debug_enemy(i, bx, by, bt);
            if (bt == E_BOMBER) { g.debug_teleport(bx - 70, by); g.player().invuln = 100000; run(g, 40, Input()); shot(g, "09_bomber"); break; }
        }
    }

    // 13. shots go straight (no diagonals), except the shotgun cone
    {
        bool diagonal = false;
        int checked = 0;
        for (int b = 0; b < NUM_BROS; b++) {
            if (BROS[b].prim == P_SHOTGUN || BROS[b].prim == P_FLAME) continue; // cone / flame spread by design
            g.debug_start_level(0, b);
            g.player().invuln = 100000;
            run(g, 5, Input());
            Input in; in.fire = true; in.right = true;
            for (int i = 0; i < 90; i++) {
                in.up = (i / 30) == 1;
                in.jump = i % 25 == 0;
                g.update(in);
                for (size_t k = 0; k < g.proj_count(); k++) {
                    float vx, vy; int kind;
                    if (!g.debug_proj(k, vx, vy, kind)) continue;
                    if (kind == PK_BULLET || kind == PK_REVBULLET || kind == PK_ROCKET || kind == PK_RAY || kind == PK_EBULLET) {
                        checked++;
                        if (std::fabs(vx) > 0.01f && std::fabs(vy) > 0.01f) diagonal = true;
                    }
                }
            }
        }
        printf("straight-shot check inspected %d projectile-frames\n", checked);
        CHECK(checked > 100 && !diagonal, "player shots only travel horizontally or vertically");
    }

    // 14. collapse: knock out the only pillar holding a platform up
    {
        g.debug_start_level(0, 0);
        run(g, 3, Input());
        int x0 = 60, gr = 40;
        for (int x = x0 - 2; x < x0 + 12; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); } // flat test ground (the real levels have relief)
        for (int x = x0; x < x0 + 8; x++) for (int y = gr - 12; y < gr; y++) g.debug_set_tile(x, y, T_AIR);
        for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x0 + 3, y, T_WOOD); // pillar
        for (int x = x0; x < x0 + 7; x++) g.debug_set_tile(x, gr - 7, T_BRICK); // platform
        g.debug_teleport(float((x0 - 20) * TS), float(gr * TS));
        g.player().invuln = 100000;
        g.debug_explode(float((x0 + 3) * TS + 4), float((gr - 3) * TS), 12, 12); // destroys the pillar only
        run(g, 2, Input());
        CHECK(g.chunk_count() > 0, "platform detaches when its support is destroyed");
        run(g, 200, Input());
        CHECK(g.chunk_count() == 0, "fallen platform settles back into the terrain");
        int bricksBelow = 0, bricksAtOld = 0;
        for (int x = x0; x < x0 + 7; x++) {
            if (g.debug_tile(x, gr - 7) == T_BRICK) bricksAtOld++;
            for (int y = gr - 6; y < gr + 3; y++) if (g.debug_tile(x, y) == T_BRICK) bricksBelow++;
        }
        printf("platform: %d bricks left at old height, %d landed lower\n", bricksAtOld, bricksBelow);
        CHECK(bricksAtOld == 0 && bricksBelow == 7, "platform tiles landed on the ground");
    }

    // 15. audio: every effect and every music track produces sound and never overflows
    {
        audio_init(22050);
        int nsamp = audio_load_samples("assets/sounds");
        printf("sample bank: %d files\n", nsamp);
        CHECK(nsamp >= 55, "the recorded sample bank loads (effects + voice lines)");
        audio_set_volumes(1.0f, 1.0f);
        bool all = true;
        static int16_t buf[22050];
        for (int sfx = 0; sfx < SFX_COUNT; sfx++) {
            audio_music(-1);
            audio_trigger(sfx, 1.0f);
            audio_mix(buf, 22050);
            long e = 0;
            for (int i = 0; i < 22050; i++) e += std::abs(buf[i]);
            if (e == 0) { all = false; printf("  silent sfx %d\n", sfx); }
        }
        CHECK(all, "all sound effects are audible");
        bool music = true;
        for (int t = 0; t < MUSIC_TRACKS; t++) {
            audio_music(t);
            audio_mix(buf, 22050);
            audio_mix(buf, 22050);
            long e = 0; int peak = 0;
            for (int i = 0; i < 22050; i++) { e += std::abs(buf[i]); peak = std::max(peak, std::abs(int(buf[i]))); }
            if (e == 0 || peak >= 32767) { music = false; printf("  bad music track %d (energy %ld peak %d)\n", t, e, peak); }
        }
        CHECK(music, "all music tracks play without clipping");
    }

    // 16. new bros fire, use specials and the jetpack/boomerang actually work
    {
        for (int b = 14; b < NUM_BROS; b++) {
            g.debug_start_level(0, b);
            g.player().invuln = 100000;
            run(g, 10, Input());
            Input in; in.fire = true; in.right = true;
            for (int i = 0; i < 120; i++) { in.special = i == 10 || i == 70; in.jump = i % 30 == 0; g.update(in); }
            char n[48]; snprintf(n, sizeof(n), "07_newbro%02d", b);
            shot(g, n);
        }
        g.debug_start_level(0, 20); // Brocketeer: jetpack
        g.player().invuln = 100000;
        run(g, 20, Input());
        float y0 = g.player().a.y;
        Input in; in.jump = true; in.special = true;
        g.update(in); in.special = false;
        float top = y0;
        for (int i = 0; i < 100; i++) { g.update(in); top = std::min(top, g.player().a.y); }
        printf("jetpack climbed %.0f px\n", y0 - top);
        CHECK(y0 - top > 60, "rocket pack lets Brocketeer climb well above a normal jump");
    }


    // ---- helpers for the fire / gas / blood scenarios: a flat, empty stage on level 1 ----
    auto stage = [&](int x0, int width, int &gr) {
        g.debug_start_level(0, 0);
        run(g, 3, Input());
        gr = g.ground_row(x0 + width / 2);
        if (gr >= 54) gr = 40; // the random level has a pit here: build the floor at a sane height anyway
        for (int x = x0; x < x0 + width; x++) {
            for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR);
            for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT);
        }
        g.debug_clear_enemies();
        g.player().invuln = 100000;
    };

    // 18. chain reaction: red crates close together go off one after another
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        for (int i = 0; i < 6; i++) g.debug_set_tile(x0 + 20 + i * 3, gr - 1, T_BARREL); // 24 px apart
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        int e0 = g.stat_explosions;
        g.debug_explode(float((x0 + 20) * TS + 4), float((gr - 1) * TS + 4), 8, 6, true);
        run(g, 3, Input());
        int early = g.stat_explosions - e0;
        run(g, 200, Input());
        int total = g.stat_explosions - e0;
        int left = 0;
        for (int i = 0; i < 6; i++) if (g.debug_tile(x0 + 20 + i * 3, gr - 1) == T_BARREL) left++;
        printf("chain: %d explosions after 3 frames, %d after 200 (crates left: %d)\n", early, total, left);
        CHECK(left == 0 && total >= 6, "an exploding red crate sets off the red crates next to it");
        CHECK(early < total, "the chain goes off in sequence, not all in the same frame");
    }

    // 19. slightly further away: the crate catches fire for a few seconds, then explodes
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        g.debug_set_tile(x0 + 20, gr - 1, T_BARREL);
        g.debug_set_tile(x0 + 25, gr - 1, T_BARREL); // 40 px away: beyond the blast, inside the heat ring
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.player().invuln = 0;
        int e0 = g.stat_explosions;
        g.debug_explode(float((x0 + 20) * TS + 4), float((gr - 1) * TS + 4), 30, 16, false);
        run(g, 6, Input());
        bool burning = g.debug_burning(x0 + 25, gr - 1);
        bool intact = g.debug_tile(x0 + 25, gr - 1) == T_BARREL;
        int early = g.stat_explosions - e0;
        run(g, 400, Input());
        bool gone = g.debug_tile(x0 + 25, gr - 1) == T_AIR;
        printf("far crate: burning=%d intact-after-6-frames=%d explosions %d -> %d\n", burning, intact, early, g.stat_explosions - e0);
        CHECK(burning && intact, "a red crate a bit further away catches fire instead of exploding at once");
        CHECK(gone && g.stat_explosions - e0 > early, "the burning red crate explodes a few seconds later");
    }

    // 20. brown crates burn but never explode and never hurt the bro
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        g.debug_set_tile(x0 + 20, gr - 1, T_BARREL);
        for (int k = 0; k < 4; k++) g.debug_set_tile(x0 + 24 + k, gr - 1, T_CRATE);
        g.debug_teleport(float((x0 + 4) * TS), float(gr * TS));
        g.player().invuln = 0;
        g.debug_explode(float((x0 + 20) * TS + 4), float((gr - 1) * TS + 4), 30, 16, false);
        run(g, 30, Input()); // let the red crate's own follow-up blast go off first
        int e0 = g.stat_explosions;
        bool sawFire = false;
        for (int i = 0; i < 80; i++) { g.update(Input()); for (int k = 0; k < 4; k++) if (g.debug_burning(x0 + 24 + k, gr - 1)) sawFire = true; }
        run(g, 60, Input());
        shot(g, "10_fire");
        run(g, 640, Input());
        int crates = 0;
        for (int k = 0; k < 4; k++) if (g.debug_tile(x0 + 24 + k, gr - 1) == T_CRATE) crates++;
        printf("brown crates: saw fire=%d, remaining=%d, extra explosions=%d\n", sawFire, crates, g.stat_explosions - e0);
        CHECK(sawFire, "brown crates catch fire from a nearby explosion");
        CHECK(crates == 0 && g.stat_explosions == e0, "brown crates burn away without exploding");
        CHECK(g.player().alive, "burning crates do not hurt the bro");
    }

    // 21. gas bottle: heats, then a fire jet throws it (slow start, accelerating), smashing the scenery, and it explodes
    {
        int gr; int x0 = 100;
        stage(x0, 60, gr);
        for (int y = gr - 3; y < gr; y++) g.debug_set_tile(x0 + 34, y, T_BRICK); // a wall in its way
        g.debug_add_gas(float((x0 + 20) * TS + 4), float(gr * TS));
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.debug_explode(float((x0 + 20) * TS + 4 - 36), float((gr - 1) * TS), 24, 6, true); // heat from a small blast nearby
        float gx, gy; int st = 0, maxSt = 0;
        bool sawHeating = false;
        std::vector<float> xs;
        int frames = 0, gone = -1;
        for (int i = 0; i < 900; i++) {
            g.update(Input());
            frames++;
            if (!g.debug_gas(0, gx, gy, st)) { gone = i; break; }
            if (st == 1) sawHeating = true;
            maxSt = std::max(maxSt, st);
            xs.push_back(gx);
        }
        float start = xs.empty() ? 0 : xs.front();
        float far_ = 0;
        for (float v : xs) far_ = std::max(far_, std::fabs(v - start));
        // speed samples during flight: first and later 10-frame displacement
        int fly0 = -1;
        for (size_t i = 1; i < xs.size(); i++) if (std::fabs(xs[i] - xs[i - 1]) > 0.01f) { fly0 = int(i); break; }
        float early = 0, late = 0;
        if (fly0 >= 0 && fly0 + 40 < int(xs.size())) { early = std::fabs(xs[size_t(fly0 + 12)] - xs[size_t(fly0 + 2)]); late = std::fabs(xs[size_t(fly0 + 40)] - xs[size_t(fly0 + 30)]); }
        printf("gas: heating=%d flew %.0f px, explodes after %d frames (%.1fs), speed early %.1f vs late %.1f px/10f, wall brick left=%d\n",
               sawHeating, far_, gone, gone / 62.5f, early, late, int(g.debug_tile(x0 + 34, gr - 1) == T_BRICK));
        CHECK(sawHeating && maxSt == 2, "a heated gas bottle first vents fire, then takes off");
        CHECK(far_ > 30, "the bottle is propelled across the level");
        CHECK(late > early * 1.5f, "it starts slowly then accelerates");
        CHECK(gone > 0 && gone / 62.5f > 0.7f && gone / 62.5f < 5.0f, "it explodes after a few seconds");
    }

    // 22. you can stand on a gas bottle and ride it; it does not hurt you while it flies
    {
        int gr; int x0 = 100;
        stage(x0, 80, gr);
        g.debug_add_gas(float((x0 + 30) * TS + 4), float(gr * TS));
        g.debug_teleport(float((x0 + 30) * TS + 4), float(gr * TS - 40));
        run(g, 40, Input());
        float gx, gy; int st;
        g.debug_gas(0, gx, gy, st);
        bool standing = std::fabs(g.player().a.y - (gy - 13)) < 1.5f;
        g.player().invuln = 0;
        g.debug_ignite_gas(0);
        float p0 = g.player().a.x;
        bool carried = false, alive = true;
        for (int i = 0; i < 160; i++) {
            g.update(Input());
            if (!g.debug_gas(0, gx, gy, st)) break;
            if (st == 2 && std::fabs(g.player().a.x - gx) < 8 && std::fabs(g.player().a.x - p0) > 25) carried = true;
            if (!g.player().alive) alive = false;
        }
        printf("riding: standing=%d carried=%d alive=%d\n", standing, carried, alive);
        CHECK(standing, "the bro can stand on top of a gas bottle");
        CHECK(carried && alive, "the bro is carried along by a flying gas bottle without being hurt");
        shot(g, "10_ride");
    }

    // 23. blood stains the scenery
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        long b0 = g.debug_blood_total();
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        for (int i = 0; i < 12; i++) g.debug_explode(float((x0 + 20) * TS + 4), float(gr * TS - 6), 4, 1, true);
        // spawn soldiers via the level's own: kill the player instead (always available gore)
        g.player().invuln = 0;
        g.debug_explode(g.player().a.x, g.player().a.y - 7, 20, 16, false);
        run(g, 22, Input());
        shot(g, "10_blood");
        run(g, 98, Input());
        long b1 = g.debug_blood_total();
        printf("blood stain total %ld -> %ld\n", b0, b1);
        CHECK(b1 > b0 + 300, "dying splatters blood on the ground and walls");
    }

    // 24. the officer cannot be killed by accident (enemy blasts), only by the bro
    {
        g.debug_start_level(0, 0);
        run(g, 3, Input());
        float ox = 0, oy = 0; int ot = 0;
        for (int i = 0; i < g.debug_enemy_count(); i++) { g.debug_enemy(i, ox, oy, ot); if (ot == g.officer_type()) break; }
        g.debug_teleport(ox - 80, oy);
        g.debug_explode(ox, oy - 7, 30, 16, false);
        run(g, 10, Input());
        CHECK(g.exfil_phase() == 0, "an enemy explosion cannot kill the officer");
        g.debug_explode(ox, oy - 7, 30, 16, true);
        run(g, 3, Input());
        CHECK(g.exfil_phase() >= 1, "the bro's own explosion does kill him");
    }

    // 25. explosions visibly shake the screen at the default setting
    {
        Settings def;
        printf("default settings: shake=%d%% vibration=%d%%\n", def.shake, def.vib);
        CHECK(def.shake == 100, "screen shake defaults to 100%");
        g.st.shake = 100;
        g.debug_start_level(0, 0);
        run(g, 60, Input());
        g.debug_explode(g.player().a.x + 150, g.player().a.y - 8, 24, 14, true);
        int mx = 0;
        for (int i = 0; i < 20; i++) { g.update(Input()); mx = std::max(mx, std::abs(g.shake_x()) + std::abs(g.shake_y())); }
        printf("shake from a rocket 150px away: up to %d px\n", mx);
        CHECK(mx >= 2, "a rocket explosion 150 px away visibly shakes the screen");
    }

    // 26. voices: laugh/gasp/scream are loud enough, not clipped
    {
        audio_init(22050);
        audio_set_volumes(1.0f, 0.0f);
        static int16_t buf[22050];
        bool clipFree = true;
        for (int s = 0; s < SFX_COUNT + SP_COUNT; s++) {
            if (s < SFX_COUNT) audio_trigger(s, 1.0f); else audio_speak(s - SFX_COUNT, 1.0f);
            audio_mix(buf, 22050);
            int peak = 0; long e = 0;
            for (int i = 0; i < 22050; i++) { peak = std::max(peak, std::abs(int(buf[i]))); e += std::abs(buf[i]); }
            if (peak >= 26000) clipFree = false;
            printf("sound %2d: peak %5d mean %ld\n", s, peak, e / 22050);
        }
        CHECK(clipFree, "no effect or voice line is driven into clipping");
        // worst-case pile-up: three explosions, a scream and a laugh at once
        audio_trigger(SFX_BOOM_BIG, 1.0f); audio_trigger(SFX_BOOM, 1.0f); audio_trigger(SFX_BOOM, 1.0f);
        audio_trigger(SFX_SCREAM, 1.0f); audio_trigger(SFX_LAUGH, 1.0f);
        audio_mix(buf, 22050);
        int pk = 0;
        for (int i = 0; i < 22050; i++) pk = std::max(pk, std::abs(int(buf[i])));
        CHECK(pk < 26500, "stacked explosions and voices are limited without hard clipping");
    }

    // ---- 26c. every level starts with a single bro ----
    {
        g.debug_start_level(1, 0);
        CHECK(g.player().lives == 1, "a level starts with exactly one bro (more come from freed prisoners)");
    }

    // ---- 26a. enemies take a random reaction time between spotting the bro and firing ----
    {
        int lo = 100000, hi = 0;
        int gr; int x0 = 100;
        stage(x0, 70, gr);
        for (int x = x0; x < x0 + 70; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); }
        auto ebullets = [&]() { int n = 0; for (size_t k = 0; k < g.proj_count(); k++) { float vx, vy; int kind; if (g.debug_proj(k, vx, vy, kind) && kind == PK_EBULLET) n++; } return n; };
        for (int trial = 0; trial < 12; trial++) { // one level, so the random generator keeps advancing between trials
            g.debug_clear_enemies();
            g.debug_teleport(float((x0 + 5) * TS), float(gr * TS));
            g.player().invuln = 100000;
            for (int w = 0; w < 200 && ebullets() > 0; w++) g.update(Input()); // let old bullets expire
            g.debug_add_enemy(E_MOOK, float((x0 + 20) * TS), float(gr * TS), -1);
            int f = 0;
            bool fired = false;
            for (; f < 400 && !fired; f++) { g.update(Input()); if (ebullets() > 0) fired = true; }
            if (trial == 0) { float ex_, ey_; int et_; bool ok = g.debug_enemy(0, ex_, ey_, et_); printf("trial0: fired=%d f=%d enemies=%d enemy0 ok=%d at %.0f,%.0f player %.0f,%.0f alive=%d\n", fired, f, g.enemies_alive(), ok, ex_, ey_, g.player().a.x, g.player().a.y, g.player().alive); }
            if (fired) { lo = std::min(lo, f); hi = std::max(hi, f); }
        }
        printf("reaction time before the first shot: %d..%d ticks\n", lo, hi);
        CHECK(lo >= 40, "an enemy never fires right after spotting the bro (at least ~0.7 s)");
        CHECK(hi - lo >= 5, "the reaction time is random from one enemy to the next");
    }

    // ---- 26b. Brodator: cloak + three piercing spears ----
    {
        int bd = -1;
        for (int b = 0; b < NUM_BROS; b++) if (BROS[b].spec == S_SPEARS) bd = b;
        CHECK(bd >= 0, "a bro has the spear special (Brodator)");
        int gr; int x0 = 100;
        stage(x0, 70, gr);
        g.debug_start_level(0, bd);
        g.debug_clear_enemies();
        for (int x = x0; x < x0 + 70; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); }
        g.debug_teleport(float((x0 + 5) * TS), float(gr * TS));
        g.player().invuln = 100000;
        for (int k = 0; k < 3; k++) g.debug_add_enemy(E_MOOK, float((x0 + 14 + k * 3) * TS), float(gr * TS));
        g.player().a.face = 1;
        Input sp; sp.special = true;
        g.update(sp);
        int spears = 0;
        for (size_t k = 0; k < g.proj_count(); k++) { float vx, vy; int kind; if (g.debug_proj(k, vx, vy, kind) && kind == PK_SPEAR) spears++; }
        for (int i = 0; i < 90; i++) g.update(Input());
        printf("brodator: %d spears thrown, enemies left %d of 3\n", spears, g.enemies_alive());
        CHECK(spears == 3 && g.enemies_alive() <= 1, "the Brodator special throws three spears that pierce through the mooks");
    }


    // ---- 27. level intro card: frozen black screen with a voice line, then a fade into the level ----
    {
        g.debug_begin(0);
        float x0 = g.player().a.x;
        bool voice = false;
        Input go; go.right = true;
        for (int i = 0; i < INTRO_HOLD - 5; i++) {
            g.update(go);
            for (const SndEvt &e : g.sounds) if (e.id >= SPEECH_BASE) voice = true;
            g.sounds.clear();
            if (i == 100) shot(g, "11_intro");
        }
        CHECK(g.intro_t() > 0 && std::fabs(g.player().a.x - x0) < 0.5f, "the game is frozen while the black level card is shown");
        CHECK(voice, "the level card plays a voice line");
        for (int i = 0; i < 40; i++) g.update(go);
        shot(g, "11_intro_fade");
        for (int i = 0; i < INTRO_FADE + 30; i++) g.update(go);
        CHECK(g.intro_t() == 0 && g.player().a.x > x0 + 5, "after the fade the level is playable");
    }

    // ---- 28. mission failed: fade to black, voice line, then retry / quit ----
    {
        g.debug_start_level(0, 0);
        g.player().lives = 0;
        g.player().invuln = 0;
        g.debug_explode(g.player().a.x, g.player().a.y - 7, 20, 16, false);
        bool voice = false;
        for (int i = 0; i < 300; i++) {
            g.update(Input());
            for (const SndEvt &e : g.sounds) if (e.id >= SPEECH_BASE) voice = true;
            g.sounds.clear();
            if (i == 160) shot(g, "12_failed");
        }
        CHECK(g.state() == GS_OVER, "no lives left leads to the mission failed screen");
        CHECK(voice, "mission failed plays a voice line");
        shot(g, "12_failed_menu");
        Input a; a.jump = true;
        g.update(a);
        g.update(Input());
        CHECK(g.state() == GS_PLAY, "RETRY restarts the mission");
    }

    // ---- 29. four save slots with completion percentages ----
    {
        Game s;
        CHECK(s.state() == GS_TITLE, "game starts on the title screen");
        Input a; a.jump = true;
        auto press = [&](Input in) { s.update(in); s.update(Input()); };
        press(a);
        CHECK(s.state() == GS_SLOTS, "START GAME opens the save slots");
        shot(s, "13_slots");
        Input dn; dn.down = true;
        press(dn);
        press(a);
        CHECK(s.state() == GS_MISSIONS && s.cur_slot() == 1, "choosing slot 2 opens its mission list");
        shot(s, "13_missions");
        press(a);
        CHECK(s.state() == GS_PLAY, "the first entry starts the game (continue)");
        s.debug_start_level(0, 0);
        s.debug_push_kill(E_MOOK, C_BULLET, 1);
        s.debug_finish_level();
        int comp = s.completion(s.st.slots[1]);
        printf("slot 2 completion after one level: %d%%, unlocked level index %d\n", comp, s.unlocked_of(s.st.slots[1]));
        CHECK(comp == 100 / (2 * NUM_LEVELS) && s.unlocked_of(s.st.slots[1]) == 1, "clearing a level (bros still missing) counts half and saves in that slot only");
        CHECK(s.st.slots[0].mask == 0 && s.st.slots[1].used == 1, "other slots are untouched");
        s.st.slots[1].mask = 0x1FFF; // 13 levels cleared: the mission list scrolls
        s.set_state(GS_MISSIONS);
        for (int k = 0; k < 11; k++) press(dn);
        shot(s, "13_missions_scrolled");
        s.set_state(GS_PLAY);
        for (int i = 0; i < NUM_LEVELS; i++) s.st.slots[2].mask |= 1 << i;
        s.st.slots[2].used = 1; s.st.slots[2].broMask = ~0ull;
        s.st.slots[3].used = 1; s.st.slots[3].mask = 0x0B;
        s.set_state(GS_SLOTS);
        shot(s, "13_slots_progress");
        CHECK(s.completion(s.st.slots[2]) == 100, "a fully cleared slot reads 100%");
    }

    // ---- 30. new enemies: dog bites, rocket soldier ----
    {
        int gr; int x0 = 100;
        stage(x0, 70, gr);
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        g.player().invuln = 0;
        g.debug_add_enemy(E_DOG, float((x0 + 30) * TS), float(gr * TS));
        int frames = 0;
        for (; frames < 300 && g.player().alive; frames++) g.update(Input());
        printf("dog reached and bit the bro after %d frames\n", frames);
        CHECK(!g.player().alive, "an attack dog charges and bites the bro");
        stage(x0, 70, gr);
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        g.player().invuln = 0;
        g.debug_add_enemy(E_RPG, float((x0 + 28) * TS), float(gr * TS));
        bool rocket = false;
        int f2 = 0;
        for (; f2 < 700 && g.player().alive; f2++) { g.update(Input()); for (size_t k = 0; k < g.proj_count(); k++) { float vx, vy; int kind; if (g.debug_proj(k, vx, vy, kind) && kind == PK_EROCKET) rocket = true; } }
        printf("rpg: rocket seen=%d player alive=%d after %d frames\n", rocket, g.player().alive, f2);
        CHECK(rocket && !g.player().alive, "a rocket soldier fires slow rockets that kill on explosion");
    }

    // ---- 31. new weapons: spear kills through several enemies, chainsaw, teleport ----
    {
        int gr; int x0 = 100;
        stage(x0, 70, gr);
        int bro = -1;
        for (int b = 0; b < NUM_BROS; b++) if (BROS[b].prim == P_SPEAR) bro = b;
        g.debug_start_level(0, bro);
        g.debug_clear_enemies();
        for (int x = x0; x < x0 + 70; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); }
        g.debug_teleport(float((x0 + 5) * TS), float(gr * TS));
        g.player().invuln = 100000;
        for (int k = 0; k < 3; k++) g.debug_add_enemy(E_MOOK, float((x0 + 14 + k * 3) * TS), float(gr * TS));
        g.player().a.face = 1;
        Input f; f.fire = true;
        g.update(f);
        for (int i = 0; i < 90; i++) g.update(Input());
        printf("spear: enemies left %d of 3\n", g.enemies_alive());
        CHECK(g.enemies_alive() <= 1, "a thrown spear pierces through several enemies");
        int tp = -1;
        for (int b = 0; b < NUM_BROS; b++) if (BROS[b].spec == S_TELEPORT) tp = b;
        g.debug_start_level(0, tp);
        g.debug_clear_enemies();
        for (int x = x0; x < x0 + 70; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); }
        g.debug_teleport(float((x0 + 5) * TS), float(gr * TS));
        g.player().invuln = 100000;
        g.player().a.face = 1;
        float before = g.player().a.x;
        Input sp; sp.special = true;
        g.update(sp);
        g.update(Input());
        printf("teleport moved %.0f px\n", g.player().a.x - before);
        CHECK(g.player().a.x - before > 40, "the teleport special blinks the bro forward");
    }

    // ---- 32. end-of-level recap: every cause of death re-enacted one after another ----
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        g.debug_start_level(0, 0);
        int types[9] = {E_MOOK, E_RPG, E_HEAVY, E_BOMBER, E_DOG, E_MOOK, E_OFFICER, E_MOOK, E_HEAVY};
        for (int c = 0; c < 9; c++) g.debug_push_kill(types[c], c, c % 2 ? -1 : 1);
        g.debug_start_recap(31);
        for (int i = 0; i < 45 + 31 * 9 + 90; i++) {
            g.update(Input());
            g.sounds.clear();
            // snapshots right in the middle of several deaths (death moment = start + 10 ticks + a few)
            int rt = g.exfil_resT();
            if (rt == 45 + 10 + 4) shot(g, "14_recap_bullet");
            if (rt == 45 + 31 * 1 + 10 + 4) shot(g, "14_recap_boom");
            if (rt == 45 + 31 * 2 + 10 + 14) shot(g, "14_recap_fire");
            if (rt == 45 + 31 * 4 + 10 + 3) shot(g, "14_recap_melee_dog");
            if (rt == 45 + 31 * 6 + 10 + 4) shot(g, "14_recap_spike_crush");
            if (rt == 45 + 31 * 8 + 80) shot(g, "14_recap_end");
        }
        CHECK(g.exfil_done(), "the recap finishes after the last victim has died");
        g.debug_start_level(0, 0);
        for (int i = 0; i < 60; i++) g.debug_push_kill(E_MOOK + (i % 3 == 0 ? E_DOG : 0), i % C_COUNT, i % 2 ? 1 : -1);
        g.debug_start_recap(15);
        for (int i = 0; i < 45 + 15 * 60 + 200; i++) { g.update(Input()); g.sounds.clear(); if (g.exfil_resT() == 45 + 15 * 40) shot(g, "14_recap_many"); }
        CHECK(g.exfil_done(), "a large body count recap also completes");
    }

    // ---- 33. 6 worlds x (3 stages + a boss level): every level builds, stages end with an officer, boss levels hold a boss ----
    {
        int officers = 0, bosses = 0;
        for (int lvl = 0; lvl < NUM_LEVELS; lvl++) {
            g.debug_start_level(lvl, 0);
            run(g, 5, Input());
            float ox, oy; int ot;
            bool has = false;
            for (int i = 0; i < g.debug_enemy_count(); i++) { g.debug_enemy(i, ox, oy, ot); if (ot == g.officer_type()) has = true; }
            if (has) officers++;
            float bx, by; int bhp, bph, bk;
            if (g.debug_boss(bx, by, bhp, bph, bk) && bk == lvl / STAGES) bosses++;
            if (g.world_now() != lvl / STAGES || g.stage_now() != lvl % STAGES) printf("bad world/stage for level %d\n", lvl);
            if (lvl % STAGES == 0 || lvl % STAGES == STAGES - 1) { char n[32]; snprintf(n, sizeof(n), "15_level%02d", lvl + 1); run(g, 60, Input()); shot(g, n); }
        }
        CHECK(officers == NUM_LEVELS - NUM_WORLDS, "every stage ends with an enemy officer");
        CHECK(bosses == NUM_WORLDS, "every world ends with its own boss");
        // the layout differs from one run to the next
        g.debug_start_level(0, 0);
        int h1 = 0; for (int x = 0; x < g.level_w(); x++) h1 = h1 * 31 + g.ground_row(x);
        g.debug_start_level(0, 0);
        int h2 = 0; for (int x = 0; x < g.level_w(); x++) h2 = h2 * 31 + g.ground_row(x);
        CHECK(h1 != h2, "each run generates a different level layout");
    }

    // ---- 35. bosses: each one has its own mechanic ----
    {
        auto boss_level = [&](int w) {
            g.debug_start_level(w * STAGES + STAGES - 1, 0);
            g.player().invuln = 100000;
            g.debug_teleport(float(40 * TS), float(40 * TS));
            run(g, 3, Input());
        };
        // every boss runs 1500 ticks of its AI without trouble, and a bar with the right name exists
        for (int w = 0; w < NUM_WORLDS; w++) {
            boss_level(w);
            float bx, by; int bhp, bph, bk;
            for (int i = 0; i < 1500; i++) { g.update(Input()); g.sounds.clear(); if (i == 500) { char n[32]; snprintf(n, sizeof(n), "17_boss%d", w + 1); shot(g, n); } }
            g.debug_boss(bx, by, bhp, bph, bk);
            g.debug_teleport(std::max(60.0f, bx - 110.0f), float(40 * TS));
            run(g, 4, Input());
            { char n[32]; snprintf(n, sizeof(n), "17_boss%d_close", w + 1); shot(g, n); }
            bool ok = g.debug_boss(bx, by, bhp, bph, bk);
            printf("boss %d (%s): hp %d phase %d\n", w, boss_name(w), ok ? bhp : -1, ok ? bph : -1);
            CHECK(ok && bk == w && bhp > 0, "the boss survives idle for a while and keeps running its AI");
        }
        float bx, by; int bhp, bph, bk;

        // 0 GENERAL MORTARO: unbreakable shelters protect from the barrage, open ground does not
        boss_level(0);
        int sx0 = -1;
        for (int x = 6; x < 190; x++) if (g.debug_tile(x, 34) == T_METAL && g.debug_tile(x + 2, 34) == T_METAL) { sx0 = x; break; }
        CHECK(sx0 > 0, "world 1 boss arena has shelters");
        g.debug_boss_set(2, 0);
        g.player().invuln = 0;
        g.debug_teleport(float((sx0 + 5) * TS), float(40 * TS)); // deep inside, next to the back wall
        for (int k = 0; k < 6; k++) g.debug_strike(float((sx0 + 3) * TS + k * 8));
        int died = -1, firstChunk = -1;
        for (int i = 0; i < 260; i++) { g.update(Input()); g.sounds.clear(); if (g.chunk_count() > 0 && firstChunk < 0) { firstChunk = i; float cx_, cy_; int cw_, ch_, cn_; g.debug_chunk(0, cx_, cy_, cw_, ch_, cn_); printf("  chunk at %.0f,%.0f size %dx%d tiles %d (shelter x0=%d)\n", cx_, cy_, cw_, ch_, cn_, sx0); } if (!g.player().alive && died < 0) died = i; }
        printf("first chunk at tick %d\n", firstChunk);
        if (died >= 0) printf("shelter test: died at tick %d, cause %d, bro at x=%.0f (shelter x0=%d)\n", died, g.death_cause_now(), g.player().a.x, sx0);
        CHECK(g.player().alive && g.chunk_count() == 0, "a shelter protects the bro from the mortar barrage and stays standing");
        boss_level(0);
        g.debug_boss_set(2, 0);
        g.player().invuln = 0;
        float ox2 = float(sx0 > 90 ? 40 * TS : 150 * TS);
        g.debug_teleport(ox2, float(40 * TS));
        for (int k = 0; k < 4; k++) g.debug_strike(ox2);
        for (int i = 0; i < 260 && g.player().alive; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(!g.player().alive, "the same barrage kills a bro standing in the open");

        // 1 IRON SCORPION: charges until it hits the wall; stunned = triple damage
        boss_level(1);
        g.debug_boss(bx, by, bhp, bph, bk);
        g.debug_boss_set(2, 0, bx);
        int stunned = -1;
        for (int i = 0; i < 1200 && stunned < 0; i++) { g.update(Input()); g.sounds.clear(); g.debug_boss(bx, by, bhp, bph, bk); if (bph == 3) stunned = i; }
        printf("tank crashed into the wall after %d ticks\n", stunned);
        CHECK(stunned > 0, "the charging tank crashes into the wall and is stunned");
        int d_stun = g.debug_boss_hurt(2, C_BULLET);
        g.debug_boss_set(0, 0);
        int d_norm = g.debug_boss_hurt(2, C_BULLET);
        CHECK(d_stun == 3 * d_norm && d_norm > 0, "a stunned tank takes triple damage");

        // 2 THE WARDEN: shield until it vents; the laser is stopped by a roof
        boss_level(2);
        g.debug_boss_set(0, 0);
        CHECK(g.debug_boss_hurt(5, C_BULLET) == 0, "the Warden's shield blocks bullets");
        g.debug_boss_set(3, 0);
        CHECK(g.debug_boss_hurt(3, C_BULLET) == 3, "the Warden is vulnerable while venting");
        int cx0 = -1;
        for (int x = 6; x < 190; x++) if (g.debug_tile(x, 35) == T_METAL && g.debug_tile(x + 3, 35) == T_METAL) { cx0 = x; break; }
        CHECK(cx0 > 0, "world 3 boss arena has laser canopies");
        g.player().invuln = 0;
        g.debug_teleport(float((cx0 + 3) * TS), float(40 * TS));
        g.debug_boss_set(1, 69, float((cx0 + 3) * TS));
        g.debug_boss_fx(float((cx0 + 3) * TS));
        for (int i = 0; i < 40; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.player().alive, "a roof stops the Warden's laser");
        boss_level(2);
        g.player().invuln = 0;
        g.debug_teleport(float(90 * TS), float(40 * TS));
        g.debug_boss_set(1, 69, float(90 * TS));
        g.debug_boss_fx(float(90 * TS));
        for (int i = 0; i < 40 && g.player().alive; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(!g.player().alive, "the laser kills a bro caught in the open");

        // 3 FROST COLOSSUS: bullets barely scratch him, falling ice crushes him
        boss_level(3);
        g.debug_boss_set(0, 0);
        int scratch = 0;
        for (int i = 0; i < 30; i++) { g.update(Input()); g.sounds.clear(); scratch += g.debug_boss_hurt(1, C_BULLET); }
        printf("colossus: 30 bullets did %d damage\n", scratch);
        CHECK(scratch <= 8, "bullets barely scratch the Frost Colossus");
        int slab = -1;
        for (int x = 40; x < 190; x++) if (g.debug_tile(x, 22) == T_STONE && g.debug_tile(x - 1, 22) == T_WOOD) { slab = x; break; }
        CHECK(slab > 0, "ice slabs hang from the ceiling");
        float hx = float((slab + 5) * TS);
        g.debug_explode(float((slab - 1) * TS + 4), float(23 * TS), 12, 10, true);
        g.debug_explode(float((slab + 10) * TS + 4), float(23 * TS), 12, 10, true);
        int hp_before = 0;
        g.debug_boss(bx, by, hp_before, bph, bk);
        for (int i = 0; i < 260; i++) { g.debug_boss_set(0, 5, hx); g.update(Input()); g.sounds.clear(); }
        g.debug_boss(bx, by, bhp, bph, bk);
        printf("colossus hp %d -> %d after the slab fell (%d chunks in flight)\n", hp_before, bhp, g.chunk_count());
        CHECK(hp_before - bhp >= 20, "a falling ice slab crushes the Frost Colossus");

        // 4 JUGGERNAUT: bullet-proof, explosions work, supply drops bring gas bottles
        boss_level(4);
        g.debug_boss_set(0, 0);
        CHECK(g.debug_boss_hurt(5, C_BULLET) == 0, "bullets bounce off the Juggernaut");
        g.debug_boss(bx, by, bhp, bph, bk);
        g.debug_explode(bx, by - 10, 30, 16, false);
        int hp2; g.debug_boss(bx, by, hp2, bph, bk);
        CHECK(hp2 < bhp, "explosions hurt the Juggernaut");
        int gas0 = g.debug_gas_count();
        g.debug_boss_set(0, 199);
        for (int i = 0; i < 10; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.debug_gas_count() >= gas0 + 2, "a supply drop brings new gas bottles");

        // a dead boss calls the helicopter
        boss_level(1);
        g.debug_boss_set(0, 0);
        g.debug_boss_hurt(500, C_BULLET);
        run(g, 3, Input());
        CHECK(g.exfil_phase() >= 1, "killing a boss calls the helicopter");
    }

    // ---- 34. HUD face portraits for the whole roster ----
    {
        static u32 sheet[546 * 312];
        for (int dead = 0; dead < 2; dead++)
        for (int half = 0; half < 3; half++) {
            for (size_t i = 0; i < 546 * 312; i++) sheet[i] = RGB(90, 150, 210);
            for (int b = half * 14; b < std::min(NUM_BROS, half * 14 + 14); b++) {
                const Sprite &f = dead ? g.debug_face_dead(b) : g.debug_face(b);
                int cx = ((b - half * 14) % 7) * 78, cy = ((b - half * 14) / 7) * 78;
                for (int y = 0; y < f.h * 3; y++) for (int x = 0; x < f.w * 3; x++) { u32 c = f.at(x / 3, y / 3); if (c) sheet[size_t((cy + 3 + y) * 546 + cx + 3 + x)] = c; }
            }
            std::string p = outdir + (dead ? (half == 0 ? "/16_dead_a.png" : half == 1 ? "/16_dead_b.png" : "/16_dead_c.png") : (half == 0 ? "/16_faces_a.png" : half == 1 ? "/16_faces_b.png" : "/16_faces_c.png"));
            write_png(p.c_str(), sheet, 546, 312);
        }
        bool distinct = true;
        for (int a = 0; a < NUM_BROS && distinct; a++)
            for (int b = a + 1; b < NUM_BROS; b++) if (g.debug_face(a).d == g.debug_face(b).d) { distinct = false; printf("faces %d and %d are identical\n", a, b); break; }
        CHECK(distinct, "every bro has a distinct face portrait");
        g.debug_start_level(0, 13); // Indiana: the whip reaches far
        g.debug_clear_enemies();
        int gr; int x0 = 100;
        stage(x0, 70, gr);
        g.debug_start_level(0, 13);
        g.debug_clear_enemies();
        for (int x = x0; x < x0 + 70; x++) { for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x, y, T_AIR); for (int y = gr; y < 58; y++) g.debug_set_tile(x, y, T_DIRT); }
        g.debug_teleport(float((x0 + 5) * TS), float(gr * TS));
        g.player().invuln = 100000;
        g.player().a.face = 1;
        g.debug_add_enemy(E_MOOK, float((x0 + 5) * TS + 48), float(gr * TS), -1);
        Input f; f.fire = true;
        g.update(f);
        g.update(Input());
        CHECK(g.enemies_alive() == 0, "Indiana's whip reaches an enemy about 48 px away");
    }


    // ---- 36. bros are unlocked by freeing the prisoners hidden in the levels ----
    {
        Game u; // fresh save: only Rambro
        int perLevel[NUM_LEVELS] = {0};
        bool allHomed = true;
        for (int b = 1; b < NUM_BROS; b++) { int L = Game::bro_home_level(b); if (L < 0 || L >= NUM_LEVELS) allHomed = false; else perLevel[L]++; }
        CHECK(allHomed, "every bro (but Rambro) is hidden in exactly one level");
        int maxPer = 0; for (int L = 0; L < NUM_LEVELS; L++) maxPer = std::max(maxPer, perLevel[L]);
        CHECK(maxPer <= 2, "no level hides more than 2 bros");
        bool onlyRambro = u.owned(0);
        for (int b = 1; b < NUM_BROS; b++) if (u.owned(b)) onlyRambro = false;
        CHECK(onlyRambro, "a new game starts with Rambro only");
        // every level has at least as many cages as bros to free
        bool enoughCages = true;
        for (int L = 0; L < NUM_LEVELS; L++) {
            u.debug_start_level(L, -1);
            int hidden = 0;
            for (int k = 0; k < u.debug_cage_count(); k++) if (u.debug_cage_bro(k) > 0) hidden++;
            if (hidden < perLevel[L]) { enoughCages = false; printf("level %d: %d bros but only %d bro cages (%d cages)\n", L, perLevel[L], hidden, u.debug_cage_count()); }
        }
        CHECK(enoughCages, "every level has a cage for each of its bros");
        // free one
        u.debug_start_level(0, 0);
        int target = -1, cageIdx = -1;
        for (int k = 0; k < u.debug_cage_count(); k++) if (u.debug_cage_bro(k) > 0) { target = u.debug_cage_bro(k); cageIdx = k; break; }
        CHECK(target > 0 && !u.owned(target), "level 1-1 holds a bro that is not owned yet");
        float cx, cy;
        u.debug_cage_pos(cageIdx, cx, cy);
        u.player().invuln = 100000;
        u.debug_teleport(cx, cy);
        bool heard = false;
        for (int i = 0; i < 120 && u.state() == GS_PLAY; i++) { u.update(Input()); for (const SndEvt &e : u.sounds) if (e.id == SPEECH_BASE + SP_NEWBRO) heard = true; u.sounds.clear(); if (u.state() == GS_PLAY) { u.debug_teleport(cx, cy); } }
        CHECK(u.state() == GS_NEWBRO, "freeing a prisoner who is a new bro stops the action on the New Bro screen");
        CHECK(u.owned(target) && u.new_bro_now() == target, "the new bro joins the roster");
        for (int i = 0; i < 40; i++) { u.update(Input()); for (const SndEvt &e : u.sounds) if (e.id == SPEECH_BASE + SP_NEWBRO) heard = true; u.sounds.clear(); }
        shot(u, "18_newbro");
        CHECK(heard, "a voice says New Bro");
        CHECK(u.state() == GS_NEWBRO, "the screen waits for a button");
        Input a; a.jump = true;
        u.update(a);
        CHECK(u.state() == GS_PLAY, "A resumes the game");
        // completion: a level cleared without all its bros is not 100%
        SaveSlot sl;
        sl.mask = 1;
        int half = u.completion(sl);
        sl.broMask = 0xFFFFFFFFu;
        int full = u.completion(sl);
        printf("completion with level 1 cleared: %d%% without its bros, %d%% with them\n", half, full);
        CHECK(half < full && full > 0, "a cleared level only counts fully once all its bros are freed");
        // debug mode: everything unlocked, invincible
        Game d;
        d.set_debug(true);
        bool all = true;
        for (int b = 0; b < NUM_BROS; b++) if (!d.owned(b)) all = false;
        CHECK(all && d.unlocked_of(SaveSlot()) == LAST_LEVEL, "debug mode unlocks every bro and every level");
        d.debug_start_level(0, 5);
        d.debug_explode(d.player().a.x, d.player().a.y - 7, 30, 20, false);
        for (int i = 0; i < 30; i++) d.update(Input());
        CHECK(d.player().alive, "debug mode: the bro cannot die");
        Input sel; sel.select = true;
        int b0 = d.player().bro;
        d.update(sel);
        CHECK(d.player().bro == (b0 + 1) % NUM_BROS, "debug mode: SELECT switches to the next bro");
    }

    // ---- 37. roster grid: freed bros show their face, the others a bagged prisoner in a cage ----
    {
        Game r;
        r.st.slots[0].broMask = 0x1A2F;
        r.debug_set_slot(0);
        r.set_state(GS_ROSTER);
        for (int i = 0; i < 5; i++) r.update(Input());
        shot(r, "18_roster");
        CHECK(r.owned_count(r.st.slots[0]) == 8, "the roster counts the freed bros");
    }


    // ---- 38. platforms on pillars (background plane), progressive collapse ----
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        for (int x = x0 + 4; x < x0 + 16; x++) g.debug_set_tile(x, gr - 8, T_BRICK);               // platform
        int base = g.debug_pillar_total();
        g.debug_add_pillar(x0 + 5, gr - 7, gr); g.debug_add_pillar(x0 + 14, gr - 7, gr);            // two pillars under it
        int total0 = g.debug_pillar_total();
        CHECK(total0 - base == 14, "a platform is carried by two pillars");
        // people walk through the pillars, bullets fly through them
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        Input rt; rt.right = true;
        for (int i = 0; i < 150; i++) g.update(rt);
        CHECK(g.player().a.x > float((x0 + 8) * TS), "the bro walks under the platform, through its pillar");
        // a bullet does not hurt a pillar
        g.debug_teleport(float((x0 + 8) * TS), float(gr * TS));
        g.player().a.face = 1;
        Input f; f.fire = true;
        for (int i = 0; i < 90; i++) g.update(f);
        CHECK(g.debug_pillar_total() == total0, "bullets pass through pillars without damaging them");
        // blow up the bottom of one pillar: the rest does not vanish at once but crumbles up
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 30) * TS), float(gr * TS));
        g.debug_explode(float((x0 + 5) * TS + 4), float((gr - 1) * TS), 10, 10, true);
        run(g, 3, Input());
        int right_after = g.debug_pillar_total();
        run(g, 300, Input());
        int later = g.debug_pillar_total();
        printf("pillars: %d -> %d right after the blast -> %d later\n", total0, right_after, later);
        CHECK(right_after > later && later - base <= 7, "the damaged pillar crumbles progressively, from the break upward");
        CHECK(g.debug_tile(x0 + 4, gr - 8) == T_BRICK && g.debug_tile(x0 + 15, gr - 8) == T_BRICK, "the platform still stands on its other pillar");
        // the second pillar goes too: the platform finally comes down
        g.debug_explode(float((x0 + 14) * TS + 4), float((gr - 1) * TS), 10, 10, true);
        bool fell = false;
        for (int i = 0; i < 500 && !fell; i++) { g.update(Input()); if (g.chunk_count() > 0) fell = true; }
        run(g, 200, Input());
        CHECK(fell, "when the last pillar gives way the platform falls");
        CHECK(g.debug_tile(x0 + 8, gr - 8) == T_AIR, "the platform is gone from its place");
    }

    // ---- 39. digging: hard ground rarely collapses, sand always does ----
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        for (int x = x0; x < x0 + 8; x++) for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x, y, T_DIRT);   // a block of hard ground
        g.debug_damage_tile(x0 + 4, gr - 1, 9);                                                                 // dig out one tile at the bottom
        run(g, 80, Input());
        CHECK(g.chunk_count() == 0 && g.debug_tile(x0 + 4, gr - 2) == T_DIRT, "digging into hard ground does not make it collapse");
        stage(x0, 40, gr);
        for (int x = x0; x < x0 + 8; x++) for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x, y, T_SAND);    // a dune
        g.debug_damage_tile(x0 + 4, gr - 1, 9);
        bool slid = false;
        for (int i = 0; i < 120; i++) { g.update(Input()); if (g.chunk_count() > 0) slid = true; }
        printf("sand: slid=%d, tile above the hole now %d (sand=%d)\n", slid, g.debug_tile(x0 + 4, gr - 1), T_SAND);
        CHECK(slid && g.debug_tile(x0 + 4, gr - 1) == T_SAND, "dug-out sand always caves in and fills the hole");
    }

    // ---- 40. relief per world, secret chambers ----
    {
        static const char *names[NUM_WORLDS] = {"jungle", "desert", "night", "arctic", "city", "hell"};
        int range[NUM_WORLDS] = {0};
        int chambers = 0;
        for (int w = 0; w < NUM_WORLDS; w++) {
            int rmax = 0, hid = 0;
            for (int rep = 0; rep < 4; rep++) {
                g.debug_start_level(w * STAGES + 1, 0);
                int mn = 999, mx = 0;
                for (int x = 0; x < g.level_w(); x++) { int sv = g.debug_surf(x); if (sv > 52) continue; /* pit */ mn = std::min(mn, sv); mx = std::max(mx, sv); }
                rmax = std::max(rmax, mx - mn);
                hid += g.debug_hidden_cages();
                if (rep == 0) { char n[40]; snprintf(n, sizeof(n), "19_relief_%s", names[w]); g.debug_teleport(float(g.level_w() / 2 * TS), float(g.debug_surf(g.level_w() / 2) * TS)); run(g, 30, Input()); shot(g, n, 1); }
            }
            { float hx, hy; if ((w == 0 || w == 1) && g.debug_hidden_cage_pos(hx, hy)) { g.debug_teleport(hx - 30, hy + 8); g.player().invuln = 100000; run(g, 10, Input()); char n[40]; snprintf(n, sizeof(n), "19_chamber_%s", names[w]); shot(g, n, 1); } }
            range[w] = rmax;
            chambers += hid;
            printf("%s: surface range %d rows, %d hidden cages in 4 levels\n", names[w], rmax, hid);
        }
        CHECK(range[0] >= 18, "the jungle has huge mountains");
        CHECK(range[1] <= 14, "the desert only has dunes");
        CHECK(range[0] > range[1] + 6, "the jungle has far more relief than the desert");
        CHECK(chambers >= NUM_WORLDS * 2, "levels hide secret chambers with prisoners");
    }

    // ---- 41. katana bros show their sword, with a slash animation ----
    {
        int blade = -1; for (int b = 0; b < NUM_BROS; b++) if (BROS[b].prim == P_KATANA) { blade = b; break; }
        g.debug_start_level(0, blade);
        g.player().invuln = 100000;
        run(g, 40, Input());
        shot(g, "20_saber_idle", 4);
        Input f; f.fire = true;
        g.update(f); g.update(Input()); g.update(Input()); g.update(Input());
        CHECK(g.player().meleeT > 0, "the katana swing is under way");
        shot(g, "20_saber_swing1", 4);
        g.update(Input()); g.update(Input()); g.update(Input());
        shot(g, "20_saber_swing2", 4);
    }

    // ---- 42. cave roofs hold, flames dig, crates hold ammo ----
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        // a roof held by one block only: a floating slab of earth resting on a single pillar of earth
        for (int x = x0; x < x0 + 10; x++) for (int y = gr - 12; y < gr - 9; y++) g.debug_set_tile(x, y, T_DIRT);
        g.debug_set_tile(x0 + 5, gr - 9, T_DIRT);
        for (int y = gr - 8; y < gr; y++) g.debug_set_tile(x0 + 5, y, T_DIRT);
        g.debug_damage_tile(x0 + 5, gr - 1, 99);
        run(g, 60, Input());
        g.debug_damage_tile(x0 + 5, gr - 8, 99);
        g.debug_damage_tile(x0 + 5, gr - 9, 99);
        run(g, 200, Input());
        CHECK(g.chunk_count() == 0 && g.debug_tile(x0 + 2, gr - 11) == T_DIRT, "a cave roof does not fall when its last support is dug out");
        // ice / stone roofs hold too
        stage(x0, 40, gr);
        for (int x = x0; x < x0 + 10; x++) for (int y = gr - 12; y < gr - 9; y++) g.debug_set_tile(x, y, T_STONE);
        g.debug_set_tile(x0 + 5, gr - 9, T_STONE);
        g.debug_damage_tile(x0 + 5, gr - 9, 99);
        run(g, 200, Input());
        CHECK(g.chunk_count() == 0 && g.debug_tile(x0 + 2, gr - 11) == T_STONE, "a stone / ice roof does not fall either");
        // brick still falls
        stage(x0, 40, gr);
        for (int x = x0; x < x0 + 10; x++) g.debug_set_tile(x, gr - 10, T_BRICK);
        g.debug_set_tile(x0 + 5, gr - 9, T_BRICK);
        g.debug_damage_tile(x0 + 5, gr - 9, 99);
        bool fell = false;
        for (int i = 0; i < 100 && !fell; i++) { g.update(Input()); if (g.chunk_count() > 0) fell = true; }
        CHECK(fell, "an unsupported brick structure still falls");
        // flames dig into earth
        stage(x0, 40, gr);
        for (int x = x0 + 8; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x, y, T_DIRT);
        int flameBro = -1; for (int b = 0; b < NUM_BROS; b++) if (BROS[b].prim == P_FLAME) flameBro = b;
        g.debug_start_level(0, flameBro);
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.player().invuln = 100000;
        stage(x0, 40, gr);
        for (int x = x0 + 4; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x, y, T_DIRT);
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.player().a.face = 1;
        int before = 0; for (int x = x0 + 4; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) if (g.debug_tile(x, y) == T_DIRT) before++;
        Input f; f.fire = true;
        for (int i = 0; i < 150; i++) g.update(f);
        int after = 0; for (int x = x0 + 4; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) if (g.debug_tile(x, y) == T_DIRT) after++;
        printf("flamethrower dug %d -> %d dirt tiles\n", before, after);
        CHECK(after < before, "the flamethrower digs into the ground");
        // crates: some drop ammo
        g.debug_start_level(0, 0);
        int props0 = 0; { float px, py; for (int k = 0; k < 1000; k++) if (g.debug_prop(PR_AMMO, px, py)) { props0++; break; } }
        stage(x0, 40, gr);
        for (int i = 0; i < 40; i++) { g.debug_set_tile(x0 + i, gr - 1, T_CRATE); g.debug_damage_tile(x0 + i, gr - 1, 9); }
        run(g, 5, Input());
        (void)props0;
        CHECK(g.ammo_props() >= 3 && g.ammo_props() <= 25, "some of the broken crates (not all) leave ammo behind");
    }

    // ---- 43. spikes: safe to walk through, deadly to land on; barbed wire slows without killing ----
    {
        int gr; int x0 = 100;
        stage(x0, 40, gr);
        for (int x = x0 + 6; x < x0 + 11; x++) g.debug_set_tile(x, gr - 1, T_SPIKE);
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        Input rt; rt.right = true;
        for (int i = 0; i < 160; i++) g.update(rt);
        CHECK(g.player().alive && g.player().a.x > float((x0 + 12) * TS), "walking through spikes is harmless");
        g.player().invuln = 0;
        g.debug_teleport(float((x0 + 8) * TS), float(gr * TS)); g.player().a.vx = 0;
        run(g, 3, Input());
        Input jp; jp.jump = true;
        bool died = false;
        for (int i = 0; i < 60 && !died; i++) { g.update(jp); if (i > 10) jp.jump = false; if (!g.player().alive) died = true; }
        CHECK(died, "jumping onto spikes kills");
        // wire
        g.debug_start_level(0, 0);
        stage(x0, 40, gr);
        g.player().invuln = 0;
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        for (int i = 0; i < 50; i++) g.update(rt);
        float freeRun = g.player().a.x - float((x0 + 2) * TS);
        for (int x = x0 + 6; x < x0 + 40; x++) g.debug_set_tile(x, gr - 1, T_WIRE);
        g.debug_teleport(float((x0 + 6) * TS), float(gr * TS));
        float xs = g.player().a.x;
        for (int i = 0; i < 50; i++) g.update(rt);
        float wired = g.player().a.x - xs;
        printf("50 ticks: %.1f px free, %.1f px in barbed wire\n", freeRun, wired);
        for (int x = x0 + 44; x < x0 + 49; x++) g.debug_set_tile(x, gr - 1, T_SPIKE);
        shot(g, "21_wire", 4);
        CHECK(g.player().alive && wired < freeRun * 0.6f, "barbed wire slows the bro down but does not kill him");
    }

    // ---- 44. every bro's primary weapon digs into earth ----
    {
        for (int b = 0; b < NUM_BROS; b++) {
            int gr; int x0 = 100;
            stage(x0, 40, gr);
            g.player().bro = b; g.player().cool = 0;
            for (int x = x0 + 4; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) g.debug_set_tile(x, y, T_DIRT);
            g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
            g.player().invuln = 100000;
            g.player().a.face = 1;
            auto cnt = [&]() { int n = 0; for (int x = x0 + 4; x < x0 + 14; x++) for (int y = gr - 6; y < gr; y++) if (g.debug_tile(x, y) == T_DIRT) n++; return n; };
            int before = cnt();
            for (int i = 0; i < 400; i++) { Input f; f.fire = i >= 250 || (i % 8) < 5; f.right = true; g.update(f); }
            int after = cnt();
            printf("bro %d (%s) actual=%s: dirt %d -> %d\n", b, BROS[b].name, BROS[g.player().bro].name, before, after);
            CHECK(after < before, "primary weapon digs into earth");
        }
        // Y melee: a visible punch / rifle butt
        for (int k = 0; k < 2; k++) {
            int gr; int x0 = 100;
            int bb = k == 0 ? 0 : 3;
            stage(x0, 40, gr);
            g.player().bro = bb; g.player().cool = 0;
            g.debug_teleport(float((x0 + 6) * TS), float(gr * TS));
            g.player().invuln = 100000; g.player().a.face = 1;
            run(g, 3, Input());
            Input y; y.melee = true;
            g.update(y);
            for (int i = 0; i < 3; i++) g.update(Input());
            CHECK(g.player().punchT > 0, "pressing Y starts a visible strike animation");
            shot(g, k == 0 ? "22_punch_a" : "22_punch_b", 4);
        }
    }

    // ---- 45. stability: metal is fixed, and tunnelling through a cave never brings its roof down ----
    {
        int gr; int x0 = 100;
        // a. metal slab (with bricks on top) held by one earth column: dig the column out
        stage(x0, 40, gr);
        for (int x = x0 + 4; x < x0 + 12; x++) { g.debug_set_tile(x, gr - 9, T_METAL); g.debug_set_tile(x, gr - 10, T_BRICK); }
        for (int y = gr - 8; y < gr; y++) g.debug_set_tile(x0 + 7, y, T_DIRT);
        for (int y = gr - 8; y < gr; y++) g.debug_damage_tile(x0 + 7, y, 99);
        bool fell = false;
        for (int i = 0; i < 120; i++) { g.update(Input()); if (g.chunk_count() > 0) fell = true; }
        CHECK(!fell && g.debug_tile(x0 + 5, gr - 9) == T_METAL && g.debug_tile(x0 + 5, gr - 10) == T_BRICK, "metal blocks (and what rests on them) stay put when the ground below is dug out");
        // b. a lone metal block floating in the air
        stage(x0, 40, gr);
        g.debug_set_tile(x0 + 20, gr - 6, T_METAL);
        g.debug_set_tile(x0 + 20, gr - 5, T_DIRT);
        g.debug_damage_tile(x0 + 20, gr - 5, 99);
        fell = false;
        for (int i = 0; i < 80; i++) { g.update(Input()); if (g.chunk_count() > 0) fell = true; }
        CHECK(!fell && g.debug_tile(x0 + 20, gr - 6) == T_METAL, "a metal block never falls");
        // c. tunnel through a cave: in one side, out the other; the roof (with wood and crates mixed in) must stay up
        stage(x0, 40, gr);
        for (int x = x0; x < x0 + 36; x++) for (int y = gr - 12; y < gr; y++) g.debug_set_tile(x, y, T_DIRT);
        for (int x = x0 + 10; x < x0 + 24; x++) for (int y = gr - 5; y < gr; y++) g.debug_set_tile(x, y, T_AIR); // the cave
        g.debug_set_tile(x0 + 15, gr - 6, T_WOOD); g.debug_set_tile(x0 + 16, gr - 6, T_CRATE); g.debug_set_tile(x0 + 17, gr - 7, T_BRICK);
        int roof0 = 0; for (int x = x0 + 10; x < x0 + 24; x++) for (int y = gr - 12; y < gr - 5; y++) if (g.debug_tile(x, y) != T_AIR) roof0++;
        for (int x = x0 + 4; x < x0 + 10; x++) for (int y = gr - 4; y < gr; y++) g.debug_damage_tile(x, y, 99);   // entrance tunnel
        for (int x = x0 + 24; x < x0 + 31; x++) for (int y = gr - 4; y < gr; y++) g.debug_damage_tile(x, y, 99);  // exit tunnel
        // also cut the roof's last side supports on both flanks, so the roof only floats
        for (int y = gr - 12; y < gr - 4; y++) { g.debug_damage_tile(x0 + 9, y, 99); g.debug_damage_tile(x0 + 24, y, 99); }
        for (int x = x0 + 10; x < x0 + 24; x++) g.debug_damage_tile(x, gr - 13, 99);
        fell = false;
        for (int i = 0; i < 200; i++) { g.update(Input()); if (g.chunk_count() > 0) fell = true; }
        int roof1 = 0; for (int x = x0 + 10; x < x0 + 24; x++) for (int y = gr - 12; y < gr - 5; y++) if (g.debug_tile(x, y) != T_AIR) roof1++;
        printf("cave roof tiles %d -> %d\n", roof0, roof1);
        CHECK(!fell && roof1 == roof0, "tunnelling through a cave does not bring its roof down");
    }

    // ---- 46. jungle palms, far explosions are audible, gas bottles go off sooner ----
    {
        int gr; int x0 = 100;
        stage(x0, 60, gr);
        g.debug_start_level(0, 0);
        int nt = g.tree_count();
        CHECK(nt >= 8, "the jungle level has a good number of palm trees");
        stage(x0, 60, gr);
        float fx = float((x0 + 10) * TS), fy = float(gr * TS);
        g.debug_clear_trees();
        g.debug_add_tree(fx, fy, 80);
        g.debug_add_tree(fx + 200, fy, 60);
        int base = g.tree_count();
        g.debug_explode(fx + 4, fy - 40, 20, 14, true);
        for (int i = 0; i < 60; i++) g.update(Input());
        CHECK(g.tree_count() == base - 1, "an explosion knocks down the palm next to it and spares the far one");
        // gas bottle flying: a palm in its blast goes down too
        stage(x0, 60, gr);
        float gx = float((x0 + 30) * TS);
        g.debug_add_tree(gx + 20, fy, 70);
        int tb = g.tree_count();
        g.debug_add_gas(gx, fy);
        g.debug_ignite_gas(0);
        int ticks = 0;
        for (; ticks < 600 && g.gas_alive() > 0; ticks++) { g.update(Input()); g.sounds.clear(); }
        printf("gas bottle: %d ticks from ignition to blast\n", ticks);
        CHECK(ticks < 200, "a gas bottle goes off within about three seconds");
        // a blast 500 px away is still heard
        g.sounds.clear();
        g.debug_teleport(float((x0 + 2) * TS), fy);
        g.debug_explode(float((x0 + 2) * TS) + 500, fy - 10, 38, 20, false);
        bool heard = false;
        for (const SndEvt &e : g.sounds) if (e.id == SFX_BOOM_BIG && e.vol > 0.2f) heard = true;
        CHECK(heard, "a gas bottle blast off-screen is still audible");
        (void)tb;
        // flamethrower loop: stays on while firing, off after
        int fb = -1; for (int b = 0; b < NUM_BROS; b++) if (BROS[b].prim == P_FLAME) fb = b;
        stage(x0, 60, gr);
        g.player().bro = fb; g.player().cool = 0;
        Input ff; ff.fire = true;
        for (int i = 0; i < 20; i++) g.update(ff);
        CHECK(g.flame_volume() > 0.5f, "the flame loop plays while the trigger is held");
        for (int i = 0; i < 10; i++) g.update(Input());
        CHECK(g.flame_volume() == 0.0f, "the flame loop stops when the trigger is released");
        stage(x0, 60, gr);
        g.debug_add_tree(float((x0 + 10) * TS), fy, 80);
        shot(g, "23_palm", 2);
    }

    // ---- 47. kamikazes always wait for the end of their fuse; a blast lights it ----
    {
        int gr; int x0 = 100;
        // a. he charges, reaches you, and still waits for the fuse (~3 s) before going off
        stage(x0, 60, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 20) * TS), float(gr * TS));
        g.debug_add_enemy(E_BOMBER, float((x0 + 32) * TS), float(gr * TS), -1);
        int e0 = g.stat_explosions;
        int boomAt = -1;
        for (int i = 0; i < 400 && boomAt < 0; i++) { g.update(Input()); g.sounds.clear(); if (i == 100) shot(g, "24_fuse", 4); if (g.stat_explosions > e0) boomAt = i; }
        printf("kamikaze blew up after %d ticks\n", boomAt);
        CHECK(boomAt > 150 && boomAt < 260, "a kamikaze reaching you still waits for his fuse to run out");
        // b. a blast next to him kills him and lights his fuse: he goes off a moment later
        stage(x0, 60, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.debug_add_enemy(E_BOMBER, float((x0 + 40) * TS), float(gr * TS), -1);
        e0 = g.stat_explosions;
        g.debug_explode(float((x0 + 40) * TS) + 10, float(gr * TS) - 6, 12, 8, true);
        int afterBlast = g.stat_explosions - e0; // only the blast itself
        CHECK(g.enemies_alive() == 0, "a blast next to a kamikaze kills him");
        for (int i = 0; i < 40; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.stat_explosions - e0 == afterBlast, "...but his belt has not gone off yet");
        for (int i = 0; i < 200; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.stat_explosions - e0 == afterBlast + 1, "...it goes off once his (standard) fuse has run out");
        // c. killed some other way before lighting the fuse: no blast
        stage(x0, 60, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
        g.debug_add_enemy(E_BOMBER, float((x0 + 50) * TS), float(gr * TS), -1);
        e0 = g.stat_explosions;
        g.debug_hurt_enemy(0, 9, C_MELEE);
        for (int i = 0; i < 300; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.enemies_alive() == 0 && g.stat_explosions == e0, "a kamikaze cut down by a blade before lighting his fuse just dies");
        // d. shot: sometimes the bullet hits the explosives, and then the blast is immediate
        int instant = 0;
        for (int k = 0; k < 40; k++) {
            stage(x0, 60, gr);
            g.player().invuln = 100000;
            g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
            g.debug_add_enemy(E_BOMBER, float((x0 + 50) * TS), float(gr * TS), -1);
            e0 = g.stat_explosions;
            g.debug_hurt_enemy(0, 9, C_BULLET);
            g.update(Input()); g.sounds.clear();
            if (g.stat_explosions > e0) instant++;
        }
        printf("shot kamikazes that blew up at once: %d / 40\n", instant);
        CHECK(instant >= 5 && instant <= 30, "a shot kamikaze sometimes (not always) explodes at once");
        // e. crushed: instant blast. lightning: lights the fuse
        for (int k = 0; k < 2; k++) {
            stage(x0, 60, gr);
            g.player().invuln = 100000;
            g.debug_teleport(float((x0 + 2) * TS), float(gr * TS));
            g.debug_add_enemy(E_BOMBER, float((x0 + 50) * TS), float(gr * TS), -1);
            e0 = g.stat_explosions;
            g.debug_hurt_enemy(0, 9, k == 0 ? C_CRUSH : C_LIGHTNING);
            g.update(Input()); g.sounds.clear();
            if (k == 0) CHECK(g.stat_explosions > e0, "a crushed kamikaze explodes at once");
            else {
                CHECK(g.stat_explosions == e0, "a kamikaze struck by lightning does not explode at once");
                for (int i = 0; i < 260; i++) { g.update(Input()); g.sounds.clear(); }
                CHECK(g.stat_explosions > e0, "...his fuse is lit and he goes off later");
            }
        }
    }

    // ---- 48. spikes go when the ground under them is dug out; Brofessional fires one silenced round; chainsaw loop ----
    {
        int gr; int x0 = 100;
        stage(x0, 60, gr);
        g.debug_set_tile(x0 + 10, gr - 1, T_SPIKE);
        g.debug_set_tile(x0 + 12, gr - 1, T_WIRE);
        g.debug_damage_tile(x0 + 10, gr, 99);
        g.debug_damage_tile(x0 + 12, gr, 99);
        CHECK(g.debug_tile(x0 + 10, gr - 1) == T_AIR && g.debug_tile(x0 + 12, gr - 1) == T_AIR, "digging out the ground under spikes / barbed wire removes them");
        // the Brofessional: a single round per shot
        int pro = -1, hard = -1, saw = -1;
        for (int b = 0; b < NUM_BROS; b++) { if (!strcmp(BROS[b].name, "THE BROFESSIONAL")) pro = b; if (!strcmp(BROS[b].name, "BRO HARD")) hard = b; if (BROS[b].prim == P_CHAINSAW) saw = b; }
        stage(x0, 60, gr);
        g.player().bro = pro; g.player().cool = 0; g.player().a.face = 1;
        Input fi; fi.fire = true;
        g.update(fi);
        CHECK(g.proj_count() == 1, "The Brofessional fires a single round");
        stage(x0, 60, gr);
        g.player().bro = hard; g.player().cool = 0; g.player().a.face = 1;
        g.update(fi);
        CHECK(g.proj_count() == 2, "Bro Hard still fires two");
        // chainsaw: the engine loop runs while firing and stops after release
        stage(x0, 60, gr);
        g.player().bro = saw; g.player().cool = 0; g.player().a.face = 1;
        for (int i = 0; i < 20; i++) g.update(fi);
        CHECK(g.saw_volume() > 0.5f, "the chainsaw engine loop plays while the trigger is held");
        shot(g, "25_chainsaw", 4);
        for (int i = 0; i < 10; i++) g.update(Input());
        CHECK(g.saw_volume() == 0.0f, "the chainsaw sound stops when the trigger is released");
    }

    // ---- 49. machine gunner, softer soldiers, spikes vs soldiers, re-aiming delay, blast zones, crouching ----
    {
        int gr; int x0 = 100;
        auto ebullets = [&]() { int n = 0; for (size_t i = 0; i < g.proj_count(); i++) { float vx, vy; int k; if (g.debug_proj(i, vx, vy, k) && k == PK_EBULLET) n++; } return n; };
        // a. the gunner's salvo
        stage(x0, 80, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 8) * TS), float(gr * TS));
        g.debug_add_enemy(E_GUNNER, float((x0 + 22) * TS), float(gr * TS), -1);
        int most = 0;
        for (int i = 0; i < 110; i++) { g.update(Input()); g.sounds.clear(); most = std::max(most, ebullets()); }
        printf("machine gunner: up to %d rounds in flight\n", most);
        CHECK(most >= 13 && most <= 17, "the machine gunner fires a salvo of about 15 rounds");
        shot(g, "26_gunner", 4);
        // b. soldiers die easily
        stage(x0, 80, gr);
        int hv = g.debug_add_enemy(E_HEAVY, float((x0 + 30) * TS), float(gr * TS), -1);
        g.debug_hurt_enemy(hv, 1, C_BULLET);
        CHECK(g.enemies_alive() == 1, "one small bullet does not kill a heavy soldier yet");
        g.debug_hurt_enemy(hv, 1, C_BULLET);
        CHECK(g.enemies_alive() == 0, "two small bullets kill a heavy soldier");
        hv = g.debug_add_enemy(E_GUNNER, float((x0 + 30) * TS), float(gr * TS), -1);
        g.debug_hurt_enemy(hv, 2, C_BULLET);
        CHECK(g.enemies_alive() == 0, "one big-calibre round kills a machine gunner");
        // c. spikes: walking is safe for a soldier too, landing on them kills
        stage(x0, 80, gr);
        for (int x = x0 + 34; x < x0 + 48; x++) g.debug_set_tile(x, gr - 1, T_SPIKE);
        g.debug_teleport(float((x0 + 30) * TS), float(gr * TS));
        g.debug_add_enemy(E_MOOK, float((x0 + 40) * TS + 4), float(gr * TS), -1);
        for (int i = 0; i < 20; i++) { g.update(Input()); g.sounds.clear(); }
        CHECK(g.enemies_alive() == 1, "a soldier standing on spikes is fine");
        g.debug_add_enemy(E_MOOK, float((x0 + 40) * TS + 4), float(gr * TS - 50), -1);
        for (int i = 0; i < 60; i++) { g.update(Input()); g.sounds.clear(); }
        { float ex_, ey_; int et_; for (int q = 0; q < g.debug_enemy_count(); q++) if (g.debug_enemy(q, ex_, ey_, et_)) printf("enemy %d at %.1f,%.1f (ground row y=%d) spike below=%d\n", q, ex_, ey_, gr * TS, int(g.debug_tile(int(ex_ / TS), int((ey_ - 2) / TS)) == T_SPIKE)); }
        CHECK(g.enemies_alive() == 1, "...but one who falls onto them dies");
        // d. spotted again: he must re-aim before shooting
        stage(x0, 120, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        g.debug_add_enemy(E_MOOK, float((x0 + 22) * TS), float(gr * TS), -1);
        for (int i = 0; i < 200; i++) { g.update(Input()); g.sounds.clear(); }
        g.debug_teleport(float((x0 + 100) * TS), float(gr * TS));
        for (int i = 0; i < 172; i++) { g.update(Input()); g.sounds.clear(); } // away long enough for his shots to expire, short enough that he is still alert
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        printf("bullets in flight when he sees you again: %d, enemies %d\n", ebullets(), g.enemies_alive());
        int first = -1;
        for (int i = 0; i < 200 && first < 0; i++) { g.update(Input()); g.sounds.clear(); if (ebullets() > 0) first = i; }
        printf("soldier re-aims for %d ticks before shooting again\n", first);
        CHECK(first >= 30 || first < 0, "a soldier who sees you again waits a moment before firing");
        // e. blast zones
        for (int z = 0; z < 3; z++) {
            stage(x0, 80, gr);
            g.player().invuln = 0;
            float bx = float((x0 + 40) * TS), by = float(gr * TS) - 7;
            float dist = z == 0 ? 8.0f : z == 1 ? 30.0f : 90.0f;
            g.debug_teleport(bx + dist, float(gr * TS));
            g.player().a.vx = 0;
            g.debug_explode(bx, by, 24, 14, false);
            if (z == 0) CHECK(!g.player().alive, "right next to a blast: dead");
            else if (z == 1) CHECK(g.player().alive && g.player().a.vx > 1.5f && g.player().knockT > 0, "in the outer zone of a blast: thrown away, not killed");
            else CHECK(g.player().alive && g.player().a.vx == 0.0f, "far from a blast: untouched");
        }
        // f. crouching
        stage(x0, 80, gr);
        g.player().invuln = 100000;
        run(g, 5, Input());
        Input dn; dn.down = true;
        for (int i = 0; i < 5; i++) g.update(dn);
        CHECK(g.player().crouch, "holding down on the ground crouches");
        shot(g, "27_crouch", 4);
        run(g, 5, Input());
        CHECK(!g.player().crouch, "releasing down stands up again");
    }

    // ---- 50. own blasts throw you, steeper wall jump with up, punch for blade fighters, ammo box message ----
    {
        int gr; int x0 = 100;
        // a. your own explosion: harmless, but it throws you when you stand too close
        stage(x0, 80, gr);
        g.player().invuln = 0;
        float bx = float((x0 + 40) * TS), by = float(gr * TS) - 7;
        g.debug_teleport(bx + 6, float(gr * TS));
        g.player().a.vx = 0;
        g.debug_explode(bx, by, 24, 14, true);
        CHECK(g.player().alive && g.player().a.vx > 1.5f && g.player().knockT > 0, "your own blast throws you when you are right next to it (no damage)");
        stage(x0, 80, gr);
        g.player().invuln = 0;
        g.debug_teleport(bx + 90, float(gr * TS));
        g.player().a.vx = 0;
        g.debug_explode(bx, by, 24, 14, true);
        CHECK(g.player().alive && g.player().a.vx == 0.0f, "...and leaves you alone from afar");
        // b. wall jump: holding up gives a steeper jump
        float vyUp = 0, vyFlat = 0, vxUp = 0, vxFlat = 0;
        for (int k = 0; k < 2; k++) {
            stage(x0, 80, gr);
            for (int y = gr - 14; y < gr; y++) g.debug_set_tile(x0 + 20, y, T_METAL);
            g.player().invuln = 100000;
            g.debug_teleport(float((x0 + 20) * TS) - 4.5f, float(gr * TS) - 40);
            g.player().a.vy = 0;
            Input w; w.right = true; w.up = k == 0;
            for (int i = 0; i < 10; i++) g.update(w); // let the ground-jump grace expire: he slides down the wall
            w.jump = true;
            g.update(w);
            if (k == 0) { vyUp = g.player().a.vy; vxUp = g.player().a.vx; } else { vyFlat = g.player().a.vy; vxFlat = g.player().a.vx; }
        }
        printf("wall jump: with up vy=%.2f vx=%.2f, without vy=%.2f vx=%.2f\n", vyUp, vxUp, vyFlat, vxFlat);
        CHECK(vyUp < vyFlat - 0.3f && fabsf(vxUp) < fabsf(vxFlat), "holding up when wall-jumping gives a steeper jump");
        // c. punching with a blade fighter
        int blade = -1; for (int b = 0; b < NUM_BROS; b++) if (BROS[b].prim == P_KATANA) { blade = b; break; }
        stage(x0, 80, gr);
        g.player().bro = blade; g.player().cool = 0; g.player().a.face = 1;
        Input y; y.melee = true;
        g.update(y);
        CHECK(g.player().punchT > 0 && g.player().meleeT == 0, "a katana bro throws a punch with Y (no sword swing)");
        shot(g, "28_blade_punch", 4);
        // d. ammo box: message when the special is already full, box stays
        stage(x0, 80, gr);
        g.player().bro = 0; g.player().spec = BROS[0].specAmmo;
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        g.debug_add_prop(PR_AMMO, float((x0 + 10) * TS), float(gr * TS));
        int before = g.ammo_props();
        g.update(Input());
        CHECK(g.toast_ticks() > 0 && g.ammo_props() == before, "walking onto an ammo box with full ammo shows a message and leaves the box");
        g.player().spec = 0;
        g.toast_ticks();
        g.update(Input());
        CHECK(g.player().spec == BROS[0].specAmmo && g.ammo_props() == before - 1, "...and with room left the box is used");
    }

    // ---- 51. new bros: cryo gun, stakes, laughing only from soldiers who saw him, dead faces ----
    {
        int gr; int x0 = 100;
        int cryo = -1, stakes = -1;
        for (int b = 0; b < NUM_BROS; b++) { if (BROS[b].prim == P_CRYO) cryo = b; if (!strcmp(BROS[b].primName, "WOODEN STAKES")) stakes = b; }
        CHECK(NUM_BROS >= 35 && cryo > 0 && stakes > 0, "Broheart, Bronan, Broffy and Demolition Bro joined the roster");
        stage(x0, 80, gr);
        g.player().bro = cryo; g.player().cool = 0; g.player().a.face = 1;
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        int mk = g.debug_add_enemy(E_MOOK, float((x0 + 22) * TS), float(gr * TS), 1);
        Input fi; fi.fire = true;
        for (int i = 0; i < 40; i++) { g.update(fi); g.sounds.clear(); }
        CHECK(g.enemies_alive() == 1 && g.debug_frozen(mk), "the cryo gun freezes a soldier solid instead of killing him");
        float fx0, fy0; int ft;
        g.debug_enemy(mk, fx0, fy0, ft);
        for (int i = 0; i < 60; i++) { g.update(Input()); g.sounds.clear(); }
        float fx1, fy1;
        g.debug_enemy(mk, fx1, fy1, ft);
        CHECK(g.debug_frozen(mk) && fabsf(fx1 - fx0) < 0.5f, "a frozen soldier stays put");
        g.debug_hurt_enemy(mk, 1, C_BULLET);
        CHECK(g.enemies_alive() == 0, "any hit shatters a frozen soldier");
        // soldiers who never saw the bro do not laugh when he dies
        stage(x0, 80, gr);
        g.player().invuln = 100000;
        g.debug_teleport(float((x0 + 10) * TS), float(gr * TS));
        for (int k = 0; k < 8; k++) g.debug_add_enemy(E_MOOK, float((x0 + 22 + k) * TS), float(gr * TS), 1); // facing away
        g.player().invuln = 0;
        g.debug_explode(g.player().a.x, g.player().a.y - 7, 20, 16, false);
        int laughs = 0;
        for (int i = 0; i < 100; i++) { g.update(Input()); g.sounds.clear(); laughs = std::max(laughs, g.laughing_count()); }
        CHECK(laughs == 0, "soldiers who did not have the bro in sight do not laugh");
        // dead faces differ from live ones
        bool alldiff = true;
        for (int b = 0; b < NUM_BROS; b++) if (g.debug_face(b).d == g.debug_face_dead(b).d) alldiff = false;
        CHECK(alldiff, "every bro has a dead face of his own");
    }

    // 17. portrait HUD screenshot
    g.debug_start_level(0, 5);
    run(g, 200, Input());
    shot(g, "08_hud");

    printf("\n%s (%d failures)\n", g_fail ? "SOME TESTS FAILED" : "ALL CHECKS PASSED", g_fail);
    return g_fail ? 1 : 0;
}
