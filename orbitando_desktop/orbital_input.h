#ifndef ORBITAL_INPUT_H
#define ORBITAL_INPUT_H

#include <SDL2/SDL.h>

/* palette exportada para uso em main */
extern const SDL_Color palette[];
#define PALETTE_COUNT 10

#define ORBITAL_MAXOBJ 15

typedef struct {
    double rx, ry; /* semi-eixos (pixels) */
    double ang;    /* posição angular (radians) */
    double omega;  /* velocidade angular (radians per second) */
    int size;      /* tamanho nominal do quadrado (pixels) */
    SDL_Color color;
} Body;

/*
 Show modal input panel.
 win/rnd : window and renderer already created.
 out_bodies : array with space for ORBITAL_MAXOBJ entries.
 outN : pointer to int; on success contains selected N.

 Returns:
   1  = OK (user pressed OK)
  -1  = Cancel (user cancelled)
*/
int oi_show_modal(SDL_Window *win, SDL_Renderer *rnd, Body out_bodies[], int *outN);

/* Optional: override built-in defaults */
void oi_set_defaults(const double *relx, const double *rely, const double *wvals, const int *gs, int count);

#endif /* ORBITAL_INPUT_H */