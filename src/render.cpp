// Broyoo - rendering, HUD and menus
#include "game.h"
#include "audio.h"
#include "phrases.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

const Theme &theme_for(int lvl);

static u32 lerp_c(u32 a, u32 b, int t, int tmax) {
    int r = (int((a >> 16) & 255) * (tmax - t) + int((b >> 16) & 255) * t) / tmax;
    int g = (int((a >> 8) & 255) * (tmax - t) + int((b >> 8) & 255) * t) / tmax;
    int bl = (int(a & 255) * (tmax - t) + int(b & 255) * t) / tmax;
    return RGB(r, g, bl);
}

static void fill_circle(Gfx &g, int cx, int cy, int r, u32 c) {
    for (int y = -r; y <= r; y++) {
        int w = int(sqrtf(float(r * r - y * y)));
        g.rect(cx - w, cy + y, w * 2 + 1, 1, c);
    }
}

static void sprite_ghost(Gfx &g, const Sprite &s, int x, int y, bool flip, int phase) {
    for (int j = 0; j < s.h; j++)
        for (int i = 0; i < s.w; i++) {
            u32 c = s.at(flip ? s.w - 1 - i : i, j);
            if (c && ((i + j + phase) & 1)) g.pset(x + i, y + j, c);
        }
}

// ---- background ---------------------------------------------------------------------
void Game::draw_bg(Gfx &g) {
    for (int y = 0; y < VH; y++) g.rect(0, y, VW, 1, lerp_c(th.skyTop, th.skyBot, y, VH));
    if (th.night) {
        for (int i = 0; i < 40; i++) {
            int sx = (i * 97 + 13) % VW, sy = (i * 53 + 7) % 130;
            g.pset(sx, sy, ((tick / 20 + i) % 7 == 0) ? RGB(255, 255, 255) : RGB(170, 170, 210));
        }
        fill_circle(g, 250, 40, 12, RGB(235, 235, 210));
        fill_circle(g, 255, 37, 10, th.skyTop);
    } else if (world == 5) {
        fill_circle(g, 80, 130, 26, RGB(255, 190, 60));
    } else {
        for (int i = 0; i < 4; i++) {
            int cx = int(fmodf(float(i * 110) + float(tick) * 0.05f - camX * 0.08f, 460.0f));
            if (cx < -50) cx += 460;
            int cy = 30 + (i * 37) % 60;
            g.rect_blend(cx, cy, 36, 7, RGB(255, 255, 255), 150);
            g.rect_blend(cx + 6, cy - 4, 22, 5, RGB(255, 255, 255), 150);
        }
    }
    float base2 = 150.0f - camY * 0.12f, base1 = 175.0f - camY * 0.2f;
    for (int x = 0; x < VW; x++) {
        float wx2 = float(x) + camX * 0.15f, wx1 = float(x) + camX * 0.3f;
        int h2 = int(base2 + sinf(wx2 * 0.013f) * 22 + sinf(wx2 * 0.041f) * 8);
        int h1 = int(base1 + sinf(wx1 * 0.021f + 1.5f) * 16 + sinf(wx1 * 0.057f) * 5);
        g.rect(x, h2, 1, VH - h2, th.hill2);
        g.rect(x, h1, 1, VH - h1, th.hill1);
    }
}

// ---- tiles --------------------------------------------------------------------------
void Game::draw_tile(Gfx &g, int tx, int ty, int sx, int sy) {
    uint8_t t = tile[ty * MAXW + tx];
    if (t == T_AIR) {
        // dug-out ground (caves, craters) shows a dark earth backdrop instead of the sky
        if (ty > surf[tx] + 1 && surf[tx] < H) g.rect(sx, sy, TS, TS, lerp_c(th.dirtDk, RGB(0, 0, 0), 1, 2));
        int ph = pillar[ty * MAXW + tx];
        if (ph > 0) { // support pillar: behind the action, so people and bullets pass in front of it
            u32 w = lerp_c(th.woodDk, RGB(0, 0, 0), 1, 3);
            g.rect(sx + 2, sy, 4, TS, w);
            g.rect(sx + 2, sy, 1, TS, lerp_c(w, RGB(255, 255, 255), 1, 6));
            g.rect(sx + 5, sy, 1, TS, lerp_c(w, RGB(0, 0, 0), 1, 3));
            if ((ty & 1) == 0) g.rect(sx + 2, sy + 3, 4, 1, lerp_c(w, RGB(0, 0, 0), 1, 4));
            if (ph < 6) { g.pset(sx + 3, sy + 1, RGB(15, 10, 8)); g.pset(sx + 4, sy + 5, RGB(15, 10, 8)); if (ph < 3) g.pset(sx + 3, sy + 4, RGB(15, 10, 8)); }
        }
        return;
    }
    g.sprite(tileSpr[t][tvar[ty * MAXW + tx] & 3], sx, sy);
    if ((t == T_DIRT || t == T_SAND) && tget(tx, ty - 1) == T_AIR) {
        for (int x = 0; x < TS; x++) {
            int gh = 2 + (((tx * 5 + x * 3) % 4) == 0 ? 1 : 0);
            g.rect(sx + x, sy, 1, gh, th.grass);
            if (gh == 3) g.pset(sx + x, sy + 2, lerp_c(th.grass, th.dirt, 1, 2));
        }
    }
    if (t == T_DIRT || t == T_SAND) {
        // edge shading toward open air
        if (tget(tx - 1, ty) == T_AIR) g.rect(sx, sy, 1, TS, th.dirtDk);
        if (tget(tx + 1, ty) == T_AIR) g.rect(sx + TS - 1, sy, 1, TS, th.dirtDk);
        if (tget(tx, ty + 1) == T_AIR) g.rect(sx, sy + TS - 1, TS, 1, th.dirtDk);
    }
    int bl = blood[ty * MAXW + tx];
    if (bl > 0) { // blood splatters, concentrated on exposed surfaces
        bool topOpen = tget(tx, ty - 1) == T_AIR;
        int n = 2 + bl / 28;
        for (int i = 0; i < n && i < 12; i++) {
            uint32_t hh = uint32_t(tx * 73856093) ^ uint32_t(ty * 19349663) ^ uint32_t(i * 2654435761u);
            hh ^= hh >> 15;
            int px = int(hh % 8), py = topOpen && i < n * 2 / 3 ? int((hh >> 5) % 3) : int((hh >> 5) % 8);
            g.pset(sx + px, sy + py, (hh & 3) ? RGB(150 + int((hh >> 9) & 31), 12, 14) : RGB(105, 8, 10));
            if (topOpen && py < 2 && (hh & 4)) g.pset(sx + (px + 1) % 8, sy + py, RGB(135, 10, 12));
        }
    }
    if (fireT[ty * MAXW + tx] > 0) { // burning: flickering flames dancing on top
        for (int k = 0; k < 3; k++) {
            int fx = sx + 1 + ((tick / 3 + k * 3 + tx * 5) % 6), fh = 3 + ((tick / 2 + k * 2 + ty) % 4);
            g.rect(fx, sy - fh + 2, 2, fh, k == 1 ? RGB(255, 225, 90) : RGB(255, 115, 30));
        }
        g.rect_blend(sx - 2, sy - 2, TS + 4, TS + 4, RGB(255, 120, 30), 45);
    }
    int mx = tile_max_hp(t), hp = thp[ty * MAXW + tx];
    if (t != T_METAL && hp < mx) {
        int n = (mx - hp) * 4 / mx + 1;
        for (int i = 0; i < n; i++) {
            uint32_t h = uint32_t(tx * 73856093) ^ uint32_t(ty * 19349663) ^ uint32_t(i * 83492791);
            h ^= h >> 11;
            g.pset(sx + int(h % 8), sy + int((h >> 4) % 8), RGB(20, 12, 10));
        }
    }
}

// ---- characters -----------------------------------------------------------------------
// Sword of the katana bros: carried at the ready, swung in an overhead arc when slashing, thrust forward on a dash.
void Game::draw_saber(Gfx &g, const Player &p, int sx, int sy) {
    const BroDef &d = BROS[p.bro];
    const int f = p.a.face >= 0 ? 1 : -1;
    const int hx = sx + f * 5, hy = sy - 9;      // the hand
    float ang;                                    // degrees above the horizontal, towards the facing side
    int len = 15;
    bool swing = p.meleeT > 0, dash = p.dashT > 0;
    if (swing) {
        float prog = 1.0f - float(p.meleeT) / (p.meleeT > 10 ? 16.0f : 10.0f);   // 0 = raised, 1 = low and forward
        ang = 110.0f - 150.0f * prog;
        len = 18;
    } else if (dash) ang = 0.0f;
    else ang = 62.0f + (p.a.ground ? 0.0f : -12.0f);
    float rad = ang * 3.14159265f / 180.0f;
    float cx = cosf(rad) * float(f), cy = -sinf(rad);
    if (swing) { // fading trail of the previous angles
        for (int k = 1; k <= 6; k++) {
            float r2 = (ang + 9.0f * float(k)) * 3.14159265f / 180.0f;
            for (int i = len - 9; i <= len; i++) g.pset(hx + int(cosf(r2) * float(f) * float(i)), hy + int(-sinf(r2) * float(i)), k <= 2 ? RGB(240, 245, 255) : k <= 4 ? RGB(175, 190, 215) : RGB(120, 135, 165));
        }
    }
    for (int i = 0; i <= len; i++) {
        int px = hx + int(cx * float(i)), py = hy + int(cy * float(i));
        if (i < 3) g.pset(px, py, RGB(90, 55, 30));                                  // hilt
        else if (i == 3) { g.pset(px, py - 1, d.extra); g.pset(px, py + 1, d.extra); g.pset(px, py, RGB(200, 170, 60)); } // guard
        else { bool steep = std::fabs(cy) > std::fabs(cx); g.pset(px, py, d.gun); g.pset(px + (steep ? 1 : 0), py + (steep ? 0 : 1), RGB(245, 248, 255)); }
    }
}

