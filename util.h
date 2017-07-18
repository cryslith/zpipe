#define ERROR(...) error_at_line(1, 0, __FILE__, __LINE__, __VA_ARGS__)

int max(int, int);
size_t getdelim_unbuf(char **lineptr, size_t *n, int delim, int fd);
ssize_t read_all(int fd, void *buf, size_t count);
