.PHONY: all clean

all: zpipe

zpipe: zpipe.c
	c99 -DKERBEROS $< -lzephyr -lcom_err -o $@
