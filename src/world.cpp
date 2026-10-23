// Broyoo - world: tiles, level generation, destruction, explosions
#include "game.h"
#include "audio.h"
#include <cmath>
#include <cstring>
#include <algorithm>

const Theme &theme_for(int lvl);

static u32 shade_c(u32 c, int d) {
    int r = int((c >> 16) & 255) + d, g = int((c >> 8) & 255) + d, b = int(c & 255) + d;
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return RGB(r, g, b);
}

float Game::rnd() {
    rngs = rngs * 1664525u + 1013904223u;
    return float((rngs >> 8) & 0xFFFF) / 65536.0f;
}
int Game::rndi(int a, int b) {
    int v = a + int(rnd() * float(b - a + 1));
    return v > b ? b : v;
}

uint8_t Game::tget(int x, int y) const {
    if (x < 0 || x >= W) return T_METAL;
    if (y < 0 || y >= H) return T_AIR;
    return tile[y * MAXW + x];
}
bool Game::solid_t(int tx, int ty) const {
    uint8_t t = tget(tx, ty);
    return t != T_AIR && t != T_SPIKE && t != T_WIRE;
}
bool Game::box_hits(float cx, float feet, int w, int h) const {
    int x0 = int(floorf((cx - float(w) / 2) / TS)), x1 = int(floorf((cx + float(w) / 2 - 0.01f) / TS));
    int y0 = int(floorf((feet - float(h)) / TS)), y1 = int(floorf((feet - 0.01f) / TS));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (solid_t(x, y)) return true;
    return false;
}

int Game::tile_max_hp(uint8_t t) const {
    switch (t) {
    case T_DIRT: return 3;
    case T_STONE: return 10;
    case T_BRICK: return 5;
    case T_WOOD: return 2;
    case T_BARREL: return 1;
    case T_CRATE: return 2;
    case T_SAND: return 2;
    case T_SPIKE: return 1;
    case T_METAL: return 255;
    default: return 1;
    }
}

void Game::set_tile(int x, int y, uint8_t t) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    tile[y * MAXW + x] = t;
    thp[y * MAXW + x] = uint8_t(tile_max_hp(t));
    tvar[y * MAXW + x] = uint8_t(rndi(0, 3));
}

void Game::add_mook(float x, float y, int type) {
    Enemy e;
    e.type = type;
    e.a.x = x;
    e.a.y = y;
    e.a.w = BW;
    e.a.h = BH;
    e.hp = (type == E_HEAVY || type == E_GUNNER) ? 2 : 1; // a big round or two small ones
    if (type == E_OFFICER) { e.dir = -1; }
    if (type == E_DOG) { e.a.w = 12; e.a.h = 8; }
    if (type == E_GUNNER) { e.a.w = BW + 2; e.a.h = BH + 2; }
    e.dir = rnd() < 0.5f ? -1 : 1;
    e.a.face = e.dir;
    e.timer = rndi(0, 60);
    en.push_back(e);
}

void Game::add_prop(int type, float x, float y) {
    Prop p;
    p.type = type;
    p.a.x = x;
    p.a.y = y;
    p.a.w = 8;
    p.a.h = 12;
    if (type == PR_AMMO) { p.a.w = 8; p.a.h = 6; }
    if (type == PR_FLAG) { p.a.w = 10; p.a.h = 24; }
    if (type == PR_HELI) { p.a.w = 40; p.a.h = 20; }
    props.push_back(p);
}

// ---- structures --------------------------------------------------------------
void Game::stamp_bunker(int x0, int g, int w, int hh) {
    uint8_t wall = T_BRICK;
    uint8_t roof = world >= 2 ? T_STONE : T_WOOD;
    for (int y = g - hh; y < g; y++)
        for (int x = x0; x < x0 + w; x++) {
            bool edge = x < x0 + 2 || x >= x0 + w - 2;
            bool top = y < g - hh + 2;
            if (top) set_tile(x, y, roof);
            else if (edge) set_tile(x, y, wall);
            else set_tile(x, y, T_AIR);
        }
    int side = rndi(0, 1);
    for (int y = g - 4; y < g; y++)
        for (int k = 0; k < 2; k++) set_tile(side ? x0 + w - 1 - k : x0 + k, y, T_AIR);
    set_tile(x0 + w / 2, g - 1, T_BARREL);
    set_tile(x0 + w / 2 + 1, g - 1, T_CRATE);
    set_tile(x0 + w / 2 + 1, g - 2, T_CRATE);
    add_gas(float((x0 + w / 2 - 2) * TS + 4), float(g * TS));
    add_mook(float((x0 + 3) * TS + 4), float(g * TS), E_MOOK);
    add_mook(float((x0 + w - 4) * TS + 4), float(g * TS), (world >= 1 || stage >= 2) && rnd() < 0.4f ? E_HEAVY : E_MOOK);
}

void Game::stamp_tower(int x0, int g) {
    for (int y = g - 12; y < g; y++) {
        set_tile(x0, y, T_WOOD);
        set_tile(x0 + 5, y, T_WOOD);
    }
    for (int x = x0 - 1; x <= x0 + 6; x++) set_tile(x, g - 13, T_WOOD);
    set_tile(x0 - 1, g - 14, T_BRICK);
    set_tile(x0 + 6, g - 14, T_BRICK);
    set_tile(x0 + 4, g - 14, T_BARREL);
    add_mook(float((x0 + 2) * TS + 4), float((g - 13) * TS), E_MOOK);
    add_mook(float((x0 + 3) * TS + 4), float(g * TS), E_MOOK);
}

void Game::stamp_cage(int x0, int g) {
    for (int x = x0; x < x0 + 4; x++)
        for (int y = g - 2; y < g; y++) set_tile(x, y, T_BRICK);
    add_prop(PR_CAGE, float(x0 * TS + 16), float((g - 2) * TS));
    add_mook(float((x0 - 3) * TS), float(g * TS), E_MOOK);
    add_mook(float((x0 + 7) * TS), float(g * TS), (world >= 1 || stage >= 2) ? E_HEAVY : E_MOOK);
}

void Game::stamp_fort(int x0, int g, int w) {
    for (int y = g - 10; y < g; y++)
        for (int x = x0; x < x0 + w; x++) {
            bool edge = x < x0 + 2 || x >= x0 + w - 2;
            bool top = y < g - 8;
            if (top || edge) set_tile(x, y, y == g - 10 ? T_METAL : T_STONE);
            else set_tile(x, y, T_AIR);
        }
    for (int y = g - 4; y < g; y++)
        for (int k = 0; k < 2; k++) set_tile(x0 + w - 1 - k, y, T_AIR);
    set_tile(x0 + 3, g - 1, T_BARREL);
    set_tile(x0 + 4, g - 1, T_BARREL);
    add_mook(float((x0 + w / 2) * TS), float(g * TS), rnd() < 0.5f ? E_GUNNER : E_HEAVY);
    add_mook(float((x0 + w - 5) * TS), float(g * TS), E_MOOK);
    add_prop(PR_AMMO, float((x0 + 6) * TS), float(g * TS));
}

// ---- level generation ----------------------------------------------------------
// enemy mix per level: {mook, bomber, heavy, rpg, dog} cumulative-free weights
static int pick_enemy(int lvl, float r) {
    static const float wt[NUM_WORLDS][5] = {
        {0.74f, 0.20f, 0.00f, 0.00f, 0.06f},
        {0.50f, 0.15f, 0.15f, 0.10f, 0.10f},
        {0.38f, 0.20f, 0.17f, 0.15f, 0.10f},
        {0.38f, 0.14f, 0.15f, 0.15f, 0.18f},
        {0.28f, 0.20f, 0.20f, 0.16f, 0.16f},
        {0.50f, 0.20f, 0.10f, 0.10f, 0.10f},
    };
    static const int ty[5] = {E_MOOK, E_BOMBER, E_HEAVY, E_RPG, E_DOG};
    float acc = 0;
    static const float gunnerShare[NUM_WORLDS] = {0.06f, 0.10f, 0.12f, 0.12f, 0.15f, 0.10f}; // part of the plain soldiers who are machine gunners
    for (int i = 0; i < 5; i++) { acc += wt[lvl % NUM_WORLDS][i]; if (r < acc) return (i == 0 && r < wt[lvl % NUM_WORLDS][0] * gunnerShare[lvl % NUM_WORLDS]) ? int(E_GUNNER) : ty[i]; }
    return E_MOOK;
}

