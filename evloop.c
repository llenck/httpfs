// for pipe2
#define _GNU_SOURCE

#include "evloop.h"

#include "fuse-includes.h"
#include "const-inodes.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <sys/epoll.h>

#include <curl/curl.h>

static int epoll_evmask_to_curl_evmask(uint32_t mask) {
	int ret = 0;
	if (mask | EPOLLIN) ret |= CURL_CSELECT_IN;
	if (mask | EPOLLOUT) ret |= CURL_CSELECT_OUT;
	if (mask | EPOLLERR) ret |= CURL_CSELECT_ERR;
	return ret;
}

static void get_time(struct timespec* t) {
	if (clock_gettime(CLOCK_MONOTONIC, t) < 0) {
		perror("Fatal error on clock_gettime");
		abort();
	}
}

static void adjust_timeout(int* timeout, struct timespec* last) {
	struct timespec now;
	get_time(&now);
	int diff_ms = (now.tv_sec - last->tv_sec) * 1000 +
			(now.tv_nsec - last->tv_nsec) / 1000000;
	if (*timeout != -1) {
		// set a timeout of 50ms if it would be < 0ms otherwise
		*timeout = *timeout - diff_ms > 0? *timeout - diff_ms : 50;
	}

	last->tv_sec = now.tv_sec;
	last->tv_nsec = now.tv_nsec;
}

int efd;
int rqfd_rd;
int rqfd_wr;
int timeout = -1;
CURLM* mh;
pthread_t evloop_handle;

static int socket_cb(CURL* easy, curl_socket_t fd, int action, void* u, void* s) {
	(void) easy;
	(void) u;
	(void) s;

	struct epoll_event event;
	event.events = 0;
	event.data.fd = fd;

	if (action == CURL_POLL_REMOVE) {
		int res = epoll_ctl(efd, EPOLL_CTL_DEL, fd, &event);
		if (res == -1 && errno != EBADF) {
			perror("Couldn't delete fd from epoll instance");
			abort();
		}

		return 0;
	}

	if (action == CURL_POLL_IN || action == CURL_POLL_INOUT) {
		event.events |= EPOLLIN;
	}
	if (action == CURL_POLL_OUT || action == CURL_POLL_INOUT) {
		event.events |= EPOLLOUT;
	}

	if (event.events != 0) {
		// try to add, and if that doesn't work, try to mod
		int res = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &event);
		if (res == -1) {
			res = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &event);
		}
		if (res == -1) {
			perror("Couldn't add to or modify fd from epoll instance");
		}
	}

	return 0;
}

static int timer_cb(CURLM* multi, long timeout_ms, void* u) {
	(void) multi;
	(void) u;

	timeout = timeout_ms;
	return 0;
}

static void epoll_error_action() {
	if (errno != EINTR) {
		perror("Fatal error on epoll_wait");
		abort();
	}
}

static void epoll_timeout_action(int* running_handles) {
	curl_multi_socket_action(mh, CURL_SOCKET_TIMEOUT, 0, running_handles);
}

static int handle_evmsg(struct evmsg* msg) {
	if (msg->type == EVMSG_EXIT) {
		// tell the caller to tell the event loop to exit
		return -1;
	}
	else if (msg->type == EVMSG_ADD_REQ) {
		curl_multi_add_handle(mh, msg->req->easy_handle);
	}
	else if (msg->type == EVMSG_DEL_REQ) {
		// the other threads should send this only if we are supposed to clean
		// msg->req up, otherwise, that is, if we are not currently in the process
		// of making this request, httpfs_release should clean the request up
		struct req_buf* req = msg->req;

		curl_multi_remove_handle(mh, req->easy_handle);
		curl_easy_cleanup(req->easy_handle);
		free(req->body);
		free(req->resp);
		free(req);
	}
	else {
		fprintf(stderr, "invalid evloop message type: %hhu\n", msg->type);
		abort();
	}

	return 0;
}

// returns 1 if curl was called, 0 if only requests on rqfd_rd were handled, and
// -1 if the event loop is to exit
static int epoll_events_action(struct epoll_event* evs, int num_events,
		int* running_handles)
{
	int curl_called = 0;

	for (int i = 0; i < num_events; i++) {
		struct epoll_event* ev = &evs[i];
		if (ev->data.fd != rqfd_rd) {
			curl_called = 1;
			curl_multi_socket_action(mh, ev->data.fd,
					epoll_evmask_to_curl_evmask(ev->events), running_handles);
		}
		else {
			errno = 0;
			struct evmsg msg;
			if (read(rqfd_rd, &msg, sizeof(msg)) != sizeof(msg)) {
				perror("Wrong size for read on rqfd_rd");
				abort();
			}

			if (handle_evmsg(&msg) < 0) {
				// pass the message that the event loop should exit
				return -1;
			}
		}
	}

	return curl_called;
}

static void* evloop(void* arg) {
	(void) arg;

	// for accurate timeouts, we can't just pass the timeout given by curl to epoll;
	// we might get notifications on rqfd_rd, or epoll might get interrupted by a
	// signal, and in that case we'll need a lower timeout for the next poll.

	// note: adjust_timeout also updates last_time, saving 1 call to clock_gettime
	int actual_timeout = timeout;
	struct timespec last_time;
	get_time(&last_time);
	
	struct epoll_event evs[16];
	int running_handles = 0;

	while (1) {
		int res = epoll_wait(efd, evs, 16, actual_timeout);
		if (res == -1) {
			epoll_error_action();

			// we got interrupted without calling curl, so adjust the timeout
			adjust_timeout(&actual_timeout, &last_time);
		}
		else if (res == 0) {
			epoll_timeout_action(&running_handles);

			// we called curl, so reset the timeout
			get_time(&last_time);
			actual_timeout = timeout;
		}
		else {
			int tmp = epoll_events_action(evs, res, &running_handles);
			if (tmp == -1) {
				// the main thread wants to clean up the event loop
				break;
			}

			if (tmp == 0) {
				// curl didn't get called, so adjust the timeout
				adjust_timeout(&actual_timeout, &last_time);
			}
			else {
				// curl got called, so just get the time and reset the actual timeout
				get_time(&last_time);
				actual_timeout = timeout;
			}
		}
	}

	return NULL;
}

