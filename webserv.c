#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define buffsize 512

void * th_response(void *arg)
{
	int new_socket = (int)arg;
	char *message="HTTP/1.0 501 Not Implemented";
	char docroot[512]="/var/www/";
	char *temp;
	char *uri;
	char buffer[buffsize];
	char buf[PATH_MAX];
	recv(new_socket,buffer,buffsize,0);
	printf("Message recived: %s\n", buffer);
	if (strncmp(buffer, "GET",3) == 0)
	{
		printf("Get request\n");
		message = "<html><b>Works</b> </html>";
		temp = buffer; // TODO do we really need this
		strsep(&temp, " "); // GET
		uri = strsep(&temp, " ");
		strcat(docroot, uri);
		char *res = realpath(docroot, buf);
		printf("REALPATH: %s\n", buf);
		int fd = open("Makefile", "r");
		char finput[1024];
		printf("File %d open %d \n",&fd, read(fd, finput, 1024));
		printf("Message: %s\n", finput);
		send(new_socket,message,strlen(message),0);
	}
	else
	{
		send(new_socket,message,strlen(message),0);
	}	
	printf("Message sent successfully by: %x", (unsigned int)pthread_self());
	close(new_socket);
	sleep(10);
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
