#include "gfx.h"
#include <cstdio>
#include <cstring>

Sprite make_sprite(const char *const *rows, int nrows, int w, const PalEntry *pal, int npal) {
    Sprite s;
    s.w = w;
    s.h = nrows;
    s.d.assign(size_t(w) * size_t(nrows), 0);
    for (int y = 0; y < nrows; y++) {
        int len = int(strlen(rows[y]));
        for (int x = 0; x < w && x < len; x++) {
            char c = rows[y][x];
            for (int i = 0; i < npal; i++)
                if (pal[i].c == c) { s.d[size_t(y) * size_t(w) + size_t(x)] = pal[i].col; break; }
        }
    }
    return s;
}

void Gfx::clear(u32 c) {
    for (int i = 0; i < VW * VH; i++) px[i] = c;
}

void Gfx::rect(int x, int y, int w, int h, u32 c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VW ? VW : x + w, y1 = y + h > VH ? VH : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) px[j * VW + i] = c;
}

void Gfx::rect_blend(int x, int y, int w, int h, u32 c, int a) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > VW ? VW : x + w, y1 = y + h > VH ? VH : y + h;
    int cr = (c >> 16) & 255, cg = (c >> 8) & 255, cb = c & 255;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++) {
            u32 &p = px[j * VW + i];
            int r = (int((p >> 16) & 255) * (256 - a) + cr * a) >> 8;
            int g = (int((p >> 8) & 255) * (256 - a) + cg * a) >> 8;
            int b = (int(p & 255) * (256 - a) + cb * a) >> 8;
            p = RGB(r, g, b);
        }
}

void Gfx::sprite(const Sprite &s, int x, int y, bool flip) {
    if (x >= 0 && y >= 0 && x + s.w <= VW && y + s.h <= VH) { // fully on screen: no per-pixel clipping
        for (int j = 0; j < s.h; j++) {
            u32 *dst = &px[(y + j) * VW + x];
            const u32 *src = &s.d[size_t(j) * size_t(s.w)];
            if (flip) { for (int i = 0; i < s.w; i++) { u32 c = src[s.w - 1 - i]; if (c) dst[i] = c; } }
            else { for (int i = 0; i < s.w; i++) { u32 c = src[i]; if (c) dst[i] = c; } }
        }
        return;
    }
    for (int j = 0; j < s.h; j++) {
        int yy = y + j;
        if ((unsigned)yy >= (unsigned)VH) continue;
        for (int i = 0; i < s.w; i++) {
            int xx = x + i;
            if ((unsigned)xx >= (unsigned)VW) continue;
            u32 c = s.at(flip ? s.w - 1 - i : i, j);
            if (c) px[yy * VW + xx] = c;
        }
    }
}

void Gfx::sprite_tint(const Sprite &s, int x, int y, bool flip, u32 tint) {
    for (int j = 0; j < s.h; j++) {
        int yy = y + j;
        if ((unsigned)yy >= (unsigned)VH) continue;
        for (int i = 0; i < s.w; i++) {
            int xx = x + i;
            if ((unsigned)xx >= (unsigned)VW) continue;
            u32 c = s.at(flip ? s.w - 1 - i : i, j);
            if (c) px[yy * VW + xx] = tint;
        }
    }
}

