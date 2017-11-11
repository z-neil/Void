/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Used to map a floating point zero-centred position into an array of discrete
 size.

 @title		Bins
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @since		2017-05 Generics.
			2016-01
			2015-06 */

#include <stdio.h> /* stderr */
#include <limits.h> /* INT_MAX */
#include "../general/OrthoMath.h" /* Rectangle4f, etc */
#include "Bins.h"

static const float epsilon = 0.0005f;

#define POOL_NAME Bin
#define POOL_TYPE int
#include "../templates/Pool.h"

enum BinsLayer { BIN_LAYER_SCREEN, BIN_LAYER_SPRITE, BIN_LAYER_NO };

struct Bins {
	int side_size;
	float half_space, one_each_bin;
	struct BinPool *pool[BIN_LAYER_NO];
};

void Bins_(struct Bins **const pthis) {
	struct Bins *this;
	unsigned i;
	if(!pthis || !(this = *pthis)) return;
	for(i = 0; i < BIN_LAYER_NO; i++) BinPool_(&this->pool[i]);
	this = *pthis = 0;
}

/** @param side_size: The size of a side; the {BinsTake} returns
 {[0, side_size^2-1]}.
 @param each_bin: How much space per bin.
 @fixme Have max values. */
struct Bins *Bins(const size_t side_size, const float each_bin) {
	struct Bins *this;
	unsigned i;
	if(!side_size || side_size > INT_MAX || each_bin < epsilon)
		{ fprintf(stderr, "Bins: parameters out of range.\n"); return 0; }
	if(!(this = malloc(sizeof *this))) { perror("Bins"); Bins_(&this);return 0;}
	this->side_size = (int)side_size;
	this->half_space = (float)side_size * each_bin / 2.0f;
	this->one_each_bin = 1.0f / each_bin;
	this->pool[0] = this->pool[1] = 0;
	for(i = 0; i < BIN_LAYER_NO; i++) {
		if(!(this->pool[i] = BinPool(0, 0))) { fprintf(stderr, "Bins: %s.\n",
			BinPoolGetError(0)); Bins_(&this); return 0; }
	}
	return this;
}

unsigned BinsVector(struct Bins *const this, struct Vec2f *const vec) {
	struct Vec2i v2i;
	if(!this || !vec) return 0;
	v2i.x = (vec->x + this->half_space) * this->one_each_bin;
	if(v2i.x < 0) v2i.x = 0;
	else if(v2i.x >= this->side_size) v2i.x = this->side_size - 1;
	v2i.y = (vec->y + this->half_space) * this->one_each_bin;
	if(v2i.y < 0) v2i.y = 0;
	else if(v2i.y >= this->side_size) v2i.y = this->side_size - 1;
	return v2i.y * this->side_size + v2i.x;
}

/** Private code for \see{BinsSet*Rectangle}.
 @return Success. */
static int set_rect_layer(struct Bins *const this,
	struct Rectangle4f *const rect, struct BinPool *const layer) {
	struct Rectangle4i bin4;
	struct Vec2i bin2i;
	int *bin;
	assert(this && rect && layer); /* ALSO layer \in this */
	BinPoolClear(layer);
	/* Map floating point rectangle {rect} to index rectangle {bin4}. */
	bin4.x_min = (rect->x_min + this->half_space) * this->one_each_bin;
	if(bin4.x_min < 0) bin4.x_min = 0;
	bin4.x_max = (rect->x_max + this->half_space) * this->one_each_bin;
	if(bin4.x_max >= this->side_size) bin4.x_max = this->side_size - 1;
	bin4.y_min = (rect->y_min + this->half_space) * this->one_each_bin;
	if(bin4.y_min < 0) bin4.y_min = 0;
	bin4.y_max = (rect->y_max + this->half_space) * this->one_each_bin;
	if(bin4.y_max >= this->side_size) bin4.y_max = this->side_size - 1;
	/* Generally goes faster when you follow the scan-lines; not sure whether
	 this is so important with buffering. */
	for(bin2i.y = bin4.y_max; bin2i.y >= bin4.y_min; bin2i.y--) {
		for(bin2i.x = bin4.x_min; bin2i.x <= bin4.x_max; bin2i.x++) {
			/*printf("sprite bin2i(%u, %u)\n", bin2i.x, bin2i.y);*/
			if(!(bin = BinPoolNew(layer))) { fprintf(stderr,
				"Bins rectangle: %s.\n", BinPoolGetError(layer)); return 0; }
			*bin = bin2i.y * this->side_size + bin2i.x;
			/*bin_to_Vec2i(bin2i_to_fg_bin(bin2i), &bin_pos);
			fprintf(gnu_glob, "set arrow from %f,%f to %d,%d lw 0.2 "
				"lc rgb \"#CCEEEE\" front;\n",
				this->x_5.x, this->x_5.y, bin_pos.x, bin_pos.y);*/
		}
	}
	return 1;	
}

/** Set screen rectangle.
 @return Success. */
int BinsSetScreenRectangle(struct Bins *const this,
	struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, this->pool[BIN_LAYER_SCREEN]);
}

/** Set sprite rectangle.
 @return Success. */
int BinsSetSpriteRectangle(struct Bins *const this,
	struct Rectangle4f *const rect) {
	if(!this || !rect) return 0;
	return set_rect_layer(this, rect, this->pool[BIN_LAYER_SPRITE]);
}

/** Private code for \see{BinsForEach*}. */
static void for_each(struct Bins *const this, const BinsAction action,
	struct BinPool *const layer) {
	size_t i = 0;
	assert(this && action && layer); /* layer \in this, don't check for this */
	while(BinPoolIsElement(layer, i))
		action(*BinPoolGetElement(layer, i++));
}

/** For each bin on screen. */
void BinsForEachScreen(struct Bins *const this, const BinsAction action) {
	if(!this || !action) return;
	for_each(this, action, this->pool[BIN_LAYER_SCREEN]);
}

/** For each bin crossing the sprite. */
void BinsForEachSprite(struct Bins *const this, const BinsAction action) {
	if(!this || !action) return;
	for_each(this, action, this->pool[BIN_LAYER_SPRITE]);
}