void stop_evloop() {
	struct evmsg msg = { EVMSG_EXIT, NULL };
	if (write(rqfd_wr, &msg, sizeof(msg)) != sizeof(msg)) {
		// log, but ignore this condition; we're probably about to exit anyway
		perror("ignoring fail to send exit message to the event loop");
		return;
	}

	pthread_join(evloop_handle, NULL);

	curl_multi_cleanup(mh);
	curl_global_cleanup();
	close(rqfd_rd);
	close(rqfd_wr);
	close(efd);
}

int start_evloop() {
	efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd < 0) {
		return -1;
	}

	int pipes[2];
	if (pipe2(pipes, O_DIRECT | O_CLOEXEC) < 0) {
		goto err_close_efd;
	}
	rqfd_rd = pipes[0];
	rqfd_wr = pipes[1];

	struct epoll_event ev;

#ifndef NDEBUG
	// shut up valgrind
	memset(&ev, 0, sizeof(ev));
#endif

	ev.events = EPOLLIN;
	ev.data.fd = rqfd_rd;
	if (epoll_ctl(efd, EPOLL_CTL_ADD, rqfd_rd, &ev) < 0) {
		goto err_close_efd;
	}

	if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
		goto err_close_rqfds;
	}

	mh = curl_multi_init();
	if (mh == NULL) {
		goto err_curl_cleanup;
	}

	curl_multi_setopt(mh, CURLMOPT_SOCKETFUNCTION, socket_cb);
	curl_multi_setopt(mh, CURLMOPT_TIMERFUNCTION, timer_cb);

	if (pthread_create(&evloop_handle, NULL, evloop, NULL) != 0) {
		goto err_multi_cleanup;
	}

	return 0;

err_multi_cleanup:
	curl_multi_cleanup(mh);
err_curl_cleanup:
	curl_global_cleanup();
err_close_rqfds:
	close(rqfd_rd);
	close(rqfd_wr);
err_close_efd:
	close(efd);
	return -1;
}

static size_t save_resp(char* buf, size_t sz, size_t n, void* arg) {
	struct req_buf* rq = arg;

	size_t new_size = sz * n;
	size_t old_size = rq->resp_len;
	size_t total_size = rq->resp_len + new_size;

	// TODO readers/writer lock in rq->lock
	char* new_body = realloc(rq->body, total_size);
	if (new_body == NULL) {
		fprintf(stderr, "Warning: stopping transfer because of low memory\n");
		return 0;
	}
	rq->body = new_body;
	rq->resp_len = total_size;

	memcpy(rq->body + old_size, buf, new_size);

	return new_size;
}

//static void copy_urldecoded(char* src, char** dst) {
//	// TODO
//}

static int send_add_evmsg(struct req_buf* req) {
	struct evmsg msg = { EVMSG_ADD_REQ, req };
	return write(rqfd_wr, &msg, sizeof(msg)) == sizeof(msg)? 0 : -1;
}

// TODO more error checking
static void apply_default_curl_opts(struct req_buf* req, fuse_ino_t par_ino) {
	curl_easy_setopt(req->easy_handle, CURLOPT_URL, req->url);
	curl_easy_setopt(req->easy_handle, CURLOPT_FOLLOWLOCATION, 1);

	if (par_ino == DELETE_INODE) {
		curl_easy_setopt(req->easy_handle, CURLOPT_CUSTOMREQUEST, "DELETE");
	}
	// TODO post, put, etc

	if (par_ino == HEAD_INODE) {
		curl_easy_setopt(req->easy_handle, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(req->easy_handle, CURLOPT_HEADERFUNCTION, save_resp);
		curl_easy_setopt(req->easy_handle, CURLOPT_HEADERDATA, (void*)req);
	}
	else {
		curl_easy_setopt(req->easy_handle, CURLOPT_WRITEFUNCTION, save_resp);
		curl_easy_setopt(req->easy_handle, CURLOPT_WRITEDATA, (void*)req);
	}
}

struct req_buf* create_req(const char* url, fuse_ino_t par_ino) {
	struct req_buf* newb = malloc(sizeof(*newb));
	if (newb == NULL) {
		return NULL;
	}

	// TODO url decode instead of just duplicating
	char* url_copy = strdup(url);
	if (url_copy == NULL) {
		goto err_clean_newb;
	}
	newb->url = url_copy;

	newb->par_ino = par_ino;
	newb->sent = 0;
	newb->body = NULL;
	newb->body_len = 0;
	newb->resp = NULL;
	newb->resp_len = 0;

	switch (par_ino) {
	case GET_INODE:
	case HEAD_INODE:
	case DELETE_INODE:
	{
		CURL* ch = curl_easy_init();
		if (ch == NULL) {
			goto err_clean_url;
		}
		newb->easy_handle = ch;

		apply_default_curl_opts(newb, par_ino);

		if (send_add_evmsg(newb) < 0) {
			goto err_clean_curl;
		}

		break;
	}

	default:
		fprintf(stderr, "Unknown/unimplemented parent ino, ignoring request\n");
		goto err_clean_url;
		break;
	}

	return newb;

err_clean_curl:
	curl_easy_cleanup(newb->easy_handle);
err_clean_url:
	free(newb->url);
err_clean_newb:
	free(newb);
	return NULL;
}
