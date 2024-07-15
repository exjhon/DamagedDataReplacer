
// DamagedDataReplacerDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "DamagedDataReplacer.h"
#include "DamagedDataReplacerDlg.h"
#include "afxdialogex.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <windows.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <Shellapi.h>
#include <sstream>
#include <vector>
#include <winbase.h>
#include <tchar.h>
#include <shlwapi.h>
#include <chrono>
#include <thread>
#include <atomic>


#ifdef _DEBUG
#define new DEBUG_NEW
#endif



namespace fs = std::filesystem;
// CDamagedDataReplacerDlg 对话框

CDamagedDataReplacerDlg::CDamagedDataReplacerDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_DAMAGEDDATAREPLACER_DIALOG, pParent)
    , m_includeSubDirs(true)
    , m_exportLog(true)
    , m_isComparing(false)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CDamagedDataReplacerDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_MFCEDITBROWSE1, m_editBrowseCtrl1);
    DDX_Control(pDX, IDC_MFCEDITBROWSE2, m_editBrowseCtrl2);
    DDX_Control(pDX, IDC_EDIT1, m_editCtrl);
    DDX_Control(pDX, IDC_CHECK1, m_checkBox1);
    DDX_Control(pDX, IDC_CHECK2, m_checkBox2);
    DDX_Control(pDX, IDC_PROGRESS1, m_progressCtrl);
    DDX_Control(pDX, IDC_BUTTON1, m_button1);
}

BEGIN_MESSAGE_MAP(CDamagedDataReplacerDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON1, &CDamagedDataReplacerDlg::OnBnClickedButton1)
    ON_BN_CLICKED(IDC_CHECK1, &CDamagedDataReplacerDlg::OnBnClickedCheck1)
    ON_BN_CLICKED(IDC_CHECK2, &CDamagedDataReplacerDlg::OnBnClickedCheck2)
END_MESSAGE_MAP()


// CDamagedDataReplacerDlg 消息处理程序

BOOL CDamagedDataReplacerDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作

    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    SetIcon(m_hIcon, TRUE);			// 设置大图标
    SetIcon(m_hIcon, FALSE);		// 设置小图标

    if (!IsUserAnAdmin) {
        RequestAdminPrivileges();
        ExitProcess(0);
    }
    // 初始化控件
    m_editBrowseCtrl1.EnableFolderBrowseButton();
    m_editBrowseCtrl2.EnableFolderBrowseButton();
    m_checkBox1.SetCheck(BST_CHECKED);
    m_checkBox2.SetCheck(BST_CHECKED);


    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CDamagedDataReplacerDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // 用于绘制的设备上下文

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // 使图标在工作区矩形中居中
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // 绘制图标
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CDamagedDataReplacerDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}


void CDamagedDataReplacerDlg::OnBnClickedButton1()
{
    if (m_isComparing)
    {
        // 比对过程中按钮不可用
        return;
    }

    ResetState();

    CString folderA, folderB;
    m_editBrowseCtrl1.GetWindowText(folderA);
    m_editBrowseCtrl2.GetWindowText(folderB);

    if (folderA.IsEmpty() || folderB.IsEmpty())
    {
        AfxMessageBox(_T("请输入有效的文件夹路径。"));
        return;
    }

    m_isComparing = true;
    m_button1.EnableWindow(FALSE); // 禁用按钮
    m_button1.SetWindowText(_T("文件比对中…"));
    m_editCtrl.SetWindowText(_T(""));

    m_filesA.clear();
    m_filesB.clear();
    m_initialDamagedFiles.clear();

    LogMessage(_T("正在遍历全部文件…"));
    TraverseFolder(folderA, m_filesA, m_includeSubDirs, true);
    TraverseFolder(folderB, m_filesB, true, false);

    // 保存初始的损坏文件列表
    m_initialDamagedFiles = m_filesA;

    m_compareThread = std::thread(&CDamagedDataReplacerDlg::CompareAndReplaceFiles, this);
}

void CDamagedDataReplacerDlg::ResetState()
{
    if (m_compareThread.joinable())
    {
        m_compareThread.join();
    }
    m_isComparing = false;
    m_button1.EnableWindow(TRUE); // 启用按钮
    m_button1.SetWindowText(_T("开始比对"));
    m_progressCtrl.SetPos(0);
    m_editCtrl.SetWindowText(_T(""));
}

