
What is KudosJudge ?
--------------------------------
A Linux daemon that can receive requests, to compile and execute programs written in C,C++,Java, Python ...
the daemon should be robust and fast, this will be acheived by using threads to execute multiple submissions, and Ramfs linux mecanism to keep the test cases and compiling output in memory (ram) instead of loading them from the hardisk in each submission. This judge has the only responsibility of compiling, executing or interpreting, and comparing the outputs. The management of contests and users are not part of the judge and are therefore left to the user of the judge to implement according to their needs, although we can develop a simple client to demonstrate how the communication with the deamon is done.
This Judge can be used in a Programming Contests web platform, in that case the judge plays the same role as Mysql Server, and can be used from PHP to process the submissions. It can also be used as a LAN Programming contest judge, in that case a custom server side middleware should be developed to manage the score and the users, and communicate with KudosJudge.
KudosJudge is a fun project, where contributors aim to improve a service.

Existing technology
---------------------------
I've done a lot of searching in opensource judges on Github. and all of them are designed to be used in easy environments (small number of requests), and are attached mostly to a PHP frontend, and most of them are not event deamon and are started at each submission. Some judges are developed in Shell, they lack speed and security, and don't benefit from threading.

Components
---------------------
KudosJudge is composed from 4 components : 
- Communication, 
- Threading & Memory Management,
- Compilers & Testcases Management,
- Execution


Architecture Overview
----------------------
1- the client sends a submission to the judge using IPC for example ( or TCP through network).
2- the Communication Component checks the submission (using a defined protocol), and puts the submission in a queue.
3- the Memory Management Component checks if there is enough memory and then launchs a new thread to execute the oldest submission in the Queue.
4- The Compilers & Testcases Component holds a list of available compilers and interpreters, and holds the testcases in the memory for the whole contest session to avoid the IO overhead.
5- The Execution Component tries to compile the code and execute it in a secure environment (Sandbox), enforcing limits on CPU time and memory size.
6- The thread exits returning the result of the execution (the judgement : SUCCESS, WRONG, TLE, RE ...).
7- the Communication Component returns the result to the client with all the information about the execution.

Notes
----------
As mentionned above, the goal of KudosJudge is the speed and the ability to manage a lot of requests robustly. That's why we should implement evey idea that will lead to acheive this goal. 
We should always check the license of the libraries we use, in order to own the right to sell the resulting product. And it's always preferable to do things from scratch. 


Platform & development kit
----------------------------
linux kernel >= 3.0, gcc >= 4.7