// Close-combat strike (Y): a punch, or a rifle-butt for the heavy-gun bros. The arm lashes out, holds, then recoils.
void Game::draw_punch(Gfx &g, const Player &p, int sx, int sy) {
    const BroDef &d = BROS[p.bro];
    const int f = p.a.face >= 0 ? 1 : -1;
    int total = p.meleeT > 6 ? 10 : 8;
    float prog = 1.0f - float(p.punchT) / float(total);               // 0..1
    float ext = prog < 0.4f ? prog / 0.4f : 1.0f - (prog - 0.4f) / 0.6f * 0.8f; // out fast, back slower
    int len = 3 + int(ext * 10.0f);
    int hy = sy - 9;
    bool butt;
    switch (d.prim) { case P_MG: case P_BURST: case P_SHOTGUN: case P_DUAL: case P_MINIGUN: case P_ROCKET: case P_HOMING: case P_RAY:
                      case P_SILENT: case P_REVOLVER: case P_CHARGE: case P_FLAME: butt = true; break; default: butt = false; }
    int x0 = sx + f * 4;
    auto bar = [&](int a, int b, int th, u32 c) { int xa = f > 0 ? x0 + a : x0 - b, w = b - a; g.rect(xa, hy - th / 2, w, th, c); };
    if (butt) {
        bar(0, len, 3, RGB(70, 45, 28));                               // stock
        bar(len - 3, len + 1, 4, d.gun);                               // gun body swung forward
        bar(0, len, 1, RGB(200, 200, 205));
    } else {
        bar(0, len - 3, 3, d.skin);                                    // arm
        bar(len - 4, len + 1, 5, d.skin);                              // fist
        bar(len - 4, len + 1, 1, lerp_c(d.skin, RGB(0, 0, 0), 1, 3));   // knuckles
        bar(0, 3, 3, d.shirt);                                         // sleeve
    }
    if (ext > 0.8f) { // impact flash
        int ix = x0 + f * (len + 3);
        g.rect(ix - 1, hy - 1, 3, 3, RGB(255, 245, 160));
        g.rect(ix - 3, hy, 7, 1, RGB(255, 220, 90));
        g.rect(ix, hy - 3, 1, 7, RGB(255, 220, 90));
    }
    if (prog < 0.5f) for (int k = 0; k < 3; k++) bar(-2 - k * 2, -k * 2 + 0, 1, RGB(235, 235, 245)); // speed lines behind
}

// Chainsaw of Ash: red engine block with a handle, a long guide bar and a chain whose teeth run while he saws. Shakes while firing.
void Game::draw_chainsaw(Gfx &g, const Player &p, int sx, int sy) {
    const BroDef &d = BROS[p.bro];
    const int f = p.a.face >= 0 ? 1 : -1;
    bool run = p.meleeT > 0 || sawT > 0;
    int shake = run ? ((tick & 1) ? 1 : 0) : 0;
    int hx = sx + f * 4, hy = sy - 8 + shake;
    auto bx = [&](int off, int w) { return f > 0 ? hx + off : hx - off - w + 1; };   // x of a horizontal span starting `off` px in front of the hand
    g.rect(bx(-2, 3), hy + 1, 3, 3, RGB(40, 40, 46));                                 // rear handle
    g.rect(bx(0, 7), hy - 2, 7, 6, d.gun);                                            // engine block
    g.rect(bx(0, 7), hy - 2, 7, 1, lerp_c(d.gun, RGB(255, 255, 255), 1, 5));
    g.rect(bx(1, 2), hy - 1, 2, 1, RGB(240, 240, 245));                               // pull-start cover
    g.rect(bx(5, 2), hy + 2, 2, 2, RGB(30, 30, 34));                                  // front grip
    g.rect(bx(7, 16), hy - 1, 16, 4, RGB(190, 195, 205));                             // guide bar
    g.rect(bx(7, 16), hy, 16, 2, RGB(120, 125, 135));
    for (int i = 0; i < 16; i++) { // chain teeth along both edges, marching when it runs
        bool on = ((i + (run ? tick / 2 : 0)) & 1) == 0;
        if (on) { g.pset(bx(7 + i, 1), hy - 1, RGB(60, 60, 66)); g.pset(bx(7 + (15 - i), 1), hy + 2, RGB(60, 60, 66)); }
    }
    g.rect(bx(22, 2), hy - 1, 2, 4, RGB(150, 155, 165));                              // rounded tip
}

void Game::draw_actor_bro(Gfx &g, const Actor &a, int bro, int anim, bool air, bool flash, bool ghost) {
    int frame = air ? 3 : (fabsf(a.vx) > 0.2f ? 1 + (anim / 6) % 2 : 0);
    const Sprite &s = broSpr[bro][frame];
    int sx = int(roundf(a.x)) - 9 - int(camX) - shakeX, sy = int(roundf(a.y)) - 17 - int(camY) - shakeY;
    if (flash) g.sprite_tint(s, sx, sy, a.face < 0, RGB(255, 255, 255));
    else if (ghost) sprite_ghost(g, s, sx, sy, a.face < 0, tick);
    else g.sprite(s, sx, sy, a.face < 0);
}

void Game::draw_enemy(Gfx &g, const Enemy &e, int sx, int sy) {
    if (e.type == E_BOSS) { draw_boss(g, e, sx, sy); return; }
    int frame = !e.a.ground ? 3 : (fabsf(e.a.vx) > 0.1f ? 1 + (e.anim / 8) % 2 : 0);
    if (e.type == E_OFFICER) frame = 0;
    const Sprite &s = enSpr[e.type][frame];
    int bob = e.laugh > 0 ? ((e.laugh / 3) % 2 ? -2 : 0) : 0; // shaking with laughter
    int x = sx - 9, y = sy - (e.type == E_DOG ? 11 : 17) + bob;
    if (e.flash > 0) g.sprite_tint(s, x, y, e.a.face < 0, RGB(255, 255, 255));
    else g.sprite(s, x, y, e.a.face < 0);
    if (e.type == E_BOMBER) { // explosive belt: a row of dynamite sticks and a lit-fuse flame
        int bx = sx - 5, by = y + 9;
        g.rect(bx - 1, by, 12, 4, RGB(50, 35, 25));
        for (int k = 0; k < 4; k++) {
            g.rect(bx + k * 3, by - 1, 2, 5, RGB(210, 40, 35));
            g.rect(bx + k * 3, by - 1, 2, 1, RGB(255, 220, 90));
        }
        if (e.aux > 0) { // lit fuse: a small flickering flame on the belt
            int fl = (e.anim / 2) % 3;
            g.rect(bx + 4, by - 3 - fl, 2, 2 + fl, RGB(255, 130, 30));
            g.rect(bx + 4, by - 2 - fl, 1, 1 + fl, RGB(255, 235, 110));
        }
    }
    if (e.type == E_RPG) { // bazooka over the shoulder
        int dir = e.a.face >= 0 ? 1 : -1;
        int tx0 = dir > 0 ? sx - 8 : sx - 10;
        g.rect(tx0, y + 7, 18, 4, RGB(30, 36, 26));
        g.rect(tx0 + 1, y + 8, 16, 2, RGB(78, 92, 64));
        g.rect(dir > 0 ? tx0 + 16 : tx0, y + 7, 2, 4, RGB(205, 60, 40));
    }
    if (e.laugh > 0) {
        const char *t = (e.laugh / 9) % 2 ? "HA HA HA!" : "HA HA!";
        int tw = Gfx::text_w(t);
        g.rect_blend(sx - tw / 2 - 2, y - 11, tw + 4, 9, RGB(255, 255, 255), 215);
        g.text(sx - tw / 2, y - 9, t, RGB(200, 30, 30));
    }
    if (e.stun > 0) {
        int k = (tick / 4) % 3;
        g.rect(sx - 4 + k * 3, sy - 21, 2, 2, RGB(255, 240, 80));
        g.rect(sx + 3 - k * 3, sy - 20, 2, 2, RGB(255, 240, 80));
    } else if (e.alert > 0 && e.alert < 40) {
        g.text(sx - 1, sy - 26, "!", RGB(255, 40, 40), 2);
    }
}

void Game::draw_heli(Gfx &g, int sx, int sy, bool big) {
    (void)big;
    u32 body = RGB(80, 86, 98), dark = RGB(45, 48, 58), glass = RGB(120, 190, 230);
    g.rect(sx - 20, sy - 9, 40, 14, dark);
    g.rect(sx - 19, sy - 8, 38, 12, body);
    g.rect(sx - 19, sy - 8, 38, 2, RGB(120, 128, 142));
    g.rect(sx + 8, sy - 7, 10, 7, glass);
    g.rect(sx + 8, sy - 7, 10, 1, RGB(200, 235, 250));
    g.rect(sx - 34, sy - 5, 16, 4, body);
    g.rect(sx - 36, sy - 9, 3, 8, dark);
    g.rect(sx - 14, sy + 5, 28, 1, dark);
    g.rect(sx - 12, sy + 3, 2, 3, dark);
    g.rect(sx + 10, sy + 3, 2, 3, dark);
    int bl = 22 + int(8 * sinf(float(tick) * 1.7f));
    g.rect(sx - bl, sy - 12, bl * 2, 1, RGB(30, 30, 36));
    g.rect(sx - 1, sy - 11, 2, 3, dark);
    g.rect(sx - 40 + int((tick / 2) % 2), sy - 8 + int((tick / 2) % 2) * 2, 2, 4, RGB(30, 30, 36));
    g.rect(sx - 3, sy - 3, 6, 2, RGB(210, 40, 40));
}

