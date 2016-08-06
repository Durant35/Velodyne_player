// playerDlg.cpp : 实现文件
#include "stdafx.h"
#include "player.h"
#include "playerDlg.h"
#include <direct.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define WM_TRC_CLOSE_DLG (WM_USER + 500)

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框
class CAboutDlg : public CDialog
{
public:
    CAboutDlg();
// 对话框数据
    enum { IDD = IDD_ABOUTBOX };
protected:
    virtual void DoDataExchange ( CDataExchange* pDX ); // DDX/DDV 支持
// 实现
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog ( CAboutDlg::IDD )
{
}

void CAboutDlg::DoDataExchange ( CDataExchange* pDX )
{
    CDialog::DoDataExchange ( pDX );
}

BEGIN_MESSAGE_MAP ( CAboutDlg, CDialog )
END_MESSAGE_MAP()

// CplayerDlg 对话框
CplayerDlg::CplayerDlg ( CWnd* pParent /*=NULL*/ )
    : CDialog ( CplayerDlg::IDD, pParent )
{
    m_hIcon = AfxGetApp()->LoadIcon ( IDR_MAINFRAME );
    m_LCMPlayer = NULL;
    m_VeloPlayer=NULL;
}


CplayerDlg::~CplayerDlg ( void )
{
    if ( m_LCMPlayer ){
        m_LCMPlayer->Close();
        delete m_LCMPlayer;
    }

    if(m_VeloPlayer){
        m_VeloPlayer->Close();
        delete m_VeloPlayer;
    }
}


void CplayerDlg::DoDataExchange ( CDataExchange* pDX )
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_SLIDER_TIME, m_sliderCtrl);
	DDX_Control(pDX, IDC_STATIC_END, m_STend);
	DDX_Control(pDX, IDC_STATIC_NUM, m_STcurrentNum);
	DDX_Control(pDX, IDC_STATIC_TIMESTAMP, m_STcurrentTimestamp);
	DDX_Control(pDX, IDC_STATIC_LIDAR_TYPE, m_lidarType);
}

BEGIN_MESSAGE_MAP ( CplayerDlg, CDialog )
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    //}}AFX_MSG_MAP
    ON_NOTIFY ( NM_CUSTOMDRAW, IDC_SLIDER_TIME, &CplayerDlg::OnNMCustomdrawSliderTime )
    ON_WM_HSCROLL()
    ON_BN_CLICKED ( IDC_BUTTON_OPEN, &CplayerDlg::OnBnClickedButtonOpen )
    ON_BN_CLICKED ( IDC_BUTTON_NEXTFRAME, &CplayerDlg::OnBnClickedButtonNextframe )
    ON_BN_CLICKED ( IDC_BUTTON_NEXTSEC, &CplayerDlg::OnBnClickedButtonNextsec )
    ON_BN_CLICKED ( IDC_BUTTON_LASTSEC, &CplayerDlg::OnBnClickedButtonLastsec )
    ON_WM_KEYDOWN()
    ON_BN_CLICKED(IDC_BUTTON_PLAY, &CplayerDlg::OnBnClickedButtonPlay)
    ON_BN_CLICKED(IDC_BUTTON_STOP, &CplayerDlg::OnBnClickedButtonStop)
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BUTTON_OPEN_PCAP, &CplayerDlg::OnBnClickedButtonOpenPcap)
	ON_STN_CLICKED(IDC_STATIC_END, &CplayerDlg::OnStnClickedStaticEnd)
	ON_BN_CLICKED(IDOK, &CplayerDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BTN_LAUNCH_VELODUMP, &CplayerDlg::OnClickedBtnLaunchVelodump)
	ON_BN_CLICKED(IDC_BUTTON_LASTFRAME, &CplayerDlg::OnBnClickedButtonLastframe)
END_MESSAGE_MAP()

