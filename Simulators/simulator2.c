// command to compile: gcc -o simulator2 simulator2.c coursework.c linkedlist.c -lpthread
#include <stdio.h>
#include "coursework.h"
#include "linkedlist.h"
#include <string.h>
#include <stdlib.h>

void GenerateProcess(int NoofProcess, LinkedList *pList,int* size)
{
    for (int i = 0; i < NoofProcess; i++)
    {
        Process *pProcess = generateProcess(i);
        printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
        addLast((void *)pProcess, pList);
        printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", pList->sName, *size, pProcess->iPID);
        (*size)= *(size)+1;
    }
}

int main(int argc, char **argv)
{
    int NumOfProcess = 10, TERMINATEDSize = 0, size = 0;
    LinkedList *oProcessQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oProcessQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oProcessQueue->sName, "READY");

    LinkedList *oTerminatedQueue = (LinkedList *)malloc(sizeof(LinkedList));
    *oTerminatedQueue = (LinkedList)LINKED_LIST_INITIALIZER;
    strcpy(oTerminatedQueue->sName, "TERMINATED");

    // generate processes
    GenerateProcess(NumOfProcess, oProcessQueue,&size);


    while (true)
    {
        Process *pProcess = (Process *)removeFirst(oProcessQueue);
        size--;
        printf("QUEUE - REMOVED: [Queue = %s, Size = %d, PID = %d]\n", oProcessQueue->sName, size, pProcess->iPID);
        runPreemptiveProcess(pProcess, false, false);
        if (pProcess->iState == TERMINATED)
        {
            TERMINATEDSize++;
            addLast((void *)pProcess, oTerminatedQueue);
            if(TERMINATEDSize==NumOfProcess){
                break;
            }
        }
        else
        {
            printf("SIMULATOR - CPU 0: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
            size++;
            printf("QUEUE - ADDED: [Queue = %s, Size = %d, PID = %d]\n", oProcessQueue->sName, size, pProcess->iPID);
            addLast((void *)pProcess, oProcessQueue);
        }
    }

    // remove process
    while(oTerminatedQueue->pHead != NULL){
        Process *pProcess = (Process *)removeFirst(oTerminatedQueue);
        free(pProcess);
        TERMINATEDSize--;
    }

    return 0;
}
