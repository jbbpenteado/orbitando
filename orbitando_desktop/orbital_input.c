/* orbital_input.c
   Modal UI for parameter input using SDL2_ttf (preferred) with bitmap fallback.
   Presents semi-eixo-x (Rx) visually on the left and semi-eixo-y (Ry) on the right.
   Internal storage and parsing remain unchanged: cells[0]=Ry, cells[1]=Rx.
*/

#include "orbital_input.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifndef WIN_W
#define WIN_W 1024
#endif
#ifndef WIN_H
#define WIN_H 768
#endif

#define CELL_BUFSZ 32

/* Rounded rect + filled circle helpers (scanline approach) */
static void fill_rounded_rect(SDL_Renderer *r, SDL_Rect R, int radius, SDL_Color col) {
    if (radius <= 0) { SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a); SDL_RenderFillRect(r, &R); return; }
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    SDL_Rect mid = { R.x + radius, R.y, R.w - radius*2, R.h };
    SDL_RenderFillRect(r, &mid);
    SDL_Rect vert = { R.x, R.y + radius, R.w, R.h - radius*2 };
    SDL_RenderFillRect(r, &vert);
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)floor(sqrt((double)(radius*radius - dy*dy)));
        if (dx > 0) {
            SDL_Rect l = { R.x + radius - dx, R.y + radius + dy, dx, 1 };
            SDL_RenderFillRect(r, &l);
            SDL_Rect rr = { R.x + R.w - radius, R.y + radius + dy, dx, 1 };
            SDL_RenderFillRect(r, &rr);
        }
    }
}
static void fill_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color col) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        int dx = (int)floor(sqrt((double)(radius*radius - dy*dy)));
        SDL_Rect line = { cx - dx, cy + dy, dx*2 + 1, 1 };
        SDL_RenderFillRect(r, &line);
    }
}

/* draw rounded rect border precisely following the curve */
static void draw_rounded_rect_border(SDL_Renderer *rnd, SDL_Rect r, int radius, SDL_Color border) {
    if (radius <= 0) {
        SDL_SetRenderDrawColor(rnd, border.r, border.g, border.b, border.a);
        SDL_RenderDrawRect(rnd, &r);
        return;
    }
    SDL_SetRenderDrawColor(rnd, border.r, border.g, border.b, border.a);
    int left = r.x, right = r.x + r.w - 1;
    int top = r.y, bottom = r.y + r.h - 1;
    SDL_RenderDrawLine(rnd, left + radius, top, right - radius, top);
    SDL_RenderDrawLine(rnd, left + radius, bottom, right - radius, bottom);
    SDL_RenderDrawLine(rnd, left, top + radius, left, bottom - radius);
    SDL_RenderDrawLine(rnd, right, top + radius, right, bottom - radius);
    int cx1 = left + radius, cy1 = top + radius;
    int cx2 = right - radius, cy2 = top + radius;
    int cx3 = left + radius, cy3 = bottom - radius;
    int cx4 = right - radius, cy4 = bottom - radius;
    for (int a = 0; a <= 90; a += 6) {
        double rad = a * M_PI / 180.0;
        int dx = (int)round(cos(rad) * radius);
        int dy = (int)round(sin(rad) * radius);
        SDL_RenderDrawPoint(rnd, cx1 - dx, cy1 - dy);
        SDL_RenderDrawPoint(rnd, cx2 + dx, cy2 - dy);
        SDL_RenderDrawPoint(rnd, cx3 - dx, cy3 + dy);
        SDL_RenderDrawPoint(rnd, cx4 + dx, cy4 + dy);
    }
}

/* built-in defaults */
static const double builtin_relx[] = {0.15,0.25,0.35,0.45,0.55,0.65,0.75,0.85,0.95};
static const double builtin_rely[] = {0.12,0.20,0.28,0.35,0.45,0.55,0.65,0.75,0.85};
static const double builtin_wv[]   = {0.8,1.0,1.2,0.6,1.5,0.9,1.3,0.7,1.1};
static const int    builtin_gs[]   = {3,4,5,4,6,7,5,8,6};
static const int builtin_count = 9;

/* palette exported via header */
const SDL_Color palette[] = {
    {255,80,80,255},{80,255,120,255},{100,160,255,255},{180,100,255,255},
    {255,200,80,255},{160,160,160,255},{0,200,200,255},{255,120,200,255},
    {200,200,100,255},{160,80,200,255}
};
#ifndef PALETTE_COUNT
#define PALETTE_COUNT (sizeof(palette)/sizeof(palette[0]))
#endif

/* optional overrides */
static const double *opt_relx = NULL;
static const double *opt_rely = NULL;
static const double *opt_wv   = NULL;
static const int    *opt_gs   = NULL;
static int opt_count = 0;
void oi_set_defaults(const double *relx, const double *rely, const double *wvals, const int *gs, int count) {
    opt_relx = relx; opt_rely = rely; opt_wv = wvals; opt_gs = gs; opt_count = count;
}

