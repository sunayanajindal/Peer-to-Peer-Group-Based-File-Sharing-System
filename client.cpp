#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>
#include <map>
using namespace std;

int trackerPort=8080;
int clientPort;
int listenFlag = 0;
string userID, password;
map<string, string> fileSrcPath; //filename->filepath
string logFile;

void error(const char *msg)
{
    perror(msg);
    exit(0);
}

void writeLog(const string &msg)
{
    ofstream log_file(logFile, ios_base::out | ios_base::app);
    log_file << msg << endl;
}

int connectToTracker(int portno)
{
    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");
    server = gethostbyname("127.0.0.1");
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }
    bzero((char *)&serv_addr, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr, server->h_length);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR connecting");
    return sockfd;
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

void writeFile(int sockPeer, string destPath)
{
    int block_size = 256;
    fstream outFile;
    outFile.open(destPath, ios::in | ios::out | ios::trunc | ios::binary);
    int blocksCount;
    read(sockPeer, &blocksCount, sizeof(blocksCount));

    for (int i = 0; i <= blocksCount; i++)
    {
        char *buffer = new char[block_size];
        bzero(buffer, block_size);
        read(sockPeer, buffer, block_size);
        int y;
        read(sockPeer, &y, sizeof(y));

        outFile.write(buffer, y);
    }
    int lastBlock;
    read(sockPeer, &lastBlock, sizeof(lastBlock));
    char *buffer = new char[lastBlock];
    bzero(buffer, lastBlock);
    read(sockPeer, buffer, lastBlock);
    int y;
    read(sockPeer, &y, sizeof(y));
    outFile.write(buffer, y);
    outFile.close();
}

void readFile(int sockPeer, string srcPath)
{
    int block_size = 256;
    fstream inFile;
    inFile.open(srcPath, ios::in | ios::binary);

    inFile.seekg(0, ios::end);
    int pos = inFile.tellg();
    inFile.seekg(0, ios::beg);
    int size = pos;
    int blocksCount = size / block_size;
    int lastBlock = size % block_size;
    write(sockPeer, &blocksCount, sizeof(blocksCount));

    for (int i = 0; i <= blocksCount; i++)
    {
        char *buffer = new char[block_size];
        inFile.read(buffer, block_size);
        int y = inFile.gcount();
        write(sockPeer, buffer, block_size);
        write(sockPeer, &y, sizeof(y));
    }
    write(sockPeer, &lastBlock, sizeof(lastBlock));
    char *buffer = new char[lastBlock];
    inFile.read(buffer, lastBlock);
    int y = inFile.gcount();
    write(sockPeer, buffer, lastBlock);
    write(sockPeer, &y, sizeof(y));
    pos = inFile.tellg();
    inFile.close();
}

void *uploadFunc(void *param)
{
    int sockListen = startListening(clientPort);
    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int sockPeer = accept(sockListen, (struct sockaddr *)&cli_addr, &clilen);
        writeLog("Connection Established with peer at port " + to_string(clientPort));

        char fileName[256];
        bzero(fileName, 256);
        read(sockPeer, fileName, 255);
        writeLog("Receiving File Details for download : " + string(fileName));
        string srcPath = fileSrcPath[fileName];
        writeLog("Source Path of file : " + srcPath);
        readFile(sockPeer, srcPath);
    }
    close(sockListen);
}

void uploadFile(vector<string> tokens, int sockTracker)
{
    string filePath = tokens[1];
    string groupID = tokens[2];
    string fileName = "";
    int j;
    for (j = filePath.length() - 1; j >= 0; j--)
    {
        if (filePath[j] != '/')
            fileName = filePath[j] + fileName;
        else
            break;
    }
    writeLog("Uploading file to the server: " + fileName);
    fileSrcPath[fileName] = filePath;
}

void downloadFunc(int sockTracker, vector<string> tokens)
{
    string groupID = tokens[1];
    string fileName = tokens[2];
    string destFilePath = tokens[3];

    // -------------------Receive Port number--------------------------------
    int peerPort;
    read(sockTracker, &peerPort, sizeof(peerPort));

    // -----------------------Connect to Peer -------------------------------
    int sockPeer = connectToTracker(peerPort);
    //cout << "Connection Established with peer\n";
    writeLog("Connection Established with peer at port " + to_string(peerPort));
    writeLog("Sending File Details for download : " + fileName);
    writeLog("Destination Path : " + destFilePath);

    char temp[256];
    bzero(temp, 256);
    for (int i = 0; i < fileName.length(); i++)
        temp[i] = fileName[i];
    write(sockPeer, temp, 255);

    //cout << "Dest Path:: " << destFilePath << endl;

    writeFile(sockPeer, destFilePath);
    writeLog("File Downloaded : " + fileName);
}


