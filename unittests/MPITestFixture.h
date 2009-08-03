#ifndef MPITESTFIXTURE_H
#define MPITESTFIXTURE_H

#include <mpi.h>
#include <cppunit/extensions/HelperMacros.h>
#include <clusterlib.h>

#define MPI_TAG 1000

/**
 * Replacement for CPPUNIT_ASSERT() that will record error points but
 * not exit the function it is called in.  You can call this anywhere
 * in the test and it will record the error point and move foward.
 * Finally, in cleanAndBarrierMPITest() the errors will be printed.
 * Therefore, do not do any group communication after
 * cleanAndBarrierMPITest() is called.  This macro solves the problem
 * where group communication is expected by CPPUNIT_ASSERT() but
 * CPPUNIT_ASSERT exits the test function without executing the
 * communication expected.
 */

#define MPI_CPPUNIT_ASSERT(_condition) \
{ \
    if (!(_condition)) { \
        addErrorFileLine(__FILE__, __LINE__); \
    } \
} 

/**
 * MPI-enhanced test fixture.
 *
 * All tests fall into 3 categories.
 * 1) Run with exactly N processes
 *    - To guarantee that only these processes are interacting, wrap all 
 *      logic with isMyRank() code blocks.
 *    - When not enough processes are present, all isMyRank() will return false
 *      and waitsForOrder() and allWaitsForOrder() will return immediately.
 *    - Make sure to set singleProcessMode to false.
 * 2) Run with either N or 1 proccess
 *    - If N processes are not met, the all logic will only match process 0.
 *    - Make sure to set singleProcessMode to true.
 *    - Wrap all logic with isMyRank() 
 * 3) Run with any number of processes
 *    - Can use isMyRank() but isn't required as it is intended for all 
 *      available processes
 *    - Make sure to set minSize to -1.  singleProcessMode is irrelevant.
 */
class MPITestFixture : public CppUnit::TestFixture {
  public:
    MPITestFixture() : 
	m_rank(MPI::COMM_WORLD.Get_rank()),
	m_size(MPI::COMM_WORLD.Get_size()),
        m_testSingleProcessMode(false),
        m_testMinSize(-1) {}

    virtual ~MPITestFixture() {}

    /**
     * Must be called prior to the beginning of any test.  It barriers
     * and sets up the arguments to be used for the remainder of the
     * test.
     *
     * @param minSize If == -1, it runs with any number of processes. If 
     *        != -1, it requires exactly this many processes.
     * @param singleProcessMode supports running when only less than minSize 
     *        processes exist.  Process 0 is the only process that meets all
     *        isMyRank() and allWaitsForOrder() conditions.
     * @param factory the factory pointer (if NULL, barrier is called without 
     *        the clusterlib sync)
     * @param clusterlibSync use the clusterlib sync with the barrier?
     * @param testName if not empty, prints a "testName: started"
     */
    void initializeAndBarrierMPITest(int32_t minSize, 
                                     bool singleProcessMode, 
                                     clusterlib::Factory *factory,
                                     bool clusterlibSync,
                                     std::string testName = "")
    {
        m_testMinSize = minSize;
        m_testSingleProcessMode = singleProcessMode;
        m_testName = testName;

        if (m_testName.empty() == false) {
            std::cerr << m_testName << ": initialized" << std::endl;
        }

        if (factory) {
            barrier(factory, true);
        }
        else {
            barrier(NULL, false);
        }
    }
    
