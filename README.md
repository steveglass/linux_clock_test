# linux_clock_test

### To build, use gcc:

gcc linux_jitter.c -o linux_jitter

### Purpose

The purpose of this simple utility is to check the integrity of the system clock versus 
the tick count on the CPU. If the system clock gets skewed (by a clock sync protocol for 
example), this utility will show the skew relative to the tick count of the CPU. 

### Executing the Test

Simply run the compiled binary, and pipe the output to a file. It will print baseline 
time measurements based on its sleep cycle, and print any skews that are off by 10%
of the baseline. 

### Configuring the Test

Linux Jitter Usage: [-h help] [-b baseline-iterations] [-n notify-threshold] [-s sleep-interval]
Available options:
    -h       Display help
    -b       Number of baseline iterations (Default=100)
    -n       Notify threshold percent(default=10)
    -s       Duration of the sleep interval in milliseconds (Default=5ms)