// ---- support pillars, sand ----------------------------------------------------------------
void Game::add_pillar(int x, int y0, int y1) {
    if (x < 0 || x >= W) return;
    for (int y = std::max(0, y0); y < std::min(H, y1); y++)
        if (tile[y * MAXW + x] == T_AIR) pillar[y * MAXW + x] = 6;
}

void Game::damage_pillar(int tx, int ty, int dmg) {
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return;
    int idx = ty * MAXW + tx;
    if (pillar[idx] == 0) return;
    if (pillar[idx] > dmg) { pillar[idx] = uint8_t(pillar[idx] - dmg); return; }
    pillar[idx] = 0;
    debris(float(tx * TS + 4), float(ty * TS + 4), 4, th.wood, 1.6f);
    crumble.push_back({tx, ty - 1, 10});        // what was held up by this cell starts to give way
    collapseQ.push_back({tx, ty});              // the platform re-checks what holds it
}

// pillars with nothing under them crumble from the bottom up, one cell at a time; the platform follows when nothing holds it
void Game::update_pillars() {
    for (size_t i = 0; i < crumble.size();) {
        Crumble &c = crumble[i];
        if (--c.t > 0) { i++; continue; }
        int x = c.x, y = c.y;
        crumble.erase(crumble.begin() + long(i));
        if (!pillar_at(x, y)) continue;
        bool supported = y + 1 >= H || solid_t(x, y + 1) || pillar_at(x, y + 1);
        if (supported) continue;
        pillar[y * MAXW + x] = 0;
        debris(float(x * TS + 4), float(y * TS + 4), 3, th.wood, 1.4f);
        if (tick % 2 == 0) snd(SFX_CRASH, float(x * TS), float(y * TS), 0.25f);
        collapseQ.push_back({x, y});
        crumble.push_back({x, y - 1, 7});
    }
}

void Game::sand_fall(int tx, int ty) { // the column of sand above a dug-out tile slides down
    if (chunks.size() >= 24) return;
    int n = 0;
    while (ty - n >= 0 && n < 16 && tile[(ty - n) * MAXW + tx] == T_SAND) n++;
    if (n == 0) return;
    Chunk c;
    for (int k = 0; k < n; k++) {
        int row = ty - (n - 1) + k;
        c.t.push_back({0, k, T_SAND, tvar[row * MAXW + tx]});
        tile[row * MAXW + tx] = T_AIR;
    }
    c.x = float(tx * TS); c.y = float((ty - n + 1) * TS); c.vy = 0; c.w = 1; c.h = n;
    chunks.push_back(c);
    debris(float(tx * TS + 4), float(ty * TS), 4, th.dirt, 1.2f);
}

// ---- relief: every world has its own landscape --------------------------------------------
void Game::gen_heights(int *hgt) {
    // per world: mountains {count, hMin, hMax, hwMin, hwMax}, rolling amplitude
    static const int MT[NUM_WORLDS][5] = {{7, 11, 22, 14, 26}, {0, 0, 0, 0, 0}, {3, 7, 12, 12, 20}, {4, 9, 16, 14, 22}, {3, 6, 10, 10, 16}, {5, 9, 16, 10, 16}};
    static const float ROLL[NUM_WORLDS] = {3.2f, 0.0f, 2.8f, 3.5f, 2.6f, 3.2f};
    mounts.clear();
    float ph1 = rnd() * 6.2832f, ph2 = rnd() * 6.2832f, ph3 = rnd() * 6.2832f;
    int want = MT[world][0] + (world == 0 ? rndi(-1, 1) : 0);
    float scale = 0.8f + 0.1f * float(stage);   // later stages have bigger mountains
    for (int tries = 0; int(mounts.size()) < want && tries < 80; tries++) {
        int cx = rndi(44, W - 54);
        bool ok = true;
        for (const Mount &m : mounts) if (std::abs(m.cx - cx) < 34) ok = false;
        if (!ok) continue;
        int h = int(float(rndi(MT[world][1], MT[world][2])) * scale);
        int hw = std::max(rndi(MT[world][3], MT[world][4]), int(float(h) * 1.1f)); // gentle enough to climb, steep enough to be a wall
        mounts.push_back({cx, hw, h});
    }
    for (int x = 0; x < W; x++) {
        float y = 40.0f;
        if (world == 1) y -= 2.4f * sinf(float(x) / 10.0f + ph1) + 1.6f * sinf(float(x) / 4.7f + ph2) + 1.2f * sinf(float(x) / 23.0f + ph3) - 1.0f; // dunes
        else y -= ROLL[world] * (sinf(float(x) / 11.0f + ph1) + 0.6f * sinf(float(x) / 5.3f + ph2));
        for (const Mount &m : mounts) {
            int d = std::abs(x - m.cx);
            if (d < m.hw) y -= float(m.h) * 0.5f * (1.0f + cosf(3.14159265f * float(d) / float(m.hw)));
        }
        float f = std::min(1.0f, std::max(0.0f, std::min(float(x - 12) / 20.0f, float(W - 24 - x) / 16.0f))); // flat ground at the start and at the end
        y = 38.0f + (y - 38.0f) * f;
        hgt[x] = std::max(14, std::min(50, int(y + 0.5f)));
    }
}

void Game::place_cage(int x0, int g, bool hidden) {
    for (int x = x0; x < x0 + 4; x++) for (int y = g - 2; y < g; y++) set_tile(x, y, T_BRICK);
    add_prop(PR_CAGE, float(x0 * TS + 16), float((g - 2) * TS));
    props.back().hidden = hidden;
}

// secret chambers inside mountains / buried bunkers under the dunes: dig (or blast) your way in
void Game::carve_chambers(const int *hgt, bool *used) {
    int want = world == 0 ? 4 : world == 1 ? 3 : world == 2 ? 2 : 3;
    std::vector<int> centers;
    int made = 0;
    for (int tries = 0; made < want && tries < 120; tries++) {
        const int cw = 13, ch = 6;
        int x0 = rndi(34, W - 34 - cw);
        bool ok = true;
        for (int k = -2; k < cw + 2; k++) if (used[x0 + k]) ok = false;
        for (int c : centers) if (std::abs(c - x0) < 36) ok = false;
        if (!ok) continue;
        int mn = 999, mx = 0;
        for (int k = 0; k < cw; k++) { mn = std::min(mn, hgt[x0 + k]); mx = std::max(mx, hgt[x0 + k]); }
        if (mx - mn > 10) continue;                                  // not on a cliff
        bool bunkerWorld = world == 1 || world == 4;
        if (!bunkerWorld && mn > 33) continue;                       // inside a real mountain
        int top = mx + 3, bottom = top + ch - 1, floorRow = bottom + 1;
        if (bottom + 5 >= H - 2) continue;
        bool bunker = bunkerWorld || rnd() < 0.3f;
        if (bunker) { // brick-lined room
            for (int y = top - 1; y <= bottom + 1; y++) for (int x = x0 - 1; x <= x0 + cw; x++) set_tile(x, y, T_BRICK);
        }
        for (int y = top; y <= bottom; y++) for (int x = x0; x < x0 + cw; x++) set_tile(x, y, T_AIR);
        if (!bunker) { // rounded rock corners
            set_tile(x0, top, T_DIRT); set_tile(x0 + cw - 1, top, T_DIRT); set_tile(x0, top + 1, T_DIRT); set_tile(x0 + cw - 1, top + 1, T_DIRT);
        }
        if (bunker && rnd() < 0.8f) { // a wooden hatch at the surface and a shaft down: the way in for the curious
            int sxp = x0 + 2 + rndi(0, 6);
            for (int y = hgt[sxp] + 1; y < top; y++) { set_tile(sxp, y, T_AIR); set_tile(sxp + 1, y, T_AIR); }
            set_tile(sxp, hgt[sxp], T_WOOD); set_tile(sxp + 1, hgt[sxp], T_WOOD);
        }
        float fy = float(floorRow * TS);
        add_prop(PR_AMMO, float((x0 + 2) * TS), fy);
        if (made == 0 || rnd() < 0.5f) place_cage(x0 + cw - 5, floorRow, true); // a prisoner (maybe a bro) in the hidden cache
        if (rnd() < 0.5f) { set_tile(x0 + 5, floorRow - 1, T_BARREL); set_tile(x0 + 6, floorRow - 1, T_BARREL); }
        if (rnd() < 0.4f) add_gas(float((x0 + 6) * TS + 4), fy);
        int n = 2 + (stage >= 1 ? 1 : 0) + rndi(0, 1);
        for (int k = 0; k < n; k++) {
            int t = pick_enemy(world, rnd());
            if (t == E_DOG && !bunker) t = E_MOOK;
            add_mook(float((x0 + 3 + k * 2) * TS + 4), fy, t);
        }
        for (int k = -2; k < cw + 2; k++) used[x0 + k] = true;
        centers.push_back(x0);
        made++;
    }
}

