/*
 * applicationmpl.cc --
 *
 * Implementation of the Application class; it represents a set of groups
 * of nodes that together form a clusterlib application.
 *
 * ============================================================================
 * $Header:$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"

#define LOG_LEVEL LOG_WARN
#define MODULE_NAME "ClusterLib"

using namespace std;

namespace clusterlib
{

Group *
ApplicationImpl::getMyGroup()
{
    throw InvalidMethodException("Application cannot be a part of a group!");
}

void
ApplicationImpl::initializeCachedRepresentation()
{
    GroupImpl::initializeCachedRepresentation();
}

};	/* End of 'namespace clusterlib' */
