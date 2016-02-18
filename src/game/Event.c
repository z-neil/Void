/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#define PRINT_PEDANTIC

#include <stdio.h>  /* snprintf */
#include <stdarg.h>
#include <string.h> /* memcpy */
#include "../Print.h"
#include "../system/Timer.h"
#include "../general/Random.h"
#include "../general/Orcish.h"
#include "Event.h"

/* Events have a specific time to call a function. Events uses a pool which is
 drawn upon and stuck in the linked-list.

 @author	Neil
 @version	3.3; 2016-01
 @since		3.2; 2015-11 */

static struct Event {
	struct Event *prev, *next;
	char         label[16];
	unsigned     t_ms;
	struct Event **notify;
	enum FnType  fn_type;
	union {
		struct {
			void (*run)(void);
		} runnable;
		struct {
			void (*accept)(void *);
			void *t;
		} consumer;
		struct {
			void (*accept)(void *, void *);
			void *t;
			void *u;
		} biconsumer;
	} fn;
} events[2048], *first_event, *last_event, *iterator;
static const int events_capacity = sizeof events / sizeof(struct Event);
static int       events_size;

static struct Event *iterate(void);
void print_all_events(void);
char *decode_fn_type(const enum FnType fn_type);

/* public */

///////////////////
unsigned EventGetN(void) {
	return events_size;
}

/** Constructor.
 @return	An object or a null pointer, if the object couldn't be created. */
int Event(struct Event **const event_ptr, const int delay_ms, const int sigma_ms, enum FnType fn_type, ...) {
	va_list args;
	struct Event *event, *prev, *next;
	int real_delay_ms;
	unsigned i;

	if(events_size >= events_capacity) {
		Warn("Event: couldn't be created; reached maximum of %u/%u.\n", events_size, events_capacity);
		return 0;
	}

	real_delay_ms = delay_ms;
	if(sigma_ms > 0) {
		/* FIXME: Truncated Guassian */
		real_delay_ms += RandomUniformInt(sigma_ms);
		if(real_delay_ms < 0) real_delay_ms = 0;
	}

	if(event_ptr && *event_ptr) {
		Warn("!!!!!! Event: would override %s on creation. WTF!!!!\n", EventToString(*event_ptr));
		return 0;
	}

	event = &events[events_size++];

	event->prev    = event->next = 0;
	Orcish(event->label, sizeof event->label);
	/*event->label   = label;*/
	event->t_ms    = TimerGetGameTime() + real_delay_ms;
	event->notify  = event_ptr;
	if(event_ptr) *event_ptr = event;
	event->fn_type = fn_type;
	/* do something different based on what the type is */
	va_start(args, fn_type);
	switch(fn_type) {
		case FN_RUNNABLE:
			event->fn.runnable.run = va_arg(args, void (*)(void));
			break;
		case FN_CONSUMER:
			event->fn.consumer.accept = va_arg(args, void (*)(void *));
			event->fn.consumer.t      = va_arg(args, void *);
			break;
		case FN_BICONSUMER:
			event->fn.biconsumer.accept = va_arg(args, void (*)(void *, void *));
			event->fn.biconsumer.t      = va_arg(args, void *);
			event->fn.biconsumer.u      = va_arg(args, void *);
			break;
	}
	va_end(args);

	/* O(n), technically; however, new values have a higher likelihood of being
	 at the end. FIXME: gets screwed up 29.3 days in!!!! use TimerCompare() */
	/* FIXME: singly-linked!!! more events are happening soon */
	for(prev = last_event, next = 0, i = 0;
		prev && prev->t_ms > event->t_ms;
		next = prev, prev = prev->prev, i++);
	if(prev) {
		event->next = prev->next;
		prev->next  = event;
	} else {
		event->next = first_event;
		first_event = event;
	}
	if(next) {
		event->prev = next->prev;
		next->prev  = event;
	} else {
		event->prev = last_event;
		last_event  = event;
	}

	Pedantic("Event: new %s at %dms inefficiency %u (size %u.)\n", EventToString(event), event->t_ms, i, events_size);

	return -1;
}

/** Destructor.
 @param event_ptr	A reference to the object that is to be deleted. */
void Event_(struct Event **event_ptr) {
	struct Event *event, *replace;
	int index;
	unsigned characters;
	char buffer[128];

	if(!event_ptr || !(event = *event_ptr)) return;
	index = event - events;
	if(index < 0 || index >= events_size) {
		Warn("~Event: %s, %u not in range %u.\n", EventToString(event), index + 1, events_size);
		print_all_events();
		return;
	}

	/* store the string for debug info */
	characters = snprintf(buffer, sizeof buffer, "%s", EventToString(event));

	/* update notify */
	if(event->notify) {
		Debug("~Event: %s notify %s to 0.\n", EventToString(event), EventToString(*event->notify));
		*event->notify = 0;
	}

	/* take it out of the queue */
	if(event->prev) event->prev->next = event->next;
	else            first_event       = event->next;
	if(event->next) event->next->prev = event->prev;
	else            last_event        = event->prev;

	/* replace it with the last one */
	if(index < --events_size) {

		replace = &events[events_size];
		memcpy(event, replace, sizeof(struct Event));

		/* insert it into the new position */
		if(event->prev) event->prev->next = event;
		else            first_event       = event;
		if(event->next) event->next->prev = event;
		else            last_event        = event;

		/* update notify */
		if(event->notify) *event->notify = event;

		if(characters < sizeof buffer) snprintf(buffer + characters, sizeof buffer - characters, "; replaced by %s", EventToString(event));
	}
	/* FIXME: this is not the same event */
	Pedantic("~Event: erase, %s size %u.\n", buffer, events_size);

	*event_ptr = event = 0;
}

