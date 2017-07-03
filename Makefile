.PHONY: all clean

all: zpipe

zpipe: zpipe.c util.c
	c99 -Werror -DKERBEROS $^ -lzephyr -lcom_err -o $@
