/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Collision detection and resolution.

 @title		Sprites
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @fixme Collision resolution wonky. */

#include "../general/OrthoMath.h" /* Vectors. */
#include "Collision.h"

#if 0

CollisionPool_(&collisions);

/* Collision is a self-referential stack; this always must be here. */
/*CollisionPoolPoolMigrate(collisions, &Collision_migrate);*/
|| !(collisions = CollisionPool())
static struct CollisionPool *collisions;
/** This is a stack. The {Pool} doesn't know that {Collision} is
 self-referential, so we have to inject code into the {realloc};
 \see{Collision_migrate}.
 @implements <Sprite, Migrate>DiAction */
static void Sprite_migrate_collisions(struct Sprite *const this,
									  void *const migrate_void) {
	const struct Migrate *const migrate = migrate_void;
	struct Collision *c;
	char a[12];
	assert(this && migrate);
	Sprite_to_string(this, &a);
	if(!this->collision_set) return;
	CollisionMigrate(migrate, &this->collision_set);
	/*printf(" %s: [%p, %p]->%lu: ", a,
	 migrate->begin, migrate->end, (unsigned long)migrate->delta);*/
	/*c = this->collision_set, printf("(collision (%.1f, %.1f):%.1f)", c->v.x, c->v.y, c->t);*/
	for(c = this->collision_set; c->next; c = c->next) {
		CollisionMigrate(migrate, &c->next);
		/*printf("(next (%.1f, %.1f):%.1f)", c->v.x, c->v.y, c->t);*/
	}
	/*printf(".\n");*/
	/*this->collision_set = 0;*/
}
/** @implements <Bin, Migrate>BiAction */
static void Bin_migrate_collisions(struct SpriteList **const pthis,
								   void *const migrate_void) {
	struct SpriteList *const this = *pthis;
	assert(pthis && this && migrate_void);
	SpriteListBiForEach(this, &Sprite_migrate_collisions, migrate_void);
}
/** @implements Migrate */
static void Collision_migrate(const struct Migrate *const migrate) {
	printf("Collision migrate [%p, %p]->%lu.\n", migrate->begin,
		   migrate->end, (unsigned long)migrate->delta);
	BinPoolBiForEach(update_bins, &Bin_migrate_collisions, (void *const)migrate);
	BinPoolBiForEach(draw_bins, &Bin_migrate_collisions, (void *const)migrate);
}

/* branch cut (-Pi,Pi]? */

/** Called from \see{elastic_bounce}. */
static void apply_bounce_later(struct Sprite *const this,
							   const struct Vec2f *const v, const float t) {
	struct Collision *const c = CollisionPoolNew(collisions);
	char a[12];
	assert(this && v && t >= 0.0f && t <= dt_ms);
	if(!c) { fprintf(stderr, "Collision set: %s.\n",
					 CollisionPoolGetError(collisions)); return; }
	c->next = this->collision_set;
	c->v.x = v->x, c->v.y = v->y;
	c->t = t;
	this->collision_set = c;
	Sprite_to_string(this, &a), printf("apply_bounce_later: %s.\n", a);
	/*for(i = this->collision_set; i; i = i->next) {
	 printf("(%.1f, %.1f):%.1f, ", i->v.x, i->v.y, i->t);
	 }
	 printf("DONE\n");*/
}

/** Elastic collision between circles; use this when we've determined that {a}
 collides with {b} at {t0_dt} past frame time. Called explicitly from collision
 handlers. Also, it may alter (fudge) the positions a little to avoid
 interpenetration.
 @param t0_dt: The added time that the collision occurs in {ms}. */
