// command to compile: gcc -o simulator8 simulator8.c coursework.c linkedlist.c -lpthread
// compile with debug flag: gcc -g -o simulator8 simulator8.c coursework.c linkedlist.c -lpthread
// Memory leak check: valgrind --leak-check=yes ./simulator8 4
// command to run: ./simulator8 <Simulators>     #i-e  [./simulator8 4]->for 4 simulators

// Extend the code above to handle multiple independent I/O devices. Every device should have its own
// queue structure and act independent of one another. For instance, read and write requests on different
// devices can happen with full parallelism, however, write requests on the same device are serialised.

#include <stdio.h>
#include "coursework.h"
#include "linkedlist.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

typedef struct
{
    int iProcessId;
    int NoofProcess;
    sem_t *Cocurrent_process;
    LinkedList *ReadyQueue;
    int *ReadyQueueSize;
    sem_t *ReadyQueueLock;
    sem_t *ReadyQueueEmpty;
    int *Processtable;
    sem_t *ProcessTableLock;
} ProcessGeneratorArg;

typedef struct
{
    int CPUNO;
    int NoofProcess;
    LinkedList *ReadyQueue;
    int *ReadyQueueSize;
    sem_t *ReadyQueueLock;
    sem_t *ReadyQueueEmpty;
    LinkedList *TerminationQueue;
    int *terminatedProcess;
    LinkedList *HardDriveQueue;
    sem_t *HardDriveQueueLock;
    sem_t *HardDriveQueueWaiting;
    sem_t *TerminationQueueLock;
    sem_t *Cocurrent_process;
    sem_t *terminationWait;

    LinkedList **IOQueue;
    sem_t *IOQueueLock;
    sem_t *IOQueueWaiting;
} ProcessSimulatorArg;

typedef struct
{
    LinkedList *terminationQueue;
    sem_t *TerminationQueueLock;
    sem_t *terminationWait;
    int *NoofProcess;
    int *terminatedProcess;
    int *Processtable;
    sem_t *ProcessTableLock;
} ProcessTerminatorArg;

typedef struct
{
    LinkedList *HardDriveQueue;
    sem_t *HardDriveQueueLock;
    sem_t *HardDriveQueueWaiting;
    LinkedList *ReadyQueue;
    int *ReadyQueueSize;
    sem_t *ReadyQueueLock;
    sem_t *ReadyQueueEmpty;
    sem_t *Cocurrent_process;
    int *NoofProcess;
    int *terminatedProcess;
    int *min;
    int *max;
    int *cur;
    int *direction;
} HardDriveSimulatorArg;

typedef struct
{
    LinkedList *IOblockProcess;
    sem_t *IOblockProcessLock;
    sem_t *IOblockProcessWaiting;
    sem_t *writingLock;
    LinkedList *ReadyQueue;
    int *ReadyQueueSize;
    sem_t *ReadyQueueLock;
    sem_t *ReadyQueueEmpty;
    int *NoofProcess;
    int *terminatedProcess;
} IOControllerArg;

// Get First available index in the process table
int GetFirstAvailableIndex(int *ProcessTable)
{
    for (int i = 0; i < SIZE_OF_PROCESS_TABLE; i++)
    {
        if (ProcessTable[i] == -1)
        {
            ProcessTable=NULL;
            return i;
        }
    }
    ProcessTable=NULL;
    return -1;
}

void ProcessGenerator(void *arg)
{
    ProcessGeneratorArg *pArg = (ProcessGeneratorArg *)arg;
    for (int i = 0; i < pArg->NoofProcess; i++)
    {
        sem_wait(pArg->Cocurrent_process);
        // Getting the first available index in the process table
        int pid = GetFirstAvailableIndex(pArg->Processtable);
        while(pid==-1){
            pid = GetFirstAvailableIndex(pArg->Processtable);
        }
        // Filling the process table with the process id
        sem_wait(pArg->ProcessTableLock);
        pArg->Processtable[pid] = pid;
        sem_post(pArg->ProcessTableLock);
        // Generating the process
        Process *pProcess = (Process *)generateProcess(pid);
        printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
        pArg->iProcessId++;
        sem_wait(pArg->ReadyQueueLock);
        addLast((void *)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize) = *(pArg->ReadyQueueSize) + 1;
        sem_post(pArg->ReadyQueueLock);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
        sem_post(pArg->ReadyQueueEmpty);
    }
    printf("GENERATOR: Finished\n");
    pthread_exit(NULL);
}

