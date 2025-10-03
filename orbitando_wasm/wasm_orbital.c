/*
 wasm_orbital.c
 WebAssembly entry: receives inputs from JS and runs the animation using SDL2.
 Exports:
   - apply_inputs_from_js(int N, double* rx, double* ry, double* omega, int* size)
   - start_animation()  -- begins main loop (uses emscripten_set_main_loop)
   - stop_animation()
   - set_canvas_size(int w, int h)
*/
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifndef WIN_W
#define WIN_W 1024
#endif
#ifndef WIN_H
#define WIN_H 768
#endif

#define ORBITAL_MAXOBJ 15
#ifndef PALETTE_COUNT
#define PALETTE_COUNT 10
#endif

typedef struct {
    double rx, ry;
    double ang;
    double omega;
    int size;
    SDL_Color color;
} Body;

static const SDL_Color palette[] = {
    {255,80,80,255},{80,255,120,255},{100,160,255,255},{180,100,255,255},
    {255,200,80,255},{160,160,160,255},{0,200,200,255},{255,120,200,255},
    {200,200,100,255},{160,80,200,255}
};

static Body bodies[ORBITAL_MAXOBJ];
static int global_N = 0;

/* SDL objects */
static SDL_Window *g_win = NULL;
static SDL_Renderer *g_rnd = NULL;
static int canvas_w = WIN_W;
static int canvas_h = WIN_H;
static int running_main = 0;
static SDL_Texture *texs[ORBITAL_MAXOBJ];

static TTF_Font *g_font = NULL;

static SDL_Texture* make_square_texture(SDL_Renderer *rnd, int size, SDL_Color col) {
    SDL_Texture *tex = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, size, size);
    if (!tex) return NULL;
    SDL_Texture *old = SDL_GetRenderTarget(rnd);
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_SetRenderTarget(rnd, tex);
    SDL_SetRenderDrawBlendMode(rnd, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(rnd, col.r, col.g, col.b, col.a);
    SDL_Rect r = {0,0,size,size};
    SDL_RenderFillRect(rnd, &r);
    SDL_SetRenderDrawColor(rnd, (Uint8)fmax(0,col.r-30), (Uint8)fmax(0,col.g-30), (Uint8)fmax(0,col.b-30), col.a);
    SDL_RenderDrawRect(rnd, &r);
    SDL_SetRenderTarget(rnd, old);
    return tex;
}

/* helper: create or update textures */
static void create_textures() {
    for (int i = 0; i < ORBITAL_MAXOBJ; ++i) {
        if (texs[i]) { SDL_DestroyTexture(texs[i]); texs[i] = NULL; }
    }
    for (int i = 0; i < global_N; ++i) {
        texs[i] = make_square_texture(g_rnd, bodies[i].size, bodies[i].color);
    }
}

