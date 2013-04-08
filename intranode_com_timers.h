/*
 * Copyright (C) Anastassios Nanos 2012 (See AUTHORS file)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE

#ifndef __intranode_com_timers_h__
#define __intranode_com_timers_h__
struct hp_timer_st {
        struct timeval tv1;
        struct timeval tv2;
        unsigned long cnt;
        unsigned long long total;
        unsigned long long val;
};

typedef struct hp_timer_st timers_t;


#ifdef TIMERS_ENABLED


#define TIMER_START(a) gettimeofday(&(a)->tv1,NULL)
#define TIMER_STOP(a) do {                                      \
        gettimeofday(&(a)->tv2, NULL);                          \
        (a)->total += (a)->tv2.tv_usec - (a)->tv1.tv_usec +     \
        ((a)->tv2.tv_sec - (a)->tv1.tv_sec) * 1000000;          \
        (a)->cnt++;                                             \
        } while (0)
#define TIMER_TOTAL(a) ((a)->total)
#define TIMER_COUNT(a) ((a)->cnt)
#define TIMER_RESET(a) do {(a)->cnt = 0; (a)->total = 0; (a)->val = 0;} while (0)
#define TIMER_AVG(tp)	((tp)->cnt ? ((tp)->total / (tp)->cnt) : -1)
#define TICKS_TO_USEC(a) ((double)a)


#else

#define TIMER_START(a)
#define TIMER_STOP(a)
#define TIMER_TOTAL(a) 0ULL
#define TIMER_COUNT(a) 0UL
#define TIMER_RESET(a)
#define TICKS_TO_USEC(a) 0ULL
#define TIMER_AVG(a) 0ULL


#endif	/* TIMERS_ENABLED */

#define var_name(x) #x

#endif /* __intranode_com_timers_h__ */
