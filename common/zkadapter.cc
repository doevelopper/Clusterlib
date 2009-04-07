/* 
 * ============================================================================
 * $Header$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"

DEFINE_LOGGER(LOG, "zookeeper.adapter")
DEFINE_LOGGER(ZK_LOG, "zookeeper.core")

/**
 * \brief A helper class to initialize ZK logging.
 */
class InitZooKeeperLogging
{
  public:
    InitZooKeeperLogging() {
        if (ZK_LOG->isDebugEnabled() || ZK_LOG->isTraceEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_DEBUG);
        } else if (ZK_LOG->isInfoEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_INFO);
        } else if (ZK_LOG->isWarnEnabled()) {
            zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
        } else {
            zoo_set_debug_level(ZOO_LOG_LEVEL_ERROR);
        }
    }
};

using namespace std;
using namespace clusterlib;

namespace zk
{

/**
 * \brief This class provides logic for checking if a request can be retried.
 */
class RetryHandler {
  public:
    RetryHandler(const ZooKeeperConfig &zkConfig)
        : m_zkConfig(zkConfig)
    {
        if (zkConfig.getAutoReconnect()) {
            retries = 2;
        } else {
            retries = 0;
        }
    }
        
    /**
     * \brief Attempts to fix a side effect of the given RC.
     * 
     * @param rc the ZK error code
     * @return whether the error code has been handled and the caller should 
     *         retry an operation the caused this error
     */
    bool handleRC(int32_t rc)
    {
        TRACE(LOG, "handleRC");

        //check if the given error code is recoverable
        if (!retryOnError(rc)) {
            return false;
        }
        LOG_TRACE(LOG, 
                  "RC: %d, retries left: %d", 
                  rc, 
                  retries);
        if (retries-- > 0) {
            return true;
        } else {
            return false;
        }
    }
        
  private:
    /**
     * The ZK config.
     */
    const ZooKeeperConfig &m_zkConfig;
        
    /**
     * The number of outstanding retries.
     */
    int32_t retries;    
        
