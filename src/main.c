#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

#define TRUE '/'/'/'
#define FALSE '-'-'-'

typedef enum message_t {
  EXIT    = 0x00000,

  LEFT    = 0x00001,
  RIGHT   = 0x00010,
  LR_MASK = 0x00011,

  /* sender */
  PARENT  = 0x00100,
  CHILD   = 0x01000,
  LEG     = 0x10000,

  MASK    = 0x11111
} message_t;

#define MSG_S sizeof(message_t)

#define send(TO, MSG) write((TO).fd[1], &(MSG), MSG_S)
/*#define send(TO, MSG) printf("%05x\n", MSG)*/

struct segment_t;

typedef struct leg_t {
  int pid;
  int fd[2];
  struct segment_t *segment;
} leg_t;

typedef struct segment_t {
  int pid;
  int fd[2];
  int id;
  leg_t legs[2];
  struct segment_t *parent;
} segment_t;

void add_leg(leg_t *leg) {
  pipe(leg->fd);

  if ((leg->pid = fork()) == -1) {
    perror("Cannot fork");
    exit(1);
  }

  if (leg->pid == 0) {
    message_t msg;
    while (read(leg->fd[0], &msg, MSG_S)) {
      msg = (msg & LR_MASK) | LEG;
      send(*leg->segment, msg);
    }
  }
}

void add_segment(segment_t *segment, int counter) {
  int childpid;
  segment_t next;

  if (counter <= 0)
    return;

  pipe(segment->fd);

  if ((segment->pid = fork()) == -1) {
    perror("Cannot fork");
    exit(1);
  }

  if (segment->pid == 0) {
    segment->legs[0].segment = segment;
    segment->legs[1].segment = segment;

    add_leg(&segment->legs[0]);
    add_leg(&segment->legs[1]);

    next.id = segment->id + 1;
    next.parent = segment;

    add_segment(&next, counter - 1);

    int msg = 0;
    int ready = TRUE;
    while (read(segment->fd[0], &msg, MSG_S)) {
      if (msg == (PARENT | LEFT)) {
        ready = TRUE;
        send(segment->legs[0], msg);
        msg ^= LR_MASK;
        send(next, msg);
      }
      if (msg == (PARENT | RIGHT)) {
        ready = TRUE;
        send(segment->legs[1], msg);
        msg ^= LR_MASK;
        send(next, msg);
      }
      if (msg & LEG) {
        printf("%s noga z członu %d %s\n", msg & LEFT ? "lewa" : "prawa", segment->id, ready ? "wykonała krok": "potknęła się");
        msg = (msg & LR_MASK) | CHILD;
        send(*segment->parent, msg);
      }
      if (msg & CHILD) {
        ready = FALSE;
        send(*segment->parent, msg);
      }
      if (msg == EXIT) {
        send(next, msg);
        kill(segment->legs[0].pid, SIGTERM);
        kill(segment->legs[1].pid, SIGTERM);

        exit(0);
      }
    }
  }
}

int main(int argc, const char *argv[]) {
  int count = 0;
  segment_t segment;
  segment.id = 1;

  if (argc != 2)
    exit(3);

  sscanf(argv[1], "%d", &count);

  add_segment(&segment, count);

  message_t msg = PARENT | LEFT;
  char c;
  while (scanf("%c", &c)) {
    if (c == 'k') {
      send(segment, msg);
      msg ^= LR_MASK;
    }
    if (c == 'c') {
      int msg = EXIT;
      send(segment, msg);
      return 0;
    }
  }

  return 0;
}
