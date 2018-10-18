
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cinttypes>

#include <sys/time.h> // gettimeofday

#include <mutex>

#include "log.h"


#ifdef WITHOUT_LOG


CLog::CLog(){}
CLog::~CLog(){}

bool CLog::Initialize( const char* logPath, LogLevel_T level ){ return true; }
void CLog::Destroy(){}
bool CLog::Reinit(){return false;}
//void CLog::Write( LogLevel_T level, char const * function, char const * formatString, va_list argList ){}

void CLog::WriteArray(LogLevel_T level, const char *  info, const unsigned char * array, size_t nBytes )
{}
void CLog::set_thread_name(const char* name)
{}
 
//void CLog::ErrorLog  (char const * function, char const * const formatString, ...){}
//void CLog::WarningLog(char const * function, char const * const formatString, ...){}
//void CLog::DebugLog  (char const * function, char const * const formatString, ...){}
//void CLog::InfoLog   (char const * function, char const * const formatString, ...){}
//

ScopedLog::ScopedLog( char const * fileName, char const * function, int flag, uintptr_t this_ ) 
: m_fileName(fileName)
, m_functionName(function)
, success( true )
, m_flag(flag){}

ScopedLog::~ScopedLog(){};
bool ScopedLog::FailureExit()
{
	return false;
}

void ScopedLog::ErrorLog  ( char const * const formatString, ... ){}
void ScopedLog::WarningLog( char const * const formatString, ... ){}
void ScopedLog::DebugLog  ( char const * const formatString, ... ){}
void ScopedLog::InfoLog   ( char const * const formatString, ... ){}

void CLog::flush(){}

//void ScopedLog::ErrorLog  ( wchar_t const * const formatString, ... ){}
//void ScopedLog::WarningLog( wchar_t const * const formatString, ... ){}
//void ScopedLog::DebugLog  ( wchar_t const * const formatString, ... ){}
//void ScopedLog::InfoLog   ( wchar_t const * const formatString, ... ){}

#else

#define STR_SIZE 8192
#define NUMBER_OF_LOG_BUFFERS  128 

namespace
{
struct thread_log_info{
    int deep;
    bool success;
    struct timeval tv_last;
    uint64_t thread_id ;
    char ts_buf[32];
    char name[ 8 ];// 8 symbols will used:  ex. client,  admin, db, eopc, period, notify, st_check, fault 
    
    inline thread_log_info()
    {
        init();
        
    }
    
    void init(){
        deep = 0;
        success = true;
        tv_last.tv_sec = 0;
        tv_last.tv_usec  = 0 ;
        thread_id = 0;
        memset(ts_buf, '0', 22); 
        memset(name, ' ', 8);
    }
    int datetimebuf(char* buf, size_t nbuf);
    int timebuf(char* buf, size_t nbuf);
    
    inline int spaceCount()const
    { 
        int spaceUnit = 4;
        
        int deep = this->deep < 16 ? this->deep : 16;
        
        return deep * spaceUnit; 
    }
    long long get_thread_id(){
        if ( thread_id == 0 ){
            thread_id = pthread_self();
        }
        return thread_id;
    }
private:
    long update_ts_buf();
    
};

#if __cplusplus >= 201103L 
#define OSON_THREAD_LOCAL  thread_local
#else 
#define OSON_THREAD_LOCAL  __thread 
#endif 

static std::mutex log_m;
static char*   filename ;
static FILE*  outfile ;


static LogLevel_T     g_level = LogLevel_Error;
static OSON_THREAD_LOCAL thread_log_info g_log_info;


static char* oson_strdup(const char* s)
{
    if (NULL == s) return NULL ;

    int len = strlen(s);

    char* r = static_cast< char* >( malloc(len + 1) );

    if (NULL == r) return r;

    memcpy(r, s, len + 1 ) ;

    return r;
}

long thread_log_info::update_ts_buf()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec != tv_last.tv_sec ){
        struct tm tm;
        localtime_r(&tv.tv_sec, &tm);
        //2017-12-23 17:51:52.123456  --> total 26 symbols
        snprintf(ts_buf, 32, "%04d-%02d-%02d\n%02d:%02d:%02d.%06ld",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, (long int)tv.tv_usec);
    } else {
        long int u = tv.tv_usec;
        for(int i= 0;i < 6; ++i){
            ts_buf[25 - i ] = u % 10 + '0', u /= 10 ;
        }
    }
    tv_last = tv;
    return tv.tv_usec;
}

