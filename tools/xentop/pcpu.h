#ifndef PCPU_H
#define PCPU_H

#include <xenctrl.h>
#include <stdbool.h>
#include <xenstat.h>

typedef struct {
    int pcpu_id;
    float usage_pct;
} pcpu_stat_t;

/* Public API */
int update_pcpu_stats(xc_interface *xch);
void print_pcpu_stats(void);
void free_pcpu_stats(void);

#endif // PCPU_H
