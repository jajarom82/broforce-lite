// Broyoo - bosses: one per world, each with its own mechanic (all original pixel art)
//   0 GENERAL MORTARO  mortar barrages; unbreakable shelters protect you but you cannot shoot from them
//   1 IRON SCORPION    charging tank; make it hit the wall to stun it (triple damage), hide on the platforms
//   2 THE WARDEN       searchlight drone; a laser strikes where the spot stops, roofs block it, shoot it while it vents
//   3 FROST COLOSSUS   armoured: bullets barely scratch it, cut the cables of the hanging ice slabs to crush it
//   4 JUGGERNAUT       bullet-proof mech: only explosions hurt it, supply drops bring gas bottles
//   5 TERRORKOPTER     helicopter gunship with airstrikes; shelters protect you
#include "game.h"
#include "audio.h"
#include "phrases.h"
#include <cmath>
#include <algorithm>

static const float PI_B = 3.14159265f;
static constexpr int G = 40; // arena floor row

const char *boss_name(int world) {
    static const char *n[NUM_WORLDS] = {"GENERAL MORTARO", "IRON SCORPION", "THE WARDEN", "FROST COLOSSUS", "JUGGERNAUT", "TERRORKOPTER"};
    return n[world % NUM_WORLDS];
}

const char *boss_hint(int world) {
    static const char *n[NUM_WORLDS] = {
        "HIDE IN THE SHELTERS DURING THE BARRAGE",
        "LURE THE TANK INTO THE WALL TO STUN IT",
        "SHOOT THE DRONE WHILE IT VENTS - AVOID THE LASER",
        "CUT THE CABLES: DROP THE ICE ON THE COLOSSUS",
        "BULLETS FAIL - USE EXPLOSIONS AND GAS BOTTLES",
        "TAKE COVER FROM THE AIRSTRIKES"};
    return n[world % NUM_WORLDS];
}

// ---- arena -----------------------------------------------------------------------------
// an unbreakable shelter: metal roof, solid back wall on the boss side (right), open to the left
void Game::stamp_shelter(int x0, int g) {
    for (int y = g - 4; y < g; y++) for (int x = x0; x < x0 + 6; x++) set_tile(x, y, T_AIR); // nothing flammable inside
    for (int y = g; y < g + 3; y++) for (int x = x0 - 1; x < x0 + 9; x++) set_tile(x, y, T_METAL); // unbreakable foundation: shells cannot undermine the shelter
    for (int x = x0; x < x0 + 8; x++) { set_tile(x, g - 6, T_METAL); set_tile(x, g - 5, T_METAL); }
    for (int y = g - 4; y < g; y++) { set_tile(x0 + 6, y, T_METAL); set_tile(x0 + 7, y, T_METAL); }
    add_prop(PR_AMMO, float((x0 + 2) * TS), float(g * TS));
}

// a roof on two pillars (blocks the Warden's laser, not bullets from the sides)
void Game::stamp_canopy(int x0, int g) {
    for (int y = g - 4; y < g; y++) for (int x = x0 + 1; x < x0 + 6; x++) set_tile(x, y, T_AIR);
    for (int y = g; y < g + 3; y++) for (int x = x0 - 1; x < x0 + 8; x++) set_tile(x, y, T_METAL);
    for (int x = x0; x < x0 + 7; x++) set_tile(x, g - 5, T_METAL);
    for (int y = g - 4; y < g; y++) { set_tile(x0, y, T_METAL); set_tile(x0 + 6, y, T_METAL); }
}

static int jitter(Game &, int base, int spread, float r) { return base + int((r - 0.5f) * 2.0f * float(spread)); }