void Game::gen_level(int lvl) {
    level = lvl;
    world = lvl / STAGES;
    stage = lvl % STAGES;
    th = theme_for(lvl);
    rngs = (seedBase ^ 0x9E3779B9u) + uint32_t(lvl) * 7919u; // a different layout on every run
    bool boss = stage == STAGES - 1;
    W = boss ? 200 : 240 + 40 * stage;               // later stages are longer
    en.reserve(160);
    H = 60;
    memset(tile, 0, sizeof(tile));
    memset(thp, 0, sizeof(thp));
    memset(tvar, 0, sizeof(tvar));
    memset(fireT, 0, sizeof(fireT));
    memset(blood, 0, sizeof(blood));
    memset(pillar, 0, sizeof(pillar));
    crumble.clear();
    trees.clear(); belts.clear(); decos.clear(); birds.clear();
    burnList.clear(); gas.clear(); chunks.clear(); collapseQ.clear(); riding = -1;
    en.clear(); pr.clear(); pa.clear(); booms.clear(); props.clear(); pend.clear(); strikes.clear();
    bossIdx = -1;
    bossDead = false;

    static int hgt[MAXW];
    static bool used[MAXW];
    memset(used, 0, sizeof(used));
    if (!boss) gen_heights(hgt);
    if (boss) for (int x = 0; x < W; x++) hgt[x] = 40;

    struct Plan { int type, x0, w; };
    std::vector<Plan> plans;
    if (!boss) {
        int x = 26;
        int nCage = 0;
        while (x < W - 40) {
            for (int tries = 0; tries < 14 && x < W - 60; tries++) { // structures go on fairly flat ground
                int mn = 99, mx = 0;
                for (int k = -2; k < 20; k++) { mn = std::min(mn, hgt[x + k]); mx = std::max(mx, hgt[x + k]); }
                if (mx - mn <= 3) break;
                x += 3;
            }
            int t;
            float r = rnd();
            if (lvl <= 1) t = r < 0.40f ? 0 : r < 0.75f ? 1 : 2;
            else t = r < 0.30f ? 0 : r < 0.55f ? 1 : r < 0.80f ? 3 : 2;
            if (x > W * 23 / 100 + rndi(0, 14) && nCage == 0) { t = 4; nCage++; }
            else if (x > W * 50 / 100 + rndi(0, 14) && nCage == 1) { t = 4; nCage++; }
            else if (x > W * 76 / 100 + rndi(0, 10) && nCage == 2) { t = 4; nCage++; }
            int w = t == 0 ? rndi(12, 16) : t == 1 ? 8 : t == 3 ? rndi(16, 20) : t == 4 ? 4 : 6;
            plans.push_back({t, x, w});
            x += w + rndi(10, 20);
        }
        // pits
        int px = 45;
        while (px < W - 45) {
            int w = rndi(4, 6); // a full-speed jump covers ~6 tiles
            bool clash = false;
            for (const Plan &p : plans)
                if (px + w + 3 > p.x0 - 3 && px - 3 < p.x0 + p.w + 3) clash = true;
            if (!clash) {
                int gl = hgt[px - 1];
                for (int k = -3; k < 0; k++) hgt[px + k] = gl;
                for (int k = 0; k < w; k++) { hgt[px + k] = H + 5; used[px + k] = true; }
                int gr = std::max(14, std::min(50, gl));
                for (int k = 0; k < 4; k++) hgt[px + w + k] = gr;
                for (int k = -3; k < w + 4; k++) used[px + k] = true;
                plans.push_back({5, px, w}); // bridge marker
                px += w + rndi(35, 60);
            } else px += 10;
        }
    }
    // flatten plan areas
    for (const Plan &p : plans) {
        if (p.type == 5) continue;
        int g = hgt[p.x0 + p.w / 2];
        for (int x = p.x0 - 2; x < p.x0 + p.w + 2 && x < W; x++) { hgt[x] = g; used[x] = true; }
    }
    for (int x = 0; x < W; x++) surf[x] = hgt[x];
    // terrain fill
    for (int x = 0; x < W; x++)
        for (int y = hgt[x]; y < H; y++) {
            uint8_t t = world == 1 ? T_SAND : T_DIRT; // the desert is made of sand
            if (y > hgt[x] + 9 && (((x * 7 + y * 13) ^ (x * y)) & 7) < 3) t = T_STONE;
            set_tile(x, y, t);
        }
    // caves
    if (!boss) {
        int nc = 12 + world * 3 + stage * 2 + rndi(0, 4);
        for (int i = 0; i < nc; i++) {
            int cx = rndi(30, W - 30), cy = hgt[cx] + rndi(7, 14), rr = rndi(3, 5);
            for (int y = cy - rr; y <= cy + rr; y++)
                for (int x = cx - rr * 2; x <= cx + rr * 2; x++) {
                    float dx = float(x - cx) / 2.0f, dy = float(y - cy);
                    if (dx * dx + dy * dy < float(rr * rr) && y > hgt[x < W && x >= 0 ? x : 0] + 3 && x > 0 && x < W && y < H - 3 && !used[x]) set_tile(x, y, T_AIR);
                }
        }
    }
    // boss arena walls
    if (boss) {
        for (int y = 0; y < H; y++)
            for (int k = 0; k < 3; k++) { set_tile(k, y, T_METAL); set_tile(W - 1 - k, y, T_METAL); }
    }
    // stamp structures
    for (const Plan &p : plans) {
        int g = hgt[std::min(W - 1, p.x0 + p.w / 2)];
        switch (p.type) {
        case 0: stamp_bunker(p.x0, g, p.w, 8); break;
        case 1: stamp_tower(p.x0, g); break;
        case 2: // barrel stack + mooks
            for (int k = 0; k < 3; k++) set_tile(p.x0 + k, g - 1, T_BARREL);
            set_tile(p.x0 + 1, g - 2, T_BARREL);
            add_gas(float((p.x0 + 6) * TS + 4), float(g * TS));
            set_tile(p.x0 + 7, g - 1, T_CRATE);
            if (!boss) { add_mook(float((p.x0 + 4) * TS), float(g * TS), E_MOOK); add_mook(float((p.x0 + 5) * TS), float(g * TS), E_MOOK); }
            break;
        case 3: stamp_fort(p.x0, g, p.w); break;
        case 4: stamp_cage(p.x0, g); break;
        case 5: { // bridge over a pit
            int gl = hgt[p.x0 - 1];
            if (rnd() < 0.7f)
                for (int k = 0; k < p.w; k++) { bool hole = p.w <= 5 && k > 0 && k < p.w - 1 && (k & 1) && rnd() < 0.25f; // an occasional missing plank
                    if (!hole) set_tile(p.x0 + k, gl, world >= 2 && rnd() < 0.3f ? T_METAL : T_WOOD); }
            break;
        }
        }
    }
    if (boss) build_boss_arena(world);
    else carve_chambers(hgt, used);
    // spikes and barrels on open ground, ammo
    if (!boss) {
        for (int x = 30; x < W - 30; x += rndi(12, 28)) {
            if (used[x]) continue;
            float r = rnd();
            int g = hgt[x];
            if (r < 0.35f && lvl >= 0) { int n = rndi(3, 5); for (int k = 0; k < n; k++) if (!used[x + k] && hgt[x + k] == g) set_tile(x + k, g - 1, T_SPIKE); }
            else if (r < 0.50f) { // red crates, sometimes with a gas bottle next to them
                set_tile(x, g - 1, T_BARREL);
                if (rnd() < 0.5f) set_tile(x, g - 2, T_BARREL);
                if (rnd() < 0.5f) add_gas(float((x + 2) * TS + 4), float(g * TS));
            }
            else if (r < 0.65f) { // brown crates (they only burn)
                for (int k = 0; k < 2; k++) { set_tile(x + k, g - 1, T_CRATE); if (rnd() < 0.6f) set_tile(x + k, g - 2, T_CRATE); }
                if (rnd() < 0.4f) set_tile(x, g - 3, T_BARREL);
            }
            else if (r < 0.72f) { add_gas(float(x * TS + 4), float(g * TS)); add_gas(float((x + 1) * TS + 6), float(g * TS)); }
            else if (r < 0.8f) add_prop(PR_AMMO, float(x * TS + 4), float(g * TS));
            else if (r < 0.9f && lvl >= 0) { int n = rndi(4, 6); for (int k = 0; k < n; k++) if (!used[x + k] && hgt[x + k] == g) set_tile(x + k, g - 1, T_WIRE); } // barbed wire
            for (int k = 0; k < 6; k++) used[std::min(W - 1, x + k)] = true;
        }
        // ground mooks
        for (int x = 34; x < W - 34; x += std::max(6, rndi(9, 16) - stage)) {
            if (used[x]) continue;
            int n = rndi(1, 3);
            for (int k = 0; k < n; k++) {
                float r = rnd();
                int t = pick_enemy(lvl, r);
                int xx = x + k * 2;
                if (xx < W - 30 && !used[xx] && hgt[xx] < H) add_mook(float(xx * TS + 4), float(hgt[xx] * TS), t);
            }
        }
        // floating dirt ledges with ammo crates
        for (int i = 0; i < 6; i++) {
            int fx = rndi(40, W - 40), fy = hgt[fx] - rndi(9, 12);
            if (used[fx]) continue;
            for (int x = fx; x < fx + 5; x++) { set_tile(x, fy, T_DIRT); set_tile(x, fy + 1, T_DIRT); }
            add_pillar(fx, fy + 2, hgt[fx]); add_pillar(fx + 4, fy + 2, hgt[fx + 4]); // held up by pillars behind the action
            add_prop(PR_AMMO, float((fx + 2) * TS), float(fy * TS));
        }
    }
    if (!boss) gen_scenery(hgt); // the living background: trees, rocks, tents, igloos, birds...
    // clear spawn area
    for (int x = 3; x < 12; x++)
        for (int y = 0; y < hgt[x]; y++) set_tile(x, y, T_AIR);
    // checkpoints
    if (!boss) {
        for (int x : {W * 32 / 100 + rndi(-6, 6), W * 64 / 100 + rndi(-6, 6)}) { int xx = x; while (used[xx] && xx < W - 30) xx++; add_prop(PR_FLAG, float(xx * TS), float(hgt[xx] * TS)); }
        // the enemy officer waits near the end of the level, guarded by a few soldiers
        for (int x = W - 24; x < W - 6; x++) used[x] = true;
        add_mook(float((W - 14) * TS + 4), float(hgt[W - 14] * TS), E_OFFICER);
        add_mook(float((W - 19) * TS), float(hgt[W - 19] * TS), E_MOOK);
        add_mook(float((W - 11) * TS), float(hgt[W - 11] * TS), (world >= 1 || stage >= 2) ? E_HEAVY : E_MOOK);
        add_mook(float((W - 22) * TS), float(hgt[W - 22] * TS), E_BOMBER);
        set_tile(W - 17, hgt[W - 17] - 1, T_CRATE);
        set_tile(W - 9, hgt[W - 9] - 1, T_CRATE);
    } else {
        spawn_boss(world);
    }
    // soldiers placed beside a structure can end up inside sloping ground: lift them onto the surface
    for (Enemy &e : en) if (e.type != E_BOSS) free_up(e.a, 40);
    assign_cage_bros();
    checkX = 6.0f * TS + 4;
    checkY = float(hgt[6] * TS);
    // keep the player's start zone free of enemies
    for (Enemy &e : en) if (e.a.x < 14 * TS) e.alive = false;
    en.erase(std::remove_if(en.begin(), en.end(), [](const Enemy &e) { return !e.alive; }), en.end());
    for (size_t i = 0; i < en.size(); i++) if (en[i].type == E_BOSS) bossIdx = int(i);
    build_tiles();
}

