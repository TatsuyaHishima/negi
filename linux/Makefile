#author: koejima
#create: 2015-08-08
#last update: 2015-12-19

CC=gcc
LFLAG = -ljansson $(pkgconfig --cflags --libs libnl-3.0)
OPTIONS = -W -Wall -O3
NAME = negi_linux
OBJECT = main.c common.c interface.c address.c route.c lib/libutil.a lib/libnetlink.a

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
