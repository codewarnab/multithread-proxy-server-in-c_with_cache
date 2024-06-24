CC = gcc
CFLAGS = -Wall -pthread
LDFLAGS = -lcurl
TARGET = proxy_server_with_cache_with_curl
OBJS = proxy_server_with_cache_with_curl.o proxy_parse.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

proxy_server_with_cache_with_curl.o: proxy_server_with_cache_with_curl.c proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_server_with_cache_with_curl.c

proxy_parse.o: proxy_parse.c proxy_parse.h
	$(CC) $(CFLAGS) -c proxy_parse.c

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
