/*
 * counter.h - Copyright (c) 2022-23 Andre M. Maree / KSS Technologies (Pty) Ltd.
 */

#pragma once

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Structures ###########################################


// ############################################ global functions ###################################

int xPulseCountInit(int);
int xPulseCountUpdate(struct tm *);
int xPulseCountIncrement(int);
void vPulseCountReport(void);

#ifdef __cplusplus
}
#endif
