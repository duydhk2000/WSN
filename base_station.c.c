#include "main.h"

float *height_readings = NULL;

void *generate_height(void *pArg) {
	int num_nodes = *((int*) pArg);
	int i;

	/* Generate a random Height. */
	for (i = 0; i < num_nodes; i++)
		height_readings[i] = (((float)rand()/(float)(RAND_MAX)) * (MAX_HEIGHT - MIN_HEIGHT + 1)) + MIN_HEIGHT;
	
	return NULL;

}

int base_station(MPI_Comm world_comm, MPI_Comm comm, int iter, int nrows, int ncols) {
	FILE *pWriteFile;
	pthread_t tid;
	int i, size, num_nodes;
	char satelliteTime[128], timeInDateTime[128], alertTime[128];
	int count = 1, exit = 0, adjacent_matches = 0;
	int flag;
	int report_size = 11;
	double report[report_size];
	double report_d[report_size];
	int total_msg = 0, total_true_alerts = 0, total_false_alerts = 0, total_msg_between = 0;
	double commTime = 0;
	struct timespec start, end;
	double time_taken = 0;
	
	clock_gettime(CLOCK_MONOTONIC, &start);
	MPI_Status status;
	MPI_Comm_size(world_comm, &size);
	num_nodes = size - 1;
	
	/* Allocate memory to height_readings. */
	height_readings = (float*) malloc((num_nodes) * sizeof(float));
	
	pWriteFile = fopen("log.txt", "w");
	
	while (iter > 0) {
		/* Signal sensor nodes to continue running. */
		MPI_Send(&exit, 1, MPI_INT, 0, 0, world_comm);
		
		/* Fork	a thread to generate Altimeter satellite readings, then join. */
		pthread_create(&tid, 0, generate_height, &num_nodes);
		pthread_join(tid, NULL);

        convertToTimeStamp(satelliteTime, 128);
		
		/* Probe if there is any incoming message. */
		MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &flag, &status);
		
		total_msg_between = 0;
		adjacent_matches = 0;
		
		if (flag) {
		    convertToTimeStamp(alertTime, 128);
			MPI_Recv(report_d, 11, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &status);
			commTime = MPI_Wtime() - report_d[10];
			
			total_msg_between++;
			
			for(i = 0; i < report_size; i++){
			    report[i] = report_d[i];
			}
			total_msg++;

			convertToTimeStamp(timeInDateTime, 128);
			fprintf(pWriteFile, "-------------------------------------------------------\n");
			fprintf(pWriteFile, "Iteration: %d\n", count);
			fprintf(pWriteFile, "Logged Time: \t\t\t\t %s\n", timeInDateTime);
			fprintf(pWriteFile, "Alert Reported Time: \t\t %s\n", alertTime);
			
			/* Compare if alerted node's height matches altimeter satellite value.
			 * True alert if alerted node is within the threshold after comparing with the Altimeter satellite. 
			 * Otherwise it is a false alert. */
			if (abs(report[5] - height_readings[(int)report[0]]) <= TOLERANCE){
				fprintf(pWriteFile, "Alert Type: %s\n\n", "True");
				total_true_alerts++;
			}	
			else{
				fprintf(pWriteFile, "Alert Type: %s\n\n", "False");
			    total_false_alerts++;
			}
			fprintf(pWriteFile, "Reporting Node \t Coordinates \t Height\n");
			fprintf(pWriteFile, "%d \t\t\t\t (%d, %d) \t\t %f\n\n", (int)report[0], (int)report[0] / ncols, (int)report[0] % ncols, report[5]);
			
			fprintf(pWriteFile, "Adjacent Nodes \t Coordinates \t Height\n");
			for (i = 0; i < NEIGHBOURS; i++) {
				if (report[i + 1] != -2){
					fprintf(pWriteFile, "%d \t\t\t\t (%d, %d) \t\t %f\n", (int)report[i + 1], (int)report[i + 1] / ncols, (int)report[i + 1] % ncols, report[i + 1 + 5]);
					adjacent_matches++;
			    }
			}
															
			fprintf(pWriteFile, "\nAltimeter Satellite Reporting Time: %s\n", satelliteTime);															
			fprintf(pWriteFile, "Altimeter Satellite Reporting (meters): %f\n", height_readings[(int)report[0]]);
			fprintf(pWriteFile, "Altimeter Satellite Reporting Coordinates: (%d, %d)\n\n", (int)report[0] / ncols, (int)report[0] % ncols);
			
			fprintf(pWriteFile, "Communication Time (seconds): %f\n", commTime);
			fprintf(pWriteFile, "Total Messages send between reporting node and base station: %d\n", total_msg_between);
			fprintf(pWriteFile, "Number of adjacent matches to reporting node: %d\n", adjacent_matches);
			fprintf(pWriteFile, "-------------------------------------------------------\n");

		}
		/* Terminate the program. */
		if (0)
			break;

		iter--;
		count++;
	}
	
	/* Base station signals sensor nodes to terminate. */
	exit = 1;
	MPI_Send(&exit, 1, MPI_INT, 0, 0, world_comm);
	
	fprintf(pWriteFile, "-------------------------------------------------------\n");
	fprintf(pWriteFile, "Summary\n\n");
	fprintf(pWriteFile, "Total number of messages received: %d\n", total_msg);
	fprintf(pWriteFile, "Total number of true alerts: %d\n", total_true_alerts);
	fprintf(pWriteFile, "Total number of false alerts: %d\n", total_false_alerts);
	
	clock_gettime(CLOCK_MONOTONIC, &end);
	time_taken = (end.tv_sec - start.tv_sec) * 1e9;
	time_taken = (time_taken + (end.tv_nsec - start.tv_nsec)) * 1e-9;
	fprintf(pWriteFile, "Total communication time: %f\n", time_taken);
	fprintf(pWriteFile, "-------------------------------------------------------\n");

	/* Deallocate memory. */
	free(height_readings);
	fclose(pWriteFile);
	return 0;
}

void convertToTimeStamp(char *buf, int size){
    struct tm ts;
    time_t now;
    time(&now);
    ts = *localtime(&now);
    strftime(buf, size, "%a %Y-%m-%d %H:%M:%S", &ts);
}
