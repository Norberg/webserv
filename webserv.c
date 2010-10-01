#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BUFF_SIZE 4096
#define DOCROOT "/var/www"

void * th_response(void *arg)
{
	int new_socket = (int)arg;
	char *message="HTTP/1.0 501 Not Implemented \r\n";
	char *temp;
	char *uri;
	char *res;
	char buffer[BUFF_SIZE];
	int fd = 0;
	int size;

	recv(new_socket,buffer,BUFF_SIZE,0);
	printf("\nMessage recived: %s\n", buffer);
	if (strncmp(buffer, "GET",3) == 0)
	{
		temp = buffer; // TODO do we really need this
		strsep(&temp, " \r\n"); // GET
		uri = strsep(&temp, " \r\n");
		res = realpath(uri, NULL);
		printf("uri: %s REALPATH: %s\n",uri, res);
		if(res == NULL)
		{
			strcpy(buffer,"HTTP/1.0 404 Not Found \r\n");
			printf("Sending: %s\n", buffer);
			send(new_socket,buffer, strlen(buffer),0);
			free(res);
			close(new_socket);
			return ((void *)0);
		}
	
		fd = open(res, O_RDONLY);
		size = read(fd, buffer, BUFF_SIZE);
		while (size > 0)
		{
			if(size == -1)
			{
				strcpy(buffer,"HTTP/1.0 501 Internal Server Error\r\n");
				send(new_socket,buffer,strlen(buffer),0);
				break;
			}
			send(new_socket,buffer,size,0);
			size = read(fd, buffer, BUFF_SIZE);
		}
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
	int master_socket;
	struct sockaddr_in address;
	int addrlen;
	int new_socket;
	
	/* tries to chroot the process and then leave privileged mode */
	chdir(DOCROOT);
	if (chroot(DOCROOT) == -1)
	{
		puts("Server started without chroot permissions\n");
		puts("exiting\n");
		return 0;
	}
	setuid(1000);

	master_socket=socket(AF_INET,SOCK_STREAM,0);

	/* set master socket to allow multiple connections */
	setsockopt(master_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));

	/* type of socket created */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(8080);
	bind(master_socket,(struct sockaddr *)&address,sizeof(address));

	/* maximum pending connections for the master socket */
	listen(master_socket,5);

	addrlen=sizeof(address);
	while (1)
	{
		new_socket=accept(master_socket,(struct sockaddr *)&address,&addrlen);
		pthread_create(&ntid, NULL, th_response, (void *) new_socket);
	}
	/* shutdown master socket properly */
	close(master_socket);
}
