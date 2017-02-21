################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################
CFLAGS = -Wall -Werror
DFLAGS = -g

default: lisod echo_client

lisod: liso_server.c readline.c hash.c strloc.c
	gcc liso_server.c readline.c hash.c strloc.c -o $@ ${CFLAGS}

echo_client: echo_client.c
	gcc echo_client.c -o $@ ${CFLAGS}

liso_debug: liso_server.c
	gcc liso_server.c readline.c hash.c strloc.c -o $@ ${CFLAGS} ${DFLAGS}


handin:
	tar cvf ../handin.tar ../15-441-project-1
clean:
	@rm lisod echo_client liso_debug

checkpoint2:
	git tag -d checkpoint-2
	git tag -a checkpoint-2 HEAD
