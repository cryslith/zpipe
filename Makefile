.PHONY: all clean

all: zpipe

zpipe: zpipe.c zpipe.h util.c util.h list.c list.h
	c99 -Werror -DKERBEROS zpipe.c util.c list.c -lzephyr -lcom_err -o $@

clean:
	rm -f zpipe
