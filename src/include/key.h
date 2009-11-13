/*
 * key.h --
 *
 * Interface of class Key; it represents an input to a data
 * distribution in clusterlib.  This interface is meant to be
 * subclassed by users.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef _KEY_H_
#define _KEY_H_

namespace clusterlib
{

/**
 * Definition of class Key
 */
class Key
{
  public:
    /**
     * Hash this key and produce a HashRange that can be used with a
     * DataDistribution.
     *
     * @return the hashed key
     */
    virtual HashRange hashKey() const = 0;
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_KEY_H_ */
