/* A mini event loop library, copy from antirez/redis.
 *
 * goby <goby@foxmail.com>
 * 2015-08-02
 *
 */
#include <stdlib.h>
#include <stdio.h>

#include <unistd.h>
#include <sys/epoll.h>
#include "mini_event.h"

/* TODO: Why 1024? */
#define EPOLL_SIZE 1024

#define API_NAME "epoll"

typedef struct _api_state{
    int epfd;
    struct epoll_event *events;
} api_state;

static int api_create(event_loop *ev) {
    api_state *state = (api_state*)malloc(sizeof(api_state));

    if (!state) {
        perror("malloc api_state");
        return -1;
    }

    state->events = (struct epoll_event*)malloc(ev->setsize *
            sizeof(struct epoll_event));
    if (!state->events) {
        free(state);
        perror("malloc epoll_event");
        return -1;
    }

    state->epfd = epoll_create(EPOLL_SIZE);
    if (state->epfd == -1) {
        perror("epoll_create");
        free(state->events);
        free(state);
        return -1;
    }

    ev->apidata = state;

    return 0;
}

static int api_resize(event_loop* ev, int setsize) {
    api_state *state = (api_state*)ev->apidata;
    state->events = realloc(state->events, setsize * sizeof(struct epoll_event));

    if (!state->events) {
        perror("realloc");
        return -1;
    }

    return 0;
}

static int api_free(event_loop *ev) {
    api_state *state = ev->apidata;

    close(state->epfd);
    free(state->events);
    free(state);
}

static int api_add_event(event_loop *ev, int fd, int mask) {
    api_state *state = ev->apidata;
    struct epoll_event ee;

    /* Insert or update
     * Check the file events mask */
    int op = ev->events[fd].mask == MINI_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    ee.events = 0;
    mask |= ev->events[fd].mask;
    /* Change library event to epoll event */
    if (mask & MINI_RD) ee.events |= EPOLLIN;
    if (mask & MINI_WR) ee.events |= EPOLLOUT;

    ee.data.u64 = 0; /* TODO: is valgrind warning here? */
    ee.data.fd = fd; /* TODO: is ev->events[fd] overflowed? */

    if (epoll_ctl(state->epfd, op, fd, &ee) == -1) {
        perror("epoll_ctl");
        return -1;
    }
    return 0;
}

static int api_del_event(event_loop *ev, int fd, int delmask) {
    api_state *state = ev->apidata;
    struct epoll_event ee;
    int mask = ev->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & MINI_RD) ee.events |= EPOLLIN;
    if (mask & MINI_WR) ee.events |= EPOLLOUT;
    ee.data.u64 = 0;
    ee.data.fd = fd;
    if (mask != MINI_NONE) {
        epoll_ctl(state->epfd, EPOLL_CTL_MOD, fd, &ee);
    }
    else {
        // FIXME: Need for NONE null
        epoll_ctl(state->epfd, EPOLL_CTL_DEL, fd, &ee);
    }
}

static int api_poll(event_loop *ev, struct timeval *tvp) {
    api_state *state = ev->apidata;
    int retval, num_events = 0;

    retval = epoll_wait(state->epfd, state->events, ev->setsize,
            tvp ? (tvp->tv_sec * 1000 + tvp->tv_usec/1000) : -1);

    if (retval > 0) {
        int idx = 0;
        num_events = retval;
        for (idx = 0; idx < num_events; idx++) {
            int mask = 0;
            struct epoll_event e = state->events[idx];

            if (e.events & EPOLLIN) mask |= MINI_RD;
            if (e.events & EPOLLOUT) mask |= MINI_WR;
            if (e.events & EPOLLERR) mask |= MINI_WR;
            if (e.events & EPOLLHUP) mask |= MINI_WR;

            ev->fired[idx].fd = e.data.fd;
            ev->fired[idx].mask = mask;
        }
    }

    return num_events;
}

