#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include "tools.c"

#define BUFF_SIZE 4096
#define NR_THREADS 5
#define TIMEOUT 5 

struct connection
{
	int socket;
	int fd;
        LIST_ENTRY(connection) entries;          /* Needed for LIST */
	
};

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

/* Returns 0 when finished and 1 otherwise*/
int send_file(int socket, int fd)
{
	char buffer[BUFF_SIZE];
	int size = read(fd, buffer, BUFF_SIZE);
	if (size == BUFF_SIZE)
	{
		size = send(socket,buffer,size,MSG_NOSIGNAL|MSG_DONTWAIT);	
		if (size == -1)
		{	
			/* Client closed connection */ 
			/* 402 - Payment Required - connection probably closed due to lack of ISP payment :P */
			write_log(NULL, socket, "-", "-", "GET", "Client closed connection", 402, 0);
			close(fd);
			close(socket);
			return 0; /*Connection lost, dont continue trying*/
		}
	}
	else if (size > 0)
	{
		/*Didnt read BUFF_SIZE bytes, the file have hit EOF*/
		send(socket,buffer,size,MSG_NOSIGNAL);
		write_log(NULL, socket, "-", "-", "GET ", "filename", 200, 512);
		//TODO fix filename and 512 to real values, hint: fstat
		close(fd);
		close(socket);
		return 0; 
	}
	else if (size == 0)
	{
		write_log(NULL, socket, "-", "-", "GET ", "filename", 200, 512);
		close(fd);
		close(socket);
		return 0;
	}
	else if(size == -1)
	{
		strcpy(buffer,"HTTP/1.0 501 Internal Server Error\r\n");
		send(socket,buffer,strlen(buffer),MSG_NOSIGNAL);
		close(fd);
		close(socket);
	}
	return 1;
}

int response(int new_socket, struct connection *conn)
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
        struct timeval tv;
	tv.tv_sec = TIMEOUT;
	tv.tv_usec = 0;
	fd_set readset;
	FD_ZERO(&readset);	
	FD_SET(new_socket, &readset);
	if(!select(FD_SETSIZE, &readset, NULL, NULL, &tv))
	{
		printf("client timeout\n");
		goto cleanup;
	}
	recv(new_socket,buffer,BUFF_SIZE,0);
	if (strncmp(buffer, "GET",3) == 0)
	{
		temp = buffer; // TODO do we really need this, yes C is this weird?
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
		conn->socket = new_socket;
		conn->fd = fd;
		free(res);
		return 0;
	}
	else if (strncmp(buffer, "HEAD",4) == 0)
	{
		temp = buffer; // TODO do we really need this, yes C is this weird?
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
	conn->fd = 0;	
	conn->socket = 0;	
	free(res);
	close(new_socket);
	close(fd); 
	return 0;
}


void * worker_thread(void *arg)
{
	int *pipe  = (int *)arg;
	int new_socket;
	struct connection *conn;
	struct connection conn2;
	fd_set readset, writeset;
	LIST_HEAD(listhead, connection) head;
	LIST_INIT(&head);
	while(1)
	{
		/* Let select watch all open connections and pipes for "access" */
		FD_ZERO(&readset);	
		FD_ZERO(&writeset);
		FD_SET(pipe[0], &readset);
		LIST_FOREACH(conn, &head, entries) {
			FD_SET(conn->socket, &writeset);
		}
		select(FD_SETSIZE, &readset, &writeset, NULL, NULL);
		if (FD_ISSET(pipe[0], &readset))
		{
			read(pipe[0],(char *)&new_socket,sizeof(int));	
			conn = malloc(sizeof(struct connection));
			response(new_socket, conn);
			if (conn->fd != 0)
				LIST_INSERT_HEAD(&head, conn, entries);

		}
		LIST_FOREACH(conn, &head, entries) {
			if (FD_ISSET(conn->socket, &writeset))
			{
				if (!send_file(conn->socket, conn->fd))
				{
					LIST_REMOVE(conn, entries);
					free(conn);
					break;
				}
			}
		}
		
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
		pipe2(pipes[i],0);// O_NONBLOCK);
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
