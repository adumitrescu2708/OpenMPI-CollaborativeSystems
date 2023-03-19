#include "supervisors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#define MIN(a,b) a < b ? a:b

/*
    Supervisors.c
    |    - describes main functionality and implementation details
    |      of supervisors workers

    @author Dumitrescu Alexandra
    @since January 2023
*/


/*

    @param1 - rank of global process

    Given a global process rank, this function checks
    whether a process is considered supervisor or not

*/
int is_supervisor(int rank) {
    if(rank == SUPERVISOR_0
    || rank == SUPERVISOR_1
    || rank == SUPERVISOR_2
    || rank == SUPERVISOR_3) {
        return 1;
    } else {
        return 0;
    }
}


/*

    @param1 - rank of global process
    @param2 - pointer to the topology of the process
    @param3 - number of workers of each superviser, used to determine
    |         dimensions of topology's dynamic arrays         

    Given a global process rank of a supervisor, this method
    reads the information from the corresponding input file
    (workers and number of workers) and allocates memory in the
    topology for workers and stores them.

    [Example]:
        topology[i] = array of i'th s supervisor workers
        number_of_workers[i] = counter of i'th s supervisor workers
        
        Supervisor X reads N workers, allocated memory in the topology
        for N workers at topology[X] and saves N in number_of_workers[X].

*/
void read_workers(int rank, int ***topology, int **number_of_workers) {
    /* Compute corresponding filename */
    char cluster_filename[13];
    strcpy(cluster_filename, "cluster");
    cluster_filename[7] = rank + '0';
    cluster_filename[8] = '\0';
    strcat(cluster_filename, ".txt");

    FILE *file = fopen(cluster_filename, "r");
    /* Check possible error */
    if(file == NULL) {
        printf("File %s not found!", cluster_filename);
    }

    /* Read number of workers and the workers and
       save information in topology and vector */
    int number_of_workers_cluster, worker;
    fscanf(file, "%d", &number_of_workers_cluster);
    (*topology)[rank] = malloc(number_of_workers_cluster * sizeof(int));
    (*number_of_workers)[rank] = number_of_workers_cluster;
    for(int i = 0; i < number_of_workers_cluster; i++) {
        fscanf(file, "%d", &worker);
        (*topology)[rank][i] = worker;
    }

    fclose(file);
}


/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor

    We first share between the supervisor the number of workers for each one.
    This information is useful especially for knowing the dimensions
    of the topology and therefor allocating the right chunks of memory.
    By the end of this cycle, SUPERVISOR_0 knows the number of workers for each
    process.
    

    [Example]
    | SUPERVISOR_0 has [1, 0, 0, 0] --> sends to SUPERVISOR_1
    | SUPERVISOR_1 has [0, 2, 0, 0] and updates with received 
    |              info from SUPERVISOR_0 [1, 2, 0, 0] --> sends to SUPERVISOR_2
    | SUPERVISOR_2 has [0, 0, 2, 0] and updates with received 
    |              info from SUPERVISOR_1 [1, 2, 2, 0] --> sends to SUPERVISOR_3
    | SUPERVISOR_3 has [0, 0, 0, 1] and updates with received 
    |              info from SUPERVISOR_2 [1, 2, 2, 1] --> sends to SUPERVISOR_0

    | 0 -> 1 -> 2 -> 3      Circular communication between supervisors
    | ^______________|
    
*/

void share_number_of_workers(int rank, int *number_of_workers) {
    int received_numbers[NUM_SUPERVISORS];

    if(rank == SUPERVISOR_0) {
        /* Send number of workers */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");
        MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, NUM_SUPERVISORS - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        /* Update number of workers */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            number_of_workers[i] = received_numbers[i];
        }
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Receive and update number of workers */
        MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < NUM_SUPERVISORS - 1; i++) {
            number_of_workers[i] = received_numbers[i];
        }
        /* Send updated number of workers to SUPERVISOR_0 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, 0, 0, MPI_COMM_WORLD);
        printf("M(%d,0)\n", rank);
    } else if(rank > SUPERVISOR_0 && rank < SUPERVISOR_3){
        /* Receive and update number of workers */
        MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                number_of_workers[i] = received_numbers[i];
            }    
        }
        /* Send updated number of workers to next supervisor */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
    }
}

