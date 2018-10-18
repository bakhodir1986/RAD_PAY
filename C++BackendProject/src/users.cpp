
#include <functional>
#include <memory>

#include "users.h"
#include "Pusher.h"
#include "msg_android.h"

#include "utils.h"
#include "png_image.h"
#include "log.h"
#include "DB_T.h"
#include "config_types.h"
#include "application.h"
#include "news.h"


#include <boost/asio/io_service.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>
 
#include <png.h>
#include <qrencode.h>

#include <cctype>
#include <boost/algorithm/string/split.hpp>

bool valid_phone( const std::string& phone)
{
    if (phone.length() < 7)
        return false;
    for(size_t i = 0 ; i < phone.size(); ++i){
        if ( ! isdigit(phone[i]))
            return false;
    }
    return true;
}

static const char* msg_type_str(Msg_type__T tp)
{
    const char* type_str = "Unknown type";

    switch(tp)
    {
        case MSG_TYPE_REQUEST               : type_str = "request"              ; break;
        case MSG_TYPE_TRANSACTION           : type_str =  "transaction"         ; break;
        case MSG_TYPE_MESSAGE               : type_str = "notification"         ; break;
        case MSG_TYPE_BULK_MESSAGE          : type_str = "bulk_message"         ; break;
        case MSG_TYPE_PERIODIC_BILL_MESSAGE : type_str = "periodicbill"         ; break;
        case MSG_TYPE_PURCHASE_MESSAGE      : type_str = "purchase_message"     ; break;
        case MSG_TYPE_BONUS_MESSAGE         : type_str = "bonus_message"        ; break;
        default: type_str = "Unknown type";
    }
    return type_str;
}
//==========================================================================================================================================
     
static app_version::value_type app_version_from_str(const std::string& s )
{
    //SCOPE_LOG(slog);
    
    app_version::value_type result = 0;
    int v = 0, state = 0, chunk = 0;
    for(char c : s){
        if (c >='0' && c <='9'){
            v = v * 10 + (c - '0');
            state = 1;
        } else {
            if (state == 1 ) {
                state = 0;
                v &= 255;
                //@note take at most first 7 chunks
                if(chunk < 7)
                    result  = (result << 8) | (v);
                v = 0;
                ++chunk;
            }
        }
    }
    if (state == 1 ) {
        
        v &= 255; 
        //@note take at most first 7 chunks
        if (chunk < 7)
            result = (result << 8) | (v);
        
        ++chunk;
    }

    // Дополни до 7 если меньше chunks.
    if (chunk < 7){
        for(int i = chunk; i < 7; ++i)result = result << 8;
    }
//    
    //slog.DebugLog("s = %s  chunk: %d, result = 0x%016llx", s.c_str(), chunk, result);
    
    return result;
}

static std::string app_version_to_str( app_version::value_type value)
{
    //SCOPE_LOG(slog);
    
    int chunk = 7;
    
    std::string result;
    
    //@note remove leading zero bytes.
    while(chunk > 1 && !(value & 255 ) )
        value = value >> 8 , --chunk;
    
    for(int i = chunk-1; i >=0 ; --i)
    {
        int v = (value >> (i*8) ) & 255;
        
        if (!result.empty())
            result += '.';
        
        //@note v has at most 3 digits
        if (v < 10){
            result += (char)(v + '0');
        } else if (v < 100){
            result += (char)(v/10 + '0');
            result += (char)(v%10 + '0');
        } else {
            result += (char)(v/100 + '0');
            result += (char)((v/10)%10 + '0');
            result += (char)(v%10 + '0');
        }
    }
    //slog.DebugLog("value = 0x%016llx  chunk: %d  result = %s", value, chunk, result.c_str());
    return result;
}

app_version::app_version(): value(0){}
    
app_version::app_version( const std::string & in ) : value(app_version_from_str(in))
{
    
}
    
std::string app_version::to_str()const{ return app_version_to_str(value); } 
     
app_version::value_type app_version::compare(const app_version & app)const {
    return    value -  app.value   ;
}
 

oson::App_info_table_T::App_info_table_T(DB_T& db) : m_db(db){}

oson::App_info_table_T::~App_info_table_T() {}

static std::string make_where (const oson::App_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.id != 0 )result += " AND ( id = " +escape(search.id) + " ) " ;
    if ( ! search.os.empty()) result += " AND (os = " + escape(search.os ) + " ) " ;
    if ( ! search.version.empty()) result += " AND (version = "+ escape(search.version) + " ) " ;
    if ( ! search.release_date.empty()) result += " AND (release_date = " + escape(search.version) + " ) " ;
    if ( ! search.expiry_date.empty()) result += " AND ( expiry_date = " + escape(search.expiry_date) + " ) " ;
    
    return result;
}

oson::App_info_list_T oson::App_info_table_T::list( const App_info_T& search, const Sort_T& sort ) 
{
    SCOPE_LOG(slog);
    std::string where_s = make_where(search), sort_s = sort.to_string();
    std::string query = "SELECT id, version, os, release_date, expiry_date FROM oson_app_info WHERE " + where_s + sort_s;
    
    oson::App_info_list_T result;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    size_t rows = st.rows_count() ;
    
    result.resize(rows);
    for(size_t i = 0; i != rows; ++ i){
        st.row(i) >> result[i].id >> result[i].version >> result[i].os >> result[i].release_date >> result[i].expiry_date ;
    }
    
    return result;
}



int32_t oson::App_info_table_T::add(const App_info_T& info)
{
    SCOPE_LOG(slog);
    const App_info_T& i = info;
    std::string query = "INSERT INTO oson_app_info (id, version, os, release_date, expiry_date) VALUES ( "
                        " DEFAULT, " + escape(i.version) + ", " + escape(i.os ) + ", " + escape(i.release_date) + ", " + escape(i.expiry_date) + ") "
                        " RETURNING id ; ";
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int32_t id = 0;
    st.row( 0 ) >> id;
    
    return id; 
}

int32_t oson::App_info_table_T::edit(int32_t id, const App_info_T& info)
{
    SCOPE_LOG(slog);
    
    std::string query = " UPDATE oson_app_info SET "
                        " version = " + escape(info.version) + ", "
                        " os = " + escape(info.os)           + ", " 
                        " release_date = " + escape(info.release_date) + ", "
                        " expiry_date = "  + escape(info.expiry_date)  +   " "
                        " WHERE id = "     + escape(id) ;
    
    DB_T::statement st( m_db );
    
    st.prepare(query);
    
    return st.affected_rows();
}

int32_t oson::App_info_table_T::del(int32_t id)
{
    SCOPE_LOG(slog);
    std::string query  = "DELETE FROM oson_app_info WHERE id = " + escape( id ) ;
     
    
    DB_T::statement st(m_db);
    
    st.prepare(  query  ) ;
    
    return st.affected_rows() ;
}


oson::App_info_T oson::App_info_table_T::get_last(const std::string& os)
{
    SCOPE_LOG(slog);
    
    oson::App_info_T result;
    
    std::string query = "SELECT id, version, os, release_date, expiry_date , min_version FROM oson_app_info WHERE os = " + escape(os) + " ORDER BY release_date DESC LIMIT 1" ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1){
        slog.WarningLog("Not found!") ;
    } else {
        st.row(0) >> result.id >> result.version >> result.os >> result.release_date >> result.expiry_date >> result.min_version;
    }
    return result;
}


//==========================================================================================================================================


oson::Users_full_info_table::Users_full_info_table(DB_T& db) : m_db( db ) {}

oson::Users_full_info_table::~Users_full_info_table() {}

static std::string make_where(const oson::Users_full_info& s)
{
    std::string result = "(1=1)";
    if (s.id != 0)result += " AND (id = " + escape(s.id) + " ) " ;
    if (s.uid != 0) result += " AND (uid = " + escape(s.uid) + " ) " ;
    if (s.status != 0) result += " AND (status = " + escape(s.status ) + " ) " ;
    if (s.level != 0 ) result += " AND (level = " + escape(s.level ) + " ) " ;
    if ( ! s.nationality.empty() ) result += " AND ( nationality = " + escape(s.nationality) + ") " ;
    if ( ! s.citizenship.empty() ) result += " AND ( citizenship = " + escape(s.citizenship) + " ) " ;
    
    return result;
}

