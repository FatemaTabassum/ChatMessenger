/****** CPDP Project 02 -_A Messenger Application: Server Program *****/

/************* Headers *************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unordered_map>
#include <unordered_set>

/************* Macros *************/

#define BUFFER_SIZE 4096
#define OPERATION_REGISTER "register"
#define OPERATION_LOGIN "login"
#define OPERATION_LOGOUT "logout"
#define VALUE_USERNAME "username"
#define VALUE_PASSWORD "password"
#define VALUE_FROMUSER "fromuser"
#define VALUE_TOUSER "touser"
#define OPERATION_INVITE "invite"
#define OPERATION_ACCEPT_INVITE "accept_invite"
#define OPERATION_LOCATION "location"
#define MAIN_DELIMITER  '|'
#define VAL_DELIMITER  ':'
#define OP_DELIMITER '~'
#define VALUE_PORT "port"
#define VALUE_IP "ip"
//#define USER_INFO_FILE_NAME "user_info_file"


using namespace std;

/************* All Classes *************/

class User {
public:
    vector<string> friendListVector;
    string username;
    string password;
    User() {
        username = "";
        password = "";
        friendListVector.clear();
    }
    User(string usrname, string passwrd) {
        username = usrname;
        password = passwrd;
        friendListVector.clear();
    }
    User(string userconfigstr) {
        string str;
        string friends;
        istringstream frnds(userconfigstr);
        getline(frnds, username, MAIN_DELIMITER);
        getline(frnds, password, MAIN_DELIMITER);
        getline(frnds, friends, MAIN_DELIMITER);
        istringstream friendStream(friends);
        while (getline(friendStream, str, ';')) {
            friendListVector.push_back(str);
        }
    }
};

class OnlineUser {
public:
    string ip;
    int listeningPort = -1;
    User user;
    int sockfd;
    OnlineUser(int fd, User onuser) {
        this->user = onuser;
        this->sockfd = fd;
    }
    OnlineUser(int fd, string ipAddr, int port, User onuser) {
        this->user = onuser;
        this->sockfd = fd;
        this->ip = ipAddr;
        listeningPort = port;
    }
    void setIp(string ip) {
        this->ip = ip;
    }
    void setListeningPort(int port) {
        listeningPort = port;
    }
    OnlineUser(){
    }
};

/*************  Function Prototypes  *************/

void parseUserInfoFile(char *userFileName);
int parsePortFromConfigFile(char * configFileName);
void signalHandlerSigInt(int signo);
void sendAcceptToInvitationOfUser(string messageBody);
void sendInvitationToUser(string messageBody);
void removeFromOnlineUsersUnorderedMap(int fd);
void sendLocationsToUser(int sockfd,string messageBody);
bool loginUser(string messageBody);
bool registerUser(string messageBody);
void logoutUser(string messageBody);
bool parseClientMessage(int sockfd,char *buf);
string createLoginAndRegResponseString(string opType,string statusCode);
string getPeerAddressFromSock(int sockfd);
string createLocationPayloadString(string username, string ipaddr, string listeningport);
string createMessagePayloadString(string fromuser, string message);
void saveUserInfoFile();
void resetMap();
void checkHostName(char hostname[]);
void printServerInfo(char hostname[]);

/************* Globals *************/

int sockfd, rec_sock;
fd_set all_select_set;
vector <int> all_sockets_vector;
unordered_map <string,User> allUsersUnorderedMap;
unordered_map <string,OnlineUser> onlineUsersUnorderedMap;
char user_info_file_name[150];

/************* Main *************/

