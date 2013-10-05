
#include <iostream>
#include <cstring>      // Needed for memset
#include <sys/socket.h> // Needed for the socket functions
#include <netdb.h>      // Needed for the socket functions
#include <unistd.h>     // Needed for close()ing returned file descriptor from socket()
#include <ctype.h>      // Needed for toupper()
#include <pthread.h>
#include <string>

#include <list>         // list class-template definition
#include <map>         // map class-template definition
#include <deque>         // queue class-template definition
#include <queue>
#include <algorithm>

//#include <locale>

#define BUFFER_SIZE 100
enum commands {BROAD, MENU, USERS, WISI, EXIT, UNKNOWN};
struct PrivateMsg{
    std::string message;
    std::string writer;
};

struct user{
    std::string nickname;
    int socketFD;
    pthread_t thread;
    std::queue<PrivateMsg> inBuffer;
    pthread_mutex_t mutex_inBuffer_write = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t  cond_empty_inBuffer = PTHREAD_COND_INITIALIZER;
    pthread_mutex_t pthread_mutex_shutingdown = PTHREAD_MUTEX_INITIALIZER;

};
struct BroadMsg{
    std::string message;
    std::string writer;
    int nReaders;
};

std::deque<BroadMsg> board;
std::map<std::string, user> users;

pthread_mutex_t mutex_users_insert = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_board_write = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_board_read = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t  cond_empty_board = PTHREAD_COND_INITIALIZER;


//std::locale loc;
bool notalnum(char c) { return !std::isalnum(c); }
inline void toupcase(char* s) {
    int sl = strlen(s);
    for (int i = 0; i < sl; i++)
        s[i]=toupper(s[i]);
}
/*inline bool isValidName( std::string name )
{
   return ( std::all_of( name.begin(), name_end(),
                                [] ( char c ) { return ( std::isalpha( c ) ); } ) );
}
inline bool isValidName( char *name )
{
    int c=0;
    while(name+c){
        if(!std::isalpha(name+c)) return false;
    }
    return true;
}
*/

inline int preparing_socket();
void * accept_connections(void *);
void * serve_client(void *);
void * global_msg_to_client(void *);
void * private_msg_to_client(void *);
bool isCommand(const char *);
commands parsCommand(char *, void **&);
std::string listUsers();
int main()
{

    int socketfd = preparing_socket();


    //  ************************* pthread  ************************

    pthread_t tsrv; // declare threads.
    pthread_create( &tsrv, NULL, accept_connections, (void*)&socketfd); // create a thread running listening()

    //pthread_exit(NULL);   //  exit main but not to terminate program until threads complete (below code wont run)
    pthread_join(tsrv, NULL); //  wait untile thread tsrv completes


    //  ***********************************************************


    //freeaddrinfo(host_info_list);
    close(socketfd);

return 0 ;
}
inline int preparing_socket (){
    int status;
    struct addrinfo host_info;       // The struct that getaddrinfo() fills up with data.
    struct addrinfo *host_info_list; // Pointer to the to the linked list of host_info's.

    // The MAN page of getaddrinfo() states "All  the other fields in the structure pointed
    // to by hints must contain either 0 or a null pointer, as appropriate." When a struct
    // is created in c++, it will be given a block of memory. This memory is not nessesary
    // empty. Therefor we use the memset function to make sure all fields are NULL.
    memset(&host_info, 0, sizeof host_info);

    std::cout << "Setting up the structs..."  << std::endl;

    host_info.ai_family = AF_UNSPEC;     // IP version not specified. Can be both.
    host_info.ai_socktype = SOCK_STREAM; // Use SOCK_STREAM for TCP or SOCK_DGRAM for UDP.
    host_info.ai_flags = AI_PASSIVE;     // IP Wildcard

    // Now fill up the linked list of host_info structs with google's address information.
    status = getaddrinfo(NULL, "5556", &host_info, &host_info_list);
    // getaddrinfo returns 0 on succes, or some other value when an error occured.
    // (translated into human readable text by the gai_gai_strerror function).
    if (status != 0)  std::cout << "getaddrinfo error" << gai_strerror(status) ;


    std::cout << "Creating a socket..."  << std::endl;
    int socketfd ; // The socket descripter
    socketfd = socket(host_info_list->ai_family, host_info_list->ai_socktype,
                      host_info_list->ai_protocol);
    if (socketfd == -1)  std::cout << "socket error " ;

    std::cout << "Binding socket..."  << std::endl;
    // we use to make the setsockopt() function to make sure the port is not in use
    // by a previous execution of our code. (see man page for more information)
    int yes = 1;
    status = setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(socketfd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1)  std::cout << "bind error" << std::endl ;

    //std::cout << "Listen()ing for connections..."  << std::endl;
    status =  listen(socketfd, 5);
    if (status == -1)  std::cout << "listen error" << std::endl ;

    return socketfd;

}

