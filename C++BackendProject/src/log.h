
#ifndef OSON_LOG_H_INCLUDED
#define OSON_LOG_H_INCLUDED 1

#pragma once

#include <cstdarg> // va_list
#include <cstddef> // size_t
#include <cstdint>  // uintptr_t 

enum LogLevel_T
{
    LogLevel_None    = 0,
    LogLevel_Fatal   = 1,
	LogLevel_Error   = 2,
	LogLevel_Warning = 3,
	LogLevel_Debug   = 4,
	LogLevel_Info    = 5
};

class CLog
{
public:
    static bool Initialize(const char*  logPath, LogLevel_T level);
	static void Destroy();
    static bool Reinit();
    
    static void set_thread_name(const char* name);
    
    void WriteArray( LogLevel_T level, const  char* info,
                     const unsigned char * buffer,  size_t nbytes );
    static void flush();
    
    CLog();
    ~CLog();

};


class ScopedLog : public CLog
{
public:
    enum Flag{ 
        Flag_none = 0,
        Flag_show_date = 1 << 1, 
        Flag_stderr_print  = 1 << 2,
        
        Flag_max_value = 1 << 30
    };
    
	~ScopedLog();
    
    ScopedLog( char const * filename, char const * funciton , int flag = 0, uintptr_t this_ = 0 );
    
	//  UFF-8 function
	//
	void ErrorLog  ( char const *  fmt, ... );
	void WarningLog( char const *  fmt, ... );
	void DebugLog  ( char const *  fmt, ... );
	void InfoLog   ( char const *  fmt, ... );

	bool FailureExit();
	void ResetStatus();

private:
    
	const char* const m_fileName;
	const char* const m_functionName;
	bool success; // this is modified.
    const int  m_flag ;
    uintptr_t  m_this;
    

};

 
#define SCOPE_LOG( variable )     ScopedLog variable( __FILE__, __FUNCTION__ )
#define SCOPE_LOGD(variable )     ScopedLog variable( __FILE__, __PRETTY_FUNCTION__ , ScopedLog::Flag_show_date )
#define SCOPE_LOGF(variable )     ScopedLog variable( __FILE__, __PRETTY_FUNCTION__ , ScopedLog::Flag_show_date | ScopedLog::Flag_stderr_print  )


#define SCOPE_LOG_C( variable )     ScopedLog variable( __FILE__, __FUNCTION__ , (uintptr_t)this )
#define SCOPE_LOGD_C(variable )     ScopedLog variable( __FILE__, __PRETTY_FUNCTION__ , ScopedLog::Flag_show_date, (uintptr_t)this )
#define SCOPE_LOGF_C(variable )     ScopedLog variable( __FILE__, __PRETTY_FUNCTION__ , ScopedLog::Flag_show_date | ScopedLog::Flag_stderr_print , (uintptr_t)this )


#endif // OSON_LOG_H_INCLUDED 
