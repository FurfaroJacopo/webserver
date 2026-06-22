#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
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
#include <uv.h>

#define PORT 8080
#define BUFFER_SIZE 65536
#define BACKLOG 1024
#define ROOT "root"
#define NUM_WORKERS 5
#define MAX_REQUESTS 500
static uv_loop_t *loop;
typedef struct {
    uv_tcp_t handle;          // The actual libuv handle
    char buffer[BUFFER_SIZE]; // Our persistent stream buffer
    size_t buf_len;           // How many bytes are currently in the buffer
} client_context_t;

static void my_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf);
static void my_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void close_client(uv_stream_t *stream);
static void handle_request(uv_stream_t *stream, client_context_t *client_context);

char* string_append(const char* string1, const char* string2) {
    size_t len1 = strlen(string1);
    size_t len2 = strlen(string2);
    size_t final_len = len1+len2+1;
    char* final_str = (char*)calloc(final_len, sizeof(char));
    if (!final_str) return NULL;
    strcpy(final_str,string1);
    strcat(final_str,string2);
    return final_str;
}

static void my_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    if(nread>0) {
        client_context_t* my_data = (client_context_t*) stream->data;
        size_t bytes_read = (size_t)nread;
        if (my_data->buf_len + bytes_read < BUFFER_SIZE) {
            memcpy(my_data->buffer + my_data->buf_len, buf->base, bytes_read);
            my_data->buf_len += bytes_read;
            my_data->buffer[my_data->buf_len] = '\0';

            char *headers_end = strstr(my_data->buffer, "\r\n\r\n");
            if (headers_end != NULL) {
                char *content_length_value = NULL;
                size_t content_length = 0;

                content_length_value = strstr(my_data->buffer, "Content-Length:");
                if (content_length_value != NULL && content_length_value < headers_end) {
                    content_length_value += strlen("Content-Length:");
                    content_length = (size_t)strtoul(content_length_value, NULL, 10);
                }

                size_t header_size = (size_t)(headers_end - my_data->buffer) + 4;
                if (my_data->buf_len >= header_size + content_length) {
                    uv_read_stop(stream);
                    handle_request(stream, my_data);
                }
            }
        } else {
            close_client(stream);
        }
    }

    if(nread<0) {
        if (nread != UV_EOF) {
            fprintf(stderr, "%s\n", uv_strerror((int)nread));
        }
        close_client(stream);
    }
    if(nread == UV_EOF) {
        close_client(stream);
    }

    if (buf->base) {
        free(buf->base);
    }
}