void* accept_connections(void *arg){

    int socketfd = *(static_cast<int*>(arg));

    int new_sd, *nsd;
    pthread_t thread;


    struct sockaddr_storage their_addr;
    socklen_t addr_size = sizeof(their_addr);

    do{
         new_sd= accept(socketfd, (struct sockaddr *)&their_addr, &addr_size);
        if (new_sd == -1){
            std::cout << "listen error" << std::endl ;
        }else{
            nsd = new int(new_sd);
            pthread_create( &thread, NULL, serve_client, (void*)nsd );
            pthread_detach(thread);
        }

    }while(1);

}

void* serve_client(void* arg){


    user tempUser;
    tempUser.socketFD = *(static_cast<int *>(arg));
    delete((int *)arg);

    ssize_t bytes_recieved, bytes_sent;
    char inBuffer[BUFFER_SIZE];
    std::string msg;
    std::string tempString;

    const char *const menu = "\n********** ISON Telnet Chat **********\n Just write something and anyone can see it or you can run flowing commands just write them in a single line.\n\t ::menu (show menu)\n\t ::users (display online users)\n\t ::wisi {nickname} {message} (send message to a specific user)\n\t ::exit (terminate yourself in server)\n**************************************\n";
    msg="\n>>>  Welcome to ISON Telnet Chat ...\n At first you need to have a nickname.\n What is your nickname (only use alphabet and numeric charcters)?\n";
    do{
        bytes_sent = send(tempUser.socketFD, msg.c_str(), msg.size(), 0);
        tempString.clear();
        bytes_recieved = recv(tempUser.socketFD, inBuffer, BUFFER_SIZE, 0);
        // If no data arrives, the program will just wait here until some data arrives.
        if (bytes_recieved == 0) {std::cout << "host shut down." << std::endl; msg="have you shuted down?";}
        if (bytes_recieved == -1){std::cout << "recieve error!" << std::endl; msg="have you some problem?";}
        if (bytes_recieved > 0){
            tempString.assign(inBuffer, inBuffer+bytes_recieved);
            // Remove characters from tempString except numerics and alphabets
            tempString.assign(tempString.begin(),std::remove_if(tempString.begin(), tempString.end(), notalnum));
            if (!tempString.empty()){
                tempUser.nickname = tempString;
            }else{ msg = "\nIt\'s invalid name, try again!\n What is your nickname (only use alphabet and numeric charcters)?\n"; }

            }
    }while(tempString.empty());

    pthread_mutex_lock(&mutex_users_insert);
    // ** Note: How to insert a single element to std::map!
    users.insert( std::pair<std::string, user>(tempUser.nickname, tempUser) );
    pthread_mutex_unlock(&mutex_users_insert);

    user *userPtr = &users[tempUser.nickname];

    msg = ">>> Your nickname is \""+userPtr->nickname+'"';
    msg += menu;
    bytes_sent = send(userPtr->socketFD, msg.c_str(), msg.size(), 0);

    pthread_t thread_global_msg, thread_private_msg;
    pthread_create( &thread_global_msg, NULL, global_msg_to_client, (void*)userPtr );
    pthread_create( &thread_private_msg, NULL, private_msg_to_client, (void*)userPtr );

    commands cmd;
    void **operands;

    BroadMsg bmsg ;
    std::map<std::string, user>::iterator tempUsersItr;
    do{
        //delete(inBuffer);

        bytes_recieved = recv(userPtr->socketFD, inBuffer, BUFFER_SIZE, 0);
        if (bytes_recieved > 0){
            inBuffer[bytes_recieved] = '\0';
            cmd = parsCommand(inBuffer, operands);
            switch(cmd){
            case BROAD:
                bmsg = {*((std::string*)operands[0]), userPtr->nickname, 0};

                pthread_mutex_lock(&mutex_board_write);
                board.push_back(bmsg);
                pthread_cond_broadcast(&cond_empty_board);
                pthread_mutex_unlock(&mutex_board_write);

                delete(*operands);
                delete(operands);
                break;
            case MENU:
                msg = menu;
                bytes_sent = send(userPtr->socketFD, msg.c_str(), msg.size(), 0);
                break;
            case USERS:
                msg = listUsers();
                bytes_sent = send(userPtr->socketFD, msg.c_str(), msg.size(), 0);
                break;
            case WISI:
                tempUsersItr = *(static_cast<std::map<std::string, user>::iterator *>(operands[0]));
                tempString = *(static_cast<std::string *>(operands[1]));

                pthread_mutex_lock(&tempUsersItr->second.mutex_inBuffer_write);
                tempUsersItr->second.inBuffer.push(PrivateMsg{tempString, userPtr->nickname});
                pthread_cond_signal(&tempUsersItr->second.cond_empty_inBuffer);
                pthread_mutex_unlock(&tempUsersItr->second.mutex_inBuffer_write);

               // delete(operands[0]);
                //delete(operands[1]);
                delete(operands);
                break;
            case EXIT:
                bmsg = {" *>>>  Has left the room!  <<<*\n", userPtr->nickname, 0};

                pthread_mutex_lock(&mutex_board_write);
                board.push_back(bmsg);
                pthread_cond_broadcast(&cond_empty_board);
                pthread_mutex_unlock(&mutex_board_write);

                msg = "\n***  Khosh Galdoon  ***\n";
                bytes_sent = send(userPtr->socketFD, msg.c_str(), msg.size(), 0);
                bytes_recieved = -2;
            }

        }

    }while(bytes_recieved > 0);

    pthread_cancel(thread_global_msg);
    pthread_cancel(thread_private_msg);

    pthread_join(thread_global_msg, NULL);
    pthread_join(thread_private_msg, NULL);

    // You must choice another way to ereas user.
    // Maybe some body wants to send a private message to the user during deleting it.
    //pthread_mutex_lock(&userPtr->mutex_inBuffer_write);
    users.erase(users.find(userPtr->nickname));

    close(userPtr->socketFD);

    pthread_exit(NULL);
}
void unlock_mutex(void *arg){
    pthread_mutex_t *mu;
    mu = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mu);
}


