#author: koejima
#create: 2015-08-08
#last update: 2015-12-19

CC=gcc
LFLAG = -ljansson $(pkgconfig --cflags --libs libnl-3.0)
OPTIONS = -W -Wall -O3
NAME_COMMIT = linux_commit
NAME_REVERT = linux_revert
OBJECT_COMMIT = commit_main.c common.c commit_interface.c commit_address.c commit_route.c lib/libutil.a lib/libnetlink.a
OBJECT_REVERT = revert_main.c common.c revert_interface.c revert_address.c revert_route.c lib/libutil.a lib/libnetlink.a

.PHONY: all,clean,re

# compile
all: $(NAME_COMMIT) $(NAME_REVERT)

# clean
clean:
	-rm $(NAME_COMMIT) $(NAME_REVERT)

# recompile
r: clean all

$(NAME_COMMIT):
	$(CC) $(OPTIONS) $(OBJECT_COMMIT) -o $(NAME_COMMIT) $(LFLAG)

$(NAME_REVERT):
	$(CC) $(OPTIONS) $(OBJECT_REVERT) -o $(NAME_REVERT) $(LFLAG)
