#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <process.h>
#include "chordFileDown.h"

#define BUF_SIZE 4096

// 사용자 정의 데이터 TCP 수신 함수
int recvn(SOCKET s, char *buf, int len, int flags)
{
	int received;
	char *ptr = buf;
	int left = len;

	while (left > 0) {
		received = recv(s, ptr, left, flags);
		if (received == SOCKET_ERROR)
			return SOCKET_ERROR;
		else if (received == 0)
			break;
		left -= received;
		ptr += received;
	}
	return (len - left);
}
void err_quit(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, (LPCWSTR)msg, MB_ICONERROR);
	LocalFree(lpMsgBuf);
	exit(-1);
}
void err_display(char *msg)
{
	LPVOID lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("[%s] %ls", msg, (LPCTSTR)lpMsgBuf);
	LocalFree(lpMsgBuf);
}

int fileSender(char filename[], struct sockaddr_in Addr, struct sockaddr_in curAddr)
{
	int retval;
	int addrLen = sizeof(struct sockaddr_in);
	SOCKET fsSock,flSock;
	if ((fsSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		//PRINT_ERROR("frSock failed…");
		exit(-1);
	}
	if ((flSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		//PRINT_ERROR("frSock failed…");
		exit(-1);
	}
	static const int optVal = 5000; // 5 seconds
	if (setsockopt(fsSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		//PRINT_ERROR("frSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	if (setsockopt(flSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		//PRINT_ERROR("frSock에 5초 timeout 설정 실패");
		exit(-1);
	}

	retval = bind(flSock, (SOCKADDR *)&curAddr,sizeof(curAddr));
	if (retval == SOCKET_ERROR) err_quit("bind()");
	// listen()
	retval = listen(flSock, SOMAXCONN);
	if (retval == SOCKET_ERROR) err_quit("listen()");

	// 파일 열기
	FILE *fp = fopen(filename, "rb");
	if (fp == NULL) {
		perror("파일 입출력 오류");
		return -1;
	}
	if (retval == SOCKET_ERROR) err_quit("send()");
	
	fseek(fp, 0, SEEK_END);      // go to the end of file
	int totalbytes = ftell(fp);  // get the current position
	rewind(fp); // 파일 포인터를 제일 앞으로 이동

	char buf[BUF_SIZE];
	unsigned int numread;
	int numtotal = 0;
	// 파일 데이터 보내기
	fsSock = accept(flSock, (SOCKADDR *)&Addr, &addrLen);
	retval = send(fsSock, filename, 256, 0);
	if (retval == SOCKET_ERROR)
		return -1;
	retval = send(fsSock, (char *)&totalbytes, sizeof(totalbytes), 0);
	if (retval == SOCKET_ERROR)
		return -1;
	while (1) {
		numread = (unsigned int)fread(buf, 1, BUF_SIZE, fp);
		if (numread > 0) {
			retval = send(fsSock, buf, numread, 0);
			if (retval == SOCKET_ERROR) {
				err_display("send()");
				break;
			}
			numtotal += numread;
		}
		else if (numread == 0 && numtotal == totalbytes) {
			printf("파일 전송 완료!: %d 바이트\n", numtotal);
			break;
		}
		else {
			perror("파일 입출력 오류");
			break;
		}
	}
	fclose(fp);
	closesocket(fsSock);
	closesocket(flSock);
	//file thread로 이동
	return 0;
}
int fileReceiver(struct sockaddr_in Addr)
{
	int retval;
	SOCKET frSock;
	if ((frSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		//PRINT_ERROR("frSock failed…");
		exit(-1);
	}
	static const int optVal = 5000; // 5 seconds
	if (setsockopt(frSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		//PRINT_ERROR("frSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	// bind()
	retval = connect(frSock, (SOCKADDR *)&Addr,sizeof(Addr));
	if (retval == SOCKET_ERROR) err_quit("connect()");

	// 데이터 통신에 사용할 변수
	char buf[BUF_SIZE];

	while (1) {
		printf("\nFileSender 접속: IP 주소=%s, 포트 번호=%d\n",inet_ntoa(Addr.sin_addr),ntohs(Addr.sin_port));
		// 파일 이름 받기
		char filename[256];
		ZeroMemory(filename, 256);
		retval = recvn(frSock, filename, 256, 0);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		printf("-> 받을 파일 이름: %s\n", filename);

		// 파일 크기 받기
		int totalbytes;
		retval = recvn(frSock, (char *)&totalbytes,sizeof(totalbytes), 0);
		if (retval == SOCKET_ERROR) {
			err_display("recv()");
			break;
		}
		printf("-> 받을 파일 크기: %d\n", totalbytes);
		// 파일 열기
		FILE *fp = fopen(filename, "wb");
		if (fp == NULL) {
			perror("파일 입출력 오류");
			break;
		}
		// 파일 데이터 받기
		int numtotal = 0;
		while (1) {
			retval = recvn(frSock, buf, BUF_SIZE, 0);
			if (retval == SOCKET_ERROR) {
				err_display("recv()");
				break;
			}
			else if (retval == 0)

				break;
			else {
				fwrite(buf, 1, retval, fp);
				if (ferror(fp)) {
					perror("파일 입출력 오류");
					break;
				}
				numtotal += retval;
			}
		}
		fclose(fp);
		// 전송 결과 출력
		if (numtotal == totalbytes)
			printf("-> 파일 전송 완료!\n");
		else
			printf("-> 파일 전송 실패!\n");

		printf("FileSender 종료: IP 주소=%s, 포트 번호=%d\n",inet_ntoa(Addr.sin_addr),ntohs(Addr.sin_port));
		break;
	}
	closesocket(frSock);
	//file thread로 이동
	return 0;
}