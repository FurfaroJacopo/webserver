#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 64000
#define BACKLOG 1024
#define ROOT "root"
#define NUM_WORKERS 5
#define MAX_REQUESTS 500
char* string_append(char* string1, char* string2) {
    int len1 = strlen(string1);
    int len2 = strlen(string2);
    int final_len = len1+len2+1;
    char* final_str = (char*)calloc(final_len, sizeof(char));
    if (!final_str) return NULL;
    strcpy(final_str,string1);
   strcat(final_str,string2);
    return final_str;
}
char* resolve_ext(char* content) {
<<<<<<< HEAD
    if(strncmp("text/html",content, sizeof(content)) == 0) {
=======
    printf("IL MIO EXT: %s\n", content);
    if(strncmp("text/html",content, 9) == 0) {
>>>>>>> 80e8bbb (Add main binary and JSON response file; update server configuration and request handling)
        return ".html";
    } else if(strncmp("application/json",content, 16)==0) {
        return ".json";
    } else return ".txt";
}
void resolve_put(char* body, char* content_type, int sock, char* len) {
    time_t currentTime;
    time(&currentTime);
    char* ext = resolve_ext(content_type);
    FILE * pFile;
    char *filename =string_append(ctime(&currentTime),ext);
    filename = string_append("/",filename);
    filename = string_append(ROOT,filename);
    	char*  header = string_append("HTTP/1.1 201 Created\r\nContent-Location: ", filename);
    	header = string_append(header,"Content-Length: ");
    	header = string_append(header, len);
    	header = string_append(header, "\r\n"); //http header to signal html
	
    pFile = fopen (filename,"w");
  if (pFile!=NULL)
  {
    fputs (body,pFile);
    send(sock, header, strlen(header),0);
    fclose (pFile);
  } else perror("file error\n\r");

  free(filename);
}
void handle_sigchld(int sig) {
    (void)sig; // Suppress unused parameter warning
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap all terminated child processes
    }
}
void sendHTML(int sock, char *file) {
    size_t read; 
    char buffer[BUFFER_SIZE] = {0};
    // 1. Inizializziamo SEMPRE header a NULL per sicurezza
    char* header = NULL; 

    if ((access(file, R_OK)) == 0) {
        if (strcmp(file, "root/favicon.ico") == 0) {
            file = "public/favicon.ico";
            header = "HTTP/1.1 200 OK\r\nContent-Type: image/x-icon\r\n"; // Header corretto per favicon
        }
        else {
            char root_slash[32];
            snprintf(root_slash, sizeof(root_slash), "%s/", ROOT);
            if (strcmp(file, root_slash) == 0) {
                file = "root/index.html"; 
            }
            // Qualsiasi file valido esistente (sia index che altri file richiesti da wrk)
            header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";   
        }
    } else {
        if (errno == ENOENT) {
            file = "public/404.html";
            header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n";
        } 
        else if (errno == EACCES) {
            file = "public/403.html";
            header = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/html\r\n";
        } else {
            // Fallback generico per altri errori di sistema (es. troppi file aperti)
            file = "public/404.html"; 
            header = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n";
        }
    }
    
    // Controlliamo la dimensione del file reale che andremo a servire
    struct stat st;
    if (stat(file, &st) < 0) {
        st.st_size = 0; 
    }

    // Composizione sicura dell'header HTTP
    char header_full[512]; 
    snprintf(header_full, sizeof(header_full), "%sConnection: close\r\nContent-Length: %ld\r\n\r\n", header, st.st_size);
    
    // Invio dell'header
    send(sock, header_full, strlen(header_full), 0);

    // Invio del file corpo
    FILE *html = fopen(file, "r");
    if (!html) return;

    size_t bytes_read = 0; 
    while ((bytes_read = fread(buffer, sizeof(buffer[0]), BUFFER_SIZE, html)) > 0) {
        send(sock, buffer, bytes_read, 0);
    }
    fclose(html);
}