static void elastic_bounce(struct Sprite *const a, struct Sprite *const b,
						   const float t0_dt) {
	struct Vec2f ac, bc, c, a_nrm, a_tan, b_nrm, b_tan, v;
	float nrm, dist_c_2, dist_c, min;
	const float a_m = a->vt->get_mass(a), b_m = b->vt->get_mass(b),
	diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* fixme: float stored in memory? */
	
	/* Check that they are not stuck together first; we absolutely do not want
	 objects to get stuck orbiting each other. (Allow a little room?) */
	c.x = b->x.x - a->x.x, c.y = b->x.y - a->x.y;
	dist_c_2 = c.x * c.x + c.y * c.y;
	min = a->bounding + b->bounding;
	if(dist_c_2 < min * min * 0.94f) {
		float push;
		dist_c = sqrtf(dist_c_2);
		push = (min - dist_c) * 0.5f;
		printf("elastic_bounce: degeneracy pressure pushing sprites "
			   "(%.1f,%.1f) apart.\n", push, push);
		/* Nomalise {c}. */
		if(dist_c < epsilon) {
			c.x = 1.0f, c.y = 0.0f;
		} else {
			const float one_dist = 1.0f / dist_c;
			c.x *= one_dist, c.y *= one_dist;
		}
		/* Degeneracy pressure. */
		a->x.x -= c.x * push, a->x.y -= c.y * push;
		b->x.x += c.x * push, b->x.y += c.y * push;
	}
	/* Extrapolate position of collision. */
	ac.x = a->x.x + a->v.x * t0_dt, ac.y = a->x.y + a->v.y * t0_dt;
	bc.x = b->x.x + b->v.x * t0_dt, bc.y = b->x.y + b->v.y * t0_dt;
	/* At point of impact. */
	c.x = bc.x - ac.x, c.y = bc.y - ac.y;
	dist_c = sqrtf(c.x * c.x + c.y * c.y);
	/* Nomalise {c}. */
	if(dist_c < epsilon) {
		c.x = 1.0f, c.y = 0.0f;
	} else {
		const float one_dist = 1.0f / dist_c;
		c.x *= one_dist, c.y *= one_dist;
	}
	/* {a}'s velocity, normal direction. */
	nrm = a->v.x * c.x + a->v.y * c.y;
	a_nrm.x = nrm * c.x, a_nrm.y = nrm * c.y;
	/* {a}'s velocity, tangent direction. */
	a_tan.x = a->v.x - a_nrm.x, a_tan.y = a->v.y - a_nrm.y;
	/* {b}'s velocity, normal direction. */
	nrm = b->v.x * c.x + b->v.y * c.y;
	b_nrm.x = nrm * c.x, b_nrm.y = nrm * c.y;
	/* {b}'s velocity, tangent direction. */
	b_tan.x = b->v.y - b_nrm.x, b_tan.y = b->v.y - b_nrm.y;
	/* Elastic collision. */
	v.x = a_tan.x +  (a_nrm.x*diff_m + 2*b_m*b_nrm.x) * invsum_m;
	v.y = a_tan.y +  (a_nrm.y*diff_m + 2*b_m*b_nrm.y) * invsum_m;
	assert(!isnan(v.x) && !isnan(v.y));
	apply_bounce_later(a, &v, t0_dt);
	v.x = b_tan.x + (b_nrm.x*diff_m + 2*a_m*a_nrm.x) * invsum_m;
	v.y = b_tan.y + (b_nrm.y*diff_m + 2*a_m*a_nrm.y) * invsum_m;
	assert(!isnan(v.x) && !isnan(v.y));
	apply_bounce_later(b, &v, t0_dt);
}

/** Called from \see{sprite_sprite_collide} to give the next level of detail
 with time.
 @param t0_ptr: If true, sets the time of collision past the current frame in
 {ms}.
 @return If {u} and {v} collided. */
