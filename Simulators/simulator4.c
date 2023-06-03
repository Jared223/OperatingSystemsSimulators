// commad to compile: gcc -o simulator4 simulator4.c coursework.c linkedlist.c -lpthread 
// command to run: ./simulator4 <Simulators>     #i-e  [./simulator4 4]->for 4 simulators

#include <stdio.h>
#include "coursework.h"
#include "linkedlist.h"
#include <string.h>
#include <stdlib.h>
#include<pthread.h>
#include<semaphore.h>



typedef struct {
    int iProcessId;
    int NoofProcess; 
    sem_t* Cocurrent_process;
    LinkedList* ReadyQueue;
    int* ReadyQueueSize; 
    sem_t* ReadyQueueLock;
    sem_t* ReadyQueueEmpty;
}ProcessGeneratorArg;


typedef struct {
    int CPUNO;
    int NoofProcess;
    LinkedList* ReadyQueue;
    int* ReadyQueueSize; 
    sem_t* ReadyQueueLock;
    sem_t* ReadyQueueEmpty; 
    LinkedList* TerminationQueue;
    int* terminatedProcess;
    sem_t* TerminationQueueLock;
    sem_t* Cocurrent_process;
    sem_t* terminationWait;
}ProcessSimulatorArg;



typedef struct {
    LinkedList* terminationQueue;
    sem_t* TerminationQueueLock;
    sem_t* terminationWait;
    int* NoofProcess;
    int*terminatedProcess;
}ProcessTerminatorArg;


