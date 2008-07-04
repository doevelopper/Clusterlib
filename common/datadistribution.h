/*
 * datadistribution.h --
 *
 * Definition of class DataDistribution; it represents a data distribution (mapping
 * from a key to a node) in clusterlib.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef	_DATADISTRIBUTION_H_
#define _DATADISTRIBUTION_H_

namespace clusterlib
{

typedef unsigned long long	HashRange;

/*
 * Definition of class DataDistribution.
 */
class DataDistribution
    : public virtual Notifyable
{
  public:
    /*
     * Enum of hash functions to use.
     */
    enum HashFunctions {
        DD_HF_USERDEF	= 0,
        DD_HF_MD5	= 1,
        DD_HF_JENKINS	= 2,
        DD_HF_END	= 3
    };

    /*
     * Define the type of a hash function.
     */
    typedef HashRange (HashFunction)(const string &key);

    /*
     * Retrieve the name of this distribution.
     */
    const string getName() { return m_name; }

    /*
     * Retrieve the application object in which this
     * distribution is contained.
     */
    Application *getApplication() { return mp_app; }

    /*
     * Deliver an event notification.
     */
    void deliverNotification(const Event e);

    /*
     * Find node the work identified by the key
     * belongs to. Body given below, because it
     * needs access to operations defined on
     * class Shard.
     */
    Node *findCoveringNode(const string &key);

    /*
     * Hash a key to a hash range value.
     */
    HashRange hashWork(const string &key);

  protected:
    /*
     * Friend declaration of Factory so that it can call the
     * protected constructor.
     */
    friend class Factory;

    /*
     * Constructor used by Factory.
     */
    DataDistribution(Application *app,
                     const string &name,
                     const string &key,
                     FactoryOps *f,
                     HashFunction *fn = NULL);

  private:
    /*
     * Make the default constructor private so it cannot be called.
     */
    DataDistribution()
        : Notifyable(NULL, "")
    {
        throw ClusterException("Someone called the DataDistribution "
                               "default constructor!");
    }

    /*
     * Unmarshall a string into this data distribution.
     */
    void unmarshall(const string &marshalledDist) throw(ClusterException);

    /*
     * Unmarshall a stringified sequence of shards.
     */
    void unmarshallShards(const string &marshalledShards, ShardList &l)
        throw(ClusterException);

    /*
     * Unmarshall a stringified sequence of manual overrides.
     */
    void unmarshallOverrides(const string &marshalledOverrides, NodeMap &m)
        throw(ClusterException);

    /*
     * Marshall a data distribution into a string.
     */
    string marshall();

    /*
     * Marshall shards into a string.
     */
    string marshallShards();

    /*
     * Marshall manual overrides into a string.
     */
    string marshallOverrides();

    /*
     * Update the distribution.
     */
    void updateDistribution() throw(ClusterException);

  private:
    /*
     * The application object for the application that contains
     * this distribution.
     */
    Application *mp_app;

    /*
     * The name of this data distribution.
     */
    const string m_name;

    /*
     * The shards in this data distribution.
     */
    ShardList m_shards;

    /*
     * The manual overrides for this data distribution.
     */
    NodeMap m_manualOverrides;

    /*
     * Which hash function to use.
     */
    unsigned int m_hashFnIndex;

    /*
     * If using a user supplied hash function, store
     * a pointer to it here. If using a built-in
     * hash function, store it here from the class-static
     * array.
     */
    HashFunction *mp_hashFnPtr;

    /*
     * Class-static variable holding the array of
     * hash functions.
     */
    static HashFunction *s_hashFunctions[];
};

/*
 * Definition of class Shard.
 */
class Shard
{
  public:
    /*
     * Get the info out of the shard.
     */
    DataDistribution *getDistribution() { return mp_dist; }
    Node *getNode() { return mp_node; }
    HashRange beginRange() { return m_beginRange; }
    HashRange endRange() { return m_endRange; }

    /*
     * Return the Node * if the work belongs to this shard,
     * NULL otherwise.
     */
    Node *contains(HashRange hash);

    /*
     * Decide whether this piece of work belongs to
     * this shard.
     */
    bool covers(const string &key);

  protected:
    /*
     * Friend declaration for Factory so that it can call the
     * protected constructor.
     */
    friend class Factory;

    /*
     * Constructor used by Factory.
     */
    Shard(DataDistribution *dist,
          Node *node,
          HashRange beginRange,
          HashRange endRange);

    /*
     * Reassign this shard to a different node.
     */
    void reassign(Node *newNode) { mp_node = newNode; }

  private:
    /*
     * Make the default constructor private so it cannot be called.
     */
    Shard()
    {
        throw ClusterException("Someone called the Shard "
                               "default constructor!");
    }

  private:
    /*
     * The data distribution this shard belongs to.
     */
    DataDistribution *mp_dist;

    /*
     * The node that this shard is assigned to.
     */
    Node *mp_node;

    /*
     * The bounds for this shard. The range is beginRange <-> endRange.
     */
    HashRange m_beginRange;
    HashRange m_endRange;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_DATADISTRIBUTION_H_ */