#include "MPITestFixture.h"
#include "testparams.h"

extern TestParams globalTestParams;

using namespace std;
using namespace clusterlib;

class MyHealthChecker : public HealthChecker {
  public:
    virtual HealthReport checkHealth() {
        return HealthReport(HealthReport::HS_HEALTHY,
                            "No real check");
    }
  private:
};

class ClusterlibHierarchy : public MPITestFixture {
    CPPUNIT_TEST_SUITE(ClusterlibHierarchy);
    CPPUNIT_TEST(testHierarchy1);
    CPPUNIT_TEST(testHierarchy2);
    CPPUNIT_TEST(testHierarchy3);
    CPPUNIT_TEST_SUITE_END();

  public:
    
    ClusterlibHierarchy() : _factoryP(NULL),
                            _clientP(NULL),
                            _appP(NULL) {}
    
    /**
     * Runs prior to all tests 
     */
    virtual void setUp() 
    {
	_factoryP = new Factory(
            globalTestParams.getZkServerPortList());
	CPPUNIT_ASSERT(_factoryP != NULL);
	_clientP = _factoryP->createClient();
	CPPUNIT_ASSERT(_clientP != NULL);
	_appP = _clientP->getRoot()->getApplication("hierarchy-app", true);
	CPPUNIT_ASSERT(_appP != NULL);
    }

    /** 
     * Runs after all tests 
     */
    virtual void tearDown() 
    {
	cerr << "delete called " << endl;
	delete _factoryP;
        _factoryP = NULL;
    }

    /** 
     * Simple test to see if basic group, data distribution, and
     * properties works.  Prefers 2 nodes, but if only one process is
     * available, runs as a single process test.
     */
    void testHierarchy1()
    {
        cerr << "testHierarchy1: started" << endl;
        INIT_BARRIER_MPI_TEST_OR_DONE(2, true, _factoryP);

        if (isMyRank(0)) {
            Group *groupP = _appP->getGroup("hierarchy-group", true);
	    CPPUNIT_ASSERT(groupP);
	    DataDistribution *distP = 
		groupP->getDataDistribution("hierarchy-dist",
					    true);
	    CPPUNIT_ASSERT(distP);
	    Node *nodeP = groupP->getNode("hierarchy-node", true);
	    CPPUNIT_ASSERT(nodeP);
	    Properties *propP = nodeP->getProperties();
	    CPPUNIT_ASSERT(propP);
        }
	
	waitsForOrder(0, 1, _factoryP, true);

        if (isMyRank(1)) {
	    CPPUNIT_ASSERT(_appP);

            /* Applications now have parents! */
            try {
                CPPUNIT_ASSERT(_appP->getMyParent() == _clientP->getRoot());
            } catch (ClusterException &e) { 
                CPPUNIT_ASSERT("UNREACHABLE BECAUSE OF EXCEPTION" == NULL);
                cerr << "Caught appP->getMyParent() exception correctly" 
                     << endl;
            }
            
            try {
                _appP->getMyGroup();
                CPPUNIT_ASSERT("UNREACHABLE BECAUSE OF EXCEPTION" == NULL);
            } catch (ClusterException &e) {                
                cerr << "Caught appP->getMyGroup() exception correctly" 
                     << endl;
            }

	    CPPUNIT_ASSERT(_appP->getMyApplication() == _appP);
	    
	    Group *groupP = _appP->getGroup("hierarchy-group");
	    CPPUNIT_ASSERT(groupP);
	    CPPUNIT_ASSERT(groupP->getMyParent() == _appP);
	    CPPUNIT_ASSERT(groupP->getMyGroup() == _appP);
	    CPPUNIT_ASSERT(groupP->getMyApplication() == _appP);
	    
	    DataDistribution *distP = 
		groupP->getDataDistribution("hierarchy-dist");
	    CPPUNIT_ASSERT(distP);
	    CPPUNIT_ASSERT(distP->getMyParent() == groupP);
	    CPPUNIT_ASSERT(distP->getMyGroup() == groupP);
	    CPPUNIT_ASSERT(distP->getMyApplication() == _appP);

	    Node *nodeP = groupP->getNode("hierarchy-node");
	    CPPUNIT_ASSERT(nodeP);
	    CPPUNIT_ASSERT(nodeP->getMyParent() == groupP);
	    CPPUNIT_ASSERT(nodeP->getMyGroup() == groupP);
	    CPPUNIT_ASSERT(nodeP->getMyApplication() == _appP);
	    
	    Properties *propP = nodeP->getProperties();
	    CPPUNIT_ASSERT(propP);
	    CPPUNIT_ASSERT(propP->getMyParent() == nodeP);
	    CPPUNIT_ASSERT(propP->getMyGroup() == groupP);
	    CPPUNIT_ASSERT(propP->getMyApplication() == _appP);
        }
	
	cerr << "testGetHierarchy1: done" << endl;
    }

