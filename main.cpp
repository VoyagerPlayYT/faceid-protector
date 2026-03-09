/*  FACE ID PROTECTOR v3  */

#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QFrame>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtGui/QAction>
#include <QtCore/QTimer>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QThread>
#include <QtCore/QDir>
#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QLinearGradient>
#include <QtGui/QRadialGradient>
#include <QtGui/QConicalGradient>
#include <QtGui/QMouseEvent>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>
#include <QtGui/QClipboard>
#include <QtWidgets/QStyle>
#include <QtGui/QCloseEvent>
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <fstream>
#include <sstream>
#include <vector>
#include <random>
#include <string>
#include <thread>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <winhttp.h>
#include <rpcdce.h>

using namespace std;
using namespace cv;
using namespace cv::face;

// ═══════════════════════════════════════════════════
//  УТИЛИТЫ — ПУТИ
// ═══════════════════════════════════════════════════
string exeDir() {
    char buf[MAX_PATH]; GetModuleFileNameA(NULL,buf,MAX_PATH);
    string p(buf); size_t s=p.find_last_of("/\\");
    return s!=string::npos ? p.substr(0,s+1) : "";
}

string PASSWORD_FILE, WORDS_FILE, APPS_FILE, FACE_DATA_FILE, CASCADE_FILE, CAM_FILE, UUID_FILE, TG_FILE;

void initPaths(){
    string d=exeDir();
    PASSWORD_FILE  = d+"password.txt";
    WORDS_FILE     = d+"recovery_words.txt";
    APPS_FILE      = d+"apps.txt";
    FACE_DATA_FILE = d+"face_data.yml";
    CASCADE_FILE   = d+"haarcascade_frontalface_default.xml";
    CAM_FILE       = d+"camera.txt";
    UUID_FILE      = d+"device_uuid.txt";
    TG_FILE        = d+"telegram.txt";
}

// ═══════════════════════════════════════════════════
//  КОНСТАНТЫ
// ═══════════════════════════════════════════════════
const double FACE_THRESHOLD = 95.0;
const int    REG_FRAMES     = 40;
const int    AUTH_FRAMES    = 25;
const string BOT_SERVER     = "faceidqt.onrender.com";

int g_camIndex = 0;

// ═══════════════════════════════════════════════════
//  ФАЙЛОВЫЕ УТИЛИТЫ
// ═══════════════════════════════════════════════════
bool   fileExists(const string& p){ return ifstream(p).good(); }
string readFile(const string& p){ ifstream f(p); return f ? string(istreambuf_iterator<char>(f),{}) : ""; }
void   writeFile(const string& p, const string& c){ ofstream f(p); f<<c; }
void   appendFile(const string& p, const string& l){ ofstream f(p,ios::app); f<<l<<"\n"; }
vector<string> readLines(const string& p){
    vector<string> v; ifstream f(p); string l;
    while(getline(f,l)) if(!l.empty()) v.push_back(l);
    return v;
}
void loadCamIndex(){ ifstream f(CAM_FILE); if(f) f>>g_camIndex; }
void saveCamIndex(int i){ g_camIndex=i; ofstream f(CAM_FILE); f<<i; }

// ═══════════════════════════════════════════════════
//  UUID
// ═══════════════════════════════════════════════════
string getDeviceUUID(){
    ifstream f(UUID_FILE); if(f){ string u; f>>u; if(u.size()>8) return u; }
    UUID uuid; UuidCreate(&uuid);
    char* str; UuidToStringA(&uuid,(RPC_CSTR*)&str);
    string r(str); RpcStringFreeA((RPC_CSTR*)&str);
    ofstream o(UUID_FILE); o<<r;
    return r;
}

// ═══════════════════════════════════════════════════
//  BASE64
// ═══════════════════════════════════════════════════
string b64encode(const vector<uchar>& data){
    static const string C="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string out; int i=0; uchar b3[3],b4[4];
    for(uchar c:data){ b3[i++]=c; if(i==3){
        b4[0]=(b3[0]&0xfc)>>2; b4[1]=((b3[0]&0x03)<<4)|((b3[1]&0xf0)>>4);
        b4[2]=((b3[1]&0x0f)<<2)|((b3[2]&0xc0)>>6); b4[3]=b3[2]&0x3f;
        for(int j=0;j<4;j++) out+=C[b4[j]]; i=0;
    }}
    if(i){ for(int j=i;j<3;j++) b3[j]=0;
        b4[0]=(b3[0]&0xfc)>>2; b4[1]=((b3[0]&0x03)<<4)|((b3[1]&0xf0)>>4);
        b4[2]=((b3[1]&0x0f)<<2)|((b3[2]&0xc0)>>6);
        for(int j=0;j<i+1;j++) out+=C[b4[j]];
        while(i++<3) out+='=';
    }
    return out;
}

// ═══════════════════════════════════════════════════
//  HTTP POST
// ═══════════════════════════════════════════════════
string httpPost(const string& path, const string& body){
    wstring wh(BOT_SERVER.begin(),BOT_SERVER.end());
    wstring wp(path.begin(),path.end());
    HINTERNET hS=WinHttpOpen(L"FaceID/1.0",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,NULL,NULL,0);
    if(!hS) return "";
    // Таймаут 15 секунд
    DWORD tout=15000;
    WinHttpSetOption(hS,WINHTTP_OPTION_CONNECT_TIMEOUT,&tout,sizeof(tout));
    WinHttpSetOption(hS,WINHTTP_OPTION_SEND_TIMEOUT,&tout,sizeof(tout));
    WinHttpSetOption(hS,WINHTTP_OPTION_RECEIVE_TIMEOUT,&tout,sizeof(tout));
    HINTERNET hC=WinHttpConnect(hS,wh.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0);
    if(!hC){WinHttpCloseHandle(hS);return "";}
    HINTERNET hR=WinHttpOpenRequest(hC,L"POST",wp.c_str(),NULL,NULL,NULL,WINHTTP_FLAG_SECURE);
    if(!hR){WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return "";}
    // Игнорируем ошибки сертификата (Render free tier)
    DWORD secFlags=SECURITY_FLAG_IGNORE_UNKNOWN_CA|SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE|
                   SECURITY_FLAG_IGNORE_CERT_CN_INVALID|SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
    WinHttpSetOption(hR,WINHTTP_OPTION_SECURITY_FLAGS,&secFlags,sizeof(secFlags));
    WinHttpAddRequestHeaders(hR,L"Content-Type: application/json\r\n",-1L,WINHTTP_ADDREQ_FLAG_ADD);
    BOOL sent=WinHttpSendRequest(hR,NULL,0,(LPVOID)body.c_str(),(DWORD)body.size(),(DWORD)body.size(),0);
    if(!sent){WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return "";}
    if(!WinHttpReceiveResponse(hR,NULL)){
        WinHttpCloseHandle(hR);WinHttpCloseHandle(hC);WinHttpCloseHandle(hS);return "";
    }
    DWORD size=0; string resp;
    do{ WinHttpQueryDataAvailable(hR,&size);
        if(size){ vector<char> buf(size+1,0);
            DWORD read=0; WinHttpReadData(hR,buf.data(),size,&read);
            resp+=string(buf.data(),read); }
    }while(size>0);
    WinHttpCloseHandle(hR); WinHttpCloseHandle(hC); WinHttpCloseHandle(hS);
    return resp;
}

// ═══════════════════════════════════════════════════
//  СКРИНШОТ
// ═══════════════════════════════════════════════════
string captureScreen(){
    try{
        HDC hScr=GetDC(NULL),hDC=CreateCompatibleDC(hScr);
        int w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN);
        HBITMAP hBmp=CreateCompatibleBitmap(hScr,w,h);
        SelectObject(hDC,hBmp); BitBlt(hDC,0,0,w,h,hScr,0,0,SRCCOPY);
        BITMAPINFOHEADER bi={sizeof(BITMAPINFOHEADER),w,-h,1,24,0};
        vector<uchar> buf(w*h*3);
        GetDIBits(hDC,hBmp,0,h,buf.data(),(BITMAPINFO*)&bi,DIB_RGB_COLORS);
        Mat img(h,w,CV_8UC3,buf.data()); cvtColor(img,img,COLOR_BGR2RGB);
        resize(img,img,Size(640,360));
        vector<uchar> jpg; imencode(".jpg",img,jpg,{IMWRITE_JPEG_QUALITY,55});
        DeleteObject(hBmp); DeleteDC(hDC); ReleaseDC(NULL,hScr);
        return b64encode(jpg);
    }catch(...){ return ""; }
}

