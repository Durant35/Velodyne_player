#include "StdAfx.h"
#include "LCMPlayer.h"
#include <algorithm>
#include <fstream>
#include <Winsock2.h>
#include <assert.h> 

#define MAGIC						((int32_t) 0xEDA1DA01L)
#define	ARBITRARY_START_PORT		10000

LONG LastSocket = ARBITRARY_START_PORT;

static int64_t timestamp_now(void){
	GTimeVal tv;
	g_get_current_time(&tv);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
/*
 *	added by durant35
 *		g_thread 后台线程
 *		  1. lcm_player->NextEvent(); 获取下一个event, 更新 m_currentTime
 *		  2. 同步播放时间与event时间(相对时间): 超前则延迟; 否则直接发送出去
 *		  3. lcm_player->PublishEvent(); 将新的event publish出去
 */
static void* timer_thread_func ( void* user ){
	LCMPlayer * lcm_player = reinterpret_cast<LCMPlayer *> ( user );

	int64_t abstime;
	int64_t now;
	int64_t starttime;
	int64_t startabstime;
	struct timeval sleep_tv;
	int64_t sleep_utime;

	fd_set fds;
	int ret;
		
	startabstime = timestamp_now();
	/*
	 *	added by durant35
	 *		获取下个event，更新 m_currentTime(播放时间/s) 和 m_currentEventNum(播放的event数)
	 */
	lcm_player->NextEvent();

	starttime = lcm_player->m_currentTime;
	while (lcm_player->m_exitThread == 0){
		ret = lcm_player->NextEvent();
		if(ret == -1){	
			TRACE("end of file\n");
			break;
		}
		abstime = lcm_player->m_currentTime - starttime + startabstime;
		if ( abstime < 0 ) 
			return NULL;

		now = timestamp_now();

		if ( abstime > now ){
			sleep_utime = abstime - now;
			sleep_tv.tv_sec = sleep_utime / 1000000;
			sleep_tv.tv_usec = sleep_utime % 1000000;
			TRACE("select %d %d\n",sleep_tv.tv_sec,sleep_tv.tv_usec);
			// sleep until the next timed message, or until an abort message
			
            FD_ZERO (&fds);
			FD_SET(lcm_player->timer_pipe[0], &fds);

			int status = select(lcm_player->timer_pipe[0] + 1, NULL, &fds, NULL, &sleep_tv);
			// select timed out, transmits a message to a multicast group
			if ( 0 == status ){
				lcm_player->PublishEvent();
			}
			else{
				TRACE("0 != status %d %d\n",status,WSAGetLastError());
			}
		}
		else {   
			if(abstime < now)
			    TRACE("abstime < now %ld\n",now-abstime);

			lcm_player->PublishEvent();
		}
	}
	TRACE("Thread stoped! \n");

	return NULL;
}
/*
 *	added by durant35
 *		调用 ntohl(network to host long) 从.pcap文件中获取4 byte数据
 *		两个高 低 两个4 byte组成8 byte的有效数据
 */
static inline int fread32(FILE *f, int32_t *v32){
	int32_t v;
	if (fread(&v, 4, 1, f) != 1)
		return -1;
	*v32 = ntohl(v);
	return 0;
}
static inline int fread64(FILE *f, int64_t *v64){
	int32_t v1, v2;
	if (fread32(f, &v1))
		return -1;
	if (fread32(f, &v2))
		return -1;
	*v64 = (((int64_t)v1) << 32) | (((int64_t)v2) & 0xffffffff);
	return 0;
}

int inet_aton(const char *cp, struct in_addr *inp)
{
	in_addr a;
	a.S_un.S_addr = inet_addr ( cp );
	if ( a.S_un.S_addr == INADDR_NONE )
		return 0;
	*inp = a;
	return a.S_un.S_addr;
}
/*
 *	added by durant35
 *		use loopback to use lcm
 *		filedes[0] = (int) recvSoc;
 *		filedes[1] = (int) sendSoc;
 */
int lcm_internal_pipe_create ( int filedes[2] ){
	int				status, SocOpt, rdyCount, nPort;
	short			Port;
	fd_set          rSSet, wSSet;
	SOCKET			listSoc, sendSoc, recvSoc;
	sockaddr_in     list_addr, recv_addr;
	timeval			tv;

	listSoc = socket ( PF_INET, SOCK_STREAM, 0 );
	recvSoc = socket ( PF_INET, SOCK_STREAM, 0 );

	list_addr.sin_family = AF_INET;
	list_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//	This loop ensures that we pick up an UNUSED port. If anything else used this port,
	//	the entire lcm notification system melts down. The assumption is that we can't bind
	//	to an address in use once the SO_EXCLUSIVEADDRUSE has been set. If this isn't true, 
	//	another method will need to be implemented.
	do{
		nPort = InterlockedIncrement ( &LastSocket );		// Make sure we're using unique port
		if (nPort > 65500){								// Wrapping, reset the port #
			InterlockedCompareExchange ( &LastSocket, ARBITRARY_START_PORT, nPort );
		}
		Port = (short) nPort;
		list_addr.sin_port = htons(Port);

		SocOpt = 1;
		status = setsockopt ( listSoc, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			(const char *) &SocOpt,
			sizeof(SocOpt) );
		if ( status )
			continue;

		status = bind ( listSoc, (LPSOCKADDR) &list_addr, sizeof (list_addr) );

	} while ( status != 0 );

	SocOpt = 1;
	status = ioctlsocket ( listSoc, FIONBIO, (u_long *) &SocOpt );
	status = listen ( listSoc, 1 );

	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	recv_addr.sin_port = htons(Port);
	SocOpt = 1;
	status = ioctlsocket ( recvSoc, FIONBIO, (u_long *) &SocOpt );
	status = connect ( recvSoc, (LPSOCKADDR) &recv_addr, sizeof(recv_addr) );

	rdyCount = 0;
	tv.tv_sec = 0;
	tv.tv_usec = 20 * 1000;
	while ( rdyCount < 2 ){
		rSSet.fd_count = 1;
		rSSet.fd_array[0] = recvSoc;
		rSSet.fd_array[1] = INVALID_SOCKET;
		wSSet.fd_count = 1;
		wSSet.fd_array[0] = listSoc;
		wSSet.fd_array[1] = INVALID_SOCKET;
		rdyCount = select ( 0, &wSSet, &rSSet, NULL, &tv );
	}

	//	Both sockets are ready now to complete the connection.
	sendSoc = accept ( listSoc, (LPSOCKADDR) &list_addr, (int*)&list_addr );
	closesocket ( listSoc );	// This port is now in use - clear listener
	//	Restore the sockets to blocking (default behavior).
	SocOpt = 0;
	status = ioctlsocket ( recvSoc, FIONBIO, (u_long *) &SocOpt );
	status = ioctlsocket ( sendSoc, FIONBIO, (u_long *) &SocOpt );

	filedes[0] = (int) recvSoc;
	filedes[1] = (int) sendSoc;

	return 0;
}

size_t lcm_internal_pipe_write ( int fd, const void *buf, size_t count ){
	int		status;
	status = send ( (SOCKET) fd, (char *) buf, (int) count, 0 );
	return (size_t) status;
}

size_t lcm_internal_pipe_read ( int fd, void *buf, size_t count ){
	int			status;
	status = recv ( (SOCKET) fd, (char *) buf, int (count), 0 );
	return status;
}

int	lcm_internal_pipe_close ( int fd ){
	return closesocket ( (SOCKET) fd );
}
/*
 *	added by durant35
 *		通过文件path 创建lcm_eventlog_t
 */
static lcm_eventlog_t *c_lcm_eventlog_create ( const char *path, const char *mode ){
	/*
	 *	added by durant35
	 *		读写方式
	 */
    assert ( !strcmp ( mode, "r" ) || !strcmp ( mode, "w" ) );
    if ( *mode == 'w' )
        mode = "wb";
    else if ( *mode == 'r' )
        mode = "rb";
    else
        return NULL;

    lcm_eventlog_t* l = ( lcm_eventlog_t* )calloc ( 1, sizeof ( lcm_eventlog_t ) );
    l->f = fopen ( path, mode );
    if ( l->f == NULL ){
        free ( l );
        return NULL;
    }

    l->eventcount = 0;
    return l;
}

static void c_lcm_eventlog_destroy ( lcm_eventlog_t *l ){
    fclose ( l->f );
    free ( l );
}

static void c_lcm_eventlog_free_event ( lcm_eventlog_event_t *le ){
    if ( le->data ) free ( le->data );
    if ( le->channel ) free ( le->channel );
    memset ( le, 0, sizeof ( lcm_eventlog_event_t ) );
    free ( le );
}
/*
 *	added by durant35
 *		从某种格式的文件读取 转换到 lcm_eventlog
 */
static lcm_eventlog_event_t *c_lcm_eventlog_read_next_event ( lcm_eventlog_t *l ){
    lcm_eventlog_event_t* le = ( lcm_eventlog_event_t* ) calloc ( 1, sizeof ( lcm_eventlog_event_t ) );

    int32_t magic = 0;
    int32_t len;

    do{
        int r = fgetc ( l->f );
        if ( r < 0 ){
            goto eof;
        }
        magic = ( magic << 8 ) | r;
    }while ( magic != MAGIC );

    fread64 ( l->f, &le->eventnum );
    fread64 ( l->f, &le->timestamp );
    fread32 ( l->f, &le->channellen );
    fread32 ( l->f, &le->datalen );

    if ( l->eventcount != le->eventnum ){
        l->eventcount = le->eventnum;
    }

    le->channel = ( char* ) calloc ( 1, le->channellen + 1 );
    if ( ( len = fread ( le->channel, 1, le->channellen, l->f ) ) != ( size_t ) le->channellen ){
        TRACE ( "%s %ld\n", strerror ( errno ), ftell ( l->f ) );
        goto eof;
    }

    le->data = calloc ( 1, le->datalen + 1 );
    if ( ( len = fread ( le->data, 1, le->datalen, l->f ) ) != ( size_t ) le->datalen ){
        TRACE ( "%s %ld\n", strerror ( errno ), ftell ( l->f ) );
        goto eof;
    }

    l->eventcount++;
    return le;

eof:
    c_lcm_eventlog_free_event ( le );
    return NULL;
}

LCMPlayer::LCMPlayer ( char *filename )
        : m_totalSecs ( 0 )
        , m_currentTime ( 0 )
        , m_currentEventNum ( 0 )
        , m_StartTime ( 0 )
        , m_totalNum ( 0 )
        , m_exitThread ( 0 ){
    if ( !g_thread_supported () )
        g_thread_init ( NULL );

    this->filename = strdup ( filename );

    thread_created=0;
    speed = 1;
    next_clock_time = -1;
    event = NULL;

	/*
	 *	added by durant35
	 *		use loopback socket to construct internal pipe by lcm
	 */
    if(lcm_internal_pipe_create(timer_pipe) != 0) {
      TRACE("lcm_internal_pipe_create failed\n");
    }

	TRACE("LCMPlayer create\n");
}

LCMPlayer::~LCMPlayer ( void )
{
	if(timer_pipe[0] >= 0)
		lcm_internal_pipe_close(timer_pipe[0]);
	if(timer_pipe[1] >= 0)
		lcm_internal_pipe_close(timer_pipe[1]);

    if ( filename )
        free ( filename );
    if ( event )
        c_lcm_eventlog_free_event ( event );
}
/*
 *	added by durant35
 *		通过LCM内部通讯 glib多线程构建每一周激光点数据包文件索引
 */
int LCMPlayer::Open ( void ){
    assert ( filename != NULL );
    TRACE ( "LCMPlayer::Open %s\n", filename );
	/*
	 *	added by durant35
	 *		create lcm_out & lcm_event_log with lcm API
	 */
    lcm_out = lcm_create ( NULL );
    if ( !lcm_out ){
        TRACE ( "Error: Failed to create LCM\n" );
        return -2;
    }
	/*
	 *	added by durant35
	 *		c_lcm_eventlog_create 与打开的文件相关
	 */
    log = c_lcm_eventlog_create ( filename, "r" );
    if ( !log ){
        TRACE ( "Error: Open %s Failed\n", filename );
        return -1;
    }
	/*
	 *	added by durant35
	 *		构建每一周激光点数据的文件指针索引vector 包括每一周的packet数目 起始时间等
	 */
	if (LoadIndex() != 0){
		MessageBox(NULL, "Finish LoadIndex...\n", "", NULL);
		if (BuildIndex() == 0){
			MessageBox(NULL, "Finish BuildIndex...\n", "", NULL);
			SaveIndex();
			MessageBox(NULL, "Finish SaveIndex...\n", "", NULL);
		}
    }
	/*
	 *	added by durant35
	 *		calculate total seconds
	 */
    m_totalNum = this->m_index.size();
    if ( m_totalNum > 1 ){
        this->m_StartTime = m_index[0].timestamp;
        this->m_totalSecs = ( m_index[m_totalNum-1].timestamp - m_index[0].timestamp ) / 1000000;
        TRACE ( "last %Ld s \n", m_totalSecs );
    }
    return 0;
}
/*
 *	added by durant35
 *		开启 timer_thread_func 线程
 *			参数: reinterpret_cast<LCMPlayer *>
 */
int LCMPlayer::Start ( void ){
    m_exitThread = 0;
    // Start the reader thread
    timer_thread = g_thread_create ( timer_thread_func, this, TRUE, NULL );
    if ( !timer_thread ){
        TRACE ( "Error: LCM failed to start timer thread\n" );
        return -1;
    }
    thread_created = 1;
    return 0;
}

bool idxInfo_lesser(LCM::idxInfo elem1, LCM::idxInfo elem2){
	return elem1.timestamp < elem2.timestamp;
}
/*
 *	added by durant35
 *		将 log->f 文件指针指向 pos 对应时刻的文件offset
 */
int LCMPlayer::SetPos ( int seconds ){
    LCM::idxInfo ii;
    ii.timestamp = m_StartTime + seconds * 1000000;

    vector<LCM::idxInfo>::iterator it = lower_bound ( m_index.begin(), m_index.end(), ii, idxInfo_lesser );
    if ( it != m_index.end() ){
        fseek ( log->f, it->offset, SEEK_SET );
    }
    return 0;
}
/*
 *	added by durant35
 *		gthread: g_thread_join returns the return value
 */
int LCMPlayer::Stop ( void ){
    if ( thread_created ){
        m_exitThread = 1;
        g_thread_join ( timer_thread );
        thread_created = 0;
    }
    return 0;
}
/*
 *	added by durant35
 *		stop timer_thread
 *		lcm_destroy lcm_out
 *		lcm_eventlog_destroy log
 */
int LCMPlayer::Close(void){
	Stop();
	if (lcm_out)
		lcm_destroy(lcm_out);

	if (log)
		c_lcm_eventlog_destroy(log);

	return 0;
}
/*
 *	added by durant35
 *		获取下个event，更新 m_currentTime(播放时间/s) 和 m_currentEventNum(播放的event数)
 */
int LCMPlayer::NextEvent ( ){
    if ( event )
        c_lcm_eventlog_free_event ( event );

    event = c_lcm_eventlog_read_next_event ( log );
    if ( event == NULL )
        return -1;
    this->m_currentTime = event->timestamp - m_StartTime;
    this->m_currentEventNum = event->eventnum;

    return 0;
}
/*
 *	added by durant35
 *		lcm_publish: transmits a message to a multicast group
 */
int LCMPlayer::PublishEvent(void){
	TRACE("%.3f Channel %-20s size %d\n", event->timestamp / 1000000.0, event->channel, event->datalen);
	lcm_publish(lcm_out, event->channel, event->data, event->datalen);
	return 0;
}
int LCMPlayer::FreeEvent(lcm_eventlog_event_t *le){
	return 0;
}
/*
 *	added by durant35
 *		从与.pcap文件相关的索引文件.idx中读取每一周激光点数据包在.pcap文件的起始文件指针 ---> LCM::idxInfo
 *		调用 fseek
 */
int LCMPlayer::LoadIndex(void)
{
	char buffer[sizeof(LCM::idxInfo)];
	int* p = reinterpret_cast<int*> (buffer);
	int len, i;
	char indexname[256];
	LCM::idxInfo ii;

	std::ifstream file;
	sprintf(indexname, "%s.idx", filename);
	file.open(indexname, std::ios::in | std::ios::binary);

	if (file.is_open() == false){
		TRACE("does not exist!!\n");
		return -1;
	}

	// 1. read magic bits for self-constructed .idx file
	file.read(buffer, 4);
	if (buffer[0] != 'I' || buffer[1] != 'D' || buffer[2] != 'X'){
		TRACE("Not an idx file!!\n");
		file.close();
		return -1;
	}
	// 2. read size of vector<LCM::idxInfo> m_index
	file.read(buffer, 4);
	len = *p;
	TRACE("LoadIndex %d\n", len);
	// 3. clear vector<LCM::idxInfo> m_index
	this->m_index.clear();
	// 4. construct idxInfo from .idx file
	for (i = 0; i < len; ++i){
		file.read(reinterpret_cast<char*> (&ii), sizeof(LCM::idxInfo));
		m_index.push_back(ii);
	}

	file.close();
	return 0;
}
/*
 *	added by durant35
 *		读取LCM Log File，构建相对应的文件指针索引 vector<LCM::idxInfo>
 */
int LCMPlayer::BuildIndex ( void ){
	/*
	 *	added by durant35
	 *		lcm 内部通讯
	 *		从与log绑定的file中获取event
	 */
    lcm_eventlog_event_t* rdevent = NULL;
    fseek ( log->f, 0, SEEK_END );
    TRACE ( "BuildIndex size:%ld\n", ftell ( log->f ) );
    fseek ( log->f, 0, SEEK_SET );
    LCM::idxInfo ii;
    ii.offset = 0;

    rdevent = c_lcm_eventlog_read_next_event ( log );
    while ( rdevent ){
        ii.eventnum = rdevent->eventnum;
        ii.timestamp = rdevent->timestamp;
        m_index.push_back ( ii );
        ii.offset = ftell ( log->f );
        c_lcm_eventlog_free_event ( rdevent );
        rdevent = c_lcm_eventlog_read_next_event ( log );
    }

    TRACE ( "BuildIndex over:%d\n", m_index.size() );
    return 0;
}

/*
 *	added by durant35
 *		将 vector<LCM::idxInfo> m_index 中的每周激光点数据索引写入.idx文件
 */
int LCMPlayer::SaveIndex ( void ){
    char buffer[sizeof ( LCM::idxInfo ) ] = {"IDX"};
    int* p = reinterpret_cast<int*> ( buffer );			// p 指向 buffer中的4个字节

    int len, i;
    char indexname[256];

    std::ofstream file;
    sprintf ( indexname, "%s.idx", filename );
    file.open ( indexname, std::ios::out | std::ios::binary );

    // write magic bits(4 bytes)
    file.write ( buffer, 4 );
    // write len(4 bytes)
    p[0] = len = m_index.size();
    file.write ( buffer, 4 );
	// write m_index[i] of structure LCM::idxInfo information
    for ( i = 0;i < len;++i ){
        file.write ( reinterpret_cast<char*> ( &m_index[i] ), sizeof ( LCM::idxInfo )  );
    }

    file.close();
    return 0;
}

