// Broyoo - gameplay: player, enemies, projectiles, particles, props
#include "game.h"
#include "audio.h"
#include "phrases.h"
#include <cmath>
#include <algorithm>
#include <cstring>

static const float GRAV = 0.19f;
static const float PI_F = 3.14159265f;

Game::Game() {
    build_sprites();
    gs = GS_TITLE;
    gen_level(0);
}

int Game::enemies_alive() const {
    int n = 0;
    for (const Enemy &e : en) if (e.alive && e.type != E_BOSS) n++;
    return n;
}

float Game::rumble_level() const {
    float v = rumble * float(st.vib) / 100.0f;
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

bool Game::in_view(float x, float y, float mx) const {
    return x > camX - mx && x < camX + VW + mx && y > camY - mx && y < camY + VH + mx;
}

// ---- movement ---------------------------------------------------------------------
void Game::move_actor(Actor &a, float dx, float dy, bool step, bool *hitx) {
    // X axis, in sub-steps of <= 1px
    int n = int(ceilf(fabsf(dx)));
    float sx = n ? dx / float(n) : 0;
    for (int i = 0; i < n; i++) {
        if (!box_hits(a.x + sx, a.y, a.w, a.h)) { a.x += sx; continue; }
        if (step && a.ground) {
            bool done = false;
            for (int up = 1; up <= TS; up++)
                if (!box_hits(a.x + sx, a.y - float(up), a.w, a.h) && !box_hits(a.x, a.y - float(up), a.w, a.h)) {
                    a.x += sx;
                    a.y -= float(up);
                    done = true;
                    break;
                }
            if (done) continue;
        }
        a.vx = 0;
        if (hitx) *hitx = true;
        break;
    }
    // Y axis
    n = int(ceilf(fabsf(dy)));
    float sy = n ? dy / float(n) : 0;
    a.ground = false;
    for (int i = 0; i < n; i++) {
        if (!box_hits(a.x, a.y + sy, a.w, a.h)) { a.y += sy; continue; }
        if (sy > 0) {
            a.impact = a.vy;
            a.ground = true;
            a.y = floorf((a.y + sy) / TS) * TS; // snap to the tile top
            if (box_hits(a.x, a.y, a.w, a.h)) a.y -= 0.01f;
        }
        a.vy = 0;
        break;
    }
    if (!a.ground && box_hits(a.x, a.y + 0.5f, a.w, a.h)) a.ground = true;
}

bool Game::los(float x0, float y0, float x1, float y1) const {
    float dx = x1 - x0, dy = y1 - y0;
    int n = int(sqrtf(dx * dx + dy * dy) / 3) + 1;
    for (int i = 1; i < n; i++) {
        float t = float(i) / float(n);
        if (solid_px(x0 + dx * t, y0 + dy * t)) return false;
    }
    return true;
}

int Game::nearest_enemy(float x, float y, float maxd, int exclude) const {
    int best = -1;
    float bd = maxd * maxd;
    for (size_t i = 0; i < en.size(); i++) {
        const Enemy &e = en[i];
        if (!e.alive || int(i) == exclude) continue;
        float dx = e.a.x - x, dy = e.a.y - float(e.a.h) / 2 - y;
        float d = dx * dx + dy * dy;
        if (d < bd && los(x, y, e.a.x, e.a.y - float(e.a.h) / 2)) { bd = d; best = int(i); }
    }
    return best;
}

// ---- level / player lifecycle -----------------------------------------------------
void Game::start_level(int lvl) {
    seedBase = seedBase * 1664525u + 1013904223u;
    gen_level(lvl);
    bossBannerT = bossWarnT = 0;
    introT = 1;
    introPhrase = rnd_ui(NUM_INTRO);
    ex = Exfil();
    killLog.clear();
    playTicks = 0;
    stat_kills = 0;
    tick = 0;
    trauma = 0;
    rumble = 0;
    flashScreen = 0;
    spawn_player(true);
    gs = GS_PLAY;
    bannerT = 150;
}

void Game::debug_start_level(int lvl, int bro) {
    start_level(lvl);
    introT = 0; // tests skip the black intro card (it has its own test)
    if (bro >= 0) swap_bro(bro);
}

void Game::debug_teleport(float x, float y) {
    pl.a.x = x;
    pl.a.y = y;
    camX = x - VW / 2.0f;
    camY = y - VH / 2.0f;
}

void Game::swap_bro(int b) {
    pl.bro = b;
    pl.spec = BROS[b].specAmmo;
    pl.cool = 0; pl.charge = 0; pl.spin = 0; pl.revAmmo = 6; pl.reload = 0;
    pl.swarm = 0; pl.lockLeft = 0; pl.cloak = 0; pl.dashT = 0; pl.grappleT = 0;
    pl.remote = false;
    bannerT = 120;
}

int Game::pick_owned(int avoid) {
    int list[NUM_BROS], n = 0;
    for (int b = 0; b < NUM_BROS; b++) if (owned(b) && b != avoid) list[n++] = b;
    if (n == 0) return avoid >= 0 ? avoid : 0;
    return list[rnd_ui(n)];
}

void Game::next_bro() { // the next bro in line takes over
    int nb;
    if (!queue.empty()) { nb = queue.front(); queue.erase(queue.begin()); }
    else nb = pick_owned(pl.bro);
    swap_bro(nb);
}

void Game::spawn_player(bool first) {
    Player old = pl;
    pl = Player();
    pl.lives = first ? 1 : old.lives; // every level starts with a single bro; freeing prisoners adds more
    if (pl.lives <= 0) pl.lives = 1;
    pl.a.x = checkX;
    pl.a.y = checkY;
    pl.a.w = BW;
    pl.a.h = BH;
    pl.alive = true;
    pl.invuln = 90;
    for (Enemy &e : en) if (e.type != E_BOSS) { e.alert = 0; e.saw = false; e.cd = std::max(e.cd, 60); e.burstT = 0; if (e.type == E_GUNNER) e.aux = 0; } // reappearing: nobody remembers you, nobody shoots at once
    if (first) { queue.clear(); swap_bro(pick_owned()); }
    else swap_bro(old.bro);
    // respawn: drop from above the checkpoint
    if (!first) pl.a.y = checkY - 60;
    camX = pl.a.x - VW / 2.0f;
    camY = pl.a.y - VH * 0.6f;
}

void Game::kill_player(int cause, float dirx) {
    if (!pl.alive) return;
    if (debugMode) { // debug mode: invincible; falling out of the world just puts you back on the last checkpoint
        if (cause == 3) { pl.a.x = checkX; pl.a.y = checkY - 20; pl.a.vx = pl.a.vy = 0; }
        return;
    }
    snd(SFX_PDEATH, pl.a.x, pl.a.y, 1.0f);
    if (rnd() < 0.5f) snd(SFX_GASP, pl.a.x, pl.a.y, 1.0f); // sometimes the bro gasps
    trigger_laughs();
    pl.alive = false;
    pl.deadT = 0;
    pl.remote = false;
    deathCause = cause;
    gore(pl.a.x, pl.a.y - 8, 80, dirx >= 0 ? 1.0f : -1.0f, RGB(200, 20, 20));
    gibs(pl.a.x, pl.a.y - 8, 6, dirx >= 0 ? 1.0f : -1.0f);
    debris(pl.a.x, pl.a.y - 8, 6, BROS[pl.bro].shirt, 2.0f);
    trauma = std::min(1.0f, trauma + 0.35f);
    rumble = std::max(rumble, 0.9f);
    pl.lives--;
    // remote rocket in flight goes with its owner
    for (Proj &p : pr) if (p.kind == PK_REMOTE) p.life = 0;
}

void Game::level_clear() {
    sounds.push_back({SFX_FANFARE, 1.0f});
    gs = GS_CLEAR;
    stateT = 0;
    menuSel = 0;
    settings_dirty = true;
}

// ---- projectiles -------------------------------------------------------------------
void Game::add_proj(const Proj &p) { if (pr.size() < 220) pr.push_back(p); }

void Game::spawn_bullet(int kind, float x, float y, float ang, float speed, int life, int dmg, bool friendly) {
    (void)friendly;
    Proj p;
    p.kind = kind; p.x = x; p.y = y; p.ang = ang;
    p.vx = cosf(ang) * speed; p.vy = sinf(ang) * speed;
    p.life = life; p.dmg = dmg;
    if (kind == PK_REVBULLET) p.pierce = 1;
    add_proj(p);
}

void Game::do_melee(float reach, int dmg, bool dig) {
    float fx = pl.a.x + float(pl.a.face) * reach / 2, fy = pl.a.y - 8;
    for (size_t i = 0; i < en.size(); i++) {
        Enemy &e = en[i];
        if (!e.alive) continue;
        float dx = fabsf(e.a.x - fx), dy = fabsf(e.a.y - float(e.a.h) / 2 - fy);
        if (dx < reach / 2 + float(e.a.w) / 2 && dy < 8 + float(e.a.h) / 2) {
            if (e.type == E_HEAVY) { e.stun = 30; e.a.vx = float(pl.a.face) * 1.5f; }
            hurt_enemy(int(i), dmg, float(pl.a.face), C_MELEE);
        }
    }
    for (Prop &c : props)
        if (c.type == PR_CAGE && !c.used && fabsf(c.a.x - fx) < reach / 2 + 8 && fabsf(c.a.y - 6 - fy) < 14) open_cage(c);
    if (dig) {
        for (int k = 0; k < 2; k++) {
            int tx = int(floorf((pl.a.x + float(pl.a.face) * float(4 + k * 7)) / TS));
            for (int ty = int(floorf((pl.a.y - 13) / TS)); ty <= int(floorf((pl.a.y - 1) / TS)); ty++) damage_tile(tx, ty, 2, true);
        }
    }
    // swing visual
    for (int i = 0; i < 7; i++) {
        Particle p;
        p.x = pl.a.x + float(pl.a.face) * float(5 + i * reach / 8); p.y = pl.a.y - 13 + float(i % 3) * 4;
        p.vx = 0; p.vy = 0; p.life = p.maxlife = 6; p.col = RGB(240, 240, 250); p.kind = 2;
        pa.push_back(p);
    }
}

// Shots go straight: horizontal, or straight up / straight down (in the air). Only the shotgun fires a cone.
static float aim_angle(const Input &in, int face, bool air) {
    if (in.up) return -PI_F / 2;
    if (in.down && air) return PI_F / 2;
    return face > 0 ? 0.0f : PI_F;
}

// big-calibre guns (revolver, assault rifle, machine gun, Mauser) use the heavy shot sound; the laser burst has its own zap; small automatics keep the light one
static int gun_sfx(const BroDef &b) {
    switch (b.prim) {
    case P_REVOLVER: return SFX_POWERSHOT;
    case P_MG: return (strcmp(b.primName, "MACHINE GUN") == 0 || strcmp(b.primName, "MAUSER") == 0) ? SFX_POWERSHOT : SFX_SHOT;
    case P_BURST: return strcmp(b.primName, "PULSE RIFLE") == 0 ? SFX_POWERSHOT : strcmp(b.primName, "LASER BURST") == 0 ? SFX_LASER : SFX_SHOT;
    default: return SFX_SHOT;
    }
}

void Game::fire_primary(const Input &in) {
    Player &p = pl;
    const BroDef &b = BROS[p.bro];
    bool air = !p.a.ground;
    float ang = aim_angle(in, p.a.face, air);
    float mx = p.a.x + cosf(ang) * 9, my = p.a.y - (p.crouch ? 5.0f : 9.0f) + sinf(ang) * 9;
    auto spread = [&](float s) { return (rnd() - 0.5f) * 2 * s; };
    // spawn parallel to the aim axis, `off` pixels to the side (never diagonal)
    auto shot = [&](int kind, float off, float speed, int life, int dmg) {
        spawn_bullet(kind, mx - sinf(ang) * off, my + cosf(ang) * off, ang, speed, life, dmg, true);
    };
    bool held = in.fire;
    bool fireEdge = held && !prev.fire;
    if (b.prim == P_CHARGE) {
        if (held) { if (p.charge < 45) p.charge++; return; }
        if (p.charge <= 0) return;
        int c = p.charge;
        p.charge = 0;
        int n = c < 10 ? 1 : 1 + c / 10;
        for (int i = 0; i < n; i++) shot(PK_BULLET, (float(i) - float(n - 1) / 2) * 4, 5.0f, 45, c >= 40 ? 2 : 1);
        p.cool = 16;
        snd(SFX_SHOT, p.a.x, p.a.y);
        trauma = std::min(1.0f, trauma + 0.03f + float(n) * 0.01f);
        return;
    }
    if (b.prim == P_MINIGUN) {
        if (held) { if (p.spin == 0) snd(SFX_SPINUP, p.a.x, p.a.y, 1.0f); if (p.spin < 40) p.spin++; } else { p.spin = std::max(0, p.spin - 2); return; }
        if (p.spin < 25 || p.cool > 0) return;
        shot(PK_BULLET, float(rndi(-2, 2)), 5.5f, 45, 1);
        p.cool = 3;
        if (tick % 2 == 0) snd(SFX_SHOT, p.a.x, p.a.y, 1.0f); // light shot every other frame, a bit louder to make up for the stacking
        trauma = std::min(1.0f, trauma + 0.012f);
        return;
    }
    if (!held || p.cool > 0) return;
    switch (b.prim) {
    case P_MG:
        shot(PK_BULLET, float(rndi(-1, 1)), 5.0f, 50, gun_sfx(b) == SFX_POWERSHOT ? 2 : 1); // big calibre: one round kills a soldier
        p.cool = 7; trauma = std::min(1.0f, trauma + 0.012f);
        snd(gun_sfx(b), p.a.x, p.a.y);
        break;
    case P_BURST:
        if (p.burst <= 0) p.burst = 3;
        shot(PK_BULLET, 0, 5.5f, 50, gun_sfx(b) == SFX_POWERSHOT ? 2 : 1);
        snd(gun_sfx(b), p.a.x, p.a.y);
        trauma = std::min(1.0f, trauma + 0.012f);
        if (--p.burst <= 0) p.cool = 24; else p.cool = 3;
        break;
    case P_ROCKET: {
        Proj r; r.kind = PK_ROCKET; r.x = mx; r.y = my; r.ang = ang; r.vx = cosf(ang) * 2.2f; r.vy = sinf(ang) * 2.2f; r.life = 100;
        add_proj(r);
        p.cool = 30; trauma = std::min(1.0f, trauma + 0.06f);
        snd(SFX_ROCKET, p.a.x, p.a.y);
        break;
    }
    case P_FLAME: {
        Proj f; f.kind = PK_FLAME; f.x = mx; f.y = my; f.life = 22; f.dmg = 1; f.pierce = 99;
        float sp = spread(0.3f);
        f.vx = cosf(ang) * 2.2f - sinf(ang) * sp; f.vy = sinf(ang) * 2.2f + cosf(ang) * sp;
        add_proj(f);
        p.cool = 2;
        flameT = 3; // the front-end loops the flame sound while this stays on
        break;
    }
    case P_SHOTGUN:
        for (int i = 0; i < 6; i++) spawn_bullet(PK_PELLET, mx, my, ang + spread(0.26f), 4.0f + rnd() * 1.2f, 16, 1, true);
        p.cool = 36; trauma = std::min(1.0f, trauma + 0.1f);
        snd(SFX_SHOTGUN, p.a.x, p.a.y);
        break;
    case P_DUAL:
        if (strcmp(b.primName, "SILENCED PISTOL") == 0) { // one silenced round at a time
            shot(PK_BULLET, 0, 5.5f, 50, 1);
            p.cool = 11; trauma = std::min(1.0f, trauma + 0.004f);
            snd(SFX_SILENCED, p.a.x, p.a.y, 0.8f);
            break;
        }
        shot(PK_BULLET, -3, 5.0f, 50, 1);
        shot(PK_BULLET, 3, 5.0f, 50, 1);
        p.cool = 8; trauma = std::min(1.0f, trauma + 0.012f);
        snd(SFX_SHOT, p.a.x, p.a.y);
        break;
    case P_DYNAMITE: {
        if (!fireEdge) return;
        Proj d; d.kind = PK_DYNA; d.x = p.a.x + float(p.a.face) * 3; d.y = p.a.y - 10; d.vx = float(p.a.face) * 2.4f + p.a.vx * 0.5f; d.vy = -2.1f; d.life = 70;
        add_proj(d);
        p.cool = 28;
        snd(SFX_THROW, p.a.x, p.a.y);
        break;
    }
    case P_KATANA: {
        if (!fireEdge && p.meleeT > 0) return;
        bool great = strcmp(b.primName, "GREAT SWORD") == 0, claymore = strcmp(b.primName, "CLAYMORE") == 0;
        do_melee(great ? 30.0f : claymore ? 26.0f : 22.0f, great ? 5 : 3, true); // the great sword: huge reach, heavy blows, slower swing
        p.meleeT = 10; p.cool = great ? 24 : 16;
        snd(SFX_SLASH, p.a.x, p.a.y);
        break;
    }
    case P_FIST:
        do_melee(17, 2, true);
        p.meleeT = 6; p.punchT = 8; p.punchMax = 8; p.cool = 9;
        snd(SFX_SLASH, p.a.x, p.a.y, 0.6f);
        break;
    case P_HOMING:
        shot(PK_HOMING, 0, 3.4f, 80, 1);
        p.cool = 14;
        snd(SFX_SHOT, p.a.x, p.a.y);
        break;
    case P_RAY:
        shot(PK_RAY, 0, 4.0f, 70, 1);
        p.cool = 26; trauma = std::min(1.0f, trauma + 0.05f);
        snd(SFX_RAY, p.a.x, p.a.y);
        break;
    case P_SILENT:
        shot(PK_BULLET, float(rndi(-1, 1)), 6.0f, 45, 1);
        p.cool = 4;
        snd(SFX_SILENCED, p.a.x, p.a.y, 0.8f);
        break;
    case P_REVOLVER:
        if (p.reload > 0) return;
        if (!fireEdge) return;
        shot(PK_REVBULLET, 0, 6.5f, 60, 3);
        p.cool = 12;
        p.revAmmo--;
        snd(gun_sfx(b), p.a.x, p.a.y);
        trauma = std::min(1.0f, trauma + 0.05f);
        if (p.revAmmo <= 0) { p.reload = 50; p.revAmmo = 6; snd(SFX_RELOAD, p.a.x, p.a.y, 0.9f); }
        break;
    case P_WHIP:
        if (!fireEdge) return;
        do_melee(54, 2, true);
        p.whipT = 12; p.whipLen = 54; p.cool = 20; p.meleeT = 10;
        snd(SFX_SLASH, p.a.x, p.a.y);
        break;
    case P_BOOMERANG: case P_CHAKRAM: {
        if (!fireEdge) return;
        for (const Proj &q : pr) if (q.kind == PK_BOOMERANG && q.life > 0) return; // one machete at a time
        Proj m; m.kind = PK_BOOMERANG; m.x = p.a.x + float(p.a.face) * 6; m.y = p.a.y - 9; m.vx = float(p.a.face) * 3.6f; m.vy = 0;
        m.life = 240; m.dmg = 2; m.target = 0;
        add_proj(m);
        p.cool = 10;
        snd(SFX_THROW, p.a.x, p.a.y);
        break;
    }
    case P_CHAINSAW: {
        do_melee(15, 1, true);
        p.cool = 4; p.meleeT = 4;
        sawT = 6; // the front-end loops the engine sound while this stays on
        if (pa.size() < 700) {
            Particle q; q.x = p.a.x + float(p.a.face) * 12; q.y = p.a.y - 9 + float(rndi(-3, 3)); q.vx = float(p.a.face) * (0.5f + rnd()); q.vy = -rnd() * 1.2f;
            q.life = q.maxlife = 10; q.col = RGB(255, 220, 120); q.kind = 2; pa.push_back(q);
        }
        trauma = std::min(1.0f, trauma + 0.01f);
        break;
    }
    case P_CRYO: { // freezes soldiers solid: they stand helpless until any hit shatters them
        shot(PK_CRYO, 0, 4.6f, 70, 0);
        p.cool = 16;
        snd(SFX_RAY, p.a.x, p.a.y, 0.8f);
        break;
    }
    case P_SPEAR: {
        if (!fireEdge) return;
        bool stake = strcmp(b.primName, "WOODEN STAKES") == 0; // Broffy: quick, light stakes
        Proj sp; sp.kind = PK_SPEAR; sp.x = mx; sp.y = my; sp.ang = ang; sp.vx = cosf(ang) * (stake ? 6.0f : 5.2f); sp.vy = sinf(ang) * (stake ? 6.0f : 5.2f);
        sp.life = stake ? 42 : 50; sp.dmg = stake ? 2 : 3; sp.pierce = stake ? 1 : 3;
        add_proj(sp);
        p.cool = stake ? 11 : 22;
        snd(SFX_THROW, p.a.x, p.a.y);
        break;
    }
    default: break;
    }
}

void Game::use_special() {
    Player &p = pl;
    const BroDef &b = BROS[p.bro];
    if (p.spec <= 0) return;
    float fx = float(p.a.face);
    auto grenade = [&](int kind) {
        Proj g; g.kind = kind; g.x = p.a.x + fx * 3; g.y = p.a.y - 10; g.vx = fx * 2.4f + p.a.vx * 0.5f; g.vy = -2.6f;
        g.life = kind == PK_TURKEY ? 100 : kind == PK_MOLOTOV ? 400 : 80;
        add_proj(g);
    };
    switch (b.spec) {
    case S_GRENADE: grenade(PK_GRENADE); break;
    case S_FIREBOMB: grenade(PK_FIREBOMB); break;
    case S_TURKEY: grenade(PK_TURKEY); break;
    case S_MOLOTOV: grenade(PK_MOLOTOV); break;
    case S_SWARM: p.swarm = 5; p.swarmT = 0; break;
    case S_AIRSTRIKE: strikes.push_back({p.a.x + fx * 70, 45, 6}); break;
    case S_FLASH:
        flashScreen = 14;
        for (Enemy &e : en)
            if (e.alive && in_view(e.a.x, e.a.y, 0) && e.type != E_BOSS) e.stun = 200;
        break;
    case S_DASH: p.dashT = 14; p.invuln = std::max(p.invuln, 16); p.a.vx = fx * 4.6f; break;
    case S_REMOTE: {
        Proj r; r.kind = PK_REMOTE; r.x = p.a.x + fx * 6; r.y = p.a.y - 9; r.ang = fx > 0 ? 0.0f : PI_F; r.vx = cosf(r.ang) * 2.2f; r.vy = sinf(r.ang) * 2.2f; r.life = 300;
        add_proj(r);
        p.remote = true;
        break;
    }
    case S_CLOAK: p.cloak = 360; break;
    case S_WARCRY: // a roar that stuns every soldier on screen
        for (Enemy &e : en) if (e.alive && in_view(e.a.x, e.a.y, 0) && e.type != E_BOSS && e.type != E_OFFICER) e.stun = 220;
        p.invuln = std::max(p.invuln, 90);
        trauma = std::min(1.0f, trauma + 0.5f);
        snd(SFX_SCREAM, p.a.x, p.a.y, 1.0f);
        break;
    case S_SPEARS: { // vanish, then three piercing spears in a tight fan
        p.cloak = 240;
        float base = p.a.face > 0 ? 0.0f : PI_F;
        for (int i = -1; i <= 1; i++) {
            float ang = base + float(i) * 0.12f;
            Proj sp; sp.kind = PK_SPEAR; sp.x = p.a.x + fx * 8; sp.y = p.a.y - 9; sp.ang = ang;
            sp.vx = cosf(ang) * 5.4f; sp.vy = sinf(ang) * 5.4f; sp.life = 55; sp.dmg = 3; sp.pierce = 3;
            add_proj(sp);
        }
        snd(SFX_THROW, p.a.x, p.a.y);
        break;
    }
    case S_INVINC: p.invuln = 300; break;
    case S_LOCKON: p.lockLeft = 5; p.lockT = 0; break;
    case S_BLAST: { // boomstick: one huge shotgun blast
        float ang = p.a.face > 0 ? 0.0f : PI_F;
        for (int i = 0; i < 16; i++) spawn_bullet(PK_PELLET, p.a.x + fx * 8, p.a.y - 9, ang + (rnd() - 0.5f) * 1.0f, 4.0f + rnd() * 1.5f, 15, 2, true);
        snd(SFX_SHOTGUN, p.a.x, p.a.y);
        trauma = std::min(1.0f, trauma + 0.3f);
        break;
    }
    case S_TELEPORT: { // blink forward through walls to the first free spot
        float nx = p.a.x;
        bool moved = false;
        for (float dd = 110; dd >= 16; dd -= 4) {
            float cx = p.a.x + fx * dd;
            if (cx > 8 && cx < float(W * TS) - 8 && !box_hits(cx, p.a.y, p.a.w, p.a.h)) { nx = cx; moved = true; break; }
        }
        if (!moved) return;
        for (int k = 0; k < 14 && pa.size() < 700; k++) {
            Particle q; q.x = (k % 2 ? p.a.x : nx) + (rnd() - 0.5f) * 8; q.y = p.a.y - 2 - rnd() * 12; q.vx = (rnd() - 0.5f) * 1.5f; q.vy = -rnd() * 1.5f;
            q.life = q.maxlife = 16; q.col = rnd() < 0.5f ? RGB(120, 220, 255) : RGB(255, 255, 255); q.kind = 2; pa.push_back(q);
        }
        p.a.x = nx; p.a.vx = 0; p.invuln = std::max(p.invuln, 20);
        break;
    }
    case S_LIGHTNING:
        flashScreen = 8;
        snd(SFX_LIGHTNING, p.a.x, p.a.y);
        for (size_t i = 0; i < en.size(); i++) {
            Enemy &e = en[i];
            if (!e.alive || !in_view(e.a.x, e.a.y, 0)) continue;
            for (float y = camY; y < e.a.y - 4 && pa.size() < 700; y += 3) {
                Particle q; q.x = e.a.x + float(rndi(-2, 2)); q.y = y; q.vx = 0; q.vy = 0; q.life = q.maxlife = 8;
                q.col = (int(y) / 3) % 2 ? RGB(255, 255, 255) : RGB(150, 190, 255); q.kind = 2; pa.push_back(q);
            }
            hurt_enemy(int(i), 3, 0, C_LIGHTNING);
        }
        trauma = std::min(1.0f, trauma + 0.4f);
        break;
    case S_ADRENALINE: p.adren = 300; break;
    case S_JETPACK: p.jet = 150; break;
    case S_GRAPPLE: {
        float dx = fx * 0.7f, dy = -0.7f, x = p.a.x, y = p.a.y - 10;
        bool hit = false;
        for (int i = 0; i < 40; i++) {
            x += dx * 2; y += dy * 2;
            if (solid_px(x, y)) { hit = true; break; }
        }
        if (!hit) return;
        p.gx = x; p.gy = y; p.grappleT = 40;
        break;
    }
    }
    snd(SFX_SPECIAL, p.a.x, p.a.y, 0.8f);
    p.spec--;
}

void Game::hurt_enemy(int i, int dmg, float dirx, int cause) {
    Enemy &e = en[size_t(i)];
    if (!e.alive) return;
    if (e.frozen && e.type != E_BOSS) { e.frozen = false; dmg = 99; debris(e.a.x, e.a.y - 8, 14, RGB(190, 235, 255), 2.2f); snd(SFX_CRASH, e.a.x, e.a.y, 0.6f); } // an ice statue shatters
    if (e.type == E_BOSS) { dmg = boss_damage(e, dmg, cause); if (dmg <= 0) return; }
    e.hp -= dmg;
    e.flash = 4;
    if (tick % 3 == 0) snd(SFX_HIT, e.a.x, e.a.y, 0.8f);
    if (e.type == E_BOSS) {
        debris(e.a.x + (rnd() - 0.5f) * 30, e.a.y - 8, 2, RGB(90, 95, 105), 2.0f);
        if (e.hp <= 0) kill_enemy(i, dirx, cause);
        return;
    }
    if (e.hp <= 0) kill_enemy(i, dirx, cause);
    else gore(e.a.x, e.a.y - 8, 9, dirx, RGB(200, 20, 20));
}

void Game::kill_enemy(int i, float dirx, int cause) {
    Enemy &e = en[size_t(i)];
    if (!e.alive) return;
    e.alive = false;
    stat_kills++;
    snd(e.type == E_DOG ? SFX_BARK : SFX_DEATH, e.a.x, e.a.y);
    // a kamikaze only goes off when his fuse runs out. Killed with the fuse lit, he still blows up when it ends;
    // killed by a blast / gas / fire he catches alight (standard fuse); a bullet sometimes hits the explosives (instant blast);
    // crushed: instant blast; lightning lights the fuse; killed any other way (blades, spikes, ...) with no fuse lit, he is just dead.
    int bomberFuse = 0;
    if (e.type == E_BOMBER) {
        if (e.aux < 0) bomberFuse = 0;                         // the fuse just ran out: handled below, right now
        else if (e.aux > 0) bomberFuse = e.aux;
        else if (cause == C_EXPLOSION || cause == C_GAS || cause == C_FIRE || cause == C_LIGHTNING) { bomberFuse = 188; snd(SFX_IGNITE, e.a.x, e.a.y, 0.9f); } // lit: the standard fuse
        else if (cause == C_CRUSH) e.aux = -1;                       // crushed: the explosives go off at once
        else if (cause == C_BULLET && rnd() < 0.35f) e.aux = -1;   // the bullet hit the explosives: instant blast
        cause = C_EXPLOSION;
    }
    if (e.type != E_BOSS && killLog.size() < 400) killLog.push_back({uint8_t(e.type), uint8_t(cause), int8_t(dirx >= 0 ? 1 : -1)});
    if (e.type == E_OFFICER) start_exfil(e.a.x, e.a.y);
    if (e.type == E_BOSS) {
        bossDead = true;
        start_exfil(std::max(20.0f * TS, std::min(float(W - 12) * TS, pl.a.x + 50)), 40.0f * TS);
        for (int k = 0; k < 6; k++)
            pend.push_back({e.a.x + float(rndi(-20, 20)), e.a.y + float(rndi(-8, 8)), 28.0f, 14.0f, true});
        pend.push_back({e.a.x, e.a.y, 44.0f, 16.0f, true});
        trauma = 1.0f;
        rumble = 1.0f;
        return;
    }
    int n = (e.type == E_HEAVY || e.type == E_GUNNER) ? 90 : e.type == E_DOG ? 28 : 50;
    gore(e.a.x, e.a.y - 8, n, dirx >= 0 ? 1.0f : -1.0f, RGB(200, 20, 20));
    gibs(e.a.x, e.a.y - 8, (e.type == E_HEAVY || e.type == E_GUNNER) ? 8 : e.type == E_DOG ? 3 : 4, dirx >= 0 ? 1.0f : -1.0f);
    if (e.type == E_BOMBER && (e.aux < 0 || bomberFuse > 0)) pend.push_back({e.a.x, e.a.y - 7, 24.0f, 12.0f, false, e.aux < 0 ? 0 : bomberFuse});
}

void Game::open_cage(Prop &c) {
    if (c.used) return;
    c.used = true;
    snd(SFX_CAGE, c.a.x, c.a.y);
    debris(c.a.x, c.a.y - 6, 10, RGB(120, 125, 140), 2.0f);
    add_trauma(c.a.x, c.a.y, 0.15f);
    Prop f;
    f.type = PR_FREED;
    f.a.x = c.a.x;
    f.a.y = c.a.y;
    f.a.w = BW;
    f.a.h = BH;
    f.a.vy = -2.5f;
    f.bro = c.bro;
    props.push_back(f);
}

// ---- player -----------------------------------------------------------------------
void Game::update_player(const Input &in) {
    Player &p = pl;
    const BroDef &b = BROS[p.bro];
    p.anim++;
    if (p.cool > 0) p.cool--;
    if (p.adren > 0) { p.adren--; if (p.cool > 0) p.cool--; }
    if (p.invuln > 0) p.invuln--;
    if (p.cloak > 0) p.cloak--;
    if (p.meleeT > 0) p.meleeT--;
    if (p.punchT > 0) p.punchT--;
    if (toastT > 0) toastT--;
    if (flameT > 0) flameT--; if (sawT > 0) sawT--;
    if (p.whipT > 0) p.whipT--;
    if (p.reload > 0) p.reload--;
    if (p.ride) { p.a.vx = 0; p.a.vy = 0; p.a.ground = false; return; } // hanging from the helicopter ladder

    // guided rocket: the bro stands still while steering it
    if (p.remote) {
        p.a.vx = 0;
        move_actor(p.a, 0, 0.0f, false);
        bool any = false;
        for (Proj &r : pr)
            if (r.kind == PK_REMOTE) {
                any = true;
                float ddx = float((in.right ? 1 : 0) - (in.left ? 1 : 0)), ddy = float((in.down ? 1 : 0) - (in.up ? 1 : 0));
                if (ddx != 0 || ddy != 0) {
                    float d = atan2f(ddy, ddx) - r.ang;
                    while (d > PI_F) d -= 2 * PI_F;
                    while (d < -PI_F) d += 2 * PI_F;
                    r.ang += std::max(-0.08f, std::min(0.08f, d));
                }
                r.vx = cosf(r.ang) * 2.4f; r.vy = sinf(r.ang) * 2.4f;
            }
        if (!any) p.remote = false;
        return;
    }

    // timed special sequences
    if (p.swarm > 0 && --p.swarmT <= 0) {
        p.swarmT = 6;
        p.swarm--;
        Proj r; r.kind = PK_ROCKET; r.x = p.a.x + float(p.a.face) * 6; r.y = p.a.y - 9 + float(rndi(-3, 3)); r.ang = p.a.face > 0 ? 0.0f : PI_F;
        r.vx = cosf(r.ang) * 2.4f; r.vy = sinf(r.ang) * 2.4f; r.life = 100;
        add_proj(r);
    }
    if (p.lockLeft > 0 && --p.lockT <= 0) {
        p.lockT = 5;
        int t = nearest_enemy(p.a.x, p.a.y - 8, 190);
        if (t >= 0) {
            Proj h; h.kind = PK_HOMING; h.x = p.a.x; h.y = p.a.y - 10; h.life = 90; h.dmg = 2; h.target = t;
            float ang = -PI_F / 2 + (rnd() - 0.5f) * 1.6f;
            h.vx = cosf(ang) * 3.4f; h.vy = sinf(ang) * 3.4f; h.ang = ang;
            add_proj(h);
        }
        p.lockLeft--;
    }

    // crouching: hold down on the ground (can't stand up under a low ceiling)
    if (in.down && p.a.ground && p.dashT == 0) p.crouch = true;
    else if (p.crouch && (!in.down || !p.a.ground) && !box_hits(p.a.x, p.a.y - 1, p.a.w, BH - 1)) p.crouch = false;
    // horizontal control
    float maxv = p.adren > 0 ? 2.0f : 1.35f;
    if (p.crouch) maxv *= 0.45f;
    if (p.wired) maxv *= 0.5f;
    if (b.prim == P_MINIGUN && in.fire) maxv = 0.55f;
    float acc = p.a.ground ? (world == 3 ? 0.07f : 0.22f) : 0.12f; // the arctic world is icy
    int dir = (in.right ? 1 : 0) - (in.left ? 1 : 0);
    if (p.dashT > 0) {
        p.dashT--;
        p.a.vx = float(p.a.face) * 4.6f;
        p.a.vy = std::min(p.a.vy, 0.3f);
        do_melee(10, 5, true);
    } else if (p.grappleT > 0) {
        float dx = p.gx - p.a.x, dy = p.gy - (p.a.y - 10);
        float d = sqrtf(dx * dx + dy * dy);
        p.grappleT--;
        if (d < 6 || in.jump) p.grappleT = 0;
        else {
            p.a.vx += dx / d * 0.35f;
            p.a.vy += dy / d * 0.35f - GRAV;
            p.a.vx = std::max(-3.2f, std::min(3.2f, p.a.vx));
            p.a.vy = std::max(-3.2f, std::min(3.2f, p.a.vy));
        }
    } else if (p.knockT > 0) {
        p.knockT--; // thrown by a blast: no steering until he lands or the moment passes
        p.a.vx *= 0.985f;
    } else if (dir != 0) {
        p.a.face = dir;
        p.a.vx += float(dir) * acc;
        if (p.a.vx > maxv) p.a.vx = std::max(maxv, p.a.vx - 0.15f);
        if (p.a.vx < -maxv) p.a.vx = std::min(-maxv, p.a.vx + 0.15f);
    } else {
        float fr = p.a.ground ? (world == 3 ? 0.025f : 0.2f) : 0.03f;
        if (p.a.vx > 0) p.a.vx = std::max(0.0f, p.a.vx - fr);
        else p.a.vx = std::min(0.0f, p.a.vx + fr);
    }

    // jumping (coyote time, buffer, wall jump)
    if (p.a.ground) p.coyote = 6; else if (p.coyote > 0) p.coyote--;
    if (!prev.jump && in.jump) p.jumpBuf = 7; else if (p.jumpBuf > 0) p.jumpBuf--;
    bool touchL = box_hits(p.a.x - 1, p.a.y - 2, p.a.w, 6), touchR = box_hits(p.a.x + 1, p.a.y - 2, p.a.w, 6);
    // wall contact: probe one pixel out on each side
    touchL = box_hits(p.a.x - 1.2f, p.a.y - 3, p.a.w, 8) && !box_hits(p.a.x, p.a.y - 3, p.a.w, 8);
    touchR = box_hits(p.a.x + 1.2f, p.a.y - 3, p.a.w, 8) && !box_hits(p.a.x, p.a.y - 3, p.a.w, 8);
    p.wall = (!p.a.ground && ((touchL && in.left) || (touchR && in.right))) ? (touchL ? -1 : 1) : 0;
    if (p.wall != 0) { p.wallT = 8; p.wallSide = p.wall; } else if (p.wallT > 0) p.wallT--;
    if (p.jumpBuf > 0) {
        if (p.coyote > 0) { p.a.vy = p.wired ? -2.9f : -3.75f; p.jumpBuf = 0; p.coyote = 0; p.a.ground = false; snd(SFX_JUMP, p.a.x, p.a.y, 0.7f); }
        else if (p.wallT > 0) { // short grace window so wall-climbing doesn't need frame-perfect timing
            if (in.up) { p.a.vy = -4.2f; p.a.vx = float(-p.wallSide) * 0.6f; } // holding up: a steeper jump, easier to climb a wall
            else { p.a.vy = -3.6f; p.a.vx = float(-p.wallSide) * 1.3f; }
            p.a.face = -p.wallSide;
            p.jumpBuf = 0; p.wall = 0; p.wallT = 0; snd(SFX_JUMP, p.a.x, p.a.y, 0.7f);
        }
    }
    if (!in.jump && p.a.vy < -1.4f && p.grappleT == 0) p.a.vy = -1.4f; // variable jump height

    if (p.jet > 0 && in.jump && !p.a.ground) { // rocket pack: hold jump to climb
        p.jet--;
        p.a.vy = std::max(p.a.vy - 0.36f, -2.4f);
        if (pa.size() < 700) {
            Particle q; q.x = p.a.x - float(p.a.face) * 2 + (rnd() - 0.5f) * 3; q.y = p.a.y; q.vx = 0; q.vy = 1.2f + rnd(); q.life = q.maxlife = 12;
            q.col = rnd() < 0.5f ? RGB(255, 220, 80) : RGB(255, 110, 30); q.kind = 2; pa.push_back(q);
        }
        if (p.jet % 8 == 0) snd(SFX_JET, p.a.x, p.a.y, 0.6f);
    }
    if (p.grappleT == 0) p.a.vy += GRAV;
    float maxfall = p.wall ? 1.0f : 5.0f;
    if (p.a.vy > maxfall) p.a.vy = maxfall;

    bool hx = false;
    if (riding >= 0 && riding < int(gas.size()) && gas[size_t(riding)].alive) { // carried by the bottle under his feet
        p.a.x += gas[size_t(riding)].mdx;
        p.a.y += gas[size_t(riding)].mdy;
    }
    float oldFeet = p.a.y, vyPre = p.a.vy;
    move_actor(p.a, p.a.vx, p.a.vy, true, &hx);
    riding = -1;
    for (size_t gi = 0; gi < gas.size(); gi++) { // gas bottles are platforms you can stand and ride on
        const Gas &gb = gas[gi];
        if (!gb.alive) continue;
        float top = gb.top();
        if (vyPre >= -0.01f && fabsf(p.a.x - gb.a.x) < gb.halfW() + float(p.a.w) / 2 - 1.0f &&
            oldFeet <= top + 2.5f + std::max(0.0f, gb.mdy) && p.a.y >= top - 0.6f && p.a.y <= top + 8) {
            p.a.y = top; p.a.vy = 0; p.a.ground = true; riding = int(gi);
            break;
        }
    }

    // actions
    if (b.prim != P_CHARGE && b.prim != P_MINIGUN && b.prim != P_REVOLVER && b.prim != P_DYNAMITE && b.prim != P_KATANA && b.prim != P_WHIP) {
        if (in.fire) fire_primary(in);
    } else fire_primary(in);
    if (in.melee && !prev.melee && p.punchT == 0) { // Y: a punch (or a rifle butt) for everyone, blade fighters included
        do_melee(14, 2, true);
        p.punchT = 10; p.punchMax = 10;
        snd(SFX_SLASH, p.a.x, p.a.y, 0.5f);
    }
    if (in.special && !prev.special) use_special();

    // hazards
    int tx0 = int(floorf((p.a.x - 2) / TS)), tx1 = int(floorf((p.a.x + 2) / TS));
    int ty0 = int(floorf((p.a.y - 4) / TS)), ty1 = int(floorf((p.a.y - 0.5f) / TS));
    for (int y = ty0; y <= ty1; y++)
        for (int x = tx0; x <= tx1; x++)
            if (tget(x, y) == T_SPIKE && p.invuln == 0 && vyPre > 1.0f) kill_player(2, 0); // only a jump or a fall onto them kills: walking through is safe
    if (p.alive && p.a.y > float(H * TS) + 40) kill_player(3, 0);
    // barbed wire: it drags at the bro (and takes a little blood) without killing him
    bool inWire = false;
    for (int y = ty0; y <= ty1; y++)
        for (int x = tx0; x <= tx1; x++)
            if (tget(x, y) == T_WIRE) inWire = true;
    p.wired = inWire;
    if (inWire && p.alive) {
        p.a.vx *= 0.55f;
        if (fabsf(p.a.vx) > 0.15f || p.a.vy < -0.5f) {
            if (p.anim % 9 == 0) debris(p.a.x, p.a.y - 4, 2, RGB(170, 15, 15), 1.0f);
            if (p.anim % 30 == 0) snd(SFX_HIT, p.a.x, p.a.y, 0.25f);
        }
    }
}

// ---- enemies ----------------------------------------------------------------------
void Game::update_enemies() {
    if (bossBannerT > 0) bossBannerT--;
    if (bossWarnT > 0) bossWarnT--;
    for (size_t i = 0; i < en.size(); i++) {
        Enemy &e = en[i];
        if (!e.alive) continue;
        if (e.flash > 0) e.flash--;
        e.anim++;
        if (e.cd > 0) e.cd--;
        bool near_ = fabsf(e.a.x - (pl.alive ? pl.a.x : camX + VW / 2.0f)) < 420 && e.a.y < float(H * TS) + 100;
        if (e.type != E_BOSS && !near_) { e.alert = 0; e.saw = false; continue; } // out of sight, out of mind: he starts afresh when you come back
        if (e.type != E_BOSS && e.a.y > float(H * TS) + 40) { kill_enemy(int(i), 0, C_FALL); continue; }

        // mocking laughter after the bro dies
        if (e.laughDelay > 0 && --e.laughDelay == 0) { e.laugh = 110; snd(SFX_LAUGH, e.a.x, e.a.y, 1.0f); }
        if (e.laugh > 0) {
            e.laugh--;
            e.a.vx = 0;
            e.a.face = (pl.a.x >= e.a.x) ? 1 : -1;
            e.a.vy += GRAV;
            move_actor(e.a, 0, e.a.vy, false);
            continue;
        }
        if (e.type == E_OFFICER) { // unarmed and static: just stands there in his khaki uniform
            e.a.vy += GRAV;
            move_actor(e.a, 0, e.a.vy, false);
            if (pl.alive && fabsf(pl.a.x - e.a.x) < 260) e.a.face = pl.a.x >= e.a.x ? 1 : -1;
            continue;
        }
        if (e.type == E_DOG) { // fast melee attacker: spots the bro, barks, charges and bites
            float dxp = pl.a.x - e.a.x, dyp = pl.a.y - e.a.y;
            bool seen = pl.alive && pl.cloak == 0 && fabsf(dxp) < 190 && fabsf(dyp) < 40 && los(e.a.x, e.a.y - 5, pl.a.x, pl.a.y - 8);
            if (seen) { if (e.alert == 0) snd(SFX_BARK, e.a.x, e.a.y); e.alert = 150; }
            else if (e.alert > 0) e.alert--;
            if (e.alert > 0 && pl.alive) { float want = dxp > 0 ? 1.0f : -1.0f; e.a.face = int(want); e.a.vx = want * 1.7f; }
            else {
                if (--e.timer < -rndi(0, 40) - 60) e.timer = rndi(30, 120);
                e.a.vx = e.timer > 0 ? float(e.dir) * 0.5f : 0.0f;
                if (e.a.vx != 0) e.a.face = e.dir;
            }
            e.a.vy += GRAV;
            if (e.a.vy > 5) e.a.vy = 5;
            bool hxd = false;
            float oldxd = e.a.x;
            move_actor(e.a, e.a.vx, e.a.vy, true, &hxd);
            if (hxd) { if (e.alert > 0 && e.a.ground) e.a.vy = -3.2f; else { e.dir = -e.dir; e.a.x = oldxd; e.a.face = e.dir; } }
            if (e.alert == 0 && e.a.ground && e.a.vx != 0 && !solid_px(e.a.x + float(e.a.face) * 6, e.a.y + 4)) { e.dir = -e.dir; e.a.face = e.dir; }
            if (pl.alive && pl.invuln == 0 && !pl.ride && !pl.remote && fabsf(dxp) < 9 && dyp > -12 && dyp < 6) kill_player(0, dxp); // the bite
            continue;
        }

        // hazards
        // spikes kill the soldiers like they kill the bro: only if they jump or fall onto them
        if (e.type != E_BOSS && e.type != E_OFFICER && e.a.impact > 1.0f && tget(int(floorf(e.a.x / TS)), int(floorf((e.a.y - 2) / TS))) == T_SPIKE) { kill_enemy(int(i), 0, C_SPIKE); continue; }
        e.a.impact = 0;

        if (e.type == E_BOSS) { update_boss(i); continue; }

        if (e.stun > 0) { e.stun--; if (e.frozen) { e.a.vx = 0; if (e.stun == 0) e.frozen = false; } else e.a.vx *= 0.9f; e.a.vy += GRAV; move_actor(e.a, e.a.vx, e.a.vy, false); continue; }

        if (e.type == E_GUNNER && e.aux > 0) { // mid-salvo: planted, spraying straight ahead
            e.a.vx = 0; e.a.vy += GRAV; if (e.a.vy > 5) e.a.vy = 5;
            move_actor(e.a, 0, e.a.vy, false);
            if (--e.burstT <= 0) {
                e.burstT = 3; e.aux--;
                float ang = e.a.face > 0 ? 0.0f : PI_F;
                spawn_bullet(PK_EBULLET, e.a.x + float(e.a.face) * 11, e.a.y - 10 + float(rndi(-2, 2)), ang, 1.9f, 160, 1, false);
                if (e.aux % 2 == 0) snd(SFX_ESHOT, e.a.x, e.a.y, 0.8f);
            }
            if (e.cd > 0) e.cd--;
            continue;
        }
        // vision
        bool see = false;
        float dx = 0, dy = 0;
        if (pl.alive && pl.cloak == 0) {
            dx = pl.a.x - e.a.x; dy = (pl.a.y - 8) - (e.a.y - 8);
            float range = e.type == E_BOMBER ? 150.0f : 170.0f;
            if (fabsf(dx) < range && fabsf(dy) < 50 && ((dx > 0) == (e.a.face > 0) || e.alert > 0) && los(e.a.x, e.a.y - 8, pl.a.x, pl.a.y - 8)) see = true;
        }
        if (see) { if (e.alert == 0) { e.alert = 1; e.timer = rndi(44, 62); /* reaction time: 0.7-1.0 s */ if (e.type == E_BOMBER) { snd(SFX_SCREAM, e.a.x, e.a.y, 1.0f); snd(SFX_IGNITE, e.a.x, e.a.y, 0.9f); e.aux = 188; } } } // the fuse is lit: ~3 s to live
        else if (e.alert > 0 && ++e.alert > 180) e.alert = 0;
        if (see && !e.saw && e.alert > 1) e.timer = std::max(e.timer, rndi(40, 60)); // spotted again after losing the bro: he has to re-aim first
        e.saw = see;
        if (e.type == E_BOMBER && e.aux > 0 && --e.aux == 0) { e.aux = -1; kill_enemy(int(i), 0, C_EXPLOSION); continue; } // blows up 3 s after charging, wherever he is

        if (e.type == E_BOMBER && e.aux > 0 && pa.size() < 700 && tick % 3 == 0) { // the lit fuse spits sparks
            Particle f; f.x = e.a.x + float(e.a.face) * 3; f.y = e.a.y - 8; f.vx = (rnd() - 0.5f) * 0.5f; f.vy = -0.5f - rnd() * 0.6f;
            f.life = f.maxlife = rndi(8, 14); f.col = rnd() < 0.5f ? RGB(255, 225, 90) : RGB(255, 120, 30); f.kind = 4;
            pa.push_back(f);
        }
        float speed = e.type == E_BOMBER ? 1.15f : (e.type == E_HEAVY || e.type == E_GUNNER) ? 0.35f : 0.45f;
        float want = 0;
        if (e.alert > 0 && e.type == E_BOMBER && see) {
            want = dx > 0 ? 1.0f : -1.0f;
            e.a.face = int(want);
            if (fabsf(dx) < 12 && fabsf(dy) < 14) want = 0; // right next to you: he waits for the fuse to run out
        } else if (e.alert > 0 && see) {
            int nf = dx > 0 ? 1 : -1;
            if (nf != e.a.face) { e.a.face = nf; e.timer = std::max(e.timer, rndi(30, 46)); } // turning round takes a moment (jumping over him buys time)
            if (e.timer > 0) e.timer--;
            else if (e.cd == 0) {
                // soldiers only shoot straight ahead, so they need to be level with the bro
                if (fabsf(dy) < 18) {
                    float ang = e.a.face > 0 ? 0.0f : PI_F;
                    float sx = e.a.x + float(e.a.face) * 8, sy = e.a.y - 9;
                    if (e.type == E_GUNNER) { // a long salvo of about 15 rounds
                        e.aux = 15; e.burstT = 0; e.cd = 130 + rndi(0, 40);
                    } else if (e.type == E_HEAVY) {
                        for (int k = -1; k <= 1; k++) spawn_bullet(PK_EBULLET, sx, sy + float(k) * 4, ang, 1.9f, 160, 1, false);
                        e.cd = 70;
                    } else if (e.type == E_RPG) { // slow rocket: dodge it or shoot it down
                        Proj rk; rk.kind = PK_EROCKET; rk.x = sx; rk.y = sy; rk.ang = ang; rk.vx = cosf(ang) * 1.5f; rk.vy = 0; rk.life = 220;
                        add_proj(rk);
                        e.cd = 160 + rndi(0, 60);
                        snd(SFX_ROCKET, e.a.x, e.a.y, 0.8f);
                    } else {
                        spawn_bullet(PK_EBULLET, sx, sy, ang, 1.8f, 160, 1, false);
                        e.cd = 60 + rndi(0, 30);
                    }
                    if (e.type != E_GUNNER) snd(SFX_ESHOT, e.a.x, e.a.y);
                }
            }
            if (fabsf(dx) > 120 || fabsf(dy) >= 18) want = dx > 0 ? 0.6f : -0.6f; // close in / line up
        } else {
            // patrol
            if (--e.timer < -rndi(0, 40) - 60) e.timer = rndi(30, 120);
            if (e.timer > 0) want = float(e.dir) * 0.5f;
        }
        e.a.vx = want * speed / 0.5f * 0.5f;
        if (e.type == E_BOMBER && e.alert > 0) e.a.vx = want * speed;
        if (want == 0) e.a.vx = 0;
        else if (e.alert == 0 || e.type == E_BOMBER) { if (want != 0) e.a.face = want > 0 ? 1 : -1; }
        e.a.vy += GRAV;
        if (e.a.vy > 5) e.a.vy = 5;
        bool hx = false;
        float oldx = e.a.x;
        move_actor(e.a, e.a.vx, e.a.vy, true, &hx);
        // turn around at walls and ledges while patrolling
        if (e.alert == 0 || e.type != E_BOMBER) {
            bool ledge = e.a.ground && e.a.vx != 0 &&
                         !solid_px(e.a.x + float(e.a.face) * 5, e.a.y + 4) && !solid_px(e.a.x + float(e.a.face) * 5, e.a.y + 12);
            if ((hx || ledge) && e.alert == 0) { e.dir = -e.dir; e.a.x = oldx; e.a.face = e.dir; }
        }
        // touching the player: bombers explode, others do nothing
    }
    en.erase(std::remove_if(en.begin(), en.end(), [this](const Enemy &e) { return !e.alive && e.type != E_BOSS; }), en.end());
    bossIdx = -1;
    for (size_t i = 0; i < en.size(); i++) if (en[i].type == E_BOSS && en[i].alive) bossIdx = int(i);
}

// ---- projectiles ------------------------------------------------------------------
void Game::update_projs() {
    for (size_t i = 0; i < pr.size(); i++) {
        Proj &p = pr[i];
        if (p.life <= 0) continue;
        p.life--;
        bool boom = false;
        float bx = p.x, by = p.y;
        switch (p.kind) {
        case PK_GRENADE: case PK_FIREBOMB: case PK_TURKEY: case PK_MOLOTOV: case PK_DYNA: {
            if (p.kind == PK_DYNA && p.stuck) {
                if (p.stuck > 0 && p.target >= 0 && p.target < int(en.size()) && en[size_t(p.target)].alive) {
                    p.x = en[size_t(p.target)].a.x; p.y = en[size_t(p.target)].a.y - 8;
                }
                if (p.life <= 0) boom = true;
                break;
            }
            p.vy += 0.13f;
            float nx = p.x + p.vx, ny = p.y + p.vy;
            if (solid_px(nx, p.y)) {
                if (p.kind == PK_DYNA) { p.stuck = 1; p.vx = p.vy = 0; p.target = -1; }
                else { p.vx = -p.vx * 0.5f; nx = p.x; }
            }
            if (solid_px(p.x, ny)) {
                if (p.kind == PK_DYNA) { p.stuck = 1; p.vx = p.vy = 0; p.target = -1; }
                else if (p.kind == PK_MOLOTOV) { p.life = 0; boom = true; }
                else {
                    p.vy = -p.vy * (p.kind == PK_TURKEY ? 0.7f : 0.45f);
                    p.vx *= 0.8f;
                    ny = p.y;
                    if (fabsf(p.vy) < 0.4f) p.vy = 0;
                }
            }
            if (!p.stuck) { p.x = nx; p.y = ny; }
            for (size_t k = 0; k < en.size() && !boom; k++) {
                Enemy &e = en[k];
                if (!e.alive) continue;
                if (fabsf(p.x - e.a.x) < float(e.a.w) / 2 + 2 && p.y > e.a.y - float(e.a.h) - 2 && p.y < e.a.y + 2) {
                    if (p.kind == PK_DYNA && !p.stuck) { p.stuck = 1; p.target = int(k); p.vx = p.vy = 0; }
                    else if (p.kind == PK_MOLOTOV) { boom = true; }
                }
            }
            if (p.life <= 0 || (p.kind == PK_DYNA && p.life <= 0)) boom = true;
            if (p.kind == PK_TURKEY && p.life <= 0) boom = true;
            if (rnd() < 0.3f && p.kind != PK_DYNA && pa.size() < 700) {
                Particle s; s.x = p.x; s.y = p.y; s.vx = 0; s.vy = -0.2f; s.life = s.maxlife = 10; s.col = RGB(200, 200, 200); s.kind = 3; pa.push_back(s);
            }
            break;
        }
        case PK_BOOMERANG: {
            if (p.target == 0) {
                p.vx *= 0.955f;
                if (solid_px(p.x + p.vx * 2, p.y)) { // the blade bites into the wall before coming back
                    int tx = int(floorf((p.x + p.vx * 2) / TS)), ty = int(floorf(p.y / TS));
                    damage_tile(tx, ty, 2, true);
                    damage_tile(tx, ty - 1, 2, true);
                    damage_tile(tx, ty + 1, 2, true);
                    p.target = 1;
                } else if (fabsf(p.vx) < 0.5f) p.target = 1;
            } else {
                float dx = pl.a.x - p.x, dy = (pl.a.y - 9) - p.y, d = sqrtf(dx * dx + dy * dy);
                if (d < 9 || !pl.alive) { p.life = 0; break; }
                p.vx = dx / d * 3.4f; p.vy = dy / d * 3.4f;
            }
            p.x += p.vx; p.y += p.vy;
            if (p.stuck > 0) p.stuck--;
            else for (size_t k = 0; k < en.size(); k++) {
                Enemy &e = en[k];
                if (!e.alive) continue;
                if (fabsf(p.x - e.a.x) < float(e.a.w) / 2 + 4 && p.y > e.a.y - float(e.a.h) - 3 && p.y < e.a.y + 3) { hurt_enemy(int(k), 2, p.vx, C_MELEE); p.stuck = 8; break; }
            }
            for (Prop &c : props)
                if (c.type == PR_CAGE && !c.used && fabsf(p.x - c.a.x) < 9 && p.y > c.a.y - 16 && p.y < c.a.y + 2) open_cage(c);
            if (tick % 5 == 0) snd(SFX_SLASH, p.x, p.y, 0.3f);
            break;
        }
        default: {
            // fast projectiles: sub-step <= 2px
            if (p.kind == PK_ROCKET) {
                float sp = sqrtf(p.vx * p.vx + p.vy * p.vy);
                if (sp < 4.6f) { p.vx *= 1.035f; p.vy *= 1.035f; }
                if (pa.size() < 700) { Particle s; s.x = p.x - p.vx * 2; s.y = p.y - p.vy * 2; s.vx = 0; s.vy = -0.1f; s.life = s.maxlife = 14; s.col = RGB(220, 220, 220); s.kind = 3; pa.push_back(s); }
            }
            if (p.kind == PK_MISSILE) { p.vy = std::min(5.0f, p.vy + 0.12f); }
            if (p.kind == PK_FLAME) { p.vy -= 0.01f; p.vx *= 0.985f; }
            if (p.kind == PK_HOMING || p.kind == PK_RAY) {
                if (p.kind == PK_HOMING) {
                    int t = p.target;
                    if (t < 0 || t >= int(en.size()) || !en[size_t(t)].alive) t = nearest_enemy(p.x, p.y, 120);
                    p.target = t;
                    if (t >= 0) {
                        float tx = en[size_t(t)].a.x - p.x, ty = en[size_t(t)].a.y - float(en[size_t(t)].a.h) / 2 - p.y;
                        float want = atan2f(ty, tx), cur = atan2f(p.vy, p.vx);
                        float d = want - cur;
                        while (d > PI_F) d -= 2 * PI_F;
                        while (d < -PI_F) d += 2 * PI_F;
                        float st2 = std::max(-0.12f, std::min(0.12f, d));
                        float sp = sqrtf(p.vx * p.vx + p.vy * p.vy);
                        p.vx = cosf(cur + st2) * sp; p.vy = sinf(cur + st2) * sp;
                    }
                }
            }
            if (p.kind == PK_REMOTE && pa.size() < 700) {
                Particle s; s.x = p.x - p.vx * 2; s.y = p.y - p.vy * 2; s.vx = 0; s.vy = 0; s.life = s.maxlife = 12; s.col = RGB(255, 170, 60); s.kind = 4; pa.push_back(s);
            }
            float sp = sqrtf(p.vx * p.vx + p.vy * p.vy);
            int n = int(sp / 2) + 1;
            float sx = p.vx / float(n), sy = p.vy / float(n);
            bool friendly = !(p.kind == PK_EBULLET || p.kind == PK_EROCKET);
            if (p.kind == PK_SPEAR && p.stuck > 0) p.stuck--;
            for (int s = 0; s < n && p.life > 0 && !boom; s++) {
                p.x += sx; p.y += sy;
                int tx = int(floorf(p.x / TS)), ty = int(floorf(p.y / TS));
                uint8_t t = tget(tx, ty);
                if (p.x < 0 || p.x > float(W * TS) || p.y > float(H * TS) + 50) { p.life = 0; break; }
                if (t != T_AIR && t != T_SPIKE && t != T_WIRE) {
                    if (p.kind == PK_ROCKET || p.kind == PK_MISSILE || p.kind == PK_REMOTE || p.kind == PK_RAY || p.kind == PK_EROCKET) boom = true;
                    else if (p.kind == PK_FLAME) { if (flammable(t)) ignite_tile(tx, ty); else damage_tile(tx, ty, 1, true); p.life = 0; } // the flames burn through earth and stone
                    else if (p.kind == PK_EBULLET) { if (t == T_BARREL) damage_tile(tx, ty, 1); p.life = 0; }
                    else { damage_tile(tx, ty, (p.kind == PK_REVBULLET || p.kind == PK_SPEAR) ? 2 : 1); p.life = 0; if (p.kind != PK_PELLET) debris(p.x, p.y, 1, RGB(255, 220, 120), 1.0f); }
                    break;
                }
                bool hitGas = false;
                for (size_t gi = 0; gi < gas.size() && !hitGas; gi++) {
                    Gas &gb = gas[gi];
                    if (!gb.alive || fabsf(p.x - gb.a.x) > gb.halfW() + 2 || p.y < gb.top() - 1 || p.y > gb.bottom() + 1) continue;
                    hitGas = true;
                    if (p.kind == PK_ROCKET || p.kind == PK_MISSILE || p.kind == PK_REMOTE || p.kind == PK_RAY || p.kind == PK_EROCKET) boom = true;
                    else {
                        ignite_gas(int(gi), rndi(10, 45));
                        if (p.kind == PK_FLAME) p.life -= 5; else p.life = 0;
                    }
                }
                if (hitGas) { if (boom || p.life <= 0) break; }
                if (friendly) {
                    for (size_t k = 0; k < en.size(); k++) {
                        Enemy &e = en[k];
                        if (!e.alive || (p.kind == PK_SPEAR && p.stuck > 0)) continue;
                        float ex = e.a.x, ey = e.a.y;
                        if (fabsf(p.x - ex) < float(e.a.w) / 2 + 3 && p.y > ey - float(e.a.h) - 2 && p.y < ey + 2) {
                            if (p.kind == PK_ROCKET || p.kind == PK_MISSILE || p.kind == PK_REMOTE || p.kind == PK_RAY) { boom = true; break; }
                            if (p.kind == PK_CRYO) { // frozen solid (bosses and the officer shrug it off)
                                if (e.type == E_BOSS || e.type == E_OFFICER) hurt_enemy(int(k), 1, p.vx, C_BULLET);
                                else { e.frozen = true; e.stun = 360; e.a.vx = 0; e.flash = 4; debris(e.a.x, e.a.y - 8, 6, RGB(170, 225, 250), 1.4f); snd(SFX_HIT, e.a.x, e.a.y, 0.8f); }
                                p.life = 0;
                                break;
                            }
                            hurt_enemy(int(k), p.dmg, p.vx, p.kind == PK_FLAME ? C_FIRE : (p.kind == PK_SPEAR ? C_MELEE : C_BULLET));
                            if (p.kind == PK_SPEAR) p.stuck = 8;
                            if (p.kind == PK_FLAME) { p.life -= 5; }
                            else if (p.pierce > 0) p.pierce--;
                            else p.life = 0;
                            break;
                        }
                    }
                    for (Prop &c : props)
                        if (c.type == PR_CAGE && !c.used && fabsf(p.x - c.a.x) < 8 && p.y > c.a.y - 14 && p.y < c.a.y) {
                            open_cage(c);
                            if (p.kind != PK_FLAME) p.life = 0;
                        }
                } else if (pl.alive && pl.invuln == 0 && !pl.remote) {
                    if (fabsf(p.x - pl.a.x) < float(pl.a.w) / 2 + 2 && p.y > pl.a.y - float(pl.a.h) - 2 && p.y < pl.a.y + 1) {
                        if (p.kind == PK_EROCKET) boom = true; // the explosion does the killing
                        else { kill_player(0, p.vx); p.life = 0; }
                    }
                }
                if (p.kind == PK_FLAME && pa.size() < 700 && rnd() < 0.5f) {
                    Particle f; f.x = p.x; f.y = p.y; f.vx = p.vx * 0.2f; f.vy = -0.3f; f.life = f.maxlife = 10; f.col = rnd() < 0.5f ? RGB(255, 220, 80) : RGB(255, 110, 30); f.kind = 4; pa.push_back(f);
                }
            }
            if (p.life <= 0 && (p.kind == PK_ROCKET || p.kind == PK_MISSILE || p.kind == PK_REMOTE || p.kind == PK_EROCKET)) boom = true;
            break;
        }
        }
        if (boom) {
            p.life = 0;
            float x = p.x, y = p.y;
            (void)bx; (void)by;
            switch (p.kind) {
            case PK_ROCKET: case PK_MISSILE: case PK_REMOTE: explode(x, y, 24, 14, true); break;
            case PK_EROCKET: explode(x, y, 22, 12, false, true); break;
            case PK_RAY: explode(x, y, 15, 10, true); break;
            case PK_GRENADE: explode(x, y, 26, 12, true); break;
            case PK_DYNA: explode(x, y, 22, 13, true); break;
            case PK_TURKEY: explode(x, y, 38, 16, true); break;
            case PK_FIREBOMB:
                explode(x, y, 14, 8, true);
                for (int k = 0; k < 16; k++) {
                    Proj f; f.kind = PK_FLAME; f.x = x; f.y = y - 2; f.life = 30; f.pierce = 99;
                    float a = -PI_F * rnd(); float s = 0.8f + rnd() * 1.6f;
                    f.vx = cosf(a) * s * (rnd() < 0.5f ? 1 : -1); f.vy = sinf(a) * s;
                    add_proj(f);
                }
                break;
            case PK_MOLOTOV:
                for (int k = 0; k < 18; k++) {
                    Proj f; f.kind = PK_FLAME; f.x = x; f.y = y - 2; f.life = 55; f.pierce = 99;
                    float a = -PI_F * rnd(); float s = 0.4f + rnd() * 1.3f;
                    f.vx = cosf(a) * s * (rnd() < 0.5f ? 1 : -1); f.vy = sinf(a) * s * 0.5f;
                    add_proj(f);
                }
                add_trauma(x, y, 0.15f);
                break;
            default: break;
            }
            if (p.kind == PK_REMOTE) pl.remote = false;
        }
    }
    pr.erase(std::remove_if(pr.begin(), pr.end(), [](const Proj &p) { return p.life <= 0; }), pr.end());

    // airstrike markers
    for (Strike &s : strikes) {
        if (s.t > 0) { s.t--; continue; }
        if (s.left > 0 && tick % 6 == 0) {
            Proj m; m.kind = PK_MISSILE; m.x = s.x + float(rndi(-36, 36)); m.y = camY - 8; m.vy = 3.0f; m.life = 200;
            if (s.hostile) { m.kind = PK_EROCKET; m.ang = PI_F / 2; m.vx = 0; m.vy = 2.6f; m.life = 260; } // a boss shell falls and explodes on whatever it hits
            add_proj(m);
            s.left--;
        }
    }
    strikes.erase(std::remove_if(strikes.begin(), strikes.end(), [](const Strike &s) { return s.t <= 0 && s.left <= 0; }), strikes.end());
}

void Game::update_particles() {
    for (Particle &p : pa) {
        p.life--;
        if (p.kind == 0 || p.kind == 1 || p.kind == 5) {
            bool blood_p = p.kind != 0;
            p.vy += 0.12f;
            float nx = p.x + p.vx, ny = p.y + p.vy;
            if (solid_px(nx, p.y)) {
                if (blood_p) { stain(int(floorf(nx / TS)), int(floorf(p.y / TS)), 38); if (p.kind == 1) p.life = 0; }
                p.vx = -p.vx * 0.4f; nx = p.x;
            }
            if (solid_px(p.x, ny)) {
                if (blood_p) {
                    int ttx = int(floorf(p.x / TS)), tty = int(floorf(ny / TS));
                    stain(ttx, tty, p.vy > 0 ? 55 : 25);
                    if (p.vy > 0) { stain(ttx - 1, tty, 18); stain(ttx + 1, tty, 18); }
                    if (p.kind == 1 && p.vy > 0) p.life = 0; // splat
                }
                p.vy = -p.vy * (p.kind == 5 ? 0.5f : 0.35f); p.vx *= 0.7f; ny = p.y; if (fabsf(p.vy) < 0.3f) p.vy = 0;
            }
            p.x = nx; p.y = ny;
        } else if (p.kind == 3 || p.kind == 4) {
            p.x += p.vx; p.y += p.vy;
            p.vy -= 0.01f;
            p.vx *= 0.96f;
        } else { p.x += p.vx; p.y += p.vy; }
    }
    pa.erase(std::remove_if(pa.begin(), pa.end(), [](const Particle &p) { return p.life <= 0; }), pa.end());
    for (Boom &b : booms) b.t++;
    booms.erase(std::remove_if(booms.begin(), booms.end(), [](const Boom &b) { return b.t >= b.maxt; }), booms.end());
}

void Game::update_props() {
    for (Prop &p : props) {
        if (p.type == PR_CAGE && !p.used && pl.alive && fabsf(pl.a.x - p.a.x) < 9 && fabsf(pl.a.y - p.a.y) < 14) open_cage(p); // touching the cage frees him
        if (p.type == PR_FREED && !p.used) {
            p.a.vy += GRAV;
            p.a.vx = 0.7f * float(((tick / 50) % 2) ? 1 : -1);
            move_actor(p.a, p.a.vx, p.a.vy, true);
            p.timer++;
            if (pl.alive && fabsf(pl.a.x - p.a.x) < 8 && fabsf(pl.a.y - p.a.y) < 12) {
                p.used = true;
                snd(SFX_PICKUP, pl.a.x, pl.a.y);
                pl.lives++;
                bool isNew = p.bro > 0 && !owned(p.bro);
                if (isNew) { // a new bro joins the roster for good
                    st.slots[curSlot].broMask |= 1ull << p.bro;
                    settings_dirty = true;
                }
                queue.push_back(pick_owned(pl.bro));
                int nb = p.bro >= 0 ? p.bro : pick_owned(pl.bro);
                swap_bro(nb);
                if (isNew) { newBro = p.bro; gs = GS_NEWBRO; stateT = 0; }
                Particle s; s.x = pl.a.x; s.y = pl.a.y - 8; s.vx = 0; s.vy = -0.5f; s.life = s.maxlife = 30; s.col = RGB(255, 255, 120); s.kind = 2; pa.push_back(s);
            }
        } else if (p.type == PR_AMMO && !p.used && pl.alive) {
            if (fabsf(pl.a.x - p.a.x) < 7 && fabsf((pl.a.y - 7) - (p.a.y - 3)) < 8) {
                if (pl.spec < BROS[pl.bro].specAmmo) {
                    p.used = true;
                    snd(SFX_PICKUP, pl.a.x, pl.a.y);
                    pl.spec = BROS[pl.bro].specAmmo;
                } else if (toastT == 0) { toastMsg = "AMMO FULL"; toastT = 70; } // the box stays where it is
            }
        } else if (p.type == PR_FLAG && !p.used && pl.alive) {
            if (fabsf(pl.a.x - p.a.x) < 14 && fabsf(pl.a.y - p.a.y) < 30) {
                p.used = true;
                snd(SFX_FLAG, p.a.x, p.a.y);
                checkX = p.a.x;
                checkY = p.a.y;
                debris(p.a.x, p.a.y - 20, 8, RGB(255, 255, 255), 1.5f);
            }
        }
    }
    props.erase(std::remove_if(props.begin(), props.end(), [](const Prop &p) { return p.used && p.type != PR_FLAG && p.type != PR_CAGE; }), props.end());
}

// ---- main update ------------------------------------------------------------------
void Game::snd(int id, float x, float y, float vol, float minK) {
    float cx = pl.alive ? pl.a.x : camX + VW / 2.0f, cy = pl.alive ? pl.a.y : camY + VH / 2.0f;
    float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
    float k = std::max(minK, 1.0f - d / 340.0f);
    if (k <= 0 || sounds.size() >= 24) return;
    sounds.push_back({id, vol * k});
}

int Game::music_track() const {
    switch (gs) {
    case GS_TITLE: case GS_SLOTS: case GS_MISSIONS: case GS_ROSTER: return 4;
    case GS_NEWBRO: return -1;
    case GS_OPTIONS: return menuFrom == GS_TITLE ? 4 : lvl_track();
    case GS_PLAY: case GS_PAUSE: return ex.phase > 0 ? 5 : stage == STAGES - 1 ? 6 : lvl_track();
    case GS_VICTORY: return 7;
    default: return -1;
    }
}

void Game::update(const Input &in) {
    if (sounds.size() > 48) sounds.clear(); // nobody is draining them (headless tests)
    if (gs == GS_NEWBRO) { // the action stops: black screen, voice, name and face until A or B is pressed
        stateT++;
        if (stateT == 12) sounds.push_back({SPEECH_BASE + SP_NEWBRO, 1.0f});
        if (stateT > 30 && ((in.jump && !prev.jump) || (in.fire && !prev.fire) || (in.start && !prev.start))) { gs = GS_PLAY; introT = 0; }
        prev = in;
        return;
    }
    if (debugMode && gs == GS_PLAY && in.select && !prev.select) { // debug: SELECT = next bro, UP+SELECT = previous bro
        swap_bro((pl.bro + (in.up ? NUM_BROS - 1 : 1)) % NUM_BROS);
        pl.spec = BROS[pl.bro].specAmmo;
    }
    if (gs == GS_PLAY && introT > 0) { // black title card with a voice line, then a slow fade into the level
        introT++;
        if (introT == 14) sounds.push_back({SPEECH_BASE + INTRO_SP[introPhrase], 1.0f});
        if (introT > 50 && introT < INTRO_HOLD && ((in.jump && !prev.jump) || (in.start && !prev.start))) introT = INTRO_HOLD;
        if (introT >= INTRO_HOLD + INTRO_FADE) introT = 0;
        if (introT < INTRO_HOLD && introT > 0) { prev = in; stateT++; return; } // frozen while the card is up
    }
    if (gs == GS_PLAY) {
        if (ex.phase == 3 && ex.done && ((in.jump && !prev.jump) || (in.start && !prev.start))) { finish_level(); prev = in; return; }
        if ((in.start && !prev.start)) { gs = GS_PAUSE; menuSel = 0; prev = in; return; }
        tick++;
        if (ex.phase == 0) playTicks++;
        stateT++;
        if (bannerT > 0) bannerT--;
        if (flashScreen > 0) flashScreen--;
        update_gas(); update_trees();
        if (pl.alive) update_player(in);
        else {
            pl.deadT++;
            if (pl.deadT > 80) {
                if (pl.lives > 0) { int l = pl.lives; spawn_player(false); pl.lives = l; next_bro(); }
                else { gs = GS_OVER; stateT = 0; menuSel = 0; failPhrase = rnd_ui(NUM_FAIL); }
            }
        }
        update_enemies();
        update_projs();
        process_explosions();
        process_collapse();
        update_chunks();
        update_pillars();
        update_particles();
        update_props();
        update_fire();
        update_exfil();
        // camera
        float tx = pl.alive ? pl.a.x : camX + VW / 2.0f, ty = pl.alive ? pl.a.y - 28 : camY + VH / 2.0f;
        for (const Proj &p : pr) if (p.kind == PK_REMOTE) { tx = p.x; ty = p.y; }
        if (pl.alive && !pl.remote) tx += float(pl.a.face) * 18 + pl.a.vx * 8;
        float wantX = tx - VW / 2.0f, wantY = ty - VH * 0.55f;
        wantX = std::max(0.0f, std::min(float(W * TS - VW), wantX));
        wantY = std::max(0.0f, std::min(float(H * TS - VH), wantY));
        if (tick < 2) { camX = wantX; camY = wantY; }
        camX += (wantX - camX) * 0.12f;
        camY += (wantY - camY) * 0.1f;
        trauma = std::max(0.0f, trauma - 0.012f);
        rumble = std::max(0.0f, rumble - 0.03f);
        float amp = powf(trauma, 1.5f) * 9.0f * float(st.shake) / 100.0f;
        shakeX = int(roundf((rnd() - 0.5f) * 2 * amp));
        shakeY = int(roundf((rnd() - 0.5f) * 2 * amp));
    } else {
        shakeX = shakeY = 0;
        update_menu(in);
        if (gs != GS_PLAY) rumble = std::max(0.0f, rumble - 0.1f);
        stateT++;
        tick++;
    }
    prev = in;
}

// ---- mockery, extraction, results ---------------------------------------------------
void Game::trigger_laughs() {
    int nearest = -1;
    float bd = 320.0f * 320.0f;
    for (size_t i = 0; i < en.size(); i++) {
        const Enemy &e = en[i];
        if (!e.alive || e.type == E_BOSS || e.type == E_BOMBER || e.type == E_DOG) continue;
        float dx = e.a.x - pl.a.x, dy = e.a.y - pl.a.y, d = dx * dx + dy * dy;
        if (d < bd) { bd = d; nearest = int(i); }
    }
    (void)nearest;
    for (size_t i = 0; i < en.size(); i++) { // only the soldiers who had the bro in sight can laugh, and only one in three does
        Enemy &e = en[i];
        if (!e.alive || e.type == E_BOSS || e.type == E_BOMBER || e.type == E_DOG || e.type == E_OFFICER) continue;
        if (e.alert > 0 && in_view(e.a.x, e.a.y, 0) && rnd() < 1.0f / 3.0f) e.laughDelay = 25 + rndi(8, 50);
    }
}

void Game::start_exfil(float hoverX, float groundY) {
    if (ex.phase != 0) return;
    ex = Exfil();
    ex.phase = 1;
    ex.hoverX = hoverX;
    ex.groundY = groundY;
    ex.hoverY = groundY - 83;
    ex.x = hoverX + 320;
    ex.y = ex.hoverY - 60;
    ex.finalTicks = playTicks;
}

void Game::finish_level() {
    SaveSlot &sl = st.slots[curSlot];
    if (!debugMode) { // debug runs never touch the saved progress
        sl.used = 1;
        sl.mask |= 1 << level;
        sl.kills += int(killLog.size());
        sl.seconds += int(float(ex.finalTicks) / 62.5f);
        settings_dirty = true;
    }
    if (level < LAST_LEVEL) start_level(level + 1);
    else { gs = GS_VICTORY; stateT = 0; ex = Exfil(); }
}

void Game::start_slot_game(int lvl) {
    st.slots[curSlot].used = 1;
    st.lastSlot = curSlot;
    settings_dirty = true;
    pl = Player();
    pl.lives = 1;
    start_level(lvl);
}

void Game::update_exfil() {
    if (ex.phase == 0) return;
    ex.t++;
    float sway = sinf(float(ex.t) * 0.07f) * 3.0f;
    float bx = ex.x + sway * ex.ladderLen / 72.0f, by = ex.y + 5 + ex.ladderLen;
    {   // rotor loudness: fades with distance, and dies away over ~5 s once the helicopter has lifted off with the bro
        float lx = pl.alive ? pl.a.x : camX + VW / 2.0f, ly = pl.alive ? pl.a.y : camY + VH / 2.0f;
        float d = sqrtf((ex.x - lx) * (ex.x - lx) + (ex.y - ly) * (ex.y - ly));
        float v = std::max(0.0f, 1.0f - d / 700.0f);
        if (ex.phase == 3 && ex.t3 > 25) v *= std::max(0.0f, 1.0f - float(ex.t3 - 25) / 330.0f);
        ex.vol = v;
    }
    if (ex.phase == 1) {
        float dx = ex.hoverX - ex.x;
        ex.vx = std::max(-3.0f, std::min(3.0f, dx * 0.03f));
        ex.x += ex.vx;
        ex.y += (ex.hoverY - ex.y) * 0.04f;
        if (fabsf(dx) < 1.5f) ex.phase = 2;
    } else if (ex.phase == 2) {
        ex.y = ex.hoverY + sinf(float(ex.t) * 0.05f) * 1.5f;
        if (ex.ladderLen < 72) ex.ladderLen += 1.6f;
        // walk under the ladder to grab it
        if (pl.alive && ex.ladderLen >= 50 && fabsf(pl.a.x - bx) < 11 && pl.a.y > ex.y + 8 && pl.a.y - float(BH) < by + 2) {
            pl.ride = true;
            pl.invuln = 100000;
            pl.remote = false;
            pl.a.vx = pl.a.vy = 0;
            ex.phase = 3;
            ex.t3 = 0;
            ex.grabOff = std::max(8.0f, std::min(ex.ladderLen + 1.0f, pl.a.y - (ex.y + 5))); // he stays glued at the height he grabbed
            snd(SFX_PICKUP, pl.a.x, pl.a.y);
        }
    }
    if (ex.phase == 3) {
        ex.t3++;
        if (ex.t3 > 25) {
            ex.vy = std::max(ex.vy - 0.03f, -2.4f);
            ex.vx = std::min(ex.vx + 0.02f, 1.6f);
        } else ex.vx *= 0.95f;
        ex.x += ex.vx;
        ex.y += ex.vy;
        float cx = ex.x + sway * ex.grabOff / 72.0f;
        pl.a.x = cx;
        pl.a.y = ex.y + 5 + ex.grabOff;
        pl.a.face = 1;
        // the recap starts only once the helicopter has carried the bro out of the screen
        if (!ex.recap && (ex.y + ex.ladderLen < camY - 20 || ex.t3 > 700)) {
            ex.recap = true;
            ex.resT = 0;
            int n = int(killLog.size());
            ex.interval = n <= 30 ? 31 : std::max(9, 930 / n); // half a second per victim, faster for huge body counts
        }
        if (ex.recap) {
            ex.resT++;
            const int base0 = 45;
            int n = std::min(int(killLog.size()), 96);
            for (int i = std::max(0, (ex.resT - base0 - 14) / ex.interval - 1); i <= std::min(n - 1, (ex.resT - base0) / ex.interval + 1); i++) {
                int age = ex.resT - base0 - i * ex.interval;
                if (age == 0) sounds.push_back({SFX_TICK, 0.8f});
                if (age == 10) {
                    switch (killLog[size_t(i)].cause) {
                    case C_BULLET: sounds.push_back({SFX_SHOT, 0.7f}); sounds.push_back({SFX_DEATH, 0.7f}); break;
                    case C_EXPLOSION: case C_GAS: sounds.push_back({SFX_BOOM_SMALL, 0.7f}); break;
                    case C_FIRE: sounds.push_back({SFX_IGNITE, 0.7f}); break;
                    case C_MELEE: sounds.push_back({SFX_SLASH, 0.8f}); sounds.push_back({SFX_DEATH, 0.6f}); break;
                    case C_CRUSH: sounds.push_back({SFX_CRASH, 0.7f}); break;
                    case C_SPIKE: sounds.push_back({SFX_HIT, 0.9f}); sounds.push_back({SFX_DEATH, 0.7f}); break;
                    case C_LIGHTNING: sounds.push_back({SFX_LIGHTNING, 0.7f}); break;
                    default: sounds.push_back({SFX_DEATH, 0.7f}); break;
                    }
                }
            }
            int endT = base0 + (n > 0 ? (n - 1) * ex.interval : 0) + 48;
            if (ex.resT > endT) { if (++ex.doneT > 15) ex.done = true; }
        }
    }
}
