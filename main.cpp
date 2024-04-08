#include <ucontext.h>
#include <iostream>
#include <vector>
#include <algorithm>
struct CO_scheduler;




struct Coroutine
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
        // after the task completed, the process goes to backward_ctx
        task_ctx.uc_link = &backward_ctx; 
    } 

    void yield() {swapcontext(&task_ctx, &backward_ctx);}
private:

    enum eState
    {
        ready = 0,
        running,
        suspend,
        complete
    };

    ucontext_t task_ctx;
    ucontext_t backward_ctx;
    CO_scheduler &scheduler;
    volatile eState status;

    friend CO_scheduler;
};


struct CO_scheduler
{
    CO_scheduler() : running_id(0){}

    CO_scheduler(const CO_scheduler &src) = delete;

    ~CO_scheduler()
    {
        for(auto item : m_task_pkg_list)
        {
            ::operator delete[](item->co.task_ctx.uc_stack.ss_sp);
            delete item;
        }
    }
    using corouting_task_t = void(*)(Coroutine&, void *args);

    using co_id = std::size_t;

    struct Task_package
    {
        Task_package(CO_scheduler &schlr, corouting_task_t task, void *args) : co(schlr), task(task), args(args){}

        Coroutine co;
        void(*task)(Coroutine&, void *args);
        void *args;
    };

    co_id Add_task(void(*task)(Coroutine&, void *args), void *args)
    {
        
        auto it = std::find_if(m_task_pkg_list.begin(), 
                               m_task_pkg_list.end(), 
                               [](const Task_package *task_pkg){return task_pkg->co.status == Coroutine::eState::complete;});

        if (it == m_task_pkg_list.end())
        {
            Task_package *task_pkg = new Task_package(*this, task, args);
            m_task_pkg_list.push_back(task_pkg);
            it = m_task_pkg_list.end()-1;
        }

        // add 1, because 0 is reserved for 'no' task 
        return it - m_task_pkg_list.begin() + 1;
    }

    Task_package& task_package(co_id id) {return *m_task_pkg_list[id-1];}

    static void run_wrapper(int upper, int lower)
    {
        uintptr_t lower_mask = 0x0000'0000'FFFF'FFFF & (((uintptr_t)lower)) ;
        uintptr_t ptr = ((((uintptr_t)upper)) << 32) | lower_mask ;

        CO_scheduler::Task_package &tsk_pkg = *reinterpret_cast<CO_scheduler::Task_package*>(ptr);
        
        tsk_pkg.co.status = Coroutine::eState::running;

        tsk_pkg.task(tsk_pkg.co, tsk_pkg.args);
        
        tsk_pkg.co.status = Coroutine::eState::complete;

        tsk_pkg.co.scheduler.running_id = 0;
    }
    void run_task(co_id id);

    co_id running_task() {return running_id;}

    std::vector<Task_package*> m_task_pkg_list;

    //if id is 0, then no running task
    co_id running_id;
};

void print_hello(Coroutine &co, void *args)
{
    printf("Hello1 \n");

    co.yield();

    printf("Hello2 \n");
}

inline void CO_scheduler::run_task(co_id id)
{
    if (id == 0 || task_package(id).co.status == Coroutine::eState::complete)
        return;
    
    auto &task_pkg = task_package(id);
    auto &co = task_pkg.co;

    getcontext(&co.backward_ctx);
    
    // three posible situation when process steps here 
    // 1. process flows from the begining contineously 
    // 2. the task was suspended, and it comes back here
    // 3. the task is completed, and it comes back here

    switch (co.status)
    {
        case Coroutine::eState::ready:
            co.scheduler.running_id = id;
            makecontext(&co.task_ctx, (void(*)(void))CO_scheduler::run_wrapper, 2, ((uintptr_t)&task_pkg)>>32, ((uintptr_t)&task_pkg)&0xFFFFFFFF);
            setcontext(&co.task_ctx);
            break;
        
        case Coroutine::eState::running:
            printf("swapped out\n");
            co.status = Coroutine::eState::suspend;
            break;

        case Coroutine::eState::suspend:
            printf("resume\n");
            co.scheduler.running_id = id;
            setcontext(&co.task_ctx);
            break;

        case Coroutine::eState::complete:
            printf("complete\n");
            break;

    default:
        abort();
    }
}

int main()
{  
    CO_scheduler schr;

    auto id = schr.Add_task(print_hello, nullptr);

    schr.run_task(id);
    schr.run_task(id);
    schr.run_task(id);
    return 0;    
}