[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)  [![Build Status](https://travis-ci.com/pcy190/TinyOS.svg?branch=master)](https://travis-ci.com/pcy190/TinyOS)  ![CodeQL](https://github.com/pcy190/TinyOS/workflows/CodeQL/badge.svg)

# 🧀TinyOS
A tiny OS running on BOCHS

# 🍉Requirement
1. Bochs (You can install it via `sudo apt install bochs` or [see it on my blog](https://www.jianshu.com/p/6b3df43932c3))
2. Qemu. It is a powerful simulator.  (Get it via `sudo apt install qemu`)

You can simply install all dependency packages by the following instructions
```
sudo apt-get install bochs bochs-x qemu
sudo apt-get install xorg-dev 
sudo apt-get install bison 
sudo apt-get install g++ 
sudo apt-get install build-essential module-assistant  
sudo apt-get install gcc-multilib g++-multilib 
```

# 🍓Run
```
//RUN IT
make run

//Clean workspace
make clean

//Clean and run
make restart
```

If you have problem with compiling GCC m32, install following libs to support `gcc -m32` option
```
sudo apt-get install build-essential module-assistant  
sudo apt-get install gcc-multilib g++-multilib 
```

After `make run`, you need to input `c` in the bochs command line.
If you want to exit the system, input `Ctrl+C` in the command line and input `quit`.


# 💡Achieved functions
- basic memory control
- basic thread schedule
- input&output
- system call handler(pid,printf,etc. )
- disk driver (partition identify, format )
- file system
- executable file support
- shell


# 🏳️‍🌈TODO
- tube

# 🍊PS:
Making something is but a lifestyle.
Simplify on Linux0.11.
Who knows. 🙂

