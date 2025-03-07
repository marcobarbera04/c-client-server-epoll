#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 256
#define MAX_LISTEN 10
#define MAX_EVENTS 10

// function to set to nonblocking a socket
void set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int main()
{
    int server_fd;
    struct sockaddr_in server_address;
    socklen_t server_address_lenght = sizeof(server_address);

    char buffer[BUFFER_SIZE];

    char *ack_message = "The server received your message";

    // create server socket
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Could not create the socket");
        exit(EXIT_FAILURE);
    }

    // set to nonblocking the server socket
    set_nonblocking(server_fd);

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    // bind
    if(bind(server_fd, (struct sockaddr *)&server_address, server_address_lenght) < 0)
    {
        perror("Could not bind the address to socket");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // listen
    if(listen(server_fd, MAX_LISTEN) < 0)
    {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
   
    // create the epoll fd
    int epoll_fd = epoll_create1(0);
    if(epoll_fd == -1)
    {
        perror("epoll_create1()");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    struct epoll_event server_event;            // create the server event
    server_event.data.fd = server_fd;           // set the server_fd as the first fd to be monitored
    server_event.events = EPOLLIN | EPOLLET;    // combine two flags to monitor edge triggered read event 
    
    // add the server event to the epoll_fd
    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event) == -1)
    {
        perror("epoll_ctl() server_fd");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    // array of structure to track fds and events
    struct epoll_event events[MAX_EVENTS];

    while(true)
    {
        // epoll_wait to get the numbers of events
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(num_events == -1)
        {
            perror("wait()");
            close(server_fd);
            close(epoll_fd);
            exit(EXIT_FAILURE);
        }

        for(int i = 0; i < num_events; i++)
        {
            if(events[i].data.fd == server_fd)
            {
                int new_client_fd;
                if((new_client_fd = accept(server_fd, (struct sockaddr *)&server_address, &server_address_lenght)) > 0)
                {
                    set_nonblocking(new_client_fd);

                    struct epoll_event new_client_event;
                    new_client_event.data.fd = new_client_fd;
                    new_client_event.events = EPOLLIN | EPOLLET;

                    if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_client_fd, &new_client_event) == -1)
                    {
                        perror("epoll_ctl() new_client_fd");
                        close(new_client_fd);
                    }

                    printf("Client %d connected\n", new_client_fd);
                }
            }
            else    // clients fds
            {
                if(events[i].events & EPOLLIN)
                {
                    int client_fd = events[i].data.fd;
                    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
    
                    if(bytes_received == 0)
                    {
                        printf("Client %d disconnectd\n", client_fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                        close(client_fd);
                    }
                    else if(bytes_received < 0)
                    {
                        perror("error on recv()");
                    }
                    else
                    {
                        buffer[bytes_received - 1] = '\0';  // null terminate string
                        printf("[CLIENT %d]: %s\n", client_fd, buffer);
                        
                        send(client_fd, ack_message, strlen(ack_message), 0);
                        printf("Message sent to client %d\n", client_fd);
                    }
                }
            }
        }    
    }

    close(server_fd);

    return 0;
}