ALL:
	mpicc -Wall -o main main.c sensors.c base_station.c -lm

run:
	mpirun -np 13 main 3 4 100 6000

clean:
	/bin/rm -f main *.o