// ---- HUD ---------------------------------------------------------------------------------
void Game::draw_hud(Gfx &g) {
    const BroDef &b = BROS[pl.bro];
    char buf[48];
    if (debugMode) { // debug mode: level id, bro index, hint
        char dbg[96];
        snprintf(dbg, sizeof(dbg), "DEBUG %d-%d  BRO %d/%d  SELECT=NEXT", world + 1, stage + 1, pl.bro + 1, NUM_BROS);
        g.text_shadow(60, 13, dbg, RGB(255, 120, 100));
    }
    // slim top bar: kills, level progress, revolver ammo
    g.rect_blend(0, 0, VW, 11, RGB(0, 0, 0), 90);
    snprintf(buf, sizeof(buf), "KILLS %d", stat_kills);
    g.text_shadow(4, 3, buf, RGB(255, 255, 255));
    if (b.prim == P_REVOLVER) {
        snprintf(buf, sizeof(buf), pl.reload > 0 ? "RELOAD" : "%d/6", pl.revAmmo);
        g.text_shadow(80, 3, buf, RGB(255, 255, 255));
    }
    int bw = 70, bx = VW - bw - 4;
    g.rect(bx, 5, bw, 2, RGB(50, 50, 60));
    g.rect(bx + int(float(bw) * std::min(1.0f, pl.a.x / float(W * TS))), 3, 2, 6, RGB(255, 235, 120));

    // bottom-left: portrait of the bro you are playing, like the original game
    int px = 4, py = VH - 54;
    g.rect(px - 2, py - 2, 52, 52, RGB(15, 15, 20));
    g.rect(px - 1, py - 1, 50, 50, RGB(255, 215, 60));
    g.rect(px, py, 48, 48, th.night ? RGB(40, 50, 90) : RGB(90, 150, 210));
    {   // face only: 24x24 drawn portrait at 2x
        const Sprite &s = broFace[pl.bro];
        for (int j = 0; j < s.h; j++)
            for (int i = 0; i < s.w; i++) {
                u32 c = s.at(i, j);
                if (c) g.rect(px + i * 2, py + j * 2, 2, 2, (pl.alive || (tick / 4) % 2) ? c : RGB(120, 30, 30));
            }
    }
    int tx = px + 54;
    g.text_shadow(tx, py + 1, b.name, RGB(255, 235, 120));
    snprintf(buf, sizeof(buf), "BROS X%d", pl.lives < 0 ? 0 : pl.lives);
    g.text_shadow(tx, py + 9, buf, RGB(255, 255, 255));
    for (int i = 0; i < b.specAmmo; i++) {
        u32 c = i < pl.spec ? RGB(120, 230, 255) : RGB(60, 70, 80);
        g.rect(tx + i * 7, py + 18, 5, 7, RGB(10, 20, 30));
        g.rect(tx + i * 7 + 1, py + 19, 3, 5, c);
    }
    g.text_shadow(tx + b.specAmmo * 7 + 2, py + 19, b.specName, RGB(190, 210, 230));
    // upcoming bros (next lives), small heads
    int shown = 0;
    for (int i = 0; i < int(queue.size()) && shown < 6; i++, shown++) {
        const Sprite &s = broFace[queue[size_t(i)]]; // next bros: 12x12 faces
        for (int j = 0; j < 12; j++)
            for (int k = 0; k < 12; k++) { u32 c = s.at(k * 2, j * 2); if (c) g.pset(tx + shown * 14 + k, py + 30 + j, c); }
    }
    // charge bar
    if (pl.alive && BROS[pl.bro].prim == P_CHARGE && pl.charge > 0) {
        int sx = int(pl.a.x) - int(camX) - shakeX - 8, sy = int(pl.a.y) - int(camY) - shakeY - 26;
        g.rect(sx, sy, 16, 3, RGB(30, 30, 40));
        g.rect(sx + 1, sy + 1, pl.charge * 14 / 45, 1, pl.charge >= 40 ? RGB(255, 80, 60) : RGB(120, 230, 255));
    }
    // boss bar
    if (bossIdx >= 0 && en[size_t(bossIdx)].state > 0) {
        const Enemy &bs = en[size_t(bossIdx)];
        int w = 160, x = (VW - w) / 2;
        const char *nm = boss_name(bs.kind);
        g.text_shadow((VW - Gfx::text_w(nm)) / 2, 13, nm, RGB(255, 120, 90));
        g.rect(x - 1, 21, w + 2, 7, RGB(10, 5, 5));
        g.rect(x, 22, w, 5, RGB(60, 14, 14));
        g.rect(x + 1, 23, std::max(0, (w - 2) * bs.hp / std::max(1, bs.maxhp)), 3, bs.kind == 2 && bs.ph != 3 ? RGB(120, 130, 160) : RGB(220, 40, 40));
        if (bs.kind == 2 && bs.ph != 3) g.text_shadow((VW - Gfx::text_w("SHIELD UP")) / 2, 30, "SHIELD UP", RGB(150, 200, 255));
        if (bs.kind == 1 && bs.ph == 3) g.text_shadow((VW - Gfx::text_w("STUNNED - HIT HIM!")) / 2, 30, "STUNNED - HIT HIM!", RGB(255, 240, 90));
    }
    if (bossBannerT > 0 && bossIdx >= 0) { // boss name and the hint about the mechanic of this fight
        int tt = std::min(bossBannerT, 40);
        const char *nm = boss_name(en[size_t(bossIdx)].kind), *hint = boss_hint(en[size_t(bossIdx)].kind);
        g.rect_blend(0, 70, VW, 44, RGB(0, 0, 0), 130 * tt / 40);
        g.text_shadow((VW - Gfx::text_w(nm, 2)) / 2, 76, nm, RGB(255, 90, 60), 2);
        g.text_shadow((VW - Gfx::text_w(hint)) / 2, 98, hint, RGB(235, 235, 240));
    }
    if (bossWarnT > 0 && bossIdx >= 0 && (tick / 5) % 2) {
        static const char *w[NUM_WORLDS] = {"INCOMING MORTARS! TAKE COVER!", "CHARGE!", "", "", "SUPPLY DROP!", "INCOMING AIRSTRIKE!"};
        const char *t = w[en[size_t(bossIdx)].kind % NUM_WORLDS];
        if (t[0]) g.text_shadow((VW - Gfx::text_w(t, 2)) / 2, 40, t, RGB(255, 70, 50), 2);
    }
    // banners
    if (bannerT > 0 && pl.alive) {
        g.text_shadow((VW - Gfx::text_w(b.name, 2)) / 2, 40, b.name, RGB(255, 235, 120), 2);
        char pb[64];
        snprintf(pb, sizeof(pb), "%s / %s", b.primName, b.specName);
        g.text_shadow((VW - Gfx::text_w(pb)) / 2, 56, pb, RGB(255, 255, 255));
    }
    if (!pl.alive && pl.deadT > 10 && pl.lives > 0) {
        const char *m = deathCause == 3 ? "LOST IN THE VOID" : "BRO DOWN";
        g.text_shadow((VW - Gfx::text_w(m, 2)) / 2, 100, m, RGB(255, 80, 60), 2);
    }
}

