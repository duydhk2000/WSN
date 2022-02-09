#include "main.h"

int sensor_nodes(MPI_Comm world_comm, MPI_Comm comm, int nrows, int ncols, float threshold) {
	int i, count, exit = 0;
    float rand_water_height;
	int ndims = 2, size, my_rank, reorder, my_cart_rank, ierr, world_size;
	int nbr_i_lo, nbr_i_hi;
	int nbr_j_lo, nbr_j_hi;
	int dims[ndims], coord[ndims];
	int wrap_around[ndims];
	double time_taken;
    float arrNumbers[10] = {};
    int pos = 0;
    float new_avrg = 0;
    float sum = 0;
    int len = sizeof(arrNumbers)/sizeof(float);
	
	MPI_Comm comm2D;
	
	MPI_Comm_size(world_comm, &world_size);		/* Size of world communicator. */
  	MPI_Comm_size(comm, &size);					/* Size of sensor node communicator. */
	MPI_Comm_rank(comm, &my_rank);				/* Rank of sensor node communicator. */
	
	dims[0] = nrows;							/* Number of rows. */
	dims[1] = ncols;							/* Number of columns. */
	
	/* Create cartesian topology for processes. */
	MPI_Dims_create(size, ndims, dims);
	if (my_rank == 0)
		printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n", my_rank, size, dims[0], dims[1]);
	
	/* Create cartesian mapping. */
	wrap_around[0] = 0;
	wrap_around[1] = 0;							/* Periodic shift is false. */
	reorder = 1;
	ierr = 0;
	ierr = MPI_Cart_create(comm, ndims, dims, wrap_around, reorder, &comm2D);
	if (ierr != 0)
		printf("ERROR[%d] creating CART\n", ierr);
	
	/* Find my coordinates in the cartesian communicator group. */
	MPI_Cart_coords(comm2D, my_rank, ndims, coord);
	
	/* Use my cartesian coordinates to find my rank in cartesian group. */
	MPI_Cart_rank(comm2D, coord, &my_cart_rank);
	
	/* Get my neighbors; axis is coordinate dimension of shift.
	 * axis = 0 ==> shift along the rows: P[my_row-1] : P[me] : P[my_row+1]
	 * axis = 1 ==> shift along the columns: P[my_col-1] : P[me] : P[my_col+1] */
	MPI_Cart_shift(comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi);
	MPI_Cart_shift(comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi);
	
	/* Initialize request and status arrays. */
	MPI_Request send_request[NEIGHBOURS];
	MPI_Request receive_request[NEIGHBOURS];
	// MPI_Status send_status[NEIGHBOURS];
	// MPI_Status receive_status[NEIGHBOURS];
	MPI_Status status;
	
	/* Seed random value. */
	srand((unsigned) time(NULL) + my_rank);
	
	while (1) {
		/* Check if base station signals to terminate. */
		MPI_Recv(&exit, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &status);
		
		/* Send received signal to the next process. */
		if (my_rank != size - 1)
			MPI_Send(&exit, 1, MPI_INT, my_rank + 1, 0, world_comm);
		if (exit)
			break;
		
		/* Generate a random height. */
		rand_water_height = (((float)rand()/(float)(RAND_MAX)) * (MAX_HEIGHT - MIN_HEIGHT + 1)) + MIN_HEIGHT;

        /* Compute moving average */
        sum = sum - arrNumbers[pos] + rand_water_height;
        //Assign the nextNum to the position in the array
        arrNumbers[pos] = rand_water_height;
        //return the average
        new_avrg = sum / len;
        pos++;

        if (pos >= len){
            pos = 0;
        }

		/* Local process asynchronously send its height to its neighbours and likewise receive its neighbours' heights. */
		int neighbours[] = {nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi};
		float recv_height[] = {-1, -1, -1, -1};
		for (i = 0; i < NEIGHBOURS; i++) {
			if (neighbours[i] != -2) {
				MPI_Isend(&new_avrg, 1, MPI_FLOAT, neighbours[i], 0, comm2D, &send_request[i]);
				MPI_Irecv(&recv_height[i], 1, MPI_FLOAT, neighbours[i], 0, comm2D, &receive_request[i]);
			}
		}
		
		// MPI_Waitall(NEIGHBOURS, send_request, send_status);
		// MPI_Waitall(NEIGHBOURS, receive_request, receive_status);
	

		if (new_avrg >= threshold) {
			/* Local process compares the received heights with its own height. */
			count = 0;
			for (i = 0; i < NEIGHBOURS; i++) {
				if (abs(new_avrg - recv_height[i]) <= TOLERANCE) {
					count++;
                }
			}
			
			/* If at least 2 neighbours have approximately same heights as the local process, alert the base station. */
			if (count >= 2) {
			/* Print results of alert. */
				printf("[ALERT] Global rank: %d. Cart rank: %d. Coord: (%d, %d). New moving average: %f. Recv Top: %f. Recv Bottom: %f. Recv Left: %f. Recv Right: %f.\n", 
						my_rank, my_cart_rank, coord[0], coord[1], new_avrg, recv_height[0], recv_height[1], recv_height[2], recv_height[3]);
				
				time_taken = MPI_Wtime();
				double report[] = {(double)my_rank, (double)nbr_i_lo, (double)nbr_i_hi, (double)nbr_j_lo, (double)nbr_j_hi, (float)new_avrg, 
									(double)recv_height[0], (double)recv_height[1], (double)recv_height[2], (double)recv_height[3], time_taken};
				
				/* Send alert report to base station. */
				MPI_Send(report, 11, MPI_DOUBLE, world_size - 1, 0, world_comm);
			}
		}
	}
	
	/* Clean up. */
	MPI_Comm_free(&comm2D);
	return 0;
}
