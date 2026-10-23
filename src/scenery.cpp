// Broyoo - living background: rocks, tents with camels, igloos with penguins, birds that scatter when somebody comes close
#include "game.h"
#include "audio.h"
#include <cmath>
#include <algorithm>

static u32 shade_c_local(u32 c, int d) {
    int r = int((c >> 16) & 255) + d, g = int((c >> 8) & 255) + d, b = int(c & 255) + d;
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return RGB(r, g, b);
}

static u32 mixc(u32 a, u32 b, int num, int den) {
    int r = int((a >> 16) & 255) * (den - num) / den + int((b >> 16) & 255) * num / den;
    int g = int((a >> 8) & 255) * (den - num) / den + int((b >> 8) & 255) * num / den;
    int bl = int(a & 255) * (den - num) / den + int(b & 255) * num / den;
    return RGB(r, g, bl);
}

// ---- generation ---------------------------------------------------------------------------------
void Game::gen_scenery(const int *hgt) {
    auto surfaceOk = [&](int x) { return x >= 16 && x < W - 12 && hgt[x] < H - 2; };
    auto flat = [&](int x, int w) {
        if (x < 18 || x + w >= W - 14) return false;
        for (int k = 0; k < w; k++) if (hgt[x + k] != hgt[x] || hgt[x + k] >= H - 2) return false;
        return true;
    };
    int treeId = 1;
    auto addTree = [&](int x, int kind, int hmin, int hmax) {
        Tree t;
        t.x = float(x * TS + rndi(0, 7)); t.y = float(hgt[x] * TS);
        t.h = rndi(hmin, hmax); t.lean = kind == 0 ? rndi(-9, 9) : rndi(-3, 3);
        t.kind = kind; t.id = treeId++;
        trees.push_back(t);
        return t.id;
    };
    auto addBird = [&](float x, float y, int tree) {
        Bird b; b.x = x; b.y = y; b.tree = tree; b.var = rndi(0, 5); b.face = rnd() < 0.5f ? -1 : 1; b.t = rndi(0, 200);
        birds.push_back(b);
    };
    // trees: dense palms in the jungle, a few oasis palms in the desert, pines on the ice, dead trunks in the ruins and in hell
    int treeKind = world == 3 ? 1 : 2;
    if (world == 0) {
        for (int x = 14; x < W - 8; x += rndi(3, 7)) {
            if (!surfaceOk(x)) continue;
            int id = addTree(x, 0, 44, 100);
            if (rnd() < 0.55f) { int n = rndi(1, 2); for (int k = 0; k < n; k++) addBird(trees.back().x + float(k * 5 - 3), trees.back().y - float(trees.back().h) + 1, id); } // birds perched in the crown
        }
    } else if (world == 1) {
        for (int i = 0; i < 6; i++) { int x = rndi(30, W - 30); if (surfaceOk(x)) addTree(x, 0, 36, 70); }
    } else if (world == 3) {
        for (int x = 14; x < W - 8; x += rndi(5, 11)) if (surfaceOk(x)) addTree(x, treeKind, 30, 70);
    } else if (world == 4 || world == 5) {
        for (int x = 14; x < W - 8; x += rndi(12, 24)) if (surfaceOk(x)) addTree(x, treeKind, 26, 56);
    }
    // rocks and boulders everywhere
    int nRocks = W / 15;
    for (int i = 0; i < nRocks; i++) {
        int x = rndi(16, W - 12);
        if (!surfaceOk(x)) continue;
        Deco d; d.type = D_ROCK; d.x = float(x * TS + rndi(0, 7)); d.y = float(hgt[x] * TS); d.var = rndi(0, 2); d.w = rndi(7, 16);
        decos.push_back(d);
        if (rnd() < 0.4f) { d.x += float(rndi(8, 14)); d.w = rndi(5, 9); d.var = rndi(0, 2); decos.push_back(d); } // a small one beside it
    }
    // desert: tents with their dromedaries, cacti
    if (world == 1) {
        int nTents = rndi(1, 2);
        for (int i = 0; i < nTents; i++) {
            for (int tries = 0; tries < 60; tries++) {
                int x = rndi(34, W - 50);
                if (!flat(x, 12)) continue;
                Deco t; t.type = D_TENT; t.x = float((x + 4) * TS); t.y = float(hgt[x] * TS); t.var = rndi(0, 1);
                decos.push_back(t);
                int nc = rndi(1, 2);
                for (int k = 0; k < nc; k++) { Deco c; c.type = D_CAMEL; c.x = t.x + 30.0f + float(k * 26); c.y = t.y; c.var = rndi(0, 3); c.face = rnd() < 0.5f ? -1 : 1; decos.push_back(c); }
                break;
            }
        }
        for (int i = 0; i < 8; i++) { int x = rndi(20, W - 20); if (surfaceOk(x)) { Deco c; c.type = D_CACTUS; c.x = float(x * TS + 4); c.y = float(hgt[x] * TS); c.var = rndi(0, 2); decos.push_back(c); } }
    }
    // ice: an igloo with its penguins, and a few other groups
    if (world == 3) {
        for (int tries = 0; tries < 600; tries++) {
            int x = rndi(40, W - 60);
            if (!flat(x, tries < 300 ? 14 : 10)) continue;
            Deco ig; ig.type = D_IGLOO; ig.x = float((x + 6) * TS); ig.y = float(hgt[x] * TS);
            decos.push_back(ig);
            int n = rndi(3, 5);
            for (int k = 0; k < n; k++) { Deco p; p.type = D_PENGUIN; p.x = ig.x + 26.0f + float(k * 8 + rndi(0, 3)); p.y = ig.y; p.var = rndi(0, 9); p.face = rnd() < 0.5f ? -1 : 1; decos.push_back(p); }
            break;
        }
        for (int gI = 0; gI < 3; gI++) {
            int x = rndi(30, W - 40);
            if (!flat(x, 8)) continue;
            int n = rndi(2, 3);
            for (int k = 0; k < n; k++) { Deco p; p.type = D_PENGUIN; p.x = float(x * TS + k * 9); p.y = float(hgt[x] * TS); p.var = rndi(0, 9); p.face = rnd() < 0.5f ? -1 : 1; decos.push_back(p); }
        }
    }
    // night base: lamp posts; ruins: rubble and broken lamps; hell: skull piles
    if (world == 2) for (int x = 24; x < W - 12; x += rndi(22, 34)) if (surfaceOk(x)) { Deco d; d.type = D_LAMP; d.x = float(x * TS + 4); d.y = float(hgt[x] * TS); d.var = 0; decos.push_back(d); }
    if (world == 4) {
        for (int x = 20; x < W - 12; x += rndi(16, 28)) if (surfaceOk(x)) { Deco d; d.type = rnd() < 0.5f ? D_RUBBLE : D_LAMP; d.x = float(x * TS + 4); d.y = float(hgt[x] * TS); d.var = 1; d.w = rndi(10, 18); decos.push_back(d); }
    }
    if (world == 5) for (int x = 20; x < W - 12; x += rndi(18, 30)) if (surfaceOk(x)) { Deco d; d.type = D_SKULLS; d.x = float(x * TS + 4); d.y = float(hgt[x] * TS); d.var = rndi(0, 2); decos.push_back(d); }
    // birds pecking on the ground: they take off when anyone gets close (jungle, desert, base, city)
    if (world != 3 && world != 5) {
        for (int i = 0; i < W / 18; i++) {
            int x = rndi(18, W - 14);
            if (!surfaceOk(x)) continue;
            int n = rndi(1, 3);
            for (int k = 0; k < n; k++) addBird(float(x * TS + k * 7), float(hgt[x] * TS), -1);
        }
    }
}

