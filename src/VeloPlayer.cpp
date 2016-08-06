#include "StdAfx.h"
#include "VeloPlayer.h"
#include "pcap.h"
#include <algorithm>

#define MAGIC					((int32_t) 0xEDA1DA01L)
#define	ARBITRARY_START_PORT	11000

static LONG	LastSocket = ARBITRARY_START_PORT;
/*
 *	added by durant35
 *		Debug function: message box for print tempory data
 */
void printFunc(const char* info, int status){
	CString tmp = " = ";
	CString str;
	str.Format("%d", status);
	MessageBox(NULL, info + tmp + str, "", NULL);
}
/*
 *	added by durant35
 *		Glib time control functions
 *		struct timeval: glib Structure used in select() call
 */
static inline int64_t timevalToInt64(struct timeval tv){
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
static int64_t timestamp_now(void){
	GTimeVal tv;
	g_get_current_time(&tv);
	return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}
struct timeval timersub(struct timeval t1, struct timeval t2){
	struct timeval t3;
	t3.tv_sec = t1.tv_sec - t2.tv_sec;
	t3.tv_usec = t1.tv_usec - t2.tv_usec;
	if (t1.tv_usec - t2.tv_usec < 0){
		t3.tv_usec--;
		t3.tv_usec += 1000000;
	}
	return t3;
}
/*
 *	added by durant35
 *		��̬�����ᱻ�Զ�������һ��һֱʹ�õĴ洢����ֱ���˳�Ӧ�ó���ʵ���������˵��ú���ʱѹջ��ջ���ٶȿ�ܶ�
 */
static int lcm_internal_pipe_create(int filedes[2]){
	int				status, SocOpt, rdyCount, nPort;
	short			Port;
	fd_set          rSSet, wSSet;
	SOCKET			listSoc, sendSoc, recvSoc;
	sockaddr_in     list_addr, recv_addr;
	timeval			tv;
	/*
	 *	added by durant35
	 *		listSoc: �������˵�socket
	 *		recvSoc: �ͻ��˵�socket
	 */
	listSoc = socket(PF_INET, SOCK_STREAM, 0);
	recvSoc = socket(PF_INET, SOCK_STREAM, 0);

	list_addr.sin_family = AF_INET;
	list_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//	This loop ensures that we pick up an UNUSED port. If anything else used this port,
	//	the entire lcm notification system melts down. The assumption is that we can't bind
	//	to an address in use once the SO_EXCLUSIVEADDRUSE has been set. If this isn't true, 
	//	another method will need to be implemented.
	do{
		nPort = InterlockedIncrement(&LastSocket);		// Make sure we're using unique port
		if (nPort > 65500){								// Wrapping, reset the port #
			InterlockedCompareExchange(&LastSocket, ARBITRARY_START_PORT, nPort);
		}
		Port = (short)nPort;
		list_addr.sin_port = htons(Port);

		SocOpt = 1;
		status = setsockopt(listSoc, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char *)&SocOpt, sizeof(SocOpt));
		if (status)
			continue;
		status = bind(listSoc, (LPSOCKADDR)&list_addr, sizeof(list_addr));
	} while (status != 0);
	/*
	 *	added by durant35
	 *		��������ʽ����
	 *		�������ֻ������һ���ͻ�������
	 */
	SocOpt = 1;
	status = ioctlsocket(listSoc, FIONBIO, (u_long *)&SocOpt);
	status = listen(listSoc, 1);

	recv_addr.sin_family = AF_INET;
	recv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	recv_addr.sin_port = htons(Port);
	/*
	 *	added by durant35
	 *		���÷�������ʽ����(http://blog.csdn.net/ludw508/article/details/8565203)
	 *		�����������׽��֣�0��ʾ��ȷ��SOCKET_ERROR��ʾ����
	 *		���ڷ������׽��֣����������ж������Ƿ���ɡ������᷵��SOCKET_ERROR�����Ⲣ����ʾ����
	 *			ʹ��select���������������������ɣ�ͨ���鿴�׽����Ƿ��д
	 */
	SocOpt = 1;
	status = ioctlsocket(recvSoc, FIONBIO, (u_long *)&SocOpt);
	if (SOCKET_ERROR == status){
		return -1;
	}
	status = connect(recvSoc, (LPSOCKADDR)&recv_addr, sizeof(recv_addr));
	rdyCount = 0;
	tv.tv_sec = 0;
	tv.tv_usec = 20 * 1000;
	while (rdyCount < 2){
		rSSet.fd_count = 1;
		rSSet.fd_array[0] = recvSoc;
		rSSet.fd_array[1] = INVALID_SOCKET;
		wSSet.fd_count = 1;
		wSSet.fd_array[0] = listSoc;
		wSSet.fd_array[1] = INVALID_SOCKET;
		rdyCount = select(0, &wSSet, &rSSet, NULL, &tv);
	}
	//	Both sockets are ready now to complete the connection.

	sendSoc = accept(listSoc, (LPSOCKADDR)&list_addr, (int*)&list_addr);
	/*
	 *	added by druant35
	 *		do not close for select accurately timeout!
	 */
	//closesocket(listSoc);												// This port is now in use - clear listener

	//	Restore the sockets to blocking (default behavior).
	SocOpt = 0;
	status = ioctlsocket(recvSoc, FIONBIO, (u_long *)&SocOpt);
	status = ioctlsocket(sendSoc, FIONBIO, (u_long *)&SocOpt);

	filedes[0] = (int)recvSoc;
	filedes[1] = (int)sendSoc;

	return 0;
}
static size_t lcm_internal_pipe_write(int fd, const void* buf, size_t count){
	int		status;
	status = send((SOCKET)fd, (char *)buf, (int)count, 0);
	return (size_t)status;
}
static size_t lcm_internal_pipe_read(int fd, void* buf, size_t count){
	int		status;
	status = recv((SOCKET)fd, (char *)buf, int(count), 0);
	return status;
}
static int lcm_internal_pipe_close(int fd){
	return closesocket((SOCKET)fd);
}
/*
 *	added by durant35
 *		gthread�̺߳���
 *		1. ͨ��NextEvent��.pcap�ļ���ȡ��һ֡��������
 *		2. ͨ��select��׼ȷtimeout��ͬ������ʱ����ץ��ʱ���
 *		3. ͨ��PublishEvent ��loopback���ͳ�ȥ���ṩ�����ݲɼ�����ʹ��
 */
