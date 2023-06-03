// commad to compile: gcc -o simulator7 simulator7.c coursework.c linkedlist.c -lpthread 
// compile with debug flag: gcc -g -o simulator7 simulator7.c coursework.c linkedlist.c -lpthread
// Memory leak check: valgrind --leak-check=yes ./simulator7 4
// command to run: ./simulator7 <Simulators>     #i-e  [./simulator7 4]->for 4 simulators

// Add a “device controller” and an appropriate queue structure to your code to simulate a single generic
// I/O device. Requests must be processed using a reader-writer approach with fairness (see above),
// allowing for parallel leading and serialised writing. Call the simulateIO() function to emulate I/O
// access times.

// Modify the code above to implement a LOOK-SCAN algorithm.



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
    int*   Processtable;
    sem_t* ProcessTableLock;
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
    LinkedList* HardDriveQueue;
    sem_t* HardDriveQueueLock;
    sem_t* HardDriveQueueWaiting;
    sem_t* TerminationQueueLock;
    sem_t* Cocurrent_process;
    sem_t* terminationWait;

    LinkedList* IOQueue;
    sem_t* IOQueueLock;
    sem_t* IOQueueWaiting;
}ProcessSimulatorArg;



typedef struct {
    LinkedList* terminationQueue;
    sem_t* TerminationQueueLock;
    sem_t* terminationWait;
    int  * NoofProcess;
    int  * terminatedProcess;
    int*   Processtable;
    sem_t* ProcessTableLock;
}ProcessTerminatorArg;

typedef struct {
    LinkedList* HardDriveQueue;
    sem_t* HardDriveQueueLock;
    sem_t* HardDriveQueueWaiting;
    LinkedList* ReadyQueue;
    int* ReadyQueueSize; 
    sem_t* ReadyQueueLock;
    sem_t* ReadyQueueEmpty; 
    sem_t* Cocurrent_process;
    int* NoofProcess;
    int* terminatedProcess;
    int* min; 
    int* max;
    int* cur;
    int* direction;
}HardDriveSimulatorArg;


typedef struct{
    LinkedList* IOblockProcess;
    sem_t* IOblockProcessLock;
    sem_t* IOblockProcessWaiting;
    sem_t* writingLock;
    LinkedList* ReadyQueue;
    int* ReadyQueueSize;
    sem_t* ReadyQueueLock;
    sem_t* ReadyQueueEmpty;
    int* NoofProcess;
    int* terminatedProcess;
}IOControllerArg;


// Get First available index in the process table
int GetFirstAvailableIndex(int* ProcessTable){
    for(int i=0; i<SIZE_OF_PROCESS_TABLE; i++){
        if(ProcessTable[i]==-1){
            return i;
        }
    }
    return -1;
}

