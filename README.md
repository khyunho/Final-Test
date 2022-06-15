# tcpip 기말고사 제출

쓰레드 기반 TCP/IP 네트워크 프로그래밍 소스 분석을 진행했습니다.


## chat_serv.c
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUF_SIZE 100
#define MAX_CLNT 256

void * handle_clnt(void * arg);
void send_msg(char * msg, int len);
void error_handling(char * msg);

int clnt_cnt=0; // 클라이언트 수 구분
int clnt_socks[MAX_CLNT]; // 위에 정의한 256, 최대 256명이 사용 가능
pthread_mutex_t mutx; // mutex 선언, 다중 스레드의 데이터 혼선 방지

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	pthread_t t_id; // thread 선언

	if(argc!=2) {   // 파일명, port번호 출력
		printf("Usage : %s <port>\n", argv[0]);
		exit(1);
	}
	
	pthread_mutex_init(&mutx, NULL);
	serv_sock=socket(PF_INET, SOCK_STREAM, 0); // mutex 생성

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1])); //IPv4, IP, Port 할당

	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bing() error");
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error"); // 주소 할당
						  // 최대 256명 접속 가능하다
						  // 5라고 지정한 것은 큐의 크기일 뿐이다.

	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr,&clnt_adr_sz);
		// 소켓 배정 clnt_socks[0]부터 시작
		
		pthread_mutex_lock(&mutx); // clnt_socks[], clnt_cnt 전역 변수 사용을 위해
					   // mutex 잠금
		clnt_socks[clnt_cnt++]=clnt_sock; // 소켓 배정 clnt_socks[0]부터 시작
		pthread_mutex_unlock(&mutx); //mutex 잠금해제

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);
		// thread 생성
		// main은 handle_clnt이며 파라미터를 받을 수 있도록 했다.

		pthread_detach(t_id);
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
		// t_id로 접근했을 때, 해당 thread가 NULL 값을 리턴하지 않으면 무시하고 진행
		// thread가 NULL 값을 리턴하면 thread 종료
	}
	close(serv_sock);
	return 0;
}

void * handle_clnt(void * arg)
{
	int clnt_sock=*((int*)arg); // 소켓 파일 디스크립터가 void 포인터로 들어왔으므로
				    // int 형변환
	int str_len=0, i;
	char msg[BUF_SIZE];


	while((str_len=read(clnt_sock, msg, sizeof(msg)))!=0)
		send_msg(msg, str_len); // 사용자 모두에게 메세지 보내기
	// 클라이언트에서 보낸 메세지를 받는다.
	// 클라이언트에서 EOF(클라이언트에서 소켓을 close)를 보내서
	// str_len 값이 0이 될떄까지 반복
	// 클라이언트가 접속하는 동안 while문을 계속 반복

	pthread_mutex_lock(&mutx); // mutex 잠금 
				   // clnt_cnt와 clnt_socks[]를 건드릴 것이기 때문
	for(i=0; i<clnt_cnt; i++) // remove disconnected client
				  // 연결 끊어진 클라이언트, 현재 thread에서 담당하는 소켓 삭제
	{
		if(clnt_sock==clnt_socks[i]) // 현재 담당하는 클라이언트 소켓의
					     // 파일 디스크립터 위치를 찾으면
					     // 현재 소켓이 있었던 위치를 기준으로
					     // 뒤의 클라이언트들을 땡겨온다
		{
			while(i++<clnt_cnt-1) // thread를 삭제할 것이므로 -1해준다.
				clnt_socks[i]=clnt_socks[i+1];
			break;
		}
	}	
	clnt_cnt--; // 클라이언트 수 줄이기
	pthread_mutex_unlock(&mutx); // mutex 잠금해제
	close(clnt_sock); // 서버의 thread 담당 클라이언트 소켓 종료
	return NULL;
}
void send_msg(char * msg, int len) // send to all 
				   // 접속한 모두에게 메세지 전송
{
	int i;
	pthread_mutex_lock(&mutx); // clnt_cnt, clnt_socks[] 사용을 위해 mutex 잠금
	for(i=0; i<clnt_cnt; i++)
		write(clnt_socks[i], msg, len); // 모두에게 메세지 전송
	pthread_mutex_unlock(&mutx); // mutex 잠금해제
}
void error_handling(char * msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}
```

## chat_clint.c
```c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define BUF_SIZE 100
#define NAME_SIZE 20

void * send_msg(void * arg);
void * recv_msg(void * arg);
void error_handling(char * msg);

char name[NAME_SIZE]="[DEFAULT]"; //채팅창에 보이는 이름 위에 정해진 대로 20자 제한
char msg[BUF_SIZE]; // 메세지 크기 제한 100자

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread; // 송신,수신용2가지 thread 선언
	void * thread_return; 

	// ip, port 뿐만 아니라 사용자 이름까지 넣어줘야한다.
	if(argc!=4) {
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}

	sprintf(name, "[%s]", argv[3]);
	sock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
		error_handling("connect() error");

	pthread_create(&snd_thread, NULL, send_msg, (void*)&sock);
	pthread_create(&rcv_thread, NULL, recv_msg, (void*)&sock);
	//2개의 thread 생성, send_msg(송신), recv_meg(수신)

	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);
	// thread 종료 대기 및 소멸 유도

	close(sock); //클라이언트 연결 종료
	return 0;
}

void * send_msg(void * arg) // send thread main
			    // send thread 의 메인
{
	int sock=*((int*)arg); // int형으로 전환
	char name_msg[NAME_SIZE+BUF_SIZE]; // 사용자의 아이디와 메세지를 붙여서 한번에 보낸다
	while(1)
	{
		fgets(msg, BUF_SIZE, stdin); // 입력 받기
		if(!strcmp(msg, "q\n")||!strcmp(msg, "Q\n")) // "Q" 입력하여 종료
		{
			close(sock); // 서버에 EOF 를 보낸다
			exit(0);
		}
		sprintf(name_msg, "%s %s", name, msg); //입력한 아이디와 내용이 
						       //name_msg로 들어가서 출력된다.
		write(sock, name_msg, strlen(name_msg)); //서버로 메세지 전송
	}
	return NULL;
}

void * recv_msg(void * arg) // recv thread main
			    // recv thread 메인
{
	int sock=*((int*)arg);
	char name_msg[NAME_SIZE+BUF_SIZE];
	int str_len;
	while(1)
	{
		str_len=read(sock, name_msg, NAME_SIZE+BUF_SIZE-1); // 서버에서 들어온 
								    // 메세지 수신
		if(str_len==-1) 
			return (void*)-1;
					// str_len 값이 -1이라면, 서버 소켓과 연결이 끊어진 상태
					// (void*)-1을 리턴시켜 pthread_join을 실행시킨다(소멸)
		name_msg[str_len]=0; // 버퍼 맨마지막 값 NULL
		fputs(name_msg, stdout); // 받은 메세지 출력
	}
	return NULL;
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}


```