static void* timer_thread_func(void* user){
	VeloPlayer* velo_player = reinterpret_cast<VeloPlayer*> (user);

	int64_t abstime;
	int64_t now;
	int64_t starttime;
	int64_t startabstime;
	struct timeval sleep_tv;

	int64_t sleep_utime;
	fd_set fds;
	int timer_pipe[2];
	TRACE("timer_thread_func \n");
	if (lcm_internal_pipe_create(timer_pipe) != 0){
		TRACE("lcm_internal_pipe_create failed\n");
		return NULL;
	}

	int status = 0;

	startabstime = timestamp_now();

	velo_player->NextEvent();
	starttime = timevalToInt64(velo_player->m_currentTime);

	while (velo_player->m_exitThread == 0){
		velo_player->NextEvent();
		abstime = timevalToInt64(velo_player->m_currentTime) - starttime + startabstime;
		if (abstime < 0)
			return NULL;

		now = timestamp_now();
		if (abstime >now){
			sleep_utime = abstime - now;
			sleep_tv.tv_sec = sleep_utime / 1000000;
			sleep_tv.tv_usec = sleep_utime % 1000000;
			// sleep until the next timed message, or until an abort message
			/*
			 *	added by durant35
			 *		select: 2rd parameter readfds identifies the sockets that are to be checked for readability
			 */
			FD_ZERO(&fds);
			FD_SET(timer_pipe[0], &fds);
			/*
			 *	added by durant35
			 *		���Կɶ��� ��д��
			 *		velo_player->timer_pipe[0]: 
			 *			�����һ��������connect������(��д)
			 *			û��������Ҫ����(���ɶ� ��������ʱ����ʱʱ��sleep_tv)
			 */
			//#define _SELECT_TEST_
			//#define _SELECT_WRITE_
#ifdef _SELECT_WRITE_
			status = select(0, 0, &fds, NULL, &sleep_tv);
			//::Sleep(sleep_utime / 1000);
#else
			status = select(0, &fds, 0, NULL, &sleep_tv);
			//int status = select(velo_player->timer_pipe[0] + 1, NULL, &fds, NULL, &sleep_tv);
#endif
#ifdef _SELECT_TEST_
			int error = WSAGetLastError();
			printFunc("status =", status);
			printFunc("error =", error);
#endif
			// select timed out
			if (!status){
				velo_player->PublishEvent();
			}
			else{
				TRACE("0 != status %d %d\n", status, WSAGetLastError());
				break;
			}
		}
		else{
			if (abstime < now)
				TRACE("abstime < now %ld\n", now - abstime);
			velo_player->PublishEvent();
		}
	}
	TRACE("Thread stoped! \n");
	if (timer_pipe[0] >= 0)
		lcm_internal_pipe_close(timer_pipe[0]);
	if (timer_pipe[1] >= 0)
		lcm_internal_pipe_close(timer_pipe[1]);

	return NULL;
}

