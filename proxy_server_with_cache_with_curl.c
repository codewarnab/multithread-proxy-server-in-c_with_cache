#include "proxy_parse.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <regex.h>
#include <curl/curl.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 8192 // Use a smaller buffer size for dynamic allocation

pthread_t tid[MAX_CLIENTS];
sem_t semaphore;
pthread_mutex_t lock;
int port_number = 8080;
int proxy_socketId;

typedef struct cache_element cache_element;

struct MemoryStruct
{
    char *memory;
    size_t size;
};

struct cache_element
{
    char *data;
    int len;
    char url[256]; // Use a reasonable length for URL
    cache_element *next;
};

cache_element *cache_head = NULL;

cache_element *find(const char *url);
char *getCurrentTime();
char *extract_url(char *request);
char *https_get(const char *url);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

void add_to_cache(const char *url, const char *response);

int handle_request(int clientSocket, const char *url)
{
    // Fetch URL from remote server
    printf("Fetching URL: %s\n", url);
    char *response = https_get(url);

    if (response == NULL)
    {
        fprintf(stderr, "Failed to fetch URL: %s\n", url);
        return -1;
    }

    // Add the response to the cache
    add_to_cache(url, response);
    printf("Added URL to cache: %s\n", url);

    // Determine response length
    int response_length = strlen(response);
    printf("Sending response of length %d bytes to client\n", response_length);

    // Prepare HTTP response headers
    const char *http_response_template = "HTTP/1.1 200 OK\r\n"
                                         "Content-Length: %d\r\n"
                                         "Content-Type: text/html\r\n"
                                         "Connection: close\r\n"
                                         "\r\n";
    char http_response[BUFFER_SIZE];
    int header_length = snprintf(http_response, BUFFER_SIZE, http_response_template, response_length);

    // Send headers
    int bytes_sent = send(clientSocket, http_response, header_length, 0);
    if (bytes_sent < 0)
    {
        perror("Error sending headers to client");
        free(response); // Free the response memory allocated in https_get
        return -1;
    }

    // Loop to send the response in chunks
    int total_bytes_sent = 0;
    while (total_bytes_sent < response_length)
    {
        bytes_sent = send(clientSocket, response + total_bytes_sent, response_length - total_bytes_sent, 0);
        if (bytes_sent < 0)
        {
            perror("Error sending data to client");
            free(response); // Free the response memory allocated in https_get
            return -1;
        }
        total_bytes_sent += bytes_sent;
    }

    printf("Response sent successfully to client:\n%s\n", response);

    free(response); // Free the response memory allocated in https_get
    return total_bytes_sent;
}

void add_to_cache(const char *url, const char *response)
{
    // Create a new cache element
    cache_element *new_element = (cache_element *)malloc(sizeof(cache_element));
    new_element->data = strdup(response); // Duplicate the response string
    new_element->len = strlen(response);
    strncpy(new_element->url, url, sizeof(new_element->url) - 1);
    new_element->url[sizeof(new_element->url) - 1] = '\0';
    new_element->next = NULL;

    // Add the new element to the cache
    pthread_mutex_lock(&lock);
    new_element->next = cache_head;
    cache_head = new_element;
    pthread_mutex_unlock(&lock);
}

void *thread_fn(void *socketNew)
{
    sem_wait(&semaphore);
    int p;
    sem_getvalue(&semaphore, &p);
    printf("%s : semaphore value : %d\n", getCurrentTime(), p);
    int *t = (int *)(socketNew);
    int socket = *t;
    int bytes_recv_client, len;

    char *buffer = (char *)malloc(BUFFER_SIZE);
    if (buffer == NULL)
    {
        perror("Error allocating buffer");
        close(socket);
        sem_post(&semaphore);
        return NULL;
    }

    memset(buffer, 0, BUFFER_SIZE);
    bytes_recv_client = recv(socket, buffer, BUFFER_SIZE - 1, 0);

    while (bytes_recv_client > 0)
    {
        len = strlen(buffer);
        if (strstr(buffer, "\r\n\r\n") == NULL)
        {
            bytes_recv_client = recv(socket, buffer + len, BUFFER_SIZE - 1 - len, 0);
        }
        else
        {
            break;
        }
    }
    printf("--------------------------------------------\n");
    printf("buffer = %s\n", buffer);
    printf("--------------------------------------------\n");

    const char *url = extract_url(buffer);

    if (url != NULL)
    {
        cache_element *temp = find(url);
        if (temp != NULL)
        {
            printf("URL found in cache: %s\n", temp->url);
            int size = temp->len;
            int pos = 0;
            while (pos < size)
            {
                char response[BUFFER_SIZE];
                memset(response, 0, BUFFER_SIZE);
                int chunk_size = (size - pos > BUFFER_SIZE) ? BUFFER_SIZE : size - pos;
                memcpy(response, temp->data + pos, chunk_size);
                pos += chunk_size;
                send(socket, response, chunk_size, 0);
            }
            printf("Data retrieved from the Cache\n\n");
        }
        else
        {
            printf("URL not found in cache.\n");
            if (handle_request(socket, url) == -1)
            {
                perror("Failed to handle request");
            }
        }
    }
    else if (bytes_recv_client < 0)
    {
        perror("Error in receiving from client.\n");
    }
    else if (bytes_recv_client == 0)
    {
        printf("Client disconnected!\n");
    }

    shutdown(socket, SHUT_RDWR);
    close(socket);
    free(buffer);
    sem_post(&semaphore);

    sem_getvalue(&semaphore, &p);
    printf("Semaphore post value:%d\n", p);
    return NULL;
}