void start_worker(int sockfd) {
    while (1) {
        struct sockaddr_in clientAddr;
        socklen_t clientlen = sizeof(clientAddr);
    	int clientSocket = accept(sockfd, (struct sockaddr *)&clientAddr, &clientlen);
        
        if (clientSocket < 0) {
            if (errno == EINTR) continue;
            perror("could not accept client");
            continue;
        }
<<<<<<< HEAD
	        char recBuf[BUFFER_SIZE] = {0};
	        recv(clientSocket, recBuf, BUFFER_SIZE, 0);
            char* recBuf_copy = malloc(sizeof(recBuf));
            memcpy(recBuf_copy,recBuf,sizeof(recBuf));
            printf("rec: %s\n", recBuf_copy);
	        char* method = strtok(recBuf_copy, " ");
	        char* route = strtok(NULL, " ");
            char* http_ver = strtok(NULL, " \n");
            strtok(NULL,"\n");
            strtok(NULL,"\n");
            strtok(NULL,"\n");
            strtok(NULL, " ");
            char* content = strtok(NULL, "\n");            
            char* len = strtok(NULL, "\n");
            printf("method: %s\n", method);
            printf("route: %s\n", route);
            printf("http version: %s\n", http_ver);
            printf("content: %s\n", content);
	        char htmlDir[] = ROOT;
	        char* finalroute = string_append(htmlDir, route);
	        printf("fr: %s\n", finalroute);
	        printf("client connected\n");
	        if (!fork()) { // this is the child process
                if(strcmp(method, "GET") == 0) {
                    sendHTML(clientSocket,finalroute);
                } else if(strcmp(method, "PUT")==0) {
                    const char *header_end_marker = "\r\n\r\n";
                    char *body_start = strstr(recBuf, header_end_marker);
                    resolve_put(body_start,content,clientSocket, len);
                }
    	        close(sockfd);	    
	            printf("client out\n");
	            exit(0);
             }
                 close(clientSocket);
             free(finalroute);
           
=======

        char recBuf[BUFFER_SIZE] = {0};
        int received = recv(clientSocket, recBuf, BUFFER_SIZE - 1, 0);
        if (received <= 0) {
            close(clientSocket);
            continue;
        }

        char* recBuf_copy = strdup(recBuf);
        if (!recBuf_copy) {
            close(clientSocket);
            continue;
        }

        char* method = strtok(recBuf_copy, " ");
        char* route = strtok(NULL, " ");
        
        if (method && route) {
            char* finalroute = string_append(ROOT, route);
            
            if (strcmp(method, "GET") == 0) {
                sendHTML(clientSocket, finalroute);
            } else if (strcmp(method, "PUT") == 0) {
                strtok(NULL, " \n"); // version
                strtok(NULL, "\n"); strtok(NULL, "\n"); strtok(NULL, "\n");
                strtok(NULL, " ");
                char* content = strtok(NULL, "\n");
                
                const char *header_end_marker = "\r\n\r\n";
                char *body_start = strstr(recBuf, header_end_marker);
                if (body_start) body_start += 4;
                
                resolve_put(body_start, content);
            }
            free(finalroute);
        }

        // RILASCIO CORRETTO RISORSE
        free(recBuf_copy);
        close(clientSocket); // Manda il segnale di chiusura corretto a wrk
        // RIMOSSO il close(sockfd) distruttivo da qui
>>>>>>> 80e8bbb (Add main binary and JSON response file; update server configuration and request handling)
    }
}
int main()
{
    


    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {   // AF_INET: IPV4, SOCK_STREAM: TCP, 0: protocol already set by sockstream
    // socket() create the socket endpoint 
        perror("couldnt get socket fd"); //print into stderror
        return -1;
    }
    const int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    perror("setsockopt(SO_REUSEADDR) failed"); 

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
    signal(SIGCHLD, handle_sigchld);
    for(int i=0 ; i<NUM_WORKERS; i++) {
        pid_t pid = fork();
        if(pid==0) {
            start_worker(sockfd);
            
            exit(0);
        }
    }
    while(1) {
        pause(); // Wait for signals
    }
    close(sockfd);
    sockfd = -1;
    return 0;
}
