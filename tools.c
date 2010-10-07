#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<fcntl.h>
#include<time.h>
#include<syslog.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>

void write_log(char *file_name, char *ip, char *ident, char *auth, char *request_type, char *request_file, int status, int bytes) 
{
	static FILE *file = NULL;
	if(file == NULL) 
	{
		file = fopen(file_name, "a+");
	} 

	if(ip == NULL) 
	{
		return;
	}
	time_t result;
	result = time(NULL);
	struct tm* brokentime = localtime(&result);
	char now[32]; 
	strftime(now, 32, "%d/%b/%Y:%T %z", brokentime);
	fprintf(file, "%s %s %s [%s] \"%s %s\" %d %d \n", ip, ident, auth, now, request_type, request_file, status, bytes);
	fflush(file);	
}
char * get_extension(char *path, char *extension)
{
	int len = strlen(path);
	for (len = strlen(path);len > 0; len--)
	{
		if(path[len] == '.')
			return path+len+1;
		else if (path[len] == '/')
			return NULL;	
	}
}
	
void write_syslog(char *msg) 
{
	if(strlen(msg) > 64) 
	{
		return;
	}
	time_t t = time(0);
	openlog("webserv: ", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_ERR, "%s", msg);
	closelog();
}

char * read_mime(char *extension, char *target)
{
	static FILE *f = NULL;
	if(f == NULL) 
	{
		f = fopen("mime.types", "r");
	}

	char content_type[256];
	char ending[7][32];
	char *line = NULL;
	size_t len = 0;
	ssize_t read;

	//Make sure the file-pointer is at the beginning
	rewind(f);

	while ((read = getline(&line, &len, f)) != -1) 
	{
		if(line[0] == '#') 
		{
			continue;
		}
		sscanf(line, "%s %s %s %s %s %s %s %s", &content_type, &ending[0], &ending[1], &ending[2], &ending[3], &ending[4], &ending[5], &ending[6]);
		if(line[0] != '#')
		{
			int i;
			for(i = 0; i < 8; i++) 
			{
				if (strcmp(ending[i], extension) == 0) 
				{
					target = content_type;
					free(line);
					return target;
				} 
			}
		}	
        }
	target = "application/octet-stream";
	free(line);
	return target;
}
void read_conf(int *port, char *docroot)
{
	
	FILE *f = fopen(".lab3-config", "r");
	char values[256];
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	while ((read = getline(&line, &len, f)) != -1) 
	{
		if(line[0] == '#') 
		{
			continue;
		}
		sscanf(line, "%s", &values);
		if(strcmp(values, "Docroot") == 0) 
		{
			sscanf(line, "%s %s", &values, docroot);
		}
		else if(strcmp(values, "Listen") == 0) 
		{
			sscanf(line, "%s %d", &values, port); 
		}
		else 
		{
			fprintf(stderr, "Unexpected configuration \n");
		}
        }
	fclose(f);
	free(line);
}
void get_opt(int argc, char **argv, int *port, char *log_file, int *daemon) 
{
	*daemon = 0;
	int c;
	while ((c = getopt (argc, argv, "p:dl:")) != -1) 
	switch (c)
	{
        	case 'p':
        		*port = atoi(optarg);
       			break;
       		case 'd':
	     	 	*daemon = 1;
             		break;
           	case 'l':
			strcpy(log_file, optarg);
             		break;
	}

}
//int main(int argc, char **argv) 
//{
/*	int port;
	int daemon;
	char log_file[256];
	get_opt(argc, argv, &port, log_file, &daemon);
	printf("%d \t %s \t %d \n", port, log_file, daemon); */
	//write_syslog("It's a syslog yo");
/*	char extension[32] = "php";
	char target[256];
	char *vad;
	vad = extension;
	vad = read_mime(extension, target);
	puts(vad);
	printf("%s %s \n", vad, extension); */
	//char docroot[256];
	//int port;

	//read_conf(&port, docroot);

	//printf("%s %d \n", docroot, port);
//	write_log("test.log", "192.0.0.2", "-", "-", "Testing testing", 404, 0);
//}
