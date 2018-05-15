/* This file is part of ToaruOS and is released under the terms
 * of the NCSA / University of Illinois License - see LICENSE.md
 * Copyright (C) 2014-2018 K. Lange
 */
#include <string.h>
#include <stdlib.h>
#include <syscall.h>

#include <toaru/pex.h>
#include <toaru/graphics.h>
#include <toaru/kbd.h>
#include <toaru/hashmap.h>
#include <toaru/list.h>
#include <toaru/yutani.h>
#include <toaru/mouse.h>

yutani_msg_t * yutani_wait_for(yutani_t * y, uint32_t type) {
	do {
		yutani_msg_t * out;
		size_t size;
		{
			char tmp[MAX_PACKET_SIZE];
			size = pex_recv(y->sock, tmp);
			out = malloc(size);
			memcpy(out, tmp, size);
		}

		if (out->type == type) {
			return out;
		} else {
			list_insert(y->queued, out);
		}
	} while (1); /* XXX: (!y->abort) */
}

size_t yutani_query(yutani_t * y) {
	if (y->queued->length > 0) return 1;
	return pex_query(y->sock);
}

static void _handle_internal(yutani_t * y, yutani_msg_t * out) {
	switch (out->type) {
		case YUTANI_MSG_WELCOME:
			{
				struct yutani_msg_welcome * mw = (void *)out->data;
				y->display_width = mw->display_width;
				y->display_height = mw->display_height;
			}
			break;
		case YUTANI_MSG_WINDOW_MOVE:
			{
				struct yutani_msg_window_move * wm = (void *)out->data;
				yutani_window_t * win = hashmap_get(y->windows, (void *)wm->wid);
				if (win) {
					win->x = wm->x;
					win->y = wm->y;
				}
			}
			break;
		default:
			break;
	}
}

yutani_msg_t * yutani_poll(yutani_t * y) {
	yutani_msg_t * out;

	if (y->queued->length > 0) {
		node_t * node = list_dequeue(y->queued);
		out = (yutani_msg_t *)node->value;
		free(node);
		_handle_internal(y, out);
		return out;
	}

	size_t size;
	{
		char tmp[MAX_PACKET_SIZE];
		size = pex_recv(y->sock, tmp);
		out = malloc(size);
		memcpy(out, tmp, size);
	}

	_handle_internal(y, out);

	return out;
}

yutani_msg_t * yutani_poll_async(yutani_t * y) {
	if (yutani_query(y) > 0) {
		return yutani_poll(y);
	}
	return NULL;
}

void yutani_msg_buildx_hello(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_HELLO;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_flip(yutani_msg_t * msg, yutani_wid_t wid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_FLIP;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_flip);

	struct yutani_msg_flip * mw = (void *)msg->data;

	mw->wid = wid;
}


void yutani_msg_buildx_welcome(yutani_msg_t * msg, uint32_t width, uint32_t height) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WELCOME;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_welcome);

	struct yutani_msg_welcome * mw = (void *)msg->data;

	mw->display_width = width;
	mw->display_height = height;
}


void yutani_msg_buildx_window_new(yutani_msg_t * msg, uint32_t width, uint32_t height) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_NEW;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_new);

	struct yutani_msg_window_new * mw = (void *)msg->data;

	mw->width = width;
	mw->height = height;
}


void yutani_msg_buildx_window_new_flags(yutani_msg_t * msg, uint32_t width, uint32_t height, uint32_t flags) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_NEW_FLAGS;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_new_flags);

	struct yutani_msg_window_new_flags * mw = (void *)msg->data;

	mw->width = width;
	mw->height = height;
	mw->flags = flags;
}


void yutani_msg_buildx_window_init(yutani_msg_t * msg, yutani_wid_t wid, uint32_t width, uint32_t height, uint32_t bufid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_INIT;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_init);

	struct yutani_msg_window_init * mw = (void *)msg->data;

	mw->wid = wid;
	mw->width = width;
	mw->height = height;
	mw->bufid = bufid;
}


void yutani_msg_buildx_window_close(yutani_msg_t * msg, yutani_wid_t wid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_CLOSE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_close);

	struct yutani_msg_window_close * mw = (void *)msg->data;

	mw->wid = wid;
}


