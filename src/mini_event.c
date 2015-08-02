/* A mini event loop library, copy from antirez/redis.
 *
 * goby <goby@foxmail.com>
 * 2015-08-02
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "mini_event.h"

/* Check platform */
#ifdef __linux__
#define USE_EPOLL 1
#include "mini_epoll.c"
#else
#    if (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)) || \
        defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#       define USE_KQUEUE 1
#       include "mini_kqueue.c"
#   else
#       define USE_SELECT 1
#       include "mini_select.c"
#   endif
#endif

#ifndef API_NAME
#define API_NAME "unknown"
#endif

char *get_api_name(void) {
    return API_NAME;
}

int create_event_loop(event_loop* ev, int setsize) {
    ev->events = realloc(ev->events, sizeof(file_event) * setsize);
    ev->fired  = realloc(ev->fired, sizeof(fired_event) * setsize);
    if (!ev->events || !ev->fired) goto error;

    ev->setsize     = setsize;
    ev->last_time   = time(NULL);
    ev->te_head     = NULL;
    ev->time_event_next_id = 0;
    ev->stop        = 0;
    ev->maxfd       = -1;
    ev->before_sleep = NULL;

    if (api_create(ev) == -1)
        goto error;

    for(int i = 0; i < setsize; i++) {
        ev->events[i].mask = MINI_NONE;
    }

    return 0;
error:
    return -1;
}

void delete_event_loop(event_loop *ev) {
    api_free(ev);
    free(ev->events);
    free(ev->fired);
}

int get_setsize(event_loop *ev) {
    return ev->setsize;
}

int set_setsize(event_loop *ev, int setsize) {
    if (setsize == ev->setsize)
        return 0;
    if (ev->maxfd >= setsize)
        return -1;
    if (api_resize(ev, setsize) == -1)
        return -1;

    ev->events = realloc(ev->events, setsize * sizeof(file_event));
    ev->fired = realloc(ev->fired, setsize * sizeof(fired_event));
    ev->setsize = setsize;

    /* because realloc dont initialize additional area */
    for (int i = ev->maxfd + 1; i < setsize; i++) {
        ev->events[i].mask = MINI_NONE;
    }

    return 0;
}

void stop_event_loop(event_loop *ev) {
    ev->stop = 1;
}

/* File event*/

/* Time event started */
static void get_time(long *seconds, long *milliseconds) {
    struct timeval tv;

    gettimeofday(&tv, NULL);

    *seconds = tv.tv_sec;
    *milliseconds = tv.tv_usec / 1000;
}

static void after_now(long long after_ms, long *sec, long *ms) {
    get_time(sec, ms);
    *sec += after_ms/1000;
    *ms  += after_ms%1000;
    if (*ms >= 1000) {
        *sec ++;
        *ms -= 1000;
    }
}

/* Create a time_event and prepend to event loop time list */
long long create_time_event(event_loop *ev, long long ms,
        time_event_cb *callback, void *client_data,
        event_finalizer_cb *finalizer) {
    long long id = ev->time_event_next_id++;
    time_event *te;
    te = (time_event*) malloc(sizeof(time_event));
    if (!te) {
        perror("malloc time_event");
        return -1;
    }
    te->id = id;
    after_now(ms, &te->second, &te->millisecond);

    te->callback = callback;
    te->finalizer = finalizer;
    te->client_data = client_data;
    te->next = ev->te_head;
    ev->te_head = te;

    return id;
}

int delete_time_event(event_loop *ev, long long id) {
    time_event *te, *prev = NULL;
    te = ev->te_head;
    while(te) {
        if (te->id == id) {
            if(prev) {
                prev->next = te->next;
            }
            else {
                ev->te_head = te->next;
            }
            if (te->finalizer) {
                te->finalizer(ev, te->client_data);
            }
            free(te);
            return 0;
        }
        prev = te;
        te = te->next;
    }

    return -1;
}

/* TODO: It's O(N), We need a priority queue or skip list */
static time_event *search_nearest_timer(event_loop *ev) {
    time_event *te = ev->te_head;
    time_event *nearest = NULL;

    while(te) {
        if (!nearest || te->second < nearest->second ||
                (te->second == nearest->second &&
                 te->millisecond < nearest->millisecond)) {
            nearest = te;
        }
        te = te->next;
    }
    return nearest;
}

static int process_time_event(event_loop *ev) {
    int processed = 0;
    time_event *te;
    long long max_id;
    time_t now = time(NULL);

    /* TODO: Why? */
    if (now < ev->last_time) {
        te = ev->te_head;
        while(te) {
            te->second = 0;
            te = te->next;
        }
    }
    ev->last_time = now;

    te = ev->te_head;
    max_id = ev->time_event_next_id - 1;
    while(te) {
        //TODO:
    }
}
