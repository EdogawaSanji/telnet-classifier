/*
 *  This version finished in 2019/10/29. This project can judge server which DONOT need response from client about options and suboptions
 *  it sent before into class shown in config file. And it can answer some childcommands to server according to server's response
 *  HOW TO RUN this porject:
 *  >./telnet IP port username(not necessary) password
 * 
 *  The config file is "./telnet.txt", should be in project content. The config file should be in the format shown below:
 *  version:
 *  xxxxxxx(command to show machine version)
 *  
 *  pagedown:
 *  xxxxxxx(pagedown flag)
 * 
 *  class:xxxxxxx(classname)
 *  identity:xxxx(identity rule, only support xxx&xxx&xxx, which means only all the xxx appear, we devide the machine into this class)
 *  command:(commands follow)
 *  command1
 *  command2
 *  module:xxxxxxxxx(change module command)
 *  command5
 *  command6
 *  answer:xxxxxxxxx(interactive commands)
 *  command3
 *  command4
 *  JKEND(this field SHOULDNOT miss)
 *
*/
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <iomanip>
#include <errno.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "switch.h"

#define maxBytes        4096
#define sendbyte        1
#define max_events      10
#define InitTimeout     500
#define CloseTimeout    5000

const char pgdw[2] = " ";
const char errorflag[7] = "at '^'";

extern std::list<mySwitchClass *> mySwitchClassList;
std::list<mySwitchClass *>::iterator mySwitchClassListIterator;
extern std::list<std::string> versionList;
std::list<std::string>::iterator versionListIterator;
extern std::list<std::string> pgFlagList;
std::list<std::string>::iterator pgFlagListIterator;

char iden_data[maxBytes];
int idenLoc = 0;

char recv_data[maxBytes];
mySwitchClass *exampleSwitch;

std::stack<std::string> hostCommandStack;
char *hostCommand;

enum state{
    STAT_INIT,//init state
    STAT_USERNAME,//send username
    STAT_PASSWORD,//send password
    STAT_COMMANDINIT,//get hostcommand
    STAT_COMMAND,//send command
    STAT_INCOMMAND,//wait for response from server
    STAT_ENDWAIT,//prepare to exit
    STAT_COMMANDEND//TODO: no command next but no quit or q sent
};