/*
    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - pointer to the topology

    In the previous method, SUPERVISOR_0 managed to find out the number of workers
    for each supervisor and now it shares this array again with all supervisors.
    This method also allocates the rest of the topology, knowing the exact
    dimensions of each array based on number_of_workers shared info.

    [Example]
    | 0 -> 1 -> 2 -> 3      Liniar communication between supervisors


*/
void complete_number_of_workers(int rank, int *number_of_workers, int ***topology) {
    int received_numbers[NUM_SUPERVISORS];

    if(rank == SUPERVISOR_0) {
        /* Allocate memory in topology for others supervisors workers */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                (*topology)[i] = malloc(number_of_workers[i] * sizeof(int));
            }
        }

        /* Send array with next supervisor*/
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Receive and update number of workers */
        MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            number_of_workers[i] = received_numbers[i];
        }

        /* Allocate memory in topology */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                (*topology)[i] = malloc(number_of_workers[i] * sizeof(int));
            }
        }
    } else if(rank > SUPERVISOR_0 && rank < SUPERVISOR_3){
        /* Receive and update number of workers and send them to next supervisor */
        MPI_Recv(received_numbers, NUM_SUPERVISORS, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            number_of_workers[i] = received_numbers[i];  
        }
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
        
        /* Allocate memory in topology */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                (*topology)[i] = malloc(number_of_workers[i] * sizeof(int));
            }
        }
    }    
}


/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - pointer to the topology

    Now that all supervisors know how many workers are in the topology
    each supervisor computes the total number of them. All supervisors
    now share between each other their workers. At the end of this cycle,
    SUPERVISOR_0 knows the topology.

    [Example]
    | SUPERVISOR_0 has 1 worker - 4 --> sends to SUPERVISOR_1
    | SUPERVISOR_1 has 2 workers - 5, 6 and receives and updates,
    |           knowing workers of SUPERVISOR_0 --> sends to SUPERVISOR_2
    | SUPERVISOR_2 has 2 workers - 7, 8 and receives and updates,
    |              knowing workers of SUPERVISOR_0, SUPERVISOR_1 --> sends to SUPERVISOR_3
    | SUPERVISOR_3 has 1 worker - 9 and receives and updates, 
    |              knowing workers of SUPERVISOR_0, SUPERVISOR_1, SUPERVISOR_2
    |                        --> sends back to SUPERVISOR_0

    | 0 -> 1 -> 2 -> 3      Circular communication between supervisors
    | ^______________|

*/
void share_workers(int rank, int *number_of_workers, int **topology)
{
    int total_number_of_workers = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total_number_of_workers += number_of_workers[i];
    }

    int send_numbers[total_number_of_workers];
    int receive_numbers[total_number_of_workers];

    if(rank == SUPERVISOR_0) {
        /* Copy workers in a vector and send them to next supervisor */
        int l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                send_numbers[l] = topology[i][j];
                l++;
            }
        }
        MPI_Send(send_numbers, total_number_of_workers, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");

        /* Receive and update topology */
        MPI_Recv(receive_numbers, total_number_of_workers, MPI_INT, NUM_SUPERVISORS - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                topology[i][j] = receive_numbers[l];
                l++;
            }
        }
        print_topology_supervisor(rank, number_of_workers, topology);
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Receive and update topology */
        MPI_Recv(receive_numbers, total_number_of_workers, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int l = 0;
        for(int i = 0; i < NUM_SUPERVISORS - 1; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                topology[i][j] = receive_numbers[l];
                l++;
            }
        }
        print_topology_supervisor(rank, number_of_workers, topology);
        /* Copy received topology and add self workers and send info back to SUPERVISOR_0 */
        l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                send_numbers[l] = topology[i][j];
                l++;
            }
        }
        MPI_Send(send_numbers, total_number_of_workers, MPI_INT, 0, 0, MPI_COMM_WORLD);
        printf("M(%d,0)\n", rank);
    } else {
        /* Receive and update topology */
        MPI_Recv(receive_numbers, total_number_of_workers, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int l = 0;
        for(int i = 0; i < NUM_SUPERVISORS - 1; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                if(i != rank) {
                    topology[i][j] = receive_numbers[l];
                }
                l++;
            }
        }

        /* Copy received topology and add self workers and send info to next supervisor */
        l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                send_numbers[l] = topology[i][j];
                l++;
            }
        }
        MPI_Send(send_numbers, total_number_of_workers, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);    
    }
}