int main(int argc, char * argv[]) {
    socklen_t len;
    struct sockaddr_in addr, recaddr;
    char buf[BUFFER_SIZE];
    fd_set readset;
    int maximumfd;
    int port;
    struct sigaction abc;
    abc.sa_handler = signalHandlerSigInt;
    sigemptyset(&abc.sa_mask);
    abc.sa_flags = 0;
    sigaction(SIGINT, &abc, NULL);
    resetMap();
    if (argv[1] != NULL) {
        strcpy(user_info_file_name, argv[1]);
    } else {
        cout << "User Information not found" << endl;
        exit(EXIT_FAILURE);
    }
    parseUserInfoFile(user_info_file_name);
    port = parsePortFromConfigFile(argv[2]);

    if (argc < 3) {
        printf("Please provide user information file and server configuration file\n");
        exit(0);
    }
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror(": Socket not found");
        exit(1);
    }
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((short)port);
    if (::bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { // if mac ::bind else bind check
        perror(": bind");
        exit(1);
    }
    char hostname[128];
    if(gethostname(hostname,128) == -1) {
        perror("Failed to get hostname");
        exit(EXIT_FAILURE); 
    }
    gethostbyname(hostname); // should use hostname instead
    checkHostName(hostname);
    printServerInfo(hostname);
    len = sizeof(addr);
    if (getsockname(sockfd, (struct sockaddr *)&addr, &len) < 0) { // motive
      perror(":Failed to get name");
      _exit(1);
   }
     //cout << addr.sin_port << endl;
     printf("\nPort:%d\n", htons(addr.sin_port));
    if (listen(sockfd, 5) < 0) {
        perror(": bind");
        exit(1);
    }
    // fd_set for select
    FD_ZERO(&all_select_set);
    FD_SET(sockfd, &all_select_set);
    all_sockets_vector.clear();
    maximumfd = sockfd;
    while (1) {
        readset = all_select_set;
        select(maximumfd+1, &readset, NULL, NULL, NULL);
        if (FD_ISSET(sockfd, &readset)) {
            len = sizeof(recaddr);
            if ((rec_sock = accept(sockfd, (struct sockaddr *)(&recaddr), &len)) < 0) {
                if (errno == EINTR)
                    continue;
                else {
                    perror(":accept error");
                    exit(1);
                }
            }
            if (rec_sock < 0) {
                perror(": accept");
                exit(1);
            }
           printf("Connected Remote Machine = %s | Port = %d.\n",inet_ntoa(recaddr.sin_addr), ntohs(recaddr.sin_port));
            
            all_sockets_vector.push_back(rec_sock);
            FD_SET(rec_sock, &all_select_set);
            if (rec_sock > maximumfd) maximumfd = rec_sock;
        }
        auto iter = all_sockets_vector.begin();
        while (iter != all_sockets_vector.end()) {
            int num, fd;
            fd = *iter;
            if (FD_ISSET(fd, &readset)) {
                num = read(fd, buf, BUFFER_SIZE);
                if (num == 0) { // why num == 0 deletes the user from the list
                    close(fd);
                    FD_CLR(fd, &all_select_set);
                    iter = all_sockets_vector.erase(iter);
                    removeFromOnlineUsersUnorderedMap(fd);
                    continue;
                }
                else {
                    bool islogout = parseClientMessage(fd,buf);
                    if(islogout) {
                        continue;
                    }
                }
            }
            ++iter;
        }
        maximumfd = sockfd;
        if (!all_sockets_vector.empty()) {
            maximumfd = max(maximumfd, *max_element(all_sockets_vector.begin(), all_sockets_vector.end())); // could write a method
        }
    }
}


/************* Parse Client Message *************/

bool parseClientMessage(int sockfd,char *bufferstr) {

    string strbuf(bufferstr);
    istringstream message(strbuf);
    string messageBody,response;
    cout << "Client message :" << strbuf << endl << flush;
    getline(message,messageBody,OP_DELIMITER);
    if(messageBody.compare(OPERATION_LOGIN) == 0) {
        getline(message,messageBody,OP_DELIMITER);
        //cout << " login type op" << endl;
        if(loginUser(messageBody)) {
            response = createLoginAndRegResponseString(OPERATION_LOGIN,"200");//Return Status Code 200 for correct username */
        } else {
            response = createLoginAndRegResponseString(OPERATION_LOGIN,"500");// Return Status Code 500 for error in username
        }
        write(sockfd,response.c_str(),strlen(response.c_str())+1);
    } else if(messageBody.compare(OPERATION_INVITE) == 0) {
        
        //cout << " invite type op" << endl;
        getline(message,messageBody,OP_DELIMITER);
        sendInvitationToUser(messageBody);
    } else if(messageBody.compare(OPERATION_LOCATION) == 0) {
        
        //cout << " location type op" << endl;
        getline(message,messageBody,OP_DELIMITER);
        sendLocationsToUser(sockfd,messageBody);
    } else if(messageBody.compare(OPERATION_REGISTER) == 0) {
        
        //cout << " register type op" << endl;
        getline(message,messageBody,OP_DELIMITER);
        if(registerUser(messageBody)) {
            response = createLoginAndRegResponseString(OPERATION_REGISTER,"200");//return status code "200" since the username is available (for registration)
        } else {
            response = createLoginAndRegResponseString(OPERATION_REGISTER,"500");//return status code "500" since the username is unavailable (for registration)
        }
        write(sockfd,response.c_str(),strlen(response.c_str())+1);
    } else if(messageBody.compare(OPERATION_ACCEPT_INVITE) == 0) {
        
        //cout << " ACCEPT_INVITE type op" << endl;
        getline(message,messageBody,OP_DELIMITER);
        sendAcceptToInvitationOfUser(messageBody);
    }
    else if(messageBody.compare(OPERATION_LOGOUT) == 0) {
        //cout << " logout type op" << endl;
        getline(message,messageBody,OP_DELIMITER);
        logoutUser(messageBody);
        return true;
    }
    message.clear();
    return false;
}

