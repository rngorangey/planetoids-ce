#include "gfx/gfx.h"

#define LCDX 320
#define LCDY 240
#define SCOREBAR_HEIGHT 16
#define UPPER_BOUND SCOREBAR_HEIGHT	// for clarity
#define LOWER_BOUND LCDY
#define PLAYFIELD_HEIGHT (LCDY-SCOREBAR_HEIGHT)

#define BLACK 1
#define WHITE 2

#define TIMER_FREQ 32768.0 // Hz
#define MAIN_MENU 1
#define GAME_RUNNING 2
#define GAME_OVER 3
#define CLOSE 10

#define AST_SKINS 4
#define AST_SEC 1.0
#define MAX_ASTEROIDS 20
#define MAX_AST_SIZE 25
#define ASTRO_HITBOXES 3
#define AST_SCORE_VALUE 10

//#define constrain(x, upper, lower) ((x > upper) ? upper : ((y < lower) ? (lower) : y));
// ---------------------------------------------------------

const char titleText[] = "PLANETOIDS";
const char subtitleText[] = "PRESS [alpha] TO START";
const char subSubtitleText[] = "PRESS [clear] TO RETURN TO TIOS";
const char scoreTextFormat[] = "SCORE: %d";

struct Asteroid {
	gfx_sprite_t* sprite;
	int x;
	int y;
	int vel;
	uint8_t active;	/* determines whether asteroids are moved and rendered each cycle;
						asteroid can be overwritten when 0*/
	uint8_t scorable;
};

// list of asteroids currently on screen
	/*  pointer wraps around to 0 after reaching the end; no more than 20 asteroids can be on screen at once. 
		older asteroids just get overwritten and appear to get deleted if more than 20 are on screen*/
struct Asteroid asteroids[MAX_ASTEROIDS];
int asteroidArrayPtr = 0;

//rectangle format required by gfx_CheckRectangleHotspot
struct Hitbox {	
	int x;	// x and y relative to astronaut's x and y
	int y;
	int width;
	int height;
};