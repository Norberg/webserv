#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define buffsize 512

void * th_response(void *arg)
{
	int new_socket = (int)arg;
	char *message="HTTP/1.0 501 Not Implemented \r\n";
	char docroot[512]="/var/www/";
	char *temp;
	char *uri;
	char *res;
	char buffer[buffsize];
	char finput[1024];
	int fd = 0;

	recv(new_socket,buffer,buffsize,0);
	printf("\nMessage recived: %s\n", buffer);
	if (strncmp(buffer, "GET",3) == 0)
	{
		temp = buffer; // TODO do we really need this
		uri = strsep(&temp, " "); // GET
		uri = strsep(&temp, " ");
		strcat(docroot, uri);
		res = realpath(docroot, NULL);
		printf("REALPATH: %s\n",res);
		if(res == NULL)
		{
			strcpy(finput,"HTTP/1.0 404 Not Found \r\n");
		}
		else
		{
			fd = open(res, O_RDONLY);
			if(read(fd, finput, 1024) == -1)
				strcpy(finput,"HTTP/1.0 501 Internal Server Error\r\n");
		}
		printf("Sending: %s\n", finput);
		send(new_socket,finput,strlen(finput),0);
		free(res);
		close(fd);
	}
	else
	{
		send(new_socket,message,strlen(message),0);
	}	
	close(new_socket);
	return ((void *)0);
}


int main()
{
	pthread_t ntid;
	int opt=1;
	int socket_desc;
	struct sockaddr_in address;
	int addrlen;
	int new_socket;

	socket_desc=socket(AF_INET,SOCK_STREAM,0);

/* set master socket to allow multiple connections */
	setsockopt(socket_desc,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));

/* type of socket created */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);
	bind(socket_desc,(struct sockaddr *)&address,sizeof(address));

/* try to specify maximum of 3 pending connections for the master socket */
	listen(socket_desc,3);

	addrlen=sizeof(address);
	while (1)
	{
		new_socket=accept(socket_desc,(struct sockaddr *)&address,&addrlen);
		pthread_create(&ntid, NULL, th_response, (void *) new_socket);
	}
	/* shutdown master socket properly */
	close(socket_desc);
}
