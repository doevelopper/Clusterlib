/*
 * periodic.h --
 *
 * Interface for objects that run periodically.
 *
 * $Header$
 * $Revision$
 * $Date$
 */

#ifndef	_CL_PERIODIC_H_
#define	_CL_PERIODIC_H_

namespace clusterlib
{

/**
 * Interface for objects that run periodically.  Users should subclass
 * this to periodically do run() i.e. health checking, updating the
 * current state, etc.
 */
class Periodic
{
  public:
    /**
     * Constructor.
     */
    explicit Periodic(int64_t msecsFrequency, 
                      Notifyable *notifyable = NULL, 
                      ClientData clientData = NULL)
        : m_runMsecsFrequency(msecsFrequency),
          m_notifyable(notifyable),
          m_clientData(clientData) {}

    /**
     * Virtual destructor.
     */
    virtual ~Periodic() {}

    /**
     * Subclasses define this function to be run periodically.
     */
    virtual void run() = 0;

    /**
     * Get the amount of msecs to periodic do run().
     * 
     * @return The amount of time in milliseconds.
     */
    int64_t getMsecsFrequency() 
    {
        Locker l(getMutex());

        return m_runMsecsFrequency;
    }

    /**
     * Get the amount of msecs to periodic do run().
     * 
     * @return The amount of time in milliseconds.
     */
    void setMsecsFrequency(int64_t msecsFrequency) 
    {
        Locker l(getMutex());

        if (msecsFrequency > 0) {
            m_runMsecsFrequency = msecsFrequency;
        }
        else {
            throw InvalidArgumentsException(
                "setMsecsFrequency: msecsAllowedPerHealthCheck <= 0");
        }
    }

    Notifyable *getNotifyable() 
    { 
        Locker l(getMutex());

        return m_notifyable; 
    }
    
    void setNotifyable(Notifyable *notifyable) 
    {
        Locker l(getMutex());

        m_notifyable = notifyable;
    }

    ClientData getClientData() 
    {
        Locker l(getMutex());

        return m_clientData; 
    }

    void setClientData(ClientData clientData) 
    {
        Locker l(getMutex());

        m_clientData = clientData;
    }

  private:
    Mutex *getMutex() { return &m_mutex; }

  private:
    /**
     * Lock to guarantee consistent internal state
     */
    Mutex m_mutex;

    /**
     * Milliseconds to wait in between run().
     */
    int64_t m_runMsecsFrequency;

    /**
     * Saved Notifyable.
     */
    Notifyable *m_notifyable;

    /**
     * Client data 
     */
    ClientData m_clientData;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_CL_PERIODIC_H_ */
