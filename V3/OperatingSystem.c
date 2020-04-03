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
void OperatingSystem_HandleClockInterrupt();


//variables nuevas
// V1 Ej 10.a
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// V1 Ej 11.a
heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// In OperatingSystem.c Exercise 5-b of V2
// Heap with blocked processes sort by when to wakeup
heapItem sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation
int initialPID = 3;

// Begin indes for daemons in programList
int baseDaemonsInProgramList;

// V2 Ej 4
int numberOfClockInterrupts;

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

	// V3 Ej 0.c y 0.d
	//ex-n
	ComputerSystem_FillInArrivalTimeQueue(); // mete programas en la cola de llegadas
	OperatingSystem_PrintStatus();
	//end ex-n

	// Create all user processes from the information given in the command line
	OperatingSystem_LongTermScheduler();
	// V1 Ej 15
	if (numberOfNotTerminatedUserProcesses == 0)
	{
		OperatingSystem_ReadyToShutdown();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess"))
	{
		// V2 Ej 1
		//ex-n
		OperatingSystem_ShowTime(SHUTDOWN);
		//end ex-n
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

	//for (i = 0; programList[i] != NULL && i < PROGRAMSMAXNUMBER; i++)
	// V3 Ej 2
	//ex-n
	while(OperatingSystem_IsThereANewProgram()==YES)
	{
		i=Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		//end ex-n
		PID = OperatingSystem_CreateProcess(i);
		//V0 Ej 4.b
		switch (PID)
		{
		case NOFREEENTRY:
			// V2 Ej 1
			//ex-n
			OperatingSystem_ShowTime(ERROR);
			//end ex-n
			ComputerSystem_DebugMessage(103, ERROR, programList[i]->executableName);
			break;
		case PROGRAMNOTVALID:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "invalid priority or size");
			// V2 Ej 1
			//ex-n
			OperatingSystem_ShowTime(ERROR);
			//end ex-n
			break;
		case PROGRAMDOESNOTEXIST:
			ComputerSystem_DebugMessage(104, ERROR, programList[i]->executableName, "it does not exist");
			// V2 Ej 1
			//ex-n
			OperatingSystem_ShowTime(ERROR);
			//end ex-n
			break;
		case TOOBIGPROCESS:
			ComputerSystem_DebugMessage(105, ERROR, programList[i]->executableName);
			// V2 Ej 1
			//ex-n
			OperatingSystem_ShowTime(ERROR);
			//end ex-n
			break;

		default:
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[i]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);
		}
	}

	// V2 Ej 7.d
	//ex-n
	if (numberOfSuccessfullyCreatedProcesses >= 1)
	{
		OperatingSystem_PrintStatus();
	}
	//end ex-n

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

	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(INIT);
	//end ex-n
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

	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(SYSPROC);
	//end ex-n

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

		// V2 Ej 1
		//ex-n
		OperatingSystem_ShowTime(SYSPROC);
		//end ex-n
		// V1 Ej 10.b
		ComputerSystem_DebugMessage(110, SYSPROC,
									PID,
									programList[processTable[PID].programListIndex]->executableName, // nombre del programa
									statesNames[estadoAnterior],									 // estado anterior
									statesNames[processTable[PID].state]);							 // estado actual
	}

	// V1 Ej 9.b
	//OperatingSystem_PrintReadyToRunQueue(); // imprime los pid con su prioridad de la cola de prioridades
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

	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(SYSPROC);
	//end ex-n

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

	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(SYSPROC);
	//end ex-n

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(71, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);

	OperatingSystem_TerminateProcess();

	// V2 Ej 7.c
	OperatingSystem_PrintStatus();
}

// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess()
{

	int selectedProcess;

	int estadoAnterior = processTable[executingProcessID].state;
	processTable[executingProcessID].state = EXIT;

	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(SYSPROC);
	//end ex-n

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
			// V2 Ej 1
			//ex-n
			OperatingSystem_ShowTime(SHUTDOWN);
			//end ex-n

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
	int anterior;
	int wakeUp;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterA();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:

		// V2 Ej 1
		//ex-n
		OperatingSystem_ShowTime(SYSPROC);
		//end ex-n

		// Show message: "Process [executingProcessID] has the processor assigned\n"
		ComputerSystem_DebugMessage(72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		break;

	case SYSCALL_END:

		// V2 Ej 1
		//ex-n
		OperatingSystem_ShowTime(SYSPROC);
		//end ex-n

		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateProcess();
		// V2 Ej 7.b
		OperatingSystem_PrintStatus();
		break;

	case SYSCALL_YIELD: // V1 Ej 12
		//saco en que cola esta el proceso que se esta ejecutando
		queueID = processTable[executingProcessID].queueID;
		// proceso que esta ejecutandose en el procesador
		pid_proceso_ejecutandose = processTable[executingProcessID].programListIndex;
		prioridad_proceso_ejecutandose = processTable[executingProcessID].priority;
		//proceso que esta con la prioridad mas alta en la cola de listos
		pid_proceso_ready = Heap_getFirst(readyToRunQueue[queueID], numberOfReadyToRunProcesses[queueID]);

		if (pid_proceso_ready != NOPROCESS)
		{
			prioridad_proceso_ready = processTable[pid_proceso_ready].priority;

			if (prioridad_proceso_ejecutandose == prioridad_proceso_ready)
			{

				// V2 Ej 1
				//ex-n
				OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
				//end ex-n

				ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE,
											prioridad_proceso_ejecutandose,
											programList[pid_proceso_ejecutandose]->executableName,
											pid_proceso_ready,
											programList[pid_proceso_ready]->executableName);
				// el proceso sale del procesador
				OperatingSystem_PreemptRunningProcess();
				//EL proceso que se saco del procesadior se volvio a meter en la cola
				//hay que sacarlo para que no se pueda volver a ejectura
				Heap_poll(readyToRunQueue[queueID], QUEUE_PRIORITY,
						  &numberOfReadyToRunProcesses[queueID]);
				// el proceso nuevo se mete en el procesador, state=executing
				OperatingSystem_Dispatch(pid_proceso_ready);

				// V2 Ej 7.a
				OperatingSystem_PrintStatus();
			}
		}

		break;
	case SYSCALL_SLEEP: //SYSCALL_SLEEP=7

		//V2 Ej 5.d
		//ex-n
		////////move to the state blocked
		OperatingSystem_SaveContext(executingProcessID);						// guardo algunos datos esenciales
		anterior = processTable[executingProcessID].state;						// guardo el estado anterior
		wakeUp = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1; // calculo el campo del whenToWakeUp
		processTable[executingProcessID].whenToWakeUp = wakeUp;					// lo introduzco en el campo del struct
		// Añado el proceso a la cola de dormidos
		if (Heap_add(executingProcessID, sleepingProcessesQueue,
					 QUEUE_WAKEUP, &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0)
		{
			processTable[executingProcessID].state = BLOCKED; // el estado pasa a bloqueado
			OperatingSystem_ShowTime(SYSPROC);
			ComputerSystem_DebugMessage(110, SYSPROC,
										executingProcessID,
										programList[processTable[executingProcessID].programListIndex]->executableName, // nombre del programa
										statesNames[anterior],															// estado anterior
										statesNames[processTable[executingProcessID].state]);
		}
		////////// end to move to the sate blocked

		// se extrae un proceso de la cola de listos listo para ejecutarse
		pid_proceso_ready = OperatingSystem_ShortTermScheduler();
		// se mete este proceso en el procesador
		OperatingSystem_Dispatch(pid_proceso_ready);
		break;
		//end ex-n

		// V2 Ej 5.e
		//ex-n
		OperatingSystem_PrintStatus();
		//end ex-n
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
	// V2 Ej 2.c
	//ex-n
	case CLOCKINT_BIT: // CLOCKINT_BIT=9
		OperatingSystem_HandleClockInterrupt();
		break;
	} //end ex-n
}

///////////// Funciones nuevas ///////////////////

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt()
{

	// V2 Ej 4
	//ex-n
	numberOfClockInterrupts++;
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);
	//end ex-n

	// V2 Ej 6.a y 6.b
	//ex-n
	int i;
	int aux_pid;

	for (i = numberOfSleepingProcesses - 1; i >= 0; i--)
	{
		aux_pid = sleepingProcessesQueue[i].info;
		if (processTable[aux_pid].whenToWakeUp == numberOfSleepingProcesses)
		{
			Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);
			OperatingSystem_MoveToTheREADYState(aux_pid);
			OperatingSystem_PrintStatus();
		}
	} //end ex-n

	// V2 Ej 6.c
	//ex-n
	//llamamos para que el planificador a largo plazo introduzca otro proceso con el que pueda compararse
	//para saber si tiene mas prioridad que el que se esta ejecutando
	int proceso_nuevo;
	proceso_nuevo = OperatingSystem_LongTermScheduler();
	if (proceso_nuevo > 0 && proceso_nuevo != 0)
	{
		OperatingSystem_MoveToTheREADYState(proceso_nuevo);
	}

	int pid_proceso_ready;

	if (numberOfNotTerminatedUserProcesses > 0)
	{
		pid_proceso_ready = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
	}
	else
	{
		pid_proceso_ready = Heap_getFirst(readyToRunQueue[DAEMONSQUEUE], numberOfReadyToRunProcesses[DAEMONSQUEUE]);
	}

	int prioridad_proceso_ready;
	prioridad_proceso_ready = processTable[pid_proceso_ready].priority;
	if (prioridad_proceso_ready < processTable[executingProcessID].priority)
	{
		pid_proceso_ready = OperatingSystem_ShortTermScheduler();
		OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
		ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE,
									executingProcessID,
									programList[executingProcessID]->executableName,
									pid_proceso_ready,
									programList[pid_proceso_ready]->executableName);
		OperatingSystem_PreemptRunningProcess();
		OperatingSystem_Dispatch(pid_proceso_ready);
		OperatingSystem_PrintStatus(); // V2 Ej 6.d
	}

	//end ex-n
}

// V1 Ej 11.b

void OperatingSystem_PrintReadyToRunQueue()
{
	// V2 Ej 1
	//ex-n
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	//end ex-n

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

//V3 Ej 1
//ex-n
int OperatingSystem_GetExecutingProcessID()
{
	return executingProcessID;
}
//end ex-n

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
