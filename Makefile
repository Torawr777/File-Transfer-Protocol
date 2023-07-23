CPP = gcc
FLAGS = -Wall

EXEC = myftp myftpserve

default:
	gcc -o myftp myftp.c
	gcc -o myftpserve myftpserve.c

clean:
	rm -f ${EXEC}
	rm -f *.o

${EXEC}:${OBJS}
	${CPP} ${FLAGS} -o ${EXEC} ${OBJS}

.c.o:
	${CPP} ${FLAGS} -c $<