void * global_msg_to_client(void *arg){

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);//cancel thread only in cancelation point

    user *userPtr = (user *)arg;
    std::deque<BroadMsg>::iterator front = board.begin();
    std::string msg;

    while(true){

        pthread_mutex_lock(&mutex_board_write);
        pthread_cleanup_push(unlock_mutex, (void*)&mutex_board_write);
        if (front == board.end())
            pthread_cond_wait(&cond_empty_board, &mutex_board_write);// Thread shall cancel in this point (because this function is a cancelation point)
        pthread_mutex_unlock(&mutex_board_write);
        pthread_cleanup_pop(NULL);

        if(front->writer != userPtr->nickname){
            msg = front->writer + " says: ";
            msg += front->message;
            send(userPtr->socketFD, msg.c_str(), msg.size(), 0);
        }
        ++front;

    }

}

void * private_msg_to_client(void *arg){

    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);//cancel thread only in cancelation point

    user *userPtr = (user *)arg;
    std::string msg;

    do{
        pthread_mutex_lock(&userPtr->mutex_inBuffer_write);
        pthread_cleanup_push(unlock_mutex, (void*)&userPtr->mutex_inBuffer_write);
        if(userPtr->inBuffer.empty())
            pthread_cond_wait(&userPtr->cond_empty_inBuffer, &userPtr->mutex_inBuffer_write);
        pthread_mutex_unlock(&userPtr->mutex_inBuffer_write);
        pthread_cleanup_pop(NULL);

        msg = userPtr->inBuffer.front().writer + " says(wisi): ";
        msg += userPtr->inBuffer.front().message;

        send(userPtr->socketFD, msg.c_str(), msg.size(), 0);
        userPtr->inBuffer.pop();

    }while(true);
}


commands parsCommand(char *strBuff, void **&operands){

    char *endTokenPtr=NULL;
    char *endStringPtr=NULL;
    std::string tempString;
    commands cmd;
    char CharPtr_wisi[] = "::wisi";
    if(strBuff[0] == ':' && strBuff[1] == ':'){
        if (strcmp(strBuff,"::menu\r\n")==0)    cmd = MENU;
        else
            if (strcmp(strBuff,"::users\r\n")==0) cmd = USERS;
            else{

                if (std::search(strBuff, strBuff+6, CharPtr_wisi, CharPtr_wisi+6) != strBuff+6) {

                    endStringPtr = strBuff+strlen(strBuff);
                    endTokenPtr = std::find<char*>(strBuff+7, endStringPtr, ' ');
                    tempString.assign(strBuff+7, endTokenPtr);

                    std::map<std::string, user>::iterator usersItr;
                    usersItr = users.find(tempString);

                    operands = new void*[2];
                    operands[0] = &usersItr;
                    operands[1] = new std::string(endTokenPtr);

                    cmd = WISI;
                }
                else
                    if (strcmp(strBuff,"::exit\r\n")==0) cmd = EXIT; else cmd = UNKNOWN;

            }
    }else{
        operands = new void*[1];
        operands[0] = new std::string(strBuff);

        cmd = BROAD;
    }

    return cmd;
}
std::string listUsers(){
    std::map<std::string, user>::const_iterator first = users.begin();
    std::map<std::string, user>::const_iterator last = users.end();
    std::string tempString("\n*** List Of Online Users ***\n");
    int i=0;
    while(first != last) {
        tempString += ++i;
        tempString += ") "+first->second.nickname+"\n";
        ++first;
    }
    return tempString += "******* End Of List *******\n";
}
