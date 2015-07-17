/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <math.h>   /* cis */
#include "Glew.h"
#include "../general/Map.h"
#include "../general/Bitmap.h"
#include "../game/Sprite.h"
#include "../game/Light.h"
#include "Draw.h"
#include "Window.h"

#define M_2PI 6.283185307179586476925286766559005768394338798750211641949889
#define M_1_2PI 0.159154943091895335768883763372514362034459645740456448747667
#ifndef M_SQRT1_2 /* for MSVC */
#define M_SQRT1_2 0.70710678118654752440084436210484903928483593768847403658833986899536623923105351942519376716382078636750692311545614851
#endif
#ifdef C89_FMINF /* for MSVC2010 */
#define fminf(a, b) ((a) < (b) ? (a) : (b))
#endif

/* hard coded resouce files; there should be the directory tools/ where you can
 compile utilities that can make these files; run "make" and this should be
 automated */
#include "../../bin/shaders/Texture_vs.h"
#include "../../bin/shaders/Texture_fs.h"
#include "../../bin/shaders/Lighting_vs.h"
#include "../../bin/shaders/Lighting_fs.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.0, 05-2015
 @since		1.0, 2000 */

/* if is started, we don't and can't start it again */
static int is_started;

/* for textures; assert GlTex: GL_TEXTUREx <-> TexArray: x;
 fixme: do more complex texture mapping management once we have textures
 "The constants obey TEXTUREi = TEXTURE0+i (i is in the range 0 to k - 1,
 where k is the value of MAX_COMBINED_TEXTURE_IMAGE_UNITS)." */
enum GlTex {
	GT_LIGHT = GL_TEXTURE0,
	GT_BACKGROUND = GL_TEXTURE1,
	GT_SPRITES = GL_TEXTURE2
};
enum TexArray {
	T_LIGHT = 0,
	T_BACKGROUND = 1,
	T_SPRITES = 2
};

/* for shader attribute assigment */
enum VertexAttribs { G_POSITION, G_TEXTURE };
/*enum SpotAttribs { S_POSITION, S_COLOUR };*/

/* corresponds to VertexAttribs on hardware;
 vbo is used ubiquitously for static geometry, uploaded into video memory;
 fixme: it's just a shift, scale; we don't need half of them
 (the G_POSITIONs, since they're already being transformed); twice the stuff
 could be shown on the screen! */
static struct Vertex {
	GLfloat x, y;
	GLfloat s, t;
} vbo[] = {
	{  1.0f,  1.0f,  1.0f,  1.0f }, /* background; on update resize() changes */
	{  1.0f, -1.0f,  1.0f,  0.0f },
	{ -1.0f,  1.0f,  0.0f,  1.0f },
	{ -1.0f, -1.0f,  0.0f,  0.0f },
	{ 0.5f,  0.5f, 1.0f, 1.0f },    /* sprite */
	{ 0.5f, -0.5f, 1.0f, 0.0f },
	{ -0.5f, 0.5f, 0.0f, 1.0f },
	{ -0.5f,-0.5f, 0.0f, 0.0f }
};
static const void *vertex_pos_offset = 0;
static const int  vertex_pos_size    = 2;
static const void *vertex_tex_offset = (void *)(sizeof(float) * 2);
static const int  vertex_tex_size    = 2;
static const int vbo_bg_first     = 0, vbo_bg_count     = 4;
static const int vbo_sprite_first = 4, vbo_sprite_count = 4;

/* corresponds to SpotAttribs on hardware */
/*static struct Spot {
	GLfloat x, y;
	GLfloat r, g, b;
} spots[512];
static const int spot_capacity = sizeof(spots) / sizeof(struct Spot);
static int       spot_size;
static const void *spot_pos_offset    = 0;
static const int  spot_pos_size       = 2;
static const void *spot_colour_offset = (void *)(sizeof(float) * 2);
static const int  spot_colour_size    = 3;*/

/* this is for drawing the background with tex_map programme; 2 * 0.5 = 1,
 column -- fixme: not really necessary for tex_map_shader to have all these
 degrees-of-freedom; position on screen and size should be enough */
/*static const GLfloat background_matrix[] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};*/

