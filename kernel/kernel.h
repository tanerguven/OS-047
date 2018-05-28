#ifndef _KERNEL_H_
#define _KERNEL_H_

// network.c
extern int getMachineId();

// task.c
extern Task_t * getActiveTask();
extern int do_getpid();
extern Task_t * allTasks_get1(int id);

// scheduler.c
extern void do_sleep(int ms);
extern void scheduler_addToRunning(Task_t * task);
extern void scheduler_removeFromRunning(Task_t * task);;
extern void scheduler_run();

// migration.c
extern int migration_sendTask(Task_t * task);

#endif /* _KERNEL_H_ */
