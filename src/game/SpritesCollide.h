/** 2017 Neil Edelman, distributed under the terms of the GNU General
 Public License 3, see copying.txt, or
 \url{ https://opensource.org/licenses/GPL-3.0 }.

 Collision detection and resolution. Part of {Sprites}, but too long; sort of
 hacky. The course grained functions are at the bottom, calling the
 fine-grained and response as it moves to the top.

 This is the calculation in \see{collide_circles},
 \${ 	     u = a.dx
             v = b.dx
 if(v-u ~= 0) t doesn't matter, parallel-ish
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
            t0 = [-2(b0-a0)*(v-u) - \sqrt((2(b0-a0)*(v-u))^2
                   - 4((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / 2(v-u)*(v-u)
            t0 = [-(b0-a0)*(v-u) - \sqrt(((b0-a0)*(v-u))^2
                   - ((v-u)*(v-u))((b0-a0)*(b0-a0) - r^2))] / (v-u)*(v-u) }

 @title		SpritesCollide
 @author	Neil
 @std		C89/90
 @version	2017-10 Broke off from Sprites.
 @fixme Collision resolution wonky. */


#if 0
/** Called from \see{elastic_bounce}. */
static void apply_bounce_later(struct Sprite *const this,
	const struct Vec2f *const v, const float t) {
	struct Collision *const c = CollisionPoolNew(collisions);
	char a[12];
	assert(this && v && t >= 0.0f && t <= sprites->dt_ms);
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

/** Used after \see{collide_extrapolate} on all the sprites you want to check.
 Uses  and the ordered list of projections on the
 {x} and {y} axis to build up a list of possible colliders based on their
 bounding box of where it moved this frame. Calls \see{collide_circles} for any
 candidites. */
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

/*fprintf(gnu_glob, "set arrow from %f,%f to %f,%f nohead lw 0.5 "
 "lc rgb \"red\" front;\n", this->x.x, this->x.y, that->x.x, that->x.y);*/
/* fixme: collision matrix */
elastic_bounce(this, that, t0);

#endif

int is_redundant;

/** Elastic collision between circles; called from \see{collide} with
 {@code t0_dt} from \see{collide_circles}. Use this when we've determined that
 {@code Sprite a} collides with {@code Sprite b} at {@code t0_dt}, where
 Sprites' {@code x, y} are at time 0 and {@code x1, y1} are at time 1 (we will
 not get here, just going towards.) Velocities are {@code vx, vy} and this
 function is responsible for setting {@code vx1, vy1}, after the collision.
 Also, it may alter (fudge) the positions a little to avoid interpenetration.
 @param t: {ms} after frame that the collision occurs. */
static void elastic_bounce(const struct Sprite *const a,
	const struct Sprite *const b, const float t) {	
#if 0
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

	/* or */

#endif
	/* Interpolate position of collision; delta. */
	const struct Vec2f u = { a->x.x + a->v.x * t, a->x.y + a->v.y * t },
	                   v = { b->x.x + b->v.x * t, b->x.y + b->v.y * t },
		d = { v.x - u.x, v.y - u.y };
	/* Normal at point of impact. */
	const float n_d2 = d.x * d.x + d.y * d.y;
	const float n_n  = (n_d2 < epsilon) ? 1.0f / epsilon : 1.0f / sqrtf(n_d2);
	const struct Vec2f n = { d.x * n_n, d.y * n_n };
	/* {a}'s velocity, normal, tangent, direction. */
	const float a_nrm_s = a->v.x * n.x + a->v.y * n.y;
	const struct Vec2f a_nrm = { a_nrm_s * n.x, a_nrm_s * n.y },
		a_tan = { a->v.x - a_nrm.x, a->v.y - a_nrm.y };
	/* {b}'s velocity, normal, tangent, direction. */
	const float b_nrm_s = b->v.x * n.x + b->v.y * n.y;
	const struct Vec2f b_nrm = { b_nrm_s * n.x, b_nrm_s * n.y },
		b_tan = { b->v.x - b_nrm.x, b->v.y - b_nrm.y };
	/* Assume all mass is strictly positive. */
	const float a_m = sprite_get_mass(a), b_m = sprite_get_mass(b);
	const float diff_m = a_m - b_m, invsum_m = 1.0f / (a_m + b_m);
	/* Elastic collision. */
	const struct Vec2f a_v = {
		a_tan.x + (a_nrm.x * diff_m + 2 * b_m * b_nrm.x) * invsum_m,
		a_tan.y + (a_nrm.y * diff_m + 2 * b_m * b_nrm.y) * invsum_m
	}, b_v = {
		b_tan.x - (b_nrm.x * diff_m - 2 * a_m * a_nrm.x) * invsum_m,
		b_tan.y - (b_nrm.y * diff_m - 2 * a_m * a_nrm.y) * invsum_m
	};
	/* Check for sanity; fixme: have a different algorithm for closer than this? */
	const float bounding = a->bounding + b->bounding;

	/* inter-penetration; happens about half the time because of IEEE754
	 numerics, which could be on one side or the other; also, sprites that just
	 appear, multiple collisions interfering, and gremlins; you absolutely do
	 not want objects to get stuck orbiting each other (fixme: this happens) */
	if(n_d2 < bounding * bounding) {
		/*const float push = (bounding - sqrtf(n_d2)) * 0.5f;
		printf("elastic_bounce: \\pushing sprites %f distance apart\n", push);*/
		/*a->x.x -= n.x * push;
		a->x.y -= n.y * push;
		b->x.x += n.x * push;
		b->x.y += n.y * push;*/
	}
#if 0
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
#endif
	{
		char str_a[12], str_b[12];
		assert(a && b && t >= 0);
		sprite_to_string(a, &str_a);
		sprite_to_string(b, &str_b);
		printf("[%s, %s] collide at %fms into the frame%s.\n", str_a, str_b, t,
			   is_redundant ? " (redundant)" : "");
	}
}

/** Checks whether two sprites intersect using inclined cylinders in
 three-dimensions, where the third dimension is linearly-interpolated time.
 Called from \see{collide_boxes}. */
static void collide_circles(const struct Sprite *const a,
	const struct Sprite *const b) {
	struct Vec2f v, z, dist;
	float t, v2, vz, z2, dist_2, det;
	const float r = a->bounding + b->bounding;
	assert(a && b);
	v.x = a->v.x - b->v.x, v.y = a->v.y - b->v.y;
	z.x = a->x.x - b->x.x, z.y = a->x.y - b->x.y;
	v2 = v.x * v.x + v.y * v.y;
	vz = v.x * z.x + v.y * z.y;
	/* Time of closest approach. */
	if(v2 < epsilon) {
		t = 0.0f;
	} else {
		t = -vz / v2;
		if(     t < 0.0f)           t = 0.0f;
		else if(t > sprites->dt_ms) t = sprites->dt_ms;
	}
	/* Distance(t_min). */
	dist.x = z.x + v.x * t, dist.y = z.y + v.y * t;
	dist_2 = dist.x * dist.x + dist.y * dist.y;
	if(dist_2 >= r * r) return;
	/* The first root or zero is when the collision happens. */
	z2 = z.x * z.x + z.y * z.y;
	det = (vz * vz - v2 * (z2 - r * r));
	t = (det <= 0.0f) ? 0.0f : (-vz - sqrtf(det)) / v2;
	if(t < 0.0f) t = 0.0f; /* bounded, dist < r^2; fixme: really want this? */
	/* fixme: collision matrix. */
	elastic_bounce(a, b, t);
}
/** This first applies the most course-grained collision detection in
 two-dimensional space, Hahn–Banach separation theorem using {box}. If passed,
 calls \see{collide_circles}. Called from \see{collide_bin}. */
static void collide_boxes(const struct Sprite *const a,
	const struct Sprite *const b) {
	assert(a && b);
	/* https://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other */
	if(!(a->box.x_min <= b->box.x_max && b->box.x_min <= a->box.x_max &&
		 b->box.y_min <= a->box.y_max && a->box.y_min <= b->box.y_max)) return;
	collide_circles(a, b);
}
/** This is the function that's calling everything else. Call after
 {extrapolate}; needs and consumes {covers}. */
static void collide_bin(unsigned bin) {
	struct CoverStack *const cover = sprites->bins[bin].covers;
	struct Cover *cover_a, *cover_b;
	struct Sprite *a, *b;
	size_t index_b;
	assert(sprites && bin < LAYER_SIZE);

	/*printf("bin %u: %lu covers\n", bin, ref->size);*/
	/* This is {O({covers}^2)/2} within the bin. {a} is poped . . . */
	while((cover_a = CoverStackPop(cover))) {
		/* . . . then {b} goes down the list. */
		if(!(index_b = CoverStackGetSize(cover))) break;
		do {
			is_redundant = 0;
			index_b--;
			cover_b = CoverStackGetElement(cover, index_b);
			assert(cover_a && cover_b);
			/* Another {bin} takes care of it? */
			if(!cover_a->is_corner && !cover_b->is_corner) is_redundant = 1;/*continue;*/
			a = cover_a->sprite, b = cover_b->sprite;
			assert(a && b);
			/* Mostly for weapons that ignore collisions with themselves. */
			if(!(sprite_get_self_mask(a) & sprite_get_others_mask(b))
				&& !(sprite_get_others_mask(a) & sprite_get_self_mask(b)))
				continue;
			/*if((!(vec.x <= min->x) && a right && b right)
			 || (!(vec.y <= min->y) && a bottom && b bottom)) */
			collide_boxes(a, b);
		} while(index_b);
	}
}

		
		
		
#if 0

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

#endif
