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

#include "clusterlibinternal.h"
#include <getopt.h>
#include "clicommand.h"
#include "cliparams.h"
#include "generalcommands.h"
#include "cliformat.h"

#if(defined(HAVE_READLINE_READLINE_H) && defined(HAVE_LIBREADLINE))
#include <readline/readline.h>
#include <readline/history.h>
#include <curses.h>
#endif

using namespace std;
using namespace clusterlib;
using namespace boost;

#if(defined(HAVE_READLINE_READLINE_H) && defined(HAVE_LIBREADLINE))
/** 
 * Used by readline.  Try to complete based on previously seen
 * Notifyable keys, children of Notifyable keys, all command names,
 * and all alias replacements.
 */
static char *
commandCompletion(const char *text, int iteration)
{
    CliParams *params = CliParams::getInstance();

    static set<string>::iterator keySetIt;
    static bool atLeastOneResult = false;

    static map<string, map<string, CliCommand *> >::iterator groupIt;
    static map<string, CliCommand *>::iterator commandIt;

    static map<string, string>::iterator aliasReplacementMapIt;

    static map<string, CliCommand::CliArg>::const_iterator argMapIt;

    static CliCommand *cliCommand = NULL;

    /* Is this a command? */
    string currentLine(rl_line_buffer);
    size_t firstSpace = currentLine.find_first_of(' ');
    bool isCommand = (firstSpace == string::npos);
    if (!isCommand) {
        cliCommand = params->getCommandByName(
            currentLine.substr(0, firstSpace));
    }
    else {
        cliCommand = NULL;
    }

    /* Initialize the static members in the first iteration */
    if (iteration == 0) {
        keySetIt = params->getKeySet()->begin();
        
        groupIt = params->getGroupCommandMap()->begin();
        commandIt = groupIt->second.begin();

        aliasReplacementMapIt = params->getAliasReplacementMap()->begin();

        if (cliCommand != NULL) {
            argMapIt = cliCommand->getArgMap().begin();
        }
    }

    /* Look for a Notifyable key if not at the beginning */
    if ((rl_point > 0) && text && (text[0] == '/')) {
        if (atLeastOneResult == false) {
            /* No results last time, let's find some */
            string chopText(text);
            shared_ptr<Root> rootSP = params->getClient()->getRoot();
            string rootKey(rootSP->getKey());
            /*
             * Algorithm: 
             * 
             * Cut off a letter each time until an
             * entry is found in the key set.  If a key is found,
             * get the children and continue.  If no notifyable
             * can be retrieved, remove that entry from the set
             * and keep going.
             */
            set<string>::iterator chopIt;
            while (chopText.size() >= rootKey.size()) {
                for (chopIt = params->getKeySet()->begin();
                     chopIt != params->getKeySet()->end();
                     ++chopIt) {
                    if (chopIt->compare(chopText) == 0) {
                        shared_ptr<Notifyable> notifyableSP = 
                            rootSP->getNotifyableFromKey(chopText);
                        if (notifyableSP == NULL) {
                            params->removeFromKeySet(chopText);
                        }
                        else {
                            NotifyableList nl = notifyableSP->getMyChildren();
                            NotifyableList::const_iterator nlIt;
                            for (nlIt = nl.begin(); 
                                 nlIt != nl.end(); 
                                 ++nlIt) {
                                params->addToKeySet((*nlIt)->getKey());
                            }
                        }
                    }
                }
                chopText.resize(chopText.size() - 1);
            }
        }
        atLeastOneResult = false;

        while (keySetIt != params->getKeySet()->end()) {
            if (keySetIt->compare(0, strlen(text), text) == 0) {
                set<string>::iterator returnIt = keySetIt;
                ++keySetIt;
                atLeastOneResult = true;
                return strdup(returnIt->c_str());
            }
            ++keySetIt;
        }
    }

    /*
     * Look for a matching command only if at the beginning of the
     * line or if it is a Help command.
     */
    if ((cliCommand == NULL) || (dynamic_cast<Help *>(cliCommand) != NULL)) {
        while (groupIt != params->getGroupCommandMap()->end()) {
            while (commandIt != groupIt->second.end()) {
                if (commandIt->first.compare(0, strlen(text), text) == 0) {
                    map<string, CliCommand *>::iterator returnIt = commandIt;
                    ++commandIt;
                    if (commandIt == groupIt->second.end()) {
                        ++groupIt;
                        commandIt = groupIt->second.begin();
                    }
                    return strdup(returnIt->first.c_str());
                }
                ++commandIt;
            }
            ++groupIt;
            commandIt = groupIt->second.begin();
        }
    }

    /* Look for a matching aliases */
    if (!isCommand) {
        while (aliasReplacementMapIt != 
               params->getAliasReplacementMap()->end()) {
            const string &res = aliasReplacementMapIt->first;
            ++aliasReplacementMapIt;            
            if (res.compare(0, strlen(text), text)== 0) {
                return strdup(res.c_str());
            }
        }
    }

    /* 
     * Look for matching arguments when there is a valid command and a
     * value isn't being worked on.
     */
    if ((cliCommand != NULL) && (*currentLine.rbegin() != '=')) {
        while (argMapIt != cliCommand->getArgMap().end()) {
            const string &res = argMapIt->first;
            ++argMapIt;
            if (res.compare(0, strlen(text), text) == 0) {
                return strdup(res.c_str());
            }
        }
    }
    
    return NULL;
}
#endif

