/*
 * clusterlib.h --
 *
 * The main include file for users of clusterlib.
 *
 * $Header$
 * $Revision$
 * $Date$
 */

#ifndef	_CLUSTERLIB_H_
#define	_CLUSTERLIB_H_

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <iostream>
#include <pthread.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace std;

#include "forwarddecls.h"

using namespace clusterlib;

#include "log.h"
#include "clusterexception.h"
#include "blockingqueue.h"
#include "mutex.h"
#include "thread.h"
#include "event.h"
#include "zkadapter.h"

using namespace zk;

#include "notifyable.h"
#include "healthchecker.h"
#include "clusterclient.h"
#include "clusterserver.h"
#include "application.h"
#include "group.h"
#include "node.h"
#include "datadistribution.h"

DEFINE_LOGGER( CL_LOG, "clusterlib" )

namespace clusterlib
{

class ClusterlibStrings
{
  public:
    /*
     * All string constants used to name ZK nodes.
     */
    static const string ROOTNODE;
    static const string PATHSEPARATOR;

    static const string CLUSTERLIB;
    static const string VERSION;

    static const string PROPERTIES;
    static const string CONFIGURATION;
    static const string ALERTS;

    static const string APPLICATIONS;
    static const string GROUPS;
    static const string NODES;
    static const string UNMANAGEDNODES;
    static const string DISTRIBUTIONS;

    static const string CLIENTSTATE;
    static const string CLIENTSTATEDESC;
    static const string ADDRESS;
    static const string LASTCONNECTED;
    static const string CLIENTVERSION;
    static const string CONNECTED;
    static const string BOUNCY;
    static const string READY;
    static const string ALIVE;
    static const string MASTERSETSTATE;
    static const string SUPPORTEDVERSIONS;

    static const string ELECTIONS;
    static const string BIDS;
    static const string CURRENTLEADER;

    static const string SHARDS;
    static const string GOLDENSHARDS;
    static const string MANUALOVERRIDES;

    static const string LOCKS;
    static const string QUEUES;
    static const string BARRIERS;
    static const string TRANSACTIONS;

    /*
     * Strings used as values of ZK nodes (or parts of values).
     */
    static const string BIDPREFIX;
    static const string INFLUX;
    static const string HEALTHY;
    static const string UNHEALTHY;

    /*
     * Names associated with the special clusterlib master
     * application.
     */
    static const string MASTER;