/* private prototypes */
static GLuint link_shader(const char *vert_vs, const char *frag_fs, void (*attrib)(const GLuint));
static void tex_map_attrib(const GLuint shader);
static int texture(const char *name, const int width, const int height, const int depth, const unsigned char *bmp);
static int light_compute_texture(void);
static void display(void);
static void resize(int width, int height);

/* so many globals! */
GLuint  vbo_geom, /*spot_geom,*/ light_tex, *texture_ids, astr_tex, bg_tex, tex_map_shader, light_shader;
GLint   tex_map_matrix_location, tex_map_texture_location;
GLint   light_size_location, light_angle_location, light_position_location, light_camera_location, light_texture_location, light_two_screen_location;
GLint   light_lights_location, light_lightpos_location, light_lightclr_location;
GLint   light_dirang_location, light_dirclr_location;
GLfloat two_width, two_height;
float   camera_x, camera_y;

/** Gets all the graphics stuff started.
 @return		All good to draw? */
int Draw(struct Map *bmps) {
	struct Bitmap *bmp;
	char *name;
	int tex;
	int is_textures_loaded = 0;

	if(is_started) return -1;

	if(!WindowStarted()) {
		fprintf(stderr, "Draw: window not started.\n");
		return 0;
	}
	glutDisplayFunc(&display);
	glutReshapeFunc(&resize);

	/* http://www.opengl.org/sdk/docs/man2/xhtml/glBlendFunc.xml */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glPointSize(5);
	glEnable(GL_POINT_SMOOTH);

	/* vbo; glVertexAttribPointer(index attrib, size, type, normaised, stride, offset) */
	glGenBuffers(1, (GLuint *)&vbo_geom);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_geom);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vbo), vbo, GL_STATIC_DRAW);
	/* fixme: the texture should be the same as the vetices, half the data */
	/* fixme: done per-frame, because apparently OpenGl does not keep track of the bindings per-buffer */
	glEnableVertexAttribArray(G_POSITION);
	glVertexAttribPointer(G_POSITION, vertex_pos_size, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), vertex_pos_offset);
    glEnableVertexAttribArray(G_TEXTURE);
    glVertexAttribPointer(G_TEXTURE,  vertex_tex_size, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), vertex_tex_offset);
	fprintf(stderr, "Draw: created vertex buffer, Vbo%u.\n", vbo_geom);

	/* textures */
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	/* lighting */
	glActiveTexture(GT_LIGHT);
	light_tex = light_compute_texture();
	/* textures stored in bmps */
	while(MapIterate(bmps, (const void **)&name, (void **)&bmp)) {
		switch(BitmapGetType(bmp)) {
			case B_BACKGROUND:
				glActiveTexture(GT_BACKGROUND);
				if(!(tex = texture(name, BitmapGetWidth(bmp), BitmapGetHeight(bmp), BitmapGetDepth(bmp), BitmapGetData(bmp)))) break;
				BitmapSetImageUnit(bmp, tex);
				glBindTexture(GL_TEXTURE_2D, tex);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
				break;
			case B_SPRITE:
				glActiveTexture(GT_SPRITES);
				if(!(tex = texture(name, BitmapGetWidth(bmp), BitmapGetHeight(bmp), BitmapGetDepth(bmp), BitmapGetData(bmp)))) break;
				BitmapSetImageUnit(bmp, tex);
				break;
			case B_DONT_KNOW:
			default:
				fprintf(stderr, "Draw: bitmap \"%s\" has an invalid type.\n", name);
		}
	}
	/* hard-code textures */
	do {
		int unit;

		bmp = MapGet(bmps, "Ngc4038_4039_bmp");
		if(!(unit = BitmapGetImageUnit(bmp))) break;
		bg_tex = unit;
		bmp = MapGet(bmps, "Nautilus_bmp");
		if(!(unit = BitmapGetImageUnit(bmp))) break;
		astr_tex = unit;
		is_textures_loaded = -1;
	} while(0);
	if(!is_textures_loaded) {
		fprintf(stderr, "Draw: don't have minimal textures.\n");
		Draw_(bmps);
		return 0;
	}

	/* shaders */
	if(!(tex_map_shader = link_shader(Texture_vs, Texture_fs, &tex_map_attrib))) { Draw_(bmps); return 0; }
	tex_map_matrix_location  = glGetUniformLocation(tex_map_shader, "matrix");
	tex_map_texture_location = glGetUniformLocation(tex_map_shader, "sampler");
	/* this is the one that's doing the work */
	if(!(light_shader = link_shader(Lighting_vs, Lighting_fs, &tex_map_attrib))) { Draw_(bmps); return 0; }
	/* vs */
	light_angle_location     = glGetUniformLocation(light_shader, "angle");
	light_size_location      = glGetUniformLocation(light_shader, "size");
	light_position_location  = glGetUniformLocation(light_shader, "position");
	light_camera_location    = glGetUniformLocation(light_shader, "camera");
	light_two_screen_location= glGetUniformLocation(light_shader, "two_screen");
	/* fs */
	light_texture_location = glGetUniformLocation(light_shader, "sampler");
	glUniform1i(glGetUniformLocation(tex_map_shader, "sampler_light"), T_LIGHT);
	light_lights_location  = glGetUniformLocation(light_shader, "lights");
	light_lightpos_location= glGetUniformLocation(light_shader, "light_position");
	light_lightclr_location= glGetUniformLocation(light_shader, "light_colour");
	light_dirang_location  = glGetUniformLocation(light_shader, "directional_angle");
	light_dirclr_location  = glGetUniformLocation(light_shader, "directional_colour");

	/* sunshine; fixme: have it change in different regions */
	{
		float sunshine[] = { 1.0f * 3.0f, 0.97f * 3.0f, 0.46f * 3.0f };
		glUseProgram(light_shader);
		glUniform1f(light_dirang_location, -2.0);
		glUniform3fv(light_dirclr_location, 1, sunshine);
	}

	/* particle spot buffer; oh man, this has to be done every time? */
	/*glGenBuffers(1, (GLuint *)&spot_geom);
	glBindBuffer(GL_ARRAY_BUFFER, spot_geom);
	glBufferData(GL_ARRAY_BUFFER, sizeof(spots), spots, GL_STREAM_DRAW);
	fprintf(stderr, "Draw: created spot particle buffer, Vbo%u.\n", spot_geom);*/

	glUseProgram(0);

	WindowIsGlError("Draw");

	is_started = -1;
	return -1;
}

