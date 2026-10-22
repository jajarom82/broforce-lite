// Broyoo - static data: bro roster, themes, procedural pixel-art sprites (all original art)
#include "game.h"
#include <cstring>

#define C(r, g, b) RGB(r, g, b)

const BroDef BROS[NUM_BROS] = {
    // name              skin           hair           accessory      shirt          pants          extra          gun            head prim        spec         ammo
    {"RAMBRO",          C(235,185,135), C(40,30,25),   C(215,30,30),  C(235,185,135), C(70,90,50),   C(90,60,30),   C(60,60,60),   0, P_MG,       S_GRENADE,   6, "MACHINE GUN", "GRENADE"},
    {"BROMMANDO",       C(235,185,135), C(60,40,20),   C(60,110,50),  C(80,100,50),   C(60,80,40),   C(30,30,30),   C(70,110,70),  2, P_ROCKET,   S_SWARM,     3, "ROCKETS", "MISSILE SWARM"},
    {"B.A. BRORACUS",   C(130,85,55),  C(200,200,200), C(210,210,210),C(90,90,100),   C(60,70,110),  C(240,200,40), C(230,120,30), 3, P_FLAME,    S_FIREBOMB,  3, "FLAMETHROWER", "FIRE BOMB"},
    {"BRODELL WALKER",  C(235,185,135), C(110,80,40),  C(140,90,40),  C(60,100,170),  C(50,70,130),  C(210,170,60), C(90,60,30),   1, P_SHOTGUN,  S_AIRSTRIKE, 3, "SHOTGUN", "AIRSTRIKE"},
    {"BRO HARD",        C(235,185,135), C(60,40,30),   C(235,185,135),C(235,235,235), C(70,70,90),   C(200,200,200),C(50,50,50),   4, P_DUAL,     S_FLASH,     3, "DUAL PISTOLS", "FLASHBANG"},
    {"MACBROVER",       C(235,185,135), C(150,110,50), C(150,110,50), C(180,50,40),   C(60,90,140),  C(230,230,230),C(200,40,40),  5, P_DYNAMITE, S_TURKEY,    3, "DYNAMITE", "BOMB TURKEY"},
    {"BLADE BRO",       C(225,200,175), C(20,20,25),   C(20,20,25),   C(30,30,40),    C(30,30,40),   C(150,0,0),    C(200,210,220),5, P_KATANA,   S_DASH,      3, "KATANA", "DASH STRIKE"},
    {"BRO DREDD",       C(235,185,135), C(60,50,40),   C(210,190,50), C(40,40,120),   C(40,40,70),   C(220,200,60), C(60,60,70),   2, P_HOMING,   S_REMOTE,    3, "SEEKING SHOT", "GUIDED ROCKET"},
    {"BRO IN BLACK",    C(235,185,135), C(30,25,20),   C(20,20,20),   C(25,25,30),    C(25,25,30),   C(240,240,240),C(190,200,210),6, P_RAY,      S_FLASH,     3, "BLASTER", "NEURALYZER"},
    {"SNAKE BROSKIN",   C(220,175,130), C(50,40,30),   C(30,30,30),   C(90,100,70),   C(60,70,50),   C(30,30,30),   C(40,40,40),   0, P_SILENT,   S_CLOAK,     3, "SILENT SMG", "CLOAK"},
    {"DIRTY BRORY",     C(225,175,125), C(80,60,40),   C(90,70,50),   C(140,100,60),  C(110,80,50),  C(200,170,100),C(170,170,170),1, P_REVOLVER, S_MOLOTOV,   3, "REVOLVER", "MOLOTOV"},
    {"BROMINATOR",      C(170,175,185), C(170,175,185),C(170,175,185),C(60,60,65),    C(50,50,55),   C(220,30,30),  C(80,80,90),   4, P_MINIGUN,  S_INVINC,    1, "MINIGUN", "INVINCIBLE"},
    {"BROBOCOP",        C(235,185,135), C(60,60,60),   C(150,160,175),C(80,100,150),  C(70,90,140),  C(200,200,210),C(100,100,110),2, P_CHARGE,   S_LOCKON,    3, "CHARGE SHOT", "TARGET LOCK"},
    {"INDIANNA BRONES", C(235,185,135), C(70,45,25),   C(130,90,45),  C(140,95,50),   C(110,90,60),  C(90,60,30),   C(110,70,30),  1, P_WHIP,     S_GRAPPLE,   4, "WHIP", "GRAPPLE"},
    {"BROLANDER",       C(235,185,135), C(90,60,30),   C(90,60,30),   C(170,40,40),   C(60,50,40),   C(220,200,160),C(200,210,220),5, P_KATANA,   S_LIGHTNING, 3, "BROADSWORD", "LIGHTNING"},
    {"BRO LEE",         C(235,190,140), C(25,20,20),   C(240,210,50), C(240,210,50),  C(240,210,50), C(30,30,30),   C(240,210,50), 0, P_FIST,     S_DASH,      3, "KUNG FU", "FLYING KICK"},
    {"ELLEN RIPBRO",    C(235,185,135), C(110,80,50),  C(80,60,40),   C(90,95,100),   C(70,75,60),   C(200,200,60), C(90,90,100),  0, P_BURST,    S_FIREBOMB,  3, "PULSE RIFLE", "INCINERATOR"},
    {"BRODATOR",        C(120,140,70),  C(60,50,30),   C(60,50,30),   C(90,100,70),   C(80,90,60),   C(200,200,210),C(170,70,210), 3, P_RAY,      S_SPEARS,    3, "PLASMA CASTER", "CLOAK+SPEARS"},
    {"BROCHETE",        C(190,140,100), C(25,20,20),   C(30,30,30),   C(150,40,40),   C(50,50,60),   C(200,200,200),C(200,205,215),0, P_BOOMERANG,S_LOCKON,    3, "MACHETE THROW", "MACHETE STORM"},
    {"CHEV BROLIOS",    C(235,185,135), C(60,45,30),   C(30,30,30),   C(150,180,200), C(150,180,200),C(120,150,170),C(60,60,60),   6, P_MG,       S_ADRENALINE,2, "AUTO PISTOL", "ADRENALINE"},
    {"BROCKETEER",      C(235,185,135), C(60,40,30),   C(200,150,40), C(130,80,40),   C(110,70,40),   C(230,60,40),  C(80,80,90),   2, P_MG,       S_JETPACK,   3, "MAUSER", "ROCKET PACK"},
    {"ASH BROLLIAMS",   C(235,185,135), C(90,60,35),   C(90,60,35),   C(60,90,170),   C(80,70,60),   C(200,200,200),C(210,60,40),  5, P_CHAINSAW, S_BLAST,     3, "CHAINSAW", "BOOMSTICK"},
    {"DOUBLE BRO SEVEN",C(235,185,135), C(80,60,40),   C(80,60,40),   C(30,30,40),    C(30,30,40),   C(240,240,240),C(70,70,80),   5, P_SILENT,   S_GRENADE,   3, "SILENCED PPK", "EXPLODING PEN"},
    {"BRO CAESAR",      C(225,175,125), C(70,50,30),   C(210,180,60), C(200,45,45),   C(200,170,60),  C(230,200,70), C(130,95,55),  2, P_SPEAR,    S_DASH,      3, "THROWING SPEAR", "CHARGE!"},
    {"BRONDLE FLY",     C(150,100,70),  C(30,25,20),   C(60,90,200),  C(70,130,80),   C(60,70,100),   C(240,230,120),C(70,70,75),   1, P_MG,       S_TURKEY,    3, "BERETTA", "BANANA BOMB"},
    {"TELEBROPORTER",   C(235,185,135), C(60,50,40),   C(40,170,225), C(235,235,245), C(60,60,85),    C(120,200,240),C(100,110,130),6, P_BURST,    S_TELEPORT,  4, "LASER BURST", "TELEPORT"},
    {"BRO MAX",         C(215,165,120), C(80,55,35),   C(80,55,35),   C(55,45,45),    C(65,55,45),    C(200,55,45),  C(90,90,95),   5, P_SHOTGUN,  S_ADRENALINE,2, "SAWED-OFF", "V8 RAGE"},
    {"THE BROFESSIONAL",C(235,185,135), C(50,40,40),   C(40,40,55),   C(60,60,72),    C(50,50,62),    C(150,160,175),C(40,40,45),   0, P_DUAL,     S_LOCKON,    3, "TWIN PISTOLS", "COLD BLOOD"},
    {"XENA",            C(235,185,135), C(40,28,22),   C(190,140,50), C(176,128,50),  C(120,70,40),   C(220,200,120),C(190,190,200),5, P_CHAKRAM,  S_WARCRY,    3, "CHAKRAM", "WAR CRY"},
    {"BRO WICK",        C(235,185,135), C(35,28,25),   C(35,35,45),   C(28,28,34),    C(28,28,34),    C(240,240,240),C(20,20,24),   5, P_DUAL,     S_DASH,      3, "DUAL PISTOLS", "TACTICAL ROLL"},
    {"ROCKY BROLBOA",   C(225,170,125), C(40,30,25),   C(200,30,30),  C(200,30,30),   C(240,240,240), C(240,200,40), C(200,30,30),  0, P_FIST,     S_ADRENALINE,2, "BOXING GLOVES", "EYE OF THE TIGER"},
};