const char* resolve_ext(const char* content) {
    if(content != NULL && strncasecmp("text/html", content, strlen("text/html")) == 0) {
        return ".html";
    } else if(content != NULL && strncasecmp("application/json", content, strlen("application/json")) == 0) {
        return ".json";
    } else return ".txt";
}
void resolve_put(const char* body, const char* content_type, int sock, const char* len) {
    time_t currentTime;
    time(&currentTime);
    const char* ext = resolve_ext(content_type);
    char timestamp[64];
    char filename[256];
    char header[512];
    struct tm *local_time = localtime(&currentTime);

    if (local_time == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d_%H-%M-%S", local_time) == 0) {
        snprintf(timestamp, sizeof(timestamp), "%ld", (long)currentTime);
    }

    snprintf(filename, sizeof(filename), "%s/%s%s", ROOT, timestamp, ext);
    snprintf(header, sizeof(header),
             "HTTP/1.1 201 Created\r\n"
             "Content-Location: %s\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n"
             "\r\n",
             filename);

    FILE *pFile = fopen(filename, "w");
    if (pFile != NULL) {
        fputs(body != NULL ? body : "", pFile);
        send(sock, header, strlen(header), 0);
        fclose(pFile);
    } else {
        perror("file error\n\r");
    }

    (void)len;
}
void handle_sigchld(int sig) {
    (void)sig; // Suppress unused parameter warning
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        // Reap all terminated child processes
    }
}
static void my_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  (void)handle;
  buf->base = malloc(suggested_size);
  buf->len = suggested_size;
}
void sendHTML(int sock, char *file) {
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

static char *copy_header_value(const char *request, const char *name) {
    const char *headers_start = strstr(request, "\r\n");
    if (headers_start == NULL) return NULL;
    headers_start += 2;

    const char *headers_end = strstr(headers_start, "\r\n\r\n");
    if (headers_end == NULL) return NULL;

    size_t name_len = strlen(name);
    const char *line = headers_start;
    while (line < headers_end) {
        const char *line_end = strstr(line, "\r\n");
        if (line_end == NULL || line_end > headers_end) {
            line_end = headers_end;
        }

        if ((size_t)(line_end - line) > name_len &&
            strncasecmp(line, name, name_len) == 0 &&
            line[name_len] == ':') {
            const char *value_start = line + name_len + 1;
            while (value_start < line_end &&
                   (*value_start == ' ' || *value_start == '\t')) {
                value_start++;
            }

            const char *value_end = line_end;
            while (value_end > value_start &&
                   (value_end[-1] == ' ' || value_end[-1] == '\t')) {
                value_end--;
            }

            size_t value_len = (size_t)(value_end - value_start);
            char *value = malloc(value_len + 1);
            if (value == NULL) return NULL;
            memcpy(value, value_start, value_len);
            value[value_len] = '\0';
            return value;
        }

        line = line_end + 2;
    }

    return NULL;
}

static void close_client_cb(uv_handle_t *handle) {
    free(handle->data);
}

static void close_client(uv_stream_t *stream) {
    uv_read_stop(stream);
    if (!uv_is_closing((uv_handle_t*)stream)) {
        uv_close((uv_handle_t*)stream, close_client_cb);
    }
}

static void handle_request(uv_stream_t *stream, client_context_t *client_context) {
    uv_os_fd_t client_socket;
    char *request_copy = malloc(client_context->buf_len + 1);
    if (request_copy == NULL) {
        close_client(stream);
        return;
    }

    memcpy(request_copy, client_context->buffer, client_context->buf_len);
    request_copy[client_context->buf_len] = '\0';

    char *line_end = strstr(request_copy, "\r\n");
    if (line_end == NULL) {
        free(request_copy);
        close_client(stream);
        return;
    }
    *line_end = '\0';

    char *method = strtok(request_copy, " ");
    char *route = strtok(NULL, " ");
    char *http_ver = strtok(NULL, " ");
    if (method == NULL || route == NULL || http_ver == NULL) {
        free(request_copy);
        close_client(stream);
        return;
    }

    if (uv_fileno((const uv_handle_t*)stream, &client_socket) != 0) {
        free(request_copy);
        close_client(stream);
        return;
    }

    char *finalroute = string_append(ROOT, route);
    if (finalroute == NULL) {
        free(request_copy);
        close_client(stream);
        return;
    }

    printf("method: %s\n", method);
    printf("route: %s\n", route);
    printf("http version: %s\n", http_ver);
    printf("fr: %s\n", finalroute);
    printf("client connected\n");

    if (strcmp(method, "GET") == 0) {
        sendHTML((int)client_socket, finalroute);
    } else if (strcmp(method, "PUT") == 0) {
        char *content = copy_header_value(client_context->buffer, "Content-Type");
        char *len = copy_header_value(client_context->buffer, "Content-Length");
        const char *header_end_marker = "\r\n\r\n";
        char *body_start = strstr(client_context->buffer, header_end_marker);
        if (body_start != NULL) {
            body_start += strlen(header_end_marker);
        }
        resolve_put(body_start, content, (int)client_socket, len);
        free(content);
        free(len);
    }

    printf("client out\n");

    free(finalroute);
    free(request_copy);
    close_client(stream);
}

void start_worker(uv_stream_t *server, int status) {


    if (status < 0) {
        fprintf(stderr, "New connection error %s\n", uv_strerror(status));
        // error!
        return;
    }
    client_context_t *client_context = calloc(1, sizeof(client_context_t));
    if (client_context == NULL) {
        return;
    }

    uv_tcp_init(loop, &client_context->handle);
    client_context->handle.data = client_context;

    if (uv_accept(server, (uv_stream_t*) &client_context->handle) == 0) {
        int read_result = uv_read_start((uv_stream_t*) &client_context->handle,
                                        my_alloc_cb,
                                        my_read_cb);
        if (read_result < 0) {
            fprintf(stderr, "Read start error %s\n", uv_strerror(read_result));
            close_client((uv_stream_t*) &client_context->handle);
        }
    } else {
        close_client((uv_stream_t*) &client_context->handle);
    }
}

int main()
{
    loop = uv_default_loop();

    uv_tcp_t server;
    uv_tcp_init(loop, &server);

    struct sockaddr_in socketAddrIn;
    memset(&socketAddrIn, 0, sizeof(socketAddrIn));
     // sockaddr_in define where to connect() or where to bind() the port
    socketAddrIn.sin_family = AF_INET; // it will accept both ipv4 and 6
    socketAddrIn.sin_addr.s_addr = INADDR_ANY; // it will accept any request
    socketAddrIn.sin_port = htons(PORT); //host to network byte order conversion LE BE
    uv_ip4_addr("0.0.0.0", PORT, &socketAddrIn);
    // binds socket to network port
    if(uv_tcp_bind(&server, (const struct sockaddr*)&socketAddrIn, 0) < 0 )
    {
        perror("couldnt bind to server socket");
        return -1;

    }
    if(     uv_listen((uv_stream_t*) &server, BACKLOG, start_worker) < 0 ) { // BACKLOG -> max number of request that can be queued
        perror("could not listen on server socket and ip");
        return -1;

    }

    return uv_run(loop, UV_RUN_DEFAULT);
}