oson::Users_full_info_list  oson::Users_full_info_table::list(const oson::Users_full_info& search, const Sort_T& sort) 
{
    SCOPE_LOGD(slog);
    // id
    // uid
    // FIO
    // PassportNumber
    // PassportSerial
    // PassportStartDate
    // PassportEndDate
    // PassportImagePath
    // DOB
    // Nationality
    // Citizenship
    // status
            
    oson::Users_full_info_list result;
    std::string where_s = make_where(search), sort_s = sort.to_string();
    std::string query = " SELECT id, uid,  fio ,  passport_number , passport_serial,  passport_start_date , "
                        " passport_end_date ,  passport_image_path ,  date_of_birth ,  nationality ,  citizenship , status, level  "
                        " FROM users_full_registers WHERE " + where_s + sort_s ;
    

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    size_t rows = st.rows_count() ;
    
    result.list.resize(rows);
    
    int i = 0;
    for( auto & e: result.list )
    {
        st.row(i++) >> e.id >> e.uid >> e.fio >> e.passport_number >> e.passport_serial >>  e.passport_start_date >> e.passport_end_date
                    >> e.passport_image_path >> e.date_of_birth >> e.nationality >> e.citizenship >> e.status >> e.level ;
    }
    
    int total_count = sort.total_count( rows );
    
    if ( total_count < 0 ) //can't determine total count
    {
        query = "SELECT COUNT(*) FROM users_full_registers WHERE " + where_s;
        st.prepare(query);
        st.row(0) >> total_count;
        
    }
    
    result.total_count = total_count;
    
    return result;
}

oson::Users_full_info  oson::Users_full_info_table::get_by_uid( int64_t uid) 
{
    SCOPE_LOGD(slog);
    oson::Users_full_info result;
    std::string query = " SELECT id, uid, fio, passport_number , passport_serial, passport_start_date, "
                        " passport_end_date  ,  passport_image_path, date_of_birth, nationality, citizenship, status, level "
                        "  FROM users_full_registers WHERE uid = " + escape(uid) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1)
    {
        slog.WarningLog("uid = %lld  Not found", (long long ) uid ) ;
        return result;
    }
    
    
    {
        auto& e = result;
        st.row(0) >> e.id >> e.uid >> e.fio >> e.passport_number >> e.passport_serial >>  e.passport_start_date >> e.passport_end_date
                  >> e.passport_image_path >> e.date_of_birth >> e.nationality >> e.citizenship >> e.status >> e.level ;
    
    }
    
    return result;
}

int64_t oson::Users_full_info_table::add(const oson::Users_full_info & info)
{
    SCOPE_LOGD(slog);
    
    std::string query = "INSERT INTO users_full_registers (id, uid, fio, passport_number, passport_serial, passport_start_date,"
                        " passport_end_date, passport_image_path, date_of_birth, nationality, citizenship, status, level ) VALUES (  "
                        " DEFAULT, "   
                        + escape(info.uid ) + " , "  
                        + escape(info.fio ) + " , "
                        + escape(info.passport_number)      + " , " 
                        + escape(info.passport_serial)      + " , "
                        + escape(info.passport_start_date)  + " , " 
                        + escape(info.passport_end_date )   + " , " 
                        + escape(info.passport_image_path ) + " , "
                        + escape(info.date_of_birth)  + " , "
                        + escape(info.nationality)    + " , "
                        + escape(info.citizenship)    + " , "
                        + escape(info.status )        + " , "
                        + escape(info.level  )        + " ) "
                        " RETURNING id ; " 
                        ;
    
    
    DB_T::statement st( m_db ) ;
    
    st.prepare( query );
    
    int64_t id = 0 ;
    
    st.row( 0 ) >> id ;
    
    return id ;
}


int32_t oson::Users_full_info_table::edit( int64_t id, const oson::Users_full_info & info ) 
{
    
    SCOPE_LOGD( slog ) ;
    //id and uid won't be update never.
    std::string query = "UPDATE users_full_registers SET " 
                        " fio = "                 + escape(info.fio )                 + " , "
                        " passport_number = "     + escape(info.passport_number)      + " , " 
                        " passport_serial = "     + escape(info.passport_serial)      + " , "
                        " passport_start_date = " + escape(info.passport_start_date)  + " , " 
                        " passport_end_date = "   + escape(info.passport_end_date )   + " , " 
                        " passport_image_path = " + escape(info.passport_image_path ) + " , "
                        " date_of_birth =  "      + escape(info.date_of_birth)        + " , "
                        " nationality =  "        + escape(info.nationality)          + " , "
                        " citizenship =  "        + escape(info.citizenship)          + " , "
                        " status =  "             + escape(info.status )              + " , "
                        " level = "               + escape(info.level )               + "   "
                        " WHERE id =  "           + escape( id ); 
                        ;
    
    
    DB_T::statement st( m_db ) ;
    
    st.prepare( query );
    
    return st.affected_rows();
}

int32_t oson::Users_full_info_table::del( int64_t id )
{
    SCOPE_LOGD( slog ) ;
    
    std::string query = "DELETE FROM users_full_registers WHERE id = " + escape(id);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return st.affected_rows();
}

/******************************************************************************************************/
Activate_table_T:: Activate_table_T( DB_T& db ) 
    : m_db( db )
{}

Activate_table_T::~Activate_table_T()
{}

int64_t Activate_table_T::add(const Activate_info_T& info)
{
    SCOPE_LOG(slog);
    std::string query = 
            " INSERT INTO activate_code (id, phone, code, add_ts, dev_id, valid, kind, other_id) VALUES ( " 
            " DEFAULT, " + escape(info.phone) + ", " + escape(info.code) + ", " + escape(info.add_ts) + ", " + 
              escape(info.dev_id) + ", 'TRUE', " + escape(info.kind) + ", " + escape(info.other_id) + " ) RETURNING id " ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int64_t id = 0;
    st.row(0) >> id;
    return id;
}

static std::string make_where(const Activate_info_T& search, bool check_code_empty ){
    
    std::string ret = " ( valid = 'TRUE' ) " ; // search always-always valid codes!
    
    if ( search.kind != Activate_info_T::Kind_none ) 
        ret += "AND ( kind = " + escape(search.kind) + " ) " ; 
    
    if (search.id != 0) 
        ret += "AND ( id = " + escape(search.id) + ") " ;
    
//    if ( ! search.code.empty() )
    if ( !check_code_empty || ! search.code.empty() )
        ret += " AND (code = " + escape(search.code) + ") " ;
    
    if (! search.dev_id.empty() )
        ret += " AND (dev_id = " + escape(search.dev_id) + ") " ;
    
    if( ! search.phone.empty() )
        ret += " AND (phone = " + escape(search.phone) + " ) " ;
        
    if (  0 != search.other_id )
        ret += " AND (other_id = " + escape(search.other_id) + ") " ;

    if( ! search.add_ts.empty() ) 
       ret += " AND (add_ts >= " +  escape(search.add_ts) + " ) " ;
        
    return ret;
}

Activate_info_T Activate_table_T::info(const Activate_info_T& search)
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT id, phone, code,  dev_id,  kind, other_id FROM activate_code WHERE " + make_where( search , false ) ;
    
    DB_T::statement st( m_db );
    st.prepare(query);
    
    Activate_info_T data;
    
    if (st.rows_count() != 1) {
        slog.WarningLog("Not found!");
        return data;
    }
    
    st.row(0) >> data.id >> data.phone >> data.code >>  data.dev_id >> data.kind >> data.other_id ;
    
    return data;
}

int Activate_table_T::count(const Activate_info_T& search)
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT COUNT(*) FROM activate_code WHERE " + make_where(search, true);
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int cnt = 0 ;

    st.row( 0 ) >> cnt ;
    
    return cnt ;
}

int Activate_table_T::deactivate(int64_t id)
{
    SCOPE_LOG(slog);
    ////////////////////////////////////////////////////////////////////|01234567890123456789|//
    std::string query  = "UPDATE activate_code SET valid  = 'FALSE' WHERE  id = " + escape(id) ;
    DB_T::statement st(m_db);
    
    st.prepare(  query  ) ; 
    
    return st.affected_rows() ;
}