    /**
     * Checks whether the given error entitles this adapter
     * to retry the previous operation.
     * 
     * @param zkErrorCode one of the ZK error code
     */
    static bool retryOnError(int32_t zkErrorCode)
    {
        return (zkErrorCode == ZCONNECTIONLOSS ||
                zkErrorCode == ZOPERATIONTIMEOUT);
    }
};
    
    
//the implementation of the global ZK event watcher
void zkWatcher(zhandle_t *zhp, 
               int32_t type, 
               int32_t state, 
               const char *path, 
               void *watcherCtx)
{
    TRACE(LOG, "zkWatcher");

    //a workaround for buggy ZK API
    string sPath = 
        (path == NULL || 
         state == ZOO_SESSION_EVENT || 
         state == ZOO_NOTWATCHING_EVENT)
        ? "" 
        : string(path);

    LOG_DEBUG(LOG,
              "zkWatcher: Received a ZK event - type: %d, state: %d, path: "
              "'%s', context: '0x%x'",
              type, 
              state, 
              sPath.c_str(), 
              (unsigned int) zoo_get_context(zhp));

    ZooKeeperAdapter *zkap = (ZooKeeperAdapter *)zoo_get_context(zhp);
    if (zkap != NULL) {
        zkap->enqueueEvent(type, state, sPath);
    } else {
        LOG_ERROR(LOG,
                  "Skipping ZK event (type: %d, state: %d, path: '%s'), "
                  "because ZK passed no context",
                  type, 
                  state, 
                  sPath.c_str());
    }
}



// =======================================================================

ZooKeeperAdapter::ZooKeeperAdapter(ZooKeeperConfig config, 
                                   ZKEventListener *lp,
                                   bool establishConnection) 
    : m_zkConfig(config),
      mp_zkHandle(NULL), 
      m_terminating(false),
      m_connected(false),
      m_state(AS_DISCONNECTED),
      m_eventDispatchAllowed(true)
{
    TRACE(LOG, "ZooKeeperAdapter");

    resetRemainingConnectTimeout();
    
    //enforce setting up appropriate ZK log level
    static InitZooKeeperLogging INIT_ZK_LOGGING;
    
    if (lp != NULL) {
        addListener(lp);
    }

    //start the event dispatcher thread
    m_eventDispatcher.Create(*this, &ZooKeeperAdapter::processEvents);

    //start the user event dispatcher thread
    m_userEventDispatcher.Create(*this, &ZooKeeperAdapter::processUserEvents);
    
    //optionally establish the connection
    if (establishConnection) {
        reconnect();
    }
}

ZooKeeperAdapter::~ZooKeeperAdapter()
{
    TRACE(LOG, "~ZooKeeperAdapter");

    try {
        disconnect();
    } catch (std::exception &e) {
        LOG_ERROR(LOG, 
                  "An exception while disconnecting from ZK: %s",
                  e.what());
    }
    m_terminating = true;
    m_userEventDispatcher.Join();
    m_eventDispatcher.Join();
}

void
ZooKeeperAdapter::validatePath(const string &path)
{
    TRACE(LOG, "validatePath");
    
    if (path.find("/") != 0) {
        throw ZooKeeperException(string("Node path must start with '/' but") +
                                 string(" it was '") +
                                 path +
                                 "'",
                                 m_state == AS_CONNECTED);
    }
    if (path.length() > 1) {
        if (path.rfind("/") == path.length() - 1) {
            throw ZooKeeperException(string("Node path must not end with "
                                             "'/' but it was '") +
                                      path +
                                      "'",
                                      m_state == AS_CONNECTED);
        }
        if (path.find("//") != string::npos) {
            throw ZooKeeperException(string("Node path must not contain "
                                             "'//' but it was '") +
                                      path +
                                      "'",
                                      m_state == AS_CONNECTED);
        }
    }
}

void
ZooKeeperAdapter::disconnect()
{
    TRACE(LOG, "disconnect");

    LOG_TRACE(LOG, 
              "mp_zkHandle: %p, state %d", 
              mp_zkHandle, m_state);

    m_stateLock.lock();
    if (mp_zkHandle != NULL) {
        zookeeper_close(mp_zkHandle);
        mp_zkHandle = NULL;
        setState(AS_DISCONNECTED);
    }
    m_stateLock.unlock();
}

void
ZooKeeperAdapter::stopEventDispatch()
{
    m_eventDispatchAllowed = false;
}

void
ZooKeeperAdapter::reconnect()
{
    TRACE(LOG, "reconnect");
    
    m_stateLock.lock();
    //clear the connection state
    disconnect();
    
    //establish a new connection to ZooKeeper
    mp_zkHandle = zookeeper_init(m_zkConfig.getHosts().c_str(), 
                                 zkWatcher, 
                                 m_zkConfig.getLeaseTimeout(),
                                 NULL, 
                                 this, 
                                 0);
    resetRemainingConnectTimeout();
    if (mp_zkHandle != NULL) {
        setState(AS_CONNECTING);
        m_stateLock.unlock();
    } else {
        m_stateLock.unlock();
        throw ZooKeeperException(
	    string("Unable to connect to ZK running at '") +
            m_zkConfig.getHosts() +
            "'",
            false);
    }
    
    LOG_DEBUG(LOG, 
              "mp_zkHandle: %p, state %d", 
              mp_zkHandle, 
              m_state); 
}

struct sync_completion {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool done;
    int32_t rc;
    ZooKeeperAdapter *zk;
};

static void waitCompletion(int32_t rc, const char *value, const void *data)
{
    int32_t ret;
    
    struct sync_completion *scp = (struct sync_completion *) data;
    scp->rc = rc;
    ret = pthread_mutex_lock(&scp->mutex);
    if (ret) {
	throw ZooKeeperException("Unable to lock mutex", 
                                 ret,
                                 scp->zk->getState() == 
                                 ZooKeeperAdapter::AS_CONNECTED);
    }
    scp->done =true;
    ret = pthread_cond_signal(&scp->cond);
    if (ret) {
	throw ZooKeeperException("Unable to wait cond", 
                                 ret,
                                 scp->zk->getState() == 
                                 ZooKeeperAdapter::AS_CONNECTED);
    }    
    ret = pthread_mutex_unlock(&scp->mutex);
    if (ret) {
	throw ZooKeeperException("Unable to unlock mutex", 
                                 ret,
                                 scp->zk->getState() == 
                                 ZooKeeperAdapter::AS_CONNECTED);
    }
}

bool
ZooKeeperAdapter::sync(const string &path,
                       ZKEventListener *listener,
                       void *context)
{
    TRACE(LOG, "sync");

    validatePath(path);

    int32_t rc = ZINVALIDSTATE, ret;
    struct sync_completion sc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
	memset(&sc, 0, sizeof(sc));
	sc.zk = this;
	ret = pthread_mutex_init(&sc.mutex, NULL);
	if (ret) {
	    throw ZooKeeperException("Unable to init mutex", 
                                     rc,
                                     m_state == AS_CONNECTED);
	}
	ret = pthread_cond_init(&sc.cond, NULL); 
	if (ret) {
	    throw ZooKeeperException("Unable to init cond", 
                                     ret,
                                     m_state == AS_CONNECTED);
	}
	rc = zoo_async(mp_zkHandle,
                       path.c_str(),
                       waitCompletion,
                       &sc);
	if (rc == ZOK) {	    
	    ret = pthread_mutex_lock(&sc.mutex);
	    if (ret) {
		throw ZooKeeperException("Unable to lock mutex",
                                         ret,
                                         m_state == AS_CONNECTED);
	    }
	    while (!sc.done) {
		ret = pthread_cond_wait(&sc.cond, &sc.mutex);
		if (ret) {
		    throw ZooKeeperException("Unable to wait cond", 
                                             ret,
                                             m_state == AS_CONNECTED);
		}
	    }
	    ret = pthread_mutex_unlock(&sc.mutex);
	    if (ret) {
		throw ZooKeeperException("Unable to unlock mutex", 
                                         ret,
                                         m_state == AS_CONNECTED);
	    }
	}

	ret = pthread_mutex_destroy(&sc.mutex);
	if (ret) {
	    throw ZooKeeperException("Unable to destroy mutex", 
                                     ret,
                                     m_state == AS_CONNECTED);
	}
	ret = pthread_cond_destroy(&sc.cond); 
	if (ret) {
	    throw ZooKeeperException("Unable to destroy cond", 
                                     ret,
                                     m_state == AS_CONNECTED);
	}
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(string("Unable to sync data for node ") +
                                 path,
                                 rc,
                                 m_state == AS_CONNECTED);
    }