static int collide_circles(const struct Sprite *const a,
						   const struct Sprite *const b, float *t0_ptr) {
	/* t \in [0,1]
	 u = a.dx
	 v = b.dx
	 if(v-u ~= 0) t = doesn't matter, parallel-ish
	 p(t) = a0 + ut
	 q(t) = b0 + vt
	 distance(t)   = |q(t) - p(t)|
	 distance^2(t) = (q(t) - p(t))^2
	 = ((b0+vt) - (a0+ut))^2
	 = ((b0-a0) + (v-u)t)^2
	 = (v-u)*(v-u)t^2 + 2(b0-a0)*(v-u)t + (b0-a0)*(b0-a0)
	 0 = 2(v-u)*(v-u)t_min + 2(b0-a0)*(v-u)
	 t_min = -(b0-a0)*(v-u)/(v-u)*(v-u)
	 this is an optimisation, if t \notin [0,frame] then pick the closest; if
	 distance^2(t_min) < r^2 then we have a collision, which happened at t0,
	 r^2 = (v-u)*(v-u)t0^2 + 2(b0-a0)*(v-u)t0 + (b0-a0)*(b0-a0)
	 t0 = [-2(b0-a0)*(v-u) - sqrt((2(b0-a0)*(v-u))^2
	 - 4((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / 2(v-u)*(v-u)
	 t0 = [-(b0-a0)*(v-u) - sqrt(((b0-a0)*(v-u))^2
	 - ((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / (v-u)*(v-u) */
	/* fixme: float stored in memory? can this be more performant? with
	 doubles? eww overkill */
	struct Vec2f v, z, dist;
	float t, v2, vz, z2, dist_2, det;
	const float r = a->bounding + b->bounding;
	assert(a && b && t0_ptr);
	v.x = a->v.x - b->v.x, v.y = a->v.y - b->v.y;
	z.x = a->x.x - b->x.x, z.y = a->x.y - b->x.y;
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	/* Time of closest approach. */
	if(v2 < epsilon) {
		t = 0.0f;
	} else {
		t = -vz / v2;
		if(     t < 0.0f)  t = 0.0f;
		else if(t > dt_ms) t = dt_ms;
	}
	/* Distance(t_min). */
	dist.x = z.x + v.x * t, dist.y = z.y + v.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	if(dist_2 >= r * r) return 0;
	/* The first root or zero is when the collision happens. */
	z2 = z.x * z.x + z.y * z.y;
	det = (vz * vz - v2 * (z2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-vz - sqrtf(det)) / v2;
	if(t < 0.0f) t = 0.0f; /* bounded, dist < r^2 */
	*t0_ptr = t;
	return 1;
}

/** This first applies the most course-grained collision detection in pure
 two-dimensional space using {x_5} and {bounding1}. If passed, calls
 \see{collide_circles} to introduce time.
 @implements <Sprite, Sprite>BiAction */
static void sprite_sprite_collide(struct Sprite *const this,
	void *const void_that) {
	struct Sprite *const that = void_that;
	struct Vec2f diff;
	float bounding1, t0;
	char a[12], b[12];
	assert(this && that);
	/* Break symmetry -- if two objects are near, we only need to report it
	 once. */
	if(this >= that) return;
	Sprite_to_string(this, &a);
	Sprite_to_string(that, &b);
	/* Calculate the distance between; the sum of {bounding1}, viz, {bounding}
	 projected on time for easy discard. */
	diff.x = this->x_5.x - that->x_5.x, diff.y = this->x_5.y - that->x_5.y;
	bounding1 = this->bounding1 + that->bounding1;
	if(diff.x * diff.x + diff.y * diff.y >= bounding1 * bounding1) return;
	if(!collide_circles(this, that, &t0)) {
		/* Not colliding. */
		/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
		 "lc rgb \"#AA44DD\" front;\n", this->x.x, this->x.y, that->x.x,
		 that->x.y);*/
		return;
	}
	/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
	 "lc rgb \"red\" front;\n", this->x.x, this->x.y, that->x.x, that->x.y);*/
	/* fixme: collision matrix */
	elastic_bounce(this, that, t0);
	/*printf("%.1f ms: %s, %s collide at (%.1f, %.1f)\n", t0, a, b, this->x.x, this->x.y);*/
}
/** @implements <Bin, Sprite *>BiAction */
static void bin_sprite_collide(struct SpriteList **const pthis,
							   void *const target_param) {
	struct SpriteList *const this = *pthis;
	struct Sprite *const target = target_param;
	struct Vec2i b2;
	const unsigned b = (unsigned)(this - bins);
	bin_to_fg_bin2i(b, &b2);
	/*printf("bin_collide_sprite: bin %u (%u, %u) Sprite: (%f, %f).\n", b, b2.x,
	 b2.y, target->x.x, target->x.y);*/
	SpriteListUnorderBiForEach(this, &sprite_sprite_collide, target);
}
/** The sprites have been ordered by their {x_5}. Now we can do collisions.
 This places a list of {Collide} in the sprite.
 @implements <Sprite>Action */
