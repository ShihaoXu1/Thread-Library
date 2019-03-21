#include <iostream>
#include <cstdlib>
#include <ucontext.h>
#include <vector>
#include <assert.h>
#include "thread.h"
#include "interrupt.h"

using namespace std;

typedef struct{
   ucontext_t* ptr;
   bool finish;
}thread;

vector <thread*> Queue;
vector < vector<thread*> > locks(100);
vector <vector<vector<thread*> > > condition(100, vector<vector<thread*> >(100));
ucontext_t* cpu;
thread* current;

bool initialised=false;

//void delete_thread(){
//   current->ptr->uc_stack.ss_sp=NULL;
//   current->ptr->uc_stack.ss_size=0;
//   current->ptr->uc_stack.ss_flags=0;
//   current->ptr->uc_link=NULL;
//   delete current->ptr;
//   current=NULL;
//}

int thread_libinit(thread_startfunc_t func, void *arg){
   if(initialised)
	return -1;
   initialised=true;

   if(thread_create(func, arg)!=0)
    	return -1;
   
   current=Queue.front();
   Queue.erase(Queue.begin());

   cpu=new ucontext_t;
   getcontext(cpu);
   interrupt_disable();
   swapcontext(cpu, current->ptr);

   while(Queue.size()>0){
      if(current->finish == true){
        current->ptr->uc_stack.ss_sp=NULL;
   		current->ptr->uc_stack.ss_size=0;
        current->ptr->uc_stack.ss_flags=0;
        current->ptr->uc_link=NULL;
        delete current->ptr;
        current=NULL;
      }
      current=Queue.front();
      Queue.erase(Queue.begin());
      swapcontext(cpu, current->ptr);
   }

     if(current != NULL){
        current->ptr->uc_stack.ss_sp=NULL;
   		current->ptr->uc_stack.ss_size=0;
        current->ptr->uc_stack.ss_flags=0;
        current->ptr->uc_link=NULL;
        delete current->ptr;
        current=NULL;
     }
  
   cout<<"Thread library exiting.\n";
   exit(0);
}

static int start(thread_startfunc_t func, void *arg){
   interrupt_enable();
   func(arg);
   interrupt_disable();
   current->finish=true;
   swapcontext(current->ptr, cpu);
   return 0;
}

int thread_yield(void){
   if(!initialised)
	return -1;

   interrupt_disable();
   Queue.push_back(current);
   swapcontext(current->ptr, cpu);
   interrupt_enable();
   return 0;

}

int thread_create(thread_startfunc_t func, void *arg){
   if(!initialised)
	return -1;
   interrupt_disable();
   thread* newthd=new thread;
   newthd->ptr=new ucontext_t;
   getcontext(newthd->ptr);
   char* stack=new char[STACK_SIZE];
   newthd->ptr->uc_stack.ss_sp=stack;
   newthd->ptr->uc_stack.ss_size=STACK_SIZE;
   newthd->ptr->uc_stack.ss_flags=0;
   newthd->ptr->uc_link=NULL;
   makecontext(newthd->ptr, (void (*)())start, 2, func, arg);
  
   newthd->finish=false;

   Queue.push_back(newthd);

   interrupt_enable();
   return 0;
}


int thread_lock(unsigned int lock){
   if(!initialised)
	return -1;

   interrupt_disable();
   if(locks.at(lock).size()==0){
      locks[lock].push_back(current);
      interrupt_enable();
      return 0;
   }else{
      int check=0;
      for(check=0;check<locks[lock].size();check++){
         if(locks[lock][check]==current){
            interrupt_enable();
            return -1;
         }
      }
      locks[lock].push_back(current);
      //switch to next available thread
      swapcontext(current->ptr, cpu);
   }
   interrupt_enable();
   return 0;
}

int thread_unlock(unsigned int lock){
   if(!initialised)
	return -1;
   interrupt_disable();
   if(locks[lock].size()==0){
      interrupt_enable();
      return -1;
   }
   if(locks[lock].front()==current){
      locks[lock].erase(locks[lock].begin());
      if(locks[lock].size()>0){
         Queue.push_back(locks[lock].front());
      }     
   }else{
       interrupt_enable();
       return -1; 
   }
   interrupt_enable();
   return 0;
}

int thread_signal(unsigned int lock, unsigned int cond){
   if(!initialised)
	return -1;
   interrupt_disable();
   if(condition[lock][cond].size()==0){
      interrupt_enable();
      return 0;
   }else{
      Queue.push_back(condition[lock][cond].front());
      condition[lock][cond].erase(condition[lock][cond].begin());
   }
   interrupt_enable();
   return 0;
}


int thread_broadcast(unsigned int lock, unsigned int cond){
   if(!initialised)
	return -1;
   interrupt_disable();
   if(condition[lock][cond].size()==0){
      interrupt_enable();
      return 0;
   }else{
      while(condition[lock][cond].size()>0){
         Queue.push_back(condition[lock][cond].front());
         condition[lock][cond].erase(condition[lock][cond].begin());     
      }

   }
   interrupt_enable();
   return 0;
}

int thread_wait(unsigned int lock, unsigned int cond){
   if(!initialised)
	return -1;
   interrupt_disable();
   if(locks[lock].size()==0){
      interrupt_enable();
      return -1;
   }
   if(locks[lock].front()==current){
      locks[lock].erase(locks[lock].begin());
      if(locks[lock].size()>0){
         Queue.push_back(locks[lock].front());
      }
      condition[lock][cond].push_back(current);
      swapcontext(current->ptr, cpu);
      interrupt_enable();
      return thread_lock(lock);
   }else{
      interrupt_enable();
      return -1;
   }
}