int Activate_table_T::kill_lives(const Activate_info_T& search ) 
{
    SCOPE_LOG(slog);
    std::string query = "SELECT id FROM activate_code WHERE " + make_where(search, true )  +  " LIMIT 10 ; " ;
    DB_T::statement st(m_db);
    
    st.prepare(query);
    if (st.rows_count() != 1 ) {
        slog.WarningLog("Not found!");
        return 0;
        
    }
    
    int64_t id = 0;
    st.row(0) >> id ;
     
    query = "UPDATE activate_code SET valid = lives > 1,  lives = lives - 1 WHERE id = " + escape( id ) ;
    
    st.prepare(query);
    return 0;
    
}
//==========================================================================================================================================

Error_Table_T::Error_Table_T( DB_T& db ) 
  : m_db( db )
{}

Error_Table_T::~Error_Table_T()
{}

static std::string make_where(const Error_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;

    if (search.id != 0 )
        result += " AND ( id = " + escape(search.id) + ") " ;

    if (search.value != 0)
        result += " AND ( value = " + escape(search.value) + " ) " ;
    
    return result;
}

Error_T Error_Table_T::list( const Error_info_T& search, const Sort_T& sort, Error_info_list_T& out )
{
    ///////////////////////////////////////////////////////////////
    std::string query = "SELECT id, value, ex_id, message_eng, message_rus, message_uzb FROM error_codes WHERE " + make_where(search) + sort.to_string()  ;
    //////////////////////////////////////////////////////////////
    DB_T::statement st(m_db);
    st.prepare(query);
    
    size_t rows = st.rows_count();
    out.list.resize(rows);
    
    for(size_t i = 0; i < rows; ++i)
    {
        Error_info_T& e = out.list[i];
        st.row(i) >> e.id >> e.value >> e.ex_id >> e.message_eng >> e.message_rus >> e.message_uzb ;
    }
    
    out.count = sort.total_count(rows);
    if (out.count < 0 ){
        st.prepare("SELECT COUNT(*) FROM error_codes WHERE " + make_where(search));
        st.row(0) >> out.count;
    }
    return Error_OK ;
}

int32_t Error_Table_T::add(const Error_info_T& info )
{
    //////////////////////////////////////////////////////////////////////
    char buf[2048];
    size_t sz;
    {//this take 0.1 us
        sz = snprintf(buf, sizeof(buf), "INSERT INTO error_codes (id, value, message_eng, message_rus, message_uzb, ex_id) "
                                        "VALUES( DEFAULT, %d, '%.512s', '%.512s', '%.512s', %d) RETURNING id; " ,
                  info.value, info.message_eng.c_str(), info.message_rus.c_str(), info.message_uzb.c_str(), info.ex_id );
    }
    /////////////////////////////////////////////////////////////////////
    std::string query;
    query.assign(buf, sz);
    DB_T::statement st(m_db);
    
    //this take 7-10 ms.
    st.prepare(  query );
    
    int32_t id = 0;
    st.row(0) >> id ;
    
    return id ;
}

Error_T Error_Table_T::edit(const Error_info_T& info)
{
    /////////////////////////////////////////////////////////////
    char buf[ 2048 ];
    size_t sz;
    {
        sz = snprintf(buf, sizeof(buf), "UPDATE error_codes SET message_eng =  '%.512s' , message_rus =  '%.512s' , message_uzb =  '%.512s' , ex_id = %d WHERE id = %u ; ",
                         info.message_eng .c_str(),  info.message_rus .c_str(),  info.message_uzb .c_str(), info.ex_id, info.id 
                );
    }
    std::string query ;
    query.assign(buf, sz);
    /////////////////////////////////////////////////////////////
    DB_T::statement st(m_db);
    st.prepare(  query );
    return Error_OK ;
}
Error_T Error_Table_T::info(uint32_t id, Error_info_T& info)
{
    ///////////////////////////////////////////////////////////////////////////
    std::string query = "SELECT id, value, message_eng, message_rus, message_uzb, ex_id FROM error_codes WHERE id = " + escape(id) ;
    
    DB_T::statement st(m_db);

    st.prepare(  query );

    if (st.rows_count() != 1)
        return Error_not_found ;
    
    st.row(0) >> info.id >> info.value >> info.message_eng >> info.message_rus >> info.message_uzb >> info.ex_id ;
    
    return Error_OK ;
}

Error_T Error_Table_T::info_by_value(int32_t value, Error_info_T& info)
{
    ///////////////////////////////////////////////////////////////////////////
    std::string query = "SELECT id, value, message_eng, message_rus, message_uzb, ex_id FROM error_codes WHERE value = " + escape(value) ;
    
    
    DB_T::statement st(m_db);

    st.prepare(  query );

    if (st.rows_count() != 1)
        return Error_not_found ;
    
    st.row(0) >> info.id >> info.value >> info.message_eng >> info.message_rus >> info.message_uzb >> info.ex_id ;
    
    return Error_OK ;
}
Error_T Error_Table_T::del(uint32_t id)
{
    /////////////////////////////////////////////////////////////////////////
    std::string query = "DELETE FROM error_codes WHERE id = " + escape(id); ;
    
    DB_T::statement st(m_db);
    
    st.prepare( query ) ;
    
    return Error_OK ;
}
     


/******************************************************************************************/
namespace
{
     struct async_notify_add_lambda
     {
         Notify_T n;
         
         explicit async_notify_add_lambda( Notify_T n): n( n ) {}
         
         void operator()(DB_T& db)const
         {
             Users_notify_T users_n(db);
             users_n.notification_add(n);
         }
     };
} 

/*********************************************************************************************/
/*********************************************************************************************/


/*********************************************************************************************/
Users_notify_T::Users_notify_T( DB_T& db )
    : m_db( db )
{}

Users_notify_T::~Users_notify_T()
{}


Error_T Users_notify_T::notification_list(uint64_t uid, Notify_list_T &nlist)
{
    SCOPE_LOG(slog);
    std::string query = " SELECT id, msg, add_ts::timestamp(0), \"type\" FROM notification WHERE send = 0 AND uid = " + escape(uid) ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();
    nlist.list.resize(rows);
    
    for( int i = 0; i < rows; i++ )
    {
        Notify_T &  info = nlist.list[i];
        st.row(i) >> info.id >> info.msg >> info.ts >> info.type ;
    }
    
    {
        nlist.count = rows;
    }
    return Error_OK;
}

Error_T Users_notify_T::notification_readed(uint64_t uid)const
{
    SCOPE_LOG(slog);
    //////////////////////////////////////////////////////////////////////////////////
    std::string query  = "UPDATE notification SET send = 1 WHERE uid = " + escape(uid) ;
    
    /////////////////////////////////////////////////////////////////////////////////
    DB_T::statement st(m_db);
    st.prepare( query ) ;
    return Error_OK ;
}

Error_T Users_notify_T::notification_add(const Notify_T & n)
{
    SCOPE_LOG(slog);
    std::string query = "INSERT INTO notification (uid, msg, \"type\", send) VALUES (" 
            + escape(n.uid) + ", " 
            + escape(n.msg) + ", " 
            + escape(n.type) + ", " 
            + escape(static_cast< long long > ( n.is_send) ) + ") " ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_notify_T::notification_send(const std::string& phone, std::string msg, Msg_type__T type)
{
    //@Note: optimize query - avoid memory allocation.
    std::string query = "SELECT id FROM users WHERE phone = " + escape(phone);
    DB_T::statement st(m_db);
    st.prepare(query);
    
    if (st.rows_count() != 1) return Error_not_found;
    
    int64_t uid = 0;
    st.row(0) >> uid;
    
    return this->notification_send( uid, msg, type);
}
Error_T Users_notify_T::notification_send(uint64_t uid,   std::string   msg, Msg_type__T type)
{
    return send2(uid, msg, type, 0 );
}

Error_T Users_notify_T::send2(uint64_t uid, std::string msg, Msg_type__T type,  int timeout_ms ) 
{
    SCOPE_LOG(slog);
    
    std::vector<Device_info_T> dev_list; 

    Users_device_T users_d(m_db);
    users_d.device_last_login(uid, dev_list);  

    if( dev_list.empty() )
        return Error_not_found;
    
    if ( 0 ==  timeout_ms )
        oson_push -> async_send_push(dev_list, msg, type);
    else
        oson_push -> async_timed_send_push(dev_list, msg, type, timeout_ms ); 
    
    bool const sended = true;
    
    //async add it.
    Notify_T notification(uid, msg, msg_type_str(type), sended ) ;
    
    Users_notify_T notify_table( m_db  ) ;
    notify_table.notification_add( notification );

    //@Note don't add bulk pushes to news
    if (uid >  0 ) 
    {
        News_info_T news_info ;
        news_info.uid  = uid  ;
        news_info.lang = 0    ; //all
        news_info.msg  = msg  ;
        
        News_T news_table{ m_db } ;
        news_table.news_add(news_info) ;
    }
    
    return Error_OK ;
}

/********************************************************************************************/
Users_device_T::Users_device_T(DB_T& db)
 : m_db(db)
{}

Users_device_T::~Users_device_T()
{}


Error_T Users_device_T::device_exist(const uint64_t uid, const std::string &dev_token, /*OUT*/ int64_t &dev_id)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT dev_id FROM user_devices WHERE uid = " + escape(uid) + " AND dev_token = " + escape(dev_token) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    dev_id  = 0;
    
    if (st.rows_count() == 1)
        st.row(0) >> dev_id;
    
    return Error_OK;
}

