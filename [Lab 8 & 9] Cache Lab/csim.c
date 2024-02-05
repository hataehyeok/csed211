// Taehyeok Ha
// hth021002
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>


int help, verbose = 0;
int s, E, b, t;
int S, B;

int hit_count = 0;
int miss_count = 0;
int evict_count = 0;

FILE *fp = NULL;

typedef struct {
    int valid;
    unsigned tag;
    int LRU;
} cache_line;

cache_line** cache = NULL;

void accessBlock(unsigned long address)
{
    int evict_lru = INT_MIN;
    int evict_idx = 0;
    unsigned set_index = (address >> b) & ((-1UL) >> (64 - s));
    unsigned tag = address >> (s + b);

    cache_line* line = cache[set_index];

    for (int i = 0; i < E; i++)
    {
        if (line[i].valid == 1)
        {
            if (line[i].tag == tag)
            {
                line[i].LRU = 0;
                hit_count++;
                return;
            }
        }
    }

    miss_count++;

    for (int i = 0; i < E; i++)
    {
        if (line[i].valid == 0)
        {
            line[i].tag = tag;
            line[i].valid = 1;
            line[i].LRU = 0;
            return;
        }
    }

    for (int i = 0; i < E; i++)
    {
        if (line[i].LRU > evict_lru)
        {
            evict_idx = i;
            evict_lru = line[i].LRU;
        }
    }

    evict_count++;
    
    line[evict_idx].tag = tag;
    line[evict_idx].LRU = 0;
    
    return;
}


int main(int argc, char* argv[])
{
    char c;
    char oper;
    unsigned long address;
    int size;

    hit_count = 0;
    miss_count = 0;
    evict_count = 0;

    while ((c = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (c) {
            case 'h':
                help = 1;
                break;
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                fp = fopen(optarg, "r");
                break;
        }
    }
    
    S = (unsigned int)1 << s;
    B = (unsigned int)1 << b;

    cache = (cache_line**)malloc(sizeof(cache_line*) * S);
    for(int i = 0; i < S; i++)
    {
        cache[i] = (cache_line*)malloc(sizeof(cache_line) * E);
        for(int j = 0; j < E; j++)
        {
            cache[i][j].valid = 0;
            cache[i][j].tag = 0;
            cache[i][j].LRU = 0;
        }
    }

    while(fscanf(fp ,"%c %lx, %d", &oper, &address, &size) > 0)
    {
        switch(oper)
        {
            case 'L':
                accessBlock(address);
                break;
            case 'S':
                accessBlock(address);
                break;
            case 'M':
                accessBlock(address);
                accessBlock(address);
                break;
        }

        for (int i = 0; i < S; i++)
        {
            for (int j = 0; j < E; j++)
            {
                if (cache[i][j].valid == 1)
                    cache[i][j].LRU++;
            }
        }
    }

    fclose(fp);

    for (int i = 0; i < S; i++)
        free(cache[i]);
    free(cache);

    printSummary(hit_count, miss_count, evict_count);
    return 0;
}