void CDamagedDataReplacerDlg::CompareAndReplaceFiles()
{
    size_t totalFiles = m_initialDamagedFiles.size();
    size_t perfectMatches = 0;
    size_t multipleMatches = 0;
    size_t unmatchedFiles = 0;

    for (size_t i = 0; i < totalFiles && m_isComparing; ++i)
    {
        const auto& damagedFile = m_initialDamagedFiles[i];
        fs::path pathA(damagedFile);
        auto sizeA = fs::file_size(pathA);

        // 找到最后一个点的位置来获取后缀名
        std::wstring filenameA = pathA.filename().wstring();
        size_t posA = filenameA.rfind(L'.');
        std::wstring extA = (posA != std::wstring::npos) ? filenameA.substr(posA) : L"";

        // 将后缀名转换为小写
        std::transform(extA.begin(), extA.end(), extA.begin(), ::towlower);

        std::vector<std::wstring> matches;
        for (const auto& fileB : m_filesB)
        {
            fs::path pathB(fileB);
            std::wstring filenameB = pathB.filename().wstring();
            size_t posB = filenameB.rfind(L'.');
            std::wstring extB = (posB != std::wstring::npos) ? filenameB.substr(posB) : L"";

            std::transform(extB.begin(), extB.end(), extB.begin(), ::towlower);

            if (extB == extA && fs::file_size(pathB) == sizeA)
            {
                matches.push_back(fileB);
            }
        }

        if (matches.size() == 1)
        {
            perfectMatches++;
            fs::copy(matches[0], damagedFile, fs::copy_options::overwrite_existing);
            fs::last_write_time(damagedFile, fs::last_write_time(pathA));
            LogMessage(CString(_T("从")) + matches[0].c_str() + _T("替换") + damagedFile.c_str());
        }
        else if (matches.size() > 1)
        {
            multipleMatches++;
            for (size_t j = 0; j < matches.size(); ++j)
            {
                fs::path newFileA = pathA;
                newFileA.replace_filename(pathA.stem().wstring() + L"(" + std::to_wstring(j + 1) + L")" + pathA.extension().wstring());
                fs::copy(matches[j], newFileA, fs::copy_options::overwrite_existing);
                fs::last_write_time(newFileA, fs::last_write_time(pathA));
                LogMessage(CString(_T("从")) + matches[j].c_str() + _T("替换") + newFileA.wstring().c_str());
            }
        }
        else
        {
            unmatchedFiles++;
            LogMessage(CString(_T("未找到匹配文件：")) + damagedFile.c_str());
        }

        // 更频繁地更新进度条
        if (i % 10 == 0 || i == totalFiles - 1) // 每处理10个文件或最后一个文件时更新一次
        {
            UpdateProgress(i + 1, totalFiles);
        }
    }

    CString summary;
    summary.Format(_T("比对完成。总用时：%d秒，被比对文件数量：%zu，完美替换文件数量：%zu，多个替换文件数量：%zu，未替换文件数量：%zu。"),
        0, totalFiles, perfectMatches, multipleMatches, unmatchedFiles); // 这里的总用时可以根据实际情况计算
    LogMessage(summary);

    if (m_exportLog)
    {
        ExportLog();
    }

    m_isComparing = false;
    m_button1.EnableWindow(TRUE); // 启用按钮
    m_button1.SetWindowText(_T("开始比对"));
}


void CDamagedDataReplacerDlg::OnBnClickedCheck1()
{
    m_includeSubDirs = (m_checkBox1.GetCheck() == BST_CHECKED);
}

void CDamagedDataReplacerDlg::OnBnClickedCheck2()
{
    m_exportLog = (m_checkBox2.GetCheck() == BST_CHECKED);
}

void CDamagedDataReplacerDlg::TraverseFolder(const CString& folderPath, std::vector<std::wstring>& fileList, bool includeSubDirs, bool isDamagedFolder)
{
    if (isDamagedFolder && !includeSubDirs)
    {
        for (const auto& entry : fs::directory_iterator(folderPath.GetString()))
        {
            if (entry.is_regular_file())
            {
                fileList.push_back(entry.path().wstring());
            }
        }
    }
    else
    {
        for (const auto& entry : fs::recursive_directory_iterator(folderPath.GetString()))
        {
            if (entry.is_regular_file())
            {
                fileList.push_back(entry.path().wstring());
            }
        }
    }
}



void CDamagedDataReplacerDlg::LogMessage(const CString& message)
{
    CString currentText;
    m_editCtrl.GetWindowText(currentText);
    currentText += message + _T("\r\n");
    m_editCtrl.SetWindowText(currentText);
}

void CDamagedDataReplacerDlg::ExportLog()
{
    CString logText;
    m_editCtrl.GetWindowText(logText);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    struct tm buf;
    localtime_s(&buf, &in_time_t);

    CString logFileName;
    logFileName.Format(_T("log_%04d%02d%02d_%02d%02d%02d.txt"),
        buf.tm_year + 1900, buf.tm_mon + 1, buf.tm_mday,
        buf.tm_hour, buf.tm_min, buf.tm_sec);

    std::ofstream logFile(logFileName);
    logFile << CT2A(logText.GetString());
    logFile.close();
}

void CDamagedDataReplacerDlg::UpdateProgress(size_t current, size_t total)
{
    int progress = static_cast<int>((static_cast<double>(current) / total) * 100);
    m_progressCtrl.SetPos(progress);
}


void CDamagedDataReplacerDlg::RequestAdminPrivileges()
{
    // 获取程序路径
    CString strAppPath;
    GetModuleFileName(NULL, strAppPath.GetBuffer(MAX_PATH), MAX_PATH);
    strAppPath.ReleaseBuffer();

    // 准备启动信息
    SHELLEXECUTEINFO shellInfo = { sizeof(SHELLEXECUTEINFO) };
    shellInfo.lpVerb = _T("runas");
    shellInfo.lpFile = strAppPath;
    shellInfo.nShow = SW_SHOWNORMAL;

    // 启动程序，请求管理员权限
    if (!ShellExecuteEx(&shellInfo))
    {
        AfxMessageBox(_T("请求管理员权限失败！"));
    }
}