/** Flush the Events without Dispatch according to predicate. */
void EventRemoveIf(int (*const predicate)(struct Event *const)) {
	struct Event *e;

	Debug("Event::clear: clearing Events.\n");
	/*print_all_events();*/
	while((e = iterate())) {
		Pedantic("Event::removeIf: consdering %s.\n", EventToString(e));
		if(!predicate || predicate(e)) Event_(&e);
	}
}

/** This updates the notify field when the thing you're notifying changes
 addresses. Takes a pointer. */
void EventSetNotify(struct Event **const e_ptr) {
	struct Event *e;

	if(!e_ptr || !(e = *e_ptr)) return;
	if(e->notify) Warn("Event::setNotify: #%p overriding a previous notification #%p on #%p.\n", (void *)e_ptr, (void *)e->notify, (void *)e);
	e->notify = e_ptr;
}

void (*EventGetConsumerAccept(const struct Event *const e))(void *) {
	if(!e || e->fn_type != FN_CONSUMER) return 0;
	return e->fn.consumer.accept;
}

/** Dispach all events up to the present. */
void EventDispatch(void) {
	struct Event *e;

	while((e = first_event) && TimerIsTime(e->t_ms)) {
		const enum FnType fn_type = e->fn_type;
		void (*runnable)(void) = 0;
		void (*consumer)(void *) = 0;
		void (*biconsumer)(void *, void *) = 0;
		void *t = 0, *u = 0;

		/* make a copy */
		switch(fn_type) {
			case FN_RUNNABLE:
				runnable = e->fn.runnable.run;
				break;
			case FN_CONSUMER:
				consumer = e->fn.consumer.accept;
				t = e->fn.consumer.t;
				break;
			case FN_BICONSUMER:
				biconsumer = e->fn.biconsumer.accept;
				t = e->fn.biconsumer.t;
				u = e->fn.biconsumer.u;
				break;
		}
		/* delete */
		Event_(&e);
		Debug("Event: %s triggered; %u events left.\n", decode_fn_type(fn_type), events_size);
		switch(fn_type) {
			case FN_RUNNABLE:	runnable();			break;
			case FN_CONSUMER:	consumer(t);		break;
			case FN_BICONSUMER:	biconsumer(t, u);	break;
		}
	}
}

/////////
struct Sprite;
void ship_recharge(struct Sprite *const a);
char *SpriteToString(const struct Sprite *const s);

/** Sometimes you just want to change your mind. */
void EventReplaceArguments(struct Event *const event, ...) {
	va_list args;

	if(!event) return;
	Pedantic("Event::replaceArguments: %s.\n", EventToString(event));

	va_start(args, event);
	switch(event->fn_type) {
		case FN_RUNNABLE:
			Warn("Event::replaceArguments: %s has no arguments.\n", EventToString(event));
			break;
		case FN_CONSUMER:
			Warn("!\tEvent::replaceArguments: %s will fuckup?!!!!\n", EventToString(event));
			// FIXME!!!!!!!
			// AHA!!!!!!!!!! I've found the problem, I think!!!
			// called by Sprite.c:450
			event->fn.consumer.t   = va_arg(args, void *);
			Warn("!\tEvent::replaceArguments: now %s\n", SpriteToString(event->fn.consumer.t));
			break;
		case FN_BICONSUMER:
			event->fn.biconsumer.t = va_arg(args, void *);
			event->fn.biconsumer.u = va_arg(args, void *);
			break;
	}
	va_end(args);
}

char *EventToString(const struct Event *const e) {
	static int b;
	static char buffer[4][64];
	int last_b;

	if(!e) {
		snprintf(buffer[b], sizeof buffer[b], "%s", "null event");
	} else {
		//snprintf(buffer[b], sizeof buffer[b], "%sEvt%u[%c at %ums]", decode_fn_type(e->fn_type), (int)(e - events) + 1, e->label, e->t_ms);
		//snprintf(buffer[b], sizeof buffer[b], "%sEvt%u[%c at %ums]%s", decode_fn_type(e->fn_type), (int)(e - events) + 1, e->label, e->t_ms, e->fn_type == FN_CONSUMER && e->fn.consumer.accept == (void (*)(void *))&ship_recharge ? SpriteToString(e->fn.consumer.t) : "<not a recharge>");
		//snprintf(buffer[b], sizeof buffer[b], "%sEvt%u[%c at %ums]#%p", decode_fn_type(e->fn_type), (int)(e - events) + 1, e->label, e->t_ms, e->fn_type == FN_CONSUMER && e->fn.consumer.accept == (void (*)(void *))&ship_recharge ? e->fn.consumer.t : 0);
		snprintf(buffer[b], sizeof buffer[b], "%sEvt%s[%u %ums]", decode_fn_type(e->fn_type), e->label, (int)(e - events) + 1, e->t_ms);
	};
	last_b = b;
	b = (b + 1) & 3;

	return buffer[last_b];
}

static struct Event *iterate(void) {
	if(!iterator) {
		iterator = first_event;
	} else {
		iterator = iterator->next;
	}
	return iterator;
}

void print_all_events(void) {
	struct Event *e;
	/*int i;*/
	Debug("Events %ums {\n", TimerGetGameTime());
	for(e = first_event; e; e = e->next) {
		Debug("\t%s\n", EventToString(e));
	}
	/*Debug("} or {\n");
	for(i = 0; i < events_size; i++) {
		Debug("\t%s\n", EventToString(events + i));
	}*/
	Debug("}\n");
}

char *decode_fn_type(const enum FnType fn_type) {
	switch(fn_type) {
		case FN_RUNNABLE:	return "<Runnable>";
		case FN_CONSUMER:	return "<Consumer>";
		case FN_BICONSUMER:	return "<Biconsumer>";
		default:			return "<not an event type>";
	}
}
