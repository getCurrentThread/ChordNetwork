#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include <string.h>
#include <process.h>
#include "chord.h"
#include "chordMath.h"
#include "chordFileDown.h"
#include "queue.h"

#define BUFSIZE 2440

struct CMD builtin[] = { //내장 명령 정보
	{"create",	"chord 네트워크를 생성합니다",		cmd_create },
	{"join",	"chord 네트워크에 Join 합니다",		cmd_join },
	{"leave",	"chord 네트워크에 Leave 합니다",	cmd_leave },
	{"add",		"네트워크에 파일을 추가합니다",		cmd_add },
	{"delete",	"네트워크에서 파일을 삭제합니다",	cmd_delete },
	{"search",	"파일을 찾고 다운로드 합니다",		cmd_search },
	{"finger",	"핑거 테이블을 확인합니다",			cmd_finger },
	{"info",	"노드 정보를 확인합니다",			cmd_info },
	{"mute",	"디버그 모드를 토글합니다",			cmd_mute },
	{"help",	"명령어 일람을 확인합니다",			cmd_help },
	{"quit",	"프로그램을 종료합니다",			cmd_quit }
};

const int builtins = 11; //내장 명령의 수

//새로운 전역 변수 선언 시작

HANDLE hThread[2];
HANDLE fThread;

nodeType curNode = { 0 };	// node information -> global variable
SOCKET rqSock;//main 쓰레드에서 쓰는 소켓
SOCKET rpSock;//procRecvMsg 쓰레드에서 쓰는 소켓
SOCKET pfSock;

fileDownInfoType fileSendInfo, fileRecvInfo;

HANDLE hMutex;				// when multiplexing is used

int silentMode = TRUE;		// silent mode
int ThreadsExitFlag = FALSE;
int initFlag = FALSE;		// 초기화 작업 진행 유무
char buf[BUFSIZE];

static const char *name = ""; //메인 쓰레드의 이름

//Worker 관련 쓰레드, 큐와 락. (초기화와 해제는 procRecvMsg에서 할 것.)

#define WORKER_COUNT 4
HANDLE hWorker[WORKER_COUNT];
int workerID[WORKER_COUNT];

#define QUEUE_SIZE 50
Queue queue;

HANDLE qmutex;
HANDLE qfull;
HANDLE qempty;

SOCKET wSock[WORKER_COUNT];

//새로운 전역 변수 선언 종료


int main(int argc, char *argv[])
{
	WSADATA wsaData;

	//memset(&curNode.nodeInfo.addrInfo, 0, sizeof(struct sockaddr_in));
	curNode.nodeInfo.addrInfo.sin_family = AF_INET;

	if (argc == 1) {
		char ipAddrStr[20] = { 0, };
		unsigned short port = 0;
		PRINT_ERROR("chord를 실행할 기본 인자가 없습니다.");
		printf("기본 수신 IP 주소 : ");
		scanf_s("%s", ipAddrStr, (unsigned int)sizeof(ipAddrStr));
		if ((curNode.nodeInfo.addrInfo.sin_addr.s_addr = inet_addr(ipAddrStr)) < 0) {
			PRINT_ERROR("잘못된 IP 주소를 입력하셨습니다.");
			exit(-1);
		}
		printf("기본 수신 PORT : ");
		scanf_s("%hd", &port);
		while (!(49152 <= port && port <= 65535)) {
			FFLUSH();
			PRINT_ERROR("<Port No> should be in [49152, 65535]!");
			printf("기본 수신 PORT : ");
			scanf_s("%hd", &port);
		}
		curNode.nodeInfo.addrInfo.sin_port = htons(port);
		curNode.nodeInfo.ID = modPlus(ringSize, str_hash(ipAddrStr), port % ringSize);
	}
	else if (argc == 2) { // port 만 입력한 경우
		unsigned short port;
		PRINT_INFO("따로 주소를 지정해주지 않아서 루프백 주소로 지정합니다.");
		curNode.nodeInfo.addrInfo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		port = atoi(argv[1]);
		curNode.nodeInfo.addrInfo.sin_port = htons(port); //port만 가지고 옴
		curNode.nodeInfo.ID = modPlus(ringSize, str_hash("127.0.0.1"), port % ringSize);
	}
	else if (argc == 3) { //port 와 ip 둘다 입력한 경우
		unsigned short port = atoi(argv[2]);
		if (!(49152 <= port && port <= 65535)) {
			PRINT_ERROR("<Port No> should be in [49152, 65535]!");
			exit(-1);
		}
		curNode.nodeInfo.addrInfo.sin_addr.s_addr = inet_addr(argv[1]);
		curNode.nodeInfo.addrInfo.sin_port = htons(port);
		curNode.nodeInfo.ID = modPlus(ringSize, str_hash(argv[1]), port % ringSize);
	}
	else {
		PRINT_ERROR("Usage : %s <IP> <port>", argv[0]);
		exit(-1);
	}

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		PRINT_ERROR("WSAStartup() error!");
		exit(-1);
	}

	int isExit = 0;

	srand((unsigned int)time(NULL));
	hMutex = CreateMutex(NULL, FALSE, NULL);

	init();

	printf("*****************************************************************\n");
	printf("*      DHT-Based P2P 프로토콜 (CHORD) 노드 컨트롤러       \t*\n");
	printf("*                  Ver. alpha1      2018년 10월 5일       \t*\n");
	printf("*                       (c) Kim, Tae-Hyong                  \t*\n");
	printf("*             Edit. Hyeon Jin-hyeok, Lim Junhyeok           \t*\n");
	printf("*****************************************************************\n\n");

	PRINT_INFO("Your IP address: %s, Port No: %d, ID: %2d", inet_ntoa(curNode.nodeInfo.addrInfo.sin_addr), ntohs(curNode.nodeInfo.addrInfo.sin_port), curNode.nodeInfo.ID);

	cmd_help(1, NULL);

	while (!isExit)
		isExit = cmdProcessing();

	release();
	CloseHandle(hMutex);
	WSACleanup();
	PRINT_INFO("프로그램을 종료합니다.");
	return 0;
}

//새로운 명령어 함수 정의 시작

#define STR_LEN 1024// 최대 명령 입력 길이
#define MAX_TOKENS 128 // 공백으로 분리되는 명령어 문자열 최대 수

int cmdProcessing(void)
{
	char cmdLine[STR_LEN]; //입력 명령 전체를 저장하는 배열
	char *cmdTokens[MAX_TOKENS];//입력 명령을 공백으로 분리하여 저장하는 배열
	char delim[] = " \t\n\r";//토큰 구분자 - strtok에서 사용
	char *token;//토큰 (분리된 입력 문자열)
	int tokenNum; //입력 명령에 저장된 토큰 수

	int exitCode = 0;// 종료 코드 (default = 0)
	fputs("CHORD> ", stdout);// 프롬프트 출력
	if (fgets(cmdLine, STR_LEN, stdin) == NULL) { // 한 줄의 명령 입력
		fputs("\n", stdout);
		return exitCode;
	}

	token = strtok(cmdLine, delim); //입력 명령의 문자열 하나 분리
	tokenNum = 0;
	while (token) {
		cmdTokens[tokenNum++] = token;//분리된 문자열을 배열에 저장
		token = strtok(NULL, delim); //연속하여 입력 명령의 문자열 하나 분리
	}
	cmdTokens[tokenNum] = NULL;
	if (tokenNum == 0)
	{
		fputs("CHORD> Enter your command ('help' for help message).\n", stdout);
		return exitCode;
	}
	else if (tokenNum == 1 && strlen(cmdTokens[0]) == 1) {//한글자 명령인 경우 해당 함수 호출
		for (int i = 0; i < builtins; i++)
		{
			if (cmdTokens[0][0] == builtin[i].name[0])
				return builtin[i].cmd(tokenNum, cmdTokens);
		}
	}

	for (int i = 0; i < builtins; ++i)
	{//내장 명령인지 검사하여 명령이 있으면 해당 함수 호출
		if (strcmp(cmdTokens[0], builtin[i].name) == 0)
			return builtin[i].cmd(tokenNum, cmdTokens);
	}

	PRINT_ERROR("Wrong command! Input a correct command.");
	return exitCode;
}

