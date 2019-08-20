#include<stdio.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<ctype.h>
#include<string.h>
#include<strings.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/wait.h>
#include<stdlib.h>
#include<stdint.h>

#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server:jdbhttpd/0.1.0\r\n"
#define STDIN  0
#define STDOUT 1
#define STDERR 2


void* accept_request(void *);//������׽��ּ�������һ��http�������ַ������������������
void bad_request(int);//���ظ��ͻ������Ǹ���������
void cat(int,FILE*);//��ȡ��������ĳ���ļ�д��socket�׽���
void cannot_execute(int);//��������cgi����ʱ���ֵĴ���
void error_die(const char *);//�Ѵ�����Ϣд��perror���˳�
void execute_cgi(int,const char *,const char *,const char *);//����cgi����Ĵ���Ҳ����Ҫ�ĺ���
int get_line(int,char*,int);//��ȡ�׽��ֵ�һ�лس����е������ת����ͳһ�Ļ��з�����
void headers(int,const char *);//��hhtp��Ӧ�ı���ͷ��д���׽���
void server_file(int,const char *filename);//����cat�ѷ������ļ����ظ������
void not_found(int);//��Ҫ�����Ҳ���������ļ�ʱ�����
int startup(u_short *);//��ʼ��hhtpd���񣬰����׽��֣��󶨶˿ڣ����м���
extern void unimplemented(int);//���ظ�����������յ���http�������õ�method ����֧��

void bad_request(int client)
{
	char buf[1024];

	//��Ӧ�ͻ��˴����HTTP����
	sprintf(buf,"HTTP/1.0 400 BAD REQUEST\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"Centent-Type:text/html\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"<P>You browser sent a bad request.");
	send(client,buf,sizeof(buf),0);
	sprintf(buf,"such as a POST without a Contnent-Length\r\n");
	send(client,buf,sizeof(buf),0);
}

void cannot_execute(int client)
{
	char buf[1024];

	//��Ӧ�ͻ���cgi�޷�ִ��
	sprintf(buf,"HTTP/1.0 500 Internal Server Error\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type:text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<P>Error prohibited CGI execution.\r\n");
	send(client,buf,strlen(buf),0);
}

void cat(int client,FILE*resource)
{
	char buf[1024];

	//��ȡ�ļ��е���������д��socket
	fgets(buf,sizeof(buf),resource);
	while(!feof(resource))
	{
		send(client,buf,strlen(buf),0);
		fgets(buf,sizeof(buf),resource);
	}
}