// the bros hidden in this level (those not freed yet) are put into its cages; hidden cages come first
void Game::assign_cage_bros() {
    std::vector<size_t> cages;
    for (size_t i = 0; i < props.size(); i++) if (props[i].type == PR_CAGE) { props[i].bro = -1; cages.push_back(i); }
    std::sort(cages.begin(), cages.end(), [this](size_t a, size_t b) {
        if (props[a].hidden != props[b].hidden) return props[a].hidden;
        return props[a].a.x < props[b].a.x;
    });
    size_t k = 0;
    for (int b = 1; b < NUM_BROS && k < cages.size(); b++)
        if (bro_home_level(b) == level && !owned(b)) props[cages[k++]].bro = b;
}

// ---- destruction ---------------------------------------------------------------
void Game::debris(float x, float y, int n, u32 col, float speed) {
    for (int i = 0; i < n && pa.size() < 700; i++) {
        Particle p;
        p.x = x + (rnd() - 0.5f) * 4;
        p.y = y + (rnd() - 0.5f) * 4;
        float a = rnd() * 6.2832f, s = rnd() * speed;
        p.vx = cosf(a) * s;
        p.vy = sinf(a) * s - 0.8f;
        p.life = p.maxlife = rndi(25, 60);
        p.col = shade_c(col, rndi(-20, 20));
        p.kind = 0;
        pa.push_back(p);
    }
}

void Game::gore(float x, float y, int n, float dirx, u32 col) {
    for (int i = 0; i < n && pa.size() < 900; i++) {
        Particle p;
        p.x = x;
        p.y = y + (rnd() - 0.5f) * 6;
        p.vx = dirx * rnd() * 2.2f + (rnd() - 0.5f) * 3.2f;
        p.vy = -rnd() * 3.2f;
        p.life = p.maxlife = rndi(40, 90);
        p.col = shade_c(col, rndi(-30, 30));
        p.kind = 1;
        pa.push_back(p);
    }
}

void Game::clear_topping(int tx, int ty) {
    if (ty <= 0) return;
    uint8_t u = tile[(ty - 1) * MAXW + tx];
    if (u != T_SPIKE && u != T_WIRE) return;
    tile[(ty - 1) * MAXW + tx] = T_AIR;
    debris(float(tx * TS + 4), float((ty - 1) * TS + 4), 3, u == T_SPIKE ? RGB(170, 175, 185) : RGB(150, 150, 155), 1.2f);
}

void Game::damage_tile(int tx, int ty, int dmg, bool silent) {
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return;
    uint8_t t = tile[ty * MAXW + tx];
    if (t == T_AIR || t == T_METAL) return;
    int hp = thp[ty * MAXW + tx] - dmg;
    u32 col = (t == T_DIRT || t == T_SAND) ? th.dirt : t == T_STONE ? th.stone : t == T_BRICK ? th.brick : t == T_WOOD ? th.wood : t == T_BARREL ? RGB(190, 40, 35) : t == T_CRATE ? RGB(150, 100, 55) : th.metal;
    if (hp <= 0) {
        tile[ty * MAXW + tx] = T_AIR;
        fireT[ty * MAXW + tx] = 0;
        if (collapseQ.size() < 600) collapseQ.push_back({tx, ty});
        clear_topping(tx, ty);
        if (ty > 0 && tile[(ty - 1) * MAXW + tx] == T_SAND) sand_fall(tx, ty - 1); // dig into sand and it always caves in
        if (t == T_CRATE && rnd() < 0.3f) { // some crates hold ammo: it drops to the ground below
            int gy = ty + 1;
            while (gy < H - 1 && tile[gy * MAXW + tx] == T_AIR && gy < ty + 24) gy++;
            add_prop(PR_AMMO, float(tx * TS + 4), float(gy * TS));
        }
        if (t == T_BARREL) pend.push_back({float(tx * TS + 4), float(ty * TS + 4), 30.0f, 16.0f, false, rndi(3, 9)});
        debris(float(tx * TS + 4), float(ty * TS + 4), silent ? 2 : 5, col, 1.8f);
    } else {
        thp[ty * MAXW + tx] = uint8_t(hp);
        if (!silent) debris(float(tx * TS + 4), float(ty * TS + 4), 1, col, 1.0f);
    }
}

void Game::add_trauma(float x, float y, float power) {
    float cx = pl.alive ? pl.a.x : camX + VW / 2.0f, cy = pl.alive ? pl.a.y - 7 : camY + VH / 2.0f;
    float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
    float k = 1.0f - d / 260.0f;
    if (k > 0) trauma = std::min(1.0f, trauma + power * k);
    float kr = 1.0f - d / 190.0f;
    if (kr > 0) rumble = std::max(rumble, std::min(1.0f, power * 1.3f * kr + (d < 40 ? 0.4f : 0.0f)));
}

void Game::explode(float x, float y, float r, float power, bool friendly, bool shell) { pend.push_back({x, y, r, power, friendly, 0, shell}); }

// how close you have to be to be killed outright: grows with the radius and, more so, with the power of the blast
float Game::blast_lethal(float r, float power) const { return r * (0.4f + power / 50.0f); }

