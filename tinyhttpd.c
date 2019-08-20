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


void* accept_request(void *);//处理从套接字监听到的一个http请求，体现服务器处理请求的流程
void bad_request(int);//返回给客户端这是个错误请求
void cat(int,FILE*);//读取服务器上某个文件写道socket套接字
void cannot_execute(int);//处理发生在cgi程序时出现的错误
void error_die(const char *);//把错误信息写到perror并退出
void execute_cgi(int,const char *,const char *,const char *);//运行cgi程序的处理，也是主要的函数
int get_line(int,char*,int);//读取套接字的一行回车换行等情况都转换成统一的换行符结束
void headers(int,const char *);//把hhtp响应的报文头部写到套接字
void server_file(int,const char *filename);//调用cat把服务器文件返回给浏览器
void not_found(int);//主要处理找不到请求的文件时的情况
int startup(u_short *);//初始化hhtpd服务，包括套接字，绑定端口，进行监听
extern void unimplemented(int);//返回给浏览器表明收到的http请求所用的method 不被支持

void bad_request(int client)
{
	char buf[1024];

	//回应客户端错误的HTTP请求
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

	//回应客户端cgi无法执行
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

	//读取文件中的所有数据写道socket
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

	//正常的HTTPD header
	strcpy(buf,"HTTP/1.0 200 OK\r\n");//头部的请求行
	send(client,buf,strlen(buf),0);
	//服务器信息
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

	//读取并丢弃header
	buf[0]='A';
	buf[1]='\0';
	while((numchars>0)&&strcmp("\n",buf))
		numchars=get_line(client,buf,sizeof(buf));

	//打开server的文件
	resource=fopen(filename,"r");
	if(resource==NULL)
		not_found(client);
	else
	{
		//写HTTP header
		headers(client,filename);
		//复制文件
		cat(client,resource);
	}
		fclose(resource);

}