void headers(int client,const char *filename)
{
	char buf[1024];
	(void) filename;

	//������HTTPD header
	strcpy(buf,"HTTP/1.0 200 OK\r\n");//ͷ����������
	send(client,buf,strlen(buf),0);
	//��������Ϣ
	strcpy(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type:text/html\r\n");
	send(client,buf,strlen(buf),0);
	strcpy(buf,"\r\n");
	send(client,buf,strlen(buf),0);
}

void server_file(int client,const char* filename)
{
	FILE *resource=NULL;
	int numchars=1;
	char buf[1024];

	//��ȡ������header
	buf[0]='A';
	buf[1]='\0';
	while((numchars>0)&&strcmp("\n",buf))
		numchars=get_line(client,buf,sizeof(buf));

	//��server���ļ�
	resource=fopen(filename,"r");
	if(resource==NULL)
		not_found(client);
	else
	{
		//дHTTP header
		headers(client,filename);
		//�����ļ�
		cat(client,resource);
	}
		fclose(resource);

}

void not_found(int client)
{
	char buf[1024];

	//404ҳ��
	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(client,buf,strlen(buf),0);
	
	//��������Ϣ
	sprintf(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type:text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<BODY><P>The server could not fulfill\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"you request because the recource specified\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"is unavailable or nonexistent.\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</BODY><HTML>\r\n");
	send(client,buf,strlen(buf),0);
}

void unimplemented(int client)
{
	char buf[1024];
	//HTTP method����֧��
	sprintf(buf,"HTTP/1.0 501 Method Not Implemented\r\n");
	send(client,buf,strlen(buf),0);
	//��������Ϣ
	sprintf(buf,SERVER_STRING);
	send(client,buf,strlen(buf),0);
	sprintf(buf,"Content-Type:text/html\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<HTML><HEAD><TITLE>Method Not Implemented\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</TITLE></HEAD>\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"<BODY><P>HTTP request method not supported.\r\n");
	send(client,buf,strlen(buf),0);
	sprintf(buf,"</BODY></HTML>\r\n");
	send(client,buf,strlen(buf),0);
}


int get_line(int sock,char*buf,int size)//�������ӿͻ��˵õ�����ĵ�һ��
{
	int i=0;
	char c='\0';
	int n;

	//����ֹ����ͳһΪ\���з�����׼��buf����
	while((i<size-1)&&(c!='\n'))
	{
		//һ�ν�����һ���ֽ�
		n=recv(sock,&c,1,0);
		//DEBUG printf("%02X\n",c);
		if(n>0)
		{
			//�յ�\r�����������һ���ֽڣ���Ϊ���з�������\r\n
			if(c=='\r')
			{
				//ʹ��MSG_PEEK��־ʹ��һ�ζ�ȡ��Ȼ���Եõ���ζ�ȡ�����ݣ�����Ϊ���ܴ��ڲ�����
				n=recv(sock,&c,1,MSG_PEEK);
				//DEBUG printf("%02X\n",c)
				//������ǻ��з���������յ�
				if((n>0)&&(c=='\n'))
					recv(sock,&c,1,0);
				else
					c='\n';
			}
			//�浽������
			buf[i]=c;
			i++;
		}
		else
			c='\n';
	}
	buf[i]='\0';
	return i;
}

void execute_cgi(int client,const char *path,const char *method,const char *query_string)
{
	char buf[1024];
	int cgi_output[2];
	int cgi_input[2];
	pid_t pid;
	int status;
	int i;
	char c;
	int numchars=1;
	int content_length=-1;

	buf[0]='A';
	buf[1]='\0';
	if(strcasecmp(method,"GET")==0)//GET����
	//�����е�http header��ȡ������
	while((numchars>0)&&strcmp("\n",buf))
		numchars=get_line(client,buf,sizeof(buf));
	else//POST����
	{
		//��POST��http�������ҳ�content_length
		numchars=get_line(client,buf,sizeof(buf));
			//����'\0'���з���
		buf[15]='\0';
		//http�������ص�
		if(strcasecmp(buf,"Content-Length:")==0)
			content_length=atoi(&(buf[16]));
		numchars=get_line(client,buf,sizeof(buf));
	}
	//û���ҵ�content_length
	if(content_length==-1)
	{
		//��������
		bad_request(client);
		return;
	}
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	send(client,buf,strlen(buf),0);
	//�����ܵ�
	if(pipe(cgi_output)<0)
	{
		//������
		cannot_execute(client);
		return;
	}
	//�����ܵ�
	if(pipe(cgi_input)<0)
	{
		cannot_execute(client);
		return;
	}

	if((pid=fork())<0)
	{
		//������
		cannot_execute(client);
		return;
	}
	if(pid==0)
	{
		char meth_env[255];
		char query_env[255];
		char length_env[255];
		//��STDOUT�ض���cgi_output��д���
		dup2(cgi_output[1],1);
		//��STDIN�ض���cgi_iput�Ķ�ȡ��
		dup2(cgi_input[0],0);
		//�ر�cgi_input��д��˺�cgi_output��д���
		close(cgi_output[0]);
		close(cgi_input[1]);
		//����request_method�Ļ�������
		printf(meth_env,"REQUEST_METHOD=%S",method);
		putenv(meth_env);
		if(strcasecmp(method,"GET")==0)
		{
			//����query_string�Ļ�������
			printf(query_env,"QUERY_STRING=%S",query_string);
			putenv(query_env);
		}
		else //����POST����
		{
			printf(length_env,"CONTENT_LENGTH",content_length);
			putenv(length_env);
		}
		//��execl����cgi����
		execl(path,path,NULL);
		exit(0);
	}
	else
	{
		//�ر�cgi�Ķ�ȡ�˺�cgi_output��д���
		close(cgi_output[1]);
		close(cgi_input[0]);
		if(strcasecmp(method,"POST")==0)
			//����POST����������
		    for(i=0;i<content_length;i++)
		{
			recv(client,&c,1,0);
			//��POST����д��cgi_input,�����ض���STDIN
			write(cgi_input[1],&c,1);
		}
		while(read(cgi_output[0],&c,1)>0)
			send(client,&c,1,0);
		//�رչܵ�
		close(cgi_output[0]);
		close(cgi_input[1]);
		waitpid(pid,&status,0);
	}
}

void* accept_request(void *arg)
{
    int client=*(int*)arg;
	char buf[1024];
	int numchars;
	char method[255];
	char url[255];
	char path[512];
	size_t i,j;
	struct stat st;
	int cgi=0;
	char *query_string=NULL;
	//�������ӿͻ��˵õ�����ĵ�һ��
	numchars=get_line(client,buf,sizeof(buf));
	i=0,j=0;
	//�ѿͻ��˵����󷽷��浽methond������
	while(!ISspace(buf[j])&&(i<sizeof(method)-1))
	{
		method[i]=buf[j];
		i++,j++;
	}
	method[i]='\0';

	//����Ȳ���GET�ֲ���POST���޷�����
	if(strcasecmp(method,"GET")&&strcasecmp(method,"POST"))//strcasecmp �Ǻ����˴�Сд���еıȽ�
	{
		unimplemented(client);
		return 0;
	}

	//POST��ʱ����cgi
	if(strcasecmp(method,"POST")==0)
		cgi=1;
	//��ȡurl��ַ
	i=0;
	while(ISspace(buf[j])&&(j<sizeof(buf)))
		j++;
	while(!ISspace(buf[j])&&(i<sizeof(url-1))&&(j<sizeof(buf)))
	{
		//����url
		url[i]=buf[j];
		i++;
		j++;
	}
	url[i]='\0';

	//����GET����
	if(strcasecmp(method,"GET")==0)
	{
		//����������Ϊurl
		query_string=url;
		while((*query_string!='?')&&(*query_string!='\0'))
			query_string++;
		//GET�����ص㣬������Ϊ����
		if(*query_string=='?')
		{
			//����cgi
			cgi=1;
			*query_string='\0';
			query_string++;
		}
	}
	//��ʽ��url��path���飬html�ļ�htdocs��
	sprintf(path,"htdocs%s",url);
	//Ĭ�����Ϊindex.html
	if(path[strlen(path)-1]=='/')
		strcat(path,"index.html");
	//����·���ҵ���Ӧ�ļ�
	if(stat(path,&st)==-1)
	{
		//������headers����Ϣ������
		while((numchars>0)&&strcmp("\n",buf))
			numchars=get_line(client,buf,sizeof(buf));
		//��Ӧ�ͻ����Ҳ���
		not_found(client);
	}
	else
	{
		//����Ǹ�Ŀ¼����Ĭ��ʹ�ø�Ŀ¼�� index.html�ļ�
		if((st.st_mode&S_IFMT)==S_IFDIR)
			strcat(path,"/index.html");
		if((st.st_mode&S_IXUSR)||(st.st_mode&S_IXGRP)||(st.st_mode&S_IXOTH))
			cgi=1;
		//����cgi��ֱ�Ӱѷ������ļ����أ�����ִ��cgi
		if(!cgi)
			server_file(client,path);
		else
			execute_cgi(client,path,method,query_string);
	}
	close(client);
	pthread_exit(0);
}

void error_die(const char *sc)
{
	//������Ϣ����
	perror(sc);
	exit(1);
}

int startup(u_short *port)
{
	int httpd=0;
	struct sockaddr_in name;

	//����socket
	httpd=socket(PF_INET,SOCK_STREAM,0);
	if(httpd==-1)
		error_die("socket.");//����ʧ��

	memset(&name,0,sizeof(name));
	name.sin_family=AF_INET;
	name.sin_port=htons(*port);
	name.sin_addr.s_addr=inet_addr("192.168.56.101");

	int on=1;
	setsockopt(httpd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

	//�󶨶˿�
	if(bind(httpd,(struct sockaddr*)&name,sizeof(name))<0)
		error_die("bind.");//��ʧ��
	//�����ǰ�ƶ��˿���0����̬�������һ���˿�
	if(*port==0)
	{
		socklen_t namelen=sizeof(name);
		if(getsockname(httpd,(struct sockaddr *)&name,&namelen)==-1)
			error_die("getsockname.");//��ȡ�˿�ʧ��
		*port=ntohs(name.sin_port);
	}

	//��ʼ����
	if(listen(httpd,5)<0)
		error_die("listen.");//����ʧ��
	return httpd;//�����׽���id
}

int main(void)
{
	int server_sock=-1;
	u_short port=8080;
	int client_sock=-1;
	struct sockaddr_in client_name;
	socklen_t client_name_len=sizeof(client_name);
	pthread_t newthread;

	//�ڶ�Ӧ�˿ڽ���httpd����
	server_sock=startup(&port);
	printf("httpd running on port %d\n",port);

	while(1)
	{
		//�׽����յ��ͻ�����������
		client_sock=accept(server_sock,(struct sockaddr*)&client_name,&client_name_len);
		if(client_sock==-1)//�������ʧ��
			error_die("accept.");
		if(pthread_create(&newthread,NULL,accept_request,&client_sock)!=0)//����ɹ�  �����߳�
			perror("pthread_create.");
	}
	close(server_sock);

	return 0;
}