void Game::process_explosions() {
    // chain reactions: explosions can carry a delay, so a row of crates goes off one after another
    std::vector<PendExpl> cur;
    cur.swap(pend);
    int ran = 0;
    for (PendExpl &ex : cur) {
        if (ex.delay > 0) { ex.delay--; pend.push_back(ex); continue; }
        if (ran >= 40) { pend.push_back(ex); continue; } // spread a huge chain over several frames
        ran++;
        stat_explosions++;
        booms.push_back({ex.x, ex.y, 0, ex.r > 36 ? 22 : 16, ex.r});
        snd(ex.r > 36 ? SFX_BOOM_BIG : ex.r > 30 ? SFX_BOOM : SFX_BOOM_SMALL, ex.x, ex.y, 1.0f, ex.r > 30 ? 0.35f : 0.0f); // big blasts are heard even from off-screen
        hit_trees(ex.x, ex.y, ex.r);
        scare_birds(ex.x, ex.y, ex.r * 3.0f + 50.0f);
        int ns = int(ex.r / 2);
        for (int i = 0; i < ns && pa.size() < 700; i++) {
            Particle p;
            float a = rnd() * 6.2832f, s = 0.4f + rnd() * 2.2f;
            p.x = ex.x; p.y = ex.y;
            p.vx = cosf(a) * s; p.vy = sinf(a) * s - 0.3f;
            p.life = p.maxlife = rndi(14, 34);
            p.col = rnd() < 0.5f ? RGB(255, 200, 60) : RGB(255, 120, 30);
            p.kind = i % 3 == 0 ? 3 : 4;
            if (p.kind == 3) p.col = RGB(80, 80, 85);
            pa.push_back(p);
        }
        int tx0 = int(floorf((ex.x - ex.r) / TS)), tx1 = int(floorf((ex.x + ex.r) / TS));
        int ty0 = int(floorf((ex.y - ex.r) / TS)), ty1 = int(floorf((ex.y + ex.r) / TS));
        for (int ty = ty0; ty <= ty1; ty++)
            for (int tx = tx0; tx <= tx1; tx++) {
                float dx = float(tx * TS + 4) - ex.x, dy = float(ty * TS + 4) - ex.y;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < ex.r) {
                    int dmg = int(ex.power * (1.0f - d / ex.r) + 0.99f);
                    if (dmg > 0) { damage_tile(tx, ty, dmg); damage_pillar(tx, ty, dmg); }
                }
            }
        // a little further out, flammable things catch fire and go off a few seconds later
        float R2 = ex.r * 2.0f;
        int rx0 = int(floorf((ex.x - R2) / TS)), rx1 = int(floorf((ex.x + R2) / TS));
        int ry0 = int(floorf((ex.y - R2) / TS)), ry1 = int(floorf((ex.y + R2) / TS));
        for (int ty = ry0; ty <= ry1; ty++)
            for (int tx = rx0; tx <= rx1; tx++) {
                if (tx < 0 || tx >= W || ty < 0 || ty >= H || !flammable(tile[ty * MAXW + tx])) continue;
                float dx = float(tx * TS + 4) - ex.x, dy = float(ty * TS + 4) - ex.y;
                float d = sqrtf(dx * dx + dy * dy);
                if (d < R2 && (tile[ty * MAXW + tx] == T_BARREL || rnd() < 0.6f)) ignite_tile(tx, ty);
            }
        for (size_t gi = 0; gi < gas.size(); gi++) {
            Gas &gb = gas[gi];
            if (!gb.alive || gb.state != 0) continue;
            float dx = gb.a.x - ex.x, dy = gb.a.y - 6 - ex.y;
            float d = sqrtf(dx * dx + dy * dy);
            if (d < R2) ignite_gas(int(gi), d < ex.r ? rndi(8, 30) : rndi(25, 90));
        }
        for (size_t i = 0; i < en.size(); i++) {
            Enemy &e = en[i];
            if (!e.alive) continue;
            if (e.type == E_OFFICER && !ex.friendly) continue; // the officer can only be killed by the bro himself
            if (e.type == E_BOSS && ex.shell) continue;        // a boss is never hurt by his own shells
            float cy = e.a.y - float(e.a.h) / 2;
            float dx = e.a.x - ex.x, dy = cy - ex.y;
            float d = sqrtf(dx * dx + dy * dy);
            float reach = ex.r + 4 + (e.type == E_BOSS ? 16.0f : 0.0f);
            if (d < reach) {
                float lethal = blast_lethal(ex.r, ex.power) + 4;
                if (e.type == E_BOSS) hurt_enemy(int(i), std::max(1, int(ex.power / 3 * (1.0f - d / reach) + 0.5f)), dx, C_EXPLOSION);
                else if (d < lethal || e.type == E_BOMBER) kill_enemy(int(i), dx, C_EXPLOSION); // close: dead. (a kamikaze catches alight)
                else { // further out the blast only throws him, and the harder the closer he stands
                    float k = 1.0f - (d - lethal) / std::max(1.0f, reach - lethal);
                    e.stun = 20 + int(25.0f * k);
                    e.a.vx = (dx >= 0 ? 1.0f : -1.0f) * (1.2f + 3.0f * k);
                    e.a.vy = -(1.8f + 2.2f * k);
                    e.a.ground = false;
                    if (e.type == E_HEAVY || e.type == E_GUNNER) hurt_enemy(int(i), 1, dx, C_EXPLOSION);
                }
            }
        }
        if (pl.alive && !pl.remote && !pl.ride && (ex.friendly || pl.invuln == 0)) { // your own blasts never hurt you, but they throw you if you stand too close
            float dx = pl.a.x - ex.x, dy = pl.a.y - 7 - ex.y;
            float d = sqrtf(dx * dx + dy * dy), lethal = blast_lethal(ex.r, ex.power) + 3, outer = lethal + ex.r * 0.6f + 10.0f;
            if (d < lethal && !ex.friendly) kill_player(1, dx);
            else if (d < outer) { // thrown by the blast: harder the closer he stands, no steering for a moment
                float k = d < lethal ? 1.0f : 1.0f - (d - lethal) / (outer - lethal);
                pl.a.vx = (dx >= 0 ? 1.0f : -1.0f) * (1.4f + 3.2f * k);
                pl.a.vy = -(1.6f + 2.4f * k);
                pl.a.ground = false;
                pl.knockT = 10 + int(16.0f * k);
                pl.crouch = false;
            }
        }
        for (Prop &p : props) {
            if (p.type != PR_CAGE || p.used) continue;
            float dx = p.a.x - ex.x, dy = p.a.y - 6 - ex.y;
            if (sqrtf(dx * dx + dy * dy) < ex.r + 6) open_cage(p);
        }
        add_trauma(ex.x, ex.y, ex.power / 16.0f);
    }
}

// ---- fire, gas bottles, blood ------------------------------------------------------------
bool Game::flammable(uint8_t t) const { return t == T_BARREL || t == T_CRATE || t == T_WOOD; }

void Game::ignite_tile(int tx, int ty, int fuse) {
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return;
    int idx = ty * MAXW + tx;
    uint8_t t = tile[idx];
    if (!flammable(t) || fireT[idx] > 0) return;
    if (fuse <= 0) fuse = t == T_BARREL ? rndi(60, 190) : t == T_CRATE ? rndi(150, 320) : rndi(180, 420);
    fireT[idx] = uint16_t(fuse);
    burnList.push_back(idx);
    snd(SFX_IGNITE, float(tx * TS), float(ty * TS), 0.6f); // crackle (the audio side never stacks two of them)
}

