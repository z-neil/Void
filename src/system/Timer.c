/* Copyright 2000, 2014 Neil Edelman, distributed under the terms of the GNU
 General Public License, see copying.txt */

#include <stdio.h>  /* fprintf */
#include "Glew.h"
#include "Timer.h"
#include "Window.h"
#include "../game/Game.h"

/** This is an idempotent class dealing with the interface to OpenGL.
 @author	Neil
 @version	3.0, 05-2015
 @since		2.0, 2014 */

static int is_started;

#define TIMER_LOG_FRAMES 2

struct Timer {
	int step;  /* ms */
	int time;
	int frame;
	int frame_time[1 << TIMER_LOG_FRAMES];
} timer;
const static int max_frame_time = sizeof(((struct Timer *)0)->frame_time) / sizeof(int);

static void update(int value);

/** This starts the Timer or modifies the Timer.
 <p>
 Fixme: I don't know how this will repond when you've not called glutInit();
 don't do it. "Requesting state for an invalid GLUT state name returns
 negative one."? Scetchy.
 @param step	The resolution of the timer. */
int Timer(const int step) {
	int i;

	timer.step  = step;
	if(is_started) return -1;

	if(!WindowStarted()) {
		fprintf(stderr, "Timer: window not started.\n");
		return 0;
	}

	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	timer.frame = 0;
	for(i = 0; i < max_frame_time; i++) timer.frame_time[i] = timer.time;
	is_started = -1;
	fprintf(stderr, "Timer: created timer with %ums.\n", timer.time);

	glutTimerFunc(timer.step, &update, timer.time);

	return -1;
}

/** This stops the Timer. */
void Timer_(void) {
	if(!is_started) return;
	is_started = 0;
	fprintf(stderr, "~Timer: erased timer.\n");
}

/** Last time an update was called. */
int TimerLastTime(void) {
	return timer.time;
}

/** Callback for glutTimerFunc.
 @param old_time	The previous time. */
static void update(int old_time) {
	int dt;

	if(!is_started) return;

	/* all times are in ms */
	timer.time  = glutGet(GLUT_ELAPSED_TIME);
	dt = timer.time - old_time;
	timer.frame = (timer.frame + 1) & (max_frame_time - 1);
	timer.frame_time[timer.frame] = dt;
	glutTimerFunc(timer.step, &update, timer.time);
	GameUpdate(timer.time, dt);
	/* now void -> if(!GameUpdate(timer.time, dt)) printf("Should really be shutting down.\n");*/
	/*fprintf(stderr, "Timer::update: %ums %ums.\n", timer.time, dt);*/
	glutPostRedisplay();
}