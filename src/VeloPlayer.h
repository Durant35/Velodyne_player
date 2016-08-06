#pragma once

#include <glib.h>
#include <fstream>
#include <vector>
#include <WinSock2.h>
#include "velodyneDriver.h"

#pragma pack(1)
using namespace std;

namespace VELO{
	class idxInfo{
	public:
		struct timeval ts;			// pcap抓包时间戳
		int PktNumber;				// 每一帧激光点数据packet数目
		int offset;					// 每个pcap抓包在.pcap文件中的文件指针偏移量
	};
}

class VeloPlayer{
private:
	// fileHandle 打开的.pcap文件文件索引
	char* filename;
	std::ifstream fileHandle;

	WSADATA wsa;							/* Used to open Windows connection */
	SOCKET sockfd;	

	int m_port_number;				        /* The port number to use */

											/* The socket descriptor */
	int server_length;						/* Length of server struct */

	struct sockaddr_in server;				/* Information about the server */
	struct sockaddr_in client;				/* Information about the client */

	int a1, a2, a3, a4;						/* Server address components in xxx.xxx.xxx.xxx form */
	int b1, b2, b3, b4;						/* Client address components in xxx.xxx.xxx.xxx form */
	
	VelodyneDriver velodyneDriver;
public:
	vector<VELO::idxInfo> m_index;

	VeloPlayer(const char *filename);
	~VeloPlayer(void);

	int Open(void);
	int SetPos(int seconds);
	int Start(void);
	int Stop(void);
	int Close(void);

	int BuildIndex(void);
	int SaveIndex(void);
	int LoadIndex(void);
	
	int NextEvent();
	int LastEvent();
	int PublishEvent(void);

	double speed;
    int64_t next_clock_time;
    int m_exitThread;
    int thread_created;
    GThread *timer_thread;

	int64_t m_totalSecs;
	struct timeval m_currentTime;
	struct timeval m_startTime;
	int64_t m_currentFrameNum;
	int64_t m_totalFrameNum;
};

