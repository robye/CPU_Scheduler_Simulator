/*  Family Name: Ye
*
*   Given Name: Hongbing
*
*   Student Number: 212678405
*
*   CSE Login: hongbing
*/
#include <stdio.h>
#include <stdlib.h>
#include "sch-helpers.h"

process processes[MAX_PROCESSES];  		// a large structure array to hold all processes read from data file
int numberOfProcesses;              	// total number of processes
int clockTime;												// the system clock time to determine the entry for any burst
int processIndex;											// index for traversal of all processes
int timeQuantum1;                     // timeQuantums initialized in command-line argument
int timeQuantum2;
int totalCW;                          // total context switches
process *tempArray[MAX_PROCESSES];		// tempArray to handle the case where some actions happen at the same time spot
int tempArrayIndex;
process *cpus[NUMBER_OF_PROCESSORS];
/* ready-queue and device-queue */
process_queue ready_queue[NUMBER_OF_LEVELS];
process_queue device_queue;

/** at each time spot based on clockTime, we want to append process to tempArray, which
*	will be used for scheduling next running process.
*/
void nextUnitProcess(void) {
	// check current process index and match the time spot, and initialize the timeSlice for newly added process
	while(processIndex < numberOfProcesses && processes[processIndex].arrivalTime <= clockTime) {
		tempArray[tempArrayIndex] = &processes[processIndex];
    tempArray[tempArrayIndex]->quantumRemaining = timeQuantum1;
		tempArrayIndex++;
		processIndex++;
	}
}
/**	based on nextUnitProcess, we have the process(es) that are ready to be added to ready_queue
*	and for processes in the ready_queue, we find any available cpus to allocate the processes.
* 	Note: we need to sort the array by process id if they have the same scheduling criterion.
*/
void readyQtoCPU(void) {
	int i;
	/* reorder the temp array based on their pid, only happen when two arrival time is the same */
	qsort(tempArray, tempArrayIndex, sizeof(process *), compareByPid);
	/* enqueue the elements in the temp to ready_queue */
	for(i = 0; i < tempArrayIndex; i++) {
		tempArray[i]->currentQueue = 0;
		enqueueProcess(&ready_queue[0], tempArray[i]);
	}
	/** reset tempArrayIndex to the front of array
	*   since we need to restore process into it later on add io to ready_queue
	*/
	tempArrayIndex = 0;
	/* find some cpus to allocate them from ready_queue */
	for(i = 0; i < NUMBER_OF_PROCESSORS; i++) {
		if(cpus[i] == NULL) {
			/* allocate process from ready_queue(by order) to cpu(if applicable) */
			if(ready_queue[0].size != 0) {
				cpus[i] = ready_queue[0].front->data;
				dequeueProcess(&ready_queue[0]);
			}
			else if(ready_queue[1].size != 0) {
				cpus[i] = ready_queue[1].front->data;
				dequeueProcess(&ready_queue[1]);
			}
			else if(ready_queue[2].size != 0) {
				cpus[i] = ready_queue[2].front->data;
				dequeueProcess(&ready_queue[2]);
			}
		}
	}
}