int thread_log_info::datetimebuf(char* buf, size_t nbuf)
{
    update_ts_buf();
    memcpy(buf, ts_buf, 26);
    
    return 26;
}

int thread_log_info::timebuf(char* buf, size_t nbuf)
{
    update_ts_buf(); // CCYY-MM-DD\n    4+2+2+3=11
    memcpy(buf, ts_buf + 11, 15);
    return 15; // '17:51:52.123456' ==> total 15 symbols.
}


} // namespace oson

#ifdef _WIN32
	#define vsnprintf vsprintf_s
	#define snprintf sprintf_s
#endif

//////////////////////////////////////////////////////////////////////////
//
//
// 
//////////////////////////////////////////////////////////////////////////

static const char* LevelString( LogLevel_T level )
{
    typedef const char* pchar;
    
	static const pchar levelStrings[] = {
		"NONE :    ", // 0
        "FATAL :   ", // 1
		"ERROR :   ", // 2
		"WARNING : ", // 3
		"DEBUG :   ", // 4
		"INFO :    ", // 5
		"BEGIN :   ", // 6
		"END :     ", // 7
		"UNKNOWN : " //  8
	};
    
    return levelStrings[  (int)level < 8 ? level : 8 ];
}

static inline size_t digits(unsigned long long u){
    size_t r = 0; do ++r ; while(u/=10);
    return r;
}
//
//static int ThreadId2Buffer(char* buf, size_t nbuf)
//{
//    pthread_t id = pthread_self();
//    //140 355 313 772 288
//    unsigned long long u = static_cast< unsigned long long > ( id ) ;
//    size_t du = digits(u);
//    assert(nbuf > du);
//    buf[ du ] = '\0';
//    for(size_t i = du; i-->0; u /= 10)
//        buf[ i ] = static_cast< char>( u % 10 + '0' ) ;
//    
//    return du;
//}

static void SetStatus( bool success )
{
    g_log_info.success = success; 
}

static void WriteToFile( const char* logString, size_t length, bool do_flush)
{
   // std::lock_guard< std::mutex > lock(log_m);
    //@Note than outfile is single for all thread, so lock mutex is needn't.
    
    fwrite(logString, 1, length, outfile); 
    if (do_flush)
        fflush(outfile);
}

    //  Utf-8 function
    //
static size_t makeString(char* buf, size_t buf_size, bool show_date, LogLevel_T level, char const * function, char const * logString, va_list argList );

////////////////////////////////////////////////////////////////////////////////////
enum LogLevelInternal_T
{
    LogLevelInternal_Begin = LogLevel_Info + 1,
    LogLevelInternal_End   = LogLevel_Info + 2
};


//////////////////////////////////////////////////////////////////////////
//
// 
//
/////////////////////////////////////////////////////////////////////////


CLog::CLog()
{
}

CLog::~CLog()
{
}

bool CLog::Initialize( const char* logPath, LogLevel_T level )
{
    /**** STATIC OBJECTS MUST BE LOCKED WITH MUTEX! */
    {
        std::lock_guard< std::mutex > lock(log_m);
        
        FILE* file = fopen(logPath, "a");
        if (NULL == file) 
            return false;
        
        outfile = file;
        filename = oson_strdup(logPath);
        g_level = level;
    }

    /**** local members. ********/
    {
        char buffer[256];
        int sz;

        char dt[40] = {};
        
        sz = g_log_info.datetimebuf( dt, 40 ) ;
        for(int i = 0; i < sz; ++i) { // I don't want include algorithm library!
            if (dt[i] == '\n') dt[i] = ' ';
        }
        
        sz = snprintf(buffer, 256, "\n==================== %s ====================\n", dt);
        WriteToFile(buffer, sz, true );// do flush
    }
	return true;
}

