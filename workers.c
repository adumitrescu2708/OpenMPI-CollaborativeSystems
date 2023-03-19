#include "workers.h"
#include "supervisors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

/*
    workers.c
    |    - describes main functionality and implementation details
    |      of supervisors workers

    @author Dumitrescu Alexandra
    @since January 2023
*/

/*

    @param1 - id of global process
    @param2 - id of supervisor rank
    @param3 - vector with number of workers for each supervisor
    @param4 - pointer to the topology

    This method is used by the worker when its corresponding supervisor
    sends him the number of workers and the topology.

*/
void get_topology_from_supervisor(int rank, int *supervisor, int *number_of_workers, int ***topology)
{
    /* Get vector with number of workers and store the parent supervisor */
    MPI_Status status;
    int received_numbers[NUM_SUPERVISORS];
    MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    
    /* Allocate memory for the topology */
    (*supervisor) = status.MPI_SOURCE;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        number_of_workers[i] = received_numbers[i];
        (*topology)[i] = malloc(number_of_workers[i] * sizeof(int));
    }

    /* Find the total number of workers in topology */
    int total = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total += number_of_workers[i];
    }
    int received_topology[total];
    /* Receive vector containing topology */
    MPI_Recv(received_topology, total, MPI_INT, *supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int l = 0;

    /* Copy topology */
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        for(int j = 0; j < number_of_workers[i]; j++) {
            (*topology)[i][j] = received_topology[l];
            l++;
        }
    }

    /* Print topology */
    print_topology_worker(rank, number_of_workers, topology);
}

/*
    This method is used for printing topology 
*/
void print_topology_worker(int rank, int *number_of_workers, int ***topology) {
    printf("%d -> ", rank);

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        printf("%d:", i);

        for(int j = 0; j < number_of_workers[i] - 1; j++) {
            printf("%d,", (*topology)[i][j]);
        }
        printf("%d", (*topology)[i][number_of_workers[i] - 1]);
        printf(" ");
    }
    printf("\n");
}

/*
    This method is used for printing topology in tasks where
    connections are broken
*/
void print_topology_bonus_worker(int rank, int *number_of_workers, int ***topology) {
    printf("%d -> ", rank);

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(number_of_workers[i] != UNKNOWN) {
            printf("%d:", i);

            for(int j = 0; j < number_of_workers[i] - 1; j++) {
                printf("%d,", (*topology)[i][j]);
            }
            printf("%d", (*topology)[i][number_of_workers[i] - 1]);
            printf(" ");            
        }

    }
    printf("\n");
}

/*
    This method is previous to the one from above, changing only the printing logic
*/
void get_topology_from_supervisor_bonus(int rank, int *supervisor, int *number_of_workers, int ***topology)
{
    MPI_Status status;
    int received_numbers[NUM_SUPERVISORS];
    MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    (*supervisor) = status.MPI_SOURCE;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        number_of_workers[i] = received_numbers[i];
        if(number_of_workers[i] != UNKNOWN) {
            (*topology)[i] = malloc(number_of_workers[i] * sizeof(int));
        }     
    }

    int total = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(number_of_workers[i] != UNKNOWN) {
            total += number_of_workers[i];
        }
        
    }
    int received_topology[total];
    MPI_Recv(received_topology, total, MPI_INT, *supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int l = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        for(int j = 0; j < number_of_workers[i]; j++) {
            (*topology)[i][j] = received_topology[l];
            l++;
        }
    }
    print_topology_bonus_worker(rank, number_of_workers, topology);
}

/*
    @param1 - id of global process
    @param2 - if of parent supervisor
    @param3 - number of elements in vector
    @param4 - shared vector
    @param5 - number of workers vector

    This method is used  by the workers when receiving from the supervisor
    its corresponding number of elements in vector, then the entire vector,
    then the start and end index and computes multiplication on corresponding 
    chunk of elements in vector.
*/
void get_vector_from_supervisor(int rank, int supervisor, int *N, int **V, int *number_of_workers)
{
    int received_N;
    int start_idx, end_idx;

    /* Receive number of elements in array, then the array, then the starting and ending index of chunk */
    MPI_Recv(&received_N, 1, MPI_INT, supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    int received_V[received_N];
    MPI_Recv(received_V, received_N, MPI_INT, supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&start_idx, 1, MPI_INT, supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(&end_idx, 1, MPI_INT, supervisor, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    /* Update corresponding chunk */
    for(int i = start_idx; i < end_idx; i++) {
        received_V[i] *= 5;
    }

    /* Send back the updates */
    MPI_Send(received_V, received_N, MPI_INT, supervisor, 0, MPI_COMM_WORLD);
    printf("M(%d,%d)\n", rank, supervisor);
}