VeloPlayer::VeloPlayer(const char *filename){
    m_totalSecs=0;
    memset(&m_currentTime, 0, sizeof(m_currentTime));
    memset(&m_startTime, 0, sizeof(m_startTime));
    m_currentFrameNum = 0;
    m_totalFrameNum = 0;
	m_exitThread = 0;
	thread_created = 0;
    this->filename = _strdup ( filename );
}
VeloPlayer::~VeloPlayer(void){
    if ( filename )
        free ( filename );
}

/*
 *	added by durant35
 *		��ȡ.pcap�ļ� ����.idx����--> ÿһ֡(��)����������ļ�ָ��offset pktNumber ʱ���
 *		fileHandle �򿪵�.pcap�ļ��ļ�����
 */
int VeloPlayer::Open ( void ){
    assert ( filename != NULL );
    printf ( "VeloPlayer::Open %s\n", filename );
	// std::ifstream fileHandle
    fileHandle.open ( filename, std::ios::in | std::ios::binary );
    if ( fileHandle.is_open() == false ){
        printf ( "%s does not exist!!\n",filename );
        return -1;
    }
    if ( LoadIndex() != 0 ){
        if(BuildIndex() == 0)
            SaveIndex();
    }

    m_totalFrameNum = this->m_index.size();
    if ( m_totalFrameNum > 1 ){
        this->m_startTime = m_index[0].ts;
		/*
		 *	��ʱ��: ����
		 */
        this->m_totalSecs = m_index[m_totalFrameNum-1].ts.tv_sec - m_index[0].ts.tv_sec ;
    }
	//a1=192, a2=168, a3=0, a4=103;
    a1=127, a2=0, a3=0, a4=1;
    ///////////////////////////���ݴ����ʼ��/////////////////////////////////////////////
    // Open windows connection
    if (WSAStartup(0x0101, &wsa) != 0){
        fprintf(stderr, "Could not open Windows connection.\n");
		exit(0);
    }
    // Open a datagram socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd == INVALID_SOCKET){
        fprintf(stderr, "Could not create socket.\n");
		WSACleanup();
		exit(0);
    }
    // Clear out server struct
    memset((void *)&server, '\0', sizeof(struct sockaddr_in));
    // Set family and port
    server.sin_family = AF_INET;
	server.sin_port = htons(VELODYNE_UDP_PORT); // 8002
    // Set server address
    server.sin_addr.S_un.S_un_b.s_b1 = (unsigned char)a1;
    server.sin_addr.S_un.S_un_b.s_b2 = (unsigned char)a2;
    server.sin_addr.S_un.S_un_b.s_b3 = (unsigned char)a3;
    server.sin_addr.S_un.S_un_b.s_b4 = (unsigned char)a4;

    return 0;
}
/*
 *	added by durant35
 *		Start��Stopͨ��Ƶ���ش������˳���̨.pcap�ļ����ݻ�ȡ��loopback�ػ������߳�
 *		m_currentTime & m_currentFrameNum������״̬
 */