static const Theme THEMES[NUM_WORLDS] = {
    {C(110,180,230), C(210,240,250), C(70,140,90),  C(50,110,70),  C(120,80,50),  C(90,58,36),  C(80,170,60),  C(130,130,135), C(170,90,70),  C(120,60,50),  C(160,115,60), C(110,75,35), C(110,120,135), C(70,78,90),  false, "JUNGLE"},
    {C(240,170,100), C(250,230,170), C(200,140,90), C(170,110,70), C(210,170,110), C(170,130,80), C(205,185,95),  C(150,130,110), C(205,150,100), C(160,110,70),  C(160,115,60), C(110,75,35), C(120,120,130), C(80,80,90),  false, "DESERT"},
    {C(15,15,45),    C(70,45,100),   C(30,30,70),   C(22,22,55),   C(80,68,80),    C(52,42,54),  C(60,110,90),  C(95,95,115),   C(120,65,75),  C(80,40,50),   C(140,100,60), C(90,60,30),  C(140,150,170), C(90,96,115), true,  "NIGHT BASE"},
    {C(150,190,230), C(235,245,255), C(200,215,235), C(170,190,215), C(225,235,245), C(170,190,215), C(255,255,255), C(130,140,160), C(160,200,230), C(110,150,190), C(150,110,70),  C(110,75,35), C(110,125,145), C(75,90,110),  false, "ARCTIC"},
    {C(110,100,115), C(210,170,140), C(80,75,90),   C(60,56,70),   C(112,106,100), C(76,72,68),   C(135,128,115), C(120,120,126), C(150,85,70),  C(105,55,45),  C(130,95,55),  C(90,62,35),  C(100,110,122), C(66,74,86),  false, "CITY RUINS"},
    {C(60,0,0),      C(210,70,20),   C(100,25,10),  C(70,12,10),   C(95,42,32),    C(62,26,20),  C(235,95,20),  C(105,75,75),   C(145,52,42),  C(95,35,30),   C(120,80,50),  C(80,50,30),  C(100,100,110), C(65,65,75),  false, "HELL"},
};

