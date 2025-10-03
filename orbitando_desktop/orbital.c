/* orbital_sdl2_v1.c
   Main adapted to call oi_show_modal in a loop.
   - Modal Cancel (-1) exits program.
   - Modal OK (1) shows animation.
   - When animation ends, return to modal preserving the number of objects
     and the current values (rx, ry, omega, size) so the user can tweak and run again.
*/
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "orbital_input.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WIN_W 1024
#define WIN_H 768

static void draw_ellipse(SDL_Renderer *rnd, int cx, int cy, int a, int b, SDL_Color col) {
    SDL_SetRenderDrawColor(rnd, col.r, col.g, col.b, col.a);
    const int TABLE_SIZE = 360;
    int px = cx + a, py = cy;
    for (int i = 0; i <= TABLE_SIZE; ++i) {
        double t = (2.0 * M_PI * i) / TABLE_SIZE;
        int x = cx + (int)round(a * cos(t));
        int y = cy + (int)round(b * sin(t));
        SDL_RenderDrawLine(rnd, px, py, x, y);
        px = x; py = y;
    }
}

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

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *win = SDL_CreateWindow("ORBITAL SDL2 - Input Enabled", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); SDL_Quit(); return 1; }

    SDL_Renderer *rnd = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!rnd) { fprintf(stderr, "SDL_CreateRenderer: %s\n", SDL_GetError()); SDL_DestroyWindow(win); SDL_Quit(); return 1; }

    Body bodies[ORBITAL_MAXOBJ];
    int finalN = 0;

    /* Keep previous values so modal reopens with them.
       We'll provide them to oi_show_modal via oi_set_defaults when available. */
    int running_main = 1;
    while (running_main) {
        /* If we have previous bodies (finalN>0) pass their relative values as defaults */
        if (finalN > 0) {
            double relx[ORBITAL_MAXOBJ];
            double rely[ORBITAL_MAXOBJ];
            double wv[ORBITAL_MAXOBJ];
            int gs[ORBITAL_MAXOBJ];
            double baseRadius = (WIN_W < WIN_H ? WIN_W : WIN_H) / 2.0 - 30.0;
            for (int i = 0; i < finalN && i < ORBITAL_MAXOBJ; ++i) {
                relx[i] = bodies[i].rx / baseRadius;
                rely[i] = bodies[i].ry / baseRadius;
                wv[i]   = bodies[i].omega / 0.5;
                gs[i]   = bodies[i].size / 4;
            }
            oi_set_defaults(relx, rely, wv, gs, finalN);
        }

        int rc = oi_show_modal(win, rnd, bodies, &finalN);
        if (rc == -1) break; /* user cancelled -> exit */

        /* rc == 1: user pressed OK and out_bodies (bodies) filled, finalN set.
           Show animation; when closed, return to modal (loop continues). */

        int N = finalN;
        if (N < 1) N = 1;
        if (N > ORBITAL_MAXOBJ) N = ORBITAL_MAXOBJ;

        /* create textures for bodies (use current bodies array) */
        SDL_Texture *texs[ORBITAL_MAXOBJ] = {0};
        for (int i = 0; i < N; ++i) texs[i] = make_square_texture(rnd, bodies[i].size, bodies[i].color);

        /* pre-render background with orbits */
        SDL_Texture *bg = SDL_CreateTexture(rnd, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, WIN_W, WIN_H);
        if (bg) {
            SDL_Texture *old = SDL_GetRenderTarget(rnd);
            SDL_SetRenderTarget(rnd, bg);
            SDL_SetRenderDrawColor(rnd, 0,0,0,255);
            SDL_RenderClear(rnd);
            SDL_Color orbit_col = {64,200,220,255};
            int cx = WIN_W/2, cy = WIN_H/2;
            for (int i = 0; i < N; ++i) draw_ellipse(rnd, cx, cy, (int)round(bodies[i].rx), (int)round(bodies[i].ry), orbit_col);
            SDL_SetRenderDrawColor(rnd, 255,215,0,255);
            SDL_Rect sun = {cx-8, cy-8, 16, 16};
            SDL_RenderFillRect(rnd, &sun);
            SDL_SetRenderTarget(rnd, old);
        }

        /* animation loop */
        int anim_running = 1;
        Uint64 last = SDL_GetPerformanceCounter();
        double freq = (double)SDL_GetPerformanceFrequency();
        SDL_Event ev;
        while (anim_running) {
            Uint64 now = SDL_GetPerformanceCounter();
            double dt = (now - last) / freq;
            last = now;
            if (dt > 0.1) dt = 0.1;

            while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) { anim_running = 0; break; }
                if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) { anim_running = 0; break; }
            }

            for (int i = 0; i < N; ++i) {
                bodies[i].ang += bodies[i].omega * dt;
                if (bodies[i].ang > 2.0*M_PI) bodies[i].ang -= 2.0*M_PI;
                if (bodies[i].ang < 0) bodies[i].ang += 2.0*M_PI;
            }

            if (bg) SDL_RenderCopy(rnd, bg, NULL, NULL);
            else { SDL_SetRenderDrawColor(rnd, 0,0,0,255); SDL_RenderClear(rnd); }

            int cx = WIN_W/2, cy = WIN_H/2;
            for (int i = 0; i < N; ++i) {
                double x = cx + bodies[i].rx * cos(bodies[i].ang);
                double y = cy + bodies[i].ry * sin(bodies[i].ang);
                int s = bodies[i].size;
                SDL_Rect dst = { (int)round(x - s/2.0), (int)round(y - s/2.0), s, s };
                if (texs[i]) {
                    double deg = (bodies[i].ang / (2.0*M_PI)) * 360.0 * 2.0;
                    SDL_Point center = { s/2, s/2 };
                    SDL_RenderCopyEx(rnd, texs[i], NULL, &dst, deg, &center, SDL_FLIP_NONE);
                } else {
                    SDL_SetRenderDrawColor(rnd, bodies[i].color.r, bodies[i].color.g, bodies[i].color.b, bodies[i].color.a);
                    SDL_RenderFillRect(rnd, &dst);
                }
            }

            SDL_RenderPresent(rnd);
            SDL_Delay(6);
        }

        /* free textures and bg, then loop back to modal (preserving bodies/finalN) */
        for (int i = 0; i < N; ++i) if (texs[i]) { SDL_DestroyTexture(texs[i]); texs[i] = NULL; }
        if (bg) { SDL_DestroyTexture(bg); bg = NULL; }

        /* Now loop returns to show modal again with current bodies preserved.
           The loop continues until the user cancels the modal. */
    }

    SDL_DestroyRenderer(rnd);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}