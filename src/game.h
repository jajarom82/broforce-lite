// Broyoo - game logic + rendering. No SDL here so it can be tested headless.
#pragma once
#define GAME_NAME "BROFORCE LITE"
#define GAME_VERSION "0.4.6"
#include "gfx.h"
#include <string>
#include <vector>
#include <utility>

struct Input {
    bool left = false, right = false, up = false, down = false;
    bool jump = false, fire = false, special = false, melee = false;
    bool start = false, select = false;
};

struct SaveSlot {
    int used = 0;     // 1 once a game was started in this slot
    int mask = 0;     // bit n set = level n cleared
    int kills = 0;    // total kills across cleared levels
    int seconds = 0;  // total time across cleared levels
    unsigned broMask = 1; // bit n set = bro n freed (Rambro is always available)
};

struct Settings {
    SaveSlot slots[4];
    int lastSlot = 0;
    int shake = 100;  // percent, steps of 25
    int vib = 100;    // percent, steps of 25
    int sfx = 75;     // percent, steps of 25
    int music = 50;   // percent, steps of 25
    bool flip = true; // 180 degree flip of the final image (Miyoo Mini+ panel orientation)
};

constexpr int TS = 8; // tile size
constexpr int BW = 8, BH = 15; // bro / soldier hitbox
constexpr int NUM_WORLDS = 6, STAGES = 4;               // each world: 3 stages + a boss level
constexpr int NUM_LEVELS = NUM_WORLDS * STAGES, LAST_LEVEL = NUM_LEVELS - 1; // the last one is the Terrorkopter boss
const char *boss_name(int world);
const char *boss_hint(int world);

enum Tile : uint8_t { T_AIR, T_DIRT, T_STONE, T_BRICK, T_WOOD, T_METAL, T_BARREL, T_SPIKE, T_CRATE, T_SAND, T_WIRE, T_COUNT };

enum Prim { P_MG, P_ROCKET, P_FLAME, P_SHOTGUN, P_DUAL, P_DYNAMITE, P_KATANA, P_HOMING, P_RAY, P_SILENT, P_REVOLVER,
            P_MINIGUN, P_CHARGE, P_WHIP, P_FIST, P_BURST, P_BOOMERANG, P_CHAINSAW, P_SPEAR, P_CHAKRAM };
enum Spec { S_GRENADE, S_SWARM, S_FIREBOMB, S_AIRSTRIKE, S_FLASH, S_TURKEY, S_DASH, S_REMOTE, S_CLOAK, S_MOLOTOV,
            S_INVINC, S_LOCKON, S_GRAPPLE, S_LIGHTNING, S_ADRENALINE, S_JETPACK, S_BLAST, S_TELEPORT, S_SPEARS, S_WARCRY };

struct BroDef {
    const char *name;
    u32 skin, hair, acc, shirt, pants, extra, gun;
    int head;
    Prim prim;
    Spec spec;
    int specAmmo;
    const char *primName, *specName;
};
constexpr int NUM_BROS = 31;
extern const BroDef BROS[NUM_BROS];

struct Actor {
    float x = 0, y = 0, vx = 0, vy = 0; // x = center, y = feet
    int w = 6, h = 11;
    bool ground = false;
    int face = 1;
};

enum EType { E_MOOK, E_BOMBER, E_HEAVY, E_BOSS, E_OFFICER, E_DOG, E_RPG };
constexpr int E_TYPES = 7;

// how a victim died (the end-of-level recap replays it)
enum Cause { C_BULLET, C_EXPLOSION, C_FIRE, C_MELEE, C_CRUSH, C_SPIKE, C_LIGHTNING, C_GAS, C_FALL, C_COUNT };
struct KillRec { uint8_t type, cause; int8_t dir; };
struct Enemy {
    Actor a;
    int type = E_MOOK;
    int hp = 1, state = 0, timer = 0, cd = 0, stun = 0, alert = 0, anim = 0, flash = 0, dir = 1, laugh = 0, laughDelay = 0;
    int kind = 0, maxhp = 1, ph = 0, aux = 0; // boss: world it belongs to, health, phase, cooldown
    float fx = 0, fy = 0;                     // boss scratch (spotlight position)
    bool alive = true;
};

