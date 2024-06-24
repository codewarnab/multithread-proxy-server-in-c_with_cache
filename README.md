# Proxy Server with Caching

## Overview
This project is a multithreaded proxy server with caching capabilities, implemented in C. The server uses the `libcurl` library to fetch data from remote servers and caches the responses to improve performance for repeated requests. The project includes the following main functionalities:
- Multithreaded handling of multiple client connections.
- Caching of responses to reduce the number of remote fetches.
- Fetching data from remote servers using HTTPS.
- Managing concurrency using pthreads and semaphores.

## Prerequisites
Ensure you have the following installed on your system:
- `gcc`: The GNU Compiler Collection
- `libcurl`: The cURL library for transferring data with URLs

## Getting Started
To get started with this project, clone the repository and navigate to the project directory. The repository includes a `Makefile` to simplify the build process.

### Clone the Repository
```sh
git clone https://github.com/your-username/proxy-server-with-caching.git
cd proxy-server-with-caching
```
## Build the Project 
The Makefile provided in the repository automates the compilation process. To build the project, simply run

```sh
make
```
This will compile the source files and generate the executable proxy_server_with_cache_with_curl.

## Run the Proxy Server
To start the proxy server, execute the following command:
```sh
./proxy_server_with_cache_with_curl <port_number>
```
Replace <port_number> with the port number you want the proxy server to listen on. For example:

```sh
./proxy_server_with_cache_with_curl 8080
```
## Connect to proxy server 

go to `http://localhost:8080/http://example.com` in browser incognito tag

This will send a request to the proxy server, which will fetch the content from http://example.com, cache the response, and return it to the client.

## Understanding the Code
The main components of the project are:

- **proxy_server_with_cache_with_curl.c**: Contains the main server logic, including thread management, client handling, and caching.
- **proxy_parse.h** and **proxy_parse.c**: Include helper functions for parsing HTTP requests and extracting URLs.

## Beej's Guide
For a comprehensive understanding of network programming in C, refer to [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/pdf/bgnet_a4_c_1.pdf). This guide is an excellent resource for learning the fundamentals of socket programming and network communication in C.

## Future Work
There are several enhancements that can be made to this project:

- Implement more robust error handling and logging.
- Add support for HTTP POST and other methods.
- Improve caching strategies (e.g., adding expiration times for cache entries).
- Implement a configuration file for customizable settings (e.g., port number, cache size).

Feel free to contribute to the project by forking the repository and submitting pull requests!