    /* Sync cannot set a watch, so manually push a zk event up to the
     * listeners.  This assumes:
     * 
     * 1) After a sync, all the watches for any other events have been
     * triggered and processed by zkWatcher.  
     * 2) Syncs complete in order. 
     * 
     * At this point, we insert the sync event into the blocking queue
     * for other listeners.
     */
    m_zkContextsMutex.acquire();
    registerContext(SYNC_DATA, 
                    clusterlib::ClusterlibStrings::SYNC, 
                    listener, 
                    context);
    m_zkContextsMutex.release();
    m_events.put(ZKWatcherEvent(ZOO_SESSION_EVENT, 
                                ZOO_CONNECTED_STATE,
                                clusterlib::ClusterlibStrings::SYNC));

    return true;
}

void
ZooKeeperAdapter::handleAsyncEvent(int32_t type, 
				   int32_t state, 
				   const string &path)
{
    TRACE(LOG, "handleAsyncEvent");

    LOG_DEBUG(LOG, 
              "handleAsyncEvent: type: %d, state %d, path: %s",
              type, 
              state, 
              path.c_str());

    Listener2Context context, context2;

    /* Ignore internal ZK events */
    if (((type != ZOO_SESSION_EVENT) || 
         (!path.compare(clusterlib::ClusterlibStrings::SYNC))) &&
        (type != ZOO_NOTWATCHING_EVENT)) {
        m_zkContextsMutex.acquire();
        /* Check if the user context is available */
        if (type == ZOO_CHANGED_EVENT || type == ZOO_DELETED_EVENT) {
            /*
             * There could be two types of interest here.  In this
             * case, try to notify twice.
             */
            context = findAndRemoveListenerContext(GET_NODE_DATA, path);
            context2 = findAndRemoveListenerContext(NODE_EXISTS, path);
        } 
        else if (type == ZOO_CHILD_EVENT) {
            context = findAndRemoveListenerContext(GET_NODE_CHILDREN, path);
        } 
        else if (type == ZOO_CREATED_EVENT) {
            context = findAndRemoveListenerContext(NODE_EXISTS, path);
        } 
        else if ((type == ZOO_SESSION_EVENT) && 
                 (!path.compare(clusterlib::ClusterlibStrings::SYNC))) {
            /* Special synthetic sync event */
            context = findAndRemoveListenerContext(SYNC_DATA, path);
        }

        m_zkContextsMutex.release();

        /*
         * Only forward events that do have context. If we
         * do not find at least one context, throw.
         */
        bool contextFound = false;
        if (!context.empty()) {
            handleEventInContext(type, state, path, context);
            contextFound = true;
        }
        if (!context2.empty()) {
            handleEventInContext(type, state, path, context2);
            contextFound = true;
        }
        if (!contextFound) {
            throw ZooKeeperException("Tried to handle non-session event "
                                     "without context", 
                                     m_state == AS_CONNECTED);
        }
    }
    else {
        /* Forward these events to listeners as they will be handled
         * specially by listeners. */
        handleEventInContext(type, state, path, context);
    }
}

