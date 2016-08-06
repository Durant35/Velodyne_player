
// playerDlg.h : ͷ�ļ�
//

#pragma once

#include "LCMPlayer.h"
#include "VeloPlayer.h"
#include "afxcmn.h"
#include "afxwin.h"


// CplayerDlg �Ի���
class CplayerDlg : public CDialog
{
// ����
public:
	CplayerDlg(CWnd* pParent = NULL);	// ��׼���캯��

// �Ի�������
	enum { IDD = IDD_PLAYER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��
// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnNMCustomdrawSliderTime(NMHDR *pNMHDR, LRESULT *pResult);
private:
	LCMPlayer* m_LCMPlayer;
	VeloPlayer* m_VeloPlayer;
public:
	virtual BOOL DestroyWindow();
	CSliderCtrl m_sliderCtrl;
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedButtonOpen();
	~CplayerDlg(void);
	afx_msg void OnBnClickedButtonNextframe();
	CStatic m_STend;
	CStatic m_STcurrentNum;
	CStatic m_STcurrentTimestamp;
	afx_msg void OnBnClickedButtonNextsec();
	afx_msg void OnBnClickedButtonLastsec();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnBnClickedButtonPlay();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	int syncTime(void);
	afx_msg void OnBnClickedButtonOpenPcap();
	afx_msg void OnStnClickedStaticEnd();
	afx_msg void OnBnClickedOk();
	CStatic m_lidarType;
	afx_msg void OnClickedBtnLaunchVelodump();
	afx_msg void OnBnClickedButtonLastframe();
private:
	int CreateDir(const char* DirName);
};
