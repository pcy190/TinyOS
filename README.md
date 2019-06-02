# 🧀TinyOS
A tiny OS running on BOCHS

# 🍉Require
1. Bochs (You can install it via `sudo apt install bochs` or [see it on my blog](https://www.jianshu.com/p/6b3df43932c3))
2. Qemu. It is a powerful simulator.  (Get it via `sudo apt install qemu`)

# 🍓Run
```
//RUN IT
make run

//Clean workspace
make clean
```

//Clean and run
make restart

If you have problem to compile with GCC m32, you can install libs to support gcc -m32 option
```
sudo apt-get install build-essential module-assistant  
sudo apt-get install gcc-multilib g++-multilib 
```

# 🍊PS:
Making something is but a lifestyle.
Simplify on Linux0.11.
Who knows. 🙂

# 💡Achieved functions
- basic memory control
- basic thread schedule
- input&output
- system call handler(pid,printf,etc.)

# 🏳️‍🌈TODO
- file system (HUGE TASK~)
- shell&tube
- disk driver

