# You Spin Me Round Robin

Implementation for round robin scheduling for a given workload and quantum length.

## Building

To build this program:

gcc -o rr rr.c

## Running

To run:

./rr processes.txt 3   

means run the processes in the txt file with a quantum length of 3.
The expected output should be: 
Average waiting time: 7.00
Average response time: 2.75

## Cleaning up

To clean up all binary files:

make clean 