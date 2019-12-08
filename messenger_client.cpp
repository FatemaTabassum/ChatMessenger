/****** CPDP Project 02 -_A Messenger Application: Client Program****/

/************* Header Files *************/
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <sstream>
#include <netinet/in.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <errno.h>
#include <unordered_map>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <pthread.h>
#include <fstream>
#include <unordered_set>
#include <arpa/inet.h>
#include <netdb.h>

/*********** Macros ***********/
#define OPERATION_REGISTER "register"
#define OPERATION_LOGIN "login"
#define OPERATION_LOGOUT "logout"
#define OPERATION_INVITE "invite"
#define OPERATION_ACCEPT_INVITE "accept_invite"
#define OPERATION_LOCATION "location"
#define OPERATION_MESSAGE "message"
#define BUFFER_SIZE 4096
#define OP_DELIMITER '~'
#define MAIN_DELIMITER '|'
#define VALUE_DELIMITER ':'
#define VALUE_PORT "port"
#define VALUE_IP "ip"
#define VALUE_USERNAME "username"
#define VALUE_PASSWORD "password"
#define VALUE_FROMUSER "fromuser"
#define VALUE_TOUSER "touser"
#define VALUE_MESSAGE "message"

using namespace std;

/************* All Classes *************/
class Friend {
public:
    int sockfd = -1;
    string ipaddress;
    int listeningPort;
    string username;
    Friend(string username, string ip, int port) {
        this->username = username;
        ipaddress = ip;
        listeningPort = port;
    }
    bool isConnected() {
        if(sockfd == -1) {
            return false;
        }
        return true;
    }
    Friend() {
    }
};

/************* Function Prototypes *************/

int connectToRemoteMachine(const char *ip, int port,bool exitIferror);
string creatLoginPayloadString(string username, string password);
string creatRegisterPayloadString(string username, string password);
string createLocationPayloadString(string username, int listeningport);
string createMessagePayloadString(string fromuser, string message);
string createInvitePayload(string targetUsername, string message);
string createAcceptInvitePayload(string inviterUsername, string message);
string createLogoutPayload();
void createPeerServerSocket(int peerServPort);
void parseConfigurationFile(char *filename, string &servhost, int &servport);
void parseServerMessage(int sockfd,char *buf);
void parseFriendLocationMessage(string messageBody);
void sendMessageToFriend(string friendUsername,string message);
void startReadThreadForSocket(int sockfd);
void removeFromOnlineFriends(int fd);
void removeFromOnlineFriends(string friendUsername);
void *acceptPeerConnection(void *arg);
void *processServerConection(void *arg);
void parseMessageFromFriend(string messageBody, int sockfd);
void parseInviteMessage(string messageBody);
void initialPromtDisplayForUser();
void userAfterLoginPromt();
void signalHandlerSigInt(int signo);
void logoutFromServer(bool showpromt);

void *process_connection(void *arg);
void resetMap();

/************* Globals *************/

int serverSockfd;
int peerServport = -1;
string username = "";
vector <int> peerSocketsVector;
unordered_map <string,Friend> onlineFriendsUnorderedMap;
unordered_set <string> inviterUsersUnorderedMap;
bool userLoggedIn = false;
const string messageSendCommand = "m ";
const string inviteAnUserCommand = "i ";
const string acceptInvitationCommand = "ia ";

/************* Main *************/

