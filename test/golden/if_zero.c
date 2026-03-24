#ifndef IF_ZERO_INCLUDE_GUARD
#define IF_ZERO_INCLUDE_GUARD

#ifdef __cplusplus
extern "C" {
#endif

#if 1
int aa;
#else
int ab;
#endif

#if 0
int ba;
#else
int bb;
#endif

#if X > Y
#if X < Z

int c;

#if 0
int da;
#elif 0
int db;
#elif 1
int dc;
#else
int dd;
#endif

int e;
#endif
int f;
#endif
int g = 0;

#ifdef __cplusplus
}
#endif

void foo(int);
extern "C" void bar(int);

#if 0
#define THING 0
#elif 1
#define THING 1
#elif 2
#define THING 2
#elif 3
#define THING 3
#else
#define THING OTHER
#endif

#endif  // IF_ZERO_INCLUDE_GUARD
