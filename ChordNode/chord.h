#pragma once

#include "chordStruct.h"
#include "chordMsg.h"


#define nodefmax 10				/* Max number of files in a node */
#define FDataMax 64				/* Max length of File Data */

//새로운 매크로 상수 선언 시작
#define FFLUSH()  while(getchar() != '\n')
#define UNUSED(x) (void)(sizeof(x), 0)

#define LOCK(a) WaitForSingleObject(a, INFINITE);
#define UNLOCK(a) ReleaseMutex(a);
//새로운 매크로 상수 선언 종료

struct CMD {
	char *name;
	char *desc;
	int(*cmd)(int argc, char *argv[]);
};

int cmdProcessing(void);
int cmd_create(int argc, char *argv[]);
int cmd_join(int argc, char *argv[]);
int cmd_leave(int argc, char *argv[]);
int cmd_add(int argc, char *argv[]);
int cmd_delete(int argc, char *argv[]);
int cmd_search(int argc, char *argv[]);
int cmd_finger(int argc, char *argv[]);
int cmd_info(int argc, char *argv[]);
int cmd_mute(int argc, char *argv[]);
int cmd_help(int argc, char *argv[]);
int cmd_quit(int argc, char *argv[]);

unsigned int WINAPI procWorker(void * arg);
unsigned int WINAPI procRecvMsg(void* arg);
unsigned int WINAPI procPPandFF(void* arg);
unsigned int WINAPI procFileSender(void* arg);
unsigned int WINAPI procFileReceiver(void* arg);

#define PRINT_INFO(fmt, ...) fprintf(stdout, "[INFO]%s" fmt "\n", name, __VA_ARGS__)
#define PRINT_ERROR(fmt, ...) fprintf(stderr, "[ERROR]%s" fmt "\n", name, __VA_ARGS__)

//#define NEED_ALTER_DEBUG - 자세한 디버그가 필요한 경우에만 사용.
#ifdef NEED_ALTER_DEBUG
	#define PRINT_DEBUG(fmt, ...) if(!silentMode)fprintf(stdout, "[DEBUG]%s-%s(%d Line.)" fmt "\n", __func__ ,name, __LINE__, __VA_ARGS__)
#endif
#ifndef NEED_ALTER_DEBUG
	#define PRINT_DEBUG(fmt, ...) if(!silentMode)fprintf(stderr, "[DEBUG]%s" fmt "\n", name, __VA_ARGS__)
#endif

#define Succ finger[0]
#define SAFE_RELEASE_PACKET(a) releaseChordPacket(a);a=NULL


//새로운 함수 선언 종료


int init();
int release();
/*
int fsInit();
int frInit();
int fsClose();
int frClose();
*/
// finger[1] ~ finger[m-1]의 노드를 갱신함.
int fix_finger(SOCKET s);
// 해당 id의 Succ를 반환함. (실패 시 노드ID : -1 노드반환)
nodeInfoType find_successor(SOCKET s, int keyID);
// 해당 id의 Pred를 반환함. (실패 시 노드ID : -1 노드반환)
nodeInfoType find_predecessor(SOCKET s, int keyID);
//내가 가지고 있는 finger 정보 중 가장 Pred에 근접한 후보를 반환함. 
nodeInfoType find_closest_predecessor(int keyID);
//해당 key의 파일을 가지고 있는 노드를 반환함. (실패 시 노드ID : -1 노드반환)
//nodeInfoType lookup(SOCKET s, int keyID);

//finger[1] ~ finger[m]까지의 노드가 살아있지 않으면 해당 노드 갱신.
void ping_pong(SOCKET s, char buf[]);
// 해당 노드가 Succ 정보를 갱신한 후에 실행. Pred와 Succ 와의 stabilize 과정 진행. rqSock으로 진행
int stabilize_Join(SOCKET s);
// 해당 노드의 Succ노드가 떠난 경우 실행, PredInfo 과정을 중첩하여 Succ 노드를 갱신.
void stablize_Leave(SOCKET s);

// Pred노드 정보와 finger 테이블을 모두 초기화 함.
void initialize();

//해당 노드 ID에 해당하는 file 정보를 지우면서 . (성공 시에는 자신 노드의 참조 키를 가져오고 0을 반환, 실패 시 -1 반환)
int move_keys(int keyID, fileInfoType *out_files);

//해당 참조 파일을 추가합니다. (성공 시 0, 실패 시 -1 반환)
int sync_addRefFile(fileRefType file);
//해당 참조 파일을 삭제합니다. (성공 시 0, 실패 시 -1 반환)
int async_delRefFile(fileRefType fileInfo);
//실제 파일이 존재하는지 확인하는 함수 (성공 시 1, 없을 시 0 반환)
int exists(const char *fname);