    /*
     * Default constructor.
     */
    ClusterlibStrings() {}
};

/*
 * The actual factory class.
 */
class Factory
    : public virtual ZKEventListener,
      public virtual ClusterlibStrings
{
  public:
    /*
     * This type defines a Factory callback function
     * that handles a particular event.
     */
    typedef EventHandlerWrapper<Factory> FactoryEventHandler;

    /*
     * Create a factory instance, connect it to
     * the specified cluster registry.
     */
    Factory(const string &registry);

    /*
     * Destructor.
     */
    virtual ~Factory();

    /*
     * Create a cluster client object.
     */
    ClusterClient *createClient();

    /*
     * Create a cluster server object. Also
     * create the needed registration if createReg
     * is set to true.
     */
    ClusterServer *createServer(const string &app,
                                const string &group,
                                const string &node,
                                HealthChecker *checker,
                                bool createReg = false);

    /*
     * This API must be provided because of inheritance from
     * EventListener.
     */
    void eventReceived(const ZKEventSource &zksrc, const ZKWatcherEvent &e);

  private:
    /*
     * Friend declaration for FactoryOps so it'll have
     * access to all these private operations.
     */
    friend class FactoryOps;

    /*
     * Manage interests in events.
     */
    void addInterests(const string &key, 
                      Notifyable *nrp,
                      const Event events);
    void removeInterests(const string &key, const Event events);

    /*
     * Retrieve (and potentially create) instances of
     * objects representing applications, groups, nodes,
     * and distributions.
     */
    Application *getApplication(const string &name);

    Group *getGroup(const string &name, Application *app);
    Group *getGroup(const string &appName,
                    const string &grpName);

    Node *getNode(const string &name, Group *grp, bool managed);
    Node *getNode(const string &appName,
                  const string &grpName,
                  const string &nodeName,
                  bool managed);

    DataDistribution *getDistribution(const string &name,
                                      Application *app);
    DataDistribution *getDistribution(const string &appName,
                                      const string &distName);

    Shard *createShard(DataDistribution *dp,
                       const string &startRange,
                       const string &endRange,
                       const string &appName,
                       const string &grpName,
                       const string &nodeName);

    string createNodeKey(const string &appName,
                         const string &groupName,
                         const string &nodeName,
                         bool managed);
    string createGroupKey(const string &appName,
                          const string &groupName);
    string createAppKey(const string &appName);
    string createDistKey(const string &appName,
                         const string &distName);

    bool isNodeKey(const string &key, bool *managedP = NULL);
    bool hasNodeKeyPrefix(const string &key, bool *managedP = NULL);
    bool hasNodeKeyPrefix(vector<string> &components, bool *managedP = NULL);
    string getNodeKeyPrefix(const string &key, bool *managedP = NULL);
    string getNodeKeyPrefix(vector<string> &components);

    bool isGroupKey(const string &key);
    bool hasGroupKeyPrefix(const string &key);
    bool hasGroupKeyPrefix(vector<string> &components);
    string getGroupKeyPrefix(const string &key);
    string getGroupKeyPrefix(vector<string> &components);

    bool isAppKey(const string &key);
    bool hasAppKeyPrefix(const string &key);
    bool hasAppKeyPrefix(vector<string> &components);
    string getAppKeyPrefix(const string &key);
    string getAppKeyPrefix(vector<string> &components);

    bool isDistKey(const string &key);
    bool hasDistKeyPrefix(const string &key);
    bool hasDistKeyPrefix(vector<string> &components);
    string getDistKeyPrefix(const string &key);
    string getDistKeyPrefix(vector<string> &components);

    /*
     * Load entities from ZooKeeper.
     */
    Application *loadApplication(const string &name,
                                 const string &key);
    DataDistribution *loadDistribution(const string &name,
                                       const string &key,
                                       Application *app);
    Group *loadGroup(const string &name,
                     const string &key,
                     Application *app);
    Node *loadNode(const string &name,
                   const string &key,
                   Group *grp);

    void fillApplicationMap(ApplicationMap *amp);
    void fillGroupMap(GroupMap *gmp, Application *app);
    void fillNodeMap(NodeMap *nmp, Group *grp);
    void fillDataDistributionMap(DataDistributionMap *dmp,
                                 Application *app);

    /*
     * Did we already fill the application map?
     */
    bool filledApplicationMap() { return m_filledApplicationMap; }
    void setFilledApplicationMap(bool v) { m_filledApplicationMap = v; }

    /*
     * Create entities in ZooKeeper.
     */
    Application *createApplication(const string &key);
    DataDistribution *createDistribution(const string &key,
                                         const string &marshalledShards,
                                         const string &marshalledManualOverrides,
                                         Application *app);
    Group *createGroup(const string &key, Application *app);
    Node *createNode(const string &key, Group *grp);

  private:
    /*
     * The factory ops delegator.
     */
    FactoryOps *mp_ops;

    /*
     * The registry of cached data distributions.
     */
    DataDistributionMap m_dataDistributions;
    Mutex m_ddLock;

    /*
     * The registry of cached applications.
     */
    ApplicationMap m_applications;
    Mutex m_appLock;

    /*
     * The registry of cached groups.
     */
    GroupMap m_groups;
    Mutex m_grpLock;

    /*
     * The registry of cached nodes.
     */
    NodeMap m_nodes;
    Mutex m_nodeLock;

    /*
     * The ZooKeeper adapter object being used.
     */
    ZooKeeperAdapter *mp_zk;

    class InterestRecord
    {
      public:
        /*
         * Constructor.
         */
        InterestRecord(Notifyable *nrp, Event e)
            : mp_nrp(nrp),
              m_e(e)
        {
        };

        /*
         * Retrieve the elements of the record.
         */
        Event getInterests() { return m_e; }
        Notifyable *getNotifyable() { return mp_nrp; }

        /*
         * Manage the selection of which notifications
         * are delivered.
         */
        void addInterests(Event e)
        {
            m_e |= e;
        }
        void removeInterests(Event e)
        {
            m_e &= (~(e));
        }

      private:
        /*
         * The notifyable object to deliver events to.
         */
        Notifyable *mp_nrp;

        /*
         * The events of interest.
         */
        Event m_e;
    };
    typedef map<string, InterestRecord *> NotificationInterestsMap;

    /*
     * Map from keys to notification interests records.
     */
    NotificationInterestsMap m_notificationInterests;
    Mutex m_notificationLock;

    /*
     * Did we already fill the application map?
     */
    bool m_filledApplicationMap;
};

/*
 * Definition and implementation of class FactoryOps. An
 * instance of this class is given to internal objects so
 * that these objects can call operations on the factory
 * that shouldn't be generally available.
 */
class FactoryOps
{
  public:
    void addInterests(Notifyable *nrp, Event events)
    {
        mp_f->addInterests(nrp->getKey(), nrp, events);
    }
    void removeInterests(Notifyable *nrp, Event events)
    {
        mp_f->removeInterests(nrp->getKey(), events);
    }