int cmd_create(int argc, char *argv[])
{
	if (initFlag == TRUE) {
		PRINT_ERROR("이미 create 또는 join을 완료하였습니다.");
		return 0;
	}
	initFlag = TRUE;
	PRINT_INFO("You have created a chord network!");
	//fingerInfo 정보를 모두 자신의 노드 정보로 채움
	curNode.chordInfo.fingerInfo.Pre = curNode.nodeInfo;
	for (int i = 0; i < baseM; i++) {
		curNode.chordInfo.fingerInfo.finger[i] = curNode.nodeInfo;
	}
	PRINT_INFO("Your finger table has been update!");

	LOCK(hThread);

	ThreadsExitFlag = FALSE;

	hThread[0] = (HANDLE)_beginthreadex(NULL, 0, (void*)procRecvMsg, NULL, 0, NULL);
	if (hThread[0] < (HANDLE)0) return -1;
	hThread[1] = (HANDLE)_beginthreadex(NULL, 0, (void*)procPPandFF, NULL, 0, NULL);
	if (hThread[1] < (HANDLE)0) return -1;

	UNLOCK(hMutex);

	return 0;
}
int cmd_join(int argc, char *argv[])
{
	struct sockaddr_in helperAddr;
	struct sockaddr_in clntAddr;
	chordPacketType *packet = NULL;
	unsigned short port = 0;
	static int temp = TRUE;
	int nbyte;
	int retVal;

	if (initFlag == TRUE) {
		PRINT_ERROR("이전에 init를 완료하였습니다.");
		return 0;
	}

	memset(&clntAddr, 0, sizeof(struct sockaddr_in));
	memset(&helperAddr, 0, sizeof(struct sockaddr_in));

	helperAddr.sin_family = AF_INET;

	//helper 노드 아이피와 포트 받아야됨.
	char ipAddrStr[20] = { 0, };

	puts("당신이 네트워크에 들어가려면 helper 노드의 정보가 있어야합니다.");
	printf("helper노드 IP 주소 : ");
	scanf_s("%s", ipAddrStr, (unsigned int)sizeof(ipAddrStr));
	if ((helperAddr.sin_addr.s_addr = inet_addr(ipAddrStr)) < 0) {
		puts("잘못된 IP 주소를 입력하셨습니다.");
		exit(-1);
	}
	printf("helper노드 PORT 번호 : ");

	scanf_s("%hd", &port);
	while (!(49152 <= port && port <= 65535)) {
		FFLUSH();
		PRINT_ERROR("<Port No> should be in [49152, 65535]!");
		printf("helper노드 PORT 번호 : ");
		scanf_s("%hd", &port);
	}
	helperAddr.sin_port = htons(port);
	FFLUSH();
	//joinInfo request
	packet = createJoinInfoRequestMsg(curNode.nodeInfo);
	retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&helperAddr, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
	}

	//joinInfo response 수신
	nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("joinInfo Request를 받지 못했습니다.");
		return 0;
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
		return 0;
	}
	else if (packet->header.moreInfo != RESPONSE_RESULT_SUCCESS) {
		PRINT_ERROR("joinInfo의 결과가 실패로 반환되었습니다. 다시 시도하세요.");
		curNode.nodeInfo.ID = (curNode.nodeInfo.ID + 1) % ringSize;
		SAFE_RELEASE_PACKET(packet);
		return 0;
	}
	else { // 정상적이게 잘 수행되어서 결과가 온 경우. 다음 단계 진행. initialize -> Stablize -> MoveKeys -> fixfinger

		//initialize
		initialize();

		LOCK(hMutex);
		curNode.chordInfo.fingerInfo.Succ = packet->header.nodeInfo;
		UNLOCK(hMutex);

		PRINT_DEBUG("Succ 갱신 완료. Succ ID : %2d", curNode.chordInfo.fingerInfo.Succ.ID);

		//Stablize
		if (stabilize_Join(rqSock) == -1) {
			PRINT_ERROR("Stablize_Join 과정 중에 에러가 발생하였습니다. JOIN 과정을 무효로 합니다.");
			return 0;
		}

		//MoveKeys
		packet = createMovekeysRequestMsg(curNode.nodeInfo);
		retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
		if (retVal == -1) {
			PRINT_ERROR("sendto() failed");
			PRINT_ERROR("MoveKeys Request를 보내는 동안 에러가 발생했습니다.");
			return 0;
		}
		SAFE_RELEASE_PACKET(packet);

		nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
		if (nbyte < 0) {
			PRINT_ERROR("recvfrom() failed");
			PRINT_ERROR("MoveKeys Request를 받지 못했습니다.");
		}
		packet = createChordPacketFromBuffer(buf, nbyte);
		if (packet == NULL) {
			PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
		}

		if ((packet->header.moreInfo * sizeof(fileRefType)) != packet->header.bodySize) {
			PRINT_ERROR("MoveKeys 로 받은 데이터의 키의 갯수가 파일 참조 정보의 크기와 일치하지 않습니다.");
			return 0;
		}
		else if (packet->header.moreInfo != 0) { // 넘겨받을 키가 0이 아닐 경우에만 진행하도록 함. ( 최적화 )
			curNode.chordInfo.FRefInfo.fileNum = packet->header.moreInfo; //키 갯수
			memcpy(&curNode.chordInfo.FRefInfo.fileRef, packet->body, packet->header.bodySize); //파일 참조 정보를 입력
		}
		SAFE_RELEASE_PACKET(packet);

	}
	//fixfinger
	fix_finger(rqSock);

	LOCK(hMutex);

	initFlag = TRUE;
	ThreadsExitFlag = FALSE;

	hThread[0] = (HANDLE)_beginthreadex(NULL, 0, (void*)procRecvMsg, NULL, 0, NULL);
	if (hThread[0] < (HANDLE)0) return -1;
	hThread[1] = (HANDLE)_beginthreadex(NULL, 0, (void*)procPPandFF, NULL, 0, NULL);
	if (hThread[1] < (HANDLE)0) return -1;

	UNLOCK(hMutex);

	return 0;
}
int cmd_leave(int argc, char *argv[])
{
	int retVal, nbyte;
	nodeInfoType node;

	if (initFlag == FALSE) {
		PRINT_ERROR("create 또는 Join이 되지 않은 상황에서 leave를 시도했습니다.");
		return 0;
	}


	for (unsigned int i = 0; i < curNode.fileInfo.fileNum; i++) { //내 소유의 파일들의 참조 정보를 네트워크에서 삭제.

		node = find_successor(rqSock, curNode.fileInfo.fileRef[i].Key); //파일 참조 소유 노드 확인

		if (node.ID == curNode.nodeInfo.ID) { // 내가 소유하고 있다면
			LOCK(hMutex);
			async_delRefFile(curNode.fileInfo.fileRef[i]);
			UNLOCK(hMutex);

		}
		else { //내가 아닌 다른 노드라면 FileRefDel request메세지로 해당 파일 참조 정보 삭제.
			chordPacketType *packet = createFileRefDelRequestMsg(curNode.nodeInfo, curNode.fileInfo.fileRef[i]);
			retVal = sendto(rqSock, (const char *)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
			if (retVal == -1) {
				PRINT_ERROR("sendto() failed");
			}
			SAFE_RELEASE_PACKET(packet);

			//FileRefDel response 수신
			nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
			if (nbyte < 0) {
				PRINT_ERROR("recvfrom() failed");
				PRINT_ERROR("FileRefDel response를 받지 못했습니다.");
			}
			packet = createChordPacketFromBuffer(buf, nbyte);
			if (packet == NULL) {
				PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
			}
			else if (packet->header.moreInfo != RESPONSE_RESULT_SUCCESS) {
				PRINT_ERROR("FileRefDel 메시지의 결과가 실패로 반환되었습니다");
			}
			SAFE_RELEASE_PACKET(packet);
		}
	}
	if (curNode.chordInfo.fingerInfo.Succ.ID == curNode.nodeInfo.ID) {
		PRINT_INFO("초기화 상태의 노드 입니다. LeaveKeys 과정을 생략하고 종료합니다.");
	}
	else {
		//LeaveKeys request 송신
		chordPacketType *packet = createLeaveKeysRequestMsg(curNode.chordInfo.FRefInfo.fileNum, sizeof(fileRefType)*curNode.chordInfo.FRefInfo.fileNum, (BYTE*)curNode.chordInfo.FRefInfo.fileRef);
		retVal = sendto(rqSock, (const char *)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
		if (retVal == -1) {
			PRINT_ERROR("sendto() failed");
		}
		SAFE_RELEASE_PACKET(packet);

		//LeaveKeys response 수신
		nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
		if (nbyte < 0) {
			PRINT_ERROR("recvfrom() failed");
			PRINT_ERROR("LeaveKeys response를 받지 못했습니다.");
			return 0;
		}
		packet = createChordPacketFromBuffer(buf, nbyte);
		if (packet == NULL) {
			PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
			return 0;
		}
		else if (packet->header.moreInfo != RESPONSE_RESULT_SUCCESS) {
			PRINT_ERROR("LeaveKeys의 결과가 실패로 반환되었습니다. 다시 시도하세요.");
			SAFE_RELEASE_PACKET(packet);
			return 0;
		}
	}
	initFlag = FALSE;
	//이 부분에서 다른 쓰레드들이 종료되는 걸 기다려줘야함.
	ThreadsExitFlag = TRUE;
	PRINT_INFO("메인 쓰레드가 다른 쓰레드의 종료를 대기하는 중입니다.");
	WaitForMultipleObjects(2, hThread, TRUE, INFINITE);

	//네트워크를 완전히 나왔으므로 이전 데이터를 모두 소거.
	memset(&curNode.chordInfo, 0, sizeof(chordInfoType));
	memset(&curNode.fileInfo, 0, sizeof(fileInfoType));

	return 0;
}
int cmd_add(int argc, char *argv[])
{
	int addrlen = sizeof(struct sockaddr_in);
	chordPacketType *packet;
	char fileName[FNameMax] = { 0 };
	fileRefType addFile;
	int nbyte, retVal;

	if (nodefmax == curNode.fileInfo.fileNum) {
		printf("[INFO] 더 이상 파일을 추가 할 수 없습니다.\n");
		return 0;
	}
	printf("Enter the your file name(max file name size is 32) : ");
	fscanf(stdin, "%s", fileName);
	FFLUSH();
	if (!exists(fileName)) {
		PRINT_INFO("파일이 존재하지 않습니다.");
		return 0;
	}
	strcpy(addFile.Name, fileName);
	addFile.Key = str_hash(fileName);
	addFile.owner = curNode.nodeInfo;
	addFile.refOwner = find_successor(rqSock, addFile.Key);
	if (addFile.refOwner.ID == -1) { //find_successor가 실패한 경우
		PRINT_ERROR("find_successor 함수로 해당 키를 소유할 노드를 찾는데 실패했습니다.");
		return 0;
	}
	else if (addFile.refOwner.ID == curNode.nodeInfo.ID) { // 해당 키의 Succ가 본인 노드인 경우
		if (curNode.fileInfo.fileNum == FileMax) {
			PRINT_INFO(" 노드에 FILE REF를 더 이상 추가할 수 없습니다.");
			return 0;
		}
		else {
			LOCK(hMutex);
			curNode.fileInfo.fileRef[curNode.fileInfo.fileNum++] = addFile;
			UNLOCK(hMutex);
			sync_addRefFile(addFile);
			PRINT_INFO(" 파일을 추가하였습니다.");
			PRINT_INFO(" 현재 파일 수 : %d", curNode.fileInfo.fileNum);
			return 0;
		}
	}

	packet = createFileRefAddRequestMsg(curNode.nodeInfo, addFile);

	retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&addFile.refOwner.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("FileRefAdd Request를 보내는 동안 에러가 발생했습니다.");
		return 0;
	}
	SAFE_RELEASE_PACKET(packet);

	nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("FileRefAdd Response를 받지 못했습니다.");
		return 0;
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
		return 0;
	}

	if (packet->header.moreInfo == RESPONSE_RESULT_SUCCESS) {
		LOCK(hMutex);
		curNode.fileInfo.fileRef[curNode.fileInfo.fileNum++] = addFile;
		UNLOCK(hMutex);
		PRINT_INFO(" 파일을 추가하였습니다.");
		PRINT_INFO(" 현재 파일 수 : %d", curNode.fileInfo.fileNum);
	}
	else {
		PRINT_INFO(" 파일 추가에 실패하였습니다.");
	}
	SAFE_RELEASE_PACKET(packet);
	return 0;
}
int cmd_delete(int argc, char *argv[])
{
	int addrlen = sizeof(struct sockaddr_in);
	chordPacketType *packet;
	int fileNum;
	int nbyte;
	int checkMyFile = 0;
	if (curNode.fileInfo.fileNum == 0) {
		PRINT_INFO(" 현재 추가되어 있는 파일이 없습니다.");
		return 0;
	}
	printf("-----file list-----\n");
	for (unsigned int i = 0; i < curNode.fileInfo.fileNum; i++)
		printf("%d : %s\n", i + 1, curNode.fileInfo.fileRef[i].Name);
	//FFLUSH();
	printf("삭제할 파일 번호를 입력해주세요(0 : 취소) : ");
	scanf("%d", &fileNum);
	FFLUSH();
	if (fileNum == 0) {
		PRINT_INFO(" 파일 삭제를 취소하였습니다.");
		return 0;
	}
	LOCK(hMutex);
	checkMyFile = async_delRefFile(curNode.fileInfo.fileRef[fileNum - 1]);
	UNLOCK(hMutex);
	if (checkMyFile) {
		LOCK(hMutex);
		for (unsigned int i = fileNum; i < curNode.fileInfo.fileNum; i++) {
			curNode.fileInfo.fileRef[i - 1] = curNode.fileInfo.fileRef[i];
		}
		curNode.fileInfo.fileNum--;
		UNLOCK(hMutex);
		PRINT_INFO(" 파일을 삭제하였습니다.");
		return 0;
	}
	else {
		packet = createFileRefDelRequestMsg(curNode.nodeInfo, curNode.fileInfo.fileRef[fileNum - 1]);
		/*sendto(rqSock, (const char*)packet, sizeof(chordPacketType), 0,
			&curNode.fileInfo.fileRef[fileNum - 1].refOwner.addrInfo, addrlen);
		releaseChordPacket(packet);*/
		int retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.fileInfo.fileRef[fileNum - 1].refOwner.addrInfo, sizeof(struct sockaddr_in));
		if (retVal == -1) {
			PRINT_ERROR("sendto() failed");
			PRINT_ERROR("FileRefDel request를 보내는 동안 에러가 발생했습니다.");
			return 0;
		}
		SAFE_RELEASE_PACKET(packet);

		//packet = recvPacketFromSocket(rqSock, &curNode.fileInfo.fileRef[fileNum - 1].refOwner.addrInfo);
		nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
		if (nbyte < 0) {
			PRINT_ERROR("recvfrom() failed");
			PRINT_ERROR("FileRefDel response를 받지 못했습니다.");
			return 0;
		}
		packet = createChordPacketFromBuffer(buf, nbyte);
		if (packet == NULL) {
			PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
			return 0;
		}

		if (packet->header.moreInfo == RESPONSE_RESULT_SUCCESS) {
			LOCK(hMutex);
			for (unsigned int i = fileNum; i < curNode.fileInfo.fileNum; i++) {
				curNode.fileInfo.fileRef[i - 1] = curNode.fileInfo.fileRef[i];
			}
			curNode.fileInfo.fileNum--;
			UNLOCK(hMutex);
			PRINT_INFO(" 파일을 삭제하였습니다.");
		}
		else PRINT_INFO(" 파일 삭제에 실패하였습니다.");
		SAFE_RELEASE_PACKET(packet);
		return 0;
	}
}
int cmd_search(int argc, char *argv[])
{
	int addrlen = sizeof(struct sockaddr_in);
	chordPacketType *packet;
	nodeInfoType refOwner;
	fileRefType fileInfo;
	char fileName[FNameMax] = { 0 };
	int key, nbyte; int retVal;

	printf("찾는 파일 이름을 입력해주세요(max file name size is 32) : ");
	fscanf(stdin, "%s", fileName);
	FFLUSH();
	//fgets(fileName, FNameMax, stdin);
	//printf("연결할 포트를 지정해주세요 :");
	key = str_hash(fileName);
	for (unsigned int i = 0; i < curNode.fileInfo.fileNum; i++) {
		if (key == curNode.fileInfo.fileRef[i].Key) {
			printf("[INFO] 내가 가진 파일입니다.\n");
			return 0;
		}
	}
	for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
		if (key == curNode.chordInfo.FRefInfo.fileRef[i].Key) {
			fileInfo = curNode.chordInfo.FRefInfo.fileRef[i];
			key = -1;
			break;
		}
	}
	if (key != -1) {
		refOwner = find_successor(rqSock, key);
		packet = createFileRefInfoRequestMsg(key);
		retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&refOwner.addrInfo, sizeof(struct sockaddr_in));
		if (retVal == -1) {
			PRINT_ERROR("sendto() failed");
			PRINT_ERROR("FileInfoRequest를 보내는 동안 에러가 발생했습니다.");
			return 0;
		}
		SAFE_RELEASE_PACKET(packet);

		nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
		if (nbyte < 0) {
			PRINT_ERROR("recvfrom() failed");
			PRINT_ERROR("FileInfoResponse를 받지 못했습니다.");
		}
		packet = createChordPacketFromBuffer(buf, nbyte);
		if (packet == NULL) {
			PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
			//continue;
		}
		if (packet->header.moreInfo == RESPONSE_RESULT_SUCCESS)fileInfo = packet->header.fileInfo;
		else {
			PRINT_INFO("파일을 찾을 수 없습니다.");
			return 0;
		}
		SAFE_RELEASE_PACKET(packet);
	}
	//fileOwner에게 다운로드 요청
	packet = createFileDownRequestMsg(fileInfo);
	retVal = sendto(rqSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&fileInfo.owner.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("FileDownRequest를 보내는 동안 에러가 발생했습니다.");
		return 0;
	}
	SAFE_RELEASE_PACKET(packet);

	nbyte = recvfrom(rqSock, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("FileDownResponse를 받지 못했습니다.");
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
		return 0;
	}
	if (packet->header.moreInfo == RESPONSE_RESULT_SUCCESS) {
		LOCK(hMutex);
		fileRecvInfo.partnerAddr = fileInfo.owner.addrInfo;
		UNLOCK(hMutex);
		fThread = (HANDLE)_beginthreadex(NULL, 0, (void*)procFileReceiver, NULL, 0, NULL);
		if (fThread < (HANDLE)0) return -1;
		//fileReceiver(port, fileInfo.owner.addrInfo);
		//UNLOCK(hMutex);
	}
	else PRINT_ERROR("다운로드에 실패했습니다.");
	SAFE_RELEASE_PACKET(packet);

	return 0;
}
int cmd_finger(int argc, char *argv[])
{
	PRINT_INFO("Finger table Information:");
	LOCK(hMutex);
	PRINT_INFO("My IP Addr: %s, Port No: %2d, ID: %2d", inet_ntoa(curNode.nodeInfo.addrInfo.sin_addr), ntohs(curNode.nodeInfo.addrInfo.sin_port), curNode.nodeInfo.ID);
	PRINT_INFO("Predecessor IP Addr: %s, Port No: %2d, ID: %2d", inet_ntoa(curNode.chordInfo.fingerInfo.Pre.addrInfo.sin_addr), ntohs(curNode.chordInfo.fingerInfo.Pre.addrInfo.sin_port), curNode.chordInfo.fingerInfo.Pre.ID);
	for (int i = 0; i < baseM; i++) {
		PRINT_INFO("Finger[%d] IP Addr: %s, Port No: %2d, ID: %2d", i, inet_ntoa(curNode.chordInfo.fingerInfo.finger[i].addrInfo.sin_addr), ntohs(curNode.chordInfo.fingerInfo.finger[i].addrInfo.sin_port), curNode.chordInfo.fingerInfo.finger[i].ID);
	}
	UNLOCK(hMutex);
	return 0;
}
int cmd_info(int argc, char *argv[])
{
	PRINT_INFO("My Node Infromation:");
	LOCK(hMutex);
	PRINT_INFO("My node IP Addr: %s, Port No: %d, ID: %2d", inet_ntoa(curNode.nodeInfo.addrInfo.sin_addr), ntohs(curNode.nodeInfo.addrInfo.sin_port), curNode.nodeInfo.ID);
	UNLOCK(hMutex);
	return 0;
}
int cmd_mute(int argc, char *argv[])
{
	silentMode = (silentMode) ? FALSE : TRUE; //모드 토글
	return 0;
}
int cmd_help(int argc, char *argv[])
{
	/*help 명령 처리*/
	if (argc == 1)//명령어만 쳤을 때
	{
		for (int i = 0; i < builtins; i++)
		{
			printf("CHORD> (%c)%-7s : %s\n", builtin[i].name[0], builtin[i].name + 1, builtin[i].desc);
		}
	}
	else
	{
		for (int i = 0; i < builtins; i++)
		{
			if (strcmp(builtin[i].name, argv[1]) == 0)
			{
				printf("CHORD> (%c)%-7s : %s\n", builtin[i].name[0], builtin[i].name + 1, builtin[i].desc);
				return 0;
			}
		}
		printf("존재하지 않는 명령어입니다\n");
	}
	return 0;
}
int cmd_quit(int argc, char *argv[])
{
	//leave를 하지 않은 상태이면 leave를 할 수 있도록 조치.
	if (initFlag == TRUE)
		cmd_leave(argc, argv);
	return 1;
}