// ---- world ---------------------------------------------------------------------------------
void Game::render(Gfx &g) {
    if (gs == GS_NEWBRO) { draw_newbro(g); return; }
    if (gs == GS_TITLE || gs == GS_SLOTS || gs == GS_MISSIONS || gs == GS_ROSTER) {
        draw_title(g);
        return;
    }
    int ox = int(camX) + shakeX, oy = int(camY) + shakeY;
    draw_bg(g);
    draw_trees(g, ox, oy); // background plane, behind the ground and everyone
    int tx0 = std::max(0, ox / TS), tx1 = std::min(W - 1, (ox + VW) / TS);
    int ty0 = std::max(0, oy / TS), ty1 = std::min(H - 1, (oy + VH) / TS);
    for (int ty = ty0; ty <= ty1; ty++)
        for (int tx = tx0; tx <= tx1; tx++) draw_tile(g, tx, ty, tx * TS - ox, ty * TS - oy);

    // collapsing structures
    for (const Chunk &c : chunks)
        for (const ChunkTile &t : c.t)
            g.sprite(tileSpr[t.type][t.var & 3], int(c.x) + t.dx * TS - ox, int(c.y) + t.dy * TS - oy);
    // props
    for (const Prop &p : props) {
        int sx = int(p.a.x) - ox, sy = int(p.a.y) - oy;
        if (sx < -50 || sx > VW + 50 || sy < -40 || sy > VH + 40) continue;
        if (p.type == PR_CAGE && !p.used) {
            g.rect(sx - 7, sy - 14, 14, 14, RGB(50, 52, 60));
            g.rect(sx - 6, sy - 13, 12, 12, RGB(30, 30, 38));
            const Sprite &s = captiveSpr; // bag over the head, tied up: he does not move until touched
            for (int j = 0; j < 12; j++)
                for (int i = 4; i < 14; i++) { u32 c = s.at(i, j + 1); if (c) g.pset(sx - 5 + (i - 4), sy - 13 + j, c); }
            for (int k = 0; k < 4; k++) g.rect(sx - 6 + k * 4, sy - 14, 1, 14, RGB(150, 155, 170));
            g.rect(sx - 7, sy - 15, 14, 2, RGB(150, 155, 170));
        } else if (p.type == PR_FREED) {
            const Sprite &s = broSpr[p.bro >= 0 ? p.bro : (tick / 10) % NUM_BROS][(tick / 6) % 2 + 1];
            g.sprite(s, sx - 9, sy - 17, (tick / 50) % 2 == 0);
            g.text((sx - 2), sy - 26 + int(sinf(float(tick) * 0.2f) * 2), "*", RGB(255, 240, 80), 2);
        } else if (p.type == PR_AMMO) {
            g.rect(sx - 4, sy - 6, 8, 6, RGB(70, 90, 50));
            g.rect(sx - 4, sy - 4, 8, 1, RGB(240, 200, 60));
            g.rect(sx - 4, sy - 6, 8, 1, RGB(110, 130, 80));
        } else if (p.type == PR_FLAG) {
            g.rect(sx, sy - 24, 1, 24, RGB(170, 175, 185));
            int fy = p.used ? sy - 24 : sy - 8;
            g.rect(sx + 1, fy, 9, 3, RGB(220, 40, 40));
            g.rect(sx + 1, fy + 3, 9, 3, RGB(240, 240, 240));
            if (p.used) g.rect(sx + 1, fy, 3, 3, RGB(40, 60, 170));
        }
    }
    // enemies
    for (const Enemy &e : en) {
        if (!e.alive) continue;
        int sx = int(roundf(e.a.x)) - ox, sy = int(roundf(e.a.y)) - oy;
        if (sx < -60 || sx > VW + 60 || sy < -40 || sy > VH + 40) continue;
        draw_enemy(g, e, sx, sy);
    }
    draw_boss_fx(g, ox, oy);
    draw_gas(g, ox, oy);
    draw_exfil_world(g, ox, oy);
    // player
    if (pl.alive) {
        bool golden = BROS[pl.bro].spec == S_INVINC && pl.invuln > 30;
        bool blink = pl.invuln > 0 && pl.invuln < 1000 && !golden && pl.dashT == 0 && (tick / 3) % 2 == 0;
        if (!blink) {
            bool flashy = golden && (tick / 2) % 2 == 0;
            draw_actor_bro(g, pl.a, pl.bro, pl.anim, !pl.a.ground, flashy, pl.cloak > 0);
        }
        int sx = int(pl.a.x) - ox, sy = int(pl.a.y) - oy;
        if (BROS[pl.bro].prim == P_KATANA) draw_saber(g, pl, sx, sy);
        else if (BROS[pl.bro].prim == P_CHAINSAW) draw_chainsaw(g, pl, sx, sy);
        else if (pl.punchT > 0) draw_punch(g, pl, sx, sy);
        if (pl.whipT > 0) {
            int len = int(float(pl.whipLen) * (pl.whipT > 7 ? 1.0f : float(pl.whipT) / 7.0f)); // lash out, hold, then coil back
            for (int i = 0; i < len; i++) g.rect(sx + pl.a.face * (6 + i) - (pl.a.face < 0 ? 1 : 0), sy - 10 + (i % 7 < 2 ? 1 : 0) - (i > len - 6 ? 1 : 0), 2, 2, i > len - 4 ? RGB(250, 230, 160) : RGB(170, 115, 60));
        }
        if (pl.grappleT > 0) {
            float dx = pl.gx - pl.a.x, dy = pl.gy - (pl.a.y - 10);
            int n = int(sqrtf(dx * dx + dy * dy));
            for (int i = 0; i < n; i += 2) g.rect(sx + int(dx * float(i) / float(n)), sy - 10 + int(dy * float(i) / float(n)), 2, 2, RGB(190, 150, 90));
        }
    }
    // projectiles (deliberately chunky so they can be seen and dodged)
    for (const Proj &p : pr) {
        int sx = int(p.x) - ox, sy = int(p.y) - oy;
        bool horiz = fabsf(p.vx) >= fabsf(p.vy);
        switch (p.kind) {
        case PK_BULLET: case PK_REVBULLET:
            if (horiz) {
                g.rect(sx - 4, sy - 2, 9, 4, RGB(150, 90, 20));
                g.rect(sx - 3, sy - 1, 7, 2, RGB(255, 225, 70));
                g.rect(sx - 1, sy - 1, 3, 2, RGB(255, 255, 230));
            } else {
                g.rect(sx - 2, sy - 4, 4, 9, RGB(150, 90, 20));
                g.rect(sx - 1, sy - 3, 2, 7, RGB(255, 225, 70));
                g.rect(sx - 1, sy - 1, 2, 3, RGB(255, 255, 230));
            }
            break;
        case PK_PELLET: g.rect(sx - 2, sy - 2, 4, 4, RGB(150, 90, 20)); g.rect(sx - 1, sy - 1, 2, 2, RGB(255, 235, 120)); break;
        case PK_EBULLET:
            g.rect(sx - 3, sy - 3, 6, 6, RGB(90, 10, 10));
            g.rect(sx - 2, sy - 2, 4, 4, RGB(255, 70, 40));
            g.rect(sx - 1, sy - 1, 2, 2, RGB(255, 230, 200));
            break;
        case PK_ROCKET: case PK_REMOTE:
            if (horiz) {
                g.rect(sx - 5, sy - 2, 10, 4, RGB(60, 65, 75));
                g.rect(sx - 4, sy - 1, 8, 2, RGB(225, 230, 235));
                g.rect(sx + (p.vx > 0 ? 3 : -5), sy - 2, 2, 4, RGB(220, 50, 40));
                g.rect(sx - int(p.vx * 3) - 1, sy - 1, 3, 2, RGB(255, 170, 40));
            } else {
                g.rect(sx - 2, sy - 5, 4, 10, RGB(60, 65, 75));
                g.rect(sx - 1, sy - 4, 2, 8, RGB(225, 230, 235));
                g.rect(sx - 2, sy + (p.vy > 0 ? 3 : -5), 4, 2, RGB(220, 50, 40));
                g.rect(sx - 1, sy - int(p.vy * 3) - 1, 2, 3, RGB(255, 170, 40));
            }
            break;
        case PK_MISSILE: g.rect(sx - 2, sy - 6, 4, 12, RGB(60, 65, 75)); g.rect(sx - 1, sy - 5, 2, 10, RGB(215, 220, 230)); g.rect(sx - 2, sy - 9, 4, 3, RGB(255, 150, 40)); break;
        case PK_GRENADE: g.rect(sx - 3, sy - 3, 6, 6, RGB(30, 45, 25)); g.rect(sx - 2, sy - 2, 4, 4, RGB(90, 125, 65)); break;
        case PK_FIREBOMB: g.rect(sx - 3, sy - 3, 6, 6, RGB(100, 40, 10)); g.rect(sx - 2, sy - 2, 4, 4, RGB(240, 120, 30)); break;
        case PK_DYNA: g.rect(sx - 2, sy - 4, 5, 8, RGB(70, 10, 10)); g.rect(sx - 1, sy - 3, 3, 6, (tick / 4) % 2 ? RGB(230, 50, 40) : RGB(255, 210, 210)); g.rect(sx, sy - 6, 1, 2, RGB(255, 220, 80)); break;
        case PK_MOLOTOV: g.rect(sx - 2, sy - 4, 5, 8, RGB(30, 70, 30)); g.rect(sx - 1, sy - 3, 3, 6, RGB(90, 180, 90)); g.rect(sx, sy - 6, 2, 2, RGB(255, 140, 30)); break;
        case PK_TURKEY: g.rect(sx - 5, sy - 4, 10, 8, RGB(90, 55, 25)); g.rect(sx - 4, sy - 3, 8, 6, RGB(180, 115, 60)); g.rect(sx + 3, sy - 6, 3, 3, RGB(210, 150, 100)); break;
        case PK_HOMING: g.rect(sx - 3, sy - 3, 6, 6, RGB(10, 70, 90)); g.rect(sx - 2, sy - 2, 4, 4, RGB(120, 230, 255)); break;
        case PK_RAY: g.rect(sx - 4, sy - 4, 8, 8, RGB(90, 30, 140)); g.rect(sx - 3, sy - 3, 6, 6, RGB(190, 80, 255)); g.rect(sx - 1, sy - 1, 3, 3, RGB(255, 255, 255)); break;
        case PK_BOOMERANG: {
            int k = (tick / 2) % 4;
            if (BROS[pl.bro].prim == P_CHAKRAM) { // Xena's chakram: a spinning steel ring
                g.rect(sx - 4, sy - 1, 9, 3, RGB(215, 220, 232)); g.rect(sx - 1, sy - 4, 3, 9, RGB(215, 220, 232));
                g.rect(sx - 3, sy - 3, 7, 7, RGB(215, 220, 232)); g.rect(sx - 2, sy - 2, 5, 5, RGB(40, 42, 52));
                g.rect(k % 2 ? sx - 4 : sx - 1, k % 2 ? sy - 1 : sy - 4, k % 2 ? 2 : 3, k % 2 ? 3 : 2, RGB(255, 255, 255));
                break;
            }
            if (k % 2) { g.rect(sx - 4, sy - 1, 9, 3, RGB(210, 215, 225)); g.rect(sx - 1, sy - 4, 3, 9, RGB(160, 120, 70)); }
            else { g.rect(sx - 1, sy - 4, 3, 9, RGB(210, 215, 225)); g.rect(sx - 4, sy - 1, 9, 3, RGB(160, 120, 70)); }
            break;
        }
        case PK_EROCKET: {
            if (fabsf(p.vy) > fabsf(p.vx)) { // a shell falling from a boss strike
                g.rect(sx - 3, sy - 6, 6, 12, RGB(30, 40, 25));
                g.rect(sx - 2, sy - 5, 4, 10, RGB(110, 140, 85));
                g.rect(sx - 3, sy + 3, 6, 3, RGB(215, 55, 40));
                g.rect(sx - 1, sy - 11 - (tick / 2) % 3, 2, 5 + (tick / 2) % 3, RGB(255, 190, 60));
                break;
            }
            int dir = p.vx >= 0 ? 1 : -1;
            g.rect(sx - 6, sy - 3, 12, 6, RGB(30, 40, 25));
            g.rect(sx - 5, sy - 2, 10, 4, RGB(110, 140, 85));
            g.rect(dir > 0 ? sx + 3 : sx - 6, sy - 3, 3, 6, RGB(215, 55, 40));
            g.rect(dir > 0 ? sx - 9 - (tick / 2) % 3 : sx + 6, sy - 1, 4 + (tick / 2) % 3, 2, RGB(255, 190, 60));
            break;
        }
        case PK_SPEAR:
            if (horiz) {
                int dir = p.vx >= 0 ? 1 : -1;
                g.rect(sx - 7, sy - 2, 15, 4, RGB(70, 45, 20));
                g.rect(sx - 6, sy - 1, 13, 2, RGB(165, 115, 60));
                g.rect(dir > 0 ? sx + 5 : sx - 9, sy - 2, 4, 4, RGB(220, 225, 235));
            } else {
                g.rect(sx - 2, sy - 7, 4, 15, RGB(70, 45, 20));
                g.rect(sx - 1, sy - 6, 2, 13, RGB(165, 115, 60));
                g.rect(sx - 2, p.vy < 0 ? sy - 9 : sy + 5, 4, 4, RGB(220, 225, 235));
            }
            break;
        default: break;
        }
    }
    // particles
    for (const Particle &p : pa) {
        int sx = int(p.x) - ox, sy = int(p.y) - oy;
        if (p.kind == 3) {
            int a = 60 + p.life * 100 / p.maxlife;
            g.rect_blend(sx - 1, sy - 1, 3, 3, p.col, a > 255 ? 255 : a);
        } else if (p.kind == 4) {
            int sz = p.life > p.maxlife / 2 ? 3 : 2;
            g.rect(sx - 1, sy - 1, sz, sz, p.col);
        } else {
            int sz = p.kind == 5 ? 3 : (p.kind == 1 ? (p.life > p.maxlife / 3 ? 2 : 1) : ((p.maxlife > 42 && p.life > 10) ? 2 : 1));
            g.rect(sx, sy, sz, sz, p.col);
        }
    }
    // explosions
    for (const Boom &b : booms) {
        int sx = int(b.x) - ox, sy = int(b.y) - oy;
        float k = float(b.t) / float(b.maxt);
        int r = int(b.r * (0.3f + 0.6f * sqrtf(k)));
        if (k < 0.7f) fill_circle(g, sx, sy, r, k < 0.25f ? RGB(255, 245, 190) : RGB(255, 140, 40));
        if (k < 0.45f) fill_circle(g, sx, sy, std::max(1, int(float(r) * 0.6f)), k < 0.15f ? RGB(255, 255, 255) : RGB(255, 220, 90));
    }
    // airstrike markers
    for (const Strike &s : strikes)
        if (s.t > 0 && (tick / 4) % 2 == 0) g.rect(int(s.x) - ox - 1, 0, 2, VH, RGB(255, 80, 60));
    if (flashScreen > 0) g.rect_blend(0, 0, VW, VH, RGB(255, 255, 255), flashScreen * 16);
    if (world == 5) g.rect_blend(0, 0, VW, VH, RGB(80, 0, 0), 28);

    draw_hud(g);
    draw_exfil_ui(g);
    draw_overlays(g);
    if (gs != GS_PLAY) draw_menu(g);
}

// ---- menus -------------------------------------------------------------------------------------
static const char *level_name(int i) {
    static const char *n[NUM_WORLDS] = {"JUNGLE", "DESERT", "NIGHT BASE", "ARCTIC", "CITY RUINS", "HELL"};
    return n[(i / STAGES) % NUM_WORLDS];
}

std::string Game::options_line(int i) const {
    char b[48];
    if (i == 0) { snprintf(b, sizeof(b), "SCREEN SHAKE  < %d%% >", st.shake); return b; }
    if (i == 1) { snprintf(b, sizeof(b), "VIBRATION  < %d%% >", st.vib); return b; }
    if (i == 2) { snprintf(b, sizeof(b), "SFX VOLUME  < %d%% >", st.sfx); return b; }
    if (i == 3) { snprintf(b, sizeof(b), "MUSIC VOLUME  < %d%% >", st.music); return b; }
    if (i == 4) { snprintf(b, sizeof(b), "FLIP SCREEN  < %s >", st.flip ? "ON" : "OFF"); return b; }
    return "BACK";
}