static std::string make_where( const Device_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.dev_id != 0)
        result += " AND ( dev_id = " + escape(search.dev_id) + ") " ;
    if (search.uid != 0)
        result += " AND (uid = " + escape(search.uid) + " ) " ;
    if ( !  search.password.empty() ) 
        result += " AND  ( password = crypt( '" + search.password + "' , password) ) " ;
    
    return result;
}

Error_T Users_device_T::device_list(const Device_info_T &search, std::vector<Device_info_T> &list)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT dev_id, uid, dev_token, add_ts, notify_token, os FROM user_devices WHERE " + make_where(search);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();
    list.resize(rows);
    for(int i = 0 ; i <rows; ++i)
    {
        Device_info_T& info = list[i];
        st.row(i) >> info.dev_id >> info.uid >> info.dev_token >>info.add_ts >> info.notify_token >> info.os ;
    }
    return Error_OK;
}

Error_T Users_device_T::device_last_login(uint64_t uid, std::vector<Device_info_T> &list)
{
    SCOPE_LOG(slog);
    const std::string query = 
    " SELECT dev_id, uid, dev_token, add_ts, notify_token, os, login_ts "
    " FROM user_devices"
    " WHERE (uid = " + escape(uid) + ") AND (os IS NOT NULL) ORDER BY login_ts DESC LIMIT 3 ; " ;
    
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int const rows = st.rows_count();
    list.resize( rows );

    // I love C++11, because
    // C++11:  for(auto& e: list) st.row(i) >> e.dev_id >> e.uid >> e.dev_token ...
    
    for(int i = 0 ; i < rows; ++i){
        Device_info_T& info = list[i];
        st.row(i) >> info.dev_id >> info.uid >> info.dev_token >> info.add_ts >> info.notify_token >> info.os >> info.login_ts ;
    }
    return Error_OK ;
}

Error_T Users_device_T::device_register(const Device_info_T& info, /*OUT*/int64_t& dev_id)
{
    SCOPE_LOG(slog);
    std::string query = 
    "INSERT INTO user_devices (dev_id, uid, dev_token, password, add_ts) VALUES "
    " ( DEFAULT, " + escape(info.uid) + ", " + escape(info.dev_token) + ", crypt( '" + info.password + "', gen_salt('bf') ), "
    " NOW()   ) RETURNING dev_id  " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> dev_id;
    
    return Error_OK ;
}



Error_T Users_device_T::update_login_ts(uint64_t uid, uint64_t dev_id)const
{
    SCOPE_LOG(slog);
    std::string  query = "UPDATE user_devices SET login_ts = now() WHERE uid = " + 
                    escape(uid) + " AND dev_id = "  + escape(dev_id) + " ; ";
 
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}


Error_T Users_device_T::device_change_password( uint64_t dev_id, const std::string& password)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE user_devices SET password = crypt( '" + password + "', gen_salt('bf') ) WHERE dev_id = " + escape(dev_id) ;
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}


/***************************************************************************************/
Users_bonus_T::Users_bonus_T(DB_T& db)
  : m_db( db )
{}

Users_bonus_T:: ~Users_bonus_T()
{}


Error_T Users_bonus_T::bonus_info(uint64_t uid, User_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    
    if ( uid == 0 )
    {
        slog.WarningLog("uid is zero");
        return Error_not_found;
    }
    
    std::string query = "SELECT id, uid, balance, earns, bonus_card_id, block, fio, pan, expire FROM user_bonus WHERE uid = " + escape(uid) ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if ( st.rows_count() != 1 )
    {
        slog.WarningLog("not found!");
        return Error_not_found;
    }
    
    st.row(0) >> info.id >> info.uid >> info.balance >> info.earns >> info.bonus_card_id >> info.block >> info.fio >> info.pan >> info.expire ;
    
    return Error_OK;
}

Error_T Users_bonus_T::reverse_balance(int64_t uid, int64_t amount)
{
    std::string query   = "UPDATE user_bonus SET balance = balance - " + escape(amount) + " WHERE uid = " + escape(uid) ;

    DB_T::statement st(m_db);
    st.prepare(  query );

    return Error_OK ;
}

Error_T Users_bonus_T::bonus_add(const User_bonus_info_T& info,  int64_t & bid)
{
    SCOPE_LOG(slog);
    std::string query = "INSERT INTO user_bonus (id, uid, balance, earns, bonus_card_id, block, fio, pan, expire) VALUES ( DEFAULT, " 
            + escape(info.uid)           + ", " 
            + escape(info.balance)       + ", "
            + escape(info.earns)         + ", "
            + escape(info.bonus_card_id) + ", "
            + escape(info.block)         + ", "
            + escape(info.fio)           + ", "
            + escape(info.pan)           + ", "
            + escape(info.expire)        + "  "
            "  ) RETURNING id " ;
    
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    st.prepare(query);
    st.row(0) >> bid;
    return Error_OK ;
}

Error_T Users_bonus_T::bonus_edit_block(uint64_t uid, uint32_t block)
{
    SCOPE_LOG(slog);
    
    std::string query = "UPDATE user_bonus SET "
            "block    = " + escape(block )  + "  "
            "WHERE uid = " + escape(uid);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_bonus_T::bonus_edit(uint64_t id, const User_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE user_bonus SET "
            "balance  = " + escape(info.balance) + ", "
            "earns    = " + escape(info.earns)   + ", "
            "block    = " + escape(info.block )  + "  "
            "WHERE id = " + escape(id);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

static std::string make_where(const User_bonus_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.uid != 0)
        result += " AND ( p.uid = " + escape(search.uid) + " ) " ;
    
    return result;
}
    
Error_T Users_bonus_T::bonus_info_list(const User_bonus_info_T& search, const Sort_T& sort, User_bonus_list_T& list)
{
    SCOPE_LOG(slog);
    
    std::string sort_s = sort.to_string();
    std::string where_s = make_where(search);
    std::string query = 
            " SELECT p.id, p.uid, p.balance, p.earns, p.bonus_card_id, p.block, p.fio, p.pan, p.expire, users.phone, users.name "
            " FROM user_bonus p LEFT JOIN users  ON (p.uid = users.id)  WHERE " + where_s + sort_s;
    //;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
     
    int rows = st.rows_count();
    
    list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        User_bonus_info_T& info = list.list[ i ];
        
        st.row( i ) >> info.id >> info.uid >> info.balance >> info.earns >> info.bonus_card_id >> info.block >> info.fio >> info.pan >> info.expire
                    >> info.phone >> info.name ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0)
    {
        list.count = total_cnt  ; 
        return Error_OK;
    } 
    
    query = "SELECT count(*) FROM user_bonus p WHERE " + where_s ;
    
    st.prepare(query);

    st.row(0) >> list.count;

    return Error_OK ;
}
    
    
 