void ProcessGenerator(void* arg)
{
    ProcessGeneratorArg* pArg = (ProcessGeneratorArg*) arg;
    for(int i=0; i<pArg->NoofProcess; i++){
        sem_wait(pArg->Cocurrent_process);
        // Getting the first available index in the process table
        int pid= GetFirstAvailableIndex(pArg->Processtable);
        // Filling the process table with the process id
        sem_wait(pArg->ProcessTableLock);
        pArg->Processtable[pid]=pid;
        sem_post(pArg->ProcessTableLock);
        // Generating the process
        Process* pProcess = (Process *)generateProcess(pid);
        printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
        pArg->iProcessId++;
        sem_wait(pArg->ReadyQueueLock);
        addLast((void*)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
        sem_post(pArg->ReadyQueueLock);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n",pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID); 
        sem_post(pArg->ReadyQueueEmpty);
    }
    printf("GENERATOR: Finished\n");
    pthread_exit(NULL);
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
            sem_post(pArg->HardDriveQueueWaiting);
            sem_post(pArg->terminationWait);
            sem_post(pArg->IOQueueWaiting);
            break;
        }

        sem_wait(pArg->ReadyQueueLock);
        if(*(pArg->ReadyQueueSize)<=0){
            sem_post(pArg->ReadyQueueLock);
            continue;
        }
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)-1;
        Process *pProcess = (Process *)removeFirst(pArg->ReadyQueue);
        sem_post(pArg->ReadyQueueLock);
        printf("QUEUE - REMOVED: [Queue = %s, Size = %d, PID = %d ]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
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
                sem_post(pArg->HardDriveQueueWaiting);
                sem_post(pArg->terminationWait);
                sem_post(pArg->IOQueueWaiting);
                break;
            }
        }
        else if(pProcess->iState==BLOCKED){
            if(pProcess->iDeviceType==HARD_DRIVE){
                if(pProcess->iDeviceType==READ){
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = Hard Drive, Type = READ]\n", pProcess->iPID);
                }else{
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = Hard Drive, Type = WRITE]\n", pProcess->iPID);
                }
                sem_wait(pArg->HardDriveQueueLock);
                addLast((void *)pProcess, pArg->HardDriveQueue);
                sem_post(pArg->HardDriveQueueLock);
                sem_post(pArg->HardDriveQueueWaiting);
            }
            else{
                if(pProcess->iDeviceType==READ){
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = 1, Type = READ]\n", pProcess->iPID);
                }else{
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = 1, Type = WRITE]\n", pProcess->iPID);
                }
                // get value of semaphore
                int value;
                sem_getvalue(pArg->IOQueueWaiting, &value);
                sem_wait(pArg->IOQueueLock);
                addLast((void *)pProcess, pArg->IOQueue);
                printf("QUEUE - ADDED: [Queue = READY_IO, Size = %d, PID = %d]\n",value+1, pProcess->iPID);
                sem_post(pArg->IOQueueLock);
                sem_post(pArg->IOQueueWaiting);
            }
        }
        else
        {
            runPreemptiveProcess(pProcess, true, true);
            printf("SIMULATOR - CPU %d: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n",pArg->CPUNO, pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
            sem_wait(pArg->ReadyQueueLock);
            addLast((void *)pProcess, pArg->ReadyQueue);
            *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
            sem_post(pArg->ReadyQueueLock);
            sem_post(pArg->ReadyQueueEmpty);
            printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n",  pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
        }
    }
    printf("SIMULATOR: Finished\n");
    pthread_exit(NULL);
}

// --------------------------------------------------------------------------
// This function is used to simulate the hard drive
// Implements the LOOK-SCAN algorithm
// It takes the hard drive queue as an argument
// --------------------------------------------------------------------------
Process* removeBasedOnCondition(LinkedList* pList, int* min, int* max, int* cur, int* direction){
    Process* pProcess=NULL, *pProcess1;
    Element * pCurrent = pList->pHead;
    int count=0;
    while(pCurrent != NULL) {
        count++;
        pCurrent = pCurrent->pNext;
    }
    if(count==1){
        pProcess = (Process*) pList->pHead->pData;
    }
    else if(*(direction)==1){
        int prevdeff=32768;
        pCurrent = pList->pHead;
        while(pCurrent != NULL) {
            pProcess1 =(Process *)pCurrent->pData;
            if((pProcess1->iTrack - *cur)<prevdeff && (pProcess1->iTrack - *cur)>0){
                prevdeff= pProcess1->iTrack - *cur;
                pProcess= (Process *)pCurrent->pData;
            }
            pCurrent = pCurrent->pNext;
        }
    }else{
        int prevdeff=0;
        pCurrent = pList->pHead;
        while(pCurrent != NULL) {
            pProcess1 =(Process *)pCurrent->pData;
            if((*cur - pProcess1->iTrack )>prevdeff){
                prevdeff=*cur-pProcess1->iTrack;
                pProcess= pProcess1;
            }
            pCurrent = pCurrent->pNext;
        }
    }
    return pProcess;
}