/** from cpu to i/o
*	For cpu burst to I/O burst, loop over all cpus, if any one is allocated to a process
*	then we need to check if current cpu burst has been finished (step = length), if so,
*	we need to check if it is not the last cpu burst, enqueue it to waiting queue, otherwise
*	set the current clockTime to the process end time. Last, free the corresponding CPU.
*/
void cpuOut(void) {
	int i;
  int tempIndex = 0;
  process *temp[NUMBER_OF_PROCESSORS];
	for(i = 0; i < NUMBER_OF_PROCESSORS; i++) {
		// Note that initiallly there is no cpu running on any processes.
		if(cpus[i] != NULL) {
			/* check if current burst is finished */
			if(isBurstFinished(cpus[i])) {
				(cpus[i]->currentBurst)++;
				/* do another check for endtime or just a enqueue step */
				if((cpus[i])->currentBurst < (cpus[i])->numberOfBursts) {
					enqueueProcess(&device_queue, cpus[i]);
				}
				else {
					cpus[i]->endTime = clockTime;
				}
				/* free current working cpu */
				cpus[i] = NULL;
      }
      // If the current cpu burst is not finised but the time quantum is running out.

			else if(cpus[i]->quantumRemaining == 0) {
				temp[tempIndex] = cpus[i];
				tempIndex++;
				totalCW++;
				cpus[i] = NULL;
			}
			else if(cpus[i]->quantumRemaining != 0) {
					if(ready_queue[0].size != 0) {
						if(cpus[i]->currentQueue == 1 || cpus[i]->currentQueue == 2) {
							enqueueProcessHead(&ready_queue[cpus[i]->currentQueue], cpus[i]);
							cpus[i] = ready_queue[0].front->data;
							dequeueProcess(&ready_queue[0]);
							totalCW++;
						}
					}
					else if(ready_queue[1].size != 0 && cpus[i]->currentQueue == 2) {
						enqueueProcessHead(&ready_queue[2], cpus[i]);
						cpus[i] = ready_queue[1].front->data;
						dequeueProcess(&ready_queue[1]);
						totalCW++;
					}
			}
		}
	}
	/* like tempArray in i/o to ready_queue function call, there might be several processes that need
	*  to be added to the tail of ready_queue, so we need to sort them by the process id.
	*/
	qsort(temp, tempIndex, sizeof(process *), compareByPid);
	for(i = 0; i < tempIndex; i++) {
		if(temp[i]->bursts[temp[i]->currentBurst].step == timeQuantum1) {
			temp[i]->quantumRemaining = timeQuantum2;
			temp[i]->currentQueue = 1;
			enqueueProcess(&ready_queue[1], temp[i]);
		}
		else if(temp[i]->bursts[temp[i]->currentBurst].step == (timeQuantum1+timeQuantum2)) {
			temp[i]->currentQueue = 2;
			enqueueProcess(&ready_queue[2], temp[i]);
		}
	}

}
/**	from i/o to ready_queue.
*	Since we have device queue that hold numbers of processes with i/o bursting.
*	Now we want to detect any finished i/o burst process and enqueue it to tempArray again,
* since the device_queue is not ordered to dequeue, So before we add it back to ready_queue,
*	we need to sort the tempArray again and add it back to ready_queue
*/
void ioToReadyQ(void) {
	int i;
	int ioSize = device_queue.size;
	for(i = 0; i < ioSize; i++) {
		/* the out element of the queue */
		process *dequeuedProcess = (device_queue.front)->data;
		dequeueProcess(&device_queue);
		/** if i/o finish add it to tempArray, otherwise enqueue back to device_queue again and
		*   try next element in device queue
		*/

		if(isBurstFinished(dequeuedProcess)) {
			/* move forward for burst */
			(dequeuedProcess->currentBurst)++;
      /* since the i/o burst is finised, hence, we need to reset the timeQuantum1 for next cpu burst */
      dequeuedProcess->quantumRemaining = timeQuantum1;
			/* use temp array to temporarily store the dequeued processes */
			tempArray[tempArrayIndex] = dequeuedProcess;
			tempArrayIndex++;
		}
		/* enqueue it back to device_queue and try next one */
		else {
			enqueueProcess(&device_queue, dequeuedProcess);
		}
	}

}

/* utility function to check burst finishes or not: 1 for yes, 0 for no */
int isBurstFinished(process *p) {
	int isFinished = 0;
	int step = p->bursts[p->currentBurst].step;
	int length = p->bursts[p->currentBurst].length;
	if(step == length) {
		isFinished = 1;
	}
	return isFinished;
}

/** makeing progress: increase one step for each running process in the cpus
*					  increase one step for each i/o burst in device_queue
*					  increase one waiting time for each process in ready_queue
*
*/
void nextUnitTime(void) {
	int i,j;
	int ioSize = device_queue.size;
	/* increase step for processes in device queue */
	for(i = 0; i < ioSize; i++) {
		process *dequeuedProcess = device_queue.front->data;
		dequeueProcess(&device_queue);
		(dequeuedProcess->bursts[dequeuedProcess->currentBurst].step)++;
		enqueueProcess(&device_queue, dequeuedProcess);
	}

	/* increase waiting time for processes in ready_queue */
	for(i = 0; i < NUMBER_OF_LEVELS; i++) {
		for(j = 0; j < ready_queue[i].size; j++){
			process *dequeuedProcess = ready_queue[i].front->data;
			dequeueProcess(&ready_queue[i]);
			dequeuedProcess->waitingTime++;
			enqueueProcess(&ready_queue[i], dequeuedProcess);
		}
	}
	/* increase step in running cpus */
	for(i = 0; i < NUMBER_OF_PROCESSORS; i++) {
		if(cpus[i] != NULL) {
			((cpus[i]->bursts[cpus[i]->currentBurst]).step)++;
      /* decrease the cpu running time by 1 */
			// if(cpus[i]->bursts[cpus[i]->currentBurst].step <= (timeQuantum1 + timeQuantum2))
      cpus[i]->quantumRemaining--;
		}
	}

}

