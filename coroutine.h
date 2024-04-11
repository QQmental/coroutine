#pragma once
#include <stdlib.h>
#include <vector>


class CO_scheduler
{
struct Coroutine;

struct Task_package;

struct context_t;

public:

    CO_scheduler() ;

    CO_scheduler(const CO_scheduler &src) = delete;

    ~CO_scheduler();

    enum eState
    {
        ready = 0,
        running,
        suspend,
        complete,

        // empty state is free for new task
        empty
    };

    
    class Coroutine_handler
    {
    public:

        Coroutine_handler(CO_scheduler::Task_package &pkg);
        Coroutine_handler(const Coroutine_handler &src) :impl(src.impl) {}
        Coroutine_handler& operator=(const Coroutine_handler &src) = delete;
        ~Coroutine_handler();
        CO_scheduler::eState Get_state()const noexcept;
        void yield() noexcept;
        void run_task() noexcept;

    private:

        CO_scheduler::Task_package &impl;
        friend CO_scheduler;
    };

    using corouting_task_t = void(*)(CO_scheduler::Coroutine_handler&, void *args);

    [[nodiscard]]Coroutine_handler Add_task(corouting_task_t task, void *args);

    void run_task(Coroutine_handler hdl);

private:

    static inline void run_wrapper(int upper, int lower);

    eState Get_state(Coroutine_handler hdl) const noexcept ;

    std::vector<Task_package*> m_task_pkg_list;

    //if id is 0, then no running task
    Task_package * invoked_task;

    friend Coroutine_handler;
};

