
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;
int slot_cpu_can_use[MAX_PRIO];
// int prioSlot[MAX_PRIO];

int MarkedPrior = 0 ;
int flag = 1;
int count = 0;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
#endif

int queue_empty(void) {
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void) {
#ifdef MLQ_SCHED
    int i ;

	for (i = 0; i < MAX_PRIO; i++) {
		mlq_ready_queue[i].size = 0;
		// prioSlot[i] = MAX_PRIO - i;
		// slot_cpu_can_use[i] = MAX_PRIO - i;
		mlq_ready_queue[i].slot = MAX_PRIO - i;
	}
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void) {
	struct pcb_t * proc = NULL;
	// /*TODO: get a process from PRIORITY [ready_queue].
	//  * Remember to use lock to protect the queue.
	//  * */
	// pthread_mutex_lock(&queue_lock);
	// label: 
	// if(flag){
	// 	for(; MarkedPrior < MAX_PRIO; MarkedPrior++){
	// 		if(mlq_ready_queue[MarkedPrior].size != 0 && prioSlot[MarkedPrior] < MAX_PRIO - MarkedPrior){
	// 			proc = dequeue(&mlq_ready_queue[MarkedPrior]);
	// 			prioSlot[MarkedPrior]++;
	// 			flag = 0;
	// 			count = 0;
	// 			pthread_mutex_unlock(&queue_lock);
	// 			return proc;
	// 		}
	// 		count++;
	// 	}
	// 	for(int j = 0; j < MAX_PRIO; j++){
	// 		prioSlot[j] = 0;
	// 	}
	// 	MarkedPrior = 0;
	// 	flag = 1;
	// 	if(count == MAX_PRIO){
	// 		pthread_mutex_unlock(&queue_lock);
	// 		return proc;
	// 	}else{
	// 		count = 0;
	// 	}
	// 	goto label;
	// }else{
	// 	if(prioSlot[MarkedPrior] < MAX_PRIO - MarkedPrior && mlq_ready_queue[MarkedPrior].size != 0 ){
	// 		printf("Inside priorSlot, second if block\n");
	// 		proc = dequeue(&mlq_ready_queue[MarkedPrior]);
	// 		prioSlot[MarkedPrior]++;
	// 		pthread_mutex_unlock(&queue_lock);
	// 		return proc;
	// 	}else{
	// 		flag = 1;
	// 		goto label;
	// 	}
	// }
	// pthread_mutex_unlock(&queue_lock);
	// return proc;	
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
    proc = dequeue(mlq_ready_queue);
    pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_mlq_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) {
	if(proc->prio < 0 || proc->prio >= MAX_PRIO || proc->priority < 0) return;
	pthread_mutex_lock(&queue_lock);
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
	pthread_mutex_lock(&queue_lock);
    proc = dequeue(&ready_queue);
    pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