static void collide(struct Sprite *const this) {
	/*SpriteBiAction biact = &collision_detection_and_response;*/
	/*printf("--Sprite bins:\n");*/
	sprite_new_bins(this);
	/*printf("--Checking:\n");*/
	BinPoolBiForEach(sprite_bins, &bin_sprite_collide, this);
}

/* Branch cut to the principal branch (-Pi,Pi] for numerical stability. We
 should really use normalised {ints}, so this would not be a problem, but
 {OpenGl} doesn't like that. */
static void branch_cut_pi_pi(float *theta_ptr) {
	assert(theta_ptr);
	*theta_ptr -= M_2PI_F * floorf((*theta_ptr + M_PI_F) / M_2PI_F);
}

/** Relies on \see{extrapolate}; all pre-computation is finalised in this step
 and values are advanced. Collisions are used up and need to be cleared after.
 @implements <Sprite>Action */
static void timestep(void) {
	struct Collision *c;
	float t0 = dt_ms;
	struct Vec2f v1 = { 0.0f, 0.0f }, d;
	assert(sprites);
	/* The earliest time to collision and sum the collisions together. */
	for(c = sprites->collision_set; c; c = c->next) {
		/*char a[12];*/
		d.x = sprites->x.x + sprites->v.x * c->t;
		d.y = sprites->x.y + sprites->v.y * c->t;
		/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f lw 0.5 "
		 "lc rgb \"#EE66AA\" front;\n", d.x, d.y,
		 d.x + c->v.x * (1.0f - c->t), d.y + c->v.y * (1.0f - c->t));*/
		/*this->vt->to_string(this, &a);
		 printf("%s collides at %.1f and ends up going (%.1f, %.1f).\n", a,
		 c->t, c->v.x * 1000.0f, c->v.y * 1000.0f);*/
		if(c->t < t0) t0 = c->t;
		v1.x += c->v.x, v1.y += c->v.y;
		/* fixme: stability! do a linear search O(n) to pick out the 2 most
		 recent, then divide by, { 1, 2, 4, 4, 4, . . . }? */
	}
	sprites->x.x = sprites->x.x + sprites->v.x * t0 + v1.x * (dt_ms - t0);
	sprites->x.y = sprites->x.y + sprites->v.y * t0 + v1.y * (dt_ms - t0);
	if(sprites->collision_set) {
		sprites->collision_set = 0;
		sprites->v.x = v1.x, sprites->v.y = v1.y;
	}
	/* Angular velocity. */
	sprites->x.theta += sprites->v.theta /* omega */ * dt_ms;
	branch_cut_pi_pi(&sprites->x.theta);
}

/* branch cut (-Pi,Pi] */
/* branch cut (-Pi,Pi] */

/** Elastic collision between circles; called from \see{collide} with
 {@code t0_dt} from \see{collide_circles}. Use this when we've determined that
 {@code Sprite a} collides with {@code Sprite b} at {@code t0_dt}, where
 Sprites' {@code x, y} are at time 0 and {@code x1, y1} are at time 1 (we will
 not get here, just going towards.) Velocities are {@code vx, vy} and this
 function is responsible for setting {@code vx1, vy1}, after the collision.
 Also, it may alter (fudge) the positions a little to avoid interpenetration.
 @param a		Sprite a.
 @param b		Sprite b.
 @param t0_dt	The fraction of the frame that the collision occurs, [0, 1]. */
