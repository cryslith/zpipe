.PHONY: all clean

all: zpipe

zpipe: zpipe.c
	gcc $< -lzephyr -lcom_err -o $@
