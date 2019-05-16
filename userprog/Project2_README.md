# Pintos Project 2 USERPROG 说明文档
---

一. 设计概览
------

pintos 的第二个项目是实现用户程序，主要实现了以下部分:

- 读取command line，并执行对应的应用程序(包含传参)
- 通过syscall处理用户的进程控制操作， 例如终止当前进程 建立新的进程等
- 通过syscall处理用户的文件操作，例如新建文件 删除文件 读写文件等
- bonus 部分 (禁止写入正在执行的可执行文件)

二. 设计细节
------------------
### 1. command line 执行可执行文件

本部分的设计主要是处理command line 的参数传递部分。传递参数主要分为两步，第一步是分离参数，第二步是把参数告诉要执行的程序。
使用strtok_r()函数可以完成实现command line中参数的切割，将一整行命令切成可执行文件名和参数。
参数分离完成之后，采用栈来使得用户程序能够读取到这些参数。以命令"/bin/ls -l foo bar"为例, 参数在栈中的位置如下所示:

    Address	        Name	        Data	        Type
    0xbffffffc	    argv[3][...]	bar\0	        char[4]
    0xbffffff8	    argv[2][...]	foo\0	        char[4]
    0xbffffff5	    argv[1][...]	-l\0	        char[3]
    0xbfffffed	    argv[0][...]	/bin/ls\0	    char[8]
    0xbfffffec	    word-align	    0	            uint8_t
    0xbfffffe8	    argv[4]	        0	            char *
    0xbfffffe4	    argv[3]	        0xbffffffc	    char *
    0xbfffffe0	    argv[2]	        0xbffffff8	    char *
    0xbfffffdc	    argv[1]	        0xbffffff5	    char *
    0xbfffffd8	    argv[0]	        0xbfffffed	    char *
    0xbfffffd4	    argv	        0xbfffffd8	    char **
    0xbfffffd0	    argc	        4	            int
    0xbfffffcc	    return address	0	            void (*) ()
只需要将分离出来的参数按照 从右到左、data-align-pointer-argv-argc-return_addr 的顺序压入栈中，程序就可以正确地读取到参数

### 2. 用户的syscall指令
用户程序有时候需要“借用”内核的权限，来进行一些比较底层的操作，比如执行一个新的程序、读写I\O等等。Syscall 提供了一个给用户程序内核权限的接口，并且要进行严格的检查以保证用户程序的所有操作都是合法的。
在Pintos中，一共涉及到了11个syscall操作，如下所示：


- exec 执行一个程序
    - 主要依靠调用process_execute() 实现。但是由于孩子与父亲之间需要进行充分的信息交流，所以借助信号量来保持孩子与父亲之间的同步问题，确保进程之间的正确调度
- wait 等待孩子执行完毕
    - 调用 process_wait() 函数实现，process_wait() 中，父进程会首先判断子进程是否已经返回，如果子进程已经返回，父进程会直接将该进程从它的儿子列表中移除。如果子进程未返回，父亲会在子进程的信号量上等，直到子进程返回
- halt 停机
    - 调用shutdown_power_off()函数实现
- exit 退出当前进程
    - child需要将自己的返回值和状态告诉father，并设置好返回值，调用thread_exit()让自己的线程退出
- open 打开某个文件
    - 调用filesys_open()函数实现。此处需要一个list记录开过的文件
- create 创建文件
    - 调用 filesys_create() 函数实现
- remove 删除文件
    - 调用 filesys_remove() 函数实现
- file_size 获得文件的大小
    - 调用 file_length() 实现
- read 读取输入源中指定长度的内容
    - 读取源分为标准输入和文件输入两种。标准输入调用 input_getc() 内容，文件输入调用 file_read() 直接读取得到指定大小的内容
- write 向输出对象写入指定长度的内容
    - 写入对象分为标准输出和文件输出两种。标准输出调用 putbuf() 输出内容，文件输出调用 file_write() 直接写入到指定文件
- seek 将文件指针跳转到文件中的指定位置
    -  调用 file_seek() 实现
- tell 获取文件指针目前在文件中的位置
    -  调用 file_tell() 实现
- close 关闭文件
    -  调用 file_close() 关闭文件 并且在已经打开的文件list中删除这个文件句柄

### 3. bonus——禁止写入正在执行的可执行文件
在 thread_exec() 中，将执行的可执行文件做 file_open() 操作， 然后调用 file_deny_write() 加锁，禁止写入文件。在 thread_exit() 中，将这个可执行文件调用 file_allow_write() 解锁，允许写入文件，再调用 file_close() 关闭文件。
