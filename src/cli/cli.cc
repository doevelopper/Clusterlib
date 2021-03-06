/*
 * Copyright (c) 2010 Yahoo! Inc. All rights reserved. Licensed under
 * the Apache License, Version 2.0 (the "License"); you may not use
 * this file except in compliance with the License. You may obtain a
 * copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License. See accompanying
 * LICENSE file.
 * 
 * $Id$
 */

#include <sys/utsname.h>
#include <string.h>
#include <sys/wait.h>
#include "clusterlibinternal.h"
#include "clicommand.h"
#include "cliparams.h"
#include "cliformat.h"
#include "generalcommands.h"

using namespace std;
using namespace boost;
using namespace clusterlib;
using namespace json;
using namespace json::rpc;

static string zkCmds = "Zookeeper Commands";
static string clusterlibCmds = "Clusterlib Commands";
static string cliCmds = "CLI Commands";
static string argCmds = "Arguments Commands";

/**
 * The command line interface may be used in a shell-like environment
 * or to execute a stand alone command.
 */
CliParams *CliParams::m_params = NULL;
int main(int argc, char* argv[]) 
{
    CliParams *params = CliParams::getInstance();

    /* Parse the arguments */
    params->parseArgs(argc, argv);
    
    /* 
     * Force the log level to be set to 0, special case for a command.
     */
    SetLogLevel *setLogLevelCommand = new SetLogLevel;
    stringstream logLevelSs;
    logLevelSs << params->getLogLevel();
    setLogLevelCommand->setArg(SetLogLevel::LEVEL_ARG, logLevelSs.str());
    setLogLevelCommand->action();

    /*
     * Initialize the factory
     */
    params->initFactoryAndClient();
                                  
    /*
     * Enable the JSON-RPC response handler and create the appropriate
     * response queue for this client.
     */
    shared_ptr<Root> rootSP = params->getClient()->getRoot();
    shared_ptr<Application> cliAppSP = rootSP->getApplication(
        CLString::DEFAULT_CLI_APPLICATION, CREATE_IF_NOT_FOUND);
    shared_ptr<Queue> respQueueSP = cliAppSP->getQueue(
        ProcessThreadService::getHostnamePidTid() + 
        CLString::DEFAULT_RESP_QUEUE, CREATE_IF_NOT_FOUND);
    string respQueueKey = respQueueSP->getKey();
    shared_ptr<Queue> completedQueueSP = cliAppSP->getQueue(
        ProcessThreadService::getHostnamePidTid() + 
        CLString::DEFAULT_COMPLETED_QUEUE, CREATE_IF_NOT_FOUND);
    string completedQueueKey = completedQueueSP->getKey();    
    Client *jsonRPCResponseClient = 
        params->getFactory()->createJSONRPCResponseClient(respQueueSP,
                                                          completedQueueSP);

    /* Register the commands after connecting */
    params->registerCommandByGroup(new GetZnode(params->getFactory(),
                                         params->getClient()),
                                   zkCmds);
    params->registerCommandByGroup(new GetZnodeChildren(params->getFactory(), 
                                                        params->getClient()),
                                   zkCmds);
    params->registerCommandByGroup(new AggZookeeperState(),
                                   zkCmds);

    params->registerCommandByGroup(setLogLevelCommand, clusterlibCmds);
    params->registerCommandByGroup(new RemoveNotifyable(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new GetChildren(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new GetLockBids(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new GetAttributes(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddApplication(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddGroup(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddDataDistribution(
                                       params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddNode(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddPropertyList(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new AddQueue(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new JSONRPCCommand(params->getClient(), 
                                                      respQueueSP),
                                   clusterlibCmds);
    params->registerCommandByGroup(new SetCurrentState(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new SetDesiredState(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new ManageProcessSlot(params->getClient()),
                                   clusterlibCmds);
    params->registerCommandByGroup(new ManageActiveNode(params->getClient()),
                                   clusterlibCmds);
    
    params->registerCommandByGroup(new AddAlias(params), cliCmds);
    params->registerCommandByGroup(new RemoveAlias(params), cliCmds);
    params->registerCommandByGroup(new GetAliasReplacement(params), cliCmds);
    params->registerCommandByGroup(new Help(params), cliCmds);
    params->registerCommandByGroup(new Quit(params), cliCmds);
    
    /* Register the arguments */
    params->registerCommandByGroup(new BoolArg(), argCmds);
    params->registerCommandByGroup(new IntegerArg(), argCmds);
    params->registerCommandByGroup(new StringArg(), argCmds);
    params->registerCommandByGroup(new NotifyableArg(params->getClient()),
                                   argCmds);
    params->registerCommandByGroup(new JsonArg(), argCmds);

    /* Keep getting commands until done. */
    while (!params->finished()) {
        try {
            params->parseAndRunLine();
        }
        catch (const clusterlib::Exception &e) {
            cout << e.what();
        }
    }
    
    /* Clean up */
    params->getFactory()->removeClient(jsonRPCResponseClient);
    map<string, map<string, CliCommand *> >::const_iterator groupCommandMapIt;
    map<string, CliCommand *>::const_iterator commandMapIt;
    for (groupCommandMapIt = params->getGroupCommandMap()->begin();
         groupCommandMapIt != params->getGroupCommandMap()->end();
         ++groupCommandMapIt) {
        for (commandMapIt = groupCommandMapIt->second.begin();
             commandMapIt != groupCommandMapIt->second.end();
             ++commandMapIt) {
            delete commandMapIt->second;
        }
    }

    respQueueSP->remove();
    completedQueueSP->remove();

    return 0;
}