    /**
     * Must be called prior to the ending of any test.  It barriers so
     * that all outstanding clusterlib events are propagated before
     * the end of the test.  It also resets the minSize and testName.
     * It should be called in the tearDown() method.  It prints the
     * test completed output if set in initializeAndBarrierMPITest().
     * No function should be called after it in the tearDown() method.
     *
     * @param factory the factory pointer (if NULL, barrier is called without 
     *        the clusterlib sync)
     * @param clusterlibSync use the clusterlib sync with the barrier?
     */
    void cleanAndBarrierMPITest(clusterlib::Factory *factory,
                                bool clusterlibSync)
    {
        if (factory) {
            barrier(factory, true);
        }
        else {
            barrier(NULL, false);
        }
        
        if (m_testName.empty() == false) {
            std::cerr << m_testName << ": finished" << std::endl;
        }

        /*
         * Reset test parameters
         */
        m_testMinSize = -1;
        m_testName.clear();

        /*
         * Print out the errors if there were any with
         * CPPUNIT_ASSERT_MESSAGE.
         */
        if (m_testError.empty() == false) {
            std::string testError(m_testError);
            m_testError.clear();
            CPPUNIT_ASSERT_MESSAGE(testError, false);
        }
    }

    /** 
     * Ensure that all processes have reached this point before
     * continuing.  Only guarantees clusterlib event synchronization
     * if parameters are set appropriately.
     *
     * @param factory pointer to the factory
     * @param clusterlibSync ensure that all processes have received 
     *        all cluserlib updates
     */
    void barrier(clusterlib::Factory *factory = NULL, 
                 bool clusterlibSync = false)
    {
	/* Synchronize */
	MPI::COMM_WORLD.Barrier();
        if (clusterlibSync && factory) {
            factory->synchronize();
            MPI::COMM_WORLD.Barrier();
            factory->synchronize();
            MPI::COMM_WORLD.Barrier();
        }
    }

    /**
     * Determine if my rank matches the rank parameter.  It returns
     * true only if the ranks match or (this is rank 0 and
     * singleProcessMode is set and we don't have enough processes).
     *
     * @param rank check to see if my rank matches this
     */
    bool isMyRank(int rank) 
    {
        /*
         * Return true if singleTestProcessMode is set and there
         * aren't enough processes and this is process 0.
         */
        if (getTestSingleProcessMode() &&
            (getTestMinSize() != -1) &&
            (getTestMinSize() > getSize()) &&
            (getRank() == 0)) {
            return true;
        }
        else if (getRank() == rank) {
            return true;
        }
        
        return false;
    }

    /** 
     * Ensures that one process waits until another has signaled it.
     * If clusterlibSync is set, all clusterlib events that have been
     * seen by process procFirst have been seen by procSecond.  If
     * singleProcessMode was set and there aren't enough processes, it
     * simply bypasses the messaging.
     *
     * @param procFirst the first process that sends a message when completed
     * @param procSecond the process that waits for procFirst
     * @param factory the factory object
     * @param clusterlibSync ensure that all processes have received 
     *        all cluserlib updates
     */
    void waitsForOrder(int procFirst, 
                       int procSecond,
                       clusterlib::Factory *factory = NULL,
                       bool clusterlibSync = false)

    {
        /*
         * Don't do anything messaging for single process mode when
         * there aren't enough processes.
         */
        if (getTestSingleProcessMode() &&
            (getTestMinSize() != -1) &&
            (getTestMinSize() > getSize())) {
            if (clusterlibSync) {
                factory->synchronize();
            }
            return;
        }

	assert((procFirst < getSize()) && (procFirst >= 0));
	assert((procSecond < getSize()) && (procSecond >= 0));
	assert(procFirst != procSecond);

	if (getRank() == procFirst) {
            if (clusterlibSync) {
                factory->synchronize();
            }
	    MPI::COMM_WORLD.Ssend(NULL, 0, MPI::BYTE, procSecond, MPI_TAG);
	}
	else if (getRank() == procSecond) {
	    MPI::COMM_WORLD.Recv(NULL, 0, MPI::BYTE, procFirst, MPI_TAG);
            if (clusterlibSync) {
                factory->synchronize();
            }
	}

        std::cerr << "waitsForOrder: proper waiting done with " 
                  << getTestMinSize()
                  << " processes exist and need " << getSize() << std::endl;
    }

