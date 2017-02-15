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

lisod: liso_server.c readline.c
	gcc liso_server.c readline.c -o $@ ${CFLAGS}

echo_client: echo_client.c
	gcc echo_client.c -o $@ ${CFLAGS}

debug: CFlAGS += -g
debug: lisod echo_client
	echo ${CFLAGS}

handin:
	tar cvf ../handin.tar ../15-441-project-1
clean:
	rm lisod echo_client
