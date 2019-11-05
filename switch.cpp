#include <iostream>
#include <fstream>
#include <assert.h>
#include <string>
#include <cstdlib>
#include "switch.h"

#define filepath    "./telnet.txt"
#define defaultPort 23

#define VersionFlag     "version:"
#define PGDNFlag        "pagedown:"
#define ClassFlag       "class:"
#define ClassFlagNum    6
#define IdentityFlag    "identity:"
#define IdentityFlagNum 9
#define CommandFlag     "command:"
#define EXFlag          "module:"
#define EXFlagNum       7
#define AnswerFlag      "answer:"
#define AnswerFlagNum   7
#define EndFlag         "JKEND"

CommandType commandConstr;

std::list<mySwitchClass *> mySwitchClassList;
std::list<std::string> versionList;
std::list<std::string> pgFlagList;

enum commandState{
    PG_FLAG,
    VER_COMMAND,
    OUT_COMMAND,
    MAIN_COMMAND,//Get main_command state
    CHILD_COMMAND,//Get answerMap
};

using namespace std;

void GetString(string &line, string substr, string &goalstr){//Substr line(ATTENTION: contain '\r')
    if (line.find(substr) != string::npos){
        goalstr = line.substr(substr.length());
        while ((goalstr[goalstr.length() - 1] == '\r') || (goalstr[goalstr.length() - 1] == '\n'))
            goalstr = goalstr.erase(goalstr.length() - 1);//erase '\r'
        goalstr += '\n';
    }
    return ;
}

void mySwitchClassFactory(){
    ifstream infile;
    infile.open(filepath);
    assert(infile.is_open());
    
    string line, childCommand;
    mySwitchClass *temp;
    commandState mycommand = OUT_COMMAND;

    while (getline(infile, line)){
        if (line[0] == '#'){
            continue;
        }
        else{
            if (line.find(PGDNFlag) != string::npos){
                mycommand = PG_FLAG;
                continue;
            }
            if (line.find(VersionFlag) != string::npos){
                mycommand = VER_COMMAND;
                continue;
            }
            else if (line.find(ClassFlag) != string::npos){//Get ClassName
                temp = new mySwitchClass;
                temp->mySwitchClassName = line.substr(ClassFlagNum);
                continue;
            }
            else if (line.find(IdentityFlag) != string::npos){//Get IdentityRule
                line = line.substr(IdentityFlagNum);
                while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                    line = line.erase(line.length() - 1);
                temp->identityRule = line;
                continue;
            }
            else if (line.find(CommandFlag) != string::npos){
                mycommand = MAIN_COMMAND;
                continue;
            }
            else if (line.find(AnswerFlag) != string::npos){//Get answermap
                mycommand = CHILD_COMMAND;
                childCommand = line.substr(AnswerFlagNum);
                while ((childCommand[childCommand.length() - 1] == '\r') || (childCommand[childCommand.length() - 1] == '\n'))
                    childCommand = childCommand.erase(childCommand.length() - 1);
                continue;
            }
            else if (line.find(EXFlag) != string::npos){
                line = line.substr(EXFlagNum);
                while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                    line = line.erase(line.length() - 1);
                commandConstr.command = line + '\n';
                commandConstr._command_Type = COMMAND_EX;
                
                if (mycommand == MAIN_COMMAND){
                    temp->commandList.push_back(commandConstr);
                }
                else if (mycommand == CHILD_COMMAND){
                    temp->AnswerMap[childCommand].push_back(commandConstr);
                }
                continue;
            }
            else if (line.find(EndFlag) != string::npos){//telnet end
                mySwitchClassList.push_back(temp);
                mycommand = OUT_COMMAND;
                continue;
            }

            if (mycommand == PG_FLAG){
                if (line.length() != 0){
                    while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                        line = line.erase(line.length() - 1);
                    if (line.length() != 0){
                        pgFlagList.push_back(line);
                    }
                }
            }
            else if (mycommand == VER_COMMAND){
                if (line.length() != 0){
                    while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                        line = line.erase(line.length() - 1);
                    if (line.length() != 0){
                        line = line + '\n';
                        versionList.push_back(line);
                    }
                }
            }
            else if (mycommand == MAIN_COMMAND){
                if (line.length() != 0){
                    while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                        line = line.erase(line.length() - 1);
                    if (line.length() != 0){
                        commandConstr.command = line + '\n';
                        commandConstr._command_Type = COMMAND_MAIN;
                        temp->commandList.push_back(commandConstr);
                    }
                }
            }
            else if (mycommand == CHILD_COMMAND){
                if (line.length() != 0){
                    while ((line[line.length() - 1] == '\r') || (line[line.length() - 1] == '\n'))
                        line = line.erase(line.length() - 1);
                    if (line.length() != 0){
                        commandConstr.command = line + '\n';
                        commandConstr._command_Type = COMMAND_MAIN;
                        temp->AnswerMap[childCommand].push_back(commandConstr);
                    }
                }
            }
        }
    }
    infile.close();
}
