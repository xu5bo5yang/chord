/*
 * chord.c
 *
 *  Created on: Mar 17, 2015
 *      Author: boyangxu
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>

#include "sha1.h"
#include "chord.h"

unsigned int hash(char* key) {
  SHA1Context sha;
  unsigned int result = 0;
  SHA1Reset(&sha);
  SHA1Input(&sha, (const unsigned char*)key, strlen(key));

  if((SHA1Result(&sha)) == 0) {
    printf("fail to compute message digest!\n");
  }
  else {
    result = sha.Message_Digest[0] ^ sha.Message_Digest[1];
    int i=2;
    for(i = 2; i < 5; i++) {
      result = result ^ sha.Message_Digest[i];
    }
  }
  return result;
}

unsigned int get_hash(int port){
	char* str;
	char* strPort;
	str=(char*)malloc(30*sizeof(char));
	strPort=(char*)malloc(20*sizeof(char));
	strcat(str,"127.0.0.1:");
	sprintf(strPort,"%d",port);
	strcat(str,strPort);
	return hash(str);
}

int find(unsigned int hashValue){
	if(hashValue == localNode.key){
		return localNode.port;
	}
	else if((hashValue > localNode.key) && (hashValue <= suc->key)){
		return suc->port;
	}
	else if(((hashValue > localNode.key) || (hashValue < suc->key))
			&& (localNode.key > suc->key)){
		return suc->port;
	}
	else{
		int i;
	    for(i=31;i>=0;i--){
			if(((localNode.key < fingerTable[i].nodeInfo->key) &&
						(fingerTable[i].nodeInfo->key < hashValue))
					||
					((hashValue < localNode.key) &&
							(fingerTable[i].nodeInfo->key > localNode.key))
					||
					((hashValue < localNode.key) &&
							(fingerTable[i].nodeInfo->key < hashValue))){
				int sock;
				struct sockaddr_in server_addr;
				char msg[MAX_MSG_LENGTH], reply[MAX_MSG_LENGTH];
				if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
					perror("Create socket error:");
				    return 0;
				}
				server_addr.sin_addr.s_addr = inet_addr(addr);
				server_addr.sin_family = AF_INET;
				server_addr.sin_port = htons(fingerTable[i].nodeInfo->port);

				if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
					perror("Connect error: ");
					return 0;
				}

				memset(msg,0,sizeof(msg));
				sprintf(msg,"find %u",hashValue);

				if(send(sock, msg, MAX_MSG_LENGTH, 0) < 0){
				    perror("Send error:");
				    return 0;
				}

				memset(reply,0,sizeof(reply));
				if(recv(sock, reply, MAX_MSG_LENGTH, 0) < 0){
					perror("Recv error: ");
					return 0;
				}

				int recvPort;
				recvPort=atoi(reply);
				close(sock);
				return recvPort;
			}
	    }
    }
	return localNode.port;
}

void fix_fingers(int i){
	int result;
	result=find(fingerTable[i].start);
	while(result==0){
		result=find(fingerTable[i].start);
	}
	fingerTable[i].nodeInfo->port=result;
	fingerTable[i].nodeInfo->key=get_hash(fingerTable[i].nodeInfo->port);
}

void notify(){
	int sock;
	struct sockaddr_in server_addr;
	char msg[MAX_MSG_LENGTH];

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Create socket error:");
	    return;
	}
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(suc->port);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		suc->port = suc2->port;
		suc->key = get_hash(suc->port);
		suc2->port = localNode.port;
		suc2->key = get_hash(suc2->port);
		server_addr.sin_port = htons(suc->port);
		reset_pre(suc->port);
		if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
			perror("Connect error:");
			return;
		}
	}

	memset(msg,0,sizeof(msg));
	sprintf(msg,"notify %d",localNode.port);

	if(send(sock, msg, MAX_MSG_LENGTH, 0) < 0){
	    perror("Send error:");
	    return;
	}
	close(sock);
}

void stabilize(){
	int sock;
	struct sockaddr_in server_addr;
	char msg[MAX_MSG_LENGTH], reply[MAX_MSG_LENGTH];

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Create socket error:");
	    return;
	}
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(suc->port);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		suc->port = suc2->port;
		suc->key = get_hash(suc->port);
		suc2->port = localNode.port;
		suc2->key = get_hash(suc2->port);
		server_addr.sin_port = htons(suc->port);
		reset_pre(suc->port);
		if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
			perror("Connect error:");
			return;
		}
	}

	memset(msg,0,sizeof(msg));
	sprintf(msg,"stable %d",localNode.port);

	if(send(sock, msg, MAX_MSG_LENGTH, 0) < 0){
	    perror("Send error:");
	    return;
	}

	memset(reply,0,sizeof(reply));
	if(recv(sock, reply, MAX_MSG_LENGTH, 0) < 0){
		perror("Recv error: ");
		return;
	}

	int recvPort;
	unsigned int recvHash;
	recvPort=atoi(reply);
	recvHash=get_hash(recvPort);
	if(recvPort>0){
	    if((suc->key > localNode.key) && (localNode.key < recvHash) && (recvHash < suc->key)){
	    	suc->port = recvPort;
	    	suc->key = get_hash(suc->port);
	    }
	    else if((suc->key < localNode.key) && (recvPort!=localNode.port) &&
	    		((recvHash > localNode.key) || (recvHash < suc->key))){
	    	suc->port = atoi(reply);
	    	suc->key = get_hash(suc->port);
	    }
	}
	close(sock);
}

void reset_pre(int port){
	int sock;
	struct sockaddr_in server_addr;
	char msg[MAX_MSG_LENGTH];

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Create socket error:");
	    return;
	}
	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Connect error in reset:");
		return;
	}

	memset(msg,0,sizeof(msg));
	sprintf(msg,"reset-pre");

	if(send(sock, msg, MAX_MSG_LENGTH, 0) < 0){
	    perror("Send error:");
	    return;
	}
	close(sock);
}

void keep_alive(){
	if(pre!=NULL){
		int sock;
		struct sockaddr_in server_addr;
		char msg[MAX_MSG_LENGTH];

		if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
			perror("Create socket error:");
		    return;
		}
		server_addr.sin_addr.s_addr = inet_addr(addr);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(pre->port);
		if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
			free(pre);
			pre=NULL;
			perror("Connect error in keep_alive: ");
			return;
		}

		memset(msg,0,sizeof(msg));
		sprintf(msg,"keep-alive %d",suc->port);

		if(send(sock, msg, MAX_MSG_LENGTH, 0) < 0){
		    perror("Send error:");
		    return;
		}
		close(sock);
	}
}

void* update(){
	int i;
	while(1){
		for(i=0;i<32;i++){
			fix_fingers(i);
		}
		sleep(1);
		stabilize();
		notify();
		keep_alive();
	}

	pthread_exit(0);
}

void* command(){
	char* cmd;
	cmd=(char*)malloc(MAX_MSG_LENGTH*sizeof(char));
	memset(cmd,0,MAX_MSG_LENGTH);
	while(strcmp(cmd,"kill")){
		fscanf(stdin,"%s",cmd);
	}
	exit(0);
}

void* chordNode(void* sock){
	char msg[MAX_MSG_LENGTH];
	char reply[MAX_MSG_LENGTH];
	int result;
	char* cmd;
	cmd=(char*)malloc(30*sizeof(char));
	char* token=" ";
	int rqst=*(int*)sock;
	free(sock);
	while((recv(rqst,msg,sizeof(msg),0))>0){
    	cmd=strtok(msg,token);
    	if(!strcmp(cmd,"join")){
    		cmd=strtok(NULL,token);
    		pcNode n;
    		n=(pcNode)malloc(sizeof(cNode));
    		n->port=atoi(cmd);
    		n->key=get_hash(n->port);
    		if(suc->port==localNode.port){
    			suc->port=n->port;
        		suc->key=get_hash(suc->port);
   	    		pre->port=n->port;
   	    		pre->key=get_hash(pre->port);
   	    		result=localNode.port;
    		}
    		else {
    			result=find(n->key);
    			while(result==0){
    				result=find(n->key);
    			}
    		}
	    	memset(reply,0,MAX_MSG_LENGTH);
	    	sprintf(reply, "%d", result);
	    	if(send(rqst, reply, MAX_MSG_LENGTH, 0) < 0){
	    		perror("Send error: ");
	    		return NULL;
	    	}
    	}
    	else if(!strcmp(cmd,"find")){
    		cmd=strtok(NULL,token);
    		unsigned int findHash;
    		findHash=atoi(cmd);
    		result=find(findHash);
    		while(result==0){
    			result=find(findHash);
    		}
    		sprintf(reply,"%d",result);
    		if(send(rqst, reply, MAX_MSG_LENGTH, 0) < 0){
    			perror("Send error: ");
        		return NULL;
    	   	}
    	}
    	else if(!strcmp(cmd,"query")){
    		while((recv(rqst, msg, MAX_MSG_LENGTH,0))>0){
    			unsigned int mHash;
    			mHash = hash(msg);
    			result = find(mHash);
    			while(result==0){
    				result = find(mHash);
    			}
    			sprintf(reply,"%d",result);
    			if(send(rqst, reply, MAX_MSG_LENGTH, 0) < 0){
    				perror("Send error: ");
    				return NULL;
    			}
    		}
    	}
    	else if(!strcmp(cmd,"stable")){
    		memset(reply,0,MAX_MSG_LENGTH);
    		if(pre==NULL){
    			sprintf(reply,"0");
    		}
    		else{
    			sprintf(reply,"%d",pre->port);
    		}
    		if(send(rqst, reply, MAX_MSG_LENGTH, 0) < 0){
    			perror("Send error: ");
    			return NULL;
    	    }
    		break;
    	}
    	else if(!strcmp(cmd,"notify")){
    		cmd=strtok(NULL,token);
    		result=atoi(cmd);
    		if(pre==NULL){
    			pre=(pcNode)malloc(sizeof(cNode));
    			pre->port=result;
    			pre->key=get_hash(pre->port);
    		}
    		else if((pre->key < localNode.key) && (pre->key < get_hash(result)) && (get_hash(result) < localNode.key)){
    			pre->port=result;
    			pre->key=get_hash(pre->port);
    		}
    		else if((pre->key > localNode.key) &&
    				((get_hash(result) > pre->key) || (get_hash(result) < localNode.key))){
    			pre->port=result;
    			pre->key=get_hash(pre->port);
    		}
    	}
    	else if(!strcmp(cmd,"keep-alive")){
    		cmd=strtok(NULL,token);
    		result=atoi(cmd);
    		suc2->port=result;
    		suc2->key=get_hash(suc2->port);
    	}
    	else if(!strcmp(cmd,"reset-pre")){
    		free(pre);
    		pre=NULL;
    	}
    	memset(msg,0,sizeof(msg));
	}
	close(rqst);
	pthread_exit(0);
}

int main(int argc, char* argv[]){
	void* self;
	localNode.key=0;
	localNode.port=0;
	suc=(pcNode)malloc(sizeof(cNode));
	suc2=(pcNode)malloc(sizeof(cNode));
	pre=NULL;

	char* cmd;
	cmd=(char*)malloc(50*sizeof(char));
	char* part;
	part=(char*)malloc(50*sizeof(char));
	char* token = " ";
	int joinPort;

	printf("Input command:\n");
	fgets(cmd,512,stdin);
	part=strtok(cmd, token);
	part=strtok(NULL, token);
	localNode.port=atoi(part);
	localNode.key=get_hash(localNode.port);
	part=strtok(NULL, token);
	int i;
	for(i=0;i<32;i++){
		fingerTable[i].start=localNode.key+pow(2,i);
		fingerTable[i].nodeInfo=(pcNode)malloc(sizeof(cNode));
		fingerTable[i].nodeInfo->port=localNode.port;
		fingerTable[i].nodeInfo->key=get_hash(fingerTable[i].nodeInfo->port);
	}
	suc2->port=localNode.port;
	suc2->key=get_hash(suc2->port);

	if(part==NULL){
		newNode();
	}
	else{
		part=strtok(NULL, token);
		joinPort=atoi(part);
		joinNode(joinPort);
	}

	int sock;
	int rqst;
	socklen_t sockLen;
	struct sockaddr_in my_addr;
	struct sockaddr_in client_addr;
	sockLen = sizeof(client_addr);
	pthread_t pthreadID;

	memset((char*)&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_addr.s_addr = INADDR_ANY;
	my_addr.sin_port = htons(localNode.port);

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("cannot create socket: ");
		return 1;
	}
	if((bind(sock, (struct sockaddr *)&my_addr, sizeof(my_addr))) < 0){
		perror("bind failed: ");
		return 1;
	}
	if(listen(sock, 5) < 0){
		perror("listen failed: ");
		return 1;
	}

	pthread_create(&pthreadID,NULL,update,NULL);

	pthread_create(&pthreadID,NULL,command,NULL);

	pthread_create(&pthreadID,NULL,print_node,NULL);

	while(1){
    	if((rqst=accept(sock, (struct sockaddr *)&client_addr, &sockLen)) < 0){
    		perror("accept failed: ");
    		continue;
    	}
    	int* pSock;
    	pSock=(int*)malloc(sizeof(int));
    	*pSock=rqst;
    	pthread_create(&pthreadID,NULL,chordNode,(void*)pSock);
    	pthread_join(pthreadID,&self);
	}

	return 0;
}

void newNode(){
	pre=(pcNode)malloc(sizeof(cNode));
	suc->key=localNode.key;
	suc->port=localNode.port;
	pre->key=localNode.key;
	pre->port=localNode.port;
}

void joinNode(int joinPort){
	char msg[MAX_MSG_LENGTH], reply[MAX_MSG_LENGTH];
	memset(msg,0,sizeof(msg));
	memset(reply,0,sizeof(msg));

	printf("Joining the Chord ring.\n");
	sprintf(msg,"join %d",localNode.port);
	int sock;
	struct sockaddr_in server_addr;

	if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("Create socket error: ");
		return;
	}

	server_addr.sin_addr.s_addr = inet_addr(addr);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(joinPort);

	if(connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Connect error in joinNode: ");
	    return;
	}

	if(send(sock, msg, sizeof(msg),0) < 0){
		perror("Send error: ");
		return;
	}

	fflush(stdin);
    if((read(sock, reply, MAX_MSG_LENGTH)) < 0){
    	perror("Recv error: ");
	   	return;
	}
    suc->port=atoi(reply);
	suc->key=get_hash(suc->port);

	close(sock);
}

void* print_node(){
	while(1){
		sleep(5);
		if(pre==NULL) continue;
		printf("You are listening on port %d.\n", localNode.port);
		printf("Your position is 0x%X.\n", localNode.key);
		printf("Your predecessor is node 127.0.0.1, port %d, position 0x%X.\n", pre->port, pre->key);
		printf("Your successor is node 127.0.0.1, port %d, position 0x%X.\n", suc->port, suc->key);
		/*for(i=0;i<32;i++){
			printf("\tFingerTable[%d] is 0x%X | %d\n", i, fingerTable[i].start, fingerTable[i].nodeInfo->port);
		}*/
		printf("Input 'kill' to close the node.\n");
	}
}