// send username to server
int Send_To_Server(int fd, std::string &data, int &loc, int state, int len){
    char send_buf[len];
    send_buf[0] = data[loc];
    if (send(fd, send_buf, len, 0) < 0){
        printf ("\r\nSend message error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }

    if (state == STAT_USERNAME){
        if (data[loc] == '\n'){
            loc = 0;
            return STAT_INIT;
        }
        else{
            loc ++;
            return STAT_USERNAME;
        }
    }
}


//send command to server
int Send_To_Server(int fd, int &loc, int state, int len, int &timeout){
    std::string data;
    if (versionListIterator != versionList.end()){
        data = *versionListIterator;//send identity_command
    }
    else if (exampleSwitch->commandList.size() != 0){
        data = (*exampleSwitch->commandListIterator).command;//send main_command
    }

    char send_buf[len];
    send_buf[0] = data[loc];
    if (send(fd, send_buf, len, 0) < 0){
        printf ("\r\nSend message error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }

    if (state == STAT_COMMAND){
        if (data[loc] == '\n'){//command end
            loc = 0;
            if (exampleSwitch->commandListIterator != exampleSwitch->commandList.end()){//main_command module
                if ((*exampleSwitch->commandListIterator)._command_Type == COMMAND_EX){//change module into childmodule
                    timeout = InitTimeout;
                    exampleSwitch->commandListIterator++;//prepare to sending next command
                    return STAT_COMMANDINIT;//get new hostcommand
                }
                else {
                    if ((data.find("quit\n") != std::string::npos) || (data.find("q\n") != std::string::npos && data.length() == 2) || (data.find("exit\n") != std::string::npos)){
                        hostCommandStack.pop();//skip out current module
                        if (hostCommandStack.empty()){//if current module is maincommand module, prepare to exit
                            return STAT_ENDWAIT;
                        }
                        else{
                            delete []hostCommand;
                            hostCommand = new char [hostCommandStack.top().length()];
                            for (int i = 0; i < hostCommandStack.top().length(); i++)
                                hostCommand[i] = hostCommandStack.top()[i];
                            return STAT_INCOMMAND;
                        }
                    }
                    else
                        return STAT_INCOMMAND;
                }
            }
            else{
                return STAT_INCOMMAND;
            }
        }
        else{
            loc ++;
            return STAT_COMMAND;
        }
    }
}

//send ' ' to server to page down
int PageDown(int fd){
    char send_buf[1];
    send_buf[0] = ' ';
    if (send(fd, send_buf, 1, 0) < 0){
        printf ("\r\nSend message error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }
    return STAT_INCOMMAND;
}


//match the identityRules
bool FindRule(){
    bool FOUND = false;
    while (mySwitchClassListIterator != mySwitchClassList.end()){
        exampleSwitch = *mySwitchClassListIterator;
        std::string data = exampleSwitch->identityRule;
        char temp[data.length() + 1];
        int begin = 0;
        FOUND = true;
        for (int i = 0; i <= data.length(); i++){
            if (i < data.length() && data[i] == '&'){
                temp[i - begin] = '\0';
                //printf("MATCHING: %s\n", temp);
                i++;
                begin = i + 1;
                if (strstr(iden_data, temp) != NULL){
                    memset(temp, 0, sizeof(temp));
                    continue;
                }
                else{
                    FOUND = false;
                    break;
                }
            }
            if (i == data.length()){
                temp[i - begin] = '\0';
                //printf("MATCHING: %s\n", temp);
                if (strstr(iden_data, temp) == NULL){
                    FOUND = false;
                    break;
                }
            }
            temp[i - begin] = data[i];
        }
        if (FOUND){
            versionListIterator = versionList.end();
            return FOUND;
        }
        else{
            mySwitchClassListIterator ++;
        }
    }
    return FOUND;
}


//send password to server(for no response from server)
int Pass_To_Server(int fd, const std::string &data){
    int len = data.length();
    char send_buf[sendbyte];
    for (int i = 0; i < len; i++){
        send_buf[0] = data[i];
        if (send(fd, send_buf, sendbyte, 0) < 0){
            printf ("\r\nSend message error: %s (errno: %d)!!!\n", strerror(errno), errno);
            return -1;
        }
    }
    return STAT_COMMANDINIT;
}


void StateEx(int &clientstate, int recv_num, int sock_Cfd, int &loc, int &timeout, int argc, char *argv[]){
    if ((clientstate == STAT_INIT) || (clientstate == STAT_USERNAME)){
        if (strstr(recv_data, "Username:") != NULL || strstr(recv_data, "username:") != NULL || strstr(recv_data, "login:") != NULL){
            clientstate = STAT_USERNAME;//To get username
        }
        else if (strstr(recv_data, "Password:") != NULL || strstr(recv_data, "password:") != NULL){
            clientstate = STAT_PASSWORD;//To get password
        }
    }

    if (clientstate == STAT_USERNAME){
        std::string username;
        for (int i = 0;;i++){
            if (argv[3][i] == '\0' || argv[3][i] == ' ' || argv[3][i] == '\r'){
                username += '\n';
                break;
            }
            username += argv[3][i];
        }
        clientstate = Send_To_Server(sock_Cfd, username, loc, clientstate, sendbyte);
    }
    else if (clientstate == STAT_PASSWORD){
        std::string password;
        for (int i = 0; ;i++){
            if (argv[argc - 1][i] == '\0' || argv[argc - 1][i] == '\r' || argv[argc - 1][i] == ' '){
                password += '\n';
                break;
            }
            password += argv[argc - 1][i];
        }
        clientstate = Pass_To_Server(sock_Cfd, password);
    }
    else if (clientstate == STAT_COMMANDINIT){//To collect hostCommand for later control
    }
    else if (clientstate == STAT_COMMAND){//this state we send command
        clientstate = Send_To_Server(sock_Cfd, loc, clientstate, sendbyte, timeout);
    }
    else if (clientstate == STAT_INCOMMAND){//this state we finished command send, but wait response
        if (strstr(recv_data, errorflag) != NULL){//if error occur
            if (exampleSwitch->commandListIterator == exampleSwitch->commandList.end()){
                versionListIterator++;
                idenLoc = 0;
                if (versionListIterator != versionList.end()){
                    memset(iden_data, 0, sizeof(iden_data));
                    clientstate = STAT_COMMAND;
                }
                else{
                    printf("\r\nERROR!!! No command can show the machine's version. Please modify the config file!!!\r\n");
                    clientstate = -1;
                }
                return;
            }
            else{
                printf ("\r\nERROR: input error!!! %s is Invalid input!!!\n", (*exampleSwitch->commandListIterator).command.data());
                clientstate = -1;
                return;
            }
        }
        else{
            pgFlagListIterator = pgFlagList.begin();
            bool pgBang = false;
            while (pgFlagListIterator != pgFlagList.end()){
                if (strstr(recv_data, (*pgFlagListIterator).data())){
                    pgBang = true;
                    break;
                }
                pgFlagListIterator++;
            }
            if (!pgBang){
                if (strstr(recv_data, hostCommand) != NULL){//if don't have "more" and hostCommand show, send next command
                    if (versionListIterator != versionList.end()){
                        if (FindRule()){
                            //printf("%s", iden_data);
                            exampleSwitch->commandListIterator = exampleSwitch->commandList.begin();
                        }
                        else{
                            printf("\r\nRULE ERROR!!! No rule match!\n");
                            clientstate = -1;
                            return ;
                        }
                    }
                    else{
                        exampleSwitch->commandListIterator++;
                    }
                    clientstate = STAT_COMMAND;
                    if (exampleSwitch->commandListIterator != exampleSwitch->commandList.end()){
                        clientstate = Send_To_Server(sock_Cfd, loc, clientstate, sendbyte, timeout);
                    }
                    else{//ATTENTION: command end() control, should deal with NO quit command
                        clientstate = STAT_COMMANDEND;
                        return;
                    }
                }
            }
            else{
                clientstate = PageDown(sock_Cfd);
            }
            /**
             *  if answermap function, add commandlist into maincommandList
            */
            if (exampleSwitch->commandListIterator != exampleSwitch->commandList.end() && exampleSwitch->AnswerMap.size() > 0){//if response in AnswerMap, we insert answermap value into commandlist
                exampleSwitch->AnswerMapIterator = exampleSwitch->AnswerMap.begin();
                while (exampleSwitch->AnswerMapIterator != exampleSwitch->AnswerMap.end()){
                    char *temp = new char[(*exampleSwitch->AnswerMapIterator).first.length() + 1];
                    strcpy(temp, (*exampleSwitch->AnswerMapIterator).first.c_str());
                    char *strloc = strstr(recv_data, temp);
                    if (strloc != NULL){
                        exampleSwitch->commandList.insert(exampleSwitch->commandListIterator, (*exampleSwitch->AnswerMapIterator).second.begin(), (*exampleSwitch->AnswerMapIterator).second.end());
                        break;
                    }
                    else
                        exampleSwitch->AnswerMapIterator++;
                }                           
            }
        }
    }
}

/*
 *  connect telnet IP&port
 *  return -1 if sth bad happenned
 *  return 0 if everything success 
*/
int TelConnect(int &sock_Cfd, char *IP, char *port){
    int opt = 1;
    sockaddr_in destAddr;

    if (setsockopt(sock_Cfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
        printf("\r\nSetSocket error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }

    if (setsockopt(sock_Cfd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0){
        printf("\r\nSetSocket error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }

    memset(&destAddr, 0, sizeof(destAddr));

    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(atoi((const char *)port));
    destAddr.sin_addr.s_addr = inet_addr(IP);

    if (connect(sock_Cfd, (sockaddr*) &destAddr, sizeof(destAddr)) < 0){
        printf("\r\nConnect error: %s (errno: %d)!!!\n", strerror(errno), errno);
        return -1;
    }

    printf("telnet %s\n", IP);
    return 0;
}

/*
 *  receive data from fd(IP version)
 *  this function will continue read unless EAGAIN appear, which means no data to read, u need to wait
 *  return received data length if receive successfully, or -1 if sth bad happenned
*/
int RecvData(int fd){
    int ret, recvLen = 0;
    memset(recv_data, 0, maxBytes);
    
    if (recvLen != maxBytes){
        while (true){
            ret = recv(fd, (char *)recv_data + recvLen, maxBytes, 0);
            if (ret == 0){
                return 0;
            }
            else if (ret < 0){
                if (errno == EINTR)
                    continue;
                else if (errno == EAGAIN){
                    if (versionListIterator != versionList.end()){//store the indentity information
                        for (int i = 0; i < recvLen; i++){
                            iden_data[i + idenLoc] = recv_data[i];
                        }
                        idenLoc += recvLen;
                    }
                    return recvLen;
                }
                else
                    return -1;
            }
            else{
                recvLen = recvLen + ret;
            }
        }
    }
}

/**
 *  we get the beginning of hostcommand to judge the end of serer's response on our command
 *  For there r some modules in server, we construct a stack to store the hostcommand to get
 *  the latest hostcommand
 *  to save space, we delete the hostcommand array when exchange hostcommand, and new one to
 *  store the latest.
*/
void HostGet(const int recv_num){
    int begin = recv_num - 1;
    while (true){
        if (recv_data[begin] != '\r' && recv_data[begin] != '\n' && begin >= 0)
            begin --;
        else
            break;
    }

    std::string temp;
    for (int i = begin + 1; i < recv_num; i++)
        temp += recv_data[i];
    temp += '\0';
    hostCommandStack.push(temp);

    if (hostCommand != NULL){
        delete[] hostCommand;
    }

    hostCommand = new char [hostCommandStack.top().length()];
    for (int i = 0; i < hostCommandStack.top().length(); i++)
        hostCommand[i] = hostCommandStack.top()[i];
}

int main(int argc, char* argv[]){
    int recv_num, clientstate;
    char send_data[sendbyte];
    int port;

    mySwitchClassFactory();

    int sock_Cfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int loc = 0, timeout, res;

    res = TelConnect(sock_Cfd, argv[1], argv[2]);

    if (res == -1){
        printf ("\r\nTelnet ERROR: connect error!!!\r\n");
        return 0;
    }
    else{
        clientstate = STAT_INIT;
        int flags = fcntl(sock_Cfd, F_GETFL, 0);
        fcntl(sock_Cfd, F_SETFL, flags | O_NONBLOCK);

        int epollfd = epoll_create(max_events);
        epoll_event event;
        event.events = EPOLLIN | EPOLLET;
        event.data.fd = sock_Cfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sock_Cfd, &event) < 0){
            printf("Epoll add fail: fd = %d\n", sock_Cfd);
            return 0;
        }

        epoll_event eventList[max_events];
        timeout = InitTimeout;

        if (mySwitchClassList.size() != 0){
            mySwitchClassListIterator = mySwitchClassList.begin();
        }
        exampleSwitch = *mySwitchClassListIterator;
        exampleSwitch->commandListIterator = exampleSwitch->commandList.end();
        
        versionListIterator = versionList.begin();

        while (true){
            if (clientstate == -1){
                printf("\r\nERROR!!! SEND or INPUT or RULE ERROR!!!\n\n");
                break;
            }

            res = epoll_wait(epollfd, eventList, max_events, timeout);
            if (res < 0){
                printf ("\r\nEpoll Error\n");
                break;
            }
            else if (res == 0){//if epoll_wait timeout
                    if (clientstate == STAT_COMMANDINIT){//response end, we get hostcommand
                        HostGet(recv_num);

                        timeout = CloseTimeout;

                        clientstate = STAT_COMMAND;
                        clientstate = Send_To_Server(sock_Cfd, loc, clientstate, sendbyte, timeout);
                    }
                    else{
                        if (clientstate == STAT_ENDWAIT){
                            printf("\r\nTelnet End. TEST SUCCESS!\n\n");
                            printf("\r\nThis machine is in class: %s\n", exampleSwitch->mySwitchClassName.data());
                        }
                        else
                            printf("\r\nReceive Error: SERVER FIN!!!\n\n");
                        close (sock_Cfd);
                        break;
                    }
                }
            else{
                for (int i = 0; i < res; i++){
                    if (eventList[i].events & EPOLLRDHUP){//server close socket
                        if (clientstate == STAT_ENDWAIT){
                            printf("\r\nTelnet End. TEST SUCCESS!\n\n");
                            printf("\r\nThis machine is in class: %s\n", exampleSwitch->mySwitchClassName.data());
                        }
                        else
                            printf("\r\nReceive Error: SERVER FIN!!!\n\n");
                        close (sock_Cfd);
                        break;
                    }
                    else if (eventList[i].events & EPOLLIN){//recvdata
                        recv_num = RecvData(eventList[i].data.fd);
                    }
                }
            }

            if (recv_num < 0){//timeout or other bad things
                if (errno == 104 && clientstate == STAT_ENDWAIT){
                        printf("\r\nTelnet End. TEST SUCCESS!\n\n");
                        printf("\r\nThis machine is in class: %s\n", exampleSwitch->mySwitchClassName.data());
                    }
                    else
                        printf("\r\nReceive Error: %s (errno: %d)!!!\n\n", strerror(errno), errno);
                    close (sock_Cfd);
                    break;
                }

            else if (recv_num == 0){//server close the connection
                if (clientstate == STAT_ENDWAIT){
                    printf("\r\nTelnet End. TEST SUCCESS!\n\n");
                    printf("\r\nThis machine is in class: %s\n", exampleSwitch->mySwitchClassName.data());
                }

                else
                    printf("\r\nReceive Error: SERVER FIN!!!\n\n");
                close (sock_Cfd);
                break;
            }

            else{//if recv_data
                recv_data[recv_num] = '\0';
                printf("%s", recv_data);

                StateEx(clientstate, recv_num, sock_Cfd, loc, timeout, argc, argv);
            }
        }
    }

    return 0;
}