/*
    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - pointer to the topology

    In the previous method, SUPERVISOR_0 managed to find out the final topology
    and now it shares it again with all supervisors.

    [Example]
    | 0 -> 1 -> 2 -> 3      Liniar communication between supervisors


*/
void complete_workers(int rank, int *number_of_workers, int **topology)
{
    int total_number_of_workers = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total_number_of_workers += number_of_workers[i];
    }
    int send_numbers[total_number_of_workers];
    int receive_numbers[total_number_of_workers];

    if(rank == SUPERVISOR_0) {
        /* Copy final topology in an array and send them to next supervisor */
        int l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                send_numbers[l] = topology[i][j];
                l++;
            }
        }

        MPI_Send(send_numbers, total_number_of_workers, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Receive and copy topology */
        MPI_Recv(receive_numbers, total_number_of_workers, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    } else {
        /* Receive, copy the topology and send it to next supervisor */
        MPI_Recv(receive_numbers, total_number_of_workers, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        int l = 0;
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            for(int j = 0; j < number_of_workers[i]; j++) {
                topology[i][j] = receive_numbers[l];
                l++;
            }
        }
        print_topology_supervisor(rank, number_of_workers, topology);
        MPI_Send(receive_numbers, total_number_of_workers, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);    
    }
}


/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - pointer to the topology

    Each supervisor knows the final topology and sends it to its workers.
    It first sends the number_of_workers vector and then the entire topology.

    [Example]
    | 1         SUPERVISOR_1 has 2 workers 5, 6 and sends them the dimensions
    | | \       and then the entire topology.
    | 5  6  

*/

void share_topology_with_workers(int rank, int *number_of_workers, int **topology)
{
    /* Send number of workers */
    for(int i = 0; i < number_of_workers[rank]; i++) {
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
    }
    /* Copy topology in an array */
    int total = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(number_of_workers[i] != UNKNOWN) 
            total += number_of_workers[i];
    }
    int send_numbers[total];
    int l = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        for(int j = 0; j < number_of_workers[i]; j++) {
            send_numbers[l] = topology[i][j];
            l++;
        }
    }
    /* Send topology to all workers */
    for(int i = 0; i < number_of_workers[rank]; i++) {
        MPI_Send(send_numbers, total, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
    }
}

/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - pointer to the topology

    This method is used for printing the final topology.

*/
void print_topology_supervisor(int rank, int *number_of_workers, int **topology) {
    printf("%d -> ", rank);

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        printf("%d:", i);

        for(int j = 0; j < number_of_workers[i] - 1; j++) {
            printf("%d,", topology[i][j]);
        }
        printf("%d", topology[i][number_of_workers[i] - 1]);
        printf(" ");
    }
    printf("\n");
}


/*

    @param1 - N number of elements in array
    @param2 - The initial vector, computed by SUPERVISOR_0

*/
void initialize_vector(int N, int **V)
{
    (*V) = malloc(N * sizeof(int));
    for(int i = 0; i < N; i++) {
        (*V)[i] = N - i - 1;
    }
}