void ProcessSimulator(void *args)
{
    ProcessSimulatorArg *pArg = (ProcessSimulatorArg *)args;
    while (true)
    {
        if (*(pArg->ReadyQueueSize) <= 0)
        {
            sem_wait(pArg->ReadyQueueEmpty);
        }

        if (*(pArg->terminatedProcess) == pArg->NoofProcess)
        {
            sem_post(pArg->ReadyQueueEmpty);
            sem_post(pArg->HardDriveQueueWaiting);
            sem_post(pArg->terminationWait);
            for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
            {
                sem_post(&(pArg->IOQueueWaiting[i]));
            }
            break;
        }

        sem_wait(pArg->ReadyQueueLock);
        if (*(pArg->ReadyQueueSize) <= 0)
        {
            sem_post(pArg->ReadyQueueLock);
            continue;
        }
        *(pArg->ReadyQueueSize) = *(pArg->ReadyQueueSize) - 1;
        Process *pProcess = (Process *)removeFirst(pArg->ReadyQueue);
        sem_post(pArg->ReadyQueueLock);
        printf("QUEUE - REMOVED: [Queue = %s, Size = %d, PID = %d ]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
        if (pProcess->iState == TERMINATED)
        {
            sem_wait(pArg->TerminationQueueLock);
            addLast((void *)pProcess, pArg->TerminationQueue);
            *(pArg->terminatedProcess) = *(pArg->terminatedProcess) + 1;
            sem_post(pArg->TerminationQueueLock);
            long Turnaround = getDifferenceInMilliSeconds(pProcess->oTimeCreated, pProcess->oLastTimeRunning);
            long Response = getDifferenceInMilliSeconds(pProcess->oTimeCreated, pProcess->oFirstTimeRunning);
            printf("SIMULATOR - TERMINATED: [PID = %d, ResponseTime = %ld, TurnAroundTime = %ld ]\n", pProcess->iPID, Response, Turnaround);
            sem_post(pArg->terminationWait);
            sem_post(pArg->Cocurrent_process);
            if (*(pArg->terminatedProcess) == pArg->NoofProcess)
            {
                sem_post(pArg->ReadyQueueEmpty);
                sem_post(pArg->HardDriveQueueWaiting);
                sem_post(pArg->terminationWait);
                for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
                {
                    sem_post(&(pArg->IOQueueWaiting[i]));
                }
                break;
            }

        }
        else if (pProcess->iState == BLOCKED)
        {
            if (pProcess->iDeviceType == HARD_DRIVE)
            {
                if (pProcess->iDeviceType == READ)
                {
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = Hard Drive, Type = READ]\n", pProcess->iPID);
                }
                else
                {
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = Hard Drive, Type = WRITE]\n", pProcess->iPID);
                }
                sem_wait(pArg->HardDriveQueueLock);
                addLast((void *)pProcess, pArg->HardDriveQueue);
                sem_post(pArg->HardDriveQueueLock);
                sem_post(pArg->HardDriveQueueWaiting);
            }
            else
            {
                if (pProcess->iDeviceType == READ)
                {
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = READ]\n", pProcess->iPID, pProcess->iDeviceID);
                }
                else
                {
                    printf("SIMULATOR - I/O BLOCKED: [PID = %d, Device = %d, Type = WRITE]\n", pProcess->iPID, pProcess->iDeviceID);
                }
                // get value of semaphore
                int value;
                sem_getvalue(&(pArg->IOQueueWaiting[pProcess->iDeviceID]), &value);
                sem_wait(&(pArg->IOQueueLock[pProcess->iDeviceID]));
                addLast((void *)pProcess, pArg->IOQueue[pProcess->iDeviceID]);
                printf("QUEUE - ADDED: [Queue = READY_IO, Size = %d, PID = %d]\n", value + 1, pProcess->iPID);
                sem_post(&(pArg->IOQueueLock[pProcess->iDeviceID]));
                sem_post(&(pArg->IOQueueWaiting[pProcess->iDeviceID]));
            }
        }
        else
        {
            runPreemptiveProcess(pProcess, true, true);
            printf("SIMULATOR - CPU %d: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n", pArg->CPUNO, pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
            sem_wait(pArg->ReadyQueueLock);
            addLast((void *)pProcess, pArg->ReadyQueue);
            *(pArg->ReadyQueueSize) = *(pArg->ReadyQueueSize) + 1;
            sem_post(pArg->ReadyQueueLock);
            sem_post(pArg->ReadyQueueEmpty);
            printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
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
Process *removeBasedOnCondition(LinkedList *pList, int *min, int *max, int *cur, int *direction)
{
    Process *pProcess = NULL, *pProcess1;
    Element *pCurrent = pList->pHead;
    int count = 0;
    while (pCurrent != NULL)
    {
        count++;
        pCurrent = pCurrent->pNext;
    }
    if (count == 1)
    {
        pProcess = (Process *)pList->pHead->pData;
    }
    else if (*(direction) == 1)
    {
        int prevdeff = 32768;
        pCurrent = pList->pHead;
        while (pCurrent != NULL)
        {
            pProcess1 = (Process *)pCurrent->pData;
            if ((pProcess1->iTrack - *cur) < prevdeff && (pProcess1->iTrack - *cur) > 0)
            {
                prevdeff = pProcess1->iTrack - *cur;
                pProcess = (Process *)pCurrent->pData;
            }
            pCurrent = pCurrent->pNext;
        }
    }
    else
    {
        int prevdeff = 0;
        pCurrent = pList->pHead;
        while (pCurrent != NULL)
        {
            pProcess1 = (Process *)pCurrent->pData;
            if ((*cur - pProcess1->iTrack) > prevdeff)
            {
                prevdeff = *cur - pProcess1->iTrack;
                pProcess = pProcess1;
            }
            pCurrent = pCurrent->pNext;
        }
    }
    return pProcess;
}

// Implement Fist Come First Serve Scheduling Algorithm
void DiskController(void *arg)
{
    HardDriveSimulatorArg *pArg = (HardDriveSimulatorArg *)arg;
    while (true)
    {
        // wait for the hard drive queue to be non-empty
        sem_wait(pArg->HardDriveQueueWaiting);
        if (*(pArg->terminatedProcess) == *(pArg->NoofProcess))
        {
            break;
        }
        // remove the process from the hard drive queue
        Process *pProcess = removeBasedOnCondition(pArg->HardDriveQueue, pArg->min, pArg->max, pArg->cur, pArg->direction);
        if (pProcess == NULL)
        {
            *(pArg->direction) = *(pArg->direction) * -1;
            pProcess = removeBasedOnCondition(pArg->HardDriveQueue, pArg->min, pArg->max, pArg->cur, pArg->direction);
        }
        // remove the process from the hard drive queue
        sem_wait(pArg->HardDriveQueueLock);
        // *(pArg->cur) = pProcess->iTrack;
        pProcess = (Process *)removeData(pProcess, pArg->HardDriveQueue);
        sem_post(pArg->HardDriveQueueLock);

        // run the process for the hard drive burst time
        simulateIO(pProcess);
        printf("%s: reading track %d\n", pArg->HardDriveQueue->sName, pProcess->iTrack);
        // add the process to the ready queue
        sem_wait(pArg->ReadyQueueLock);
        pProcess->iState = READY;
        addLast((void *)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize) = *(pArg->ReadyQueueSize) + 1;
        sem_post(pArg->ReadyQueueLock);
        sem_post(pArg->ReadyQueueEmpty);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
    }
    printf("HARD DRIVE: Finished\n");
    pthread_exit(NULL);
}

// This function Implement the I/O Controlling machanism
void IOControllerFunc(void *args)
{
    IOControllerArg *pArg = (IOControllerArg *)args;
    while (true)
    {
        // wait for the printer queue to be non-empty
        sem_wait(pArg->IOblockProcessWaiting);
        if (*(pArg->terminatedProcess) == *(pArg->NoofProcess))
        {
            break;
        }
        int value;
        // remove the process from the printer queue
        sem_wait(pArg->IOblockProcessLock);
        Process *pProcess = (Process *)removeFirst(pArg->IOblockProcess);
        sem_getvalue(pArg->IOblockProcessWaiting, &value);
        sem_post(pArg->IOblockProcessLock);
        printf("QUEUE - REMOVED: [Queue = READY_IO, Size = %d, PID = %d]\n", value, pProcess->iPID);
        // run the process for the printer burst time
        if (pProcess->iRW == READ)
        {
            // Reading is simulated concurrently between the devices
            simulateIO(pProcess);
        }
        else
        {
            // Writing is serialized within multiple threads(I/O devices)
            sem_wait(pArg->writingLock);
            simulateIO(pProcess);
            sem_post(pArg->writingLock);
        }

        // add the process to the ready queue
        sem_wait(pArg->ReadyQueueLock);
        pProcess->iState = READY;
        addLast((void *)pProcess, pArg->ReadyQueue);
        *(pArg->ReadyQueueSize) = *(pArg->ReadyQueueSize) + 1;
        sem_post(pArg->ReadyQueueLock);
        sem_post(pArg->ReadyQueueEmpty);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", pArg->ReadyQueue->sName, *(pArg->ReadyQueueSize), pProcess->iPID);
    }
}

void ProcessTerminator(void *args)
{
    // remove the process from the termination queue
    ProcessTerminatorArg *pArg = (ProcessTerminatorArg *)args;
    while (true)
    {
        // wait for the termination queue to be non-empty
        sem_wait(pArg->terminationWait);
        // remove the process from the termination queue
        sem_wait(pArg->TerminationQueueLock);
        Process *pProcess = (Process *)removeFirst(pArg->terminationQueue);
        if (pProcess != NULL)
        {
            // clear the process table entry
            // clearing the entry from the table
            sem_wait(pArg->ProcessTableLock);
            pArg->Processtable[pProcess->iPID] = -1;
            sem_post(pArg->ProcessTableLock);
            // free of Process Memory
            printf("TERMINATION DAEMON - CLEARED: [#iTerminated = %d, PID = %d]\n", *(pArg->terminatedProcess), pProcess->iPID);
            free(pProcess);
        }
        sem_post(pArg->TerminationQueueLock);
        if (*(pArg->terminatedProcess) == *(pArg->NoofProcess))
        {
            break;
        }
    }
    printf("TERMINATION DAEMON: Finished\n");
    // clear the terminationQueue
    if(pArg->terminationQueue->pHead != NULL)
    {
        sem_wait(pArg->TerminationQueueLock);
        Process *p = (Process *)removeFirst(pArg->terminationQueue);
        while (p != NULL)
        {
            free(p);
            p = (Process *)removeFirst(pArg->terminationQueue);
        }
        sem_post(pArg->TerminationQueueLock);
    }
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: ./simulator8 <No of Simulators>\n");
        return -1;
    }

    // variables
    int NoofSimulators = atoi(argv[1]);
    int NumOfProcess = NUMBER_OF_PROCESSES, readyQueueSize = 0, terminatedProcess = 0;
    int min = -1, max = -1, cur = 0, direction = 1;
    sem_t concurentProcess, readyQueueLock, terminationQueueLock, terminationWait, ReadyQueueEmpty, processTableLock, diskControllerLock, diskControllerWait, writingLock;

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

    // Linked List for the Printer Queue
    LinkedList **oIOQueue = (LinkedList **)malloc(sizeof(LinkedList *) * NUMBER_OF_IO_DEVICES);
    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        oIOQueue[i] = (LinkedList *)malloc(sizeof(LinkedList));
        *oIOQueue[i] = (LinkedList)LINKED_LIST_INITIALIZER;
        // Number of IO devices
        sprintf(oIOQueue[i]->sName, "DEVICE %d", i);
    }

    // IOController varaibles
    sem_t* IOblockProcessLock= malloc(sizeof(sem_t)*NUMBER_OF_IO_DEVICES);
    sem_t* IOblockProcessWaiting=malloc(sizeof(sem_t)*NUMBER_OF_IO_DEVICES);
    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        sem_init(&IOblockProcessLock[i], 0, 1);
        sem_init(&IOblockProcessWaiting[i], 0, 0);
    }

    // declare the process table memory
    int* processTable = (int*)malloc(sizeof(int) * SIZE_OF_PROCESS_TABLE);
    int* processTableAdr= processTable;
    for (int i = 0; i < SIZE_OF_PROCESS_TABLE; i++)
    {
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
    ProcessSimulatorArg *pSimulatorArg = (ProcessSimulatorArg *)malloc(sizeof(ProcessSimulatorArg) * NoofSimulators);
    for (int i = 0; i < NoofSimulators; i++)
    {
        pSimulatorArg[i].NoofProcess = NumOfProcess;
        pSimulatorArg[i].ReadyQueue = oProcessQueue;
        pSimulatorArg[i].TerminationQueue = oTerminatedQueue;
        pSimulatorArg[i].ReadyQueueSize = &readyQueueSize;
        pSimulatorArg[i].ReadyQueueLock = &readyQueueLock;
        pSimulatorArg[i].terminatedProcess = &terminatedProcess;
        pSimulatorArg[i].CPUNO = i;
        pSimulatorArg[i].Cocurrent_process = &concurentProcess;
        pSimulatorArg[i].TerminationQueueLock = &terminationQueueLock;
        pSimulatorArg[i].terminationWait = &terminationWait;
        pSimulatorArg[i].ReadyQueueEmpty = &ReadyQueueEmpty;
        pSimulatorArg[i].HardDriveQueue = oHardDiskQueue;
        pSimulatorArg[i].HardDriveQueueLock = &diskControllerLock;
        pSimulatorArg[i].HardDriveQueueWaiting = &diskControllerWait;
        pSimulatorArg[i].IOQueue = oIOQueue;
        pSimulatorArg[i].IOQueueLock = IOblockProcessLock;
        pSimulatorArg[i].IOQueueWaiting = IOblockProcessWaiting;
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
    pDiskControllerArg->NoofProcess = &NumOfProcess;

    // Variable for lock and Scan Algorithm Implementation
    pDiskControllerArg->min = &min;
    pDiskControllerArg->max = &max;
    pDiskControllerArg->cur = &cur;
    pDiskControllerArg->direction = &direction;

    // IO Controller thread Arguments
    IOControllerArg *pIOControllerArg = (IOControllerArg *)malloc(sizeof(IOControllerArg) * NUMBER_OF_IO_DEVICES);
    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        pIOControllerArg[i].IOblockProcessLock = &IOblockProcessLock[i];
        pIOControllerArg[i].IOblockProcessWaiting = &IOblockProcessWaiting[i];
        pIOControllerArg[i].IOblockProcess = oIOQueue[i];

        pIOControllerArg[i].writingLock = &writingLock;
        pIOControllerArg[i].ReadyQueue = oProcessQueue;
        pIOControllerArg[i].ReadyQueueLock = &readyQueueLock;
        pIOControllerArg[i].ReadyQueueEmpty = &ReadyQueueEmpty;
        pIOControllerArg[i].ReadyQueueSize = &readyQueueSize;
        pIOControllerArg[i].NoofProcess = &NumOfProcess;
        pIOControllerArg[i].terminatedProcess = &terminatedProcess;
    }

    // Threads
    pthread_t pGenerator, pTerminator, diskController, IOController[NUMBER_OF_IO_DEVICES];

    // Arr of Simulator Threads
    pthread_t *pSimulator = (pthread_t *)malloc(sizeof(pthread_t) * NoofSimulators);

    // Create threads
    pthread_create(&pGenerator, NULL, (void *)ProcessGenerator, (void *)pGeneratorArg);
    for (int i = 0; i < NoofSimulators; i++)
    {
        pthread_create(&pSimulator[i], NULL, (void *)ProcessSimulator, (void *)&pSimulatorArg[i]);
    }
    pthread_create(&pTerminator, NULL, (void *)ProcessTerminator, (void *)pTerminatorArg);
    pthread_create(&diskController, NULL, (void *)DiskController, (void *)pDiskControllerArg);

    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        pthread_create(&IOController[i], NULL, (void *)IOControllerFunc, (void *)&pIOControllerArg[i]);
    }

    // Join threads
    pthread_join(pGenerator, NULL);

    for (int i = 0; i < NoofSimulators; i++)
    {
        pthread_join(pSimulator[i], NULL);
    }
    pthread_join(pTerminator, NULL);
    pthread_join(diskController, NULL);
    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        pthread_join(IOController[i], NULL);
    }

    // Destroy semaphores
    sem_destroy(&concurentProcess);
    sem_destroy(&readyQueueLock);
    sem_destroy(&terminationQueueLock);
    sem_destroy(&terminationWait);
    sem_destroy(&ReadyQueueEmpty);
    sem_destroy(&processTableLock);
    sem_destroy(&diskControllerLock);
    sem_destroy(&diskControllerWait);
    sem_destroy(&writingLock);
    for (int i = 0; i < NUMBER_OF_IO_DEVICES; i++)
    {
        sem_destroy(&IOblockProcessLock[i]);
        sem_destroy(&IOblockProcessWaiting[i]);
        free(oIOQueue[i]);
    }

    // IO Parameters free
    if(processTableAdr!=NULL){
        free(processTableAdr);
    }
    free(pIOControllerArg);
    free(IOblockProcessLock);
    free(IOblockProcessWaiting);
    // deallocation of the memory
    free(pGeneratorArg);
    free(pSimulatorArg);
    free(pTerminatorArg);
    free(pDiskControllerArg);
    free(pSimulator);
    free(oProcessQueue);
    free(oTerminatedQueue);
    free(oHardDiskQueue);
    free(oIOQueue);
    return 0;
}