// CplayerDlg 消息处理程序
BOOL CplayerDlg::OnInitDialog()
{
    CDialog::OnInitDialog();

    // 将“关于...”菜单项添加到系统菜单中。
    if (!g_thread_supported ()) //如果gthread没有被初始化
             g_thread_init (NULL); 

    // IDM_ABOUTBOX 必须在系统命令范围内。
    ASSERT ( ( IDM_ABOUTBOX & 0xFFF0 ) == IDM_ABOUTBOX );
    ASSERT ( IDM_ABOUTBOX < 0xF000 );

    CMenu* pSysMenu = GetSystemMenu ( FALSE );
    if ( pSysMenu != NULL )
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString ( IDS_ABOUTBOX );
        ASSERT ( bNameValid );
        if ( !strAboutMenu.IsEmpty() )
        {
            pSysMenu->AppendMenu ( MF_SEPARATOR );
            pSysMenu->AppendMenu ( MF_STRING, IDM_ABOUTBOX, strAboutMenu );
        }
    }

    // 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作
    SetIcon ( m_hIcon, TRUE );   // 设置大图标
    SetIcon ( m_hIcon, FALSE );  // 设置小图标

    // TODO: 在此添加额外的初始化代码
    this->m_sliderCtrl.SetRange ( 0, 1000 );

#ifdef USE_HDL_64E_
	this->m_lidarType.SetWindowText("Velodyne-HDL-64E");
#endif
#ifdef USE_VLP_16_
	this->m_lidarType.SetWindowText("Velodyne-VLP-16");