/*************  Update Online User List  *************/

void removeFromOnlineUsersUnorderedMap(int fd) {
    OnlineUser user;
    auto it = onlineUsersUnorderedMap.begin();
    while(it!=onlineUsersUnorderedMap.end()) {
        user = it->second;
        if(user.sockfd == fd){
            it = onlineUsersUnorderedMap.erase(it);
            cout<<"Current Online Users Count: " << onlineUsersUnorderedMap.size() << endl;
            return;
        }
        ++it;
    }
}

/******* Logout User ******/

void logoutUser(string messageBody) {
    string s,username;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_USERNAME) == 0) {
            getline(valueStream,username,VAL_DELIMITER);
        }
    }

    auto it = onlineUsersUnorderedMap.find(username);
    if(it!=onlineUsersUnorderedMap.end()) {
        OnlineUser loggedOutUser = it->second;
        onlineUsersUnorderedMap.erase(it);
        cout<<"Online users:"<<onlineUsersUnorderedMap.size()<<endl;
    }
}

/********** Sending Invitation Request **********/

void sendInvitationToUser(string messageBody) {
    string s, fromuser, touser, invitemessage, payload;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_TOUSER) == 0) {
            getline(valueStream, touser, VAL_DELIMITER);
        } else if(s.compare(VALUE_FROMUSER) == 0) {
            getline(valueStream, fromuser, VAL_DELIMITER);
        }
    }
    mesageBodyStream.clear();
    valueStream.clear();
    ostringstream bufferstr;
    bufferstr<<OPERATION_INVITE<<OP_DELIMITER<<messageBody;
    payload = bufferstr.str();
    bufferstr.clear();
    
    auto it = onlineUsersUnorderedMap.find(touser);
    if(it != onlineUsersUnorderedMap.end()) {
        OnlineUser invitedUser = it->second;
        write(invitedUser.sockfd,payload.c_str(),strlen(payload.c_str())+1);
    } else {
        auto it2 = onlineUsersUnorderedMap.find(fromuser);
        if (it2 != onlineUsersUnorderedMap.end()) {
            OnlineUser invitee = it2->second;
            write(invitee.sockfd,payload.c_str(),strlen(payload.c_str())+1);
        }
    }
}

/********** Sending Invitation Acceptance ********/

