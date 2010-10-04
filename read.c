#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<time.h>

void write_log(char *file_name, char *ip, char *ident, char *auth, char *request, int status, int bytes) 
{
	FILE *f = fopen(file_name, "a+");
	time_t result;
	result = time(NULL);
	struct tm* brokentime = localtime(&result);
	char *now = asctime(brokentime);
	now[24] = '\0';
	fprintf(f, "%s %s %s [%s] %s %d %d \n", ip, ident, auth, now, request, status, bytes);
	
	fclose(f);
}
char * read_mime(char *extension)
{
	FILE *f = fopen("mime.types", "r");
	
	fclose(f);	
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
int main() 
{
	char docroot[256];
	int port;

	read_conf(&port, docroot);

	//printf("%s %d \n", docroot, port);
	write_log("test.log", "192.0.0.2", "-", "-", "Testing testing", 404, 0);
}
