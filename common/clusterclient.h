/*
 * client.h --
 *
 * Include file for client side types. Include this file if you're only writing
 * a pure clusterlib client. If you are creating a server (a node that is in a
 * group inside some app using clusterlib) then you need to include the server-
 * side functionality: server/clusterserver.h.
 *
 * $Header$
 * $Revision$
 * $Date$
 */

#ifndef _CLUSTERCLIENT_H_
#define _CLUSTERCLIENT_H_

namespace clusterlib
{

class Client
{
  public:
    /*
     * Retrieve an application.
     */
    Application *getApplication(const string &appName,
                                bool create = false)
        throw(ClusterException);

  protected:
    /*
     * Make the factory a friend.
     */
    friend class Factory;

    /*
     * Constructor used by the factory.
     */
    Client(FactoryOps *f)
        : mp_f(f)
    {
        m_eventThread.Create(*this, &Client::consumeEvents);
    }

    /*
     * Send an event to this client.
     */
    void sendEvent(Payload *pp)
    {
        m_queue.put(pp);
    }

    /*
     * Make the destructor protected so it can only be invoked
     * from derived classes.
     */
    virtual ~Client() {}

  private:
    /*
     * Make the default constructor private so
     * noone can call it.
     */
    Client()
    {
        throw ClusterException("Someone called the Client "
                               "default constructor!");
    }

    /*
     * Consume events. This method runs in a separate thread.
     */
    void consumeEvents();

  private:
    /*
     * The factory delegate instance we're using.
     */
    FactoryOps *mp_f;

    /*
     * The blocking queue for delivering notifications
     * to this client.
     */
    PayloadQueue m_queue;

    /*
     * The thread handling the events.
     */
    CXXThread<Client> m_eventThread;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_CLUSTERCLIENT_H_ */