void Game::update_fire() {
    if (!burnList.empty() && tick % 200 == 0) { int b = burnList[0]; snd(SFX_IGNITE, float((b % MAXW) * TS), float((b / MAXW) * TS), 0.6f); } // keeps crackling while it burns
    for (size_t i = 0; i < burnList.size();) {
        int idx = burnList[i];
        int x = idx % MAXW, y = idx / MAXW;
        uint8_t t = tile[idx];
        if (fireT[idx] == 0 || !flammable(t)) { // destroyed by something else
            fireT[idx] = 0;
            burnList[i] = burnList.back();
            burnList.pop_back();
            continue;
        }
        fireT[idx]--;
        float cx = float(x * TS + 4), cy = float(y * TS + 4);
        if (pa.size() < 700 && rnd() < 0.18f) {
            Particle f; f.x = cx + (rnd() - 0.5f) * 6; f.y = cy - 2; f.vx = (rnd() - 0.5f) * 0.3f; f.vy = -0.6f - rnd() * 0.5f;
            f.life = f.maxlife = rndi(12, 24); f.col = rnd() < 0.5f ? RGB(255, 210, 70) : RGB(255, 110, 30); f.kind = 4;
            pa.push_back(f);
            if (rnd() < 0.3f) { f.kind = 3; f.col = RGB(60, 60, 65); f.life = f.maxlife = 30; f.vy = -0.5f; pa.push_back(f); }
        }
        if ((fireT[idx] & 15) == 0) { // fire spreads to flammable neighbours and heats gas bottles
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                    if (flammable(tile[ny * MAXW + nx]) && fireT[ny * MAXW + nx] == 0 && rnd() < 0.12f) ignite_tile(nx, ny);
                }
            for (size_t gi = 0; gi < gas.size(); gi++)
                if (gas[gi].alive && gas[gi].state == 0 && fabsf(gas[gi].a.x - cx) < 22 && fabsf(gas[gi].a.y - 6 - cy) < 22 && rnd() < 0.5f)
                    ignite_gas(int(gi), rndi(40, 120));
        }
        if (fireT[idx] == 0) { // burnt through
            tile[idx] = T_AIR;
            if (collapseQ.size() < 600) collapseQ.push_back({x, y});
            clear_topping(x, y);
            if (t == T_BARREL) pend.push_back({cx, cy, 30.0f, 16.0f, false, 0});
            else {
                debris(cx, cy, 4, t == T_WOOD ? th.wood : RGB(70, 50, 30), 1.2f);
                if (pa.size() < 700) { Particle s; s.x = cx; s.y = cy; s.vx = 0; s.vy = -0.4f; s.life = s.maxlife = 40; s.col = RGB(60, 60, 65); s.kind = 3; pa.push_back(s); }
            }
            burnList[i] = burnList.back();
            burnList.pop_back();
            continue;
        }
        i++;
    }
}

void Game::add_gas(float x, float y) {
    Gas g;
    g.a.x = x; g.a.y = y; g.a.w = 6; g.a.h = 13;
    gas.push_back(g);
}

void Game::ignite_gas(int i, int delay) {
    Gas &g = gas[size_t(i)];
    if (!g.alive || g.state != 0) return;
    g.state = 1;
    g.timer = std::max(1, (delay > 0 ? delay : rndi(30, 90)) / 2); // half the time it used to take
    snd(SFX_IGNITE, g.a.x, g.a.y, 0.7f);
}

void Game::update_gas() {
    for (size_t i = 0; i < gas.size(); i++) {
        Gas &g = gas[i];
        if (!g.alive) continue;
        float ox = g.a.x, oy = g.a.y;
        if (g.a.x < -60 || g.a.x > float(W * TS) + 60 || g.a.y > float(H * TS) + 120 || g.a.y < -400) { g.alive = false; continue; }
        if (g.state != 2) {
            g.a.vy += 0.19f;
            if (g.a.vy > 5) g.a.vy = 5;
            move_actor(g.a, 0, g.a.vy, false);
            if (g.state == 1) { // heating up: flame leaks from the cap
                g.timer--;
                if (g.timer % 3 == 0 && pa.size() < 700) {
                    Particle f; f.x = g.a.x + (rnd() - 0.5f) * 2; f.y = g.a.y - 14; f.vx = (rnd() - 0.5f) * 0.4f; f.vy = -1.0f - rnd();
                    f.life = f.maxlife = rndi(8, 14); f.col = rnd() < 0.5f ? RGB(255, 220, 90) : RGB(255, 120, 30); f.kind = 4;
                    pa.push_back(f);
                }
                if (g.timer % 14 == 0) snd(SFX_JET, g.a.x, g.a.y, 0.6f);
                if (g.timer <= 0) { // the jet takes over: the bottle is thrown the other way, slowly at first
                    float sgn = rnd() < 0.5f ? -1.0f : 1.0f, ang = rnd() * 0.6f;
                    g.dx = sgn * cosf(ang);
                    g.dy = -sinf(ang);
                    g.state = 2;
                    g.t = 0;
                    g.fuse = rndi(45, 80); // a short flight: it goes off before it leaves the screen
                    g.a.vy = 0;
                    snd(SFX_IGNITE, g.a.x, g.a.y, 1.0f);
                }
            }
        } else {
            g.t++;
            g.speed = std::min(3.4f, 0.12f + float(g.t) * 0.03f);
            int ns = int(ceilf(g.speed));
            float sp = g.speed / float(ns);
            for (int k = 0; k < ns; k++) {
                g.a.x += g.dx * sp;
                g.a.y += g.dy * sp;
                float cy = g.a.y - 6;
                float nx = g.a.x + g.dx * 8, ny = cy + g.dy * 8;
                bool blocked = false;
                for (int off = -3; off <= 3; off += 3) { // smash through the scenery in front
                    float px = nx - g.dy * float(off), py = ny + g.dx * float(off);
                    int tx = int(floorf(px / TS)), ty = int(floorf(py / TS));
                    if (solid_t(tx, ty)) {
                        damage_tile(tx, ty, 4);
                        if (solid_t(tx, ty)) blocked = true;
                    }
                }
                if (blocked) { // indestructible: bounce off
                    if (fabsf(g.dx) > fabsf(g.dy)) g.dx = -g.dx; else g.dy = -g.dy;
                    g.a.x += g.dx * 3; g.a.y += g.dy * 3;
                    g.t = std::max(0, g.t - 40);
                    snd(SFX_CRASH, g.a.x, g.a.y, 0.4f);
                    break;
                }
            }
            float cy = g.a.y - 6;
            // a flying bottle kills what it hits
            for (size_t k = 0; k < en.size(); k++) {
                Enemy &e = en[k];
                if (!e.alive || e.type == E_OFFICER) continue;
                if (fabsf(e.a.x - g.a.x) < 7 + float(e.a.w) / 2 + 2 && cy > e.a.y - float(e.a.h) - 4 && cy < e.a.y + 4) {
                    if (e.type == E_BOSS) { if (tick % 15 == 0) hurt_enemy(int(k), 1, g.dx); }
                    else kill_enemy(int(k), g.dx, C_GAS);
                }
            }
            for (Prop &c : props)
                if (c.type == PR_CAGE && !c.used && fabsf(c.a.x - g.a.x) < 12 && fabsf(c.a.y - 6 - cy) < 12) open_cage(c);
            float tx = g.a.x - g.dx * 8, ty = cy - g.dy * 8; // exhaust at the cap, opposite to the direction of travel
            for (int k = 0; k < 2 && pa.size() < 700; k++) {
                Particle f; f.x = tx; f.y = ty; f.vx = -g.dx * (1.0f + rnd() * 1.6f) + (rnd() - 0.5f) * 0.6f; f.vy = -g.dy * (1.0f + rnd() * 1.6f) + (rnd() - 0.5f) * 0.6f;
                f.life = f.maxlife = rndi(8, 16); f.col = k ? RGB(255, 220, 90) : RGB(255, 120, 30); f.kind = 4;
                pa.push_back(f);
            }
            if (g.t % 3 == 0 && pa.size() < 700) {
                Particle s; s.x = tx; s.y = ty; s.vx = -g.dx * 0.4f; s.vy = -g.dy * 0.4f - 0.2f; s.life = s.maxlife = 30; s.col = RGB(70, 70, 75); s.kind = 3; pa.push_back(s);
            }
            if (g.t % 8 == 0) snd(SFX_JET, g.a.x, g.a.y, 0.7f);
            if (--g.fuse <= 0) {
                pend.push_back({g.a.x, cy, 38.0f, 20.0f, false, 0}); // may well go off outside the screen
                g.alive = false;
            }
        }
        g.mdx = g.a.x - ox;
        g.mdy = g.a.y - oy;
    }
    gas.erase(std::remove_if(gas.begin(), gas.end(), [](const Gas &g) { return !g.alive; }), gas.end());
    if (riding >= int(gas.size())) riding = -1;
}

void Game::stain(int tx, int ty, int amt) {
    if (tx < 0 || tx >= W || ty < 0 || ty >= H) return;
    int idx = ty * MAXW + tx;
    if (tile[idx] == T_AIR) return;
    blood[idx] = uint8_t(std::min(255, int(blood[idx]) + amt));
}

void Game::gibs(float x, float y, int n, float dirx) {
    for (int i = 0; i < n && pa.size() < 900; i++) {
        Particle p;
        p.x = x + (rnd() - 0.5f) * 4;
        p.y = y + (rnd() - 0.5f) * 6;
        p.vx = dirx * rnd() * 1.8f + (rnd() - 0.5f) * 3.0f;
        p.vy = -1.0f - rnd() * 3.0f;
        p.life = p.maxlife = rndi(90, 170);
        p.col = rnd() < 0.5f ? RGB(190, 75, 65) : RGB(130, 25, 25);
        p.kind = 5;
        pa.push_back(p);
    }
}

