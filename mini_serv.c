#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/ip.h>

typedef struct s_client {
    size_t  id;
    char    msg[4096];
}   t_client;

void makeError(char *str)
{
    if (str)
        write(2, str, strlen(str));
    else
        write(2, "Fatal error", strlen("Fatal error"));
    write(2, "\n", 1);
    exit(1);
}

void broadcastMessage(int socketFd, int maxFd, fd_set *readSet, char writeBuffer[], int ignoreFd)
{
    for (int i = 0; i <= maxFd; i++)
    {
        if (i == socketFd)
            write(1, writeBuffer, strlen(writeBuffer));
        else if (FD_ISSET(i, readSet) && i != ignoreFd)
            send(i, writeBuffer, strlen(writeBuffer), 0);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
        makeError("Wrong number of arguments");

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd == -1)
        makeError(NULL);

    int nextId = 0;
    int usedPort = atoi(argv[1]);
    t_client clients[FD_SETSIZE];
    char readBuffer[FD_SETSIZE * 100];
    char writeBuffer[FD_SETSIZE * 100];

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    memset(&addr, 0, addr_len);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(usedPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int bindRes = bind(socketFd, (struct sockaddr *)&addr, addr_len);
    if (bindRes == -1)
        makeError(NULL);

    int listenRes = listen(socketFd, SOMAXCONN);
    if (listenRes == -1)
        makeError(NULL);

    fd_set actives;
    FD_ZERO(&actives);
    FD_SET(socketFd, &actives);

    while (1)
    {
        fd_set copies;
        int maxFd = socketFd;

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &actives))
            {
                FD_SET(i, &copies);
                if (maxFd < i)
                    maxFd = i;
            }

        }

        int selectRes = select(maxFd + 1, &copies, NULL, NULL, NULL);
        if (selectRes == -1)
            makeError(NULL);

        for (int i = 0; i <= maxFd; i++)
        {
            if (FD_ISSET(i, &copies))
            {
                if (i == socketFd)
                {

                    if (nextId >= FD_SETSIZE)
                        makeError(NULL);

                    int newClientFd = accept(i, (struct sockaddr *)&addr, &addr_len);
                    if (newClientFd == -1)
                        makeError(NULL);

                    clients[newClientFd].id = nextId++;
                    FD_SET(newClientFd, &actives);

                    sprintf(writeBuffer, "server: client %ld just arrived\n", clients[newClientFd].id);
                    broadcastMessage(socketFd, maxFd, &copies, writeBuffer, -1);
                    break;

                }
                else
                {

                    int recvRes = recv(i, readBuffer, 4096, 0);
                    if (recvRes == -1)
                        makeError(NULL);
                    else if (recvRes == 0)
                    {
                        sprintf(writeBuffer, "server: client %ld just left\n", clients[i].id);
                        broadcastMessage(socketFd, maxFd, &copies, writeBuffer, i);
                        FD_CLR(i, &actives);
                        close(i);
                        break;
                    }
                    else
                    {

                        for (int k = 0, j = strlen(clients[i].msg); k < recvRes; k++, j++)
                        {
                            clients[i].msg[j] = readBuffer[k];

                            if (clients[i].msg[j] == '\n')
                            {
                                clients[i].msg[j] = '\0';
                                sprintf(writeBuffer, "client %ld: %s\n", clients[i].id, clients[i].msg);
                                broadcastMessage(socketFd, maxFd, &copies, writeBuffer, i);
                                bzero(&clients[i].msg, strlen(clients[i].msg));
                                j = -1;
                            }
                        }
                        break;

                    }
                }
            }
        }
    }

    return (0);
}