/** Destructor. */
void Draw_(struct Map *bmps) {
	struct Bitmap *bmp;
	char *name;
	int unit;

	/*if(glIsBuffer(spot_geom)) {
		glDeleteBuffers(1, &spot_geom);
		fprintf(stderr, "~Draw: Deleted particle buffer.\n");
		spot_geom = 0;
	}*/
	if(light_shader) {
		fprintf(stderr, "~Draw: erase Sdr%u.\n", light_shader);
		glDeleteProgram(light_shader);
		light_shader = 0;
	}
	if(tex_map_shader) {
		fprintf(stderr, "~Draw: erase Sdr%u.\n", tex_map_shader);
		glDeleteProgram(tex_map_shader);
		tex_map_shader = 0;
	}
	/* erase the textures */
	if(bmps) {
		while(MapIterate(bmps, (const void **)&name, (void **)&bmp)) {
			if(!bmp || !(unit = BitmapGetImageUnit(bmp))) continue;
			fprintf(stderr, "~Draw: erase \"%s,\" Tex%u.\n", name, unit);
			glDeleteTextures(1, (unsigned *)&unit);
			BitmapSetImageUnit(bmp, 0);
		}
	}
	if(light_tex) {
		fprintf(stderr, "~Draw: erase lighting texture, Tex%u.\n", light_tex);
		glDeleteTextures(1, &light_tex);
		light_tex = 0;
	}
	if(vbo_geom && glIsBuffer(vbo_geom)) {
		fprintf(stderr, "~Draw: erase Vbo%u.\n", vbo_geom);
		glDeleteBuffers(1, &vbo_geom);
		vbo_geom = 0;
	}

	/* get all the errors that we didn't catch */
	WindowIsGlError("~Draw");

	is_started = 0;
}

/** Sets the camera location.
 @param x
 @param y	(x, y) in pixels. */