    Application *getApplication(const string &name)
    {
        return mp_f->getApplication(name);
    }
    void fillApplicationMap(ApplicationMap *amp)
    {
        if (mp_f->filledApplicationMap() == false) {
            mp_f->fillApplicationMap(amp);
        }
    }

    Group *getGroup(const string &name, Application *app)
    {
        return mp_f->getGroup(name, app);
    }
    Group *getGroup(const string &appName, const string &grpName)
    {
        return mp_f->getGroup(appName, grpName);
    }
    void fillGroupMap(GroupMap *gmp, Application *app)
    {
        if (app->filledGroupMap() == false) {
            mp_f->fillGroupMap(gmp, app);
        }
    }

    Node *getNode(const string &name, Group *grp, bool managed)
    {
        return mp_f->getNode(name, grp, managed);
    }
    Node *getNode(const string &appName,
                  const string &grpName,
                  const string &nodeName,
                  bool managed)
    {
        return mp_f->getNode(appName, grpName, nodeName, managed);
    }
    void fillNodeMap(NodeMap *nmp, Group *grp)
    {
        if (grp->filledNodeMap() == false) {
            mp_f->fillNodeMap(nmp, grp);
        }
    }

    DataDistribution *getDistribution(const string &name,
                                      Application *app)
    {
        return mp_f->getDistribution(name, app);
    }
    DataDistribution *getDistribution(const string &appName,
                                      const string &distName)
    {
        return mp_f->getDistribution(appName, distName);
    }
    void fillDataDistributionMap(DataDistributionMap *dmp,
                                 Application *app)
    {
        if (app->filledDataDistributionMap() == false) {
            mp_f->fillDataDistributionMap(dmp, app);
        }
    }

    Shard *createShard(DataDistribution *dp,
                       const string &beginRange,
                       const string &endRange,
                       const string &appName,
                       const string &grpName,
                       const string &nodeName)
    {
        return mp_f->createShard(dp,
                                 beginRange,
                                 endRange,
                                 appName,
                                 grpName,
                                 nodeName);
    }

    string createNodeKey(const string &appName,
                         const string &groupName,
                         const string &nodeName,
                         bool managed)
    {
        return mp_f->createNodeKey(appName,
                                   groupName,
                                   nodeName,
                                   managed);
    }
    string createGroupKey(const string &appName,
                          const string &groupName)
    {
        return mp_f->createGroupKey(appName, groupName);
    }
    string createAppKey(const string &appName)
    {
        return mp_f->createAppKey(appName);
    }
    string createDistKey(const string &appName,
                         const string &distName)
    {
        return mp_f->createDistKey(appName, distName);
    }

    bool isNodeKey(const string &key, bool *managedP = NULL)
    {
        return mp_f->isNodeKey(key, managedP);
    }
    bool hasNodeKeyPrefix(const string &key, bool *managedP = NULL)
    {
        return mp_f->hasNodeKeyPrefix(key, managedP);
    }
    bool isGroupKey(const string &key)
    {
        return mp_f->isGroupKey(key);
    }
    bool hasGroupKeyPrefix(const string &key)
    {
        return mp_f->hasGroupKeyPrefix(key);
    }
    bool isAppKey(const string &key)
    {
        return mp_f->isAppKey(key);
    }
    bool hasAppKeyPrefix(const string &key)
    {
        return mp_f->hasAppKeyPrefix(key);
    }
    bool isDistKey(const string &key)
    {
        return mp_f->isDistKey(key);
    }
    bool hasDistKeyPrefix(const string &key)
    {
        return mp_f->hasDistKeyPrefix(key);
    }
    
  private:
    /*
     * Friend declaration for Factory so it can call
     * the constructor.
     */
    friend class Factory;
    
    /*
     * Constructor used by Factory.
     */
    FactoryOps(Factory *f) : mp_f(f) {};

    /*
     * Default constructor throws an exception.
     */
    FactoryOps()
    {
        throw ClusterException("Someone called the default "
                               "FactoryOps constructor!");
    }

  private:
    /*
     * The factory associated with this instance of FactoryOps.
     */
    Factory *mp_f;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_CLUSTERLIB_H_ */