// ---- birds ---------------------------------------------------------------------------------------
static void take_off(Bird &b, float fromX) {
    b.state = 1; b.t = 0;
    int h = int(b.x * 13.0f + b.y * 7.0f) & 63;
    b.vx = (b.x >= fromX ? 1.0f : -1.0f) * (0.8f + float(h) / 70.0f);
    b.vy = -(1.4f + float((h * 5) & 63) / 105.0f);
    b.face = b.vx >= 0 ? 1 : -1;
}

void Game::scare_birds(float x, float y, float r) {
    for (Bird &b : birds) {
        if (b.state != 0) continue;
        float dx = b.x - x, dy = b.y - y;
        if (dx * dx + dy * dy < r * r) take_off(b, x);
    }
}

void Game::scare_tree(int id) {
    for (Bird &b : birds) if (b.state == 0 && b.tree == id) take_off(b, b.x + (int(b.x) & 1 ? -1.0f : 1.0f));
}

void Game::update_scenery() {
    if (birds.empty()) return;
    float cx = camX + VW / 2.0f;
    for (size_t i = 0; i < birds.size();) {
        Bird &b = birds[i];
        if (fabsf(b.x - cx) > 300) { // far from the screen: asleep (flying ones are simply dropped)
            if (b.state == 1) { birds[i] = birds.back(); birds.pop_back(); continue; }
            i++; continue;
        }
        b.t++;
        if (b.state == 0) {
            // somebody close by (the bro or any soldier) makes them take off
            if (pl.alive && fabsf(pl.a.x - b.x) < 34 && fabsf((pl.a.y - 6) - b.y) < 28) take_off(b, pl.a.x);
            else for (size_t k = 0; k < en.size(); k++) {
                const Enemy &e = en[k];
                if (e.alive && fabsf(e.a.x - b.x) < 30 && fabsf((e.a.y - 6) - b.y) < 26) { take_off(b, e.a.x); break; }
            }
        } else {
            b.x += b.vx; b.y += b.vy;
            b.vy = std::min(b.vy + 0.012f, -0.45f);
            if (b.t > 220 || b.y < -40) { birds[i] = birds.back(); birds.pop_back(); continue; }
        }
        i++;
    }
}