/*************************************************************************************/
/************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/

User_online_info_T::User_online_info_T()
    : uid( 0 )
    , dev_id( 0 )
    , token ( )
{}
User_online_info_T::User_online_info_T(int64_t uid, int64_t dev_id, std::string token)
    : uid    ( uid    )
    , dev_id ( dev_id )
    , token  ( token  )
{}




Users_online_T::Users_online_T(DB_T& db)
: m_db(db)
{}


Error_T Users_online_T::info( const std::string& token, User_online_info_T& info)const
{
    SCOPE_LOG(slog);
    
    std::string  query = "SELECT uid, dev_id FROM users_online WHERE token = "  + escape(token);
    
    DB_T::statement st( m_db ) ;

    st.prepare( query  ) ;

    if ( st.rows_count() != 1 )
    {
        slog.WarningLog("Found many (or zero)!");
        return Error_not_found;
    }

    st.row(0) >>info.uid >> info.dev_id;
    
    return Error_OK ;    
}


Error_T Users_online_T::add(const User_online_info_T& info)const
{
    SCOPE_LOG(slog);
    
    std::string query = "INSERT INTO users_online ( uid, token, dev_id ) VALUES ( " + escape(info.uid) + ", " + escape(info.token) + ", " + escape(info.dev_id) + " ) " ;
   
    
    DB_T::statement st(m_db);
    st.prepare(query);

    /****@Note remove it some later*/
    if (info.uid == 894 /*VLAD*/){
        query = "UPDATE users_online SET last_online = NOW() + INTERVAL '1 hour' WHERE token = " + escape(info.token) + " AND uid = 894 " ; 
        st.prepare(query);
    }
    return Error_OK ;

}

//Error_T Users_online_T::update_online_time( const std::string & token)const
//{
//    SCOPE_LOG(slog);
//    std::string query = "UPDATE users_online SET last_online = NOW() WHERE token = " + escape(token);
//    
//    DB_T::statement st(m_db);
//    st.prepare(query);
//    
//    return Error_OK ;
//}

Error_T Users_online_T::del( const std::string& token)const
{
    SCOPE_LOG(slog);

    if( token.empty() ) {
        slog.WarningLog("Can't parse token");
        return Error_SRV_data_length;
    }
    
    DB_T::statement st(m_db);
    
    std::string query = "DELETE FROM users_online WHERE token = " + escape(token) ;
    
    st.prepare( query );
    
    return Error_OK ;
}

Error_T Users_online_T::del(int64_t uid, int64_t dev_id)
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    std::string query  = "DELETE FROM users_online WHERE uid = " + escape(uid) + " AND dev_id = " + escape(dev_id) ;
    st.prepare(query);
    return Error_OK ;
}
    

Error_T Users_online_T::check_online()const
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    st.prepare(  "DELETE FROM users_online WHERE last_online < now() - INTERVAL '15 minute' ; "  ) ;
    return Error_OK ;
}
/************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/
/**************************************************************************************/


 Users_cabinet_T::Users_cabinet_T(DB_T& db)
    : m_db( db )
 {}
 
 Users_cabinet_T::~Users_cabinet_T()
 {}
    
void Users_cabinet_T::add_client_password(uint64_t uid, const std::string& password)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE users_cabinet SET password = crypt ( '" + password  + "', gen_salt('md5') ) WHERE uid = " + escape( uid );
    DB_T::statement st(m_db);
    st.prepare( query );
}

bool Users_cabinet_T::check_client_password( uint64_t uid, const std::string& password )
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    std::string query = "SELECT 1 FROM users_cabinet WHERE uid = " + escape(uid) + " AND  password = crypt( '" + password + "', password ) " ;
    st.prepare(query ) ;

    return  st.rows_count() > 0;
}

static bool empty(const User_cabinet_info_T& search)
{
    return  search.id == 0 && search.uid == 0;
}
static std::string make_where(const User_cabinet_info_T& search)
{
    std::string result = " ( 1 = 1 ) ";

    if (search.id != 0) result += " AND (id = " + escape(search.id) + ") " ;

    if (search.uid != 0) result += " AND (uid = " + escape(search.uid) + " ) " ;

    if ( ! search.password.empty() ) result += " AND ( password = crypt( '" + search.password + "', password ) ) " ;

    return result;
}

Error_T Users_cabinet_T::edit_client_cabinet(const User_cabinet_info_T& search, const User_cabinet_info_T& info)
{
    SCOPE_LOG(slog);
    
    if (empty(search) || search.password.empty() )
        return Error_not_found;
    
    std::string query  =  "UPDATE users_cabinet SET uid = uid  ";  

    std::string where_s = make_where(search);

    if ( ! info.password.empty() )
    {
        query += ", password = crypt( '" + info.password + "', gen_salt('md5') )  " ;
    }

    if (info.checkpassword  != 0)
        query += ", checkpassword = " + escape(info.checkpassword)  + "  ";
    
    
    query += "WHERE " + where_s;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_cabinet_T::edit_last_password(int64_t id, const User_cabinet_info_T& info ) 
{
    SCOPE_LOG(slog);
     
    std::string query = "UPDATE users_cabinet SET last_passwd_check = " + 
                        escape(info.last_passwd_check) + 
                        ", check_count = " + escape(info.check_count) +
                        " WHERE id = " + escape(id ) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_cabinet_T::get_client_cabinet(const User_cabinet_info_T& search, User_cabinet_info_T& info)
{
    SCOPE_LOG(slog);
    
    if (empty(search))
        return Error_not_found;
    
    std::string query = "SELECT id, uid, checkpassword, last_passwd_check, check_count   FROM users_cabinet WHERE " + make_where(search);

    DB_T::statement st(m_db);
    st.prepare(query);
    
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> info.id >> info.uid >> info.checkpassword >> info.last_passwd_check >> info.check_count ;
    
    return Error_OK ;
}


/*****************************************************************************************/
/*****************************************************************************************/
/*****************************************************************************************/
/*****************************************************************************************/

Users_T::Users_T( DB_T & db ) : m_db(db) {
}

int Users_T::user_language(uint64_t uid)const // because there no change Users_T 
{
    //////////////////////////////////////////////////////////////////////////
    std::string query = " SELECT lang FROM users WHERE id = " + escape(uid) ;

    DB_T::statement st(m_db);

    st.prepare( query );

    if (st.rows_count() != 1)
    {    
        return LANG_all;
    }
        
    return st.get_int(0, 0);
    
}


Error_T Users_T::login_with_dev_token(const User_info_T& login_data, uint64_t& uid, uint64_t& dev_id )
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
   
    {
        std::string query = "SELECT id FROM users WHERE phone = " + escape(login_data.phone) ;
  
        st.prepare(query);
        
        if (st.rows_count() != 1)
            return Error_not_found ;
        
        st.row(0) >> uid ;
    }

    // get dev-id
    std::string query = 
            " SELECT dev_id FROM user_devices WHERE uid = " +escape(uid) + " AND dev_token = " + escape(login_data.dev_token) + " AND "
            " password = crypt( '" + login_data.password + "' , password) " ;

    st.prepare(query);

    if (st.rows_count() == 1){
        st.row(0) >> dev_id;
        return Error_OK ;
    }

   query = "SELECT dev_id FROM user_devices WHERE uid = " + escape(uid) 
           + " AND  password = crypt( '" + login_data.password +  "' , password) AND (login_ts < NOW() - INTERVAL '5 day' ) " ;
    
    st.prepare(query);
    
    if (st.rows_count() == 1){
        
        st.row(0) >> dev_id;

        query = "UPDATE user_devices SET dev_token = " + escape(login_data.dev_token) + " WHERE dev_id = " + escape(dev_id) ;
        st.prepare(query);
        
        return Error_OK ;
    }

    
    //not found
    return Error_login_failed;
}
Error_T Users_T::login_without_dev_token(const User_info_T& login_data, uint64_t& uid )
{
    SCOPE_LOG(slog);
    
    std::string query;

    query = "SELECT id  FROM users  WHERE  phone = "+escape(login_data.phone)+" AND  password = crypt("+escape(login_data.password)+",  password )   " ;

    DB_T::statement st(m_db);

   st.prepare(query);
   if (st.rows_count() != 1)
   {
       return Error_login_failed;
   }
   st.row(0) >> uid;

    
   return Error_OK ;

}



Error_T Users_T::add( User_info_T & data, const std::string &password)
{
	SCOPE_LOG(slog);

    std::string code, query;
    
    //;
    
    code = oson::utils::generate_password();

    if (data.dev_token.empty())
    {
        query = "INSERT INTO users (phone, password, qrstring) VALUES (" + escape(data.phone) + ", crypt( '" + password + "', gen_salt('bf') ), "
                " upper(encode(hmac('" + code + "', gen_salt('md5'), 'sha256'), 'hex') ) ) RETURNING id";
        DB_T::statement st(m_db);
        st.prepare(query);
        
        st.row(0) >> data.id;
        slog.InfoLog("uid: %ld", data.id);
        return Error_OK;
    }
    
    query = "INSERT INTO users (phone, qrstring) VALUES (" + escape(data.phone) + ", "
            " upper(encode(hmac('" + code + "', gen_salt('md5'), 'sha256'), 'hex') ) ) RETURNING id";
        
    
    DB_T::statement st(m_db);
    st.prepare(query);

    st.row(0) >> data.id ;
    
    slog.InfoLog("uid: %ld", data.id);
    //insert user_device_table also
    
    query = "INSERT INTO user_devices (uid, dev_token, password, add_ts) VALUES (" + escape(data.id) + ", " + escape(data.dev_token) + 
            ", crypt( '" + password + "', gen_salt('bf') ), NOW() ) " ;
        
    
    st.prepare(query);
    
    return Error_OK ;
}
    
    
User_info_T Users_T::info(const std::string & phone,  /*OUT*/ Error_T& ec )
{
	SCOPE_LOG(slog);
    User_info_T data;
    
    //@Note optimize it, avoid allocation memory
    std::string query = "SELECT id, phone, registration, sex, tr_limit, avatar, lang FROM users WHERE phone = " + escape(phone);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1){
        ec =  Error_not_found;
    }
    else{
        st.row(0) >> data.id >> data.phone >> data.registration >> data.sex >> data.tr_limit >> data.avatar >> data.lang ;
        ec = Error_OK ;
    }
	return data;
}