void Game::build_boss_arena(int wd) {
    int g = G;
    // checkpoint flag + ammo + two cages (each frees a bro)
    add_prop(PR_FLAG, float(20 * TS), float(g * TS));
    auto cage = [&](int x0) {
        for (int x = x0; x < x0 + 4; x++) for (int y = g - 2; y < g; y++) set_tile(x, y, T_BRICK);
        add_prop(PR_CAGE, float(x0 * TS + 16), float((g - 2) * TS));
    };
    int cageA = 40 + rndi(0, 14), cageB = 104 + rndi(0, 16);
    switch (wd) {
    case 0: { // shelters at random places on the left/middle, sandbags and crates between them
        int xs[3] = {jitter(*this, 30, 5, rnd()), jitter(*this, 74, 6, rnd()), jitter(*this, 118, 6, rnd())};
        for (int i = 0; i < 3; i++) stamp_shelter(xs[i], g);
        cageA = xs[0] + 16; cageB = xs[2] + 14;
        for (int i = 0; i < 4; i++) {
            int x = rndi(22, 150);
            bool near = false;
            for (int k = 0; k < 3; k++) if (x > xs[k] - 4 && x < xs[k] + 10) near = true;
            if (near) continue;
            set_tile(x, g - 1, T_CRATE); set_tile(x + 1, g - 1, T_CRATE); set_tile(x, g - 2, T_BARREL);
        }
        break;
    }
    case 1: { // platforms the tank cannot reach, crates on top; destructible pillars it will plough through
        int n = 5;
        for (int i = 0; i < n; i++) {
            int px = 24 + i * 32 + rndi(-4, 4), py = g - 6 - rndi(0, 2);
            for (int x = px; x < px + 8; x++) set_tile(x, py, T_BRICK);
            add_pillar(px + 1, py + 1, g); add_pillar(px + 6, py + 1, g);
            if (rnd() < 0.6f) set_tile(px + 3, py - 1, T_CRATE);
            if (rnd() < 0.4f) add_prop(PR_AMMO, float((px + 5) * TS), float((py - 1) * TS + 8));
        }
        for (int i = 0; i < 4; i++) { int x = rndi(30, 160); for (int y = g - 5; y < g; y++) set_tile(x, y, T_WOOD); }
        break;
    }
    case 2: { // four canopies (laser cover) and some crates
        for (int i = 0; i < 4; i++) stamp_canopy(26 + i * 38 + rndi(-4, 4), g);
        for (int i = 0; i < 5; i++) { int x = rndi(20, 170); set_tile(x, g - 1, T_CRATE); }
        cageA = 52 + rndi(0, 8); cageB = 128 + rndi(0, 10);
        break;
    }
    case 3: { // a metal ceiling and ice slabs hung by two wooden cables each (cut both and the slab drops)
        for (int x = 3; x < W - 3; x++) for (int y = 18; y <= 20; y++) set_tile(x, y, T_METAL);
        int n = 8;
        for (int i = 0; i < n; i++) {
            int bx = 30 + i * 18 + rndi(-3, 3), bw = 10;
            for (int y = 22; y <= 27; y++) for (int x = bx; x < bx + bw; x++) set_tile(x, y, T_STONE);
            for (int y = 21; y <= 24; y++) { set_tile(bx - 1, y, T_WOOD); set_tile(bx + bw, y, T_WOOD); }
        }
        cageA = 36 + rndi(0, 6); cageB = 100 + rndi(0, 10);
        break;
    }
    case 4: { // red crate stacks and gas bottles to blow up under the juggernaut
        for (int i = 0; i < 7; i++) {
            int x = 26 + i * 22 + rndi(-4, 4);
            for (int k = 0; k < 2; k++) { set_tile(x + k, g - 1, T_BARREL); set_tile(x + k, g - 2, T_BARREL); }
            if (rnd() < 0.5f) set_tile(x, g - 3, T_BARREL);
        }
        for (int x : {44, 88, 132, 160}) add_gas(float((x + rndi(-4, 4)) * TS + 4), float(g * TS));
        for (int i = 0; i < 2; i++) { int px = 50 + i * 70 + rndi(-6, 6); for (int x = px; x < px + 10; x++) set_tile(x, g - 7, T_BRICK); add_pillar(px + 1, g - 6, g); add_pillar(px + 8, g - 6, g); }
        break;
    }
    default: { // the Terrorkopter: the old platform arena plus three shelters
        for (int i = 0; i < 4; i++) {
            int px = 40 + i * 38, py = 33 + (i & 1) * 3;
            for (int x = px; x < px + 12; x++) set_tile(x, py, T_BRICK);
            add_pillar(px + 1, py + 1, 40); add_pillar(px + 10, py + 1, 40);
            set_tile(px + 5, py - 1, T_BARREL);
            set_tile(px + 7, py - 1, T_CRATE);
        }
        for (int x = 20; x < 190; x += 14) if (x % 3 == 0) set_tile(x, 39, T_BARREL);
        for (int x : {34, 92, 150}) add_gas(float(x * TS + 4), float(40 * TS));
        int xs[3] = {jitter(*this, 24, 3, rnd()), jitter(*this, 80, 6, rnd()), jitter(*this, 136, 6, rnd())};
        for (int i = 0; i < 3; i++) stamp_shelter(xs[i], g);
        cageA = 62 + rndi(0, 6); cageB = 118 + rndi(0, 8);
        break;
    }
    }
    add_prop(PR_AMMO, float(30 * TS), float(g * TS));
    add_prop(PR_AMMO, float(100 * TS), float(g * TS));
    cage(cageA);
    cage(cageB);
}

void Game::spawn_boss(int wd) {
    Enemy b;
    b.type = E_BOSS;
    b.kind = wd;
    b.state = 0;
    b.dir = -1;
    b.a.face = -1;
    switch (wd) {
    case 0: b.a.w = 30; b.a.h = 42; b.a.x = float((W - 16) * TS); b.a.y = float(G * TS); b.hp = b.maxhp = 46; break;
    case 1: b.a.w = 46; b.a.h = 24; b.a.x = float((W - 18) * TS); b.a.y = float(G * TS); b.hp = b.maxhp = 52; break;
    case 2: b.a.w = 30; b.a.h = 20; b.a.x = float(100 * TS); b.a.y = float(24 * TS); b.hp = b.maxhp = 40; b.fx = float(100 * TS); break;
    case 3: b.a.w = 28; b.a.h = 38; b.a.x = float((W - 30) * TS); b.a.y = float(G * TS); b.hp = b.maxhp = 100; break;
    case 4: b.a.w = 36; b.a.h = 38; b.a.x = float((W - 24) * TS); b.a.y = float(G * TS); b.hp = b.maxhp = 64; break;
    default: b.a.w = 36; b.a.h = 16; b.a.x = float((W - 40) * TS); b.a.y = 24.0f * TS; b.hp = b.maxhp = 70; break;
    }
    en.push_back(b);
    bossIdx = int(en.size()) - 1;
}