CliParams::CliParams() 
    : m_factory(NULL),
      m_client(NULL),
      m_finished(false), 
      m_line(NULL),
      m_keySetMaxSize(1024*1024),
      m_logLevel(0),
      m_listCommands(false)
{

#if(defined(HAVE_READLINE_READLINE_H) && defined(HAVE_LIBREADLINE))
    /* 
     * Set readline to use commandCompletion instead of the
     * default. 
     */
    rl_completion_entry_function = commandCompletion;
#endif
}

void 
CliParams::printUsage(char *exec) const
{
    cout <<
"Usage: " << exec <<
" [OPTION]... [VAR=VALUE]...\n\n"
" -h  --help            Display this help and exit.\n"
" -l  --list_cmds       List all available commands.\n"
" -z  --zk_server_port  Zookeeper server port list \n"
" -a  --add_alias       Add an alias in the form 'key=value'\n"
" -d  --debug_level     Set the debug level 0-5 (default 0)\n"
" -c  --command         Run a command.  Spaces delimit arguments\n";
}

void 
CliParams::parseArgs(int argc, char **argv)
{
    static struct option longopts[] =
    {
        {"help", no_argument, NULL, 'h'},
        {"zk_server_port_list", required_argument, NULL, 'z'},
        {"list_cmds", no_argument, NULL, 'l'},
        {"add_alias", no_argument, NULL, 'a'},
        {"command", required_argument, NULL, 'c'},
        {0,0,0,0}
    };

    /* Index of current long option into opt_lng array */
    int32_t option_index = 0;
    int32_t err = -1;
    const char *optstring = ":hz:d:la:c:";

    /* Parse all standard command line arguments */
    while (1) {
        err = getopt_long(argc, argv, optstring, longopts, &option_index);
        if (err == -1) {
            break;
        }
        switch(err) {
            case 'h':
                printUsage(argv[0]);
                exit(-1);
            case 'z':
                m_zkServerPortList = optarg;
                break;
            case 'd':
                m_logLevel = atoi(optarg);
                break;
            case 'l':
                m_listCommands = true;
                break;
            case 'a':
                {
                    vector<string> keyValueVec;
                    split(keyValueVec, optarg, is_any_of("="));
                    if (keyValueVec.size() != 2) {
                        cout << "Option -a is not in the format 'key=value'"
                             << endl;
                    }
                    addAlias(keyValueVec[0], keyValueVec[1]);
                }
                break;
            case 'c':
                m_command = optarg;
                break;
            default:
                cout << "Option -" 
                     << static_cast<char>(optopt) 
                     << " is invalid" << endl;
                exit(-1);
        }
    }

    if (m_zkServerPortList.empty()) {
        cout << "Option -z needs to be set" << endl;
        printUsage(argv[0]);
        ::exit(-1);
    }
}

/**
 * Split the line into tokens and respect 'TOKEN' tokens.
 * Specifically looks for a command in the beginning and then
 * arguments in the form:
 * '<cmd> arg1=value1 arg2=value2 arg3='value3.1 value3.2' arg4. 
 * Tokens separated by ' are only allowed for 'values'.
 *
 * @param input The input string to parse.
 * @param commandName Returned command name.
 * @param argValueVec Returned argument value vector.
 */
