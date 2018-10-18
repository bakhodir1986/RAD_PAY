
#include <cassert>
#include <cstdio>
#include <ctime>

#include "types.h"
#include "utils.h"
#include "exception.h"
#include "config_types.h"

Server_head_T parse_header(const uint8_t* data, size_t length)
{
    
    Server_head_T h = {};
    if (length < 18)
        return h;
    
    ByteReader_T reader(data, length);
   
    h.version       = reader.readByte4();
    h.cmd_id        = reader.readByte4();
    h.cmd_inner_id  = reader.readByte4();
    h.inner_id_val  = reader.readByte4();
    h.data_size     = reader.readByte2();
    
    return h;
}
void encode_header(const Server_head_T& head, uint8_t * data, size_t length)
{
    ByteWriter_T writer(data, length);
    writer.writeByte4(head.version);
    writer.writeByte4(head.cmd_id);
    writer.writeByte4(head.cmd_inner_id);
    writer.writeByte4(head.inner_id_val);
    writer.writeByte2(head.data_size);
}

const char * oson::error_str(Error_T e) {
    switch (e) {
    case Error_OK:           return "No error";
    case Error_DB_connection: return "DB connection failed." ;
    case Error_DB_exec:      return "Error in db query";
    case Error_EOPC_connect: return "EOPC connection failed." ;
    case Error_EOPC_not_valid_phone: return "EOPC not valid phone" ;
    case Error_timeout: return "Error timeout" ;
    default: return "Error";
        
    }
}


std::string num2string( long long number )
{
    return to_str(number);
}

std::string to_str( long long number)
{
    char buf[32] = {};
    int i = 31;
    long long abs_number = number < 0 ? -number : number;
    bool sign = number < 0;
    do{
        buf[ --i] = char( abs_number % 10 + '0' );
    }while(abs_number /= 10);
    if (sign)
        buf[ --i] = '-';
    return std::string( (const char*)(buf + i), (size_t)(31 - i) );
    
    
}

std::string to_str(double t, int precision, bool trim_leading_zero /*= true*/ )
{
    char buf[64] = {};
    size_t z = 0;
    
    if (!precision)
        precision = 8; 
    
    z = snprintf(buf, 64, "%.*f", precision, t ) ;

    if ( trim_leading_zero ) 
    {
        if ( const char* dot = ::std::char_traits<char>::find(buf, z, '.' )  ) 
        {
            size_t dot_pos = dot - buf;
         
            while(z > dot_pos && buf[z-1] == '0'){
                buf[--z] = '\0';
            }
            if (z>0 && buf[z-1] == '.' ) {
                buf[--z] = '\0';
            }
        }
    }

    return std::string(static_cast< const char*>(buf), z);
}

long long string2num( const std::string& str  )
{
    long long result = 0;
    sscanf(str.c_str(), "%lld",&result);
    return result;
}


std::string to_money_str(long long money_tiyin, char sep /* =  ',' */)
{
    char buf[64] = {};
    
    size_t sz;   
    if (money_tiyin >= 0)
    {
        sz = snprintf(buf, 60, "%lld%c%02lld", money_tiyin/100, sep, money_tiyin%100);
    }
    else
    {
        money_tiyin = -money_tiyin;
        sz = snprintf(buf, 60, "-%lld%c%02lld", money_tiyin/100, sep, money_tiyin%100);
    }
    
    return std::string ( (const char*)buf, sz);
}

std::string formatted_time(const char* format, std::time_t t)
{
    struct tm now = {};
    localtime_r(&t, &now);
    char bf[ 64 ] ={};
    strftime( bf, sizeof( bf ), format, &now );
    return (const char*)bf;
}

std::string formatted_time_now(const char* format)
{
    ::std::time_t now = ::std::time( 0 ) ; 
    return formatted_time(format, now ) ; 
}

std::time_t str_2_time(const char* str)
{
    struct tm tm={};
    if (!str || str[0] == '\0' ) return 0;
    strptime(str, "%Y-%m-%d %H:%M:%S", &tm);
    return mktime(&tm);
}
std::time_t str_2_time_T(const char* str)
{
    struct tm tm={};
    strptime(str, "%Y-%m-%dT%H:%M:%S", &tm);
    return mktime(&tm);
    
}
//----------------------------------------------------------------------------------------------------
void debug_check_length(const char* filename, size_t actual_length, size_t required_length)
{
#ifndef OSON_DEBUG_CHECK_LENGTH_DISABLED
    if (actual_length < required_length){
        char bf[256]={};
        snprintf(bf, 256, "%s: Length error: actual length: %lu, required_length: %lu ", filename, (unsigned long)actual_length, (unsigned long)required_length);
        std::string msg = (const char*)bf;
        throw oson::exception(  msg, Error_SRV_data_length );
    }
#endif
}