void Game::draw_title(Gfx &g) {
    float sx = camX, sy = camY;
    camX = 40.0f + float(stateT) * 0.25f;
    if (camX > float(W * TS - VW)) camX = float(W * TS - VW);
    camY = 38.0f * TS - 150.0f;
    int ox = int(camX), oy = int(camY);
    draw_bg(g);
    for (int ty = std::max(0, oy / TS); ty <= std::min(H - 1, (oy + VH) / TS); ty++)
        for (int tx = std::max(0, ox / TS); tx <= std::min(W - 1, (ox + VW) / TS); tx++) draw_tile(g, tx, ty, tx * TS - ox, ty * TS - oy);
    // roster parade on the ground
    g.rect(0, 190, VW, VH - 190, th.dirt);
    g.rect(0, 190, VW, 3, th.grass);
    for (int i = 0; i < NUM_BROS; i++) {
        float x = float((stateT / 2 + i * 22) % (VW + 40 + NUM_BROS * 2)) - 20;
        const Sprite &s = broSpr[i][1 + (stateT / 6 + i) % 2];
        g.sprite(s, int(x) - 9, 190 - 17, false);
    }
    camX = sx; camY = sy;
    if (gs == GS_TITLE) {
        const char *t1 = "BROFORCE", *t2 = "LITE";
        int x1 = (VW - Gfx::text_w(t1, 6)) / 2, x2 = (VW - Gfx::text_w(t2, 6)) / 2;
        g.text(x1 + 3, 12 + 3, t1, RGB(0, 0, 0), 6);
        g.text(x1, 12 + 1, t1, RGB(255, 140, 30), 6);
        g.text(x1, 12, t1, RGB(255, 215, 60), 6);
        g.text(x2 + 3, 48 + 3, t2, RGB(0, 0, 0), 6);
        g.text(x2, 48 + 1, t2, RGB(255, 140, 30), 6);
        g.text(x2, 48, t2, RGB(255, 215, 60), 6);
        char ver[32];
        snprintf(ver, sizeof(ver), "V%s%s", GAME_VERSION, debugMode ? " [DEBUG]" : "");
        g.text_shadow(x2 + Gfx::text_w(t2, 6) + 6, 48 + 34, ver, debugMode ? RGB(255, 120, 100) : RGB(255, 255, 255)); // small, next to the title
        const char *fan = "FANGAME - CONSIDER BUYING THE REAL ONE, BRO";
        g.rect_blend(0, 94, VW, 11, RGB(0, 0, 0), 170);
        g.text_shadow((VW - Gfx::text_w(fan)) / 2, 96, fan, RGB(235, 40, 40));
    } else {
        g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 120);
    }
    draw_menu(g);
}

void Game::draw_slots(Gfx &g) {
    const char *title = "SELECT SAVE";
    g.text_shadow((VW - Gfx::text_w(title, 3)) / 2, 14, title, RGB(255, 215, 60), 3);
    for (int i = 0; i < 4; i++) {
        const SaveSlot &sl = st.slots[i];
        bool sel = menuSel == i;
        int x = 24, y = 52 + i * 38, w = VW - 48, h = 34;
        g.rect(x - 1, y - 1, w + 2, h + 2, sel ? RGB(255, 215, 60) : RGB(20, 20, 28));
        g.rect(x, y, w, h, sel ? RGB(40, 46, 70) : RGB(22, 26, 40));
        char b[64];
        snprintf(b, sizeof(b), "SLOT %d", i + 1);
        g.text_shadow(x + 8, y + 5, b, sel ? RGB(255, 235, 90) : RGB(220, 220, 230), 2);
        int pct = completion(sl);
        if (!sl.used) snprintf(b, sizeof(b), "NEW GAME");
        else {
            int nl = unlocked_of(sl);
            if (pct >= 100) snprintf(b, sizeof(b), "ALL MISSIONS DONE");
            else snprintf(b, sizeof(b), "NEXT: LEVEL %d  %s", nl + 1, level_name(nl));
        }
        g.text(x + 8, y + 22, b, RGB(170, 180, 200));
        // completion bar
        int bx = x + w - 112, by = y + 6, bw = 100;
        g.rect(bx - 1, by - 1, bw + 2, 10, RGB(10, 10, 16));
        g.rect(bx, by, bw, 8, RGB(48, 52, 70));
        g.rect(bx, by, bw * pct / 100, 8, pct >= 100 ? RGB(255, 215, 60) : RGB(90, 200, 110));
        snprintf(b, sizeof(b), "%d%%", pct);
        g.text_shadow(bx + bw - Gfx::text_w(b, 2), by + 13, b, RGB(255, 255, 255), 2);
    }
    const char *back = "BACK";
    bool sel = menuSel == 4;
    int bw = Gfx::text_w(back, 2);
    g.text_shadow((VW - bw) / 2, 52 + 4 * 38 + 4, back, sel ? RGB(255, 235, 90) : RGB(220, 220, 230), 2);
    if (sel) g.text_shadow((VW - bw) / 2 - 12, 52 + 4 * 38 + 4, ">", RGB(255, 235, 90), 2);
}

void Game::draw_missions(Gfx &g) {
    const SaveSlot &sl = st.slots[curSlot];
    int unl = unlocked_of(sl);
    char b[80];
    snprintf(b, sizeof(b), "SLOT %d - %d%%", curSlot + 1, completion(sl));
    g.text_shadow((VW - Gfx::text_w(b, 3)) / 2, 8, b, RGB(255, 215, 60), 3);
    std::vector<std::string> items, status;
    auto add = [&](const std::string &a, const std::string &s) { items.push_back(a); status.push_back(s); };
    if (debugMode) add("DEBUG: ALL LEVELS, ALL BROS", "");
    else { snprintf(b, sizeof(b), "%s %d-%d", sl.used ? "CONTINUE" : "NEW GAME", unl / STAGES + 1, unl % STAGES + 1); add(b, ""); }
    for (int k = 0; k <= unl; k++) {
        if (k % STAGES == STAGES - 1) snprintf(b, sizeof(b), "%d-%d %s", k / STAGES + 1, k % STAGES + 1, boss_name(k / STAGES));
        else snprintf(b, sizeof(b), "%d-%d %s", k / STAGES + 1, k % STAGES + 1, level_name(k));
        int tot = level_bro_total(k), own = level_bro_owned(sl, k);
        char s[24] = "";
        if (level_full(sl, k)) snprintf(s, sizeof(s), "OK");
        else if (tot > 0) snprintf(s, sizeof(s), "%s%d/%d", (sl.mask & (1 << k)) ? "* " : "", own, tot);
        add(b, s);
    }
    snprintf(b, sizeof(b), "BRO ROSTER %d/%d", owned_count(sl), NUM_BROS);
    add(b, "");
    add(eraseArm ? "PRESS A AGAIN TO ERASE!" : "ERASE THIS SAVE", "");
    add("BACK", "");
    const int ROWS = 8, RH = 18, Y0 = 40;
    int n = int(items.size());
    int top = std::max(0, std::min(menuSel - ROWS / 2, n - ROWS));
    for (int r = 0; r < ROWS && top + r < n; r++) {
        int i = top + r;
        bool sel = i == menuSel;
        int y = Y0 + r * RH;
        g.rect_blend(24, y - 3, VW - 48, 16, RGB(0, 0, 0), sel ? 170 : 95);
        u32 col = sel ? RGB(255, 235, 90) : (i == n - 2 && eraseArm ? RGB(255, 90, 80) : RGB(220, 220, 230));
        if (i >= 1 && i <= unl + 1 && ((i - 1) % STAGES) == STAGES - 1 && !sel) col = RGB(255, 150, 120);
        if (i == n - 3 && !sel) col = RGB(150, 220, 255);
        g.text_shadow(36, y, items[size_t(i)].c_str(), col, 2);
        if (!status[size_t(i)].empty()) {
            bool ok = status[size_t(i)] == "OK";
            g.text_shadow(VW - 30 - Gfx::text_w(status[size_t(i)].c_str(), 2), y, status[size_t(i)].c_str(), ok ? RGB(120, 255, 140) : RGB(255, 200, 90), 2);
        }
        if (sel) g.text_shadow(26, y, ">", RGB(255, 235, 90), 2);
    }
    if (top > 0) g.text_shadow(VW - 16, Y0 - 10, "^", RGB(255, 235, 90));
    if (top + ROWS < n) g.text_shadow(VW - 16, Y0 + ROWS * RH - 2, "v", RGB(255, 235, 90));
    g.text_shadow(8, VH - 12, "B = BACK    * = CLEARED, BROS MISSING", RGB(200, 200, 210));
}

// ---- new bro screen and roster grid ---------------------------------------------------------
void Game::draw_newbro(Gfx &g) {
    g.clear(RGB(0, 0, 0));
    int fade = std::min(255, stateT * 14);
    auto dim = [&](u32 c) { return RGB(int((c >> 16) & 255) * fade >> 8, int((c >> 8) & 255) * fade >> 8, int(c & 255) * fade >> 8); };
    const BroDef &b = BROS[std::max(0, newBro)];
    const char *hdr = "NEW BRO!";
    g.text((VW - Gfx::text_w(hdr, 2)) / 2, 14, hdr, dim(RGB(255, 215, 60)), 2);
    int sc = Gfx::text_w(b.name, 3) <= VW - 10 ? 3 : 2;
    g.text_shadow((VW - Gfx::text_w(b.name, sc)) / 2, 40, b.name, dim(RGB(255, 255, 255)), sc);
    int fx = (VW - 24 * 5) / 2, fy = 76;
    g.rect(fx - 6, fy - 6, 24 * 5 + 12, 24 * 5 + 12, dim(RGB(255, 215, 60)));
    g.rect(fx - 3, fy - 3, 24 * 5 + 6, 24 * 5 + 6, dim(RGB(15, 15, 22)));
    g.rect(fx, fy, 24 * 5, 24 * 5, dim(RGB(90, 150, 210)));
    const Sprite &s = broFace[std::max(0, newBro)];
    for (int j = 0; j < s.h; j++)
        for (int i = 0; i < s.w; i++) { u32 c = s.at(i, j); if (c) g.rect(fx + i * 5, fy + j * 5, 5, 5, dim(c)); }
    char pb[80];
    snprintf(pb, sizeof(pb), "%s / %s", b.primName, b.specName);
    g.text((VW - Gfx::text_w(pb)) / 2, 204, pb, dim(RGB(200, 210, 230)));
    if (stateT > 30 && (stateT / 24) % 2 == 0) g.text((VW - Gfx::text_w("PRESS A")) / 2, 222, "PRESS A", RGB(255, 235, 120));
}