static void elastic_bounce(struct Sprite *u, struct Sprite *v, const float t0_dt) {
	/* interpolate position of collision */
	const struct Vec2f a = {
		u->r.x * t0_dt + u->r1.x * (1.0f - t0_dt),
		u->r.y * t0_dt + u->r1.y * (1.0f - t0_dt)
	}, b = {
		v->r.x * t0_dt + v->r1.x * (1.0f - t0_dt),
		v->r.y * t0_dt + v->r1.y * (1.0f - t0_dt)
	},
	/* delta */
	d = {
		b.x - a.x,
		b.y - a.y
	};
	/* normal at point of impact; fixme: iffy infinities */
	const float n_d2 = d.x * d.x + d.y * d.y;
	const float n_n  = (n_d2 < epsilon) ? 1.0f / epsilon : 1.0f / sqrtf(n_d2);
	const struct Vec2f n = {
		d.x * n_n,
		d.y * n_n
	};
	/* a's velocity, normal direction */
	const float a_nrm_s = u->v.x * n.x + u->v.y * n.y;
	const struct Vec2f a_nrm = {
		a_nrm_s * n.x,
		a_nrm_s * n.y
	},
	/* a's velocity, tangent direction */
	a_tan = {
		u->v.x - a_nrm.x,
		u->v.y - a_nrm.y
	};
	/* b's velocity, normal direction */
	const float b_nrm_s = v->v.x * n.x + v->v.y * n.y;
	const struct Vec2f b_nrm = {
		b_nrm_s * n.x,
		b_nrm_s * n.y
	},
	/* b's velocity, tangent direction */
	b_tan = {
		v->v.x - b_nrm.x,
		v->v.y - b_nrm.y
	};
	/* mass (assume all mass is positive!) */
	const float a_m = u->mass, b_m = v->mass;
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* elastic collision */
	const struct Vec2f a_v = {
		a_tan.x + (a_nrm.x*diff_m + 2*b_m*b_nrm.x) * invsum_m,
		a_tan.y + (a_nrm.y*diff_m + 2*b_m*b_nrm.y) * invsum_m
	}, b_v = {
		(-b_nrm.x*diff_m + 2*a_m*a_nrm.x) * invsum_m + b_tan.x,
		(-b_nrm.y*diff_m + 2*a_m*a_nrm.y) * invsum_m + b_tan.y
	};
	/* check for sanity */
	const float bounding = u->bounding + v->bounding;
	/* fixme: float stored in memory? */
	
	/*pedantic("elasitic_bounce: colision between %s--%s norm_d %f; sum_r %f, %f--%ft\n",
	 SpriteToString(u), SpriteToString(v), sqrtf(n_d2), bounding, a_m, b_m);*/
	
	/* interpenetation; happens about half the time because of IEEE754 numerics,
	 which could be on one side or the other; also, sprites that just appear,
	 multiple collisions interfering, and gremlins; you absolutely do not want
	 objects to get stuck orbiting each other (fixme: this happens) */
	if(n_d2 < bounding * bounding) {
		const float push = (bounding - sqrtf(n_d2)) * 0.5f;
		pedantic("elastic_bounce: \\pushing sprites %f distance apart\n", push);
		u->r.x -= n.x * push;
		u->r.y -= n.y * push;
		v->r.x += n.x * push;
		v->r.y += n.y * push;
	}
	
	if(!u->no_collisions) {
		/* first collision */
		u->no_collisions = 1;
		u->t0_dt_collide = t0_dt;
		u->v1.x = a_v.x;
		u->v1.y = a_v.y;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		/* fixme: shouldn't it be all inside? */
		if(t0_dt < u->t0_dt_collide) u->t0_dt_collide = t0_dt;
		u->v1.x += a_v.x;
		u->v1.y += a_v.y;
		u->no_collisions++;
		/*pedantic(" \\%u collisions %s (%s.)\n", u->no_collisions, SpriteToString(u), SpriteToString(v));*/
	}
	if(!v->no_collisions) {
		/* first collision */
		v->no_collisions = 1;
		v->t0_dt_collide = t0_dt;
		v->v1.x          = b_v.x;
		v->v1.y          = b_v.y;
	} else {
		/* there are multiple collisions, rebound from the 1st one and add */
		if(t0_dt < v->t0_dt_collide) v->t0_dt_collide = t0_dt;
		v->v1.x          += b_v.x;
		v->v1.y          += b_v.y;
		v->no_collisions++;
		/*pedantic(" \\%u collisions %s (%s.)\n", v->no_collisions, SpriteToString(v), SpriteToString(u));*/
	}
	
}

/** Pushes a from angle, amount.
 @param a		The object you want to push.
 @param angle	Standard co\:ordainates, radians, angle.
 @param impulse	tonne pixels / ms */
