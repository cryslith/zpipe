#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <error.h>

#include "util.h"

#define GETDELIM_BUFFER_SIZE 120


int max(int a, int b) {
  return (a > b) ? a : b;
}

size_t getdelim_unbuf(char **lineptr, size_t *n, int delim, int fd) {
  if (lineptr == NULL || n == NULL) {
    ERROR("lineptr and n must not be null!");
  }

  if (*lineptr == NULL || *n == 0) {
    if ((*lineptr = malloc(*n = GETDELIM_BUFFER_SIZE)) == NULL) {
      ERROR("malloc returned null");
    }
  }
  size_t head = 0;

  for (;;) {
    if (head + 1 >= *n) {
      if ((*lineptr = realloc(*lineptr, (*n = *n * 2 + 1))) == NULL) {
        ERROR("realloc returned null");
      }
    }

    int r;
    if ((r = read(fd, *lineptr + head, 1)) < 0) {
      ERROR("read error");
    }
    if (r == 0 || (*lineptr)[head++] == delim) {
      (*lineptr)[head++] = '\0';
      return head;
    }
  }
}

ssize_t read_all(int fd, void *buf, size_t count) {
  size_t n = 0;
  ssize_t r;
  for (int n = 0; n < count; n += r) {
    if ((r = read(fd, buf + n, count - n)) < 0) {
      return -1;
    }
    if (r == 0) {
      return n;
    }
  }
  return count;
}
