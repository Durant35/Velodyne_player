#pragma once
// #include <stdint.h> in lcm.h
#include "lcm.h"
#include <vector>
#include <glib.h>

using namespace std;

namespace LCM{
	class idxInfo{
	public:
		int64_t eventnum;			// packet数目
		int64_t timestamp;
		long offset;				// 在.pcap文件中起始文件offset
	};
}

class LCMPlayer{
private:
	char* filename;
	lcm_t* lcm_out;
    
    int8_t writer;

    lcm_eventlog_t * log;
    
	vector<LCM::idxInfo> m_index;

	int FreeEvent(lcm_eventlog_event_t *le);
public:
	LCMPlayer(char *filename);
	~LCMPlayer(void);

    lcm_eventlog_event_t* event;
    double speed;
    int64_t next_clock_time;
    int m_exitThread;
    int thread_created;
    GThread *timer_thread;
	int64_t m_StartTime;
    int timer_pipe[2];

	int Open(void);
	int Start(void);
	int SetPos(int seconds);
	int Stop(void);
	int Close(void);

	int BuildIndex(void);
	int SaveIndex(void);
	int LoadIndex(void);

	int64_t m_totalSecs;
	int64_t m_currentTime;
	int64_t m_currentEventNum;
	int64_t m_totalNum;

	int NextEvent();
	int PublishEvent(void);
};

