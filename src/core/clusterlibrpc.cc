/*
 * clusterlibrpc.cc --
 *
 * Implementation of the ClusterlibRPCRequest class.
 *
 * ============================================================================
 * $Header:$
 * $Revision$
 * $Date$
 * ============================================================================
 */

#include "clusterlibinternal.h"
#include "clusterlibrpc.h"

#define LOG_LEVEL LOG_WARN
#define MODULE_NAME "ClusterLib"

using namespace std;
using namespace json;
using namespace json::rpc;

namespace clusterlib
{

ClusterlibRPCRequest::ClusterlibRPCRequest(Client *client) 
    : m_gotResponse(false)
{
    TRACE(CL_LOG, "ClusterlibRPCRequest");

    m_client = dynamic_cast<ClientImpl *>(client);
    m_root = m_client->getRoot();
}

void
ClusterlibRPCRequest::prepareRequest(const JSONValue::JSONArray &paramArr)
{
    TRACE(CL_LOG, "prepareRequest");

    if (!checkParams(paramArr)) {
        LOG_ERROR(CL_LOG, "prepareRequest: checkParams failed");
        throw JSONRPCInvocationException("prepareRequest: checkParams failed");
    }

    m_paramArr = paramArr;
}

void
ClusterlibRPCRequest::sendRequest(const void *destination)
{
    TRACE(CL_LOG, "sendRequest");

    if (destination == NULL) {
        throw InvalidArgumentsException("sendRequest: Destination is NULL");
    }
    const char *queuePtr = reinterpret_cast<const char *>(destination);
    
    Queue *queue = 
        dynamic_cast<Queue *>(m_root->getNotifyableFromKey(queuePtr));
    if (queue == NULL) {
        throw InvalidArgumentsException(
            string("sendRequest: Invalid queue at key ") + queuePtr);
    }
    else {
        /* Ready the response */
        stringstream idSs;
        idSs << ClientImpl::getHostnamePidTid() 
             << m_client->fetchAndIncrRequestCounter();
        m_id = idSs.str();
        m_client->getOps()->getResponseSignalMap()->addRefPredMutexCond(m_id);

        /* Format according the JSON-RPC 1.0 */
        JSONValue::JSONObject rpcObj;
        rpcObj["method"] = getName();
        rpcObj["params"] = m_paramArr;
        rpcObj["id"] = idSs.str();
        LOG_DEBUG(CL_LOG, 
                  "sendRequest: Putting request (%s) on queue (%s) "
                  "with id (%s)",
                  JSONCodec::encode(rpcObj).c_str(),
                  queue->getKey().c_str(),
                  m_id.c_str());
        queue->put(JSONCodec::encode(rpcObj));
    }
}

bool
ClusterlibRPCRequest::waitResponse(int64_t timeout)
{
    TRACE(CL_LOG, "waitResponse");
    
    bool finished = 
        m_client->getOps()->getResponseSignalMap()->waitPredMutexCond(
            m_id, 
            timeout);
    if (finished) {
        m_client->getOps()->getResponseSignalMap()->removeRefPredMutexCond(
            m_id);
        return true;
    }
    return false;
}

const JSONValue::JSONObject &
ClusterlibRPCRequest::getResponse()
{
    TRACE(CL_LOG, "getResponse");

    if (!m_gotResponse) {
        m_response = m_client->getOps()->getIdResponse(m_id);
        m_gotResponse = true;
    }

    return m_response;
}

bool
ClusterlibRPCRequest::isValidJSONRPCRequest(
    const JSONValue::JSONObject &rpcObj)
{
    TRACE(CL_LOG, "isValidJSONRPCRequest");

    JSONValue::JSONObject::const_iterator rpcObjIt = rpcObj.find("method");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find method");
        return false;
    }
    rpcObjIt = rpcObj.find("params");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find params");
        return false;
    }
    rpcObjIt = rpcObj.find("id");
    if (rpcObjIt == rpcObj.end()) {
        LOG_WARN(CL_LOG, "isValidJSONRPCRequest: Couldn't find id");
        return false;
    }
    if (rpcObj.size() != 3) {
        LOG_WARN(
            CL_LOG,
            "isValidJSONRPCRequest: Should be exactly 3 keys and found %d",
            rpcObj.size());
        return false;
    }

    return true;
}

}	/* End of 'namespace clusterlib' */