#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "ui_cmd_all.h"

const QString cmd_name[max_cmd] = {"167", "168", "176", "169", "161", "170", "171", "172"};
const QString cmd_help[max_cmd] = {"Init send", "Status send", "Read SMS",
                           "Init check Balance", "Check balance",
                           "Init add balance", "Read balance status",
                           "Read balance status ext."};
const char * mk_table_db3 = "CREATE TABLE IF NOT EXISTS sms ("\
        "number INTEGER primary key autoincrement," \
        "s_rk INTEGER," \
        "s_len INTEREG," \
        "s_type INTEGER," \
        "s_num INTEGER," \
        "s_part INTEGER," \
        "s_cnt_part INTEGER," \
        "s_body TEXT," \
        "s_epoch TIMESTAMP);";
const char * mk_table_mysql = "CREATE TABLE `sms` ("\
        "`number` INT(11) PRIMARY KEY NOT NULL AUTO_INCREMENT,"\
        "`s_rk` INT(2),"\
        "`s_len` INT(4),"\
        "`s_type` INT(2),"\
        "`s_num` INT(2),"\
        "`s_part` INT(2),"\
        "`s_cnt_part` INT(2),"\
        "`s_body` TEXT COLLATE utf8_unicode_ci,"\
        "`s_epoch` INT(10)) ENGINE=MyISAM DEFAULT CHARSET=utf8 COLLATE utf8_unicode_ci;";

enum btis {No, Ok, Can};
static int btn = No;          // флаг кнопки окна диалога
static int CMD;               // код команды для сервера
const int wait_sec = 5;       // ждем успешного соединения с сервером
const int max_cnt = 25;       // максимальное количество выводимых записей из БД
static int NoDlg = 1;         // флаг окна диалога, по умолчанию - окно не создано
unsigned char ver_prot = 2;   // версия протокола обмена с сервером, по умолчанию - 2
unsigned char mysql = 0;      // тип базы данных, по умолчанию - sqlite3
static unsigned int sms_index = 0;
//static unsigned int cnt_inline = 0;

const unsigned char BIT_FLASH   = 0x80;
const unsigned char BIT_UCS2    = 0x01;
const unsigned char BIT_GSM7BIT = 0x02;
const unsigned char BIT_GSM8BIT = 0x04;
const unsigned char BIT_CDS     = 0x40;

//const char *ver = "4.3";//add __asm()
//const char *ver = "4.4";
const QString ver = "v4.5";


#ifdef CMD_FILE
const char *fd_name = "cmd.bin";
#endif

#ifdef TRACE
static QByteArray sps = "";
#endif

