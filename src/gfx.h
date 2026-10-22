// Broyoo - tiny software renderer (320x240 ARGB framebuffer, no SDL dependency)
#pragma once
#include <cstdint>
#include <vector>

constexpr int VW = 320;
constexpr int VH = 240;
typedef uint32_t u32;

constexpr u32 RGB(int r, int g, int b) { return 0xFF000000u | (u32(r) << 16) | (u32(g) << 8) | u32(b); }

struct Sprite {
    int w = 0, h = 0;
    std::vector<u32> d; // 0 = transparent
    u32 at(int x, int y) const { return d[size_t(y) * size_t(w) + size_t(x)]; }
};

// Build a sprite from rows of characters; `pal` maps a char to a color (0 = transparent).
struct PalEntry { char c; u32 col; };
Sprite make_sprite(const char *const *rows, int nrows, int w, const PalEntry *pal, int npal);

struct Gfx {
    u32 px[VW * VH];
    void clear(u32 c);
    void pset(int x, int y, u32 c) {
        if ((unsigned)x < (unsigned)VW && (unsigned)y < (unsigned)VH) px[y * VW + x] = c;
    }
    void rect(int x, int y, int w, int h, u32 c);
    void rect_blend(int x, int y, int w, int h, u32 c, int alpha256);
    void sprite(const Sprite &s, int x, int y, bool flip = false);
    void sprite_tint(const Sprite &s, int x, int y, bool flip, u32 tint); // flash: silhouette in `tint`
    void text(int x, int y, const char *s, u32 c, int scale = 1);
    void text_shadow(int x, int y, const char *s, u32 c, int scale = 1);
    static int text_w(const char *s, int scale = 1);
};

bool write_png(const char *path, const u32 *argb, int w, int h);
