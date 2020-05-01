
# sys2019-pintos

powered by soʇuıd team.

```
                                     ___   
       	           __               | _/
       /\________ /  | ___ ______ __| |
      /  ___/  _ \|  | \  |  \  |/ __ | 
      \___ (  <_> )  _\|  |  /  / /_/ | 
     /______>____/|__| |____/|__\____ | 
       	                     |__|    \/ 

```

-------

> ### 运行编译环境
> * gcc 版本不低于 gcc-6
> * bochs 版本为 2.6.9


------

## Project 1 THREADS

### 一. 设计概览
* 重写 timer_sleep 函数，采取设置闹钟时间唤醒进程的机制。
* 实现优先级调度，包含抢占机制和优先级捐赠。 
* 实现多级反馈调度，使用 BSD Scheduler 机制定时更新线程的优先级。

### 二. 设计细节

#### 1. Timer_sleep
结构设计：
1. 在 `thread` 类中增加一个成员变量 `sleep_ticks` 来记录这个线程的剩余休眠时间。
2. 增加 `thread_sleep_ticks_handler` 函数用来更新 `sleep_ticks`。

算法设计：
调用 `timer_sleep` 函数时把线程阻塞， 利用操作系统自身的时钟中断加入对线程状态的检测， 每次检测将调用 `thread_sleep_ticks_handler` 更新 `sleep_ticks`，若 `sleep_ticks` 归零则唤醒这个线程。

#### 2. Priority
1）Preemption

基本思想：
**维护就绪队列为一个优先级队列**，**且正在运行的进程的优先级是最高的**。

算法设计：
考虑一个进程何时进入就绪队列:
1. `thread_unblock`
2. `init_thread`
3. `thread_yield`
那么我们只要在这三种情况下维护就绪队列为优先级队列即可。

具体实现用 `list_insert_ordered` 完成。

2）Donation

基本思想：
当发现高优先级的任务因为低优先级任务占用资源而阻塞时，就将低优先级任务的优先级提升到等待它所占有的资源的最高优先级任务的优先级。

结构设计：
1. 增加 `thread_get_priority`，`thread_set_priority`，`thread_update_priority` 函数。
2. 在 thread 类中加入优先级队列，用于记录捐赠情况。
3. 重写 `lock_acquire`, `lock_release` 函数。
3. 在 `lock` 类中加入优先级队列，用于记录捐赠情况。

算法设计：
1. 在一个线程获取一个锁的时候， 如果拥有这个锁的线程优先级比自己低就提高它的优先级，并且如果这个锁还被别的锁锁着， 将会递归地捐赠优先级， 然后在这个线程释放掉这个锁之后恢复未捐赠逻辑下的优先级。
1. 如果一个线程被多个线程捐赠， 维护当前优先级为捐赠优先级中的最大值（acquire和release之时）。
2. 在释放锁对一个锁优先级有改变的时候应考虑其余被捐赠优先级和当前优先级。
3. 将锁的等待队列实现为优先级队列，释放锁的时候若优先级改变则可以发生抢占。

#### 3 BSD Scheduler
基本思想：
维护了64个队列， 每个队列对应一个优先级， 从PRI_MIN到PRI_MAX。每隔一定时间，通过一些公式计算来计算出线程当前的优先级， 系统调度的时候会从高优先级队列开始选择线程执行， 这里线程的优先级随着操作系统的运转数据而动态改变。

结构设计：
1. 增加浮点类，用于计算优先级。
2. 增加用来修改公式参数的函数以及更新优先级的函数。

算法设计：http://web.stanford.edu/class/cs140/projects/pintos/pintos_7.html

### 三. 测试时遇到的问题

编译命令是否加 -O (优化) 会影响 2 个测试点的测试结果。这是因为 4.4BSD Scheduler 算法本身就是依赖时间的，加优化与否会导致运行速度不一样，从而影响优先级的计算。


------

## Project 2 USERPROG

### 一. 设计概览

pintos 的第二个项目是实现用户程序，主要实现了以下部分:

- 读取command line，并执行对应的应用程序(包含传参)
- 通过syscall处理用户的进程控制操作， 例如终止当前进程 建立新的进程等
- 通过syscall处理用户的文件操作，例如新建文件 删除文件 读写文件等
- bonus 部分 (禁止写入正在执行的可执行文件)

### 二. 设计细节

#### 1. command line 执行可执行文件