const Theme &theme_for(int lvl) { return THEMES[(lvl / STAGES) % NUM_WORLDS]; }

static uint32_t hash2(int x, int y, int s) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(s) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static u32 shade(u32 c, int d) {
    int r = int((c >> 16) & 255) + d, g = int((c >> 8) & 255) + d, b = int(c & 255) + d;
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    return RGB(r, g, b);
}

static Sprite outline(const Sprite &s, u32 col) {
    Sprite o;
    o.w = s.w + 2;
    o.h = s.h + 2;
    o.d.assign(size_t(o.w) * size_t(o.h), 0);
    for (int y = 0; y < s.h; y++)
        for (int x = 0; x < s.w; x++)
            if (s.at(x, y)) {
                static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    size_t i = size_t(y + 1 + dy[k]) * size_t(o.w) + size_t(x + 1 + dx[k]);
                    if (!o.d[i]) o.d[i] = col;
                }
            }
    for (int y = 0; y < s.h; y++)
        for (int x = 0; x < s.w; x++)
            if (s.at(x, y)) o.d[size_t(y + 1) * size_t(o.w) + size_t(x + 1)] = s.at(x, y);
    return o;
}

// 12x12 character, facing right. 5 head rows (per head variant) + 7 body rows (per frame).
static const char *HEADS[9][5] = {
    {"....rrrr....", "..hhhhhhh...", "....sssse...", "....sssss...", ".....ss....."}, // headband
    {"....hhhh....", "..hhhhhhhh..", "....sssse...", "....sssss...", ".....ss....."}, // hat
    {"....hhhh....", "...hhhhhh...", "...hssssse..", "....sssss...", ".....ss....."}, // helmet
    {".....hh.....", "....hhhh....", "....sssse...", "....sssss...", ".....ss....."}, // mohawk
    {"....ssss....", "....ssss....", "....sssse...", "....sssss...", ".....ss....."}, // bald
    {"...rrrrr....", "..rrrrrrr...", "..rrsssse...", "..rrsssss...", "..rr.ss....."}, // long hair
    {"....rrrr....", "....ssss....", "....hhhhh...", "....sssss...", ".....ss....."}, // shades
    {"...hhhhhh...", "..hhxxxxhhh.", "....sssse...", "....sssss...", ".....ss....."}, // officer peaked cap
    {"....hhhh....", "...hhhhhh...", "...hhhhhh...", "...hxxxxh...", ".....ss....."}, // sack over the head, tied with a rope
};
static const char *BODIES[4][7] = {
    {"...bbbbbb...", "..sbbbbbggg.", "..sbbbbbbg..", "...xxxxxx...", "...pppppp...", "...pp..pp...", "..kkk..kkk.."}, // idle
    {"...bbbbbb...", "..sbbbbbggg.", "..sbbbbbbg..", "...xxxxxx...", "...pppppp...", "..pp....pp..", ".kkk....kkk."}, // run1
    {"...bbbbbb...", "..sbbbbbggg.", "..sbbbbbbg..", "...xxxxxx...", "...pppppp...", "....pppp....", "....kkkk...."}, // run2
    {"...bbbbbb...", "..sbbbbbggg.", "..sbbbbbbg..", "...xxxxxx...", "...pppppp...", "..pp..pp....", "..kk..kk...."}, // jump
};

