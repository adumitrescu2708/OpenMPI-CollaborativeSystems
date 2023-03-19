/*
    workers.h
    
    For details on implementation see workers.c

    @author Dumitrescu Alexandra
    @since January 2023
*/

#ifndef _WORKERS_H

void get_topology_from_supervisor(int rank, int *supervisor, int *number_of_workers, int ***topology);
void get_vector_from_supervisor(int rank, int supervisor, int *N, int **V, int *number_of_workers);
void get_topology_from_supervisor_bonus(int rank, int *supervisor, int *number_of_workers, int ***topology);
void print_topology_worker(int rank, int *number_of_workers, int ***topology);
#endif