void Game::draw_roster(Gfx &g) {
    const SaveSlot &sl = st.slots[curSlot];
    g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 170);
    char b[80];
    snprintf(b, sizeof(b), "BROS %d/%d", owned_count(sl), NUM_BROS);
    g.text_shadow((VW - Gfx::text_w(b, 3)) / 2, 6, b, RGB(255, 215, 60), 3);
    const int COLS = 8, CW = 38, CH = 40, X0 = (VW - COLS * CW) / 2, Y0 = 36;
    for (int i = 0; i < NUM_BROS; i++) {
        int cx = X0 + (i % COLS) * CW, cy = Y0 + (i / COLS) * CH;
        bool has = owned(i), sel = i == rosterSel;
        g.rect(cx + 2, cy, 34, 36, sel ? RGB(255, 215, 60) : RGB(40, 44, 58));
        g.rect(cx + 3, cy + 1, 32, 34, has ? RGB(90, 150, 210) : RGB(46, 48, 56));
        if (has) {
            const Sprite &f = broFace[i];
            for (int j = 0; j < f.h; j++) for (int k = 0; k < f.w; k++) { u32 c = f.at(k, j); if (c) g.pset(cx + 7 + k, cy + 6 + j, c); }
        } else { // caged: the same bagged, tied-up prisoner for everybody
            for (int j = 0; j < captiveSpr.h; j++) for (int k = 0; k < captiveSpr.w; k++) { u32 c = captiveSpr.at(k, j); if (c) g.pset(cx + 8 + k, cy + 8 + j, c); }
            for (int k = 0; k < 6; k++) g.rect(cx + 5 + k * 5, cy + 2, 1, 32, RGB(150, 155, 170));
            g.rect(cx + 4, cy + 2, 30, 2, RGB(150, 155, 170)); g.rect(cx + 4, cy + 32, 30, 2, RGB(150, 155, 170));
        }
    }
    int sy = Y0 + ((NUM_BROS + COLS - 1) / COLS) * CH + 2;
    const BroDef &d = BROS[rosterSel];
    if (owned(rosterSel)) {
        g.text_shadow(12, sy, d.name, RGB(255, 235, 120), 2);
        snprintf(b, sizeof(b), "%s / %s", d.primName, d.specName);
        g.text_shadow(12, sy + 18, b, RGB(220, 225, 240));
    } else {
        int lv = bro_home_level(rosterSel);
        g.text_shadow(12, sy, "???", RGB(160, 165, 180), 2);
        snprintf(b, sizeof(b), "FREE HIM IN LEVEL %d-%d (%s)", lv / STAGES + 1, lv % STAGES + 1, level_name(lv));
        g.text_shadow(12, sy + 18, b, RGB(255, 200, 120));
    }
    g.text_shadow(8, VH - 12, "B = BACK", RGB(200, 200, 210));
}

void Game::draw_menu(Gfx &g) {
    if (gs == GS_SLOTS) { draw_slots(g); return; }
    if (gs == GS_MISSIONS) { draw_missions(g); return; }
    if (gs == GS_ROSTER) { draw_roster(g); return; }
    std::vector<std::string> items;
    const char *title = nullptr;
    int sel = menuSel;
    if (gs == GS_TITLE) {
        items = {"START GAME", "OPTIONS", "QUIT"};
    } else if (gs == GS_PAUSE) {
        g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 120);
        title = "PAUSED";
        items = {"RESUME", "OPTIONS", "QUIT TO TITLE"};
    } else if (gs == GS_OPTIONS) {
        if (menuFrom != GS_TITLE) g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 150);
        else g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 110);
        title = "OPTIONS";
        for (int i = 0; i < 6; i++) items.push_back(options_line(i));
    } else if (gs == GS_CLEAR) {
        g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 130);
        title = "LEVEL CLEAR!";
        items = {level < LAST_LEVEL ? "NEXT LEVEL" : "FINISH"};
    } else if (gs == GS_OVER) {
        if (stateT < FAIL_MENU) return; // the black "MISSION FAILED" card is drawn by draw_overlays
        items = {"RETRY LEVEL", "QUIT TO TITLE"};
    } else if (gs == GS_VICTORY) {
        g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), 170);
        title = "FREEDOM RESTORED!";
        g.text_shadow((VW - Gfx::text_w("THE BROS SAVED THE DAY. HOO-RAH!")) / 2, 80, "THE BROS SAVED THE DAY. HOO-RAH!", RGB(255, 255, 255));
        items = {"BACK TO TITLE"};
    }
    int y0 = gs == GS_TITLE ? 110 : (gs == GS_OPTIONS ? 80 : gs == GS_OVER ? 150 : 105);
    if (title) g.text_shadow((VW - Gfx::text_w(title, 3)) / 2, 55, title, RGB(255, 215, 60), 3);
    for (size_t i = 0; i < items.size(); i++) {
        bool s = int(i) == sel;
        const char *t = items[i].c_str();
        int x = (VW - Gfx::text_w(t, 2)) / 2, y = y0 + int(i) * 17;
        if (gs == GS_OPTIONS) { x += shakeX; y += shakeY; } // live preview of the shake setting
        if (gs == GS_TITLE) g.rect_blend(60, y - 3, VW - 120, 15, RGB(0, 0, 0), s ? 150 : 80);
        g.text_shadow(x, y, t, s ? RGB(255, 235, 90) : RGB(220, 220, 230), 2);
        if (s) g.text_shadow(x - 12, y, ">", RGB(255, 235, 90), 2);
    }
    if (gs == GS_TITLE) {
        g.text_shadow(6, VH - 22, "D-PAD MOVE  A JUMP  B FIRE  X SPECIAL  Y MELEE", RGB(255, 255, 255));
        g.text_shadow(6, VH - 12, "START PAUSE   FALL = DEATH   ONE HIT = DEATH", RGB(255, 255, 255));
    }
}

static void return_to_title(Game &, int &menuSel) { menuSel = 0; }

void Game::menu_activate() {
    if (gs == GS_TITLE) {
        if (menuSel == 0) { gs = GS_SLOTS; menuSel = st.lastSlot; }
        else if (menuSel == 1) { menuFrom = GS_TITLE; gs = GS_OPTIONS; menuSel = 0; }
        else if (menuSel == 2) want_quit = true;
    } else if (gs == GS_SLOTS) {
        if (menuSel < 4) { curSlot = menuSel; gs = GS_MISSIONS; menuSel = 0; eraseArm = false; }
        else { gs = GS_TITLE; return_to_title(*this, menuSel); }
    } else if (gs == GS_MISSIONS) {
        int unl = unlocked_of(st.slots[curSlot]);
        if (menuSel == 0) start_slot_game(debugMode ? 0 : unl);
        else if (menuSel <= unl + 1) start_slot_game(menuSel - 1);
        else if (menuSel == unl + 2) { gs = GS_ROSTER; rosterSel = 0; }
        else if (menuSel == unl + 3) {
            if (!eraseArm) eraseArm = true;
            else { st.slots[curSlot] = SaveSlot(); eraseArm = false; settings_dirty = true; gs = GS_SLOTS; menuSel = curSlot; }
        } else { gs = GS_SLOTS; menuSel = curSlot; }
    } else if (gs == GS_PAUSE) {
        if (menuSel == 0) gs = GS_PLAY;
        else if (menuSel == 1) { menuFrom = GS_PAUSE; gs = GS_OPTIONS; menuSel = 0; }
        else { gs = GS_TITLE; menuSel = 0; gen_level(0); stateT = 0; introT = 0; }
    } else if (gs == GS_OPTIONS) {
        if (menuSel == 5) { gs = menuFrom; menuSel = 1; }
    } else if (gs == GS_CLEAR) {
        if (level < LAST_LEVEL) start_level(level + 1);
        else { gs = GS_VICTORY; stateT = 0; }
    } else if (gs == GS_OVER) {
        if (stateT < FAIL_MENU) return;
        if (menuSel == 0) start_slot_game(level);
        else { gs = GS_TITLE; menuSel = 0; gen_level(0); stateT = 0; }
    } else if (gs == GS_VICTORY) {
        gs = GS_TITLE; menuSel = 0; gen_level(0); stateT = 0;
    }
}

void Game::update_menu(const Input &in) {
    bool up = in.up && !prev.up, down = in.down && !prev.down, left = in.left && !prev.left, right = in.right && !prev.right;
    bool ok = (in.jump && !prev.jump) || (in.start && !prev.start);
    bool back = in.fire && !prev.fire;
    int unl = unlocked_of(st.slots[curSlot]);
    int n = gs == GS_TITLE ? 3 : gs == GS_PAUSE ? 3 : gs == GS_OPTIONS ? 6 : gs == GS_OVER ? 2 : gs == GS_SLOTS ? 5 : gs == GS_MISSIONS ? unl + 5 : 1;
    if (gs == GS_ROSTER) { // the grid of bros of this save
        const int COLS = 8;
        if (left) rosterSel = (rosterSel + NUM_BROS - 1) % NUM_BROS;
        if (right) rosterSel = (rosterSel + 1) % NUM_BROS;
        if (up) rosterSel = (rosterSel - COLS + NUM_BROS) % NUM_BROS;
        if (down) rosterSel = (rosterSel + COLS) % NUM_BROS;
        if (left || right || up || down) sounds.push_back({SFX_MENU, 1.0f});
        if (back || ok) { gs = GS_MISSIONS; menuSel = 0; }
        return;
    }
    if (gs == GS_OVER) {
        if (stateT == 30) sounds.push_back({SPEECH_BASE + FAIL_SP[failPhrase], 1.0f});
        if (stateT < FAIL_MENU) return; // the card is fading in: no input yet
    }
    if (up) { menuSel = (menuSel + n - 1) % n; eraseArm = false; sounds.push_back({SFX_MENU, 1.0f}); }
    if (down) { menuSel = (menuSel + 1) % n; eraseArm = false; sounds.push_back({SFX_MENU, 1.0f}); }
    if (ok) sounds.push_back({SFX_SELECT, 1.0f});
    if (gs == GS_SLOTS && back) { gs = GS_TITLE; menuSel = 0; return; }
    if (gs == GS_MISSIONS && back) { gs = GS_SLOTS; menuSel = curSlot; eraseArm = false; return; }
    if (gs == GS_OPTIONS) {
        int d = (right ? 1 : 0) - (left ? 1 : 0);
        if (menuSel < 4 && (d != 0 || ok)) {
            // left/right step by 25%; A cycles 0 -> 100 and wraps
            int *vals[4] = {&st.shake, &st.vib, &st.sfx, &st.music};
            int &v = *vals[menuSel];
            if (d != 0) v = std::max(0, std::min(100, v + d * 25));
            else v = v >= 100 ? 0 : v + 25;
            if (menuSel == 0) trauma = 0.7f;
            else if (menuSel == 1) rumble = 1.0f;
            else if (menuSel == 2) sounds.push_back({SFX_BOOM_SMALL, 1.0f});
            settings_dirty = true;
        } else if (menuSel == 4 && (d != 0 || ok)) { st.flip = !st.flip; settings_dirty = true; }
        if (back) { gs = menuFrom; menuSel = 1; }
        if (ok && menuSel == 5) menu_activate();
        // preview the shake/vibration level while in the menu
        float amp = powf(trauma, 1.5f) * 9.0f * float(st.shake) / 100.0f;
        shakeX = int(roundf((rnd() - 0.5f) * 2 * amp));
        shakeY = int(roundf((rnd() - 0.5f) * 2 * amp));
        trauma = std::max(0.0f, trauma - 0.012f);
        return;
    }
    if (gs == GS_PAUSE && back) { gs = GS_PLAY; return; }
    if (ok) menu_activate();
}

// ---- extraction: helicopter + rope ladder, and the results panel ---------------------------
static void draw_icon12(Gfx &g, const Sprite &s, int x, int y) {
    for (int j = 0; j < 12; j++)
        for (int i = 0; i < 12; i++) {
            u32 c = s.at(i * s.w / 12, j * s.h / 12);
            if (c) g.pset(x + i, y + j, c);
        }
}

