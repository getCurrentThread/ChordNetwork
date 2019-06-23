#pragma once

#define ringSize 64				/* ringSize = 2^baseM */

// A Hash Function from a string to the ID/key space
unsigned int str_hash(const char *);

// For getting a power of 2 
int twoPow(int power);

// For modN modular operation of "minend - subtrand"
int modMinus(int modN, int minuend, int subtrand);

// For modN modular operation of "addend1 + addend2"
int modPlus(int modN, int addend1, int addend2);

// For checking if targNum is "in" the range using left and right modes 
// under modN modular environment 
int modIn(int modN, int targNum, int range1, int range2, int leftmode, int rightmode);