#include <stdint.h>
#include "../general/OrthoMath.h"
struct Rectangle4f;
struct Sprite;
struct Layer;
typedef void (*LayerAction)(const unsigned);
typedef void (*LayerNoSpriteAction)(const unsigned, const unsigned,
	struct Sprite *const*const);

void Layer_(struct Layer **const pthis);
struct Layer *Layer(const size_t size_side, const float each_bin);
unsigned LayerGetOrtho(const struct Layer *const this, struct Ortho3f *const o);
int LayerSetScreenRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
int LayerSetSpriteRectangle(struct Layer *const this,
	struct Rectangle4f *const rect);
void LayerSetRandom(struct Layer *const this, struct Ortho3f *const o);
void LayerForEachScreen(struct Layer *const this, const LayerAction action);
void LayerSpriteForEachSprite(struct Layer *const this,
	struct Sprite *const*const sprite_ref, const LayerNoSpriteAction action);