static void parseCommandLine(const string &input, 
                             string &commandName,
                             vector<pair<string, string> > &argValueVec)
{
    ostringstream oss;
    size_t index = 0;
    size_t startTokenIndex = string::npos;

    commandName.clear();
    while (index != input.size()) {
        if (input[index] == ' ') {
            if (startTokenIndex != string::npos) {
                commandName = input.substr(startTokenIndex,
                                           index - startTokenIndex);
                break;
            }
        }
        else if (startTokenIndex == string::npos) {
            startTokenIndex = index;
        }
        ++index;
    }
    if (startTokenIndex != string::npos) {
        commandName = input.substr(startTokenIndex, index - startTokenIndex);
    }
    if (commandName.empty()) {
        oss.str("");
        oss << "parseCommandLine; Couldn't find commandName in input ("
            << input << ")";
        throw InvalidArgumentsException(oss.str());
    }

    bool quoteToken = false;
    bool argValueReady = false;
    string arg;
    string value;
    startTokenIndex = string::npos;
    argValueVec.clear();
    while (index != input.size()) {
        switch (input[index]) {
            case '\'':
                if (quoteToken == false) {
                    quoteToken = true;
                    startTokenIndex = index + 1;
                }
                else {
                    value = 
                        input.substr(startTokenIndex, index - startTokenIndex);
                    startTokenIndex = string::npos;
                    quoteToken = false;
                    argValueReady = true;
                }
                if (arg.empty()) {
                    oss.str("");
                    oss << "parseCommandLine: ' cannot be used for arg, "
                        << "index = " << index;
                    throw InvalidArgumentsException(oss.str());
                }
                break;
            case ' ':
                if (quoteToken == false) {
                    if (startTokenIndex != string::npos) {
                        if (arg.empty()) {
                            oss.str("");
                            oss << "parseCommandLine: "
                                << "Cannot have space in arg or value";
                            throw InvalidArgumentsException(oss.str());
                        }
                        value = input.substr(startTokenIndex, 
                                             index - startTokenIndex);
                        startTokenIndex = string::npos;
                        argValueReady = true;
                    }
                }
                break;
            case '=':
                if (quoteToken == false) {
                    if (startTokenIndex == string::npos) {
                        oss.str("");
                        oss << "parseCommandLine: token cannot start with =";
                        throw InvalidArgumentsException(oss.str());
                    }
                    if (!arg.empty()) {
                        oss.str("");
                        oss << "parseCommandLine: Impossible that previous "
                            << "arg " << "(" << arg << ") is still here";
                        throw InvalidArgumentsException(oss.str());
                    }
                    arg = input.substr(startTokenIndex,
                                       index - startTokenIndex);
                    startTokenIndex = index + 1;
                }
                break;
            default:
                if (startTokenIndex == string::npos) {
                    startTokenIndex = index;
                }
                break;
        }
        if (argValueReady) {
            argValueVec.push_back(make_pair<string, string>(arg, value));
            arg.clear();
            value.clear();
            argValueReady = false;
        }
        ++index;
    }
    if (quoteToken == true) {
        ostringstream oss;
        oss << "parseCommandLine: Missing ' terminator in input: " << input;
        throw InvalidArgumentsException(oss.str());
    }
    if (startTokenIndex != string::npos) {
        if (arg.empty()) {
            argValueVec.push_back(make_pair<string, string>(
                                      input.substr(
                                          startTokenIndex,
                                          index - startTokenIndex),
                                      string()));
        }
        else {
            argValueVec.push_back(make_pair<string, string>(
                                      arg, 
                                      input.substr(
                                          startTokenIndex, 
                                          index - startTokenIndex)));
        }
    }
}

void
CliParams::parseAndRunLine()
{        
    cout << endl;
    string commandName;
    vector<pair<string, string> > argValueVec;

    if (m_listCommands) {
        printCommandNamesByGroup();
        setFinished();
        return;
    }

    if (m_command.empty()) {
#if(defined(HAVE_READLINE_READLINE_H) && defined(HAVE_LIBREADLINE))
        m_line = readline(generateWelcomeMessage().c_str());
        /* If the line has text, save it to history. */
        if (m_line && *m_line) {
            add_history(m_line);
        }
        parseCommandLine(m_line, commandName, argValueVec);
        /* Clean up. */
        if (m_line) {
            free(m_line);
            m_line = NULL;
        }        
#else
        const int32_t lineStringSize = 4096;
        char lineString[lineStringSize];
        cout << generateWelcomeMessage();
        cin.getline(lineString, lineStringSize);
        parseCommandLine(lineString, commandName, argValueVec);
#endif
    }
    else {
        parseCommandLine(m_command, commandName, argValueVec);
        setFinished();
    }

    CliCommand *command = getCommandByName(commandName);
    if (command != NULL) {
        cout << endl;
        
        /* Change all tokens to aliases if not removeTokenAlias command */
        if (command->getCommandName().compare("removeTokenAlias")) {
            vector<pair<string, string> >::iterator argValueVecIt;
            map<string, string>::const_iterator aliasReplacementMapIt;
            for (argValueVecIt = argValueVec.begin();
                 argValueVecIt != argValueVec.end();
                 ++argValueVecIt) {
                aliasReplacementMapIt = m_aliasReplacementMap.find(
                    argValueVecIt->second);
                if (aliasReplacementMapIt != m_aliasReplacementMap.end()) {
                    cout << "Note: Setting value of key '"
                         << argValueVecIt->first << "' with alias '" 
                         << aliasReplacementMapIt->second << "'" << endl;
                    argValueVecIt->second = aliasReplacementMapIt->second;
                }
            }
        }
        
        try {
            command->setArg(argValueVec);
            command->checkArgs();
            command->action();
            command->resetArgs();
        }
        catch (const clusterlib::Exception &ex) {
            cout << "Command '" << command->getCommandName() << "' failed "
                 << "with error: " << ex.what() << endl;
            command->resetArgs();
        }
    }
    else {
        cout << "Command '" << commandName << "' not found" << endl;
    }
}