void CLog::Destroy()
{
    std::lock_guard < std::mutex > lock(log_m);
    fclose(outfile);//auto flushes.
    outfile = NULL;
    free(filename);
    filename = NULL ;
}

void CLog::set_thread_name(const char* name)
{
    thread_log_info& thinfo = g_log_info;
    
    size_t i = 0;
        
    if ( name != NULL ){
        while( i < 8 && name[i] != '\0' ) {
            thinfo.name[i] = name[i];
            ++i;
        }
    }
    
    while(i < 8 ) {
        thinfo.name [ i ] = ' '; 
        ++i;
    }
}
bool CLog::Reinit()
{
    char* logPath = oson_strdup( filename )  ;
    Destroy();
    
    bool ret = Initialize( logPath, g_level);
    
    free(logPath);
    return ret;
}

static bool allowedLevel( LogLevel_T level)
{
    if( level > g_level )
	{
		bool isDebug = ( g_level >= LogLevel_Debug && level > LogLevel_Info );
		bool isInfo = ( g_level >= LogLevel_Info && level >= LogLevel_Info );
		if( isDebug || isInfo ) {
		} else {
			return false;
		}
	}
    return true;
}
static size_t makeHeader(char *buffer, size_t buffer_size, bool show_date, LogLevel_T level , const char* function)
{
    //  CCYY-MM-DD hh:mm:ss.ms thread-id | <spaces> level func : user-info
    thread_log_info& thinfo = g_log_info;
    
    char* pb = buffer;
    int sz;
    //1.date
    
    if(show_date){
        sz =  thinfo.datetimebuf(pb, 30); 
        pb += sz;
        *pb++ = ' ';
    }
    else
    {
        //2. Time only
        sz = thinfo.timebuf(pb, 30);
        pb += sz;
        *pb++ = ' ';
    }
    
    //3. Thread id
    {
        uint64_t id = thinfo.get_thread_id();
        for(int i = 0; i < 16; ++i)
            pb[ 15 - i ] = id % 10 + '0', id /= 10 ;
        sz  = 16;
    }
    pb += sz;
    *pb++ = ' ';
    *pb++ = '|';
    //3. deeper spaces
    sz = thinfo.spaceCount();
    if ( sz >= 8 )
    {
        sz -= 8;
        if (sz < 2)sz = 2;
        for(int i = 0; i != 8; ++i)
            *pb++ = thinfo.name[i]; 
        *pb++ = '|';
    }
    else {
        sz = 2;
        for(int i = 0; i != 8; ++i)
            *pb++ = thinfo.name[i];
        *pb++ = '|';
    }
    
    if (sz > 0){
        memset(pb, ' ', sz);
        pb += sz;
    }
    //4. level string
    const char* slevel = LevelString(level);
    sz = strlen(slevel);
    memcpy(pb, slevel, sz);
    pb += sz;
   
    //5. function
    const char* ifun = strchr(function, '(');
    bool hasArg = false;
    
    int const sz_fun = strlen(function);
    
    if (!ifun )
    {
        sz = sz_fun;
        memcpy(pb, function, sz);
        pb += sz;
    }   
    else
    {
        //@fix operator ()()
        if (ifun - function < sz_fun - 2 && ifun[ 2 ] == '(')
            ifun += 2;
 
        hasArg = ( ifun - function < sz_fun - 1 && ifun[1] != ')' ) ;
        
        sz = ifun - function;
        memcpy(pb, function, sz);
        pb += sz;
    }
    *pb++  = '(' ;
    if (hasArg){
        *pb++ = '.'; 
        *pb++ = '.';
        *pb++ = '.';
    }
    *pb++  = ')' ;
    
    *pb++ = ' ';
    
    return size_t(pb - buffer);
}