    /** 
     * Ensures that all processes (except procFirst) wait until
     * another has signaled it.  If clusterlibSync is set, all
     * clusterlib events that have been seen by process procFirst have
     * been seen by all other processes.  If singleProcessMode was set
     * and there aren't enough processes, it simply bypasses this.
     *
     * @param procFirst the first process that sends a message when completed
     * @param factory the factory object
     * @param clusterlibSync ensure that all processes have received 
     *        all cluserlib updates
     * @param singleProcessMode supports running when only one process exists 
     *                          by having one process do all work
     */
    void allWaitsForOrder(int procFirst,
                          clusterlib::Factory *factory,
                          bool clusterlibSync = false)
    {
        /*
         * Don't do anything messaing for single process mode when
         * there aren't enough processes.
         */
        if (getTestSingleProcessMode() &&
            (getTestMinSize() != -1) &&
            (getTestMinSize() > getSize())) {
            if (clusterlibSync) {
                factory->synchronize();
            }

            return;
        }

        assert(procFirst < getSize());
        if (getRank() == procFirst) {
            if (clusterlibSync) {
                factory->synchronize();
            }
        }
        MPI::COMM_WORLD.Bcast(NULL, 0, MPI::BYTE, procFirst);
        if (getRank() != procFirst) {
            if (clusterlibSync) {
                factory->synchronize();
            }
        }
    }

    /**
     * Do not use this function directly, it helps
     * INIT_BARRIER_MPI_TEST_OR_DONE
     */
    bool getTestSingleProcessMode() const 
    {
        return m_testSingleProcessMode; 
    }

    /**
     * Do not use this function directly, it helps
     * INIT_BARRIER_MPI_TEST_OR_DONE
     */
    void setTestSingleProcessMode(bool testSingleProcessMode)
    { 
        m_testSingleProcessMode = testSingleProcessMode;
    }

    /**
     * Do not use this function directly, it helps
     * INIT_BARRIER_MPI_TEST_OR_DONE
     */
    void setTestMinSize(int testMinSize)
    { 
        m_testMinSize = testMinSize;
    }

    /**
     * Do not use this function directly, it helps
     * INIT_BARRIER_MPI_TEST_OR_DONE
     */
    int getSize() const { return m_size; }

    /** 
     * Get the rank of your process.  If MPI was started with n
     * processes, the rank will be in the range of 0 to n-1.
     */
    int getRank() const { return m_rank; }

    /**
     * Add an error message for this test for file and line number.
     * Used by MPI_CPPUNIT_ASSERT.
     *
     * @param file name of the file the error happened in
     * @param line the line number where the error happened.
     */
    void addErrorFileLine(const std::string &file, int32_t line)
    {
        std::stringstream ss;
        ss << "Assert failed in file: " << file << ", line: " 
           << line << std::endl;
        if (m_testError.empty()) {
            m_testError.append("\n");
        }
        m_testError.append(ss.str());
    }

  private:

    int getTestMinSize() const { return m_testMinSize; }

    /**
     * My process rank
     */
    int m_rank;

    /**
     * Total number of processes available
     */
    int m_size;

    /**
     * Should be initialized by each test trhough
     * INIT_BARRIER_MPI_TEST_OR_DONE.  Does this test support single
     * process mode?
     */
    bool m_testSingleProcessMode;

    /**
     * Should be initialized by each test trhough
     * INIT_BARRIER_MPI_TEST_OR_DONE.  How many processes are required
     * for this test to run?
     */     
    int m_testMinSize;

    /**
     * Should be initialized by each test through
     * INIT_BARRIER_MPI_TEST_OR_DONE. Name of this test.
     */
    std::string m_testName;

    /**
     * This saves the first error that this test ran into.  It should
     * be outputted by the cleanAndBarrierMPITest() in the tearDown().
     */
    std::string m_testError;
};

#endif