#endif

	if (this->CreateDir("tmp")){
		return FALSE;
	}
	if (this->CreateDir("pcap")){
		return FALSE;
	}

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CplayerDlg::OnSysCommand ( UINT nID, LPARAM lParam )
{
    if ( ( nID & 0xFFF0 ) == IDM_ABOUTBOX ){
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else{
        CDialog::OnSysCommand ( nID, lParam );
    }
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。
void CplayerDlg::OnPaint()
{
    if ( IsIconic() ){
        CPaintDC dc ( this ); // 用于绘制的设备上下文

        SendMessage ( WM_ICONERASEBKGND, reinterpret_cast<WPARAM> ( dc.GetSafeHdc() ), 0 );

        // 使图标在工作区矩形中居中
        int cxIcon = GetSystemMetrics ( SM_CXICON );
        int cyIcon = GetSystemMetrics ( SM_CYICON );
        CRect rect;
        GetClientRect ( &rect );
        int x = ( rect.Width() - cxIcon + 1 ) / 2;
        int y = ( rect.Height() - cyIcon + 1 ) / 2;

        // 绘制图标
        dc.DrawIcon ( x, y, m_hIcon );
    }
    else
    {
        CDialog::OnPaint();
    }
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CplayerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR> ( m_hIcon );
}

void CplayerDlg::OnNMCustomdrawSliderTime ( NMHDR *pNMHDR, LRESULT *pResult )
{
    LPNMCUSTOMDRAW pNMCD = reinterpret_cast<LPNMCUSTOMDRAW> ( pNMHDR );
    // TODO: Add your control notification handler code here

    *pResult = 0;
}

BOOL CplayerDlg::DestroyWindow()
{
    // TODO: Add your specialized code here and/or call the base class
    return CDialog::DestroyWindow();
}
// 时间轴拖动
void CplayerDlg::OnHScroll ( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar ){
    if ( m_LCMPlayer == NULL &&m_VeloPlayer == NULL){
        this->MessageBox ( "Open file first!" );
        this->m_sliderCtrl.SetPos ( 0 );
        return;
    }
    if ( nSBCode == SB_ENDSCROLL ){
        UpdateData ( TRUE );
        int pos = this->m_sliderCtrl.GetPos();
        UpdateData ( FALSE );
        if(m_LCMPlayer){
            m_LCMPlayer->Stop();
            m_LCMPlayer->SetPos( pos );
        }
        if(m_VeloPlayer){
            m_VeloPlayer->Stop();
            m_VeloPlayer->SetPos( pos );
        }
        OnBnClickedButtonNextframe();
    }
    CDialog::OnHScroll ( nSBCode, nPos, pScrollBar );
}
/*
 *	added by durant35
 *		LCMPlayer 从与打开的文件相关的event流中获取event 然后publish出去
 */
void CplayerDlg::OnBnClickedButtonOpen(){
    // TODO: Add your control notification handler code here
    CString FilePathName;
    CFileDialog dlg ( TRUE );///TRUE为OPEN对话框，FALSE为SAVE AS对话框
    if ( dlg.DoModal() == IDOK ){
        FilePathName = dlg.GetPathName();
		/*
		 *	added by durant35
		 *		通过LCM传递激光点数据???
		 */
        if ( this->m_LCMPlayer ){
            m_LCMPlayer->Close();
            delete m_LCMPlayer;
        }
        m_LCMPlayer = new LCMPlayer ( FilePathName.GetBuffer() );
        FilePathName.ReleaseBuffer();
		//this->MessageBox("m_LCMPlayer->Open()");
        if ( m_LCMPlayer->Open() ){
            this->MessageBox ( "Open failed" );
            return ;
        }

        this->m_sliderCtrl.SetRange ( 0, m_LCMPlayer->m_totalSecs );
        this->m_sliderCtrl.SetPageSize ( m_LCMPlayer->m_totalSecs / 20 );

        char buf[256];
        sprintf ( buf, "%lds", m_LCMPlayer->m_totalSecs );
        this->m_STend.SetWindowText ( buf );
    }

}
/*
 *	added by durant35
 *		打开.pcap文件 --> 记录每帧激光点数据信息(.idx 主要是文件指针偏移量[ts;PktNumber;offset])
 *			--> 周期性获取新一帧激光点数据，并通过loopback发送出去(直接.pcao文件读写操作:NextEvent PublishEvent)
 *		计算 总帧数(m_totalNum)、总时长(m_totalSecs)/s
 *		初始化当前帧时间戳(m_currentTime)、当前帧号(m_currentEventNum)
 *		显示相关界面信息
 */
void CplayerDlg::OnBnClickedButtonOpenPcap(){
    // TODO: Add your control notification handler code here
    CString FilePathName;
    CFileDialog dlg ( TRUE );///TRUE为OPEN对话框，FALSE为SAVE AS对话框
    if ( dlg.DoModal() == IDOK ){
        FilePathName = dlg.GetPathName();

		ofstream foutFileName;         //
		foutFileName.open("./tmp/FileName.txt");
		foutFileName << FilePathName;
		foutFileName.close();

		if ( this->m_VeloPlayer){
			m_VeloPlayer->Close();
			delete m_VeloPlayer;
		}
		m_VeloPlayer = new VeloPlayer ( FilePathName.GetBuffer() );
		FilePathName.ReleaseBuffer();
		if ( m_VeloPlayer->Open() ){
			this->MessageBox ( "Open failed" );
			return ;
		}
        this->m_sliderCtrl.SetRange ( 0, m_VeloPlayer->m_totalSecs );
        this->m_sliderCtrl.SetPageSize ( m_VeloPlayer->m_totalSecs / 20 );
		/*
		 *	added by durant35
		 *		更新总时长等显示文本信息
		 *		更新帧数等显示文本信息
		 */
        char buf[256];
        sprintf( buf, "%lds", m_VeloPlayer->m_totalSecs );
        this->m_STend.SetWindowText ( buf );
		sprintf(buf, "%d|%d",
			static_cast<int> (m_VeloPlayer->m_currentFrameNum),
			static_cast<int> (m_VeloPlayer->m_totalFrameNum));
		this->m_STcurrentNum.SetWindowText(buf);
    }
}
/*
 *	added by durant35
 *		下一帧 下一秒 上一秒等操作后 均处于停止状态
 */

void CplayerDlg::OnBnClickedButtonLastframe()
{
	if (m_LCMPlayer == NULL && m_VeloPlayer == NULL){
		this->MessageBox("Open file first!");
		return;
	}
	if (m_LCMPlayer){
		m_LCMPlayer->Stop();
		/*
		*	added by durant35
		*		LCMPlayer就是 NextEvent获取 PublishEvent发布
		*/
		//if (m_LCMPlayer->LastEvent() == 0)
		//	m_LCMPlayer->PublishEvent();
	}
	if (m_VeloPlayer){
		m_VeloPlayer->Stop();
		if (m_VeloPlayer->LastEvent() == 0){
			m_VeloPlayer->PublishEvent();
		}
	}
	syncTime();
}
void CplayerDlg::OnBnClickedButtonNextframe(){
    if ( m_LCMPlayer == NULL && m_VeloPlayer == NULL){
        this->MessageBox ( "Open file first!" );
        return;
    }
    if(m_LCMPlayer){
        m_LCMPlayer->Stop();
		/*
		 *	added by durant35
		 *		LCMPlayer就是 NextEvent获取 PublishEvent发布
		 */
        if(m_LCMPlayer->NextEvent() == 0)
			m_LCMPlayer->PublishEvent();
    }
    if(m_VeloPlayer){
        m_VeloPlayer->Stop();
        m_VeloPlayer->NextEvent();
        m_VeloPlayer->PublishEvent();
    }
    syncTime();
}
// 下一秒
void CplayerDlg::OnBnClickedButtonNextsec(){
    if ( m_LCMPlayer == NULL && m_VeloPlayer == NULL){
        this->MessageBox ( "Open file first!" );
        return;
    }
    if(m_LCMPlayer){
        m_LCMPlayer->Stop();
        m_LCMPlayer->SetPos ( ( m_LCMPlayer->m_currentTime + 1500000 ) / 1000000 );
        OnBnClickedButtonNextframe();
    }

    if(m_VeloPlayer)
    {
        m_VeloPlayer->Stop();
        m_VeloPlayer->SetPos (  m_VeloPlayer->m_currentTime.tv_sec + 1 );
        OnBnClickedButtonNextframe();
    }


}
// 上一秒
void CplayerDlg::OnBnClickedButtonLastsec(){
    // TODO: Add your control notification handler code here
    if ( m_LCMPlayer == NULL && m_VeloPlayer == NULL ){
        this->MessageBox ( "Open file first!" );
        return;
    }
    if(m_LCMPlayer){
        m_LCMPlayer->Stop();
        m_LCMPlayer->SetPos ( ( m_LCMPlayer->m_currentTime - 500000 ) / 1000000 );
        OnBnClickedButtonNextframe();
    }
    if(m_VeloPlayer){
        m_VeloPlayer->Stop();
        m_VeloPlayer->SetPos (  m_VeloPlayer->m_currentTime.tv_sec - 2 );
        OnBnClickedButtonNextframe();
    }
}

void CplayerDlg::OnKeyDown ( UINT nChar, UINT nRepCnt, UINT nFlags ){
    // TODO: Add your message handler code here and/or call default
    switch ( nChar ){
    case 'N':
        OnBnClickedButtonNextframe();
        break;
	default:
        break;
    }
    CDialog::OnKeyDown ( nChar, nRepCnt, nFlags );
}
/* 
 *	added by durant35
 *		播放与停止通过频繁地退出和开启各自的实时更新线程 打开和关闭界面更新定时器来实现
 *		操作不影响 m_currentTime，故能保存每次播放所处的时刻
 */
void CplayerDlg::OnBnClickedButtonPlay(){
    // TODO: Add your control notification handler code here
    if ( m_LCMPlayer == NULL && m_VeloPlayer==NULL ){
        this->MessageBox ( "Open file first!" );
        return;
    }
    if(m_LCMPlayer){
        m_LCMPlayer->Start();
    }
	if(m_VeloPlayer){
        m_VeloPlayer->Start();
    }
    this->SetTimer(1, 1000, NULL);
}
// 停止
void CplayerDlg::OnBnClickedButtonStop(){
    // TODO: Add your control notification handler code here
    if ( m_LCMPlayer == NULL && m_VeloPlayer == NULL ){
        this->MessageBox ( "Open file first!" );
        return;
    }
    this->KillTimer(1);
    if(m_LCMPlayer)
        m_LCMPlayer->Stop();
	 if(m_VeloPlayer)
        m_VeloPlayer->Stop();
}

void CplayerDlg::OnTimer(UINT_PTR nIDEvent){
    // TODO: Add your message handler code here and/or call default
    syncTime();
    CDialog::OnTimer(nIDEvent);
}
/*
 *	added by durant35
 *		1. 根据 LCMPlayer 的 m_currentTime 更新当前event显示信息，时间显示信息，更新时间轴
 *		2. 根据 VeloPlayer 的 m_currentTime 更新当前帧数显示信息，更新时间轴
 */
int CplayerDlg::syncTime(void){
    char buf[256];
    if(m_LCMPlayer){
        sprintf ( buf, "%d|%d", 
				static_cast<int> ( m_LCMPlayer->m_currentEventNum ), 
				static_cast<int> ( m_LCMPlayer->m_totalNum ) );
        this->m_STcurrentNum.SetWindowText ( buf );

        sprintf ( buf, "%lf", ( m_LCMPlayer->m_currentTime*1.0 ) / 1000000 );
        this->m_STcurrentTimestamp.SetWindowText ( buf );
        this->m_sliderCtrl.SetPos ( m_LCMPlayer->m_currentTime / 1000000 );
    }

	ofstream foutFrameNum;
	foutFrameNum.open("./tmp/FrameNum.txt");
	foutFrameNum << m_VeloPlayer->m_currentFrameNum;
	foutFrameNum.close();

    if(m_VeloPlayer){
        sprintf ( buf, "%d|%d", 
					static_cast<int> ( m_VeloPlayer->m_currentFrameNum ), 
					static_cast<int> ( m_VeloPlayer->m_totalFrameNum ) );
        this->m_STcurrentNum.SetWindowText ( buf );
		//sprintf ( buf, "%lfs",  m_VeloPlayer->m_currentTime.tv_sec+ m_VeloPlayer->m_StartTime.tv_sec);
		//this->m_STcurrentTimestamp.SetWindowText ( buf );
        this->m_sliderCtrl.SetPos (m_VeloPlayer->m_currentTime.tv_sec );
    }
    return 0;
}

void CplayerDlg::OnStnClickedStaticEnd(){
	// TODO: Add your control notification handler code here
}

void CplayerDlg::OnBnClickedOk(){
	// TODO:  在此添加控件通知处理程序代码
	CDialog::OnOK();
}

/*
 *	added by durant35
 *		启动激光数据记录程序
 */
void CplayerDlg::OnClickedBtnLaunchVelodump()
{
	ShellExecute(NULL, "open", 
		//"D:\\Workspace\\VS_Workspace\\Velodyne_player\\Debug\\VelodynePcap.exe", 
		".\\VelodynePcap.exe",
		NULL, NULL, SW_SHOWNORMAL);
}

int CplayerDlg::CreateDir(const char* DirName){
	FILE* fp = NULL;
	char TempDir[200];
	memset(TempDir, '\0', sizeof(TempDir));
	sprintf(TempDir, DirName);
	strcat(TempDir, "\\");
	strcat(TempDir, ".temp.fortest");
	fp = fopen(TempDir, "w");
	if (!fp){
		if (mkdir(DirName) == 0){
			return 0;				// 文件夹创建成功  
		}
		else{
			return -1;				// can not make a dir;  
		}
	}
	else{
		fclose(fp);
		remove(TempDir);
	}
	return 0;
}

