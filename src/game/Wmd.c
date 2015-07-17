/** Copyright 2015 Neil Edelman, distributed under the terms of the GNU General
 Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* sinf, cosf */
#include <string.h> /* memcpy */
#include "Sprite.h"
#include "Wmd.h"
#include "Light.h"
#include "../system/Timer.h"

/* Wmd is cannons, bombs, and other cool stuff that has sprites and can hurt.

 @author	Neil
 @version	3.2, 2015-06
 @since		3.2, 2015-06 */

static const float shot_speed    = 300.0f/*2600.0f*/;/* pixel / s */
static const float shot_distance = 16.0f + 4.0f + 1.0f; /* pixel; (5.0 :] 3.0 :[) so it won't hit the ship; fixme: this is such a hack) */ /* fixme: have size/2+1 */
const static int   shot_range    = 768;  /* ms */
const static int   shot_damage   = 32;   /* joule */
const static int   shot_rechage  = 256;  /* ms */
const static float shot_impact   = 6.0f; /* tonne pixels / s */

struct Wmd {
	struct Sprite *sprite;
	int           expires;
	struct Sprite *from;
	int           light;
	/* fixme: resource */
} wmds[1024];
static const int wmds_capacity = sizeof(wmds) / sizeof(struct Wmd);
static int       wmds_size;

/* public */

/** Get a new wmds from the pool of unused.
 @param		Optional ship.
 @return	A Wmd or null. */
struct Wmd *Wmd(struct Sprite *const from, const int colour) {
	struct Wmd *wmd;
	float x = 0.0f, y = 0.0f, theta = 0.0f;
	float c, s, vx, vy;

	if(!from) {
		fprintf(stderr, "Wmd: invalid.\n");
		return 0;
	}
	if(wmds_size >= wmds_capacity || (unsigned)colour > 2) {
		fprintf(stderr, "Wmd: couldn't be created; %u/%u.\n", wmds_size, wmds_capacity);
		return 0;
	}
	wmd = &wmds[wmds_size];
	if(!(wmd->sprite = Sprite(S_WMD, 5, 8))) return 0; /* FIXME */
	SpriteSetContainer(wmd, &wmd->sprite);
	wmd->expires= TimerLastTime() + shot_range;
	wmd->from   = from;
	/* don't really care if it fails */
	wmd->light  = Light(64.0f, colour == 0 ? 1.0f : 0.0f, colour == 1 ? 1.0f : 0.0f, colour == 2 ? 1.0f : 0.0f);
	LightSetContainer(&wmd->light);
	wmds_size++;
	/* set to ship's position */
	SpriteGetOrientation(from, &x, &y, &theta);
	c = cosf(theta);
	s = sinf(theta);
	/* fixme: have bounding circle */
	SpriteSetOrientation(wmd->sprite, x + c*shot_distance, y + s*shot_distance, theta);
	SpriteGetVelocity(from, &vx, &vy);
	SpriteSetVelocity(wmd->sprite, vx + c*shot_speed, vy + s*shot_speed);

	fprintf(stderr, "Wmd: created from pool, Wmd%u->Lgt%u,Spr%u.\n", WmdGetId(wmd), wmd->light, SpriteGetId(wmd->sprite));
	/*SpriteGetOrientation(wmd->sprite, &x, &y, &theta);
	fprintf(stderr, "Wmd: %f %f %f\n", x, y, theta);*/

	return wmd;
}

/** Erase a wmds from the pool (array of static wmds.)
 @param wmds_ptr	A pointer to the wmds; gets set null on success. */
void Wmd_(struct Wmd **wmd_ptr) {
	struct Wmd *wmd;
	int index;

	if(!wmd_ptr || !(wmd = *wmd_ptr)) return;
	index = wmd - wmds;
	if(index < 0 || index >= wmds_size) {
		fprintf(stderr, "~Wmd: Wmd%u not in range Wmd%u.\n", index + 1, wmds_size);
		return;
	}
	fprintf(stderr, "~Wmd: returning to pool, Wmd%u->Lgt%u,Spr%u.\n", WmdGetId(wmd), wmd->light, SpriteGetId(wmd->sprite));

	/* superclasses */
	Sprite_(&wmd->sprite);
	Light_(wmd->light);

	/* move the terminal wmd to replace this one */
	if(index < --wmds_size) {
		memcpy(wmd, &wmds[wmds_size], sizeof(struct Wmd));
		SpriteSetContainer(wmd, &wmd->sprite);
		LightSetContainer(&wmd->light);
		fprintf(stderr, "~Wmd: Wmd%u has become Wmd%u.\n", wmds_size + 1, WmdGetId(wmd));
	}

	*wmd_ptr = wmd = 0;
}

int WmdGetExpires(const struct Wmd *wmd) {
	if(!wmd) return 0;
	return wmd->expires;
}

float WmdGetImpact(const struct Wmd *wmd) {
	if(!wmd) return 0.0f;
	return shot_impact;
}

int WmdGetDamage(const struct Wmd *wmd) {
	if(!wmd) return 0;
	return shot_damage;
}

/** Updates the light to be at the Wmd.
 @param wmd		The Wmd. */
void WmdUpdateLight(struct Wmd *wmd) {
	float x, y, theta;

	if(!wmd) return;
	SpriteGetOrientation(wmd->sprite, &x, &y, &theta);
	LightSetPosition(wmd->light, x, y);
}

int WmdGetId(const struct Wmd *wmd) {
	if(!wmd) return 0;
	return (int)(wmd - wmds) + 1;
}