/* bitmap fallback glyphs (small) */
static const unsigned char glyph_digits[][7] = {
    {0x1E,0x11,0x13,0x15,0x19,0x11,0x1E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x1E,0x11,0x01,0x06,0x08,0x10,0x1F},{0x1E,0x11,0x01,0x0E,0x01,0x11,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
};
static const unsigned char glyph_dot[7]   = {0,0,0,0,0,0x06,0x06};
static const unsigned char glyph_minus[7] = {0,0,0,0x1F,0,0,0};
static const unsigned char glyph_space[7] = {0,0,0,0,0,0,0};
static const unsigned char glyph_A[7] = {0x04,0x0A,0x11,0x11,0x1F,0x11,0x11};
static const unsigned char glyph_E[7] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F};
static const unsigned char glyph_I[7] = {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E};
static const unsigned char glyph_O[7] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E};
static const unsigned char glyph_R[7] = {0x1E,0x11,0x11,0x1E,0x12,0x11,0x11};
static const unsigned char glyph_S[7] = {0x0E,0x11,0x10,0x0E,0x01,0x11,0x0E};
static const unsigned char glyph_T[7] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04};
static const unsigned char glyph_U[7] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E};

static const unsigned char* select_fallback_glyph(char c) {
    if (c >= '0' && c <= '9') return glyph_digits[c - '0'];
    if (c == '.') return glyph_dot;
    if (c == '-') return glyph_minus;
    if (c == ' ') return glyph_space;
    switch (toupper((unsigned char)c)) {
        case 'A': return glyph_A;
        case 'E': return glyph_E;
        case 'I': return glyph_I;
        case 'O': return glyph_O;
        case 'R': return glyph_R;
        case 'S': return glyph_S;
        case 'T': return glyph_T;
        case 'U': return glyph_U;
        default: return glyph_space;
    }
}

/* clamp helper */
static inline int clampi(int v, int a, int b) { if (v < a) return a; if (v > b) return b; return v; }

/* TTF */
static int ttf_inited = 0;
static TTF_Font *g_font = NULL;
static TTF_Font *g_font_title = NULL;

static int load_fonts_if_needed(void) {
    if (ttf_inited) return (g_font != NULL) || (g_font_title != NULL);
    if (TTF_Init() != 0) { ttf_inited = 0; return 0; }
    ttf_inited = 1;
    const char *font_path = "assets/fonts/LiberationSans-Regular.ttf";
    g_font = TTF_OpenFont(font_path, 16);
    g_font_title = TTF_OpenFont(font_path, 20);
    return (g_font != NULL) || (g_font_title != NULL);
}

/* forward */
static void draw_text_any(SDL_Renderer *rnd, int x, int y, const char *s, SDL_Color col);
static void draw_text_center_any(SDL_Renderer *rnd, SDL_Rect r, const char *s, SDL_Color col);

static void draw_title(SDL_Renderer *rnd, int x, int y, const char *s, SDL_Color col) {
    if (load_fonts_if_needed() && g_font_title) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(g_font_title, s, col);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(rnd, surf);
            if (tex) {
                SDL_Rect dst = {x, y, surf->w, surf->h};
                SDL_RenderCopy(rnd, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
            return;
        }
    }
    draw_text_any(rnd, x, y, s, col);
}

