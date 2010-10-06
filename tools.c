#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<time.h>

void write_log(char *file_name, char *ip, char *ident, char *auth, char *request, int status, int bytes) 
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
	char *now = asctime(brokentime);
	now[24] = '\0';
	fprintf(file, "%s %s %s [%s] %s %d %d \n", ip, ident, auth, now, request, status, bytes);
	fflush(file);	
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

//int main() 
//{
//	char extension[25] = "pdf";
//	char target[256];
//	char *vad;
//	vad = read_mime(extension, target);
//	printf("%s \t %s \n", vad, extension);
	//char docroot[256];
	//int port;

	//read_conf(&port, docroot);

	//printf("%s %d \n", docroot, port);
	//write_log("test.log", "192.0.0.2", "-", "-", "Testing testing", 404, 0);
//}