/*
 * Inject a terminating event -- we simulate SESSION_EXPIRED.
 */
void
ZooKeeperAdapter::injectEndEvent()
{
    m_events.put(ZKWatcherEvent(ZOO_SESSION_EVENT,
                                ZOO_EXPIRED_SESSION_STATE,
                                ""));
}

/** If there is no context, forward events to all listeners */
void
ZooKeeperAdapter::handleEventInContext(int32_t type,
				       int32_t state,
				       const string &path,
				       const Listener2Context &listeners)
{
    TRACE(LOG, "handleEventInContext");

    LOG_DEBUG(LOG,
              "handleEventInContext: listeners: %u, type: %d, state: %d, "
              "path: %s", 
              listeners.empty() ? 0 : 1, 
              type, 
              state, 
              path.c_str());
    
    if (listeners.empty()) {
        /* Propagate with empty context - i.e. ZOO_SESSION_EVENTS */
        ZKWatcherEvent event(type, state, path);
        LOG_DEBUG(LOG,
                  "handleEventInContext: Listener does not exist, event 0x%x",
                  reinterpret_cast<uint32_t>(&event));
        fireEventToAllListeners(event);
    } 
    else {
        for (Listener2Context::const_iterator i = listeners.begin();
             i != listeners.end();
             ++i) {
            ZKWatcherEvent event(type, state, path, i->second);
            if (i->first != NULL) {
                LOG_DEBUG(LOG,
                          "handleEventInContext: Listener exists, event 0x%x",
                          reinterpret_cast<uint32_t>(&event));
                
                fireEvent(i->first, event);
            } 
            /* 
             * This functionality seems to apply if you want to send
             * an event to all listeners with a context.  It is
             * unintuitive and might need to be removed if not used.
             */
            else {
                LOG_WARN(LOG,
                         "handleEventInContext: Listener is NULL, "
                         "but context exists!");
                fireEventToAllListeners(event);
            }
        }
    }
}

void 
ZooKeeperAdapter::enqueueEvent(int32_t type,
                               int32_t state,
                               const string &path)
{
    TRACE(LOG, "enqueueEvents");

    /*
     * Drop the event if dispatch is not allowed.
     */
    if (m_eventDispatchAllowed == false) {
        return;
    }

    /*
     * Pass the event to the handler.
     */
    m_events.put(ZKWatcherEvent(type, state, path));
}

void
ZooKeeperAdapter::processEvents()
{
    TRACE(LOG, "processEvents");

    LOG_INFO(LOG,
             "Hello from ZooKeeperAdapter::processEvents: this = 0x%x, "
             "tid = %d",
             (int32_t) this,
             (uint32_t) pthread_self());

    while (!m_terminating) {
        bool timedOut = false;
        ZKWatcherEvent source = m_events.take(100, &timedOut);
        if (!timedOut) {
            if (source.getType() == ZOO_SESSION_EVENT) {
                LOG_INFO(LOG,
                         "processEvents: Received SESSION event, state: %d. "
                         "Adapter state: %d",
                         source.getState(), 
                         m_state);
                m_stateLock.lock();
                if (source.getState() == ZOO_CONNECTED_STATE) {
                    resetRemainingConnectTimeout();
                    setState(AS_CONNECTED);
                } else if (source.getState() == ZOO_CONNECTING_STATE) {
                    setState(AS_CONNECTING);
                } else if (source.getState() == ZOO_EXPIRED_SESSION_STATE) {
                    LOG_INFO(LOG, "Received EXPIRED_SESSION event");
                    setState(AS_SESSION_EXPIRED);
                }
                m_stateLock.unlock();
            }

            LOG_DEBUG(LOG,
                      "processEvents: Received event, type: %d, state: %d, "
                      "path: %s. Adapter state: %d",
                      source.getType(), 
                      source.getState(), 
                      source.getPath().c_str(), 
                      m_state);

            m_userEvents.put(source);
        }
    }

    LOG_INFO(LOG,
             "End ZooKeeperAdapter::processEvents(): this = 0x%x, "
             "tid = %d",
             (int32_t) this,
             (uint32_t) pthread_self());
}

