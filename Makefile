#author: pkj
#create: 2015-08-08
#last update: 2015-08-20

CC=gcc
LFLAG = -ljansson $(pkgconfig --cflags --libs libnl-3.0)
OPTIONS = -W -Wall -O3
NAME = negi
OBJECT = main.c interface.c

.PHONY: all,clean,re

# compile
all: $(NAME)

# clean
clean:
	-rm $(NAME)

# recompile
r: clean all

$(NAME):
	$(CC) $(OPTIONS) $(OBJECT) -o $(NAME) $(LFLAG)