// ═══════════════════════════════════════════════════
//  АЛЕРТ В TELEGRAM
// ═══════════════════════════════════════════════════
void sendAlert(const string& uuid, int attempts, const Mat& camFrame){
    thread([uuid,attempts,camFrame](){
        string camB64;
        if(!camFrame.empty()){
            Mat s; resize(camFrame,s,Size(320,240));
            vector<uchar> buf; imencode(".jpg",s,buf,{IMWRITE_JPEG_QUALITY,65});
            camB64=b64encode(buf);
        }
        string screen=captureScreen();
        SYSTEMTIME st; GetLocalTime(&st);
        char t[64]; sprintf(t,"%02d:%02d:%02d %02d.%02d.%04d",
            st.wHour,st.wMinute,st.wSecond,st.wDay,st.wMonth,st.wYear);
        string json="{\"uuid\":\""+uuid+"\","
            "\"attempts\":"+to_string(attempts)+","
            "\"time\":\""+string(t)+"\","
            "\"camera\":\""+camB64+"\","
            "\"screenshot\":\""+screen+"\"}";
        httpPost("/api/alert",json);
    }).detach();
}

// ═══════════════════════════════════════════════════
//  ПРОВЕРКА КОДА ПОДТВЕРЖДЕНИЯ
// ═══════════════════════════════════════════════════
bool verifyCode(const string& uuid, const string& code){
    string json="{\"uuid\":\""+uuid+"\",\"code\":\""+code+"\"}";
    string resp=httpPost("/api/verify",json);
    return resp.find("\"ok\":true")!=string::npos;
}

// ── Polling команд от бота ──────────────────────────────
string pollCommand(const string& uuid){
    string json="{\"uuid\":\""+uuid+"\"}";
    return httpPost("/api/poll",json);
}

// ── Отправка результата боту ────────────────────────────
void sendResult(const string& uuid,const string& cmd,
                const string& imageB64="",const string& extra=""){
    string json="{\"uuid\":\""+uuid+"\",\"cmd\":\""+cmd+"\"";
    if(!imageB64.empty()) json+=",\"image\":\""+imageB64+"\"";
    if(!extra.empty())    json+=","+extra;
    json+="}";
    thread([json](){ httpPost("/api/result",json); }).detach();
}

// ── Снимок с камеры по команде ──────────────────────────
string captureCamera(){
    try{
        VideoCapture cap(g_camIndex);
        if(!cap.isOpened()) return "";
        cap.set(CAP_PROP_FRAME_WIDTH,640);
        cap.set(CAP_PROP_FRAME_HEIGHT,480);
        Mat fr;
        for(int i=0;i<5;i++){ cap>>fr; Sleep(50); }
        cap>>fr; cap.release();
        if(fr.empty()) return "";
        vector<uchar> buf;
        imencode(".jpg",fr,buf,{IMWRITE_JPEG_QUALITY,70});
        return b64encode(buf);
    }catch(...){ return ""; }
}


// ═══════════════════════════════════════════════════
//  УСТАНОВКА В СИСТЕМУ + АВТОЗАПУСК
// ═══════════════════════════════════════════════════

// Папка установки
string getInstallDir(){
    char pf[MAX_PATH]={0};
    SHGetFolderPathA(NULL,CSIDL_PROGRAM_FILES,NULL,0,pf);
    return string(pf)+"\\FaceIDProtector\\";
}

// Проверяем — запущены ли мы уже из папки установки
bool isInstalled(){
    char exe[MAX_PATH]={0}; GetModuleFileNameA(NULL,exe,MAX_PATH);
    string ep(exe); string ip=getInstallDir();
    // Приводим к нижнему регистру для сравнения
    string epl=ep,ipl=ip;
    for(auto&c:epl) c=tolower(c);
    for(auto&c:ipl) c=tolower(c);
    return epl.find(ipl)!=string::npos;
}

// Установить себя в систему
bool installSelf(){
    char exe[MAX_PATH]={0}; GetModuleFileNameA(NULL,exe,MAX_PATH);
    string src(exe);
    string dir=getInstallDir();
    string dst=dir+"FaceIDProtector.exe";

    // Создаём папку
    CreateDirectoryA(dir.c_str(),NULL);

    // Копируем exe
    if(!CopyFileA(src.c_str(),dst.c_str(),FALSE)){
        return false;
    }

    // Прописываем автозапуск в HKLM (системный, для всех пользователей)
    string val="\""+dst+"\"";
    HKEY k;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,KEY_SET_VALUE,&k)==ERROR_SUCCESS){
        RegSetValueExA(k,"FaceIDProtector",0,REG_SZ,
            (BYTE*)val.c_str(),(DWORD)val.size()+1);
        RegCloseKey(k);
    }

    // Дополнительно — автозапуск через планировщик задач при входе
    string task = "schtasks /create /f /tn \"FaceIDProtector\" "
                  "/tr \"\\\""+dst+"\\\"\" "
                  "/sc ONLOGON /rl HIGHEST /delay 0000:10";
    WinExec(task.c_str(), SW_HIDE);

    return true;
}

// Обычный автозапуск (если уже установлены)
void addToStartup(){
    char exe[MAX_PATH]={0}; GetModuleFileNameA(NULL,exe,MAX_PATH);
    string val=string("\"")+exe+"\"";
    // HKLM — системный реестр (требует админа)
    HKEY k;
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,KEY_SET_VALUE,&k)==ERROR_SUCCESS){
        RegSetValueExA(k,"FaceIDProtector",0,REG_SZ,
            (BYTE*)val.c_str(),(DWORD)val.size()+1);
        RegCloseKey(k);
    }
    // Планировщик задач — запускается с правами HIGHEST при входе
    string task = "schtasks /create /f /tn \"FaceIDProtector\" "
                  "/tr \"\\\""+string(exe)+"\\\"\" "
                  "/sc ONLOGON /rl HIGHEST /delay 0000:10";
    WinExec(task.c_str(), SW_HIDE);
}