static Sprite build_char(int head, u32 skin, u32 hair, u32 acc, u32 shirt, u32 pants, u32 extra, u32 gun, int frame) {
    const char *rows[12];
    for (int i = 0; i < 5; i++) rows[i] = HEADS[head][i];
    for (int i = 0; i < 7; i++) rows[5 + i] = BODIES[frame][i];
    PalEntry pal[] = {{'s', skin}, {'r', hair}, {'h', acc}, {'b', shirt}, {'p', pants}, {'x', extra},
                      {'g', gun}, {'k', RGB(35, 30, 30)}, {'e', RGB(15, 15, 20)}};
    Sprite small = make_sprite(rows, 12, 12, pal, 9);
    // upscale 12x12 -> 16x16 (nearest neighbour, 3 of every 4 source pixels kept once, 1 doubled)
    Sprite big;
    big.w = big.h = 16;
    big.d.assign(256, 0);
    for (int y = 0; y < 16; y++)
        for (int x = 0; x < 16; x++) big.d[size_t(y * 16 + x)] = small.at(x * 12 / 16, y * 12 / 16);
    return outline(big, RGB(20, 14, 16));
}

static Sprite build_dog(int frame) {
    Sprite s;
    s.w = 16; s.h = 10;
    s.d.assign(160, 0);
    u32 b = C(150, 100, 55), dk = C(100, 64, 32), lt = C(195, 145, 88), blk = C(20, 14, 12);
    auto px = [&](int x, int y, u32 c) { if (x >= 0 && x < 16 && y >= 0 && y < 10) s.d[size_t(y * 16 + x)] = c; };
    for (int y = 3; y <= 6; y++) for (int x = 3; x <= 11; x++) px(x, y, y == 6 ? dk : b);
    for (int y = 1; y <= 5; y++) for (int x = 11; x <= 14; x++) px(x, y, b);
    px(14, 3, lt); px(15, 3, blk); px(14, 4, lt); px(15, 4, lt); px(13, 2, blk); px(12, 0, dk); px(13, 0, dk); px(12, 1, dk);
    px(2, 3, b); px(1, 2, b); px(0, 1, b);
    static const int legs[4][4] = {{4, 5, 9, 10}, {3, 4, 10, 11}, {5, 6, 8, 9}, {3, 4, 10, 11}};
    for (int k = 0; k < 4; k++) { px(legs[frame][k], 7, dk); px(legs[frame][k], 8, dk); px(legs[frame][k], 9, blk); }
    return outline(s, RGB(20, 14, 16));
}

