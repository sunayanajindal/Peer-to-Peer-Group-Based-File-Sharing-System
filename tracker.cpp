#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <map>
#include <vector>
using namespace std;

int trackerPort=8080;
map<string, string> clientLogin;             //clientid->password
map<string, string> groupOwner;              //groupid->clientid
map<string, vector<string>> groupMembers;    //groupid->members clientID
map<string, vector<string>> pendingRequests; //groupid->pending clintid
map<string, int> clientPort;                 //clientid->portnumber
map<string, string> filePaths;               //filename->path
map<string, vector<string>> upload;          //groupid->filename uploaded
map<string, map<string, string>> details;    //groupid->filename->clientid
map<string, vector<string>> downloads;       //groupid->filename downloaded

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

int startListening(int portno)
{
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");
    listen(sockfd, 5);

    return sockfd;
}

vector<string> processInput(string str)
{
    vector<string> tokens;
    unsigned int i = 0;
    tokens.clear();
    int flag = 0;
    while (i < str.length())
    {
        string sub = "";
        while (str[i] != ' ' && i < str.length())
        {
            sub += str[i];
            i++;
        }
        tokens.push_back(sub);
        i++;
    }
    return tokens;
}

void createGroup(int sockClient, vector<string> tokens, string clientID)
{
    string msg = "";
    if (groupOwner.find(tokens[1]) == groupOwner.end())
    {
        groupOwner[tokens[1]] = clientID;
        groupMembers[tokens[1]].push_back(clientID);
        msg = "Group Created Successfully";
    }
    else
    {
        msg = "Group already exsists";
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void leaveGroup(int sockClient, vector<string> tokens, string clientID)
{
    string msg = "";
    if (groupMembers.find(tokens[1]) == groupMembers.end())
    {
        msg = "Group ID does not exsist";
    }
    else
    {
        if (groupOwner[tokens[1]] == clientID)
        {
            groupMembers[tokens[1]].clear();
            groupMembers.erase(tokens[1]);
            groupOwner.erase(tokens[1]);
            pendingRequests.erase(tokens[1]);
            msg = "Group Deleted";
        }
        else
        { 
            int flag = 0;
            for (auto i = groupMembers[tokens[1]].begin(); i != groupMembers[tokens[1]].end(); i++)
            {
                if (*i == clientID)
                {
                    groupMembers[tokens[1]].erase(i);
                    flag = 1;
                    break;
                }
            }
            if (flag == 1)
            {
                msg = "Removed from the group successfully";
            }
            else
                msg = "Not a member of the group";
        }
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void joinGroup(int sockClient, vector<string> tokens, string clientID)
{
    string msg = "";
    if (groupMembers.find(tokens[1]) == groupMembers.end())
    {
        msg = "Group ID does not exsist";
    }
    else
    {
        int flag = 0;
        for (auto i = groupMembers[tokens[1]].begin(); i != groupMembers[tokens[1]].end(); i++)
        {
            if (*i == clientID)
            {
                flag = 1;
                break;
            }
        }
        if (flag == 1)
        {
            
            msg = "Already a member of the group";
        }
        else
        {
            pendingRequests[tokens[1]].push_back(clientID);
            msg = "Request Sent";
        }
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void listGroups(int sockClient)
{
    string msg = " ";
    for (auto a : groupMembers)
    {
        msg += "[ " + a.first + " :: ";
        for (auto i = a.second.begin(); i != a.second.end(); i++)
        {
            msg += *i + " ";
        }
        msg += "]  ";
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void listRequests(int sockClient, vector<string> tokens, string clientID)
{
    string msg = " ";
    if (groupMembers.find(tokens[1]) == groupMembers.end())
    {
        msg = "Group ID does not exsist";
    }
    else
    {
        if (groupOwner[tokens[1]] != clientID)
        {
            msg = "Only Group Owner can view pending Requests";
        }
        else
        {
            for (int i = 0; i < pendingRequests[tokens[1]].size(); i++)
            {
                msg += pendingRequests[tokens[1]][i] + " ";
            }
            if(pendingRequests[tokens[1]].size()==0)
            {
                msg="No Pending Requests";
            }
        }
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void acceptRequest(int sockClient, vector<string> tokens, string clientID)
{
    string msg = "";
    if (groupMembers.find(tokens[1]) == groupMembers.end() || clientLogin.find(tokens[2]) == clientLogin.end())
    {
        msg = "Invalid Group ID or client ID";
    }
    else
    {
        if (groupOwner[tokens[1]] == clientID)
        {
            int flag = 0;
            for (auto i = pendingRequests[tokens[1]].begin(); i != pendingRequests[tokens[1]].end(); i++)
            {
                if (*i == tokens[2])
                {
                    pendingRequests[tokens[1]].erase(i);
                    flag = 1;
                    break;
                }
            }
            if (flag == 1)
            {
                groupMembers[tokens[1]].push_back(tokens[2]);
                msg = "Request Accepted";
            }
            else
            {
                msg = "No Joining Request from the Client is present";
            }
        }
        else
        {
            msg = "Only Group Owner can accept the request";
        }
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void uploadFile(int sockClient, string clientID, vector<string> tokens)
{
    string groupID = tokens[2];
    string filePath = tokens[1];
    int flag = 0;
    if (groupOwner.find(groupID) != groupOwner.end())
    {
        string fileName = "";
        int j;
        for (j = filePath.length() - 1; j >= 0; j--)
        {
            if (filePath[j] != '/')
                fileName = filePath[j] + fileName;
            else
                break;
        }
        string fp = filePath.substr(0, j);

        upload[groupID].push_back(fileName);
        filePaths[fileName] = fp;
        details[groupID][fileName] = clientID;
        flag = 1;
        int temp = 1;
        write(sockClient, &temp, sizeof(temp));
    }
    else
    {
        int temp = 0;
        write(sockClient, &temp, sizeof(temp));
    }
}

void downloadFile(int sockClient, vector<string> tokens, string clientID)
{
    string groupID = tokens[1];
    string fileName = tokens[2];
    int flag = 1;
    string msg="";
    if (groupOwner.find(groupID) == groupOwner.end())
    {
        msg="Group ID does not exsist";
        flag=0;
    }
    else if(flag==1)
    {
        int tt=0;
        for (auto i = groupMembers[tokens[1]].begin(); i != groupMembers[tokens[1]].end(); i++)
        {
            if (*i == clientID)
            {
                tt = 1;
                break;
            }
        }
        if(tt==0)
        {
            msg="Not a member of the Group";
            flag=0;
        }
    }
    if (flag == 0)
    {
        int temp = 0;
        write(sockClient, &temp, sizeof(temp));
        char *temp2 = &msg[0];
        write(sockClient, temp2, strlen(temp2));
    }
    else
    {
        int temp = 1;
        write(sockClient, &temp, sizeof(temp));
        string userID = details[groupID][fileName];
        int p = clientPort[userID];
        write(sockClient, &p, sizeof(p));
        downloads[groupID].push_back(fileName);
    }
}

void listFiles(int sockClient, vector<string> tokens, string clientID)
{
    string msg = " ";
    string groupID = tokens[1];
    int flag = 0;
    for (auto i = groupMembers[groupID].begin(); i != groupMembers[groupID].end(); i++)
    {
        if (*i == clientID)
        {
            flag = 1;
            break;
        }
    }
    if (flag == 0)
    {
        msg = "Not a member of the group";
    }
    else
    {

        for (int i = 0; i < upload[groupID].size(); i++)
        {
            msg += upload[groupID][i] + "  ";
        }
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void showDownloads(int sockClient, vector<string> tokens)
{
    string msg = " ";
    for (auto a : downloads)
    {
        msg += "[ " + a.first + " :: ";
        for (auto i = a.second.begin(); i != a.second.end(); i++)
        {
            msg += *i + " ";
        }
        msg += "]  ";
    }
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

void stopShare(int sockClient, vector<string> tokens)
{
    string groupID = tokens[1];
    for (auto a = upload[groupID].begin(); a != upload[groupID].end(); a++)
    {
        if (*a == tokens[2])
        {
            upload[groupID].erase(a);
            break;
        }
    }
    string msg = "File is no more shareable";
    char *temp = &msg[0];
    write(sockClient, temp, strlen(temp));
}

// ----------------------------Thread Function---------------------------
// ----------------------------------------------------------------------
void *reader(void *param)
{
    string clientID;
    char cmd[256];
    bzero(cmd, 256);
    int sockClient = *((int *)param);
    int n = read(sockClient, cmd, 255);
    if (n < 0)
        error("ERROR reading from socket");

    vector<string> tokens = processInput(cmd);
    if (tokens[0] == "create_user")
        clientLogin[tokens[1]] = tokens[2];
    clientID = tokens[1];

    int port2;
    read(sockClient, &port2, sizeof(port2));
    //cout << clientID << "  " << port2 << endl;
    clientPort[clientID] = port2;

    // ----------------------------Execute Command---------------------------
    // ----------------------------------------------------------------------
    while (1)
    {

        bzero(cmd, 256);
        n = read(sockClient, cmd, 255);
        if (n < 0)
            error("ERROR reading from socket");
        tokens.clear();
        tokens = processInput(cmd);

        if (tokens[0] == "create_group" && tokens.size() == 2)
        {
            createGroup(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "leave_group" && tokens.size() == 2)
        {
            leaveGroup(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "join_group" && tokens.size() == 2)
        {
            joinGroup(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "list_groups" && tokens.size() == 1)
        {
            listGroups(sockClient);
        }
        else if (tokens[0] == "list_requests" && tokens.size() == 2)
        {
            listRequests(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "accept_request" && tokens.size() == 3)
        {
            acceptRequest(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "logout" && tokens.size() == 1)
        {
            break;
        }
        else if (tokens[0] == "upload_file" && tokens.size() == 3)
        {
            uploadFile(sockClient, clientID, tokens);
        }
        else if (tokens[0] == "download_file" && tokens.size() == 4)
        {
            downloadFile(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "list_files" && tokens.size() == 2)
        {
            listFiles(sockClient, tokens, clientID);
        }
        else if (tokens[0] == "show_downloads" && tokens.size() == 1)
        {
            showDownloads(sockClient, tokens);
        }
        else if (tokens[0] == "stop_share" && tokens.size() == 3)
        {
            stopShare(sockClient, tokens);
        }
        else
        {
            string msg = "Invalid Command";
            char *temp = &msg[0];
            write(sockClient, temp, strlen(temp));
        }
    }
    pthread_exit(NULL);
}

void* closeServer(void* param){
    while(true){
        string inp;
        getline(cin, inp);
        if(inp == "quit"){
            exit(0);
        }
    }
}

int main(int argc, char *argv[])
{
    int sockfd, sockClient, portno;
    struct sockaddr_storage serverStorage;

    socklen_t clilen;
    char buffer[256];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    string trackerFileName=argv[1];
    sockfd = startListening(trackerPort);

    pthread_t endThread;
    if(pthread_create(&endThread, NULL, closeServer, NULL) == -1){
        perror("pthread"); 
        exit(EXIT_FAILURE); 
    }

    while (1)
    {
        clilen = sizeof(serverStorage);
        sockClient = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        cout << "Client Connected\n";
        pthread_t tt;
        if (pthread_create(&tt, NULL, reader, &sockClient) != 0)
            cout << "Failed to create thread" << endl;

        pthread_detach(tt);
    }
    cout << "Closing connection with client\n";
    close(sockClient);
    close(sockfd);
    return 0;
}

/*
g++ tracker.cpp -o tracker -lpthread

./tracker tracker_info.txt

*/