void yutani_msg_buildx_key_event(yutani_msg_t * msg, yutani_wid_t wid, key_event_t * event, key_event_state_t * state) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_KEY_EVENT;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_key_event);

	struct yutani_msg_key_event * mw = (void *)msg->data;

	mw->wid = wid;
	memcpy(&mw->event, event, sizeof(key_event_t));
	memcpy(&mw->state, state, sizeof(key_event_state_t));
}


void yutani_msg_buildx_mouse_event(yutani_msg_t * msg, yutani_wid_t wid, mouse_device_packet_t * event, int32_t type) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_MOUSE_EVENT;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_mouse_event);

	struct yutani_msg_mouse_event * mw = (void *)msg->data;

	mw->wid = wid;
	memcpy(&mw->event, event, sizeof(mouse_device_packet_t));
	mw->type = type;
}


void yutani_msg_buildx_window_move(yutani_msg_t * msg, yutani_wid_t wid, int32_t x, int32_t y) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_MOVE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_move);

	struct yutani_msg_window_move * mw = (void *)msg->data;

	mw->wid = wid;
	mw->x = x;
	mw->y = y;
}


void yutani_msg_buildx_window_stack(yutani_msg_t * msg, yutani_wid_t wid, int z) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_STACK;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_stack);

	struct yutani_msg_window_stack * mw = (void *)msg->data;

	mw->wid = wid;
	mw->z = z;
}


void yutani_msg_buildx_window_focus_change(yutani_msg_t * msg, yutani_wid_t wid, int focused) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_FOCUS_CHANGE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_focus_change);

	struct yutani_msg_window_focus_change * mw = (void *)msg->data;

	mw->wid = wid;
	mw->focused = focused;
}


void yutani_msg_buildx_window_mouse_event(yutani_msg_t * msg, yutani_wid_t wid, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y, uint8_t buttons, uint8_t command) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_MOUSE_EVENT;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_mouse_event);

	struct yutani_msg_window_mouse_event * mw = (void *)msg->data;

	mw->wid = wid;
	mw->new_x = new_x;
	mw->new_y = new_y;
	mw->old_x = old_x;
	mw->old_y = old_y;
	mw->buttons = buttons;
	mw->command = command;
}


void yutani_msg_buildx_flip_region(yutani_msg_t * msg, yutani_wid_t wid, int32_t x, int32_t y, int32_t width, int32_t height) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_FLIP_REGION;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_flip_region);

	struct yutani_msg_flip_region * mw = (void *)msg->data;

	mw->wid = wid;
	mw->x = x;
	mw->y = y;
	mw->width = width;
	mw->height = height;
}


void yutani_msg_buildx_window_resize(yutani_msg_t * msg, uint32_t type, yutani_wid_t wid, uint32_t width, uint32_t height, uint32_t bufid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = type;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_resize);

	struct yutani_msg_window_resize * mw = (void *)msg->data;

	mw->wid = wid;
	mw->width = width;
	mw->height = height;
	mw->bufid = bufid;
}


void yutani_msg_buildx_window_advertise(yutani_msg_t * msg, yutani_wid_t wid, uint32_t flags, uint16_t * offsets, size_t length, char * data) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_ADVERTISE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_advertise) + length;

	struct yutani_msg_window_advertise * mw = (void *)msg->data;

	mw->wid = wid;
	mw->flags = flags;
	mw->size = length;
	if (offsets) {
		memcpy(mw->offsets, offsets, sizeof(uint16_t)*5);
	} else {
		memset(mw->offsets, 0, sizeof(uint16_t)*5);
	}
	if (data) {
		memcpy(mw->strings, data, mw->size);
	}
}


void yutani_msg_buildx_subscribe(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_SUBSCRIBE;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_unsubscribe(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_UNSUBSCRIBE;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_query_windows(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_QUERY_WINDOWS;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_notify(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_NOTIFY;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_session_end(yutani_msg_t * msg) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_SESSION_END;
	msg->size  = sizeof(struct yutani_message);
}


void yutani_msg_buildx_window_focus(yutani_msg_t * msg, yutani_wid_t wid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_FOCUS;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_focus);

	struct yutani_msg_window_focus * mw = (void *)msg->data;

	mw->wid = wid;
}


void yutani_msg_buildx_key_bind(yutani_msg_t * msg, kbd_key_t key, kbd_mod_t mod, int response) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_KEY_BIND;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_key_bind);

	struct yutani_msg_key_bind * mw = (void *)msg->data;

	mw->key = key;
	mw->modifiers = mod;
	mw->response = response;
}


