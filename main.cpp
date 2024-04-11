#include <ucontext.h>
#include <iostream>
#include <vector>
#include <algorithm>

#include "coroutine.h"





void print_hello1(CO_scheduler::Coroutine_handler &co, void *args)
{
/*     uint64_t i = 0, j = 1;
    for(uint64_t num = 0 ; num < 1000000 ; i++, num++)
    {
        j += i;
        if (num % 50 == 0)
            co.yield();
    } */
    int num = 0;

    printf("Hello1 %d\n", num);
    num++;
    co.yield();

    printf("Hello1 %d\n", num);
    num++;
    co.yield();

    printf("Hello1 %d\n", num);
    
    
}
void print_hello2(CO_scheduler::Coroutine_handler &co, void *args)
{
/*      uint64_t i = 0, j = 1;
    for(uint64_t num = 0 ; num < 1000000 ; i++, num++)
    {
        j += i;
        if (num % 50 == 0)
            co.yield();
    } */
    int num = 0;

    printf("Hello2 %d\n", num);
    num++;
    co.yield();

    printf("Hello2 %d\n", num);
    num++;
    co.yield();

    printf("Hello2 %d\n", num);
    
}

void print_hello(CO_scheduler::Coroutine_handler &co, void *args)
{
    auto &schr = *reinterpret_cast<CO_scheduler*>(args);

    auto hdl_1 = schr.Add_task(print_hello1, nullptr);

    auto hdl_2 = schr.Add_task(print_hello2, nullptr);

    schr.run_task(hdl_1);
    schr.run_task(hdl_2);

    hdl_1.run_task();
    hdl_2.run_task();

    schr.run_task(hdl_1);
    schr.run_task(hdl_2);

    schr.run_task(hdl_1);
    schr.run_task(hdl_2);

    schr.run_task(hdl_1);
    schr.run_task(hdl_2);

    schr.run_task(hdl_1);
    schr.run_task(hdl_2);
}

int main()
{  
    CO_scheduler schr;

    auto hdl = schr.Add_task(print_hello, &schr);

    schr.run_task(hdl);
/*     for(int i = 0 ; i < 1000000/50 ; i++)
    {
        schr.run_task(id);
        schr.run_task(id2);
    } */



    
    
    return 0;    
}