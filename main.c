#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 6767
#define BUFFER_SIZE 1024
#define BACKLOG 10

void sendHTML(int sock, char *file) {
    FILE *html = fopen(file, "r");
    if(!html) {
        perror("couldnt open html file");
        return;
    }

    size_t read; //size_t è un unsigned integer che viene usato com return type di size of e che sarà quindi sempre positivo
    char buffer[BUFFER_SIZE] = {0};
    
    char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"; //http header to signal html
    send(sock, header, strlen(header),0);

    while ((read = fread(buffer,sizeof(buffer[0]), BUFFER_SIZE, html)) > 0) {
        send(sock, buffer, read, 0);
    }
  
    fclose(html);
}
int main()
{
    int sockfd, send_sock;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {   // AF_INET: IPV4, SOCK_STREAM: TCP, 0: protocol already set by sockstream
    // socket() create the socket endpoint 
        perror("couldnt get socket fd"); //print into stderror
        return -1;
    } 

    struct sockaddr_in socketAddrIn;
    memset(&socketAddrIn, 0, sizeof(socketAddrIn)); 
     // sockaddr_in define where to connect() or where to bind() the port
    socketAddrIn.sin_family = AF_INET; // it will accept both ipv4 and 6
    socketAddrIn.sin_addr.s_addr = INADDR_ANY; // it will accept any request
    socketAddrIn.sin_port = htons(PORT); //host to network byte order conversion LE BE

    if(bind(sockfd, (struct  sockaddr *) &socketAddrIn, sizeof socketAddrIn) < 0) // binds socket to network port 
    {
        perror("couldnt bind to server socket");
        return -1;

    }
    if(listen(sockfd, BACKLOG ) < 0 ) { // BACKLOG -> max number of request that can be queued
        perror("could not listen on server socket and ip");
        return -1;

    }

    printf("listening on port %d\n", PORT);

    while (1)
    {
        struct sockaddr_in clientAddr;
        socklen_t clientlen = sizeof clientAddr ;
    int clientSocket;
        if((clientSocket = accept(sockfd, (struct sockaddr *)&clientAddr, &clientlen)) < 0) {

            perror("could not accept client");
            continue;
        }

        printf("client connected\n");
  
        sendHTML(clientSocket, "html/index.html");
        close(clientSocket);
        printf("client out\n");
    }
    close(sockfd);
    
    return 0;
}