// ---------------------Main Function------------------------------------
// ----------------------------------------------------------------------
int main(int argc, char *argv[])
{
    int sockfd;
    char buffer[256];

    string s = string(argv[1]);
    int k;
    for (k = 0; k < s.length() && s[k] != ':'; k++)
        ;
    string portt=s.substr(k + 1, s.length());
    char pp[256]={0};

    for(int i=0;i<portt.length();i++)
        pp[i]=portt[i];
    clientPort = atoi(pp);
    string p = argv[1];
    logFile = p + "_log.txt";
    writeLog("Tracker Port: 8080");
    writeLog("Client Details: " + string(argv[2]));
    writeLog("Log file name:" + logFile);

    string trackerInfoFile=argv[2];

    // ------------------Set UserID and Passwoord----------------------------
    int flag = 0;
    vector<string> tokens;
    string input;
    while (flag == 0)
    {
        //cout << "\nCreate UserID and Password\n";
        getline(cin >> ws, input);
        tokens.clear();
        tokens = processInput(input);
        if (tokens[0] == "create_user" && tokens.size() == 3)
        {
            userID = tokens[1];
            password = tokens[2];
            flag = 1;
            writeLog("UserID : " + userID);
            writeLog("Password : " + password);
        }
        else
            cout << "Invalid! Please create an account" << endl;
    }
    cout << "User Created successfully" << endl;

    // ---------------------------------Login--------------------------------
    flag = 1;
    while (flag == 1)
    {
        //cout << "login: " << endl;
        string login;
        flag = 0;
        getline(cin >> ws, login);
        tokens.clear();
        tokens = processInput(login);
        if (tokens[0] != "login" || tokens[1] != userID || tokens[2] != password)
        {
            cout << "Invalid Login" << endl;
            flag = 1;
        }
    }
    cout << "Logged In" << endl;
    writeLog("Client Logged In");

    // ------------------------Connect to Tracker----------------------------
    int sockTracker = connectToTracker(trackerPort);
    cout << "\nConnection Established with Tracker\n";
    writeLog("connection established with Tracker : 8080");

    // ---------------Send UserID and Password to Tracker--------------------
    char inp[256];
    bzero(inp, 256);
    for (int i = 0; i < input.length(); i++)
        inp[i] = input[i];
    int n = write(sockTracker, inp, 255);
    if (n < 0)
        error("ERROR writing to socket");

    int port2 = clientPort;
    write(sockTracker, &port2, sizeof(port2));

    // ----------------------------Execute Command---------------------------
    while (1)
    {
        string command;
        cout << "Enter:   ";
        getline(cin >> ws, command);
        tokens.clear();
        tokens = processInput(command);
        writeLog("\n"+command);

        char cmd[256];
        bzero(cmd, 256);
        for (int i = 0; i < command.length(); i++)
            cmd[i] = command[i];
        int n = write(sockTracker, cmd, 255);

        if (n < 0)
            error("ERROR writing to socket");

        if ((tokens[0] == "create_group" || tokens[0] == "join_group" || tokens[0] == "leave_group" ||
             tokens[0] == "list_requests") &&
            tokens.size() == 2)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else if (tokens[0] == "list_groups" && tokens.size() == 1)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else if (tokens[0] == "accept_request" && tokens.size() == 3)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else if (tokens[0] == "logout")
        {
            break;
        }
        else if (tokens[0] == "upload_file" && tokens.size() == 3)
        {
            int flag;
            read(sockTracker, &flag, sizeof(flag));
            if (flag == 1)
            {
                uploadFile(tokens, sockTracker);
                if (listenFlag == 0)
                {
                    pthread_t tt;
                    listenFlag = 1;
                    pthread_create(&tt, NULL, uploadFunc, &clientPort);
                    pthread_detach(tt);
                }
            }
            else
            {
                writeLog("Group ID does not exsist");
                cout << "Group ID does not exsist" << endl;
            }
        }
        else if (tokens[0] == "download_file" && tokens.size() == 4)
        {
            int flag;
            read(sockTracker, &flag, sizeof(flag));
            if (flag == 0)
            {
                bzero(cmd, 256);
                read(sockTracker, cmd, 255);
                cout << cmd << endl;
                writeLog(string(cmd));
            }
            else
                downloadFunc(sockTracker, tokens);
        }
        else if (tokens[0] == "list_files" && tokens.size() == 2)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else if (tokens[0] == "show_downloads" && tokens.size() == 1)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else if (tokens[0] == "stop_share" && tokens.size() == 3)
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
        else
        {
            bzero(cmd, 256);
            read(sockTracker, cmd, 255);
            cout << cmd << endl;
            writeLog(string(cmd));
        }
    }

    return 0;
}

//assuming file can be with only one client

/*

g++ client.cpp -o client1 -lpthread

g++ client.cpp -o client2 -lpthread

g++ client.cpp -o client3 -lpthread

./client1 127.0.0.1:5050 tracker_info.txt

./client2 127.0.0.1:5050 tracker_info.txt

*/
