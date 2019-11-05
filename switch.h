#ifndef SWITCH_H
#define SWITCH_H

#include <iostream>
#include <string>
#include <cstring>
#include <string.h>
#include <list>
#include <map>
#include <stack>

enum TypeOFCommand{//command type
    COMMAND_EX,
    COMMAND_MAIN
};

struct CommandType{
    std::string command;
    TypeOFCommand _command_Type;
};


struct mySwitchClass{
    std::string mySwitchClassName;
    std::string identityRule;

    std::list<CommandType> commandList;//The main command of SwitchBoard
    std::list<CommandType>::iterator commandListIterator;

    std::map<std::string, std::list<CommandType> > AnswerMap;//The answer of some response
    std::map<std::string, std::list<CommandType> >::iterator AnswerMapIterator;//The response iterator

    int size;
};

extern std::list<mySwitchClass *> mySwitchClassList;
extern std::list<mySwitchClass *>::iterator mySwitchClassListIterator;
extern std::list<std::string> versionList;
extern std::list<std::string>::iterator versionListIterator;
extern std::list<std::string> pgFlagList;
extern std::list<std::string>::iterator pgFlagListIterator;

void mySwitchClassFactory();//SwitchBoards born Factory

#endif