本部分的设计主要是处理command line的参数传递部分。传递参数主要分为两步，第一步是分离参数，第二步是把参数告诉要执行的程序。
使用`strtok_r`函数可以完成实现command line中参数的切割，将一整行命令切成可执行文件名和参数。
参数分离完成之后，采用栈来使得用户程序能够读取到这些参数。以命令`/bin/ls -l foo bar`为例, 参数在栈中的位置如下所示:

    Address         Name                Data            Type
    0xbffffffc      argv[3][...]        bar             char[4]
    0xbffffff8      argv[2][...]        foo             char[4]
    0xbffffff5      argv[1][...]        -l              char[3]
    0xbfffffed      argv[0][...]        /bin/ls         char[8]
    0xbfffffec      word-align          0               uint8_t
    0xbfffffe8      argv[4]             0               char *
    0xbfffffe4      argv[3]             0xbffffffc      char *
    0xbfffffe0      argv[2]             0xbffffff8      char *
    0xbfffffdc      argv[1]             0xbffffff5      char *
    0xbfffffd8      argv[0]             0xbfffffed      char *
    0xbfffffd4      argv                0xbfffffd8      char **
    0xbfffffd0      argc                4               int
    0xbfffffcc      return address      0               void (*) ()
只需要将分离出来的参数按照 从右到左、data-align-pointer-argv-argc-return_addr 的顺序压入栈中，程序就可以正确地读取到参数

#### 2. 用户的syscall指令
用户程序有时候需要“借用”内核的权限，来进行一些比较底层的操作，比如执行一个新的程序、读写I\O等等。Syscall 提供了一个给用户程序内核权限的接口，并且要进行严格的检查以保证用户程序的所有操作都是合法的。
在pintos中，一共涉及到了11个syscall操作，如下所示：


- `exec` 执行一个程序
    - 主要依靠调用`process_execute` 实现。但是由于孩子与父亲之间需要进行充分的信息交流，所以借助信号量来保持孩子与父亲之间的同步问题，确保进程之间的正确调度
- `wait` 等待孩子执行完毕
    - 调用 `process_wait` 函数实现，`process_wait` 中，父进程会首先判断子进程是否已经返回，如果子进程已经返回，父进程会直接将该进程从它的儿子列表中移除。如果子进程未返回，父亲会在子进程的信号量上等，直到子进程返回
- `halt` 停机
    - 调用`shutdown_power_off`函数实现
- `exit` 退出当前进程
    - child需要将自己的返回值和状态告诉father，并设置好返回值，调用`thread_exit`让自己的线程退出
- `open` 打开某个文件
    - 调用`filesys_open`函数实现。此处需要一个list记录开过的文件
- `create` 创建文件
    - 调用 `filesys_create` 函数实现
- `remove` 删除文件
    - 调用 `filesys_remove` 函数实现
- `file_size` 获得文件的大小
    - 调用 `file_length` 实现
- `read` 读取输入源中指定长度的内容
    - 读取源分为标准输入和文件输入两种。标准输入调用 `input_getc` 内容，文件输入调用 `file_read` 直接读取得到指定大小的内容
- `write` 向输出对象写入指定长度的内容
    - 写入对象分为标准输出和文件输出两种。标准输出调用 `putbuf` 输出内容，文件输出调用 `file_write` 直接写入到指定文件
- `seek` 将文件指针跳转到文件中的指定位置
    -  调用 `file_seek` 实现
- `tell` 获取文件指针目前在文件中的位置
    -  调用 `file_tell` 实现
- `close` 关闭文件
    -  调用 `file_close` 关闭文件 并且在已经打开的文件list中删除这个文件句柄

#### 3. bonus——禁止写入正在执行的可执行文件
在 `thread_exec` 中，将执行的可执行文件做 `file_open` 操作， 然后调用 `file_deny_write` 加锁，禁止写入文件。在 `thread_exit` 中，将这个可执行文件调用 `file_allow_write` 解锁，允许写入文件，再调用 `file_close` 关闭文件。


------

## Pintos Project 3 VM

### 一. 设计概览

pintos的第三个项目是在前两个项目的基础上实现虚拟内存，主要干了以下这么几件事：

* 设计并实现了一个 supplemental page table，封装了用户程序使用的虚拟页的所有细节，处理了缺页中断
* 设计并实现了一个 frame table，封装了用户程序使用物理页的所有细节，实现了页面置换的时钟算法
* 设计了一个 swap table，以使用硬盘交换区
* 通过mmap、unmap这两个syscall处理memory mapped files

### 二. 设计细节

#### 1. supplemental page table

1）对每个进程维护一个表，记录用户程序可能用到的所有的虚拟页的信息。对每个虚拟页提供以下信息：

- 当前页的状态：FRAME（页框，在物理内存中），SWAP（在交换区上），FILE（在文件系统中）
- key：虚拟页地址
- value：物理页地址 或 交换区上的编号 或 文件信息（origin）
- origin：储存来自文件系统的页的对应细节（文件指针、文件偏移量、页对齐信息等）
- writable：用来指示来自文件系统的页是否允许写回文件系统