// Face portrait (24x24) for the HUD: a drawn face with eyes, brows, nose, mouth, headgear and per-bro details
static Sprite build_face(const BroDef &d) {
    const int N = 24;
    Sprite s;
    s.w = s.h = N;
    s.d.assign(size_t(N * N), 0);
    auto px = [&](int x, int y, u32 c) { if (x >= 0 && x < N && y >= 0 && y < N) s.d[size_t(y * N + x)] = c; };
    auto rect = [&](int x0, int y0, int x1, int y1, u32 c) { for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) px(x, y, c); };
    auto is = [&](const char *n) { return !strcmp(d.name, n); };
    const u32 dk = RGB(20, 14, 16), white = RGB(240, 240, 245);
    u32 skin = d.skin, skinD = shade(skin, -30), skinL = shade(skin, 20), skinDD = shade(skin, -55);
    u32 hair = d.hair, hairD = shade(hair, -32), hairL = shade(hair, 38);
    u32 acc = d.acc, accD = shade(acc, -45), accL = shade(acc, 45);
    auto in_ellipse = [](int x, int y, float cx, float cy, float rx, float ry) {
        float dx = (float(x) + 0.5f - cx) / rx, dy = (float(y) + 0.5f - cy) / ry;
        return dx * dx + dy * dy <= 1.0f;
    };
    // shoulders, collar, neck
    rect(2, 21, 21, 23, d.shirt);
    rect(4, 20, 19, 20, d.shirt);
    rect(1, 22, 1, 23, shade(d.shirt, -30)); rect(22, 22, 22, 23, shade(d.shirt, -30));
    rect(19, 21, 21, 23, shade(d.shirt, -25));
    rect(9, 19, 14, 21, skinD);
    rect(10, 21, 13, 22, shade(skin, -20));
    px(9, 21, shade(d.shirt, -40)); px(14, 21, shade(d.shirt, -40));
    // face: oval with the light coming from the left
    for (int y = 3; y <= 20; y++)
        for (int x = 4; x <= 19; x++)
            if (in_ellipse(x, y, 12.0f, 12.4f, 7.7f, 8.6f)) px(x, y, x >= 16 ? skinD : (x <= 6 && y > 8 && y < 16 ? skinL : skin));
    for (int x = 8; x <= 15; x++) if (in_ellipse(x, 19, 12.0f, 12.4f, 7.7f, 8.6f)) px(x, 19, skinD); // chin shadow
    rect(3, 11, 3, 15, skinD); rect(20, 11, 20, 15, skinD);          // ears
    px(3, 12, skinDD); px(20, 12, skinDD);
    // eyes (looking slightly to the side), brows, nose, mouth
    for (int e = 0; e < 2; e++) {
        int ex = e ? 14 : 8;
        rect(ex, 12, ex + 2, 13, white);
        rect(ex + 2, 12, ex + 2, 13, dk);
        px(ex + 1, 12, RGB(70, 110, 160));
        rect(ex, 14, ex + 2, 14, skinD);
        rect(ex, 10, ex + 2, 10, hairD);
        px(e ? ex : ex + 2, 11, hairD);                                // slightly angry brows
    }
    rect(12, 13, 12, 16, skinD); px(11, 16, skinD); px(12, 15, skinL);
    rect(10, 18, 14, 18, RGB(110, 45, 45));
    rect(10, 18, 13, 18, dk);
    px(14, 17, dk);                                                      // smirk
    px(9, 18, skinD);
    // headgear by style
    auto dome = [&](int ymax, u32 c, u32 hl) {
        for (int y = 1; y <= ymax; y++)
            for (int x = 2; x <= 21; x++)
                if (in_ellipse(x, y, 12.0f, 11.0f, 9.0f, 10.2f)) px(x, y, (x < 8 && y < ymax - 1) ? hl : (x > 17 ? shade(c, -26) : c));
    };
    switch (d.head) {
    case 0: // headband: hair under a knotted band
        dome(8, hair, hairL);
        rect(4, 8, 19, 9, acc); rect(4, 8, 19, 8, accL); rect(4, 9, 19, 9, accD);
        rect(20, 9, 22, 10, acc); rect(22, 11, 22, 13, acc); px(21, 11, accD);
        break;
    case 1: // hat
        rect(7, 2, 16, 6, acc); rect(7, 2, 16, 2, accL); rect(7, 6, 16, 6, accD);
        rect(2, 7, 21, 8, acc); rect(2, 8, 21, 8, accD); rect(3, 7, 20, 7, accL);
        rect(4, 9, 5, 13, hair); rect(18, 9, 19, 13, shade(hair, -20));
        break;
    case 2: // helmet
        dome(9, acc, accL);
        rect(4, 9, 19, 10, accD); rect(4, 9, 19, 9, shade(acc, -20));
        rect(4, 11, 4, 17, accD); rect(19, 11, 19, 17, accD); rect(5, 17, 6, 17, accD); rect(17, 17, 18, 17, accD);
        break;
    case 3: // mohawk
        dome(6, skin, skinL);
        rect(10, 0, 13, 7, hair); rect(10, 0, 10, 7, hairL); rect(13, 2, 13, 7, hairD);
        px(11, 0, hairL); rect(7, 6, 16, 7, skinD);
        break;
    case 4: // bald
        dome(5, skin, skinL);
        px(8, 3, skinL); px(9, 3, skinL); px(9, 4, skinL); px(10, 3, white); rect(8, 8, 16, 8, skinD);
        break;
    case 5: // long hair
        dome(8, hair, hairL);
        rect(3, 8, 5, 20, hair); rect(18, 8, 20, 20, hairD); rect(3, 8, 3, 20, hairD);
        rect(6, 8, 17, 9, hair); px(12, 6, hairD); px(12, 7, hairD); px(11, 8, hairD); px(13, 9, hairD); px(8, 9, skin); px(15, 9, skin);
        break;
    case 6: // shades
        dome(7, hair, hairL);
        px(6, 8, hair); px(17, 8, hair); px(6, 9, hair); px(17, 9, hair);
        rect(7, 11, 11, 13, dk); rect(13, 11, 17, 13, dk); rect(12, 11, 12, 11, dk);
        rect(5, 11, 6, 11, dk); rect(18, 11, 19, 11, dk);
        px(8, 11, RGB(150, 170, 200)); px(9, 11, RGB(90, 110, 140)); px(14, 11, RGB(150, 170, 200)); px(15, 11, RGB(90, 110, 140));
        break;
    case 7: // peaked officer cap with a badge
        rect(6, 1, 17, 5, acc); rect(6, 1, 17, 1, accL);
        rect(5, 6, 18, 7, accD); rect(7, 8, 17, 9, shade(acc, -70));
        rect(11, 2, 12, 4, d.extra); px(11, 2, white);
        rect(4, 9, 5, 12, hair); rect(18, 9, 19, 12, hair);
        break;
    }
    // per-bro details
    auto stubble = [&](u32 c, int density) {
        for (int y = 15; y <= 20; y++)
            for (int x = 6; x <= 17; x++)
                if (in_ellipse(x, y, 12.0f, 12.4f, 7.6f, 8.5f) && !(x >= 10 && x <= 14 && y == 18) && ((x * 7 + y * 13) % density) == 0) px(x, y, c);
    };
    u32 beard = shade(hair, -10);
    if (is("XENA")) { for (int x = 7; x <= 16; x++) px(x, 21, d.extra); rect(5, 9, 6, 20, hairD); rect(17, 9, 18, 20, hairD); rect(8, 11, 10, 11, dk); rect(14, 11, 16, 11, dk); px(14, 17, RGB(190, 70, 70)); rect(8, 7, 15, 7, d.extra); px(11, 6, RGB(220, 40, 40)); }
    if (is("ROCKY BROLBOA")) { rect(6, 12, 7, 13, RGB(150, 60, 60)); px(16, 11, RGB(150, 60, 60)); rect(4, 21, 19, 23, RGB(200, 30, 30)); rect(10, 18, 14, 19, dk); rect(11, 18, 13, 18, white); }
    if (is("DIRTY BRORY") || is("BRO WICK") || is("BRODELL WALKER") || is("BRO MAX") || is("INDIANNA BRONES") || is("ASH BROLLIAMS") || is("BRO HARD") || is("CHEV BROLIOS") || is("SNAKE BROSKIN"))
        stubble(beard, 5);
    if (is("BROLANDER")) { // full scruffy beard
        for (int y = 15; y <= 20; y++) for (int x = 6; x <= 17; x++) if (in_ellipse(x, y, 12.0f, 12.4f, 7.6f, 8.5f) && !(x >= 10 && x <= 13 && y == 18)) px(x, y, ((x + y) & 1) ? beard : shade(beard, -22));
        rect(10, 18, 13, 18, dk);
    }
    if (is("BROCHETE") || is("MACBROVER")) { rect(8, 16, 16, 16, hairD); px(8, 17, hairD); px(16, 17, hairD); rect(10, 16, 14, 16, hair); }
    if (is("ELLEN RIPBRO")) { rect(10, 18, 14, 18, RGB(200, 70, 80)); px(14, 17, RGB(200, 70, 80)); rect(8, 12, 10, 12, dk); rect(14, 12, 16, 12, dk); px(9, 15, skinL); }
    if (is("RAMBRO")) { px(15, 8, RGB(190, 90, 90)); px(15, 9, RGB(190, 90, 90)); px(16, 10, RGB(190, 90, 90)); px(16, 11, RGB(190, 90, 90)); px(7, 16, RGB(120, 190, 235)); px(6, 17, RGB(120, 190, 235)); }
    if (is("BRO LEE")) { for (int i = 0; i < 3; i++) { px(15 + i, 14 + i, RGB(200, 70, 70)); px(16 + i, 14 + i, RGB(200, 70, 70)); } rect(10, 18, 14, 19, dk); rect(11, 18, 13, 18, white); }
    if (is("B.A. BRORACUS")) { for (int x = 7; x <= 16; x++) px(x, 21 + ((x - 7) * (16 - x)) / 20, d.extra); px(12, 22, shade(d.extra, 30)); rect(9, 16, 15, 16, white); }
    if (is("BRODATOR")) { // mandibles, mask lines, dreadlocks
        rect(9, 17, 9, 21, d.extra); rect(14, 17, 14, 21, d.extra); rect(10, 22, 10, 22, accD); rect(13, 22, 13, 22, accD);
        rect(10, 18, 13, 18, dk);
        for (int k = 0; k < 3; k++) { rect(3 + k, 8, 3 + k, 17 + k * 2, (k & 1) ? hairD : hair); rect(20 - k, 8, 20 - k, 17 + k * 2, (k & 1) ? hair : hairD); }
        rect(8, 11, 10, 13, RGB(230, 190, 40)); rect(14, 11, 16, 13, RGB(230, 190, 40)); px(9, 12, dk); px(15, 12, dk);
    }
    if (is("BROMINATOR")) { // half metal face, red cybernetic eye
        for (int y = 5; y <= 20; y++) for (int x = 12; x <= 19; x++) if (s.at(x, y) == skin || s.at(x, y) == skinD) px(x, y, (x + y) & 1 ? RGB(150, 155, 165) : RGB(120, 126, 138));
        rect(14, 12, 16, 13, RGB(255, 40, 30)); px(15, 12, RGB(255, 190, 150)); rect(13, 10, 17, 10, RGB(60, 60, 68));
        rect(12, 5, 12, 12, RGB(60, 62, 70));
    }
    if (is("BROBOCOP") || is("BRO DREDD")) { // visor over the eyes
        u32 vis = is("BROBOCOP") ? RGB(15, 20, 30) : RGB(20, 25, 60);
        rect(6, 10, 17, 14, vis); rect(7, 10, 16, 10, shade(vis, 28));
        px(8, 11, RGB(110, 160, 220)); px(9, 11, RGB(70, 110, 170));
        if (is("BRO DREDD")) { rect(5, 15, 18, 16, acc); rect(10, 18, 13, 18, dk); rect(5, 15, 18, 15, accL); }
    }
    if (is("DOUBLE BRO SEVEN")) { rect(7, 3, 16, 7, hair); rect(7, 3, 16, 3, hairL); px(15, 10, hairD); px(16, 9, hairD); }
    if (is("BRO CAESAR")) { for (int i = 0; i < 8; i++) { px(4 + i * 2, 7 + (i % 2), acc); px(5 + i * 2, 7 + (i % 2), accL); } }
    if (is("BLADE BRO")) { px(10, 12, RGB(200, 40, 40)); px(15, 12, RGB(200, 40, 40)); }
    if (is("BRO IN BLACK")) { rect(4, 20, 19, 21, white); rect(11, 21, 12, 23, RGB(30, 30, 38)); } // white shirt collar and tie
    px(8, 9, skinL);
    // dark rim so the face reads well on the blue backdrop
    Sprite out = s;
    for (int y = 0; y < N; y++)
        for (int x = 0; x < N; x++)
            if (!s.at(x, y)) {
                bool edge = (x > 0 && s.at(x - 1, y)) || (x < N - 1 && s.at(x + 1, y)) || (y > 0 && s.at(x, y - 1)) || (y < N - 1 && s.at(x, y + 1));
                if (edge) out.d[size_t(y * N + x)] = RGB(20, 14, 16);
            }
    return out;
}