EMSCRIPTEN_KEEPALIVE
int apply_inputs_from_js(int N, double *rx, double *ry, double *omega, int *size) {
    if (N < 1) return 0;
    if (N > ORBITAL_MAXOBJ) N = ORBITAL_MAXOBJ;
    double baseRadius = (canvas_w < canvas_h ? canvas_w : canvas_h) / 2.0 - 30.0;
    for (int i = 0; i < N; ++i) {
        bodies[i].rx = rx[i] * baseRadius;
        bodies[i].ry = ry[i] * baseRadius;
        bodies[i].omega = omega[i] * 0.5;
        bodies[i].size = size[i];
        bodies[i].ang = (double)i * (2.0*M_PI / (double)N);
        bodies[i].color = palette[i % PALETTE_COUNT];
    }
    global_N = N;
    if (g_rnd) create_textures();
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void set_canvas_size(int w, int h) {
    if (w > 0) canvas_w = w;
    if (h > 0) canvas_h = h;
    if (g_win) {
        SDL_SetWindowSize(g_win, canvas_w, canvas_h);
    }
}

static void step(void *arg) {
    (void)arg;
    if (!running_main) return;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) { running_main = 0; }
    }

    /* advance simulation */
    static double last = 0;
    static int first = 1;
    double dt = 0.016;
    if (!first) dt = 0.016; else first = 0;

    for (int i = 0; i < global_N; ++i) {
        bodies[i].ang += bodies[i].omega * dt;
        if (bodies[i].ang > 2.0*M_PI) bodies[i].ang -= 2.0*M_PI;
        if (bodies[i].ang < 0) bodies[i].ang += 2.0*M_PI;
    }

    /* render */
    if (g_rnd == NULL) return;
    SDL_SetRenderDrawColor(g_rnd, 0,0,0,255);
    SDL_RenderClear(g_rnd);

    int cx = canvas_w / 2;
    int cy = canvas_h / 2;

    /* orbits */
    SDL_SetRenderDrawColor(g_rnd, 64,200,220,255);
    for (int i = 0; i < global_N; ++i) {
        int a = (int)round(bodies[i].rx);
        int b = (int)round(bodies[i].ry);
        const int TABLE_SIZE = 120;
        int px = cx + a, py = cy;
        for (int k = 0; k <= TABLE_SIZE; ++k) {
            double t = (2.0 * M_PI * k) / TABLE_SIZE;
            int x = cx + (int)round(a * cos(t));
            int y = cy + (int)round(b * sin(t));
            SDL_RenderDrawLine(g_rnd, px, py, x, y);
            px = x; py = y;
        }
    }

    SDL_Rect sun = {cx-8, cy-8, 16, 16};
    SDL_SetRenderDrawColor(g_rnd, 255,215,0,255);
    SDL_RenderFillRect(g_rnd, &sun);

    for (int i = 0; i < global_N; ++i) {
        double x = cx + bodies[i].rx * cos(bodies[i].ang);
        double y = cy + bodies[i].ry * sin(bodies[i].ang);
        int s = bodies[i].size;
        SDL_Rect dst = { (int)round(x - s/2.0), (int)round(y - s/2.0), s, s };
        if (texs[i]) {
            double deg = (bodies[i].ang / (2.0*M_PI)) * 360.0 * 2.0;
            SDL_Point center = { s/2, s/2 };
            SDL_RenderCopyEx(g_rnd, texs[i], NULL, &dst, deg, &center, SDL_FLIP_NONE);
        } else {
            SDL_SetRenderDrawColor(g_rnd, bodies[i].color.r, bodies[i].color.g, bodies[i].color.b, bodies[i].color.a);
            SDL_RenderFillRect(g_rnd, &dst);
        }
    }

    SDL_RenderPresent(g_rnd);
}

EMSCRIPTEN_KEEPALIVE
int start_animation() {
    if (running_main) return 1;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }
    if (TTF_Init() != 0) {
        printf("TTF_Init failed: %s\n", TTF_GetError());
        /* Not fatal: we don't require fonts for this setup */
    }
    g_win = SDL_CreateWindow("ORBITAL WASM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, canvas_w, canvas_h, SDL_WINDOW_SHOWN);
    if (!g_win) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 0;
    }
    g_rnd = SDL_CreateRenderer(g_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g_rnd) {
        printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
        return 0;
    }
    create_textures();
    running_main = 1;
    /* use emscripten main loop */
    emscripten_set_main_loop_arg(step, NULL, 0, 1);
    return 1;
}

EMSCRIPTEN_KEEPALIVE
void stop_animation() {
    if (!running_main) return;
    running_main = 0;
    emscripten_cancel_main_loop();
    for (int i = 0; i < ORBITAL_MAXOBJ; ++i) {
        if (texs[i]) { SDL_DestroyTexture(texs[i]); texs[i] = NULL; }
    }
    if (g_rnd) { SDL_DestroyRenderer(g_rnd); g_rnd = NULL; }
    if (g_win) { SDL_DestroyWindow(g_win); g_win = NULL; }
    TTF_Quit();
    SDL_Quit();
}