void
CliParams::registerCommandByGroup(CliCommand *command, const string &groupName)
{
    /*
     * Do not allow commands with the same name to be registered even
     * if they are in different groups.
     */
    map<string, map<string, CliCommand *> >::const_iterator groupCommandMapIt;
    map<string, CliCommand *>::const_iterator commandMapIt;
    for (groupCommandMapIt = m_groupCommandMap.begin();
         groupCommandMapIt != m_groupCommandMap.end();
         ++groupCommandMapIt) {
        commandMapIt = groupCommandMapIt->second.find(
            command->getCommandName());
        if (commandMapIt != groupCommandMapIt->second.end()) {
            throw clusterlib::InvalidArgumentsException(
                "registerCommand: Command " + command->getCommandName() + 
                " with group " + groupName + " already exists!");
        }
    }
    
    m_groupCommandMap[groupName][command->getCommandName()] = command;
}

void
CliParams::printCommandNamesByGroup()
{
    map<string, map<string, CliCommand *> >::const_iterator groupCommandMapIt;
    map<string, CliCommand *>::const_iterator commandMapIt;
    for (groupCommandMapIt = m_groupCommandMap.begin();
         groupCommandMapIt != m_groupCommandMap.end();
         ++groupCommandMapIt) {
        cout << groupCommandMapIt->first << ":" << endl;
        for (commandMapIt = groupCommandMapIt->second.begin();
             commandMapIt != groupCommandMapIt->second.end();
             ++commandMapIt) {
            cout << " " << commandMapIt->first << endl;
        }
        cout << endl;
    }
}

CliCommand *
CliParams::getCommandByName(const string &name)
{
    map<string, map<string, CliCommand *> >::const_iterator groupCommandMapIt;
    map<string, CliCommand *>::const_iterator commandMapIt;
    for (groupCommandMapIt = m_groupCommandMap.begin();
         groupCommandMapIt != m_groupCommandMap.end();
         ++groupCommandMapIt) {
        commandMapIt = groupCommandMapIt->second.find(name);
        if (commandMapIt != groupCommandMapIt->second.end()) {
            return commandMapIt->second;
        }
    }
    
    return NULL;
}

void
CliParams::addAlias(const string &token, const string &alias)
{
    pair<map<string, string>::iterator, bool> ret = 
        m_aliasReplacementMap.insert(make_pair<string, string>(token, alias));
    if (ret.second == false) {
        ostringstream oss;
        oss << "addAlias: Alias " << alias << " already exists";
        throw InvalidArgumentsException(oss.str());
    }
}

size_t
CliParams::removeAlias(const string &alias)
{
    return m_aliasReplacementMap.erase(alias);
}

string
CliParams::getAliasReplacement(const string &alias)
{
    map<string, string>::const_iterator aliasReplacementMapIt = 
        m_aliasReplacementMap.find(alias);
    if (aliasReplacementMapIt == m_aliasReplacementMap.end()) {
        ostringstream oss;
        oss << "getAliasReplacement: Alias " << alias << " not found";
        throw InvalidArgumentsException(oss.str());
    }
    
    return aliasReplacementMapIt->second;
}

void
CliParams::initFactoryAndClient()
{
    m_factory = new Factory(getZkServerPortList());
    m_client = m_factory->createClient();

    /* Add the root key to the key set. */
    addToKeySet(m_client->getRoot()->getKey());
}

static size_t MaxAliasSize = 15;

string
CliParams::generateWelcomeMessage()
{
    string res;
    int32_t remainingAliasSize = -1;

    if (!m_aliasReplacementMap.empty()) {
        res.append("Alias listed below:\n");
        map<string, string>::const_iterator aliasReplacementMapIt;
        for (aliasReplacementMapIt = m_aliasReplacementMap.begin();
             aliasReplacementMapIt != m_aliasReplacementMap.end();
             ++aliasReplacementMapIt) {
            res.append("'");
            res.append(aliasReplacementMapIt->first);
            res.append("' ");
            remainingAliasSize = 
                MaxAliasSize - aliasReplacementMapIt->first.size();
            if (remainingAliasSize > 0) {
                res.append(remainingAliasSize, '-');
            }
            res.append("> '");
            res.append(aliasReplacementMapIt->second);
            res.append("'");
            res.append("\n");
        }
    }
    res.append("Enter command (Use '?' if help is required):\n");

    return res;
}
