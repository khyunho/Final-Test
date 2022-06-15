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