static std::string make_where(const User_info_T & search)
{
    std::string result = " ( 1 = 1 ) " ;
    
    if (search.id != 0)
       result += " AND ( id = " + escape(search.id) + "  ) " ; 
    
    if (search.phone.size() != 0)
       result += " AND ( phone = "+ escape(search.phone) + " ) " ;
    
    if (search.name.size() != 0)
       result += " AND ( name = " + escape(search.name) + " ) " ;
    
    if (search.token.length() > 0)
       result += " AND ( token = " + escape(search.token) + " ) " ;  
    
    if (search.qr_token.size() != 0)
        result += " AND ( qrstring = " + escape(search.qr_token) + ") " ;

    return result;
}

static bool empty_search(const User_info_T& search)
{
    return (search.id == 0) && ( search.phone.empty() ) && (search.name.empty() ) && (search.token.empty()) && (search.qr_token.empty());
}

Error_T Users_T::info(const User_info_T & search, User_info_T & data)
{
	SCOPE_LOG(slog);
    
    if (empty_search(search))
    {
        slog.ErrorLog("search is emtpy!");
        return Error_not_found;
    }
    
    std::string where_s = make_where(search);
	std::string query = 
            " SELECT id, phone, name, registration::timestamp(0), sex, notify_token, qrstring, tr_limit, email, avatar, lang, blocked " 
            " FROM users WHERE " + where_s;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
	if( st.rows_count() != 1 ) {
        slog.ErrorLog("Find more than 1 users or find nothing");
		return Error_not_found;
	}
    st.row(0) >> data.id       >> data.phone >> data.name   >> data.registration >> data.sex >> data.notify_token >> data.qr_token
              >> data.tr_limit >> data.email >> data.avatar >> data.lang         >> data.blocked ;
    
    return Error_OK;
}

User_info_T Users_T::get(uint64_t uid, Error_T& ec)
{
    SCOPE_LOG(slog);
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string query = " SELECT id, phone, name, registration, sex, notify_token, qrstring, tr_limit, email, avatar, lang, blocked "
                        " FROM users WHERE id = " + escape(uid) ;
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////
    User_info_T data;
    DB_T::statement st(m_db);
    st.prepare(  query , ec );
    if (ec) 
    {
         ;
    }
    else
    if (st.rows_count() != 1)
    {
        ec = Error_not_found;
    }
    else
    {
        ec = Error_OK ;
        st.row(0) >> data.id       >> data.phone >> data.name   >> data.registration >> data.sex >> data.notify_token >> data.qr_token
                  >> data.tr_limit >> data.email >> data.avatar >> data.lang         >> data.blocked ;
    }
    
    return data;
}

Error_T Users_T::change(uint64_t uid, const User_info_T & data)
{
	SCOPE_LOG(slog);
    std::string sex   =  ( data.sex  != SEX_none ) ? escape( data.sex  ) : " sex "   ;
    std::string lang  =  ( data.lang != 0        ) ? escape( data.lang ) : " lang "  ;
    
    std::string query  = 
            " UPDATE users SET "
            " name     = " + escape(data.name)     + ", " 
            " tr_limit = " + escape(data.tr_limit) + ", "
            " email    = " + escape(data.email)    + ", "
            " sex  = "     + sex                   + ", "
            " lang = "     + lang                  + "  "
            " WHERE id = " + escape(uid)
            ;
    
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK ;
}

Error_T Users_T::register_notify_token(const uint64_t uid, uint64_t dev_id,
                                       const std::string &notify_token, const std::string &os)
{
    SCOPE_LOG(slog);

    DB_T::statement st(m_db);

    if ( dev_id == 0 )
    {
        std::string query = "UPDATE users SET notify_token = " + escape(notify_token) + " WHERE id = " + escape(uid)  ;
        st.prepare(query) ;
    }
    else
    {
        std::string query = "UPDATE user_devices SET notify_token = " + escape(notify_token) + ", os = " + escape(os) + " WHERE dev_id = " + escape(dev_id);
        st.prepare( query ) ;
    }
    return Error_OK ;
}

Error_T Users_T::user_change_password(  uint64_t uid, const std::string &password)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE users SET password = crypt( '" + password + "', gen_salt('bf') ) WHERE id = " + escape(uid) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    return Error_OK ;
}
    

Error_T Users_T::del(uint64_t uid)
{
    SCOPE_LOG(slog);
    if(uid == 0) {
        slog.WarningLog("Empty field");
        return Error_login_empty;
    }
    std::string query = "DELETE FROM users WHERE id = " + escape(uid) ;
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_T::edit_block(uint64_t uid, uint32_t block_value)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE users SET blocked = " + escape(block_value) + " WHERE id = " + escape(uid) + "; ";
    //(query.c_str(), query.size());
    
    DB_T::statement st((m_db));
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_T::deposit(const uint64_t uid, uint64_t & amount)
{
	SCOPE_LOG(slog);
    std::string query = "SELECT deposit::numeric::bigint FROM bills WHERE uid = " + escape(uid) ;
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    
    if (st.rows_count() != 1) return Error_not_found;
    
    st.row(0) >> amount;
    
    return Error_OK ;
}

Error_T Users_T::create_deposit(const uint64_t uid)
{
	SCOPE_LOG(slog);
    std::string query = "INSERT INTO bills (uid) VALUES ( " + escape(uid) + ") " ;
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Users_T::set_avatar(uint64_t uid, const std::string & avatar)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE users SET avatar = " + escape(avatar) + " WHERE id = " + escape(uid);
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

static std::string make_where(const User_search_info_T& search){
    std::string result= "( 1 = 1 )";
    
    if ( search.id           != 0 ) result += " AND ( id = " + escape(search.id)    + ") " ;
    
    if ( ! search.phone.empty()  )
    { 
        std::string phone = search.phone;
        
        std::replace(phone.begin(), phone.end(), '*', '%') ; // postresql uses '%'  for *
        std::replace(phone.begin(), phone.end(), '?', '_') ; // postresql uses '_'  for ?
    

        if ( phone .length() == 12 && boost::all(phone, ::isdigit )) { // a full version
            result += " AND ( phone = " + escape( phone) + " ) " ; 
        }
        else if (phone.length() <= 12) 
        {
            if (phone[0] != '%')
                phone.insert(phone.begin(), '%');
            
            result += " AND ( phone LIKE  " + escape(phone)  + "  ) " ; 
        }
        else
        {
            result  += " AND ( 0 = 1 ) " ;
        }
    }
    
    if ( ! search.name.empty() ) 
    {
        std::string like_name = "%" + search.name + "%";
        result += " AND ( name LIKE   " + escape(like_name)   + " ) " ;
    }
    if ( ! search.register_from_date.empty() ) {
        std::string full_date = search.register_from_date + " 00:00:00" ;
        
        result += " AND ( registration >= " + escape(full_date ) + " ) " ;
    }
    
    if ( ! search.register_to_date.empty() ) {
        std::string full_date = search.register_to_date + " 23:59:59" ;
        result += " AND ( registration <= " + escape(full_date) + " ) " ;
    }
    
    
    return result;
}

Error_T Users_T::list(const User_search_info_T &search, const Sort_T &sort, User_list_T &u_list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = "SELECT id, phone, name, registration, sex, tr_limit FROM users WHERE " + where_s + sort_s ;
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query) ;
    
    int rows = st.rows_count();
    u_list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        User_info_T& info = u_list.list[i];
        st.row(i) >> info.id >> info.phone >> info.name >> info.registration >> info.sex >> info.tr_limit;
    }

    int const total_cnt = sort.total_count(rows);
    
    if( total_cnt >= 0 )
    {
        u_list.count  = total_cnt;  
        return Error_OK ;
    }
    query = "SELECT count(*) FROM users WHERE " + where_s;
    st.prepare(query);
    
    u_list.count = st.get_int(0, 0);
    
    return Error_OK;
}


 
static std::vector< std::string  > split_view( std::string  s)
{
    std::vector< std::string  > ret;
    size_t i = 0, prev_i;
    size_t const n = s.size();
    while( i < n){
        //skip; non digits
        while( i < n &&   ! ::isdigit(s[i]))
        {
            ++i;
        }

        prev_i = i;//save it

        while( i < n && ::isdigit(s[i]))
        {
            ++i;
        }

        // [prev_i ... i)  - is a digits sequence

        if (prev_i < i){
            std::string  view(s.c_str() + prev_i, i - prev_i );
            ret.push_back(view);
        }
        //there i >=  n  OR  s[i] is not digit.
    }
    
    return ret;
}

