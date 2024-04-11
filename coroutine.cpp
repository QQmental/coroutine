#include <ucontext.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include "coroutine.h"

struct CO_scheduler::context_t
{
    ucontext_t context;
};

struct CO_scheduler::Coroutine
{

    Coroutine(CO_scheduler &scheduler) : scheduler(scheduler), status(eState::ready)
    {
        getcontext(&task_ctx);
        
        constexpr std::size_t stack_size = 1<<14;

        auto stack = operator new[](stack_size);

        // initialize stack pointer
        task_ctx.uc_stack.ss_sp = stack;

        // init stack size
        task_ctx.uc_stack.ss_size = stack_size;

        // init next context
        // after the task completed or swapped out, the process goes to backward_ctx
        task_ctx.uc_link = &caller_ctx; 
    } 

    ~Coroutine() {::operator delete[](task_ctx.uc_stack.ss_sp);}

    void yield() {swapcontext(&task_ctx, task_ctx.uc_link);}

private:

    ucontext_t task_ctx;
    ucontext_t caller_ctx;
    CO_scheduler &scheduler;
    volatile CO_scheduler::eState status;

    friend CO_scheduler;
};

CO_scheduler::CO_scheduler() {}

struct CO_scheduler::Task_package
{
    
    Task_package(CO_scheduler &schlr, corouting_task_t task, void *args) 
                 : co(schlr), 
                   task(task), 
                   args(args){}

    Coroutine co;
    void(*task)(CO_scheduler::Coroutine_handler&, void *args);
    void *args;
    
};

CO_scheduler::eState CO_scheduler::Get_state(Coroutine_handler hdl) const noexcept {return hdl.impl.co.status;}

CO_scheduler::~CO_scheduler()
{
    for(auto &p : m_task_pkg_list)
        delete p;
}

void CO_scheduler::run_wrapper(int upper, int lower)
{
    static_assert(sizeof(uintptr_t) == sizeof(int) * 2, "a pointer is seperated into two 'int' variables. If it's not, modify this yourself");

    uintptr_t lower_mask = 0x0000'0000'FFFF'FFFF & (((uintptr_t)lower)) ;
    uintptr_t ptr = ((((uintptr_t)upper)) << 32) | lower_mask ;

    CO_scheduler::Task_package &task_pkg = *reinterpret_cast<CO_scheduler::Task_package*>(ptr);
    
    task_pkg.co.status = eState::running;

    CO_scheduler::Coroutine_handler hdl(task_pkg);

    task_pkg.task(hdl, task_pkg.args);
    
    task_pkg.co.status = eState::complete;
}

[[nodiscard]]CO_scheduler::Coroutine_handler CO_scheduler::Add_task(corouting_task_t task, void *args)
{
    
    auto it = std::find_if( m_task_pkg_list.begin(), 
                            m_task_pkg_list.end(), 
                            [](const Task_package *task_pkg){return task_pkg->co.status == eState::complete;});

    if (it == m_task_pkg_list.end())
    {
        Task_package *task_pkg = new Task_package(*this, task, args);
        m_task_pkg_list.push_back(task_pkg);
        it = m_task_pkg_list.end()-1;
    }

    CO_scheduler::Task_package &task_pkg = **it;
    makecontext(&task_pkg.co.task_ctx, (void(*)(void))CO_scheduler::run_wrapper, 2, ((uintptr_t)&task_pkg)>>32, ((uintptr_t)&task_pkg)&0xFFFFFFFF);

    return CO_scheduler::Coroutine_handler(task_pkg);
}

void CO_scheduler::run_task(Coroutine_handler hdl)
{
    if (auto status = hdl.impl.co.status ; status == eState::complete)
        return;
    
    auto &task_pkg = hdl.impl;
    auto &co = task_pkg.co;

    getcontext(&task_pkg.co.caller_ctx);
    
    // three posible situations when process steps here 
    // 1. process flows from the begining contineously 
    // 2. the task was suspended, and it comes back here
    // 3. the task was completed, and it comes back here

    switch (co.status)
    {
        case eState::ready:
            setcontext(&co.task_ctx);
            break;
        
        case eState::running:
            //printf("swapped out\n");
            co.status = eState::suspend;
            break;

        case eState::suspend:
            //printf("resume\n");
            co.status = eState::running;
            setcontext(&co.task_ctx);
            break;

        case eState::complete:
            //printf("complete\n");
            break;

    default:
        abort();
    }
}

CO_scheduler::Coroutine_handler::Coroutine_handler(CO_scheduler::Task_package &pkg) : impl(pkg)
{

}

CO_scheduler::Coroutine_handler::~Coroutine_handler() = default;


CO_scheduler::eState CO_scheduler::Coroutine_handler::Get_state() const noexcept
{
    return impl.co.status;
}

void CO_scheduler::Coroutine_handler::yield() noexcept
{
    impl.co.yield();
}

void CO_scheduler::Coroutine_handler::run_task() noexcept
{   
    impl.co.scheduler.run_task(*this);
}