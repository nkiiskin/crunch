#ifndef ALIAS_POWER_GLOBALS_SHARED_H
#define ALIAS_POWER_GLOBALS_SHARED_H

typedef double Value;
typedef Value *ValuePtr;
typedef int Count;

extern Value initial;
extern Value results[];
extern ValuePtr latest;
extern Count changes;

Count populate();
Count resize_results();
#endif
