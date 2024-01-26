#ifndef UTILS_H
#define UTILS_H

#include "errors.h"

extern void free_bufs(uint8_t *nickname, uint8_t *password, uint8_t *user, uint8_t *host, uint8_t *port);
extern enum error alloc_bufs(uint8_t **nickname, uint8_t **password, uint8_t **user, uint8_t **host, uint8_t **port);
extern enum error get_details(uint8_t *nickname, uint8_t *password, uint8_t *user, uint8_t *host, uint8_t *port);
extern enum error establish_connection(uint8_t *host, uint8_t *port, int *sd);
extern enum error send_details(uint8_t *nickname, uint8_t *password, uint8_t *user, int sd);
extern enum error check(uint8_t *recbuf, ssize_t rsize, uint8_t *nickname, uintmax_t *atnamesi, int sd);

#endif
