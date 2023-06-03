#include<stdio.h>
#include"coursework.h"

// commad to compile: gcc -o simulator1 simulator1.c coursework.c linkedlist.c -lpthread


int main(int argc, char**argv){
    Process *pProcess = generateProcess(1);
    
    printf("GENERATOR - ADMITTED: [PID = %d, Initial BurstTime = %d, Remaining BurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
    do{
        runPreemptiveProcess(pProcess, false, false);
        printf("SIMULATOR - CPU 0: [PID = %d, InitialBurstTime = %d, RemainingBurstTime = %d]\n", pProcess->iPID, pProcess->iBurstTime, pProcess->iRemainingBurstTime);
    }while(pProcess->iState != TERMINATED);
    
    return 0;
}