void
ZooKeeperAdapter::processUserEvents()
{
    TRACE(LOG, "processUserEvents");

    LOG_WARN(LOG,
             "Hello from ZooKeeperAdapter::processUserEvents: this = 0x%x, "
             "tid = %d",
             (int32_t) this,
             (uint32_t) pthread_self());

    while (!m_terminating) {
        bool timedOut = false;
        ZKWatcherEvent source = m_userEvents.take(100, &timedOut);
        if (!timedOut) {
            try {
		LOG_INFO(LOG,
                         "processUserEvents: processing event (type: %d, "
                         "state: %d, path: %s)",
                         source.getType(),
                         source.getState(),
                         source.getPath().c_str());
			  
                handleAsyncEvent(source.getType(),
                                 source.getState(),
                                 source.getPath());
            } catch (std::exception &e) {
                LOG_ERROR(LOG, 
                          "Unable to process event (type: %d, state: %d, "
                          "path: %s), because of exception: %s",
                          source.getType(),
                          source.getState(),
                          source.getPath().c_str(),
                          e.what());
            }
        }
    }

    LOG_INFO(LOG,
             "End ZooKeeperAdapter::processUserEvents(): this = 0x%x, "
             "tid = %d",
             (int32_t) this,
             (uint32_t) pthread_self());
}

void 
ZooKeeperAdapter::registerContext(WatchableMethod method,
                                  const string &path,
                                  ZKEventListener *listener,
                                  ContextType context)
{
    TRACE(LOG, "registerContext");

    LOG_DEBUG(LOG,
	      "registerContext: path %s, listener 0x%x",
              path.c_str(),
              (uint32_t) listener);

    if (!listener) {
	LOG_ERROR(LOG, 
                  "registerContext must have a valid listener");
	throw ZooKeeperException(
            "registerContext must have a valid listener",
            m_state == AS_CONNECTED);
        
    }

    if (!context) {
	LOG_ERROR(LOG, 
                  "registerContext must have a valid context");
	throw ZooKeeperException(
            "registerContext must have a valid context",
            m_state == AS_CONNECTED);
    }

    m_zkContexts[method][path][listener] = context;
}

ZooKeeperAdapter::Listener2Context
ZooKeeperAdapter::findAndRemoveListenerContext(WatchableMethod method,
                                               const string &path)
{
    TRACE(LOG, "findAndRemoveListenerContext");

    Listener2Context listeners;
    Path2Listener2Context::iterator elem = m_zkContexts[method].find(path);
    if (elem != m_zkContexts[method].end()) {
        listeners = elem->second;
        m_zkContexts[method].erase(elem);
    } 
    return listeners;
}

void 
ZooKeeperAdapter::setState(AdapterState newState)
{
    TRACE(LOG, "setState");    
    if (newState != m_state) {
        LOG_INFO(LOG, 
                 "Adapter state transition: %d -> %d", 
                 m_state, 
                 newState);
        m_state = newState;
        m_connected = (newState == AS_CONNECTED);
        m_stateLock.notify();
    } else {
        LOG_TRACE(LOG,
                  "New state same as the current: %d", 
                  newState);
    }
}