// ---- structural collapse ---------------------------------------------------------------
// After tiles are destroyed, any group of solid tiles no longer connected to the ground
// (bottom row of the world, or a very large mass) detaches and falls as one chunk.
void Game::process_collapse() {
    if (collapseQ.empty()) return;
    static int visitStamp[MAXW * MAXH];
    static int bfsId = 0;
    static std::vector<int> stack, region;
    frameId++;
    static const int dx4[4] = {1, -1, 0, 0}, dy4[4] = {0, 0, 1, -1};
    for (const auto &q : collapseQ) {
        for (int k = 0; k < 4; k++) {
            int sx = q.first + dx4[k], sy = q.second + dy4[k];
            if (sx < 0 || sx >= W || sy < 0 || sy >= H || !solid_t(sx, sy)) continue;
            int start = sy * MAXW + sx;
            if (anchorStamp[start] == frameId) continue;
            bfsId++;
            region.clear();
            stack.clear();
            stack.push_back(start);
            visitStamp[start] = bfsId;
            bool anchored = false;
            while (!stack.empty() && !anchored) {
                int idx = stack.back();
                stack.pop_back();
                region.push_back(idx);
                int x = idx % MAXW, y = idx / MAXW;
                if (y >= H - 1 || region.size() > 1500 || tile[idx] == T_METAL) { anchored = true; break; } // metal never falls, and holds up whatever touches it
                for (int m = 0; m < 4; m++) {
                    int nx = x + dx4[m], ny = y + dy4[m];
                    if (nx < 0 || nx >= W || ny < 0 || ny >= H || !(solid_t(nx, ny) || pillar[ny * MAXW + nx])) continue;
                    int n = ny * MAXW + nx;
                    if (anchorStamp[n] == frameId) { anchored = true; break; }
                    if (visitStamp[n] != bfsId) { visitStamp[n] = bfsId; stack.push_back(n); }
                }
            }
            if (anchored) {
                for (int r : region) anchorStamp[r] = frameId;
                for (int r : stack) anchorStamp[r] = frameId;
                continue;
            }
            { // natural ground never falls: a mass that is mostly (3 to 1) earth / stone stays put even with some bricks, wood or crates mixed in.
              // Tunnels and caves must not collapse when a support is dug out. Only the Frost Colossus ice blocks are meant to fall.
                int nat = 0, built = 0; bool earth = false;
                for (int r : region) { uint8_t t = tile[r]; if (t == T_DIRT) { nat++; earth = true; } else if (t == T_STONE) nat++; else if (t != T_AIR) built++; }
                bool iceBlocks = boss_level() && world == 3;
                if (nat > 0 && nat >= 3 * built && (earth || !iceBlocks)) { for (int r : region) anchorStamp[r] = frameId; continue; }
            }
            if (chunks.size() >= 24) continue;
            Chunk c;
            int minx = W, miny = H, maxx = 0, maxy = 0;
            for (int r : region) {
                if (tile[r] == T_AIR) continue; // a pillar cell: it crumbles, it does not fall as a block
                int x = r % MAXW, y = r / MAXW;
                minx = std::min(minx, x); miny = std::min(miny, y); maxx = std::max(maxx, x); maxy = std::max(maxy, y);
            }
            for (int r : region) {
                int x = r % MAXW, y = r / MAXW;
                if (tile[r] == T_AIR) { pillar[r] = 0; debris(float(x * TS + 4), float(y * TS + 4), 2, th.wood, 1.0f); continue; }
                c.t.push_back({x - minx, y - miny, tile[r], tvar[r]});
                clear_topping(x, y);
                tile[r] = T_AIR;
                if (pa.size() < 700 && rnd() < 0.15f) debris(float(x * TS + 4), float(y * TS + 4), 1, th.dirt, 0.8f);
            }
            c.x = float(minx * TS); c.y = float(miny * TS); c.vy = 0; c.w = maxx - minx + 1; c.h = maxy - miny + 1;
            chunks.push_back(c);
            snd(SFX_CRASH, c.x + float(c.w * TS) / 2, c.y, 0.5f);
        }
    }
    collapseQ.clear();
}

void Game::update_chunks() {
    for (size_t ci = 0; ci < chunks.size();) {
        Chunk &c = chunks[ci];
        c.vy = std::min(c.vy + 0.15f, 4.5f);
        int steps = int(ceilf(c.vy));
        bool landed = false;
        for (int s = 0; s < steps && !landed; s++) {
            float ny = c.y + c.vy / float(steps);
            for (const ChunkTile &t : c.t) {
                int cx = int(floorf(c.x / TS)) + t.dx;
                int row = int(floorf((ny + float(t.dy * TS) + TS - 0.01f) / TS));
                if (solid_t(cx, row)) { landed = true; break; }
            }
            if (!landed) c.y = ny;
        }
        // people under it get crushed; people standing on it ride it down
        float cx0 = c.x, cx1 = c.x + float(c.w * TS);
        auto interact = [&](Actor &a, bool isPlayer, int idx) {
            for (const ChunkTile &t : c.t) {
                float tx0 = c.x + float(t.dx * TS), ty0 = c.y + float(t.dy * TS);
                if (a.x + float(a.w) / 2 <= tx0 || a.x - float(a.w) / 2 >= tx0 + TS) continue;
                if (a.y <= ty0 || a.y - float(a.h) >= ty0 + TS) continue;
                bool onTop = a.y <= ty0 + 6;
                if (onTop) { a.y = ty0; a.vy = 0; a.ground = true; return; }
                if (c.vy > 0.8f) { if (isPlayer) kill_player(4, 0); else kill_enemy(idx, 0, C_CRUSH); }
                return;
            }
        };
        if (pl.alive && pl.invuln == 0 && pl.a.x > cx0 - 8 && pl.a.x < cx1 + 8) interact(pl.a, true, -1);
        for (size_t i = 0; i < en.size(); i++)
            if (en[i].alive && en[i].type != E_BOSS && en[i].type != E_OFFICER && en[i].a.x > cx0 - 8 && en[i].a.x < cx1 + 8) interact(en[i].a, false, int(i));
        for (size_t bi = 0; bi < en.size(); bi++) { // falling ice crushes the Frost Colossus (the only way to hurt him properly)
            Enemy &b = en[bi];
            if (!b.alive || b.type != E_BOSS || b.kind != 3 || b.aux > 0 || c.vy < 0.3f) continue;
            for (const ChunkTile &t : c.t) {
                float tx0 = c.x + float(t.dx * TS), ty0 = c.y + float(t.dy * TS);
                if (b.a.x + float(b.a.w) / 2 <= tx0 || b.a.x - float(b.a.w) / 2 >= tx0 + TS) continue;
                if (b.a.y <= ty0 || b.a.y - float(b.a.h) >= ty0 + TS) continue;
                b.aux = 70; b.ph = 2; b.timer = 0;
                debris(b.a.x, b.a.y - 20, 30, RGB(200, 225, 245), 3.0f);
                trauma = 1.0f; rumble = 1.0f;
                hurt_enemy(int(bi), 25, 0, C_CRUSH);
                break;
            }
        }
        if (landed) {
            c.y = ceilf(c.y / TS) * TS;
            int bx = int(floorf(c.x / TS)), by = int(floorf(c.y / TS));
            for (const ChunkTile &t : c.t) {
                int x = bx + t.dx, y = by + t.dy;
                if (x < 0 || x >= W || y < 0 || y >= H) continue;
                if (tile[y * MAXW + x] == T_AIR) {
                    set_tile(x, y, t.type);
                    tvar[y * MAXW + x] = t.var;
                    if (t.type == T_BARREL) { tile[y * MAXW + x] = T_AIR; pend.push_back({float(x * TS + 4), float(y * TS + 4), 30.0f, 16.0f, false, 0}); }
                } else debris(float(x * TS + 4), float(y * TS + 4), 3, th.dirt, 1.5f);
                if (t.dy == c.h - 1) debris(float(x * TS + 4), float(y * TS + 8), 2, th.dirt, 1.5f);
            }
            add_trauma(c.x + float(c.w * TS) / 2, c.y, std::min(0.45f, 0.1f + float(c.t.size()) * 0.004f));
            snd(SFX_CRASH, c.x + float(c.w * TS) / 2, c.y, 1.0f);
            chunks.erase(chunks.begin() + long(ci));
            continue;
        }
        if (c.y > float(H * TS + 80)) { chunks.erase(chunks.begin() + long(ci)); continue; }
        ci++;
    }
}