// ---- damage rules -------------------------------------------------------------------------
int Game::boss_damage(Enemy &e, int dmg, int cause) {
    auto spark = [&]() {
        for (int k = 0; k < 3 && pa.size() < 700; k++) {
            Particle p; p.x = e.a.x + (rnd() - 0.5f) * float(e.a.w); p.y = e.a.y - rnd() * float(e.a.h);
            p.vx = (rnd() - 0.5f) * 3; p.vy = -rnd() * 2; p.life = p.maxlife = 10; p.col = RGB(255, 240, 150); p.kind = 2;
            pa.push_back(p);
        }
        if (tick % 4 == 0) snd(SFX_HIT, e.a.x, e.a.y - 10, 0.8f);
    };
    switch (e.kind) {
    case 1: if (e.ph == 3) dmg *= 3; break;                       // stunned against the wall: triple damage
    case 2: if (e.ph != 3) { spark(); return 0; } break;          // shield up unless venting
    case 3:
        if (cause == C_CRUSH) return dmg;                         // falling ice
        if (cause == C_EXPLOSION) return std::max(1, dmg / 2);
        if (tick % 5 != 0) { spark(); return 0; }                 // bullets barely scratch the armour
        return 1;
    case 4:
        if (cause == C_EXPLOSION) return dmg * 3 / 2 + 1;         // only explosions work
        spark();
        return 0;
    default: break;
    }
    return dmg;
}

// ---- behaviour ----------------------------------------------------------------------------
void Game::boss_barrage(int n) {
    for (int k = 0; k < n; k++) {
        Strike s;
        s.x = float(rndi(14, W - 10) * TS);
        s.t = rndi(0, 110);
        s.left = 4;
        s.hostile = true;
        strikes.push_back(s);
    }
}