//TODO move this code to verifyConnection so reconnect()
//is called from one place only
void
ZooKeeperAdapter::waitUntilConnected() 
{
    TRACE(LOG, "waitUntilConnected");    
    int64_t timeout = getRemainingConnectTimeout();
    LOG_INFO(LOG,
             "Waiting up to %lld ms until a connection to ZK is established",
             timeout);
    bool connected;
    if (timeout > 0) {
        int64_t toWait = timeout;
        while (m_state != AS_CONNECTED && toWait > 0) {
            //check if session expired and reconnect if so
            if (m_state == AS_SESSION_EXPIRED) {
                LOG_INFO(LOG,
                         "Reconnecting because the current session "
                         "has expired");
                reconnect();
            }
            struct timeval now;
            gettimeofday(&now, NULL);
            int64_t milliSecs = -(now.tv_sec * 1000LL + now.tv_usec / 1000);
            LOG_TRACE(LOG, 
                      "About to wait %lld ms", 
                      toWait);
            m_stateLock.wait(toWait);
            gettimeofday(&now, NULL);
            milliSecs += now.tv_sec * 1000LL + now.tv_usec / 1000;
            toWait -= milliSecs;
        }
        waitedForConnect(timeout - toWait);
        LOG_INFO(LOG, 
                 "Waited %lld ms", 
                 timeout - toWait);
    }
    connected = (m_state == AS_CONNECTED);
    if (!connected) {
        if (timeout > 0) {
            LOG_WARN(LOG, 
                     "Timed out while waiting for connection to ZK");
            throw ZooKeeperException(
		"Timed out while waiting for connection to ZK",
                false);
        } else {
            LOG_ERROR(LOG, 
                      "Global timeout expired and "
                      "still not connected to ZK");
            throw ZooKeeperException(
		"Global timeout expired and still not connected to ZK",
                false);
        }
    }
    LOG_INFO(LOG, "Connected!");
}

void
ZooKeeperAdapter::verifyConnection()
{
    TRACE(LOG, "verifyConnection");

    m_stateLock.lock();
    try {
        if (m_state == AS_DISCONNECTED) {
            throw ZooKeeperException(
		"Disconnected from ZK. " \
                "Please use reconnect() before attempting to use any ZK API",
                false);
        } else if (m_state != AS_CONNECTED) {
            LOG_TRACE(LOG, "Checking if need to reconnect...");
            //we are not connected, so check if connection in progress...
            if (m_state != AS_CONNECTING) {
                LOG_TRACE(LOG, 
                          "yes. Checking if allowed to auto-reconnect...");
                //...not in progres, so check if we can reconnect
                if (!m_zkConfig.getAutoReconnect()) {
                    //...too bad, disallowed :(
                    LOG_TRACE(LOG, "no. Sorry.");
                    throw ZooKeeperException("ZK connection is down and "
                                             "auto-reconnect is not allowed",
                                             false);
                } else {
                    LOG_TRACE(LOG, 
                              "...yes. About to reconnect");
                }
                //...we are good to retry the connection
                reconnect();
            } else {
                LOG_TRACE(LOG, 
                          "...no, already in CONNECTING state");
            }               
            //wait until the connection is established
            waitUntilConnected(); 
        }
    } catch (ZooKeeperException &e) {
        m_stateLock.unlock();
        throw;
    }
    m_stateLock.unlock();
}

bool
ZooKeeperAdapter::createNode(const string &path, 
                             const string &value, 
                             int32_t flags, 
                             bool createAncestors,
                             string &createdPath) 
{
    TRACE(LOG, "createNode (internal)");
    validatePath(path);
    
    const int32_t MAX_PATH_LENGTH = 1024;
    char realPath[MAX_PATH_LENGTH];
    realPath[0] = 0;
    
    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        rc = zoo_create(mp_zkHandle, 
                        path.c_str(), 
                        value.c_str(),
                        value.length(),
                        &ZOO_OPEN_ACL_UNSAFE,
                        flags,
                        realPath,
                        MAX_PATH_LENGTH);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNODEEXISTS) {
            //the node already exists
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            return false;
        } else if (rc == ZNONODE && createAncestors) {
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            //one of the ancestors doesn't exist so lets start from the root 
            //and make sure the whole path exists, creating missing nodes if
            //necessary
            for (string::size_type pos = 1; pos != string::npos;) {
                pos = path.find("/", pos);
                if (pos != string::npos) {
                    try {
                        createNode(path.substr(0, pos), "", 0, true);
                    } catch (ZooKeeperException &e) {
                        throw ZooKeeperException(string("Unable to create "
                                                        "node ") + 
                                                 path, 
                                                 rc,
                                                 m_state == AS_CONNECTED);
                    }
                    pos++;
                } else {
                    //no more path components
                    return createNode(path, value, flags, false, createdPath);
                }
            }
        }
        LOG_ERROR(LOG,
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(string("Unable to create node ") +
                                 path,
                                 rc,
                                 m_state == AS_CONNECTED);
    } else {
        LOG_INFO(LOG, 
                 "%s has been created", 
                 realPath);
        createdPath = string(realPath);
        return true;
    }
}