// ---- jungle palms ----------------------------------------------------------------------
// trunk point at fraction f (0 foot, 1 top), before any toppling
static void tree_point(const Tree &t, float f, float &px, float &up) {
    px = float(t.lean) * f * f;
    up = float(t.h) * f;
}

void Game::hit_trees(float x, float y, float r) {
    for (Tree &t : trees) {
        if (t.fall > 0) continue;
        bool hit = false;
        for (int i = 0; i <= 8 && !hit; i++) {
            float px, up;
            tree_point(t, float(i) / 8.0f, px, up);
            float dx = t.x + px - x, dy = t.y - up - y;
            float reach = r + (i == 8 ? 14.0f : 3.0f); // the crown is wide
            if (dx * dx + dy * dy < reach * reach) hit = true;
        }
        if (!hit) continue;
        t.fall = 1;
        scare_tree(t.id); // the birds in its crown fly off
        t.dir = x < t.x ? 1 : -1; // topples away from the blast
        snd(SFX_CRASH, t.x, t.y - float(t.h) / 2, 0.5f);
    }
}

void Game::update_belts() {
    for (size_t i = 0; i < belts.size();) {
        Belt &b = belts[i];
        b.age++;
        if (!solid_px(b.x, b.y + 1)) { b.vy = std::min(b.vy + 0.19f, 4.5f); for (int k = 0; k < int(ceilf(b.vy)) && !solid_px(b.x, b.y + 1); k++) b.y += 1; } else b.vy = 0;
        if (pa.size() < 700 && b.age % 3 == 0) { // sparks off the fuse
            Particle f; f.x = b.x + (rnd() - 0.5f) * 4; f.y = b.y - 6; f.vx = (rnd() - 0.5f) * 0.6f; f.vy = -0.6f - rnd() * 0.7f;
            f.life = f.maxlife = rndi(8, 14); f.col = rnd() < 0.5f ? RGB(255, 225, 90) : RGB(255, 120, 30); f.kind = 4;
            pa.push_back(f);
        }
        if (b.age % 190 == 0) snd(SFX_IGNITE, b.x, b.y, 0.8f);   // keeps crackling
        if (--b.t <= 0) {
            pend.push_back({b.x, b.y - 4, 24.0f, 12.0f, false, 0});
            belts[i] = belts.back(); belts.pop_back();
            continue;
        }
        i++;
    }
}

void Game::draw_belts(Gfx &g, int ox, int oy) {
    for (const Belt &b : belts) {
        int bx = int(b.x) - ox - 6, by = int(b.y) - oy - 5;
        if (bx < -20 || bx > VW + 20 || by < -20 || by > VH + 20) continue;
        g.rect(bx - 1, by + 1, 14, 4, RGB(50, 35, 25));                       // the strap
        for (int k = 0; k < 4; k++) {
            g.rect(bx + k * 3 + 1, by, 2, 5, RGB(210, 40, 35));               // dynamite
            g.rect(bx + k * 3 + 1, by, 2, 1, RGB(255, 220, 90));
        }
        int fl = (b.age / 2) % 3;                                             // the fuse's flame
        g.rect(bx + 5, by - 3 - fl, 2, 2 + fl, RGB(255, 130, 30));
        g.rect(bx + 5, by - 2 - fl, 1, 1 + fl, RGB(255, 235, 110));
    }
}

void Game::update_trees() {
    for (size_t i = 0; i < trees.size();) {
        Tree &t = trees[i];
        if (t.fall > 0 && ++t.fall > 34) { // lands: leaves and splinters
            for (int k = 0; k < 6; k++) {
                float f = 0.4f + 0.12f * float(k);
                debris(t.x + float(t.dir) * float(t.h) * f * 0.9f, t.y - 3, 2, k < 2 ? RGB(60, 120, 40) : th.woodDk, 1.4f);
            }
            trees[i] = trees.back();
            trees.pop_back();
            continue;
        }
        i++;
    }
}

void Game::draw_trees(Gfx &g, int ox, int oy) {
    for (const Tree &t : trees) {
        if (t.x + 40 < float(ox) || t.x - 40 > float(ox + VW) || t.y < float(oy) || t.y - float(t.h) - 40 > float(oy + VH)) continue;
        float ang = t.fall > 0 ? 1.45f * (float(t.fall) / 34.0f) * (float(t.fall) / 34.0f) : 0.0f; // accelerating topple, up to ~83 degrees
        float th2 = ang * float(t.dir), cs = cosf(th2), sn = sinf(th2);
        float sway = t.fall > 0 ? 0.0f : sinf(float(tick) * 0.03f + t.x * 0.1f) * 1.2f;
        auto place = [&](float px, float up, int &sx, int &sy) { // rotate about the foot
            sx = int(t.x + px * cs + up * sn) - ox;
            sy = int(t.y - (up * cs - px * sn)) - oy;
        };
        int n = t.h;
        for (int i = 0; i <= n; i++) { // trunk, a little thinner towards the top, with growth rings
            float f = float(i) / float(n), px, up;
            tree_point(t, f, px, up);
            int sx, sy;
            place(px + sway * f * f, up, sx, sy);
            int w = t.kind == 2 ? 2 + (f < 0.4f ? 1 : 0) : (f < 0.5f ? 4 : 3);
            if (t.kind == 1 && f > 0.3f) continue;                      // a pine's trunk disappears behind its needles
            bool ring = (i % 5) == 0;
            u32 c = t.kind == 2 ? (i & 1 ? RGB(78, 66, 58) : RGB(64, 54, 48)) : ring ? RGB(88, 62, 38) : (i & 1 ? RGB(128, 92, 58) : RGB(116, 82, 50));
            g.rect(sx - w / 2, sy, w, 2, c);
            if (t.kind != 2) g.pset(sx - w / 2, sy, RGB(150, 112, 74));
        }
        float tpx, tup;
        tree_point(t, 1.0f, tpx, tup);
        int cx, cy;
        place(tpx + sway, tup, cx, cy);
        if (t.kind == 1) { // pine: stacked tiers of dark needles with snow on the ledges
            int ch = t.h * 3 / 4;
            for (int j = 0; j < ch; j++) {
                float u = float(j) / float(ch);
                int half = 1 + int(u * float(6 + t.h / 8) * (0.72f + 0.28f * float(j % 9) / 9.0f));
                int yy = int(t.y - float(t.h) + float(j)) - oy, xx = cx - int(t.lean * 0 + 0) + int(float(t.lean) * (1.0f - u) * 0.0f);
                xx = int(t.x + tpx * (1.0f - u * 0.0f) * (1.0f - u) * 0.0f) + int((tpx) * (1.0f - u)) - ox;
                g.rect(xx - half, yy, half * 2 + 1, 1, (j & 1) ? RGB(26, 78, 58) : RGB(34, 94, 68));
                if (j % 9 == 0) g.rect(xx - half, yy, std::max(2, half), 1, RGB(244, 248, 252));
            }
            continue;
        }
        if (t.kind == 2) { // bare trunk: a few broken branches
            for (int k = 0; k < 4; k++) {
                int by = cy + k * 5, sgn = (k & 1) ? 1 : -1;
                for (int s2 = 0; s2 < 7 - k; s2++) g.pset(cx + sgn * s2, by - s2 / 2, RGB(70, 58, 50));
            }
            continue;
        }
        // crown: drooping fronds with leaflets
        int flen = 14 + t.h / 7;
        for (int k = 0; k < 8; k++) {
            float phi = (-0.15f + 3.3f * float(k) / 7.0f) - th2; // from just below the horizon on the right, over the top, to the left
            float cp = cosf(phi), sp = sinf(phi);
            for (int s2 = 1; s2 <= flen; s2++) {
                float u = float(s2) / float(flen);
                int fx = cx + int(cp * float(s2)), fy = cy - int(sp * float(s2) * 0.75f) + int(u * u * float(flen) * 0.55f);
                u32 c = (k & 1) ? RGB(40, 120, 48) : RGB(62, 150, 58);
                g.pset(fx, fy, c);
                if (s2 > 3 && (s2 & 1)) { // leaflets hang down on both sides of the rib
                    g.pset(fx, fy + 1, RGB(34, 96, 40));
                    g.pset(fx + (cp > 0 ? 1 : -1), fy + 1, c);
                }
            }
        }
        g.rect(cx - 1, cy, 3, 3, RGB(104, 70, 40));  // coconuts under the crown
        g.rect(cx + 2, cy + 1, 2, 2, RGB(88, 58, 34));
    }
}