void yutani_msg_buildx_window_drag_start(yutani_msg_t * msg, yutani_wid_t wid) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_DRAG_START;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_drag_start);

	struct yutani_msg_window_drag_start * mw = (void *)msg->data;

	mw->wid = wid;
}


void yutani_msg_buildx_window_update_shape(yutani_msg_t * msg, yutani_wid_t wid, int set_shape) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_UPDATE_SHAPE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_update_shape);

	struct yutani_msg_window_update_shape * mw = (void *)msg->data;

	mw->wid = wid;
	mw->set_shape = set_shape;
}


void yutani_msg_buildx_window_warp_mouse(yutani_msg_t * msg, yutani_wid_t wid, int32_t x, int32_t y) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_WARP_MOUSE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_warp_mouse);

	struct yutani_msg_window_warp_mouse * mw = (void *)msg->data;

	mw->wid = wid;
	mw->x = x;
	mw->y = y;
}


void yutani_msg_buildx_window_show_mouse(yutani_msg_t * msg, yutani_wid_t wid, int32_t show_mouse) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_SHOW_MOUSE;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_show_mouse);

	struct yutani_msg_window_show_mouse * mw = (void *)msg->data;

	mw->wid = wid;
	mw->show_mouse = show_mouse;
}


void yutani_msg_buildx_window_resize_start(yutani_msg_t * msg, yutani_wid_t wid, yutani_scale_direction_t direction) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_WINDOW_RESIZE_START;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_window_resize_start);

	struct yutani_msg_window_resize_start * mw = (void *)msg->data;

	mw->wid = wid;
	mw->direction = direction;
}


void yutani_msg_buildx_special_request(yutani_msg_t * msg, yutani_wid_t wid, uint32_t request) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_SPECIAL_REQUEST;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_special_request);

	struct yutani_msg_special_request * sr = (void *)msg->data;

	sr->wid   = wid;
	sr->request = request;
}

void yutani_msg_buildx_clipboard(yutani_msg_t * msg, char * content) {
	msg->magic = YUTANI_MSG__MAGIC;
	msg->type  = YUTANI_MSG_CLIPBOARD;
	msg->size  = sizeof(struct yutani_message) + sizeof(struct yutani_msg_clipboard) + strlen(content);

	struct yutani_msg_clipboard * cl = (void *)msg->data;

	cl->size = strlen(content);
	memcpy(cl->content, content, strlen(content));
}

int yutani_msg_send(yutani_t * y, yutani_msg_t * msg) {
	return pex_reply(y->sock, msg->size, (char *)msg);
}

yutani_t * yutani_context_create(FILE * socket) {
	yutani_t * out = malloc(sizeof(yutani_t));

	out->sock = socket;
	out->display_width  = 0;
	out->display_height = 0;
	out->windows = hashmap_create_int(10);
	out->queued = list_create();
	return out;
}

yutani_t * yutani_init(void) {
	/* XXX: Display, etc? */
	char * server_name = getenv("DISPLAY");
	if (!server_name) {
		server_name = "compositor";
	}
	FILE * c = pex_connect(server_name);

	if (!c) {
		return NULL; /* Connection failed. */
	}

	yutani_t * y = yutani_context_create(c);
	yutani_msg_buildx_hello_alloc(m);
	yutani_msg_buildx_hello(m);
	yutani_msg_send(y, m);

	yutani_msg_t * mm = yutani_wait_for(y, YUTANI_MSG_WELCOME);
	struct yutani_msg_welcome * mw = (void *)&mm->data;
	y->display_width = mw->display_width;
	y->display_height = mw->display_height;
	y->server_ident = server_name;
	free(mm);

	return y;
}

yutani_window_t * yutani_window_create_flags(yutani_t * y, int width, int height, uint32_t flags) {
	yutani_window_t * win = malloc(sizeof(yutani_window_t));

	yutani_msg_buildx_window_new_flags_alloc(m);
	yutani_msg_buildx_window_new_flags(m, width, height, flags);
	yutani_msg_send(y, m);

	yutani_msg_t * mm = yutani_wait_for(y, YUTANI_MSG_WINDOW_INIT);
	struct yutani_msg_window_init * mw = (void *)&mm->data;

	win->width = mw->width;
	win->height = mw->height;
	win->bufid = mw->bufid;
	win->wid = mw->wid;
	win->focused = 0;
	win->is_decorated = 0;
	win->x = 0;
	win->y = 0;
	win->user_data = NULL;
	win->ctx = y;
	free(mm);

	hashmap_set(y->windows, (void*)win->wid, win);

	char key[1024];
	YUTANI_SHMKEY(y->server_ident, key, 1024, win);

	size_t size = (width * height * 4);
	win->buffer = (char *)syscall_shm_obtain(key, &size);
	return win;

}

