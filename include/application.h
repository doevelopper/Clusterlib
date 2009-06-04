/*
 * application.h --
 *
 * Interface of class Application; it represents a set of groups of
 * nodes that together form a clusterlib application.
 *
 * $Header:$
 * $Revision$
 * $Date$
 */

#ifndef	_APPLICATION_H_
#define _APPLICATION_H_

namespace clusterlib
{

/**
 * Definition of class Application.
 */
class Application
    : public virtual Group
{
};

};	/* End of 'namespace clusterlib' */

#endif	/* !_APPLICATION_H_ */