#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define buffsize 512

void * th_response(void *arg)
{
	int new_socket = (int)arg;
	char *message="HTTP/1.0 501 Not Implemented";
	char buffer[buffsize];
	recv(new_socket,buffer,buffsize,0);
	printf("Message recived: %s\n", buffer);
	send(new_socket,message,strlen(message),0);
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
