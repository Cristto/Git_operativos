#ifndef OPERATINGSYSTEMBASE_H
#define OPERATINGSYSTEMBASE_H

#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "Heap.h"
#include <stdio.h>

// Prototypes of OS functions that students should not change
int OperatingSystem_ObtainAnEntryInTheProcessTable();
int OperatingSystem_ObtainProgramSize(FILE **, char *);
int OperatingSystem_ObtainPriority(FILE *);
int OperatingSystem_LoadProgram(FILE *, int, int);
void OperatingSystem_ReadyToShutdown();
void OperatingSystem_TerminatingSIP();
void OperatingSystem_ShowTime(char);
void OperatingSystem_PrintStatus();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_PrepareTeachersDaemons();
int OperatingSystem_IsThereANewProgram();
int OperatingSystem_InitializePartitionTable();
void OperatingSystem_ShowPartitionTable(char *);

#define EMPTYQUEUE -1
#define NO 0
#define YES 1

#ifdef SLEEPINGQUEUE
extern heapItem sleepingProcessesQueue[];
extern int numberOfSleepingProcesses; 
#endif

#ifdef ARRIVALQUEUE
extern int numberOfProgramsInArrivalTimeQueue;
extern heapItem arrivalTimeQueue[];
#endif

// Begin indes for daemons in programList
extern int baseDaemonsInProgramList; 


#ifdef MEMCONFIG
typedef struct {
     int initAddress; // Lowest physical address of the partition
     int size; // Size of the partition in memory positions
     int PID; // PID of the process using the partition, or NOPROCESS if it's free
} PARTITIONDATA;

#define PARTITIONTABLEMAXSIZE PROCESSTABLEMAXSIZE*2
extern PARTITIONDATA partitionsTable[PARTITIONTABLEMAXSIZE];
#endif

#endif
