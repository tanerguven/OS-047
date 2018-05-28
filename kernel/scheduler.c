#include <kernel/kernel.h>
#include "task.h"

static Task_t * runningTasks_last;
static Task_t * runningTasks_first;

static Task_t * nextTask;
uint32_t lastRunTime = 0;

static Task_t * sleepList_last;
static Task_t * sleepList_first;

void scheduler_init();
void scheduler_timerUpdate(uint32_t time);
void scheduler_run();
void scheduler_addToRunning(Task_t * task);
void scheduler_removeFromRunning(Task_t * task);
void scheduler_removeFromSleepList(Task_t * task);

extern Task_t * task0;

void scheduler_init() {
	runningTasks_first = runningTasks_last = NULL;
	nextTask = NULL;
}

extern int migration_sendTask(Task_t * task);

void scheduler_timerUpdate(uint32_t time) {

	Task_t * t;
	Task_t * next;
	for (next = sleepList_first ; next != NULL ; ) {

		ASSERT(next->next != next);
		t = next;
		// print_info("%d %d\n", time * 10, t->sleep_returnMs);
		next = t->next;
		if (time * 10 > t->sleep_returnMs) {
			// print_info("add to running\n");
			scheduler_removeFromSleepList(t);
			scheduler_addToRunning(t);
		}
	}


	Task_t * activeTask = getActiveTask();

	if (activeTask == task0 || time - lastRunTime > 10) {
		lastRunTime = time;
		scheduler_run();
	}
}

void scheduler_run() {
	// print_info("scheduler_run\n");

	do {
		if (nextTask == NULL)
			nextTask = runningTasks_first;

		if (nextTask == NULL) {
			nextTask = task0;
			break;
		}

		if (nextTask->migrationState == 1) {
			Task_t * t = nextTask;
			nextTask = nextTask->next;
			scheduler_removeFromRunning(t);
			t->state = TASK_STATE_MIGRATING;
			migration_sendTask(t);
		}
	} while (nextTask == NULL);

	Task_t * task = nextTask;
	nextTask = nextTask->next;

	// print_info("[%d] switch to() %d\n", getActiveTask()->id, task->id);
	task->state = TASK_STATE_RUNNING;
	task->task_switch_method(task);
	// print_info("[%d] switch OK\n", getActiveTask()->id);
}


void list_removeTask(Task_t ** first, Task_t ** last, Task_t * task) {
	if (*first == task)
		*first = task->next;
	if (*last == task)
		*last = task->prev;
	if (task->next)
		task->next->prev = task->prev;
	if (task->prev)
		task->prev->next = task->next;
	task->prev = NULL;
	task->next = NULL;
}

void list_addTask(Task_t ** first, Task_t ** last, Task_t * task) {
	ASSERT(task->next == NULL && task->prev == NULL);
	if (*last == NULL) {
		task->prev = NULL;
		task->next = NULL;
		*first = *last = task;
	} else {
		task->next = NULL;
		task->prev = *last;

		(*last)->next = task;
		(*last) = task;
	}
}

void scheduler_addToRunning(Task_t * task) {
	list_addTask(&runningTasks_first, &runningTasks_last, task);
	task->state = TASK_STATE_READY;

	if (nextTask == NULL)
		nextTask = task;
}

void scheduler_removeFromRunning(Task_t * task) {
	list_removeTask(&runningTasks_first, &runningTasks_last, task);
	task->state = TASK_STATE_REMOVED;

	if (nextTask == task)
		nextTask = task->next;
}

void scheduler_addToSleepList(Task_t * task) {
	list_addTask(&sleepList_first, &sleepList_last, task);
	task->state = TASK_STATE_WAITING;
}

void scheduler_removeFromSleepList(Task_t * task) {
	list_removeTask(&sleepList_first, &sleepList_last, task);
	task->state = TASK_STATE_REMOVED;
}


void do_sleep(int ms) {
	Task_t * task = getActiveTask();
	scheduler_removeFromRunning(task);
	task->sleep_returnMs = (lastRunTime * 10) + ms;
	scheduler_addToSleepList(task);
	scheduler_run();
}