void Game::build_sprites() {
    if (spritesBuilt) return;
    spritesBuilt = true;
    for (int b = 0; b < NUM_BROS; b++)
        for (int f = 0; f < 4; f++) {
            const BroDef &d = BROS[b];
            broSpr[b][f] = build_char(d.head, d.skin, d.hair, d.acc, d.shirt, d.pants, d.extra, d.gun, f);
        }
    for (int b = 0; b < NUM_BROS; b++) broFace[b] = build_face(BROS[b]);
    { // the captive: a sack over the head, ragged clothes, ropes around the body
        captiveSpr = build_char(8, C(225,180,135), C(60,50,40), C(176,148,98), C(128,122,112), C(98,92,84), C(150,118,70), 0, 0);
        u32 rope = C(176,140,80);
        for (int y = 8; y <= 13; y += 2) for (int x = 3; x < captiveSpr.w - 3; x++) if (captiveSpr.at(x, y)) captiveSpr.d[size_t(y * captiveSpr.w + x)] = ((x + y) & 1) ? rope : C(120,92,52);
    }
    for (int f = 0; f < 4; f++) {
        enSpr[E_MOOK][f] = build_char(2, C(225,180,135), C(60,50,40), C(105,115,75), C(150,140,90), C(110,100,70), C(70,60,40), C(40,40,45), f);
        enSpr[E_BOMBER][f] = build_char(0, C(225,180,135), C(40,30,25), C(240,200,40), C(190,50,40), C(80,60,50), C(240,200,40), C(40,40,45), f);
        enSpr[E_DOG][f] = build_dog(f);
        enSpr[E_RPG][f] = build_char(2, C(225,180,135), C(60,50,40), C(160,40,40), C(80,95,65), C(65,80,55), C(40,50,35), C(60,70,55), f);
        enSpr[E_OFFICER][f] = build_char(7, C(225,180,135), C(60,50,40), C(172,152,96), C(190,168,108), C(168,148,94), C(235,205,70), 0, f);
        enSpr[E_HEAVY][f] = build_char(2, C(200,160,125), C(40,40,45), C(60,62,72), C(85,85,98), C(65,65,78), C(190,60,50), C(30,30,34), f);
    }
}

