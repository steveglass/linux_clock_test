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
#define SKEW_NOTIFY_THRESHOLD_PERCENT 5

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

void normalize_ts(struct timespec *ts)
{
        if (ts->tv_nsec >= NSECS_IN_SECOND) {
                        /* if more than 5 trips through loop required, then do divide and mod */
                        if (ts->tv_nsec >= (NSECS_IN_SECOND*5)) {
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
                        if (ts->tv_nsec <= (-NSECS_IN_SECOND*5)) {
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

	printf("Test will gather basline differences for the first %d iterations, %dms per iteration\n", BASELINE_ITERATIONS, SLEEP_ITERATION_MS);

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

		SLEEP_MSEC(SLEEP_ITERATION_MS);

		if(clock_gettime(CLOCK_MONOTONIC_RAW,&ts_end) != 0) {
                        printf("Error getting raw cpu time!\n");
                        exit(1);
                }

                if (gettimeofday(&tv_end,NULL) != 0)
                {
                        printf("Error getting gettimeofday!\n");
                        exit(1);
                }

		if (count <= BASELINE_ITERATIONS)
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
			else if (ts_delta.tv_nsec > raw_avg_nsec + (raw_avg_nsec * ((double) SKEW_NOTIFY_THRESHOLD_PERCENT / 100)) ||
				ts_delta.tv_nsec < raw_avg_nsec - (raw_avg_nsec * ((double)SKEW_NOTIFY_THRESHOLD_PERCENT / 100)) )
			{
				/* Discrepency is raw cpu nsec higher than threshold */
                                printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                                printf("JITTER: Change in raw cpu nsec higher than threshold (%d%%).\n", SKEW_NOTIFY_THRESHOLD_PERCENT);
                                printf("        Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
                                printf("        Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
			}
			else if (tv_delta.tv_usec > gtd_avg_usec + (gtd_avg_usec * ((double) SKEW_NOTIFY_THRESHOLD_PERCENT / 100)) ||
                                ts_delta.tv_nsec < gtd_avg_usec - (gtd_avg_usec * ((double)SKEW_NOTIFY_THRESHOLD_PERCENT / 100)) )
                        {
                                /* Discrepency is gettimeofday usec higher than threshold */
                                printf("%d-%d-%d %d:%d:%d - ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
                                printf("JITTER: Change in gettimeofday usec higher than threshold (%d%%).\n", SKEW_NOTIFY_THRESHOLD_PERCENT);
                                printf("        Raw Clock Delta: %ld.%ld (sec.nsec)\n", ts_delta.tv_sec, ts_delta.tv_nsec);
                                printf("        Sys Clock Delta: %ld.%ld (sec.usec)\n", tv_delta.tv_sec, tv_delta.tv_usec);
                        }
		}

		if (count == BASELINE_ITERATIONS)
		{
			raw_avg_nsec = raw_avg_nsec / BASELINE_ITERATIONS;
			raw_avg_sec = raw_avg_sec / BASELINE_ITERATIONS;
			gtd_avg_usec = gtd_avg_usec / BASELINE_ITERATIONS;
			gtd_avg_sec = gtd_avg_sec / BASELINE_ITERATIONS;

			printf("Baseline raw average diff: %ld.%ld\n", raw_avg_sec, raw_avg_nsec);
			printf("Baseline gtd average diff: %ld.%ld\n", gtd_avg_sec, gtd_avg_usec);
		}
		count++;
	}
}
