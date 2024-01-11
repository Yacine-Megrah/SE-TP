CC=gcc
OBJETCS= main.o
BIN= prog.exe

all: run

build: ${OBJETCS}
	${CC} -o ${BIN} ${OBJETCS}
.c.o:
	${CC} -c $<
clean:
	rm -rf *.o
run: build
	./${BIN}