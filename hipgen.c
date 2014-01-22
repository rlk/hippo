#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "hippo.h"

int main(int argc, char *argv[])
{
    const char *H = NULL;
    const char *T = NULL;
    uint32_t    d =   10;

    int c;

    opterr = 0;

    while ((c = getopt(argc, argv, "H:T:d:")) != -1)

        switch (c)
        {
            case 'T': T = optarg; break;
            case 'H': H = optarg; break;
            case 'd': d = (uint32_t) strtol(optarg, 0, 0); break;
        }

    if (optind < argc)
    {
        if (T && hippo_write(hippo_read_tyc(T, d), argv[optind])) return 0;
        if (H && hippo_write(hippo_read_hip(H, d), argv[optind])) return 0;
    }

    fprintf(stderr, "Usage: %s [-T tyc2.dat] "
                              "[-H hip_main.dat] output.riff\n", argv[0]);
    return 1;
}
