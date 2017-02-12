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

default: liso echo_client

liso: liso_server.c
	gcc liso_server.c readline.c -o lisod ${CFLAGS}

echo_client: echo_client.c
	gcc echo_client.c -o $@ ${CFLAGS}

debug: CFlAGS += -g
debug: CFLAGS += -g
debug: liso echo_client

handin:
	tar cvf ../handin.tar ../15-441-project-1
clean:
	rm echo_server echo_client
