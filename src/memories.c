#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <string.h>

#include "utils.h"
#include "config.h"

#define ASCII_CR_S "\x0d"
#define ASCII_LF_S "\x0a"

#define MAX_RECV_LEN 512

int main(void) {
  uint8_t *nickname = NULL, *password = NULL, *user = NULL, *host = NULL, *serv = NULL;
  enum error rerr;

  if ((rerr = alloc_bufs(&nickname, &password, &user, &host, &serv)) != ERR_SUCCESS) {
    return rerr;
  }

  if ((rerr = get_details(nickname, password, user, host, serv)) != ERR_SUCCESS) {
    free_bufs(nickname, password, user, host, serv);
    return rerr;
  }

  uint8_t *recbuf;
  if ((recbuf = malloc(MAX_RECV_LEN)) == NULL) {
    free_bufs(nickname, password, user, host, serv);
    return ERR_MALLOC;
  }

  uintmax_t atnamesi = 0;

  _Bool terminate = 0;

  while (!terminate) {
    fprintf(stderr, "memories (info): establishing connection to %s:%s\n", (char *) host, (char *) serv);

    int sd;
    if ((rerr = establish_connection(host, serv, &sd)) != ERR_SUCCESS) {
      free_bufs(nickname, password, user, host, serv);
      return rerr;
    }

    if ((rerr = send_details(nickname, password, user, sd)) != ERR_SUCCESS) {
      close(sd);
      free_bufs(nickname, password, user, host, serv);
      return rerr;
    }

    time_t lrectime;
    if ((lrectime = time(NULL)) == -1) {
      free(recbuf);
      close(sd);
      free_bufs(nickname, password, user, host, serv);
      return ERR_TIME;
    }

    while (1) {
      fd_set rset;
      FD_ZERO(&rset);
      FD_SET(0, &rset);
      FD_SET(sd, &rset);

      struct timeval timeout;
      timeout.tv_sec = TIMEOUT_S;
      timeout.tv_usec = TIMEOUT_US;

      int sr = select(sd + 1, &rset, NULL, NULL, &timeout);

      if (sr == -1) {
	free(recbuf);
	close(sd);
	free_bufs(nickname, password, user, host, serv);
	return ERR_SELECT;
      }
      else if (sr == 0) {
	if (difftime(time(NULL), lrectime) >= RECONNECT_TIME) {
	  fputs("memories (info): reconnecting (timed out)\n", stderr);
	  break;
	}
      }
      else if (FD_ISSET(0, &rset)) {
	if ((lrectime = time(NULL)) == -1) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return ERR_TIME;
	}

	if (fgets((char *) recbuf, MAX_RECV_LEN - 1, stdin) == NULL) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return ERR_FGETS;
	}

	recbuf[strcspn((char *) recbuf, "\n")] = '\0';

	if (strcmp((char *) recbuf, "/help") == 0) {
	  fputs("memories (info): help    shows this help message\n", stderr);
	  fputs("memories (info): license    shows copyright information\n", stderr);
	  fputs("memories (info): reconnect    performs a forced reconnect\n", stderr);
	  fputs("memories (info): quit    gracefully quits memories\n", stderr);
	  fputs("memories (info): unread    shows the number of unread atname instances\n", stderr);
	  fputs("memories (info): read    resets the atname instance count\n", stderr);
	  fputs("memories (info): pause    waits until a message can be read from the input device; the message is then sent to the server\n", stderr);
	}
	else if (strcmp((char *) recbuf, "/license") == 0) {
	  fputs("memories (info): Copyright (c) 2024 Somdipto Chakraborty, licensed under the GNU General Public Licence v3.0\n", stderr);
	  break;
	}
	else if (strcmp((char *) recbuf, "/reconnect") == 0) {
	  fputs("memories (info): reconnecting (forced)\n", stderr);
	  break;
	}
	else if (strcmp((char *) recbuf, "/quit") == 0) {
	  fputs("memories (info): terminating\n", stderr);
	  close(sd);
	  terminate = 1;
	  break;
	}
	else if (strcmp((char *) recbuf, "/unread") == 0) {
	  fprintf(stderr, "memories(info): %ju unread atname instances\n", atnamesi);
	}
	else if (strcmp((char *) recbuf, "/read") == 0) {
	  fprintf(stderr, "memories(info): %ju atname instances read\n", atnamesi);
	  atnamesi = 0;
	}
	else {
	  if (strcmp((char *) recbuf, "/pause") == 0) {
	    fputs("memories (info): paused\n", stderr);

	    if (fgets((char *) recbuf, MAX_RECV_LEN - 1, stdin) == NULL) {
	      free(recbuf);
	      close(sd);
	      free_bufs(nickname, password, user, host, serv);
	      return ERR_FGETS;
	    }

	    fputs("memories (info): unpaused\n", stderr);
	  }

	  for (size_t i = 0; recbuf[i] != '\0'; i++) {
	    if (recbuf[i] == '\n') {
	      recbuf[i] = '\0';
	      break;
	    }
	    else if (recbuf[i] == '\r') {
	      fputs("memories (info): message with carriage return truncated\n", stderr);
	      recbuf[i] = '\0';
	      break;
	    }
	    else if (recbuf[i] == '\0') {
	      fputs("memories (info): message with null truncated\n", stderr);
	      recbuf[i] = '\0';
	      break;
	    }
	  }

	  if (recbuf[0] != '\0') {
	    fprintf(stderr, "memories (preview): %s\n", (char *) recbuf);

	    int d;
	    if ((d = getchar()) == EOF) {
	      free(recbuf);
	      close(sd);
	      free_bufs(nickname, password, user, host, serv);
	      return ERR_GETCHAR;
	    }
	    else if (d == '\n') {
	      if (write(sd, recbuf, strlen((char *) recbuf)) == -1) {
		free(recbuf);
		close(sd);
		free_bufs(nickname, password, user, host, serv);
		return ERR_WRITE;
	      }

	      if (write(sd, ASCII_CR_S ASCII_LF_S, 2) == -1) {
		free(recbuf);
		close(sd);
		free_bufs(nickname, password, user, host, serv);
		return ERR_WRITE;
	      }
	    }
	    else {
	      if (getchar() == EOF) {
		free(recbuf);
		close(sd);
		free_bufs(nickname, password, user, host, serv);
		return ERR_GETCHAR;
	      }
	    }
	  }
	}
      }
      else if (FD_ISSET(sd, &rset)) {
	if ((lrectime = time(NULL)) == -1) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return ERR_TIME;
	}

	ssize_t rsize;
	if ((rsize = read(sd, recbuf, MAX_RECV_LEN)) == -1) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return ERR_READ;
	}

	if (fwrite(recbuf, 1, rsize, stderr) < (unsigned long) rsize) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return ERR_FWRITE;
	}

	if ((rerr = check(recbuf, rsize, nickname, &atnamesi, sd)) != ERR_SUCCESS) {
	  free(recbuf);
	  close(sd);
	  free_bufs(nickname, password, user, host, serv);
	  return rerr;
	}
      }
    }
  }

  free(recbuf);
  free_bufs(nickname, password, user, host, serv);

  return ERR_SUCCESS;
}
