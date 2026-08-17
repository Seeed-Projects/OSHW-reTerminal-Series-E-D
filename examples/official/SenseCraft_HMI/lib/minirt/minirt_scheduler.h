#pragma once

#include "minirt_config.h"
#include "minirt_types.h"

namespace MiniRT 
{
    extern Task high_priority_tasks[MINIRT_MAX_HIGH_PRIORITY_TASKS];
    extern Task low_priority_tasks[MINIRT_MAX_LOW_PRIORITY_TASKS];

    Task* findTask(Task* tasks, int size, void (*cb)());
    bool addTask(void (*cb)(), unsigned long iv, Priority p);
    bool removeTask(void (*cb)(), Priority p);
    bool setTaskEnabled(void (*cb)(), Priority p, bool enable);
    void runHighPriorityTasks();
    void runLowPriorityTasks();
    void run();


}