std::string Users_T::check_phone( const  std::string& all_phones)
{
    SCOPE_LOG(slog);
    std::string   registered_phones ;
    auto view = split_view(all_phones);
    
    if ( view.empty() ){
        return registered_phones ;
    }
    
    std::string query_header = "SELECT string_agg( phone::text, ',' ) FROM users WHERE phone IN (  "; 
    
    char main_comma = ' ';
    
    for(size_t part = 0; part < view.size(); part += 128 ) 
    {
        std::string query = query_header;
        //join it
        char  comma = ' '; // a space at first , instead of comma.
        for(size_t i = part; i < view.size() && i < part + 128; ++i)
        {
            std::string v = view[i];
                
            query += comma;

            query += '\''; // add quot
            query.append( v.c_str(), v.size() ) ;
            query += '\''; // add quot

            comma = ',';
        }
        
        query += " ) ; " ;

        DB_T::statement st( m_db );

        st.prepare(query);
        
        std::string ret;
        st.row(0) >> ret;
        if (ret.empty())
            continue;

        registered_phones += main_comma ;

        registered_phones += ret;
        
        main_comma = ',';
    }
    return registered_phones;
}

Error_T Users_T::generate_img(uint64_t uid, std::string& file_location)
{
    SCOPE_LOG(slog);
    enum{QR_SIZE = 8};

    Error_T ec = Error_OK ;
    //1. get user informations.
    User_info_T info = this->get(uid, ec);
    if (ec != Error_OK)
        return ec;

    //2. make qrcode from qr_token
    QRcode *qrcode;
    int version = 5;
    int casesensitive = 1;
    QRecLevel level = QR_ECLEVEL_H;
    QRencodeMode hint = QR_MODE_8;
    slog.DebugLog("Encode: %s", info.qr_token.c_str());

    qrcode = QRcode_encodeString(info.qr_token.c_str(), version, level, hint, casesensitive);
    if(qrcode == NULL) {
        slog.ErrorLog("Can't create qrcode");
        return Error_internal;
    }
    
    //scoped free.
    struct qrcode_free_t{ QRcode* qrcode; ~qrcode_free_t(){QRcode_free(qrcode);} } qrcode_free_e = {qrcode};
    
    //3. make PNG file from qrcode.
    static const std::string img_location = "/etc/oson/img/qr/";
    std::string location = img_location + num2string(uid) +  ".png";

    PNG_image_T qr_image;
    png_byte base_color[4] = {5, 87, 152, 255};

    ec = qr_image.fill_qr( qrcode->width, qrcode->data, QR_SIZE, base_color);
    if(ec != Error_OK) {
        printf("Failed to generate qr_omage");
        return ec;
    }

    // Set angle
    std::string qr_angle_file = img_location + "qr8_angle.png";
    PNG_image_T angle_image;
    //  Reader qr_angle_file to angle_image content.
    ec = angle_image.read_file(qr_angle_file);
    if(ec != Error_OK) {
        printf("Failed to read angle image");
        return ec;
    }

    ec = qr_image.set_qr_angle(angle_image, qrcode->width, QR_SIZE);
    if(ec != Error_OK) {
        printf("Failed to set angle for qr image");
        return ec;
    }

    // Set ico
    std::string ico_file = img_location + "oson_ico.png";
    PNG_image_T ico_image;
    ec = ico_image.read_file(ico_file);
    if(ec != Error_OK) {
        printf("Failed to read angle image");
        return ec;
    }
    ec = qr_image.add_top_image(ico_image, 0);
    if(ec != Error_OK) {
        printf("Failed to set angle for qr image");
        return ec;
    }

    ec = qr_image.write_file(location);
    if(ec != Error_OK) {
        printf("Failed to save png file \"%s\"", location.c_str());
        return ec;
    }

    //insert to qr_image table
    {
        DB_T::statement st(m_db);
        st.prepare("DELETE FROM qr_image WHERE uid = " + escape( uid ) )  ; // delete old qr_image iff exists, of course!
        
        // add a new qr_image
        std::string query = "INSERT INTO qr_image ( uid,  location ) VALUES ( " + escape(uid) + ", " + escape(location) + " ) " ;
        
        st.prepare(query);
    }
    
    file_location = location ;
    
    return Error_OK;
}

std::string Users_T::qr_code_location(uint64_t uid )
{
    SCOPE_LOG(slog);
    std::string file_location;
    
    DB_T::statement st(m_db);
    st.prepare( "SELECT location FROM qr_image WHERE uid = " + escape(uid) );
    
    if (st.rows_count() == 1) 
    {
        st.row( 0 ) >> file_location;
    }
    else{
        slog.WarningLog("Not found location!") ;
    }
    
    return file_location;
}

static std::string string_msg_ios_eoln_fix(   std::string  msg)
{
    ::boost::algorithm::replace_all(msg, "\n", "\\n");
    ::boost::algorithm::replace_all(msg, "\r", "\\r");
    return msg;
}

std::string oson::random_user_phone( )
{
    Users_T users_table( oson_this_db ) ;
    return  users_table.get_far_user_phone();
}

std::string Users_T::get_far_user_phone()
{
    SCOPE_LOG(slog);
    
    std::string phone;
    
    std::string query =  
            " SELECT phone, purch_ts, id FROM users "
            " WHERE ( (purch_ts IS NULL) OR (purch_ts < NOW() - interval '1 hour' ) ) AND "
            " NOT (id  < 200  )  ORDER BY purch_ts  LIMIT 1 " 
             ;
    
    DB_T::statement st(m_db);
    
    st.prepare( query );
    
    if ( st.rows_count() == 0 ) {
        return phone;
    }
    
    std::string ts;
    
    int64_t uid = 0;
    
    st.row( 0 ) >> phone >> ts >> uid;
    slog.DebugLog("phone: %s, ts: %s, uid: %lld", phone.c_str(), ts.c_str(), uid);
            
    if ( ts.empty() )
    {
        std::string query = "UPDATE users SET purch_ts = Now() - interval '16 hour' WHERE id = " + escape(uid) ;
        st.prepare(query);
    }
    else{
        std::string query = "UPDATE users SET purch_ts = greatest( purch_ts + interval '1 hour', Now() - interval '16 hour')  WHERE id = " + escape(uid) ;
        st.prepare(query);
    }
    return phone ;
}
/***********************************************************************************************************/

#include <boost/algorithm/string/predicate.hpp>
#include "http_request.h"
#include "exception.h"