enum PKind { PK_BULLET, PK_EBULLET, PK_ROCKET, PK_GRENADE, PK_FLAME, PK_DYNA, PK_MOLOTOV, PK_HOMING, PK_RAY, PK_MISSILE,
             PK_REMOTE, PK_TURKEY, PK_FIREBOMB, PK_REVBULLET, PK_PELLET, PK_BOOMERANG, PK_EROCKET, PK_SPEAR };
struct Proj {
    float x = 0, y = 0, vx = 0, vy = 0;
    int kind = PK_BULLET, life = 0, dmg = 1, pierce = 0, bounces = 0, target = -1, stuck = 0;
    float ang = 0;
};

struct Particle {
    float x, y, vx, vy;
    int life, maxlife;
    u32 col;
    uint8_t kind; // 0 debris, 1 gore, 2 spark, 3 smoke, 4 fire
};

struct ChunkTile { int dx, dy; uint8_t type, var; };
struct Chunk { std::vector<ChunkTile> t; float x, y, vy; int w, h; }; // x,y = top-left pixel of the bounding box

struct SndEvt { int id; float vol; };

// end-of-level extraction: helicopter with a rope ladder, then the results panel
struct Exfil {
    int phase = 0; // 0 none, 1 helicopter inbound, 2 ladder down, 3 bro on the ladder, then the recap
    float x = 0, y = 0, vx = 0, vy = 0, hoverX = 0, hoverY = 0, groundY = 0, ladderLen = 0, grabOff = 0, vol = 0; // grabOff: where on the ladder the bro caught it
    int t = 0, t3 = 0, resT = 0, doneT = 0, finalTicks = 0, interval = 31;
    bool done = false, recap = false; // recap = the helicopter has left the screen, the results are showing
};

struct Boom { float x, y; int t, maxt; float r; };

enum PropType { PR_CAGE, PR_AMMO, PR_FLAG, PR_HELI, PR_FREED };
struct Prop {
    int type = PR_AMMO;
    Actor a;
    int hp = 3, timer = 0;
    int bro = -1;         // cage / freed prisoner: the bro he is (-1 = anonymous prisoner)
    bool hidden = false;  // hidden in a secret chamber
    bool used = false;
};

// explosive gas bottle: stands around, then when heated shoots a jet of fire out of its cap and takes off
struct Gas {
    Actor a;                 // a.y = bottom of the (standing) bottle
    int state = 0;           // 0 idle, 1 heating, 2 flying
    int timer = 0, t = 0, fuse = 0;
    float dx = 1, dy = 0, speed = 0, mdx = 0, mdy = 0;
    bool alive = true;
    float top() const { return state == 2 ? a.y - 9 : a.y - 13; }
    float bottom() const { return state == 2 ? a.y - 3 : a.y; }
    float halfW() const { return state == 2 ? 7.0f : 3.0f; }
};

struct Player {
    Actor a;
    bool wired = false; // tangled in barbed wire
    int bro = 0, lives = 4, spec = 0, cool = 0, invuln = 0, cloak = 0, dashT = 0, grappleT = 0;
    float gx = 0, gy = 0;
    int meleeT = 0, punchT = 0, anim = 0, charge = 0, spin = 0, revAmmo = 6, reload = 0, swarm = 0, swarmT = 0;
    int lockLeft = 0, lockT = 0, burst = 0, adren = 0, jet = 0;
    int coyote = 0, jumpBuf = 0, wall = 0, wallT = 0, wallSide = 0, whipT = 0, whipLen = 0, whipDir = 0;
    bool alive = false, remote = false, ride = false;
    int deadT = 0;
};