/* return 0 if no process is running */
int isAllIdle() {
	int i;
	int runningCPU = 0;
	for(i = 0; i < NUMBER_OF_PROCESSORS; i++) {
		if(cpus[i] != NULL) {
			runningCPU++;
		}
	}
	return runningCPU;
}

int main(int argc, char **argv) {

	/* initialzie some global and local variables */
	int status = 0;
	int lastPid, i,j;
	int totalUtilized = 0, totalWaiting = 0, totalTurnAround = 0;
	double avgWaiting, avgTurnAround, avgUtil;
	clockTime = 0;
	processIndex = 0;
  tempArrayIndex = 0;
  timeQuantum1 = atoi(argv[1]);
	timeQuantum2 = atoi(argv[2]);


	/* initialize the CPUs, set all cpus to idle status and no process is assigned to any one of them */
	for(j = 0; j < NUMBER_OF_PROCESSORS; j++) {
		cpus[j] = NULL;
	}

	/* initialize process queue to store processes */
	for(i = 0; i < NUMBER_OF_LEVELS; i++) {
			initializeProcessQueue(&ready_queue[i]);
	}

	initializeProcessQueue(&device_queue);

	/* read from the file and sort them in order for later on enqueue */
	while((status = readProcess(&processes[numberOfProcesses]))) {
		if(status == 1) {
			numberOfProcesses++;
		}
		if (numberOfProcesses > MAX_PROCESSES || numberOfProcesses == 0){
			error_invalid_number_of_processes(numberOfProcesses);
		}
	}
	qsort(processes, numberOfProcesses, sizeof(process), compareByArrival);

	/** the idea is: as we increase the clockTime from 0 to a very large number until some conditions,
	*	at every time spot, we need to allocate cpu to a process, enqueue process to device_queque or
	*	enqueue process back to ready_queue.
	*/
	while(1) {
		/* some actions at current time spot */
		/** first we want to call nextUnitProcess() to get a temporary array of process(es) which are
		*	arrange by arrival time.
		*	second do all the checks for all queue and process to determin where are they going to
		*	and what actions they have to do.
		*/
		nextUnitProcess();		// at each time spot, add matched process to be run to tempArray

		cpuOut(); 						// initially not executed since no cpus is running.
		ioToReadyQ();					// to check io queue if some processes should go back to ready_queue.
		readyQtoCPU();				// equeue all processes from sorted tempArray into ready_queue and allocate cpus for them.
		/* now we have to make progress, which is going to next unit of time */
		nextUnitTime();
		/* for each clockTime spot, we sum up the total */
		totalUtilized += isAllIdle();

		/**	now it is time to do some termination check
		*	exit the progress-making when:
		*	1: all cpus are idle status
		*	2: no next unit of processes to be added to tempArray
		*	3: the waiting queue is empty
		*/
		if((isAllIdle() == 0) && ((numberOfProcesses - processIndex) == 0) && (device_queue.size == 0) ) {
			break;
		}
		/* add one more unit time to next step */
		clockTime++;
	}
	/* calculation and display result */
	for(j = 0; j < numberOfProcesses; j++) {

		totalWaiting += processes[j].waitingTime;
		totalTurnAround += (processes[j].endTime - processes[j].arrivalTime);
		if(processes[j].endTime == clockTime) {

			lastPid = processes[j].pid;
		}
	}
	avgWaiting = totalWaiting / (double)numberOfProcesses;
	avgTurnAround = totalTurnAround / (double)numberOfProcesses;
	avgUtil = totalUtilized / (double)clockTime;

	printf("The average waiting time is:      %.2f\n"
					"The average turnaround time is:   %.2f\n"
					"The CPUs finished at:             %d\n"
          "The average cpu utilization is:   %.1f%%\n"
					"Total context switches:           %d\n"
					"The last process is:              %d\n", avgWaiting, avgTurnAround, clockTime, avgUtil*100, totalCW, lastPid);

}