int main(int argc, char *argv[])
{
    int client_socketId, client_len;
    struct sockaddr_in server_addr, client_addr;

    sem_init(&semaphore, 0, MAX_CLIENTS);
    pthread_mutex_init(&lock, NULL);

    if (argc == 2)
    {
        port_number = atoi(argv[1]);
    }
    else
    {
        printf("%s : Too few arguments\n", getCurrentTime());
        exit(1);
    }
    printf("%s : Setting proxy Server Port : %d\n", getCurrentTime(), port_number);

    proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);

    if (proxy_socketId < 0)
    {
        perror("Failed to create socket.\n");
        exit(1);
    }

    int reuse = 1;
    if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed\n");

    memset((char *)&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_number); // Assigning port to the Proxy
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Any available address assigned

    // Binding the socket
    if (bind(proxy_socketId, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("%s : Port is not free\n", getCurrentTime());
        exit(1);
    }
    printf("%s : Binding on port: %d\n\n", getCurrentTime(), port_number);

    // Proxy socket listening to the requests
    int listen_status = listen(proxy_socketId, MAX_CLIENTS);

    if (listen_status < 0)
    {
        perror("Error while Listening !\n");
        exit(1);
    }

    int i = 0;                           // Iterator for thread_id (tid) and Accepted Client_Socket for each thread
    int Connected_socketId[MAX_CLIENTS]; // This array stores socket descriptors of connected clients

    // Infinite Loop for accepting connections
    while (1)
    {
        memset((char *)&client_addr, 0, sizeof(client_addr)); // Clears struct client_addr
        client_len = sizeof(client_addr);

        // Accepting the connections
        client_socketId = accept(proxy_socketId, (struct sockaddr *)&client_addr, (socklen_t *)&client_len); // Accepts connection
        if (client_socketId < 0)
        {
            fprintf(stderr, "Error in Accepting connection !\n");
            exit(1);
        }
        else
        {
            Connected_socketId[i] = client_socketId; // Storing accepted client into array
        }

        // Getting IP address and port
        struct sockaddr_in *ipv4_client = (struct sockaddr_in *)&client_addr;
        void *addr_client = &(ipv4_client->sin_addr);
        char ip_client[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, addr_client, ip_client, INET_ADDRSTRLEN);
        int port_client = ntohs(ipv4_client->sin_port);
        printf("Connected to client %s:%d\n", ip_client, port_client);

        // Creating thread for each client
        if (pthread_create(&tid[i], NULL, thread_fn, &Connected_socketId[i]) != 0)
        {
            fprintf(stderr, "Failed to create thread for client %s:%d\n", ip_client, port_client);
            continue;
        }

        pthread_detach(tid[i]);
        i = (i + 1) % MAX_CLIENTS; // Loop index i back to 0 after reaching MAX_CLIENTS
    }

    close(proxy_socketId);
    return 0;
}

// Helper functions

cache_element *find(const char *url)
{
    pthread_mutex_lock(&lock);
    cache_element *current = cache_head;
    while (current != NULL)
    {
        if (strcmp(current->url, url) == 0)
        {
            pthread_mutex_unlock(&lock);
            return current;
        }
        current = current->next;
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

char *getCurrentTime()
{
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    return asctime(timeinfo);
}

char *extract_url(char *request)
{
    const char *delimiter = "\n";
    char *token;
    char *copy = strdup(request); // Make a copy of the request string
    char *saveptr;
    char *url = NULL;

    token = strtok_r(copy, delimiter, &saveptr);

    while (token != NULL)
    {
        if (strncmp(token, "GET", 3) == 0)
        {
            // Found the line starting with 'GET'
            char *url_start = strchr(token, ' ') + 1;
            char *url_end = strchr(url_start, ' ');
            if (url_end != NULL)
            {
                // Calculate URL length and copy it, skipping the leading '/'
                int url_length = url_end - url_start;
                if (url_start[0] == '/')
                {
                    url_start++; // Skip the leading '/'
                    url_length--;
                }
                url = (char *)malloc((url_length + 1) * sizeof(char));
                strncpy(url, url_start, url_length);
                url[url_length] = '\0';
                break;
            }
            else
            {
                printf("Invalid GET request format\n");
            }
        }
        token = strtok_r(NULL, delimiter, &saveptr);
    }

    free(copy); // Free the allocated copy

    return url;
}

char *https_get(const char *url)
{
    CURL *curl_handle;
    CURLcode res;

    struct MemoryStruct chunk;
    chunk.memory = malloc(1); // Initial size of 1 byte
    chunk.size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if (curl_handle)
    {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);

        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(chunk.memory); // Free allocated memory on error
            curl_easy_cleanup(curl_handle);
            return NULL;
        }

        curl_easy_cleanup(curl_handle);
    }

    curl_global_cleanup();
    return chunk.memory;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (ptr == NULL)
    {
        // out of memory!
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}
