#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PrepareDaemons();
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
//funciones nuevas
void OperatingSystem_PrintReadyToRunQueue();

//variables nuevas
// V1 Ej 10.a
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// V1 Ej 11.a
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID = 0;

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
//heapItem readyToRunQueue[PROCESSTABLEMAXSIZE];
//int numberOfReadyToRunProcesses=0;

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex)
{

	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// Obtain the memory requirements of the program
	int processSize = OperatingSystem_ObtainProgramSize(&programFile, "OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
	{
		processTable[i].busy = 0;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 2);

	// Include in program list  all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();
	// V1 Ej 15
	if (numberOfNotTerminatedUserProcesses == 0)
	{
		OperatingSystem_ReadyToShutdown();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(99, SHUTDOWN, "FATAL ERROR: Missing SIP program!\n");
		exit(1);
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
void OperatingSystem_PrepareDaemons(int programListDaemonsBase)
{

	// Include a entry for SystemIdleProcess at 0 position
	programList[0] = (PROGRAMS_DATA *)malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName = "SystemIdleProcess";
	programList[0]->arrivalTime = 0;
	programList[0]->type = DAEMONPROGRAM; // daemon program

	sipID = initialPID % PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList = programListDaemonsBase;
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the
// 			command lineand daemons programs
int OperatingSystem_LongTermScheduler()
{

	int PID, i,
		numberOfSuccessfullyCreatedProcesses = 0;

	for (i = 0; programList[i] != NULL && i < PROGRAMSMAXNUMBER; i++)
	{
		PID = OperatingSystem_CreateProcess(i);
		//V0 Ej 4.b
		switch (PID)
		{
		case NOFREEENTRY:
			ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
			break;
		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
			break;
		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
			break;
		case TOOBIGPROCESS:
			ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
			break;

		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
		}
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{

	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	//V0 Ej 4.a
	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();
	if (PID == NOFREEENTRY)
		return NOFREEENTRY;

	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);
	if (processSize == PROGRAMDOESNOTEXIST) // V0 Ej 5.b
		return PROGRAMDOESNOTEXIST;
	if (processSize == PROGRAMNOTVALID) // V0 Ej 5.c
		return PROGRAMNOTVALID;

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);
	if (priority == PROGRAMNOTVALID) // V0 Ej 5.c
		return PROGRAMNOTVALID;

	// Obtain enough memory space
	loadingPhysicalAddress = OperatingSystem_ObtainMainMemory(processSize, PID);
	//aqui es cuando el programa tiene un tamaño mayor al espacio reservado en memoria
	if (loadingPhysicalAddress == TOOBIGPROCESS) //V1 Ej 6
		return TOOBIGPROCESS;

	// Load program in the allocated memory
	int size;
	size = OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);
	//aqui es cuando el programa tiene mas instrucciones que lo que soporta por el tamaño del programa
	if (size == TOOBIGPROCESS) // V1 Ej 7
		return TOOBIGPROCESS;

	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);

	// Show message "Process [PID] created from program [executableName]\n"
	ComputerSystem_DebugMessage(70, INIT, PID, executableProgram->executableName);

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{

	if (processSize > MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;

	return PID * MAINMEMORYSECTIONSIZE;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].state = NEW; //es un enumerado con la posicion cero

	//V1 Ej 10.b
	ComputerSystem_DebugMessage(111, SYSPROC,
								PID,
								programList[processTable[PID].programListIndex]->executableName, // nombre del programa
								statesNames[processTable[PID].state]);							 // estado actual

	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;

	// V1 Ej 16.b modo protegido.
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE; // V1 Ej 11.c
	}
	else
	{
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
		// V1 Ej 13
		processTable[PID].copyOfAcummRegister = 0;
		// V1 Ej 11.c
		processTable[PID].queueID = USERPROCESSQUEUE;
	}
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID)
{

	int queueID = processTable[PID].queueID; // V1 Ej 11.c

	if (Heap_add(PID, readyToRunQueue[queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[queueID], PROCESSTABLEMAXSIZE) >= 0)
	{
		int estadoAnterior = processTable[PID].state;
		processTable[PID].state = READY;
		// V1 Ej 10.b
		ComputerSystem_DebugMessage(110, SYSPROC,
									PID,
									programList[processTable[PID].programListIndex]->executableName, // nombre del programa
									statesNames[estadoAnterior],									 // estado anterior
									statesNames[processTable[PID].state]);							 // estado actual
	}

	// V1 Ej 9.b
	OperatingSystem_PrintReadyToRunQueue(); // imprime los pid con su prioridad de la cola de prioridades
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{

	int selectedProcess;

	selectedProcess = OperatingSystem_ExtractFromReadyToRun();

	return selectedProcess;
}

// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun()
{

	int selectedProcess = NOPROCESS;

	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
	{
		selectedProcess = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY,
									&numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	}
	else if (numberOfReadyToRunProcesses[DAEMONSQUEUE] > 0)
	{
		selectedProcess = Heap_poll(readyToRunQueue[DAEMONSQUEUE], QUEUE_PRIORITY,
									&numberOfReadyToRunProcesses[DAEMONSQUEUE]);
	}
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;
	int estadoAnterior = processTable[PID].state;
	// Change the process' state
	processTable[PID].state = EXECUTING;
	// V1 Ej 10.b
	ComputerSystem_DebugMessage(110, SYSPROC,
								PID,
								programList[processTable[PID].programListIndex]->executableName, // nombre del programa
								statesNames[estadoAnterior],									 // estado anterior
								statesNames[processTable[PID].state]);							 // estado actual

	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{

	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 2, processTable[PID].copyOfPSWRegister);

	// V1, Ej 13
	Processor_CopyInSystemStack(MAINMEMORYSIZE - 3, processTable[PID].copyOfAcummRegister);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}

// Function invoked when the executing process leaves the CPU
void OperatingSystem_PreemptRunningProcess()
{

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID)
{

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 1);

	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 2);

	// V1 Ej 13
	processTable[PID].copyOfAcummRegister = Processor_CopyFromSystemStack(MAINMEMORYSIZE - 3);
}

// Exception management routine
void OperatingSystem_HandleException()
{

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(71, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	OperatingSystem_TerminateProcess();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;

	int estadoAnterior = processTable[executingProcessID].state;
	processTable[executingProcessID].state = EXIT;
	// V1 Ej 10.b
	ComputerSystem_DebugMessage(110, SYSPROC,
								executingProcessID,																// pid del proceso que se estaría ejecutando
								programList[processTable[executingProcessID].programListIndex]->executableName, // nombre del programa
								statesNames[estadoAnterior],													// estado anterior
								statesNames[processTable[executingProcessID].state]);							// estado actual

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;

	// V1 Ej 14 mirarlo
	if (numberOfNotTerminatedUserProcesses == 0)
	{
		if (executingProcessID == sipID)
		{ // si el que se esta ejecutando es el daemmon
			// finishing sipID, change PC to address of OS HALT instruction
			Processor_CopyInSystemStack(MAINMEMORYSIZE - 1, OS_address_base + 1);
			ComputerSystem_DebugMessage(99, SHUTDOWN, "The system will shut down now...\n");
			return; // Don't dispatch any process
		}
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall()
{

	int systemCallID;
	int pid_proceso_ejecutandose;
	int prioridad_proceso_ejecutandose;
	int pid_proceso_ready;
	int prioridad_proceso_ready;
	int queueID;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:
		// Show message: "Process [executingProcessID] has the processor assigned\n"
		ComputerSystem_DebugMessage(72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		break;

	case SYSCALL_YIELD: // V1 Ej 12
		// proceso que esta ejecutandose en el procesador
		//saco la cola del proceso que se esta ejecutando
		queueID=processTable[executingProcessID].queueID;
		//esta linea no me hace falta pq ya tengo el pid en executingProcessID
		pid_proceso_ejecutandose = processTable[executingProcessID].programListIndex;
		prioridad_proceso_ejecutandose = processTable[executingProcessID].priority;
		//proceso que esta con la prioridad mas alta en la cola de listos que el proceso que se esta ejecutando
		pid_proceso_ready = Heap_getFirst(readyToRunQueue[queueID],numberOfReadyToRunProcesses[queueID]);
		prioridad_proceso_ready = processTable[pid_proceso_ready].priority;

		if (prioridad_proceso_ejecutandose == prioridad_proceso_ready)
		{
			ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE,
										prioridad_proceso_ejecutandose,
										programList[pid_proceso_ejecutandose]->executableName,
										pid_proceso_ready,
										programList[pid_proceso_ready]->executableName);
			OperatingSystem_PreemptRunningProcess();	 // el proceso sale del procesador
			Heap_poll(readyToRunQueue[queueID], QUEUE_PRIORITY, &numberOfReadyToRunProcesses[queueID]);
			OperatingSystem_Dispatch(pid_proceso_ready); // el proceso nuevo se mete en el procesador, state=executing
		}

		break;
	}
}

//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint)
{
	switch (entryPoint)
	{
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException();
		break;
	}
}

///////////// Funciones nuevas ///////////////////
// V1 Ej 11.b

void OperatingSystem_PrintReadyToRunQueue()
{

	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE, "Ready-to-run processes queues:\n");
	int i, aux_pid, prioridad;
	if (numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
	{
		ComputerSystem_DebugMessage(112, SHORTTERMSCHEDULE);
		for (i = 0; i < numberOfReadyToRunProcesses[USERPROCESSQUEUE]; i++)
		{
			aux_pid = readyToRunQueue[USERPROCESSQUEUE][i].info;
			prioridad = processTable[aux_pid].priority;
			if (i == numberOfReadyToRunProcesses[USERPROCESSQUEUE] - 1)
			{
				ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE, aux_pid, prioridad);
				printf("\n");
			}
			else
			{
				ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE, aux_pid, prioridad);
				printf(", ");
			}
		}
	}

	ComputerSystem_DebugMessage(113, SHORTTERMSCHEDULE);
	for (i = 0; i < numberOfReadyToRunProcesses[DAEMONSQUEUE]; i++)
	{
		aux_pid = readyToRunQueue[DAEMONSQUEUE][i].info;
		prioridad = processTable[aux_pid].priority;
		if (i == numberOfReadyToRunProcesses[DAEMONSQUEUE] - 1)
		{
			ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE, aux_pid, prioridad);
			printf("\n");
		}
		else
		{
			ComputerSystem_DebugMessage(114, SHORTTERMSCHEDULE, aux_pid, prioridad);
			printf(", ");
		}
	}
}

/*

// V1 Ej 9.a
void OperatingSystem_PrintReadyToRunQueue(){

	ComputerSystem_DebugMessage(106,SHORTTERMSCHEDULE,"Ready-to-run processes queue:");
	int i,aux_pid,prioridad;
	for(i=0;i<numberOfReadyToRunProcesses;i++){
		aux_pid=readyToRunQueue[i].info;
		prioridad=processTable[aux_pid].priority;
		if(i==numberOfReadyToRunProcesses-1){
			ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE,aux_pid,prioridad);			
		}		
		else{		
			ComputerSystem_DebugMessage(107,SHORTTERMSCHEDULE,aux_pid,prioridad);
			printf(", ");		
		}						
	}
	printf("\n");
	

}*/
