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

송
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
				   // 접속한 모두에게 메세지 전
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