// Implement Fist Come First Serve Scheduling Algorithm 
void DiskController(void* arg){
    HardDriveSimulatorArg* pArg = (HardDriveSimulatorArg*)arg;
    while(true){
        // wait for the hard drive queue to be non-empty
        sem_wait(pArg->HardDriveQueueWaiting);
        if(*(pArg->terminatedProcess)==*(pArg->NoofProcess)){
            break;
        }
        // remove the process from the hard drive queue
        Process *pProcess = removeBasedOnCondition(pArg->HardDriveQueue, pArg->min, pArg->max, pArg->cur, pArg->direction);
        if(pProcess==NULL){
            *(pArg->direction)=*(pArg->direction)*-1;
            pProcess = removeBasedOnCondition(pArg->HardDriveQueue, pArg->min, pArg->max, pArg->cur, pArg->direction);
        }
        // remove the process from the hard drive queue
        sem_wait(pArg->HardDriveQueueLock);
        // *(pArg->cur) = pProcess->iTrack;
        pProcess= (Process*)removeData(pProcess, pArg->HardDriveQueue);
        sem_post(pArg->HardDriveQueueLock);

        // run the process for the hard drive burst time
        simulateIO(pProcess);
        printf("%s: reading track %d\n",pArg->HardDriveQueue->sName,  pProcess->iTrack);
        // add the process to the ready queue
        sem_wait(pArg->ReadyQueueLock);
        pProcess->iState=READY;
        addLast((void *)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
        sem_post(pArg->ReadyQueueLock);
        sem_post(pArg->ReadyQueueEmpty);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n",  pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
    }
    printf("HARD DRIVE: Finished\n");
    pthread_exit(NULL);
}


// This function Implement the I/O Controlling machanism
void IOControllerFunc(void* args){ 
    IOControllerArg* pArg = (IOControllerArg*)args;
    while(true){
        // wait for the printer queue to be non-empty
        sem_wait(pArg->IOblockProcessWaiting);
        if(*(pArg->terminatedProcess)==*(pArg->NoofProcess)){
            break;
        }
        int value;
        // remove the process from the printer queue
        sem_wait(pArg->IOblockProcessLock);
        Process *pProcess = (Process*)removeFirst(pArg->IOblockProcess);
        sem_getvalue(pArg->IOblockProcessWaiting, &value);
        sem_post(pArg->IOblockProcessLock);
        printf("QUEUE - REMOVED: [Queue = READY_IO, Size = %d, PID = %d]\n", value, pProcess->iPID);
        // run the process for the printer burst time
        if(pProcess->iRW==READ){
            // Reading is simulated concurrently between the devices
            simulateIO(pProcess);
        }
        else{
            // Writing is serialized within multiple threads(I/O devices)
            sem_wait(pArg->writingLock);
            simulateIO(pProcess);
            sem_post(pArg->writingLock);
        }

        // add the process to the ready queue
        sem_wait(pArg->ReadyQueueLock);
        pProcess->iState=READY;
        addLast((void *)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize)= *(pArg->ReadyQueueSize)+1;
        sem_post(pArg->ReadyQueueLock);
        sem_post(pArg->ReadyQueueEmpty);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n",  pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);    
    }
}

void ProcessTerminator(void* args){
    // remove the process from the termination queue
    ProcessTerminatorArg* pArg = (ProcessTerminatorArg*) args;
    while(true){
        // wait for the termination queue to be non-empty
        sem_wait(pArg->terminationWait);
        if(*(pArg->terminatedProcess)==*(pArg->NoofProcess)){
            break;
        }
        // remove the process from the termination queue
        sem_wait(pArg->TerminationQueueLock);
        Process *pProcess = (Process *)removeFirst(pArg->terminationQueue);
        if(pProcess!=NULL){
            // clear the process table entry
            // clearing the entry from the table
            sem_wait(pArg->ProcessTableLock);
            pArg->Processtable[pProcess->iPID]=-1;
            sem_post(pArg->ProcessTableLock);
            // free of Process Memory
            printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d]\n", *(pArg->terminatedProcess), pProcess->iPID);
            free(pProcess);
        }
        sem_post(pArg->TerminationQueueLock);
        if(*(pArg->terminatedProcess)==*(pArg->NoofProcess)){
            break;
        }
    }
    printf("TERMINATION DAEMON: Finished\n");
    // clear the terminationQueue
    sem_wait(pArg->TerminationQueueLock);
    Process* p =  (Process *)removeFirst(pArg->terminationQueue);
    while(p!=NULL){
        free(p);
        p = (Process *)removeFirst(pArg->terminationQueue);
    }
    sem_post(pArg->TerminationQueueLock);
    pthread_exit(NULL);
}




