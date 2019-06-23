#include "chordMsg.h"

chordHeaderType * createEmptyChordHeader()
{
	chordHeaderType* header = malloc(sizeof(chordHeaderType));
	memset(header, 0, sizeof(chordHeaderType));
	return header;
}

void releaseChordHeader(chordHeaderType * header)
{
	free(header);
	return;
}

void setMessageIDandType(chordPacketType * ref_packet, unsigned short msgID, unsigned short msgType)
{
	ref_packet->header.msgID = msgID;
	ref_packet->header.msgType = msgType;
}

chordPacketType* createPingPongRequestMsg() {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PINGPONG, MSGTYPE_REQUEST);
	return packet;
}

chordPacketType* createPingPongResponseMsg() {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PINGPONG, MSGTYPE_RESPONSE);
	return packet;
}

chordPacketType * createJoinInfoRequestMsg(nodeInfoType curNodeInfo)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_JOININFO, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	return packet;
}

chordPacketType * createJoinInfoResponseMsg(nodeInfoType succNode, short result)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_JOININFO, MSGTYPE_RESPONSE);
	packet->header.nodeInfo = succNode;
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType * createMovekeysRequestMsg(nodeInfoType curNodeInfo)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_MOVEKEYS, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	return packet;
}

chordPacketType * createMoveKeysResponseMsg(short keysCount, unsigned int keyInfoBytes, BYTE keysInfo[])
{
	chordPacketType *packet = malloc(sizeof(chordPacketType) + keyInfoBytes);
	memset(packet, 0, sizeof(chordPacketType) + keyInfoBytes);
	setMessageIDandType(packet, MSGID_MOVEKEYS, MSGTYPE_RESPONSE);
	packet->header.moreInfo = keysCount;
	packet->header.bodySize = keyInfoBytes;
	if(keyInfoBytes != 0){
		memcpy(packet->body, keysInfo, keyInfoBytes);
	}
	return packet;
}

chordPacketType* createPredInfoRequestMsg() {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PREDINFO, MSGTYPE_REQUEST);
	return packet;
}

chordPacketType* createPredInfoResponseMsg(nodeInfoType PredNodeInfo, short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PREDINFO, MSGTYPE_RESPONSE);
	packet->header.nodeInfo = PredNodeInfo;
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createPredUpdateRequestMsg(nodeInfoType curNodeInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PREDUPDATE, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	return packet;
}

chordPacketType* createPredUpdateResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_PREDUPDATE, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createSuccInfoRequestMsg() {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_SUCCINFO, MSGTYPE_REQUEST);
	return packet;
}

chordPacketType* createSuccInfoResponseMsg(nodeInfoType mySuccNodeInfo, short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_SUCCINFO, MSGTYPE_RESPONSE);
	packet->header.nodeInfo = mySuccNodeInfo;
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createSuccUpdateRequestMsg(nodeInfoType curNodeInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_SUCCUPDATE, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	return packet;
}

chordPacketType* createSuccUpdateResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_SUCCUPDATE, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createFindPredRequestMsg(int targetID) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FINDPRED, MSGTYPE_REQUEST);
	packet->header.nodeInfo.ID = targetID;
	return packet;
}

chordPacketType* createFindPredResponseMsg(nodeInfoType targetPredNodeInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FINDPRED, MSGTYPE_RESPONSE);
	packet->header.nodeInfo = targetPredNodeInfo;
	return packet;
}

chordPacketType* createLeaveKeysRequestMsg(short keysCount, unsigned int keyInfoBytes, BYTE keysInfo[]) {
	chordPacketType *packet = malloc(sizeof(chordPacketType) + keyInfoBytes);
	memset(packet, 0, sizeof(chordPacketType) + keyInfoBytes);
	setMessageIDandType(packet, MSGID_LEAVEKEYS, MSGTYPE_REQUEST);
	packet->header.moreInfo = keysCount;
	packet->header.bodySize = keyInfoBytes;
	memcpy(packet->body, keysInfo, keyInfoBytes);
	return packet;
}

chordPacketType* createLeaveKeysResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_LEAVEKEYS, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createFileRefAddRequestMsg(nodeInfoType curNodeInfo, fileRefType fileRefInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFADD, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	packet->header.fileInfo = fileRefInfo;
	return packet;
}

chordPacketType* createFileRefAddResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFADD, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createFileRefDelRequestMsg(nodeInfoType curNodeInfo, fileRefType fileRefInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFDEL, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	packet->header.fileInfo = fileRefInfo;
	return packet;
}

chordPacketType* createFileRefDelResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFDEL, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType * createFileDownRequestMsg(fileRefType fileRefInfo)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEDOWN, MSGTYPE_REQUEST);
	packet->header.fileInfo = fileRefInfo;
	return packet;
}

chordPacketType * createFileDownResponseMsg(short result)
{
	int bodySize = sizeof(chordPacketType) + sizeof(unsigned short); 
	chordPacketType *packet = malloc(bodySize);
	memset(packet, 0, bodySize);
	setMessageIDandType(packet, MSGID_FILEDOWN, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType* createFileRefInfoRequestMsg(int key) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFINFO, MSGTYPE_REQUEST);
	packet->header.fileInfo.Key = key;
	return packet;
}

chordPacketType* createSuccessFileRefInfoResponseMsg(short result, fileRefType fileRefInfo) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFINFO, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	packet->header.fileInfo = fileRefInfo;
	return packet;
}
chordPacketType* createFailureFileRefInfoResponseMsg(short result) {
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_FILEREFINFO, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

chordPacketType * createNotifyLeaveNodeRequestMsg(nodeInfoType curNodeInfo, int leaveNodeID, nodeInfoType newSuccNodeInfo)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_NOTIFYLEAVE, MSGTYPE_REQUEST);
	packet->header.nodeInfo = curNodeInfo;
	packet->header.moreInfo = leaveNodeID;
	packet->header.fileInfo.owner = newSuccNodeInfo;

	return packet;
}

chordPacketType * createNotifyLeaveNodeResponseMsg(short result)
{
	chordPacketType *packet = malloc(sizeof(chordPacketType));
	memset(packet, 0, sizeof(chordPacketType));
	setMessageIDandType(packet, MSGID_NOTIFYLEAVE, MSGTYPE_RESPONSE);
	packet->header.moreInfo = result;
	return packet;
}

void releaseChordPacket(chordPacketType * packet)
{
	free(packet);
	return;
}

chordPacketType * createChordPacketFromBuffer(BYTE * buffer, unsigned int nbyte)
{
	chordHeaderType *header = createEmptyChordHeader();
	chordPacketType *packet = NULL;

	if (nbyte >= sizeof(chordHeaderType))
		memcpy(header, buffer, sizeof(chordHeaderType));

	if (header->bodySize != 0) { //버퍼에 body가 있다면...
		if ((header->bodySize + sizeof(chordHeaderType)) <= nbyte) {
			packet = malloc(header->bodySize + sizeof(chordHeaderType));
			memset(packet, 0, sizeof(chordPacketType) + header->bodySize);
			memcpy(packet, header, sizeof(chordHeaderType));
			memcpy(packet->body, buffer + (header->bodySize), header->bodySize);
		}
		else {
			perror("수신한 패킷의 bodySize가 실제 수신 byte보다 큽니다.\n");
		}
	}
	else {   //body가 없다면...헤더만으로 packet 만들기.
		packet = (chordPacketType*)header;
		header = NULL;
	}

	releaseChordHeader(header);
	return packet;
}
