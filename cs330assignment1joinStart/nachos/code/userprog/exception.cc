// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//================Added================
char tName='A';
static void
TimerInterruptHandler(int dummy)
{
  IntStatus oldLevel = interrupt->SetLevel(IntOff);
    NachOSThread* temp;
    if (interrupt->getStatus() != IdleMode){
       temp=scheduler->FindNextToWake();
       while(temp!=NULL){
        temp->setStatus(READY);
        scheduler->ReadyToRun(temp);
        temp=scheduler->FindNextToWake();
       }
       interrupt->YieldOnReturn(); 
    }

   (void) interrupt->SetLevel(oldLevel); 
  
}

void
StartProcessExec(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
  printf("Unable to open file %s\n", filename);
  return;
    }
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;      // close file

    space->InitRegisters();   // set the initial register values
    space->RestoreState();    // load page table register

    machine->Run();     // jump to the user progam
    ASSERT(FALSE);      // machine->Run never returns;
          // the address space exits
          // by doing the syscall "exit"
}

//==================Added===============



static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;        // Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);;

    if ((which == SyscallException) && (type == syscall_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == syscall_PrintInt)) {
       printval = machine->ReadRegister(4);
       if (printval == 0) {
	  writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
	     writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
	     writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintChar)) {
	writeDone->P() ;
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
	  writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPA)) {
//Copied
	unsigned int addr=machine->ReadRegister(4);
	printf("addr=%u\n",addr);
	unsigned int vpn=(unsigned) addr/PageSize;
	if(vpn>=machine->pageTableSize) machine->WriteRegister(2,-1);
	else if(machine->pageTable[vpn].valid==false) machine->WriteRegister(2,-1);
	else if((&(machine->pageTable[vpn]))->physicalPage >= NumPhysPages) machine->WriteRegister(2,-1);
	else
	{
		unsigned int offset = (unsigned) addr%PageSize;
		unsigned int pageFrame = (&(machine->pageTable[vpn]))->physicalPage;
		int physAddr;
		physAddr=pageFrame*PageSize+offset;
		printf("physAddr=%u\n",physAddr);
		machine->WriteRegister(2,physAddr);
	}
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetReg)) {
	//Read argument passed in reg:-4 then from write the value of that register in register 2 to return it.
	machine->WriteRegister(2,machine->ReadRegister(machine->ReadRegister(4)));
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPID)) {
	//Read argument passed in reg:-4 then from write the value of that register in register 2 to return it.
	machine->WriteRegister(2,currentThread->getpid());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_GetPPID)) {
	machine->WriteRegister(2,currentThread->getppid());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Time)) {
       machine->WriteRegister(2,stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == syscall_Sleep)) {
      //stats->Print();
      //printf("first= %d\n",stats->totalTicks);
     IntStatus oldLevel1 = interrupt->SetLevel(IntOff);
      int ticking = machine->ReadRegister(4);
      if(ticking==0)
          currentThread->YieldCPU();
      else
      {
        int deadline=ticking+stats->totalTicks;
        currentThread->setwhentowake(deadline);
        scheduler->RunToSleep(currentThread,deadline);
        timer = new Timer(TimerInterruptHandler, 0, false);
        currentThread->PutThreadToSleep();
      }
      (void) interrupt->SetLevel(oldLevel1);
     // stats->Print();
      // printf("firsti= %d\n",stats->totalTicks);
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if((which == SyscallException) && (type == syscall_Yield)) {
      currentThread->YieldCPU();
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if((which == SyscallException) && (type == syscall_NumInstr))
    {
      machine->WriteRegister(2,stats->numInstructions);
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if((which == SyscallException) && (type == syscall_Exec))
    {
      char fName[100];
      int temp;
      int i=0;
      int file=machine->ReadRegister(4);
      machine->ReadMem(file, 1, &temp);
        while ((*(char*)&temp) != '\0') {
            fName[i++]  = (char)temp;
            file++;
            machine->ReadMem(file, 1, &temp);
        }
        fName[i]  = (char)temp;
        printf("%s",fName);
        StartProcessExec(fName);
    }
    else if((which == SyscallException) && (type == syscall_Join))
    {
      printf("In Fork Join\n");
      int x= machine->ReadRegister(4);
      List* temp= currentThread->getChildren();
      int flag=0;
      for(;temp!=NULL;temp=temp->next) {
        printf("TESTING ::::==>  Child is %d \n",(int)temp->item);
        if((int)temp->item==x) {
          flag=1;
          break;
        }
      }

      if(flag==1)
      {
          temp = exitedThread;
          for(;temp!=NULL;temp=temp->next) {
              if((int)temp->item==x) {
                flag=2;
                break;
              }
          }
          if(flag!=2)
          {
                NachOSThread *nextThread;
                IntStatus oldLevel = interrupt->SetLevel(IntOff);
                nextThread = scheduler->FindNextToRun();
                if (nextThread != NULL) {
                  //scheduler->ReadyToRun(this);
                  scheduler->RuntoWait(currentThread);
                  scheduler->Run(nextThread);
                }
                (void) interrupt->SetLevel(oldLevel);
                
          }
      }
      else{
        machine->WriteRegister(2,-1);
      }
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if((which == SyscallException) && (type == syscall_Exit))
    {
      //exitedThread->Append((void*)currentThread->getpid());

      currentThread->FinishThread();
      
    }
    else if((which == SyscallException) && (type == syscall_Fork))
    {
      machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
      machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
      machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);

      currentThread->SaveUserState();
      currentThread->space->SaveState();      
      NachOSThread* child = new NachOSThread("ForkChild");
      int store=currentThread->getpid();
      child->SaveUserState();
      currentThread->setChild(child);
      currentThread->children->Append((void*)child->getpid());
      machine->WriteRegister(2,child->getpid());
    }
    else {
	   printf("Unexpected user mode exception %d %d\n", which, type);
	   ASSERT(FALSE);
    }
}