void not_found(int client)
{
	char buf[1024];

	//404页面
	sprintf(buf,"HTTP/1.0 404 NOT FOUND\r\n");
	send(client,buf,strlen(buf),0);
	
	//服务器信息
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
	//HTTP method不被支持
	sprintf(buf,"HTTP/1.0 501 Method Not Implemented\r\n");
	send(client,buf,strlen(buf),0);
	//服务器信息
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


int get_line(int sock,char*buf,int size)//服务器从客户端得到请求的第一行
{
	int i=0;
	char c='\0';
	int n;

	//把终止条件统一为\换行符，标准化buf数组
	while((i<size-1)&&(c!='\n'))
	{
		//一次仅接受一个字节
		n=recv(sock,&c,1,0);
		//DEBUG printf("%02X\n",c);
		if(n>0)
		{
			//收到\r则继续接受下一个字节，因为换行符可能是\r\n
			if(c=='\r')
			{
				//使用MSG_PEEK标志使下一次读取依然可以得到这次读取的内容，可认为接受窗口不滑动
				n=recv(sock,&c,1,MSG_PEEK);
				//DEBUG printf("%02X\n",c)
				//但如果是换行符则把它吸收掉
				if((n>0)&&(c=='\n'))
					recv(sock,&c,1,0);
				else
					c='\n';
			}
			//存到缓冲区
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
	if(strcasecmp(method,"GET")==0)//GET方法
	//把所有的http header读取并丢弃
	while((numchars>0)&&strcmp("\n",buf))
		numchars=get_line(client,buf,sizeof(buf));
	else//POST方法
	{
		//对POST的http请求中找出content_length
		numchars=get_line(client,buf,sizeof(buf));
			//利用'\0'进行分析
		buf[15]='\0';
		//http的请求特点
		if(strcasecmp(buf,"Content-Length:")==0)
			content_length=atoi(&(buf[16]));
		numchars=get_line(client,buf,sizeof(buf));
	}
	//没有找到content_length
	if(content_length==-1)
	{
		//错误请求
		bad_request(client);
		return;
	}
	sprintf(buf,"HTTP/1.0 200 OK\r\n");
	send(client,buf,strlen(buf),0);
	//建立管道
	if(pipe(cgi_output)<0)
	{
		//错误处理
		cannot_execute(client);
		return;
	}
	//建立管道
	if(pipe(cgi_input)<0)
	{
		cannot_execute(client);
		return;
	}

	if((pid=fork())<0)
	{
		//错误处理
		cannot_execute(client);
		return;
	}
	if(pid==0)
	{
		char meth_env[255];
		char query_env[255];
		char length_env[255];
		//把STDOUT重定向到cgi_output的写入端
		dup2(cgi_output[1],1);
		//把STDIN重定向到cgi_iput的读取端
		dup2(cgi_input[0],0);
		//关闭cgi_input的写入端和cgi_output的写入端
		close(cgi_output[0]);
		close(cgi_input[1]);
		//设置request_method的环境变量
		printf(meth_env,"REQUEST_METHOD=%S",method);
		putenv(meth_env);
		if(strcasecmp(method,"GET")==0)
		{
			//设置query_string的环境变量
			printf(query_env,"QUERY_STRING=%S",query_string);
			putenv(query_env);
		}
		else //设置POST方法
		{
			printf(length_env,"CONTENT_LENGTH",content_length);
			putenv(length_env);
		}
		//用execl运行cgi程序
		execl(path,path,NULL);
		exit(0);
	}
	else
	{
		//关闭cgi的读取端和cgi_output的写入端
		close(cgi_output[1]);
		close(cgi_input[0]);
		if(strcasecmp(method,"POST")==0)
			//接收POST过来的数据
		    for(i=0;i<content_length;i++)
		{
			recv(client,&c,1,0);
			//把POST数据写入cgi_input,现在重定向到STDIN
			write(cgi_input[1],&c,1);
		}
		while(read(cgi_output[0],&c,1)>0)
			send(client,&c,1,0);
		//关闭管道
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
	//服务器从客户端得到请求的第一行
	numchars=get_line(client,buf,sizeof(buf));
	i=0,j=0;
	//把客户端的请求方法存到methond数组中
	while(!ISspace(buf[j])&&(i<sizeof(method)-1))
	{
		method[i]=buf[j];
		i++,j++;
	}
	method[i]='\0';

	//如果既不是GET又不是POST则无法处理
	if(strcasecmp(method,"GET")&&strcasecmp(method,"POST"))//strcasecmp 是忽略了大小写进行的比较
	{
		unimplemented(client);
		return 0;
	}

	//POST的时候开启cgi
	if(strcasecmp(method,"POST")==0)
		cgi=1;
	//读取url地址
	i=0;
	while(ISspace(buf[j])&&(j<sizeof(buf)))
		j++;
	while(!ISspace(buf[j])&&(i<sizeof(url-1))&&(j<sizeof(buf)))
	{
		//存下url
		url[i]=buf[j];
		i++;
		j++;
	}
	url[i]='\0';

	//处理GET方法
	if(strcasecmp(method,"GET")==0)
	{
		//待处理请求为url
		query_string=url;
		while((*query_string!='?')&&(*query_string!='\0'))
			query_string++;
		//GET方法特点，？后面为参数
		if(*query_string=='?')
		{
			//开启cgi
			cgi=1;
			*query_string='\0';
			query_string++;
		}
	}
	//格式化url到path数组，html文件htdocs中
	sprintf(path,"htdocs%s",url);
	//默认情况为index.html
	if(path[strlen(path)-1]=='/')
		strcat(path,"index.html");
	//根据路径找到对应文件
	if(stat(path,&st)==-1)
	{
		//把所有headers的信息都丢弃
		while((numchars>0)&&strcmp("\n",buf))
			numchars=get_line(client,buf,sizeof(buf));
		//回应客户端找不到
		not_found(client);
	}
	else
	{
		//如果是个目录，则默认使用该目录下 index.html文件
		if((st.st_mode&S_IFMT)==S_IFDIR)
			strcat(path,"/index.html");
		if((st.st_mode&S_IXUSR)||(st.st_mode&S_IXGRP)||(st.st_mode&S_IXOTH))
			cgi=1;
		//不是cgi，直接把服务器文件返回，否则执行cgi
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
	//出错信息处理
	perror(sc);
	exit(1);
}

int startup(u_short *port)
{
	int httpd=0;
	struct sockaddr_in name;

	//建立socket
	httpd=socket(PF_INET,SOCK_STREAM,0);
	if(httpd==-1)
		error_die("socket.");//创建失败

	memset(&name,0,sizeof(name));
	name.sin_family=AF_INET;
	name.sin_port=htons(*port);
	name.sin_addr.s_addr=inet_addr("192.168.56.101");

	int on=1;
	setsockopt(httpd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));

	//绑定端口
	if(bind(httpd,(struct sockaddr*)&name,sizeof(name))<0)
		error_die("bind.");//绑定失败
	//如果当前制定端口是0，则动态随机分配一个端口
	if(*port==0)
	{
		socklen_t namelen=sizeof(name);
		if(getsockname(httpd,(struct sockaddr *)&name,&namelen)==-1)
			error_die("getsockname.");//获取端口失败
		*port=ntohs(name.sin_port);
	}

	//开始监听
	if(listen(httpd,5)<0)
		error_die("listen.");//监听失败
	return httpd;//返回套接字id
}

int main(void)
{
	int server_sock=-1;
	u_short port=8080;
	int client_sock=-1;
	struct sockaddr_in client_name;
	socklen_t client_name_len=sizeof(client_name);
	pthread_t newthread;

	//在对应端口建立httpd服务
	server_sock=startup(&port);
	printf("httpd running on port %d\n",port);

	while(1)
	{
		//套接字收到客户端连接请求
		client_sock=accept(server_sock,(struct sockaddr*)&client_name,&client_name_len);
		if(client_sock==-1)//如果创建失败
			error_die("accept.");
		if(pthread_create(&newthread,NULL,accept_request,&client_sock)!=0)//如果成功  创建线程
			perror("pthread_create.");
	}
	close(server_sock);

	return 0;
}