int recentLeaveNodeID = -1;
////쓰레드 함수 선언 시작
unsigned int WINAPI procWorker(void * arg)
{
	char name[10] = "[worker ]";
	int id = *((int *)arg);
	RecvMsgDataType data;
	int addrlen = sizeof(struct sockaddr_in);
	int nbyte = 0; int retVal;
	struct sockaddr_in clntAddr;
	nodeInfoType node;
	chordPacketType *packet = NULL, *responsePacket = NULL;
	fileRefType *leaveKey = NULL;
	SOCKET rpSock = wSock[id]; //소켓 Override.
	fileInfoType files; // ref파일을 넘겨주기 위한 용도
	name[7] = '0' + id;

	while (!ThreadsExitFlag) {

		LOCK(qfull);

		LOCK(qmutex);
		dequeue(&queue, &data); //c.s
		UNLOCK(qmutex);

		ReleaseSemaphore(qempty, 1, NULL);

		if (memcmp(&DATA_END_VALUE, &data, sizeof(RecvMsgDataType)) == 0) {
			PRINT_DEBUG("Worker[%d] 쓰레드가 종료 메시지를 받았습니다.", id);
			break;
		}
		memcpy(&clntAddr, &data.senderInfo, sizeof(struct sockaddr_in));
		packet = data.packet;

		switch (packet->header.msgID) {
			unsigned int i;
			responsePacket = NULL;
			node.ID = -1;
		case MSGID_PINGPONG: // O
			PRINT_DEBUG("PingPong request 수신");
			responsePacket = createPingPongResponseMsg();
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("PingPong request 처리완료");
			break;
		case MSGID_JOININFO: // O
			PRINT_DEBUG("JoinInfo request 수신");
			int targetID = modPlus(ringSize, packet->header.nodeInfo.ID, 1); //요청한 노드의 succ 정보를 가져오기 위해 요청한 노드의 ID + 1의 Succ를 찾음.
			node = find_successor(rpSock, targetID);
			if (node.ID == -1) {
				PRINT_DEBUG("find_successor 함수에서 실패가 반환되었습니다.");
				PRINT_ERROR("JoinInfo request 처리실패");
				break;
			}
			responsePacket = createJoinInfoResponseMsg(node, RESPONSE_RESULT_SUCCESS);
			retVal = sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			if (retVal == -1) {
				PRINT_ERROR("sendto() failed");
				PRINT_ERROR("JoinInfo Response를 보내는 동안 에러가 발생했습니다.");
				return 0;
			}
			SAFE_RELEASE_PACKET(packet);
			PRINT_DEBUG("JoinInfo request 처리완료");
			break;
		case MSGID_MOVEKEYS: // O
			PRINT_DEBUG("MoveKeys request 수신");
			memset(&files, 0, sizeof(fileInfoType));
			move_keys(packet->header.nodeInfo.ID, &files);
			responsePacket = createMoveKeysResponseMsg(files.fileNum, sizeof(fileRefType)*files.fileNum, (BYTE*)&files.fileRef);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType) + (responsePacket->header.bodySize), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("MoveKeys request 처리완료");
			break;
		case MSGID_PREDINFO: // O
			PRINT_DEBUG("PredInfo request 수신");
			LOCK(hMutex);
			node = curNode.chordInfo.fingerInfo.Pre; // pred 가져옴
			UNLOCK(hMutex);
			responsePacket = createPredInfoResponseMsg(node, RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("PredInfo request 처리완료");
			break;
		case MSGID_PREDUPDATE: // O
			PRINT_DEBUG("PredUpdate request 수신");
			LOCK(hMutex);
			curNode.chordInfo.fingerInfo.Pre = packet->header.nodeInfo; // 해당 노드 정보로 Predcessor 갱신
			UNLOCK(hMutex);
			responsePacket = createPredUpdateResponseMsg(RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("PredUpdate request 처리완료");
			break;
		case MSGID_SUCCINFO: // O
			PRINT_DEBUG("SuccInfo request 수신");
			LOCK(hMutex);
			node = curNode.chordInfo.fingerInfo.Succ;
			UNLOCK(hMutex);
			responsePacket = createSuccInfoResponseMsg(node, RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("SuccInfo request 처리완료");
			break;
		case MSGID_SUCCUPDATE: // O
			PRINT_DEBUG("SuccUpdate request 수신");
			LOCK(hMutex);
			curNode.chordInfo.fingerInfo.Succ = packet->header.nodeInfo;
			UNLOCK(hMutex);
			responsePacket = createSuccUpdateResponseMsg(RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("SuccUpdate request 처리완료");
			break;
		case MSGID_FINDPRED: // O
			PRINT_DEBUG("FindPred request 수신");
			//LOCK(hMutex);
			node = find_predecessor(rpSock, packet->header.nodeInfo.ID); // pred 정보를 가져오고 보냄.
																		 //UNLOCK(hMutex);
			if (node.ID == -1) {
				PRINT_ERROR("FindPred request 처리실패");
				break;
			}
			responsePacket = createFindPredResponseMsg(node);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("FindPred request 처리완료");
			break;
		case MSGID_LEAVEKEYS: // O
			PRINT_DEBUG("LeaveKeys request 수신");
			recentLeaveNodeID = packet->header.nodeInfo.ID;
			leaveKey = malloc(sizeof(fileRefType)*(packet->header.moreInfo));
			memcpy(leaveKey, packet->body, packet->header.bodySize);
			LOCK(hMutex);
			for (int i = 0; i < packet->header.moreInfo; i++) {
				curNode.chordInfo.FRefInfo.fileRef[curNode.chordInfo.FRefInfo.fileNum++] = leaveKey[i];
			}
			UNLOCK(hMutex);
			responsePacket = createLeaveKeysResponseMsg(RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("LeaveKeys request 처리완료");
			break;
		case MSGID_FILEREFADD: // O
			PRINT_DEBUG("FileRedAdd request 수신");
			if (curNode.chordInfo.FRefInfo.fileNum == FileMax) {
				responsePacket = createFileRefAddResponseMsg(RESPONSE_RESULT_FAILURE);
			}
			else {
				LOCK(hMutex);
				curNode.chordInfo.FRefInfo.fileRef[curNode.chordInfo.FRefInfo.fileNum++] = packet->header.fileInfo;
				UNLOCK(hMutex);
				printf("\n[INFO] %d번 노드로부터 파일 REF [%s]가 추가되었습니다.\nCHORD> ", packet->header.fileInfo.owner.ID, packet->header.fileInfo.Name);
				responsePacket = createFileRefAddResponseMsg(RESPONSE_RESULT_SUCCESS);
			}
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("FileRedAdd request 처리완료");
			break;
		case MSGID_FILEREFDEL: // O
			PRINT_DEBUG("FileRefDel request 수신");
			/*for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
				if (packet->header.fileInfo.Key == curNode.chordInfo.FRefInfo.fileRef[i].Key) {
					for (unsigned int j = i + 1; j < curNode.chordInfo.FRefInfo.fileNum; j++) {
						curNode.chordInfo.FRefInfo.fileRef[j - 1] = curNode.chordInfo.FRefInfo.fileRef[j];
					}
					break;
				}
			}*/
			LOCK(hMutex);
			async_delRefFile(packet->header.fileInfo);
			UNLOCK(hMutex);
			printf("\n[INFO] %d번 노드로부터 파일 REF [%s]가 삭제되었습니다.\nCHORD> ", packet->header.fileInfo.owner.ID, packet->header.fileInfo.Name);
			responsePacket = createFileRefDelResponseMsg(RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *) &clntAddr, addrlen);
			PRINT_DEBUG("FileRefDel request 처리완료");
			break;
		case MSGID_FILEDOWN: // O
			PRINT_DEBUG("FileRefDel request 수신");
			LOCK(hMutex);
			for (i = 0; i < curNode.fileInfo.fileNum; i++) {
				if (!strcmp(curNode.fileInfo.fileRef[i].Name, packet->header.fileInfo.Name)) {
					fileSendInfo.partnerAddr = clntAddr;
					strcpy(fileSendInfo.fileName, packet->header.fileInfo.Name);
					responsePacket = createFileDownResponseMsg(RESPONSE_RESULT_SUCCESS);
					break;
				}

			}
			if (i == curNode.fileInfo.fileNum) {
				responsePacket = createFileDownResponseMsg(RESPONSE_RESULT_FAILURE);
			}
			UNLOCK(hMutex);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType) + sizeof(unsigned short), 0, (const struct sockaddr *) &clntAddr, addrlen);
			fThread = (HANDLE)_beginthreadex(NULL, 0, (void*)procFileSender, NULL, 0, NULL);
			if (fThread < (HANDLE)0) return -1;
			//fileSender(packet->header.fileInfo.Name, fileSendPort, clntAddr,curNode.nodeInfo.addrInfo);
			PRINT_DEBUG("FileRefDel request 처리완료");
			break;
		case MSGID_FILEREFINFO: // O
			PRINT_DEBUG("FileRefInfo request 수신");
			if (curNode.chordInfo.FRefInfo.fileNum != 0) {
				for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
					if (packet->header.fileInfo.Key == curNode.chordInfo.FRefInfo.fileRef[i].Key) {
						responsePacket = createSuccessFileRefInfoResponseMsg(RESPONSE_RESULT_SUCCESS, curNode.chordInfo.FRefInfo.fileRef[i]);
						//sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
						break;
					}
					responsePacket = createFailureFileRefInfoResponseMsg(RESPONSE_RESULT_FAILURE);
				}
			}
			else responsePacket = createFailureFileRefInfoResponseMsg(RESPONSE_RESULT_FAILURE);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen);
			PRINT_DEBUG("FileRefInfo request 처리완료");
			break;
		case MSGID_NOTIFYLEAVE: // 해당 노드가 빠져나갔음을 tokenRing처럼 전파
			PRINT_DEBUG("NotifyLeave request 수신");
			responsePacket = createNotifyLeaveNodeResponseMsg(RESPONSE_RESULT_SUCCESS);
			sendto(rpSock, (const char*)responsePacket, sizeof(chordPacketType), 0, (const struct sockaddr *)&clntAddr, addrlen); //먼저 메시지를 수신 받았음을 알림. (빠른 수신처리를 위해.)
			SAFE_RELEASE_PACKET(responsePacket);

			if (recentLeaveNodeID == packet->header.moreInfo) {
				PRINT_DEBUG("graceful하게 종료된 노드에 대한 Notify이므로 세부 과정 패스");
				recentLeaveNodeID = -1;
				PRINT_DEBUG("NotifyLeave request 처리완료.");
				break;
			}

			if (curNode.nodeInfo.ID != packet->header.nodeInfo.ID) { //현재 노드가 처음 전파한 노드라면 전파를 중지를 한다.
				PRINT_DEBUG("NotifyLeave request를 전파합니다.");
				retVal = sendto(rpSock, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
				if (retVal == -1) {
					PRINT_ERROR("sendto() failed");
					PRINT_ERROR("notifyLeave Request를 보내는 동안 에러가 발생했습니다.");
					//break;
				}
				nbyte = recvfrom(rpSock, buf, BUFSIZE, 0, NULL, NULL);
				if (nbyte < 0) {
					PRINT_ERROR("recvfrom() failed");
					PRINT_ERROR("notifyLeave Response를 받지 못했습니다.");
					//break;
				}
			}
			//파일 참조 정보 Update.

			int leaveNodeID = packet->header.moreInfo;

			LOCK(hMutex);
			for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
				if (curNode.chordInfo.FRefInfo.fileRef[i].owner.ID == leaveNodeID) { // 떠난 노드의 참조 키를 가지고 있다면, 해당 파일 참조 정보를 삭제
					async_delRefFile(curNode.chordInfo.FRefInfo.fileRef[i]);
				}
			}
			UNLOCK(hMutex);

			nodeInfoType targetNode=packet->header.fileInfo.owner; //참조 파일 정보를 받을 타겟 노드.
			//내 소유의 파일 정보 Update.
			//LOCK(hMutex);

			chordPacketType *refPacket = NULL;
			for (unsigned int i = 0; i < curNode.fileInfo.fileNum; i++) {
				if (leaveNodeID == curNode.fileInfo.fileRef[i].refOwner.ID) {
					if (targetNode.ID != curNode.nodeInfo.ID) { // 떠난 노드가 내가 소유한 파일의 참조 정보를 가지고 있다면. 이를 재전파.
						curNode.fileInfo.fileRef[i].refOwner = targetNode;
						refPacket = createFileRefAddRequestMsg(curNode.nodeInfo, curNode.fileInfo.fileRef[i]);
						printf("%d에게 [%s] ref Info 전송\n", targetNode.ID, curNode.fileInfo.fileRef[i].Name);
						retVal = sendto(rpSock, (const char*)refPacket, sizeof(chordPacketType) + refPacket->header.bodySize, 0, (const struct sockaddr *)&targetNode.addrInfo, sizeof(struct sockaddr_in));
						if (retVal == -1) {
							PRINT_ERROR("sendto() failed");
							PRINT_ERROR("NotifyLeave과정 중 FileRefAdd request를 보내는 동안 에러가 발생했습니다.");
							break;
						}

						nbyte = recvfrom(rpSock, buf, BUFSIZE, 0, NULL, NULL);
						if (nbyte < 0) {
							PRINT_ERROR("recvfrom() failed");
							PRINT_ERROR("NotifyLeave과정 중 FileRefAdd request를 받지 못했습니다.");
						}

						SAFE_RELEASE_PACKET(refPacket);
					}
					else {
						LOCK(hMutex);
						curNode.fileInfo.fileRef[i].refOwner = targetNode;
						UNLOCK(hMutex);
						sync_addRefFile(curNode.fileInfo.fileRef[i]);
						printf("내 chordInfo에 [%s] ref Info 저장\n", curNode.fileInfo.fileRef[i].Name);
					}
				}
			}
			PRINT_DEBUG("NotifyLeave request 처리완료.");
			break;
		default:
			PRINT_DEBUG("정의되어 있지 않은 메시지가 발생하였습니다.");
			break;
		}
		SAFE_RELEASE_PACKET(packet);
		SAFE_RELEASE_PACKET(responsePacket);
	}
	SAFE_RELEASE_PACKET(packet);
	_endthreadex(0);
	return 0;
}
unsigned int WINAPI procRecvMsg(void * arg)
{
	int i;
	const char* name = "[procRecvMsg]";
	int nbyte = 0;
	struct sockaddr_in clntAddr;
	int addrlen = sizeof(struct sockaddr_in);
	BYTE buf[BUFSIZE];
	memset(&clntAddr, 0, addrlen);
	chordPacketType *packet;
	RecvMsgDataType data;

	UNUSED(arg);
	LOCK(hMutex);
	UNLOCK(hMutex);

	qempty = CreateSemaphore(NULL, QUEUE_SIZE, QUEUE_SIZE, NULL);
	qfull = CreateSemaphore(NULL, 0, QUEUE_SIZE, NULL);
	qmutex = CreateMutex(NULL, FALSE, NULL);

	for (i = 0; i < WORKER_COUNT; i++) {
		workerID[i] = i;
	}
	if (initqueue(&queue, QUEUE_SIZE) == -1)
	{
		PRINT_ERROR("init queue error.");
		return -1;
	}
	for (i = 0; i < WORKER_COUNT; i++) {
		if ((wSock[i] = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //wSock[i] - UDP
			PRINT_ERROR("wSock[%d] failed…", i);
			exit(-1);
		}
	}

	static const int optVal = 5000; // 5 seconds

	for (i = 0; i < WORKER_COUNT; i++) {
		if (setsockopt(wSock[i], SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
			PRINT_ERROR("wsock[%d]에 5초 timeout 설정 실패", i);
			exit(-1);
		}
	}
	for (i = 0; i < WORKER_COUNT; i++) {
		hWorker[i] = (HANDLE)_beginthreadex(NULL, 0, (void*)procWorker, (void*)&workerID[i], 0, NULL);
		if (hWorker[i] < (HANDLE)0) return -1;
	}

	while (!ThreadsExitFlag) {
		nbyte = recvfrom(rpSock, buf, BUFSIZE, 0, (struct sockaddr *)&clntAddr, &addrlen);
		if (nbyte < 0) {
			if (!silentMode) {
				PRINT_INFO("rqSock 타임아웃");
			}
			continue;
		}
		if (!silentMode) {
			PRINT_INFO("rpSock이 메세지를 수신하였습니다.");
		}
		packet = createChordPacketFromBuffer(buf, nbyte);
		if (packet == NULL) {
			PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
			continue;
		}
		if (packet->header.msgType != MSGTYPE_REQUEST) {
			PRINT_ERROR("rpSock은 처음 시작은 request를 수신해야 합니다");
			SAFE_RELEASE_PACKET(packet);
			continue;
		}
		memcpy(&data.senderInfo, &clntAddr, sizeof(struct sockaddr_in));
		data.packet = packet;

		LOCK(qempty);

		LOCK(qmutex);
		enqueue(&queue, data); //c.s
		UNLOCK(qmutex);

		ReleaseSemaphore(qfull, 1, NULL);
	}
	//send task-end message to workers.
	for (i = 0; i < WORKER_COUNT; i++)
	{
		LOCK(qempty);

		LOCK(qmutex);
		enqueue(&queue, DATA_END_VALUE); //c.s
		UNLOCK(qmutex);

		ReleaseSemaphore(qfull, 1, NULL);
	}

	PRINT_DEBUG("Worker 쓰레드가 종료하기를 기다립니다.");
	WaitForMultipleObjects(WORKER_COUNT, hWorker, TRUE, INFINITE);

	for (int i = 0; i < WORKER_COUNT; i++) {
		CloseHandle(hWorker[i]);
	}

	CloseHandle(qempty);
	CloseHandle(qfull);
	CloseHandle(qmutex);

	destroyqueue(&queue);

	PRINT_INFO("procRecvMsg 쓰레드를 종료합니다.");
	_endthreadex(0);
	return 0;
}
unsigned int WINAPI procPPandFF(void * arg)
{
	const char* name = "[procPPandFF]";
	char buf[BUFSIZE];

	UNUSED(arg);

	LOCK(hMutex);
	UNLOCK(hMutex);

	while (!ThreadsExitFlag)
	{
		Sleep(5000 + (rand() % 5000));

		//pingpong
		ping_pong(pfSock, buf);

		//fixfinger
		fix_finger(pfSock);
	}

	PRINT_INFO("procPPandFF 쓰레드를 종료합니다.");
	_endthreadex(0);
	return 0;
}
unsigned int WINAPI procFileSender(void * arg)
{
	fileSender(fileSendInfo.fileName, fileSendInfo.partnerAddr, curNode.nodeInfo.addrInfo);
	printf("[INFO] fileSender 쓰레드를 종료합니다.\nCHORD> ");
	_endthreadex(0);
	return 0;
}
unsigned int WINAPI procFileReceiver(void * arg)
{
	fileReceiver(fileRecvInfo.partnerAddr);
	printf("[INFO] fileReceiver 쓰레드를 종료합니다.\nCHORD> ");
	_endthreadex(0);
	return 0;
}
////쓰레드 함수 선언 종료

int init()
{
	if ((rqSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //rqSock - UDP
		PRINT_ERROR("rqSock failed…");
		exit(-1);
	}
	if ((rpSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //rpSock - UDP
		PRINT_ERROR("rpSock failed…");
		exit(-1);
	}
	if ((pfSock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { //pfSock - UDP
		PRINT_ERROR("pfSock failed…");
		exit(-1);
	}
	static const int optVal = 5000; // 5 seconds
	static const int fpOptVal = 1000; // 1 seconds

	if (setsockopt(rpSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		PRINT_ERROR("rpSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	if (setsockopt(rqSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		PRINT_ERROR("rqSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	if (setsockopt(pfSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&fpOptVal, sizeof(fpOptVal)) < 0) {
		PRINT_ERROR("pfSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	if (bind(rpSock, (struct sockaddr *)&curNode.nodeInfo.addrInfo, sizeof(struct sockaddr_in)) < 0) {
		PRINT_ERROR("바인드 에러!");
		exit(-1);
	}

	return 0;
}/*
int fsInit() {
	if ((flSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		PRINT_ERROR("flSock failed…");
		exit(-1);
	}
	if ((fsSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		PRINT_ERROR("fsSock failed…");
		exit(-1);
	}
	static const int optVal = 5000; // 5 seconds
	if (setsockopt(flSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		PRINT_ERROR("flSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	if (setsockopt(fsSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		PRINT_ERROR("fsSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	return 0;
}
int frInit() {
	if ((frSock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { //flSock - TCP
		PRINT_ERROR("frSock failed…");
		exit(-1);
	}
	static const int optVal = 5000; // 5 seconds
	if (setsockopt(frSock, SOL_SOCKET, SO_RCVTIMEO, (char *)&optVal, sizeof(optVal)) < 0) {
		PRINT_ERROR("frSock에 5초 timeout 설정 실패");
		exit(-1);
	}
	return 0;
}
int fsClose() {
	closesocket(fsSock);
	closesocket(flSock);
	return 0;
}
int frClose() {
	closesocket(frSock);
	return 0;
}*/
int release()
{
	PRINT_DEBUG("쓰레드에 할당된 리소스 해제.");
	CloseHandle(hThread[0]);
	CloseHandle(hThread[1]);
	closesocket(rpSock);
	closesocket(rqSock);
	closesocket(pfSock);
	closesocket(flSock);
	closesocket(frSock);
	closesocket(fsSock);
	return 0;
}

nodeInfoType find_successor(SOCKET s, int keyID)
{
	chordPacketType *packet = NULL;
	nodeInfoType pred;
	nodeInfoType succ;
	int nbyte;
	char buf[BUFSIZE];

	succ.ID = -1;
	PRINT_DEBUG("find_successor 과정을 시작keyID : %2d, 나의 ID : %2d", keyID, curNode.nodeInfo.ID);

	LOCK(hMutex);
	if (curNode.nodeInfo.ID == keyID) { //if (N = id) return N;
		succ = curNode.nodeInfo;
	}
	else if (curNode.nodeInfo.ID == curNode.chordInfo.fingerInfo.Succ.ID) { // init node 상태인 나에게 Join을 시도했던 거임.
		succ = curNode.nodeInfo;
	}
	else if (modIn(ringSize, keyID, curNode.nodeInfo.ID, curNode.chordInfo.fingerInfo.Succ.ID, 0, 1)) { // 현재 노드 <---(여기)---> Succ 에 있었던 거임. 
		succ = curNode.chordInfo.fingerInfo.Succ;
	}
	UNLOCK(hMutex);

	if (succ.ID != -1) { //Succ 노드를 찾았다면 이를 반환
		return succ;
	}

	//find_pred 함수 진행
	pred = find_predecessor(s, keyID);
	if (pred.ID == -1) {
		PRINT_ERROR("find_successor에서 find_predecessor진행 중에 실패하였습니다.");
		return succ;
	}
	else if (pred.ID == curNode.nodeInfo.ID) {
		PRINT_DEBUG("본인 노드가 pred인 상황은 이미 확인을 하였음에도 같은 결과가 나와 find_successor 과정을 실패하였습니다.");
		return succ;
	}

	//succInfo 과정 진행
	packet = createSuccInfoRequestMsg();
	int retVal = sendto(s, (const char *)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&pred.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		return succ;
	}
	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		return succ;
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("버퍼에서 패킷 데이터를 생성하는데에 실패하였습니다.");
		return succ;
	}
	if (!(packet->header.msgID == MSGID_SUCCINFO && packet->header.msgType == MSGTYPE_RESPONSE)) { //findPred 메세지가 아니거나, response가 아닌 경우는 예외 처리
		PRINT_ERROR("find_successor의 SuccInfo 과정 중 엉뚱한 메세지가 응답으로 왔습니다.");
		return succ;
	}
	succ = packet->header.nodeInfo; //최종적으로 얻어온 succ를 전달
	return succ;
}
nodeInfoType find_predecessor(SOCKET s, int keyID) {
	int nbyte = 0;
	int retVal;
	BYTE buf[BUFSIZE];
	int addrlen = sizeof(struct sockaddr_in);
	chordPacketType *responsePacket = NULL, *requestPacket = NULL;
	nodeInfoType pred = { 0, };
	nodeInfoType pred_candi;

	pred.ID = -1;
	PRINT_DEBUG("find_predecessor과정 시작. keyID : %2d, 나의 ID : %2d", keyID, curNode.nodeInfo.ID);
	if (modIn(ringSize, keyID, curNode.nodeInfo.ID, curNode.chordInfo.fingerInfo.Succ.ID, 0, 1)) { //내 자신이 타겟ID의 Pred인 경우.
		PRINT_DEBUG("해당 타겟의 pred가 본인 노드임");
		return curNode.nodeInfo;
	}

	pred_candi = find_closest_predecessor(keyID);

	if (pred_candi.ID == curNode.nodeInfo.ID) { // 현재 노드 <---(여기) ---->Succ 에 있었던 거임.
		return curNode.chordInfo.fingerInfo.Succ;
	}
	// Pred 후보 노드에게 FindPred 메시지를 보내서 Pred 정보를 받아오기
	PRINT_DEBUG("FindPred를 받을 후보를 선정하였습니다. keyID : %2d, 후보 ID : %2d", keyID, pred_candi.ID);
	requestPacket = createFindPredRequestMsg(keyID);
	retVal = sendto(s, (const char*)requestPacket, sizeof(chordPacketType), 0, (const struct sockaddr *) &pred_candi.addrInfo, addrlen);
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		return pred;
	}
	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) { //수신 실패 에러 처리
		PRINT_ERROR("recvfrom() failed");
		return pred;
	}
	responsePacket = createChordPacketFromBuffer(buf, nbyte);
	if (responsePacket == NULL) {
		PRINT_ERROR("버퍼에서 패킷 데이터를 생성하는데에 실패하였습니다.");
		return pred;
	}
	if (!(responsePacket->header.msgID == MSGID_FINDPRED && responsePacket->header.msgType == MSGTYPE_RESPONSE)) {
		PRINT_ERROR("find_predecessor의 findPred 과정 중 엉뚱한 메세지가 응답으로 왔습니다.");
		return pred;
	}
	//response 패킷으로부터 pred 노드 정보를 얻어옴.
	pred = responsePacket->header.nodeInfo;
	PRINT_DEBUG("findPred 결과 target ID : %2d, Pred ID : %2d", keyID, pred.ID);

	SAFE_RELEASE_PACKET(requestPacket);
	SAFE_RELEASE_PACKET(responsePacket);
	return pred;
}
nodeInfoType find_closest_predecessor(int IDkey)
{
	int i;
	nodeInfoType node;
	node.ID = -1;

	LOCK(hMutex);
	for (i = baseM - 1; i >= 0; i--) {																			//for (i = m down to 1)
		if (modIn(ringSize, curNode.chordInfo.fingerInfo.finger[i].ID, curNode.nodeInfo.ID, IDkey, 0, 0)) {		//if (finger[i] ∈ (N, id)) N is the current node
			node = curNode.chordInfo.fingerInfo.finger[i];
			if (node.ID == -1) {
				continue;
			};
			break;
		}
	}
	if (i == -1) {
		node = curNode.nodeInfo;																				// The node itself
	}
	UNLOCK(hMutex);

	return node;
}

//nodeInfoType lookup(SOCKET s, int keyID)
//{
//	nodeInfoType node;
//	node.ID = -1;
//
//	LOCK(hMutex);
//	for (unsigned int i = 0; i < curNode.fileInfo.fileNum; i++) {	//내가 가지고 있는 노드 중에 있다면 나의 노드를 반환.
//		if (keyID = curNode.fileInfo.fileRef[i].Key) {
//			node = curNode.nodeInfo;
//			break;
//		}
//	}
//	UNLOCK(hMutex);
//
//	if (node.ID == -1) {									//내가 가지고 있지 않았다면 해당 key를 가지고 있는 노드를 찾아오라 함.
//		node = find_successor(s, keyID);
//	}
//
//	return node;
//}

void ping_pong(SOCKET s, char buf[])
{
	chordPacketType *packet = NULL;
	int retVal, nbyte;

	packet = createPingPongRequestMsg();

	int deadCheckFlag = 0;
	while (deadCheckFlag < 3) {
		//pred가 살아있는지 확인
		if (curNode.nodeInfo.ID != curNode.chordInfo.fingerInfo.Pre.ID && curNode.chordInfo.fingerInfo.Pre.ID != -1) {
			retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Pre.addrInfo, sizeof(struct sockaddr_in));
			if (retVal == -1) {
				PRINT_ERROR("sendto() failed");
				PRINT_DEBUG("PingPong request를 보내는 동안 에러가 발생했습니다.");
				return;
			}

			nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
			if (nbyte < 0) {
				deadCheckFlag++;
				PRINT_DEBUG("Pred가 PingPong 메시지의 response를 전달하지 않았습니다. count : %d", deadCheckFlag);
			}
			else {
				break;
			}
		}
	}

	if (deadCheckFlag >= 3) {
		PRINT_DEBUG("pred가 3번의 PingPong 메시지의 response를 못했으므로 갱신합니다.");
		LOCK(hMutex);
		memcpy(&curNode.chordInfo.fingerInfo.Pre, &curNode.nodeInfo, sizeof(nodeInfoType));	// Pre = curNode;
		UNLOCK(hMutex);
	}

	//finger[1] ~ finger[baseM-1] 까지
	for (int i = baseM - 1; i >= 0; i--) {
		if (curNode.chordInfo.fingerInfo.finger[i].ID != curNode.nodeInfo.ID && curNode.chordInfo.fingerInfo.finger[i].ID != -1) { // 핑거 노드가 초기화되어 있지 않고, 본인 노드가 아니라면 진행.
			if (i == baseM - 1 || curNode.chordInfo.fingerInfo.finger[i].ID != curNode.chordInfo.fingerInfo.finger[i + 1].ID) { // 마지막 핑거 노드이거나 이전 핑거 노드와 동일하다면 진행.
				retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.finger[i].addrInfo, sizeof(struct sockaddr_in));
				if (retVal == -1) {
					PRINT_ERROR("sendto() failed");
					PRINT_DEBUG("PingPong request를 보내는 동안 에러가 발생했습니다.");
					break;
				}

				nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
				if (nbyte < 0) {
					PRINT_DEBUG("PingPong response를 받지 못했습니다. 응답이 없는 노드 finger[%d]", i);
					if (i == (baseM - 1)) { // finger[baseM - 1] 노드가 응답이 없다면
						LOCK(hMutex);
						curNode.chordInfo.fingerInfo.finger[i] = curNode.chordInfo.fingerInfo.Pre;
						UNLOCK(hMutex);
						PRINT_DEBUG("finger[%d]가 응답하지 않아서 Pred 노드 정보로 테이블을 덮어씁니다.", i);
					}
					else if (i == 0) { // Succ 노드가 응답이 없다면
						stablize_Leave(s);
					}
					else { // finger 중간 노드가 응답이 없다면
						LOCK(hMutex);
						curNode.chordInfo.fingerInfo.finger[i] = curNode.chordInfo.fingerInfo.finger[i + 1];
						UNLOCK(hMutex);
						PRINT_DEBUG("finger[%d]가 응답하지 않아서 finger[%d] 노드 정보로 테이블을 덮어씁니다.", i, i + 1);
					}
				}
			}
		}
	}

	SAFE_RELEASE_PACKET(packet);
}
int fix_finger(SOCKET s)
{
	int i; int keyID;
	nodeInfoType node;

	PRINT_DEBUG("Fixfinger를 시작.");
	for (i = 1; i < baseM; i++) {
		keyID = modPlus(ringSize, curNode.nodeInfo.ID, twoPow(i));
		node = find_successor(s, keyID);
		if (node.ID == -1) { //에러처리
			PRINT_ERROR("find_successor에서 잘못된 Succ 노드가 반환되었습니다.");
		}
		else {
			LOCK(hMutex);
			if (memcpy(&curNode.chordInfo.fingerInfo.finger[i], &node, sizeof(nodeInfoType))) {
				curNode.chordInfo.fingerInfo.finger[i] = node;
			}
			UNLOCK(hMutex);
		}
	}
	PRINT_DEBUG("Fixfinger를 종료.");
	return 0;
}

int stabilize_Join(SOCKET s)
{
	chordPacketType *packet;
	int retVal, nbyte;

	PRINT_DEBUG("Stablize_Join 시작.");

	//PredInfo
	packet = createPredInfoRequestMsg();
	//sendChordPacketToClnt(s, packet, &curNode.chordInfo.fingerInfo.Succ.addrInfo);
	retVal = sendto(s, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("SuccInfo Request를 보내는 동안 에러가 발생했습니다.");
		return -1;
	}
	SAFE_RELEASE_PACKET(packet);

	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("SuccInfo Request를 받지 못했습니다.");
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
	}
	LOCK(hMutex);
	curNode.chordInfo.fingerInfo.Pre = packet->header.nodeInfo;
	UNLOCK(hMutex);
	SAFE_RELEASE_PACKET(packet);
	PRINT_DEBUG("Pred 갱신 완료. Pred ID : %2d", curNode.chordInfo.fingerInfo.Pre.ID);

	if (curNode.chordInfo.fingerInfo.Pre.ID == curNode.nodeInfo.ID || curNode.chordInfo.fingerInfo.Succ.ID == curNode.nodeInfo.ID) { // 만약 내 ID와 동일한 노드가 네트워크에 존재한다면.
		memset(&curNode.chordInfo.fingerInfo.Pre, 0, sizeof(nodeInfoType));
		memset(&curNode.chordInfo.fingerInfo.Succ, 0, sizeof(nodeInfoType));
		curNode.nodeInfo.ID = (ringSize, curNode.nodeInfo.ID, 1);
		PRINT_ERROR("이미 내 노드의 ID와 동일한 노드가 네트워크에 가입해 있습니다. 다시 시도해보세요.");
		return -1;
	}

	//SuccUpdate
	PRINT_DEBUG("SuccUpdate Request 송신");
	packet = createSuccUpdateRequestMsg(curNode.nodeInfo);
	retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Pre.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("SuccUpdate Request를 보내는 동안 에러가 발생했습니다.");
		return -1;
	}
	SAFE_RELEASE_PACKET(packet);
	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("SuccUpdate Response를 받지 못했습니다.");
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
	}
	SAFE_RELEASE_PACKET(packet);

	//PredUpate
	PRINT_DEBUG("PredUpdate Request 송신");
	packet = createPredUpdateRequestMsg(curNode.nodeInfo);
	//sendChordPacketToClnt(s, packet, &curNode.chordInfo.fingerInfo.Pre.addrInfo);
	retVal = sendto(s, (const char*)packet, sizeof(chordPacketType) + packet->header.bodySize, 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("PredUpdate Request를 보내는 동안 에러가 발생했습니다.");
		return -1;
	}
	SAFE_RELEASE_PACKET(packet);
	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("PredUpdate Response를 받지 못했습니다.");
	}
	packet = createChordPacketFromBuffer(buf, nbyte);
	if (packet == NULL) {
		PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
	}
	SAFE_RELEASE_PACKET(packet);

	PRINT_DEBUG("Stablize_Join 종료.");

	return 0;
}
void stablize_Leave(SOCKET s)
{
	nodeInfoType node, succ_candi;
	chordPacketType *packet = NULL;
	chordPacketType *responsePacket = NULL;
	int retVal, nbyte;
	char buf[BUFSIZE];
	int leaveNodeID = curNode.chordInfo.fingerInfo.Succ.ID;

	if (curNode.nodeInfo.ID == curNode.chordInfo.fingerInfo.Succ.ID) {
		PRINT_DEBUG("현재 노드가 init 상태이므로 stablize leave 과정을 건너 뜁니다.");
		return;
	}
	succ_candi.ID = node.ID = -1;
	PRINT_DEBUG("Stablize_Leave 시작.");

	//LOCK(hMutex); <- 락이 너무 광범위하여 다른 작업에 문제 가능성이 있음. //stablize_leave 과정 중에 다른 작업을 하게 될 경우 잘못될 수 있으므로 전체 락 필요.

	packet = createPredInfoRequestMsg();

	node = curNode.chordInfo.fingerInfo.finger[1]; //finger[1]은 반드시 살아있는 노드여야한다.

	if (node.ID == curNode.nodeInfo.ID || node.ID == -1) { // finger[1]이 초기 노드 상태이거나 초기화된 노드인 경우.
		PRINT_DEBUG("네트워크 전체가 초기화된 노드입니다. Succ 노드를 초기화 합니다.");
		LOCK(hMutex);
		curNode.chordInfo.fingerInfo.Succ = curNode.nodeInfo;
		UNLOCK(hMutex);
	}
	else {
		//PredInfo 메세지를 계속 릴레이 하여 Succ 노드 탐색
		while (node.ID != curNode.chordInfo.fingerInfo.Succ.ID) {
			succ_candi = node; // Succ 후보를 미리 넣어둠
			retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&node.addrInfo, sizeof(struct sockaddr_in));
			if (retVal == -1) {
				PRINT_ERROR("sendto() failed");
				PRINT_ERROR("PredInfo request를 보내는 동안 에러가 발생했습니다.");
				break;
			}

			nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
			if (nbyte < 0) {
				PRINT_ERROR("recvfrom() failed");
				PRINT_ERROR("PredInfo response를 받지 못했습니다.");
				break;
			}
			responsePacket = createChordPacketFromBuffer(buf, nbyte);
			if (packet == NULL) {
				PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
				break;
			}
			node = responsePacket->header.nodeInfo; // PredInfo의 결과를 node에 대입.
			if (node.ID == succ_candi.ID) { // 반환된 노드가 이미 Pred 노드를 제거한 상태라면 반복문 종료.
				break;
			}
			SAFE_RELEASE_PACKET(responsePacket);
		}
		SAFE_RELEASE_PACKET(packet);
		SAFE_RELEASE_PACKET(responsePacket);

		if (node.ID == curNode.chordInfo.fingerInfo.Succ.ID || (node.ID != -1 && node.ID == succ_candi.ID)) { // 해당 조건은 Succ_candi가 찾고자 했던 Succ 노드라는 이야기 이므로...
			curNode.chordInfo.fingerInfo.Succ = succ_candi;

			//PredUpdate 과정으로 Succ 노드의 Pred를 자신으로 변경 시킴.
			packet = createPredUpdateRequestMsg(curNode.nodeInfo);
			retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
			if (retVal == -1) {
				PRINT_ERROR("sendto() failed");
				PRINT_ERROR("PredUpdate request를 보내는 동안 에러가 발생했습니다.");
			}
			else {
				nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
				if (nbyte < 0) {
					PRINT_ERROR("recvfrom() failed");
					PRINT_ERROR("PredUpdate response를 받지 못했습니다.");
				}
				SAFE_RELEASE_PACKET(packet);
				packet = createChordPacketFromBuffer(buf, nbyte);
				if (packet == NULL) {
					PRINT_ERROR("정상적인 구조의 패킷으로 받아들일 수 없는 데이터입니다");
				}
				else if (packet->header.moreInfo != RESPONSE_RESULT_SUCCESS) {
					PRINT_ERROR("PredUpdate request로 실패를 전달 받았습니다.");
				}
			}
			SAFE_RELEASE_PACKET(packet);
		}
	}

	//UNLOCK(hMutex); <- 락이 너무 광범위하여 다른 작업에 문제 가능성이 있음. 

	//추가기능을 위한 NotifyLeaveNode 메시지 전달.
	packet = createNotifyLeaveNodeRequestMsg(curNode.nodeInfo, leaveNodeID, curNode.chordInfo.fingerInfo.Succ);
	retVal = sendto(s, (const char*)packet, sizeof(chordPacketType), 0, (const struct sockaddr *)&curNode.chordInfo.fingerInfo.Succ.addrInfo, sizeof(struct sockaddr_in));
	if (retVal == -1) {
		PRINT_ERROR("sendto() failed");
		PRINT_ERROR("NotifyLeaveNode request를 보내는 동안 에러가 발생했습니다.");
		return;
	}
	SAFE_RELEASE_PACKET(packet);


	nbyte = recvfrom(s, buf, BUFSIZE, 0, NULL, NULL);
	if (nbyte < 0) {
		PRINT_ERROR("recvfrom() failed");
		PRINT_ERROR("NotifyLeaveNode response를 받지 못했습니다.");
	}
	SAFE_RELEASE_PACKET(packet);
	PRINT_DEBUG("Stablize_Leave 종료.");
	return;
}
void initialize()
{
	LOCK(hMutex);

	memset(&curNode.chordInfo.fingerInfo, 0, sizeof(fingerInfoType));

	//Pre와 finger들의 ID를 -1로 전환
	curNode.chordInfo.fingerInfo.Pre.ID = -1;
	for (int i = 0; i < baseM; i++) {
		curNode.chordInfo.fingerInfo.finger[i].ID = -1;
	}

	UNLOCK(hMutex);
}