// 3x5 font, rows packed as octal digits (3 bits per row, MSB = left pixel).
static const char *glyph_for(char c) {
    switch (c) {
    case '0': return "75557"; case '1': return "26227"; case '2': return "71747"; case '3': return "71717";
    case '4': return "55711"; case '5': return "74717"; case '6': return "74757"; case '7': return "71111";
    case '8': return "75757"; case '9': return "75717";
    case 'A': return "25755"; case 'B': return "65656"; case 'C': return "34443"; case 'D': return "65556";
    case 'E': return "74647"; case 'F': return "74644"; case 'G': return "34553"; case 'H': return "55755";
    case 'I': return "72227"; case 'J': return "11152"; case 'K': return "55655"; case 'L': return "44447";
    case 'M': return "57755"; case 'N': return "65555"; case 'O': return "25552"; case 'P': return "65644";
    case 'Q': return "25573"; case 'R': return "65655"; case 'S': return "34216"; case 'T': return "72222";
    case 'U': return "55557"; case 'V': return "55552"; case 'W': return "55775"; case 'X': return "55255";
    case 'Y': return "55222"; case 'Z': return "71247";
    case '.': return "00002"; case ':': return "02020"; case '-': return "00700"; case '!': return "22202";
    case '?': return "61202"; case '%': return "51245"; case '/': return "11244"; case '+': return "02720";
    case '(': return "12221"; case ')': return "42224"; case '>': return "42124"; case '<': return "12421";
    case '\'': return "22000"; case ',': return "00024"; case '*': return "05250"; case '=': return "07070";
    case '_': return "00007"; case '#': return "57575";
    default: return "00000";
    }
}

int Gfx::text_w(const char *s, int scale) {
    int n = int(strlen(s));
    return n ? (n * 4 - 1) * scale : 0;
}

void Gfx::text(int x, int y, const char *s, u32 c, int scale) {
    for (; *s; s++, x += 4 * scale) {
        char ch = *s;
        if (ch >= 'a' && ch <= 'z') ch = char(ch - 32);
        const char *g = glyph_for(ch);
        for (int r = 0; r < 5; r++) {
            int bits = g[r] - '0';
            for (int k = 0; k < 3; k++)
                if (bits & (4 >> k)) rect(x + k * scale, y + r * scale, scale, scale, c);
        }
    }
}

void Gfx::text_shadow(int x, int y, const char *s, u32 c, int scale) {
    text(x + scale, y + scale, s, RGB(0, 0, 0), scale);
    text(x, y, s, c, scale);
}

// ---- minimal PNG writer (stored deflate blocks), used by the headless test tool ----
static uint32_t crc_table[256];
static void crc_init() {
    for (uint32_t n = 0; n < 256; n++) {
        uint32_t c = n;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_table[n] = c;
    }
}
static uint32_t crc_update(uint32_t crc, const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) crc = crc_table[(crc ^ b[i]) & 255] ^ (crc >> 8);
    return crc;
}
static void put32(std::vector<uint8_t> &v, uint32_t x) {
    v.push_back(uint8_t(x >> 24)); v.push_back(uint8_t(x >> 16)); v.push_back(uint8_t(x >> 8)); v.push_back(uint8_t(x));
}
static void chunk(FILE *f, const char *type, const std::vector<uint8_t> &data) {
    std::vector<uint8_t> hdr;
    put32(hdr, uint32_t(data.size()));
    fwrite(hdr.data(), 1, 4, f);
    uint32_t crc = 0xFFFFFFFFu;
    crc = crc_update(crc, (const uint8_t *)type, 4);
    fwrite(type, 1, 4, f);
    if (!data.empty()) {
        crc = crc_update(crc, data.data(), data.size());
        fwrite(data.data(), 1, data.size(), f);
    }
    std::vector<uint8_t> tail;
    put32(tail, crc ^ 0xFFFFFFFFu);
    fwrite(tail.data(), 1, 4, f);
}

bool write_png(const char *path, const u32 *argb, int w, int h) {
    crc_init();
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, uint32_t(w)); put32(ihdr, uint32_t(h));
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(f, "IHDR", ihdr);
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        for (int x = 0; x < w; x++) {
            u32 p = argb[y * w + x];
            raw.push_back(uint8_t(p >> 16)); raw.push_back(uint8_t(p >> 8)); raw.push_back(uint8_t(p));
        }
    }
    std::vector<uint8_t> z;
    z.push_back(0x78); z.push_back(0x01);
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t n = raw.size() - pos;
        if (n > 65535) n = 65535;
        bool last = pos + n == raw.size();
        z.push_back(last ? 1 : 0);
        z.push_back(uint8_t(n & 255)); z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n & 255)); z.push_back(uint8_t((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + long(pos), raw.begin() + long(pos + n));
        pos += n;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    fclose(f);
    return true;
}