void Game::update_boss(size_t idx) {
    Enemy &e = en[idx];
    if (e.aux > 0) e.aux--;
    if (e.state == 0) { // sleeps until the bro walks into the arena
        if (pl.alive && pl.a.x > 36.0f * TS) {
            e.state = 1;
            e.ph = 0;
            e.timer = 0;
            bossBannerT = 170;
            snd(SFX_BOOM_SMALL, e.a.x, e.a.y, 1.0f);
        }
        return;
    }
    e.timer++;
    float dxp = pl.alive ? pl.a.x - e.a.x : 0.0f;
    if (pl.alive) e.a.face = dxp >= 0 ? 1 : -1;
    float minX = 6.0f * TS + e.a.w / 2, maxX = float(W - 6) * TS - e.a.w / 2;
    auto aimed_shot = [&](int kind, float speed, float spread) {
        float sx = e.a.x + float(e.a.face) * float(e.a.w) / 2, sy = e.a.y - float(e.a.h) * 0.65f;
        float ang = atan2f((pl.a.y - 8) - sy, pl.a.x - sx) + spread;
        if (kind == PK_EROCKET) {
            Proj rk; rk.kind = PK_EROCKET; rk.x = sx; rk.y = sy; rk.ang = ang; rk.vx = cosf(ang) * speed; rk.vy = sinf(ang) * speed; rk.life = 260;
            add_proj(rk);
            snd(SFX_ROCKET, e.a.x, e.a.y, 0.9f);
        } else {
            spawn_bullet(PK_EBULLET, sx, sy, ang, speed, 160, 1, false);
            snd(SFX_ESHOT, e.a.x, e.a.y, 0.8f);
        }
    };
    auto smash_ahead = [&](int dir, int dmg) { // ploughs through destructible scenery in front of the body
        int tx = int(floorf((e.a.x + float(dir) * (float(e.a.w) / 2 + 2)) / TS));
        for (int ty = int(floorf((e.a.y - float(e.a.h)) / TS)); ty <= int(floorf((e.a.y - 1) / TS)); ty++) damage_tile(tx, ty, dmg, true);
    };
    auto contact_kill = [&](float reachX) {
        if (pl.alive && pl.invuln == 0 && !pl.ride && fabsf(pl.a.x - e.a.x) < reachX + float(pl.a.w) / 2 && pl.a.y > e.a.y - float(e.a.h) && pl.a.y - float(pl.a.h) < e.a.y)
            kill_player(0, pl.a.x - e.a.x);
    };

    switch (e.kind) {
    case 0: { // GENERAL MORTARO: now a walking artillery mech. It keeps its distance, backs off when you close in, stops at shelters
        e.a.y = float(G * TS);
        if (e.ph == 0 || e.ph == 2) {
            float dir = 0;
            if (pl.alive) { if (fabsf(dxp) > 150) dir = dxp >= 0 ? 1.0f : -1.0f; else if (fabsf(dxp) < 85) dir = dxp >= 0 ? -1.0f : 1.0f; }
            if (e.ph == 2) dir = 0;                                            // planted while the mortars fire
            if (dir != 0) {
                float ax = e.a.x + dir * (float(e.a.w) / 2 + 3);
                bool metal = false;
                for (int yy = 2; yy <= int(e.a.h) - 2; yy += 6) if (tget(int(floorf(ax / TS)), int(floorf((e.a.y - float(yy)) / TS))) == T_METAL) metal = true;
                if (!metal) { smash_ahead(int(dir), 3); e.a.x = std::max(minX, std::min(maxX, e.a.x + dir * 0.4f)); e.dir = int(dir); e.aux2 = 1; }
                else e.aux2 = 0;
            } else e.aux2 = 0;
        }
        if (e.ph == 0) {
            if (pl.alive && e.timer % 75 == 40 && fabsf((pl.a.y - 8) - (e.a.y - 20)) < 40 && los(e.a.x, e.a.y - 16, pl.a.x, pl.a.y - 8))
                for (int k = -1; k <= 1; k++) aimed_shot(PK_EBULLET, 1.9f, float(k) * 0.12f);
            if (e.timer % 420 == 300 && en.size() < 26) { add_mook(e.a.x - 30, e.a.y, E_MOOK); add_mook(e.a.x - 18, e.a.y, E_RPG); }
            if (e.timer > 360) { e.ph = 1; e.timer = 0; bossWarnT = 100; snd(SFX_TICK, e.a.x, e.a.y, 1.0f); }
        } else if (e.ph == 1) {
            if (e.timer % 14 == 0) snd(SFX_TICK, e.a.x, e.a.y, 1.0f);
            if (e.timer >= 100) { e.ph = 2; e.timer = 0; boss_barrage(11); snd(SFX_ROCKET, e.a.x, e.a.y, 1.0f); }
        } else if (e.timer > 190) { e.ph = 0; e.timer = 0; }
        break;
    }
    case 1: { // IRON SCORPION
        e.a.y = float(G * TS);
        if (e.ph == 0) { // roll slowly towards the bro, shelling him
            float dir = dxp >= 0 ? 1.0f : -1.0f;
            if (fabsf(dxp) > 70) e.a.x += dir * 0.45f;
            if (pl.alive && e.timer % 130 == 60) aimed_shot(PK_EROCKET, 1.5f, 0.0f);
            if (e.timer > 380) { e.ph = 1; e.timer = 0; e.dir = dxp >= 0 ? 1 : -1; bossWarnT = 60; }
        } else if (e.ph == 1) { // wind-up: engine roars, dust
            e.a.x += (e.timer % 4 < 2 ? 0.6f : -0.6f);
            if (pl.alive) e.dir = dxp >= 0 ? 1 : -1;
            if (e.timer % 6 == 0 && pa.size() < 700) { Particle p; p.x = e.a.x - float(e.dir) * 20; p.y = e.a.y - 2; p.vx = -float(e.dir) * rnd(); p.vy = -rnd(); p.life = p.maxlife = 18; p.col = RGB(190, 170, 130); p.kind = 3; pa.push_back(p); }
            if (e.timer >= 60) { e.ph = 2; e.timer = 0; snd(SFX_BOOM_SMALL, e.a.x, e.a.y, 1.0f); }
        } else if (e.ph == 2) { // charge until something stops it
            e.a.vx = std::min(3.4f, std::fabs(e.a.vx) + 0.12f) * float(e.dir);
            float nx = e.a.x + e.a.vx;
            smash_ahead(e.dir, 8);
            e.a.x = nx;
            contact_kill(float(e.a.w) / 2);
            if (e.timer % 5 == 0) add_trauma(e.a.x, e.a.y, 0.12f);
            bool wall = e.a.x <= minX || e.a.x >= maxX;
            int tx = int(floorf((e.a.x + float(e.dir) * (float(e.a.w) / 2 + 3)) / TS));
            for (int ty = int(floorf((e.a.y - float(e.a.h)) / TS)); ty <= int(floorf((e.a.y - 1) / TS)); ty++) if (tget(tx, ty) == T_METAL) wall = true;
            if (wall) { // crash: stunned, vulnerable
                e.a.x = std::max(minX, std::min(maxX, e.a.x));
                e.ph = 3; e.timer = 0; e.a.vx = 0;
                trauma = 1.0f; rumble = 1.0f;
                debris(e.a.x + float(e.dir) * 22, e.a.y - 10, 24, RGB(120, 120, 130), 3.0f);
                snd(SFX_BOOM, e.a.x, e.a.y, 1.0f);
            }
        } else { // stunned
            if (e.timer % 8 == 0 && pa.size() < 700) { Particle p; p.x = e.a.x + float(rndi(-12, 12)); p.y = e.a.y - float(e.a.h); p.vx = (rnd() - 0.5f) * 0.6f; p.vy = -0.8f; p.life = p.maxlife = 30; p.col = RGB(80, 80, 85); p.kind = 3; pa.push_back(p); }
            if (e.timer > 200) { e.ph = 0; e.timer = 0; }
        }
        e.a.x = std::max(minX, std::min(maxX, e.a.x));
        if (e.ph != 2) contact_kill(float(e.a.w) / 2 - 4);
        break;
    }
    case 2: { // THE WARDEN: searchlight, lock, laser, vent
        if (e.ph == 0) { // the spot creeps towards the bro
            float tx = pl.alive ? pl.a.x : e.fx;
            e.fx += std::max(-1.15f, std::min(1.15f, tx - e.fx)) + sinf(float(e.timer) * 0.09f) * 0.5f;
            e.fx = std::max(float(6 * TS), std::min(float((W - 6) * TS), e.fx));
            e.a.x += (e.fx - e.a.x) * 0.02f;
            e.a.y += (24.0f * TS + sinf(float(tick) * 0.04f) * 4 - e.a.y) * 0.05f;
            if (e.timer > 330) { e.ph = 1; e.timer = 0; snd(SFX_TICK, e.a.x, e.a.y, 1.0f); }
        } else if (e.ph == 1) { // lock-on: the spot freezes, warning flashes
            if (e.timer % 10 == 0) snd(SFX_TICK, e.a.x, e.a.y, 0.9f);
            if (e.timer > 70) {
                e.ph = 2; e.timer = 0;
                e.fy = float(H * TS);
                for (float y = e.a.y; y < float(H * TS); y += 4) if (solid_px(e.fx, y)) { e.fy = y; break; }
                explode(e.fx, e.fy - 2, 18, 12, false, true);
                snd(SFX_LIGHTNING, e.fx, e.fy, 1.0f);
                trauma = std::min(1.0f, trauma + 0.5f);
                if (pl.alive && pl.invuln == 0 && fabsf(pl.a.x - e.fx) < 10) {
                    bool covered = false; // any solid roof between the drone and the bro's head blocks the beam
                    for (float y = e.a.y; y < pl.a.y - 16; y += 4) if (solid_px(pl.a.x, y)) { covered = true; break; }
                    if (!covered) kill_player(6, 0);
                }
            }
        } else if (e.ph == 2) { // beam is on
            if (e.timer > 22) { e.ph = 3; e.timer = 0; e.aux = 0; }
        } else { // vent: drops low, shield open
            e.a.y += (29.0f * TS - e.a.y) * 0.05f;
            e.a.x += (e.fx - e.a.x) * 0.02f;
            if (e.timer > 260) { e.ph = 0; e.timer = 0; }
        }
        break;
    }
    case 3: { // FROST COLOSSUS
        e.a.y = float(G * TS);
        if (e.ph == 2) { // dazed by falling ice
            if (e.timer > 70) { e.ph = 0; e.timer = 0; }
            break;
        }
        if (e.ph == 0) {
            float speed = 0.36f;
            e.a.x += float(e.dir) * speed;
            smash_ahead(e.dir, 30);
            if (e.a.x < 30.0f * TS) e.dir = 1;
            if (e.a.x > float(W - 26) * TS) e.dir = -1;
            if (e.timer > 150 + (int(e.a.x) % 40)) { e.ph = 1; e.timer = 0; }
        } else {
            if (e.timer == 24 && pl.alive) for (int k = -1; k <= 1; k++) aimed_shot(PK_EBULLET, 1.7f, float(k) * 0.2f);
            if (e.timer > 48) { e.ph = 0; e.timer = 0; if (rnd() < 0.4f) e.dir = -e.dir; }
        }
        contact_kill(float(e.a.w) / 2);
        break;
    }
    case 4: { // JUGGERNAUT
        e.a.y = float(G * TS);
        float dir = dxp >= 0 ? 1.0f : -1.0f;
        if (fabsf(dxp) > 70) { e.a.x += dir * 0.32f; smash_ahead(int(dir), 20); }
        e.a.x = std::max(minX, std::min(maxX, e.a.x));
        if (pl.alive && e.timer % 170 >= 60 && e.timer % 170 < 90 && e.timer % 6 == 0 && fabsf((pl.a.y - 8) - (e.a.y - 24)) < 60) aimed_shot(PK_EBULLET, 2.1f, (rnd() - 0.5f) * 0.1f);
        if (pl.alive && e.timer % 330 == 250) aimed_shot(PK_EROCKET, 1.6f, 0.0f);
        if (e.timer % 440 == 200) { // supply drop: gas bottles fall from the sky
            bossWarnT = 70;
            for (int k = 0; k < 2; k++) add_gas(float(rndi(24, W - 24) * TS + 4), camY - 16.0f - float(k * 20));
        }
        if (e.a.x < 20 * TS) e.a.x = 20.0f * TS;
        contact_kill(float(e.a.w) / 2 - 2);
        break;
    }
    default: { // TERRORKOPTER (the original helicopter), now calling airstrikes
        float tx = pl.alive ? pl.a.x + float(((tick / 240) % 2) ? 60 : -60) : e.a.x;
        e.a.vx += (tx > e.a.x ? 1 : -1) * 0.012f;
        e.a.vx = std::max(-1.1f, std::min(1.1f, e.a.vx));
        e.a.x += e.a.vx;
        e.a.y = 26.0f * TS + sinf(float(tick) * 0.03f) * 10;
        e.a.x = std::max(6.0f * TS, std::min(float(W - 6) * TS, e.a.x));
        e.a.face = e.a.vx >= 0 ? 1 : -1;
        if (pl.alive && e.timer % 80 == 0) {
            for (int k = -1; k <= 1; k++) {
                float ang = atan2f((pl.a.y - 8) - e.a.y, pl.a.x - e.a.x) + float(k) * 0.18f;
                spawn_bullet(PK_EBULLET, e.a.x, e.a.y + 4, ang, 1.7f, 160, 1, false);
            }
        }
        if (e.timer % 320 == 0 && en.size() < 40) { add_mook(e.a.x, e.a.y + 6, E_BOMBER); en.back().a.vy = 0; }
        if (e.timer % 760 == 640) { bossWarnT = 110; snd(SFX_TICK, e.a.x, e.a.y, 1.0f); }
        if (e.timer % 760 == 0) boss_barrage(10);
        break;
    }
    }
}