static size_t makeString(char* buffer, size_t buffer_size, bool show_date, LogLevel_T level, char const * function, char const * formatString, va_list argList )
{
    
    char* pb = buffer;
    int sz;
    sz= makeHeader(buffer, buffer_size, show_date, level, function);
    pb += sz;
    
    //user messages
    size_t remain = (pb < buffer + buffer_size ) ? buffer + buffer_size - pb - 1  :  0;
	sz = vsnprintf( pb, remain, formatString, argList );
    if (sz<0)sz = 0;//iff error, skip it.
    pb += sz;
    *pb++ = '\n';

    return size_t(pb - buffer);
}

void CLog::flush()
{
    fflush(outfile);
}

void CLog::WriteArray( LogLevel_T level, const char * info, const unsigned char * buffer,   size_t nbytes  )
{
    enum{ n_pos = 32 , n_mask = n_pos - 1};
    
	if( level > g_level ) {
		return;
	}
    thread_log_info& thinfo = g_log_info;
    
    enum{ nlog = 4096 };
    char logString[ nlog ] = {};
    char* plog = logString;
    int np;

    np =  thinfo.timebuf(plog, 30);
    plog += np;
    *plog++ = ' ';
    
    {
        uint64_t id = thinfo.get_thread_id();
        np = 16;
        for(int i= 0; i < 16; ++i)
            plog[15-i] = id % 10 + '0', id/=10;
    }
    plog += np;
    *plog++ = ' ';
    *plog++ = '|';
    *plog++ = ' ';
    
    np = strlen(info);
    memcpy(plog, info, np);
    plog += np;
    *plog++ = ' ';

    for ( size_t i = 0; i < nbytes; ++i )
    {
        if ( !(i  & n_mask ) && nbytes > n_pos) // if there more lines.
        {
            long d = i;
            plog[7] = char(' ');
            plog[6] = char('|');
            plog[5] = char(' ');
            plog[4] = char(d%10+'0'); d/=10;
            plog[3] = char(d%10+'0'); d/=10;
            plog[2] = char(d%10+'0'); d/=10;
            plog[1] = char(d%10+'0'); d/=10;
            plog[0] = char('\n');
            
            plog += 8;
        }
        
        
        {
            unsigned u = buffer[i];
            
            plog[0] = "0123456789abcdef"[ u >> 4   ];
            plog[1] = "0123456789abcdef"[ u & 0x0F ];
            plog[2] = ' ';
            plog += 3;
            
        }
        
        if (( i & 7 ) == 7)
           *plog++ = ' ';
        
        
        if ( (plog - logString) >= nlog - 128 && (nbytes - i > 1) )
        {
            np = snprintf(plog, 128, " ... ( %u bytes )\n ", (unsigned)(nbytes - i - 1));
            plog += np;
            break;
        }
    }

    *plog++ = '\n';
    
    WriteToFile( (const char*)logString, plog - logString, false );
}


//////////////////////////////////////////////////////////////////////////
//
// Scoped log
// 
//////////////////////////////////////////////////////////////////////////
ScopedLog::ScopedLog( char const * fileName, char const * function, int flag /* = 0 */, uintptr_t this_ ) 
: m_fileName(fileName)
, m_functionName(function)
, success( true )
, m_flag( flag )
, m_this(this_)

{
    LogLevel_T level = (LogLevel_T)LogLevelInternal_Begin;
    
    //if (allowedLevel(level))
    {
        bool show_date = (flag & Flag_show_date) != 0;
        va_list argList;
        char buffer[STR_SIZE + 4 ] = {};
        size_t len =  makeString(buffer, STR_SIZE, show_date, level, m_functionName, "", argList );
        
        if ( m_this ) 
        {
            --len;
            
            //we know last symbol is '\n' so we must start buffer[len-1] position.
            len += snprintf( buffer + len , 64, " this: %016" PRIxPTR "\n", m_this );
        }
        
        bool do_flush = false;//show_date;
        WriteToFile(buffer, len, do_flush);
        
        if (m_flag & Flag_stderr_print)
        {
            fputs(buffer, stderr);
        }
    }
    
    //set thread info
    {
        
        g_log_info.deep++;
        g_log_info.success = success;
    }
}