int main(int argc, char * argv[]) {

    pthread_t tid;
    int servport,*sock_ptr;
    string bufferString, password, payloadString, servhost;
    struct sigaction abc;
    abc.sa_handler = signalHandlerSigInt;
    sigemptyset(&abc.sa_mask);
    abc.sa_flags = 0;
    sigaction(SIGINT, &abc, NULL);
    resetMap();
    if (argc < 2) {
        printf("Please provide client configuration file\n");
        exit(EXIT_SUCCESS);
    }
    parseConfigurationFile(argv[1], servhost, servport);
    serverSockfd = connectToRemoteMachine(servhost.c_str(),servport,true);
    if(argc == 3) {
        peerServport = atoi(argv[2]);
    }
    sock_ptr = (int *)malloc(sizeof(int));
    *sock_ptr = serverSockfd;
    pthread_create(&tid, NULL, &processServerConection, (void*)sock_ptr);

    istringstream inputMessageStream("");
    string forUser;
    string messageString;
    initialPromtDisplayForUser();
    
/************* Command Check and Execute ****************/

    while (getline(cin, bufferString)) {
		/************* User Registration ****************/
        if(bufferString.compare("r") == 0) {
            cout << "Username:";
            cin >> username;
            cout << "Password:";
            cin >> password;
            payloadString = creatRegisterPayloadString(username,password);
            write(serverSockfd, payloadString.c_str(), strlen(payloadString.c_str())+1);
        }
        /************* User Login ****************/
        else if(bufferString.compare("l") == 0) {
            if(userLoggedIn) {
                cout << "You are already logged in" << endl;
                continue;
            }
            cout << "Username:";
            cin >> username;
            cout << "Password:";
            cin >> password;
            payloadString = creatLoginPayloadString(username,password);
            write(serverSockfd, payloadString.c_str(), strlen(payloadString.c_str())+1);
        }
        /*************Logout from the Server****************/
        else if(bufferString.compare("logout") == 0) {
            if(!userLoggedIn) {
                cout << "User is not logged in" << endl;
                continue;
            }
            logoutFromServer(true);
        }
        /*************Quit the Program****************/
        else if(bufferString.compare("exit") == 0) {
            logoutFromServer(false);
            close(serverSockfd);
            exit(0);
        }
        else {
			/*************Message Command****************/
            if(strncmp(bufferString.c_str(),messageSendCommand.c_str(),strlen(messageSendCommand.c_str())) == 0) {
                inputMessageStream.clear();
                inputMessageStream.str(bufferString);
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,messageString);
                sendMessageToFriend(forUser,messageString);
            }
            /*************Accept Invitations****************/
            else if(strncmp(bufferString.c_str(),acceptInvitationCommand.c_str(),strlen(acceptInvitationCommand.c_str())) == 0){
                inputMessageStream.clear();
                inputMessageStream.str(bufferString);
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,messageString,' ');
                auto itr = inviterUsersUnorderedMap.find(forUser);
                if(itr == inviterUsersUnorderedMap.end()) {
                    cout << "No invitation found from " << forUser << endl;
                    continue;
                }
                payloadString = createAcceptInvitePayload(forUser,messageString);
                write(serverSockfd, payloadString.c_str(), strlen(payloadString.c_str())+1);
                
            }
            /*************Invite Users****************/
            else if(strncmp(bufferString.c_str(),inviteAnUserCommand.c_str(),strlen(inviteAnUserCommand.c_str())) == 0) {
                inputMessageStream.clear();
                inputMessageStream.str(bufferString);
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,forUser,' ');
                getline(inputMessageStream,messageString);
                payloadString = createInvitePayload(forUser,messageString);
                write(serverSockfd, payloadString.c_str(), strlen(payloadString.c_str())+1);
            }
        }
    }
}

/************* Prompt/Display Messages ****************/

void initialPromtDisplayForUser() {
    cout << "1. Enter 'r' for Registration" << endl;
    cout << "2. Enter 'l' to login" << endl;
    cout << "3. Enter 'exit' to quit the program" << endl;
    
}
void userAfterLoginPromt() {
    cout << "1. Enter 'm friend_username message' to send message" << endl;
    cout << "2. Enter 'i potential_friend_username message' to invite to chat" << endl;
    cout << "3. Enter 'ia inviter_username' to accept invitation" << endl;
    cout << "4. Enter 'logout' to logout from the server" << endl;
}

/************* Sending Message to Friend ****************/

void sendMessageToFriend(string friendUsername,string message) {
    auto it = onlineFriendsUnorderedMap.find(friendUsername);
    if(it != onlineFriendsUnorderedMap.end()) {
        Friend friendUser = it->second;
        if(!friendUser.isConnected()) {
            int friendSockfd = connectToRemoteMachine(friendUser.ipaddress.c_str(),friendUser.listeningPort,false);
            if(friendSockfd!=-1) {
                friendUser.sockfd = friendSockfd;
                it->second = friendUser;
                startReadThreadForSocket(friendSockfd);
                peerSocketsVector.push_back(friendSockfd);
            }
            else {
                cout << "Could not connect to peer" << endl;
                removeFromOnlineFriends(friendUser.username);
                return;
            }
        }
        string payload = createMessagePayloadString(username,message);
        write(friendUser.sockfd,payload.c_str(),strlen(payload.c_str())+1);
    }
    else {
        cout << "Friend " << friendUsername << " is not online" << endl;
    }
}

void writeToSocket(int sockfd,string payload) {
    write(sockfd, payload.c_str(), strlen(payload.c_str())+1);
}

