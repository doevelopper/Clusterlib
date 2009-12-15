/*
 * timerservice.h --
 *
 * Definition and implementation of TimerService class.
 *
 * $Header$
 * $Revision$
 * $Date$
 */

#ifndef	_TIMERSERVICE_H_
#define	_TIMERSERVICE_H_

namespace clusterlib
{

/**
 * This class provides a static timer.
 */
class TimerService {
  public:
    /**
     * Convenience function -- return the current time in msec
     * from the unix epoch.
     *
     * @return current time in milliseconds
     */
    static int64_t getCurrentTimeMsecs()
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        return now.tv_sec * 1000LL + now.tv_usec / 1000;
    }

    /**
     * Convenience function -- return the current time in usec
     * from the unix epoch.
     *
     * @return current time in useconds
     */
    static int64_t getCurrentTimeUsecs()
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        return now.tv_sec * 1000000LL + now.tv_usec;
    }

    /**
     * Is the current time less than the current time, equal to the
     * current time (given rounding errors) or greater than the
     * current time. 
     *
     * @param usecs the amount of milliseconds to compare
     * @return -1 if current time is less than usecs
     *         0  if current time is equal to usecs
     *         1  if current time is greater than current time
     */
    static int32_t compareTimeUsecs(int64_t usecs)
    {
        struct timeval now;
        gettimeofday(&now, NULL);
        if ((now.tv_sec * 1000000LL + now.tv_usec) < usecs) {
            return -1;
        }
        if ((now.tv_sec * 1000000LL + now.tv_usec) == usecs) {
            return 0;
        }
        else {
            return 1;
        }
    }
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_TIMERSERVICE_H_ */
