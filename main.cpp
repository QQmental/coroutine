#include <ucontext.h>
#include <iostream>
#include <vector>
#include <algorithm>
struct CO_scheduler;

void print_hello(CO_scheduler &sechlr, void *args)
{
    printf("Hello \n");
}

enum eState
{
    ready = 0,
    running,
    suspend,
    complete
};

struct Coroutine
{
    Coroutine(CO_scheduler &scheduler) : scheduler(scheduler), status(eState::ready)
    {
        getcontext(&task_ctx);

        auto stack = operator new[](4096);

        // initialize stack pointer
        task_ctx.uc_stack.ss_sp = stack;

        // init stack size
        task_ctx.uc_stack.ss_size = 4096;

        // init next context
        // after the task completed, the process goes to backward_ctx
        task_ctx.uc_link = &backward_ctx; 
    } 

    ucontext_t task_ctx;
    ucontext_t backward_ctx;
    CO_scheduler &scheduler;
    volatile int status;
};


struct CO_scheduler
{
    CO_scheduler() : running_id(0){}

    using corouting_task_t = void(*)(CO_scheduler&, void *args);

    using co_id = std::size_t;

    struct Task_package
    {
        Task_package(CO_scheduler &schlr, corouting_task_t task, void *args) : co(schlr), task(task), args(args){}

        Coroutine co;
        void(*task)(CO_scheduler&, void *args);
        void *args;
    };

    co_id Add_task(void(*task)(CO_scheduler&, void *args), void *args)
    {
        
        auto it = std::find_if(m_task_pkg_list.begin(), 
                               m_task_pkg_list.end(), 
                               [](const Task_package *task_pkg){return task_pkg->co.status == eState::complete;});

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
        
        tsk_pkg.task(tsk_pkg.co.scheduler, tsk_pkg.args);
        
        tsk_pkg.co.status = eState::complete;

        tsk_pkg.co.scheduler.running_id = 0;
    }
    void run_task(co_id id);

    co_id running_task() {return running_id;}

    std::vector<Task_package*> m_task_pkg_list;

    //if id is 0, then no running task
    co_id running_id;
};


inline void CO_scheduler::run_task(co_id id)
{
    if (id == 0 || task_package(id).co.status == eState::complete)
        return;
    
    auto &task_pkg = task_package(id);
    auto &co = task_pkg.co;

    getcontext(&co.backward_ctx);
    
    // process flows from the begining contineously and the task haven't been executed
    if (co.status == eState::ready)
    {
        makecontext(&co.task_ctx, (void(*)(void))CO_scheduler::run_wrapper, 2, ((uintptr_t)&task_pkg)>>32, ((uintptr_t)&task_pkg)&0xFFFFFFFF);
        setcontext(&co.task_ctx);
        co.status = 3;
    }
    else if (co.status == eState::running) // the task is swapped out
    {
        printf("swapped out\n");
        co.status = eState::suspend;
    }
    else if (co.status == eState::suspend) // resume
    {
        setcontext(&co.task_ctx);
        printf("resume\n");
    }
    else if (co.status == eState::complete) // complete
    {
        printf("complete\n");
    }
}

void func2_run(int upper, int lower)
{
    uintptr_t lower_mask = 0x0000'0000'FFFF'FFFF & (((uintptr_t)lower)) ;
    uintptr_t ptr = ((((uintptr_t)upper)) << 32) | lower_mask ;
    Coroutine &co = *reinterpret_cast<Coroutine*>(ptr);
    if (co.status == 0)
    {
        co.status = 1;
        swapcontext(&co.task_ctx, &co.backward_ctx);
    }
    co.status = 3;

}

void func2_run_wrapper(int upper, int lower)
{
    //unc2_run(upper, lower);

    uintptr_t lower_mask = 0x0000'0000'FFFF'FFFF & (((uintptr_t)lower)) ;
    uintptr_t ptr = ((((uintptr_t)upper)) << 32) | lower_mask ;
    Coroutine &co = *reinterpret_cast<Coroutine*>(ptr);
    setcontext(&co.task_ctx);
    printf("resume\n");
}

void func2()
{
    printf("hi\n");
}

void func1(Coroutine &co)
{
    getcontext(&co.backward_ctx);
    
    // process flows from the begining contineously and the task haven't been executed
    if (co.status == 0)
    {
        makecontext(&co.task_ctx, (void(*)(void))func2_run, 2, ((uintptr_t)&co)>>32, ((uintptr_t)&co)&0xFFFFFFFF);
        //func2_run_wrapper(((uintptr_t)&co)>>32, ((uintptr_t)&co)&0xFFFFFFFF);
        setcontext(&co.task_ctx);
        co.status = 3;
    }
    else if (co.status == 1) // the task is swapped out
    {
        printf("swapped out\n");
        co.status = 2;
    }
    else if (co.status == 2) // resume
    {
        //swapcontext(&co.backward_ctx, &co.task_ctx);
        printf("try resume\n");
        //func2_run_wrapper(((uintptr_t)&co)>>32, ((uintptr_t)&co)&0xFFFFFFFF);
        setcontext(&co.task_ctx);
        co.status = 3;
        printf("resume\n");
    }
    else if (co.status == 3) // complete
    {
        printf("complete\n");
    }

}

int main()
{  
    CO_scheduler schr;

    auto id = schr.Add_task(print_hello, nullptr);

    schr.run_task(id);
 /*    Coroutine co;
    co.status = 0;


    getcontext(&co.task_ctx);

    auto stack = new char[1024];
    //char stack[1024];
    // 初始化栈顶指针
    co.task_ctx.uc_stack.ss_sp = stack;
    // 栈大小
    co.task_ctx.uc_stack.ss_size = 1024;
    // 不指定下一个上下文 
    co.task_ctx.uc_link = &co.backward_ctx; 
    // 指定待执行的函数入口，并且该函数不需要参数 */



    //func1(co);
    //func1(co);
    return 0;    
}