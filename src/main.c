#include <string.h>
#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <fileioc.h>
#include <debug.h>

#include "main.h"

void spawnAsteroid(int x, int y, int vel) {
	gfx_sprite_t* asteroidSkins[AST_SKINS] = {asteroid10, asteroid15, asteroid20, asteroid25};
	
	asteroids[asteroidArrayPtr] = (struct Asteroid) {	
		.sprite = 	asteroidSkins[randInt(0, AST_SKINS-1)],
		.x =		x,
		.y = 		y,
		.vel = 		vel,
		.active = 	1,
		.scorable = 1
	};
	
	asteroidArrayPtr = (asteroidArrayPtr >= MAX_ASTEROIDS-1) ? 0 : asteroidArrayPtr+1 ; // wrap array pointer if max number of asteroids reached
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
	int astro_y, score, prevScore, hiScore; 
	ti_var_t hiScoreVar;
		//astroOffset = -(astronaut->height/2);
	uint16_t i, j;
	float 	moveAcc, acc, vel, drag, minAstSec, maxAstSec;
	uint8_t gameState = MAIN_MENU, collided;
	char scoreText[ sizeof(scoreTextFormat)+10 ], hiScoreText[ sizeof(hiScoreTextFormat)+10 ];
	
	//struct Asteroid* curAst;
	
	gfx_UninitedSprite(emptySprite, 1, 1);
	
	struct Hitbox astronautHitboxes[] = {
		(struct Hitbox) { .x = 26,	.y = 1,  .width = 13, .height = 13 },	// head
		(struct Hitbox) { .x = 8,	.y = 10, .width = 19, .height = 23 },	// body
		(struct Hitbox) { .x = 10,	.y = 33, .width = 11, .height = 13 }
	};
	
	// -----------------------------------------------------------------------------
	
	// open highscore var
	ti_CloseAll();
	
	hiScoreVar = ti_Open(APPVAR_NAME, "w");
	if (hiScoreVar == 0) return 1;	// return error if file couldn't be opened
	
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
		
		astro_y = (PLAYFIELD_HEIGHT - astronaut->height)/2 + UPPER_BOUND;
		moveAcc = 0.5;
		acc = 0;
		vel = 0.0;
		drag = 0.2;
		minAstSec = 0.5;
		maxAstSec = 1.0;
		score = 0,
		prevScore = -1;	// setting this to -1 rather than 0 makes it draw the score when the game begins
	
		memset(scoreText, '\0', sizeof(scoreText));
		
		// Initialize asteroid array so it's not full of garbage that causes the game to end immediately
		for (i=0; i<sizeof(asteroids); i++) {
			asteroids[i] = (struct Asteroid) {emptySprite, 0, LOWER_BOUND+10, 0, 0, 0};
		}
		gfx_SetColor(BLACK);
		
		srandom(rtc_Time());
		
		timer_SetReload(1, 1*TIMER_FREQ);	// initialize system timer
		resetTimer(minAstSec, maxAstSec);
		
		hiScore = ti_Read(&hiScore, sizeof(hiScore), 1, hiScoreVar);
		
		// draw high score ----
		gfx_SetColor(WHITE);		
		sprintf(hiScoreText, hiScoreTextFormat, hiScore);
		gfx_PrintStringXY(hiScoreText, LCDX-3-gfx_GetStringWidth(hiScoreText), 3);
		gfx_SetColor(BLACK);
		
		while(gameState == GAME_RUNNING) {
			// Check for keys pressed -------------------------
			kb_Scan();
			
			switch (kb_Data[7]) {
				case kb_Down:	// can't do it properly since switches require integer constants becauuuuse reasons
					acc = (acc >= 0) ? moveAcc : 0;
					break;
				case kb_Up:
					acc = (acc <= 0) ? -moveAcc : 0;
					break;
				default:
					acc = 0;
			}
			
			if (kb_IsDown(kb_KeyClear)) gameState = MAIN_MENU;
			
			// Move astronaut ---------------------------------
			vel += acc;
			astro_y += vel;
			
				// if astronaut's position is going to be outside of the screen, constrain it
			if (astro_y > LOWER_BOUND-astronaut->height) {
				astro_y = LOWER_BOUND-astronaut->height;
				acc = vel = 0;
			} else if (astro_y < UPPER_BOUND) {
				astro_y = UPPER_BOUND;
				acc = vel = 0;
			}
			
			dbg_printf("acc: %f\n vel: %f\n\n", acc, vel);
			
			// Spawn an asteroid ----------------------------- (change to random interval at some point?)
			
			//dbg_sprintf(dbgout, "timer1 value: %lu\n", timer_GetSafe(1, TIMER_DOWN));
			//dbg_sprintf(dbgout, "ChkInterrupt: %d\n", timer_ChkInterrupt(1, TIMER_RELOADED));
			if (timer_ChkInterrupt(1, TIMER_RELOADED)) {
				spawnAsteroid(LCDX+MAX_AST_SIZE, randInt(0+MAX_AST_SIZE, PLAYFIELD_HEIGHT-MAX_AST_SIZE), 3);
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
						
						if (
							isBetween(	asteroids[i].y + asteroids[i].sprite->height,	// bottom of asteroid
										astro_y - astronaut->height*BONUS_SCORE_RANGE,	// top of astronaut + range
										astro_y	)										// top of astronaut
							||
							isBetween(	asteroids[i].y,														// top of asteroid
										astro_y + astronaut->height,										// bottom of astronaut
										astro_y + astronaut->height + astronaut->height*BONUS_SCORE_RANGE )	// bottom of astronaut + range
						) {
							score += AST_SCORE_VALUE;
						}
					}
				}
			}
			
			// Rendering --------------------------------------
			
				// Background -----------------
			gfx_FillRectangle(0, UPPER_BOUND, LCDX, PLAYFIELD_HEIGHT);
			
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
				// current score ----
				gfx_SetColor(BLACK);
				gfx_FillRectangle(0, 0, LCDX/2, SCOREBAR_HEIGHT-1);
				gfx_SetColor(WHITE);
				gfx_HorizLine(0, SCOREBAR_HEIGHT-1, LCDX);
				//gfx_SetColor(BLACK);
				
				sprintf(scoreText, scoreTextFormat, score);
				gfx_PrintStringXY(scoreText, 3, 3);
				
				prevScore = score;
				
				gfx_SetColor(BLACK);
			}
			
				// Debug ----------------------
			// Bonus score
			/* gfx_SetColor(WHITE);
			gfx_HorizLine(astro_x, asteroids[i].y + asteroids[i].sprite->height, astronaut->width);		// bottom of asteroid
			gfx_HorizLine(astro_x, astro_y - astronaut->height*BONUS_SCORE_RANGE, astronaut->width);	// top of astronaut + range
			gfx_HorizLine(astro_x, astro_y, astronaut->width);											// top of astronaut
			gfx_HorizLine(astro_x, asteroids[i].y, astronaut->width);														// top of asteroid
			gfx_HorizLine(astro_x, astro_y + astronaut->height, astronaut->width);											// bottom of astronaut
			gfx_HorizLine(astro_x, astro_y + astronaut->height + astronaut->height*BONUS_SCORE_RANGE, astronaut->width);	// bottom of astronaut + range
			gfx_SetColor(BLACK); */
			
			gfx_BlitBuffer();
			
			// Check for collisions ---------------------------
			collided = 0;
			
			for (j=0; j<MAX_ASTEROIDS; j++) {
				for (i=0; i<ASTRONAUT_HITBOXES; i++) {
					collided += gfx_CheckRectangleHotspot(
						astro_x + astronautHitboxes[i].x ,
						astro_y + astronautHitboxes[i].y ,
						astronautHitboxes[i].width ,
						astronautHitboxes[i].height ,
						
						asteroids[j].x ,
						asteroids[j].y ,
						asteroids[j].sprite->width ,
						asteroids[j].sprite->height
					);
					
					//dbg_sprintf(dbgout, "%d\n", collided);
					
					/* dbg_sprintf(dbgout, "%d\n", astro_x + astronautHitboxes[i].x);
					dbg_sprintf(dbgout, "%d\n", astro_y + astronautHitboxes[i].y );
					dbg_sprintf(dbgout, "%d\n",	astronautHitboxes[i].width );
					dbg_sprintf(dbgout, "%d\n\n",	astronautHitboxes[i].height );
						
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].x );
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].y );
					dbg_sprintf(dbgout, "%d\n",	asteroids[j].sprite->width );
					dbg_sprintf(dbgout, "%d\n\n\n",	asteroids[j].sprite->height); */
				}
			}
			
			//collided = 0;	// uncomment to disable collision
			
			if (collided) gameState = GAME_OVER;
		
			//delay(1000/60);
		}
		
		timer_Disable(1);
		
		while (gameState == GAME_OVER) {
			if (score > hiScore) hiScore = score;
			
			if (ti_Write(&hiScore, sizeof(hiScore), 1, hiScoreVar) != 1) return 1;
			
			
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
	
	timer_Disable(1);
	gfx_End();
	
	return 0;
}