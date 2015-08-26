#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>

#   define SLEEP_MSEC(x) \
                do{ \
                        if ((x) >= 1000){ \
                                sleep((x) / 1000); \
                                usleep((x) % 1000 * 1000); \
                        } \
                        else{ \
                                usleep((x)*1000); \
                        } \
                }while (0)

#define USECS_IN_SECOND 1000000
#define NSECS_IN_SECOND 1000000000
#define SLEEP_ITERATION_MS 5
#define BASELINE_ITERATIONS 100
#define SKEW_NOTIFY_THRESHOLD_PERCENT 10

/* Used for getting command line options */
extern int toptind;
extern int toptreset;
extern char *toptarg;
int tgetopt(int nargc, char * const *nargv, const char *ostr);

char usage_str[] = "[-h help] [-b baseline-iterations] [-n notify-threshold] [-s sleep-interval]";

void help()
{
        fprintf(stderr, "Linux Jitter Usage: %s\n", usage_str);
        fprintf(stderr, "Available options:\n"
                        "    -h       Display help\n"
                        "    -b       Number of baseline iterations (Default=100)\n"
                        "    -n       Notify threshold percent(default=10)\n"
                        "    -s       Duration of the sleep interval in milliseconds (Default=5ms)\n\n");
}

/* Nomrmalize system time in case it wraps */
void normalize_tv(struct timeval *tv)
{
        if (tv->tv_usec >= USECS_IN_SECOND) {
                        /* if more than 5 trips through loop required, then do divide and mod */
                        if (tv->tv_usec >= (USECS_IN_SECOND*5)) {
                                tv->tv_sec += (tv->tv_usec / USECS_IN_SECOND);
                                tv->tv_usec = (tv->tv_usec % USECS_IN_SECOND);
                        } else {
                                do {
                                        tv->tv_sec++;
                                        tv->tv_usec -= USECS_IN_SECOND;
                                } while (tv->tv_usec >= USECS_IN_SECOND);
                        }
        } else if (tv->tv_usec <= -USECS_IN_SECOND) {
                        /* if more than 5 trips through loop required, then do divide and mod */
                        if (tv->tv_usec <= (-USECS_IN_SECOND*5)) {
                                tv->tv_sec -= (tv->tv_usec / -USECS_IN_SECOND); /* neg / neg = pos so subtract */
                                tv->tv_usec = (tv->tv_usec % -USECS_IN_SECOND); /* neg % neg = neg */
                        } else {
                                do {
                                        tv->tv_sec--;
                                        tv->tv_usec += USECS_IN_SECOND;
                                } while (tv->tv_usec <= -USECS_IN_SECOND);
                        }
        }
        if (tv->tv_sec >= 1 && tv->tv_usec < 0) {
                tv->tv_sec--;
                tv->tv_usec += USECS_IN_SECOND;
        } else if (tv->tv_sec < 0 && tv->tv_usec > 0) {
                        tv->tv_sec++;
                        tv->tv_usec -= USECS_IN_SECOND;
        }
}

/* Normailze cpu tick time in case it wraps */
void normalize_ts(struct timespec *ts)
{
        if (ts->tv_nsec >= NSECS_IN_SECOND) {
                        /* if more than 5 trips through loop required, then do divide and mod */
                        if (ts->tv_nsec >= ((long int)NSECS_IN_SECOND*5)) {
                                ts->tv_sec += (ts->tv_nsec / NSECS_IN_SECOND);
                                ts->tv_nsec = (ts->tv_nsec % NSECS_IN_SECOND);
                        } else {
                                do {
                                        ts->tv_sec++;
                                        ts->tv_nsec -= NSECS_IN_SECOND;
                                } while (ts->tv_nsec >= NSECS_IN_SECOND);
                        }
        } else if (ts->tv_nsec <= -NSECS_IN_SECOND) {
                        /* if more than 5 trips through loop required, then do divide and mod */
                        if (ts->tv_nsec <= ((long int)-NSECS_IN_SECOND*5)) {
                                ts->tv_sec -= (ts->tv_nsec / -NSECS_IN_SECOND); /* neg / neg = pos so subtract */
                                ts->tv_nsec = (ts->tv_nsec % -NSECS_IN_SECOND); /* neg % neg = neg */
                        } else {
                                do {
                                        ts->tv_sec--;
                                        ts->tv_nsec += NSECS_IN_SECOND;
                                } while (ts->tv_nsec <= -NSECS_IN_SECOND);
                        }
        }
        if (ts->tv_sec >= 1 && ts->tv_nsec < 0) {
                ts->tv_sec--;
                ts->tv_nsec += NSECS_IN_SECOND;
        } else if (ts->tv_sec < 0 && ts->tv_nsec > 0) {
                        ts->tv_sec++;
                        ts->tv_nsec -= NSECS_IN_SECOND;
        }
}