yutani_window_t * yutani_window_create(yutani_t * y, int width, int height) {
	return yutani_window_create_flags(y,width,height,0);
}

void yutani_flip(yutani_t * y, yutani_window_t * win) {
	yutani_msg_buildx_flip_alloc(m);
	yutani_msg_buildx_flip(m, win->wid);
	yutani_msg_send(y, m);
}

void yutani_flip_region(yutani_t * yctx, yutani_window_t * win, int32_t x, int32_t y, int32_t width, int32_t height) {
	yutani_msg_buildx_flip_region_alloc(m);
	yutani_msg_buildx_flip_region(m, win->wid, x, y, width, height);
	yutani_msg_send(yctx, m);
}

void yutani_close(yutani_t * y, yutani_window_t * win) {
	yutani_msg_buildx_window_close_alloc(m);
	yutani_msg_buildx_window_close(m, win->wid);
	yutani_msg_send(y, m);

	/* Now destroy our end of the window */
	{
		char key[1024];
		YUTANI_SHMKEY_EXP(y->server_ident, key, 1024, win->bufid);
		syscall_shm_release(key);
	}

	hashmap_remove(y->windows, (void*)win->wid);
	free(win);
}

void yutani_window_move(yutani_t * yctx, yutani_window_t * window, int x, int y) {
	yutani_msg_buildx_window_move_alloc(m);
	yutani_msg_buildx_window_move(m, window->wid, x, y);
	yutani_msg_send(yctx, m);
}

void yutani_set_stack(yutani_t * yctx, yutani_window_t * window, int z) {
	yutani_msg_buildx_window_stack_alloc(m);
	yutani_msg_buildx_window_stack(m, window->wid, z);
	yutani_msg_send(yctx, m);
}

void yutani_window_resize(yutani_t * yctx, yutani_window_t * window, uint32_t width, uint32_t height) {
	yutani_msg_buildx_window_resize_alloc(m);
	yutani_msg_buildx_window_resize(m, YUTANI_MSG_RESIZE_REQUEST, window->wid, width, height, 0);
	yutani_msg_send(yctx, m);
}

void yutani_window_resize_offer(yutani_t * yctx, yutani_window_t * window, uint32_t width, uint32_t height) {
	yutani_msg_buildx_window_resize_alloc(m);
	yutani_msg_buildx_window_resize(m, YUTANI_MSG_RESIZE_OFFER, window->wid, width, height, 0);
	yutani_msg_send(yctx, m);
}

void yutani_window_resize_accept(yutani_t * yctx, yutani_window_t * window, uint32_t width, uint32_t height) {
	yutani_msg_buildx_window_resize_alloc(m);
	yutani_msg_buildx_window_resize(m, YUTANI_MSG_RESIZE_ACCEPT, window->wid, width, height, 0);
	yutani_msg_send(yctx, m);

	/* Now wait for the new bufid */
	yutani_msg_t * mm = yutani_wait_for(yctx, YUTANI_MSG_RESIZE_BUFID);
	struct yutani_msg_window_resize * wr = (void*)mm->data;

	if (window->wid != wr->wid) {
		/* I am not sure what to do here. */
		return;
	}

	/* Update the window */
	window->width = wr->width;
	window->height = wr->height;
	window->oldbufid = window->bufid;
	window->bufid = wr->bufid;
	free(mm);

	/* Allocate the buffer */
	{
		char key[1024];
		YUTANI_SHMKEY(yctx->server_ident, key, 1024, window);

		size_t size = (window->width * window->height * 4);
		window->buffer = (char *)syscall_shm_obtain(key, &size);
	}
}