void createPeerServerSocket(int peerServPort) {
    int serv_sockfd;
    struct sockaddr_in serv_addr;
    pthread_t tid;
    serv_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero((void*)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(peerServPort);
    ::bind(serv_sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    listen(serv_sockfd, 5);
    int * sock_ptr;
    sock_ptr = (int *)malloc(sizeof(int));
    *sock_ptr = serv_sockfd;
    pthread_create(&tid, NULL, &acceptPeerConnection, (void*)sock_ptr);
}

/************* Parse Server Message ****************/

void parseServerMessage(int sockfd,char *buf) {
    string sbuf(buf);
    istringstream message(sbuf);
    string messageBody,response;
    getline(message,messageBody,OP_DELIMITER);

    if(messageBody.compare(OPERATION_LOGIN) == 0) {
        getline(message,messageBody,OP_DELIMITER);
        if(messageBody.compare("200") == 0) {
            userLoggedIn = true;
            cout << "Login successful" << endl;
            userAfterLoginPromt();
            //saveUserName();
            createPeerServerSocket(peerServport);
            writeToSocket(sockfd,createLocationPayloadString(username,peerServport));
        }
        else {
            cout << "Login failed. Please try again" << endl;
        }
    }
    else if(messageBody.compare(OPERATION_REGISTER) == 0) {
        getline(message,messageBody,OP_DELIMITER);
        if(messageBody.compare("200") == 0) {
            cout << "Registration successful. Please Login to continue" << endl;

        }
        else {
            cout << "Registration failed. Please try again" << endl;
        }

    }
    else if(messageBody.compare(OPERATION_LOCATION) == 0) {
        getline(message,messageBody,OP_DELIMITER);
        parseFriendLocationMessage(messageBody);
    }
    else if(messageBody.compare(OPERATION_INVITE) == 0){
        getline(message,messageBody,OP_DELIMITER);
        parseInviteMessage(messageBody);
    }
    message.clear();
}

/************* Parse Invite Message ****************/

void parseInviteMessage(string messageBody) {
    string s,fromuser, invitemessage, touser;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VALUE_DELIMITER);
        if(s.compare(VALUE_FROMUSER) == 0) {
            getline(valueStream,fromuser,VALUE_DELIMITER);
        } else if (s.compare(VALUE_TOUSER) == 0) {
            getline(valueStream,touser,VALUE_DELIMITER);
        } else if(s.compare(VALUE_MESSAGE) == 0){
            getline(valueStream,invitemessage,VALUE_DELIMITER);
        }

    }
    if (strcmp(username.c_str(), fromuser.c_str()) == 0) {
        cout << touser << " is not online " << endl;
        mesageBodyStream.clear();
        valueStream.clear();
        return;
    }
    cout << "You have received an invitation from " << fromuser << endl;
    if ( invitemessage.size() > 1 ) {
        cout << fromuser;
        cout << " >> " << invitemessage << endl;
    }
    else {
        cout << endl;
    }
    auto it = inviterUsersUnorderedMap.find(fromuser);
    if(it == inviterUsersUnorderedMap.end()) {
        inviterUsersUnorderedMap.insert(fromuser);
    }
    mesageBodyStream.clear();
    valueStream.clear();
}

/************* Connect to Remote Machine ****************/

int connectToRemoteMachine(const char *ip, int port,bool exitIferrorIsTrue) {
    int sockfd = -1;
    int rv, isConnectflag;
    struct addrinfo hints, *res, *ressave;
    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    //hints.ai_protocol = IPPROTO_TCP; // me
    if ((rv = getaddrinfo(ip, to_string(port).c_str() , &hints, &res)) != 0) {
        cout << "getaddrinfo() wrong: " << gai_strerror(rv) << endl;
        if(exitIferrorIsTrue) {
            exit(EXIT_FAILURE);
        }
        return sockfd;
    }
    ressave = res;
    //printf("ip = %s", res->ai_addr->sa_data);
    isConnectflag = 0;
    do {
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0)
            continue;
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0) {
            isConnectflag = 1;
            break;
        }
        close(sockfd);
    }
    while ((res = res->ai_next) != NULL);
    freeaddrinfo(ressave);
    if (isConnectflag == 0) {
        fprintf(stderr, "Connect Error\n");
        if(exitIferrorIsTrue) {
            exit(1);
        }
    }
    return sockfd;
}

/************* Parse Friend's Message ****************/

