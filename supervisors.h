/*
    Supervisors.h
    
    For details on implementation see supervisors.c

    @author Dumitrescu Alexandra
    @since January 2023
*/

#ifndef _SUPERVISOR_H

#define SUPERVISOR_0 0
#define SUPERVISOR_1 1
#define SUPERVISOR_2 2
#define SUPERVISOR_3 3
#define NUM_SUPERVISORS 4
#define NO_NEIGH -1
#define UNKNOWN -1

void print_topology_supervisor(int rank, int *number_of_workers, int **topology);
int is_supervisor(int rank);
void read_workers(int rank, int ***topology, int **number_of_workers);
void share_number_of_workers(int rank, int *number_of_workers);
void complete_number_of_workers(int rank, int *number_of_workers, int ***topology);
void share_workers(int rank, int *number_of_workers, int **topology);
void complete_workers(int rank, int *number_of_workers, int **topology);
void share_topology_with_workers(int rank, int *number_of_workers, int **topology);
void initialize_vector(int N, int **V);
void share_vector(int rank, int *N, int **V, int *number_of_workers, int **topology);
void share_workers_vector(int rank, int N, int *V, int *number_of_workers, int **topology);
void share_workers_vector_bonus(int rank, int N, int *V, int *number_of_workers, int **topology);
void merge_vector(int rank, int N, int *V);
void share_number_of_workers_after_partition(int rank, int *number_of_workers, int next_neigh, int prev_neigh);
void share_workers_after_partition(int rank, int *number_of_workers, int **topology, int next_neigh, int prev_neigh);
void share_topology_with_workers_after_partition(int rank, int *number_of_workers, int **topology);
void share_vector_after_partition(int rank, int *N, int **V, int *number_of_workers, int **topology, int next_neigh, int prev_neigh);
void merge_vector_after_partition(int rank, int N, int *V);
void share_number_of_workers_bonus(int rank, int *number_of_workers, int next_neigh, int prev_neigh);
void share_workers_bonus(int rank, int *number_of_workers, int **topology, int next_neigh, int prev_neigh);
void share_vector_bonus(int rank, int *N, int **V, int *number_of_workers, int **topology, int next_neigh, int prev_neigh);
void merge_vector_bonus(int rank, int N, int *V, int next_neigh, int prev_neigh);

#endif