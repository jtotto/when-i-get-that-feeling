#ifndef CABOOSE_PLATFORM_UTIL_H
#define CABOOSE_PLATFORM_UTIL_H

/* Assume that the start register is 4 bytes. */
#define PADNAME2(unique) pad ## unique
#define PADNAME(unique) PADNAME2(unique)
#define PAD(start, end) char PADNAME(__LINE__) [(end) - ((start) + 4)]
#define LEADPAD(base, start) char PADNAME(__LINE__) [(start) - (base)]

#endif
