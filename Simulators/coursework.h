/**************************************************************************************************************************
			- DO NOT CHANGE THE CONTENTS OF THIS FILE FOR YOUR COURSEWORK (APART FROM THE VALUES FOR THE CONSTANTS)
			- ONLY WORK WITH THE OFFICIAL VERSION
**************************************************************************************************************************/

#include <sys/time.h>

typedef enum {false, true} bool;

#define NUMBER_OF_PROCESSES 1000 // number of processes to simulate
#define MAX_CONCURRENT_PROCESSES 32  // max number of processes in the system at any one point in time
#define SIZE_OF_PROCESS_TABLE MAX_CONCURRENT_PROCESSES // number of processes in the process table
#define NUMBER_OF_IO_DEVICES 2 // number of generic IO devices
#define NUMBER_OF_CPUS 2 // number of CPU emulators
#define NUMBER_OF_READERS 5 // number of readers per generic IO device (not hard drive)
#define NUMBER_OF_WRITERS 2 // number of writers per generic IO device (not hard drive)

#define TIME_SLICE 5 // Duration of the time slice for the round robin algorithm
#define MAX_BURST_TIME 50 // Max CPU time that any one process requires
// process states
#define READY 1
#define RUNNING 2
#define BLOCKED 3
#define TERMINATED 4

#define READ 0 // request is for reading
#define WRITE 1  // request is for writing
#define READ_RATE 80 // X% of the requests is for reading

#define READERS_WRITERS 0 // request is for generic IO device, i.e. readers writers
#define HARD_DRIVE 1  // request is for a hard drive

// struct representing a (simplified) process control block 
typedef struct {
  struct timeval oTimeCreated;
  struct timeval oFirstTimeRunning;
  struct timeval oLastTimeRunning;

  int iPID; // process identifier, assumed to be positive int up to MAX_VALUE
  int iHash;  // hash code (based on process identifier)

  int iBurstTime; // Initial CPU time required by the process 
  int iRemainingBurstTime; // CPU time left for the process
  
  int iState; // process state 
  int iDeviceType; // either readers writers or hard drive
  int iDeviceID; // unique ID identifying the device of a given type
  int iRW; // read or write
  int iTrack;
} Process;

// Macro to initialise a process struct (Process control block)
#define PROCESS_INITIALIZER {.iPID = -1, .iHash = -1, .iBurstTime = 0, .iRemainingBurstTime = 0, .iState = READY}

// creates a process control block in dynamic memory. initialises it, and returns it
Process * generateProcess(int iPID);

// simulates a process running in round robin
void runPreemptiveProcess(Process * pTemp, bool bSimulateHD, bool bSimulateIO);

// simulates IO operations
void simulateIO(Process * pTemp);

// function to calculate the difference between two time stamps
long int getDifferenceInMilliSeconds(struct timeval start, struct timeval end);
 