void parseMessageFromFriend(string messageBody, int sockfd) {
    string s,username, message;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    
    getline(mesageBodyStream,s,OP_DELIMITER);
    
    if(s.compare(OPERATION_MESSAGE) == 0) {
        while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
            valueStream.clear();
            valueStream.str(s);
            getline(valueStream,s,VALUE_DELIMITER);
            if(s.compare(VALUE_USERNAME) == 0) {
                getline(valueStream,username,VALUE_DELIMITER);
            }
            else if(s.compare(VALUE_MESSAGE) == 0) {
                getline(valueStream,message,VALUE_DELIMITER);
            }
        }
        cout << username << " >> " << message << endl;
        mesageBodyStream.clear();
        valueStream.clear();
        auto it = onlineFriendsUnorderedMap.find(username);
        if(it!=onlineFriendsUnorderedMap.end()) {
            Friend onlineFriend=it->second;
            if(onlineFriend.sockfd == -1) {
                onlineFriend.sockfd = sockfd;
            }
            it->second = onlineFriend;
        }
    }
}

/************* Parse Friend's Location Message ****************/

void parseFriendLocationMessage(string messageBody) {
    string s,username, ipaddress, listetingport;
    istringstream mesageBodyStream(messageBody);
    istringstream valueStream("");
    while(getline(mesageBodyStream,s,MAIN_DELIMITER)) {
        valueStream.clear();
        valueStream.str(s);
        getline(valueStream,s,VALUE_DELIMITER);
        if(s.compare(VALUE_USERNAME) == 0) {
            getline(valueStream,username,VALUE_DELIMITER);
        }
        else if(s.compare(VALUE_IP) == 0) {
            getline(valueStream,ipaddress,VALUE_DELIMITER);
        }
        else if(s.compare(VALUE_PORT) == 0) {
            getline(valueStream,listetingport,VALUE_DELIMITER);
        }
    }
    cout << "Friend:" << username << " is online" << endl;
    mesageBodyStream.clear();
    valueStream.clear();
    Friend onlineFriend(username,ipaddress,stoi(listetingport));
    onlineFriendsUnorderedMap.insert({username,onlineFriend});
}

/************* Parse Client Configuration File ****************/

void parseConfigurationFile(char *filename, string &servhost, int &servport) {
    ifstream infile;
    infile.open(filename);
    string str, strbuf;
    for(int i = 0; i < 2; i++) {
        getline(infile,str);
        istringstream lineStream(str);
        getline(lineStream,strbuf,':');

        if(strbuf.compare("servhost") == 0) {
            getline(lineStream,servhost,':');
        }
        else if(strbuf.compare("servport") == 0) {
            getline(lineStream,strbuf,':');
            servport = stoi(strbuf);
        }
    }
}

/***************** Create Payloads String ******************/

string createMessagePayloadString(string fromuser, string message) {
    ostringstream buffer;
    string payload;
    buffer << OPERATION_MESSAGE << OP_DELIMITER << VALUE_USERNAME << VALUE_DELIMITER << fromuser << MAIN_DELIMITER << VALUE_MESSAGE << VALUE_DELIMITER << message;
    payload = buffer.str();
    buffer.clear();
    return payload;

}

string createLocationPayloadString(string username, int listeningport) {
    ostringstream buffer;
    string payload;
    buffer << OPERATION_LOCATION << OP_DELIMITER << VALUE_USERNAME << VALUE_DELIMITER << username << MAIN_DELIMITER << VALUE_PORT << VALUE_DELIMITER << to_string(listeningport);
    payload = buffer.str();
    buffer.clear();
    return payload;

}

string creatLoginPayloadString(string username, string password) {
    ostringstream buffer;
    string payload;
    buffer << OPERATION_LOGIN << OP_DELIMITER << VALUE_USERNAME << VALUE_DELIMITER << username << MAIN_DELIMITER << VALUE_PASSWORD << VALUE_DELIMITER << password;
    payload = buffer.str();
    buffer.clear();
    return payload;
}

string creatRegisterPayloadString(string username, string password) {

    ostringstream buffer;
    string payload;
    buffer << OPERATION_REGISTER << OP_DELIMITER << VALUE_USERNAME << VALUE_DELIMITER << username << MAIN_DELIMITER << VALUE_PASSWORD << VALUE_DELIMITER << password;
    payload=buffer.str();
    buffer.clear();
    return payload;
}

string createInvitePayload(string targetUsername, string message) {

    ostringstream buffer;
    string payload;
    buffer << OPERATION_INVITE << OP_DELIMITER << VALUE_FROMUSER << VALUE_DELIMITER << username << MAIN_DELIMITER << VALUE_TOUSER << VALUE_DELIMITER << targetUsername << MAIN_DELIMITER << VALUE_MESSAGE << VALUE_DELIMITER << message;
    payload = buffer.str();
    buffer.clear();
    return payload;
}

