#pragma once

SOCKET flSock, fsSock, frSock;
int recvn(SOCKET s, char *buf, int len, int flags);
void err_quit(char *msg);
void err_display(char *msg);

int fileSender(char filename[], struct sockaddr_in Addr,struct sockaddr_in curAddr);
int fileReceiver(struct sockaddr_in Addr);