static void push(struct Sprite *a, const float angle, const float impulse) {
	const float deltav = a->mass ? impulse / a->mass : 1.0f; /* pixel / s */
	a->v.x += cosf(angle) * deltav;
	a->v.y += sinf(angle) * deltav;
}

/* type collisions; can not modify list of Sprites as it is in the middle of
 x/ylist or delete! */

static void shp_shp(struct Sprite *a, struct Sprite *b, const float d0) {
	elastic_bounce(a, b, d0);
}

static void deb_deb(struct Sprite *a, struct Sprite *b, const float d0) {
	elastic_bounce(a, b, d0);
}

static void shp_deb(struct Sprite *s, struct Sprite *d, const float d0) {
	/*printf("shpdeb: collide Shp(Spr%u): Deb(Spr%u)\n", SpriteGetId(s), SpriteGetId(d));*/
	elastic_bounce(s, d, d0);
	/*d->vx += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);
	 d->vy += 32.0f * (2.0f * rand() / RAND_MAX - 1.0f);*/
}

static void deb_shp(struct Sprite *d, struct Sprite *s, const float d0) {
	shp_deb(s, d, d0);
}

static void wmd_deb(struct Sprite *w, struct Sprite *d, const float d0) {
	/*pedantic("wmd_deb: %s -- %s\n", SpriteToString(w), SpriteToString(d));*/
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(d)) return;
	push(d, atan2f(d->y - w->y, d->x - w->x), w->mass);
	SpriteRecharge(d, -SpriteGetDamage(w));
	SpriteDestroy(w);
	UNUSED(d0);
}

static void deb_wmd(struct Sprite *d, struct Sprite *w, const float d0) {
	wmd_deb(w, d, d0);
}

static void wmd_shp(struct Sprite *w, struct Sprite *s, const float d0) {
	pedantic("wmd_shp: %s -- %s\n", SpriteToString(w), SpriteToString(s));
	/* avoid inifinite destruction loop */
	if(SpriteIsDestroyed(w) || SpriteIsDestroyed(s)) return;
	push(s, atan2f(s->y - w->y, s->x - w->x), w->mass);
	SpriteRecharge(s, -SpriteGetDamage(w));
	SpriteDestroy(w);
	UNUSED(d0);
}

static void shp_wmd(struct Sprite *s, struct Sprite *w, const float d0) {
	wmd_shp(w, s, d0);
}

static void shp_eth(struct Sprite *s, struct Sprite *e, const float d0) {
	/*void (*fn)(struct Sprite *const, struct Sprite *);*/
	/*Info("Shp%u colliding with Eth%u . . . \n", ShipGetId(ship), EtherealGetId(eth));*/
	/*if((fn = EtherealGetCallback(eth))) fn(eth, s);*/
	/* while in iterate! danger! */
	if(e->sp.ethereal.callback) e->sp.ethereal.callback(e, s);
	UNUSED(d0);
}

static void eth_shp(struct Sprite *e, struct Sprite *s, const float d0) {
	shp_eth(s, e, d0);
}

/** @implements <Sprite>Action */
static void sprite_update(struct Sprite *const sprites) {
	sprites->vt->update(sprites);
}

/* Compute bounding boxes of where the sprite wants to go this frame,
 containing the projected circle along a vector {x -> x + v*dt}. This is the
 first step in collision detection. */
static void collide_extrapolate(struct Sprite *const sprites) {
	/* where they should be if there's no collision */
	sprites->r1.x = sprites->r.x + sprites->v.x * dt_ms;
	sprites->r1.y = sprites->r.y + sprites->v.y * dt_ms;
	/* clamp */
	if(sprites->r1.x > de_sitter)       sprites->r1.x = de_sitter;
	else if(sprites->r1.x < -de_sitter) sprites->r1.x = -de_sitter;
	if(sprites->r1.y > de_sitter)       sprites->r1.y = de_sitter;
	else if(sprites->r1.y < -de_sitter) sprites->r1.y = -de_sitter;
	/* extend the bounding box along the circle's trajectory */
	if(sprites->r.x <= sprites->r1.x) {
		sprites->box.x_min = sprites->r.x  - sprites->bounding - bin_half_space;
		sprites->box.x_max = sprites->r1.x + sprites->bounding + bin_half_space;
	} else {
		sprites->box.x_min = sprites->r1.x - sprites->bounding - bin_half_space;
		sprites->box.x_max = sprites->r.x  + sprites->bounding + bin_half_space;
	}
	if(sprites->r.y <= sprites->r1.y) {
		sprites->box.y_min = sprites->r.y  - sprites->bounding - bin_half_space;
		sprites->box.y_max = sprites->r1.y + sprites->bounding + bin_half_space;
	} else {
		sprites->box.y_min = sprites->r1.y - sprites->bounding - bin_half_space;
		sprites->box.y_max = sprites->r.y  + sprites->bounding + bin_half_space;
	}
}

