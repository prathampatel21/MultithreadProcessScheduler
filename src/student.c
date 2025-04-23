/*
 * student.c
 * Multithreaded OS Simulation for CS 2200
 * Fall 2024
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "student.h"
#include <unistd.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);

static unsigned int cpu_count;

/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 *
 * rq is a pointer to a struct you should use for your ready queue
 * implementation. The head of the queue corresponds to the process
 * that is about to be scheduled onto the CPU, and the tail is for
 * convenience in the enqueue function. See student.h for the
 * relevant function and struct declarations.
 *
 * Similar to current[], rq is accessed by multiple threads,
 * so you will need to use a mutex to protect it. ready_mutex has been
 * provided for that purpose.
 *
 * The condition variable queue_not_empty has been provided for you
 * to use in conditional waits and signals.
 *
 * Please look up documentation on how to properly use pthread_mutex_t
 * and pthread_cond_t.
 *
 * A scheduler_algorithm variable and sched_algorithm_t enum have also been
 * supplied to you to keep track of your scheduler's current scheduling
 * algorithm. You should update this variable according to the program's
 * command-line arguments. Read student.h for the definitions of this type.
 */
static pcb_t **current;
static queue_t *rq;

static pthread_mutex_t current_mutex;
static pthread_mutex_t queue_mutex;
static pthread_cond_t queue_not_empty;

static sched_algorithm_t scheduler_algorithm;
static unsigned int cpu_count;
static unsigned int age_weight;
static unsigned int timeslice;


/** ------------------------Problem 3-----------------------------------
 * Checkout PDF Section 5 for this problem
 * 
 * priority_with_age() is a helper function to calculate the priority of a process
 * taking into consideration the age of the process.
 * 
 * It is determined by the formula:
 * Priority With Age = Priority - (Current Time - Enqueue Time) * Age Weight
 * 
 * @param current_time current time of the simulation
 * @param process process that we need to calculate the priority with age
 * 
 */
extern double priority_with_age(unsigned int current_time, pcb_t *process) {
    /* FIX ME */
    return process->priority - (current_time - process->enqueue_time) * age_weight;
}

/** ------------------------Problem 0 & 3-----------------------------------
 * Checkout PDF Section 2 and 5 for this problem
 * 
 * enqueue() is a helper function to add a process to the ready queue.
 * 
 * NOTE: For Priority, FCFS, and SRTF scheduling, you will need to have additional logic
 * in this function and/or the dequeue function to account for enqueue time
 * and age to pick the process with the smallest age priority.
 * 
 * We have provided a function get_current_time() which is prototyped in os-sim.h.
 * Look there for more information.
 *
 * @param queue pointer to the ready queue
 * @param process process that we need to put in the ready queue
 */
void enqueue(queue_t *queue, pcb_t *process)
{
    /* FIX ME */
    (* process).enqueue_time = get_current_time();
    (* process).next = NULL;
    if (is_empty(queue)) {
        queue->head = process;
        queue->tail = process;
    } else {
        
        queue->tail->next = process;
        queue->tail = process;
    }
}

/**
 * dequeue() is a helper function to remove a process to the ready queue.
 *
 * NOTE: For Priority, FCFS, and SRTF scheduling, you will need to have additional logic
 * in this function and/or the enqueue function to account for enqueue time
 * and age to pick the process with the smallest age priority.
 * 
 * We have provided a function get_current_time() which is prototyped in os-sim.h.
 * Look there for more information.
 * 
 * @param queue pointer to the ready queue
 */
