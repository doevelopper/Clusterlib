/*
 * group.h --
 *
 * Definition of class Group; it represents a set of nodes within a specific
 * application of clusterlib.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef	_GROUP_H_
#define _GROUP_H_

namespace clusterlib
{

/*
 * Definition of class Group.
 */
class Group
    : public virtual NotificationTarget
{
  public:
    /*
     * Retrieve the name of this group.
     */
    const string getName() { return m_name; }

    /*
     * Retrieve the application object for the application
     * that this group is part of.
     */
    const Application *getApplication() { return mp_app; }

    /*
     * Retrieve a node with a given name in this group.
     */
    const Node *getNode(const string &nodeName) throw(ClusterException);

    /*
     * Retrieve the map of nodes in this group.
     */
    NodeMap *getNodes() { return &m_nodes; }

    /*
     * Retrieve a named data distribution that covers (some or all
     * of) its shards with nodes from this group.
     */
    const DataDistribution *getDistribution(const string &distName)
        throw(ClusterException);

    /*
     * Retrieve the map of data distributions that cover
     * (some or all of) their shards with nodes from this group.
     */
    DataDistributionMap *getDistributions() { return &m_distributions; }

  protected:
    /*
     * Friend declaration so that Factory can call the constructor.
     */
    friend class Factory;

    /*
     * Constructor used by the factory.
     */
    Group(const Application *app,
          const string &name,
          const string &key,
          Factory *f,
          Notifyable *nrp)
        : NotificationTarget(nrp),
          mp_f(f),
          mp_app(app),
          m_name(name),
          m_key(key)
    {
        m_nodes.clear();
        m_distributions.clear();
    }

    /*
     * Allow the factory access to my key.
     */
    const string getKey() { return m_key; }

  private:
    /*
     * Make the default constructor private so it cannot be called.
     */
    Group()
        : NotificationTarget(NULL)
    {
        throw ClusterException("Someone called the Group default "
                               "constructor!");
    }

  private:
    /*
     * The factory instance we're using.
     */
    Factory *mp_f;

    /*
     * The application object that contains this group.
     */
    const Application *mp_app;

    /*
     * The name of this group.
     */
    const string m_name;

    /*
     * The key associated with this group.
     */
    const string m_key;

    /*
     * Map of all nodes within this group.
     */
    NodeMap m_nodes;

    /*
     * Map of all data distributions that can use nodes within
     * this group.
     */
    DataDistributionMap m_distributions;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_GROUP_H_ */