static void draw_text_any(SDL_Renderer *rnd, int x, int y, const char *s, SDL_Color col) {
    if (load_fonts_if_needed() && g_font) {
        SDL_Surface *surf = TTF_RenderUTF8_Blended(g_font, s, col);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(rnd, surf);
            if (tex) {
                SDL_Rect dst = {x, y, surf->w, surf->h};
                SDL_RenderCopy(rnd, tex, NULL, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
            return;
        }
    }
    int cx = x;
    while (*s) {
        const unsigned char *g = select_fallback_glyph(*s);
        for (int row = 0; row < 7; ++row) {
            unsigned char bits = g[row];
            for (int bit = 0; bit < 5; ++bit) {
                if (bits & (1 << (4 - bit))) {
                    SDL_Rect r = { cx + bit*2, y + row*2, 2, 2 };
                    SDL_SetRenderDrawColor(rnd, col.r, col.g, col.b, col.a);
                    SDL_RenderFillRect(rnd, &r);
                }
            }
        }
        cx += (5 + 1) * 2;
        ++s;
    }
}

static void measure_text(SDL_Renderer *rnd, const char *s, int *w, int *h) {
    if (load_fonts_if_needed() && g_font) { TTF_SizeUTF8(g_font, s, w, h); return; }
    *w = (int)strlen(s) * (5 + 1) * 2;
    *h = 7 * 2;
}
static void draw_text_center_any(SDL_Renderer *rnd, SDL_Rect r, const char *s, SDL_Color col) {
    int tw, th; measure_text(rnd, s, &tw, &th);
    int x = r.x + (r.w - tw) / 2;
    int y = r.y + (r.h - th) / 2;
    draw_text_any(rnd, x, y, s, col);
}

static void draw_panel(SDL_Renderer *rnd, SDL_Rect r, SDL_Color bg, SDL_Color border) {
    SDL_SetRenderDrawColor(rnd, bg.r, bg.g, bg.b, bg.a); SDL_RenderFillRect(rnd, &r);
    SDL_SetRenderDrawColor(rnd, border.r, border.g, border.b, border.a); SDL_RenderDrawRect(rnd, &r);
}

typedef struct { char buf[CELL_BUFSZ]; int valid; } Cell;
typedef struct { int N; Cell cells[ORBITAL_MAXOBJ * 4]; } Grid;

/* helper prototypes */
static void grid_fill_one(Grid *g, int i);
static void grid_resize(Grid *g, int newN);

/* Persistent state across multiple calls to oi_show_modal:
   saves last grid contents, toggle state, chosenN, focus and scroll so that when
   the simulation window is opened and closed, the input modal restores previous state.
*/
static Grid _saved_grid;
static int _saved_use_defaults = 1;
static int _saved_chosenN = 9;
static int _saved_focus = 0;
static int _saved_scroll = 0;
static int _saved_initialized = 0;

static void grid_fill_defaults(Grid *g, int N) {
    const double *rxs = opt_relx ? opt_relx : builtin_relx;
    const double *rys = opt_rely ? opt_rely : builtin_rely;
    const double *wvs = opt_wv   ? opt_wv   : builtin_wv;
    const int    *gss = opt_gs   ? opt_gs   : builtin_gs;
    int cnt = opt_count ? opt_count : builtin_count;
    if (N < 1) N = 1;
    if (N > ORBITAL_MAXOBJ) N = ORBITAL_MAXOBJ;
    g->N = N;
    for (int i = 0; i < N; ++i) {
        double rx = (i < cnt ? rxs[i] : (0.1 + 0.08 * i));
        double ry = (i < cnt ? rys[i] : (0.1 + 0.08 * i));
        double w  = (i < cnt ? wvs[i] : (0.8 + 0.05 * i));
        int gs = (i < cnt ? gss[i] : (4 + (i % 4)));
        snprintf(g->cells[i*4 + 0].buf, CELL_BUFSZ, "%.2f", ry);
        snprintf(g->cells[i*4 + 1].buf, CELL_BUFSZ, "%.2f", rx);
        snprintf(g->cells[i*4 + 2].buf, CELL_BUFSZ, "%.2f", w);
        snprintf(g->cells[i*4 + 3].buf, CELL_BUFSZ, "%d", gs*4);
        for (int k = 0; k < 4; ++k) g->cells[i*4 + k].valid = 1;
    }
}

/* populate defaults for a single object index (preserve others) */
static void grid_fill_one(Grid *g, int i) {
    const double *rxs = opt_relx ? opt_relx : builtin_relx;
    const double *rys = opt_rely ? opt_rely : builtin_rely;
    const double *wvs = opt_wv   ? opt_wv   : builtin_wv;
    const int    *gss = opt_gs   ? opt_gs   : builtin_gs;
    int cnt = opt_count ? opt_count : builtin_count;
    double rx = (i < cnt ? rxs[i] : (0.1 + 0.08 * i));
    double ry = (i < cnt ? rys[i] : (0.1 + 0.08 * i));
    double w  = (i < cnt ? wvs[i] : (0.8 + 0.05 * i));
    int gs = (i < cnt ? gss[i] : (4 + (i % 4)));
    snprintf(g->cells[i*4 + 0].buf, CELL_BUFSZ, "%.2f", ry);
    snprintf(g->cells[i*4 + 1].buf, CELL_BUFSZ, "%.2f", rx);
    snprintf(g->cells[i*4 + 2].buf, CELL_BUFSZ, "%.2f", w);
    snprintf(g->cells[i*4 + 3].buf, CELL_BUFSZ, "%d", gs*4);
    for (int k = 0; k < 4; ++k) g->cells[i*4 + k].valid = 1;
}

static void grid_resize(Grid *g, int newN) {
    if (newN < 1) newN = 1;
    if (newN > ORBITAL_MAXOBJ) newN = ORBITAL_MAXOBJ;
    int oldN = g->N;
    if (oldN <= 0) oldN = 0;
    if (newN == oldN) { g->N = newN; return; }
    if (newN > oldN) {
        for (int i = oldN; i < newN; ++i) {
            grid_fill_one(g, i);
        }
    }
    g->N = newN;
}

static int grid_to_bodies(const Grid *g, Body out[], int *outN, char *errmsg, size_t emsz) {
    int N = g->N;
    double baseRadius = (WIN_W < WIN_H ? WIN_W : WIN_H) / 2.0 - 30.0;
    for (int i = 0; i < N; ++i) {
        const char *sry = g->cells[i*4 + 0].buf;
        const char *srx = g->cells[i*4 + 1].buf;
        const char *sw  = g->cells[i*4 + 2].buf;
        const char *ss  = g->cells[i*4 + 3].buf;
        char *end;
        double ryr = strtod(sry, &end); if (end == sry) { snprintf(errmsg, emsz, "Ry invalid at %d", i+1); return 0; }
        double rxr = strtod(srx, &end); if (end == srx) { snprintf(errmsg, emsz, "Rx invalid at %d", i+1); return 0; }
        double omega = strtod(sw, &end); if (end == sw) { snprintf(errmsg, emsz, "Omega invalid at %d", i+1); return 0; }
        long size = strtol(ss, &end, 10); if (end == ss) { snprintf(errmsg, emsz, "Size invalid at %d", i+1); return 0; }

        if (!(ryr >= 0.01 && ryr <= 1.5)) { snprintf(errmsg, emsz, "Ry out of range at %d", i+1); return 0; }
        if (!(rxr >= 0.01 && rxr <= 1.5)) { snprintf(errmsg, emsz, "Rx out of range at %d", i+1); return 0; }
        if (!(omega >= -10.0 && omega <= 10.0)) { snprintf(errmsg, emsz, "Omega out of range at %d", i+1); return 0; }
        if (!(size >= 2 && size <= 200)) { snprintf(errmsg, emsz, "Size out of range at %d", i+1); return 0; }

        out[i].rx = rxr * baseRadius;
        out[i].ry = ryr * baseRadius;
        out[i].omega = omega * 0.5;
        out[i].ang = (double)i * (2.0*M_PI / (double)N);
        out[i].size = (int)size;
        out[i].color = palette[i % PALETTE_COUNT];
    }
    *outN = N; return 1;
}

/* draw one cell */
static void draw_cell(SDL_Renderer *rnd, SDL_Rect rect, const char *buf, int focus, int valid) {
    SDL_Color bg = {28,28,36,230};
    SDL_Color border = focus ? (SDL_Color){255,200,80,255} : (SDL_Color){90,90,100,255};
    if (!valid) border = (SDL_Color){220,80,80,255};
    draw_panel(rnd, rect, bg, border);
    draw_text_any(rnd, rect.x + 8, rect.y + 8, buf, (SDL_Color){230,230,230,255});
}

/* main modal */
int oi_show_modal(SDL_Window *win, SDL_Renderer *rnd, Body out_bodies[], int *outN) {
    Grid grid; grid.N = 0;
    int use_defaults = 1;     /* toggle starts ON */
    int chosenN = use_defaults ? 9 : 1;          /* when defaults on, use 9 by policy */
    int focus = 0;            /* focused cell index */
    int scroll = 0;           /* scroll offset (body index) */
    int running = 1, res = -1;
    int edit_started = 0;     /* indicates overwrite begun for focused cell */
    int edit_index = -1;      /* which cell backup corresponds to */
    char edit_backup[CELL_BUFSZ] = {0};

    SDL_StartTextInput();
    if (_saved_initialized) {
        grid = _saved_grid;
        use_defaults = _saved_use_defaults;
        chosenN = _saved_chosenN;
        focus = _saved_focus;
        scroll = _saved_scroll;
    } else {
        grid_resize(&grid, chosenN);
    }

    /* geometry */
    SDL_Rect modal = { (WIN_W - 840)/2, (WIN_H - 620)/2, 840, 620 };
    int padding = 16;
    int header_h = 32;
    int cell_h = 42;
    int cell_w = (modal.w - padding*2 - 24) / 2;

    /* fixed 2 objects per page */
    int max_vis_bodies = 2;
    int view_h = modal.h - padding*3 - 120;
    int per_body_h = view_h / max_vis_bodies;
    if (per_body_h < (2*cell_h + 60)) per_body_h = 2*cell_h + 60;

    char errmsg[128] = {0};
    SDL_Event ev;

    int dragging_thumb = 0;
    int thumb_drag_offset = 0;

    while (running) {
        int body_area_x = modal.x + padding;
        int body_area_y = modal.y + padding + header_h + 24;
        int body_area_h = modal.h - (padding*3 + 120);
        int scroll_voffset = (cell_h / 2);
        int view_h_adj = body_area_h - scroll_voffset;
        if (view_h_adj < 0) view_h_adj = body_area_h;
        SDL_Rect view = { body_area_x, body_area_y + scroll_voffset, modal.w - padding*2 - 8, view_h_adj };

        int control_w = 36 + 8 + 160 + 8 + 36;
        int right_col_right = body_area_x + 2 * cell_w + 12;
        int ctrl_x = right_col_right - control_w;
        int min_ctrl_x = modal.x + padding;
        if (ctrl_x < min_ctrl_x) ctrl_x = min_ctrl_x;

        int control_voff = scroll_voffset;

        SDL_Rect minus_btn = { ctrl_x, modal.y + padding + 28 + control_voff, 36, 32 };
        SDL_Rect nbox = { minus_btn.x + minus_btn.w + 8, modal.y + padding + 24 + control_voff, 160, 40 };
        SDL_Rect plus_btn = { nbox.x + nbox.w + 8, modal.y + padding + 28 + control_voff, 36, 32 };

        SDL_Rect b_toggle = { modal.x + padding, modal.y + modal.h - padding - 48, 220, 40 };
        b_toggle.w = (int)round(b_toggle.w * 0.6);

        SDL_Rect b_ok = { modal.x + modal.w - padding - 160, modal.y + modal.h - padding - 48, 160, 40 };
        SDL_Rect b_cancel = { modal.x + modal.w - padding - 320, modal.y + modal.h - padding - 48, 160, 40 };
        SDL_Rect track = { view.x + view.w + 8, view.y, 12, view.h };

        int total_items = grid.N;
        int visible_items = max_vis_bodies;
        int thumb_h = 16;
        int range_items = 0;
        int thumb_y = track.y;
        if (total_items > visible_items) {
            thumb_h = (int)fmax(16.0, (double)track.h * (double)visible_items / (double)total_items);
            range_items = total_items - visible_items;
            if (range_items > 0) {
                thumb_y = track.y + (int)round((double)(track.h - thumb_h) * ((double)scroll / (double)range_items));
            } else {
                thumb_y = track.y;
            }
        } else {
            thumb_h = (int)fmax(16.0, (double)track.h);
            thumb_y = track.y;
        }

        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) { running = 0; res = -1; break; }

            if (ev.type == SDL_KEYDOWN) {
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    if (edit_started && edit_index == focus) {
                        strncpy(grid.cells[focus].buf, edit_backup, CELL_BUFSZ);
                        edit_started = 0; edit_index = -1;
                    } else {
                        running = 0; res = -1; break;
                    }
                    continue;
                }

                if (!use_defaults) {
                    if (ev.key.keysym.sym == SDLK_PLUS || ev.key.keysym.sym == SDLK_KP_PLUS || ev.key.keysym.sym == SDLK_EQUALS) {
                        chosenN = clampi(chosenN + 1, 1, ORBITAL_MAXOBJ);
                        grid_resize(&grid, chosenN);
                        if (grid.N > max_vis_bodies) scroll = chosenN - max_vis_bodies;
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_MINUS || ev.key.keysym.sym == SDLK_KP_MINUS) {
                        chosenN = clampi(chosenN - 1, 1, ORBITAL_MAXOBJ);
                        grid_resize(&grid, chosenN);
                        if (scroll > grid.N - max_vis_bodies) scroll = clampi(grid.N - max_vis_bodies, 0, grid.N);
                        if (focus >= grid.N*4) focus = grid.N*4 - 1;
                        continue;
                    }

                    if (ev.key.keysym.sym == SDLK_TAB && grid.N > 0) {
                        int shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                        int total = grid.N * 4;
                        if (!shift) focus = (focus + 1) % total;
                        else focus = (focus - 1 + total) % total;
                        strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ);
                        edit_index = focus; edit_started = 0;
                        int bi = focus / 4;
                        if (bi < scroll) scroll = bi;
                        if (bi >= scroll + max_vis_bodies) scroll = bi - max_vis_bodies + 1;
                        continue;
                    } else if ((ev.key.keysym.sym == SDLK_RETURN || ev.key.keysym.sym == SDLK_KP_ENTER) && grid.N > 0) {
                        focus = (focus + 1) % (grid.N*4);
                        strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ);
                        edit_index = focus; edit_started = 0;
                        int bi = focus / 4;
                        if (bi < scroll) scroll = bi;
                        if (bi >= scroll + max_vis_bodies) scroll = bi - max_vis_bodies + 1;
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_BACKSPACE && grid.N > 0) {
                        char *b = grid.cells[focus].buf; size_t L = strlen(b);
                        if (L) { b[L-1] = '\0'; edit_started = 1; }
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_PAGEUP) {
                        scroll = clampi(scroll - max_vis_bodies, 0, (grid.N > max_vis_bodies) ? (grid.N - max_vis_bodies) : 0);
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_PAGEDOWN) {
                        scroll = clampi(scroll + max_vis_bodies, 0, (grid.N > max_vis_bodies) ? (grid.N - max_vis_bodies) : 0);
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_UP && grid.N > 0) {
                        int c = focus % 4; int b = focus / 4;
                        if (b > 0) { b--; focus = b*4 + c; if (b < scroll) scroll = b; }
                        strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ); edit_index = focus; edit_started = 0;
                        continue;
                    } else if (ev.key.keysym.sym == SDLK_DOWN && grid.N > 0) {
                        int c = focus % 4; int b = focus / 4;
                        if (b < grid.N - 1) { b++; focus = b*4 + c; if (b >= scroll + max_vis_bodies) scroll = b - max_vis_bodies + 1; }
                        strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ); edit_index = focus; edit_started = 0;
                        continue;
                    }
                }
            }
            else if (ev.type == SDL_TEXTINPUT && grid.N > 0) {
                if (use_defaults) continue;
                if (edit_index != focus) {
                    strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ);
                    edit_index = focus; edit_started = 0;
                }
                if (!edit_started) {
                    grid.cells[focus].buf[0] = '\0';
                    edit_started = 1;
                }
                const char *text = ev.text.text;
                for (int k = 0; text[k]; ++k) {
                    char ch = text[k];
                    if ((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') {
                        char *b = grid.cells[focus].buf;
                        if (strlen(b) + 1 < CELL_BUFSZ) { size_t L = strlen(b); b[L] = ch; b[L+1] = '\0'; }
                    }
                }
                continue;
            }
            else if (ev.type == SDL_MOUSEWHEEL && grid.N > 0) {
                if (ev.wheel.y > 0) scroll = (scroll > 0) ? scroll - 1 : 0;
                else { if (grid.N > max_vis_bodies) { int max = grid.N - max_vis_bodies; scroll = (scroll < max) ? scroll + 1 : max; } }
                continue;
            }
            else if (ev.type == SDL_MOUSEBUTTONDOWN) {
                int mx = ev.button.x, my = ev.button.y;

                if (grid.N > max_vis_bodies) {
                    SDL_Rect thumb_rect = { track.x + 2, thumb_y, track.w - 4, thumb_h };
                    if (mx >= thumb_rect.x && mx <= thumb_rect.x + thumb_rect.w &&
                        my >= thumb_rect.y && my <= thumb_rect.y + thumb_rect.h) {
                        dragging_thumb = 1;
                        thumb_drag_offset = my - thumb_rect.y;
                        continue;
                    }
                }

                if (!use_defaults) {
                    if (mx >= plus_btn.x && mx <= plus_btn.x + plus_btn.w && my >= plus_btn.y && my <= plus_btn.y + plus_btn.h) {
                        chosenN = clampi(chosenN + 1, 1, ORBITAL_MAXOBJ);
                        grid_resize(&grid, chosenN);
                        if (grid.N > max_vis_bodies) scroll = chosenN - max_vis_bodies;
                        edit_started = 0; edit_index = -1;
                        continue;
                    } else if (mx >= minus_btn.x && mx <= minus_btn.x + minus_btn.w && my >= minus_btn.y && my <= minus_btn.y + minus_btn.h) {
                        chosenN = clampi(chosenN - 1, 1, ORBITAL_MAXOBJ);
                        grid_resize(&grid, chosenN);
                        if (scroll > grid.N - max_vis_bodies) scroll = clampi(grid.N - max_vis_bodies, 0, grid.N);
                        if (focus >= grid.N*4) focus = grid.N*4 - 1;
                        edit_started = 0; edit_index = -1;
                        continue;
                    }
                }

                /* click in view: visual left now shows Rx, visual right shows Ry */
                if (!use_defaults && mx >= view.x && mx <= view.x + view.w && my >= view.y && my <= view.y + view.h && grid.N > 0) {
                    int relY = my - body_area_y;
                    int vis_idx = relY / per_body_h;
                    if (vis_idx >= 0 && vis_idx < max_vis_bodies) {
                        int bi = scroll + vis_idx;
                        if (bi >= 0 && bi < grid.N) {
                            int by = body_area_y + vis_idx * per_body_h;
                            SDL_Rect leftRect  = { body_area_x, by + 52, cell_w, cell_h };                    /* visually left = Rx */
                            SDL_Rect rightRect = { body_area_x + cell_w + 12, by + 52, cell_w, cell_h };       /* visually right = Ry */
                            SDL_Rect wrect = { body_area_x, by + 52 + cell_h + 10 + 24, cell_w, cell_h };
                            SDL_Rect srect = { body_area_x + cell_w + 12, by + 52 + cell_h + 10 + 24, cell_w, cell_h };
                            if (mx >= leftRect.x && mx <= leftRect.x + leftRect.w && my >= leftRect.y && my <= leftRect.y + leftRect.h) {
                                focus = bi*4 + 1; /* left -> Rx stored at cell index 1 */
                            } else if (mx >= rightRect.x && mx <= rightRect.x + rightRect.w && my >= rightRect.y && my <= rightRect.y + rightRect.h) {
                                focus = bi*4 + 0; /* right -> Ry stored at cell index 0 */
                            } else if (mx >= wrect.x && mx <= wrect.x + wrect.w && my >= wrect.y && my <= wrect.y + wrect.h) {
                                focus = bi*4 + 2;
                            } else if (mx >= srect.x && mx <= srect.x + srect.w && my >= srect.y && my <= srect.y + srect.h) {
                                focus = bi*4 + 3;
                            }
                            strncpy(edit_backup, grid.cells[focus].buf, CELL_BUFSZ);
                            edit_index = focus; edit_started = 0;
                        }
                    }
                    continue;
                }

                if (mx >= b_toggle.x && mx <= b_toggle.x + b_toggle.w && my >= b_toggle.y && my <= b_toggle.y + b_toggle.h) {
                    use_defaults = !use_defaults;
                    if (use_defaults) { chosenN = 9; grid_fill_defaults(&grid, chosenN); scroll = 0; focus = 0; }
                    else { grid_resize(&grid, chosenN); scroll = 0; }
                    edit_started = 0; edit_index = -1;
                    continue;
                }
                if (mx >= b_ok.x && mx <= b_ok.x + b_ok.w && my >= b_ok.y && my <= b_ok.y + b_ok.h) {
                    if (use_defaults) {
                        chosenN = 9;
                        grid_fill_defaults(&grid, chosenN);
                        scroll = 0; focus = 0;
                    }
                    if (grid_to_bodies(&grid, out_bodies, outN, errmsg, sizeof(errmsg))) { res = 1; running = 0; break; }
                    else {
                        int idx = 0; sscanf(errmsg, "%*s %*s %*s %d", &idx);
                        if (idx >= 1 && idx <= grid.N) {
                            for (int k = 0; k < 4; ++k) grid.cells[(idx-1)*4 + k].valid = 0;
                            focus = (idx-1)*4;
                            if ((idx-1) < scroll) scroll = idx-1;
                            if ((idx-1) >= scroll + max_vis_bodies) scroll = idx-1 - max_vis_bodies + 1;
                        }
                    }
                    continue;
                }
                if (mx >= b_cancel.x && mx <= b_cancel.x + b_cancel.w && my >= b_cancel.y && my <= b_cancel.y + b_cancel.h) {
                    res = -1; running = 0; break;
                }
                if (grid.N > max_vis_bodies && mx >= track.x && mx <= track.x + track.w && my >= track.y && my <= track.y + track.h) {
                    int total = grid.N; int visible = max_vis_bodies;
                    int thumb_h_local = thumb_h;
                    int range = total - visible;
                    int thumb_y_local = thumb_y;
                    if (range > 0) thumb_y_local = track.y + (int)round((double)(track.h - thumb_h_local) * ((double)scroll / (double)range));
                    if (my < thumb_y_local) scroll = clampi(scroll - visible, 0, range);
                    else if (my > thumb_y_local + thumb_h_local) scroll = clampi(scroll + visible, 0, range);
                    continue;
                }
            }
            else if (ev.type == SDL_MOUSEMOTION) {
                if (dragging_thumb && grid.N > max_vis_bodies) {
                    int my = ev.motion.y;
                    int rel = my - track.y - thumb_drag_offset;
                    int track_range = track.h - thumb_h;
                    if (track_range < 1) track_range = 1;
                    if (rel < 0) rel = 0;
                    if (rel > track_range) rel = track_range;
                    int range_items_local = grid.N - max_vis_bodies;
                    double frac = (double)rel / (double)track_range;
                    int new_scroll = (int)round(frac * (double)range_items_local);
                    scroll = clampi(new_scroll, 0, range_items_local);
                }
            }
            else if (ev.type == SDL_MOUSEBUTTONUP) {
                if (dragging_thumb) { dragging_thumb = 0; }
            }
        } /* event loop */

        /* render */
        SDL_SetRenderDrawBlendMode(rnd, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(rnd, 0,0,0,160); SDL_RenderFillRect(rnd, NULL);
        draw_panel(rnd, modal, (SDL_Color){18,18,22,240}, (SDL_Color){110,110,130,255});

        draw_title(rnd, modal.x + padding, modal.y + padding, "PARÂMETROS DE ENTRADA", (SDL_Color){200,220,255,255});

        if (!use_defaults) draw_text_any(rnd, modal.x + padding, modal.y + padding + 42, "Edição manual ativa", (SDL_Color){200,200,200,255});
        else draw_text_any(rnd, modal.x + padding, modal.y + padding + 42, "Usando defaults (desative toggle para editar)", (SDL_Color){180,180,180,255});

        /* draw control group inside panel */
        draw_panel(rnd, minus_btn, (SDL_Color){50,50,60,255}, (SDL_Color){100,100,120,255});
        draw_panel(rnd, nbox, (SDL_Color){36,36,46,255}, (SDL_Color){100,100,120,255});
        draw_panel(rnd, plus_btn, (SDL_Color){50,50,60,255}, (SDL_Color){100,100,120,255});
        char nbuf[16]; snprintf(nbuf, sizeof(nbuf), "%d", chosenN);
        draw_text_center_any(rnd, nbox, nbuf, (SDL_Color){230,230,230,255});
        SDL_Color ctrlcol = use_defaults ? (SDL_Color){120,120,120,255} : (SDL_Color){220,220,220,255};
        draw_text_any(rnd, minus_btn.x + 10, minus_btn.y + 8, "-", ctrlcol);
        draw_text_any(rnd, plus_btn.x + 10, plus_btn.y + 8, "+", ctrlcol);

        /* draw view */
        SDL_RenderSetClipRect(rnd, &view);
        for (int bi = 0; bi < grid.N; ++bi) {
            int vis = bi - scroll; if (vis < 0 || vis >= max_vis_bodies) continue;
            int by = body_area_y + vis * per_body_h + (vis == 0 ? (cell_h / 2) : 0);

            char title[64]; snprintf(title, sizeof(title), "Objeto %d", bi+1);
            draw_text_any(rnd, body_area_x + 2, by, title, (SDL_Color){200,220,255,255});

            /* VISUAL: display Rx left and Ry right */
            draw_text_any(rnd, body_area_x + 2, by + 22, "SEMI-EIXOS (Rx esquerda ; Ry direita)", (SDL_Color){200,200,200,255});
            SDL_Rect leftRect  = { body_area_x, by + 52, cell_w, cell_h };
            SDL_Rect rightRect = { body_area_x + cell_w + 12, by + 52, cell_w, cell_h };
            int fi = bi*4;
            draw_cell(rnd, leftRect,  grid.cells[fi+1].buf, focus==fi+1, grid.cells[fi+1].valid); /* show Rx */
            draw_cell(rnd, rightRect, grid.cells[fi+0].buf, focus==fi+0, grid.cells[fi+0].valid); /* show Ry */

            draw_text_any(rnd, body_area_x + 2, by + 52 + cell_h + 10, "VELOCIDADE ANGULAR E TAMANHO", (SDL_Color){200,200,200,255});
            SDL_Rect wrect = { body_area_x, by + 52 + cell_h + 10 + 24, cell_w, cell_h };
            SDL_Rect srect = { body_area_x + cell_w + 12, by + 52 + cell_h + 10 + 24, cell_w, cell_h };
            draw_cell(rnd, wrect, grid.cells[fi+2].buf, focus==fi+2, grid.cells[fi+2].valid);
            draw_cell(rnd, srect, grid.cells[fi+3].buf, focus==fi+3, grid.cells[fi+3].valid);
        }
        SDL_RenderSetClipRect(rnd, NULL);

        /* scrollbar (visual) */
        if (grid.N > max_vis_bodies) {
            draw_panel(rnd, track, (SDL_Color){40,40,40,200}, (SDL_Color){90,90,90,200});
            int total = grid.N; int visible = max_vis_bodies;
            int thumb_h_now = (int)fmax(16.0, (double)track.h * (double)visible / (double)total);
            int range = total - visible;
            int thumb_y_now = track.y;
            if (range > 0) thumb_y_now = track.y + (int)round((double)(track.h - thumb_h_now) * ((double)scroll / (double)range));
            SDL_Rect thumb = { track.x + 2, thumb_y_now, track.w - 4, thumb_h_now };
            draw_panel(rnd, thumb, (SDL_Color){120,120,120,220}, (SDL_Color){200,200,200,220});
        }

        /* bottom hints and controls */
        draw_text_any(rnd, modal.x + padding, modal.y + modal.h - padding - 88, "Tab/Shift+Tab mover  Clique para foco  Enter proximo  Esc cancelar", (SDL_Color){180,180,180,255});

        /* Modern rounded toggle UI */
        {
            SDL_Color btn_bg = (SDL_Color){50,50,60,255};
            SDL_Color btn_border = (SDL_Color){100,100,120,255};
            SDL_Color knob_on_color = (SDL_Color){40,80,40,255};
            SDL_Color knob_off_color = (SDL_Color){200,220,255,255};

            SDL_Rect tbg = b_toggle;
            int radius = tbg.h / 2;

            fill_rounded_rect(rnd, tbg, radius, btn_bg);
            draw_rounded_rect_border(rnd, tbg, radius, btn_border);

            int krad = radius - 3; if (krad < 4) krad = radius - 2;
            int pad = 3;
            int left_x  = tbg.x + pad + krad;
            int right_x = tbg.x + tbg.w - pad - krad;
            int ky = tbg.y + tbg.h/2;
            int kx = use_defaults ? right_x : left_x;
            SDL_Color knob_col = use_defaults ? knob_on_color : knob_off_color;
            SDL_Color shadow = (SDL_Color){20,20,20,120};
            fill_circle(rnd, kx, ky + 1, krad + 1, shadow);
            fill_circle(rnd, kx, ky, krad, knob_col);

            draw_text_any(rnd, tbg.x + tbg.w + 12, tbg.y + (tbg.h - 16)/2, "Use defaults", (SDL_Color){220,220,220,255});
        }

        draw_panel(rnd, b_cancel, (SDL_Color){80,40,40,255}, (SDL_Color){160,100,100,255});
        draw_panel(rnd, b_ok, (SDL_Color){40,80,40,255}, (SDL_Color){120,200,120,255});
        draw_text_center_any(rnd, b_cancel, "CANCEL", (SDL_Color){255,220,220,255});
        draw_text_center_any(rnd, b_ok, "OK", (SDL_Color){220,255,220,255});

        if (errmsg[0]) draw_text_any(rnd, modal.x + padding + 240, modal.y + modal.h - padding - 48, errmsg, (SDL_Color){255,120,120,255});

        SDL_RenderPresent(rnd);
        SDL_Delay(12);
    } /* loop */

    _saved_grid = grid;
    _saved_use_defaults = use_defaults;
    _saved_chosenN = chosenN;
    _saved_focus = focus;
    _saved_scroll = scroll;
    _saved_initialized = 1;

    SDL_StopTextInput();
    if (g_font) { TTF_CloseFont(g_font); g_font = NULL; }
    if (g_font_title) { TTF_CloseFont(g_font_title); g_font_title = NULL; }
    if (ttf_inited) { TTF_Quit(); ttf_inited = 0; }
    return res;
}