void yutani_window_resize_done(yutani_t * yctx, yutani_window_t * window) {
	/* Destroy the old buffer */
	{
		char key[1024];
		YUTANI_SHMKEY_EXP(yctx->server_ident, key, 1024, window->oldbufid);
		syscall_shm_release(key);
	}

	yutani_msg_buildx_window_resize_alloc(m);
	yutani_msg_buildx_window_resize(m, YUTANI_MSG_RESIZE_DONE, window->wid, window->width, window->height, window->bufid);
	yutani_msg_send(yctx, m);
}

void yutani_window_advertise(yutani_t * yctx, yutani_window_t * window, char * name) {

	uint32_t flags = 0; /* currently, no client flags */
	uint16_t offsets[5] = {0,0,0,0,0};
	uint32_t length = 0;
	char * strings;

	if (!name) {
		length = 1;
		strings = " ";
	} else {
		length = strlen(name) + 1;
		strings = name;
		/* All the other offsets will point to null characters */
		offsets[1] = strlen(name);
		offsets[2] = strlen(name);
		offsets[3] = strlen(name);
		offsets[4] = strlen(name);
	}

	yutani_msg_buildx_window_advertise_alloc(m, length);
	yutani_msg_buildx_window_advertise(m, window->wid, flags, offsets, length, strings);
	yutani_msg_send(yctx, m);
}

void yutani_window_advertise_icon(yutani_t * yctx, yutani_window_t * window, char * name, char * icon) {

	uint32_t flags = 0; /* currently no client flags */
	uint16_t offsets[5] = {0,0,0,0,0};
	uint32_t length = strlen(name) + strlen(icon) + 2;
	char * strings = malloc(length);

	if (name) {
		memcpy(&strings[0], name, strlen(name)+1);
		offsets[0] = 0;
		offsets[1] = strlen(name);
		offsets[2] = strlen(name);
		offsets[3] = strlen(name);
		offsets[4] = strlen(name);
	}
	if (icon) {
		memcpy(&strings[offsets[1]+1], icon, strlen(icon)+1);
		offsets[1] = strlen(name)+1;
		offsets[2] = strlen(name)+1+strlen(icon);
		offsets[3] = strlen(name)+1+strlen(icon);
		offsets[4] = strlen(name)+1+strlen(icon);
	}

	yutani_msg_buildx_window_advertise_alloc(m, length);
	yutani_msg_buildx_window_advertise(m, window->wid, flags, offsets, length, strings);
	yutani_msg_send(yctx, m);
	free(strings);
}

void yutani_subscribe_windows(yutani_t * y) {
	yutani_msg_buildx_subscribe_alloc(m);
	yutani_msg_buildx_subscribe(m);
	yutani_msg_send(y, m);
}

void yutani_unsubscribe_windows(yutani_t * y) {
	yutani_msg_buildx_unsubscribe_alloc(m);
	yutani_msg_buildx_unsubscribe(m);
	yutani_msg_send(y, m);
}

void yutani_query_windows(yutani_t * y) {
	yutani_msg_buildx_query_windows_alloc(m);
	yutani_msg_buildx_query_windows(m);
	yutani_msg_send(y, m);
}

void yutani_session_end(yutani_t * y) {
	yutani_msg_buildx_session_end_alloc(m);
	yutani_msg_buildx_session_end(m);
	yutani_msg_send(y, m);
}

void yutani_focus_window(yutani_t * yctx, yutani_wid_t wid) {
	yutani_msg_buildx_window_focus_alloc(m);
	yutani_msg_buildx_window_focus(m, wid);
	yutani_msg_send(yctx, m);
}

void yutani_key_bind(yutani_t * yctx, kbd_key_t key, kbd_mod_t mod, int response) {
	yutani_msg_buildx_key_bind_alloc(m);
	yutani_msg_buildx_key_bind(m, key,mod,response);
	yutani_msg_send(yctx, m);
}

void yutani_window_drag_start(yutani_t * yctx, yutani_window_t * window) {
	yutani_msg_buildx_window_drag_start_alloc(m);
	yutani_msg_buildx_window_drag_start(m, window->wid);
	yutani_msg_send(yctx, m);
}

void yutani_window_drag_start_wid(yutani_t * yctx, yutani_wid_t wid) {
	yutani_msg_buildx_window_drag_start_alloc(m);
	yutani_msg_buildx_window_drag_start(m, wid);
	yutani_msg_send(yctx, m);
}

