#include <string.h>
#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>

#include "gfx/gfx.h"

#define LCDX 320
#define LCDY 240
#define SCOREBAR_HEIGHT 16
#define PLAYFIELD_Y (LCDY-SCOREBAR_HEIGHT)

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

void spawnAsteroid(int x, int y, int vel) {
	gfx_sprite_t* asteroidSkins[AST_SKINS] = {asteroid10, asteroid15, asteroid20, asteroid25};
	int tmp;
	
	asteroids[asteroidArrayPtr] = (struct Asteroid) {	
		.sprite = 	asteroidSkins[randInt(0, AST_SKINS-1)],
		.x =		x,
		.y = 		y,
		.vel = 		vel,
		.active = 	1,
		.scorable = 1
	};
	
	asteroidArrayPtr = ( (tmp = asteroidArrayPtr+1) >= MAX_ASTEROIDS ) ? 0 : tmp ; // wrap array pointer if max number of asteroids reached
}

void resetTimer(float min, float max) {
	// generate a random float between max and min to set the timer with
	float randDelay = (((float)(random()&0x0000FFFF) * (max-min)) / 0xFFFF ) + min;
	//dbg_sprintf(dbgout, "randDelay: %f\n", randDelay);
	
	timer_AckInterrupt(1, TIMER_RELOADED);
	timer_Disable(1);
	timer_Set		(1, (int) (randDelay * TIMER_FREQ));
	//timer_SetReload	(1, (int) (randDelay * TIMER_FREQ));	// not needed since timer is never allowed to cycle
	timer_Enable(1, TIMER_32K, TIMER_0INT, TIMER_DOWN);
}