int main(int argc, char **argv)
{
	struct timeval tv_start, tv_end, tv_delta;
	struct timespec ts_start, ts_end, ts_delta;
	long int raw_avg_sec = 0, raw_avg_nsec = 0, gtd_avg_sec = 0, gtd_avg_usec = 0, count = 0;
	int opt;
	int baseline_iter = BASELINE_ITERATIONS;
	int sleep_iter = SLEEP_ITERATION_MS;
	int notify_per = SKEW_NOTIFY_THRESHOLD_PERCENT;

	while ((opt = tgetopt(argc, argv, "b:hn:s:")) != EOF) {
		switch (opt) {
			case 'h':
				help();
				exit(1);
				break;
			case 'b':
				baseline_iter = atoi(toptarg);
				break;
			case 'n':
				notify_per = atoi(toptarg);
				if (notify_per < 1) {
					fprintf(stderr, "Notify threshold must be greater than 0\n");
					exit(1);
				}
				break;				
			case 's':
				sleep_iter = atoi(toptarg);
				break;
			default:
				fprintf(stderr, "Unrecognized option");
				help();
				exit(1);
			break;
		}
	}

	printf("Test will gather basline differences for the first %d iterations, %dms per iteration\n", baseline_iter, sleep_iter);

	while (1)
	{
	
		if(clock_gettime(CLOCK_MONOTONIC_RAW,&ts_start) != 0) {
			printf("Error getting raw cpu time!\n");
			exit(1);
		}
	
		if (gettimeofday(&tv_start,NULL) != 0)
		{
			printf("Error getting gettimeofday!\n");
                        exit(1);
		}

		SLEEP_MSEC(sleep_iter);

		if(clock_gettime(CLOCK_MONOTONIC_RAW,&ts_end) != 0) {
                        printf("Error getting raw cpu time!\n");
                        exit(1);
                }

                if (gettimeofday(&tv_end,NULL) != 0)
                {
                        printf("Error getting gettimeofday!\n");
                        exit(1);
                }

		if (count <= baseline_iter)
		{
			ts_delta.tv_sec = ts_end.tv_sec - ts_start.tv_sec;
                        ts_delta.tv_nsec = ts_end.tv_nsec - ts_start.tv_nsec;
                        tv_delta.tv_sec = tv_end.tv_sec - tv_start.tv_sec;
                        tv_delta.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
                        normalize_tv(&tv_delta);
                        normalize_ts(&ts_delta);

			raw_avg_nsec += ts_delta.tv_nsec;
			raw_avg_sec += ts_delta.tv_sec;
			gtd_avg_usec += tv_delta.tv_usec;
			gtd_avg_sec += tv_delta.tv_sec;
			
		}
		else
		{
			time_t t = time(NULL);
			struct tm tm = *localtime(&t);

			ts_delta.tv_sec = ts_end.tv_sec - ts_start.tv_sec;
			ts_delta.tv_nsec = ts_end.tv_nsec - ts_start.tv_nsec;
			tv_delta.tv_sec = tv_end.tv_sec - tv_start.tv_sec;
			tv_delta.tv_usec = tv_end.tv_usec - tv_start.tv_usec;
			normalize_tv(&tv_delta);
			normalize_ts(&ts_delta);
			
			if (ts_delta.tv_sec != raw_avg_sec)
			{
				/* Big discrepency is raw cpu seconds, no bueno */
				printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				printf("JITTER: Change is raw cpu ticks not expected.\n");
				printf("	Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
				printf("	Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
			}
			else if (tv_delta.tv_sec != gtd_avg_sec)
			{
				/* Big discrepency is system seconds, no bueno */
				printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
				printf("JITTER: Change is system time seconds not expected.\n");
                                printf("        Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
                                printf("        Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
			}
			else if (ts_delta.tv_nsec > raw_avg_nsec + (raw_avg_nsec * ((double) notify_per / 100)) ||
				ts_delta.tv_nsec < raw_avg_nsec - (raw_avg_nsec * ((double) notify_per / 100)) )
			{
				/* Discrepency is raw cpu nsec higher than threshold */
                                printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                                printf("JITTER: Change in raw cpu nsec higher than threshold (%d%%).\n", SKEW_NOTIFY_THRESHOLD_PERCENT);
                                printf("        Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
                                printf("        Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
			}
			else if (tv_delta.tv_usec > gtd_avg_usec + (gtd_avg_usec * ((double) notify_per / 100)) ||
                                ts_delta.tv_nsec < gtd_avg_usec - (gtd_avg_usec * ((double) notify_per / 100)) )
                        {
                                /* Discrepency is gettimeofday usec higher than threshold */
                                printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                                printf("JITTER: Change in gettimeofday usec higher than threshold (%d%%).\n", notify_per);
                                printf("        Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
                                printf("        Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
                        }
		}

		if (count == baseline_iter)
		{
			raw_avg_nsec = raw_avg_nsec / baseline_iter;
			raw_avg_sec = raw_avg_sec / baseline_iter;
			gtd_avg_usec = gtd_avg_usec / baseline_iter;
			gtd_avg_sec = gtd_avg_sec / baseline_iter;

			printf("Baseline raw average diff: %ld.%ld\n", raw_avg_sec, raw_avg_nsec);
			printf("Baseline gtd average diff: %ld.%ld\n", gtd_avg_sec, gtd_avg_usec);
		}
		count++;
	} /* Loop */
} /* Main */

/* tgetopt.c - (renamed from BSD getopt) - this source was adapted from BSD
 *
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef _BSD
extern char *__progname;
#else
#define __progname "tgetopt"
#endif

int	topterr = 1,		/* if error message should be printed */
	toptind = 1,		/* index into parent argv vector */
	toptopt,			/* character checked for validity */
	toptreset;		/* reset getopt */
char	*toptarg;		/* argument associated with option */

#define	BADCH	(int)'?'
#define	BADARG	(int)':'
#define	EMSG	""

/*
 * tgetopt --
 *	Parse argc/argv argument vector.
 */
int
tgetopt(nargc, nargv, ostr)
	int nargc;
	char * const *nargv;
	const char *ostr;
{
	static char *place = EMSG;		/* option letter processing */
	char *oli;				/* option letter list index */

	/* really reset */
	if (toptreset) {
		topterr = 1;
		toptind = 1;
		toptopt = 0;
		toptreset = 0;
		toptarg = NULL;
		place = EMSG;
	}
	if (!*place) {		/* update scanning pointer */
		if (toptind >= nargc || *(place = nargv[toptind]) != '-') {
			place = EMSG;
			return (-1);
		}
		if (place[1] && *++place == '-') {	/* found "--" */
			++toptind;
			place = EMSG;
			return (-1);
		}
	}					/* option letter okay? */
	if ((toptopt = (int)*place++) == (int)':' ||
	    !(oli = strchr(ostr, toptopt))) {
		/*
		 * if the user didn't specify '-' as an option,
		 * assume it means -1.
		 */
		if (toptopt == (int)'-')
			return (-1);
		if (!*place)
			++toptind;
		if (topterr && *ostr != ':')
			(void)fprintf(stderr,
			    "%s: illegal option -- %c\n", __progname, toptopt);
		return (BADCH);
	}
	if (*++oli != ':') {			/* don't need argument */
		toptarg = NULL;
		if (!*place)
			++toptind;
	}
	else {					/* need an argument */
		if (*place)			/* no white space */
			toptarg = place;
		else if (nargc <= ++toptind) {	/* no arg */
			place = EMSG;
			if (*ostr == ':')
				return (BADARG);
			if (topterr)
				(void)fprintf(stderr,
				    "%s: option requires an argument -- %c\n",
				    __progname, toptopt);
			return (BADCH);
		}
	 	else				/* white space */
			toptarg = nargv[toptind];
		place = EMSG;
		++toptind;
	}
	return (toptopt);			/* dump back option letter */
}  /* tgetopt */