void ProcessGenerator(void* arg)
{
    ProcessGeneratorArg* pArg = (ProcessGeneratorArg*) arg;
    for(int i=0; i<pArg->NoofProcess; i++){
        sem_wait(pArg->Cocurrent_process);
        Process* pProcess = (Process *)generateProcess(pArg->iProcessId);
        printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
        pArg->iProcessId++;
        sem_wait(pArg->ReadyQueueLock);
        addLast((void*)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
        sem_post(pArg->ReadyQueueLock);
        sem_post(pArg->ReadyQueueEmpty);
    }
}

void ProcessSimulator(void* args){
    ProcessSimulatorArg* pArg = (ProcessSimulatorArg*) args;
    while (true)
    {
        if(*(pArg->ReadyQueueSize)<=0){
            sem_wait(pArg->ReadyQueueEmpty);
        }

        if(*(pArg->terminatedProcess)==pArg->NoofProcess){
            sem_post(pArg->ReadyQueueEmpty);
            break;
        }

        sem_wait(pArg->ReadyQueueLock);
        if(*(pArg->ReadyQueueSize)<=0){
            sem_post(pArg->ReadyQueueLock);
            continue;
        }
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)-1;
        sem_post(pArg->ReadyQueueLock);


        sem_wait(pArg->ReadyQueueLock);
        Process *pProcess = (Process *)removeFirst(pArg->ReadyQueue);
        sem_post(pArg->ReadyQueueLock);
        printf("QUEUE - REMOVED: [Queue = %s, Size = %d, PID = %d]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
        runPreemptiveProcess(pProcess, false, false);
        printf("SIMULATOR - CPU %d: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",pArg->CPUNO, pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
        
        if (pProcess->iState == TERMINATED)
        {
            sem_post(pArg->Cocurrent_process);
            sem_wait(pArg->TerminationQueueLock);
            addLast((void *)pProcess, pArg->TerminationQueue);
            *(pArg->terminatedProcess)= *(pArg->terminatedProcess)+1;
            sem_post(pArg->TerminationQueueLock);
            sem_post(pArg->terminationWait);

            if(*(pArg->terminatedProcess)==pArg->NoofProcess){
                sem_post(pArg->ReadyQueueEmpty);
                break;
            }
        }
        else
        {
            sem_wait(pArg->ReadyQueueLock);
            addLast((void *)pProcess, pArg->ReadyQueue);
            *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
            sem_post(pArg->ReadyQueueLock);
            sem_post(pArg->ReadyQueueEmpty);
            printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n",  pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
        }
    }
}


void ProcessTerminator(void* args){
    // remove the process from the termination queue
    ProcessTerminatorArg* pArg = (ProcessTerminatorArg*) args;
    while(true){
        // wait for the termination queue to be non-empty
        sem_wait(pArg->terminationWait);
        // remove the process from the termination queue
        sem_wait(pArg->TerminationQueueLock);
        Process *pProcess = (Process *)removeFirst(pArg->terminationQueue);
        free(pProcess);
        sem_post(pArg->TerminationQueueLock);
        sem_post(pArg->terminationWait);
        // check if all the processes have been terminated, if so, break the loop
        if(*(pArg->terminatedProcess)==*(pArg->NoofProcess)){
            break;
        }
    }
}



int main(int argc, char **argv)
{
    if(argc!=2){
        printf("Usage: ./simulator4 <No of Simulators>\n");
        return -1;
    }

    int NoofSimulators = atoi(argv[1]);
    int NumOfProcess = 10, readyQueueSize=0, terminatedProcess=0;
    sem_t concurentProcess, readyQueueLock, terminationQueueLock, terminationWait, ReadyQueueEmpty;
    sem_init(&concurentProcess, 0, MAX_CONCURRENT_PROCESSES);
    sem_init(&readyQueueLock, 0, 1);
    sem_init(&terminationQueueLock, 0, 1);
    sem_init(&terminationWait, 0, 0);
    sem_init(&ReadyQueueEmpty, 0, 0);

    LinkedList *oProcessQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oProcessQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oProcessQueue->sName, "READY");

    LinkedList *oTerminatedQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oTerminatedQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oTerminatedQueue->sName, "TERMINATED");


    // Intialize threads Argiments
    // Process Generator thread Arguments
    ProcessGeneratorArg *pGeneratorArg = (ProcessGeneratorArg *)malloc(sizeof(ProcessGeneratorArg));
    pGeneratorArg->iProcessId = 0;
    pGeneratorArg->NoofProcess = NumOfProcess;
    pGeneratorArg->ReadyQueue = oProcessQueue;
    pGeneratorArg->ReadyQueueSize = &readyQueueSize;
    pGeneratorArg->Cocurrent_process = &concurentProcess;
    pGeneratorArg->ReadyQueueLock = &readyQueueLock;
    pGeneratorArg->ReadyQueueEmpty = &ReadyQueueEmpty;


    // Array of ProcessSimulatorArg
    ProcessSimulatorArg *pSimulatorArg = (ProcessSimulatorArg *)malloc(sizeof(ProcessSimulatorArg)*NoofSimulators);
    for(int i=0; i<NoofSimulators; i++){
        pSimulatorArg[i].NoofProcess = NumOfProcess;
        pSimulatorArg[i].ReadyQueue = oProcessQueue;
        pSimulatorArg[i].TerminationQueue= oTerminatedQueue;
        pSimulatorArg[i].ReadyQueueSize = &readyQueueSize;
        pSimulatorArg[i].ReadyQueueLock = &readyQueueLock;
        pSimulatorArg[i].terminatedProcess = &terminatedProcess;
        pSimulatorArg[i].CPUNO=i;
        pSimulatorArg[i].Cocurrent_process = &concurentProcess;
        pSimulatorArg[i].TerminationQueueLock = &terminationQueueLock;
        pSimulatorArg[i].terminationWait = &terminationWait;
        pSimulatorArg[i].ReadyQueueEmpty = &ReadyQueueEmpty;
    }

    // Process Terminator thread Arguments
    ProcessTerminatorArg *pTerminatorArg = (ProcessTerminatorArg *)malloc(sizeof(ProcessTerminatorArg));
    pTerminatorArg->terminationQueue = oTerminatedQueue;
    pTerminatorArg->terminatedProcess = &terminatedProcess;
    pTerminatorArg->TerminationQueueLock = &terminationQueueLock;
    pTerminatorArg->terminationWait = &terminationWait;
    pTerminatorArg->NoofProcess = &NumOfProcess;


    // Threads
    pthread_t pGenerator, pTerminator;

    // Arr of Simulator Threads
    pthread_t* pSimulator= (pthread_t *)malloc(sizeof(pthread_t)*NoofSimulators);
    
    // Create threads
    pthread_create(&pGenerator, NULL, (void *)ProcessGenerator, (void *)pGeneratorArg);
    for(int i=0; i<NoofSimulators; i++){
        pthread_create(&pSimulator[i], NULL, (void *)ProcessSimulator, (void *)&pSimulatorArg[i]);
    }
    pthread_create(&pTerminator, NULL, (void *)ProcessTerminator, (void *)pTerminatorArg);

    // Join threads
    pthread_join(pGenerator, NULL);
    for(int i=0; i<NoofSimulators; i++){
        pthread_join(pSimulator[i], NULL);
    }
    pthread_join(pTerminator, NULL);

    // Destroy semaphores
    sem_destroy(&concurentProcess);
    sem_destroy(&readyQueueLock);
    sem_destroy(&terminationQueueLock);
    sem_destroy(&terminationWait);
    sem_destroy(&ReadyQueueEmpty);

    // deallocation of the memory
    free(pGeneratorArg);
    free(pSimulatorArg);
    free(pTerminatorArg);
    free(oProcessQueue);
    free(oTerminatedQueue);
    return 0;
}
