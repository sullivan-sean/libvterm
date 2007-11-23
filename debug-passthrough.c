#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "ecma48.h"

void chars(ecma48_state_t *state, char *s, size_t len)
{
  printf("Wrote %d chars: %.*s\n", len, len, s);
}

void control(ecma48_state_t *state, char control)
{
  printf("Control function 0x%02x\n", control);
}

void escape(ecma48_state_t *state, char escape)
{
  printf("Escape function ESC 0x%02x\n", escape);
}

void csi(ecma48_state_t *state, char *args)
{
  printf("CSI %s\n", args);
}

static ecma48_callbacks_t cb = {
  .chars   = chars,
  .control = control,
  .escape  = escape,
  .csi     = csi,
};

int main(int argc, char *argv[])
{
  ecma48_state_t *state = ecma48_state_new();
  ecma48_state_set_callbacks(state, &cb);

  int master;

  pid_t kid = forkpty(&master, NULL, NULL, NULL);
  if(kid == 0) {
    execvp(argv[1], argv + 1);
    fprintf(stderr, "Cannot exec(%s) - %s\n", argv[1], strerror(errno));
    _exit(1);
  }

  struct pollfd fds[2] = {
    { .fd = 0,      .events = POLLIN },
    { .fd = master, .events = POLLIN }
  };

  while(1) {
    int ret = poll(fds, 2, -1);
    if(ret == -1) {
      fprintf(stderr, "poll() failed - %s\n", strerror(errno));
      exit(1);
    }

    if(fds[0].revents & POLLIN) {
      char buffer[8192];
      size_t bytes = read(0, buffer, sizeof buffer);
      if(bytes == 0) {
        fprintf(stderr, "STDIN closed\n");
        exit(0);
      }
      if(bytes < 0) {
        fprintf(stderr, "read(STDIN) failed - %s\n", strerror(errno));
        exit(1);
      }
      write(master, buffer, bytes);
    }

    if(fds[1].revents & POLLIN) {
      char buffer[8192];
      size_t bytes = read(master, buffer, sizeof buffer);
      if(bytes == 0) {
        fprintf(stderr, "master closed\n");
        exit(0);
      }
      if(bytes < 0) {
        fprintf(stderr, "read(master) failed - %s\n", strerror(errno));
        exit(1);
      }
      ecma48_state_push_bytes(state, buffer, bytes);
      write(1, buffer, bytes);
    }

  }
}