int main() {
	const int astro_x = 80;
	int astro_y, score, prevScore; 
		//astroOffset = -(astronaut->height/2);
	uint16_t i, j;
	float 	moveVel, vel, drag, minAstSec, maxAstSec;
	uint8_t key, gameState = MAIN_MENU, collided;
	const char titleText[] = "PLANETOIDS";
	const char subtitleText[] = "PRESS [alpha] TO START";
	const char subSubtitleText[] = "PRESS [clear] TO RETURN TO MENU";
	const char scoreTextFormat[] = "SCORE: %d";
	char scoreText[ sizeof(scoreTextFormat)+10 ];
	
	//struct Asteroid* curAst;
	
	gfx_UninitedSprite(emptySprite, 1, 1);
	
	struct Hitbox astroHitboxes[] = {
		(struct Hitbox) { .x = 26,	.y = 1,  .width = 13, .height = 13 },	// head
		(struct Hitbox) { .x = 8,	.y = 10, .width = 19, .height = 23 },	// body
		(struct Hitbox) { .x = 10,	.y = 33, .width = 11, .height = 13 }
	};
	
	// -----------------------------------------------------------------------------
	
	gfx_Begin();
	gfx_SetDrawBuffer();
	
	gfx_SetPalette(global_palette, sizeof_global_palette, 0);
	gfx_SetTransparentColor(0);
	
	while (gameState != CLOSE) {
		if (gameState == MAIN_MENU) {
			gfx_FillScreen(BLACK);
			gfx_SetTextFGColor(WHITE);
			
			gfx_SetTextScale(2, 2);
			gfx_PrintStringXY(titleText, (LCDX-gfx_GetStringWidth(titleText))/2, 100);
			
			gfx_SetTextScale(1, 1);
			gfx_PrintStringXY(subtitleText, (LCDX-gfx_GetStringWidth(subtitleText))/2, 130);
			gfx_PrintStringXY(subSubtitleText, (LCDX-gfx_GetStringWidth(subSubtitleText))/2, 140);
			
			gfx_BlitBuffer();
			
			delay(100);	// so that the game doesn't immediately exit if coming from GAME_RUNNING
			
			while (gameState == MAIN_MENU) {
				switch (os_GetCSC()) {
					case sk_Alpha:
						gameState = GAME_RUNNING;
						break;
					case sk_Clear:
						gameState = CLOSE;
						break;
				}
			}
		}
		
		// ----------------------------------------------------------------------------
		
	// Initialize stuff
		gfx_FillScreen(BLACK);
		
		astro_y = (PLAYFIELD_Y - astronaut->height)/2 + SCOREBAR_HEIGHT;
		moveVel = 5;
		vel = 0.0;
		drag = 0.2;
		minAstSec = 0.5;
		maxAstSec = 1.0;
		score = 0,
		prevScore = -1;
	
		memset(scoreText, '\0', sizeof(scoreText));
		
		// Initialize asteroid array so it's not full of garbage that causes the game to end immediately
		for (i=0; i<sizeof(asteroids); i++) {
			asteroids[i] = (struct Asteroid) {emptySprite, 0, LCDY+10, 0, 0, 0};
		}
		gfx_SetColor(BLACK);
		
		srandom(rtc_Time());
		
		timer_SetReload(1, 1*TIMER_FREQ);	// initialize system timer
		resetTimer(minAstSec, maxAstSec);
		
		while(gameState == GAME_RUNNING) {
			// Check for keys pressed -------------------------
			kb_Scan();
			
			switch (key = kb_Data[7]) {
				case kb_Down:
					vel = moveVel;
					break;
				case kb_Up:
					vel = -moveVel;
					break;
				default:
					// apply drag to the velocity to slow the astronaut down if no key is pressed, until a certain point where it's considered to be zero
					vel = (vel > drag) ? 
						vel - drag : 
						(vel < drag) ?
							vel + drag :
							0;
			}
			
			if (kb_Data[6] & kb_Clear) {
				gameState = MAIN_MENU;
			}
			
			// Move astronaut ---------------------------------
			astro_y += vel;
				// if astronaut's position is going to be outside of the screen, constrain it
			astro_y = (int) (astro_y > LCDY-astronaut->height) ?
				LCDY-astronaut->height : 
				(astro_y < SCOREBAR_HEIGHT) ? SCOREBAR_HEIGHT : astro_y;
			
			// Spawn an asteroid ----------------------------- (change to random interval at some point?)
			
			//dbg_sprintf(dbgout, "timer1 value: %lu\n", timer_GetSafe(1, TIMER_DOWN));
			//dbg_sprintf(dbgout, "ChkInterrupt: %d\n", timer_ChkInterrupt(1, TIMER_RELOADED));
			if (timer_ChkInterrupt(1, TIMER_RELOADED)) {
				spawnAsteroid(LCDX+MAX_AST_SIZE, randInt(0+MAX_AST_SIZE, PLAYFIELD_Y-MAX_AST_SIZE), 3);
				resetTimer(minAstSec, maxAstSec);
			}
			
			// Move asteroids --------------------------------- 
				// (combine with rendering code later if possible for speedz?)
			for (i=0; i<MAX_ASTEROIDS; i++) {
				if (asteroids[i].active) {
					asteroids[i].x -= (int) asteroids[i].vel;	// subtraction to move them to the left
					
					if (asteroids[i].x < 0-MAX_AST_SIZE) {
						asteroids[i].active = 0;
						//dbg_sprintf(dbgout, "%d.active: %d\n", i, curAst->active);
					}
					
					if (asteroids[i].x < astro_x && asteroids[i].scorable) {
						score += AST_SCORE_VALUE;	// increment score for each asteroid passed while we're dealing with positions
						asteroids[i].scorable = 0;
					}
				}
			}
			
			// Rendering --------------------------------------
			
				// Background -----------------
			gfx_FillRectangle(0, SCOREBAR_HEIGHT, LCDX, PLAYFIELD_Y);
			
				// Astronaut ------------------
			gfx_TransparentSprite(astronaut, astro_x, astro_y);
			
				// Asteroids ------------------
			for (i=0; i<MAX_ASTEROIDS; i++) {
				if (asteroids[i].active) {
					gfx_TransparentSprite(asteroids[i].sprite, asteroids[i].x, asteroids[i].y);
				}
			}
			
				// Score ----------------------
			if (score != prevScore) {
				gfx_SetColor(BLACK);
				gfx_FillRectangle(0, 0, LCDX, SCOREBAR_HEIGHT-1);
				gfx_SetColor(WHITE);
				gfx_HorizLine(0, SCOREBAR_HEIGHT-1, LCDX);
				gfx_SetColor(BLACK);
				
				sprintf(scoreText, scoreTextFormat, score);
				gfx_PrintStringXY(scoreText, 3, 3);
				
				prevScore = score;
			}
			
			gfx_BlitBuffer();
			
			// Check for collisions ---------------------------
			collided = 0;
			
			for (j=0; j<MAX_ASTEROIDS; j++) {
				for (i=0; i<ASTRO_HITBOXES; i++) {
					collided += gfx_CheckRectangleHotspot(
						astro_x + astroHitboxes[i].x ,
						astro_y + astroHitboxes[i].y ,
						astroHitboxes[i].width ,
						astroHitboxes[i].height ,
						
						asteroids[j].x ,
						asteroids[j].y ,
						asteroids[j].sprite->width ,
						asteroids[j].sprite->height
					);
					
					//dbg_sprintf(dbgout, "%d\n", collided);
					
					/* dbg_sprintf(dbgout, "%d\n", astro_x + astroHitboxes[i].x);
					dbg_sprintf(dbgout, "%d\n", astro_y + astroHitboxes[i].y );
					dbg_sprintf(dbgout, "%d\n",	astroHitboxes[i].width );
					dbg_sprintf(dbgout, "%d\n\n",	astroHitboxes[i].height );
						
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].x );
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].y );
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].sprite->width );
					dbg_sprintf(dbgout, "%d\n\n\n",	asteroids[j].sprite->height); */
				}
			}
			
			if (collided) gameState = GAME_OVER;
		
			//delay(1000/60);
		}
		
		timer_Disable(1);
		
		while (gameState == GAME_OVER) {
			switch (os_GetCSC()) {
				case sk_Alpha:
					gameState = GAME_RUNNING;
					break;
				case sk_Clear:
					gameState = MAIN_MENU;
					break;
			}
		}
	}
	
	gfx_End();
	
	return 0;
}