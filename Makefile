################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################
CFLAGS = -Wall -Werror -lssl
DFLAGS = -g

default: lisod echo_client

lisod: liso_server.c readline.c hash.c strloc.c daemonize.c
	gcc liso_server.c readline.c hash.c strloc.c daemonize.c -o $@ ${CFLAGS} ${DFLAGS}

echo_client: echo_client.c
	gcc echo_client.c -o $@ ${CFLAGS}

liso_debug: liso_server.c readline.c
	gcc liso_server.c readline.c hash.c strloc.c daemonize.c -DDEBUG -o $@ ${CFLAGS} ${DFLAGS}


handin:
	tar cvf ../handin.tar ../15-441-project-1
clean:
	rm -f lisod echo_client liso_debug

checkpoint2:
	git tag -d checkpoint-2
	git tag -a checkpoint-2 HEAD