void Game::draw_birds(Gfx &g, int ox, int oy) {
    static const u32 col[6][2] = {
        {RGB(210, 50, 45), RGB(240, 200, 60)}, {RGB(50, 170, 80), RGB(230, 200, 60)}, {RGB(60, 110, 210), RGB(240, 210, 70)},
        {RGB(130, 100, 70), RGB(230, 190, 70)}, {RGB(90, 90, 100), RGB(220, 170, 70)}, {RGB(220, 160, 50), RGB(250, 230, 120)}};
    for (const Bird &b : birds) {
        int x = int(b.x) - ox, y = int(b.y) - oy;
        if (x < -10 || x > VW + 10 || y < -50 || y > VH + 10) continue;
        u32 c = world == 0 ? col[b.var % 3][0] : world == 1 ? col[3][0] : world == 2 ? RGB(40, 40, 52) : col[4][0];
        u32 c2 = world == 0 ? col[b.var % 3][1] : RGB(235, 190, 70);
        int f = b.face;
        if (b.state == 0) {
            bool peck = ((b.t + b.var * 11) / 14) % 4 == 0 && b.tree < 0;
            g.rect(x - 2, y - 4, 5, 3, c);                                  // body
            g.pset(x - 3 * f, y - 3, shade_c_local(c, -35));                // tail
            g.rect(x + (f > 0 ? 2 : -3), y - (peck ? 3 : 5), 2, 2, c);      // head
            g.pset(x + (f > 0 ? 4 : -4), y - (peck ? 2 : 4), c2);           // beak
            g.pset(x, y - 1, RGB(60, 50, 40)); g.pset(x + 1, y - 1, RGB(60, 50, 40));
        } else {
            bool up = (b.t / 3) % 2 == 0;
            g.rect(x - 1, y - 1, 3, 2, c);
            g.pset(x + 2 * f, y - 1, c2);
            if (up) { g.pset(x - 2, y - 3, c); g.pset(x - 3, y - 4, c); g.pset(x + 2, y - 3, c); g.pset(x + 3, y - 4, c); }
            else { g.pset(x - 2, y + 1, c); g.pset(x - 3, y + 2, c); g.pset(x + 2, y + 1, c); g.pset(x + 3, y + 2, c); }
        }
    }
}