bool
ZooKeeperAdapter::createNode(const string &path,
                             const string &value,
                             int32_t flags,
                             bool createAncestors) 
{
    TRACE(LOG, "createNode");

    string createdPath;
    return createNode(path, value, flags, createAncestors, createdPath);
}

int64_t
ZooKeeperAdapter::createSequence(const string &path,
                                 const string &value,
                                 int32_t flags,
                                 bool createAncestors,
                                 string &createdPath) 
{
    TRACE(LOG, "createSequence");

    bool result = createNode(path,
                             value,
                             flags | ZOO_SEQUENCE,
                             createAncestors,
                             createdPath);
    if (!result) {
        return -1;
    } else {
        //extract sequence number from the returned path
        if (createdPath.find(path) != 0) {
            throw ZooKeeperException(string("Expecting returned path '") +
                                     createdPath + 
                                     "' to start with '" +
                                     path +
                                     "'",
                                     m_state == AS_CONNECTED);
        }
        string seqSuffix =
            createdPath.substr(path.length(), 
                               createdPath.length() - path.length());
        char *ptr = NULL;
        int64_t seq = strtol(seqSuffix.c_str(), &ptr, 10);
        if (ptr != NULL && *ptr != '\0') {
            throw ZooKeeperException(string("Expecting a number but got ") +
                                     seqSuffix,
                                     m_state == AS_CONNECTED);
        }
        return seq;
    }
}

bool
ZooKeeperAdapter::deleteNode(const string &path,
                             bool recursive,
                             int32_t version)
{
    TRACE(LOG, "deleteNode");

    validatePath(path);
        
    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        rc = zoo_delete(mp_zkHandle, path.c_str(), version);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNONODE) {
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            return false;
        }
        if (rc == ZNOTEMPTY && recursive) {
            LOG_WARN(LOG, 
                     "Error %d for %s", 
                     rc, 
                     path.c_str());
            //get all children and delete them recursively...
            vector<string> nodeList;
            getNodeChildren(nodeList, path, false);
            for (vector<string>::const_iterator i = nodeList.begin();
                 i != nodeList.end();
                 ++i) {
                deleteNode(*i, true);
            }
            //...and finally attempt to delete the node again
            return deleteNode(path, false); 
        }
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(string("Unable to delete node ") + path,
                                 rc,
                                 m_state == AS_CONNECTED);
    } else {
        LOG_INFO(LOG, 
                 "%s has been deleted", 
                 path.c_str());
        return true;
    }
}

bool
ZooKeeperAdapter::nodeExists(const string &path,
                             ZKEventListener *listener,
                             void *context, 
                             Stat *stat)
{
    TRACE(LOG, "nodeExists");

    validatePath(path);

    struct Stat tmpStat;
    if (stat == NULL) {
        stat = &tmpStat;
    }
    memset(stat, 0, sizeof(Stat));

    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        if (context != NULL) {    
            m_zkContextsMutex.acquire();
            rc = zoo_exists(mp_zkHandle,
                            path.c_str(),
                            (listener != NULL ? 1 : 0),
                            stat);
            if (rc == ZOK || rc == ZNONODE) {
                registerContext(NODE_EXISTS, path, listener, context);
            }
            m_zkContextsMutex.release();
        } else {
	    if (listener) {
		throw ZooKeeperException(
                    "A watch can be set only if the context is not NULL",
                    -1,
                    m_state == AS_CONNECTED);
            }
            rc = zoo_exists(mp_zkHandle,
                            path.c_str(),
                            (listener != NULL ? 1 : 0),
                            stat);
        }
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        if (rc == ZNONODE) {
            LOG_TRACE(LOG, 
                      "Node %s does not exist", 
                      path.c_str());
            return false;
        }
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(
            string("Unable to check existence of node ") + path,
            rc,
            m_state == AS_CONNECTED);
    } else {
        return true;        
    }
}