// ---- effects drawn in the world (beam, spotlight, markers) ---------------------------------
void Game::draw_boss_fx(Gfx &g, int ox, int oy) {
    if (bossIdx < 0) return;
    const Enemy &e = en[size_t(bossIdx)];
    if (e.state == 0) return;
    if (e.kind == 2) {
        int sx = int(e.a.x) - ox, sy = int(e.a.y) - oy;
        int fx = int(e.fx) - ox;
        // find where the spot lands
        int gy = int(e.fy) - oy;
        if (e.ph != 2) {
            float yy = float(H * TS);
            for (float y = e.a.y; y < float(H * TS); y += 4) if (solid_px(e.fx, y)) { yy = y; break; }
            gy = int(yy) - oy;
        }
        if (e.ph <= 1) { // cone of light + spot
            u32 col = e.ph == 1 && (tick / 4) % 2 ? RGB(255, 80, 70) : RGB(255, 250, 200);
            for (int k = 0; k < 14; k++) {
                int y = sy + 6 + k * (gy - sy - 6) / 14;
                int w = 2 + k * 6 / 14 + 2;
                int cx = sx + (fx - sx) * k / 14;
                g.rect_blend(cx - w, y, w * 2, std::max(1, (gy - sy - 6) / 14 + 1), col, e.ph == 1 ? 38 : 28);
            }
            g.rect_blend(fx - 14, gy - 3, 28, 4, col, 150);
            g.rect_blend(fx - 8, gy - 2, 16, 2, RGB(255, 255, 255), 160);
        } else if (e.ph == 2) { // the laser itself
            int w = 5 - e.timer / 8;
            g.rect_blend(fx - w - 2, sy, (w + 2) * 2, gy - sy, RGB(255, 60, 60), 120);
            g.rect(fx - w + 1, sy, std::max(2, (w - 1) * 2), gy - sy, RGB(255, 230, 230));
            g.rect_blend(fx - 16, gy - 8, 32, 10, RGB(255, 220, 160), 170);
        }
    }
    if (e.kind == 1 && e.ph == 1) { // charge warning arrow
        int sx = int(e.a.x) - ox, sy = int(e.a.y) - oy;
        if ((tick / 5) % 2) for (int k = 0; k < 3; k++) g.text_shadow(sx + e.dir * (34 + k * 9) - 3, sy - 30, e.dir > 0 ? ">" : "<", RGB(255, 80, 60), 2);
    }
}