pcb_t *dequeue(queue_t *q) {
    if (is_empty(q))
        return NULL;

    if (scheduler_algorithm == RR) {
        pcb_t *front = q->head;
        q->head = front->next;
        if (q->head == NULL)
            q->tail = NULL;
        front->next = NULL;
        return front;
    }

    if (scheduler_algorithm == FCFS) {
        pcb_t *earliest = q->head;
        pcb_t *pre_earliest = NULL;
        pcb_t *cursor = q->head->next;
        pcb_t *trail = q->head;

        while (cursor != NULL) {
            if (cursor->arrival_time < earliest->arrival_time) {
                earliest = cursor;
                pre_earliest = trail;
            }
            trail = cursor;
            cursor = cursor->next;
        }

        if (pre_earliest == NULL)
            q->head = earliest->next;
        else
            pre_earliest->next = earliest->next;

        if (earliest == q->tail)
            q->tail = pre_earliest;

        earliest->next = NULL;
        return earliest;
    }

    if (scheduler_algorithm == PA) {
        unsigned int current_time = get_current_time();
        pcb_t *highest_priority = q->head;
        pcb_t *pre_priority = NULL;
        double min_effective_priority = priority_with_age(current_time, highest_priority);

        pcb_t *prev = highest_priority;
        pcb_t *scan = highest_priority->next;

        while (scan != NULL) {
            double curr_priority = priority_with_age(current_time, scan);
            if (curr_priority < min_effective_priority) {
                min_effective_priority = curr_priority;
                highest_priority = scan;
                pre_priority = prev;
            }
            prev = scan;
            scan = scan->next;
        }

        if (pre_priority == NULL)
            q->head = highest_priority->next;
        else
            pre_priority->next = highest_priority->next;

        if (highest_priority == q->tail)
            q->tail = pre_priority;

        highest_priority->next = NULL;
        return highest_priority;
    }

    if (scheduler_algorithm == SRTF) {
        pcb_t *shortest = q->head;
        pcb_t *pre_shortest = NULL;

        if (!shortest)
            return NULL;

        unsigned int min_remaining = shortest->total_time_remaining;
        pcb_t *backtrack = shortest;
        pcb_t *step = shortest->next;

        while (step != NULL) {
            if (step->total_time_remaining < min_remaining) {
                min_remaining = step->total_time_remaining;
                shortest = step;
                pre_shortest = backtrack;
            }
            backtrack = step;
            step = step->next;
        }

        if (pre_shortest == NULL)
            q->head = shortest->next;
        else
            pre_shortest->next = shortest->next;

        if (shortest == q->tail)
            q->tail = pre_shortest;

        shortest->next = NULL;
        return shortest;
    }

    // Fallback (should not happen)
    pcb_t *fallback = q->head;
    q->head = fallback->next;
    if (q->head == NULL)
        q->tail = NULL;
    fallback->next = NULL;
    return fallback;
}

/** ------------------------Problem 0-----------------------------------
 * Checkout PDF Section 2 for this problem
 * 
 * is_empty() is a helper function that returns whether the ready queue
 * has any processes in it.
 * 
 * @param queue pointer to the ready queue
 * 
 * @return a boolean value that indicates whether the queue is empty or not
 */
bool is_empty(queue_t *queue)
{
    /* FIX ME */
    return (* queue).head == NULL;
}

/** ------------------------Problem 1B-----------------------------------
 * Checkout PDF Section 3 for this problem
 * 
 * schedule() is your CPU scheduler.
 * 
 * Remember to specify the timeslice if the scheduling algorithm is Round-Robin
 * 
 * @param cpu_id the target cpu we decide to put our process in
 */