static std::string make_xmpp_request( const std::string & login, const std::string& password)
{
    return login + "\n" + password + "\n" ;
}

oson::XMPP_manager::XMPP_manager( std::shared_ptr< boost::asio::io_service > const & io_service, const xmpp_network_info& xmpp)
: io_service_(io_service)
, xmpp_(xmpp)
{
    SCOPE_LOGD(slog);
}

oson::XMPP_manager::~XMPP_manager()
{ 
    SCOPE_LOGD(slog);
}
    
void oson::XMPP_manager::change_password( const std::string& login, const std::string& password )
{
    //@Note: FROM 2018-06-10 XMPP SERVER DOES NOT WORK, SO DISABLED IT. WILL ENABLED WHEN XMPP SERVER WORKS AND NEEDED XMPP.
#if 0
    SCOPE_LOGD(slog);
    io_service_->post(
            std::bind( &XMPP_manager::change_password_impl, this,
                            ::make_xmpp_request(login, password) 
                        )
            );
#endif 
    oson::ignore_unused( make_xmpp_request( "", "" ) );
}
    
namespace
{
    
class session_t: public std::enable_shared_from_this< session_t >
{
public:

    explicit session_t( const oson::io_service_ptr& ios )
    : ios_( ios ) 
    {
        SCOPE_LOGD(slog);
    }
    
    ~session_t()
    {
        SCOPE_LOGD(slog);
    }
    
    void async_start( xmpp_network_info const& xmpp, std::string const& request)
    {
       typedef oson::network::tcp::client tcp_client;
       typedef tcp_client::pointer pointer;
       
       pointer cl = std::make_shared< tcp_client >(ios_);
       
       cl->set_address(xmpp.address, xmpp.port);
       cl->set_response_handler(std::bind(&session_t::on_finish, shared_from_this(), std::placeholders::_1, std::placeholders::_2) ) ;
       cl->set_request(request);
       cl->async_start();

    }
private:
    void on_finish(const std::string& content, const boost::system::error_code& ec)
    {
        SCOPE_LOGD(slog);
        if (ec == boost::asio::error::operation_aborted ) // io_service stop
        {
            slog.WarningLog("operation aborted!");
        }
        slog.DebugLog("content: %.*s",  std::min<int>(2048, content.length()), content.c_str());
    }

private:
    oson::io_service_ptr ios_;
};  

} // end noname namespace
void oson::XMPP_manager::change_password_impl( const std::string& request )
{
    SCOPE_LOGD( slog );
    //@Note: session_t automatic deleted when finished!
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr p = std::make_shared< session_t > (io_service_);
    p->async_start(xmpp_, request ) ;
}
    
    
    
/***********************************************************************************************************/

    
oson::PUSH_manager::PUSH_manager( const oson::io_service_ptr & io_service, ios_notify_cert_info const& cert_info)
: io_service_(io_service)
, ios_ctx_( std::make_shared< boost::asio::ssl::context > (  boost::asio::ssl::context::sslv23 ) )
, and_ctx_( std::make_shared< boost::asio::ssl::context > (  boost::asio::ssl::context::sslv23 ) )
, cert_info_(cert_info)
{
    SCOPE_LOGD(slog);
    ////////////////////////////////////////////////////
    ios_ctx_->load_verify_file( cert_info.certificate );
    
    ios_ctx_->set_verify_mode( ios_ctx_->verify_none ); 
    ios_ctx_->use_certificate_file(cert_info.certificate, ios_ctx_->pem );
    ios_ctx_->use_private_key_file(cert_info.certificate, ios_ctx_->pem );
    
    ////////////////////////////////////
    and_ctx_->set_default_verify_paths();
}

oson::PUSH_manager::~PUSH_manager()
{
    SCOPE_LOGD(slog);
}



namespace
{

class push_session: public std::enable_shared_from_this< push_session >
{
public:
    push_session(  const oson::core::io_service_ptr & ios, const oson::core::ssl_ctx_ptr & ios_ctx , const oson::core::ssl_ctx_ptr&  and_ctx)
    : ios_( ios )
    , ios_ctx_( ios_ctx)
    , and_ctx_( and_ctx )
    , tm_( *ios )
    {
        
    }
    
    ~push_session()
    {
        
    }
    
    void timed_start( int timeout_ms,   ios_notify_cert_info cert_info, const std::vector< Device_info_T >& devices, std::string msg, Msg_type__T type ) 
    {
        tm_.expires_from_now(boost::posix_time::millisec(timeout_ms));
        tm_.async_wait(std::bind(&push_session::start, shared_from_this(), cert_info, devices, msg, type ) ) ;
    }
    
    void start(ios_notify_cert_info cert_info, const std::vector< Device_info_T >& devices, std::string msg, Msg_type__T type ) 
    {
        SCOPE_LOG(slog);
        
        std::vector< std::string> iosTokens;
        
        for (size_t i_ = 0, n_ = devices.size(); i_ != n_; ++ i_ )
        {
            const Device_info_T & dev = devices[ i_ ];

            if (dev.os == "ios") 
            {
                slog.DebugLog("dev(IOS): %lld", dev.dev_id);
                iosTokens.push_back( dev.notify_token);
                
            }
            else if (dev.os == "android") 
            {
                slog.DebugLog("dev(Android): %lld", dev.dev_id);

                Msg_android_content_T content(msg, type);
                
                //@Note: auto deleted when service finished.
                typedef oson::core::android_push_session android_st;
                typedef std::shared_ptr< android_st > android_ptr;
                
                android_ptr ap = std::make_shared< android_st >(  ios_, and_ctx_ ) ;
                
                ap->async_start(content, dev.notify_token, dev.dev_id);

            }
        } // end iteration

        //send all ios devices together
        if (iosTokens.size() > 0)
        {
            oson::core::PusherContent content;
            content.badge    = cert_info.badge;
            content.content  = string_msg_ios_eoln_fix(msg);
            content.sound    = "sound.caf";
            content.userData = std::string("\"type\":\"") + msg_type_str(type) + "\"";
       
            oson::core::PushInfo push_info;
            push_info.content   = content;
            push_info.tokens    = iosTokens;
            push_info.isSandBox = cert_info.isSandbox;
            
            typedef oson::core::ios_push_session ios_session;
            typedef std::shared_ptr< ios_session > ios_session_ptr;
            
            ios_session_ptr p  = std::make_shared< ios_session >( ios_, ios_ctx_);
            
            p->async_start(push_info);
        }
    }
    
private:
    oson::core::io_service_ptr ios_;
    oson::core::ssl_ctx_ptr    ios_ctx_, and_ctx_;
    boost::asio::deadline_timer tm_;
};


} // end noname namespace

void oson::PUSH_manager::async_send_push(const std::vector< Device_info_T> & devices, const std::string& msg, Msg_type__T type ) 
{
    SCOPE_LOGD(slog);
    
    if ( ! devices.empty() )
    {
        std::shared_ptr< push_session> s = std::make_shared< push_session >( io_service_, ios_ctx_, and_ctx_ );
        s->start(cert_info_, devices, msg, type );
        
    }
    else
    {
        slog.WarningLog("devices is empty.");
    }
}


void oson::PUSH_manager::async_timed_send_push(const std::vector<Device_info_T >& devices, const std::string& msg, Msg_type__T type, int milliseconds)
{
    SCOPE_LOGD(slog);
    if ( ! devices.empty() ) 
    {
        std::shared_ptr< push_session> s = std::make_shared< push_session >( io_service_, ios_ctx_, and_ctx_ );
        s->timed_start(milliseconds, cert_info_, devices, msg, type);
    }
    else
    {
        slog.WarningLog("devices is empty.");
    }
}

const oson::io_service_ptr& oson::PUSH_manager::io_service() const
{
    return io_service_;
}

/***********************************************************************************************************/

#if __cplusplus >= 201103L            //c++11
#define OSON_THREAD_LOCAL  thread_local
#else 
#define OSON_THREAD_LOCAL  __thread   //plain-old gcc keyword, from gcc 3.3
#endif 

static OSON_THREAD_LOCAL DB_T   this_db_d { DB_tag{} } ;

::DB_T& oson::this_db()
{
    DB_T& d = this_db_d;
    if ( ! d.isconnected()) {
        d.connect();
    }
    return d;
}

/***********************************************************************************************************/
 