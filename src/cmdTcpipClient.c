#include <stdio.h>
#include <unistd.h>
#include <memory.h>
#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#ifdef __WIN32__
#pragma comment(lib, "Ws2_32.lib")
#endif

int lsock;
int csock = 0;

int startSocket(void)
{
#ifdef __WIN32__
	WSADATA wsa;
	return WSAStartup(MAKEWORD(2, 0), &wsa);
#else
	return 0;
#endif
}

int setupCmdTcpServer(int port)
{
	
	struct sockaddr_in addr;
	int rc;
	rc = startSocket();

	if (rc != 0)
	{
		printf("Fehler: startWinsock, fehler code: %d\n", rc);
		return 1;
	}
	else
	{
		lsock = socket(AF_INET, SOCK_STREAM, 0);

		memset(&addr, 0, sizeof(addr)); // set to 0
		addr.sin_family = AF_INET;
		addr.sin_port = htons((unsigned short)port); // port
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if ((bind(lsock, (struct sockaddr*)& addr, sizeof(addr))) != 0)
		{
			printf("socket bind failed...\n");
			return 0;
		}
		else
		{
			printf("Socket successfully binded..\n");

			if (listen(lsock, 5))
			{
				printf("listen failed\n");
				return 0;
			}
		}
	}

	return 1;
}

void closeTcpConnection()
{
	char rbuff[1] = { 0 };

	printf("close connection\n");
	if (csock)
	{
		shutdown(csock, SHUT_WR);
		while (recv(csock, rbuff, 1, 0) > 0);
#ifdef __WIN32__
		closesocket(csock);
#else
		close(csock);
#endif
		csock = 0;
	}

}

void sendTcpData(char* data, int lenght)
{
	if (csock)
		if (send(csock, data, (unsigned short)lenght, MSG_NOSIGNAL) < 0) closeTcpConnection();
}

int waitForTcpData(char* cmdData, int lenght)
{
	int ret = 0;

	do
	{
		if (csock == 0)
		{
			printf("wait for incomming connection\n");

			csock = accept(lsock, NULL, NULL);

			printf("incomming connection\n");
		}
		else
		{
			ret = recv(csock, cmdData, (unsigned int)lenght, 0);

			if (ret == 0)
			{
				printf("socket closed\n");
				closeTcpConnection();
			}
			else if (ret < 0)
			{
				printf("socket error\n");
				csock = 0;
				return 0;
			}
		}

	}
	while (ret < 1);
	
	return ret;
}