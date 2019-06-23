#pragma once

#include "chordStruct.h"

#define MSGID_PINGPONG		0
#define MSGID_JOININFO		1
#define MSGID_MOVEKEYS		2
#define MSGID_PREDINFO		3
#define MSGID_PREDUPDATE	4
#define MSGID_SUCCINFO		5
#define MSGID_SUCCUPDATE	6
#define MSGID_FINDPRED      7
#define MSGID_LEAVEKEYS		8
#define MSGID_FILEREFADD	9
#define MSGID_FILEREFDEL	10
#define MSGID_FILEDOWN		11
#define MSGID_FILEREFINFO	12
#define MSGID_NOTIFYLEAVE	13

#define MSGTYPE_REQUEST		0
#define MSGTYPE_RESPONSE	1

#define RESPONSE_RESULT_SUCCESS	0
#define RESPONSE_RESULT_FAILURE	-1

#define MSGID_AND_TYPE(id, type) (((unsigned int)(id) << 16) + (type))

typedef struct {         // 메세지 구조 설계
	unsigned short	msgID;		// message ID
	unsigned short	msgType;	// message type (0: request, 1: response)
	nodeInfoType	nodeInfo;	// node address info
	short			moreInfo;	// more info
	fileRefType		fileInfo;	// file (reference) info
	unsigned int	bodySize;	// body size in Bytes
} chordHeaderType;		// CHORD message header type

typedef struct {
	chordHeaderType header;
	BYTE body[];
} chordPacketType;

typedef struct {
	struct sockaddr_in senderInfo;	/*패킷을 보낸 사람 정보*/
	chordPacketType *packet;		/*패킷 정보*/
}RecvMsgDataType;


chordHeaderType* createEmptyChordHeader();
void releaseChordHeader(chordHeaderType *header);

void setMessageIDandType(chordPacketType *ref_packet, unsigned short msgID, unsigned short msgType);

chordPacketType* createPingPongRequestMsg();
chordPacketType* createPingPongResponseMsg();

chordPacketType* createJoinInfoRequestMsg(nodeInfoType curNodeInfo);
chordPacketType* createJoinInfoResponseMsg(nodeInfoType succNode, short result);

chordPacketType* createMovekeysRequestMsg(nodeInfoType curNodeInfo);
chordPacketType* createMoveKeysResponseMsg(short keysCount, unsigned int keyInfoBytes, BYTE keysInfo[]);
//PredInfo -> 타겟 노드의 Predessor를 알기위한 메세지
chordPacketType* createPredInfoRequestMsg();
chordPacketType* createPredInfoResponseMsg(nodeInfoType PredNodeInfo, short result);
//PredUpdate -> 수신 받는 노드에게 이 노드의 정보로 Pred를 변경하라 하기위한 메세지
chordPacketType* createPredUpdateRequestMsg(nodeInfoType curNodeInfo);
chordPacketType* createPredUpdateResponseMsg(short result);
//SuccInfo -> 해당 노드의 Succ 노드 정보를 받아오기 위한 메세지
chordPacketType* createSuccInfoRequestMsg();
chordPacketType* createSuccInfoResponseMsg(nodeInfoType mySuccNodeInfo, short result);
//SuccUpdate -> 수신 받는 노드에게 이 노드의 정보로 Succ를 변경하라 하기위한 메세지
chordPacketType* createSuccUpdateRequestMsg(nodeInfoType curNodeInfo);
chordPacketType* createSuccUpdateResponseMsg(short result);
//FindPred -> 해당하는 ID의 Predcessor를 찾기 위한 메세지
chordPacketType* createFindPredRequestMsg(int targetID);
chordPacketType* createFindPredResponseMsg(nodeInfoType targetPredNodeInfo);
//LeaveKeys -> 본인이 나감을 알림과 동시에, 모든 key 정보를 Succ에게 양도
chordPacketType* createLeaveKeysRequestMsg(short keysCount, unsigned int keyInfoBytes, BYTE keysInfo[]);
chordPacketType* createLeaveKeysResponseMsg(short result);
//FileRefAdd -> 파일 참조 정보를 추가하기 위한 메세지
chordPacketType* createFileRefAddRequestMsg(nodeInfoType curNodeInfo, fileRefType fileRefInfo);
chordPacketType* createFileRefAddResponseMsg(short result);
//FileRefDel -> 파일 참조 정보를 삭제하기 위한 메세지
chordPacketType* createFileRefDelRequestMsg(nodeInfoType curNodeInfo, fileRefType fileRefInfo);
chordPacketType* createFileRefDelResponseMsg(short result);
//FileDown -> 파일 다운로드 준비를 위한 메세지
chordPacketType* createFileDownRequestMsg(fileRefType fileRefInfo);
chordPacketType* createFileDownResponseMsg(short result);
//FileRefInfo -> 파일 참조 정보를 받아오기 위한 메세지
chordPacketType* createFileRefInfoRequestMsg(int key);
chordPacketType* createSuccessFileRefInfoResponseMsg(short result, fileRefType fileRefInfo);
chordPacketType* createFailureFileRefInfoResponseMsg(short result);
////해당 헤더를 읽은 후, Body가 존재하면 반영하여 이를 chordPacketType으로 변환해줍니다.
//chordPacketType* createChordPacketFromChordHeader(const chordHeaderType * const in_header, SOCKET* ref_sock);
////소켓으로부터 읽어들여 packetType으로 만듦.
//chordPacketType* createPacketFormSocket(SOCKET *ref_sock);
// 빠져나간 노드에 대해 네트워크 전역에 알림
chordPacketType* createNotifyLeaveNodeRequestMsg(nodeInfoType curNodeInfo, int leaveNodeID, nodeInfoType newSuccNodeInfo);
chordPacketType* createNotifyLeaveNodeResponseMsg(short result);

//버퍼로부터 패킷 하나를 생성
chordPacketType* createChordPacketFromBuffer(BYTE* buffer, unsigned int nbyte);

void releaseChordPacket(chordPacketType *packet);