int VeloPlayer::Start(void){
	m_exitThread = 0;
	// Start the reader thread
	timer_thread = g_thread_create(timer_thread_func, this, TRUE, NULL);
	if (!timer_thread){
		TRACE("Error: VeloPlayer failed to start timer thread\n");
		return -1;
	}
	thread_created = 1;

	return 0;
}
int VeloPlayer::Stop(void){
	if (thread_created){
		m_exitThread = 1;
		g_thread_join(timer_thread);
		thread_created = 0;
	}
	return 0;
}
/*
 *	adde by durant35
 *		ͨ��ʱ��Ѱ����ƥ���m_currentFrameNum ���Ÿ�֡���������
 */
bool idxInfo_lesser(VELO::idxInfo elem1, VELO::idxInfo elem2){
	if (elem1.ts.tv_sec == elem2.ts.tv_sec)
		return elem1.ts.tv_usec < elem2.ts.tv_usec;
	else
		return elem1.ts.tv_sec < elem2.ts.tv_sec;
}
int VeloPlayer::SetPos(int seconds){
	VELO::idxInfo idx_info;
	idx_info.ts.tv_sec = m_startTime.tv_sec + seconds;

	vector<VELO::idxInfo>::iterator it = lower_bound(m_index.begin(), m_index.end(), idx_info, idxInfo_lesser);
	if (it != m_index.end()){
		m_currentFrameNum = it - m_index.begin();
	}
	return 0;
}
/*
 *	added by durant35
 *		�ر�socket
 *		ע��Sockets����
 *		�رմ򿪵��ļ�
 */
int VeloPlayer::Close ( void ){
	closesocket(sockfd);
    WSACleanup();
    fileHandle.close();
    return 0;
}
/*
 *	added by durant35
 *		��.pcap������.idx�����ļ��м��� vector<VELO::idxInfo> m_index;
 */
int VeloPlayer::LoadIndex ( void ){
    char buffer[sizeof ( VELO::idxInfo ) ];
    int *p = reinterpret_cast<int*> ( buffer );
    int len, i;
    char indexname[256];
    VELO::idxInfo idx_info;

    std::ifstream file;
    _snprintf ( indexname,sizeof(indexname), "%s.idx", filename );
    file.open ( indexname, std::ios::in | std::ios::binary );
    if ( file.is_open() == false ){
        printf ( "%s does not exist!!\n" ,indexname);
        return -1;
    }
    // read magic bits
    file.read ( buffer, 4 );
    if ( buffer[0] != 'I' || buffer[1] != 'D' || buffer[2] != 'X' )
    {
        printf ( "Not an idx file!!\n" );
        file.close();
        return -1;
    }
	// read total Frame Number
    file.read ( buffer, 4 );
    len = *p;
	// ����������Ϣ
    this->m_index.clear();
    for ( i = 0; i < len; ++i ){
		file.read(reinterpret_cast<char*> (&idx_info), sizeof(VELO::idxInfo));
		m_index.push_back(idx_info);
    }
    file.close();
	printFunc("LoadIndex done! FrameTotalNumber", len);
    return 0;
}
int VeloPlayer::BuildIndex ( void ){
    unsigned int fileLen = 0;
    fileHandle.seekg(0, ios_base::end);
    fileLen = fileHandle.tellg();
	/*
	 *	added by durant35
	 *		���� pcap file header
	 */
    fileHandle.seekg(sizeof(pcap_file_header), ios_base::beg);

    VELO::idxInfo idx_info;
    pcap_pkthdr pkthdr;
	idx_info.offset = 0;
	idx_info.PktNumber = 0;
    unsigned int currentOffset = 0;

	VelodyneDriver::velodyne_packet_t pkt;
	/*
	 *	added by durant35
	 *		pcapÿ��ץ���Դ�ͷ��: pcap_pkthdr
	 *		����һ����
	 */
    while ( fileLen > sizeof(pcap_pkthdr) + VELODYNE_PACKET_SIZE){
		currentOffset = fileHandle.tellg();

		fileHandle.read(reinterpret_cast<char*>(&pkthdr), sizeof(pcap_pkthdr));
		/*
		*	added by durant35
		*		pkthdr ����ÿ��ץ���ĳ���
		*/
		if (pkthdr.len != VELODYNE_PACKET_SIZE){
			MessageBox(NULL, "packet length != VELODYNE_PACKET_SIZE...", "", 0);
			printf("bad packet!-%u-%u\n", pkthdr.len, pkthdr.caplen);
			return 0;
		}

		fileHandle.seekg(VELODYNE_UDP_OFFSET, ios_base::cur);
		fileHandle.read(reinterpret_cast<char*>(&pkt), sizeof(VelodyneDriver::velodyne_packet_t));

        // �ҵ���һ��Frame�Ŀ�ʼ������һ��Frame��¼����
        if(this->velodyneDriver.isNewScan(pkt)){
            //printf("push back!-%d %d\n",m_index.size(),ii.scanNumber);
			if (idx_info.PktNumber > PKT_NUM_IN_CIRCLE_LOWER_BOUND)
				m_index.push_back(idx_info);
			idx_info.offset = currentOffset;
			idx_info.ts = pkthdr.ts;
			idx_info.PktNumber = 1;
        }
        else{
			idx_info.PktNumber++;
        }

		fileLen -= VELODYNE_PACKET_WITH_PCAPHDR_SIZE;
    }
    //printFunc ( "BuildIndex over\n", m_index.size() );

    return 0;
}