struct Theme {
    u32 skyTop, skyBot, hill1, hill2, dirt, dirtDk, grass, stone, brick, brickDk, wood, woodDk, metal, metalDk;
    bool night;
    const char *name;
};

enum GState { GS_TITLE, GS_PLAY, GS_PAUSE, GS_OPTIONS, GS_CLEAR, GS_OVER, GS_VICTORY, GS_SLOTS, GS_MISSIONS, GS_NEWBRO, GS_ROSTER };

class Game {
public:
    Game();
    void update(const Input &in);
    void render(Gfx &g);
    float rumble_level() const; // 0..1 after applying the vibration setting
    bool want_quit = false;
    bool settings_dirty = false;
    Settings st;

    // test hooks
    void debug_start_level(int lvl, int bro = -1);
    Player &player() { return pl; }
    int level_w() const { return W; }
    int cam_x() const { return int(camX); }
    void debug_teleport(float x, float y);
    int enemies_alive() const;
    GState state() const { return gs; }
    int stat_kills = 0;
    int stat_explosions = 0;
    int shake_x() const { return shakeX; }
    int shake_y() const { return shakeY; }
    float get_trauma() const { return trauma; }
    uint8_t debug_tile(int x, int y) const { return tget(x, y); }
    void debug_explode(float x, float y, float r, float p, bool friendly = true) { explode(x, y, r, p, friendly); process_explosions(); }
    int ground_row(int tx) const { for (int y = 0; y < H; y++) if (solid_t(tx, y)) return y; return H; }
    int progress() const { return unlocked_of(st.slots[curSlot]); }
    void set_progress(int u) { // legacy single-slot save files
        u = u < 0 ? 0 : u > LAST_LEVEL ? LAST_LEVEL : u;
        st.slots[0].used = u > 0; for (int i = 0; i < u; i++) st.slots[0].mask |= 1 << i;
    }
    int unlocked_of(const SaveSlot &s) const { if (debugMode) return LAST_LEVEL; int n = 0; while (n < LAST_LEVEL && (s.mask & (1 << n))) n++; return n; }
    // each bro (but Rambro) is hidden in one level; a level counts as 100% only when it is cleared AND all its bros are freed
    static int bro_home_level(int b) { return b <= 0 ? -1 : (b - 1) * NUM_LEVELS / (NUM_BROS - 1); }
    int level_bro_total(int lvl) const { int n = 0; for (int b = 1; b < NUM_BROS; b++) if (bro_home_level(b) == lvl) n++; return n; }
    int level_bro_owned(const SaveSlot &s, int lvl) const { int n = 0; for (int b = 1; b < NUM_BROS; b++) if (bro_home_level(b) == lvl && ((s.broMask >> b) & 1u)) n++; return n; }
    bool level_full(const SaveSlot &s, int lvl) const { return (s.mask & (1 << lvl)) && level_bro_owned(s, lvl) == level_bro_total(lvl); }
    int completion(const SaveSlot &s) const {
        int half = 0; // in half-levels: cleared = 1, cleared with every bro freed = 2
        for (int i = 0; i < NUM_LEVELS; i++) if (s.mask & (1 << i)) half += level_full(s, i) ? 2 : 1;
        return half * 100 / (2 * NUM_LEVELS);
    }
    int owned_count(const SaveSlot &s) const { int n = 0; for (int b = 0; b < NUM_BROS; b++) if (b == 0 || ((s.broMask >> b) & 1u)) n++; return n; }
    bool owned(int b) const { return debugMode || b == 0 || ((st.slots[curSlot].broMask >> b) & 1u); }
    void set_debug(bool d) { debugMode = d; }
    bool debug_mode() const { return debugMode; }
    int new_bro_now() const { return newBro; }
    int queue_size() const { return int(queue.size()); }
    int debug_cage_bro(int i) const { int k = 0; for (const Prop &p : props) if (p.type == PR_CAGE) { if (k == i) return p.bro; k++; } return -2; }
    int debug_cage_count() const { int k = 0; for (const Prop &p : props) if (p.type == PR_CAGE) k++; return k; }
    bool debug_cage_pos(int i, float &x, float &y) const { int k = 0; for (const Prop &p : props) if (p.type == PR_CAGE) { if (k == i) { x = p.a.x; y = p.a.y; return true; } k++; } return false; }
    void debug_set_slot(int s) { curSlot = s; }
    void set_state(GState s) { gs = s; }
    size_t proj_count() const { return pr.size(); }
    bool debug_proj(size_t i, float &vx, float &vy, int &kind) const {
        if (i >= pr.size()) return false;
        vx = pr[i].vx; vy = pr[i].vy; kind = pr[i].kind;
        return true;
    }
    int debug_gas_count() const { return int(gas.size()); }
    bool debug_gas(int i, float &x, float &y, int &state) const {
        if (i < 0 || i >= int(gas.size())) return false;
        x = gas[size_t(i)].a.x; y = gas[size_t(i)].a.y; state = gas[size_t(i)].state;
        return true;
    }
    void debug_clear_enemies() { en.clear(); bossIdx = -1; gas.clear(); riding = -1; }
    void debug_add_gas(float x, float y) { add_gas(x, y); }
    void debug_ignite_gas(int i) { ignite_gas(i, 10); }
    void debug_ignite_tile(int x, int y) { ignite_tile(x, y); }
    bool debug_burning(int x, int y) const { return x >= 0 && x < W && y >= 0 && y < H && fireT[y * MAXW + x] > 0; }
    int debug_burn_count() const { return int(burnList.size()); }
    long debug_blood_total() const { long s = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) s += blood[y * MAXW + x]; return s; }
    int intro_t() const { return introT; }
    int cur_slot() const { return curSlot; }
    bool exfil_recap() const { return ex.recap; }
    void debug_begin(int lvl) { start_slot_game(lvl); }
    void debug_finish_level() { finish_level(); }
    int debug_add_enemy(int type, float x, float y, int face = -1) { add_mook(x, y, type); en.back().dir = face; en.back().a.face = face; return int(en.size()) - 1; }
    void debug_push_kill(int type, int cause, int dir) { killLog.push_back({uint8_t(type), uint8_t(cause), int8_t(dir)}); }
    void debug_start_recap(int interval) { ex = Exfil(); ex.phase = 3; ex.recap = true; ex.interval = interval; ex.finalTicks = 3790; gs = GS_PLAY; introT = 0; }
    int exfil_resT() const { return ex.resT; }
    int exfil_phase() const { return ex.phase; }
    const Sprite &debug_face(int b) const { return broFace[b]; }
    float heli_volume() const { return (ex.phase > 0 && gs == GS_PLAY) ? ex.vol : 0.0f; } // rotor loop level 0..1
    bool exfil_done() const { return ex.done; }
    int kill_log_size() const { return int(killLog.size()); }
    bool debug_ladder(float &x, float &y) const { x = ex.x; y = ex.y + 5 + ex.ladderLen; return ex.phase > 0; }
    int officer_type() const { return E_OFFICER; }
    int laughing_count() const { int n = 0; for (const Enemy &e : en) if (e.alive && e.laugh > 0) n++; return n; }
    int tick_count() const { return tick; }
    int chunk_count() const { return int(chunks.size()); }
    bool debug_chunk(int i, float &x, float &y, int &w, int &h, int &n) const { if (i < 0 || i >= int(chunks.size())) return false; const Chunk &c = chunks[size_t(i)]; x = c.x; y = c.y; w = c.w; h = c.h; n = int(c.t.size()); return true; }
    std::vector<SndEvt> sounds;          // drained by the front-end each frame
    int music_track() const;             // -1 = silence, 0..3 levels, 4 = title/menus
    bool debug_prop(int type, float &x, float &y) const {
        for (const Prop &p : props) if (p.type == type && !p.used) { x = p.a.x; y = p.a.y; return true; }
        return false;
    }
    int ammo_props() const { int n = 0; for (const Prop &p : props) if (p.type == PR_AMMO && !p.used) n++; return n; }
    void debug_set_tile(int x, int y, uint8_t t) { set_tile(x, y, t); }
    int debug_enemy_count() const { return int(en.size()); }
    bool debug_enemy(int i, float &x, float &y, int &type) const {
        if (i < 0 || i >= int(en.size())) return false;
        x = en[size_t(i)].a.x; y = en[size_t(i)].a.y; type = en[size_t(i)].type;
        return true;
    }
    void debug_damage_tile(int x, int y, int dmg) { damage_tile(x, y, dmg); }
    int debug_hidden_cages() const { int n = 0; for (const Prop &p : props) if (p.type == PR_CAGE && p.hidden) n++; return n; }
    bool debug_hidden_cage_pos(float &x, float &y) const { for (const Prop &p : props) if (p.type == PR_CAGE && p.hidden) { x = p.a.x; y = p.a.y; return true; } return false; }
    int debug_surf(int x) const { return surf[x]; }
    int debug_pillar(int x, int y) const { return pillar_at(x, y) ? pillar[y * MAXW + x] : 0; }
    int debug_pillar_total() const { int n = 0; for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) if (pillar[y * MAXW + x]) n++; return n; }
    void debug_add_pillar(int x, int y0, int y1) { add_pillar(x, y0, y1); }
    int boss_hp() const { return bossIdx >= 0 ? en[size_t(bossIdx)].hp : 0; }
    int death_cause_now() const { return deathCause; }
    void seed_random(uint32_t s) { seedBase = s | 1u; }
    int world_now() const { return world; }
    int stage_now() const { return stage; }
    bool boss_level() const { return stage == STAGES - 1; }
    // boss test hooks
    bool debug_boss(float &x, float &y, int &hp, int &ph, int &kind) const {
        if (bossIdx < 0) return false;
        const Enemy &e = en[size_t(bossIdx)];
        x = e.a.x; y = e.a.y; hp = e.hp; ph = e.ph; kind = e.kind;
        return true;
    }
    void debug_boss_set(int ph, int timer, float x = -1) { if (bossIdx < 0) return; Enemy &e = en[size_t(bossIdx)]; e.state = 1; e.ph = ph; e.timer = timer; if (x >= 0) e.a.x = x; }
    int debug_boss_hurt(int dmg, int cause) { if (bossIdx < 0) return 0; int h0 = en[size_t(bossIdx)].hp; hurt_enemy(bossIdx, dmg, 1.0f, cause); return h0 - en[size_t(bossIdx)].hp; }
    void debug_boss_fx(float fx) { if (bossIdx >= 0) en[size_t(bossIdx)].fx = fx; }
    void debug_strike(float x) { Strike s; s.x = x; s.t = 0; s.left = 4; s.hostile = true; strikes.push_back(s); }
    void debug_barrage(int n) { boss_barrage(n); }

