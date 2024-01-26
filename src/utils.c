#define _POSIX_C_SOURCE 200809L

#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"

#define ASCII_SPACE 0x20
#define ASCII_COLON 0x3b
#define ASCII_CR 0x0d
#define ASCII_LF 0x0a
#define ASCII_CR_S "\x0d"
#define ASCII_LF_S "\x0a"

#define MAX_MSG_LEN 512

void free_bufs(uint8_t *nickname, uint8_t *password, uint8_t *user, uint8_t *host, uint8_t *serv) {
  free(nickname);
  free(password);
  free(user);
  free(host);
  free(serv);
}

enum error alloc_bufs(uint8_t **nickname, uint8_t **password, uint8_t **user, uint8_t **host, uint8_t **serv) {
  *nickname = NULL, *password = NULL, *user = NULL, *host = NULL, *serv = NULL;

  if ((*nickname = malloc(MAX_MSG_LEN - 1)) == NULL) { /* one extra byte for the null character */
    fprintf(stderr, "memories (error): failed to allocate enough memory\n");
    free_bufs(*nickname, *password, *user, *host, *serv);
    return ERR_MALLOC;
  }

  if ((*password = malloc(MAX_MSG_LEN - 1)) == NULL) {
    fprintf(stderr, "memories (error): failed to allocate enough memory\n");
    free_bufs(*nickname, *password, *user, *host, *serv);
    return ERR_MALLOC;
  }

  if ((*user = malloc(MAX_MSG_LEN - 1)) == NULL) {
    fprintf(stderr, "memories (error): failed to allocate enough memory\n");
    free_bufs(*nickname, *password, *user, *host, *serv);
    return ERR_MALLOC;
  }

  if ((*host = malloc(MAX_MSG_LEN + 1)) == NULL) {
    fprintf(stderr, "memories (error): failed to allocate enough memory\n");
    free_bufs(*nickname, *password, *user, *host, *serv);
    return ERR_MALLOC;
  }

  if ((*serv = malloc(MAX_MSG_LEN + 1)) == NULL) {
    fprintf(stderr, "memories (error): failed to allocate enough memory\n");
    free_bufs(*nickname, *password, *user, *host, *serv);
    return ERR_MALLOC;
  }

  return ERR_SUCCESS;
}

static enum error add_detail(uint8_t *d, size_t dsize, char *ddiag) {
  while (1) {
    fputs(ddiag, stderr);

    if (fgets((char *) d, dsize + 1, stdin) == NULL) {
      return ERR_FGETS;
    }

    for (size_t i = 0; d[i] != '\0'; i++) {
      if (d[i] == '\n') {
	d[i] = '\0';
	break;
      }
      else if (d[i] == '\r') {
	fputs("memories (info): message with carriage return truncated\n", stderr);
	d[i] = '\0';
	break;
      }
      else if (d[i] == '\0') {
	fputs("memories (info): message with null truncated\n", stderr);
	d[i] = '\0';
	break;
      }
    }

    if (d[0] != '\0') {
      break;
    }

    fputs("memories (info): detail cannot be empty\n", stderr);
  }

  return ERR_SUCCESS;
}

enum error get_details(uint8_t *nickname, uint8_t *password, uint8_t *user, uint8_t *host, uint8_t *serv) {
  enum error rerr;

  if ((rerr = add_detail(nickname, MAX_MSG_LEN - 2, "nickname\n")) != ERR_SUCCESS) {
    return rerr;
  }

  if ((rerr = add_detail(password, MAX_MSG_LEN - 2, "password\n")) != ERR_SUCCESS) {
    return rerr;
  }

  if ((rerr = add_detail(user, MAX_MSG_LEN - 2, "user\n")) != ERR_SUCCESS) {
    return rerr;
  }

  if ((rerr = add_detail(host, MAX_MSG_LEN, "host\n")) != ERR_SUCCESS) {
    return rerr;
  }

  if ((rerr = add_detail(serv, MAX_MSG_LEN, "service\n")) != ERR_SUCCESS) {
    return rerr;
  }

  return ERR_SUCCESS;
}

enum error establish_connection(uint8_t *host, uint8_t *serv, int *sd) {
  *sd = -1;
  struct addrinfo hints, *result, *rp;

  memset(&hints, 0, sizeof(hints));

  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = 0;
  hints.ai_protocol = 0;