void Game::draw_exfil_world(Gfx &g, int ox, int oy) {
    if (ex.phase == 0) return;
    int sx = int(ex.x) - ox, sy = int(ex.y) - oy;
    float sway = sinf(float(ex.t) * 0.07f) * 3.0f;
    int len = int(ex.ladderLen);
    for (int i = 0; i < len; i++) {
        int lx = sx + int(sway * float(i) / 72.0f);
        g.rect(lx - 4, sy + 5 + i, 1, 1, RGB(140, 100, 55));
        g.rect(lx + 4, sy + 5 + i, 1, 1, RGB(140, 100, 55));
        if (i % 6 == 3) g.rect(lx - 4, sy + 5 + i, 9, 2, RGB(205, 165, 95));
    }
    draw_heli(g, sx, sy, false);
}

// ---- end-of-level recap: every victim re-enacts how he died ---------------------------------------
static void blit_xf(Gfx &g, const Sprite &s, float cx, float cy, float ang, float scx, float scy, bool flip, int dark, u32 tint) {
    float ca = cosf(-ang), sa = sinf(-ang);
    float hw = float(s.w) * 0.5f, hh = float(s.h) * 0.5f;
    int R = int(std::max(float(s.w) * scx, float(s.h) * scy) * 0.8f) + 2;
    for (int dy = -R; dy <= R; dy++)
        for (int dx = -R; dx <= R; dx++) {
            float ux = (float(dx) * ca - float(dy) * sa) / scx + hw;
            float uy = (float(dx) * sa + float(dy) * ca) / scy + hh;
            int ix = int(floorf(ux)), iy = int(floorf(uy));
            if (ix < 0 || iy < 0 || ix >= s.w || iy >= s.h) continue;
            if (flip) ix = s.w - 1 - ix;
            u32 c = s.at(ix, iy);
            if (!c) continue;
            if (tint) c = tint;
            else if (dark > 0) {
                int r = (int((c >> 16) & 255) * (256 - dark)) >> 8, gg = (int((c >> 8) & 255) * (256 - dark)) >> 8, b = (int(c & 255) * (256 - dark)) >> 8;
                c = RGB(r, gg, b);
            }
            g.pset(int(cx) + dx, int(cy) + dy, c);
        }
}

static uint32_t hsh(int a, int b) {
    uint32_t h = uint32_t(a) * 374761393u + uint32_t(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

void Game::draw_victim(Gfx &g, const KillRec &k, int age, int cx, int base, bool small) {
    if (age < 0) return;
    const Sprite &s = enSpr[k.type < E_TYPES ? k.type : 0][0];
    const float sc = small ? 0.67f : 1.0f;
    const int sd = k.dir >= 0 ? 1 : -1;
    const bool flip = sd > 0; // he faces whoever is about to kill him
    const float hh = float(s.h) * sc * 0.5f, ww = float(s.w) * sc * 0.5f;
    const float cyIdle = float(base) - hh + 1, cyLie = float(base) - ww + 1;
    const float PI_H = 1.5708f;
    const int HIT = 10;
    const u32 RED = RGB(190, 15, 15), WHITE = RGB(255, 255, 255);
    const int px = small ? 1 : 2;

    auto spray = [&](int n, int t, float x0, float y0) {
        if (t < 0 || t > 28) return;
        for (int i = 0; i < n; i++) {
            uint32_t hv = hsh(i, k.type * 7 + cx);
            float vx = (1.0f + float(hv % 4)) * 0.6f * float(sd), vy = -(1.5f + float((hv >> 4) % 5)) * 0.7f;
            float xx = x0 + vx * float(t), yy = y0 + vy * float(t) + 0.16f * float(t * t);
            if (yy > float(base)) continue;
            g.rect(int(xx), int(yy), px, px, RED);
        }
    };
    auto pool = [&](int t, int w) {
        if (t > 6) g.rect_blend(cx - w / 2 + sd * 3, base - 1, w, 2, RGB(120, 8, 8), std::min(200, (t - 6) * 12));
    };
    auto flames = [&](int t, float x0, float y0, int hgt) {
        for (int i = 0; i < 3; i++) {
            int fx = int(x0) - 5 + i * 4 + ((t / 2 + i) % 3), fy = int(y0) - hgt + ((t + i * 2) % 4);
            g.rect(fx, fy, 2, hgt - ((t + i) % 3), i == 1 ? RGB(255, 225, 90) : RGB(255, 115, 30));
        }
    };
    auto spikes = [&]() {
        for (int i = 0; i < 5; i++) {
            int xs = cx - 10 + i * 5;
            g.rect(xs, base - 3, 4, 3, RGB(150, 155, 165));
            g.rect(xs + 1, base - 6, 2, 3, RGB(210, 215, 225));
        }
    };

    if (age < HIT) { // he walks in and waits his turn
        float pop = age < 4 ? 0.45f + 0.14f * float(age) : 1.0f;
        float bob = ((age / 5) % 2) ? 1.0f : 0.0f;
        bool burning = k.cause == C_FIRE && age >= 6;
        float jx = burning ? float((age % 2) * 2 - 1) : 0.0f;
        if (k.cause == C_SPIKE) spikes();
        blit_xf(g, s, float(cx) + jx, cyIdle + (1.0f - pop) * hh - bob, 0, sc * pop, sc * pop, flip, 0, 0);
        switch (k.cause) {
        case C_FIRE: if (burning) flames(age, float(cx), float(base) - hh, int(14 * sc) + 2); break;
        case C_CRUSH: {
            float f = std::min(1.0f, float(age) / 9.0f);
            int by = int(float(base) - 70.0f + (70.0f - 2 * hh - 6.0f) * f * f);
            g.rect(cx - 8, by, 16, 6, RGB(150, 85, 70));
            g.rect(cx - 8, by, 16, 1, RGB(200, 130, 110));
            g.rect(cx - 8, by + 5, 16, 1, RGB(105, 55, 45));
            break;
        }
        case C_LIGHTNING:
            if (age >= 7) for (int y = base - 80; y < int(cyIdle - hh); y += 4) g.rect(cx + ((y / 4) % 2 ? 2 : -2), y, 2, 5, age == 9 ? WHITE : RGB(150, 190, 255));
            break;
        case C_MELEE: case C_BULLET:
            if (k.cause == C_BULLET && age >= 4) {
                int bx = cx - sd * int(28.0f - float(age - 4) * 4.5f);
                g.rect(bx - 2, int(cyIdle) - 1, 5, 2, RGB(255, 235, 110));
                g.rect(bx - sd * 6, int(cyIdle), 3, 1, RGB(255, 170, 60));
            }
            break;
        default: break;
        }
        return;
    }

    const int t = age - HIT;
    switch (k.cause) {
    case C_EXPLOSION: case C_GAS: {
        float R = (k.cause == C_GAS ? 24.0f : 16.0f) * (small ? 0.8f : 1.0f);
        if (t < 12) {
            float kk = float(t) / 12.0f;
            int r = int(R * (0.3f + 0.7f * sqrtf(kk)));
            if (kk < 0.7f) fill_circle(g, cx, int(cyIdle), r, kk < 0.25f ? RGB(255, 245, 190) : RGB(255, 140, 40));
            if (kk < 0.45f) fill_circle(g, cx, int(cyIdle), std::max(1, int(float(r) * 0.6f)), kk < 0.15f ? WHITE : RGB(255, 220, 90));
        }
        float dd = cyIdle - cyLie;
        float tLand = (7.0f + sqrtf(std::max(0.0f, 49.0f - 1.52f * dd))) / 0.76f;
        float tt = float(t), te = std::min(tt, tLand);
        float yy = cyIdle - (7.0f * te - 0.38f * te * te);
        float xx = float(cx) + float(sd) * std::min(11.0f, 1.5f * te);
        bool landed = tt >= tLand;
        blit_xf(g, s, xx, yy, landed ? float(sd) * PI_H : float(sd) * tt * 0.45f, sc, sc, flip, landed ? 150 : 40, t < 2 ? WHITE : 0);
        for (int i = 0; i < 5 && t < 34; i++) { // gibs
            uint32_t hv = hsh(i, cx);
            float gx = float(cx) + float(sd) * (1.0f + float(hv % 4)) * 0.9f * tt, gy = cyIdle - (3.0f + float((hv >> 3) % 5)) * 1.3f * tt + 0.3f * tt * tt;
            if (gy < float(base)) g.rect(int(gx), int(gy), px + 1, px + 1, i % 2 ? RGB(170, 60, 50) : RGB(120, 20, 20));
        }
        if (landed && t < 60) { // smoke
            int ts = t - int(tLand);
            for (int i = 0; i < 3; i++) g.rect_blend(cx + sd * 8 - 3 + i * 3, base - 4 - ts / 2 - i * 3, 4, 4, RGB(70, 70, 75), std::max(0, 150 - ts * 4));
        }
        pool(t - 12, small ? 12 : 18);
        break;
    }
    case C_FIRE: {
        float dark = std::min(230.0f, float(t) * 14.0f);
        bool collapse = t >= 22;
        float prog = collapse ? std::min(1.0f, float(t - 22) / 6.0f) : 0.0f;
        float cy = cyIdle + (cyLie - cyIdle) * prog;
        float jx = !collapse ? float((t % 2) * 2 - 1) : 0.0f;
        blit_xf(g, s, float(cx) + jx, cy, float(sd) * PI_H * prog, sc, sc, flip, int(dark), 0);
        if (t < 34) flames(t, float(cx), cy + hh * 0.3f, int(13 * sc) + 3);
        if (t > 28 && (t / 3) % 2) { g.rect(cx - 4, base - 2, 1, 1, RGB(255, 140, 40)); g.rect(cx + 3, base - 3, 1, 1, RGB(255, 200, 80)); }
        break;
    }
    case C_CRUSH: {
        const float squash = 0.28f;
        blit_xf(g, s, float(cx), float(base) - hh * squash + 1, 0, sc * 1.25f, sc * squash, flip, t > 12 ? 40 : 0, 0);
        int by = int(float(base) - 2.0f * hh * squash - 6.0f);
        g.rect(cx - 8, by, 16, 6, RGB(150, 85, 70));
        g.rect(cx - 8, by, 16, 1, RGB(200, 130, 110));
        g.rect(cx - 8, by + 5, 16, 1, RGB(105, 55, 45));
        if (t < 14) {
            g.rect_blend(cx - 12 - t, base - 4 - t / 2, 6, 4, RGB(150, 140, 130), 160 - t * 10);
            g.rect_blend(cx + 7 + t, base - 4 - t / 2, 6, 4, RGB(150, 140, 130), 160 - t * 10);
        }
        pool(t + 8, 18);
        break;
    }
    case C_SPIKE: {
        float drop = std::min(1.0f, float(t) / 6.0f);
        blit_xf(g, s, float(cx), cyIdle - 12.0f * (1.0f - drop) + 2.0f * drop, float(sd) * 0.3f * drop, sc, sc, flip, t > 24 ? 60 : 0, t < 2 ? WHITE : 0);
        spikes();
        if (t > 6) g.rect(cx + (t % 3) - 1, base - 9 + (t / 2) % 6, 1, 2, RED);
        break;
    }
    case C_LIGHTNING: {
        if (t < 10) {
            bool inv = (t / 2) % 2 == 0;
            blit_xf(g, s, float(cx), cyIdle, 0, sc, sc, flip, 0, inv ? WHITE : RGB(20, 20, 30));
            for (int y = base - 80; y < int(cyIdle - hh); y += 4) g.rect(cx + ((y / 4 + t) % 2 ? 2 : -2), y, 2, 5, inv ? WHITE : RGB(150, 190, 255));
            g.rect_blend(cx - 20, base - 40, 40, 40, RGB(180, 210, 255), 50);
        } else if (t < 26) {
            blit_xf(g, s, float(cx) + float((t % 2) - 0), cyIdle, 0, sc, sc, flip, 200, 0);
            g.rect_blend(cx - 2, int(cyIdle - hh) - (t - 10) / 2, 5, 4, RGB(70, 70, 75), 140);
        } else { // ash
            g.rect(cx - 6, base - 3, 12, 3, RGB(40, 40, 44));
            g.rect(cx - 4, base - 5, 8, 2, RGB(62, 62, 66));
            if ((t / 4) % 2) g.rect(cx + 2, base - 6, 1, 1, RGB(255, 140, 40));
        }
        break;
    }
    case C_FALL: {
        float yy = cyIdle + 0.6f * float(t) * float(t);
        if (yy < float(base) + 26) blit_xf(g, s, float(cx), yy, float(sd) * float(t) * 0.35f, sc, sc, flip, 0, 0);
        else { g.rect(cx - 4, base - 2, 8, 2, RGB(90, 80, 70)); g.rect(cx - 2, base - 3, 4, 1, RGB(120, 108, 95)); }
        break;
    }
    default: { // bullet, melee: knocked back and down
        const bool melee = k.cause == C_MELEE;
        float prog = std::min(1.0f, float(t) / (melee ? 12.0f : 16.0f));
        float push = float(sd) * std::min(melee ? 13.0f : 7.0f, float(t) * (melee ? 1.5f : 0.8f));
        float cy = cyIdle + (cyLie - cyIdle) * prog;
        blit_xf(g, s, float(cx) + push, cy, float(sd) * PI_H * prog, sc, sc, flip, t > 20 ? 70 : 0, t < 2 ? WHITE : 0);
        spray(10, t, float(cx), cyIdle - hh * 0.4f);
        pool(t, small ? 12 : 18);
        if (melee && t < 4) for (int i = 0; i < 6; i++) g.rect(cx - sd * 8 + sd * i * 3, int(cyIdle - hh) + i * 3, 2, 2, WHITE); // slash
        break;
    }
    }
}

void Game::draw_exfil_ui(Gfx &g) {
    if (!(ex.phase == 3 && ex.recap)) return;
    int fade = std::min(ex.resT, 30);
    g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), fade * 7);
    const int PW = VW - 16, PH = 224;
    const int PX = 8, PY = 8 - (30 - fade) * 8; // the panel slides down from the top
    g.rect(PX - 2, PY - 2, PW + 4, PH + 4, RGB(12, 12, 18));
    g.rect(PX - 1, PY - 1, PW + 2, PH + 2, RGB(255, 215, 60));
    g.rect(PX, PY, PW, PH, RGB(26, 30, 46));
    const char *title = "MISSION COMPLETE";
    g.text_shadow(PX + (PW - Gfx::text_w(title)) / 2, PY + 5, title, RGB(255, 215, 60));
    char buf[48];
    float secs = float(ex.finalTicks) / 62.5f;
    snprintf(buf, sizeof(buf), "TIME %02d:%02d.%d", int(secs) / 60, int(secs) % 60, int(secs * 10) % 10);
    g.text_shadow(PX + 8, PY + 16, buf, RGB(255, 255, 255));
    snprintf(buf, sizeof(buf), "KILLS %d", int(killLog.size()));
    g.text_shadow(PX + 8, PY + 26, buf, RGB(255, 255, 255));
    int counts[E_TYPES] = {0};
    for (const KillRec &kr : killLog) if (kr.type < E_TYPES) counts[kr.type]++;
    static const int order6[6] = {E_MOOK, E_RPG, E_HEAVY, E_BOMBER, E_DOG, E_OFFICER};
    for (int k = 0; k < 6; k++) {
        int x = PX + 100 + k * 32;
        draw_icon12(g, enSpr[order6[k]][0], x, PY + 14);
        snprintf(buf, sizeof(buf), "X%d", counts[order6[k]]);
        g.text(x + 1, PY + 28, buf, RGB(200, 210, 230));
    }
    // victims: one after another, each re-enacting his death, filling the rows left to right
    int total = int(killLog.size());
    bool small = total > 40;
    int n = std::min(total, 96);
    int cols = small ? 16 : 10, cellW = small ? 18 : 28, cellH = small ? 24 : 38;
    int gx0 = PX + (PW - cols * cellW) / 2, gy0 = PY + 48;
    for (int i = 0; i < n; i++) {
        int age = ex.resT - 45 - i * ex.interval;
        if (age < 0) break;
        int row = i / cols, col = i % cols;
        int cx = gx0 + col * cellW + cellW / 2, base = gy0 + row * cellH + cellH - 4;
        g.rect(cx - cellW / 2 + 2, base + 1, cellW - 4, 1, RGB(62, 54, 50));
        draw_victim(g, killLog[size_t(i)], age, cx, base, small);
    }
    if (ex.done && (tick / 20) % 2 == 0) g.text_shadow(PX + (PW - Gfx::text_w("PRESS A")) / 2, PY + PH - 11, "PRESS A", RGB(255, 235, 120));
}

