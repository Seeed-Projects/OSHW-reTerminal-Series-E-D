#include <Arduino.h>
#include "minirt_scheduler.h"

namespace MiniRT
{
    Task high_priority_tasks[MINIRT_MAX_HIGH_PRIORITY_TASKS] = {};
    Task low_priority_tasks[MINIRT_MAX_LOW_PRIORITY_TASKS] = {};

    Task* findTask(Task* tasks, int size, void (*cb)()) {
        if (cb == nullptr) {
            return nullptr;
        }
        for (int i = 0; i < size; ++i) {
            if (tasks[i].callback == cb) {
                return &tasks[i];
            }
        }
        return nullptr;
    }

    bool addTask(void (*cb)(), unsigned long iv, Priority p) {
        Task* tasks = (p == Priority::High) ? high_priority_tasks : low_priority_tasks;
        const int size = (p == Priority::High) ? MINIRT_MAX_HIGH_PRIORITY_TASKS : MINIRT_MAX_LOW_PRIORITY_TASKS;

        if (Task* existing = findTask(tasks, size, cb)) {
            existing->interval = iv;
            existing->last_run_ms = 0;
            existing->enabled = true;
            return true;
        }

        for (int i = 0; i < size; ++i) {
            if (tasks[i].callback == nullptr) {
                tasks[i].callback = cb;
                tasks[i].interval = iv;
                tasks[i].last_run_ms = 0;
                tasks[i].enabled = true;
                return true;
            }
        }
        return false;
    }

    bool removeTask(void (*cb)(), Priority p) {
        Task* tasks = (p == Priority::High) ? high_priority_tasks : low_priority_tasks;
        const int size = (p == Priority::High) ? MINIRT_MAX_HIGH_PRIORITY_TASKS : MINIRT_MAX_LOW_PRIORITY_TASKS;

        if (Task* slot = findTask(tasks, size, cb)) {
            *slot = Task{};
            return true;
        }
        return false;
    }

    bool setTaskEnabled(void (*cb)(), Priority p, bool enable) {
        Task* tasks = (p == Priority::High) ? high_priority_tasks : low_priority_tasks;
        const int size = (p == Priority::High) ? MINIRT_MAX_HIGH_PRIORITY_TASKS : MINIRT_MAX_LOW_PRIORITY_TASKS;

        if (Task* slot = findTask(tasks, size, cb)) {
            slot->enabled = enable;
            return true;
        }
        return false;
    }

    void runHighPriorityTasks() {
        unsigned long now = millis();
        for (int i = 0; i < MINIRT_MAX_HIGH_PRIORITY_TASKS; ++i) {
            Task& task = high_priority_tasks[i];
            if (task.enabled && task.callback != nullptr && (now - task.last_run_ms >= task.interval)) {
                task.last_run_ms = now;
                task.callback();
            }
        }
    }

    void runLowPriorityTasks() {
        unsigned long now = millis();
        for (int i = 0; i < MINIRT_MAX_LOW_PRIORITY_TASKS; ++i) {
            Task& task = low_priority_tasks[i];
            if (task.enabled && task.callback != nullptr && (now - task.last_run_ms >= task.interval)) {
                task.last_run_ms = now;
                task.callback();
            }
        }
    }

    void run() {
        runHighPriorityTasks();
        runLowPriorityTasks();
    }
}
