#ifndef MAINWINDOW_H
#define MAINWINDOW_H

//#define WITH_ASM

//#define TRACE

//#define CMD_FILE

#include <stdio.h>
#include <QApplication>
#include <QtSql/QtSql>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QAbstractSocket>
#include <QMouseEvent>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QIODevice>
#include <QTextCodec>
#include <QMutex>
#include <QProgressBar>
#include <QFileDialog>

#include <QWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QTableWidgetItem>
#include <QtWidgets/QPushButton>

#ifdef CMD_FILE
    #include <fstream>
#endif

#include <iostream>


//********************************************************************************

const int max_st = 2048;
const int max_cmd = 8;
const int len_log_pas = 20;
extern unsigned char ver_prot;
extern unsigned char mysql;
//********************************************************************************

#pragma pack(push,1)
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char cmd;          // CMD      +12
    unsigned char bos;          // BOS      +13
} s_ack_hdr;//header 14 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char log[len_log_pas];      // login    +12,...,31
    unsigned char pas[len_log_pas];      // password +32,...,51
    unsigned char cmd;          // CMD      +52
} s_cmd_168;//53 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char log[len_log_pas];      // login    +12,...,31
    unsigned char pas[len_log_pas];      // password +32,...,51
    unsigned char cmd;          // CMD      +52
    unsigned char rk;           // rk       +53
    unsigned char line[31];     // code     +54,...,84
} s_cmd_169;//85 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char log[len_log_pas];      // login    +12,...,31
    unsigned char pas[len_log_pas];      // password +32,...,51
    unsigned char cmd;          // CMD      +52
    unsigned char rk;           // rk       +53
} s_cmd_176;//54 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char cmd;          // CMD      +12
    unsigned char bos;          // BOS      +13
    unsigned char done;         // last_sms +14
    unsigned char sms_len;      // body_len +15 //if (sms_type!=2)
    unsigned char sms_type;     // type     +16 //0-обычная, 1-часть, 2-склеенная, 255-квитанция
    unsigned char sms_num;      //номер смс +17
    unsigned char sms_cnt_part; //кол.частей+18
    unsigned char sms_part;     //номер.части+19
} s_ack_176;//header 20 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char log[len_log_pas];      // login    +12,...,31
    unsigned char pas[len_log_pas];      // password +32,...,51
    unsigned char cmd;          // CMD      +52
    unsigned char rk;           // rk       +53
    unsigned char mode;         // mode     +54
    unsigned char gsm[16];      // gsmnumber+55,...,70
    unsigned char body_len;     // body_len +71
    unsigned int index;         // index    +72,73,74,75
} s_cmd_167;//76 bytes + body
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char cmd;          // CMD      +12
    unsigned char bos;          // BOS      +13
} s_ack_167;//header 14 bytes
typedef struct
{
    unsigned char marker[4];    // 4 bytes  +0,1,2,3
    unsigned char ver;          // 1 byte   +4
    unsigned short len;         // 2 bytes  +5,6
    unsigned char ks1;          //ks1       +7
    unsigned short ks2;         //ks2       +8,9
    unsigned short unused;      //reserve   +10,11
    unsigned char cmd;          // CMD      +12
    unsigned char bos;          // BOS      +13
    unsigned int money;         // money    +14,15,16,17
    unsigned char min;          // min      +18
    unsigned char chas;         // chas     +19
    unsigned char day;          // day      +20
    unsigned char mes;          // mes      +21
} s_ack_161;//header 22 bytes
typedef struct
{
    unsigned char status;       // stat     +0
    unsigned char min;          // min      +1
    unsigned char chas;         // chas     +2
    unsigned char day;          // day      +3
    unsigned char mes;          // mes      +4
    unsigned char cmgs;         // cmgs     +5
} s_str_6;
typedef struct
{
    unsigned char status;       // stat     +0
    unsigned char check_min;    // min      +1
    unsigned char check_chas;   // chas     +2
    unsigned char check_day;    // day      +3
    unsigned char check_mes;    // mes      +4
    unsigned char add_min;      // min      +5
    unsigned char add_chas;     // chas     +6
    unsigned char add_day;      // day      +7
    unsigned char add_mes;      // mes      +8
} s_str_9;
#pragma pack(pop)
//********************************************************************************
namespace Ui {
class SmsWindow;
class SmsDialog;
}
using namespace std;
//********************************************************************************
#ifdef TRACE

class TrLog
{
public:

    TrLog(const char* fileName, const char* funcName, int lineNumber);
    ~TrLog();

private:

    const char *_fileName;
    const char *_funcName;
};

#define TRACER TrLog TrLog(__FILE__ , __FUNCTION__ , __LINE__);

#endif
//--------------------------------------------------------------
class SmsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SmsDialog(QWidget *parent = 0);
    ~SmsDialog();

private:

    Ui::SmsDialog *ui_dlg;

public slots:

    void OkSlot();
    void CancelSlot();

    friend class SmsWindow;
};
//--------------------------------------------------------------
class SmsWindow : public QMainWindow
{
    Q_OBJECT

public:

    class TheError {
        public :
            int code;
            TheError(int);
    };

    explicit SmsWindow(QWidget *parent = 0,
                       QString dbname = "sms",
                       QStringList *dblist = new QStringList (QStringList() << "localhost" << "3306" << "root" << ""));
    ~SmsWindow();

    int GetTick();
    void SetTick(int t);
    void timerEvent(QTimerEvent *);
    void GetAdrPort();
    void NextRec();
    void SetDBName(QString &);
    void SetSeparators(QString &, QString &);
    void DeleteRec(const int nr);
    void SelectServer(QString &, unsigned short);
    void buttonClickedSlot(const QString *);
    int ReadDB(QString *, const int);
    int TotalRecords();
    void UpdateTitle();
    void InitWaitBar();
    void DelWaitBar();

public slots:
    void TimeOut();
    void About();
    void ClkSend();
    void cmd_176();
    void cmd_167();
    void cmd_168();
    void cmd_169();
    void cmd_161();
    void cmd_170();
    void cmd_171();
    void cmd_172();
    void prepare_to_readDB();
    void connectTcp();
    void disconnectTcp();
    void SokError(QAbstractSocket::SocketError SocketError);
    void readData();
    void DlgEndOk();
    void NextRecDown();
    void NextRecUp();
    void DelRecord();
    void RowNum(int, int) const;

private:
    char *st;
    unsigned short srv_port;
    int tick, rec_count, current, tmr, MyError, total_records;
    unsigned int Cnt_send;//send counter
    struct tm *tim;//for convert time (localtime)
    unsigned char RK;
    bool openok;
    Ui::SmsWindow *ui;//interface form
    QString srv_adr, db_name, db_host, sep, sepl;
    SmsDialog *Dlg;
    QTcpSocket *_pSocket;
    QByteArray data, rdata;
    QSqlDatabase db;
    QDialog *wnd;
    QPushButton *bnext_down, *bnext_up, *b_del;
    QTableWidget *tbl;
    QLineEdit *from_rec_w;
    QLabel *nrec;
    QMutex mutex;//mutex_lock/unlock for tick
    QProgressBar *wait_bar;
};

#endif // MAINWINDOW_H