/** Used after \see{collide_extrapolate} on all the sprites you want to check.
 Uses Hahn–Banach separation theorem and the ordered list of projections on the
 {x} and {y} axis to build up a list of possible colliders based on their
 bounding box of where it moved this frame. Calls \see{collide_circles} for any
 candidites. */
static void collide_boxes(struct Sprite *const sprites) {
	/* assume it can be in a maximum of four bins at once as it travels? */
	/*...*/
	struct Sprite *b, *b_adj_y;
	float t0;
	if(!sprites) return;
	/* mark x */
	for(b = sprites->prev_x; b && b->x >= explore_x_min; b = b->prev_x) {
		if(sprites < b && sprites->x_min < b->x_max) b->is_selected = -1;
	}
	for(b = sprites->next_x; b && b->x <= explore_x_max; b = b->next_x) {
		if(sprites < b && sprites->x_max > b->x_min) b->is_selected = -1;
	}
	/* consider y and maybe add it to the list of colliders */
	for(b = sprites->prev_y; b && b->y >= explore_y_min; b = b_adj_y) {
		b_adj_y = b->prev_y; /* b can be deleted; make a copy */
		if(b->is_selected
		   && sprites->y_min < b->y_max
		   && (response = collision_matrix[b->sp_type][sprites->sp_type])
		   && collide_circles(sprites->x, sprites->y, sprites->x1, sprites->y1, b->x,
							  b->y, b->x1, b->y1, sprites->bounding + b->bounding, &t0))
			response(sprites, b, t0);
		/*debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	for(b = sprites->next_y; b && b->y <= explore_y_max; b = b_adj_y) {
		b_adj_y = b->next_y;
		if(b->is_selected
		   && sprites->y_max > b->y_min
		   && (response = collision_matrix[b->sp_type][sprites->sp_type])
		   && collide_circles(sprites->x, sprites->y, sprites->x1, sprites->y1, b->x,
							  b->y, b->x1, b->y1, sprites->bounding + b->bounding, &t0))
			response(sprites, b, t0);
		/*debug("Collision %s--%s\n", SpriteToString(a), SpriteToString(b));*/
	}
	/* uhh, yes? I took this code from SpriteUpdate */
	if(!s->no_collisions) {
		/* no collisions; apply position change, calculated above */
		s->x = s->x1;
		s->y = s->y1;
	} else {
		/* if you skip this step, it's unstable on multiple collisions */
		const float one_collide = 1.0f / s->no_collisions;
		const float s_vx1 = s->vx1 * one_collide;
		const float s_vy1 = s->vy1 * one_collide;
		/* before and after collision;  */
		s->x += (s->vx * s->t0_dt_collide + s_vx1 * (1.0f - s->t0_dt_collide)) * dt_ms;
		s->y += (s->vy * s->t0_dt_collide + s_vy1 * (1.0f - s->t0_dt_collide)) * dt_ms;
		s->vx = s_vx1;
		s->vy = s_vy1;
		/* unmark for next frame */
		s->no_collisions = 0;
		/* apply De Sitter spacetime? already done? */
		if(s->x      < -de_sitter) s->x =  de_sitter - 20.0f;
		else if(s->x >  de_sitter) s->x = -de_sitter + 20.0f;
		if(s->y      < -de_sitter) s->y =  de_sitter - 20.0f;
		else if(s->y >  de_sitter) s->y = -de_sitter + 20.0f;
	}
	/* bin change */
}

#endif