void sendAcceptToInvitationOfUser(string messageBody) {
    string s,touser,fromuser,payload;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_TOUSER) == 0) {
            getline(valueStream,touser,VAL_DELIMITER);
        } else if(s.compare(VALUE_FROMUSER) == 0) {
            getline(valueStream,fromuser,VAL_DELIMITER);
        }
    }

    mesageBodyStream.clear();
    valueStream.clear();
    ostringstream bufferstr;
    bufferstr<<OPERATION_ACCEPT_INVITE<<OP_DELIMITER<<messageBody;
    payload = bufferstr.str();
    bufferstr.clear();

    auto it = onlineUsersUnorderedMap.find(touser);
    if(it != onlineUsersUnorderedMap.end()) {
        OnlineUser inviter = it->second;
        inviter.user.friendListVector.push_back(fromuser);
        it->second = inviter;
        write(inviter.sockfd,payload.c_str(),strlen(payload.c_str())+1);
        
        auto itr = onlineUsersUnorderedMap.find(fromuser);
        OnlineUser invitee = itr->second;
        invitee.user.friendListVector.push_back(touser);
        itr->second = invitee;
        
        auto i = allUsersUnorderedMap.find(touser);
        User inviterUser = i->second;
        inviterUser.friendListVector.push_back(fromuser);
        i->second = inviterUser;

        i = allUsersUnorderedMap.find(fromuser);
        User inviteeUser = i->second;

        inviteeUser.friendListVector.push_back(touser);
        i->second = inviteeUser;

        sleep(1);
        
        payload = createLocationPayloadString(inviter.user.username,inviter.ip,to_string(inviter.listeningPort));
        write(invitee.sockfd,payload.c_str(),strlen(payload.c_str())+1);

        payload = createLocationPayloadString(invitee.user.username,invitee.ip,to_string(invitee.listeningPort));
        write(inviter.sockfd,payload.c_str(),strlen(payload.c_str())+1);
    }

}

/********** Sending User Locations ***********/

void sendLocationsToUser(int sockfd,string messageBody) {
    //cout<<"In user location send"<<endl;
    string s,username, ipaddress, listetingport,payload;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_USERNAME) == 0) {
            getline(valueStream,username,VAL_DELIMITER);
        }
        else if(s.compare(VALUE_PORT) == 0) {
            getline(valueStream,listetingport,VAL_DELIMITER);
        }
    }
    ipaddress = getPeerAddressFromSock(sockfd);
    cout<<"User: "<<username<<" addr: "<<ipaddress<<" Listeningport: "<<listetingport<<endl;
    mesageBodyStream.clear();
    valueStream.clear();
    payload = createLocationPayloadString(username,ipaddress,listetingport);
    string payloadOfrndLocation;
    auto it = allUsersUnorderedMap.find(username);
    if(it != allUsersUnorderedMap.end()) {
        User user = it->second;
        auto itr = user.friendListVector.begin();
        while (itr != user.friendListVector.end()) {
            User friendUser = *itr;

            auto frnd = onlineUsersUnorderedMap.find(friendUser.username);
            if(frnd!=onlineUsersUnorderedMap.end()) {
                OnlineUser onlinefriend = frnd->second;
                cout<<"Online friend: "<< friendUser.username << endl;
                write(onlinefriend.sockfd,payload.c_str(),strlen(payload.c_str())+1);
                payloadOfrndLocation = createLocationPayloadString(onlinefriend.user.username,onlinefriend.ip,to_string(onlinefriend.listeningPort));
                write(sockfd,payloadOfrndLocation.c_str(),strlen(payloadOfrndLocation.c_str())+1);
            }
            ++itr;
        }
        OnlineUser onlineUser(sockfd, ipaddress, stoi(listetingport), user);
        onlineUsersUnorderedMap.insert({user.username, onlineUser});
        cout<<"Online User Count: " << onlineUsersUnorderedMap.size() << endl;
    }
}

string getPeerAddressFromSock(int sockfd) {
    string peerAdr;
    struct sockaddr_in addr;
    socklen_t sock_len;
    memset(&addr, 0, sizeof(addr));
    sock_len = sizeof(addr);
    getpeername( sockfd,(struct sockaddr *) &addr,&sock_len);
    peerAdr = inet_ntoa(addr.sin_addr);
    cout << "Peer address from socket "<< peerAdr << endl;
    return peerAdr;
    
}

/********** Create Location Payload **********/

string createLocationPayloadString(string username, string ipaddr, string listeningport) {
    ostringstream bufferstr;
    string payload;
    bufferstr<<OPERATION_LOCATION<<OP_DELIMITER<<VALUE_USERNAME<<VAL_DELIMITER<<username<<MAIN_DELIMITER<<VALUE_PORT<<VAL_DELIMITER<<listeningport<<MAIN_DELIMITER<<VALUE_IP<<VAL_DELIMITER<<ipaddr;
    payload = bufferstr.str();
    bufferstr.clear();
    return payload;

}

/*********** Login User ***********/

