SRC	= polygon0.c cclip.c polygon1.c polyio.c vectmatr.c
OBJ	= $(SRC:%.c=%.o)
CFLAGS	= -Wall -ansi -pedantic -g -std=c99
CC	= gcc

prog:	$(OBJ)
	$(CC) $(CFLAGS) -o cclip $(OBJ) -lm