// ---- full-screen cards: level intro and mission failed, arctic snow -------------------------------
void Game::draw_overlays(Gfx &g) {
    if (gs == GS_PLAY && world == 3) { // arctic snowfall
        for (int i = 0; i < 70; i++) {
            int x = (i * 53 + tick / 2 + int(camX * 0.5f)) % VW, y = (i * 29 + tick * (1 + i % 3) / 2) % VH;
            g.rect(x, y, 2, 2, RGB(255, 255, 255));
        }
    }
    if (gs == GS_PLAY && introT > 0) { // black card, level title, a pep-talk (voiced), then it fades into the level
        int a = introT < INTRO_HOLD ? 256 : 256 - 256 * (introT - INTRO_HOLD) / INTRO_FADE;
        if (a < 0) a = 0;
        static Gfx card;
        card.clear(RGB(0, 0, 0));
        char b[48];
        snprintf(b, sizeof(b), "LEVEL %d-%d", world + 1, stage + 1);
        card.text_shadow((VW - Gfx::text_w(b, 3)) / 2, 62, b, RGB(255, 215, 60), 3);
        card.text_shadow((VW - Gfx::text_w(th.name, 2)) / 2, 96, th.name, RGB(255, 255, 255), 2);
        card.rect(60, 124, VW - 120, 1, RGB(110, 40, 30));
        const char *phrase = INTRO_TXT[introPhrase % NUM_INTRO];
        card.text_shadow((VW - Gfx::text_w(phrase, 2)) / 2, 138, phrase, RGB(255, 120, 40), 2);
        char objb[48];
        if (stage == STAGES - 1) snprintf(objb, sizeof(objb), "BOSS: %s", boss_name(world)); else snprintf(objb, sizeof(objb), "ELIMINATE THE ENEMY OFFICER");
        const char *obj = objb;
        card.text((VW - Gfx::text_w(obj)) / 2, 192, obj, RGB(170, 170, 185));
        if (a > 0)
            for (int i = 0; i < VW * VH; i++) {
                u32 p = g.px[i], c = card.px[i];
                int r = (int((p >> 16) & 255) * (256 - a) + int((c >> 16) & 255) * a) >> 8;
                int gg = (int((p >> 8) & 255) * (256 - a) + int((c >> 8) & 255) * a) >> 8;
                int bb = (int(p & 255) * (256 - a) + int(c & 255) * a) >> 8;
                g.px[i] = RGB(r, gg, bb);
            }
    }
    if (gs == GS_OVER) { // fade to black, MISSION FAILED, and a voice line
        int a = std::min(256, stateT * 256 / FAIL_FADE);
        g.rect_blend(0, 0, VW, VH, RGB(0, 0, 0), a);
        if (stateT > 40) {
            int tt = std::min(256, (stateT - 40) * 256 / 40);
            const char *title = "MISSION FAILED";
            g.text((VW - Gfx::text_w(title, 3)) / 2, 60, title, RGB(255 * tt >> 8, 60 * tt >> 8, 50 * tt >> 8), 3);
        }
    }
}

// ---- gas bottles ------------------------------------------------------------------------
void Game::draw_gas(Gfx &g, int ox, int oy) {
    for (const Gas &b : gas) {
        if (!b.alive) continue;
        int sx = int(b.a.x) - ox, sy = int(b.a.y) - oy;
        if (sx < -30 || sx > VW + 30 || sy < -30 || sy > VH + 30) continue;
        u32 body = RGB(70, 118, 170), light = RGB(130, 175, 215), dark = RGB(30, 55, 90);
        if (b.state != 2) {
            int jx = b.state == 1 ? ((tick / 2) % 2 ? 1 : -1) : 0; // rattles while heating up
            g.rect(sx - 4 + jx, sy - 12, 8, 12, RGB(15, 25, 40));
            g.rect(sx - 3 + jx, sy - 11, 6, 10, body);
            g.rect(sx - 3 + jx, sy - 11, 2, 10, light);
            g.rect(sx - 3 + jx, sy - 6, 6, 2, RGB(235, 235, 240));
            g.rect(sx - 3 + jx, sy - 2, 6, 2, dark);
            g.rect(sx - 2 + jx, sy - 14, 4, 3, RGB(180, 150, 60));   // valve / cap
            g.rect(sx - 1 + jx, sy - 15, 2, 1, RGB(120, 100, 40));
            if (b.state == 1) {
                g.rect(sx - 1 + jx, sy - 19 - (tick / 2) % 3, 2, 4, RGB(255, 140, 30));
                g.rect(sx + jx, sy - 18 - (tick / 3) % 2, 1, 2, RGB(255, 235, 120));
            }
        } else {
            int cy = sy - 6;
            int dir = b.dx >= 0 ? 1 : -1; // nose points the way it travels, cap (and the flame) at the back
            int bx = sx - 7;
            g.rect(bx - 1, cy - 4, 16, 8, RGB(15, 25, 40));
            g.rect(bx, cy - 3, 14, 6, body);
            g.rect(bx, cy - 3, 14, 2, light);
            g.rect(bx + (dir > 0 ? 8 : 4), cy - 3, 2, 6, RGB(235, 235, 240));
            g.rect(bx + (dir > 0 ? 12 : 0), cy - 3, 2, 6, dark);
            g.rect(sx - dir * 8 - (dir > 0 ? 3 : -1), cy - 2, 3, 4, RGB(180, 150, 60));
            g.rect(sx - dir * 11 - (dir > 0 ? 3 : -1), cy - 1, 4 + (tick / 2) % 3, 2, RGB(255, 225, 90));
        }
    }
}
