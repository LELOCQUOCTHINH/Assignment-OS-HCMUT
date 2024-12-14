#include <stdio.h>
#include <stdlib.h>
#include "queue.h"

int empty(struct queue_t * q) {
        if (q == NULL) return 1;
	return (q->size == 0);
}

void enqueue(struct queue_t * q, struct pcb_t * proc) {
        /* TODO: put a new process to queue [q] */
        // if(q->size == 0){
        //         q->proc[0] = proc;
        //         q->size++;
        //         return ;
        // }
        // q->size++;
        // q->proc[q->size-1] = proc;
        if (q == NULL)  return;//Check for NULL Multilevel queue and the priority of process is invalid
        if (q->size >= MAX_QUEUE_SIZE)  return;//Check if priority queue is full

        int idx = q->size++;
        for (;idx - 1 >= 0 && q->proc[idx - 1]->priority > proc->priority;
                idx--)
        q->proc[idx] = q->proc[idx - 1] ;//Add a new process at the end of the queue has the same priority value, then increase the size
        q->proc[idx] = proc;
}

struct pcb_t * dequeue(struct queue_t * q) {
        /* TODO: return a pcb whose prioprity is the highest
                * in the queue [q] and remember to remove it from q
                * */
        // struct pcb_t* proc = NULL;
        // if(q->proc[0] != NULL){
        //         proc = q->proc[0];
        //         for(int i = 0; i < q->size - 1; i++){
        //                 q->proc[i] = q->proc[i+1];
        //         }
        //         q->size --;
        // }
        // return proc;
        if (q == NULL)          return NULL;
        struct pcb_t * proc = NULL;
#ifdef MLQ_SCHED
        for (int level = 0; level < MAX_PRIO; level++) {
            /*
            If queue at that level is empty or it's out of slot, then move to next level immediately
            Else take the first process and move the rest processes of queue to the front by one index
            */
            if (q[level].size == 0 || q[level].slot == 0)         continue;
            
            proc = q[level].proc[0];
            q[level].slot--;

            for (int idx = 1; idx < q[level].size; idx++) {
                q[level].proc[idx - 1] = q[level].proc[idx];
                if (idx == q[level].size - 1)
                    q[level].proc[idx] = NULL;
            }

            q[level].size--;

            if (level == MAX_PRIO - 1) {
                for (int queue_level = 0; queue_level < MAX_PRIO; queue_level++)
                    q[queue_level].slot = MAX_PRIO - queue_level;
            }
            break;
        }
#else
        if (q[0].size == 0)  return NULL;
        proc = q[0].proc[0];
        q[0].proc[0] = NULL;

        for (int idx = 1; idx < q[0].size; idx++)
            q[0].proc[idx - 1] = q[0].proc[idx];

        q[0].size--;
#endif
	return proc;
}

