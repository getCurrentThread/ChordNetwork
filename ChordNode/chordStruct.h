#pragma once

#include <winsock2.h>

#define FNameMax 32				/* Max length of File Name */
#define FileMax 32				/* Max number of Files */
#define baseM 6					/* base number */
#define fBufSize 1024			/* file buffer size */

// 새로운 정적 구조체 선언 시작
typedef struct {				/* Node Info Type Structure */
	int ID;							/* ID */
	struct sockaddr_in addrInfo;	/* Socket address */
} nodeInfoType;

typedef struct {				/* File Type Structure */
	char Name[FNameMax];			/* File Name */
	int Key;						/* File Key */
	nodeInfoType owner;				/* Owner's Node */
	nodeInfoType refOwner;			/* Ref Owner's Node */
} fileRefType;

typedef struct {				/* Global Information of Current Files */
	unsigned int fileNum;			/* Number of files */
	fileRefType fileRef[FileMax];	/* The Number of Current Files */
} fileInfoType;

typedef struct {				/* Finger Table Structure */
	nodeInfoType Pre;				/* Predecessor pointer */
	nodeInfoType finger[baseM];		/* Fingers (array of pointers) */
} fingerInfoType;

typedef struct {				/* Chord Information Structure */
	fileInfoType FRefInfo;			/* File Ref Own Information */
	fingerInfoType fingerInfo;		/* Finger Table Information */
} chordInfoType;

typedef struct {				/* Node Structure */
	nodeInfoType nodeInfo;			/* Node's IPv4 Address */
	fileInfoType fileInfo;			/* File Own Information */
	chordInfoType chordInfo;		/* Chord Data Information */
} nodeType;

typedef struct {
	char fileName[FNameMax];
	struct sockaddr_in partnerAddr;
}fileDownInfoType; //파일 다운 관련 구조체