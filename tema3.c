#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include "supervisors.h"
#include "workers.h"


/*
    @param1 - rank of global process
    @param2 - decide next neigh based on diagram in task
    @param3 - decide previous neigh based on diagram

    This method is used when SUPERVISOR_1 is isolated.
*/
void decide_neighs_bonus(int rank, int *next_neigh, int *prev_neigh)
{
    if(rank == SUPERVISOR_0) {
        *next_neigh = SUPERVISOR_3;
        *prev_neigh = NO_NEIGH;
    } else if(rank == SUPERVISOR_3) {
        *next_neigh = SUPERVISOR_2;
        *prev_neigh = SUPERVISOR_0;
    } else if(rank == SUPERVISOR_2) {
        *next_neigh = NO_NEIGH;
        *prev_neigh = SUPERVISOR_3;
    } else if(rank == SUPERVISOR_1) {
        *next_neigh = NO_NEIGH;
        *prev_neigh = NO_NEIGH;
    }
}

/*
    @param1 - rank of global process
    @param2 - decide next neigh based on diagram in task
    @param3 - decide previous neigh based on diagram

    This method is used when connection between SUPERVISOR_0 and SUPERVISOR_1 is broken.
*/
void decide_neighs(int rank, int *next_neigh, int *prev_neigh)
{
    if(rank == SUPERVISOR_0) {
        *next_neigh = SUPERVISOR_3;
        *prev_neigh = NO_NEIGH;
    } else if(rank == SUPERVISOR_3) {
        *next_neigh = SUPERVISOR_2;
        *prev_neigh = SUPERVISOR_0;
    } else if(rank == SUPERVISOR_2) {
        *next_neigh = SUPERVISOR_1;
        *prev_neigh = SUPERVISOR_3;
    } else if(rank == SUPERVISOR_1) {
        *next_neigh = NO_NEIGH;
        *prev_neigh = SUPERVISOR_2;
    }    
}


