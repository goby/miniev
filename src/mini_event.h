/* A mini event loop library, copy from antirez/redis.
 *
 * goby <goby@foxmail.com>
 * 2015-08-02
 *
 */

#ifndef __MINI_EVENT_H__
#define __MINI_EVENT_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Some macro */
#define MINI_OK     0
#define MINI_ERROR  -1

#define MINI_NONE   0x0
#define MINI_RD     0x1
#define MINI_WR     0x2

#define MINI_FILE_EVENTS    0x1
#define MINI_TIME_EVENTS    0x2
#define MINI_ALL_EVENTS     (MINI_FILE_EVENTS | MINI_TIME_EVENTS)
#define MINI_DONT_WAIT      0x4

#define MINI_NOMORE -1

/* Avoid warning */
#define MINI_NOTUSED(V) ((void) V)

#include <time.h>

/* The main struct in the loop */
struct event_loop;

/* function prototype for fd processing
 * @param ev the target event loop
 * @param fd the file description
 * @param data TODO:
 * @param mask Readable or writable
 */
typedef void file_event_cb(struct event_loop *ev, int fd,
        void *data, int mask);

/* function prototype for time event processing */
typedef int time_event_cb(struct event_loop *ev, long long id,
        void *data);

/* function prototype for event clearing */
typedef void event_finalizer_cb(struct event_loop *ev, void *data);

/* function prototype for doing something before sleep */
typedef void before_sleep_cb(struct event_loop *ev);

typedef struct _file_event {
    int mask;               /* readable or writable */
    file_event_cb *read;    /* read processing function */
    file_event_cb *write;   /* write processing function */
    void *client_data;      /* attaching data */
} file_event;

typedef struct _time_event {
    long long id;           /* time event identifier */
    long second;            /* second time stamp */
    long millisecond;       /* when millisecond */
    time_event_cb *callback;     /* processing function */
    event_finalizer_cb *finalizer;
    void *client_data;
    struct _time_event *next;
} time_event;

/* TODO: Do what? */
typedef struct _fired_event {
    int fd;
    int mask;
} fired_event;

typedef struct event_loop {
    int         maxfd;
    int         setsize;
    long long   time_event_next_id;
    time_t      last_time;
    file_event  *events;
    fired_event *fired;
    time_event  *te_head;
    int         stop;
    void        *apidata;
    before_sleep_cb *before_sleep;
} event_loop;

/* API */
int create_event_loop(event_loop *ev, int setsize);
void delete_event_loop(event_loop *ev);

void stop_event_loop(event_loop *ev);

int  create_file_event(event_loop *ev, int fd, int mask,
        file_event_cb *callback, void *client_data);
void delete_file_event(event_loop *ev, int fd, int mask);
int  get_file_event(event_loop *ev, int fd);

long long /* tid */ create_time_event(event_loop *ev,
        long long milliseconds, time_event_cb *callback,
        void *client_data, event_finalizer_cb *finalizer);
int delete_time_event(event_loop *ev, long long id);

int process_event(event_loop *ev, int flags);
int mini_wait(int fd, int mask, long long milliseconds);
int mini_main(event_loop *ev);

char *get_api_name(void);

void set_before_sleep(event_loop *ev, before_sleep_cb *callback);

int get_setsize(event_loop *ev);
int set_setsize(event_loop *ev, int setsize);

#ifdef __cplusplus
}
#endif

#endif