ScopedLog::~ScopedLog()
{
    struct timeval tv_last ;
    // get thread info
    {
        thread_log_info& info = g_log_info; //getThreadInfo(id);
        tv_last = info.tv_last ;
		if (success) 
            success = info.success;
        else
            info.success = false;
        
        info.deep --;
        if (info.deep <= 0)
            info.deep = 0;
    }

    typedef const char* pchar;
    static const pchar status_str[] = {  "FAILURE",  "SUCCESS"   };
    
    LogLevel_T level = (LogLevel_T)LogLevelInternal_End;
    
    //if (allowedLevel(level))
    {
        bool show_date = (m_flag & Flag_show_date) != 0;
        
        va_list argList;
        char buffer[STR_SIZE+4]={};
        size_t len = makeString(buffer, STR_SIZE, show_date, level, m_functionName, status_str[success], argList );
        
        if (m_this)
        {
            --len;
            len += snprintf(buffer + len, 64, " this: %016" PRIxPTR  "  \n", m_this);
        }
        struct timeval tv_cur = g_log_info.tv_last;
        long long diff = (tv_cur.tv_sec * 1000000ll + tv_cur.tv_usec) - (tv_last.tv_sec * 1000000ll + tv_last.tv_usec) ;
        
        bool do_flush  = show_date && diff > 10* 1000 ; //10milliseconds.
        WriteToFile(buffer, len, do_flush);
        
        if (m_flag & Flag_stderr_print)
        {
            fputs(buffer, stderr);
            fflush(stderr);
        }
    }
}


bool ScopedLog::FailureExit()
{
    success = false;
	return false;
}

void ScopedLog::ResetStatus()
{
	SetStatus( true );
}


void ScopedLog::ErrorLog( char const *  fmt, ... )
{
    //printLog(LogLevel_Error, m_functionName, formatString, ... );
    if (allowedLevel(LogLevel_Error))
    {
        bool show_date = ( m_flag & Flag_show_date ) != 0 ;
        va_list argList;
        va_start( argList, fmt );
        char buffer[STR_SIZE+4];    
        size_t sz = makeString(buffer, STR_SIZE, show_date, LogLevel_Error, m_functionName, fmt, argList );

        va_end( argList );
        
        //after va_end.
        bool do_flush = true;
        WriteToFile(buffer, sz, do_flush);
    }

    success = false;
}

void ScopedLog::WarningLog( char const *  fmt, ... )
{
    if (allowedLevel(LogLevel_Warning))
    {
        bool show_date = ( m_flag & Flag_show_date ) != 0 ;
        va_list argList;
        va_start( argList, fmt );
        char buffer[STR_SIZE + 4];
        size_t len = makeString(buffer, STR_SIZE, show_date, LogLevel_Warning, m_functionName, fmt, argList );

        va_end( argList );
        
        bool do_flush = true;
        WriteToFile( buffer, len, do_flush);
    }
}


void ScopedLog::DebugLog( char const *  fmt, ... )
{
    if ( allowedLevel(LogLevel_Debug))
    {
        bool show_date = ( m_flag & Flag_show_date ) != 0 ;
        va_list argList;
        va_start( argList, fmt );

        char buffer[STR_SIZE + 4];
        size_t len = makeString(buffer, STR_SIZE, show_date, LogLevel_Debug, m_functionName, fmt, argList );

        va_end( argList );
        
        bool do_flush = false ;
        WriteToFile(buffer, len, do_flush);
    }
}


void ScopedLog::InfoLog( char const *  fmt, ... )
{
    if ( allowedLevel(LogLevel_Info))
    {
        bool show_date = ( m_flag & Flag_show_date ) != 0 ;
        
        va_list argList;
        va_start( argList, fmt );
        char buffer[STR_SIZE + 4];
       
        size_t len = makeString(buffer, STR_SIZE, show_date, LogLevel_Info, m_functionName, fmt, argList );
        va_end( argList );
        
        bool do_flush = false;
        WriteToFile(buffer, len, do_flush);
    }
}


#endif

