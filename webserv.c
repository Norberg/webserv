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
#include "tools.c"

#define BUFF_SIZE 4096
#define NR_THREADS 5

void create_header(char *path, char *buffer)
{
	char temp[32];
	char *content_type;
	char *file_extension;
	file_extension = get_extension(path, temp);
	content_type = read_mime(file_extension, temp);
	strcpy(buffer,"HTTP/1.0 200 OK\r\nContent-Type: ");
	strcat(buffer, content_type);
	strcat(buffer, "\r\n\r\n");
}

int response(int new_socket)
{
	char *temp = NULL;
	char *uri = NULL;
	char *res = NULL;
	char *httpv = NULL;
	char buffer[BUFF_SIZE];
	char extension[32];
	int fd = 0;
	int size = 0;
	int bytes = 0;
	int err = 0;

	recv(new_socket,buffer,BUFF_SIZE,0);
	if (strncmp(buffer, "GET",3) == 0)
	{
		temp = buffer; // TODO do we really need this
		uri = strsep(&temp, " \r\n"); // GET
		uri = strsep(&temp, " \r\n");
		httpv = strsep(&temp, " \r\n"); // HTTP version
		uri = resolve_path(uri);
		if (uri == NULL)
		{
			strcpy(buffer,"HTTP/1.0 400 Bad Request \r\n");
			send(new_socket,buffer, strlen(buffer),0);
			write_log(NULL, new_socket, "-", "-", "GET", "Bad Request", 400, 0);
			goto cleanup;
			
		}		
		res = realpath(uri, NULL);
		printf("uri: %s REALPATH: %s\n",uri, res);
		if(res == NULL)
		{
			write_log(NULL, new_socket, "-", "-", "GET ", uri, 404, 0);
			strcpy(buffer,"HTTP/1.0 404 Not Found \r\n");
			send(new_socket,buffer, strlen(buffer),0);
			goto cleanup;
		}
		fd = open(res, O_RDONLY);
		if (fd == -1)
		{
			strcpy(buffer,"HTTP/1.0 403 Forbidden\r\n");
			send(new_socket,buffer,strlen(buffer),0);
			goto cleanup;
		}
		if (httpv[0] != '\0') //Full request
		{
			create_header(res,buffer);
			send(new_socket,buffer,strlen(buffer),0);
		}	
		size = read(fd, buffer, BUFF_SIZE);
		while (size > 0)
		{
			if(size == -1)
			{
				strcpy(buffer,"HTTP/1.0 501 Internal Server Error\r\n");
				send(new_socket,buffer,strlen(buffer),0);
				break;
			}
			bytes += size;
			err = send(new_socket,buffer,size,MSG_NOSIGNAL); /* We dont want any SIGPIPE */
			if (err == -1)
			{
				/* Client closed connection */ 
				/* 402 - Payment Required - connection probably closed due to lack of ISP payment :P */
				write_log(NULL, new_socket, "-", "-", "GET", "Client closed connection", 402, bytes);
				goto cleanup;
			}	
			size = read(fd, buffer, BUFF_SIZE);
		}
		write_log(NULL, new_socket, "-", "-", "GET ", res, 200, bytes);
	}
	else if (strncmp(buffer, "HEAD",4) == 0)
	{
		temp = buffer; // TODO do we really need this
		uri = strsep(&temp, " \r\n"); // GET
		uri = strsep(&temp, " \r\n");
		res = realpath(uri, NULL);
		printf("uri: %s REALPATH: %s\n",uri, res);
		if(res == NULL)
		{
			strcpy(buffer,"HTTP/1.0 404 Not Found \r\n");
			send(new_socket,buffer, strlen(buffer),0);
			goto cleanup;
		}
		create_header(res,buffer);
		send(new_socket,buffer,strlen(buffer),0);
	}
	else
	{
		strcpy(buffer,"HTTP/1.0 501 Not Implemented \r\n");
		send(new_socket,buffer,strlen(buffer),0);
	}
	cleanup:
	free(res);
	close(fd);
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

int main(int argc, char **argv)
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
	int port;
	int daemon;
	char docroot[256];
	char extension[32];
	char type[32];
	char logfile[32] = "webserv.log";
	
	read_conf(&port, docroot);
	get_opt(argc, argv, &port, logfile, &daemon); 
	write_log(logfile,0,NULL, NULL, NULL, NULL, 0, 0);
	read_mime(extension, type);
	write_syslog("Server starting up..");	

		
	/* tries to chroot the process and */
	chdir(docroot);
	if (chroot(docroot) == -1)
	{
		puts("Server started without chroot permissions");
		puts("exiting");
		return 0;
	}
	
	/* if -d specifed deamonize the server */
	if (daemon)
	{
		/*Fork and kill parent*/
		if (fork() != 0)
			exit(0);
		setsid();
		umask(0);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);		
	}


	/* create and set master socket to allow multiple connections */
	master_socket=socket(AF_INET,SOCK_STREAM,0);
	setsockopt(master_socket,SOL_SOCKET,SO_REUSEADDR,(char *)&opt,sizeof(opt));

	/* type of socket created */
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	bind(master_socket,(struct sockaddr *)&address,sizeof(address));
	
	/*leave privileged mode */
	setuid(1000);

	/* maximum pending connections for master socket */
	listen(master_socket,5);
	
	/* Spawn worker threads */
	for (i = 0; i < NR_THREADS; i++)
	{
		pipe(pipes[i]);
		pthread_create(&thread[i], NULL, worker_thread, pipes[i]);
	}
	write_syslog("Server succesfully started!");	
	addrlen=sizeof(address);
	while (1)
	{
		new_socket=accept(master_socket,(struct sockaddr *)&address,&addrlen);
		write(pipes[next_thread(&last_thread)][1],(char *)&new_socket, sizeof(int));
	}
	/* shutdown master socket properly */
	close(master_socket);
}
