
What is KudosJudge ?
--------------------------------
Short: Open Source Online Judge Server.
Long: That's a Daemon that receives submissions over network, every submission is a code to be compiled, run and compared to the correct answer of a given problem, a verdict is returned to the client.
The Daemon is supposed to be fast and secure. to acheive this, threads are used (pthreads) and a sandboxing system using Linux Containers is implemented. To reduce IO operations, as compiling source code, reading input files, and writing output files, a filesystem-on-memory is used (RAMFS). More Innovative ideas are experimented here to make the process of executing the submission fast.

The Daemon can be easily deployed on multiple nodes, by only using a custom-made DNS Load Balancer (no yet developed)
The ultimate goal of such a software is to develop an API the contests organizers can use to make their own judging systems by only focusing on user and score management and let the judging part to KudosJudge.

How To Install ?
------------------------------
KudosJudge is tested in Ubuntu with Linux > 3.0.
clone the repository.
```make
```
```
   sudo make install
```

Start the server :
```sudo kudosd start
```
you can check the server's state :
```sudo kudosd state
```
or stop it
```sudo kudosd stop
```
to get Kudos Daemon's log :
```kudos-syslog
```
Components
---------------------
KudosJudge is composed from 4 components : 
- Communication, 
- Threading & Memory Management,
- Compilers & Testcases Management,
- Execution


Platform & development kit
----------------------------
linux kernel >= 3.0, gcc >= 4.7

Development state
--------------------------
the project is on hold. I need some contributors to help me resume the development. 
the network part, the threading, and sandboxing seem to work just fine.
Credits 
----------------------------
Ouadim Jamal           : sandboxing,execution, configuration.
Beqqi Mohammed         : threading/network.

contact
------------------------------
please contact if you're intersted in the idea, or wish to contribute so that the project can be deployed and used. 
contact also if you have questions .