int main(int argc, char **argv)
{
    if(argc!=2){
        printf("Usage: ./simulator7 <No of Simulators>\n");
        return -1;
    }

    // variables
    int NoofSimulators = atoi(argv[1]);
    int NumOfProcess = 10, readyQueueSize=0, terminatedProcess=0, *processTable;
    int min=-1, max=-1, cur=0, direction=1;
    sem_t concurentProcess, readyQueueLock, terminationQueueLock, terminationWait, ReadyQueueEmpty, processTableLock, diskControllerLock, diskControllerWait, writingLock;

    // IOController varaibles
    LinkedList* oIOQueue=(LinkedList *)malloc(sizeof(LinkedList));
    *oIOQueue= (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oIOQueue->sName, "1");
    sem_t IOblockProcessLock, IOblockProcessWaiting;



    // Semaphore Initialization
    sem_init(&concurentProcess, 0, MAX_CONCURRENT_PROCESSES);
    sem_init(&readyQueueLock, 0, 1);
    sem_init(&terminationQueueLock, 0, 1);
    sem_init(&terminationWait, 0, 0);
    sem_init(&ReadyQueueEmpty, 0, 0);
    sem_init(&processTableLock, 0, 1);
    sem_init(&diskControllerLock, 0, 1);
    sem_init(&diskControllerWait, 0, 0);
    // IO process variables
    sem_init(&writingLock, 0, 1);
    sem_init(&IOblockProcessLock, 0, 1);
    sem_init(&IOblockProcessWaiting, 0, 0);

    LinkedList *oProcessQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oProcessQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oProcessQueue->sName, "READY");

    LinkedList *oTerminatedQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oTerminatedQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oTerminatedQueue->sName, "TERMINATED");

    // Linked List for the Hard Drive Queue
    LinkedList *oHardDiskQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oHardDiskQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oHardDiskQueue->sName, "HARD DRIVE");
    
    // declare the process table memory
    processTable = (int *)malloc(sizeof(int)*MAX_CONCURRENT_PROCESSES);
    for(int i=0; i<MAX_CONCURRENT_PROCESSES; i++){
        processTable[i] = -1;
    }

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
    pGeneratorArg->Processtable = processTable;
    pGeneratorArg->ProcessTableLock = &processTableLock;


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
        pSimulatorArg[i].HardDriveQueue = oHardDiskQueue;
        pSimulatorArg[i].HardDriveQueueLock = &diskControllerLock;
        pSimulatorArg[i].HardDriveQueueWaiting = &diskControllerWait;
        pSimulatorArg[i].IOQueue = oIOQueue;
        pSimulatorArg[i].IOQueueLock = &IOblockProcessLock;
        pSimulatorArg[i].IOQueueWaiting = &IOblockProcessWaiting;
    }

    // Process Terminator thread Arguments
    ProcessTerminatorArg *pTerminatorArg = (ProcessTerminatorArg *)malloc(sizeof(ProcessTerminatorArg));
    pTerminatorArg->terminationQueue = oTerminatedQueue;
    pTerminatorArg->terminatedProcess = &terminatedProcess;
    pTerminatorArg->TerminationQueueLock = &terminationQueueLock;
    pTerminatorArg->terminationWait = &terminationWait;
    pTerminatorArg->NoofProcess = &NumOfProcess;
    pTerminatorArg->Processtable = processTable;
    pTerminatorArg->ProcessTableLock = &processTableLock;


    // Disk Controller thread Arguments
    HardDriveSimulatorArg *pDiskControllerArg = (HardDriveSimulatorArg *)malloc(sizeof(HardDriveSimulatorArg));
    pDiskControllerArg->HardDriveQueue = oHardDiskQueue;
    pDiskControllerArg->HardDriveQueueLock = &diskControllerLock;
    pDiskControllerArg->ReadyQueue = oProcessQueue;
    pDiskControllerArg->ReadyQueueLock = &readyQueueLock;
    pDiskControllerArg->ReadyQueueEmpty = &ReadyQueueEmpty;
    pDiskControllerArg->ReadyQueueSize = &readyQueueSize;
    pDiskControllerArg->Cocurrent_process = &concurentProcess;
    pDiskControllerArg->HardDriveQueueWaiting = &diskControllerWait;
    pDiskControllerArg->terminatedProcess = &terminatedProcess;
    pDiskControllerArg->NoofProcess= &NumOfProcess;
   
    // Variable for lock and Scan Algorithm Implementation
    pDiskControllerArg->min=&min;
    pDiskControllerArg->max=&max;
    pDiskControllerArg->cur=&cur;
    pDiskControllerArg->direction=&direction;




    // IO Controller thread Arguments
    IOControllerArg *pIOControllerArg = (IOControllerArg *)malloc(sizeof(IOControllerArg));
    pIOControllerArg->IOblockProcessLock = &IOblockProcessLock;
    pIOControllerArg->IOblockProcessWaiting = &IOblockProcessWaiting;
    pIOControllerArg->IOblockProcess = oIOQueue;
    pIOControllerArg->writingLock = &writingLock;
    pIOControllerArg->ReadyQueue = oProcessQueue;
    pIOControllerArg->ReadyQueueLock = &readyQueueLock;
    pIOControllerArg->ReadyQueueEmpty = &ReadyQueueEmpty;
    pIOControllerArg->ReadyQueueSize = &readyQueueSize;
    pIOControllerArg->NoofProcess = &NumOfProcess;
    pIOControllerArg->terminatedProcess = &terminatedProcess;

    // Threads
    pthread_t pGenerator, pTerminator, diskController, IOController;

    // Arr of Simulator Threads
    pthread_t* pSimulator= (pthread_t *)malloc(sizeof(pthread_t)*NoofSimulators);
    
    // Create threads
    pthread_create(&pGenerator, NULL, (void *)ProcessGenerator, (void *)pGeneratorArg);
    for(int i=0; i<NoofSimulators; i++){
        pthread_create(&pSimulator[i], NULL, (void *)ProcessSimulator, (void *)&pSimulatorArg[i]);
    }
    pthread_create(&pTerminator, NULL, (void *)ProcessTerminator, (void *)pTerminatorArg);
    pthread_create(&diskController, NULL, (void *)DiskController, (void *)pDiskControllerArg);
    pthread_create(&IOController, NULL, (void *)IOControllerFunc, (void *)pIOControllerArg);


    // Join threads
    pthread_join(pGenerator, NULL);

    for(int i=0; i<NoofSimulators; i++){
        pthread_join(pSimulator[i], NULL);
    }
    pthread_join(pTerminator, NULL);
    pthread_join(diskController, NULL);
    pthread_join(IOController, NULL);

    // Destroy semaphores
    sem_destroy(&concurentProcess);
    sem_destroy(&readyQueueLock);
    sem_destroy(&terminationQueueLock);
    sem_destroy(&terminationWait);
    sem_destroy(&ReadyQueueEmpty);
    sem_destroy(&processTableLock);
    sem_destroy(&diskControllerLock);
    sem_destroy(&diskControllerWait);
    sem_destroy(&IOblockProcessLock);
    sem_destroy(&IOblockProcessWaiting);

    // deallocation of the memory
    free(pGeneratorArg);
    free(pSimulatorArg);
    free(pTerminatorArg);
    free(pDiskControllerArg);
    free(pIOControllerArg);
    free(pSimulator);
    free(oProcessQueue);
    free(oTerminatedQueue);
    free(oHardDiskQueue);
    free(processTable);
    free(oIOQueue);
    return 0;
}