int main(int argc, char* argv[])
{
    int **topology;
    int *number_of_workers;
    int supervisor;
    int N;
    int *V;

    int **topology_after_partitions;
    int *number_of_workers_after_partition;
    int N_after_partition;
    int *V_after_partition;

    int **topology_bonus;
    int *number_of_workers_bonus;
    int N_bonus;
    int *V_bonus;
    int rank_bonus;

    int next_neigh, prev_neigh;
    int next_neigh_partition, prev_neigh_partition;
    int rank;
    int task = atoi(argv[2]);

    /* Allocate memory */
    topology = malloc(NUM_SUPERVISORS * sizeof(int *));
    topology_after_partitions = malloc(NUM_SUPERVISORS * sizeof(int *));
    number_of_workers = malloc(NUM_SUPERVISORS * sizeof(int));
    number_of_workers_after_partition = malloc(NUM_SUPERVISORS * sizeof(int));

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if(task == 0) {
        /* Task #1 - Share topology */
        if(is_supervisor(rank)) {
            /* Read from input file and store information on workers */
            read_workers(rank, &topology, &number_of_workers);

            /* Share between the supervisors the number of workers */
            share_number_of_workers(rank, number_of_workers);
            complete_number_of_workers(rank, number_of_workers, &topology);
            
            /* Share between the supervisors the topology */
            share_workers(rank, number_of_workers, topology);
            complete_workers(rank, number_of_workers, topology);

            /* Share final topology with workers */
            share_topology_with_workers(rank, number_of_workers, topology);
        } else {

            /* Workers receive topology from supervisor */
            get_topology_from_supervisor(rank, &supervisor, number_of_workers, &topology);
        }


        /* Task #2 - Find the resulted vector after equally distributing tasks between workers */
        if(rank == SUPERVISOR_0) {
            /* Get number of elements */
            N = atoi(argv[1]);
            /* Compute initial vector */
            initialize_vector(N, &V);
        }

        if(is_supervisor(rank)) {
            /* Share the vector between supervisors */
            share_vector(rank, &N, &V, number_of_workers, topology);

            /* Share vector with workers */
            share_workers_vector(rank, N, V, number_of_workers, topology);

            /* Merge and share final vector */
            merge_vector(rank, N, V);
        } else {

            /* Get vector from supervisor and modify corresponding chunk */
            get_vector_from_supervisor(rank, supervisor, &N, &V, number_of_workers);
        }

        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            free(topology[i]);
        }

    } else if(task == 1) {

        /* Task #3 - Same tasks but connection between 0 and 1 is broken */    
        if(is_supervisor(rank)) {
            /* Read from input file and store information on workers */
            read_workers(rank, &topology_after_partitions, &number_of_workers_after_partition);

            /* Decide neighs */
            decide_neighs(rank, &next_neigh_partition, &prev_neigh_partition);

            /* Share number of workers between supervisors */
            share_number_of_workers_after_partition(rank, number_of_workers_after_partition, next_neigh_partition, prev_neigh_partition);

            /* Allocate memory in the topology for the rest of the supervisors workers */
            for(int i = 0; i < NUM_SUPERVISORS; i++) {
                if(i != rank) {
                    topology_after_partitions[i] = malloc(number_of_workers_after_partition[i] * sizeof(int));
                }  
            }

            /* Share between the supervisors the topology */
            share_workers_after_partition(rank, number_of_workers_after_partition, topology_after_partitions, next_neigh_partition, prev_neigh_partition);

            /* Share final topology with workers */
            share_topology_with_workers(rank, number_of_workers_after_partition, topology_after_partitions);
        } else {

            /* Workers receive topology from supervisor */
            get_topology_from_supervisor(rank, &supervisor, number_of_workers_after_partition, &topology_after_partitions);
        }

        if(rank == SUPERVISOR_0) {
            /* Get number of elements */
            N_after_partition = atoi(argv[1]);

            /* Compute initial vector */
            initialize_vector(N_after_partition, &V_after_partition);
        }

        if(is_supervisor(rank)) {
            /* Share the vector between supervisors */
            share_vector_after_partition(rank, &N_after_partition,
                            &V_after_partition,
                            number_of_workers_after_partition,
                            topology_after_partitions, next_neigh_partition, prev_neigh_partition);

            /* Share vector with workers */                
            share_workers_vector(rank, N_after_partition,
                            V_after_partition,
                            number_of_workers_after_partition,
                            topology_after_partitions);
            
            /* Merge and share final vector */
            merge_vector(rank, N_after_partition, V_after_partition);
       } else {

            /* Get vector from supervisor and modify corresponding chunk */
            get_vector_from_supervisor(rank, supervisor, &N_after_partition, &V_after_partition, number_of_workers_after_partition);
        }

        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            free(topology_after_partitions[i]);
        }
      
    } else if(task == 2) {
        /* Task #BONUS - Same tasks but SUPERVISOR_1 is isolated */
        topology_bonus = malloc(NUM_SUPERVISORS * sizeof(int *));
        number_of_workers_bonus = malloc(NUM_SUPERVISORS * sizeof(int));

        if(is_supervisor(rank)) {
            /* Read from input file and store information on workers */
            read_workers(rank, &topology_bonus, &number_of_workers_bonus);

            /* Get corresponding direct connections */
            decide_neighs_bonus(rank, &next_neigh, &prev_neigh);

            /* Share number of workers between supervisors */
            share_number_of_workers_bonus(rank, number_of_workers_bonus, next_neigh, prev_neigh);
            
            if(rank != SUPERVISOR_1) {
                /* SUPERVISOR_1 has an unknown number of workers */
                number_of_workers_bonus[SUPERVISOR_1] = UNKNOWN;

                /* Allocate memory for the rest of the supervisors */
                for(int i = 0; i < NUM_SUPERVISORS; i++) {
                    if(i != SUPERVISOR_1 && i != rank) {
                        topology_bonus[i] = malloc(number_of_workers_bonus[i] * sizeof(int));
                    }
                }
            } else {
                /* SUPERVISOR_1 is isolated and has no information regarding the rest of the supervisors */
                number_of_workers_bonus[SUPERVISOR_0] = number_of_workers_bonus[SUPERVISOR_2] = number_of_workers_bonus[SUPERVISOR_3] = UNKNOWN;
            }

            /* Share between the supervisors the topology */
            share_workers_bonus(rank, number_of_workers_bonus, topology_bonus, next_neigh, prev_neigh);

            /* Share final topology with workers */
            share_topology_with_workers(rank, number_of_workers_bonus, topology_bonus);
        } else {

            /* Workers receive topology from supervisor */
            get_topology_from_supervisor_bonus(rank, &supervisor, number_of_workers_bonus, &topology_bonus);
        }

        if(rank == SUPERVISOR_0) {
            /* Get number of elements */
            N_bonus = atoi(argv[1]);

            /* Compute initial vector */
            initialize_vector(N_bonus, &V_bonus);
        }
        if(is_supervisor(rank) && rank != SUPERVISOR_1) {
            /* Share the vector between supervisors */
            share_vector_bonus(rank, &N_bonus, &V_bonus,
                            number_of_workers_bonus,
                            topology_bonus, next_neigh, prev_neigh);

            /* Share vector with workers */ 
            share_workers_vector_bonus(rank, N_bonus, V_bonus, number_of_workers_bonus, topology_bonus);
            
            /* Merge and share final vector */
            merge_vector_bonus(rank, N_bonus, V_bonus, next_neigh, prev_neigh);
        } else if(rank != SUPERVISOR_1 && supervisor != SUPERVISOR_1){

            /* Get vector from supervisor and modify corresponding chunk */
            get_vector_from_supervisor(rank, supervisor, &N_bonus, &V_bonus, number_of_workers_bonus);
        }
    }
    free(topology);
    free(topology_after_partitions);
    free(number_of_workers);
    free(number_of_workers_after_partition);  
    
    MPI_Finalize(); 

	return 0;
}
