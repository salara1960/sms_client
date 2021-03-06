#include "mainwindow.h"

//---------------------------------------------------------
int main(int argc, char *argv[])
{
int cerr = 0;
QString temp = "127.0.0.1";
QString dbn;
unsigned short sport = 9005;
char stz[256] = {0};

#ifdef Q_WS_X11
    bool useGUI = getenv("DISPLAY") != 0;
#else
    bool useGUI = true;
#endif

    if (!useGUI) {
        cout << "Console version not support, bye.\n";
        return 1;
    }

    if (argc > 1) {
        if (!strcmp(argv[1], "--help")) {
            cout << "\nYou can enter 4 parametrs, for example\n"\
                    "./sms_client par1 par2 par3 par4\n where :\n"\
                    "\tpar1 = sqlite3 or mysql  -  database type\n"\
                    "\tpar2 = 2 or 3 or 4  -  protocol version\n"\
                    "\tpar3 = 127.0.0.1  -  ip address of sms server\n"\
                    "\tpar4 = 9005  -  tcp port of sms server\n";
            return 0;
        }
        if (!strcmp(argv[1], "mysql")) mysql = 1; else mysql = 0;
    }
    if (argc > 2) {
        ver_prot = atoi(argv[2]);
        if ((ver_prot == 0) || (ver_prot > 4)) ver_prot = 2;
    }
    if (argc > 3) {
        temp.clear(); temp.append(argv[3]);
    }
    if (argc > 4) sport = atoi(argv[4]);

    setlocale(LC_ALL,"UTF8"); 

    try {
        QApplication arm(argc, argv);
        if (mysql) dbn = "sms"; else dbn = "sms.db3";
        QStringList lst(QStringList() << "localhost" << "3306" << "root" << "");//для mysql :ip,port,login,password
        s_srv_url srv_url = {temp, sport};
        SmsWindow wnd(nullptr, dbn, &lst, &srv_url);
        wnd.show();
        arm.exec();
    }
    // Error handler block:
    // - 0x01 - memory error
    // - 0x02 - timer error
    // - 0x04 - Qstring::toInt error
    // - 0x08 - bad index
    // - 0x10 - bad pointer
    // - 0x20 - bad socket memory error
    catch (SmsWindow::TheError (er)) {
        memset(stz, 0, sizeof(stz));
        cerr = er.code;
        if ((cerr>0) && (cerr<=0x20)) {
            if (cerr & 1) sprintf(stz, "Error in calloc function (%d)\n", cerr);
            if (cerr & 2) sprintf(stz, "Error in startTimer function (%d)\n", cerr);
            if (cerr & 4) sprintf(stz, "Error in 'Qstring::toInt' function (%d)\n", cerr);
            if (cerr & 8) sprintf(stz, "Error index of massive (%d)\n", cerr);
            if (cerr & 0x10) sprintf(stz, "Error pointer (%d)\n", cerr);
            if (cerr & 0x20) sprintf(stz, "Error create socket - no memory (%d)\n", cerr);
        }
        if (!strlen(stz)) sprintf(stz, "Unknown Error (%d)\n", cerr);
        perror(stz);

        return cerr;
    }
    catch (bad_alloc) {
        perror("Error while alloc memory via call function new\n");
        return -1;
    }

    return 0;
}