// Tiles ---------------------------------------------------------------------
void Game::build_tiles() {
    for (int t = 0; t < T_COUNT; t++)
        for (int v = 0; v < 4; v++) {
            Sprite &s = tileSpr[t][v];
            s.w = s.h = TS;
            s.d.assign(TS * TS, 0);
            for (int y = 0; y < TS; y++)
                for (int x = 0; x < TS; x++) {
                    uint32_t h = hash2(x, y, t * 7 + v * 31);
                    u32 c = 0;
                    switch (t) {
                    case T_DIRT:
                        c = th.dirt;
                        if ((h & 7) == 0) c = th.dirtDk;
                        else if ((h & 15) == 3) c = shade(th.dirt, 16);
                        break;
                    case T_SAND: // loose sand: pale, with wind-blown ripples
                        c = shade(th.dirt, 22);
                        if ((h & 7) == 0) c = shade(th.dirt, 6);
                        else if ((h & 15) == 3) c = shade(th.dirt, 40);
                        if (((x + y * 2 + v * 3) % 8) == 0) c = shade(c, -12);
                        break;
                    case T_STONE:
                        c = th.stone;
                        if ((h & 7) < 2) c = shade(th.stone, -18);
                        else if ((h & 15) == 5) c = shade(th.stone, 14);
                        if (y == 0 || x == 0) c = shade(c, 10);
                        if (y == TS - 1 || x == TS - 1) c = shade(c, -20);
                        break;
                    case T_BRICK:
                        c = th.brick;
                        if (y == 3 || y == 7) c = th.brickDk;
                        else if ((y < 4 && x == 3) || (y >= 4 && x == 7) || (y >= 4 && x == 0 && false)) c = th.brickDk;
                        else if ((h & 15) == 2) c = shade(th.brick, 14);
                        break;
                    case T_WOOD:
                        c = th.wood;
                        if (x == 0 || x == 4) c = th.woodDk;
                        else if ((h & 7) == 1) c = shade(th.wood, -14);
                        if (y == 0) c = shade(c, 12);
                        break;
                    case T_METAL:
                        c = th.metal;
                        if (x == 0 || y == 0) c = shade(th.metal, 25);
                        if (x == TS - 1 || y == TS - 1) c = th.metalDk;
                        if ((x == 2 || x == 5) && (y == 2 || y == 5)) c = th.metalDk;
                        break;
                    case T_BARREL: // red explosive crate with hazard stripes
                        c = C(205, 48, 42);
                        if (x == 0 || x == 7 || y == 0 || y == 7) c = C(120, 20, 20);
                        else if (x >= 2 && x <= 5 && y >= 2 && y <= 5 && ((x + y) & 1) == 0) c = C(250, 215, 50);
                        else if (y == 1 || x == 1) c = C(235, 85, 70);
                        break;
                    case T_CRATE: // brown wooden crate with cross bracing
                        c = C(160, 108, 58);
                        if (x == 0 || x == 7 || y == 0 || y == 7) c = C(105, 66, 30);
                        else if (x == y || x == 7 - y) c = C(125, 80, 38);
                        else if ((h & 15) == 4) c = C(175, 125, 70);
                        break;
                    case T_WIRE: { // barbed wire: two posts and coils of wire with barbs
                        c = 0;
                        if ((x == 0 || x == 7) && y >= 1) c = C(95, 62, 32);
                        else if (y >= 2 && y <= 6 && (((x + y) & 3) == 0 || ((x - y + 8) & 3) == 0)) c = C(175, 180, 190);
                        if (y >= 2 && y <= 6 && (x == 2 || x == 5) && ((x + y) & 1)) c = C(120, 125, 135);
                        if ((x == 3 && y == 1) || (x == 6 && y == 7)) c = C(200, 70, 60);
                        break;
                    }
                    case T_SPIKE: {
                        static const int ht[8] = {3, 6, 3, 1, 3, 6, 3, 1};
                        if (7 - y < ht[x]) c = (7 - y >= ht[x] - 1) ? C(235, 235, 245) : C(160, 165, 175);
                        if (y == 7) c = C(70, 70, 78);
                        break;
                    }
                    default: break;
                    }
                    s.d[size_t(y * TS + x)] = c;
                }
        }
}