void yutani_window_update_shape(yutani_t * yctx, yutani_window_t * window, int set_shape) {
	yutani_msg_buildx_window_update_shape_alloc(m);
	yutani_msg_buildx_window_update_shape(m, window->wid, set_shape);
	yutani_msg_send(yctx, m);
}

void yutani_window_warp_mouse(yutani_t * yctx, yutani_window_t * window, int32_t x, int32_t y) {
	yutani_msg_buildx_window_warp_mouse_alloc(m);
	yutani_msg_buildx_window_warp_mouse(m, window->wid, x, y);
	yutani_msg_send(yctx, m);
}

void yutani_window_show_mouse(yutani_t * yctx, yutani_window_t * window, int32_t show_mouse) {
	yutani_msg_buildx_window_show_mouse_alloc(m);
	yutani_msg_buildx_window_show_mouse(m, window->wid, show_mouse);
	yutani_msg_send(yctx, m);
}

void yutani_window_resize_start(yutani_t * yctx, yutani_window_t * window, yutani_scale_direction_t direction) {
	yutani_msg_buildx_window_resize_start_alloc(m);
	yutani_msg_buildx_window_resize_start(m, window->wid, direction);
	yutani_msg_send(yctx, m);
}

void yutani_special_request(yutani_t * yctx, yutani_window_t * window, uint32_t request) {
	/* wid isn't necessary; if window is null, set to 0 */
	yutani_msg_buildx_special_request_alloc(m);
	yutani_msg_buildx_special_request(m, window ? window->wid : 0, request);
	yutani_msg_send(yctx, m);
}

void yutani_special_request_wid(yutani_t * yctx, yutani_wid_t wid, uint32_t request) {
	/* For working with other applications' windows */
	yutani_msg_buildx_special_request_alloc(m);
	yutani_msg_buildx_special_request(m, wid, request);
	yutani_msg_send(yctx, m);
}

void yutani_set_clipboard(yutani_t * yctx, char * content) {
	/* Set clipboard contents */
	int len = strlen(content);
	if (len > 511) {
		char tmp_file[100];
		sprintf(tmp_file, "/tmp/.clipboard.%s", yctx->server_ident);
		FILE * tmp = fopen(tmp_file, "w+");
		fwrite(content, len, 1, tmp);
		fclose(tmp);

		char tmp_data[100];
		sprintf(tmp_data, "\002 %d", len);
		yutani_msg_buildx_clipboard_alloc(m, strlen(tmp_data));
		yutani_msg_buildx_clipboard(m, tmp_data);
		yutani_msg_send(yctx, m);
	} else {
		yutani_msg_buildx_clipboard_alloc(m, len);
		yutani_msg_buildx_clipboard(m, content);
		yutani_msg_send(yctx, m);
	}
}

FILE * yutani_open_clipboard(yutani_t * yctx) {
	char tmp_file[100];
	sprintf(tmp_file, "/tmp/.clipboard.%s", yctx->server_ident);
	return fopen(tmp_file, "r");
}

gfx_context_t * init_graphics_yutani(yutani_window_t * window) {
	gfx_context_t * out = malloc(sizeof(gfx_context_t));
	out->width  = window->width;
	out->height = window->height;
	out->depth  = 32;
	out->size   = GFX_H(out) * GFX_W(out) * GFX_B(out);
	out->buffer = window->buffer;
	out->backbuffer = out->buffer;
	out->clips  = NULL;
	return out;
}

gfx_context_t *  init_graphics_yutani_double_buffer(yutani_window_t * window) {
	gfx_context_t * out = init_graphics_yutani(window);
	out->backbuffer = malloc(GFX_B(out) * GFX_W(out) * GFX_H(out));
	return out;
}

void reinit_graphics_yutani(gfx_context_t * out, yutani_window_t * window) {
	out->width  = window->width;
	out->height = window->height;
	out->depth  = 32;
	out->size   = GFX_H(out) * GFX_W(out) * GFX_B(out);
	if (out->buffer == out->backbuffer) {
		out->buffer = window->buffer;
		out->backbuffer = out->buffer;
	} else {
		out->buffer = window->buffer;
		out->backbuffer = realloc(out->backbuffer, GFX_B(out) * GFX_W(out) * GFX_H(out));
	}
}

void release_graphics_yutani(gfx_context_t * gfx) {
	if (gfx->backbuffer != gfx->buffer) {
		free(gfx->backbuffer);
	}
	free(gfx);
}