void DrawSetCamera(const float x, const float y) {
	if(!is_started) return;
	camera_x = x;
	camera_y = y;
}

/** Compiles, links and verifies a shader.
 @param vert_vs		Vertex shader source.
 @param frag_fs		Fragment shader source.
 @param attrib(const GLuint)
					A function which is passed the uncompiled shader source and
					which is supposed to set attributes. Can be null.
 @return			A shader or 0 if it didn't link. */
static GLuint link_shader(const char *vert_vs, const char *frag_fs, void (*attrib)(const GLuint)) {
	GLchar info[128];
	GLuint vs = 0, fs = 0, shader = 0;
	GLint status;
	enum { S_NOERR = 0, S_VERT, S_FRAG, S_LINK, S_VALIDATE } error = S_NOERR;

	/* try */
	do {
		/* compile vs; shader, count, **string, *length */
		vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, &vert_vs, 0);
		glCompileShader(vs);
		glGetShaderiv(vs, GL_COMPILE_STATUS, &status);
		if(!status) { error = S_VERT; break; }
		fprintf(stderr, "Draw::link_shader: compiled vertex shader, Sdr%u.\n", vs);

		/* compile fs */
		fs = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(fs, 1, &frag_fs, 0);
		glCompileShader(fs);
		glGetShaderiv(fs, GL_COMPILE_STATUS, &status);
		if(!status) { error = S_FRAG; break; }
		fprintf(stderr, "Draw::link_shader: compiled fragment shader, Sdr%u.\n", fs);

		/* link */
		shader = glCreateProgram();
		glAttachShader(shader, vs);
		glAttachShader(shader, fs);
		/* callback */
		if(attrib) attrib(shader);
		glLinkProgram(shader);
		glGetProgramiv(shader, GL_LINK_STATUS, &status);
		if(!status) { error = S_LINK; break; }
		fprintf(stderr, "Draw::link_shader: linked shader programme, Sdr%u.\n", shader);

		/* validate */
		glValidateProgram(shader);
		glGetProgramiv(shader, GL_VALIDATE_STATUS, &status);
		if(!status) { error = S_VALIDATE; break; }
		fprintf(stderr, "Draw::link_shader: validated shader programme, Sdr%u.\n", shader);

	} while(0);

	/* catch */
	switch(error) {
		case S_NOERR:
			break;
		case S_VERT:
			glGetShaderInfoLog(vs, sizeof(info), 0, info);
			fprintf(stderr, "Draw::link_shader: Sdr%u failed vertex shader compilation; OpenGL: %s", vs, info);
			break;
		case S_FRAG:
			glGetShaderInfoLog(fs, sizeof(info), 0, info);
			fprintf(stderr, "Draw::link_shader: Sdr%u failed fragment shader compilation; OpenGL: %s", fs, info);
			break;
		case S_LINK:
			glGetProgramInfoLog(shader, sizeof(info), 0, info);
			fprintf(stderr, "Draw::link_shader: Sdr%u failed shader linking; OpenGL: %s", shader, info);
			break;
		case S_VALIDATE: /* fixme: untested! */
			glGetProgramInfoLog(shader, sizeof(info), 0, info);
			fprintf(stderr, "Draw::link_shader: Sdr%u failed shader validation; OpenGL: %s", shader, info);
			break;
	}

	/* don't need these anymore, whatever the result */
	if(fs) {
		glDeleteShader(fs);
		if(shader) glDetachShader(shader, fs);
		fprintf(stderr, "Draw::link_shader: erasing fragment shader, Sdr%u.\n", fs);
	}
	if(vs) {
		glDeleteShader(vs);
		if(shader) glDetachShader(shader, vs);
		fprintf(stderr, "Draw::link_shader: erasing vertex shader, Sdr%u.\n", vs);
	}

	/* resume catching */
	if(error) {
		if(shader) {
			glDeleteProgram(shader);
			fprintf(stderr, "Draw::link_shader: deleted shader programme, Sdr%u.\n", shader);
		}
		return 0;
	}

	/* immediately use the shader; we probably want to set some uniforms */
	glUseProgram(shader);

	/* catch any errors */
	WindowIsGlError("shader");

	return shader;
}