bool loginUser(string messageBody) {
    string s,username,password;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_USERNAME) == 0) {
            getline(valueStream,username,VAL_DELIMITER);
        }
        else if(s.compare(VALUE_PASSWORD) == 0) {
            getline(valueStream,password,VAL_DELIMITER);
        }
    }

    mesageBodyStream.clear();
    valueStream.clear();
    auto it = allUsersUnorderedMap.find(username);
    if(it != allUsersUnorderedMap.end()) {
        User user = it->second;
        if(user.password.compare(password) == 0) {
            cout << "Login Successful for user: " << username << endl;
            return true;
        }
        else {
            cout<<"Login failed"<<endl;
        }
    }
    else {
        cout<<"Login failed"<<endl;
    }
    return  false;
}

/************ User Registration ************/

bool registerUser(string messageBody) {
    string s,username,password;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VAL_DELIMITER);
        if(s.compare(VALUE_USERNAME) == 0) {
            getline(valueStream,username,VAL_DELIMITER);
        }
        else if(s.compare(VALUE_PASSWORD) == 0) {
            getline(valueStream,password,VAL_DELIMITER);
        }
    }
    mesageBodyStream.clear();
    valueStream.clear();

    cout << "user: " << username << " pass: " << password << endl;
    auto it = allUsersUnorderedMap.find(username);
    if(it != allUsersUnorderedMap.end()) {
        cout << "Registration Failed" << endl;
    }
    else {
        cout << "Registration Successful" << endl;
        User user(username,password);
        allUsersUnorderedMap.insert({user.username,user});
        saveUserInfoFile();
        return true;
    }
    return  false;
}
/*********** Create Login And Response String ***********/

string createLoginAndRegResponseString(string opType,string statusCode) {
    ostringstream bufferstr;
    string payload;
    bufferstr<<opType<<OP_DELIMITER<<statusCode;
    payload = bufferstr.str();
    bufferstr.clear();
    return payload;
}

/************ Parsing User Info File ************/

void parseUserInfoFile(char *userFileName) {
    ifstream infile;
    infile.open(userFileName);
    allUsersUnorderedMap.clear();
    string s;
    while(getline(infile,s)) {
        User user(s);
        allUsersUnorderedMap.insert({user.username,user});
    }
    infile.close();
}

/************ Getting Port from Config File ************/

int parsePortFromConfigFile(char *configFileName) {
    ifstream infile;
    infile.open(configFileName);
    string str;
    getline(infile,str,VAL_DELIMITER);
    if(str.compare(VALUE_PORT) == 0) {
        getline(infile,str,VAL_DELIMITER);
    }
    else {
        cout <<"error in Configuration. Port not found" << flush;
        exit(1);
    }
    infile.close();
    return stoi(str);

}

/************ Saving User Information to File ************/


void saveUserInfoFile() {
    ofstream userInfoFile (user_info_file_name);
    User user;
    if (userInfoFile.is_open()) {
        auto iter = allUsersUnorderedMap.begin();
        while(iter != allUsersUnorderedMap.end()) {
            user = iter->second;
            userInfoFile << user.username << "|" << user.password << "|";
            auto itr = user.friendListVector.begin();
            while(itr != user.friendListVector.end()) {
                userInfoFile << *itr;
                ++itr;
                if(itr != user.friendListVector.end()) {
                    userInfoFile<<";";
                }
            }
            userInfoFile << "\n";
            ++iter;
        }
        userInfoFile.close();
    }
}

/************ Ctrl-C: SIGINT Occurred: Signal Handler ************/

void signalHandlerSigInt(int signo) {
    cout << endl << "SIGINT" << endl;
    saveUserInfoFile();
    auto itr = all_sockets_vector.begin();
    while (itr != all_sockets_vector.end()){
        int fd;
        fd = *itr;
        close(fd);
        ++itr;
    }
    close(sockfd);
    exit(0);
}


/******* Helpers functions ********/
void resetMap() {
    onlineUsersUnorderedMap.clear();
}

void checkHostName(char hostname[]) {
    int len = strlen(hostname);
    for (int i = 0; i < len; i++) {
        //cout << "hostname[" << i << "]" << " ("<< hostname[i] << ")" << endl;
        if (hostname[i] == '.') {
            hostname[i] = '\0';
            break;
        }
    }
}

void printServerInfo(char hostname[]) {
    cout << "Server Started" << endl;
    cout << "Server: " << hostname;
    return;
}