// ---- static scenery -------------------------------------------------------------------------------
void Game::draw_scenery(Gfx &g, int ox, int oy) {
    for (const Deco &d : decos) {
        int x = int(d.x) - ox, y = int(d.y) - oy;
        if (x < -60 || x > VW + 60 || y < -20 || y > VH + 60) continue;
        int f = d.face;
        switch (d.type) {
        case D_ROCK: {
            int w = d.w, h = std::max(4, w * 2 / 3);
            u32 base = mixc(th.stone, th.dirt, 1, 4);
            for (int j = 0; j < h; j++) {
                float t = float(j) / float(h);                       // 0 top .. 1 bottom
                int half = int(float(w) / 2.0f * sqrtf(std::max(0.0f, 1.0f - (1.0f - t) * (1.0f - t) * 0.95f)));
                if (half < 1) half = 1;
                for (int i = -half; i <= half; i++) {
                    u32 c = base;
                    if (i < -half / 3 && j < h - 2) c = shade_c_local(base, 26);        // lit side
                    else if (i > half / 3 || j >= h - 2) c = shade_c_local(base, -38);  // shadow side / underside
                    if ((i * 5 + j * 3 + d.var + 90) % 9 == 0) c = shade_c_local(c, -18); // speckle
                    g.pset(x + i, y - h + j + 1, c);
                }
            }
            break;
        }
        case D_TENT: {
            u32 a = d.var ? RGB(196, 62, 52) : RGB(60, 110, 170), b = RGB(236, 224, 196);
            int H2 = 20, W2 = 18;
            for (int j = 0; j < H2; j++) {
                int half = W2 * (j + 1) / H2;
                for (int i = -half; i <= half; i++) g.pset(x + i, y - H2 + j + 1, ((i + 40) / 3) % 2 ? a : b);
                g.pset(x - half, y - H2 + j + 1, shade_c_local(a, -45)); g.pset(x + half, y - H2 + j + 1, shade_c_local(a, -45));
            }
            g.rect(x - 3, y - 11, 6, 11, RGB(50, 34, 28));              // the entrance
            g.rect(x - 2, y - 12, 4, 2, RGB(50, 34, 28));
            g.rect(x, y - H2 - 5, 1, 6, RGB(110, 80, 50));               // the pole and its pennant
            g.rect(x + 1, y - H2 - 5, 4, 2, a);
            break;
        }
        case D_CAMEL: { // dromedary: standing or kneeling, chewing
            bool kneel = d.var % 2 == 1;
            int bob = int(sinf(float(tick) * 0.05f + d.x) * 1.4f);
            u32 body = RGB(198, 152, 94), dk = RGB(150, 108, 64), lt = RGB(226, 188, 132);
            auto R = [&](int x0, int y0, int w, int h, u32 c) { int xa = x + x0 * f, xb = x + (x0 + w - 1) * f; g.rect(std::min(xa, xb), y + y0, w, h, c); };
            int legH = kneel ? 3 : 8;
            R(-9, -legH - 9, 18, 7, body);                  // body
            R(-9, -legH - 9, 18, 1, lt);
            R(-2, -legH - 13, 5, 4, body);                  // the hump
            R(-2, -legH - 13, 5, 1, lt);
            R(7, -legH - 16 + bob / 2, 3, 9, body);         // neck
            R(8, -legH - 19 + bob, 6, 3, body);             // head
            R(13, -legH - 18 + bob, 2, 2, dk);              // muzzle
            g.pset(x + 10 * f, y - legH - 18 + bob, RGB(20, 14, 12));
            R(-9, -legH - 4, 18, 2, dk);                    // belly shadow
            R(-8, -legH, 2, legH, dk); R(-4, -legH, 2, legH, body); R(3, -legH, 2, legH, body); R(7, -legH, 2, legH, dk);   // legs
            R(-10, -legH - 8, 1, 4, dk);                    // tail
            break;
        }
        case D_CACTUS: {
            u32 gr = RGB(60, 130, 70), gl = RGB(96, 170, 100);
            int h = 14 + d.var * 3;
            g.rect(x - 1, y - h, 3, h, gr); g.rect(x - 1, y - h, 1, h, gl);
            g.rect(x - 5, y - h / 2 - 2, 4, 2, gr); g.rect(x - 5, y - h / 2 - 6, 2, 5, gr);
            g.rect(x + 2, y - h / 2 + 1, 4, 2, gr); g.rect(x + 4, y - h / 2 - 4, 2, 6, gr);
            break;
        }
        case D_IGLOO: {
            int W2 = 15, H2 = 14;
            u32 ice = RGB(232, 242, 252), line = RGB(160, 190, 220), shadow = RGB(190, 212, 236);
            for (int j = 0; j < H2; j++) {
                float t = float(H2 - j) / float(H2);                      // 1 at the base, 0 at the top
                int half = int(float(W2) * sqrtf(std::max(0.0f, 1.0f - (1.0f - t) * (1.0f - t))));
                for (int i = -half; i <= half; i++) {
                    bool course = (j % 4) == 3, joint = ((i + 60 + (j / 4 % 2) * 3) % 6) == 0;
                    g.pset(x + i, y - H2 + j, (course || joint) ? line : (i > half / 2 ? shadow : ice));
                }
            }
            g.rect(x - 4, y - 7, 8, 7, RGB(60, 90, 130)); g.rect(x - 3, y - 8, 6, 1, RGB(60, 90, 130)); // entrance tunnel
            g.rect(x - 2, y - 6, 4, 6, RGB(24, 36, 60));
            break;
        }
        case D_PENGUIN: {
            int phase = (tick / 14 + d.var * 3) % 12;
            int sway = phase < 3 ? 1 : phase < 6 ? 0 : phase < 9 ? -1 : 0;     // a little waddle on the spot
            u32 blk = RGB(28, 30, 40), wht = RGB(240, 240, 245), org = RGB(240, 150, 40);
            int px0 = x + sway;
            g.rect(px0 - 2, y - 9, 5, 8, blk);                  // body
            g.rect(px0 - 1, y - 7, 3, 5, wht);                  // belly
            g.rect(px0 - 2, y - 12, 5, 3, blk);                 // head
            g.pset(px0 + (f > 0 ? 3 : -3), y - 10, org);        // beak
            g.pset(px0 + (f > 0 ? 1 : -1), y - 11, RGB(255, 255, 255));   // eye
            g.pset(px0 + (f > 0 ? -3 : 3), y - 6, blk);         // flipper
            g.rect(px0 - 2, y - 1, 2, 1, org); g.rect(px0 + 1, y - 1, 2, 1, org);   // feet
            break;
        }
        case D_LAMP: {
            bool broken = d.var == 1;
            u32 pole = RGB(70, 72, 82);
            int h = broken ? 17 : 26;
            g.rect(x - 1, y - h, 2, h, pole);
            g.rect(x - 3, y - 2, 6, 2, shade_c_local(pole, -15));
            if (!broken) {
                g.rect(x - 1, y - h - 2, 6, 2, pole);
                bool on = ((tick / 40) + int(d.x)) % 11 != 0;
                if (on) { g.rect(x + 3, y - h, 2, 2, RGB(255, 236, 150)); g.rect_blend(x - 4, y - h - 4, 14, 12, RGB(255, 236, 150), 38); }
            } else { g.rect(x - 1, y - h - 1, 5, 2, pole); g.pset(x + 4, y - h + 1, RGB(120, 120, 130)); }
            break;
        }
        case D_RUBBLE: {
            for (int i = 0; i < d.w; i += 3) {
                int hh = 2 + ((i * 7 + int(d.x)) % 5);
                g.rect(x - d.w / 2 + i, y - hh, 3, hh, shade_c_local(th.stone, ((i / 3) % 3) * 14 - 22));
            }
            g.rect(x - d.w / 2 + 1, y - 7, 2, 1, RGB(120, 60, 50));
            break;
        }
        case D_SKULLS: {
            u32 bone = RGB(222, 214, 190), bd = RGB(150, 140, 120);
            for (int k = 0; k < 3 + d.var; k++) {
                int sx = x - 7 + (k * 5) % 14, sy = y - 5 - (k / 3) * 4;
                g.rect(sx, sy, 5, 4, bone); g.rect(sx + 1, sy + 3, 3, 1, bone);
                g.pset(sx + 1, sy + 1, RGB(20, 10, 10)); g.pset(sx + 3, sy + 1, RGB(20, 10, 10)); g.pset(sx + 2, sy + 3, bd);
            }
            g.rect(x + 8, y - 12, 1, 12, RGB(80, 60, 50)); g.rect(x + 6, y - 15, 5, 4, bone); g.pset(x + 7, y - 14, RGB(20, 10, 10)); g.pset(x + 9, y - 14, RGB(20, 10, 10)); // skull on a pike
            break;
        }
        default: break;
        }
    }
}