/** Setup tex_map attributes callback.
 @param shader	The unlinked shader. */
static void tex_map_attrib(const GLuint shader) {
	/* programme, index attrib, name */
	/* fixme: calculated from vbo_texture now; gives almost no speedup :[ */
	/*glBindAttribLocation(shader, G_POSITION, "vbo_position"); <- pos! */
	glBindAttribLocation(shader, G_TEXTURE,  "vbo_texture");
}

/** Creates a texture from a bitmap in memory.
 @param width
 @param height
 @param depth
 @param bmp		The bitmap; must be the product of the last three long.
 @return		The name (int) of the newly created texture or 0 on error. */
static int texture(const char *name, const int width, const int height, const int depth, const unsigned char *bmp) {
	int format = 0, internal = 0;
	int id;

	if(width <= 0 || height <= 0 || !bmp) return 0;
	if(depth == 1) {
		/* we use exclusively for shaders, so I don't think this matters */
		internal = GL_RGB;
		/*format = GL_LUMINANCE; <- "core context depriciated" what */
		format   = GL_RED;
	} else if(depth == 3) {
		internal = GL_RGB8;
		format   = GL_RGB;
	} else if(depth == 4) {
		internal = GL_RGBA8;
		format   = GL_RGBA;
	} else {
		fprintf(stderr, "Draw::texture: not a recognised depth, %d.\n", depth);
		return 0;
	}
	glGenTextures(1, (unsigned *)&id);
	glBindTexture(GL_TEXTURE_2D, id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	/* void glTexImage2D(target, level, internalFormat, width, height,
	 border, format, type, *data); */
	glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height,
				 0, format, GL_UNSIGNED_BYTE, bmp);
	fprintf(stderr, "Draw::texture: created %dx%dx%d texture out of \"%s,\" Tex%u.\n", width, height, depth, name, id);

	WindowIsGlError("texture");

	return id;
}

/* Creates a hardcoded lighting texture with the Red the radius and Green the
 angle.
 @return	The texture (you will have to delete it.) */
static int light_compute_texture(void) {
	int   i, j;
	float x, y;
	float buffer[64][64][2];
	const int buffer_ysize = sizeof(buffer)  / sizeof(*buffer);
	const int buffer_xsize = sizeof(*buffer) / sizeof(**buffer);
	const float buffer_norm = (float)M_SQRT1_2 * 4.0f / sqrtf((float)buffer_xsize * buffer_xsize + buffer_ysize * buffer_ysize);
	int name;

	for(j = 0; j < buffer_ysize; j++) {
		for(i = 0; i < buffer_xsize; i++) {
			x = (float)i - 0.5f*buffer_xsize + 0.5f;
			y = (float)j - 0.5f*buffer_ysize + 0.5f;
			buffer[j][i][0] = fminf(sqrtf(x*x + y*y) * buffer_norm, 1.0f);
			/* NOTE: opengl clips [0, 1), even if it says different;
			 maybe it's GL_LINEAR? */
			buffer[j][i][1] = fmodf(atan2f(y, x) + (float)M_2PI, (float)M_2PI) * (float)M_1_2PI;
		}
	}
	glGenTextures(1, (unsigned *)&name);
	glBindTexture(GL_TEXTURE_2D, name);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	/* NOTE: GL_LINEAR is a lot prettier, but causes a crease when the
	 principal value goes from [0, 1), ie, [0, 2Pi) */
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	/* void glTexImage2D(target, level, internalFormat, width, height,
	 border, format, type, *data); */
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, buffer_xsize, buffer_ysize, 0,
		GL_RG, GL_FLOAT, buffer);
	fprintf(stderr, "Draw::texture: created %dx%dx%d hardcoded lighting texture, Tex%u.\n", buffer_xsize, buffer_ysize, 2, name);

	WindowIsGlError("light_compute_texture");

	return name;
}

