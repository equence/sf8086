/*
 * Sieve of Eratosthenes - a slightly bigger program that exercises loops,
 * arrays and printf, and gives a feel for how fast the emulator runs.
 */
#include <stdio.h>
#include <time.h>

#define LIMIT 2000

static char composite[LIMIT + 1];

int main(void)
{
    int i, j, count = 0;
    clock_t start = clock();

    for (i = 2; i * i <= LIMIT; i++)
    {
        if (!composite[i])
        {
            for (j = i * i; j <= LIMIT; j += i)
                composite[j] = 1;
        }
    }

    for (i = 2; i <= LIMIT; i++)
    {
        if (!composite[i])
        {
            printf("%d ", i);
            count++;
        }
    }

    printf("\n%d primes below %d\n", count, LIMIT);
    printf("elapsed: %lu clock ticks\n", (unsigned long)(clock() - start));
    return 0;
}