static void schedule(unsigned int cpu_id)
{
    pcb_t *selected_proc = NULL;
    int quantum = -1;

    pthread_mutex_lock(&queue_mutex);
    if (!is_empty(rq)) {
        selected_proc = dequeue(rq);
    }
    pthread_mutex_unlock(&queue_mutex);

    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = selected_proc;
    
    if (selected_proc != NULL) {
        selected_proc->state = PROCESS_RUNNING;
    }
    pthread_mutex_unlock(&current_mutex);

    if (scheduler_algorithm == RR) {
        quantum = timeslice; 
    }

    context_switch(cpu_id, selected_proc, quantum);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3 for this problem
 * 
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled. This function should block until a process is added
 * to your ready queue.
 *
 * @param cpu_id the cpu that is waiting for process to come in
 */
extern void idle(unsigned int cpu_id)
{
   /* FIX ME */

    pthread_mutex_lock(&queue_mutex);

    
    while (is_empty(rq)) {
        pthread_cond_wait(&queue_not_empty, &queue_mutex);
    }

    
    pthread_mutex_unlock(&queue_mutex);

    
    schedule(cpu_id);

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
}

/** ------------------------Problem 2 & 3-----------------------------------
 * Checkout Section 4 and 5 for this problem
 * 
 * preempt() is the handler used in Round-robin, Preemptive Priority, and SRTF scheduling.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 * 
 * @param cpu_id the cpu in which we want to preempt process
 */
extern void preempt(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    pcb_t* process = current[cpu_id];

    if (process != NULL) {
        
        pthread_mutex_lock(&queue_mutex);
        process->state = PROCESS_READY;
        enqueue(rq, process);

        
    }

    if (process != NULL) {
        pthread_cond_signal(&queue_not_empty);
        pthread_mutex_unlock(&queue_mutex);

        current[cpu_id] = NULL;
    }
    pthread_mutex_unlock(&current_mutex);
    
    schedule(cpu_id);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3 for this problem
 * 
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * @param cpu_id the cpu that is yielded by the process
 */
extern void yield(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);

    pcb_t* process = current[cpu_id];
    if (process) {
        process->state = PROCESS_WAITING;
        current[cpu_id] = NULL;
    }

    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/**  ------------------------Problem 1A-----------------------------------
 * Checkout PDF Section 3
 * 
 * terminate() is the handler called by the simulator when a process completes.
 * 
 * @param cpu_id the cpu we want to terminate
 */
extern void terminate(unsigned int cpu_id)
{
    pthread_mutex_lock(&current_mutex);
    pcb_t* process = current[cpu_id];
    if (process) {
        process->state = PROCESS_TERMINATED;
        current[cpu_id] = NULL;
    }
    pthread_mutex_unlock(&current_mutex);
    schedule(cpu_id);
}

/**  ------------------------Problem 1A & 3---------------------------------
 * Checkout PDF Section 3 and 5 for this problem
 * 
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes. 
 * This method will also need to handle priority and SRTF preemption.
 * Look in section 5 of the PDF for more info on priority.
 * Look in section 6 of the PDF for more info on SRTF preemption.
 * 
 * We have provided a function get_current_time() which is prototyped in os-sim.h.
 * Look there for more information.
 * 
 * @param process the process that finishes I/O and is ready to run on CPU
 */
extern void wake_up(pcb_t *process)
{
    if (!process) {
        return;
    }

    
    
    

    // Set the enqueue time for priority aging
    

    // Enqueue the process in a thread-safe way
    pthread_mutex_lock(&queue_mutex);
    process->state = PROCESS_READY;
    process->enqueue_time = get_current_time();
    enqueue(rq, process);
    pthread_cond_signal(&queue_not_empty); // Wake up any idle CPUs
    pthread_mutex_unlock(&queue_mutex);

    int cpuPrempt = -50;

    pthread_mutex_lock(&current_mutex);
    int isIdle = 0;

    for (int i = 0; i < cpu_count; i++) {
        if (current[i]) {
            continue;
        } else {
            isIdle = 1;
            break;
        }
    }

    if (isIdle) {
        pthread_mutex_unlock(&current_mutex);
        return;
    }

    
    if (scheduler_algorithm == PA) {
        unsigned int present = get_current_time();
        // pthread_mutex_lock(&current_mutex);
        double newPrio = priority_with_age(present, process);
        double WORST = newPrio;

        for (int i = 0; i < cpu_count; i++) {
            pcb_t *running = current[i];
            if (running == NULL) continue;
            if (running->state != PROCESS_RUNNING) continue;
            
            
            double runningPrio = priority_with_age(present, running);
            if (runningPrio > WORST) {
                WORST = runningPrio;
                cpuPrempt = i;
            }
            
        }
    } else if (scheduler_algorithm == SRTF) {
        
        unsigned int maxRun = 0;
        for (unsigned int i = 0; i < cpu_count; i++) {
            pcb_t *running = current[i];
            if (running == NULL) continue;
            if (running->state != PROCESS_RUNNING) continue;
            unsigned int runningTime = (* running).total_time_remaining;

            if ((process->total_time_remaining < running->total_time_remaining) && (running->total_time_remaining > maxRun)) {
                maxRun = running->total_time_remaining;
                cpuPrempt = i;
            }
        }
    }
    pthread_mutex_unlock(&current_mutex);

    if (cpuPrempt != -50) {
        force_preempt(cpuPrempt);
    }
}

/**
 * main() simply parses command line arguments, then calls start_simulator().
 * Add support for -r, -p, and -s parameters. If no argument has been supplied, 
 * you should default to FCFS.
 * 
 * HINT:
 * Use the scheduler_algorithm variable (see student.h) in your scheduler to 
 * keep track of the scheduling algorithm you're using.
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p <age weight> | -s ]\n");
        exit(EXIT_FAILURE);
    }

    int firstcpu_count = atoi(argv[1]);

    if ((firstcpu_count >= 1 && firstcpu_count <= 16)) {
        cpu_count = (unsigned int) firstcpu_count;
    } else {
        fprintf(stderr,"CPU must be in range\n");
        exit(EXIT_FAILURE);

    }

    
    scheduler_algorithm = FCFS;
    timeslice = 0;
    age_weight = 0;

    optind = 2;
    int option;
    while ((option = getopt(argc, argv, "r:p:s")) != -1) {
        switch (option) {
            case 'r': {
                int quantum = atoi(optarg);
                if (quantum <= 0) {
                    fprintf(stderr, "Error: timeslice must be a positive integer.\n");
                    exit(EXIT_FAILURE);
                }
                timeslice = (unsigned int)quantum;
                scheduler_algorithm = RR;
                break;
            }
            case 'p': {
                age_weight = (unsigned int)atoi(optarg);
                scheduler_algorithm = PA;
                break;
            }
            case 's':
                scheduler_algorithm = SRTF;
                break;
            default:
                fprintf(stderr, "Usage: %s <# CPUs> [-r <timeslice> | -p <age_weight> | -s]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        fprintf(stderr, "Unexpected trailing argument: %s\n", argv[optind]);
        exit(EXIT_FAILURE);
    }

    
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_cond_init(&queue_not_empty, NULL);

    
    current = malloc(sizeof(pcb_t *) * cpu_count);
    assert(current != NULL);

    
    rq = malloc(sizeof(queue_t));
    assert(rq != NULL);
    rq->head = NULL;
    rq->tail = NULL;
    
    start_simulator(cpu_count);

    return 0;
}


// int main(int argc, char *argv[])
// {
//     /* FIX ME */
//     scheduler_algorithm = FCFS;
//     age_weight = 0;

//     if (argc != 2)
//     {
//         fprintf(stderr, "CS 2200 Project 4 -- Multithreaded OS Simulator\n"
//                         "Usage: ./os-sim <# CPUs> [ -r <time slice> | -p <age weight> | -s ]\n"
//                         "    Default : FCFS Scheduler\n"
//                         "         -r : Round-Robin Scheduler\n1\n"
//                         "         -p : Priority Aging Scheduler\n"
//                         "         -s : Shortest Remaining Time First\n");
//         return -1;
//     }


//     /* Parse the command line arguments */
//     cpu_count = strtoul(argv[1], NULL, 0);

//     /* Allocate the current[] array and its mutex */
//     current = malloc(sizeof(pcb_t *) * cpu_count);
//     assert(current != NULL);
//     pthread_mutex_init(&current_mutex, NULL);
//     pthread_mutex_init(&queue_mutex, NULL);
//     pthread_cond_init(&queue_not_empty, NULL);
//     rq = (queue_t *)malloc(sizeof(queue_t));
//     assert(rq != NULL);

//     /* Start the simulator in the library */
//     start_simulator(cpu_count);

//     return 0;
// }


#pragma GCC diagnostic pop