// ═══════════════════════════════════════════════════
//  ЯРЛЫК НА РАБОЧЕМ СТОЛЕ
// ═══════════════════════════════════════════════════
bool createShortcut(const string& target, const string& name){
    CoInitialize(NULL);
    char exe[MAX_PATH]; GetModuleFileNameA(NULL,exe,MAX_PATH);
    char desk[MAX_PATH]; SHGetFolderPathA(NULL,CSIDL_DESKTOP,NULL,0,desk);
    string lnk=string(desk)+"\\"+name+".lnk";
    string args="--launch \""+target+"\"";
    IShellLinkA* psl=nullptr; bool ok=false;
    if(SUCCEEDED(CoCreateInstance(CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,IID_IShellLinkA,(void**)&psl))){
        psl->SetPath(exe); psl->SetArguments(args.c_str()); psl->SetIconLocation(target.c_str(),0);
        IPersistFile* ppf=nullptr;
        if(SUCCEEDED(psl->QueryInterface(IID_IPersistFile,(void**)&ppf))){
            wstring w(lnk.begin(),lnk.end()); ok=SUCCEEDED(ppf->Save(w.c_str(),TRUE)); ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize(); return ok;
}

// ═══════════════════════════════════════════════════
//  СЛОВА ВОССТАНОВЛЕНИЯ
// ═══════════════════════════════════════════════════
struct RecovWord { string hint, word; };

vector<RecovWord> loadRecovWords(){
    vector<RecovWord> v; ifstream f(WORDS_FILE); string line;
    while(getline(f,line)){ size_t p=line.find('|');
        if(p!=string::npos) v.push_back({line.substr(0,p),line.substr(p+1)});
    }
    return v;
}
void saveRecovWords(const vector<RecovWord>& words){
    ofstream f(WORDS_FILE);
    for(auto& w:words) f<<w.hint<<"|"<<w.word<<"\n";
}

// ═══════════════════════════════════════════════════
//  ПОТОК — РЕГИСТРАЦИЯ
// ═══════════════════════════════════════════════════
class RegisterThread : public QThread {
    Q_OBJECT
public:
    void run() override {
        CascadeClassifier cc; if(!cc.load(CASCADE_FILE)){emit done(false);return;}
        auto rec=LBPHFaceRecognizer::create();
        VideoCapture cap(g_camIndex); if(!cap.isOpened()){emit done(false);return;}
        cap.set(CAP_PROP_FRAME_WIDTH,640); cap.set(CAP_PROP_FRAME_HEIGHT,480);
        vector<Mat> faces; vector<int> labels; Mat fr,gray; int n=0;
        while(n<REG_FRAMES){
            cap>>fr; if(fr.empty()){msleep(30);continue;}
            cvtColor(fr,gray,COLOR_BGR2GRAY); equalizeHist(gray,gray);
            vector<Rect> det; cc.detectMultiScale(gray,det,1.1,5,0,Size(80,80));
            for(auto& r:det){ Mat roi=gray(r); resize(roi,roi,Size(100,100));
                faces.push_back(roi.clone()); labels.push_back(1); n++; emit progress(n); }
            Mat rgb; cvtColor(fr,rgb,COLOR_BGR2RGB);
            QImage img(rgb.data,rgb.cols,rgb.rows,rgb.step,QImage::Format_RGB888);
            emit frame(img.copy()); msleep(30);
        }
        cap.release();
        if(n<10){emit done(false);return;}
        rec->train(faces,labels); rec->save(FACE_DATA_FILE); emit done(true);
    }
signals:
    void frame(QImage); void progress(int); void done(bool);
};

// ═══════════════════════════════════════════════════
//  ПОТОК — АУТЕНТИФИКАЦИЯ
// ═══════════════════════════════════════════════════
class AuthThread : public QThread {
    Q_OBJECT
public:
    void run() override {
        CascadeClassifier cc; if(!cc.load(CASCADE_FILE)){emit done(false,999);return;}
        auto rec=LBPHFaceRecognizer::create();
        try{rec->read(FACE_DATA_FILE);}catch(...){emit done(false,999);return;}
        VideoCapture cap(g_camIndex); if(!cap.isOpened()){emit done(false,999);return;}
        cap.set(CAP_PROP_FRAME_WIDTH,640); cap.set(CAP_PROP_FRAME_HEIGHT,480);
        Mat fr,gray,lastFr; double best=9999; int bestL=-1,checked=0;
        while(checked<AUTH_FRAMES){
            cap>>fr; if(fr.empty()){msleep(30);continue;}
            cvtColor(fr,gray,COLOR_BGR2GRAY); equalizeHist(gray,gray);
            vector<Rect> det; cc.detectMultiScale(gray,det,1.1,5,0,Size(80,80));
            for(auto& r:det){ Mat roi=gray(r); resize(roi,roi,Size(100,100));
                int l=-1; double c=0; rec->predict(roi,l,c);
                if(c<best){best=c;bestL=l;lastFr=fr.clone();}
                checked++;
                rectangle(fr,r,(l==1&&c<FACE_THRESHOLD)?Scalar(0,220,100):Scalar(80,80,255),2);
            }
            Mat rgb; cvtColor(fr,rgb,COLOR_BGR2RGB);
            QImage img(rgb.data,rgb.cols,rgb.rows,rgb.step,QImage::Format_RGB888);
            emit frame(img.copy()); msleep(30);
        }
        cap.release();
        emit lastFrame(lastFr);
        emit done(bestL==1&&best<FACE_THRESHOLD,(int)best);
    }
signals:
    void frame(QImage); void done(bool,int); void lastFrame(Mat);
};

// ═══════════════════════════════════════════════════
//  ВИДЖЕТ — АНИМИРОВАННЫЙ КРУГ
// ═══════════════════════════════════════════════════
class ScanRing : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float angle READ angle WRITE setAngle)
    Q_PROPERTY(float pulse READ pulse WRITE setPulse)
    Q_PROPERTY(float progress READ progress WRITE setProgress)
public:
    enum State { IDLE, SCANNING, SUCCESS, FAIL, WAITING };
    ScanRing(QWidget* p=nullptr):QWidget(p){
        setFixedSize(220,220);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        spinA=new QPropertyAnimation(this,"angle",this);
        spinA->setStartValue(0.f); spinA->setEndValue(360.f);
        spinA->setDuration(1600); spinA->setLoopCount(-1);
        pulseA=new QPropertyAnimation(this,"pulse",this);
        pulseA->setStartValue(0.f); pulseA->setEndValue(1.f);
        pulseA->setDuration(1400); pulseA->setLoopCount(-1);
        pulseA->setEasingCurve(QEasingCurve::SineCurve); pulseA->start();
        progA=new QPropertyAnimation(this,"progress",this);
    }
    float angle() const{return m_angle;} void setAngle(float v){m_angle=v;update();}
    float pulse() const{return m_pulse;} void setPulse(float v){m_pulse=v;update();}
    float progress() const{return m_prog;} void setProgress(float v){m_prog=v;update();}
    void setState(State s){
        m_state=s;
        if(s==SCANNING) spinA->start(); else spinA->stop();
        update();
    }
    void animateSuccess(){
        setState(SUCCESS);
        progA->setStartValue(0.f); progA->setEndValue(1.f);
        progA->setDuration(600); progA->start();
    }
    void animateFail(){
        setState(FAIL);
        int* cnt=new int(0); QRect orig=geometry();
        auto* t=new QTimer(this);
        connect(t,&QTimer::timeout,this,[this,t,cnt,orig](){
            int d=((*cnt)%2==0)?-8:8; move(orig.x()+d,orig.y());
            if(++(*cnt)>=8){move(orig.topLeft());t->stop();delete cnt;t->deleteLater();}
        }); t->start(50);
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        int cx=width()/2,cy=height()/2,R=95;
        QColor col;
        switch(m_state){
            case SUCCESS: col=QColor(39,174,96); break;
            case FAIL:    col=QColor(192,57,43); break;
            case WAITING: col=QColor(241,196,15); break;
            case SCANNING:col=QColor(52,152,219); break;
            default:      col=QColor(44,62,80); break;
        }
        for(int i=3;i>=0;i--){
            QColor gc=col; gc.setAlphaF(m_pulse*0.07f*(4-i));
            p.setPen(Qt::NoPen); p.setBrush(gc);
            int pr=R+8+i*8; p.drawEllipse(cx-pr,cy-pr,pr*2,pr*2);
        }
        QRadialGradient bg(cx,cy,R); bg.setColorAt(0,QColor(12,18,35)); bg.setColorAt(1,QColor(8,12,22));
        p.setPen(QPen(QColor(20,32,52),2)); p.setBrush(bg); p.drawEllipse(cx-R,cy-R,R*2,R*2);
        if(m_state==SUCCESS&&m_prog>0){
            p.setPen(QPen(QColor(39,174,96),4,Qt::SolidLine,Qt::RoundCap)); p.setBrush(Qt::NoBrush);
            p.drawArc(cx-R,cy-R,R*2,R*2,90*16,-(int)(360*16*m_prog));
        }
        if(m_state==SCANNING){
            QConicalGradient cg(cx,cy,m_angle);
            cg.setColorAt(0,QColor(52,152,219,255)); cg.setColorAt(0.3,QColor(52,152,219,100));
            cg.setColorAt(0.7,Qt::transparent); cg.setColorAt(1.0,Qt::transparent);
            p.setPen(QPen(QBrush(cg),4,Qt::SolidLine,Qt::RoundCap)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(cx-R,cy-R,R*2,R*2);
        }
        QColor fc=m_state==IDLE?QColor(40,60,90):col;
        p.setPen(QPen(fc,2.5)); p.setBrush(Qt::NoBrush);
        p.drawEllipse(cx-30,cy-36,60,60);
        if(m_state==FAIL){
            p.setPen(QPen(col,3,Qt::SolidLine,Qt::RoundCap));
            p.drawLine(cx-16,cy-10,cx-8,cy-2); p.drawLine(cx-8,cy-10,cx-16,cy-2);
            p.drawLine(cx+8,cy-10,cx+16,cy-2); p.drawLine(cx+16,cy-10,cx+8,cy-2);
            QPainterPath sad; sad.moveTo(cx-12,cy+18); sad.cubicTo(cx-6,cy+10,cx+6,cy+10,cx+12,cy+18);
            p.drawPath(sad);
        } else if(m_state==SUCCESS){
            p.setPen(Qt::NoPen); p.setBrush(col);
            p.drawEllipse(cx-16,cy-10,9,9); p.drawEllipse(cx+7,cy-10,9,9);
            p.setPen(QPen(col,2.5,Qt::SolidLine,Qt::RoundCap)); p.setBrush(Qt::NoBrush);
            QPainterPath sm; sm.moveTo(cx-12,cy+10); sm.cubicTo(cx-6,cy+20,cx+6,cy+20,cx+12,cy+10);
            p.drawPath(sm);
        } else if(m_state==WAITING){
            for(int i=0;i<3;i++){
                float a=m_angle*3.14159f/180.f+i*2.094f;
                int x=cx+(int)(18*cos(a)),y=cy+(int)(18*sin(a));
                p.setPen(Qt::NoPen); p.setBrush(col); p.drawEllipse(x-5,y-5,10,10);
            }
        } else {
            p.setPen(Qt::NoPen); p.setBrush(fc);
            p.drawEllipse(cx-16,cy-10,9,9); p.drawEllipse(cx+7,cy-10,9,9);
            p.setPen(QPen(fc,2.5,Qt::SolidLine,Qt::RoundCap)); p.setBrush(Qt::NoBrush);
            p.drawLine(cx-10,cy+14,cx+10,cy+14);
        }
        p.setPen(QPen(col,3,Qt::SolidLine,Qt::RoundCap)); int len=14;
        p.drawLine(cx-R+2,cy-R+len,cx-R+2,cy-R+2); p.drawLine(cx-R+2,cy-R+2,cx-R+len,cy-R+2);
        p.drawLine(cx+R-2,cy-R+len,cx+R-2,cy-R+2); p.drawLine(cx+R-2,cy-R+2,cx+R-len,cy-R+2);
        p.drawLine(cx-R+2,cy+R-len,cx-R+2,cy+R-2); p.drawLine(cx-R+2,cy+R-2,cx-R+len,cy+R-2);
        p.drawLine(cx+R-2,cy+R-len,cx+R-2,cy+R-2); p.drawLine(cx+R-2,cy+R-2,cx+R-len,cy+R-2);
    }
private:
    float m_angle=0,m_pulse=0.5f,m_prog=0; State m_state=IDLE;
    QPropertyAnimation *spinA,*pulseA,*progA;
};

// ═══════════════════════════════════════════════════
//  ВИДЖЕТ — КАМЕРА
// ═══════════════════════════════════════════════════
class CamView : public QLabel {
public:
    CamView(QWidget* p=nullptr):QLabel(p){
        setFixedSize(300,220); setAlignment(Qt::AlignCenter);
        setStyleSheet("background:#050810;border-radius:14px;border:1px solid #0d1a2a;color:#1a2535;font-size:12px;");
        setText("Камера");
    }
    void setFrame(const QImage& img){
        setPixmap(QPixmap::fromImage(img).scaled(size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    }
};

// ═══════════════════════════════════════════════════
//  ГЛАВНОЕ ОКНО
// ═══════════════════════════════════════════════════
class App : public QMainWindow {
    Q_OBJECT
public:
    enum Page { LOCK=0, SCAN=1, REG=2, APPS=3, SETUP=4, RECOVERY=5 };

    App(QString launch="") : m_launch(launch) {
        setWindowTitle("Face ID Protector");
        setWindowFlags(Qt::FramelessWindowHint|Qt::Window);
        setAttribute(Qt::WA_TranslucentBackground);

        if(!launch.isEmpty()){
            setWindowFlags(Qt::FramelessWindowHint|Qt::Window|Qt::WindowStaysOnTopHint);
        } else {
            setFixedSize(480,720);
            auto sg=QGuiApplication::primaryScreen()->availableGeometry();
            move((sg.width()-480)/2,(sg.height()-720)/2);
        }

        m_deviceUUID=getDeviceUUID();
        addToStartup();
        loadCamIndex();

        // Polling команд от бота каждые 5 секунд
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &App::pollBotCommand);
        m_pollTimer->start(5000);

        // Системный трей
        setupTray();

        auto* root=new QWidget(this); setCentralWidget(root);
        auto* lay=new QVBoxLayout(root); lay->setContentsMargins(0,0,0,0);
        stack=new QStackedWidget(root); lay->addWidget(stack);

        applyStyle();
        buildLock(); buildScan(); buildReg(); buildApps(); buildSetup(); buildRecov();

        if(!fileExists(PASSWORD_FILE)) goTo(SETUP); else goTo(LOCK);

        if(!fileExists(CASCADE_FILE))
            QTimer::singleShot(500,this,[this]{
                string cmd="curl -s -L -o \""+CASCADE_FILE+"\" "
                    "\"https://raw.githubusercontent.com/opencv/opencv/master/data/haarcascades/haarcascade_frontalface_default.xml\"";
                system(cmd.c_str());
            });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
        if(!m_launch.isEmpty()){ p.fillRect(rect(),QColor(4,6,12,245)); return; }
        QPainterPath path; path.addRoundedRect(rect(),20,20);
        QLinearGradient bg(0,0,0,height());
        bg.setColorAt(0,QColor(8,12,22)); bg.setColorAt(1,QColor(5,8,16));
        p.fillPath(path,bg); p.setPen(QPen(QColor(20,32,52),1)); p.drawPath(path);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if(e->button()==Qt::LeftButton) drag=e->globalPosition().toPoint()-frameGeometry().topLeft();
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        if((e->buttons()&Qt::LeftButton)&&m_launch.isEmpty()) move(e->globalPosition().toPoint()-drag);
    }

    void closeEvent(QCloseEvent* e) override {
        if(m_reallyClose || !m_tray){ e->accept(); return; }
        hide(); e->ignore();
        m_tray->showMessage("Face ID Protector",
            "Работает в фоне. Клик для открытия.",
            QSystemTrayIcon::Information, 2000);
    }

private:
    QStackedWidget* stack; QPoint drag; QString m_launch;
    string m_deviceUUID; int failCount=0; int recovIdx=0;
    QTimer* m_pollTimer=nullptr; int m_streamCount=0;
    QSystemTrayIcon* m_tray=nullptr;
    bool m_reallyClose=false;
    Mat m_lastCamFrame;

    ScanRing *lockRing,*scanRing,*regRing;
    QLabel *lockStatus,*scanStatus,*regStatus,*lockHint,*recovQuestion;
    QLineEdit *lockPass,*setupPass,*recovEdit;
    QLineEdit *hintEdits[3],*wordEdits[3];
    QLabel *setupStatus;
    QListWidget* appList;
    QProgressBar *scanProg,*regProg;
    CamView *scanCam,*regCam;

    void goTo(Page p){ stack->setCurrentIndex(p); }

    void applyStyle(){
        setStyleSheet(R"(
        * { font-family:'Segoe UI'; color:#c8d8e8; }
        QPushButton#prim {
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1565c0,stop:1 #0d47a1);
            border:none;border-radius:14px;padding:15px 40px;
            font-size:13px;font-weight:700;letter-spacing:2px;color:white;
        }
        QPushButton#prim:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1e88e5,stop:1 #1565c0);}
        QPushButton#prim:pressed{background:#0d47a1;}
        QPushButton#sec{background:transparent;border:1px solid #1a2a3a;border-radius:10px;
            padding:11px 28px;font-size:12px;color:#3a5068;}
        QPushButton#sec:hover{border-color:#2a4060;color:#5a7898;background:rgba(26,42,60,0.3);}
        QPushButton#danger{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #c62828,stop:1 #8e0000);
            border:none;border-radius:10px;padding:11px 28px;font-size:12px;font-weight:600;color:white;}
        QPushButton#danger:hover{background:#e53935;}
        QPushButton#close{background:transparent;border:none;color:#1a2a3a;font-size:18px;padding:4px 10px;}
        QPushButton#close:hover{color:#e53935;}
        QLineEdit{background:rgba(10,16,30,0.9);border:1px solid #1a2a3a;border-radius:12px;
            padding:13px 18px;font-size:13px;color:#b8ccd8;}
        QLineEdit:focus{border-color:#1565c0;}
        QListWidget{background:rgba(8,12,22,0.9);border:1px solid #1a2535;border-radius:12px;padding:6px;}
        QListWidget::item{padding:13px 16px;border-radius:8px;margin:2px 4px;color:#a0b8cc;font-size:13px;}
        QListWidget::item:hover{background:rgba(21,101,192,0.2);}
        QListWidget::item:selected{background:rgba(21,101,192,0.35);color:#e0f0ff;border:1px solid #1565c0;}
        QProgressBar{background:#080c18;border-radius:5px;border:none;height:6px;}
        QProgressBar::chunk{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1565c0,stop:1 #00acc1);border-radius:5px;}
        QScrollBar:vertical{background:#080c18;width:5px;border:none;}
        QScrollBar::handle:vertical{background:#1a2535;border-radius:2px;}
        QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}
        QScrollArea{border:none;background:transparent;}
        )");
    }

    QWidget* hdr(const QString& t, const QString& s, bool close=true){
        auto* w=new QWidget; auto* l=new QHBoxLayout(w); l->setContentsMargins(32,28,24,0);
        auto* tl=new QVBoxLayout;
        auto* lbl=new QLabel(t); lbl->setStyleSheet("font-size:20px;font-weight:700;letter-spacing:5px;color:#e8f4ff;");
        auto* sub=new QLabel(s); sub->setStyleSheet("font-size:9px;letter-spacing:3px;color:#1a4a70;");
        tl->addWidget(lbl); tl->addWidget(sub); l->addLayout(tl); l->addStretch();
        if(close){
            auto* c=new QPushButton("✕"); c->setObjectName("close"); c->setFixedSize(34,34);
            connect(c,&QPushButton::clicked,qApp,&QApplication::quit); l->addWidget(c);
        }
        return w;
    }

    QFrame* sep(){ auto* f=new QFrame; f->setFrameShape(QFrame::HLine);
        f->setStyleSheet("color:#0a1520;margin:0 8px;"); return f; }

    // ══════ LOCK ══════
    void buildLock(){
        auto* pg=new QWidget; auto* l=new QVBoxLayout(pg);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("FACE ID","PROTECTOR"));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(24);

        lockRing=new ScanRing;
        auto* cl=new QHBoxLayout; cl->addStretch(); cl->addWidget(lockRing); cl->addStretch();
        l->addLayout(cl); l->addSpacing(16);

        lockStatus=new QLabel("Нажмите для сканирования");
        lockStatus->setAlignment(Qt::AlignCenter);
        lockStatus->setStyleSheet("font-size:13px;color:#2a5878;letter-spacing:1px;");
        l->addWidget(lockStatus); l->addSpacing(24);

        auto* btn=new QPushButton("  СКАНИРОВАТЬ ЛИЦО  "); btn->setObjectName("prim"); btn->setFixedHeight(54);
        connect(btn,&QPushButton::clicked,this,&App::doAuth);
        l->addWidget(btn); l->addSpacing(22);

        auto* dl=new QHBoxLayout;
        auto* ln1=sep(); auto* ln2=sep();
        auto* orL=new QLabel("или"); orL->setAlignment(Qt::AlignCenter);
        orL->setStyleSheet("font-size:10px;color:#0d1a26;padding:0 10px;"); orL->setFixedWidth(40);
        dl->addWidget(ln1); dl->addWidget(orL); dl->addWidget(ln2);
        l->addLayout(dl); l->addSpacing(18);

        lockPass=new QLineEdit; lockPass->setPlaceholderText("Резервный пароль...");
        lockPass->setEchoMode(QLineEdit::Password); lockPass->setFixedHeight(50);
        connect(lockPass,&QLineEdit::returnPressed,this,&App::checkPass);
        l->addWidget(lockPass); l->addSpacing(8);

        auto* pb=new QPushButton("ВОЙТИ ПО ПАРОЛЮ"); pb->setObjectName("sec"); pb->setFixedHeight(44);
        connect(pb,&QPushButton::clicked,this,&App::checkPass);
        l->addWidget(pb); l->addSpacing(8);

        lockHint=new QLabel(""); lockHint->setAlignment(Qt::AlignCenter);
        lockHint->setStyleSheet("font-size:11px;color:#c62828;"); l->addWidget(lockHint);
        l->addStretch();

        auto* rb=new QPushButton("Восстановить доступ"); rb->setObjectName("sec");
        rb->setStyleSheet("border:none;color:#0d2030;font-size:10px;");
        connect(rb,&QPushButton::clicked,this,[this](){ pickRecov(); goTo(RECOVERY); });
        l->addWidget(rb);
        stack->addWidget(pg);
    }

    // ══════ SCAN ══════
    void buildScan(){
        auto* pg=new QWidget; auto* l=new QVBoxLayout(pg);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("СКАНИРОВАНИЕ","FACE ID",false));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(16);

        scanRing=new ScanRing;
        auto* cl=new QHBoxLayout; cl->addStretch(); cl->addWidget(scanRing); cl->addStretch();
        l->addLayout(cl); l->addSpacing(12);

        scanCam=new CamView;
        auto* caml=new QHBoxLayout; caml->addStretch(); caml->addWidget(scanCam); caml->addStretch();
        l->addLayout(caml); l->addSpacing(16);

        scanStatus=new QLabel("Инициализация...");
        scanStatus->setAlignment(Qt::AlignCenter);
        scanStatus->setStyleSheet("font-size:13px;color:#2a5878;");
        l->addWidget(scanStatus); l->addSpacing(10);

        scanProg=new QProgressBar; scanProg->setRange(0,AUTH_FRAMES);
        scanProg->setValue(0); scanProg->setFixedHeight(6); scanProg->setTextVisible(false);
        l->addWidget(scanProg); l->addStretch();

        auto* can=new QPushButton("ОТМЕНА"); can->setObjectName("sec"); can->setFixedHeight(44);
        connect(can,&QPushButton::clicked,this,[this](){ goTo(LOCK); });
        l->addWidget(can);
        stack->addWidget(pg);
    }

    // ══════ REG ══════
    void buildReg(){
        auto* pg=new QWidget; auto* l=new QVBoxLayout(pg);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("РЕГИСТРАЦИЯ","ЛИЦА",false));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(16);

        auto* hint=new QLabel("Смотрите прямо в камеру, не двигайтесь");
        hint->setAlignment(Qt::AlignCenter);
        hint->setStyleSheet("font-size:11px;color:#1a3a50;letter-spacing:1px;");
        l->addWidget(hint); l->addSpacing(12);

        regRing=new ScanRing;
        auto* cl=new QHBoxLayout; cl->addStretch(); cl->addWidget(regRing); cl->addStretch();
        l->addLayout(cl); l->addSpacing(10);

        regCam=new CamView;
        auto* caml=new QHBoxLayout; caml->addStretch(); caml->addWidget(regCam); caml->addStretch();
        l->addLayout(caml); l->addSpacing(14);

        regStatus=new QLabel("Запуск...");
        regStatus->setAlignment(Qt::AlignCenter);
        regStatus->setStyleSheet("font-size:13px;color:#2a5878;");
        l->addWidget(regStatus); l->addSpacing(8);

        regProg=new QProgressBar; regProg->setRange(0,REG_FRAMES);
        regProg->setValue(0); regProg->setFixedHeight(6); regProg->setTextVisible(false);
        l->addWidget(regProg); l->addStretch();
        stack->addWidget(pg);
    }

    // ══════ APPS ══════
    void buildApps(){
        auto* pg=new QWidget; auto* l=new QVBoxLayout(pg);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("ДОСТУП ОТКРЫТ","ЗАЩИЩЁННЫЕ ПРОГРАММЫ"));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(16);

        appList=new QListWidget; appList->setMinimumHeight(220);
        connect(appList,&QListWidget::itemDoubleClicked,this,&App::launchSel);
        l->addWidget(appList); l->addSpacing(12);

        auto* lb=new QPushButton("  ЗАПУСТИТЬ  "); lb->setObjectName("prim"); lb->setFixedHeight(50);
        connect(lb,&QPushButton::clicked,this,&App::launchSel);
        l->addWidget(lb); l->addSpacing(8);

        auto* row=new QHBoxLayout;
        auto* ab=new QPushButton("+ Добавить"); ab->setObjectName("sec"); ab->setFixedHeight(40);
        connect(ab,&QPushButton::clicked,this,&App::addApp); row->addWidget(ab);
        auto* rb=new QPushButton("✕ Удалить"); rb->setObjectName("sec"); rb->setFixedHeight(40);
        connect(rb,&QPushButton::clicked,this,&App::removeApp); row->addWidget(rb);
        l->addLayout(row); l->addSpacing(6);

        auto* row2=new QHBoxLayout;
        auto* camBtn=new QPushButton("🎥 Камера"); camBtn->setObjectName("sec"); camBtn->setFixedHeight(36);
        connect(camBtn,&QPushButton::clicked,this,&App::selectCamera); row2->addWidget(camBtn);
        auto* tgBtn=new QPushButton("✈️ Telegram"); tgBtn->setObjectName("sec"); tgBtn->setFixedHeight(36);
        connect(tgBtn,&QPushButton::clicked,this,&App::setupTelegram); row2->addWidget(tgBtn);
        auto* reregBtn=new QPushButton("👤 Переобучить"); reregBtn->setObjectName("sec"); reregBtn->setFixedHeight(36);
        connect(reregBtn,&QPushButton::clicked,this,&App::startReg); row2->addWidget(reregBtn);
        l->addLayout(row2); l->addSpacing(6);

        auto* lkb=new QPushButton("ЗАБЛОКИРОВАТЬ"); lkb->setObjectName("danger"); lkb->setFixedHeight(40);
        connect(lkb,&QPushButton::clicked,this,[this](){
            lockPass->clear(); lockHint->clear();
            lockRing->setState(ScanRing::IDLE);
            lockStatus->setText("Нажмите для сканирования");
            lockStatus->setStyleSheet("font-size:13px;color:#2a5878;");
            failCount=0; goTo(LOCK);
        });
        l->addWidget(lkb);
        stack->addWidget(pg);
    }

    // ══════ SETUP ══════
    void buildSetup(){
        auto* inner=new QWidget; auto* l=new QVBoxLayout(inner);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("ПЕРВЫЙ ЗАПУСК","СОЗДАНИЕ ПРОФИЛЯ",false));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(20);

        auto* d=new QLabel("Создайте резервный пароль и 3 секретных слова.\nЕсли Face ID не сработает — войдёте через них.");
        d->setAlignment(Qt::AlignCenter); d->setWordWrap(true);
        d->setStyleSheet("font-size:12px;color:#1e3a52;"); l->addWidget(d); l->addSpacing(20);

        auto* pl=new QLabel("РЕЗЕРВНЫЙ ПАРОЛЬ");
        pl->setStyleSheet("font-size:9px;letter-spacing:2px;color:#0d2030;"); l->addWidget(pl); l->addSpacing(6);
        setupPass=new QLineEdit; setupPass->setPlaceholderText("Придумайте надёжный пароль...");
        setupPass->setEchoMode(QLineEdit::Password); setupPass->setFixedHeight(48);
        l->addWidget(setupPass); l->addSpacing(20);

        auto* wl=new QLabel("3 СЕКРЕТНЫХ СЛОВА С ПОДСКАЗКАМИ");
        wl->setStyleSheet("font-size:9px;letter-spacing:2px;color:#0d2030;"); l->addWidget(wl); l->addSpacing(10);

        QStringList defHints={"Любимая игра?","Кого ты любишь?","Кличка питомца?"};
        for(int i=0;i<3;i++){
            auto* hl=new QLabel(QString("Подсказка %1:").arg(i+1));
            hl->setStyleSheet("font-size:10px;color:#0d2030;"); l->addWidget(hl);
            hintEdits[i]=new QLineEdit; hintEdits[i]->setPlaceholderText(defHints[i]);
            hintEdits[i]->setFixedHeight(42); l->addWidget(hintEdits[i]); l->addSpacing(4);
            auto* wll=new QLabel(QString("Секретное слово %1:").arg(i+1));
            wll->setStyleSheet("font-size:10px;color:#0d2030;"); l->addWidget(wll);
            wordEdits[i]=new QLineEdit; wordEdits[i]->setPlaceholderText("Ваш секретный ответ...");
            wordEdits[i]->setFixedHeight(42); l->addWidget(wordEdits[i]); l->addSpacing(12);
        }

        auto* sb=new QPushButton("  НАЧАТЬ РЕГИСТРАЦИЮ ЛИЦА  "); sb->setObjectName("prim"); sb->setFixedHeight(54);
        connect(sb,&QPushButton::clicked,this,&App::beginSetup); l->addWidget(sb); l->addSpacing(10);

        setupStatus=new QLabel(""); setupStatus->setAlignment(Qt::AlignCenter);
        setupStatus->setStyleSheet("font-size:11px;color:#c62828;"); l->addWidget(setupStatus);
        l->addStretch();
        l->addWidget(new QLabel("Данные хранятся локально"));

        auto* sa=new QScrollArea; sa->setWidget(inner); sa->setWidgetResizable(true);
        stack->addWidget(sa);
    }

    // ══════ RECOVERY ══════
    void buildRecov(){
        auto* pg=new QWidget; auto* l=new QVBoxLayout(pg);
        l->setContentsMargins(40,0,40,40); l->setSpacing(0);
        l->addWidget(hdr("ВОССТАНОВЛЕНИЕ","СЕКРЕТНОЕ СЛОВО"));
        l->addSpacing(8); l->addWidget(sep()); l->addSpacing(30);

        recovQuestion=new QLabel("Подсказка: ...");
        recovQuestion->setAlignment(Qt::AlignCenter); recovQuestion->setWordWrap(true);
        recovQuestion->setStyleSheet("font-size:16px;color:#1a6a9a;font-weight:600;");
        l->addWidget(recovQuestion); l->addSpacing(20);

        recovEdit=new QLineEdit; recovEdit->setPlaceholderText("Введите секретное слово...");
        recovEdit->setFixedHeight(50); l->addWidget(recovEdit); l->addSpacing(14);

        auto* cb=new QPushButton("ПРОВЕРИТЬ"); cb->setObjectName("prim"); cb->setFixedHeight(50);
        connect(cb,&QPushButton::clicked,this,&App::checkRecov); l->addWidget(cb); l->addSpacing(8);

        auto* bk=new QPushButton("НАЗАД"); bk->setObjectName("sec"); bk->setFixedHeight(44);
        connect(bk,&QPushButton::clicked,this,[this](){ goTo(LOCK); }); l->addWidget(bk);
        l->addStretch();
        stack->addWidget(pg);
    }

    // ═══════════════════════════════════════════════
    //  СЛОТЫ
    // ═══════════════════════════════════════════════
private slots:

    void doAuth(){
        goTo(SCAN); scanProg->setValue(0);
        scanRing->setState(ScanRing::SCANNING);
        scanStatus->setText("Сканирование лица...");
        scanStatus->setStyleSheet("font-size:13px;color:#2a5878;");
        scanCam->setText("Запуск камеры...");

        auto* w=new AuthThread;
        int* cnt=new int(0);
        connect(w,&AuthThread::frame,scanCam,&CamView::setFrame);
        connect(w,&AuthThread::frame,this,[this,cnt](const QImage&){ scanProg->setValue(++(*cnt)); });
        connect(w,&AuthThread::lastFrame,this,[this](const Mat& f){ m_lastCamFrame=f.clone(); });
        connect(w,&AuthThread::done,this,[this,w,cnt](bool ok,int conf){
            delete cnt; w->deleteLater(); scanProg->setValue(AUTH_FRAMES);
            if(ok){
                scanRing->animateSuccess();
                scanStatus->setText("Лицо распознано!");
                scanStatus->setStyleSheet("font-size:13px;color:#27ae60;");
                QTimer::singleShot(2000,this,[this](){ showApps(); });
            } else {
                failCount++;
                scanRing->animateFail();
                scanStatus->setText(QString("Не распознан (conf: %1)").arg(conf));
                scanStatus->setStyleSheet("font-size:13px;color:#e53935;");
                if(failCount>=3){
                    lockHint->setText("Попробуйте пароль или восстановление");
                    sendAlert(m_deviceUUID, failCount, m_lastCamFrame);
                }
                QTimer::singleShot(1800,this,[this](){
                    lockRing->setState(ScanRing::FAIL);
                    lockStatus->setText("Лицо не распознано");
                    lockStatus->setStyleSheet("font-size:13px;color:#e53935;");
                    goTo(LOCK);
                });
            }
        });
        w->start();
    }

    void checkPass(){
        string saved=readFile(PASSWORD_FILE);
        while(!saved.empty()&&(saved.back()=='\n'||saved.back()=='\r')) saved.pop_back();
        if(lockPass->text().toStdString()==saved){
            lockHint->clear(); lockPass->clear(); showApps();
        } else {
            lockHint->setText("Неверный пароль"); shake(lockPass); lockPass->clear();
        }
    }

    void checkRecov(){
        auto words=loadRecovWords();
        if(recovIdx<(int)words.size()&&recovEdit->text().toStdString()==words[recovIdx].word){
            recovEdit->clear(); showApps();
        } else { shake(recovEdit); recovEdit->clear(); pickRecov(); }
    }

    void showApps(){
        if(!m_launch.isEmpty()){
            goTo(SCAN); scanRing->setState(ScanRing::WAITING);
            scanStatus->setText("Запуск программы...");
            scanStatus->setStyleSheet("font-size:13px;color:#f1c40f;");
            QTimer::singleShot(2000,this,[this](){
                ShellExecuteA(NULL,"open",m_launch.toStdString().c_str(),NULL,NULL,SW_SHOW);
                QApplication::quit();
            });
            return;
        }
        appList->clear();
        for(auto& a:readLines(APPS_FILE)){
            string n=a; size_t s=a.find_last_of("/\\");
            if(s!=string::npos) n=a.substr(s+1);
            auto* it=new QListWidgetItem("    "+QString::fromStdString(n));
            it->setData(Qt::UserRole,QString::fromStdString(a));
            appList->addItem(it);
        }
        lockRing->setState(ScanRing::SUCCESS);
        lockStatus->setText("Добро пожаловать!");
        lockStatus->setStyleSheet("font-size:13px;color:#27ae60;");
        failCount=0; goTo(APPS);
    }

    void launchSel(){
        auto* it=appList->currentItem();
        if(!it){ QMessageBox::information(this,"","Выберите программу!"); return; }
        ShellExecuteA(NULL,"open",it->data(Qt::UserRole).toString().toStdString().c_str(),NULL,NULL,SW_SHOW);
    }

    void addApp(){
        QString path=QFileDialog::getOpenFileName(this,"Выберите файл","C:\\",
            "Все файлы (*.exe *.lnk *.txt *.pdf *);;Программы (*.exe)");
        if(path.isEmpty()) return;
        appendFile(APPS_FILE,path.toStdString());
        string name=path.toStdString();
        size_t s=name.find_last_of("/\\"); if(s!=string::npos) name=name.substr(s+1);
        size_t d=name.find_last_of('.'); if(d!=string::npos) name=name.substr(0,d);
        bool ok=createShortcut(path.toStdString(),name);
        QMessageBox::information(this,"Face ID Protector",
            ok?"✓ Добавлено!\nЯрлык «"+QString::fromStdString(name)+"» создан на рабочем столе."
              :"Добавлено! Ярлык создать не удалось — запустите от администратора.");
        showApps();
    }

    void removeApp(){
        auto* it=appList->currentItem(); if(!it) return;
        string t=it->data(Qt::UserRole).toString().toStdString();
        auto apps=readLines(APPS_FILE); ofstream f(APPS_FILE);
        for(auto& a:apps) if(a!=t) f<<a<<"\n";
        showApps();
    }

    void beginSetup(){
        if(setupPass->text().isEmpty()){ setupStatus->setText("Введите пароль!"); return; }
        for(int i=0;i<3;i++){
            if(wordEdits[i]->text().isEmpty()){
                setupStatus->setText(QString("Введите слово %1!").arg(i+1)); return;
            }
        }
        writeFile(PASSWORD_FILE,setupPass->text().toStdString());
        vector<RecovWord> words;
        for(int i=0;i<3;i++){
            string hint=hintEdits[i]->text().isEmpty()?
                QString("Слово %1").arg(i+1).toStdString():hintEdits[i]->text().toStdString();
            words.push_back({hint,wordEdits[i]->text().toStdString()});
        }
        saveRecovWords(words);
        setupStatus->setText("Сохранено! Запускаем камеру...");
        setupStatus->setStyleSheet("font-size:11px;color:#27ae60;");
        QTimer::singleShot(700,this,&App::startReg);
    }

    void startReg(){
        goTo(REG); regProg->setValue(0); regStatus->setText("Смотрите в камеру...");
        regRing->setState(ScanRing::SCANNING);
        auto* w=new RegisterThread;
        connect(w,&RegisterThread::frame,regCam,&CamView::setFrame);
        connect(w,&RegisterThread::progress,this,[this](int n){
            regProg->setValue(n);
            regStatus->setText(QString("Снято: %1 / %2").arg(n).arg(REG_FRAMES));
        });
        connect(w,&RegisterThread::done,this,[this,w](bool ok){
            w->deleteLater();
            if(ok){
                regRing->animateSuccess();
                regStatus->setText("Готово!");
                regStatus->setStyleSheet("font-size:13px;color:#27ae60;");
                QTimer::singleShot(1200,this,[this](){
                    QMessageBox::information(this,"Готово!","Профиль создан!\nДобавьте программы для защиты.");
                    showApps();
                });
            } else {
                regRing->animateFail();
                regStatus->setText("Ошибка камеры!");
                regStatus->setStyleSheet("font-size:13px;color:#e53935;");
                QTimer::singleShot(2000,this,[this](){ goTo(SETUP); });
            }
        });
        w->start();
    }

    void pickRecov(){
        auto words=loadRecovWords();
        if(words.empty()){ recovQuestion->setText("Слова не настроены"); return; }
        random_device rd; mt19937 g(rd());
        uniform_int_distribution<int> d(0,(int)words.size()-1);
        recovIdx=d(g);
        recovQuestion->setText("📌 "+QString::fromStdString(words[recovIdx].hint));
    }

    void selectCamera(){
        QStringList items;
        for(int i=0;i<10;i++){
            VideoCapture t(i); if(t.isOpened()){ items<<QString("Камера %1").arg(i); t.release(); }
        }
        if(items.isEmpty()){ QMessageBox::warning(this,"","Камеры не найдены!"); return; }
        bool ok; QString sel=QInputDialog::getItem(this,"Выбор камеры","Камеры:",items,g_camIndex,false,&ok);
        if(ok){ saveCamIndex(items.indexOf(sel));
            QMessageBox::information(this,"Камера","Выбрана камера "+QString::number(g_camIndex)+
                "\nПеререгистрируйте лицо."); }
    }

    void setupTelegram(){
        QString uuid=QString::fromStdString(m_deviceUUID);
        QDialog dlg(this); dlg.setWindowTitle("Привязка Telegram");
        dlg.setFixedSize(420,320);
        dlg.setStyleSheet("background:#080c18;color:#c8d8e8;font-family:'Segoe UI';");
        auto* l=new QVBoxLayout(&dlg);

        auto* info=new QLabel(
            "<b>Как привязать Telegram:</b><br><br>"
            "1. Открой бота в Telegram<br>"
            "2. Отправь команду:<br>"
            "<code style='color:#00acc1'>/register "+uuid+"</code><br><br>"
            "3. Бот пришлёт 6-значный код<br>"
            "4. Введи его ниже:");
        info->setTextFormat(Qt::RichText); info->setWordWrap(true);
        info->setStyleSheet("color:#8ab8cc;font-size:12px;"); l->addWidget(info);

        auto* copyBtn=new QPushButton("📋 Скопировать UUID"); copyBtn->setObjectName("sec");
        connect(copyBtn,&QPushButton::clicked,this,[uuid](){
            QApplication::clipboard()->setText(uuid);
        }); l->addWidget(copyBtn);

        auto* codeEdit=new QLineEdit; codeEdit->setPlaceholderText("6-значный код...");
        codeEdit->setMaxLength(6); codeEdit->setFixedHeight(48); l->addWidget(codeEdit);

        auto* confirmBtn=new QPushButton("ПОДТВЕРДИТЬ"); confirmBtn->setObjectName("prim");
        connect(confirmBtn,&QPushButton::clicked,&dlg,[&](){
            if(codeEdit->text().size()!=6){
                QMessageBox::warning(&dlg,"","Введите 6-значный код!"); return;
            }
            confirmBtn->setText("Проверяем...");
            confirmBtn->setEnabled(false); QApplication::processEvents();
            string code = codeEdit->text().toStdString();
            string json = "{\"uuid\":\""+m_deviceUUID+"\",\"code\":\""+code+"\"}";
            string resp = httpPost("/api/verify", json);
            confirmBtn->setText("ПОДТВЕРДИТЬ"); confirmBtn->setEnabled(true);
            if(resp.find("\"ok\":true")!=string::npos){
                QMessageBox::information(&dlg,"✅ Готово!",
                    "Telegram привязан!\nТеперь при попытке взлома получите фото злоумышленника.");
                dlg.accept();
            } else {
                QString errMsg = "Сервер ответил:\n" + QString::fromStdString(resp.empty() ? "Нет ответа (сервер недоступен)" : resp);
                errMsg += "\n\nUUID: " + QString::fromStdString(m_deviceUUID);
                QMessageBox::warning(&dlg,"Ошибка",errMsg);
            }
        }); l->addWidget(confirmBtn);
        dlg.exec();
    }

    // ════════════════════════════════════════════════
    //  СИСТЕМНЫЙ ТРЕЙ
    // ════════════════════════════════════════════════
    void setupTray(){
        m_tray = new QSystemTrayIcon(this);
        // Используем встроенную иконку
        m_tray->setIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
        m_tray->setToolTip("Face ID Protector");

        auto* menu = new QMenu(this);
        auto* showAct  = menu->addAction("🔓 Открыть");
        auto* lockAct  = menu->addAction("🔒 Заблокировать");
        menu->addSeparator();
        auto* quitAct  = menu->addAction("✕ Выйти");

        connect(showAct, &QAction::triggered, this, [this](){
            showNormal(); raise(); activateWindow();
        });
        connect(lockAct, &QAction::triggered, this, [this](){
            lockPass->clear(); lockHint->clear();
            lockRing->setState(ScanRing::IDLE);
            lockStatus->setText("Нажмите для сканирования");
            lockStatus->setStyleSheet("font-size:13px;color:#2a5878;");
            failCount=0; goTo(LOCK);
            showNormal(); raise(); activateWindow();
        });
        connect(quitAct, &QAction::triggered, this, [this](){
            m_reallyClose=true; close();
        });

        connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason r){
                if(r==QSystemTrayIcon::Trigger||r==QSystemTrayIcon::DoubleClick){
                    showNormal(); raise(); activateWindow();
                }
            });

        m_tray->setContextMenu(menu);
        m_tray->show();
    }

    void shake(QWidget* w){
        auto* a=new QPropertyAnimation(w,"geometry"); QRect r=w->geometry();
        a->setDuration(350);
        a->setKeyValueAt(0,r); a->setKeyValueAt(0.15,r.adjusted(-10,0,10,0));
        a->setKeyValueAt(0.30,r.adjusted(10,0,-10,0)); a->setKeyValueAt(0.45,r.adjusted(-7,0,7,0));
        a->setKeyValueAt(0.60,r.adjusted(7,0,-7,0)); a->setKeyValueAt(0.75,r.adjusted(-4,0,4,0));
        a->setKeyValueAt(1,r); a->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // ════════════════════════════════════════════════
    //  POLLING КОМАНД ОТ БОТА
    // ════════════════════════════════════════════════
    void pollBotCommand(){
        string uuid = m_deviceUUID;
        std::thread([this, uuid](){
            string resp = pollCommand(uuid);
            if(resp.empty()) return;
            if(resp.find("null")!=string::npos && resp.find("cmd")!=string::npos) return;
            string cmd;
            size_t p = resp.find("cmd");
            if(p!=string::npos){
                p = resp.find('"',p); if(p==string::npos) return; p++;
                p = resp.find('"',p); if(p==string::npos) return; p++;
                size_t e = resp.find('"',p);
                if(e!=string::npos) cmd=resp.substr(p,e-p);
            }
            if(cmd.empty()) return;
            QTimer::singleShot(0,this,[this,cmd,uuid](){
                executeBotCommand(cmd,uuid);
            });
        }).detach();
    }

    void executeBotCommand(const string& cmd, const string& uuid){
        if(cmd=="screenshot"){
            std::thread([uuid](){
                sendResult(uuid,"screenshot",captureScreen());
            }).detach();
        }
        else if(cmd=="camera"){
            std::thread([uuid](){
                sendResult(uuid,"camera",captureCamera());
            }).detach();
        }
        else if(cmd=="lock"){
            lockPass->clear(); lockHint->clear();
            lockRing->setState(ScanRing::IDLE);
            lockStatus->setText("Заблокировано удалённо");
            lockStatus->setStyleSheet("font-size:13px;color:#f39c12;");
            failCount=0; goTo(LOCK);
            LockWorkStation();
            std::thread([uuid](){ sendResult(uuid,"locked"); }).detach();
        }
        else if(cmd=="shutdown"){
            std::thread([uuid](){ sendResult(uuid,"shutdown"); }).detach();
            QTimer::singleShot(3000,this,[](){ WinExec("shutdown /s /t 0", SW_HIDE); });
        }
        else if(cmd=="reboot"){
            std::thread([uuid](){ sendResult(uuid,"reboot"); }).detach();
            QTimer::singleShot(3000,this,[](){ WinExec("shutdown /r /t 0", SW_HIDE); });
        }
        else if(cmd=="stream"){
            m_streamCount=0;
            auto* st=new QTimer(this);
            connect(st,&QTimer::timeout,this,[this,uuid,st](){
                if(m_streamCount>=5){ st->stop(); st->deleteLater(); return; }
                m_streamCount++;
                std::thread([uuid](){
                    sendResult(uuid,"stream_frame",captureScreen());
                }).detach();
            });
            st->start(1000);
        }
        else if(cmd=="askpass"){
            lockPass->clear(); lockHint->clear();
            lockRing->setState(ScanRing::IDLE);
            lockStatus->setText("Требуется верификация");
            lockStatus->setStyleSheet("font-size:13px;color:#e74c3c;");
            failCount=0; goTo(LOCK);
            LockWorkStation();
        }
        else if(cmd=="faceid"){
            // Запрос сканирования Face ID через бота
            showNormal(); raise(); activateWindow();
            lockPass->clear(); lockHint->clear();
            lockRing->setState(ScanRing::IDLE);
            lockStatus->setText("Запрос с Telegram — сканируйте лицо");
            lockStatus->setStyleSheet("font-size:13px;color:#9b59b6;");
            failCount=0; goTo(LOCK);
            QTimer::singleShot(500,this,&App::doAuth);
        }
        else if(cmd.substr(0,8)=="sendfile"){
            // cmd = "sendfile:/path/to/file"
            string path = cmd.size()>9 ? cmd.substr(9) : "";
            if(path.empty()){
                std::thread([uuid](){
                    sendResult(uuid,"file_error","","\"error\":\"no_path\"");
                }).detach();
                return;
            }
            std::thread([uuid,path](){
                // Читаем файл и отправляем base64
                ifstream f(path,ios::binary);
                if(!f){ sendResult(uuid,"file_error","","\"error\":\"not_found\""); return; }
                vector<uchar> buf((istreambuf_iterator<char>(f)),{});
                if(buf.size()>50*1024*1024){ // лимит 50MB
                    sendResult(uuid,"file_error","","\"error\":\"too_large\""); return;
                }
                // имя файла
                size_t s=path.find_last_of("/\\");
                string name = s!=string::npos ? path.substr(s+1) : path;
                string b64 = b64encode(buf);
                string extra = "\"filename\":\""+name+"\"";
                sendResult(uuid,"file",b64,extra);
            }).detach();
        }
        else if(cmd=="listapps"){
            // Отправляем список приложений
            std::thread([uuid](){
                auto apps = readLines(APPS_FILE);
                string list;
                for(size_t i=0;i<apps.size();i++){
                    size_t s=apps[i].find_last_of("/\\");
                    string name = s!=string::npos ? apps[i].substr(s+1) : apps[i];
                    if(!list.empty()) list+=",";
                    list+="{\"idx\":"+to_string(i)+",\"name\":\""+name+"\",\"path\":\""+apps[i]+"\"}";
                }
                sendResult(uuid,"apps_list","","\"apps\":["+list+"]");
            }).detach();
        }
        else if(cmd.size()>8 && cmd.substr(0,8)=="listdir:"){
            string dirPath = cmd.substr(8);
            auto expandPath=[](string p)->string{
                char buf[MAX_PATH]={0};
                if(p=="DESKTOP"){SHGetFolderPathA(NULL,CSIDL_DESKTOP,NULL,0,buf);return buf;}
                if(p=="DOWNLOADS"){SHGetFolderPathA(NULL,CSIDL_PROFILE,NULL,0,buf);return string(buf)+"\\Downloads";}
                if(p=="DOCUMENTS"){SHGetFolderPathA(NULL,CSIDL_MYDOCUMENTS,NULL,0,buf);return buf;}
                if(p=="PICTURES"){SHGetFolderPathA(NULL,CSIDL_MYPICTURES,NULL,0,buf);return buf;}
                if(p=="MUSIC"){SHGetFolderPathA(NULL,CSIDL_MYMUSIC,NULL,0,buf);return buf;}
                if(p=="VIDEOS"){SHGetFolderPathA(NULL,CSIDL_MYVIDEO,NULL,0,buf);return buf;}
                if(p=="C:") return "C:\\";
                if(p=="D:") return "D:\\";
                return p;
            };
            dirPath=expandPath(dirPath);
            std::thread([uuid,dirPath](){
                string entries; int cnt=0;
                string pat=dirPath; if(pat.back()!='\\') pat+='\\'; pat+="*";
                WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pat.c_str(),&fd);
                if(h!=INVALID_HANDLE_VALUE){
                    // Сначала папки потом файлы
                    vector<pair<bool,string>> items;
                    do {
                        string n(fd.cFileName); if(n=="."||n=="..") continue;
                        bool isD=(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0;
                        LARGE_INTEGER sz; sz.LowPart=fd.nFileSizeLow; sz.HighPart=fd.nFileSizeHigh;
                        long long kb=sz.QuadPart/1024;
                        string sn; for(char c:n){if(c=='"')sn+="\\\"";else if(c=='\\')sn+="\\\\";else sn+=c;}
                        string e="{\"name\":\""+sn+"\",\"type\":\""+string(isD?"dir":"file")+"\",\"size_kb\":"+to_string(kb)+"}";
                        items.push_back({isD,e});
                    } while(FindNextFileA(h,&fd) && items.size()<80);
                    FindClose(h);
                    // Сортируем: папки первыми
                    sort(items.begin(),items.end(),[](auto&a,auto&b){return a.first>b.first;});
                    for(auto&it:items){ if(cnt>0) entries+=","; entries+=it.second; cnt++; }
                }
                string sp; for(char c:dirPath){if(c=='\\')sp+="\\\\";else if(c=='"')sp+="\\\"";else sp+=c;}
                sendResult(uuid,"listdir","","\"path\":\""+sp+"\",\"entries\":["+entries+"]");
            }).detach();
        }
        else if(cmd.substr(0,8)=="launchapp"){
            // cmd = "launchapp:N" — запустить N-й приложение
            int idx = cmd.size()>10 ? stoi(cmd.substr(10)) : -1;
            auto apps = readLines(APPS_FILE);
            if(idx>=0 && idx<(int)apps.size()){
                string path = apps[idx];
                ShellExecuteA(NULL,"open",path.c_str(),NULL,NULL,SW_SHOW);
                std::thread([uuid,path](){
                    size_t s=path.find_last_of("/\\");
                    string name = s!=string::npos ? path.substr(s+1) : path;
                    sendResult(uuid,"app_launched","","\"name\":\""+name+"\"");
                }).detach();
            }
        }
        else if(cmd=="status"){
            char hn[256]={0}; DWORD sz=255; GetComputerNameA(hn,&sz);
            char un[256]={0}; DWORD usz=255; GetUserNameA(un,&usz);
            bool lk=(stack->currentIndex()==LOCK);
            string shn(hn), sun(un);
            string extra = string("\"status\":{\"hostname\":\"")
                + shn + string("\",\"user\":\"")
                + sun + string("\",\"locked\":")
                + string(lk?"true":"false") + string("}");
            std::thread([uuid,extra](){ sendResult(uuid,"status","",extra); }).detach();
        }
    }
};

#include "main.moc"

int main(int argc,char* argv[]){
    QApplication app(argc,argv);
    app.setStyle("Fusion");
    initPaths();

    QString launch;
    for(int i=1;i<argc;i++){
        if(string(argv[i])=="--launch"&&i+1<argc){
            launch=QString::fromLocal8Bit(argv[i+1]);
            if(launch.startsWith('"')) launch=launch.mid(1);
            if(launch.endsWith('"')) launch.chop(1);
            break;
        }
    }

    if(!launch.isEmpty()&&!fileExists(PASSWORD_FILE)){
        QMessageBox::warning(nullptr,"Face ID","Профиль не создан!\nНастройте Face ID Protector.");
        return 1;
    }

    // ── Установка в систему при первом запуске ──
    if(!isInstalled()){
        int r = QMessageBox::question(nullptr,
            "Face ID Protector — Установка",
"Установить Face ID Protector в систему?\n\n""Приложение будет скопировано в:\n""C:\\Program Files\\FaceIDProtector\\\n\n""и добавлено в автозапуск.\n""Потребуются права администратора.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if(r == QMessageBox::Yes){
            bool ok = installSelf();
            if(ok){
                // Запускаем установленную версию и выходим
                string dst = getInstallDir()+"FaceIDProtector.exe";
                ShellExecuteA(NULL,"runas",dst.c_str(),NULL,
                    getInstallDir().c_str(),SW_SHOW);
                return 0;
            } else {
                QMessageBox::warning(nullptr,"Ошибка",
"Не удалось установить.\nЗапустите файл от имени администратора.");
            }
        }
    }

    App window(launch);
    if(!launch.isEmpty()) window.showFullScreen();
    else window.show();
    return app.exec();
}