// ---- boss sprites ---------------------------------------------------------------------------
static void scaled2(Gfx &g, const Sprite &s, int x, int y, bool flip, bool tint) {
    for (int j = 0; j < s.h; j++)
        for (int i = 0; i < s.w; i++) {
            u32 c = s.at(flip ? s.w - 1 - i : i, j);
            if (c) g.rect(x + i * 2, y + j * 2, 2, 2, tint ? RGB(255, 255, 255) : c);
        }
}

void Game::draw_boss(Gfx &g, const Enemy &e, int sx, int sy) {
    bool fl = e.flash > 0;
    int f = e.a.face >= 0 ? 1 : -1;
    switch (e.kind) {
    case 0: { // the general's artillery mech: steel hull, piston legs that really walk, a cockpit, twin mortar tubes
        u32 steel = fl ? RGB(255, 255, 255) : RGB(92, 102, 90), dk = fl ? RGB(230, 230, 230) : RGB(44, 50, 44), hi = fl ? RGB(255, 255, 255) : RGB(150, 164, 140);
        u32 rivet = RGB(30, 34, 30), yel = RGB(220, 180, 40);
        int ph = e.aux2 ? int(e.a.x / 5.0f) : 0;           // stride phase: only advances while it actually walks
        int stepA = (ph & 1) ? 3 : 0, stepB = (ph & 1) ? 0 : 3;
        // two piston legs with knees and wide feet
        for (int l = 0; l < 2; l++) {
            int lx = sx + (l ? 9 : -9), lift = l ? stepB : stepA;
            g.rect(lx - 6, sy - 3 - lift, 12, 3, dk);                                   // foot
            g.rect(lx - 6, sy - 3 - lift, 12, 1, hi);
            g.rect(lx - 2, sy - 12 - lift / 2, 4, 9, rivet);                             // lower strut
            g.rect(lx - 2, sy - 12 - lift / 2, 1, 9, steel);
            g.rect(lx - 4 + f * 2, sy - 15 - lift / 2, 8, 4, steel);                      // knee joint
            g.rect(lx - 3, sy - 22, 6, 8, dk);                                           // upper piston
            g.rect(lx - 3, sy - 22, 2, 8, steel);
        }
        // hull
        g.rect(sx - 16, sy - 34, 32, 14, dk);
        g.rect(sx - 15, sy - 33, 30, 12, steel);
        g.rect(sx - 15, sy - 33, 30, 2, hi);
        g.rect(sx - 15, sy - 23, 30, 2, dk);
        for (int k = 0; k < 4; k++) g.rect(sx - 15 + k * 8, sy - 22, 4, 2, k & 1 ? yel : rivet);   // hazard stripe
        for (int k = 0; k < 5; k++) { g.pset(sx - 13 + k * 6, sy - 31, rivet); g.pset(sx - 13 + k * 6, sy - 26, rivet); }
        g.rect(sx - 2, sy - 33, 1, 11, dk); g.rect(sx + 6, sy - 33, 1, 11, dk);        // panel seams
        // cockpit dome on the facing side, the general behind the glass
        int cx = sx + f * 5;
        g.rect(cx - 6, sy - 41, 12, 9, dk);
        g.rect(cx - 5, sy - 40, 10, 7, fl ? RGB(255, 255, 255) : RGB(110, 180, 210));
        g.rect(cx - 5, sy - 40, 10, 1, RGB(190, 230, 245));
        g.rect(cx - 2, sy - 38, 5, 4, RGB(225, 180, 135));                              // his face
        g.rect(cx - 3, sy - 40, 7, 2, RGB(70, 90, 50));                                 // peaked cap
        g.pset(cx - 1 + (f > 0 ? 1 : 0), sy - 37, RGB(20, 20, 20)); g.pset(cx + 1 + (f > 0 ? 1 : 0), sy - 37, RGB(20, 20, 20));
        // twin mortar tubes on the back, raised while it prepares the barrage
        int up = e.ph == 1 ? 8 + (tick / 3) % 3 : e.ph == 2 ? 10 : 0;
        for (int t = 0; t < 2; t++) {
            int tx = sx - f * (8 + t * 7);
            g.rect(tx - 2, sy - 40 - up, 5, 9 + up, fl ? RGB(255, 255, 255) : RGB(60, 66, 54));
            g.rect(tx - 3, sy - 41 - up, 7, 3, RGB(25, 28, 22));
            if (e.ph == 1 && (tick / 4) % 2) g.rect(tx - 1, sy - 45 - up, 3, 3, RGB(255, 200, 80));
            if (e.ph == 2 && e.timer < 8) g.rect(tx - 2, sy - 46 - up, 5, 5, RGB(255, 230, 140));   // muzzle flash as the shells leave
        }
        g.rect(sx + f * 12, sy - 30, 3, 5, dk);                                         // exhaust stack
        g.rect_blend(sx + f * 11, sy - 36 - (tick / 6) % 4, 5, 4, RGB(90, 90, 95), 120);
        break;
    }
    case 1: { // iron scorpion: tracked hull, turret, stinger tail
        u32 hull = fl ? RGB(255, 255, 255) : RGB(150, 118, 78), dark = fl ? RGB(230, 230, 230) : RGB(78, 62, 44), mid = RGB(110, 88, 58);
        g.rect(sx - 24, sy - 8, 48, 8, RGB(30, 30, 34));                       // tracks
        for (int k = 0; k < 7; k++) g.rect(sx - 22 + k * 7 + (tick / 3) % 3 * (e.ph == 2 ? 1 : 0), sy - 7, 4, 6, RGB(70, 70, 76));
        g.rect(sx - 22, sy - 17, 44, 10, hull);
        g.rect(sx - 22, sy - 17, 44, 2, RGB(190, 160, 110));
        g.rect(sx - 22, sy - 9, 44, 2, dark);
        g.rect(sx - 10, sy - 24, 22, 8, mid); g.rect(sx - 10, sy - 24, 22, 2, RGB(170, 140, 96)); // turret
        g.rect(sx + f * 12, sy - 21, f * 18, 4, dark);                           // cannon
        // stinger tail
        for (int k = 0; k < 4; k++) g.rect(sx - f * (20 + k * 3), sy - 19 - k * 3, 4, 4, hull);
        g.rect(sx - f * 31, sy - 33, 5, 5, RGB(210, 60, 50));
        if (e.ph == 3 && (tick / 5) % 2) g.text_shadow(sx - 8, sy - 38, "STUN", RGB(255, 240, 90));
        if (e.ph == 2) g.rect(sx - e.dir * 28, sy - 6, 6, 4, RGB(255, 190, 90));  // exhaust flare
        break;
    }
    case 2: { // warden: hovering sensor disc with a big lens and a searchlight dish
        bool open = e.ph == 3;
        u32 body = fl ? RGB(255, 255, 255) : RGB(86, 92, 108), dk = RGB(40, 44, 56);
        g.rect(sx - 15, sy - 14, 30, 10, dk);
        g.rect(sx - 14, sy - 13, 28, 8, body);
        g.rect(sx - 14, sy - 13, 28, 2, RGB(140, 148, 168));
        g.rect(sx - 6, sy - 18, 12, 5, body);
        g.rect(sx - 5, sy - 10, 10, 6, RGB(15, 15, 25));                          // lens
        u32 eye = e.ph == 1 ? ((tick / 4) % 2 ? RGB(255, 60, 50) : RGB(120, 20, 20)) : e.ph == 2 ? RGB(255, 255, 255) : open ? RGB(80, 255, 140) : RGB(255, 190, 60);
        g.rect(sx - 3, sy - 9, 6, 4, eye);
        g.rect(sx - 5, sy - 4, 10, 4, dk);                                         // searchlight
        if (open) { g.rect(sx - 20, sy - 12, 5, 3, RGB(255, 210, 80)); g.rect(sx + 15, sy - 12, 5, 3, RGB(255, 210, 80)); if ((tick / 3) % 2) g.rect(sx - 8, sy - 22, 16, 3, RGB(200, 220, 255)); }
        else { // energy shield bubble
            g.rect_blend(sx - 20, sy - 20, 40, 2, RGB(120, 200, 255), 70);
            g.rect_blend(sx - 22, sy - 18, 44, 18, RGB(120, 200, 255), 40);
            g.rect_blend(sx - 20, sy - 2, 40, 2, RGB(120, 200, 255), 70);
        }
        break;
    }
    case 3: { // frost colossus: a towering ice yeti in armour plates
        u32 fur = fl ? RGB(255, 255, 255) : RGB(214, 228, 240), sh = RGB(150, 175, 205), plate = RGB(96, 120, 150), dk = RGB(60, 80, 110);
        int bob = e.ph == 0 ? (tick / 14) % 2 : 0;
        g.rect(sx - 12, sy - 12, 9, 12, fur); g.rect(sx + 3, sy - 12, 9, 12, fur);        // legs
        g.rect(sx - 13, sy - 3, 11, 3, dk); g.rect(sx + 2, sy - 3, 11, 3, dk);
        g.rect(sx - 15, sy - 34 + bob, 30, 24, fur);                                        // torso
        g.rect(sx - 15, sy - 34 + bob, 30, 3, RGB(240, 248, 255));
        g.rect(sx + 8, sy - 34 + bob, 7, 24, sh);
        g.rect(sx - 13, sy - 26 + bob, 26, 8, plate); g.rect(sx - 13, sy - 26 + bob, 26, 2, RGB(140, 165, 195)); // armour plate
        g.rect(sx - 20, sy - 33 + bob, 8, 18, fur); g.rect(sx + 12, sy - 33 + bob, 8, 18, fur);  // arms
        g.rect(sx - 21, sy - 16 + bob, 10, 6, sh); g.rect(sx + 11, sy - 16 + bob, 10, 6, sh);    // fists
        g.rect(sx - 8, sy - 45 + bob, 16, 12, fur);                                         // head
        g.rect(sx - 8, sy - 45 + bob, 16, 2, RGB(240, 248, 255));
        g.rect(sx - 5, sy - 40 + bob, 4, 3, RGB(255, 80, 60)); g.rect(sx + 2, sy - 40 + bob, 4, 3, RGB(255, 80, 60));
        g.rect(sx - 4, sy - 36 + bob, 8, 2, dk);
        g.rect(sx - 10, sy - 48 + bob, 4, 5, RGB(190, 215, 235)); g.rect(sx + 6, sy - 48 + bob, 4, 5, RGB(190, 215, 235)); // ice horns
        if (e.ph == 2) { for (int k = 0; k < 3; k++) g.rect(sx - 8 + ((tick / 3 + k * 7) % 14), sy - 52 + k % 2 * 2, 2, 2, RGB(255, 240, 80)); } // dizzy stars
        break;
    }
    case 4: { // juggernaut: walking mech, bullet-proof plating, twin minigun arms
        u32 pl1 = fl ? RGB(255, 255, 255) : RGB(96, 100, 112), pl2 = RGB(60, 64, 76), hi = RGB(150, 156, 172);
        int leg = (tick / 12) % 2;
        g.rect(sx - 16, sy - 14, 11, 14 - leg * 2, pl2); g.rect(sx + 5, sy - 14, 11, 12 + leg * 2, pl2);
        g.rect(sx - 18, sy - 3 - leg * 2, 14, 3, RGB(30, 32, 40)); g.rect(sx + 4, sy - 1 + leg * 2 - 2, 14, 3, RGB(30, 32, 40));
        g.rect(sx - 17, sy - 30, 34, 18, pl1);
        g.rect(sx - 17, sy - 30, 34, 3, hi);
        for (int k = 0; k < 4; k++) g.rect(sx - 14 + k * 8, sy - 22, 4, 4, pl2);         // rivets
        g.rect(sx - 8, sy - 38, 16, 9, pl1); g.rect(sx - 8, sy - 38, 16, 2, hi);        // cockpit hump
        g.rect(sx - 6, sy - 35, 12, 4, RGB(255, 80, 60));                                 // slit window
        g.rect(sx + f * 15, sy - 28, f * 14, 5, RGB(30, 32, 40));                         // minigun
        g.rect(sx + f * 28 - (f < 0 ? 4 : 0), sy - 29, 4, 7, RGB(200, 200, 210));
        g.rect(sx - f * 22 - 3, sy - 32, 6, 14, pl2);                                     // rear exhaust / fuel
        if ((tick / 6) % 2) g.rect(sx - f * 22 - 2, sy - 36, 4, 4, RGB(255, 170, 60));
        if (e.flash > 0) g.rect_blend(sx - 18, sy - 40, 36, 40, RGB(255, 255, 255), 90);
        break;
    }
    default: draw_heli(g, sx, sy, true); break;
    }
    if (fl && e.kind != 0 && e.kind != 4) g.rect_blend(sx - e.a.w / 2, sy - e.a.h, e.a.w, e.a.h, RGB(255, 255, 255), 90);
}