private:
    // world
    static constexpr int MAXW = 320, MAXH = 64;
    int W = 0, H = 0;
    uint8_t tile[MAXW * MAXH];
    uint8_t thp[MAXW * MAXH];
    uint8_t tvar[MAXW * MAXH];
    int surf[MAXW]; // original ground row per column (cave/crater backdrop)
    uint8_t pillar[MAXW * MAXH]; // support pillars (background plane): hp, 0 = none. Only explosions damage them
    struct Crumble { int x, y, t; };
    std::vector<Crumble> crumble; // pillar cells waiting to be checked for support (they crumble one after another)
    struct Mount { int cx, hw, h; };
    std::vector<Mount> mounts;
    uint16_t fireT[MAXW * MAXH]; // burning countdown per tile (0 = not burning)
    uint8_t blood[MAXW * MAXH];  // blood stains on tiles
    std::vector<int> burnList;
    std::vector<Gas> gas;
    int riding = -1;             // index of the gas bottle the bro is standing on
    int level = 0, world = 0, stage = 0;
    uint32_t seedBase = 7;
    int bossBannerT = 0, bossWarnT = 0;
    Theme th;
    uint32_t rngs = 1;
    float rnd();                      // 0..1
    int rndi(int a, int b);           // inclusive
    uint8_t &T(int x, int y) { return tile[y * MAXW + x]; }
    uint8_t tget(int x, int y) const;
    bool solid_t(int tx, int ty) const;
    bool solid_px(float x, float y) const { return solid_t(int(x >= 0 ? x / TS : -1), int(y >= 0 ? y / TS : -1) ); }
    bool box_hits(float cx, float feet, int w, int h) const;
    void gen_level(int lvl);
    void stamp_bunker(int x0, int g, int w, int hh);
    void stamp_tower(int x0, int g);
    void stamp_cage(int x0, int g);
    void stamp_fort(int x0, int g, int w);
    void gen_heights(int *hgt);
    void carve_chambers(const int *hgt, bool *used);
    void place_cage(int x0, int g, bool hidden);
    void add_pillar(int x, int y0, int y1);
    void damage_pillar(int tx, int ty, int dmg);
    void update_pillars();
    void sand_fall(int tx, int ty);
    bool pillar_at(int x, int y) const { return x >= 0 && x < W && y >= 0 && y < H && pillar[y * MAXW + x] > 0; }
    void stamp_shelter(int x0, int g);
    void stamp_canopy(int x0, int g);
    void build_boss_arena(int wd);
    void spawn_boss(int wd);
    void update_boss(size_t idx);
    int boss_damage(Enemy &e, int dmg, int cause);
    void boss_barrage(int n);
    void draw_boss(Gfx &g, const Enemy &e, int sx, int sy);
    void draw_boss_fx(Gfx &g, int ox, int oy);
    void set_tile(int x, int y, uint8_t t);
    void add_mook(float x, float y, int type);
    void add_prop(int type, float x, float y);
    int tile_max_hp(uint8_t t) const;

    // entities
    Player pl;
    std::vector<Enemy> en;
    std::vector<Proj> pr;
    std::vector<Particle> pa;
    std::vector<Boom> booms;
    std::vector<Prop> props;
    struct PendExpl { float x, y, r, power; bool friendly; int delay = 0; bool shell = false; }; // shell: fired by an enemy/boss, never hurts bosses
    std::vector<PendExpl> pend;
    struct Strike { float x; int t; int left; bool hostile = false; };
    std::vector<Strike> strikes;
    std::vector<Chunk> chunks;
    Exfil ex;
    std::vector<KillRec> killLog;
    int playTicks = 0;
    void start_exfil(float hoverX, float groundY);
    void update_exfil();
    void finish_level();
    void trigger_laughs();
    void draw_exfil_world(Gfx &g, int ox, int oy);
    void draw_exfil_ui(Gfx &g);
    void draw_victim(Gfx &g, const KillRec &k, int age, int cx, int base, bool small);
    void draw_overlays(Gfx &g);
    void draw_slots(Gfx &g);
    void draw_missions(Gfx &g);
    void start_slot_game(int level);
    std::vector<std::pair<int, int>> collapseQ;
    int anchorStamp[MAXW * MAXH];
    int frameId = 0;
    void process_collapse();
    bool flammable(uint8_t t) const;
    void ignite_tile(int tx, int ty, int fuse = 0);
    void update_fire();
    void add_gas(float x, float y);
    void ignite_gas(int i, int delay);
    void update_gas();
    void stain(int tx, int ty, int amt);
    void gibs(float x, float y, int n, float dirx);
    void draw_gas(Gfx &g, int ox, int oy);
    void update_chunks();
    void snd(int id, float x, float y, float vol = 1.0f);
    std::vector<int> queue;      // upcoming bros (one per spare life)
    int newBro = -1, rosterSel = 0;
    bool debugMode = false;
    Sprite captiveSpr;           // the bagged, tied-up prisoner
    void assign_cage_bros();
    int pick_owned(int avoid = -1);
    void draw_newbro(Gfx &g);
    void draw_roster(Gfx &g);
    float checkX = 0, checkY = 0;
    int bossIdx = -1;
    bool bossDead = false;
    int tick = 0;
    int flashScreen = 0;
    int deathCause = 0;

    // camera
    float camX = 0, camY = 0;
    float trauma = 0, rumble = 0;
    int shakeX = 0, shakeY = 0;

    // state
    GState gs = GS_TITLE;
    GState menuFrom = GS_TITLE;
    int menuSel = 0;
    int curSlot = 0;
    int introT = 0, introPhrase = 0, failPhrase = 0;
    bool eraseArm = false;
    uint32_t uiSeed = 12345;
    int rnd_ui(int n) { uiSeed = uiSeed * 1664525u + 1013904223u + uint32_t(stateT) * 7919u; return int((uiSeed >> 8) % uint32_t(n)); }
    int lvl_track() const { static const int m[NUM_WORLDS] = {0, 1, 2, 1, 2, 3}; return m[world % NUM_WORLDS]; }
    int stateT = 0;
    int bannerT = 0;
    Input prev;
    bool pressed(bool cur, bool old) const { return cur && !old; }

    // gameplay
    void start_level(int lvl);
    void spawn_player(bool first);
    void next_bro();
    void swap_bro(int b);
    void kill_player(int cause, float dirx);
    void update_player(const Input &in);
    void update_enemies();
    void update_projs();
    void update_particles();
    void update_props();
    void fire_primary(const Input &in);
    void use_special();
    void do_melee(float reach, int dmg, bool dig);
    void spawn_bullet(int kind, float x, float y, float ang, float speed, int life, int dmg, bool friendly);
    void add_proj(const Proj &p);
    void explode(float x, float y, float r, float power, bool friendly, bool shell = false);
    void process_explosions();
    void hurt_enemy(int i, int dmg, float dirx, int cause = C_BULLET);
    void kill_enemy(int i, float dirx, int cause = C_BULLET);
    void damage_tile(int tx, int ty, int dmg, bool silent = false);
    void gore(float x, float y, int n, float dirx, u32 col);
    void debris(float x, float y, int n, u32 col, float speed = 1.5f);
    void add_trauma(float x, float y, float power);
    void move_actor(Actor &a, float dx, float dy, bool step, bool *hitx = nullptr);
    bool los(float x0, float y0, float x1, float y1) const;
    int nearest_enemy(float x, float y, float maxd, int exclude = -1) const;
    void open_cage(Prop &c);
    void level_clear();
    bool in_view(float x, float y, float mx = 40) const;

    // menus
    void update_menu(const Input &in);
    void menu_activate();
    std::string options_line(int i) const;

    // render
    void build_tiles();
    void draw_tile(Gfx &g, int tx, int ty, int sx, int sy);
    void draw_bg(Gfx &g);
    void draw_saber(Gfx &g, const Player &p, int sx, int sy);
    void draw_punch(Gfx &g, const Player &p, int sx, int sy);
    void draw_actor_bro(Gfx &g, const Actor &a, int bro, int anim, bool air, bool flash, bool ghost);
    void draw_enemy(Gfx &g, const Enemy &e, int sx, int sy);
    void draw_hud(Gfx &g);
    void draw_menu(Gfx &g);
    void draw_title(Gfx &g);
    void draw_heli(Gfx &g, int sx, int sy, bool big);
    void build_sprites();

    Sprite tileSpr[T_COUNT][4];
    Sprite broSpr[NUM_BROS][4];
    Sprite broFace[NUM_BROS]; // 24x24 HUD portraits
    Sprite enSpr[E_TYPES][4];
    bool spritesBuilt = false;
};