/*
    @param1 - rank of global process
    @param2 - N number of elements in the shared array
    @param3 - pointer to shared array
    @param4 - number of workers for each supervisor in topology
    @param5 - topology

    In this method, SUPERVISOR_0 shares the array with all supervisors
    in topology. It firsts sends the number of elements N and then
    the entire array. The rest of the supervisors allocate memory
    and copies the information received.

    [Example]
    | 0 -> 1 -> 2 -> 3      Liniar communication between supervisors


*/
void share_vector(int rank, int *N, int **V, int *number_of_workers, int **topology)
{
    int received_N;
    
    if(rank == SUPERVISOR_0) {
        /* Send array to next supervisor */
        MPI_Send(N, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");
        MPI_Send((*V), *N, MPI_INT, 1, 0, MPI_COMM_WORLD);
        printf("M(0,1)\n");
        
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Copy and alocate memory for received array */
        MPI_Recv(&received_N, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        (*N) = received_N;
        (*V) = malloc(received_N * sizeof(int));
        int received_V[received_N];
        MPI_Recv(received_V, received_N, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < received_N; i++) {
            (*V)[i] = received_V[i];
        }
    } else {
        /* Copy and alocate memory for received array and send the information to next supervisor */
        MPI_Recv(&received_N, 1, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        (*N) = received_N;
        (*V) = malloc(received_N * sizeof(int));
        int received_V[received_N];
        MPI_Recv(received_V, received_N, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < received_N; i++) {
            (*V)[i] = received_V[i];
        }
        MPI_Send(&received_N, 1, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
        MPI_Send(received_V, received_N, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
    }
}

/*

    @param1 - rank of global process
    @param2 - N number of elements in the shared array
    @param3 - pointer to shared array
    @param4 - number of workers for each supervisor in topology
    @param5 - topology

    Once all supervisors have the vector, each one of them shares it
    with his own workers. After sending the vector, the supervisor waits
    to receive back updates from workers and updates the vector.

*/
void share_workers_vector(int rank, int N, int *V, int *number_of_workers, int **topology)
{
    int total_workers = 0;
    int start_idx, end_idx;

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total_workers += number_of_workers[i];
    }

    for(int i = 0; i < number_of_workers[rank]; i++) {
        /* Send vector to workers */
        int received_updates[N];
        MPI_Send(&N, 1, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
        MPI_Send(V, N, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);

        int worker_id = topology[rank][i];

        start_idx = (int) ((worker_id - NUM_SUPERVISORS) * ((double) N/ (double) total_workers));
        end_idx = (int) MIN(N, (worker_id - NUM_SUPERVISORS + 1) * ((double) N / (double) total_workers));        

        MPI_Send(&start_idx, 1, MPI_INT, worker_id, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
        MPI_Send(&end_idx, 1, MPI_INT, worker_id, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);

        // /* Wait for updates and copy them */
        MPI_Recv(received_updates, N, MPI_INT, worker_id, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int j = start_idx; j < end_idx; j++) {
            V[j] = received_updates[j];
        }
    }
}


/*

    @param1 - rank of global process
    @param2 - N number of elements in the shared array
    @param3 - pointer to shared array
    @param4 - number of workers for each supervisor in topology
    @param5 - topology

    Once all supervisors have the vector, each one of them shares it
    with his own workers. After sending the vector, the supervisor waits
    to receive back updates from workers and updates the vector.

*/
void share_workers_vector_bonus(int rank, int N, int *V, int *number_of_workers, int **topology)
{
    int total_workers = 0;
    int start_idx, end_idx;

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total_workers += number_of_workers[i];
    }


    int start_rank = 0;
    for(int i = 0; i < rank; i++) {
        if(i != SUPERVISOR_1) {
            start_rank += number_of_workers[i];
        }
    }


    for(int i = 0; i < number_of_workers[rank]; i++, start_rank++) {
        /* Send vector to workers */
        int received_updates[N];
        MPI_Send(&N, 1, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
        MPI_Send(V, N, MPI_INT, topology[rank][i], 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);

        int worker_id = topology[rank][i];

        start_idx = (int) ((start_rank) * ((double) N/ (double) total_workers));
        end_idx = (int) MIN(N, (start_rank + 1) * ((double) N / (double) total_workers));        

        MPI_Send(&start_idx, 1, MPI_INT, worker_id, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);
        MPI_Send(&end_idx, 1, MPI_INT, worker_id, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, topology[rank][i]);

        // /* Wait for updates and copy them */
        MPI_Recv(received_updates, N, MPI_INT, worker_id, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int j = start_idx; j < end_idx; j++) {
            V[j] = received_updates[j];
        }
    }
}


/*
    @param1 - rank of global process
    @param2 - N number of elements in the shared array
    @param3 - pointer to shared array

    Each supervisor has received the updates from his workers and now
    they share between each other the updates, starting from 
    SUPERVISOR_1 to SUPERVISOR_0. In the end, SUPERVISOR_0 has
    the finally updated array and prints the result.

    [Disclaimer]
    This method is used also in the second task, where connection
    between SUPERVISOR_0 and SUPERVISOR_1 is broken

    [Example]

    [Example]
    | Initial vecor [1, 2, 3, 4, 5, 6]
    | SUPERVISOR_0 has 1 worker - modifies [5, 2, 3, 5, 6]--> sends to SUPERVISOR_1
    | SUPERVISOR_1 has 2 workers - receives, copies and then modifies [5, 10, 15, 4, 5, 6]
    |                       --> sends to SUPERVISOR_2
    | SUPERVISOR_2 has 2 workers - receives, copies and then modifies [5, 10, 15, 20, 25, 6]
    |                       --> sends to SUPERVISOR_3    
    | SUPERVISOR_3 has 1 workers - receives, copies and then modifies [5, 10, 15, 20, 25, 30]
    |                       --> sends to SUPERVISOR_0


    | 1 -> 2 -> 3 ->0    Circular communication between supervisors
    | 
*/
void merge_vector(int rank, int N, int *V) {
    if(rank == SUPERVISOR_1) {
        /* Start communication and share updates to next supervisor */
        MPI_Send(V, N, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
    } else if(rank == NUM_SUPERVISORS - 1) {
        /* Receive updates, copy them and share them to the next supervisor */
        int received[N];
        MPI_Recv(received, N, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < N; i++) {
            if(V[i] != received[i] && V[i] == (N - i - 1)) {
                V[i] = received[i];
            }
        }
        MPI_Send(V, N, MPI_INT, 0, 0, MPI_COMM_WORLD);
        printf("M(%d,0)\n", rank);
    } else if(rank == SUPERVISOR_2) {
        int received[N];
        MPI_Recv(received, N, MPI_INT, rank - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < N; i++) {
            if(V[i] != received[i] && V[i] == (N - i - 1)) {
                V[i] = received[i];
            }
        }
        MPI_Send(V, N, MPI_INT, rank + 1, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, rank + 1);
    } else if(rank == SUPERVISOR_0){
        /* Receive and merge final vector and print the result */
        int received[N];
        MPI_Recv(received, N, MPI_INT, NUM_SUPERVISORS - 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < N; i++) {
            if(V[i] != received[i] && V[i] == N - i - 1) {
                V[i] = received[i];
            }
        }
        printf("Rezultat: ");
        for(int i = 0; i < N; i++) {
            printf("%d ", V[i]);
        }
        printf("\n");
    }
}

/*

    @param1 - rank of global process
    @param2 - vector with number of workers
    @param3 - next neigh global process id
    @param4 - previous neigh global process id

    Once the connection between SUPERVISOR_0 and SUPERVISOR_1 is broken,
    we will establish the communication between supervisors in linear way,
    from SUPERVISOR_0 to SUPERVISOR_1 and back.
    
    [Example]
|    SUPERVISOR_0 -> SUPERVISOR_3 -> SUPERVISOR_2 -> SUPERVISOR_1
|           ^____________|   ^___________|     ^ __________|

    This method is used for sharing the number of workers vector.
                                            

*/
void share_number_of_workers_after_partition(int rank, int *number_of_workers, int next_neigh, int prev_neigh)
{
    int received[NUM_SUPERVISORS];

    if(rank == SUPERVISOR_0) {
        /* Send vector to SUPERVISOR_3 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive back the vector from SUPERVISOR_3 */
        MPI_Recv(received, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Update the vector */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                number_of_workers[i] = received[i];
            }
        }        
    } else if(rank == SUPERVISOR_1) {
        /* Receive from SUPERVISOR_2 */
        MPI_Recv(received, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Update the vector */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                number_of_workers[i] = received[i];
            }
        }

        /* Send back the updated vector to SUPERVISOR_2 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);
    
    } else {
        /* Receive from previous neigh */
        MPI_Recv(received, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Update the vector */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            if(i != rank) {
                number_of_workers[i] = received[i];
            }
        }

        /* Send updates to next neigh */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive updates back from next neigh */
        MPI_Recv(received, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Update the vector */
        for(int i = 0; i < NUM_SUPERVISORS; i++) {
            number_of_workers[i] = received[i];
        }

        /* Send the updates back to previous neigh */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);     
    }
}

/*
    @param1 - topology
    @param2 - vector with number of workers
    @param3 - vector where the topology is copied
*/
void copy_topology_to_dest(int **topology, int *number_of_workers, int *dest)
{
    int l = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        for(int j = 0; j < number_of_workers[i]; j++) {
            dest[l] = topology[i][j];
            l++;
        }
    }    
}

/*
    @param1 - rank of callee process
    @param2 - topology
    @param3 - vector with number of workers
    @param4 - vector from where the updates are copied
*/
void copy_topology_from_src(int rank, int **topology, int *number_of_workers, int *src)
{
    int l = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        for(int j = 0; j < number_of_workers[i]; j++) {
            if(rank != i) {
                topology[i][j] = src[l];
            }
            l++;
        }
    }
}

/*

    @param1 - rank of global process
    @param2 - vector with number of workers
    @param3 - topology
    @param4 - next neigh global process id
    @param5 - previous neigh global process id

    Once all supervisors know each others dimensions and had allocated memory
    for the topology, they share in similar way their workers. SUPERVISOR_1
    will know all others workers and merging them with its own will have
    the final topology that is being send back.
    
    [Example]
|    SUPERVISOR_0 -> SUPERVISOR_3 -> SUPERVISOR_2 -> SUPERVISOR_1
|           ^____________|   ^___________|     ^ __________|
                                        

*/
void share_workers_after_partition(int rank, int *number_of_workers, int **topology, int next_neigh, int prev_neigh)
{
    /* Compute the total number of workers */
    int total = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        total += number_of_workers[i];
    }

    int send_numbers[total];
    int receive_numbers[total];

    if(rank == SUPERVISOR_0) {
        /* Copy and send self workers to SUPERVISOR_3 */
        copy_topology_to_dest(topology, number_of_workers, send_numbers);

        MPI_Send(send_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive updates and copy them from SUPERVISOR_3 */
        MPI_Recv(receive_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);

        /* Print the final topology once computed */
        print_topology_supervisor(rank, number_of_workers, topology);
    } else if(rank == SUPERVISOR_1) {
        /* Receive updates and compute the final topology */
        MPI_Recv(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);
        copy_topology_to_dest(topology, number_of_workers, send_numbers);

        /* Print the final topology once computed */
        print_topology_supervisor(rank, number_of_workers, topology);

        /* Send back to SUPERVIOR_2 the final topology */
        MPI_Send(send_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);
    } else {
        /* Receive updates from previous neigh and copy them */
        MPI_Recv(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);

        /* Merge received updates with self workers */
        copy_topology_to_dest(topology, number_of_workers, send_numbers);

        /* Send merged topology to next neigh */
        MPI_Send(send_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive final topology from next neigh */
        MPI_Recv(receive_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);

        /* Print the final topology once computed */
        print_topology_supervisor(rank, number_of_workers, topology);

        /* Send it to prev neigh */
        MPI_Send(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);     
    }
}

/*

    @param1 - rank of global process
    @param2 - vnumber of elements in vector
    @param3 - shared vector
    @param4 - topology
    @param5 - next neigh global process id
    @param6 - previous neigh global process id

    Share number of elements in vector and then the elements of
    the vector. SUPERVISOR_0 starts the communication
    
    [Example]
|    SUPERVISOR_0 -> SUPERVISOR_3 -> SUPERVISOR_2 -> SUPERVISOR_1
|           ^____________|   ^___________|     ^ __________|
                                        

*/
void share_vector_after_partition(int rank, int *N, int **V, int *number_of_workers, int **topology, int next_neigh, int prev_neigh)
{
    int received_N;

    if(rank == SUPERVISOR_0) {
        /* Send number of elements and vector to SUPERVISOR_3 */
        MPI_Send(N, 1, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);
        MPI_Send(*V, *N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);
        
    } else {
        /* Receive number of elements from previous neigh */
        MPI_Recv(&received_N, 1, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Copy data and allocate memory */
        (*N) = received_N;
        int received_V[received_N];
        (*V) = malloc(received_N * sizeof(int));

        /* Receive vector and copy elements */
        MPI_Recv(received_V, received_N, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        for(int i = 0; i < received_N; i++) {
            (*V)[i] = received_V[i];
        }

        /* Send to next supervisor, until SUPERVISOR_1 is reached */
        if(rank != SUPERVISOR_1) {
            MPI_Send(&received_N, 1, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, next_neigh); 
            MPI_Send(received_V, received_N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, next_neigh);       
        }
    }
}

void update_numbers(int rank, int *src, int *dest) {
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(i != rank) {
            dest[i] = src[i];
        }
    }
}

/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - next neigh
    @param4 - previous neigh

    This method is used for sharing the number of workers vector in topology
    when conection between SUPERVISOR_0 and SUPERVISOR_1 is broken.
    The idea is the same, this time SUPERVISOR_0 sends to SUPERVISOR_3, which
    sends then to SUPERVISOR_2 and then back again the information.

    [Example]
|    SUPERVISOR_0 -> SUPERVISOR_3 -> SUPERVISOR_2
|           ^____________|   ^___________|   

*/
void share_number_of_workers_bonus(int rank, int *number_of_workers, int next_neigh, int prev_neigh)
{
    int updated_workers[NUM_SUPERVISORS];

    if(rank == SUPERVISOR_0) {
        /* Send number of workers to SUPERVISOR_3 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive and update from SUPERVISOR_3 */
        MPI_Recv(updated_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        update_numbers(rank, updated_workers, number_of_workers);        
    } else if(rank == SUPERVISOR_3) {
        /* Receive and update from SUPERVISOR_1 */
        MPI_Recv(updated_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        update_numbers(rank, updated_workers, number_of_workers);

        /* Send received updates merged with self number of workers to SUPERVISOR_2 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive final array from SUPERVISOR_2 */
        MPI_Recv(updated_workers, NUM_SUPERVISORS, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        update_numbers(rank, updated_workers, number_of_workers);

        /* Send final array back to SUPERVISOR_0 */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);     
    } else if(rank == SUPERVISOR_2) {
        /* Receive SUPERVISOR'S 0 and SUPERVISOR'S 1 number of workers */
        MPI_Recv(updated_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Compute number of workers */
        update_numbers(rank, updated_workers, number_of_workers);
        
        /* Send final vector back */
        MPI_Send(number_of_workers, NUM_SUPERVISORS, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);        
    }
}

/* 
    This method is used for printing topology when SUPERVISOR_1 is
    disconected.
*/
void print_topology_bonus(int rank, int *number_of_workers, int **topology) {
    printf("%d -> ", rank);

    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(number_of_workers[i] != UNKNOWN) {
            printf("%d:", i);

            for(int j = 0; j < number_of_workers[i] - 1; j++) {
                printf("%d,", topology[i][j]);
            }
            printf("%d", topology[i][number_of_workers[i] - 1]);
            printf(" ");            
        }

    }
    printf("\n");
}

/*

    @param1 - rank of global process
    @param2 - array with counter of workers for each supervisor
    @param3 - topology
    @param4 - next neigh
    @param5 - previous neigh

    This method is used for sharing workers in topology
    when conection between SUPERVISOR_0 and SUPERVISOR_1 is broken.
    The idea is the same, this time SUPERVISOR_0 sends to SUPERVISOR_3, which
    sends then to SUPERVISOR_2 and then back again the information. This way,
    SUPERVISOR_2 is the first one to knoe the topology.

    [Example]
|    SUPERVISOR_0 -> SUPERVISOR_3 -> SUPERVISOR_2
|           ^____________|   ^___________|   

*/
void share_workers_bonus(int rank, int *number_of_workers, int **topology, int next_neigh, int prev_neigh)
{
    /* Compute total number of workers */
    int total = 0;
    for(int i = 0; i < NUM_SUPERVISORS; i++) {
        if(i != SUPERVISOR_1) {
            total += number_of_workers[i];
        }

    }

    int send_numbers[total];
    int receive_numbers[total];

    if(rank == SUPERVISOR_0) {
        /* Copy workers and send to SUPERVISOR_3 */
        copy_topology_to_dest(topology, number_of_workers, send_numbers);
        MPI_Send(send_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Receive and update topology from SUPERVISOR_3 */
        MPI_Recv(receive_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);

        /* Print final topology once computed */
        print_topology_bonus(rank, number_of_workers, topology);
    } else if(rank == SUPERVISOR_2) {
        /* Receive updates */
        MPI_Recv(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);
        copy_topology_to_dest(topology, number_of_workers, send_numbers);

        /* Print final topology */
        print_topology_bonus(rank, number_of_workers, topology);

        /* Send back the final merged topology */
        MPI_Send(send_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);
    } else if(rank == SUPERVISOR_3) {
        /* Receive and update from SUPERVISOR_0 */
        MPI_Recv(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);
        copy_topology_to_dest(topology, number_of_workers, send_numbers);

        /* Send topology to next neigh */
        MPI_Send(send_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);

        /* Recerive merged topology from next neigh */
        MPI_Recv(receive_numbers, total, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        copy_topology_from_src(rank, topology, number_of_workers, receive_numbers);

        /* Print topology once known */
        print_topology_bonus(rank, number_of_workers, topology);

        /* Send topology back to previous neigh */
        MPI_Send(receive_numbers, total, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);            
    } else if (rank == SUPERVISOR_1) {

        /* SUPERVISOR_1 doesn't communicate with the rest of the supervisors and prints directly */
        print_topology_bonus(rank, number_of_workers, topology);
    }
}

/*
    @param1 - rank of global process
    @param2 - number of elements in array
    @param3 - shared array
    @param4 - number of workers vector
    @param5 - topology
    @param4 - next neigh
    @param5 - previous neigh

    This method is used for sharing the vector between supervisors in case
    of SUPERVISOR_1 is disconected. SUPERVISOR_0 starts the communication and
    sends his vector to SUPERVISOR_3 who sends to SUPERVISOR_2.
*/
void share_vector_bonus(int rank, int *N, int **V, int *number_of_workers, int **topology, int next_neigh, int prev_neigh)
{
    int received_N;

    if(rank == SUPERVISOR_0) {
        /* Send number of elements and then entire vector */
        MPI_Send(N, 1, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);  
        MPI_Send(*V, *N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, next_neigh);        
    } else {
        /* Receive vector and allocate memory for it */
        MPI_Recv(&received_N, 1, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        (*N) = received_N;
        int received_V[received_N];
        (*V) = malloc(received_N * sizeof(int));
        MPI_Recv(received_V, received_N, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        /* Copy recieved date */
        for(int i = 0; i < received_N; i++) {
            (*V)[i] = received_V[i];
        }

        /* Send vector until SUPERVISOR_2 who has no next supervisor in line */
        if(next_neigh != NO_NEIGH) {
            MPI_Send(&received_N, 1, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, next_neigh);
            MPI_Send(received_V, received_N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD);
            printf("M(%d,%d)\n", rank, next_neigh);             
        }        
    }
}

/*

    @param1 - rank of global process
    @param2 - number of elements in array
    @param3 - shared array
    @param4 - next neigh
    @param5 - previous neigh

    This time, SUPERVISOR_2 starts the communication and sends
    back the updates to SUPERVISOR_3 who sends back to SUPERVISOR_0.
    SUPERVISOR_3 copies updates from SUPERVISOR_2 only if the value is
    different from the initial one and is not the same with what he 
    already has.

*/
void merge_vector_bonus(int rank, int N, int *V, int next_neigh, int prev_neigh)
{
    if(rank == SUPERVISOR_2) {
        /* Send the updated from the workers */
        MPI_Send(V, N, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);
    } else if(rank == SUPERVISOR_0){
        /* Receive vector */
        int received[N];
        MPI_Recv(received, N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Update vector if there are updates */
        for(int i = 0; i < N; i++) {
            if(V[i] != received[i] && V[i] == N - i - 1) {
                V[i] = received[i];
            }
        }

        /* Print final result */
        printf("Rezultat: ");
        for(int i = 0; i < N; i++) {
            printf("%d ", V[i]);
        }
        printf("\n");
    } else {
        /* Receive updates and copy them */
        int received[N];
        MPI_Recv(received, N, MPI_INT, next_neigh, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        for(int i = 0; i < N; i++) {
            if(V[i] != received[i] && V[i] == (N - i - 1)) {
                V[i] = received[i];
            }
        }

        /* Send back the updated vector */
        MPI_Send(V, N, MPI_INT, prev_neigh, 0, MPI_COMM_WORLD);
        printf("M(%d,%d)\n", rank, prev_neigh);        
    }
}