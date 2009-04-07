#include "MPITestFixture.h"
#include "testparams.h"

extern TestParams globalTestParams;

using namespace std;
using namespace clusterlib;

/*
 * Forward decl needed for the timer & user
 * event handlers.
 */
class ClusterlibCache;

class CacheClusterEventHandler
    : public ClusterEventHandler
{
  public:
    CacheClusterEventHandler(Client *cp,
                             Event mask,
                             Notifyable *np)
        : ClusterEventHandler(np, mask, cp),
          counter(0),
          lastEvent(EN_NOEVENT)
    {
    }

    int32_t getCounter() { return counter; }
    void setCounter(int32_t newval) { counter = newval; }

    Event getLastEvent() { return lastEvent; }

    void handleClusterEvent(Event e)
    {
        counter++;
        lastEvent = e;
    }

  private:
    /*
     * Count how many times this handler was called.
     */
    int32_t counter;

    /*
     * Last event fired.
     */
    Event lastEvent;
};

/*
 * The test class itself.
 */
class ClusterlibCache
    : public MPITestFixture
{
    CPPUNIT_TEST_SUITE( ClusterlibCache );
    CPPUNIT_TEST( testCache1 );
    CPPUNIT_TEST( testCache2 );
    CPPUNIT_TEST( testCache3 );
    CPPUNIT_TEST( testCache4 );
    CPPUNIT_TEST( testCache5 );
    CPPUNIT_TEST( testCache6 );
    CPPUNIT_TEST( testCache7 );
    CPPUNIT_TEST( testCache8 );
    CPPUNIT_TEST( testCache9 );
    CPPUNIT_TEST( testCache10 );
    CPPUNIT_TEST( testCache11 );
    CPPUNIT_TEST( testCache12 );
    CPPUNIT_TEST( testCache13 );
    CPPUNIT_TEST( testCache14 );
    CPPUNIT_TEST( testCache15 );
    CPPUNIT_TEST( testCache16 );
    CPPUNIT_TEST( testCache17 );
    CPPUNIT_TEST( testCache18 );
    CPPUNIT_TEST( testCache19 );

    CPPUNIT_TEST_SUITE_END();

  public:
    
    ClusterlibCache()
        : _factory(NULL),
          _client0(NULL),
          _app0(NULL),
          _grp0(NULL),
          _nod0(NULL),
          _dist0(NULL),
          _zk(NULL)
    {
    }

    /* Runs prior to all tests */
    virtual void setUp() 
    {
	_factory =
            new Factory(globalTestParams.getZkServerPortList());

	CPPUNIT_ASSERT(_factory != NULL);
        _zk = _factory->getRepository();
        CPPUNIT_ASSERT(_zk != NULL);
	_client0 = _factory->createClient();
	CPPUNIT_ASSERT(_client0 != NULL);
        _app0 = _client0->getRoot()->getApplication("foo-app", true);
        CPPUNIT_ASSERT(_app0 != NULL);
        _grp0 = _app0->getGroup("bar-group", true);
        CPPUNIT_ASSERT(_grp0 != NULL);
        _nod0 = _grp0->getNode("nod3", true);
        CPPUNIT_ASSERT(_nod0 != NULL);
        _dist0 = _grp0->getDataDistribution("dist1", true);
        CPPUNIT_ASSERT(_dist0 != NULL);
    }

    /* Runs after all tests */
    virtual void tearDown() 
    {
	cerr << "delete called " << endl;

        /*
         * Delete only the factory, that automatically deletes
         * all the other objects.
         */
	delete _factory;
        _factory = NULL;
        _client0 = NULL;
        _app0 = NULL;
        _grp0 = NULL;
        _nod0 = NULL;
        _dist0 = NULL;
    }

    void testCache1()
    {
        /*
         * Set the health report of a node and see if the cache
         * is updated.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 1" << endl;

        string hrpath =
            _nod0->getKey() +
            "/" +
            "clientState";
        _zk->setNodeData(hrpath, "healthy");
        _factory->synchronize();
        CPPUNIT_ASSERT(string("healthy") == _nod0->getClientState());

        cerr << "Test 1 end" << endl;
    }
    void testCache2()
    {
        /*
         * Test ready protocol.
         *
         * Set and unset the ready bit on a node and see if the
         * cache is updated. Same on group, application, and
         * data distribution.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 2" << endl;

        /*
         * This test needs to be rewritten for the new
         * implementation of the READY protocol, punt for
         * now.
         */

        cerr << "Test 2 end" << endl;
    }
    void testCache3()
    {
        /*
         * Testing whether groups change notification works
         * in an application.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 3 " << endl;

        string rpath =
            _app0->getKey() +
            "/" +
            "groups" +
            "/" +
            "g15";
        bool found = false;

        /*
         * Make sure the group does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * groups in _app0.
         */
        NameList gnl = _app0->getGroupNames();

        /*
         * Create the new group..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();
        
        /*
         * Now check that the new group appears.
         */
        gnl = _app0->getGroupNames();
        NameList::iterator nlIt;

        for (nlIt = gnl.begin(); nlIt != gnl.end(); nlIt++) {
            if ((*nlIt) == "g15") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        Group *groupP = _app0->getGroup("g15");

        CPPUNIT_ASSERT(groupP->getState() == Notifyable::INIT);

        cerr << "Test 3 end" << endl;
    }
    void testCache4()
    {
        /*
         * Testing whether distribution change notification works
         * in an application.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 4" << endl;

        string rpath =
            _app0->getKey() +
            "/" +
            "distributions" +
            "/" +
            "d15";
        bool found = false;

        /*
         * Make sure the distribution does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * distributions in _app0.
         */
        NameList ddnl = _app0->getDataDistributionNames();

        /*
         * Create the new data distribution..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new distribution appears.
         */
        ddnl = _app0->getDataDistributionNames();

        NameList::iterator nlIt;

        for (nlIt = ddnl.begin(); nlIt != ddnl.end(); nlIt++) {
            if ((*nlIt) == "d15") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        DataDistribution *distP = _app0->getDataDistribution("d15");

        CPPUNIT_ASSERT(distP->getState() == Notifyable::INIT);

        cerr << "Test 4 end" << endl;
    }
    void testCache5()
    {
        /*
         * Test whether node membership change notification works
         * in a group.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 5" << endl;

        string rpath = 
            _grp0->getKey() +
            "/" +
            "nodes" +
            "/" +
            "n111";
        bool found = false;

        /*
         * Make sure the node does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * nodes in _grp0.
         */
        NameList nnl = _grp0->getNodeNames();

        /*
         * Create a new node..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new node appears.
         */
        nnl = _grp0->getNodeNames();

        NameList::iterator nlIt;

        for (nlIt = nnl.begin(); nlIt != nnl.end(); nlIt++) {
            if ((*nlIt) == "n111") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        Node *np = _grp0->getNode("n111");

        CPPUNIT_ASSERT(np->getState() == Notifyable::INIT);

        cerr << "Test 5 end" << endl;
    }

    void testCache6()
    {
        /*
         * Testing whether distribution change notification works
         * in a group.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 6" << endl;

        string rpath =
            _grp0->getKey() +
            "/" +
            "distributions" +
            "/" +
            "d15";
        bool found = false;

        /*
         * Make sure the distribution does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * distributions in _grp0.
         */
        NameList ddnl = _grp0->getDataDistributionNames();

        /*
         * Create the new data distribution..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new distribution appears.
         */
        ddnl = _grp0->getDataDistributionNames();

        NameList::iterator nlIt;

        for (nlIt = ddnl.begin(); nlIt != ddnl.end(); nlIt++) {
            if ((*nlIt) == "d15") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        DataDistribution *distP = _grp0->getDataDistribution("d15");

        CPPUNIT_ASSERT(distP->getState() == Notifyable::INIT);

        cerr << "Test 6 end" << endl;
    }
    void testCache7()
    {
        /*
         * Test whether node membership change notification works
         * in an application.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 7" << endl;

        string rpath = 
            _app0->getKey() +
            "/" +
            "nodes" +
            "/" +
            "n111";
        bool found = false;

        /*
         * Make sure the node does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * nodes in _app0.
         */
        NameList nnl = _app0->getNodeNames();

        /*
         * Create a new node..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the new node appears.
         */
        nnl = _app0->getNodeNames();

        NameList::iterator nlIt;
        
        for (nlIt = nnl.begin(); nlIt != nnl.end(); nlIt++) {
            if ((*nlIt) == "n111") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        Node *np = _app0->getNode("n111");

        CPPUNIT_ASSERT(np->getState() == Notifyable::INIT);

        cerr << "Test 7 end" << endl;
    }
    void testCache8()
    {
        /*
         * Testing whether groups change notification works
         * in a group.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 8 " << endl;

        string rpath =
            _grp0->getKey() +
            "/" +
            "groups" +
            "/" +
            "g15";
        bool found = false;

        /*
         * Make sure the group does not exist when we start.
         */
        try {
            _zk->deleteNode(rpath, true, -1);
        } catch (...) {
        }

        /*
         * Force clusterlib to inform us of any changes in the
         * groups in _app0.
         */
        NameList gnl = _grp0->getGroupNames();

        /*
         * Create the new group..
         */
        _zk->createNode(rpath, "", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();
        
        /*
         * Now check that the new group appears.
         */
        gnl = _grp0->getGroupNames();

        NameList::iterator nlIt;

        for (nlIt = gnl.begin(); nlIt != gnl.end(); nlIt++) {
            if ((*nlIt) == "g15") {
                found = true;
                break;
            }
        }

        CPPUNIT_ASSERT(found == true);

        Group *groupP = _grp0->getGroup("g15");

        CPPUNIT_ASSERT(groupP->getState() == Notifyable::INIT);

        cerr << "Test 8 end" << endl;
    }
    void testCache9()
    {
        /*
         * Test whether node connectivity notification
         * works.
         */

        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 9" << endl;

        CPPUNIT_ASSERT(_nod0->isConnected() == false);

        string rpath =
            _nod0->getKey() +
            "/" +
            "connected";

        /*
         * Create the connectivity znode.
         */
        _zk->createNode(rpath, "yes", 0, false);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the node is "connected".
         */
        CPPUNIT_ASSERT(_nod0->isConnected() == true);

        /*
         * Delete the connectivity znode.
         */
        _zk->deleteNode(rpath, true, -1);

        /*
         * Wait for event propagation.
         */
        _factory->synchronize();

        /*
         * Now check that the node is not "connected".
         */
        CPPUNIT_ASSERT(_nod0->isConnected() == false);

        cerr << "Test 9 end" << endl;
    }
    void testCache10()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 10" << endl;

        cerr << "Test 10 end" << endl;
    }
    void testCache11()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 11" << endl;

        cerr << "Test 11 end" << endl;
    }
    void testCache12()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 12" << endl;

        cerr << "Test 12 end" << endl;
    }
    void testCache13()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 13" << endl;

        cerr << "Test 13 end" << endl;
    }
    void testCache14()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 14" << endl;

        cerr << "Test 14 end" << endl;
    }
    void testCache15()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 15" << endl;

        cerr << "Test 15 end" << endl;
    }
    void testCache16()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 16" << endl;

        cerr << "Test 16 end" << endl;
    }
    void testCache17()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 17" << endl;

        cerr << "Test 17 end" << endl;
    }
    void testCache18()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 18" << endl;

        cerr << "Test 18 end" << endl;
    }
    void testCache19()
    {
        INIT_BARRIER_MPI_TEST_OR_DONE(-1, true, _factory);

        if (!isMyRank(0)) {
            return;
        }

        cerr << "Test 19" << endl;

        cerr << "Test 19 end" << endl;
    }

  private:
    Factory *_factory;
    Client *_client0;
    Application *_app0;
    Group *_grp0;
    Node *_nod0;
    DataDistribution *_dist0;
    zk::ZooKeeperAdapter *_zk;
};

/* Registers the fixture into the 'registry' */
CPPUNIT_TEST_SUITE_REGISTRATION( ClusterlibCache );

