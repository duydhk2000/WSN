#include "main.h"

int main(int argc, char *argv[]) {
	int my_rank, size;
	int nrows, ncols, iter;
	float threshold;
	MPI_Comm new_comm;
	
	/* Start up initial MPI environment. */
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	
	/* Process command line arguments. */
	if (argc == 5) {
		nrows = atoi(argv[1]);
		ncols = atoi(argv[2]);
		iter = atoi(argv[3]);
		threshold = atoi(argv[4]);
		
		if ((nrows * ncols) != size - 1) {
			if (my_rank == 0)
				printf("ERROR: nrows * ncols = %d * %d = %d != %d\n", nrows, ncols, nrows * ncols, size - 1);
			MPI_Finalize(); 
			return 0;
		}
	}
	else {
		/* Assign default values for grid row and column and base station iteration value. */
		nrows = (int)sqrt(size-1);
		ncols = (int)((size-1)/nrows);
		iter = 100;
		threshold = 6000;
	}
	
	/* Split communicator group into base station and sensor nodes. */
	MPI_Comm_split(MPI_COMM_WORLD, my_rank == size - 1, 0, &new_comm);
	
	if (my_rank == size - 1)
		base_station(MPI_COMM_WORLD, new_comm, iter, nrows, ncols);
	else
		sensor_nodes(MPI_COMM_WORLD, new_comm, nrows, ncols, threshold);
	
	/* Clean up. */
	printf("Rank %d finished\n", my_rank);
	MPI_Finalize();
	return 0;
}
