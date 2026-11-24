#ifndef EXTERNAL_ARRAYS_DATA_H
#define EXTERNAL_ARRAYS_DATA_H

struct entry { char marker; int value; long tag; };

extern char message[], empty[];
extern int numbers[];
extern long big[];
extern double fractions[];
extern struct entry entries[];

int resize_arrays();
int check_arrays();
#endif