int VeloPlayer::SaveIndex ( void ){
    char buffer[sizeof ( VELO::idxInfo ) ] = {"IDX"};
    int *p = reinterpret_cast<int*> ( buffer );
    int len, i;
    char indexname[256];
	/*
	 *	filename �򿪵�.pcap�ļ���
	 *		fstream
	 */
    std::ofstream file;
    _snprintf ( indexname, sizeof(indexname), "%s.idx", filename );
	//MessageBox(filename);
	file.open(indexname, std::ios::out | std::ios::binary);
    // write magic bits
    file.write ( buffer, 4 );
    // write
    p[0] = len = m_index.size();
	/*
	 *	added by durant35
	 *		class idxInfo
			{
			public:
				struct timeval ts;			// pcapÿ��ץ��ʱ���
				int PktNumber;				// ÿһ֡��������ݵ�packet��Ŀ
				int offset;					// ÿ��pcapץ����.pcap�ļ��е��ļ�ָ��ƫ����
			};
	 */
    file.write ( buffer, 4 );
    for ( i = 0; i < len; ++i ){
        file.write ( reinterpret_cast<char*> ( &m_index[i] ), sizeof ( VELO::idxInfo )  );
        //printf("write[%d]  offset[%u] number[%d]\n",i,m_index[i].offset,m_index[i].scanNumber);
    }
    file.close();

	//printFunc("SaveIndex done! FrameTotalNumber", len);
    return 0;
}
/*
 *	added by durant35
 *		NextEvent: �ļ�ָ��ָ��ָ��һ֡���ݵ��±�
 *		PublishEvent: ����֡�е�size��packet���ζ��� Ȼ��ͨ��sendto���͸�loopback
 */
int VeloPlayer::LastEvent(void){
	if (m_currentFrameNum > 0)
		m_currentFrameNum--;

	m_currentTime = timersub(m_index[m_currentFrameNum].ts, m_index[0].ts);
	return 0;
}
int VeloPlayer::NextEvent(void){
    if(m_currentFrameNum < m_totalFrameNum-1)
        m_currentFrameNum++;
	
	m_currentTime = timersub(m_index[m_currentFrameNum].ts, m_index[0].ts);
    return 0;
}
int VeloPlayer::PublishEvent(void){
    TRACE ( "PublishEvent %d\n" ,m_currentFrameNum);
    char packet[VELODYNE_MSG_BUFFER_SIZE];

    fileHandle.seekg(m_index[m_currentFrameNum].offset, ios_base::beg);

    int size = m_index[m_currentFrameNum].PktNumber;
    int i=0;
    for(; i<size; ++i){
		fileHandle.seekg(VELODYNE_UDP_PCAP_OFFSET, ios_base::cur);
        fileHandle.read(packet,sizeof(packet));
		sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
        //if(i%3==0)
        //	Sleep(1);
    }

    return 0;
}

