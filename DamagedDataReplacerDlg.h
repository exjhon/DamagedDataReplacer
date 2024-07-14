
// DamagedDataReplacerDlg.h: 头文件
//

#pragma once

#include "afxwin.h"
#include "afxcmn.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>

// CDamagedDataReplacerDlg 对话框
class CDamagedDataReplacerDlg : public CDialogEx
{
    // 构造
public:
    CDamagedDataReplacerDlg(CWnd* pParent = nullptr);	// 标准构造函数
    // 对话框数据
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_DAMAGEDDATAREPLACER_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


    // 实现
protected:
    HICON m_hIcon;
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    void RequestAdminPrivileges();

    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedButton1();
    afx_msg void OnBnClickedCheck1();
    afx_msg void OnBnClickedCheck2();

private:
    void TraverseFolder(const CString& folderPath, std::vector<std::wstring>& fileList, bool includeSubDirs, bool isDamagedFolder);
    void CompareAndReplaceFiles();
    void LogMessage(const CString& message);
    void ExportLog();
    void UpdateProgress(size_t current, size_t total);
    void ResetState();
    CMFCEditBrowseCtrl m_editBrowseCtrl1;
    CMFCEditBrowseCtrl m_editBrowseCtrl2;
    CEdit m_editCtrl;
    CButton m_checkBox1;
    CButton m_checkBox2;
    CProgressCtrl m_progressCtrl;
    CButton m_button1;

    std::vector<std::wstring> m_filesA;
    std::vector<std::wstring> m_filesB;
    bool m_includeSubDirs;
    bool m_exportLog;
    std::atomic<bool> m_isComparing;
    std::thread m_compareThread;

    std::vector<std::wstring> m_initialDamagedFiles; // 保存初始的损坏文件列表
};