#ifndef WITH_ASM
//--------------------------------------------------------------------------------
template <class Tips>
inline Tips ksum(Tips n, unsigned char *uk, int len)
{
    n = 0;
    for (int i = 0; i < len; i++) n += *(uk + i);
    return n;
}
#else
//--------------------------------------------------------------------------------
template <class Tips>
inline Tips ksum(Tips n, unsigned char *uk, int len)
{
    unsigned int sz = sizeof(n), res = 0, mask = 0xFFFFFFFF;
    if (sz == 1) mask = 0xFF;
    else
    if (sz == 2) mask = 0xFFFF;
    Tips rt;
    //char labelka[256] = {0};
    //cnt_inline++;

    __asm("pushl %%ebx\n\t\
          movl %1, %%edx\n\t\
          movl %2, %%ecx\n\t\
          xorl %%eax, %%eax\n\t\
          xorl %%ebx, %%ebx\n\
          lab:\n\
          movb (%%edx), %%bl\n\t\
          addl %%ebx, %%eax\n\t\
          incl %%edx\n\t\
          loop lab\n\t\
          popl %%ebx\n\t\
          andl %3, %%eax\n\t\
          movl %%eax, %0\n"
            : "=g" (res)
            : "g" (uk), "g" (len), "g" (mask)
            : "%edx","%ecx","%eax");

    rt = res;
     return rt;
}
#endif
//--------------------------------------------------------------------------------
#ifdef TRACE
//                          class TLog
// Конструктор
TrLog::TrLog(const char *fileName, const char *funcName, int lineNumber)
{
    _fileName = fileName;
    _funcName = funcName;
    cerr << sps.data() << "Entering " << _funcName << "() - (" << _fileName << ":" << lineNumber << ")\n";
    sps.append("  ");

}
// Деструктор
TrLog::~TrLog()
{
    sps.resize(sps.length() - 2);
    cerr << sps.data() << "Leaving  " << _funcName << "() - (" << _fileName << ")\n";
}
#endif
//--------------------------------------------------------------------------------
//                          class SmsDialog
SmsDialog::SmsDialog(QWidget *parent) : QDialog(parent), ui_dlg(new Ui::SmsDialog)
{
    NoDlg = 0;
    btn = No;
    ui_dlg->setupUi(this);
    connect(ui_dlg->Ok_Cancel, SIGNAL(accepted()), SLOT(OkSlot()));
    connect(ui_dlg->Ok_Cancel, SIGNAL(rejected()), SLOT(CancelSlot()));
}
SmsDialog::~SmsDialog()
{
    NoDlg = 1;
    delete ui_dlg;
    delete this;
}
void SmsDialog::OkSlot() {
    btn = Ok;
}
void SmsDialog::CancelSlot() {
    btn = Can;
}
//
//--------------------------------------------------------------------------------
//                         clss SmsWindow
SmsWindow::SmsWindow(QWidget *parent, QString dbname, QStringList *dblist) :
                            QMainWindow(parent), ui(new Ui::SmsWindow)
{
    tick = tmr = 0;   //TimerID -> 0
    Cnt_send = 0;     //Message counter
    tim = NULL;       //pointer to struct tm
    MyError = 0;      //Code error for catch block
    st = NULL;        //pointer to temp char_string
    _pSocket = NULL;
    srv_adr = "127.0.0.1";
    srv_port = 9005;
    Dlg = NULL;
    RK = 0;
    rec_count = current = 0;
    total_records = -1;
    openok = false;
    QString tbl_stat, temp, dbs;
    wnd = NULL;
    wait_bar = NULL;

    ui->setupUi(this);
    this->setFixedSize(this->size());

    SetDBName(dbname);

    if (mysql) {// MYSQL
        tbl_stat = "mysql DB " + db_name + " : ";
/*
        QStringList list = QSqlDatabase::drivers( );
        for(int i = 0; i < list.size( ); i++) qDebug() << list[i];
*/
        db = QSqlDatabase::addDatabase("QMYSQL");
        db.setHostName(dblist->at(0));//host
        dbs = dblist->at(1);//port
        bool good;
        int prt = dbs.toInt(&good,10);
        if (!good) prt = 3306;
        db.setPort(prt);
        db.setDatabaseName(db_name);
        db.setUserName(dblist->at(2));//user
        db.setPassword(dblist->at(3));//password
        QSqlQuery query(db);
        openok = db.open();
        if (!openok) {
            db.setDatabaseName("test");
            if (db.open()) {
                temp = "CREATE DATABASE "+db_name+";";
                if (query.exec(temp)) {
                    db.close();
                    db.setDatabaseName(db_name);
                    openok = db.open();
                    if (openok) {
                        if (query.exec(mk_table_mysql)) openok = true;
                        else {
                            tbl_stat += " | Error create table sms : "+query.lastError().text();
                            openok = false;
                        }
                    } else tbl_stat += " | Error open : " + db.lastError().text();
                } else tbl_stat += " | Error create DB : " + query.lastError().text();
            } else {
                tbl_stat += " | Error open DB test (mysqld not started): " + db.lastError().text();
                openok = false;
            }
        }
    } else {// SQLITE3
        tbl_stat = "sqlite3 DB " + db_name + " : ";
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(db_name);
        openok = db.open();
        QSqlQuery query(db);
        if (query.exec(mk_table_db3)) tbl_stat += "Table sms present";
                                else  tbl_stat += "Warning : " + query.lastError().text();
    }

    if (openok) {// DB открыта, с ней можно работать
        QSqlQuery query(db);
        if (query.exec("select * from sms order by number desc limit 1;")) {
            while (query.next()) {
                QString ttt = query.value(0).toString();
                bool gd;
                rec_count = ttt.toInt(&gd,10);
                if (!gd) rec_count = 0;
                tbl_stat += " | Last record " + ttt;
                break;
            }
        } else {
            tbl_stat += " | Error : "+query.lastError().text();
            qDebug() << "Error select * from sms" << query.lastError().text();
        }
        current=rec_count;
    }// else tbl_stat += "DB ERROR : "+db.lastError().text();

    ui->db_w->setText(tbl_stat);

    st = (char *)calloc(1, max_st);//get memory for temp char_string and fill zero
    if (!st) {
        MyError |= 1;//memory error
        throw TheError(MyError);
    }

    tmr = startTimer(1000);
    if (tmr <= 0) {
        MyError |= 2;//timer error
        throw TheError(MyError);
    }

    // прикручиваем сигнал к слоту
    //connect(ui->pbSend, SIGNAL(clicked()), this, SLOT(ClkSend()));
    connect(ui->pbSend, &QPushButton::clicked, this, &SmsWindow::ClkSend);
    //Menu
    connect(ui->action176_Read, SIGNAL(triggered(bool)), this, SLOT(cmd_176()));
    connect(ui->action167_Init_send, SIGNAL(triggered(bool)), this, SLOT(cmd_167()));
    connect(ui->action168_Status_send, SIGNAL(triggered(bool)), this, SLOT(cmd_168()));
    connect(ui->action169_Init_check_balance, SIGNAL(triggered(bool)), this, SLOT(cmd_169()));
    connect(ui->action161_Check_balance, SIGNAL(triggered(bool)), this, SLOT(cmd_161()));
    connect(ui->action170_Init_add_balance, SIGNAL(triggered(bool)), this, SLOT(cmd_170()));
    connect(ui->action171_Read_balance_status, SIGNAL(triggered(bool)), this, SLOT(cmd_171()));
    connect(ui->action172_Read_balance_status_ext, SIGNAL(triggered(bool)), this, SLOT(cmd_172()));
    connect(ui->actionVersion, SIGNAL(triggered(bool)), this, SLOT(About()));
    //DB dialog
    connect(ui->actionRead , SIGNAL(triggered(bool)), this, SLOT(prepare_to_readDB()));

    _pSocket = new QTcpSocket(this);
    if (!_pSocket) {
        MyError |= 0x20;//create socket error - no memory
        throw TheError(MyError);
    }

    UpdateTitle();

    //инициализация некоторых элементов главного окна программы
    statusBar()->showMessage(trUtf8("Готов :"));
    sprintf(st, "%d", Cnt_send);
    ui->kol_w->setText(st);

}
//--------------------------------------------------------------------------------
SmsWindow::~SmsWindow()
{

    DelWaitBar();
    if (_pSocket->isOpen()) _pSocket->close();
    delete _pSocket;
    if (db.isOpen()) db.close();
    if (wnd) {
        delete wnd;
        wnd = NULL;
    }
    delete ui;
    if (tmr > 0) killTimer(tmr);
    if (st) free(st);
}
//--------------------------------------------------------------------------------
int SmsWindow::TotalRecords()
{
int ret = -1;

    QSqlQuery query(db);
    if (query.exec("select count(*) from sms;")) {
        bool gd = false;
        query.first();
        ret = query.value(0).toInt(&gd);
        if (!gd) ret = -1;
    } else ret = -1;

    return ret;
}
//--------------------------------------------------------------------------------
void SmsWindow::UpdateTitle()
{
QString tit = "SMS client " + ver + " | server - " + ui->ip_w->text();

    total_records = TotalRecords();
    setWindowTitle(tit);

}
//--------------------------------------------------------------------------------
void SmsWindow::About()
{
QString stx;

    stx.append("SMS клиент для устройств g20/i32\nВерсия " + ver + "\nТекущая БД '"+ db_name+"' ");
    if (mysql) {
        stx.append("(mysql)\n    host : " + db.hostName() + ":" + QString::number(db.port()) + "\n");
        stx.append("    user '%s'" + db.userName() + " pas '%s'" + db.password());
    } else stx.append("(sqlite3)");
    stx.append("\nСервер : " + ui->ip_w->text());

    QMessageBox::information(this, "About this", stx);
}
//--------------------------------------------------------------------------------
void SmsWindow::SelectServer(QString &adr, unsigned short port)
{
    srv_adr = adr;
    srv_port = port;
    QString tp = "";
    tp.sprintf(":%d", port);
    tp.insert(0, adr);
    ui->ip_w->setText(tp);
}
//--------------------------------------------------------------------------------
void SmsWindow::SetDBName(QString &new_db_name)
{
    db_name = new_db_name;
}
//--------------------------------------------------------------------------------
void SmsWindow::SetSeparators(QString &sep1, QString &sep2)
{
    sep = sep1; sepl = sep2;
}
//--------------------------------------------------------------------------------
void SmsWindow::cmd_176() { ui->cmd_w->setText(cmd_name[2]); buttonClickedSlot(&cmd_help[2]); }
void SmsWindow::cmd_167() { ui->cmd_w->setText(cmd_name[0]); buttonClickedSlot(&cmd_help[0]); }
void SmsWindow::cmd_168() { ui->cmd_w->setText(cmd_name[1]); buttonClickedSlot(&cmd_help[1]); }
void SmsWindow::cmd_169() { ui->cmd_w->setText(cmd_name[3]); buttonClickedSlot(&cmd_help[3]); }
void SmsWindow::cmd_161() { ui->cmd_w->setText(cmd_name[4]); buttonClickedSlot(&cmd_help[4]); }
void SmsWindow::cmd_170() { ui->cmd_w->setText(cmd_name[5]); buttonClickedSlot(&cmd_help[5]); }
void SmsWindow::cmd_171() { ui->cmd_w->setText(cmd_name[6]); buttonClickedSlot(&cmd_help[6]); }
void SmsWindow::cmd_172() { ui->cmd_w->setText(cmd_name[7]); buttonClickedSlot(&cmd_help[7]); }
//--------------------------------------------------------------------------------
void SmsWindow::GetAdrPort()
{
QString sp;
QByteArray mas(ui->ip_w->text().toLocal8Bit());
char *tp = NULL, *uk = NULL;

    sp.clear();
    tp = mas.data();
    uk = strchr(tp, ':');
    if (uk) {
        srv_port = atoi(uk + 1);
        sp.append(uk + 1);
        *(uk) = 0;
        srv_adr.clear();
        srv_adr.append(tp);
        UpdateTitle();
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::DlgEndOk() //нажата клавиша из OkCancel
{

    sprintf(st, "Command %d", CMD);

    switch (btn) {
        case Ok :
            statusBar()->showMessage(trUtf8("Wait answer..."));
            GetAdrPort();
            if (!NoDlg) {
                ui->cmd_w->setText(Dlg->ui_dlg->cmd_d_w->displayText());
                ui->lgn_w->setText(Dlg->ui_dlg->lgn_d_w->displayText());
                ui->pwd_w->setText(Dlg->ui_dlg->pwd_d_w->displayText());
                if ((CMD != 168) && (CMD != 171)) ui->rk_w->setText(Dlg->ui_dlg->rk_d_w->displayText());
                if (CMD==167) {
                    ui->subs_w->setText(Dlg->ui_dlg->num_d_w->displayText());
                    ui->cyr_w->setText(Dlg->ui_dlg->cyr_d_w->displayText());
                    ui->flash_w->setText(Dlg->ui_dlg->flash_d_w->displayText());
                    ui->mess_w->setText(Dlg->ui_dlg->text_d_w->toPlainText());
                }
                if ((CMD == 169) || (CMD == 170)) ui->sim_w->setText(Dlg->ui_dlg->sim_d_w->displayText());
            }
            SetTick(wait_sec);

            InitWaitBar();

            if (_pSocket->isOpen()) _pSocket->close();
            connect(_pSocket, SIGNAL(connected()), this, SLOT(connectTcp()));
            connect(_pSocket, SIGNAL(disconnected()), this, SLOT(disconnectTcp()));
            connect(_pSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(SokError(QAbstractSocket::SocketError)));
            connect(_pSocket, SIGNAL(readyRead()), this, SLOT(readData()));
            _pSocket->connectToHost(srv_adr, srv_port);
        break;
        case Can :
            statusBar()->showMessage(trUtf8(st) + ", <Cancel>");
        break;
            default : statusBar()->showMessage(trUtf8(st) + ", <Unknown>");
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::InitWaitBar()
{
    DelWaitBar();
    wait_bar = new QProgressBar(this);
    if (wait_bar) {
        QRect rr = this->ui->answer_w->geometry();
        wait_bar->setGeometry(rr.left() + rr.width()/2 - 100, rr.height()-60, 200, 20);
        wait_bar->setRange(0, wait_sec);
        wait_bar->setValue(0);
        wait_bar->setTextVisible(true);
        wait_bar->show();
    } else {
        MyError |= 1;//memory error
        throw TheError(MyError);
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::DelWaitBar()
{
    if (wait_bar) {
        delete wait_bar;
        wait_bar = NULL;
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::ClkSend() {
    btn = Ok;
    QString qst = ui->cmd_w->text();
    if (qst != "") {
        bool ok;
        CMD = qst.toInt(&ok, 10);
        if (!ok) {
            MyError |= 4;//"atoi" error
            throw TheError(MyError);
        } else {
            NoDlg = 1;
            DlgEndOk();
        }
    }
}
//--------------------------------------------------------------------------------
// Конструктор класса ошибок
SmsWindow::TheError::TheError(int err) { code = err; }
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//метод обработки нажатия кнопки Send
void SmsWindow::buttonClickedSlot(const QString *hdr) {

    QString qst = ui->cmd_w->text();

    if (qst != "") {
        bool ok;
        CMD = qst.toInt(&ok, 10);
        if (!ok) {
            MyError |= 4;//"atoi" error
            throw TheError(MyError);
        }
        int rdy = 0;
        Dlg = new SmsDialog;
        if (!Dlg) {
            MyError |= 1;//memory error
            throw TheError(MyError);
        }
        rdy = 1;
        switch (CMD) {
            case 167:// Init send
                Dlg->ui_dlg->cmd_d_w->setText(ui->cmd_w->displayText());
                Dlg->ui_dlg->rk_d_w->setText(ui->rk_w->displayText());
                Dlg->ui_dlg->num_d_w->setText(ui->subs_w->displayText());
                Dlg->ui_dlg->cyr_d_w->setText(ui->cyr_w->displayText());
                Dlg->ui_dlg->flash_d_w->setText(ui->flash_w->displayText());
                Dlg->ui_dlg->lgn_d_w->setText(ui->lgn_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::Normal);
                Dlg->ui_dlg->pwd_d_w->setText(ui->pwd_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                Dlg->ui_dlg->sim_d_w->hide();
                Dlg->ui_dlg->text_d_w->setText(ui->mess_w->toPlainText());
            break;
            case 168:// Status send
            case 171:// Read balance status
                Dlg->ui_dlg->cmd_d_w->setText(ui->cmd_w->displayText());
                Dlg->ui_dlg->rk_d_w->hide();
                Dlg->ui_dlg->num_d_w->hide();
                Dlg->ui_dlg->cyr_d_w->hide();
                Dlg->ui_dlg->flash_d_w->hide();
                Dlg->ui_dlg->lgn_d_w->setText(ui->lgn_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::Normal);
                Dlg->ui_dlg->pwd_d_w->setText(ui->pwd_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                Dlg->ui_dlg->sim_d_w->hide();
                Dlg->ui_dlg->text_d_w->hide();
            break;
            case 176:// Read SMS
            case 161:// Check balance
            case 172:// Read balance status ext
                Dlg->ui_dlg->cmd_d_w->setText(ui->cmd_w->displayText());
                Dlg->ui_dlg->rk_d_w->setText(ui->rk_w->displayText());
                Dlg->ui_dlg->num_d_w->hide();
                Dlg->ui_dlg->cyr_d_w->hide();
                Dlg->ui_dlg->flash_d_w->hide();
                Dlg->ui_dlg->lgn_d_w->setText(ui->lgn_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::Normal);
                Dlg->ui_dlg->pwd_d_w->setText(ui->pwd_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                Dlg->ui_dlg->sim_d_w->hide();
                Dlg->ui_dlg->text_d_w->hide();
            break;
            case 169:// Check init balance
                Dlg->ui_dlg->cmd_d_w->setText(ui->cmd_w->displayText());
                Dlg->ui_dlg->rk_d_w->setText(ui->rk_w->displayText());
                Dlg->ui_dlg->num_d_w->hide();
                Dlg->ui_dlg->cyr_d_w->hide();
                Dlg->ui_dlg->flash_d_w->hide();
                Dlg->ui_dlg->lgn_d_w->setText(ui->lgn_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::Normal);
                Dlg->ui_dlg->pwd_d_w->setText(ui->pwd_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                Dlg->ui_dlg->sim_d_w->setText(ui->sim_w->displayText());
                Dlg->ui_dlg->text_d_w->hide();
            break;
            case 170:// Init add balance
                Dlg->ui_dlg->cmd_d_w->setText(ui->cmd_w->displayText());
                Dlg->ui_dlg->rk_d_w->setText(ui->rk_w->displayText());
                Dlg->ui_dlg->num_d_w->hide();
                Dlg->ui_dlg->cyr_d_w->hide();
                Dlg->ui_dlg->flash_d_w->hide();
                Dlg->ui_dlg->lgn_d_w->setText(ui->lgn_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::Normal);
                Dlg->ui_dlg->pwd_d_w->setText(ui->pwd_w->displayText());
                ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
                Dlg->ui_dlg->sim_d_w->setText(ui->sim_w->displayText());
                Dlg->ui_dlg->text_d_w->hide();
            break;
                default : rdy = 0;
        }

        if (rdy) {
            connect(Dlg->ui_dlg->Ok_Cancel , SIGNAL(accepted()) , this , SLOT(DlgEndOk()));
            connect(Dlg->ui_dlg->Ok_Cancel , SIGNAL(rejected()) , this , SLOT(DlgEndOk()));
            Dlg->setWindowTitle(qst + " / " + *hdr);
            Dlg->show();
        }
    }

}
//--------------------------------------------------------------------------------
int SmsWindow::GetTick()
{
int t;

    mutex.lock();
        t = tick;
    mutex.unlock();
    return t;
}
//--------------------------------------------------------------------------------
void SmsWindow::SetTick(int t)
{
    mutex.lock();
        tick = t;
    mutex.unlock();
}
//--------------------------------------------------------------------------------
// метод обработки события от таймера (таймаут обработчик)
void SmsWindow::timerEvent(QTimerEvent *event)
{
    if (tmr == event->timerId()) {
        time_t ttm = QDateTime::currentDateTime().toTime_t();
        tim = localtime(&ttm);
        QString *stk = new QString;
        if (stk) {
            stk->clear();
            stk->sprintf("%02d.%02d.%04d  %02d:%02d:%02d",
                tim->tm_mday, tim->tm_mon + 1, tim->tm_year + 1900,
                tim->tm_hour, tim->tm_min, tim->tm_sec);
            ui->tmr_w->setText(*stk);
            delete stk;
        }
        bool ti = false;
        mutex.lock();
        ti = true;
        if (tick > 0) {
            tick--;
            if (wait_bar) wait_bar->setValue(wait_sec-tick);
            if (tick <= 0) {
                tick = 0;
                mutex.unlock();
                ti = false;
                if (_pSocket->isOpen()) disconnectTcp();
                TimeOut();
            }
        }
        if (ti) mutex.unlock();
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::TimeOut()
{
    statusBar()->showMessage("!!! TimeOut socket ERROR !!!");
    DelWaitBar();
    QMessageBox::information(0,"Information message","Timeout Socket !");
}
//--------------------------------------------------------------------------------
//-----  TCP   ---------------------------
void SmsWindow::SokError(QAbstractSocket::SocketError SocketError)
{
    SetTick(0);
    DelWaitBar();
    QString *qs = new QString;
    if (!qs) {
        MyError |= 1;//memory error
        throw TheError(MyError);
    }
    qs->sprintf("Socket ERROR (%d) : server ", SocketError);
    qs->append(ui->ip_w->text());
    qs->append(" <- ");
    if (_pSocket) {
        qs->append(_pSocket->errorString());
        disconnectTcp();
        statusBar()->showMessage(*qs);
        QMessageBox::information(this, "ERROR socket", *qs);
    } else {
        disconnectTcp();
        statusBar()->showMessage(*qs + "unknown error");
    }
    delete qs;
}
//--------------------------------------------------------------------------------
void SmsWindow::connectTcp()
{
unsigned char *uki = NULL, *bu = NULL;
int dl, sz, pint;
bool ready = false, done;
unsigned char k8 = 0, byte = 0;
unsigned short k16 = 0, ks16 = 0;
QString tmp, tp;
QByteArray text, auth;
s_cmd_176 *buf_176 = NULL;
s_cmd_167 *buf_167 = NULL;
s_cmd_168 *buf_168 = NULL;
s_cmd_169 *buf_169 = NULL;

    DelWaitBar();

    switch (CMD) {
        case 176 :
        case 161 :
        case 172 :
            sz=sizeof(s_cmd_176);
            //bu = (unsigned char *)calloc(1,sz);
            bu = reinterpret_cast<unsigned char *>(calloc(1,sz));
            if (!bu) break;
            buf_176 = (s_cmd_176 *)bu;

            tmp.clear();
            memcpy((unsigned char *)buf_176, "abcd", 4);
            //memcpy(reinterpret_cast<unsigned char *>(buf_176),"abcd",4);
            buf_176->ver = ver_prot;
            buf_176->len = (unsigned short)(sz - 8);
            k8 = 0; buf_176->ks1 = ksum(k8, (unsigned char *)bu, 7);
            // login
            auth.clear();
            tmp = ui->lgn_w->text();
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ', true);
            auth.append(tmp);
            uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_176->log, (unsigned char *)uki, len_log_pas);
            // password
            auth.clear();
            ui->pwd_w->setEchoMode(QLineEdit::Normal);
            tmp = ui->pwd_w->text();
            ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ');
            auth.append(tmp);
            uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_176->pas, (unsigned char *)uki, len_log_pas);

            buf_176->cmd = CMD;

            tmp = ui->rk_w->text();
            buf_176->rk = (unsigned char)tmp.toInt(&done, 10);
            if (buf_176->rk > 0) buf_176->rk--;
            RK = buf_176->rk;

            k16 = 0; buf_176->ks2 = ksum(k16, (unsigned char *)(bu + 10), sz - 10);

            data.clear();
            data.append((char *)buf_176, sz);
            dl = data.size();
            if (dl != sz) {
                tmp.clear();
                tmp.sprintf("data_size = %d, struct_size=%d", dl, sz);
                QMessageBox::information(this,"Error MkCmd", tmp);
            }

            ready = true;

        break;
        case 168 :
        case 171 :
            sz = sizeof(s_cmd_168);
            bu = (unsigned char *)calloc(1, sz);
            if (!bu) break;
            buf_168 = (s_cmd_168 *)bu;

            tmp.clear();
            memcpy((unsigned char *)buf_168, "abcd", 4);
            buf_168->ver = ver_prot;
            buf_168->len = sz - 8;
            k8 = 0; buf_168->ks1 = ksum(k8, (unsigned char *)bu, 7);
            // login
            tmp = ui->lgn_w->text();
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ', true);
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_168->log, (unsigned char *)uki, len_log_pas);
            // password
            ui->pwd_w->setEchoMode(QLineEdit::Normal);
            tmp = ui->pwd_w->text();
            ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ');
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_168->pas, (unsigned char *)uki, len_log_pas);

            buf_168->cmd = (unsigned char)CMD;

            k16 = 0; buf_168->ks2 = ksum(k16, (unsigned char *)(bu + 10), sz - 10);

            data.clear(); data.append((char *)buf_168, sz);
            dl = data.size();
            if (dl != sz) {
                tmp.clear(); tmp.sprintf("data_size = %d, struct_size=%d", dl, sz);
                QMessageBox::information(this,"Error MkCmd", tmp);
            }

            ready=true;
        break;
        case 169 :
        case 170 :
            sz = sizeof(s_cmd_169);
            bu = (unsigned char *)calloc(1, sz);
            if (!bu) break;
            buf_169 = (s_cmd_169 *)bu;

            tmp.clear();
            memcpy((unsigned char *)buf_169, "abcd", 4);
            buf_169->ver = ver_prot;
            buf_169->len = sz - 8;
            k8 = 0; buf_169->ks1 = ksum(k8, (unsigned char *)bu, 7);
            // login
            tmp = ui->lgn_w->text();
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ', true);
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_169->log, (unsigned char *)uki, len_log_pas);
            // password
            ui->pwd_w->setEchoMode(QLineEdit::Normal);
            tmp = ui->pwd_w->text();
            ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ');
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_169->pas, (unsigned char *)uki, len_log_pas);

            buf_169->cmd = CMD;
            tmp = ui->rk_w->text();
            buf_169->rk = (unsigned char)tmp.toInt(&done, 10);
            if (buf_169->rk > 0) buf_169->rk--;
            RK = buf_169->rk;

            tmp=ui->sim_w->text();
            if (CMD == 169) pint = 15; else pint = 31;//CMD=170
            if (tmp.length() > pint) tmp.truncate(pint);
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_169->line, (unsigned char *)uki, auth.length());

            k16 = 0; buf_169->ks2 = ksum(k16, (unsigned char *)(bu + 10), sz - 10);

            data.clear(); data.append((char *)buf_169, sz);
            dl = data.size();
            if (dl != sz) {
                tmp.clear(); tmp.sprintf("data_size = %d, struct_size=%d", dl, sz);
                QMessageBox::information(this,"Error MkCmd", tmp);
            }

            ready=true;

        break;
        case 167 :
            sz = sizeof(s_cmd_167);
            bu = (unsigned char *)calloc(1,sz);
            if (!bu) break;
            buf_167 = (s_cmd_167 *)bu;

            tmp.clear();
            memcpy((unsigned char *)buf_167, "abcd", 4);
            buf_167->ver = ver_prot;
            // login
            tmp = ui->lgn_w->text();
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ', true);
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_167->log, (unsigned char *)uki, len_log_pas);
            // password
            ui->pwd_w->setEchoMode(QLineEdit::Normal);
            tmp = ui->pwd_w->text();
            ui->pwd_w->setEchoMode(QLineEdit::PasswordEchoOnEdit);
            dl = tmp.length();
            if (dl >= len_log_pas) { tmp = tmp.left(len_log_pas); dl = tmp.length(); }
                else
            if (dl < len_log_pas) tmp = tmp.leftJustified(len_log_pas, ' ');
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_167->pas, (unsigned char *)uki, len_log_pas);

            buf_167->cmd = (unsigned char)CMD;
            tmp = ui->rk_w->text();
            buf_167->rk = (unsigned char)tmp.toInt(&done, 10);
            if (buf_167->rk>0) buf_167->rk--;
            RK = buf_167->rk;

            tp.clear(); tp = ui->flash_w->text();
            if (tp.contains("flash", Qt::CaseSensitive)) byte |= BIT_FLASH;
            tp.clear(); tp = ui->cyr_w->text();
            if (tp.contains("cds", Qt::CaseSensitive)) byte |= BIT_CDS;
            if (tp.contains("ucs2", Qt::CaseSensitive)) byte |= BIT_UCS2;
            else
            if (tp.contains("gsm7bit", Qt::CaseSensitive)) byte |= BIT_GSM7BIT;
            else
            if (tp.contains("gsm8bit", Qt::CaseSensitive)) byte |= BIT_GSM8BIT;
            buf_167->mode = byte;

            //gsm subscriber number
            tmp = ui->subs_w->text();
            dl = tmp.length();
            if (dl >= 16) { tmp = tmp.left(16); dl = tmp.length(); }
                else
            if (dl < 16) tmp = tmp.leftJustified(16,' ');
            auth.clear(); auth.append(tmp); uki = (unsigned char *)auth.data();
            memcpy((unsigned char *)buf_167->gsm, (unsigned char *)uki, 16);

            tmp = ui->mess_w->toPlainText();
            if (tmp.length() > 180) tmp.truncate(180);
            text.clear();
            if (buf_167->mode & BIT_UCS2) {
                //convert body from utf8 to koi8-r
                QTextCodec *codec = QTextCodec::codecForName("KOI8-R");
                text = codec->fromUnicode(tmp);
                //--------------------------------
            } else text.append(tmp);
            dl = text.length();

            buf_167->body_len = dl;
            buf_167->len = (unsigned short)(sz - 8 + dl);
            sms_index++;
            buf_167->index = sms_index;
            k8=0; buf_167->ks1 = ksum(k8, (unsigned char *)bu, 7);

            data.clear();
            data.append((char *)buf_167, sz);
            data.append(text);

            uki = (unsigned char *)data.data();
            k16 = 0; ks16 = ksum(k16, (unsigned char *)(uki + 10), sz + dl - 10);
            data.data()[8] = (unsigned char)ks16;
            data.data()[9] = (unsigned char)(ks16 >> 8);

            ready=true;

        break;

    }//end switch(CMD)

    if (bu) free(bu);

    if (ready) {
        _pSocket->write(data,data.size());
        Cnt_send++;
        tp.clear(); tp.sprintf("%d", Cnt_send); ui->kol_w->setText(tp);
#ifdef CMD_FILE
        ofstream *fil_cmd = new ofstream (fd_name, ios::trunc | ios::binary);
        if (fil_cmd) {
            fil_cmd->write((const char *)data.data(), data.size());
            fil_cmd->close();
            delete fil_cmd;
        }
#endif
    }
}

void SmsWindow::disconnectTcp()
{
#ifdef TRACE
    TRACER;
#endif

    SetTick(0);
    if (_pSocket->isOpen()) {
        disconnect(_pSocket, SIGNAL(connected()), this, SLOT(connectTcp()));
        disconnect(_pSocket, SIGNAL(disconnected()), this, SLOT(disconnectTcp()));
        disconnect(_pSocket, SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(SokError(QAbstractSocket::SocketError)));
        disconnect(_pSocket, SIGNAL(readyRead()), this, SLOT(readData()));
        _pSocket->disconnectFromHost();
        _pSocket->close();//delete _pSocket;
    }
}
//--------------------------------------------------------------------------------
void SmsWindow::readData()
{
int len, dl = 8, i = 0, j = 0, rk;
unsigned short word = 0, k16 = 0, ln = 0;
unsigned char k8 = 0;
bool gd, ready = false;
QString tmp = "?????", tmp1, tmp_stat, tmp_db, tp, ttt;
QTextCodec *codec;
s_ack_hdr * uk_hdr = NULL;
s_ack_176 * uk_hdr_176 = NULL;
s_ack_167 * uk_hdr_167 = NULL;
s_ack_161 * uk_hdr_161 = NULL;
s_str_6 *dat6 = NULL;
s_str_9 *dat9 = NULL;
char *stn=NULL, *uk = NULL;
unsigned char *uki = NULL;

    stn = (char *)calloc(1, max_st);
    if (!stn) {
        MyError |= 1;//memory error
        throw TheError(MyError);
    }

if ((CMD == 161) || (CMD == 167) || (CMD == 168) || (CMD == 169) ||
        (CMD == 170) || (CMD == 171) || (CMD == 172) || (CMD == 176)) {
    rdata.clear();
    rdata = _pSocket->read(dl);//прием первых 8-ми байт пакета от сервера
    len = rdata.size();
    if (len != dl) {
        tmp.clear(); tmp.sprintf("8: recv = %d but wait %d bytes", len, dl);
        statusBar()->showMessage(tmp); tmp = "???";
    } else {
        uki = (unsigned char *)rdata.data();
        memcpy((unsigned char *)&word, uki + 5, 2);//берем длину информационной части пакета
        if (word > 0) {
            rdata += _pSocket->read(word);//принимаем информационную часть
            len = rdata.size();
            if (len != (word + 8)) {
                memset(stn, 0, max_st); sprintf(stn, "+: recv = %d but wait %d bytes", len, word);
                tmp.clear(); tmp.sprintf("+: recv = %d but wait %d bytes", len, word);
                statusBar()->showMessage(tmp); tmp = "???";
            } else {// прием информационной части пакета успешно завершен
                SetTick(0); tmp_stat.clear(); ln = rdata.size();
                switch (CMD) {
                case 176 :
                    uk = rdata.data();
                    k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                    k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10),ln - 10);// подсчет контрольной суммы ks2
                    uk_hdr = (s_ack_hdr *)uk;
                    memset(stn, 0, max_st); tmp.clear();
                    sprintf(stn,"[%d/%d] ver=%d BOS=%d ", ln, uk_hdr->len, uk_hdr->ver, uk_hdr->bos);
                    if (k8 != uk_hdr->ks1) sprintf(stn+strlen(stn), "ks1=0x%02X/0x%02X ", uk_hdr->ks1, k8);
                    if (k16 != uk_hdr->ks2) sprintf(stn+strlen(stn), "ks2=0x%04X/0x%04X ", uk_hdr->ks2, k16);
                    if (ln >= sizeof(s_ack_hdr) + 2) {
                        uk_hdr_176 = (s_ack_176 *)uk;
                        sprintf(stn+strlen(stn), " flg=%d len=%d", uk_hdr_176->done, uk_hdr_176->sms_len);
                    }
                    if (ln > sizeof(s_ack_hdr) + 2) {
                        uk_hdr_176 = (s_ack_176 *)uk;
                        sprintf(stn+strlen(stn), " type=%d id=%d part=%d/%d",
                                uk_hdr_176->sms_type, uk_hdr_176->sms_num,
                                uk_hdr_176->sms_part, uk_hdr_176->sms_cnt_part);
                        if (uk_hdr_176->done) {
                            uk = &rdata.data()[20];// указатель на "тело смски"
                            tmp1.clear(); tmp1.append(uk); ln = tmp1.length();
                            codec = QTextCodec::codecForName("KOI8-R");
                            tmp = codec->toUnicode(uk, ln);
                            /**/
                            // блок записи принятой смски в базу данных sqlite3/mysql
                            if (openok) {
                                QSqlQuery query(db);
                                tp.clear();
                                tp.sprintf("%d", (uint)QDateTime::currentDateTime().toTime_t() );
                                tmp1.clear();
                                tmp1.sprintf("INSERT INTO sms "\
                                             "(s_rk,s_len,s_type,s_num,s_part,s_cnt_part,s_body,s_epoch)"\
                                             "VALUES (%d,%d,%d,%d,%d,%d,'",
                                    RK+1, uk_hdr_176->sms_len,
                                    uk_hdr_176->sms_type, uk_hdr_176->sms_num,
                                    uk_hdr_176->sms_part, uk_hdr_176->sms_cnt_part);
                                tmp1 += tmp +"'," + tp +");";
                                if (mysql) tmp_db = "mysql "; else tmp_db = "sqlite3 ";
                                    tmp_db += "DB "+db_name + " : ";
                                if (query.exec(tmp1)) {
                                    tmp_db += "Insert to table sms Ok | Last record ";
                                    if (query.exec("select * from sms order by number desc limit 1;")) {
                                        while (query.next()) {
                                            ttt = query.value(0).toString(); dl = ttt.toInt(&gd, 10);
                                            if (gd) rec_count = current = dl;
                                            tmp_db += ttt; break;
                                        }
                                    } else tmp_db += "?";
                                } else tmp_db += query.lastError().text();
                                ui->db_w->setText(tmp_db);
                            }
                            /**/
                        } else tmp = "SMS buffer is empty";
                    }
                    tmp_stat.sprintf("%s",stn);
                    ready = true;
                break;
                case 167 :
                case 169 :
                case 170 :
                    if (ln < sizeof(s_ack_167)) tmp.sprintf("%d bytes recv\nSMS Buffer is empty", ln);
                    else {// контроль динны информационной части пакета
                        uk = rdata.data();
                        k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                        k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10),ln - 10);// подсчет контрольной суммы ks2
                        uk_hdr_167 = (s_ack_167 *)uk;
                        memset(stn, 0, max_st);
                        sprintf(stn, "[%d/%d] ver=%d BOS=%d cmd=%d ",
                                ln, uk_hdr_167->len, uk_hdr_167->ver, uk_hdr_167->bos, uk_hdr_167->cmd);
                        if (k8 != uk_hdr_167->ks1)
                                sprintf(stn+strlen(stn),"ks1=0x%02X/0x%02X ", uk_hdr_167->ks1, k8);
                        if (k16 != uk_hdr_167->ks2)
                                sprintf(stn+strlen(stn),"ks2=0x%04X/0x%04X ", uk_hdr_167->ks2, k16);
                        tmp_stat.sprintf("%s", stn);
                        tmp.clear();
                    }
                    ready = true;
                break;
                case 168 :
                    dl = sizeof(s_ack_167);
                    if (ln < dl) tmp.sprintf("%d bytes recv\nSMS Buffer is empty", ln);
                    else {// контроль динны информационной части пакета
                        uk = rdata.data();
                        k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                        k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10), ln - 10);// подсчет контрольной суммы ks2
                        uk_hdr_167 = (s_ack_167 *)uk;
                        memset(stn, 0, max_st);
                        sprintf(stn, "[%d/%d] ver=%d BOS=%d cmd=%d ",
                                ln, uk_hdr_167->len, uk_hdr_167->ver, uk_hdr_167->bos, uk_hdr_167->cmd);
                        if (k8 != uk_hdr_167->ks1)
                                sprintf(stn+strlen(stn),"ks1=0x%02X/0x%02X ", uk_hdr_167->ks1, k8);
                        if (k16 != uk_hdr_167->ks2)
                                sprintf(stn+strlen(stn),"ks2=0x%04X/0x%04X ", uk_hdr_167->ks2, k16);
                        tmp_stat.sprintf("%s", stn);

                        tmp.clear(); memset(stn, 0, max_st);
                        j = sizeof(s_str_6);
                        i = (ln - dl) / j;
                        if (i > 0) {//количество строк (по 6 байт каждая)
                            uk = rdata.data(); uk += dl;
                            rk = 0;
                            while (i > 0) {
                                dat6 = (s_str_6 *)uk;
                                sprintf(stn+strlen(stn), "[%02d] stat: 0x%02X , ind=%d , %02d.%02d %02d:%02d\n",
                                            rk + 1, dat6->status, dat6->cmgs,
                                            dat6->day, dat6->mes, dat6->chas, dat6->min);
                                uk += j; i--; rk++;
                            }
                            tmp.sprintf("%s", stn);
                        } else tmp = "???";
                    }
                    ready = true;
                break;
                case 172 :
                    dl = sizeof(s_ack_167);
                    if (ln < dl) tmp.sprintf("%d bytes recv\nSMS Buffer is empty", ln);
                    else {// контроль динны информационной части пакета
                        uk = rdata.data();
                        k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                        k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10), ln - 10);// подсчет контрольной суммы ks2
                        uk_hdr_167 = (s_ack_167 *)uk;
                        memset(stn, 0, max_st);
                        sprintf(stn, "[%d/%d] ver=%d BOS=%d cmd=%d ", ln, uk_hdr_167->len, uk_hdr_167->ver, uk_hdr_167->bos, uk_hdr_167->cmd);
                        if (k8 != uk_hdr_167->ks1) sprintf(stn+strlen(stn),"ks1=0x%02X/0x%02X ", uk_hdr_167->ks1, k8);
                        if (k16 != uk_hdr_167->ks2) sprintf(stn+strlen(stn),"ks2=0x%04X/0x%04X ", uk_hdr_167->ks2, k16);
                        tmp_stat.sprintf("%s", stn);
                        tmp.clear();
                        i = ln - dl;
                        if (i > 0) {
                            uk = rdata.data(); uk += dl;
                            tmp.append(uk); dl = tmp.length();
                            codec = QTextCodec::codecForName("KOI8-R");
                            tmp = codec->toUnicode(uk, dl);
                        }
                    }
                    ready=true;
                break;
                case 161 :
                    dl = sizeof(s_ack_161);
                    if (ln < dl) tmp.sprintf("%d bytes recv\nSMS Buffer is empty", ln);
                    else {// контроль динны информационной части пакета
                        uk = rdata.data();
                        k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                        k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10), ln - 10);// подсчет контрольной суммы ks2
                        uk_hdr_161 = (s_ack_161 *)uk;
                        memset(stn, 0, max_st);
                        sprintf(stn, "[%d/%d] ver=%d BOS=%d cmd=%d ", ln, uk_hdr_161->len, uk_hdr_161->ver, uk_hdr_161->bos, uk_hdr_161->cmd);
                        if (k8 != uk_hdr_161->ks1) sprintf(stn+strlen(stn), "ks1=0x%02X/0x%02X ", uk_hdr_161->ks1, k8);
                        if (k16 != uk_hdr_161->ks2) sprintf(stn+strlen(stn), "ks2=0x%04X/0x%04X ", uk_hdr_161->ks2, k16);
                        tmp_stat.sprintf("%s", stn);

                        tmp.clear();
                        tmp.sprintf("Money : %d | date : %02d.%02d %02d:%02d",
                                    uk_hdr_161->money,
                                    uk_hdr_161->day,
                                    uk_hdr_161->mes,
                                    uk_hdr_161->chas,
                                    uk_hdr_161->min);
                    }
                    ready = true;
                break;
                case 171 :
                    dl = sizeof(s_ack_167);
                    if (ln < dl) tmp.sprintf("%d bytes recv\nSMS Buffer is empty", ln);
                    else {// контроль динны информационной части пакета
                        uk = rdata.data();
                        k8 = 0; k8 = ksum(k8, (unsigned char *)uk, 7);// подсчет контрольной суммы ks1
                        k16 = 0; k16 = ksum(k16, (unsigned char *)(uk + 10), ln - 10);// подсчет контрольной суммы ks2
                        uk_hdr_167 = (s_ack_167 *)uk;
                        memset(stn, 0, max_st);
                        sprintf(stn, "[%d/%d] ver=%d BOS=%d cmd=%d ", ln, uk_hdr_167->len, uk_hdr_167->ver, uk_hdr_167->bos, uk_hdr_167->cmd);
                        if (k8 != uk_hdr_167->ks1) sprintf(stn+strlen(stn), "ks1=0x%02X/0x%02X ", uk_hdr_167->ks1, k8);
                        if (k16 != uk_hdr_167->ks2) sprintf(stn+strlen(stn), "ks2=0x%04X/0x%04X ", uk_hdr_167->ks2, k16);
                        tmp_stat.sprintf("%s", stn);

                        tmp.clear(); memset(stn, 0, max_st);
                        j = sizeof(s_str_9);
                        i = (ln - dl) / j;
                        if (i>0) {//количество строк (по 9 байт каждая)
                            uk = rdata.data(); uk += dl;
                            rk = 0;
                            while (i > 0) {
                                dat9 = (s_str_9 *)uk;
                                sprintf(stn+strlen(stn), "[%02d] stat: 0x%02X , check: %02d.%02d %02d:%02d , add : %02d.%02d %02d:%02d\n",
                                        rk+1, dat9->status,
                                        dat9->check_day, dat9->check_mes, dat9->check_chas, dat9->check_min,
                                        dat9->add_day, dat9->add_mes, dat9->add_chas, dat9->add_min);
                                uk += j; i--; rk++;
                            }
                            tmp.sprintf("%s", stn);
                        } else tmp = "???";
                    }
                    ready = true;
                break;
                }//end switch(CMD)
            }// else // прием информационной части пакета успешно завершен
        }//if (word>0)
    }
}// end CMD=161 || CMD=167 ....
if (ready) {
    SetTick(0);
    statusBar()->showMessage(tmp_stat);
    ui->answer_w->clear(); ui->answer_w->setText(tmp);
    disconnectTcp();

    UpdateTitle();
}
if (stn) free(stn);

}
//--------------------------------------------------------------------------------
int SmsWindow::ReadDB(QString *stm, const int kol)
{
int cnt = 0;
QSqlQuery query(db);

    if (!stm) {
        MyError |= 16;//pointer error
        throw TheError(MyError);
    }

    if (openok) {
        time_t ttm;
        struct tm *timi;//for convert time (localtime)
        bool good;
        QString tmp, tp;
        if (!kol)
            tp.sprintf("SELECT * FROM sms order by number desc limit %d;", max_cnt);
        else
            tp.sprintf("SELECT * FROM sms where number<=%d order by number desc limit %d;", kol, max_cnt);

        QString tmp_db = "Error reading from DB -> ";

        tmp.clear();
        if (query.exec(tp)) {
            while (query.next()) {
                cnt++;
                if (cnt <= max_cnt) {//выводить будем не более max_cnt записей
                //number,s_rk,s_len,s_type,s_num,s_part,s_cnt_part,s_body,s_epoch
                    tmp += query.value(0).toString() + sep;//number
                    tmp += query.value(1).toString() + sep;//s_rk
                    tmp += query.value(7).toString() + sep;//s_body
                    tmp += query.value(2).toString() + sep;//s_len
                    tmp += query.value(3).toString() + sep;//s_type
                    tmp += query.value(4).toString() + sep;//s_num
                    tmp += query.value(5).toString() + "/";//s_part
                    tmp += query.value(6).toString() + sep;//s_cnt_part
                    tp.clear();
                    tp = query.value(8).toString();//s_epoch
                    ttm = (time_t)(tp.toInt(&good, 10));
                    if (good) {
                        timi = localtime(&ttm);
                        tp.clear();
                        tp.sprintf("%02d.%02d.%04d %02d:%02d:%02d",
                            timi->tm_mday, timi->tm_mon + 1, timi->tm_year + 1900,
                            timi->tm_hour, timi->tm_min, timi->tm_sec);
                    }
                    tmp += tp + sepl;
                } else break;
            }
        } else {
            tmp_db += "cnt=" + cnt + query.lastError().text();
            ui->db_w->setText(tmp_db);
        }

        stm->append(tmp);
    }
    return cnt;
}
//--------------------------------------------------------------------------------
void SmsWindow::prepare_to_readDB()
{
QString tp;

    tp.clear();
    if (!openok) tp = "DateBase " + db_name + " not open";
        else
    if (rec_count <= 0) tp = "DateBase " + db_name + " is emtpy";
    if (tp.length() > 0) {
        QMessageBox::information(this,"Informaion Message", tp);
        return;
    }

    if (wnd) {
        wnd->close();
        if (wnd) delete wnd;
        wnd = NULL;
    }
    wnd = new QDialog;
    if (wnd) {
        int x, y, w = 50, h = 25, total;
        QString line;
        QStringList list, lst;
        QRect rr = this->ui->answer_w->geometry();

        QPalette *pal = new QPalette(this->ui->db_w->palette());
        QFont *font = new QFont(this->ui->ip_w->font());

        wnd->setWindowIcon(QIcon(QPixmap(":png/sql.png")));

        rr.setLeft(rr.left() + 20);
        rr.setHeight(rr.height() + 30);
        rr.setWidth(rr.width() + 140);
        rr.setHeight(rr.height() + 180);

        wnd->setGeometry(rr);

        nrec = new QLabel(wnd);
        nrec->setGeometry(rr.width() / 4 - w/2, rr.height() - 30, w + 30, h);
        nrec->setText(tr("Record #"));

        from_rec_w = new QLineEdit(wnd);
        x=((rr.width() / 2) / 2) + w;
        y=rr.height() - 30; from_rec_w->setGeometry(x, y, w, h);
        tp.clear(); tp.sprintf("%d", current);
        from_rec_w->setText(tp);

        bnext_down = new QPushButton(wnd);
        x=(rr.width() / 2) - (w/2) - 25;
        y=rr.height() - 30; bnext_down->setGeometry(x, y, w, h);
        bnext_down->setText("Down");
        connect(bnext_down, SIGNAL(clicked()), this, SLOT(NextRecDown()));

        bnext_up = new QPushButton(wnd);
        x=(rr.width() / 2) - (w/2) + 25;
        y=rr.height() - 30; bnext_up->setGeometry(x, y, w, h);
        bnext_up->setText("Up");
        connect(bnext_up, SIGNAL(clicked()), this, SLOT(NextRecUp()));

        b_del = new QPushButton(wnd);
        x=(rr.width() / 2) + w * 2;
        y=rr.height() - 30; b_del->setGeometry(x, y, w, h);
        b_del->setText("del");
        connect(b_del, SIGNAL(clicked()), this, SLOT(DelRecord()));


        tp.sprintf("From DB: Last records %d", rec_count);
        wnd->setWindowTitle(tp);

        line.clear();
        total = ReadDB(&line, current);
        list = line.split(sepl);

        tbl = new QTableWidget(wnd);
        tbl->setFont(*font);
        tbl->setPalette(*pal);

        delete font;
        delete pal;

        tbl->resize(rr.width(), rr.height() - 40);
        tbl->setRowCount(total);//количество строк
        tbl->setColumnCount(8); //количество столбцов


        int row, column;
        QStringList *lt = new QStringList(QStringList() << "#" << "RK" << "Body" << "Len" << "Type" << "Num" << "Part" << "Time");
        for (column = 0; column < tbl->columnCount(); column++)
            tbl->setHorizontalHeaderLabels(*lt);
        delete lt;

        tbl->resizeRowsToContents();
        for (row = 0; row < tbl->rowCount(); row++) {
            lst = list.at(row).split(sep);
            tbl->resizeColumnsToContents();
            for (column = 0; column < tbl->columnCount(); column++) {
                QTableWidgetItem *item = new QTableWidgetItem;
                item->setText(lst.at(column));
                tbl->setItem(row, column, item);
            }
        }

        connect(tbl, SIGNAL(cellClicked(int,int)), this, SLOT(RowNum(int,int)));

        wnd->show();

    } else {
        MyError |= 1;//memory error
        throw TheError(MyError);
    }

}
//--------------------------------------------------------------------------------
void SmsWindow::DeleteRec(const int nr)
{
QString tp;
bool good;
int newr = 0;

    if (openok) {
        QSqlQuery query(db);
        tp.sprintf("DELETE FROM sms where number=%d;", nr);
        if (query.exec(tp)) {
            if (query.exec("select * from sms order by number desc limit 1;")) {
                tp.clear();
                while (query.next()) {
                    tp = query.value(0).toString();
                    newr = tp.toInt(&good, 10);
                    if (good) {
                        rec_count = current = newr;
                    }
                    break;
                }
            }
            if (good) {
                tp.clear();
                tp.sprintf("%d", current); from_rec_w->setText(tp);
                tp.insert(0, "From DB: Last records ");
                wnd->setWindowTitle(tp);
                NextRec();
            }
        }

        UpdateTitle();
    }

}
//--------------------------------------------------------------------------------
void SmsWindow::NextRecDown()
{
    b_del->setText("del");
    bool good, bye = false;
    QString stp = from_rec_w->text();
    int dig = stp.toInt(&good, 10);
    if ((good) && (dig > 0)) {
        if (dig != current) {
            current = dig;
            bye = true;
        }
    }
    if (!bye) {
        if (current > 1) current--; else current = rec_count;
    }
    NextRec();
}
//--------------------------------------------------------------------------------
void SmsWindow::NextRecUp()
{
    b_del->setText("del");
    bool good, bye = false;
    QString stp = from_rec_w->text();
    int dig = stp.toInt(&good, 10);
    if ((good) && (dig > 0)) {
        if (dig != current) {
            current = dig;
            bye = true;
        }
    }
    if (!bye) {
        if (current < rec_count) current++; else current = 1;
    }
    NextRec();
}
//--------------------------------------------------------------------------------
void SmsWindow::NextRec()
{
QString line, tp;
QStringList list, lst;

    line.clear();

    int total = ReadDB(&line,current);

    list = line.split(sepl);

    tbl->setRowCount(total);//количество строк

    for (int row = 0; row < tbl->rowCount(); row++) {
        lst = list.at(row).split(sep);
        tbl->resizeColumnsToContents();
        for (int column = 0; column < tbl->columnCount(); column++) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setText(lst.at(column));
            tbl->setItem(row, column, item);
        }
    }
    line.clear(); line.sprintf("%d", current);
    from_rec_w->setText(line);
    if (mysql) tp = "mysql "; else tp = "sqlite3 ";
    tp += "DB " + db_name + " | Last record " + line;
    ui->db_w->setText(tp);

}
//--------------------------------------------------------------------------------
void SmsWindow::DelRecord()
{
int r;
bool good;

    QString tp = b_del->text();
    r = tp.toInt(&good, 10);
    if ((good)  && ((r > 0) && (r <= rec_count))) {
        b_del->setText("del");
        DeleteRec(r);
    }

}
//--------------------------------------------------------------------------------
void SmsWindow::RowNum(int r, int c) const
{
    switch (c) {
        case 0: //delete record
            b_del->setText(tbl->item(r, 0)->text());
        break;
        case 2: //show sms body
            QMessageBox::information(wnd, "Sms body", tbl->item(r, c)->text());
        break;
    }
}
//--------------------------------------------------------------------------------

