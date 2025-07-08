#include "pcpu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define MAX_PCPUS 128

// Convert Xen's idle time (nanoseconds) to microseconds to match gettimeofday()
#define XEN_IDLETIME_TO_USEC(idle) ((idle) / 1000)

// File-scope variables (static for module privacy)
static pcpu_stat_t *pcpu_stats = NULL;
static int num_pcpus = 0;
static uint64_t *prev_idle = NULL;
static uint64_t *prev_total = NULL;

int update_pcpu_stats(xc_interface *xch)
{
    struct xen_sysctl_cpuinfo info[MAX_PCPUS];
    struct timeval now;
    int nr_cpus = 0;
    int i;

    if (!xch || xc_getcpuinfo(xch, MAX_PCPUS, info, &nr_cpus) < 0) {
        return -1;
    }

    gettimeofday(&now, NULL);
    uint64_t current_total = (uint64_t)now.tv_sec * 1000000 + now.tv_usec;

    /* Allocate memory if needed */
    if (!pcpu_stats || nr_cpus > num_pcpus) {
        pcpu_stat_t *new_stats = realloc(pcpu_stats, nr_cpus * sizeof(pcpu_stat_t));
        uint64_t *new_prev_idle = realloc(prev_idle, nr_cpus * sizeof(uint64_t));
        uint64_t *new_prev_total = realloc(prev_total, nr_cpus * sizeof(uint64_t));

        if (!new_stats || !new_prev_idle || !new_prev_total) {
            free(new_stats);
            free(new_prev_idle);
            free(new_prev_total);
            return -1;
        }

        pcpu_stats = new_stats;
        prev_idle = new_prev_idle;
        prev_total = new_prev_total;
        num_pcpus = nr_cpus;

        /* Initialize previous values (skip first calculation) */
        for (i = 0; i < nr_cpus; i++) {
            prev_idle[i] = XEN_IDLETIME_TO_USEC(info[i].idletime);
            prev_total[i] = current_total;
            pcpu_stats[i].pcpu_id = i;
            /* Default to 0% on first run */
            pcpu_stats[i].usage_pct = 0.0;
        }
        return 0;
    }

    /* Calculate CPU usage */
    for (i = 0; i < nr_cpus; i++) {
        uint64_t current_idle = XEN_IDLETIME_TO_USEC(info[i].idletime);
        uint64_t idle_diff = current_idle - prev_idle[i];
        uint64_t total_diff = current_total - prev_total[i];
        
        if (total_diff > 0) {
            double usage = 100.0 * (1.0 - ((double)idle_diff / total_diff));
            pcpu_stats[i].usage_pct = (usage < 0) ? 0 : (usage > 100) ? 100 : usage;
        } else {
            pcpu_stats[i].usage_pct = 0.0;
        }
        pcpu_stats[i].pcpu_id = i;
        /* Update history */
        prev_idle[i] = current_idle;
        prev_total[i] = current_total;
    }

    return 0;
}

void print_pcpu_stats(void)
{
    if (!pcpu_stats || num_pcpus == 0) {
        printf("No PCPU data available\n");
        return;
    }

    printf("\nPhysical CPU Usage:\n");
    
    // Print table header
    printf("┌───────┬────────┐\n");
    printf("│ Core  │ Usage  │\n");
    printf("├───────┼────────┤\n");
    
    // Print each CPU's data
    for (int i = 0; i < num_pcpus; i++) {
        printf("│ %-5d │ %5.1f%% │\n",
               pcpu_stats[i].pcpu_id,
               pcpu_stats[i].usage_pct);
    }
    
    // Print table footer
    printf("└───────┴────────┘\n");
}

void free_pcpu_stats(void)
{
    free(pcpu_stats);
    free(prev_idle);
    free(prev_total);
    pcpu_stats = NULL;
    prev_idle = NULL;
    prev_total = NULL;
    num_pcpus = 0;
}
