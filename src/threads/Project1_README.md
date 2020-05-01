# Pintos Project 1 THREADS 说明文档

------

## 一. 设计概览

* 重写 timer_sleep 函数，采取设置闹钟时间唤醒进程的机制。
* 实现优先级调度，包含抢占机制和优先级捐赠。 
* 实现多级反馈调度，使用 4.4BSD Scheduler 机制定时更新线程的优先级。

## 二. 设计细节

### 1. timer_sleep

结构设计：
1. 在 thread 类中增加一个成员变量 sleep_ticks 来记录这个线程的剩余休眠时间。
2. 增加 thread_sleep_ticks_handler 函数用来更新 sleep_ticks。

算法设计：
调用 timer_sleep 函数时把线程阻塞， 利用操作系统自身的时钟中断加入对线程状态的检测， 每次检测将调用 thread_sleep_ticks_handler 更新 sleep_ticks，若 sleep_ticks 归零则唤醒这个线程。

### 2. priority

1）preemption
基本思想：
**维护就绪队列为一个优先级队列**，**且正在运行的进程的优先级是最高的**。

算法设计：
考虑一个进程何时进入就绪队列:
1. thread_unblock
2. init_thread
3. thread_yield
那么我们只要在这三种情况下维护就绪队列为优先级队列即可。

具体实现用 list_insert_ordered 完成。

2）donation

基本思想：
当发现高优先级的任务因为低优先级任务占用资源而阻塞时，就将低优先级任务的优先级提升到等待它所占有的资源的最高优先级任务的优先级。

结构设计：
1. 增加 thread_get_priority，thread_set_priority，thread_update_priority 函数。
2. 在 thread 类中加入优先级队列，用于记录捐赠情况。
3. 重写 lock_acquire, lock_release 函数。
3. 在 lock 类中加入优先级队列，用于记录捐赠情况。

算法设计：
1.  在一个线程获取一个锁的时候， 如果拥有这个锁的线程优先级比自己低就提高它的优先级，并且如果这个锁还被别的锁锁着， 将会递归地捐赠优先级， 然后在这个线程释放掉这个锁之后恢复未捐赠逻辑下的优先级。
2. 如果一个线程被多个线程捐赠， 维护当前优先级为捐赠优先级中的最大值（acquire和release之时）。
4. 在释放锁对一个锁优先级有改变的时候应考虑其余被捐赠优先级和当前优先级。
6. 将锁的等待队列实现为优先级队列，释放锁的时候若优先级改变则可以发生抢占。

### 3.  4.4BSD Scheduler
基本思想：
维护了64个队列， 每个队列对应一个优先级， 从PRI_MIN到PRI_MAX。每隔一定时间，通过一些公式计算来计算出线程当前的优先级， 系统调度的时候会从高优先级队列开始选择线程执行， 这里线程的优先级随着操作系统的运转数据而动态改变。

结构设计：
1. 增加浮点类，用于计算优先级。
2. 增加用来修改公式参数的函数以及更新优先级的函数。

算法设计：
http://web.stanford.edu/class/cs140/projects/pintos/pintos_7.html

## 三. 测试时遇到的问题

编译命令是否加 -O (优化) 会影响 2 个测试点的测试结果。这是因为 4.4BSD Scheduler 算法本身就是依赖时间的，加优化与否会导致运行速度不一样，从而影响优先级的计算。