2）在发生缺页中断时，先查对应进程的supplemental page table得到出问题的虚拟地址所在的虚拟页的各种信息，再向frame table申请一个物理页（可能会进行页面置换），接着从交换区或文件中load相应的页到刚刚申请到的物理页中，最后重新执行出问题的指令。

3）为了保证线程安全，每次新建/修改/删除页表项的时候都会上锁。

4）每个进程所有的页表项存在一个hash table中，用虚拟页地址作为索引。

#### 2. frame table

1）对整个操作系统维护一个表，记录所有用户程序可能用到的所有物理页的信息。对每个物理页提供以下信息：

* 所属进程的指针
* 虚拟页地址
* 物理页地址
* pinned：表示该物理页是否允许被页面置换算法置换出去

2）本项目实现的页面置换算法是全局的时钟算法。

3）Pintos将内存平分成两部分，user pool是给用户程序用的，到project3时只有通过frame table才能替用户程序申请到user pool的物理页，其余所有的动态内存申请（页表、struct thread之类的）得到的都是kernel pool的物理页。

4）同样为了线程安全，在申请物理页、释放物理页等过程时会上锁。

5）所有用户进程的申请到的所有frame存在一个hash table中，用物理页地址作为索引。

### 3. swap table

1）swap table也是全局的，负责和硬盘上的交换区交互，使用了系统提供的block接口。

2）swap table 用block设备块的序号来索引和访问。

3）每次释放一个块时，将它的索引添加到释放列表中，这个块可以在下次申请的时候继续使用。

### 4. 系统调用mmap,unmap

这一部分提供了两个系统调用，处理将一段虚拟地址映射到一个硬盘文件上的相关任务：

* mmap 建立映射
  - 在supplemental page table 处注册这一段虚拟地址所涵盖的所有的虚拟页，并提供对应的文件信息给页表中的origin项
  - 注册的虚拟地址不能“沾染”已经注册过的任何虚拟地址
* unmap 解除映射
  - 将之前注册过的、修改过的页写回硬盘
  - 在supplemental page table 处注销之前注册的页

为了实现上述两个syscall，在struct thread记录了一个map file list，用来维护每个映射文件的状态。

另外还有两个特殊的映射：在load可执行文件的时候，建立code segment上一段虚拟地址到只读的可执行文件的映射，以及data segment上一段地址到可写的静态区的映射（当然这个映射对应的页应该被置换到swap区而不是文件系统中，因为我们不应该修改可执行文件中静态变量的信息）。

## 三. 一些调试问题

* 因为makefile中链接了项目中的所有文件，它们都会被编译，所以有时候缺少了include头文件不会被发现，实际运行时可能会调用一些只有声明没有实体的函数，得到错误的结果。
* 为了得到完整的调试信息，我们中途删去了编译选项中的-O，但是没有得到足够优化一度使project1中的两个测试点始终不得通过。
* 实现虚拟内存前后判断用户访问内存是否合法的方法不一样，应该用#ifdef VM分别处理。
* Makefile.userprog里LDFLAGS要加上`max-page-size=0x1000`，不然在gcc（非工具链）下无法通过所有测试。
* load可执行文件时并没有读任何可执行文件的内容，只是在supplemental page table处注册了一下而已，运行的时候code segment里出现了page fault后才会真正地去读——所以load完了后不能关闭可执行文件的指针。


------
# Project 4 FILESYS

------

## 一. 设计概览

* 设计文件索引和可拓展文件，实现文件大小的自动增长，改变文件组织方式，使用inode分散的在硬盘中记录文件。  
* 设计目录组织方式，实现分层目录空间，将目录设计成文件的格式，并使用固定长度的条目进行索引。
* 实现硬盘缓存，使用32KB的空间实现了基本的硬盘缓存，使用时钟算法进行缓存替换。

## 二. 设计细节

### 1.索引与可拓展文件
1) 使用二级inode索引实现文件的索引，文件增长是扩展inode。
2) 文件写入溢出时，申请新的block并修改inode，实现文件增长。


### 2.目录结构

1) 借助文件的结构直接存储每个目录，并设置一个布尔标识符说明其目录属性。
2) 每个目录的条目记录文件的名字和所在位置，以及该条目是否为空条目。
3) 删除条目时直接把该条目置位空条目，增加条目时优先使用空条目。
4) 在文件句柄中使用pos记录被打开的目录当前指向哪一个条目，以实现 `readdir` syscall。

### 3.缓存

1) 在操作系统读写和硬件扇区读写中间添加一层cache作中间层，cache由64个block做成，每个block恰好是一个扇区的大小。每个block记录硬盘的对应位置。
2) 使用`dirty`位和`accessed`位配合时钟算法实现cache的替换。