    /** 
     * Try to create a large hierarchy of groups with nodes at every
     * level.  Prefers 2 nodes, but if only one process is available,
     * runs as a single process test.
     */
    void testHierarchy2()
    {
        cerr << "testHierarchy2: started" << endl;
        INIT_BARRIER_MPI_TEST_OR_DONE(2, true, _factoryP);

        if (isMyRank(0)) {
	    Node *node1P = _appP->getNode("node1", true);
	    CPPUNIT_ASSERT(node1P);

	    Group *group1P = _appP->getGroup("hier-group1", true);
	    CPPUNIT_ASSERT(group1P);
	    Node *node2P = group1P->getNode("node2", true);
	    CPPUNIT_ASSERT(node2P);

	    Group *group2P = group1P->getGroup("hier-group2", true);
	    CPPUNIT_ASSERT(group2P);
	    Node *node3P = group2P->getNode("node3", true);
	    CPPUNIT_ASSERT(node3P);

	    Properties *propP = node3P->getProperties();
	    CPPUNIT_ASSERT(propP);
        }
	
	waitsForOrder(0, 1, _factoryP, true);

        if (isMyRank(1)) {
	    CPPUNIT_ASSERT(_appP);

            /* Applications now have parents! */
            try {
                CPPUNIT_ASSERT(_appP->getMyParent() == _clientP->getRoot());
            } catch (ClusterException &e) { 
                CPPUNIT_ASSERT("UNREACHABLE BECAUSE OF EXCEPTION" == NULL);
                cerr << "Caught appP->getMyParent() exception correctly" 
                     << endl;
            }
            
            try {
                _appP->getMyGroup();
                CPPUNIT_ASSERT("UNREACHABLE BECAUSE OF EXCEPTION" == NULL);
            } catch (ClusterException &e) {                
                cerr << "Caught appP->getMyGroup() exception correctly" 
                     << endl;
            }

	    CPPUNIT_ASSERT(_appP->getMyApplication() == _appP);
	    
	    Node *node1P = _appP->getNode("node1");
            CPPUNIT_ASSERT(node1P->getMyParent() == _appP);
            CPPUNIT_ASSERT(node1P->getMyGroup() == _appP);
            CPPUNIT_ASSERT(node1P->getMyApplication() == _appP);

            Group *group1P = _appP->getGroup("hier-group1");
            CPPUNIT_ASSERT(group1P->getMyParent() == _appP);
            CPPUNIT_ASSERT(group1P->getMyGroup() == _appP);
            CPPUNIT_ASSERT(group1P->getMyApplication() == _appP);

	    Node *node2P = group1P->getNode("node2", true);
            CPPUNIT_ASSERT(node2P->getMyParent() == group1P);
            CPPUNIT_ASSERT(node2P->getMyGroup() == group1P);
            CPPUNIT_ASSERT(node2P->getMyApplication() == _appP);

            Group *group2P = group1P->getGroup("hier-group2");
            CPPUNIT_ASSERT(group2P->getMyParent() == group1P);
            CPPUNIT_ASSERT(group2P->getMyGroup() == group1P);
            CPPUNIT_ASSERT(group2P->getMyApplication() == _appP);

	    Node *node3P = group2P->getNode("node3");
            CPPUNIT_ASSERT(node3P->getMyParent() == group2P);
            CPPUNIT_ASSERT(node3P->getMyGroup() == group2P);
            CPPUNIT_ASSERT(node3P->getMyApplication() == _appP);

	    Properties *propP = node3P->getProperties();
            CPPUNIT_ASSERT(propP->getMyParent() == node3P);
            CPPUNIT_ASSERT(propP->getMyGroup() == group2P);
            CPPUNIT_ASSERT(propP->getMyApplication() == _appP);
        }
	
	cerr << "testHierarchy2 passed" << endl;
    }

    /** 
     * Try to create a new application and see if the other process
     * picks it up.  Prefers 2 nodes, but if only one process is
     * available, runs as a single process test.
     */
    void testHierarchy3()
    {
        cerr << "testHierarchy3: started" << endl;
        INIT_BARRIER_MPI_TEST_OR_DONE(2, true, _factoryP);

        uint32_t appCount = 0;
        if (isMyRank(1)) {
            NameList nl =  _clientP->getRoot()->getApplicationNames();
            appCount = nl.size();
        }
        
	waitsForOrder(1, 0, _factoryP, true);

        if (isMyRank(0)) {
            NameList nl =  _clientP->getRoot()->getApplicationNames();
            /* Get the longest name and then add a character to
             * guarantee it's a new one. */
            uint32_t biggestSize = 0, index = 0;
            for (uint32_t i = 0; i < nl.size(); i++) {
                if (i == 0) {
                    biggestSize = nl[i].size();
                    index = i;
                }
                else {
                    if (nl[i].size() > biggestSize) {
                        biggestSize = nl[i].size();
                        index = i;
                    }
                }
            }

            string newkey = nl[index] + "next";
            Application *app0 = 
                _clientP->getRoot()->getApplication(newkey, true);
            CPPUNIT_ASSERT(app0);
        }        
        
	waitsForOrder(0, 1, _factoryP, true);

        if (isMyRank(1)) {
            NameList nl =  _clientP->getRoot()->getApplicationNames();
            cerr << "Orignally had " << appCount << " applications, now has "
                 << nl.size() << " ." << endl; 
            CPPUNIT_ASSERT(appCount + 1 == nl.size());
        }

	cerr << "testHierarchy3 passed" << endl;
    }

  private:
    Factory *_factoryP;
    Client *_clientP;
    Application *_appP;
};

/* Registers the fixture into the 'registry' */
CPPUNIT_TEST_SUITE_REGISTRATION(ClusterlibHierarchy);