/** Callback for glutDisplayFunc; this is where all of the drawing happens. */
static void display(void) {
	int lights;
	/* for SpriteIterate */
	float x, y, t;
	int old_texture = 0, texture, size;

	/* use sprites */
	glBindBuffer(GL_ARRAY_BUFFER, vbo_geom);

	/* use simple tex_map_shader */
	glUseProgram(tex_map_shader);

	/* fixme: don't use painters' algorithm; stencil test! */

	/* background:
	 turn off transperency
	 background vbo
	 update glUniform1i(GLint location, GLint v0)
	 update glUniformMatrix4fv(location, count, transpose, *value)
	 glDrawArrays(type flag, offset, no) */
	glDisable(GL_BLEND);
	glUniform1i(tex_map_texture_location, T_BACKGROUND);
	/*glUniformMatrix4fv(tex_map_matrix_location, 1, GL_FALSE, background_matrix);*/
	glDrawArrays(GL_TRIANGLE_STRIP, vbo_bg_first, vbo_bg_count);

	/* turn on lighting */
	glUseProgram(light_shader);
	lights = LightGetNo();
	glUniform1i(light_lights_location, lights);
	if(lights) {
		glUniform2fv(light_lightpos_location, lights, (GLfloat *)LightGetPositions());
		glUniform3fv(light_lightclr_location, lights, (GLfloat *)LightGetColours());
	}

	/* sprites:
	 turn on transperency
	 sprite vbo
	 for @ sprite:
	 -calculate matrix
	 -upload to card
	 -draw */
	glEnable(GL_BLEND);
	/* fixme: have different indices to textures; keep track with texture manager; have to worry about how many tex units there are */
	glUniform1i(light_texture_location, T_SPRITES);
	glUniform2f(light_camera_location, camera_x, camera_y);
	while(SpriteIterate(&x, &y, &t, &texture, &size)) {
		/* draw a sprite; fixme: minimise texture transitions */
		if(old_texture != texture) {
			glBindTexture(GL_TEXTURE_2D, texture);
			old_texture = texture;
		}
		glUniform1f(light_size_location, (float)size);
		glUniform1f(light_angle_location, t);
		glUniform2f(light_position_location, x, y);
		glDrawArrays(GL_TRIANGLE_STRIP, vbo_sprite_first, vbo_sprite_count);
	}

	/* create spots */
	/*glBindBuffer(GL_ARRAY_BUFFER, spot_geom);
	glEnableVertexAttribArray(S_POSITION);
	glVertexAttribPointer(S_POSITION, spot_pos_size, GL_FLOAT, GL_FALSE, sizeof(struct Spot), spot_pos_offset);
	glEnableVertexAttribArray(S_COLOUR);
	glVertexAttribPointer(S_COLOUR, spot_colour_size, GL_FLOAT, GL_FALSE, sizeof(struct Spot), spot_colour_offset);
	glDrawArrays(GL_POINTS, 0, 1);
	glDisableVertexAttribArray(S_POSITION);
	glDisableVertexAttribArray(S_COLOUR);*/

	/* disable, swap */
	glUseProgram(0);
	glutSwapBuffers();

}

/** Callback for glutReshapeFunc.
 @param width
 @param height	The size of the client area. */
static void resize(int width, int height) {
	int w_tex, h_tex;
	float w_w_tex, h_h_tex;

	if(width <= 0 || height <= 0) return;
	glViewport(0, 0, width, height);

	/* update the inverse screen on the card */
	two_width  = 2.0f / width;
	two_height = 2.0f / height;

	glBindTexture(GL_TEXTURE_2D, bg_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &w_tex);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h_tex);
	/*fprintf(stderr, "w %d h %d\n", w_tex, h_tex);*/
	w_w_tex = (float)width  / w_tex;
	h_h_tex = (float)height / h_tex;

	/* update the background texture vbo on the card */
	vbo[0].s = vbo[1].s =  w_w_tex;
	vbo[2].s = vbo[3].s = -w_w_tex;
	vbo[0].t = vbo[2].t =  h_h_tex;
	vbo[1].t = vbo[3].t = -h_h_tex;
	glBufferSubData(GL_ARRAY_BUFFER, vbo_bg_first,
		vbo_bg_count * sizeof(struct Vertex), vbo + vbo_bg_first);

	/* the shaders need to know as well */
	glUseProgram(light_shader);
	glUniform2f(light_two_screen_location, two_width, two_height);
}