int move_keys(int keyID, fileInfoType *out_files)
{
	LOCK(hMutex);
	for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
		if (modIn(ringSize, curNode.chordInfo.FRefInfo.fileRef[i].Key, keyID, curNode.nodeInfo.ID, 1, 0)) {
			out_files->fileRef[out_files->fileNum++] = curNode.chordInfo.FRefInfo.fileRef[i];
			async_delRefFile(curNode.chordInfo.FRefInfo.fileRef[i]);
		}
	}
	UNLOCK(hMutex);

	return 0;
}

int sync_addRefFile(fileRefType file)
{
	unsigned int i;
	LOCK(hMutex);

	for (i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
		if (curNode.chordInfo.FRefInfo.fileRef[i].Key == file.Key) {
			PRINT_ERROR("중복되는 Key값을 가진 파일을 add 시도하였습니다.");
			break;
		}
	}
	if (i == curNode.chordInfo.FRefInfo.fileNum)
		curNode.chordInfo.FRefInfo.fileRef[curNode.chordInfo.FRefInfo.fileNum++] = file;

	UNLOCK(hMutex);
	return 0;
}

int async_delRefFile(fileRefType fileInfo)
{
	int ret = 0;
	for (unsigned int i = 0; i < curNode.chordInfo.FRefInfo.fileNum; i++) {
		if (curNode.chordInfo.FRefInfo.fileRef[i].Key == fileInfo.Key && (!strcmp(curNode.chordInfo.FRefInfo.fileRef[i].Name, fileInfo.Name))) {
			//LOCK(hMutex); move_keys의 락 과정과 겹침
			memset(&curNode.chordInfo.FRefInfo.fileRef[i], 0, sizeof(fileRefType));
			if (i != curNode.chordInfo.FRefInfo.fileNum - 1)
				memcpy(&curNode.chordInfo.FRefInfo.fileRef[i], &curNode.chordInfo.FRefInfo.fileRef[i + 1], sizeof(fileRefType)*(curNode.chordInfo.FRefInfo.fileNum - i - 1));
			curNode.chordInfo.FRefInfo.fileNum--;
			ret = 1;
			//UNLOCK(hMutex);

			break;
		}
	}
	return ret;
}
int exists(const char *fname)
{
	FILE *file;
	if ((file = fopen(fname, "r")) != NULL)
	{
		fclose(file);
		return 1;
	}
	return 0;
}