  if (getaddrinfo((char *) host, (char *) serv, &hints, &result) != 0) {
    close(*sd);
    return ERR_GETADDRINFO;
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    if ((*sd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
      continue;
    }
    if (connect(*sd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
  }

  freeaddrinfo(result);

  if (rp == NULL) {
    close(*sd);
    return ERR_GETADDRINFO;
  }

  return ERR_SUCCESS;
}

enum error send_details(uint8_t *nickname, uint8_t *password, uint8_t *user, int sd) {
  if (write(sd, "PASS ", 5) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, password, strlen((char *) password)) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, ASCII_CR_S ASCII_LF_S, 2) == -1) {
    return ERR_WRITE;
  }

  if (write(sd, "NICK ", 5) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, nickname, strlen((char *) nickname)) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, ASCII_CR_S ASCII_LF_S, 2) == -1) {
    return ERR_WRITE;
  }

  if (write(sd, "USER ", 5) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, user, strlen((char *) user)) == -1) {
    return ERR_WRITE;
  }
  if (write(sd, ASCII_CR_S ASCII_LF_S, 2) == -1) {
    return ERR_WRITE;
  }

  return ERR_SUCCESS;
}

enum error check(uint8_t *recbuf, ssize_t rsize, uint8_t *nickname, uintmax_t *atnamesi, int sd) {
  enum ping_state {
    PS_SSN,
    PS_SSR,
    PS_P_OR_COLON,
    PS_PREFIX,
    PS_ATLEASTONESPACE,
    PS_OPTSPACESEQUENCE,
    PS_I,
    PS_N,
    PS_G,
    PS_SPACE,
    PS_ESR,
    PS_ESN
  };

  static enum ping_state ps = PS_P_OR_COLON;
  static ssize_t as = -1;

  for (ssize_t i = 0; i != rsize; i++) {
    if (as == -1) {
      if (recbuf[i] == '@') {
	as = 0;
      }
    }
    else{
      if (recbuf[i] != nickname[as]) {
	as = -1;
      }
      else {
	as++;
	if (nickname[as] == '\0') {
	  fputc('\a', stderr);
	  as = -1;
	  (*atnamesi)++;
	}
      }
    }
    switch(ps) {
    case PS_SSR:
      if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      break;

    case PS_SSN:
      if (recbuf[i] == ASCII_LF) {
	ps = PS_P_OR_COLON;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_P_OR_COLON:
      if (recbuf[i] == 'P'){
	ps = PS_I;
      }
      else if(recbuf[i] == ASCII_COLON) {
	ps = PS_PREFIX;
      }
      else if(recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_PREFIX:
      if (recbuf[i] == ASCII_SPACE) {
	ps = PS_ATLEASTONESPACE;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_ATLEASTONESPACE:
      if (recbuf[i] == ASCII_SPACE) {
	ps = PS_OPTSPACESEQUENCE;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_OPTSPACESEQUENCE:
      if (recbuf[i] == ASCII_SPACE) {
	ps = PS_OPTSPACESEQUENCE;
      }
      else if (recbuf[i] == 'P') {
	ps = PS_I;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_I:
      if (recbuf[i] == 'I') {
	ps = PS_N;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_N:
      if (recbuf[i] == 'N') {
	ps = PS_G;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_G:
      if (recbuf[i] == 'G') {
	ps = PS_SPACE;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_SPACE:
      if (recbuf[i] == ASCII_SPACE) {
	if (write(sd, "PONG ", 5) == -1) {
	  return ERR_WRITE;
	}

	ps = PS_ESR;
      }
      else if (recbuf[i] == ASCII_CR) {
	ps = PS_SSN;
      }
      else {
	ps = PS_SSR;
      }
      break;

    case PS_ESR:
      if (write(sd, &recbuf[i], 1) == -1) {
	return ERR_WRITE;
      }

      if (recbuf[i] == ASCII_CR) {
	ps = PS_ESN;
      }
      break;

    case PS_ESN:
      if (write(sd, &recbuf[i], 1) == -1) {
	return ERR_WRITE;
      }
      else if (recbuf[i] == ASCII_LF) {
	ps = PS_P_OR_COLON;
      }
      else {
	ps = PS_ESR;
      }
      break;
    }
  }

  return ERR_SUCCESS;
}
