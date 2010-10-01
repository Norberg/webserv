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
#define NR_THREADS 5

int response(int new_socket)
{
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
		uri = strsep(&temp, " \r\n"); // GET
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
			return (0);
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
	return (0);
}

void * worker_thread(void *arg)
{
	int *pipe  = (int *)arg;
	int new_socket;
	while(1)
	{
		read(pipe[0],(char *)&new_socket,sizeof(int));	
		printf("response to socket: %d \n", new_socket);
		response(new_socket);
	}
	return ((void *)0);
}

int next_thread(int *last_thread)
{
	(*last_thread)++;
	if (*last_thread >= NR_THREADS)
		*last_thread = 0;
	return *last_thread;
}

int main()
{
	pthread_t thread[NR_THREADS];
	int pipes[NR_THREADS][2];
	int opt=1;
	int master_socket;
	struct sockaddr_in address;
	int addrlen;
	int new_socket;
	int last_thread = 0;
	int i;
	
	/* tries to chroot the process and then leave privileged mode */
	chdir(DOCROOT);
	if (chroot(DOCROOT) == -1)
	{
		puts("Server started without chroot permissions");
		puts("exiting");
		return 0;
	}
	setuid(1000);

	/* Spawn worker threads */
	for (i = 0; i < NR_THREADS; i++)
	{
		pipe(pipes[i]);
		pthread_create(&thread[i], NULL, worker_thread, pipes[i]);
	}	

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
		write(pipes[next_thread(&last_thread)][1],(char *)&new_socket, sizeof(int));
	}
	/* shutdown master socket properly */
	close(master_socket);
}