string createAcceptInvitePayload(string inviterUsername, string message) {

    ostringstream buffer;
    string payload;
    buffer << OPERATION_ACCEPT_INVITE << OP_DELIMITER << VALUE_FROMUSER << VALUE_DELIMITER << username << MAIN_DELIMITER << VALUE_TOUSER << VALUE_DELIMITER << inviterUsername << MAIN_DELIMITER << VALUE_MESSAGE << VALUE_DELIMITER << message;
    payload = buffer.str();
    buffer.clear();
    return payload;;
}

string createLogoutPayload() {

    ostringstream buffer;
    string payload;
    buffer << OPERATION_LOGOUT << OP_DELIMITER << VALUE_USERNAME << VALUE_DELIMITER << username;
    payload = buffer.str();
    buffer.clear();
    return payload;;
}

/* Server Connection Process */

void *processServerConection(void *arg) {
    int n;
    int sockfd;
    char buf[BUFFER_SIZE];
    sockfd = *((int *)arg);
    free(arg);
    pthread_detach(pthread_self());
    while (1) {
        n = read(sockfd, buf, BUFFER_SIZE);
        if (n == 0){
            printf("Server Not Found (Crashed)!!!\n");
            close(sockfd);
            exit(0);
        }
        parseServerMessage(sockfd, buf);
    }
}


/************** Friend Process Connection *************/

void *process_connection(void *arg) {
    int sockfd;
    ssize_t n;
    char buf[BUFFER_SIZE];
    string bufferString;
    sockfd = *((int *)arg);
    free(arg);
    pthread_detach(pthread_self());
    while ((n = read(sockfd, buf, BUFFER_SIZE)) > 0) {
        buf[n] = '\0';
        bufferString = buf;
        parseMessageFromFriend(bufferString,sockfd);
    }
    if (n == 0) {
        cout << "peer closed" << endl;
    }
    else {
        cout << "something is wrong" << endl;
    }
    removeFromOnlineFriends(sockfd);
    close(sockfd);
    return(NULL);
}

/* Accept Connection from Friends */

void *acceptPeerConnection(void *arg) {
    struct sockaddr_in client_addr;
    socklen_t sock_len;
    int serv_sockfd;
    int client_sockfd;
    serv_sockfd = *((int *)arg);
    free(arg);
    while (1) {
        sock_len = sizeof(client_addr);
        client_sockfd = accept(serv_sockfd, (struct sockaddr *)&client_addr, &sock_len);
        peerSocketsVector.push_back(client_sockfd);
        startReadThreadForSocket(client_sockfd);
    }
    return(NULL);
}


/* Update Friendlist */

void removeFromOnlineFriends(string friendUsername) {
    Friend onlineFriend;
    auto it = onlineFriendsUnorderedMap.find(friendUsername);
    if(it!=onlineFriendsUnorderedMap.end()) {
        it = onlineFriendsUnorderedMap.erase(it);
        cout << "Friend " << friendUsername << " is offline" << endl;
        return;
    }
}

void removeFromOnlineFriends(int fd) {
    Friend onlineFriend;
    auto it = onlineFriendsUnorderedMap.begin();
    while(it!=onlineFriendsUnorderedMap.end()) {
        onlineFriend = it->second;
        if(onlineFriend.sockfd == fd) {
            it = onlineFriendsUnorderedMap.erase(it);
            cout << "Friend " << onlineFriend.username << " is offline" << endl;
            return;
        }
        ++it;
    }
}

/* Start Read Thread for Socket */

void startReadThreadForSocket(int sockfd) {
    int *sock_ptr;
    sock_ptr = (int *)malloc(sizeof(int));
    *sock_ptr = sockfd;
    pthread_t tid;
    pthread_create(&tid, NULL, &process_connection, (void*)sock_ptr);
}

/******* Client Logout from Server **********/

void logoutFromServer(bool flagShowpromt) {
    auto itr = peerSocketsVector.begin();
    while (itr != peerSocketsVector.end()) {
        int fd = *itr;
        close(fd);
        ++itr;
    }
    onlineFriendsUnorderedMap.clear();
    writeToSocket(serverSockfd,createLogoutPayload());
    userLoggedIn = false;
    if(flagShowpromt){
         initialPromtDisplayForUser();
    }
}
		
/********* Ctrl-C Pressed: SIGINT Occurred: Signal Handler ********/

void signalHandlerSigInt(int signo) {
    cout << endl << "SIGINT" << endl;
    logoutFromServer(false); 
    close(serverSockfd);
    exit(0);
}

/***** Helper Functions ****/
void resetMap() {
    onlineFriendsUnorderedMap.clear();
}

//void saveUserName() {
//    username =
//}