void
ZooKeeperAdapter::getNodeChildren(vector<string> &nodeList,
                                  const string &path, 
                                  ZKEventListener *listener,
                                  void *context)
{
    TRACE(LOG, "getNodeChildren");

    validatePath(path);
    
    String_vector children;
    memset(&children, 0, sizeof(children));

    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        if (context != NULL) {
            m_zkContextsMutex.acquire();
            rc = zoo_get_children(mp_zkHandle,
                                  path.c_str(), 
                                  (listener != NULL ? 1 : 0), 
                                  &children);
            if (rc == ZOK) {
                registerContext(GET_NODE_CHILDREN, path, listener, context);
            }
            m_zkContextsMutex.release();
        } else {
	    if (listener) {
		throw ZooKeeperException(
                    "A watch can be set only if the context is not NULL",
                    -1,
                    m_state == AS_CONNECTED);
            }
            rc = zoo_get_children(mp_zkHandle,
                                  path.c_str(), 
                                  (listener != NULL ? 1 : 0),
                                  &children);
        }
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(string("Unable to get children of node ") +
                                 path, 
                                 rc,
                                 m_state == AS_CONNECTED);
    } else {
        for (int32_t i = 0; i < children.count; ++i) {
            //convert each child's path from relative to absolute 
            string absPath(path);
            if (path != "/") {
                absPath.append("/");
            } 
            absPath.append(children.data[i]); 
            nodeList.push_back(absPath);
        }
        //make sure the order is always deterministic
        sort(nodeList.begin(), nodeList.end());
    }
}

/*
  listener here is an alternative listener.  If chosen then the events
  will not only go to this listener, not any default listeners.
 */

string
ZooKeeperAdapter::getNodeData(const string &path,
                              ZKEventListener *listener,
                              void *context, 
                              Stat *stat)
{
    TRACE(LOG, "getNodeData");

    validatePath(path);
   
    const int32_t MAX_DATA_LENGTH = 128 * 1024;
    char buffer[MAX_DATA_LENGTH];
    memset(buffer, 0, MAX_DATA_LENGTH);
    struct Stat tmpStat;
    if (stat == NULL) {
        stat = &tmpStat;
    }
    memset(stat, 0, sizeof(Stat));
    
    int32_t rc;
    int32_t len;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        len = MAX_DATA_LENGTH - 1;
        if (context != NULL) {
            m_zkContextsMutex.acquire();
            rc = zoo_get(mp_zkHandle, 
                         path.c_str(),
                         (listener != NULL ? 1 : 0),
                         buffer, &len, stat);
            if (rc == ZOK) {
                registerContext(GET_NODE_DATA, path, listener, context);
            }
            m_zkContextsMutex.release();
        } else {
	    if (listener) {
		throw ZooKeeperException(
                    "A watch can be set only if the context is not NULL",
                    -1,
                    m_state == AS_CONNECTED);
            }
            rc = zoo_get(mp_zkHandle,
                         path.c_str(),
                         (listener != NULL ? 1 : 0),
                         buffer, &len, stat);
        }
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(
            string("Unable to get data of node ") + path,
            rc,
            m_state == AS_CONNECTED);
    } else {
        return string(buffer, buffer + len);
    }
}

void
ZooKeeperAdapter::setNodeData(const string &path,
                              const string &value,
                              int32_t version,
                              Stat *stat)
{
    TRACE(LOG, "setNodeData");

    validatePath(path);

    int32_t rc;
    RetryHandler rh(m_zkConfig);
    do {
        verifyConnection();
        rc = zoo_set2(mp_zkHandle,
                      path.c_str(),
                      value.c_str(),
                      value.length(), 
                      version,
                      stat);
    } while ((rc != ZOK) && (rh.handleRC(rc)));
    if (rc != ZOK) {
        LOG_ERROR(LOG, 
                  "Error %d for %s", 
                  rc, 
                  path.c_str());
        throw ZooKeeperException(string("Unable to set data for node ") +
                                 path,
                                 rc,
                                 m_state == AS_CONNECTED);
    }
}

}   /* end of 'namespace zk' */

