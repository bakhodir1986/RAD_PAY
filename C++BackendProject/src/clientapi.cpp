#include <cstdio>
#include <ctime>
#include <numeric>
#include <functional>
#include <memory>

#include <boost/algorithm/string.hpp>

#include <boost/iterator/transform_iterator.hpp>
#include <boost/range/adaptor/transformed.hpp>
#include <boost/scope_exit.hpp>

#include <boost/asio/placeholders.hpp>
#include <set>
#include <fstream>

#include "clientapi.h"
#include "runtime_options.h"
#include "log.h"
#include "users.h"
#include "cards.h"
#include "transaction.h"
#include "Merchant_T.h"
#include "sms_sender.h"
#include "purchase.h"
#include "news.h"
#include "eocp_api.h"
#include "merchant_api.h"
#include "fault.h"
#include "periodic_bill.h"
#include "bills.h"
#include "bank.h"
#include "utils.h"
#include "DB_T.h"
#include "exception.h"
#include "application.h"
#include "topupmerchant.h"
#include "icons.h"


#define FUNC_COUNT 200

static const size_t       SRV_HEAD_LENGTH      = 32      ;
static const size_t       SRV_RESP_HEAD_LENGTH = 6       ;
static const char* const  SMS_MASTER_CODE      = "60498" ;

static const char* const  SMS_MASTER_CODE_00   = "46000" ;

static const int g_month_days[2][12] = 
{
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};
static int g_is_leap(int year){ return year % 4 ==  0  && ( year % 100 != 0  || year % 400 == 0 ) ; }

static inline bool allow_master_code(uint64_t uid)
{
    return (uid == 81 || uid == 84 || uid == 85 || uid == 86 || uid == 87 || uid == 89 || uid == 91 || uid == 154 || uid == 7 || uid == 17 || uid == 6404 ) ; // 6404 - demo 998000000000 user.
}

static bool allow_master_code( const std::string& phone)
{
    typedef const char* pstr;
    static const pstr phones[] = 
    {
        "998946968835",
        "998909730048",
        "998998380050",
        "998977030353",
        "998974480001",
        "998909052225",
        "998998280050",
        "998977755030",
        "998000000000"
    };
    
    for(size_t i = 0;  i < sizeof(phones) / sizeof(phones[0]); ++i ) 
    {
        if (phones[i] == phone ) return true;
    }
    
    return false;
}

static bool allow_master_code(const std::string& phone, const std::string & code)
{
    return (code == SMS_MASTER_CODE && allow_master_code(phone)) ||
            (code == SMS_MASTER_CODE_00 && phone == "998951946000" ) ;
}
//static std::time_t if_mod_since_2_time(std::string if_mod )
//{
//    std::time_t if_mod_time = 0;
//    
//    if ( ! if_mod.empty() ) {
//        //slog.DebugLog("if_mod: '%s'\n", if_mod.c_str());
//        std::time_t result = str_2_time(if_mod.c_str());
//        if (result != (time_t)-1)
//        {
//            if_mod_time = result;
//        } else {
//           // slog.WarningLog("Can't convert to time_t.");
//        }
//    }
//    
//    return if_mod_time;
//}

namespace {
struct api_data
{
    std::shared_ptr< boost::asio::io_service > m_io_service ;
    int64_t              m_uid          ;
    int64_t              m_dev_id       ;
    ByteStreamWriter     m_writer       ;
    rsp_handler_type     m_ssl_response ;
   
    explicit api_data( std::shared_ptr< boost::asio::io_service > io_service )
    : m_io_service(io_service), m_uid(), m_dev_id(),   m_writer(), m_ssl_response()/*, m_active(false)*/
    {}
    
    /****************************************/
    void send_result(Error_T ec);
    void send_result_i(Error_T ec);
    /****************************************/
};


typedef std::shared_ptr< api_data > api_pointer;

typedef const api_pointer&  api_pointer_cref;
typedef api_pointer&        api_pointer_ref;

} // end noname namespace

static std::string gen_token_and_add_users_online(api_pointer_cref d ) ;
/***********************************************************************************/
static Error_T dispatch_command(int cmd, api_pointer_cref d, ByteReader_T& reader);
static void exec_impl(api_pointer_cref d, const byte_array& data);

void show_header( const uint8_t * data, size_t length );
void show_data( const uint8_t * data, size_t length );

/***********************************************************************************/
ClientApi_T::ClientApi_T( std::shared_ptr< boost::asio::io_service > io_service )
: d_ptr( new api_data(io_service ) )
{
    //SCOPE_LOGD( slog );
}

ClientApi_T::~ClientApi_T() 
{
    //SCOPE_LOGD( slog );
    api_data* d = static_cast< api_data* > ( d_ptr ) ;
    delete d;
}

void ClientApi_T::exec(  const byte_array&   data,    rsp_handler_type   response_handler )
{
    api_data* d = static_cast< api_data* >(d_ptr);
    d->m_ssl_response.swap( response_handler ) ;
    
    api_pointer p = api_pointer(shared_from_this(), d);
    
    return exec_impl(p, data);
}

int64_t ClientApi_T::uid()const 
{
    api_data* d = static_cast< api_data* >(d_ptr);
    return d->m_uid ;
}
/************************************************************************/

static void exec_impl(api_pointer_cref d, const std::vector<uint8_t>& data)
{
    SCOPE_LOGD(slog);
    //slog.DebugLog("data.ptr: %p", data.data());
    /////////////////////////////////////////////////////////////////////////////////
    d->m_writer.clear();
    d->m_writer << b2( 0 ) << b4( 0 ); // 2 bytes for error-code, 4-bytes for length of data.
    
    Error_T ec = Error_OK ;
    ////////////////////////////////////////////////////////////////////////// 
    Server_head_T head = parse_header(data.data(), data.size());

    if( head.version != 1 ){
         return d->send_result( Error_SRV_version ) ;
    }
    
    show_header(data.data(), SRV_HEAD_LENGTH);
    show_data(data.data() + SRV_HEAD_LENGTH, head.data_size);
    
    try
    {
        ByteReader_T reader(data.data() + SRV_HEAD_LENGTH, head.data_size);
        ec =  dispatch_command(head.cmd_id, d, reader ) ; 
    }
    catch(oson::exception& e){
        slog.ErrorLog("oson exception: msg = '%s', code = %d", e.what(), e.error_code());
        ec =  (Error_T)e.error_code();
    }
    catch(std::exception & e){
        slog.ErrorLog("standard exception: msg = '%s'", e.what());
        ec= Error_internal;
    }
    return d->send_result(ec);
    ///////////////////////////////////////////////////////////////
}

void api_data::send_result(Error_T ec)
{
    if (ec == Error_async_processing)
    {
        return ;
    }
    
    if (ec > Error_internal)
        ec = Error_internal;
    
    return send_result_i( ec );
}

void api_data::send_result_i(Error_T ec)
{
    SCOPE_LOGD(slog);
    
    byte_array& buffer = m_writer.get_buf();
    
    if (buffer.size() < SRV_RESP_HEAD_LENGTH) //check enough size.
        buffer.resize(SRV_RESP_HEAD_LENGTH) ;
    
    size_t buffer_length = buffer.size() - SRV_RESP_HEAD_LENGTH;
    
    ByteWriter_T writer(buffer);
   
    writer.writeByte2(ec);
    writer.writeByte4(buffer_length);
    
    show_header( buffer.data(), SRV_RESP_HEAD_LENGTH );
    show_data( buffer.data() + SRV_RESP_HEAD_LENGTH, buffer_length   );
    //////////////////////////////////////////////////////////////
    if ( static_cast< bool > ( m_ssl_response)   ) 
    {
        rsp_handler_type tmp_resp;
        tmp_resp.swap(m_ssl_response) ; // noexcept
        
        //slog.DebugLog("buffer.ptr: %p", buffer.data());
        //on this point m_ssl_response already is empty!
        tmp_resp( std::move( buffer )  );

    } else {
        slog.WarningLog("ssl_response is null");
    }
    
}

/**********************************************************************/
/**************=========  UTILS INNER FUNCTIONS ========************* */
static Error_T api_bonus_earns(api_pointer_cref d, Transaction_info_T tr_info )    ;
/*******************************************************************************************/
/**=======================  INNER IMPLEMENTATIONS ========================================**/
/*******************************************************************************************/

#define OSON_PP_USER_LOGIN(d, reader)                                        \
do{                                                                          \
    std::string token = reader.readAsString(reader.readByte2() );  \
    if (Error_T ec = api_user_login(d, token ) )                             \
        return ec;                                                           \
} while((void)0, 0)                                                          \
/****/

#define OSON_PP_USER_LOGIN_NOT_EMPTY(d, reader)                              \
do{                                                                          \
    std::string token = reader.readAsString(reader.readByte2() );  \
    if ( ! token.empty() )                                                   \
    if (Error_T ec = api_user_login(d, token ) )                             \
        return ec;                                                           \
} while((void)0, 0)                                                          \
/****/

static Error_T api_null(api_pointer_cref d, ByteReader_T& reader){
    SCOPE_LOGD(slog);
    return Error_SRV_unknown_cmd;
}

static Error_T api_logging_debug(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    size_t len  = ::std::min< size_t >( 4096, reader.readByte2() ) ;
    const std::string s = reader.readAsString( len  )  ; 
    slog.DebugLog("\n'%s'\n", s.c_str());
    return Error_OK;
}
/**************************************************************************************************************/
static Error_T api_user_login(api_pointer_cref d, const std::string&   token)
{
    SCOPE_LOG(slog);
    
    static const std::string  PUBLIC_TOKEN =   "Ft5LtRhD76_oson_8IKFyLgcSj"  ;
    
    if ( token == PUBLIC_TOKEN )
    {
        slog.DebugLog("public token");
        d->m_uid    = -1 ;
        d->m_dev_id = -1 ;
        return Error_OK ;
    }
    
    if ( token.length() > PUBLIC_TOKEN.length() &&  
         boost::algorithm::starts_with( token, PUBLIC_TOKEN  ) && 
         token[ PUBLIC_TOKEN.length() ] == '_' ) 
    {
        int32_t timeout_s = string2num( token.substr( 1 + PUBLIC_TOKEN.length() ) ) ;
        // timeout must be within [0..300]
        timeout_s = oson::utils::clamp< int32_t > ( timeout_s, 0, 300 ) ;

        sleep(timeout_s);
        return Error_login_failed;
    }
    
    if( token.empty() ) {
        slog.WarningLog("Can't parse token");
        return Error_login_failed;
    }
    Users_online_T user( oson_this_db  );
    User_online_info_T info;
    Error_T  ec = user.info(token, info);
    
    if ( Error_OK == ec  ){
        d->m_uid    = info.uid;
        d->m_dev_id = info.dev_id;
        slog.DebugLog("uid: %lld, dev_id: %lld", (long long)(d->m_uid), (long long)(d->m_dev_id));
        
        //update online ts
        //user.update_online_time( token );
    } else {
        slog.WarningLog("Can't login!");
        ec = Error_login_failed;
    }
    return ec;
}

static ByteStreamWriter& operator << (ByteStreamWriter& bw, const Error_info_T& e)
{
    return bw << b4(e.id) << b4(e.value) << b4(e.ex_id) << e.message_eng << e.message_rus << e.message_uzb ;
}

static Error_T api_user_error_codes(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    uint32_t id     = reader.readByte4();
    uint32_t value  = reader.readByte4();
    ///////////////////////////////////////
    Users_T users( oson_this_db  ) ;
    int lang =  users.user_language( d->m_uid );
    //////////////////////////////////////
    Error_info_T info;
    Error_Table_T table( oson_this_db  );
    
    if (id != 0 )
        table.info(id, info);
    else 
        table.info_by_value(value, info);
    //////////////////////////////////////
    std::string msg;
    switch(lang)
    {
        case LANG_rus: msg = info.message_rus; break;
        case LANG_uzb: msg = info.message_uzb; break;
        case LANG_all: msg = info.message_eng; break;
        default: msg  = info.message_eng;
    }
    d->m_writer << msg << b8( 0 )  << info;
    
    return Error_OK ;
}

static Error_T api_user_error_codes_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    int32_t id = reader.readByte4();
    int32_t val = reader.readByte4();
    //////////////////////////////////////
    if (id > 0 || val > 0) // single element
    {
        Error_info_T info;
        Error_Table_T table( oson_this_db  );
        Error_T ec = id > 0 ? table.info(id, info) : table.info_by_value(val, info);
        if (ec) return ec;
        //total-cnt = 1, list-cnt=1
        d->m_writer << b4(1)  << b4(1) << b4(info.id) << b4(info.value) << b4(info.ex_id) << info.message_eng << info.message_rus << info.message_uzb ;
    } else {  // all
        Error_Table_T table( oson_this_db  );
        Error_info_list_T list;
        Error_info_T search; // no search
        Sort_T sort; // no sort
        Error_T ec = table.list(search, sort, list);
        if (ec) return ec;
        
        d->m_writer << b4(list.count) << b4(list.list.size()) ;
        for(const Error_info_T& e: list.list){
            d->m_writer << b4(e.id) << b4(e.value) << b4(e.ex_id) << e.message_eng << e.message_rus << e.message_uzb ;
        }
    }
    return Error_OK ;
}

static Error_T api_client_auth_token(api_pointer_cref d, ByteReader_T & reader)
{
    SCOPE_LOGD( slog );
    OSON_PP_USER_LOGIN(d, reader) ;
    
    d->m_writer << b8(d->m_uid) ;
    return Error_OK;
}

static Error_T api_client_auth(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    Error_T ec = Error_OK ;
    User_info_T user_info;
    reader >> r2(user_info.phone) >> r2(user_info.password) >> r2(user_info.dev_token)>> r2(user_info.platform)>> r2(user_info.app_version) ;
    ///////////////////////////////////////////////////////////////////////////////////
    if ( ! valid_phone(user_info.phone ) ) {
        slog.ErrorLog("phone is not valid: %s", user_info.phone.c_str());
        return Error_login_empty;
    }
    if (user_info.password.empty()  ){
        slog.ErrorLog("password  is empty!");
        return Error_parameters;
    }
    
    uint64_t uid = 0, dev_id = 0 ;
    
    std::string token   = oson::utils::generate_token();

    DB_T::transaction db_tran( oson_this_db  );
    
    Users_T users( oson_this_db  ) ;
    if (user_info.dev_token.empty())
    {
        ec =  users.login_without_dev_token(user_info, /*OUT*/ uid);
        if (ec != Error_OK ) return Error_user_not_found; // exit if failed.
    } else {
        ec =  users.login_with_dev_token(user_info,  /*OUT*/uid, dev_id);
        if (ec != Error_OK ){
            slog.WarningLog("Please_register_phone_against_we_long_not_in_OSON !");
            if (uid != 0 ) {
                return Error_device_token_not_found;
            } else {
                return Error_user_not_found;
            }
        }
    }
    
    /************************************/
    User_info_T  u_user_info =  users.get( uid, ec );
    if( ec )  return Error_user_not_found;
     
    if (u_user_info.blocked) {
        slog.WarningLog("Blocked user try login");
        return Error_blocked_user;
    }
    
    d->m_uid    =  uid;
    d->m_dev_id = dev_id;
    
       //add token to users_online
    User_online_info_T online_info(uid, dev_id, token);
    Users_online_T users_o( oson_this_db  );
    users_o.del(uid, dev_id);
    users_o.add(online_info);
    /*********************************/
 
    db_tran.commit();
    
    //update device ts.
    if (0 != d->m_dev_id){
        Users_device_T user_d( oson_this_db ) ;
        user_d.update_login_ts(d->m_uid, d->m_dev_id);
    }
    
    //Check Version
    
    oson::App_info_table_T table( oson_this_db ) ;
    
    oson::App_info_T info  = table.get_last( user_info.platform );
    app_version user_version(user_info.app_version);
    app_version info_version(info.version);
    app_version min_version(info.min_version);
    
    
    int32_t is_update_required = info.id > 0 && info_version >= user_version && user_version >= min_version;
    
    int32_t is_expired = info.id == 0 || (info_version > user_version ) ;
    
    
    //async request sends
    oson_xmpp ->change_password(user_info.phone, token);
    /////////////////////////////////////////
    d->m_writer << token <<   b4(is_update_required ) << b4( is_expired ) ;
    //////////////////////////////////////
     
    return Error_OK;
}

static Error_T api_client_logout(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    std::string  token = reader.readAsString( reader.readByte2() ) ;
    Users_online_T user_o( oson_this_db ) ;
    user_o.del( token );
    return Error_OK ;
}

static Error_T api_client_register(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    ////////////////////////////////////////////////////////
    const std::string phone = reader.readAsString(reader.readByte2());
    const std::string dev_id = reader.readAsString(reader.readByte2());
    //////////////////////////////////////////////////////////////////
    if (phone.empty() || !valid_phone( phone ))
    {
        slog.WarningLog("phone is invalid!");
        return Error_login_empty;
    }
    Users_T users(  oson_this_db  ) ;
    Error_T ec = Error_OK ;
    User_info_T user_info =  users.info(phone, /*OUT*/ ec ); 
    
    if (ec == Error_OK && user_info.blocked) {
        slog.WarningLog("Register blocked user");
        return Error_login_failed;
    }

    ////////////////////////////////////////////
    Activate_table_T act_table( oson_this_db ) ;
    Activate_info_T act_srch;
    act_srch.dev_id = dev_id;
    act_srch.kind   = Activate_info_T::Kind_user_register ;
    act_srch.phone  = phone;
    act_srch.add_ts =  formatted_time("%Y-%m-%d %H:%M:%S", std::time(0) - 60 ) ;// 60 seconds before
    
    
    int cnt = act_table.count(act_srch);
    
    if ( cnt > 0 )// so there already has sms
    {
        slog.WarningLog("There already has SMS!");
        return Error_OK ;
    }
    ///////////////////////////////////////////////////////
    
    std::string code = oson::utils::generate_code(CHECK_CODE_LENGTH);
    Activate_info_T act_i  ;
    act_i.code    = code   ;
    act_i.dev_id  = dev_id ;
    act_i.kind    = act_i.Kind_user_register ;
    act_i.phone   = phone  ;
    act_i.add_ts  = formatted_time_now_iso_S();
    
    act_table.add( act_i );
    
    SMS_info_T sms_info(phone, "www.oson.uz: код подтверждения регистрации " + code , SMS_info_T::type_client_register_code_sms  ) ;
    
    oson_sms -> async_send( sms_info ) ;
    
    return Error_OK ;
}

static Error_T api_client_checkcode(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    /////////////////////////////////////////////////////////////////////
    const std::string phone = reader.readAsString(reader.readByte2());
    const std::string code  = reader.readAsString(reader.readByte2());
    const std::string dev_token = reader.readAsString(reader.readByte2());
    //////////////////////////////////////////////////////////////////////////
    
    slog.InfoLog("phone: %s, code: %s, dev-token: %s", phone.c_str(), code.c_str(), dev_token.c_str()) ;
    
    if (code.empty()){
        slog.ErrorLog("code is empty!");
        return Error_parameters;
    }
    
    Error_T ec = Error_OK ;
    
    if ( dev_token.empty() ){
        slog.WarningLog("warning_dev_token_empty !");
    }
    DB_T::transaction transaction( ( oson_this_db  ) );
    
    Activate_info_T act_s, act_i;
    act_s.phone  = phone                    ;
    act_s.dev_id = dev_token                ;
    act_s.code   = code                     ;
    act_s.kind   = act_s.Kind_user_register ;
    
    Activate_table_T act_table(  oson_this_db   );
    act_i = act_table.info( act_s );

    int64_t const code_id = act_i.id ;
    
    if (!code_id || act_i.code != code ) {
        
        bool const allow_code = allow_master_code(phone, code ) ;
        
         if ( ! allow_code ){
                slog.DebugLog("Wrong check code");
                return Error_checkcode_failed;
         }
    }
    
    
    //======== @Note: codes will delete after 8 hours  =================
    if (code_id) // if exists code_id.
        act_table.deactivate( code_id );

    Users_T users( oson_this_db  ) ;
    User_info_T user_info = users.info(phone, ec);
    //======= @Note  User_T::info  return only Error_OK or Error_not_found !
    
    Users_device_T users_d( oson_this_db  );
    
    
    const std::string password = oson::utils::generate_password();

    // If user not found
    if (user_info.id == 0 ) {
        user_info.phone    = phone;
        user_info.dev_token   = dev_token;
        
        //========== @Note: User_T::add always return Error_OK
        users.add(user_info, password);
    } else if ( dev_token.empty() ) {
        users.user_change_password(user_info.id, password);
    } else { // dev_token is not empty
        int64_t dev_id  = 0;
        
        
        users_d.device_exist(/*IN*/user_info.id, /*IN*/dev_token, /*OUT*/dev_id);
        
        if(!dev_id) {
            Device_info_T info;
            info.uid        = user_info.id ;
            info.dev_token  = dev_token;
            info.password   = password;
            //======= @Note device_register always return Error_OK 
            users_d.device_register(info, /*OUT*/ dev_id);  
        }
        else 
        {  // update device password because token is not empty
            users_d.device_change_password(dev_id, password);
        }
    }
    
    const std::string token = oson::utils::generate_token();

    user_info.phone     = phone;
    user_info.password  = password;
    user_info.dev_token = dev_token;
    uint64_t uid = 0, dev_id = 0;
    
    if ( ! user_info.dev_token.empty() )
        ec = users.login_with_dev_token(user_info, /*OUT*/ uid, dev_id );
    else
        ec = users.login_without_dev_token(user_info, /*OUT*/ uid );
    
    if (ec != Error_OK) {
        return ec;
    }
    
    d->m_uid    = uid    ;
    d->m_dev_id = dev_id ;
    /********************************************/
    User_online_info_T online_info;
    online_info.dev_id =  dev_id;
    online_info.uid    =  uid;
    online_info.token  = token;
    
    Users_online_T users_o(  oson_this_db   );
    
    users_o.add(online_info);
    /********************************************/
    transaction.commit();
    
    oson_xmpp -> change_password(phone, token);
    ////////////////////////////////////////////////////////////////////
    d->m_writer << token << password ;
    ///////////////////////////////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_client_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    Error_T ec = Error_OK ;
    Users_T users(  oson_this_db   ) ;
    User_info_T user_info = users.get( d->m_uid, ec);
    if (ec != Error_OK)return ec;
    ///////////////////////////////////////////////
    d->m_writer << b2(user_info.sex ) << b2(user_info.lang) 
                << user_info.phone    << user_info.name << user_info.registration 
                << user_info.email    << user_info.avatar;
    /////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_client_change_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    ////////////////////////////////////////////////////////////////////////
    User_info_T user_info;
    reader >> r2(user_info.sex) >> r2(user_info.lang) >> r2(user_info.name) >> r2(user_info.email) ;
    //////////////////////////////////////////////////////////////////////////
    Users_T users(  oson_this_db   ) ;
    return users.change(d->m_uid, user_info);
}

static Error_T api_client_notify_register(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    OSON_PP_USER_LOGIN(d, reader);
    /***********************************************************************************/
    const std::string notify_token   = reader.readAsString( reader.readByte2() );
    const std::string os             = reader.readAsString( reader.readByte2() );
    /***********************************************************************************/
    Users_T users(  oson_this_db   ) ;
    return  users.register_notify_token( d->m_uid, d->m_dev_id, notify_token, os ) ;
}

static Error_T api_client_balance(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    d->m_writer << b8(0); 
    return Error_OK;
}

static Error_T api_client_contacts_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog ); 
    Users_T users( oson_this_db  );
    d->m_writer <<  users.check_phone( reader.readAsString( reader.readByte2( )) ) ; //Это ради забава :) 
    return Error_OK;
}

static Error_T api_client_avatar(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    ///////////////////////////////////////////////////////////////////
    std::string phone,uid;  
    int32_t link = 0;
    reader >> r2( phone ) >> r2(uid) >> r4(link);
    
    if (phone.length() > 1024 || uid.length() > 1024 ) {
        return Error_parameters;
    }
    slog.InfoLog("phone: %s   uid = %s  link = %d ", phone.c_str(), uid.c_str(), (int)link ) ;
    ///////////////////////////////////////////////////////////////////
    if (link == 0 ) 
    {
        Error_T ec = Error_OK ;
        Users_T users(  oson_this_db   );
        User_info_T info = users.info(phone, ec);
        /////////////////////////////////////////////
        d->m_writer << info.avatar ;
        ////////////////////////////////////////////
        return ec ;
    } // old API
    
    
    //a new API
    {
        size_t number_phones = !phone.empty();
        for(char& c : phone){
            bool valid = ::isdigit(c) || (c == ',' || c == ';' ) ;
            if (!valid){
                return Error_parameters;
            }
            if (c == ';' ){
                c = ',';
            
            }
            if ( c == ',' ) 
            {
                ++number_phones;
            }
        }
        
        size_t number_uids = !uid.empty() ;
        size_t len_uid = 0;
        for(char & c : uid ) {
            bool valid = ::isdigit(c) || ( c == ',' || c == ';' ) ;
            if ( ! valid ) {
                return Error_parameters;
            }
            
            if(c == ';' ) {
                c = ',';
            }
            
            if (c == ',') { 
                ++number_uids ;
                if (len_uid > 18){
                    return Error_parameters;
                }
                len_uid = 0;
            } else {
                //a digit
                ++len_uid;
            }
        }
        
        std::string phone_esc ;
        phone_esc.reserve (phone.size() + number_phones*2) ;
        
        for(size_t i = 0, n = phone.size(); i != n; ++i)
        {
            if ( (i == 0 || phone[i-1] == ',' )   ) // a start or end
                phone_esc += '\'';
            
            phone_esc += phone[i];
            
            if ( (i == n-1 || phone[i+1] == ',' ) )
                phone_esc += '\'';
        }
        
         
        
        std::string query = "SELECT id, phone, avatar FROM users WHERE (1=1)" ;
        
        if ( ! phone.empty() ) 
        {
            if (number_phones == 1 ) 
                query += "AND ( phone = " + phone_esc + " ) " ;
            else 
                query += "AND ( phone IN ( " + phone_esc + " ) ) " ;
        }
        
        if ( ! uid.empty() ) 
        { 
            if (number_uids == 1 )  
                query += " OR ( id = " + uid + " ) " ;
            else
                query += " OR ( id IN ( " + uid + " ) ) " ;
        }
        
        DB_T::statement st( oson_this_db ) ;
        
        st.prepare( query ) ;
        int rows = st.rows_count();
        
        std::vector< User_info_T > info_list;
        for(int i = 0; i < rows; ++i){
            User_info_T info;
            st.row(i) >> info.id >> info.phone >> info.avatar ;
            
            info_list.push_back(info);
        }
        
        d->m_writer << b4(info_list.size()) ;
        
        for(User_info_T const& info : info_list )
        {
            d->m_writer << b8(info.id ) << info.phone << info.avatar ;
        }
        return Error_OK ;
    }
}
/****************************************************************************************************/
static ByteStreamWriter&  operator << (ByteStreamWriter& bw, const Card_info_T& card)
{
   return bw   << b8(card.id)         << b8(card.deposit)      << b4(card.tpl)  << b2(card.is_primary)
               << b2(card.user_block) << b2(card.foreign_card) << card.pan      << card.expire
               << card.name           << card.owner            << b4( card.isbonus_card ) ;
}

static void update_card_deposit( const EOCP_Card_list_T& infos,  /*IN-OUT*/Card_info_T& card  )
{
 //   SCOPE_LOG(slog);
    if (card.isbonus_card )
        return ;
    
    if (card.foreign_card != FOREIGN_NO )
        return ;
    
    oson::backend::eopc::resp::card eocp_card = infos.get(card.pc_token);
    
    if (eocp_card.empty() ) // not found
        return ;
    
    
    if (eocp_card.phone != card.owner_phone )
    {
       SCOPE_LOGD(slog);
       slog.WarningLog("owner phone changed!");

       DB_T::statement st{ oson_this_db } ;
       Error_T ec = Error_OK ;
       std::string query =  "UPDATE cards SET foreign_card = " + escape( FOREIGN_YES ) + " WHERE card_id = " + escape( card.id ) ;
       st.prepare( query, ec ) ;
       card.foreign_card = FOREIGN_YES ;
       return ;
    }
    
    card.deposit = eocp_card.balance;

    int const card_eocp_status = eocp_card.status;

    //slog.DebugLog("deposit: %lld, status: %d", (long long)(card.deposit),  card_eocp_status);

    if ( card_eocp_status != VALID_CARD ){
       card.deposit     = 100; //1 sum.
       card.user_block  = 1;
    }
    
    
}

namespace 
{
    
class card_list_session: public std::enable_shared_from_this<card_list_session>
{
public:
    typedef card_list_session self_type;
    typedef std::shared_ptr< self_type > pointer;
private:
    api_pointer d;
    //////////////////////////////////
    std::vector< Card_info_T > list;
    
public:
    explicit card_list_session( api_pointer d);

    ~card_list_session();

    void async_start();
private:
    void start() ;
    void on_card_info_eopc(const std::vector<std::string> &ids, const EOCP_Card_list_T& card_map, Error_T ec)  ;
    void on_card_info( const EOCP_Card_list_T& eocp_card_list, Error_T ec) ;    
};
} // end noname namespace

card_list_session::card_list_session( api_pointer d)
:  d( d )
{
    SCOPE_LOGD(slog);
}

card_list_session::~card_list_session()
{
    SCOPE_LOGD(slog);
    if (static_cast< bool >(d->m_ssl_response) ) {
        slog.WarningLog("~card_list_session something is wrong go!");
        d->send_result(Error_internal);
    }
}

void card_list_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this() ) ) ;
}


void card_list_session::start()
{
    SCOPE_LOGD(slog);

    Cards_T cards(  oson_this_db   );

    list = cards.card_list( d->m_uid );

    std::vector< std::string > ids;
    ids.reserve(list.size());
    for(size_t i = 0; i < list.size(); ++i)
        ids.push_back(list[i].pc_token) ;

        //add bonus card
    Card_info_T cbinfo;
    if ( Error_OK == cards.make_bonus_card( d->m_uid, /*OUT*/cbinfo) ) {
        list.push_back(cbinfo);
    }

    // no real cards there, but may be bonus cards.
    if( ids.empty() ) 
    {
        d->m_writer << list ;
        return d->send_result(Error_OK);
    }

    oson_eopc ->async_card_info( ids, std::bind(&self_type::on_card_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
}

void card_list_session::on_card_info_eopc(const std::vector<std::string> &ids, const EOCP_Card_list_T& card_map, Error_T ec)  
{
    //post it to client thread.
    d->m_io_service -> post( std::bind(&self_type::on_card_info, shared_from_this(), card_map, ec ) ) ;
}

void card_list_session::on_card_info( const EOCP_Card_list_T& eocp_card_list, Error_T ec)
{
    SCOPE_LOGD(slog);

    if (Error_OK == ec){
        for( Card_info_T& card : list ) {
            update_card_deposit( eocp_card_list, /*out*/card );
            slog.DebugLog("card-pan: '%s', balance: %ld, isbcard: %d, foreign_card: %d", card.pan.c_str(), card.deposit, card.isbonus_card, card.foreign_card);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    d->m_writer << list ; 
    ///////////////////////////////////////////////////
    return d->send_result( Error_OK );
}


namespace
{
struct send_notify_card_owner_changed
{
  void operator()(int64_t uid, const std::string& pan)const
  {
//        DB_T& db = oson_this_db ;
//       
//        Error_T ec = Error_OK ;
//        Users_T users( db );
//        User_info_T info = users.get(uid, ec);
        
//        int const lang =  info.lang; //users.user_language(uid);

  //      std::string phone = info.phone;
        
        //Users_notify_T user_n( db );
//        std::string msg_rus = "Номер телефона владельца карты изменилось. Пожалуйста, удалите и перерегистрируйте карту.\nВаша карта: " + pan ;
//
//        std::string msg_uzb = "Karta egasining telefon raqami o'zgargan. Iltimos, kartani tizimdan o'ching va qayta ro'yxatdan o'tkazing.\nKarta: " + pan ;
//
//        std::string msg =   ( lang == LANG_uzb) ? msg_uzb : msg_rus;
//        
//        SMS_info_T sms(phone, msg, SMS_info_T::type_card_owner_changed_sms ) ;
//        oson_sms -> async_send( sms   );
        //user_n.send2( uid, msg,  MSG_TYPE_PURCHASE_MESSAGE, 7 * 1000 );
  }  
};

struct change_card_owner
{
    void operator()(int64_t card_id, const std::string& new_owner_phone )const
    {
        Cards_T cards( oson_this_db );
        cards.card_edit_owner(card_id, new_owner_phone );
    }
};
} // end noname
//@Note actually there no need DB!! 
 



class card_monitoring_edit_session: public ::std::enable_shared_from_this< card_monitoring_edit_session > 
{
public:
    typedef card_monitoring_edit_session self_t;
    
    explicit card_monitoring_edit_session(api_pointer d,   int32_t monitoring_flag);
    ~card_monitoring_edit_session();
    void async_start();
private:
    void start();
    
    Error_T init_card_monitoring_cabinet();
    
    Error_T async_card_info();
    
    void on_card_info_eopc(const std::vector< std::string> & ids, const EOCP_Card_list_T& info, Error_T ec);
    void on_card_info(const std::vector< std::string> & ids, const EOCP_Card_list_T& info, Error_T ec);
    
    Error_T async_pay();
    
    void on_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec );
    
    void on_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    
    Error_T handle_on_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    
    Error_T save_flag(Error_T ec );
    
private:
    api_pointer d               ;
    int64_t     card_id         ;
    int32_t     monitoring_flag ;
    
    User_info_T  user_info;
    Card_info_T  card_info ;
    Card_list_T card_list;
    Card_cabinet_info_T card_cabinet_info;
    Card_monitoring_tarif_info_T card_tarif;
    Purchase_info_T  p_info ;
    Merchant_info_T  m_info ;
    
};

card_monitoring_edit_session::card_monitoring_edit_session(api_pointer d,  int32_t monitoring_flag)
    : d( d )
    , card_id( 0 )
    , monitoring_flag( monitoring_flag )
{
    SCOPE_LOGD(slog);
}

card_monitoring_edit_session::~card_monitoring_edit_session()
{
    SCOPE_LOGD(slog);
    
    if (p_info.id != 0 ) 
    {
        Purchase_T table(oson_this_db);
        table.update(p_info.id, p_info);
    }
    
    if (card_cabinet_info.id != 0 ) 
    {
        Cards_cabinet_table_T table( oson_this_db ) ;
        table.edit(card_cabinet_info.id, card_cabinet_info);
    }
}

void card_monitoring_edit_session::async_start()
{
    SCOPE_LOGD(slog);
    d->m_io_service->post( ::std::bind( & self_t::start, shared_from_this() ) ) ;
} 

void card_monitoring_edit_session::start()
{
    SCOPE_LOGD(slog);
    
    //monitoring_off_on_timeout
    std::int64_t const MAX_DIFF_SECONDS =  oson_opts -> client.monitoring_off_on_timeout * 60 ;   //60 * 60;// 1 hour

    slog.InfoLog("MAX_DIFF_SECONDS: %lld", (long long)MAX_DIFF_SECONDS ) ;

    
    
    //1. check parameters
    if ( !( monitoring_flag == MONITORING_ON || monitoring_flag == MONITORING_OFF ) ) 
    {
        slog.ErrorLog("parameters are not valid: card-id = %ld , monitoring-flag: %d ", card_id, monitoring_flag ) ;
        return d->send_result( Error_parameters ) ;
    }
    if ( ! ( d->m_uid > 0 ) )
    {
        slog.ErrorLog("not supported public token!");
        return d->send_result( Error_login_failed ) ;
    }
    
    Error_T ec;
    
    Users_T user_table(oson_this_db);
    this->user_info = user_table.get(d->m_uid, ec);
    if(ec) return d->send_result(ec);
    if (user_info.blocked ) {
        slog.WarningLog("blocked user");
        return d->send_result(Error_blocked_user);
    }
    
    ec = init_card_monitoring_cabinet();
    
    if ( ec  )
    {
        if (monitoring_flag == MONITORING_OFF ) 
        {
            slog.WarningLog("Need turn off monitoring, but there do not exists it.");
            return d->send_result( Error_OK ) ;
        }
    }

    const int64_t id_cab = card_cabinet_info.id;
    card_cabinet_info.id = 0;//disable edit on destructor.

    /******************************* OFF situation  ****************************/
    if ( monitoring_flag == MONITORING_OFF ) 
    {
        
        if (card_cabinet_info.monitoring_flag == MONITORING_OFF ) {
            //already off.
            slog.WarningLog("card_monitoring_cabinet monitoring_flag already off.") ;
            d->m_writer << b8(0);
            return d->send_result( Error_OK ) ; 
        }
        
        std::time_t  add_ts =  str_2_time(card_cabinet_info.add_ts.c_str());
        std::time_t  now_ts = std::time(0);
        std::int64_t diff   = (int64_t) now_ts -  (int64_t)add_ts ;
        
        if ( diff < MAX_DIFF_SECONDS ) 
        {
            slog.WarningLog("Very often change, last change time: %s ", card_cabinet_info.add_ts.c_str() );
            
            std::int64_t wait_time = MAX_DIFF_SECONDS - diff +10 ;
            
            d->m_writer << b8( wait_time );
            
            return d->send_result( Error_very_often_access ) ;
        }
        
        //change card_monitoring_cabinet table.
        {
            card_cabinet_info.monitoring_flag = MONITORING_OFF ;
            card_cabinet_info.off_ts          = formatted_time_now_iso_S() ;
            Cards_cabinet_table_T table( oson_this_db ) ;
            table.edit( id_cab , card_cabinet_info ) ;
        }
        
        return d->send_result(Error_OK);
    }
    
    /******************************* ON situation  ****************************/
    if ( id_cab >  0 &&  card_cabinet_info.monitoring_flag == MONITORING_ON ) 
    {
            //already off.
            slog.WarningLog("card_monitoring_cabinet monitoring_flag already on.") ;
            return d->send_result( Error_OK ) ;
    }
    
    // check last off-ts
    {
        std::time_t  off_ts = str_2_time(card_cabinet_info.off_ts.c_str());
        std::time_t  now_ts = std::time(0);
        std::int64_t  diff  = (int64_t)now_ts - (int64_t)off_ts;
        if (diff < MAX_DIFF_SECONDS ) 
        {
            slog.WarningLog("Very often change, last change time: %s ", card_cabinet_info.off_ts.c_str());
            
            std::int64_t wait_time = MAX_DIFF_SECONDS - diff +10 ;
            
            d->m_writer << b8( wait_time );
            
            return d->send_result(Error_very_often_access ) ;
        }
    }
    
    //get tarif
    {
        Cards_monitoring_tarif_table_T tarif_table( oson_this_db ) ;

        Card_monitoring_tarif_info_T ss;
        ss.status  = 1 ;

        ec = tarif_table.info(ss, /*out*/ card_tarif ) ;

        if (ec) return d->send_result( ec ) ;
    }
    
//   add a new row, this is prevent if, there monitoring request very often.
    {
        Cards_cabinet_table_T cc_table(oson_this_db);
        
        bool const isfirst =    ( cc_table.total_payed(d->m_uid)  ==  0 ) ;
        
        std::time_t start_date = std::time(0);//now
        std::time_t end_date   = start_date;
        
        std::tm rt = {}, re = {};
        
        localtime_r(&start_date, &rt ) ;
        
        re = rt;
        
        rt.tm_mday = 1; //always start from 1 day of month
        
        if (isfirst)
        {
            rt.tm_mon -- ;
            if (rt.tm_mon < 0 ) {
                rt.tm_mon = 11;
                rt.tm_year -- ;
            }
        }
        
        re.tm_mday = g_month_days[g_is_leap( re.tm_year + 1900 ) ][ re.tm_mon ] ; // take last day
        
        start_date = ::std::mktime(&rt);
        end_date   = ::std::mktime(&re);
        
        card_cabinet_info.id              = 0 ;
        card_cabinet_info.add_ts          = formatted_time_now_iso_S();
        card_cabinet_info.off_ts          = formatted_time_now_iso_S();
        card_cabinet_info.status          = TR_STATUS_REGISTRED ;
        card_cabinet_info.card_id         = 0;
        card_cabinet_info.monitoring_flag = MONITORING_ON ;
        card_cabinet_info.purchase_id     = 0;
        card_cabinet_info.start_date      = formatted_time("%Y-%m-%d",  start_date );  
        card_cabinet_info.end_date        = formatted_time("%Y-%m-%d",  end_date   ) ; 
        card_cabinet_info.uid             = d->m_uid; 
        
        Cards_cabinet_table_T table( oson_this_db ) ;
        card_cabinet_info.id    = table.add( card_cabinet_info ) ;
        
        
        card_cabinet_info.monitoring_flag = MONITORING_OFF ;//candidate when occurred an error.
        card_cabinet_info.status          = TR_STATUS_ERROR; //candidate when occurred an error.
    }
    
    
    
    ec = async_card_info();
    
    return d->send_result(ec); // even Error_async_processing !
}

Error_T card_monitoring_edit_session::init_card_monitoring_cabinet()
{
    SCOPE_LOG(slog);
    Card_cabinet_info_T  info;
     
    Cards_cabinet_table_T table( oson_this_db ) ;
    
    Error_T ec = table.last_info( d->m_uid , info ) ;
    if (ec) return Error_not_found;
    
    this->card_cabinet_info = info;
    
    
    return Error_OK ;
}


Error_T card_monitoring_edit_session::async_card_info()
{
    SCOPE_LOG(slog);

    Cards_T card_table(oson_this_db);
    Card_info_T search;
    search.uid = d->m_uid;
    
    Card_list_T list;
    
    Sort_T sort(0, 30);//no more 30 supported.
    Error_T ec = card_table.card_list(search, sort, list);
    if (ec) return ec;

    std::vector< std::string > pc_tokens;
    for(const auto& c: list.list)
    {
        if (c.admin_block || c.user_block || c.foreign_card != FOREIGN_NO ) 
            continue;
        
        pc_tokens.push_back(c.pc_token);
    }
    
    
    this->card_list = list;
    
    using namespace ::std::placeholders;
    oson_eopc->async_card_info(  pc_tokens,  ::std::bind( &self_t::on_card_info_eopc, shared_from_this(), _1, _2, _3 )  ) ;
    return Error_async_processing ;
}

void card_monitoring_edit_session::on_card_info_eopc(const std::vector< std::string> & ids, const EOCP_Card_list_T& info, Error_T ec)
{
    d->m_io_service->post( ::std::bind( &self_t::on_card_info, shared_from_this(), ids, info, ec ) ) ;
}

void card_monitoring_edit_session::on_card_info(const std::vector< std::string> & ids, const EOCP_Card_list_T& eopc_card_list, Error_T ec)
{
    SCOPE_LOGD(slog);
    
    if ( ec )
    {
        return d->send_result(ec);
    }
    
    bool primary_card_use = false;
    //1. can use primary-card
    for(const auto& c : card_list.list )
    {
        if (c.is_primary ) {
            oson::backend::eopc::resp::card eocp_card = eopc_card_list.get(c.pc_token)  ;
            if (eocp_card.empty())
                continue;
            
            primary_card_use = eocp_card.status == VALID_CARD && eocp_card.balance >= card_tarif.amount; 
            if (primary_card_use)
            {
                this->card_id = c.id;
                this->card_info = c;
            }
            break;
        }
         
    }
    
    //2. or choose any other if can't choose primary-card
    if( ! primary_card_use )
    {
        slog.WarningLog("Primary card NOT choosen!");
        for(const auto& c : card_list.list )
        {
            oson::backend::eopc::resp::card eocp_card = eopc_card_list.get(c.pc_token)  ;
            if (eocp_card.empty())
                continue;
            bool can_use = eocp_card.status == VALID_CARD && eocp_card.balance >= card_tarif.amount; 
            if (can_use)
            {
                this->card_id = c.id;
                this->card_info = c;
                primary_card_use = true;
                break;
            }
        }
    }
    
    if ( ! primary_card_use ) {
        slog.WarningLog("No available card for pay monitoring!");
        return d->send_result( Error_card_not_found ) ;
    }
    
    slog.DebugLog("Choosen card_id = %lld", (long long)(this->card_id));
    
    this->card_cabinet_info.card_id = this->card_id;
    
    ec =  async_pay() ;
    
    return d->send_result(ec);
}
    
Error_T card_monitoring_edit_session::async_pay()
{
    SCOPE_LOG(slog);
    
    //1. get merchant-info
    Error_T ec ;
    Merchant_T m_table( oson_this_db ) ;
    
    m_info = m_table.get(card_tarif.mid, ec ) ;
    
    if (ec) return ec;
    
    //2. create purchase 
    p_info.id          = 0;
    p_info.amount      = card_tarif.amount;
    p_info.mID         = card_tarif.mid ;
    p_info.uid         = d->m_uid;
    p_info.login       = user_info.phone ;
    p_info.eopc_trn_id = "0";
    p_info.pan         = card_info.pan ;
    p_info.ts          = formatted_time_now("%Y-%m-%d %H:%M:%S");
    p_info.status      = TR_STATUS_REGISTRED;
    p_info.commission  = 0;
    p_info.card_id     = card_info.id; 
     
    Purchase_T p_table(oson_this_db ) ;
    int64_t trn_id = p_table.add(p_info);
    
    p_info.id = trn_id ; 
    p_info.status = TR_STATUS_ERROR ; // candidate when occurred an error.
    
    //save trn_id to card-cabinet-monitoring blabla table.
    card_cabinet_info.purchase_id = trn_id; 
    
    //3. pay to eopc. 
    EOPC_Debit_T in;

    in.amount      = p_info.amount       ;
    in.cardId      = card_info.pc_token  ;
    in.merchantId  = m_info.merchantId   ;
    in.terminalId  = m_info.terminalId   ;
    in.port        = m_info.port         ;
    in.ext         = num2string(trn_id)  ;
    in.stan        = make_stan(trn_id)   ;


    oson_eopc ->async_trans_pay( in,  std::bind(&self_t::on_pay_eopc, shared_from_this(),  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 )   );

  
    return Error_async_processing ;
    
}

void card_monitoring_edit_session::on_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec )
{
    d->m_io_service->post( ::std::bind( & self_t ::on_pay, shared_from_this(), debin, tran, ec ) ) ;
}

void card_monitoring_edit_session::on_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec )
{
    SCOPE_LOGD(slog);
    
    ec = handle_on_pay(debin, tran, ec ) ;
    
    ec = save_flag( ec );
    
    return d->send_result(ec);
}

Error_T card_monitoring_edit_session::handle_on_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOG(slog);
    
    p_info.eopc_trn_id = tran.refNum;

    if (ec != Error_OK) 
    {
        p_info.merch_rsp = "trans pay failed. ec = " + to_str( ec );
        return ec;
    }

    if( !tran.status_ok()  || tran.resp != 0) 
    {
        slog.ErrorLog("line %d Error in EOPC while perform transaction", __LINE__);
        p_info.merch_rsp += ", trans pay error. tran.status: " + tran.status + ", tran.resp = " + to_str(tran.resp);
        return Error_eopc_error ;
    }

    return Error_OK ;
}
Error_T card_monitoring_edit_session::save_flag(Error_T ec)
{
    SCOPE_LOG(slog);
    
    if  ( ec != Error_OK )
    {
        //1. commit purchase
        p_info.status = TR_STATUS_ERROR ;
        Purchase_T p_table(oson_this_db ) ;
        p_table.update(p_info.id, p_info);
        
        p_info.id = 0;//disable update on destructor.
        
        //2. commit card_cabinet
        card_cabinet_info.status          = TR_STATUS_ERROR ;
        card_cabinet_info.off_ts          = formatted_time_now_iso_S();
        card_cabinet_info.monitoring_flag = MONITORING_OFF ;
        
        Cards_cabinet_table_T c_table( oson_this_db ) ;
        c_table.edit(card_cabinet_info.id, card_cabinet_info ) ;
        
        card_cabinet_info.id = 0;//disable update on destructor.
        
        slog.FailureExit();
        
        return  ec ;
    }
    p_info.merch_rsp = "SUCCESS" ;
    p_info.status = TR_STATUS_SUCCESS ;
    Purchase_T p_table(oson_this_db ) ;
    p_table.update(p_info.id, p_info ) ;
    
    p_info.id = 0;//disable update on destructor.
        
    
    
    card_cabinet_info.monitoring_flag = MONITORING_ON ;
    card_cabinet_info.status = TR_STATUS_SUCCESS ;
    
    Cards_cabinet_table_T c_table(oson_this_db ) ;
    c_table.edit(card_cabinet_info.id, card_cabinet_info ) ;
 
    card_cabinet_info.id = 0;//disable update on destructor.
    
    return Error_OK   ;
}
    
/** This used on osoninit PeridicOperations. */
/*static*/ Error_T  global_card_monitoring_edit( int64_t uid, 
                                                 /*int64_t card_id, */
                                                 int32_t monitoring_flag,  
                                                 std::shared_ptr< boost::asio::io_service > io_service_ptr 
                                                )
{
    SCOPE_LOGD(slog);
    
    api_pointer d = std::make_shared< api_data >( io_service_ptr ) ;
    
    d->m_uid = uid;
    
    d->m_dev_id = 0;
    
    auto s   = std::make_shared< card_monitoring_edit_session > (d, /*card_id,*/ monitoring_flag) ;
    
    s->async_start();
    
    return Error_async_processing ;
}

static Error_T api_card_monitoring_edit(api_pointer_cref d, ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    
    int64_t card_id = 0 ;
    int32_t monitoring_flag = 0;
    reader >> r8( card_id ) >> r4(monitoring_flag);
    
    oson::ignore_unused(card_id);
    
    auto s   = std::make_shared< card_monitoring_edit_session > (d, /*card_id,*/ monitoring_flag) ;
    
    s->async_start();
    
    return Error_async_processing ;
    
}

static Error_T  api_card_monitoring_tarif (api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    /****************************/
    Cards_monitoring_tarif_table_T table(oson_this_db ) ;
    Card_monitoring_tarif_info_T search, info;
    //get all
    search.status = 0;
    search.id     = 1;

    Error_T ec = table.info(search, info);
    if ( ec ) return ec;

    d->m_writer << b4( info.id )  << b8( info.amount )  << b4( info.mid )   << b4( info.status ) ;
    
    return Error_OK ;
}

static Error_T api_card_monitoring_payed_months(api_pointer_cref d, ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    
    int64_t card_id; 
    std::string start_date, end_date;
    Sort_T sort;
    
    reader >> r8(card_id) >> r2(start_date) >> r2(end_date) >> r4(sort.offset) >> r2(sort.limit);
    oson::ignore_unused(card_id);
    //////////////////
    Card_cabinet_info_T search;
    search.start_date = start_date;
    search.end_date   = end_date;
    search.card_id    = 0;
    search.status     = TR_STATUS_SUCCESS ;
    search.uid        = d->m_uid;
    
    if (sort.limit <= 0 || sort.limit > 256) {
        sort.limit = 256;
    }
    
    Card_cabinet_list_T out;
    Cards_cabinet_table_T table( oson_this_db ) ;
    
    table.list(search, sort, out ) ;
    
    /**************************************/
    std::set< std::string > ss;
    for(const auto & c : out.list)
    {
        const std::string& start_date = c.start_date;
        const std::string& end_date = c.end_date ;
        //YYYY-mm-dd
        size_t i = start_date.find_last_of('-');//@Note: if there no '-' symbol ==>  i == npos  ==>  start_date.subset(0, npos) ==> start_date itself.
        std::string sd = start_date.substr(0,i);
        ss.insert( sd ) ;
        
        i = end_date.find_last_of('-');
        
        std::string ed = end_date.substr(0,i);
        if (sd != ed ){
            ss.insert( ed );
        }
        
        slog.InfoLog("start_date: %s ,  end-date: %s,  sd: %s   ed: %s ", start_date.c_str(), end_date.c_str(), sd.c_str(), ed.c_str());
    }
    /**************************************************/
    d->m_writer << b4(out.total_count) << b4(ss.size()) ;
    for(std::string const& s: ss){
        d->m_writer << s;
    }
    return Error_OK ;
    
}


static Error_T api_client_card_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    OSON_PP_USER_LOGIN(d, reader);
    
    typedef card_list_session session;
    
    session::pointer s = std::make_shared< session > ( d ) ;
    s->async_start();
    
    return Error_async_processing ;
}
    
static Error_T foreign_card_activate_req_i(api_pointer_cref d, const Card_info_T& c_info)
{
    SCOPE_LOG(slog);
    
    if ( ! c_info.id )
        return Error_parameters;
    
    Error_T ec = Error_OK ;
    Users_T users( oson_this_db  );
    User_info_T user_info = users.get(c_info.uid, ec);
    if ( ec )return ec;

    std::string phone = c_info.owner_phone;
    
    Activate_table_T act_table( oson_this_db  );
    
    {
        Activate_info_T s;
        s.phone = phone;
        s.kind  = Activate_info_T::Kind_foreign_card_register;
        s.add_ts = formatted_time("%Y-%m-%d %H:%M:%S", std::time(0) - 60 ) ;// 60 seconds earyly
        
        int cnt = act_table.count(s);
        if (cnt > 0)
        {
            slog.WarningLog("There already sms (  past 30 seconds ) " ) ;
            return Error_OK ;
        }
    }
    
    
    std::string code = oson::utils::generate_code( 5 ) ;
    
    if ( ! code.empty() && code[0] == '0' ) //@fix Android trailing zero error.
    {
        code[0] = '7';
    }
    
    std::string message  = "Код активации: " + code + ". Ваша карта добавляется к пользователю "
                            + user_info.phone + ". Если вы согласны, то сообщите ему данный код. www.oson.uz";
    SMS_info_T sms_info( phone, message, SMS_info_T::type_client_card_add_code_sms ) ;
    oson_sms -> async_send (sms_info);

    Activate_info_T act_i;
    act_i.phone    = user_info.phone;
    act_i.code     = code ;
    act_i.kind     = act_i.Kind_foreign_card_register;
    act_i.other_id = c_info.id ;
    act_i.dev_id   = "foreign_card";
    act_i.add_ts   = formatted_time_now_iso_S();
    
    act_table.add(act_i);
    
    if ( phone.length() >= 9)  // fill with '*'  all digits except on last 2, and first 3.
        std::fill_n(phone.begin() + 3, phone.length() - 5, '*') ;

    
    {
        std::string msg = "Код активации отправлен на номер "+ phone + ". Код действителен в течении 8 часов." ;
        Users_notify_T notify{ oson_this_db } ;
        notify.send2( d->m_uid, msg, MSG_TYPE_BULK_MESSAGE, 5 * 1000 );
    }
    return Error_OK ;
}

static Error_T foreign_card_activate_req_i(api_pointer_cref d, int64_t card_id )
{
    SCOPE_LOG(slog);
    Error_T ec = Error_OK;
    Cards_T card( oson_this_db  );
    Card_info_T c_info = card.get(card_id, ec);
    if ( ec ) return ec ;
            
    if( !( c_info.foreign_card == FOREIGN_YES && c_info.uid == d->m_uid ) ) {
        slog.WarningLog("Try get code for own card");
        return Error_not_found;
    }
    return foreign_card_activate_req_i(d, c_info);
}
namespace{
    
struct async_client_card_add_handler
{
    typedef async_client_card_add_handler self_type;
    
    api_pointer      d         ;
    Card_info_T      c_data    ;

    explicit async_client_card_add_handler( api_pointer d, const Card_info_T & c_info) ;
    
    void operator()(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec) ;
    
    void handler(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec);
    Error_T  add( const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & out, Error_T ec) ;
    
}; // end async_client_card_add_handler

async_client_card_add_handler::async_client_card_add_handler( api_pointer d, const Card_info_T & c_info)
         : d( d ),  c_data( c_info )
{
   
}

void async_client_card_add_handler::operator()(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec)
{
    self_type self_copy(*this);
    
    //post it to client thread
    d->m_io_service -> post ( std::bind(&self_type::handler, self_copy, in, card, ec ) ) ;
}

void async_client_card_add_handler::handler(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec)
{
    SCOPE_LOGD(slog);
    ec =  add( in, card, ec );
    return d->send_result(ec);
}

Error_T  async_client_card_add_handler::add(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & out, Error_T ec)
{
    SCOPE_LOGD(slog);

    if( ec != Error_OK ) return ec;
    
    Users_T users( oson_this_db  );

    User_info_T user_info = users.get(d->m_uid, ec);

    if (ec) return ec ;

    if ( out.status != VALID_CARD )
    {
        slog.ErrorLog("This card is blocked, for more information see status.");
        std::string msg = "\"" + out.pan + "\"  карта заблокирована. Добавить карту в систему OSON не возможно. Обратитесь в банк, пожалуйста."  ;
        Users_notify_T notify{ oson_this_db } ;
        notify.send2(user_info.id, msg,  MSG_TYPE_BULK_MESSAGE, 5 * 1000 ) ;
       return Error_card_blocked;
    }

    c_data.pan         = out.pan      ;
    c_data.owner       = out.fullname ;
    c_data.pc_token    = out.id       ;
    c_data.owner_phone = out.phone    ;

    if (out.phone.empty() || ! valid_phone(out.phone ) )
    {
        slog.ErrorLog("phone not determined.");
        std::string msg = "На \"" + out.pan + "\"  карту SMS оповещение не подключено. Обратитесь в банк, пожалуйста." ;
        Users_notify_T notify{ oson_this_db } ;
        notify.send2(user_info.id, msg,  MSG_TYPE_BULK_MESSAGE, 5 * 1000 ) ;
        
        return Error_not_found;//Error_card_phone_not_found will used in future!
    }

    Cards_T card_table(  oson_this_db   );
    //check already exists situation
    {
        size_t count = 0;
        card_table.card_count(c_data.uid, c_data.pc_token, /*OUT*/ count);
        if ( count > 0 ) {
            slog.WarningLog("This card already exists in the database.");
            
             std::string msg = "\"" + out.pan + "\"  карта уже добавлена." ;
             Users_notify_T notify{ oson_this_db } ;
             notify.send2(user_info.id, msg,  MSG_TYPE_BULK_MESSAGE, 5 * 1000 ) ;
       
            return Error_a_new_card_already_exists;
        }
    }

    bool const identity =  ( user_info.phone == out.phone ) ;  

    c_data.foreign_card = (!identity ) ? FOREIGN_YES : FOREIGN_NO ;
    c_data.id           = card_table.card_add( c_data );

    /////////////////////////////////////
    d->m_writer << b8( c_data.id ); 
    ////////////////////////////////////

    if(  !identity ) {
        ec = foreign_card_activate_req_i( d, c_data );
        if ( ec ) return ec;//if fail exit.
        return Error_card_foreign; // on success return foreign card error.
    }
    return Error_OK;
}

} // end noname namespace

static Error_T async_client_card_add(api_pointer_cref d, const Card_info_T& c_info)
{
    SCOPE_LOG(slog);
    oson::backend::eopc::req::card in = { c_info.pan, expire_date_rotate(c_info.expire) } ;
    oson_eopc -> async_card_new (in,  async_client_card_add_handler(d, c_info) ) ;
    
    return Error_async_processing ;
}

static Error_T api_client_card_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    if (0 == d->m_uid ){
        slog.ErrorLog("Unauthorization access!") ; 
        return Error_login_failed;
    }
    /////////////////////////////////////////////////
    Card_info_T c_data;
    c_data.uid          = d->m_uid;
    c_data.is_primary   = PRIMARY_NO;
    c_data.foreign_card = FOREIGN_NO;

    reader >> r2(c_data.is_primary) >> r4(c_data.tpl) >> r2(c_data.pan) >> r2(c_data.expire) >> r2(c_data.name) ;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    if(c_data.expire.length() != 4) {
        slog.WarningLog("Wrong expire length");
        return Error_parameters;
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    Error_T ec = Error_OK;
    Users_T users( oson_this_db  );
    User_info_T u_user_info = users.get( d->m_uid, ec );
    if ( ec ) return ec;
    
    Cards_T card( oson_this_db  );
    
    size_t const card_count = card.card_count(d->m_uid);

    if ( card_count >= 30 ) {
        slog.ErrorLog("This user already have many-many cards ( %zu ) ",  card_count );
//        SMS_info_T sms_info( u_user_info.phone, "Слишком много добавленных карт : " + to_str(card_count), SMS_info_T::type_client_card_add_code_sms  ) ;
//        oson_sms -> async_send (sms_info);
        return Error_operation_not_allowed;
    }

    if( !card_count )
        c_data.is_primary = PRIMARY_YES;

     return async_client_card_add( d, c_data );
}


static Error_T api_client_card_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    if ( 0 == d->m_uid ) {
        slog.ErrorLog("Unauthorized access!");
        return Error_login_failed;
    }
    //////////////////////////////////////////////
    Card_info_T c_data;
    reader >> r8(c_data.id) >> r4(c_data.tpl) >> r2(c_data.user_block) >> r2(c_data.name);
    /////////////////////////////////////////////////////////////////////////////////////
    if ( ! c_data.id ){
        slog.ErrorLog("Card id not set!");
        return Error_parameters;
    }
    
    Error_T ec;
    Cards_T card( oson_this_db  );
    Card_info_T info = card.get(c_data.id, ec);
    if ( ec )return ec;
    
    if (info.uid != d->m_uid){
        slog.ErrorLog("This is not own card!");
        return Error_card_foreign;
    }
    
    //@Note: avoid access to database twice.
    info.tpl         = c_data.tpl        ;
    info.user_block  = c_data.user_block ;
    info.name        = c_data.name       ;

    //@Note: do it asyncronously
    if (info.user_block == 1) {
        //don't wait!
        oson_eopc -> async_card_block(info.pc_token, oson::card_block_handler() ) ;
    }
    return card.card_edit(c_data.id, info);
}

static bool set_is_primary_automatic(int64_t uid )
{
    SCOPE_LOG(slog);
    Cards_T card( oson_this_db );
    std::vector< Card_info_T > list = card.card_list( uid );
    int64_t card_id = std::numeric_limits< int64_t >::max();
    for(const Card_info_T& c : list)
    {
        if (c.id < card_id &&  ! c.admin_block   &&  ! c.user_block  && c.foreign_card == FOREIGN_NO && c.is_primary == PRIMARY_NO )
        {
            card_id = c.id;
        }
    }

    if (card_id <  std::numeric_limits< int64_t >::max() )
    {
        card.set_primary(card_id);
        return true;
    }
    return false;
}

static Error_T api_client_card_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    //////////////////////////////////////////////
    int64_t const card_id = reader.readByte8();
    ///////////////////////////////////////////////
    if (d->m_uid == 0 ){
        slog.ErrorLog("Unauthorizated access!"); 
        return Error_login_failed;
    }
    if (card_id  == 0  ) {
        slog.ErrorLog("card_id not set!");
        return Error_parameters;
    }
    /////////////////////////////////////////
    Error_T ec = Error_OK ;
    
    Cards_T card(  oson_this_db   );
    Card_info_T info  = card.get(card_id, ec);
    if ( ec ) return ec; // not found!
    
    if ( info.uid != d->m_uid ){
        slog.ErrorLog("This card is not own card!") ;
        return Error_card_foreign ;
    }
    
    bool const was_primary = ( info.is_primary == PRIMARY_YES );
    card.card_delete( card_id );
    //////////////////////////////////////////////////////////////////////////    
    if ( was_primary )
    {
        set_is_primary_automatic( d->m_uid ) ;
    }
    //////////////////////////////////////////////////////////////////////////////////////
    return Error_OK ;
}

static Error_T api_client_primary_card(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    if ( 0 == d->m_uid){
        slog.ErrorLog("Unauthorized access!");
        return Error_login_failed;
    }

    Card_info_T c_search;
    reader >>  r2(c_search.pan) >> r8(c_search.id);
    ///////////////////////////////////////////////
    if (c_search.id > 0)
        c_search.pan.clear(); //ненужен этот пан.

    c_search.uid = d->m_uid   ;
    
    //@Note Что за чёрт возми здесь происходят былин
    Card_list_T c_list;
    Cards_T cards( oson_this_db  );
    Sort_T sort;
    Error_T error = cards.card_list(c_search, sort, c_list);
    if (error != Error_OK)
        return error;

    if(c_list.count == 0 || c_list.list.empty() ){
        slog.WarningLog("Card not found");
        return Error_login_empty;
    }
    c_search.uid = c_list.list[0].uid;
    c_search.id = c_list.list[0].id;

    error = cards.unchek_primary(d->m_uid);
    if (error != Error_OK)
        return error;

    return cards.set_primary(c_search.id);
}

static Error_T api_foreign_card_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    /////////////////////
    Card_info_T card_search;
    card_search.uid           = d->m_uid;
    card_search.foreign_card  = FOREIGN_YES;
    Cards_T cards( oson_this_db  );
    Card_list_T list;
    Error_T ec = cards.card_list(card_search, Sort_T(), list);
    if ( ec )return ec;
    ///////////////////////////////////////////////////////////////////
    d->m_writer << b2( list.list.size() );
    
    for(const Card_info_T& card : list.list)
    {
        d->m_writer << b8(card.id) << b4(card.tpl) << card.pan << card.expire ;
    }
    //////////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_foreign_card_activate_req(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    int64_t  const card_id = reader.readByte8() ;
    return foreign_card_activate_req_i( d, card_id ) ;
}

class card_history_session : public  std::enable_shared_from_this< card_history_session> 
{
public:
    typedef card_history_session self_t;
    
    struct date_t
    {
        int year ;
        int mon  ;
        int day  ;
        
        std::string to_string()const;
        std::string to_eopc_str()const;
        
        inline bool operator == (const date_t& o)const
        { 
            return year == o.year && mon == o.mon && day == o.day ; 
        }
    };
    
    struct in_t
    {
        int64_t uid, card_id;
        int32_t offset, count, order;
        date_t startDate, endDate, today;
        
        static date_t make_date(const std::string & ds ) ;
        
        int total_days()const{ return endDate.day - startDate.day + 1; }
    };

    card_history_session(api_pointer d, in_t const& in);
    ~card_history_session();
    
    void async_start();
private:
    void start();
    Error_T start_e();
    
    Error_T offline_results();
    Error_T online_results(std::vector<int> const& );
    //check that, the user was payed for d - date.
    static int payed_month(date_t d, int64_t uid);
    
    std::vector<int> loaded_month();
    
    void load_history(date_t s, date_t e,  int64_t tid, int offset, int count );
    
    void on_load_history_eopc(  int64_t tid,  const EOCP_card_history_req& , const EOCP_card_history_list&, Error_T ec );
    void on_load_history(  int64_t tid,    EOCP_card_history_req , const EOCP_card_history_list&, Error_T ec );
private:
    api_pointer d ;
    /*const*/ in_t        i ;
    
    Card_info_T card_info;
};

std::string card_history_session::date_t::to_string()const
{
    //SCOPE_LOGD(slog);
    char buf[64] = {};
    size_t z;
    z = snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, mon, day);
    
   // slog.InfoLog("z = %zu " ,z ) ;
    
    //basic_string( const char*, size_t, Allocator)  
    std::string result =  std::string( (const char*)buf, z);
    
    //slog.InfoLog("result: %s", result.c_str());
    return result;
}

std::string card_history_session::date_t::to_eopc_str()const
{
    char buf[64] = {};
    size_t z;
    z = snprintf(buf, sizeof(buf),"%04d%02d%02d", year, mon, day);
    return std::string( (const char*)buf, z);
}

int card_history_session::payed_month(date_t date, int64_t uid )
{
    SCOPE_LOG(slog);
    Cards_cabinet_table_T table( oson_this_db ) ;
    
    int cnt = table.payed_date_count( date.to_string() , uid ) ;
    
    return cnt  ;
}

std::vector<int> card_history_session::loaded_month()
{
    SCOPE_LOG(slog);
    std::vector< int > res;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    {
        std::string query = 
                "SELECT string_agg( date_part( 'day', d) ::text, ',' ) FROM generate_series ( " + escape(i.startDate.to_string()) + " ::date, " 
                + escape (i.endDate.to_string()) + ", '1 day' ) d "
                "WHERE EXISTS( SELECT 1 FROM card_monitoring_load WHERE card_id = " + escape(i.card_id) +
                " AND ( status = 1 OR status = 6 ) AND from_date <= d AND to_date >= d ) " 
                ;
        DB_T::statement st(oson_this_db);
        st.prepare(query);
        
        
        std::string ls;
        st.row(0)>>ls;
        
        slog.InfoLog("days: %s ", ls.c_str());
        
        ls += ',';

        std::vector< int > days; 
        int val = 0;
        
        const bool exclude_today = i.endDate == i.today;
        /***************************************************/
        for(char c: ls)
        {
            if ( c == ',' )  
            {
                if ( val > 0 )
                {
                    //@Note: do not add today!
                    if ( exclude_today && i.endDate.day == val )
                    {
                    }    
                    else
                    {
                        days.push_back( val ); 
                    }
                }
                val = 0;
            }
            else 
            { 
                val = val * 10 + ( c - '0' ) ;
            }
        }
        /*****************************************************/
        res.swap( days );
    }
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    return res;
}

card_history_session::card_history_session(api_pointer d, const in_t& in)
   : d(d), i(in)
{
    SCOPE_LOGD(slog);
    
     if ( ! (i.startDate.day >= 1 && i.startDate.day <= 31 && i.endDate.day >= 1 && i.endDate.day <= 31 &&   i.startDate.day <= i.endDate.day ) )
    {
        slog.WarningLog("Some variables are invalid!!");
        throw std::runtime_error("some variables are invalid!");
    }
}

card_history_session::~card_history_session()
{
    SCOPE_LOGD(slog);
    if ( static_cast<bool>( d->m_ssl_response) ){
        slog.WarningLog("Something is wrong!");
        d->send_result(Error_internal);
    }
}

void card_history_session::async_start()
{
    d->m_io_service->post( std::bind(&self_t::start, shared_from_this() ) ) ;
}

void card_history_session::start()
{
    SCOPE_LOGD(slog);
    
    Error_T ec = start_e();
    return d->send_result(ec);
}

Error_T card_history_session::start_e()
{
    SCOPE_LOG(slog);
    
    /////////////////////1. check card-id, uid...
    if ( !( i.uid > 0)  || ! ( i.card_id > 0 )  ) 
    {
        slog.WarningLog("parameters are invalid! uid = %ld  card-id: %ld ", i.uid, i.card_id);
        return Error_parameters;
    }
    
    if ( !( i.startDate.year == i.endDate.year  && i.startDate.mon == i.endDate.mon))
    {
        slog.WarningLog("startDate and endDate different month. startDate{ %04d-%02d-%02d }, endDate{ %04d-%02d-%02d } ", i.startDate.year, i.startDate.mon, i.startDate.day,
                i.endDate.year, i.endDate.mon, i.endDate.day ) ;
        
        return Error_parameters;
    }
    
    {
        i.today = i.make_date(formatted_time_now("%Y%m%d"));
        slog.InfoLog("today: year: %d   mon: %d  day: %d", i.today.year, i.today.mon, i.today.day);
        
        if (i.today.year == i.endDate.year && i.today.mon == i.endDate.mon){
            if (i.endDate.day > i.today.day)
            {
                slog.WarningLog("endDate downside to today!");
                i.endDate.day = i.today.day;
            }
        }
        
        if (i.endDate.day < i.startDate.day ) {
            slog.WarningLog("endDate ( day: %d )  less than startDate ( day: %d ) . ans is empty! ", i.endDate.day, i.startDate.day);
            d->m_writer << b4(0) << b4(0) << b4(0) ;
            return Error_OK ;
        }
    }
    ////////////////////////////////////////////////////////
    
    
    Card_info_T  card_search;
    card_search.uid = i.uid;
    card_search.id  = i.card_id ;
    
    Cards_T  card_table( oson_this_db ) ;
    Error_T ec = card_table.info(card_search, this->card_info ) ;
    if ( ec )return ec ;

    if (card_info.admin_block || card_info.user_block ) {
        slog.ErrorLog("blocked cards");
        d->m_writer << b4( 0 ) << b4(0) << b4( 0 );
        return Error_OK ;
    }
    if (card_info.foreign_card != FOREIGN_NO )
    {
        slog.ErrorLog("foreign cards");
        d->m_writer << b4( 0 ) << b4(0) << b4( 0 );
        return Error_OK ;
    }
    
    
    int const payed = payed_month(i.startDate, d->m_uid );
    
    if ( ! payed )
    {
        d->m_writer << b4( 0 ) << b4(0) << b4( 0 );
        return Error_OK ;
    }
    
    std::vector< int > r = loaded_month(  );
     
    if ( (int)( r.size() )== i.total_days()  ) {
        
        return offline_results();
    }
     
    
    
    return online_results( r );
}

Error_T card_history_session::offline_results()
{
    SCOPE_LOG(slog);
     
    
    Cards_monitoring_table_T table(oson_this_db ) ;
    
    std::string from_date = i.startDate.to_string()
              , to_date   = i.endDate.to_string();
    
    Sort_T sort(i.offset, i.count, Order_T( 5, 0, i.order ) ) ; // 5 - is ts.
    
    Card_monitoring_list_T out;
    Card_monitoring_search_T search;
    search.card_id = i.card_id;
    search.from_date = from_date;
    search.to_date = to_date;
    
    table.list( search, sort, out);
    
    /////////////////////////////////////
    d->m_writer << b4(out.total_count) << b4(out.list.size()) << b4(0) ; // last it flag
    for(const auto& o : out.list )
    {
        d->m_writer << b8(o.id) << b8(o.card_id) << b8(o.uid ) << o.pan << o.ts << b8(o.amount) << b4( (int)o.reversal) << b4( (int)o.credit) << o.refnum
                    << b4(o.status) << b8(o.oson_pid) << b8(o.oson_tid)  << o.merchant_name
                    << o.epos_merchant_id << o.epos_terminal_id <<   o.street << o.city;
    }
    
    return Error_OK ;
}

Error_T card_history_session::online_results( std::vector<int> const& rd )
{
    SCOPE_LOG(slog);
    
    
    // separate  30  days  by 1    to 30 parts.
    // parallel  load all, and save it   card_monitoring  and card_monitoring_load (start_date, to_date).
    // after finished,  merge all start_date and to_date's.
    if ( rd.empty() ) 
    {
        d->m_writer << b4(0) << b4(0) << b4(1) ; // last is flag: 1 - online loaded.
    } 
    else 
    {
        offline_results();//if there exists in database.
    }
    
    d->send_result(Error_OK);
    /////////////////////////////////////////////////////////////////////////////////////////
    const int days = g_month_days[g_is_leap(i.startDate.year)][i.startDate.mon - 1 ] ;
    slog.InfoLog("days of this month: %d ", days ) ;
    
    Cards_monitoring_load_table_T table( oson_this_db ) ;
    Card_monitoring_load_data_T load_data;
   
    
    int has_days[ 40 ] = {};
    for(int d: rd) 
       has_days[ oson::utils::clamp( d, 0, 33 ) ] = 1 ;
    
    /////////////////////////////////////////////////////////////////////////////////////////////
    const int chunk_size = 1;
    const int chunk_number = (days + chunk_size - 1) / chunk_size ;
    
    std::vector< std::pair< date_t, date_t > > loaded_dates ;
    
    for(int ichunk = 0; ichunk != chunk_number; ++ichunk)
    {
        int start_day = ichunk * chunk_size  + 1 ;
        int end_day   = std::min< int>( days, start_day + chunk_size - 1) ;
        
        if ( end_day < i.startDate.day  || start_day > i.endDate.day ) 
        {
            slog.WarningLog("[%d .. %d ] interval not in [%d .. %d ] ", start_day, end_day, i.startDate.day, i.endDate.day);
            continue;
        }
        //all [start_day...end_day]  interval  already loaded
        if ( std::accumulate( has_days + start_day, has_days + end_day + 1, 0) == end_day + 1 - start_day   )
        {
                slog.WarningLog("[%d .. %d] interval already loaded!", start_day, end_day ) ;
                continue;
        }
        
        date_t sd = i.startDate, ed = i.endDate;
        sd.day = start_day;
        ed.day = end_day ;
        loaded_dates.push_back( std::make_pair(sd, ed ) ) ;
    }
    
    for(auto dates: loaded_dates )
    {
        date_t sd = dates.first , ed = dates.second;
        
        load_data.id        = 0                ;
        load_data.card_id   = i.card_id        ;
        load_data.from_date = sd.to_string()   ;
        load_data.to_date   = ed.to_string()   ;
        load_data.ts        = formatted_time_now_iso_S() ;
        load_data.status    = TR_STATUS_REGISTRED ;
        
        
        load_data.status = TR_STATUS_ERROR ; 
        table.del(load_data);//if there exists error status, delete it.
        
        //now add a new.
        load_data.status    = TR_STATUS_REGISTRED ;
        int64_t tid  = table.add( load_data );
        
        int offset = 0, count = 10;
        
        load_history( sd, ed, tid, offset, count );
    }
    return Error_async_processing ;
}

void card_history_session::load_history(date_t s, date_t e,  int64_t tid, int offset, int count )
{
    SCOPE_LOG(slog);
    
    EOCP_card_history_req eocp_req;
    eocp_req.card_id    = card_info.pc_token ;
    eocp_req.startDate  = s.to_eopc_str()  ;
    eocp_req.endDate    = e.to_eopc_str()  ;
    eocp_req.pageNumber = offset     ;
    eocp_req.pageSize   = count   ; 
    
    using namespace std::placeholders;
    oson_eopc->async_card_history( eocp_req,   
        std::bind( &self_t::on_load_history_eopc, shared_from_this(), tid, _1, _2, _3 )
    ) ;

}

void card_history_session::on_load_history_eopc( int64_t tid,  const EOCP_card_history_req& req, const EOCP_card_history_list& out, Error_T ec )
{
    d->m_io_service->post( std::bind(&self_t::on_load_history, shared_from_this(), tid, req, out, ec ) ) ;
}

void card_history_session::on_load_history( int64_t tid,   EOCP_card_history_req in, const EOCP_card_history_list& out, Error_T ec )
{
    SCOPE_LOG(slog);
    /////////////////////////////////////////////////////////////
    {
        Cards_monitoring_load_table_T table( oson_this_db ) ;

        if(ec != Error_OK )
        {
            slog.ErrorLog("ec = %d" ,(int)ec);
            if ( ec == Error_timeout )
            {
                in.pageSize /= 2 ;
                
                if (in.pageSize > 0 ) 
                {
                    slog.WarningLog("RETRY WITH LESS pageSize!");
                    using namespace std::placeholders;
                        oson_eopc->async_card_history( in,   
                            std::bind( &self_t::on_load_history_eopc, shared_from_this(), tid, _1, _2, _3 )
                        ) ;
                    return ;    
                }
            }
            
            //@Note if this is first request, then set error status.
            if (in.pageNumber == 0 ) {
                table.set_status( tid, TR_STATUS_ERROR ) ;
            }
            return ;
        }

        //add to table
        table.set_status(tid, TR_STATUS_SUCCESS );
    }
    ///////////////////////////////////////////////////////////////
    Cards_monitoring_table_T table( oson_this_db ) ;
    
    for(const EOCP_card_history_resp& p : out.list ) 
    {
        Card_monitoring_data_T data;
        data.id = 0;
        data.card_id = i.card_id  ;
        data.uid     = i.uid      ;
        data.pan     = p.hpan     ;
        data.ts      = p.date_time() ;
        data.amount  = p.reqamt   ;
        data.reversal= p.reversal ;
        data.credit  = p.credit   ;
        data.refnum  = p.utrnno   ;// please test it.
        data.status  = p.resp == -1 ? TR_STATUS_SUCCESS : TR_STATUS_ERROR ;
        data.oson_pid = 0 ;
        data.oson_tid = 0 ;
        data.merchant_name    = p.merchantName;
        data.epos_merchant_id = p.merchantId;
        data.epos_terminal_id = p.terminalId;
        data.city     = p.city;
        data.street   = p.street;
        
        data.id = table.add( data ) ;
       
    }
    
    if ( ! out.last   )
    {
        slog.InfoLog("Not last retrieve remain part also!");
        in.pageNumber++; 
        using namespace std::placeholders;
        oson_eopc->async_card_history( in,   
            std::bind( &self_t::on_load_history_eopc, shared_from_this(), tid, _1, _2, _3 )
        ) ;

    }
    
}

card_history_session::date_t  card_history_session::in_t::make_date(const std::string& ds)
{
    //     01234567
    //ds:  YYYMMDD
    date_t res= {};
    int p[8] = {};

    if (ds.length() < 8) 
        return res;
    
    for(int i = 0; i < 8; ++i)p[i] = ds[i] - '0';
    
    res.day  = p[6] * 10 + p[7];
    res.mon  = p[4] * 10 + p[5];
    res.year = p[0] * 1000 + p[1] * 100 + p[2] * 10 + p[3];
    
    res.day = oson::utils::clamp(res.day, 1, 31 ) ;
    res.mon = oson::utils::clamp(res.mon, 1, 12 ) ;
    res.year = oson::utils::clamp(res.year, 1, 999999) ;
    
    return res;
}

static Error_T api_card_history_list(api_pointer_cref d, ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    
    int64_t card_id  = 0;
    std::string start_date, end_date, sort_order;
    int32_t offset, count;
    
    reader >> r8(card_id) >> r2(start_date) >> r2(end_date) >> r4(offset) >> r2(count) >> r2( sort_order ) ;
    slog.InfoLog("card-id: %ld, start-date: %s  end-date: %s   offset: %d, count: %d ", card_id, start_date.c_str(), end_date.c_str(), offset, count);
    /*******************************************************/
    if (card_id == 0 ) {
        return Error_parameters;
    }
    
    if (start_date.empty() ) 
    {
        start_date = formatted_time("%Y%m%d",  std::time(0) - 1 * 24 * 60 * 60 ) ; // 1 day
    }  else { 
        start_date = formatted_time("%Y%m%d", str_2_time(start_date.c_str()) ) ;
    } 
    
    if (end_date.empty() ) 
    {
        end_date = formatted_time("%Y%m%d", std::time(0) ) ;
    } else{
        end_date = formatted_time("%Y%m%d", str_2_time(end_date.c_str())) ;
    }
    
    slog.InfoLog("EOCP: start-date: %s  end-date: %s ", start_date.c_str(), end_date.c_str());
    /**************************************************************/
    
    //////////////////////////////////////////// 
    card_history_session::in_t i ;
    i.uid       = d->m_uid ;
    i.card_id   = card_id  ;
    i.offset    = offset   ;
    i.count     = count    ;
    i.order     = make_order_from_string(sort_order, Order_T::DESC); // default DESC
    i.startDate = i.make_date( start_date ) ;
    i.endDate   = i.make_date( end_date   ) ;
    
    
    slog.DebugLog("i{ uid = %ld card-id = %ld offset = %d count = %d startDate{ year= %d mon = %d day = %d } , endDate{ year  = %d mon = %d day = %d } ",
                i.uid, i.card_id, (int)i.offset, (int)i.count, i.startDate.year, i.startDate.mon, i.startDate.day, 
                i.endDate.year, i.endDate.mon, i.endDate.day ) ;
    
    
    if( i.count <= 0 || i.count > 1024){
        i.count = 1024;
    }
    
    auto session = std::make_shared< card_history_session>( d, i ) ;
    
    session->async_start();
    ////////////////////////////////////////////////
    return Error_async_processing ;
}


static Error_T api_foreign_card_activate(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    //////////////////////////////////////////////////////////////
    int64_t  const card_id  = reader.readByte8();
    std::string act_code    = reader.readAsString( reader.readByte2() );
    ////////////////////////////////////////////////////////////
    Error_T ec = Error_OK;
    Cards_T cards( oson_this_db  );
    Card_info_T c_info = cards.get(card_id, ec);
    if (ec)return ec;
    
    if (c_info.uid != d->m_uid || c_info.foreign_card != FOREIGN_YES){
        slog.ErrorLog("not own card! card uid: %lld  foreign_card: %d", (long long)c_info.uid, (int)c_info.foreign_card);
        return Error_operation_not_allowed;
    }
    
    ////////////////////////////////////////////////////
    Activate_info_T act_s, act_i;
    act_s.kind     = act_s.Kind_foreign_card_register ;
    act_s.code     = act_code ;
    act_s.other_id = card_id  ;
    
    Activate_table_T act_table( oson_this_db  );
    act_i  = act_table.info( act_s ) ;
    
    if ( ! act_i.id ){
        return Error_not_found ;
    }
    /////////////////////////////////////////////
    c_info.foreign_card = FOREIGN_NO ;
    cards.card_edit( card_id, c_info ) ;
    
    
    act_table.deactivate( act_i.id );
    
    
    return Error_OK ;
}

static Error_T api_notification_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );

    Notify_list_T list;
    Users_notify_T users_n( oson_this_db  );
    
    users_n.notification_list(d->m_uid, list);
    users_n.notification_readed(d->m_uid);
    
    //////////////////////////////////////////////////////////////
    d->m_writer << b2(list.count) << b2(list.list.size());
    
    for(const Notify_T& e :  list.list)
    {
        d->m_writer << b4(e.id) << e.msg << e.ts << e.type ;
    }
    //////////////////////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_set_avatar(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    std::string avatar =  reader.readAsString( reader.readByte2() );
    Users_T users( oson_this_db  );
    return users.set_avatar(d->m_uid, avatar);
}

static Error_T api_decode_merchant_qr(api_pointer_cref d, const std::string & qr_token)
{
    SCOPE_LOGD(slog);
    if (! boost::algorithm::starts_with(qr_token, "merchant:")){
        slog.ErrorLog("not valid token");
        return Error_internal;
    }
    std::string qr_data = qr_token.substr(qr_token.find(':' ) + 1 );
    
    uint32_t merchant_id = string2num(qr_data);
    uint32_t tp = 3;//merchant-qr
    /////////////////////////////////////////////
    d->m_writer << b2( tp ) << b4( merchant_id ) ; 
    ///////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_decode_business_qr(api_pointer_cref d, const std::string& qr_token)
{
    SCOPE_LOGD(slog);
    if (! boost::algorithm::starts_with(qr_token, "business-bill:") )
    {
        slog.ErrorLog("not valid token");
        return Error_internal;
    }
    std::string qr_data = qr_token.substr( qr_token.find( ':' ) + 1 );
    
    uint64_t bill_id = string2num(qr_data);
    
    Error_T ec;
    Bills_T bills( oson_this_db  );
    Bill_data_T data = bills.get(bill_id, ec);
    
    if (ec)return ec;
    
    uint32_t tp = 2; // business-bill-qr
  
    //////////////////////////////////////////////
    d->m_writer << b2(tp)       << b8(data.id) << b8(data.uid) << b8(data.amount) << b4(data.merchant_id) 
                << b2(data.status) << data.phone  << data.add_ts  << data.comment ; 
   ////////////////////////////////////////////
    
    return Error_OK;

}

static Error_T api_decode_qr(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    std::string qr_token = reader.readString();
    
    if (qr_token.empty()){
        slog.ErrorLog("qr token is empty!");
        return Error_SRV_data_length;
    }
    
    if ( boost::algorithm::starts_with(qr_token, "business-bill:") ){
        return api_decode_business_qr(d, qr_token);
    }
    if ( boost::algorithm::starts_with(qr_token, "merchant:")){
        return api_decode_merchant_qr(d, qr_token);
    }
    
    User_info_T search;
    search.qr_token = qr_token;

    Users_T users( oson_this_db  );
    User_info_T user_info;
    Error_T ec = users.info(search, user_info);
    if (ec != Error_OK)
        return ec;
    
    uint32_t tp = 1; //client-bill 1-version;
    ////////////////////////////////////////////////////
    d->m_writer << b2(tp) << user_info.phone;
    ///////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_user_qr_code(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    int32_t link = 0;
    reader >> r4(link);
    
    Users_T users( oson_this_db  ) ;

    std::string qr_location = users.qr_code_location( d->m_uid ) ;
    
    if ( qr_location.empty() || ! oson::utils::file_exists(qr_location) ){
        Error_T ec = users.generate_img(d->m_uid, qr_location);
        if (ec ) return ec;
    }


    if ( ! link ) { // old API 
        std::string img = oson::utils::load_file_contents( qr_location );

        img = oson::utils::encodebase64(img);
        
        
        /////////////////////////////////////////////////////////
        d->m_writer << b4(link) << img;
        ////////////////////////////////////////////////////////
        return Error_OK;
    }
    
    //link != 0 : A new api
    std::string www_name = "user_qr_" + to_str(d->m_uid);
    std::string www_location = "/var/www/oson.client/img/" + www_name + ".png";
    if (! oson::utils::file_exists(www_location)){
        //a copy it.
        std::ifstream ifs(qr_location, std::ios_base::in | std::ios_base::binary );
        std::ofstream ofs(www_location, std::ios_base::out | std::ios_base::binary );
        
        ofs << ifs.rdbuf();
    }
    
    std::string qr_link = oson::utils::bin2hex(www_name) + ".png";
    d->m_writer << b4(link) << qr_link ;
    return Error_OK ;
    
}

static Error_T api_send_notify(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////
    std::string phone   = reader.readAsString(reader.readByte2());
    std::string message = reader.readAsString(reader.readByte2());
    //////////////////////////////////////////////////////////////////
    Users_notify_T users_n(  oson_this_db  );
    return users_n.notification_send(phone, message, MSG_TYPE_MESSAGE);
}

static Error_T api_transaction_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );

    Transaction_list_T list;
    Transactions_T tr( oson_this_db  );
    Transaction_info_T tr_search;
    tr_search.uid    = d->m_uid;
    tr_search.status = TR_STATUS_SUCCESS;
    
    Sort_T sort(0, 0, Order_T( 8, 0, Order_T::DESC ) );// 8-timestamp
    
    Error_T error = tr.transaction_list(tr_search, sort, list);
    if (error != Error_OK)
        return error;

    /////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    
    for(const Transaction_info_T& t : list.list)
    {
        d->m_writer << b8(t.id) << b8(t.amount) << t.srccard << t.dstcard << t.srcphone << t.dstphone << t.ts ; 
    }
    ///////////////////////////////////////////////////////
    
    return Error_OK;
}

namespace 
{
class transaction_perform_session: public std::enable_shared_from_this< transaction_perform_session >
{
private:
    typedef transaction_perform_session self_type;
    
    api_pointer d;
    Transaction_info_T tr_info;
    ///////////////
    Card_info_T src_card_info ;
    Card_info_T dst_card_info ;
    User_info_T user_info     ;
    Bank_info_T bank_info     ;
    
   
    
    int32_t tr_old_status;
public:
    transaction_perform_session(api_pointer d, Transaction_info_T tr_info ) ;
    
    ~transaction_perform_session();
    
    void async_start() ;
private:
    void update_trans();
    void start() ;
    
    Error_T handle_start();
    
    void on_dst_card_p2p_info_eopc(std::string hpan, const EOPC_p2p_info_T& info, Error_T ec) ;
    
    void on_dst_card_p2p_info(std::string hpan, const EOPC_p2p_info_T& info, Error_T ec) ;
   
    Error_T check_user_info() ;
    
    Error_T find_src_card_info() ;
    
    void on_dst_card_new_eopc(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec) ;
    
    void on_dst_card_new(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & out, Error_T ec) ;
    
    Error_T find_dst_card_info();
    
    Error_T check_double_trans();
    
    Error_T check_bank_limit();
    
    
    void on_src_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
    
    void on_src_card_info(const std::string & id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
    
    Error_T handle_src_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec);
    
    void on_trans_pay_eopc(const EOPC_Debit_T&in, const EOPC_Tran_T& tran, Error_T ec);
    
    void on_trans_pay(const EOPC_Debit_T& in, const EOPC_Tran_T& tran, Error_T ec);
    Error_T handle_on_trans_pay(const EOPC_Debit_T& in, const EOPC_Tran_T& tran, Error_T ec);
    
    void on_p2p_credit_eopc(const EOPC_Credit_T & credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec);
    
    void on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec);
    
    Error_T handle_on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec);
    
    void on_trans_reverse_eopc(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T ec) ;
};
} // end noname namespace

transaction_perform_session::transaction_perform_session(api_pointer d, Transaction_info_T tr_info )
: d(d), tr_info(tr_info) 
{
    SCOPE_LOGF(slog);
     tr_old_status = 0;
}

transaction_perform_session::~transaction_perform_session()
{
    SCOPE_LOGF(slog);

    if (tr_info.id != 0)
    {
        try{ update_trans(); }catch(std::exception & e){}
    }

    if ( d->m_ssl_response ){
        slog.WarningLog("~transaction_perform_session_some_on_wrong_get !");
        d->send_result(Error_internal);
    }
}

void transaction_perform_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this()));
}

void transaction_perform_session::update_trans()
{
    Transactions_T table( oson_this_db  ) ;
    table.transaction_edit(tr_info.id, tr_info);
}
void transaction_perform_session::start()
{
    SCOPE_LOGD(slog);
 
    Error_T ec = handle_start();

    //@note that, if there ec == Error_async_processing, d->send_result ignored it.
    return d->send_result(ec);

}

Error_T transaction_perform_session::handle_start()
{
    SCOPE_LOG(slog);

    Error_T ec = Error_OK;


    Transactions_T trans( oson_this_db  );

    int64_t trn_id = tr_info.id;

    if (trn_id == 0) 
    {
        tr_info.status = TR_STATUS_REGISTRED;
        trn_id  = trans.transaction_add(tr_info);
        tr_info.id = trn_id;
    }

    tr_old_status = tr_info.status;

    tr_info.status = TR_STATUS_ERROR ; // candidate!!!
    //1. user info
    ec = check_user_info();
    if (ec) return ec;



    //2. src card
    ec = find_src_card_info();
    if (ec) return ec;

    //3. dst card
    //@note: there may be asynchronously.
    ec = find_dst_card_info();
    if (ec) return ec;


    tr_info.status = tr_old_status;
    update_trans();

    tr_info.status = TR_STATUS_ERROR; //candidate!!!



    //4. check probably double trans
    ec = check_double_trans();
    if (ec) return ec;


    //5. check bank limit
    ec = check_bank_limit();
    if (ec) return ec;



    //6. get src EOPC card info
    oson_eopc -> async_card_info(this->src_card_info.pc_token, std::bind(&self_type::on_src_card_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) ;

    return Error_async_processing ;
}

void transaction_perform_session::on_dst_card_p2p_info_eopc(std::string hpan, const EOPC_p2p_info_T& info, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_dst_card_p2p_info, shared_from_this(), hpan, info, ec) ) ;
}

void transaction_perform_session::on_dst_card_p2p_info(std::string hpan, const EOPC_p2p_info_T& info, Error_T ec)
{
    SCOPE_LOGD(slog);
    if (ec != Error_OK )
    {
        slog.ErrorLog("Can't take p2p info.");
        return d->send_result(ec);
    }

    std::string card_type = boost::algorithm::trim_copy(info.card_type);

    if ( ! boost::algorithm::iequals(card_type, "private") ) 
    {
        slog.ErrorLog("Unsupported card-type: '%s'", card_type.c_str());
        return d->send_result( Error_card_blocked ) ;
    }

    slog.DebugLog("Supported card-type( '%s' ) for %s pan", card_type.c_str(),  hpan.c_str());


//        if ( d->m_uid == 17 || d->m_uid == 7 ) // a test developer uid's.
//        {
//            slog.WarningLog("Test destination pan: %s.  by-by", hpan.c_str());
//            return ;
//        }
//        
    //continue where we stop:  handle_start step 4.
    tr_info.status = TR_STATUS_IN_PROGRESS;
    update_trans();

    tr_info.status = TR_STATUS_ERROR; //candidate!!!

    //4. check probably double trans
    ec = check_double_trans();
    if (ec) return d->send_result( ec ) ;



    //5. check bank limit
    ec = check_bank_limit();
    if (ec) return d->send_result( ec ) ;



    //6. get src EOPC card info
    oson_eopc -> async_card_info(this->src_card_info.pc_token, std::bind(&self_type::on_src_card_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) ;
}

Error_T transaction_perform_session::check_user_info()
{
    SCOPE_LOG(slog);
    Error_T ec = Error_OK ;        
    Users_T users( oson_this_db  );
    this->user_info  = users.get( d->m_uid, ec);

    if (ec) {
        tr_info.status_text = "User not found. uid: " + to_str( d->m_uid);
        return ec;
    }
    //@Note: why check with m_user_info???
    if (user_info.tr_limit > 0 && tr_info.amount > user_info.tr_limit) {
        tr_info.status_text = "amount limit exceeded" ;
        slog.WarningLog("Transaction limit, m_user_info.tr_limit: %d, tr_info.amount = %lld", user_info.tr_limit, tr_info.amount);
        return Error_limit_exceeded;
    }

    return Error_OK ;
}

Error_T transaction_perform_session::find_src_card_info()
{
    SCOPE_LOG(slog);

    slog.DebugLog("srccard: %s,  srccard-id: %ld, srcphone: %s", tr_info.srccard.c_str(), tr_info.srccard_id, tr_info.srcphone.c_str());

    const bool src_card_by_pan     = ! ( tr_info.srccard.empty()  ) ;
    const bool src_card_by_id      = ! ( tr_info.srccard_id == 0  ) ;
    const bool src_card_by_primary = ! ( tr_info.srcphone.empty() ) ;

    if ( src_card_by_id )
    {
        slog.DebugLog( "srccard_id = %lld", tr_info.srccard_id );

        if ( is_bonus_card( oson_this_db  , tr_info.srccard_id) )
        {
            slog.ErrorLog(" =========== This is bonus card! =========== ");
            Users_notify_T users_n( oson_this_db  );
            users_n.send2( d->m_uid, 
                    "С бонусной карты нельзя осуществлять перевод денежных средств на другие карты, есть только возможность оплаты товаров и услуг.",
            MSG_TYPE_BONUS_MESSAGE, 7 * 1000 ) ;

            tr_info.status_text = "This is bonus card!";

            return Error_transaction_not_allowed;
        }

        Card_info_T c_search;

        c_search.id         = tr_info.srccard_id;
        c_search.is_primary = PRIMARY_UNDEF;
        c_search.uid        = 0;

        Cards_T cards( oson_this_db  );
        Error_T error = cards.info(c_search, src_card_info);
        
        if (error != Error_OK) 
        {
            tr_info.status_text = "card not found : card-id = " + to_str(tr_info.srccard_id);
            return error;
        }

        tr_info.srccard = src_card_info.pan ;
    }
    else if ( src_card_by_pan ) 
    {
        slog.DebugLog("srccard: '%s'", tr_info.srccard.c_str());
        // Get card by pan
        Card_info_T c_search;
        c_search.uid        = d->m_uid;//@Note: why uid assign this->m_uid?
        c_search.pan        = tr_info.srccard;
        c_search.is_primary = PRIMARY_UNDEF;
        Cards_T cards( oson_this_db  );

        Error_T error = cards.info( c_search, src_card_info ) ;
        if (error != Error_OK)
            return error;

        tr_info.srccard     = src_card_info.pan;
        tr_info.srccard_id  = src_card_info.id;

    }
    else  if ( src_card_by_primary )
    {
        // Get primary card of user
        Card_info_T c_search;
        c_search.uid        = d->m_uid;
        c_search.is_primary = PRIMARY_YES;
        Cards_T cards( oson_this_db  );

        Error_T error = cards.info(c_search, src_card_info);
        if (error != Error_OK){
            tr_info.status_text = "There no primary card for this user!";
            return error;
        }

        tr_info.srccard = src_card_info.pan;

        tr_info.srccard_id = src_card_info.id ;

    } 
    else 
    {
        slog.ErrorLog("src undefieed.");
        tr_info.status_text = "There no source card!";
        return Error_src_undefined;
    }

    if( src_card_info.admin_block || src_card_info.user_block ) 
    {
        tr_info.status_text = "source card is blocked!" ;
        slog.WarningLog("card is blocked by %s", ( src_card_info.admin_block ? "admin" : "user") );
        return Error_card_blocked;
    }

   return Error_OK;
}

void transaction_perform_session::on_dst_card_new_eopc(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec)
{
    d->m_io_service->post(std::bind(&self_type::on_dst_card_new, shared_from_this(), in, card, ec ) ) ;
}

void transaction_perform_session::on_dst_card_new(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & out, Error_T ec)
{
    SCOPE_LOGD(slog);
    if (ec) 
    {
        slog.WarningLog("Error: %d", (int)ec);
        return ;
    }

    int64_t dst_uid = 0;

    DB_T::statement st( oson_this_db  );

    const std::string & owner_phone = out.phone;
    const std::string & pc_token    = out.id;

     if ( ! owner_phone.empty() ){
         st.prepare("SElECT id FROM users WHERE phone = " + escape(owner_phone));
         
         if (st.rows_count() == 1)
         {
             st.row(0) >> dst_uid;
         }
     }

     if (dst_uid == 0)
     {
        st.prepare("SELECT uid FROM cards WHERE pc_token = " + escape(pc_token) + " ORDER BY uid ASC LIMIT 7 " );
         if (st.rows_count() == 1) // if multiple users has this token, but there no owner user, so needn't put dst_uid!
         {
             st.row(0) >> dst_uid ;
         }
     }

     if (dst_uid != 0)
     {
         tr_info.dst_uid = dst_uid;
         st.prepare( "UPDATE transaction SET dst_uid = " + escape(dst_uid) + " WHERE id = " + escape(tr_info.id) ) ;
     }
}

Error_T transaction_perform_session::find_dst_card_info()
{
    SCOPE_LOG(slog);

    const bool dst_card_by_phone = ! ( tr_info.dstphone.empty() ) && valid_phone(tr_info.dstphone);
    const bool dst_card_by_card  = ! ( tr_info.dstcard.empty() ) && valid_card_pan(tr_info.dstcard);


    Error_T ec = Error_OK ;

    if (dst_card_by_card){
        slog.DebugLog("dst_card_by_card. dstcard = %s", tr_info.dstcard.c_str());

        dst_card_info.pan    = tr_info.dstcard;
        dst_card_info.expire = tr_info.dstcard_exp;

        oson::backend::eopc::req::card in = { dst_card_info.pan , expire_date_rotate( dst_card_info.expire ) } ;


        oson_eopc -> async_card_new( in,  std::bind(&self_type::on_dst_card_new_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) ;

        oson_eopc -> async_p2p_info( dst_card_info.pan, std::bind(&self_type::on_dst_card_p2p_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) ) ;

        return Error_async_processing ;
    }
    else if(dst_card_by_phone){
        slog.DebugLog("dst_card_by_phone. phone: %s", tr_info.dstphone.c_str());
        // Put to phone
        Users_T users( oson_this_db  ) ;
        User_info_T u_info = users.info(tr_info.dstphone, ec);

        if (ec != Error_OK){
            tr_info.status_text = "There no user with phone " + escape(tr_info.dstphone);
            return ec;
        }

        Card_info_T c_search;
        c_search.uid = u_info.id;
        c_search.is_primary = PRIMARY_YES;
        Cards_T cards( oson_this_db  );

        ec = cards.info(c_search, dst_card_info);
        if (ec != Error_OK) {
            tr_info.status_text = "There no primary card of destination user!";
            return ec;
        }
        tr_info.dst_uid = u_info.id;
        tr_info.dstcard = dst_card_info.pan;
    }
    else 
    {
        tr_info.status_text = "Destination card undefined!";
        slog.ErrorLog("line  %d: destination card undefined.", __LINE__);
        return Error_dst_undefiend;
    }

    if (dst_card_info.id != 0) 
    {
        if(dst_card_info.admin_block || dst_card_info.user_block) {
            tr_info.status_text = "Destination card blocked!";
            return Error_card_blocked;
        }
        tr_info.dst_uid = dst_card_info.uid;
    }

    return Error_OK ;
}


Error_T transaction_perform_session::check_double_trans()
{
    SCOPE_LOG(slog);     
    std::string query = "SELECT id FROM transaction WHERE \n"
                        " ( id <> "         + escape(tr_info.id)      + " ) \n"
                        " AND ( uid = "     + escape(d->m_uid)        + " ) \n"
                        " AND ( srccard = " + escape(tr_info.srccard) + " ) \n" 
                        " AND ( dstcard = " + escape(tr_info.dstcard) + " ) \n" 
                        " AND ( amount  = " + escape(tr_info.amount)  + " ) \n"
                        " AND ( ts >= Now() - interval '10 minute' )        \n"
                        " ORDER BY id DESC LIMIT 1 " ;

   DB_T::statement st( oson_this_db  );
   st.prepare(query);
   bool has = st.rows_count() == 1 ;// possible values only 0 or 1, because LIMIT 1 on query.

   if (has){
       int64_t previous_id = 0;
       st.row(0) >> previous_id;
       slog.ErrorLog("Seems Double transaction. previous-id: %lld", (long long)previous_id);
       tr_info.status_text = "Seems double transaction! previous-id: " + to_str(previous_id);
       return Error_operation_not_allowed;
   }

   return Error_OK ;

}

Error_T transaction_perform_session::check_bank_limit()
{
    SCOPE_LOG(slog);

    const std::string dst_card_pan = dst_card_info.pan;

    Bank_T bank( oson_this_db  );
    Bank_info_T b_search ; 
    b_search.bin_code =  dst_card_pan  ;//now bin parsed within bank search.
    
    Error_T ec = Error_OK ;

    this->bank_info = bank.info(b_search, ec);

    if (ec != Error_OK)
    {
        return ec;
    }
    
    if (bank_info.status != Bank_info_T::ES_active ) 
    {
        slog.WarningLog("Inactive bank!!!");
        return Error_bank_not_supported ;
    }
    
    const Bank_info_T::Constraint constrant = bank_info.check_amount(tr_info.amount);

    if ( constrant != Bank_info_T::C_OK ){

        slog.WarningLog("line: %d , amount %lld , min_limit: %lld, max_limit: %lld", __LINE__, tr_info.amount, bank_info.min_limit, bank_info.max_limit);
        tr_info.status_text = "Bank limit exceeded. ";

        return  (constrant == Bank_info_T::C_LESS_MIN_LIMIT )
                ? Error_min_limit
                : Error_max_limit;
    }

    return Error_OK ;
}


void transaction_perform_session::on_src_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_src_card_info, shared_from_this(), id, eocp_card, ec));
}

void transaction_perform_session::on_src_card_info(const std::string & id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    SCOPE_LOGD(slog);
    ec = handle_src_card_info(id, eocp_card, ec);
    return d->send_result(ec);
}

Error_T transaction_perform_session::handle_src_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    SCOPE_LOG(slog);

    if (ec != Error_OK){
        tr_info.status_text = "card info source card failed.";
        slog.ErrorLog("line: %d , eocp_card_info error: %d", __LINE__, (int)ec);
        return ec;
    }

    if (src_card_info.owner_phone.size() > 0){
         if (eocp_card.phone.size() > 0 && src_card_info.owner_phone != eocp_card.phone){
             slog.ErrorLog("owner-phone: %s, eocp-card-phone: %s", src_card_info.owner_phone.c_str(), eocp_card.phone.c_str());
             tr_info.status_text = "source card owner phone changed";
             return Error_card_owner_changed;
         }
    }
    else
    {
         if (eocp_card.phone.size() > 0){
            src_card_info.owner_phone = eocp_card.phone;
            Cards_T cards( oson_this_db  );
            cards.card_edit_owner(src_card_info.id,  eocp_card.phone);
         }
    }

    if (eocp_card.status != VALID_CARD) {
        slog.DebugLog("Perform transaction whith not valid card %d", eocp_card.status);
        tr_info.status_text = "source card blocked. status: " + to_str(eocp_card.status);
        return Error_card_blocked;
    }

    tr_info.comission =  bank_info.commission(tr_info.amount); //tr_info.amount * bank_info.rate / 10000; // 10000 - 1% is 100

    if(eocp_card.balance < tr_info.amount + tr_info.comission) {
        slog.ErrorLog("Not enough amount. card balance: %d tiyin, amount: %d tiyin.", eocp_card.balance, tr_info.amount );
        tr_info.status_text  = "Not enough amount";
        return Error_not_enough_amount;
    }

    slog.DebugLog("\nstart eopc transaction.\n");

     ////////////////////////////////////////////////////////////////////////////////////////
     //            Debit
     ////////////////////////////////////////////////////////////////////////////////////////
    int64_t const trn_id = tr_info.id; 
    EOPC_Debit_T in;

    in.amount      = tr_info.amount + tr_info.comission;
    in.cardId      = src_card_info.pc_token;
    in.merchantId  = bank_info.merchantId;
    in.terminalId  = bank_info.terminalId;
    in.port        = bank_info.port;
    in.ext         = num2string(trn_id);
    in.stan        = make_stan(trn_id);


    oson_eopc ->async_trans_pay( in,  std::bind(&self_type::on_trans_pay_eopc, shared_from_this(),  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 )   );

  
    return Error_async_processing ;
}

void transaction_perform_session::on_trans_pay_eopc(const EOPC_Debit_T&in, const EOPC_Tran_T& tran, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_trans_pay, shared_from_this(), in, tran, ec));
}

void transaction_perform_session::on_trans_pay(const EOPC_Debit_T& in, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD(slog);
    
    ec = handle_on_trans_pay(in, tran, ec);
    return d->send_result(ec);
}

Error_T transaction_perform_session::handle_on_trans_pay(const EOPC_Debit_T& in, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOG(slog);

    tr_info.eopc_id = tran.refNum;

    if (ec != Error_OK) {
        tr_info.status_text += ", trans pay failed.";
        return ec;
    }


    if( !tran.status_ok()  || tran.resp != 0) {
        slog.ErrorLog("line %d Error in EOPC while perform transaction", __LINE__);
        tr_info.status_text += ", trans pay error. tran.status: " + tran.status + ", tran.resp = " + to_str(tran.resp);
        return Error_eopc_error ;
    }
     ////////////////////////////////////////////////////////////////////////////////////////
     //            Credit
     ////////////////////////////////////////////////////////////////////////////////////////
    int64_t const trn_id = tr_info.id; 

    EOPC_Credit_T credit      ;
    credit.amount = tr_info.amount;
    credit.merchant_id = bank_info.merchantId;
    credit.terminal_id = bank_info.terminalId;
    credit.port        = bank_info.port;
    credit.ext = num2string(trn_id);

    //@Note simplify it, WTF
    if (dst_card_info.pc_token.length() != 0) {
        credit.card_id = dst_card_info.pc_token;
    }
    else if (tr_info.dstcard.length() != 0) { // if dst user not exist
        credit.card.pan    = tr_info.dstcard;
        credit.card.expiry = expire_date_rotate( tr_info.dstcard_exp ) ;
    }
    else {
        slog.WarningLog("Can't find dst card or user");
        tr_info.status_text += ", Can't find dst card or user!" ;
        return ec;
    }

    int old_status = tr_info.status;
    tr_info.status_text = "CREDIT SUCCESS" ;
    tr_info.status = TR_STATUS_SUCCESS;
    update_trans();
    tr_info.status = old_status;


    oson_eopc ->async_p2p_credit( credit, std::bind(&self_type::on_p2p_credit_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
     
    return Error_async_processing ;
}

void transaction_perform_session::on_p2p_credit_eopc(const EOPC_Credit_T & credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_p2p_credit, shared_from_this(), credit, trans_dest, ec));

}

void transaction_perform_session::on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec)
{
    SCOPE_LOGD(slog);
    
    ec = handle_on_p2p_credit(credit, trans_dest, ec);
    return d->send_result(ec);


}

Error_T transaction_perform_session::handle_on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& trans_dest, Error_T ec)
{
    SCOPE_LOG(slog);

    if (ec != Error_OK ||   trans_dest.resp != 0 ) {
        tr_info.status = TR_STATUS_REVERSE ;
        tr_info.status_text += ", p2p credit failed";
        slog.ErrorLog("line %d Error in EOPC while perform transaction", __LINE__);

        oson_eopc -> async_trans_reverse( tr_info.eopc_id, std::bind(&self_type::on_trans_reverse_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) ;

        return Error_eopc_error ;
    }

    tr_info.eopc_id += "," + trans_dest.refNum;

    tr_info.status = TR_STATUS_SUCCESS;
    tr_info.status_text += ", DEBIT SUCCESS";
    update_trans();

     ////////////////////////////////////////////////////////////////////////////////////////
     //          Notify the user
     ////////////////////////////////////////////////////////////////////////////////////////

    Users_notify_T  users_n( oson_this_db  );
    std::string msg = "Вы получили " + to_money_str(tr_info.amount, ',' ) + " сум.";
    users_n.send2(tr_info.dst_uid, msg, MSG_TYPE_TRANSACTION, 0);

    if(tr_info.temp_token.size() > 0) {
         Transactions_T trans( oson_this_db  ) ;
         trans.temp_transaction_del(tr_info.temp_token);
    }

    Transaction_info_T tr_info_copy = tr_info;
    tr_info.id = 0;//no more update it.
    
    d->m_io_service->post( std::bind(&api_bonus_earns, d, tr_info_copy ) ) ;

    return Error_OK ;
}

void transaction_perform_session::on_trans_reverse_eopc(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD(slog);
    if (ec){
        slog.ErrorLog("can't reverse it! ref-num: %s", ref_num.c_str() ) ;
    }
}



static Error_T api_transaction_perform(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    Transaction_info_T tr_info;

    reader >> r8(tr_info.amount)   >> r8(tr_info.srccard_id) >> r8(tr_info.dstcard_id) >> r2(tr_info.srccard) >> r2(tr_info.dstcard) >> r2(tr_info.dstcard_exp)
           >> r2(tr_info.srcphone) >> r2(tr_info.dstphone)   >> r2(tr_info.temp_token) ;

    slog.DebugLog("transaction amount: %ld", tr_info.amount);

    tr_info.uid = d->m_uid;
    
    typedef transaction_perform_session session;
    typedef std::shared_ptr< session > session_ptr;

    session_ptr s = std::make_shared< session > (d, tr_info ) ;
    s->async_start();

    return Error_async_processing;
}

//@Note: this does 3 different functionality, separate it!!!
static Error_T api_transaction_request(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /*********************************************************/
    Transaction_info_T tr_info;
    reader >> r2(tr_info.srcphone) >> r2(tr_info.dstcard) >> r8(tr_info.amount);
   /*************************************************************/
    Error_T ec = Error_OK;
    
    tr_info.dst_uid = d->m_uid;

    if(  ! tr_info.srcphone.empty()  ) {
        // We have person who response to transaction
        Users_T users( oson_this_db  );
        User_info_T u_info = users.info(tr_info.srcphone, ec);
        if ( ec )return ec;

        User_info_T self_info = users.get( d->m_uid, ec);
        if ( ec ) return ec;
        
        if (  tr_info . dstcard . empty() ) { // main card
            Cards_T cards( oson_this_db  );
            Card_info_T card_search, card_info;
            card_search.uid = d->m_uid;
            card_search.is_primary = PRIMARY_YES;
            ec = cards.info(card_search, card_info);
            if (ec != Error_OK)
                return ec;
            tr_info.dstcard = card_info.pan ;
        }
        
        tr_info.dstphone  = self_info.phone;
        tr_info.uid       = u_info.id;
        tr_info.status    = TR_STATUS_REQUEST_DST;
 
        Transactions_T transaction( oson_this_db  );
        /*int64_t trn_id =*/ transaction.transaction_add(tr_info);
        
        std::string msg = "Пополните мою карту, пожалуйста (" + self_info.phone + ")";
        Users_notify_T users_n( oson_this_db  );
        return users_n.notification_send(tr_info.uid, msg, MSG_TYPE_REQUEST);
    } else {
        // We don't know who confirm this request
        // Create temporary request and generate token for them
        Transactions_T temp_tr( oson_this_db  );
        Error_T error = temp_tr.temp_transaction_add(tr_info);
        if (error != Error_OK)
            return error;

        std::string token;
        error = temp_tr.temp_transaction_info(temp_tr.m_transaction_id, token);
        if (error != Error_OK)
            return error;

        /////////////////////////////////////////
        d->m_writer << token ;
        ////////////////////////////////////////
    }

    return Error_OK;
}

static Error_T api_transaction_check(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    const std::string token = reader.readString(); 
    
    if (token.empty())
    {
        slog.ErrorLog("Token is empty.");
        return Error_SRV_data_length;
    }
    
    Transactions_T tr( oson_this_db  );
    Transaction_info_T info;
    Error_T ec = tr.temp_transaction_info(token, info);
    if(ec != Error_OK) {
        return ec;
    }

    Users_T users( oson_this_db  );
    User_info_T u_info =  users.get(info.dst_uid, ec);
    
    if(ec != Error_OK) {
        return ec;
    }
    info.dst_name = u_info.name;
    if( info.dstcard.empty() ) {
        info.dstphone = u_info.phone;
    }

    ///////////////////////////////////////////////////////
    d->m_writer << b8(info.amount) << info.dst_name << info.dstcard << info.dstphone ;
    ///////////////////////////////////////////////////    
    
    return Error_OK;
}


namespace
{
    
class card_info_by_phone_session: public std::enable_shared_from_this< card_info_by_phone_session >
{
private:
    typedef card_info_by_phone_session self_type;
    api_pointer d;
    std::string phone;
    
    //////////////
    User_info_T info;
    Card_info_T c_info;
public:
    
    card_info_by_phone_session(api_pointer d, const std::string& phone) ; 
    
    ~card_info_by_phone_session() ; 
    
    void async_start() ; 
    
private:
    void start() ;
     
    
    Error_T handle_start() ;
    
    void on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
    void on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
    
    Error_T handle_on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
};
} // end noname namespace


card_info_by_phone_session::card_info_by_phone_session(api_pointer d, const std::string& phone)
 : d(d), phone(phone)
 {
     SCOPE_LOGD(slog);
 }

 card_info_by_phone_session::~card_info_by_phone_session()
 {
     SCOPE_LOGD(slog);
     if (d->m_ssl_response ){
         slog.WarningLog("~card_info_by_phone_session_some_is_wrong !");
         d->send_result(Error_internal);
     }
 }

 void card_info_by_phone_session::async_start()
 {
     d->m_io_service->post( std::bind(&self_type::start, shared_from_this())) ;
 }

 void card_info_by_phone_session::start()
 {
     SCOPE_LOGD(slog);
     Error_T ec = handle_start();
     return d->send_result(ec);
 }

 Error_T card_info_by_phone_session::handle_start()
 {
     SCOPE_LOG(slog);

     Error_T ec;
     Users_T users( oson_this_db  );
     this->info  = users.info(phone, /*out*/ ec );

     if ( ec != Error_OK){
         slog.ErrorLog("can't get info from users by phone. error = %d", (int)ec);
         return ec;
     }
     int64_t uid = info.id ;

     Cards_T cards( oson_this_db  );
     Card_info_T c_search;
     c_search.uid = uid;
     c_search.is_primary = PRIMARY_YES;

     ec = cards.info(c_search, this->c_info);
     if ( ec != Error_OK){
         slog.ErrorLog("Can't find primary card. error = %d", (int)ec);
         return ec;
     }

     std::string pc_token = c_info.pc_token ;

     oson_eopc -> async_card_info( pc_token, std::bind(&self_type::on_card_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;

     return Error_async_processing ;
 }

 void card_info_by_phone_session::on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
 {
     d->m_io_service->post(std::bind(&self_type::on_card_info, shared_from_this(), id, eocp_card, ec));
 }

 void card_info_by_phone_session::on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
 {
     SCOPE_LOGD(slog);
     ec= handle_on_card_info(id, eocp_card, ec);
     return d->send_result(ec);
 }

 Error_T card_info_by_phone_session::handle_on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
 {
     SCOPE_LOG(slog);


     if (ec){
         slog.ErrorLog("eocp_card_info failed. error = %d", (int)ec);
         return ec;
     }

     uint32_t bank_rate;

     std::string owner = eocp_card.fullname;

     int64_t balance = eocp_card.balance;
     std::string pan    = eocp_card.pan ;


     std::string expire = expire_date_rotate(eocp_card.expiry);

     if (c_info.owner_phone.size() > 0){
         if (eocp_card.phone.size() > 0 && c_info.owner_phone != eocp_card.phone){
             slog.ErrorLog("owner-phone: %s, eocp-card-phone: %s", c_info.owner_phone.c_str(), eocp_card.phone.c_str());
             return Error_card_owner_changed;
         }
     }
     else{
         if (eocp_card.phone.size() > 0){
             c_info.owner_phone = eocp_card.phone;
             Cards_T cards( oson_this_db  ) ;
             cards.card_edit_owner(c_info.id, eocp_card.phone);
         }
     }

    // get bank information.
     {
         Bank_T bank( oson_this_db   );
         Bank_info_T b_search;  
         b_search.bin_code = pan ; // within Bank::info extracted bin from pan.
         Bank_info_T bank_info = bank.info(b_search, ec);

         if (ec) return ec;
         
         if (bank_info.status != Bank_info_T::ES_active ) 
         {
             slog.WarningLog("Bank is disabled!") ;
             return Error_bank_not_supported ;
         }
         
         bank_rate = bank_info.rate;
     }
     ///////////////////////////////////////////////////////////////
     d->m_writer << b4(bank_rate) << owner << expire << b8(balance);
     /////////////////////////////////////////////////////////////
     return Error_OK ;
 }

static Error_T api_card_info_by_phone(api_pointer_cref d, const std::string& phone)
{
    SCOPE_LOG(slog);
    typedef card_info_by_phone_session session;
    typedef std::shared_ptr< session> session_ptr;

    session_ptr s = std::make_shared< session > (d, phone);

    s->async_start();

    return Error_async_processing ;
}

namespace 
{
class card_info_by_pan_session: public std::enable_shared_from_this< card_info_by_pan_session >
{
private:
    typedef card_info_by_pan_session self_type;
    
    api_pointer d;
    std::string pan;
public:
    card_info_by_pan_session(api_pointer d, std::string pan) ;
    
    ~card_info_by_pan_session() ;
    
    void async_start() ;

private:
    void start() ;
    
    void on_p2p_info_eopc(const std::string&pan, const EOPC_p2p_info_T & info, Error_T ec) ;
    
    void on_p2p_info(const std::string& pan, const EOPC_p2p_info_T& info, Error_T ec) ;
    
    Error_T handle_p2p_info(const std::string& pan, const EOPC_p2p_info_T& info, Error_T ec) ;
    
};
    
} // end noname namespace


card_info_by_pan_session::card_info_by_pan_session(api_pointer d, std::string pan)
: d(d), pan(pan)
{
    SCOPE_LOGD(slog);
}

card_info_by_pan_session::~card_info_by_pan_session()
{
    SCOPE_LOGD(slog);

    if ( d->m_ssl_response ) //something go wrong
    {
        slog.WarningLog("~card_info_by_pan_session_something_go_wrong!");
        d->send_result(Error_internal);
    }
}
void card_info_by_pan_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this())) ;
}


void card_info_by_pan_session::start()
{
    SCOPE_LOGD(slog);
    oson_eopc -> async_p2p_info(  pan, std::bind(&self_type::on_p2p_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) ) ;
}

void card_info_by_pan_session::on_p2p_info_eopc(const std::string&pan, const EOPC_p2p_info_T & info, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_p2p_info, shared_from_this(), pan, info, ec)) ;
}

void card_info_by_pan_session::on_p2p_info(const std::string& pan, const EOPC_p2p_info_T& info, Error_T ec)
{
    ec = handle_p2p_info(pan, info, ec);
    d->send_result(ec);
    //d->m_io_service->post( std::bind( &api_data::send_result, d, ec ) );
}

Error_T card_info_by_pan_session::handle_p2p_info(const std::string& pan, const EOPC_p2p_info_T& info, Error_T ec)
{
    SCOPE_LOGD(slog);

    if( ec )
    {
        slog.ErrorLog("eopc_p2p_info error: %d", ec);
        return  ec   ;
    }
    //output data
    uint32_t bank_rate;
    std::string owner,expire;
    int64_t balance= 0 ;

    //output data.
    owner = info.owner;
    expire = info.exp_dt;

    // get bank information.
    {
        Bank_T bank( oson_this_db   );
        Bank_info_T b_search; 
        b_search.bin_code = pan ; // within bank info extracted bin code from the pan.
        Bank_info_T bank_info = bank.info(b_search, ec )  ;

        if ( ec ) return   ec  ;


        if (bank_info.status != Bank_info_T::ES_active)
        {
            slog.WarningLog("this bank not active!");
            return  Error_bank_not_supported ;
        }
        bank_rate = bank_info.rate;
    }
    ////////////////////////////////////////////////////////////////
    d->m_writer << b4(bank_rate) << owner << expire << b8(balance);
    /////////////////////////////////////////////////////////
    return  Error_OK ;
}


static Error_T api_card_info_by_pan(api_pointer_cref d, const std::string& pan)
{
    SCOPE_LOG(slog);
    typedef card_info_by_pan_session session;
    typedef std::shared_ptr< session > session_ptr;

    session_ptr s = std::make_shared< session > (d, pan);
    s->async_start();

    return Error_async_processing ;
}

static Error_T api_card_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_USER_LOGIN(d, reader);

    std::string pan   = reader.readAsString(reader.readByte2()); 
    std::string phone = reader.readAsString( reader.readByte2() ) ;
    
    if ( ! pan.empty() ){
        return api_card_info_by_pan(d, pan);
    } else {
        return api_card_info_by_phone(d, phone);
    }
}

static Error_T api_transaction_bill_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    Transactions_T tr( oson_this_db  );
    
    Transaction_info_T search;
    search.uid     = d->m_uid;
    search.status  = TR_STATUS_REQUEST_DST;
    
    Transaction_list_T list;
    
    tr.transaction_list(search, Sort_T(), list);
    
    Bank_list_T bank_list;
    
    if ( ! list.list.empty() ){ // most users does not have transactions, so don't needed get bank info : a little optimization!
        Bank_T banks( oson_this_db  );
        Bank_info_T search;
        Sort_T sort;

        banks.list(search, sort, bank_list); // get all bank list.
    }
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size()) ;

    for(const Transaction_info_T& info : list.list )
    {
        int32_t rate = 0;
        for(const Bank_info_T& binfo : bank_list.list){
            if (boost::starts_with( info.dstcard, binfo.bin_code ) ) { //@Note: this implementation MUST move to bank.cpp
                rate = binfo.rate;
                break;
            }
        }
        d->m_writer << b8(info.id) << b8(info.amount) << b4(rate) << info.dstphone << info.ts ;
    }
    ////////////////////////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_transaction_bill_accept(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    if ( 0 == d->m_uid){
        slog.ErrorLog("Unauthorized access!");
        return Error_login_failed;
    }
    Transaction_info_T tr_info;
    ///////////////////////////////////////////////////////
    tr_info.id = reader.readByte8();

    const uint16_t accepted = reader.readByte2();
    const uint64_t card_id  = reader.readByte8();
    /////////////////////////////////////////////////////
    bool const success_accepted = accepted != 0;

    Transactions_T tr( oson_this_db  );
    Error_T ec = tr.info(tr_info.id, tr_info);
    if (ec)return ec;
    
    if ( ! success_accepted )
    {
        tr_info.status = TR_STATUS_DECLINE;
        return tr.bill_accept(tr_info);
    }

    tr_info.srccard_id = card_id ;
    
    {
        typedef transaction_perform_session  session     ;
        typedef std::shared_ptr< session > session_ptr ;
        
        session_ptr s = std::make_shared< session > (d, tr_info );
        
        s->async_start();
        
        return Error_async_processing ;
    }
}

static Error_T api_incoming_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    if ( 0 == d->m_uid){
        slog.ErrorLog("Unauthorized access!") ; 
        return Error_login_failed;
    }
    
    Sort_T sort(0, 0, Order_T(2, 0, Order_T::ASC));
    std::string sort_order, sort_field;
    reader >> r4(sort.offset) >> r2(sort.limit) >> r2(sort_order) >> r2(sort_field);
    
    boost::trim(sort_order);
    boost::trim(sort_field);
    //determine sort_order and sort_field
    if ( ! sort_order.empty()  )
    {
        if (  boost::iequals(sort_order, "ASC")  || sort_order == "0") 
        {
            sort.order.order = Order_T::ASC;
        } 
        else if ( boost::iequals(sort_order, "DESC") || sort_order == "1" ) 
        {
            sort.order.order = Order_T::DESC;
        } else {
            slog.WarningLog("sort_order invalid: %.*s", ::std::min<int>(128, sort_order.size()), sort_order.c_str());
        }
    }  
    
    if (! sort_field.empty() )
    {
        if ( std::all_of(sort_field.begin(), sort_field.end(), ::isdigit ) && sort_field.size() < 9 )
        {
            int value = string2num(sort_field);
            if (value >= 1 && value <= 8 ) 
            {
                sort.order.field = value;
            } else {
                slog.WarningLog("sort_field very large(or 0): %s", sort_field.c_str());
            }
        } else {
            
            typedef const char* pchar;
            
            static const pchar FIELDS [8] = {  "type", "id", "ts", "amount", "srccard", "srcphone", "commision", "status" } ;
            bool fnd = false;
            for(int i = 0; i< 8; ++i){
                if (boost::iequals(sort_field, FIELDS[i])){
                    sort.order.field = 1 + i ;//break;
                    fnd = true;
                    break;
                }
            }
            
            //exception situation.
            if (boost::iequals(sort_field, "commission"))
            {
                sort.order.field = 7;
                fnd = true;
            }
            
            if (!fnd)
            {
                slog.WarningLog("sort_field invalid: %.*s", ::std::min<int>(128, sort_field.size()), sort_field.c_str());
            }
        }
    }
    
    Transactions_T tr( oson_this_db  );
    Incoming_list_T list = tr.incoming_list(d->m_uid, sort);
    
    /////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());

    for(const Incoming_T& e : list.list)
    {
        d->m_writer << b2(e.type)   << b8(e.amount)  << b8(0 /*e.commision*/ ) 
                    << b4(e.status) << e.ts  << e.src_phone    << e.src_card  << e.dst_card;
    }
    ////////////////////////////////////////////////////////////////
    return Error_OK;
    
}

static Error_T api_outgoing_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );

    Sort_T sort(0, 0, Order_T(3, 0, Order_T::ASC ) ) ;
    std::string sort_order, sort_field;
    Outgoing_T srch;
    
    reader >> r4(sort.offset ) >> r2(sort.limit) >> r2(sort_order) >> r2( sort_field ) ;
    reader >> r4(srch.type) >> r4(srch.merchant_id) >> r4(srch.status) >> r8(srch.card_id) >> r2(srch.from_date) >> r2(srch.to_date) ;
    
    boost::trim(sort_order);
    boost::trim(sort_field);
    
    //determine sort_order and sort_field
    if ( ! sort_order.empty() )
    {
        if (  boost::iequals(sort_order, "ASC")  || sort_order == "0") 
        {
            sort.order.order = Order_T::ASC;
        } 
        else if ( boost::iequals(sort_order, "DESC") || sort_order == "1" ) 
        {
            sort.order.order = Order_T::DESC;
        } else {
            slog.WarningLog("sort_order invalid: %s", sort_order.c_str());
        }
    }
    
    if (! sort_field.empty() )
    {
        if ( std::all_of(sort_field.begin(), sort_field.end(), ::isdigit ) && sort_field.size() < 9 )
        {
            int value = string2num(sort_field);
            if (value >= 1 && value <= 14 ) 
            {
                sort.order.field = value;
            } else {
                slog.WarningLog("sort_field very large(or 0): %s", sort_field.c_str());
            }
        } else {
            
            typedef const char* pchar;
            
            static const pchar FIELDS [14] = 
            {  
                "type", "id", "ts", "amount", "merchant_id", "login", "dstcard", "dstphone",
                "commision", "status", "srccard", "oson_tr_id", "card_id", "bearn"
            } ;
            bool fnd = false;
            for(int i = 0; i< 14; ++i){
                if (boost::iequals(sort_field, FIELDS[i])){
                    sort.order.field = 1 + i ;//break;
                    fnd = true;
                    break;
                }
            }
            
            //exception situation.
            if (boost::iequals(sort_field, "commission"))
            {
                sort.order.field = 9;
                fnd = true;
            }
            
            
            if (!fnd)
            {
                slog.WarningLog("sort_field invalid: %.*s", ::std::min<int>(128, sort_field.size()), sort_field.c_str());
            }
        }
    }
    
    Transactions_T tr( oson_this_db  );
    Outgoing_list_T o_list  = tr.outgoing_list(d->m_uid, srch, sort );
    ///////////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(o_list.count) << b4( o_list.list.size() );

    for(const Outgoing_T & o : o_list.list)
    {
        d->m_writer << b2(o.type)   << b4(o.merchant_id) << b8(o.amount)     << b8(o.commision)
                    << b4(o.status) << o.login           << o.dst_card       << o.dstphone    << o.ts 
                    << o.pan        << b8(o.id)          << b8(o.oson_tr_id) << b8(o.card_id) << b8(o.bearn);
    }
    
    return Error_OK;
}

static Error_T api_detail_info_a_newscheme(api_pointer_cref d, uint32_t merchant_id, uint64_t oson_tr_id)
{
    SCOPE_LOGD(slog);
    
    Error_T ec;
    Purchase_T purch( oson_this_db  );
    
    Purchase_details_info_T info;
    
    ec = purch.get_detail_info(oson_tr_id, info) ;
    if (ec)
        return ec;
    
    Merchant_T merch( oson_this_db  );
    Merchant_info_T  merchant_info = merch.get(merchant_id, /*OUT*/ec);
    
    Merch_acc_T acc;
    Merchant_api_T merch_api( merchant_info, acc);
    
    Merch_trans_status_T trans_status;
    try{
        merch_api.make_detail_info(info.json_text, trans_status);
    }
    catch(std::exception& e)
    {
        slog.WarningLog("Can't parse ( what: %s) json_text: %.*s", e.what(),  ::std::min<int>(1024, info.json_text.length() ), info.json_text.c_str());
    }
    std::vector<Merchant_field_T>resp_fields;
    //FILL resp_fields.
    {
        Merchant_field_list_T fields;
        Merchant_field_T search;
        search.usage = search.USAGE_INFO;
        search.merchant_id = merchant_id;
        Sort_T sort(0, 0 ,  Order_T(1, 4, Order_T::ASC)); // order by mid, position
        
        merch.fields(search, sort, fields);
        
        resp_fields.swap(fields.list);
        
        size_t money_rate_idx = size_t( -1 );
        
        for(size_t i = 0; i < resp_fields.size(); ++i)
        {
            const std::string& param_name = resp_fields[i].param_name;
            if ( trans_status.kv.count(param_name) > 0 )
            {
                resp_fields[i].value = trans_status.kv.at(param_name);
            }
            else if (param_name == "rate")
            {
                resp_fields[i].value = to_str(merchant_info.rate);
            }
            else if (param_name == "rate_money")
            {
                resp_fields[i].value = to_money_str(merchant_info.rate_money);
                money_rate_idx = i;
            }
            else
            {
                slog.WarningLog("fID: %d, field[%d]  param_name: '%s'  NOT FOUND\n", resp_fields[i].fID, i, param_name.c_str());
            }
        }
        
        if (money_rate_idx != size_t(-1))
            resp_fields.erase(resp_fields.begin() + money_rate_idx);
    }//end fill.

    //////////////////////////////////////////////////
    uint32_t version = 2;// 2- a new version
    d->m_writer << b2(version) << b2(resp_fields.size()) << b8(oson_tr_id) << b4(merchant_id);
    for(const Merchant_field_T& field : resp_fields)
    {
        d->m_writer << b2(field.position)     << b4(field.fID)        << b2(field.type)       << b2(field.input_digit) 
                    << b2(field.input_letter) << b2(field.min_length) << b2(field.max_length) << b4(field.parent_fID) 
                    << field.label            << field.prefix_label   << b4(field.usage)      << field.value;
    } 
    //////////////////////////////////////////////////
    return Error_OK ;
}


static Error_T api_detail_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    uint64_t const oson_tr_id = reader.readByte8();
    
    //1. check a new scheme or not
    do
    {
        std::string query = "SELECT merchant_id FROM purchases WHERE oson_tr_id = " + escape(oson_tr_id);
        DB_T::statement st( oson_this_db  );
        st.prepare(query);
        
        if (st.rows_count() != 1)
            break;
        
        uint32_t merchant_id = st.get_int(0,0);
       
        query = "SELECT api_id from merchant where id = " + escape(merchant_id);
        
        st.prepare(query);
        
        if (st.rows_count() != 1)
            break;
        
        int32_t api_id = st.get_int(0,0);
        
        //@Note there added a new merchants.
        if ( ! ( merchant_identifiers::is_munis(merchant_id) ||
                api_id == merchant_api_id::mplat
               )
           )
        {
            break;
        }
        
        return api_detail_info_a_newscheme(d, merchant_id, oson_tr_id);
    }while(0);
    
    
    Purchase_T purch( oson_this_db  );
    
    Purchase_details_info_T info;
    if (Error_T ec = purch.get_detail_info(oson_tr_id, info) ) {  return ec;  }
    
    ////////////////////////////////////////////////////////
    uint32_t tp = 1; // old version
    d->m_writer << b2( tp ) << info.json_text ;
    /////////////////////////////////////////////////////
    
    return Error_OK;
}


static Error_T api_merchant_group_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN_NOT_EMPTY(d, reader);

    std::string if_mod  ;
    reader >> r2(if_mod );
    //////////////////////////////////////////////////////////////////
    Users_T users( oson_this_db  );
    int const lang =  users.user_language( d->m_uid ); //LANG_all;
   
    /////////////////////////////////////////////////////////////////////////////////////////////
    
    Merchant_group_T groups( oson_this_db  );
    
    std::vector< Merch_group_info_T > group_list;
    groups.list( 0, group_list );
    
    /////////////////////////////////////////////////////////////////////////////
    d->m_writer << b2(group_list.size());
    for(const Merch_group_info_T& g : group_list)
    {
        bool const sel_uzb = (lang == LANG_uzb &&  ! g.name_uzb.empty() );
        //general name
        const std::string& name =(sel_uzb) ? g.name_uzb : g.name ;
        
        d->m_writer << b4(g.id) << b4(g.icon_id) << name << g.icon_path;
    }
    
    return Error_OK;
}

static Error_T api_client_top_merchant_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////////////////////////////
    OSON_PP_USER_LOGIN(d, reader);
    uint32_t const count    = reader.readByte2() ;
    ////////////////////////////////////////////////////////////////////////
    Users_T users( oson_this_db  );
    int  const lang = users.user_language( d->m_uid );
    ///////////////////////////////////////////////////////////////
    enum{ DEFAULT_COUNT = 5, DEFAULT_MAX_COUNT = 20 } ;
    uint32_t const limit = oson::utils::clamp<uint32_t>(count, DEFAULT_COUNT, DEFAULT_MAX_COUNT);//  count < 5 ==> 5, count > 20 ==> 20, otherwise count
    slog.DebugLog("count: %u, limit: %u", static_cast<unsigned>(count), static_cast<unsigned>(limit));
    
    Sort_T sort(0, limit, Order_T(1, 0, Order_T::ASC) ); // order by id
    
    Merchant_list_T list;
    Merchant_T merchant( oson_this_db  );

    Error_T ec = merchant.top_list(d->m_uid, sort, list);
    if(ec != Error_OK)
        return ec;

    // GET all fields.  sorted by merchant-id.
    std::vector<Merchant_field_T> fields;
    {
        Merchant_field_T search;
        search.usage = Merchant_field_T::USAGE_REQUEST;
        Sort_T sort(0, 0 , Order_T(1, 4, Order_T::ASC ) ); // order by mid, position
        
        Merchant_field_list_T flist;
        ec = merchant.fields(search, sort, flist);
        if(ec != Error_OK)
            return ec;
        fields.swap(flist.list);
    }
    
    
    //GET all fields_data   sorted by fid.
    std::vector<Merchant_field_data_T> field_data_list;
    {
        Sort_T sort(0, 0, Order_T(2, 0, Order_T::ASC ) );//no limit, order by fID
        
        Merchant_field_data_T search; // no search parameters.
        Merchant_field_data_list_T flist;
        ec = merchant.field_data_list(search, sort, flist );

        if(ec != Error_OK) {
            return ec;
        }
        
        field_data_list.swap(flist.list);
    }
    
    size_t const n_field = fields.size();
    size_t i_field = 0 ;
    /////////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) <<  b4(list.list.size()) ;

    for(const Merchant_info_T& merch : list.list )
    {
        d->m_writer << b4(merch.id) << b4(merch.group)      << b8( merch.min_amount) << b8(merch.max_amount)
                    << merch.name   << b4(/*view-mode*/1 ) << b4(merch.status)      << b4(merch.rate) ;

        
        //============= add fields: =======================================
        while ( i_field < n_field && fields[i_field].merchant_id < merch.id )
            ++i_field;
        
        size_t i_field_end = i_field;
        while(i_field_end < n_field && fields[i_field_end].merchant_id == merch.id) 
            ++i_field_end;
        
        d->m_writer << b2( i_field_end - i_field ) ;
        for( ; i_field < i_field_end; ++i_field)
        {
            const Merchant_field_T& field =  fields[ i_field ];

            bool const label_is_uzb =  ( lang == LANG_uzb && ! field.label_uz1.empty() );
            std::string const& label_lang = (label_is_uzb) ? field.label_uz1 : field.label ;

            
            d->m_writer << b2(field.position)     << b4(field.fID)        << b2(field.type)       << b2(field.input_digit)
                        << b2(field.input_letter) << b2(field.min_length) << b2(field.max_length) << b4(field.parent_fID)
                        << label_lang             << field.prefix_label ;

            if (field.type == M_FIELD_TYPE_INPUT) 
            {
                typedef std::vector< Merchant_field_data_T>::iterator citer;
                typedef std::pair< citer, citer> citer_ii;
                
                citer_ii  ii = std::equal_range(field_data_list.begin(), field_data_list.end(), field.fID, fid_comparator() );
                
                d->m_writer << b2(ii.second - ii.first);
                for( ; ii.first != ii.second ; ++ii.first)
                {
                    d->m_writer << (*ii.first).prefix ;
                }//end field-data list
            }//end if
        }
        // ================== end for fields ================================================
    }//end for merchant list
    
    return Error_OK ;
}

static Error_T api_merchant_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    ///////////////////////////////////////////////////////////////////////
    OSON_PP_USER_LOGIN_NOT_EMPTY(d, reader);
    /////////////////////////////////////////////////
    uint32_t group_id    = reader.readByte4() ;
    uint32_t merchant_id = reader.readByte4() ;
    std::string id_list  = reader.readString();
    uint32_t showall     = reader.readByte4() ;
    //////////////////////////////////////////////////////////////
    Users_T users( oson_this_db  ) ;
    int lang = users.user_language( d->m_uid ) ;
    //////////////////////////////////////////////////////////////
    
    {
        std::set< int32_t > ss_id;
        long long u = 0;
        for(size_t i = 0, n = id_list.size(); i != n && ss_id.size() < 100; ++i)
        {
            char c = id_list[i];
            if (c == ',' || c == ';' ) // a separator
            {
                if (u != 0){
                    ss_id.insert(u);
                    
                }
                u = 0;
            } else if( c >='0' && c <='9' ) {
                u = u * 10 + (c - '0' ) ;
                if (u >= INT32_MAX ){
                    break;
                }
                if (i + 1 == n ) // the last digit
                {
                    ss_id.insert( u ) ;
                }
            } else { 
                break;
            }
        }
        
        id_list.clear();
        
        if (ss_id.size() == 1 ) //only one
        {
            merchant_id = *ss_id.begin();
        } 
        else if (ss_id.size() > 1 ) { 
            char comma = ' ' ;
            for(int32_t id : ss_id)
            {
                id_list += comma;
                id_list += escape(id);
                comma = ',';
            }  
        }
    }
    ////////////////////////////////////////////////////////////
    
    Merchant_list_T list;
    Merchant_T merchant( oson_this_db );
    Merchant_info_T search;
    search.status = MERCHANT_STATUS_SHOW;
    search.group  = group_id;
    search.id     = merchant_id ;
    search.id_list = id_list;
    
    bool is_test = ( 
                       d->m_uid == 7   || 
                       d->m_uid == 17  || 
                    // d->m_uid == 84  || 
                       d->m_uid == 85  ||
                       d->m_uid == 87  || 
                       d->m_uid == 91  ||
                    // d->m_uid == 81  ||
                       false 
                    )  // test for mplat
                    ;
    
    if ( is_test || showall)
        search.status = 0;
    
    Sort_T sort(0, 0, Order_T(1, 0, Order_T::ASC));// no limit, order by id 
    
    Error_T ec = merchant.list(search, sort, list);
    if(ec != Error_OK)
        return ec;
    
    // GET all fields.  sorted by merchant-id.
    std::vector<Merchant_field_T> fields;
    {
        Merchant_field_T search;
        search.usage = Merchant_field_T::USAGE_REQUEST;
        Sort_T sort(0, 0, Order_T(1, 4, Order_T::ASC));

        Merchant_field_list_T flist;
        ec = merchant.fields(search, sort, flist);
        if(ec != Error_OK)
            return ec;
        fields.swap(flist.list);
    }

    //GET all fields_data   sorted by fid.
    std::vector<Merchant_field_data_T> field_data_list;
    {
        Sort_T sort(0, 0, Order_T(2, 1, Order_T::ASC));//no limit, order by fID, id 
        
        Merchant_field_data_T search; // no search parameters.
        Merchant_field_data_list_T flist;
        ec = merchant.field_data_list(search, sort, flist );

        if(ec != Error_OK) {
            return ec;
        }
        
        field_data_list.swap(flist.list);
    }
    
    std::map< int32_t , std::string > icon_link_map;
    {
        std::string query = " SELECT m.id,  i.path_hash FROM icons i INNER JOIN merchant m ON (m.icon_id = i.id ) " ;
        DB_T::statement st(oson_this_db);
        st.prepare(query);
        int rows = st.rows_count();
        for(int i = 0; i < rows; ++i)
        {
            int32_t id;
            std::string link;
            st.row(i) >> id >> link;
            icon_link_map[id] = link;
        }
    }
    
    size_t const n_field = fields.size();
    size_t i_field = 0 ;
    /////////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size()); 

    for(const Merchant_info_T& merch : list.list )
    {
        std::string icon_link = icon_link_map[merch.id ] ;
        
        d->m_writer << b4(merch.id)        << b4(merch.group )      << b8(merch.min_amount)  << b8(merch.max_amount) 
                    << merch.name          << b4(/*view-mode*/1)    << b4(merch.status)      << b4(merch.rate) 
                    << b4(merch.rate_money) << b4(merch.position)   << b4(0) << b4(0) /*<< b4(merch.filial_flag) << b4(merch.parent_id) */
                    << icon_link ;
        
        while ( i_field < n_field && fields[i_field].merchant_id < merch.id )
            ++i_field;
        
        size_t i_field_end = i_field;
        while(i_field_end < n_field && fields[i_field_end].merchant_id == merch.id) 
            ++i_field_end;
        
        d->m_writer << b2(i_field_end - i_field) ;

        for( ; i_field < i_field_end; ++i_field)
        {
            const Merchant_field_T& field =  fields[ i_field ];

            bool const label_is_uzb =  ( lang == LANG_uzb && ! field.label_uz1.empty()  );
            std::string const& label_lang = (label_is_uzb) ? field.label_uz1 : field.label ;

            d->m_writer << b2(field.position)     << b4(field.fID)        << b2(field.type )      << b2(field.input_digit)
                        << b2(field.input_letter) << b2(field.min_length) << b2(field.max_length) << b4(field.parent_fID)
                        << label_lang             << field.prefix_label ;
                        

            if (field.type == M_FIELD_TYPE_INPUT) 
            {
                typedef std::vector< Merchant_field_data_T>::iterator citer;
                typedef std::pair< citer, citer> citer_ii;
                
                citer_ii  ii = std::equal_range(field_data_list.begin(), field_data_list.end(), field.fID, fid_comparator() );
                
                d->m_writer << b2(ii.second - ii.first);
                
                for( ; ii.first != ii.second ; ++ii.first)
                {
                    d->m_writer << (*ii.first).prefix ;
                }//end field-data list
            }//end if
        }// end for fields 
    }//end for merchant list
    
    return Error_OK;
    
}

static Error_T api_merchant_info(api_pointer_cref d, ByteReader_T& reader)
{
    
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////
    const uint32_t merchant_id = reader.readByte4();
    const uint32_t fID         = reader.readByte4();
    const uint32_t usage       = reader.readByte4();
    /////////////////////////////////////////////////////////////
    
    Merchant_field_T search;
    search.merchant_id =  merchant_id;
    search.usage       =  (usage == 0 && fID == 0) ? Merchant_field_T::USAGE_REQUEST : usage; // by default search only purchase request fields.
    search.fID         =  fID ;
    
    Merchant_T merchant_table(  oson_this_db   );
    std::vector<Merchant_field_T> fields;
    {
        Merchant_field_list_T flist;
        Sort_T sort( 0 , 0, Order_T(1, 4, Order_T::ASC ));// order by mid, position
        Error_T error = merchant_table.fields(search, sort, flist);
        if(error != Error_OK)
            return error;
        fields.swap(flist.list);
    }
    Error_T ec = Error_OK ; 
    Merchant_info_T merch = merchant_table.get(merchant_id, ec);
    if (ec) return ec;
    ///////////////////////////////////////////////////////////////////////////////
     d->m_writer << b4(merch.id)         << b4(merch.group )      << b8(merch.min_amount)  << b8(merch.max_amount) 
                 << merch.name           << b4(/*merch.view_mode*/1)   << b4(merch.status)      << b4(merch.rate) 
                 << b4(merch.rate_money) << b4(merch.position)    << b4(/*merch.filial_flag*/0) << b4(/*merch.parent_id*/0) ;
       

    d->m_writer << b2(fields.size());
    
    for(const Merchant_field_T& field :  fields )
    {
        d->m_writer << b2(field.position)     << b4(field.fID)        << b2(field.type)       << b2(field.input_digit) 
                    << b2(field.input_letter) << b2(field.min_length) << b2(field.max_length) << b4(field.parent_fID) 
                    << field.label            << field.prefix_label   << b4(field.usage) ;

        if (field.type == M_FIELD_TYPE_INPUT) {

            Merchant_field_data_list_T  field_data_list;
            Merchant_field_data_T search;
            search.fID = field.fID;
            
            Sort_T sort(0,0);
            merchant_table.field_data_list(search, sort, field_data_list);

            d->m_writer << b2(field_data_list.list.size());

            for(const Merchant_field_data_T& data : field_data_list.list )
            {
                d->m_writer << data.prefix ;
            }
        } else {
            d->m_writer << b2( 0 ) ; // no field_data.
        }
    }
    //////////////////////////////////////////////////////////////////////////////
    
    return Error_OK;
}


static Error_T api_merchant_field(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN_NOT_EMPTY(d, reader);
    
    //////////////////////////////////////////////////////////////
    const uint32_t fid = reader.readByte4();
    const uint32_t parent_key = reader.readByte4();
    /////////////////////////////////////////////////////////////
    
    std::vector<Merchant_field_data_T> fields;

    Merchant_T merchant(  oson_this_db   );
    Sort_T sort(0,0);
    
    Merchant_field_data_list_T flist;
    
    Merchant_field_data_T search;
    
    search.fID        = fid;
    search.parent_key = parent_key;
    
    Error_T ec = merchant.field_data_list(search, sort, flist);
    if (ec != Error_OK)
        return ec;

    fields.swap(flist.list);
    ////////////////////////////////////////////////////////////
    d->m_writer << b2(fields.size());

    for(const Merchant_field_data_T& fd : fields)
    {
        d->m_writer << b4(fd.key) << fd.value << fd.prefix;
    }
    //////////////////////////////////////////////////////////
    return Error_OK ;

}

static Error_T api_merchant_ico(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////////////////////////////
    const uint32_t    id          = reader.readByte4();
    /////////////////////////////////////////////////////////////////////////
    std::string query = "SELECT icon_id FROM merchant WHERE id = "+ escape(id);
    
    DB_T::statement st(oson_this_db);
    st.prepare(query);
    if (st.rows_count() != 1 ) return Error_not_found;
    
    int64_t icon_id = 0;
    st.row(0) >> icon_id; 
    
    std::string img;
        
    namespace ic = oson::icons;

    ic::content content;
    ic::manager manager;
    int ret = manager.load_icon(icon_id, content);
    if (!ret)return Error_not_found;

    img = std::move(content.image);
    
    img = oson::utils::encodebase64(img);
    ///////////////////////////////////////////////////////////
    d->m_writer << b4(img);
    ///////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_bonus_merchant_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////////
    uint64_t id = reader.readByte8();
    
    uint32_t merchant_id = reader.readByte4();
    
    std::string start_date = reader.readAsString( reader.readByte2() );
    std::string end_date  = reader.readAsString( reader.readByte2() );
    
    uint32_t offset = reader.readByte4();
    uint32_t limit  = reader.readByte4();
    
    //-------------------------------------------------------------------------
    // ORDER BY start_ts DESC
    Sort_T sort( offset, limit , Order_T( 6, 0, Order_T::DESC ) ) ;
    
    Merchant_bonus_info_T search;
    search.id           = id;
    search.merchant_id  = merchant_id;
    //search.status       = 1; //active
    
    Merchant_bonus_list_T list;
    Merchant_T merch( oson_this_db   );
    
    Error_T ec = merch.bonus_list(search, sort, list);
    
    if (ec) return ec;
    /////////////////////////////////////////////////////////
    d->m_writer << b4(list.total_count) << b4(list.list.size());
    
    for(size_t i = 0;i < list.list.size(); ++i)
    {
        const Merchant_bonus_info_T& e = list.list[i];
        d->m_writer << b8(e.id)      << b4(e.merchant_id) << b8(e.min_amount)  <<  b8(e.bonus_amount) 
                    << b4(e.percent) << e.start_date      << e.end_date        <<  e.description 
                    << b4(e.status)  << e.longitude       << e.latitude        <<  b4(e.group_id) ;
    }
    ///////////////////////////////////////////////////////
    return Error_OK ;
}


static Error_T api_bonus_purchase_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    if (d->m_uid == 0)
    {
        slog.WarningLog("Login failed");
        return Error_login_failed;
    }
    /////////////////////////////////////////
    const int32_t mid = reader.readByte4();
    const int32_t off = reader.readByte4();
    const int32_t lim = reader.readByte4();
    //////////////////////////////////////////
    Sort_T sort(off, lim, Order_T(1, 0, Order_T::DESC));
    
    Purchase_search_T search;
    search.uid  = d->m_uid;
    search.mID  = mid;
    
    Purchase_T purch(  oson_this_db   );
    Purchase_list_T list;
  
    Error_T ec = purch.bonus_list_client(search, sort, list);
    
    if (ec) return ec;
    
    /////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size()) ;
    
    for(size_t i = 0; i < list.list.size(); ++i)
    {
        const Purchase_info_T& info = list.list[i];
        d-> m_writer << b8(info.id)      << b4(info.mID) << b8(info.amount) << b8(info.bearns) 
                     << b8(info.card_id) << info.login   << info.ts ;
    }
    ///////////////////////////////////////////////////////////
    
    return Error_OK ;
}

    
static Error_T api_purchase_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    Purchase_list_T list;
    Purchase_T purch(  oson_this_db   );
    Purchase_search_T pr_search;
    pr_search.uid = d->m_uid;
    Sort_T sort(0, 256, Order_T(1, 0, Order_T::DESC));//last 256!
    Error_T error = purch.list(pr_search, sort, list);
    if (error != Error_OK)
        return error;

    ///////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    
    for(const  Purchase_info_T & info : list.list)
    {
        d->m_writer << b8(info.id) << b4(info.mID) << b8(info.amount) << info.login << info.ts ;
    }
    ////////////////////////////////////////////////////
    return Error_OK;
}


static std::vector<Merchant_field_data_T> parse_merchant_field_data(ByteReader_T& reader)
{
    SCOPE_LOG(slog);
    size_t cnt = reader.readByte2();
    
    std::vector<Merchant_field_data_T> result(cnt);
    
    for(size_t i = 0; i != cnt; ++i)
    {
        result[i].fID    = reader.readByte4();
        result[i].value  = reader.readString();
        result[i].prefix = reader.readString();
        slog.InfoLog("fID: %ld, value: '%s' prefix: '%s' ", result[i].fID, result[i].value.c_str(), result[i].prefix.c_str());
    }
    return result;
}
namespace 
{

class purchase_info_session: public std::enable_shared_from_this< purchase_info_session >
{
private:
    api_pointer d;
    int32_t merchant_id;
    std::vector< Merchant_field_data_T> list;
    int64_t amount;

    /////////////////////
    User_info_T     m_user;
    Merchant_info_T m_merchant;
    Merch_trans_T   m_trans;
    Merch_trans_status_T m_trans_status;
public:
    
    typedef purchase_info_session self_type;
    
    explicit purchase_info_session(api_pointer d, int32_t merchant_id, std::vector< Merchant_field_data_T> list, int64_t amount);
    ~purchase_info_session();
    
    void async_start() ;
    
private:
    void start();
    Error_T start_ec();
    Error_T init_user();
    
    Error_T init_merchant();
    
    Error_T init_fields();
    
    Error_T merchant_info();
    
    Error_T async_merchant_info();
    
    void on_merchant_info_o(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec);
    
    void on_merchant_info(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec);
    
    Error_T build_fields();
};


purchase_info_session::purchase_info_session(api_pointer d, int32_t merchant_id, std::vector<Merchant_field_data_T> list, int64_t amount)
   : d(d), merchant_id(merchant_id), list(list), amount(amount)
{}

purchase_info_session::~purchase_info_session()
{}


void purchase_info_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this() ) ) ;
}

void purchase_info_session::start()
{
    SCOPE_LOGD(slog);

    Error_T ec = start_ec();
    if (ec != Error_async_processing)
    {
        d->send_result(ec);
    }
}

Error_T purchase_info_session::start_ec()
{
    SCOPE_LOGD(slog);
    //1. init users
    Error_T ec;
    
    ec = init_user();
    if (ec) return ec;
    
    ec = init_merchant();
    if (ec ) return ec;
    
    
    m_trans.uid        =  d->m_uid;
    
    ec = init_fields();
    if (ec) return ec;
    
    ec = merchant_info();
    
    if (ec) return ec;
    
    ec = build_fields();
    if (ec) return ec;
    
    return Error_OK ;
}

Error_T purchase_info_session::init_user()
{
    SCOPE_LOG(slog);
    Error_T ec = Error_OK ;
    Users_T users(   oson_this_db   ) ;
    m_user = users.get( d->m_uid, ec);
    if(ec != Error_OK ){
        m_user.phone = "998997776655"; // some random number
    }
    return Error_OK ;
}

Error_T purchase_info_session::init_merchant()
{
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    Merchant_T merchant_table(  oson_this_db   ) ;
    
    m_merchant = merchant_table.get( merchant_id, ec ) ;
    
    if( ec != Error_OK ) // probably not found
    {
        return ec;
    }
    
    return Error_OK;
}

Error_T purchase_info_session::init_fields()
{
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    m_trans.uid        =  d->m_uid ;
    m_trans.user_phone =  m_user.phone;
    m_trans.uid_lang   =  m_user.lang;
    m_trans.amount     =  amount; 
    
    Merchant_T merchant_table(  oson_this_db   ) ;
    
    std::vector<Merchant_field_T> fields = merchant_table.request_fields( merchant_id );

    std::map<std::string, std::string> merch_api_params;

    for(const Merchant_field_T& field : fields)
    {
        const uint32_t    fID         = field.fID; 
        const std::string param_name  = field.param_name;

        switch(field.type)
        {
            case M_FIELD_TYPE_INPUT:
            {
                std::vector<Merchant_field_data_T>::iterator it = std::find_if(list.begin(), list.end(), fid_comparator::fid(fID));
                if ( it == list.end() )
                {
                    slog.WarningLog("No in input");
                    continue;
                }
                std::string value = (*it).prefix + (*it).value;
                
                if ( ! oson::utils::valid_ascii_text(value ) ) 
                {
                    slog.ErrorLog("value is not ASCII formatted: '%s'", value.c_str());
                    return Error_parameters ;
                }
                
               // //@fix Eleketr customer-code
                if (merchant_id == merchant_identifiers::Electro_Energy || merchant_id == merchant_identifiers::Musor)
                    value = (*it).value;

                merch_api_params[param_name] = value; 

                if (m_trans.param.empty())
                    m_trans.param = value;
            }
            break;

            case M_FIELD_TYPE_LIST:
            {    
                std::vector<Merchant_field_data_T>::iterator it = std::find_if(list.begin(), list.end(), fid_comparator::fid(fID));
                if ( it == list.end() )
                {
                    slog.WarningLog("No in input");
                    continue;
                }

                std::string value = (*it).value;//input_fields[fID];

                Merchant_field_data_T fdata;
                fdata.fID = fID;                  
                fdata.key = string2num( value );
                Sort_T sort(0,0);
                ec = merchant_table.field_data_search(fdata, sort, fdata);

                if(ec == Error_OK) 
                {
                    if (fdata.service_id_check != 0)
                    {
                        m_trans.service_id = to_str( fdata.service_id_check ) ;
                    }
                    
                    if (merchant_identifiers::is_munis(merchant_id) )
                    {
                        value = to_str(fdata.service_id);
                    }

                    else if (merchant_id == merchant_identifiers::TPS_I ||
                             merchant_id == merchant_identifiers::TPS_I_direct
                            )
                    {
                        value = to_str(fdata.extra_id);
                    }
                }

                slog.DebugLog("param[%s]: '%s'", param_name.c_str(), value.c_str()) ;
                {
                    merch_api_params[ param_name ] = value;
                }
            }
            break;
            default:
                break;
        }
    }

//    if (merchant_identifiers::is_mplat(merchant_id) && d->m_uid > 0)
//    {
//        // get last purchase login this user to this merchant
//        std::string query = "SELECT ts, login FROM purchases WHERE ( status = 1  ) AND uid =  " 
//                           + escape(d->m_uid) + "   AND merchant_id = " + escape(merchant_id) + 
//                           " ORDER BY ts DESC LIMIT 1" ;
//        DB_T::statement st( oson_this_db  );
//        st.prepare(query);
//        if (st.rows_count() == 1){
//            std::string ts, login;
//            st.row(0) >> ts >> login;
//           merch_api_params["oson_last_purchase_login"] = login;
//           merch_api_params["oson_last_purchase_ts"]    = ts;
//        }
//    }
   
    
    // swap is very lightweight operation than assign
    m_trans.merch_api_params.swap( merch_api_params );

    slog.InfoLog("trans.service_id: %s ", m_trans.service_id.c_str());
    
    // Paynet
    if ( ! m_merchant.extern_service.empty() &&  m_trans.service_id.empty() ) {
    
        m_trans.service_id = m_merchant.extern_service;
        
        slog.InfoLog(" taken from merchant. trans.service_id: %s ", m_trans.service_id.c_str());
    }
    
    return Error_OK ;
}

Error_T purchase_info_session::merchant_info()
{
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    Merch_acc_T acc;

    Merchant_T merchant_table(  oson_this_db   ) ;

    merchant_table.acc_info( m_merchant.id, acc);
    
    if(m_merchant.api_id == merchant_api_id::ums ||
       m_merchant.api_id == merchant_api_id::paynet)
    {
        merchant_table.api_info(m_merchant.api_id, acc);
    }
    
    Merchant_api_T merch_api( m_merchant, acc);
    
    m_trans.transaction_id = merchant_table.next_transaction_id();
    m_trans.acc            = acc;
    m_trans.merchant       = m_merchant;
    
    const bool developer_mode = (d->m_uid == 7 || d->m_uid == 17 ) ;
    
    const bool has_async_version =  m_merchant.api_id == merchant_api_id::mplat              || 
                                    ( merchant_id == merchant_identifiers::Electro_Energy )  ||
                                    ( merchant_id == merchant_identifiers::Musor )           ||
                                    ( merchant_id == merchant_identifiers::Webmoney_Direct /*&& developer_mode*/ )  || 
                                    ( m_merchant.api_id == merchant_api_id::qiwi  ) ||
                                    ( merchant_id == merchant_identifiers::Ucell_direct /*&& developer_mode*/ ) ||
    
                                    false;
                                    
    if (has_async_version )
    {
        return async_merchant_info();
    }
    
    oson::ignore_unused(developer_mode);
    
    //GET response fields.
    ec = merch_api.get_info( m_trans, m_trans_status);

    if(ec != Error_OK) {
//        d->m_last_err_txt = m_trans_status.merch_rsp ;
        return ec;
    }

//    if ( m_trans_status.merchant_status != 0) 
//    {
//        slog.ErrorLog("merchant-status is not zero!");
//        return Error_merchant_operation;
//    }

    //convert JSON AND save it table.
    {
        Purchase_details_info_T detail_info;

        detail_info.oson_tr_id = m_trans.transaction_id;
        detail_info.trn_id     = 0;
        detail_info.json_text  = m_trans_status.kv_raw;

        Purchase_T purch( oson_this_db  );
        purch.add_detail_info(detail_info);
    }


    return Error_OK ;
}

Error_T purchase_info_session::async_merchant_info()
{
    SCOPE_LOG(slog);
    auto api = oson_merchant_api;
    api -> async_purchase_info(m_trans, std::bind(&self_type::on_merchant_info_o, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) );
    
    return Error_async_processing;
}
    
void purchase_info_session::on_merchant_info_o(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_merchant_info, shared_from_this(), trans, response, ec ) ) ;
}

void purchase_info_session::on_merchant_info(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec)
{
    SCOPE_LOGD(slog);
    if (ec)
    {
        slog.ErrorLog("ec = %d", (int)ec);
//        d->m_last_err_txt = response.merch_rsp ;
        return d->send_result(ec);
    }
    
    m_trans_status = response;
    
    ec = build_fields();
    
    return d->send_result(ec);
}

Error_T purchase_info_session::build_fields()
{
    SCOPE_LOG(slog);

    
    Merchant_T merchant_table(  oson_this_db   ) ;

    std::vector<Merchant_field_T>resp_fields;
    const auto & kv = m_trans_status.kv ;
    
    //FILL resp_fields.
    {
        Merchant_field_T search;

        search.usage = search.USAGE_INFO;

        search.merchant_id = merchant_id;

        Sort_T sort(0, 0, Order_T(1, 4, Order_T::ASC));

        Merchant_field_list_T flist;

        merchant_table.fields(search, sort, flist);

        resp_fields.swap(flist.list);
        
        for(size_t i = 0; i < resp_fields.size(); ++i)
        {
            const std::string& param_name = resp_fields[i].param_name;
            if (  kv.count(param_name) > 0 )
            {
                resp_fields[i].value =  kv.at(param_name);
            }
            else if (param_name == "rate")
            {
                std::string rate = to_str( m_merchant.rate/100.0, 2, true) + "%"  ;
                
                //@Note: Webmoney direct downside rate.
                if (  m_merchant.commission_subtracted() )   //merchant_identifiers::commission_subtracted(  m_merchant.id ) )
                    rate = "-" + rate;
                
                //if ( int64_t commission = m_merchant.commission( amount ) ) 
               // {
                    //rate += " ( " + to_str ( commission / 100.0, 2, true )  + " сум ) " ;
               // }
                
               // if (m_trans_status.merchant_status != 0 )
               // {
                //    slog.WarningLog("merchant_status not zero: %d", m_trans_status.merchant_status ) ;
               // }
                //else
                //{
                    resp_fields[i].value = rate ;
                //}
            }
            else if (param_name == "rate_money")
            {
                resp_fields[i].value = to_money_str( m_merchant.rate_money);
            }
            else
            {
                slog.WarningLog("fID: %d, field[%d]  param_name: '%s'  NOT FOUND\n", resp_fields[i].fID, i, param_name.c_str());
            }
        }
    }//end fill.

    //////////////////////////////////////////////////
    d->m_writer << b2(2) << b2(resp_fields.size()) << b8(m_trans.transaction_id) << b4(merchant_id);
    for( const Merchant_field_T& field  :  resp_fields )
    {
     d->m_writer << b2(field.position)      << b4(field.fID)        << b2(field.type)       << b2(field.input_digit)
                 << b2(field.input_letter)  << b2(field.min_length) << b2(field.max_length) << b4(field.parent_fID)    
                 << field.label             << field.prefix_label   << b4(field.usage)      << field.value ;
    } 
    //////////////////////////////////////////////////
    

    return Error_OK ;
}


} // end noname namespace

static Error_T api_purchase_info_new_scheme(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////////////////
    const uint32_t merchant_id = reader.readByte4();
    const uint32_t version     = reader.readByte4();
    std::vector<Merchant_field_data_T>  list  = parse_merchant_field_data(reader);
    const uint64_t amount      = reader.readByte8();
    /////////////////////////////////////////////////////////////////////////////////////
//    boost::ignore_unused(version);
    slog.InfoLog("version: %d", (int)version ) ;
    //if (d->m_uid == 7 || d->m_uid == 17)
    {
        auto session = std::make_shared< purchase_info_session > (d, merchant_id, list, amount ) ;
        session->async_start();
        return Error_async_processing;
    }
}

static std::vector<Merchant_field_data_T> parse_merchant_field_data(ByteReader_T& reader);

Error_T ss_bonus_earns(Purchase_info_T p_info);

static Card_info_T  api_validate_card_info(api_pointer_cref d, int64_t card_id, /*out*/ Error_T& ec )
{
    SCOPE_LOG(slog);
    Card_info_T  out ;
    ec = Error_OK ;
    Cards_T card( oson_this_db );

#define OSON_PP_EXIT_EC(e,r)    if ( (e) != Error_OK ) return ( r ) ;
    
    bool const bonus_card = card_id != 0 && is_bonus_card( oson_this_db, card_id);
    if ( ! bonus_card ) 
    {
        //@Note simplify it
        Card_info_T card_search ; 
        card_search.id = card_id;
        card_search.is_primary = (card_id != 0) ? PRIMARY_UNDEF : PRIMARY_YES ;
        card_search.uid = d->m_uid;
        ec = card.info(card_search, out );
        
        OSON_PP_EXIT_EC(ec, out)
        
        
        if ( out.foreign_card == FOREIGN_YES ) {
            slog.ErrorLog("Foreign card  %s   not activated yet!!" , out.pan.c_str());
            Users_notify_T users_n( oson_this_db  );
            users_n.send2( d->m_uid, "Чужая карта (" + out.pan + ") ещё не активировано!!", MSG_TYPE_PURCHASE_MESSAGE, 7*1000) ;
            ec  =  Error_card_foreign ;
            OSON_PP_EXIT_EC(ec, out)
        }
        
        if ( out.admin_block || out.user_block ) {
            slog.ErrorLog("card is blocked!") ;
            //Users_notify_T users_n( oson_this_db  );
            //users_n.send2( d->m_uid, "Ваша карта заблокирована в системе ОСОН.", MSG_TYPE_PURCHASE_MESSAGE, 7 * 1000 ) ;
            ec = Error_card_blocked;
            OSON_PP_EXIT_EC(ec, out);
        } 
        out.isbonus_card = 0; // no bonus card
    } else { // this card is bonus card
        
        Users_bonus_T users_b( oson_this_db  );
        
        User_bonus_info_T user_bonus_card;
        
        ec = users_b.bonus_info(d->m_uid, user_bonus_card);
        OSON_PP_EXIT_EC(ec, out)
         
        slog.DebugLog("bonus-balance: %lld, earns: %lld", (long long)user_bonus_card.balance, (long long)(user_bonus_card.earns));
        
        if (user_bonus_card.bonus_card_id != card_id)
        {
            slog.WarningLog("card id different.");
            ec =  Error_card_not_found;
            OSON_PP_EXIT_EC(ec, out)
        }
        
        if ( ! is_valid_expire_now(user_bonus_card.expire) )
        {
            slog.WarningLog("expire date is expired: %s", user_bonus_card.expire.c_str());
            ec =  Error_card_blocked;
            OSON_PP_EXIT_EC(ec, out)
        }
        
        if (user_bonus_card.block != 1) // 1 - active
        {
            slog.WarningLog("card is blocked for this user.");
            ec =  Error_card_blocked;
            OSON_PP_EXIT_EC(ec, out)
        }
        if (user_bonus_card.balance > user_bonus_card.earns){
            slog.WarningLog("balance > earns");
            ec =  Error_internal;
            OSON_PP_EXIT_EC(ec, out);
        }
        
        Card_bonus_info_T briogroup_card;
        ec = card.bonus_card_info(card_id, briogroup_card);
        OSON_PP_EXIT_EC(ec, out)

        out.id       = card_id                   ;
        out.uid      = d->m_uid                  ;
        out.pc_token = briogroup_card.pc_token   ;
        out.expire   = briogroup_card.expire     ;
        out.deposit  = user_bonus_card.balance   ;
        out.isbonus_card  = 1                         ; // this is bonus card
        out.pan      = user_bonus_card.pan       ;
    }
#undef OSON_PP_EXIT_EC
    ec = Error_OK;
    return out;
}


/*static*/ Error_T check_card_daily_limit(const User_info_T& user_info, const Card_info_T& card_info, int64_t pay_amount)
{
    SCOPE_LOG(slog);
    
    if (card_info.owner_phone == user_info.phone)
    {
        // no foreign card
        return Error_OK ;
    }
    
    DB_T::statement st( oson_this_db ) ;

    int64_t const daily_limit = card_info.daily_limit;
    ////////////////////////////////////////
    int64_t today_amount = pay_amount ;

    if (today_amount < daily_limit)
    {

        std::string query = " SELECT sum(amount) FROM purchases "
                            " WHERE  ( ts >= NOW() - interval '1 day' ) AND "
                            " uid = " + escape(user_info.id) + " AND "
                            " (status  = 1 OR status =  6)  AND "  //success + in-progress purchases.
                            " card_id = " + escape( card_info.id ) ;

        st.prepare(query);

        int64_t sum = 0;
        st.row(0) >> sum;
        
        today_amount += sum;
        slog.InfoLog("daily_limit = %ld, sum = %ld, amount: %ld", daily_limit, sum, pay_amount);
    }
    ///////////////////////////////////
    if (today_amount <= daily_limit)
        return Error_OK ;
    ///////////////////////////////////
    st.prepare("UPDATE cards SET foreign_card = '2' , daily_limit = LEAST(daily_limit + 30000000, 90000000 ) WHERE card_id = " + escape(card_info.id)) ;//2 - foreign_yes
    //1. send sms to an user.
    {
        std::string msg = "Дневной лимит чужой карты исчерпан.\n"
                          "Карта зарегистрирован на другой номер. Для дальнейшего использования  переактивируете  карту.\n"
                          "Карта: " + card_info.pan ;

        Users_notify_T notify_table { oson_this_db } ;
        
        notify_table.send2( user_info.id, msg, MSG_TYPE_BULK_MESSAGE, 5 * 1000 ) ;
    }

    //@Note: fix it and change error to  Error_card_daily_limit_exceeded
    return Error_not_enough_amount ;
}

namespace
{
    
class purchase_buy_session : public std::enable_shared_from_this< purchase_buy_session >
{
private:
    typedef purchase_buy_session self_type;
    
    api_pointer     d    ;
    int32_t merchant_id  ;
    int64_t amount       ;
    int64_t card_id      ;
    int64_t oson_tr_id   ;
    
    std::vector< Merchant_field_data_T > list;
    //////////////////////////////////////////
    Error_T ec_;
    /////////////////////////////////////
    int64_t           commission ;
    User_info_T       user_info  ;
    Card_info_T       card_info  ;
    Purchase_info_T   p_info     ;
    Merchant_info_T   merchant   ;
    Merch_acc_T       acc        ;
    Merch_trans_T     trans      ;
    Merchant_field_data_T pay_field ;
    
public:
    purchase_buy_session( api_pointer d, int32_t merchant_id, int64_t amount, int64_t card_id, int64_t oson_tr_id, std::vector< Merchant_field_data_T> list )
    ;
    
    ~purchase_buy_session();
    
    void async_start();

private:
    
    void update_purchase();
    void start();
    
    Error_T init_merchant();
    
    Error_T init_fields();
    
    Error_T init_card_info();
    
    Error_T init_user_info();
    
    Error_T create_purchase_info();
    
    void on_check_i(const Merch_trans_T& trans, const Merch_check_status_T& status,   Error_T ec);

    void on_check(const Merch_trans_T& trans, const Merch_check_status_T& status,   Error_T ec);
    
    Error_T merchant_check_login() ;
    
    void on_card_info_eopc(const std::string&id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec);
    void on_card_info(const std::string&, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) ;
    
    
    Error_T validate_eopc_card(const oson::backend::eopc::resp::card& eocp_card) ;
    
    Error_T trans_pay();
    
    void on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec ) ; 
    void on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec) ;
    
    Error_T merchant_pay() ;
    Error_T merchant_pay_start();
    
    void  on_merchant_pay_i(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) ;
    void  on_merchant_pay(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) ;
    
    void reverse_notify(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) ;
    void reverse_notify_i(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) ;
    
    
    void in_progress_notify();
    void in_progress_notify_i() ;
    
};
    
} // end noname namespace

purchase_buy_session::purchase_buy_session( api_pointer d, int32_t merchant_id, int64_t amount, int64_t card_id, int64_t oson_tr_id, std::vector< Merchant_field_data_T> list )
: d(d), merchant_id(merchant_id), amount(amount), card_id(card_id), oson_tr_id(oson_tr_id), list(list)
{
    SCOPE_LOGF_C(slog);
  
    ec_ = Error_OK ;

    trans.uid = d->m_uid;
}

purchase_buy_session::~purchase_buy_session()
{
    SCOPE_LOGF_C(slog);

    if (p_info.id != 0)
    {
        try
        { 
            update_purchase(); 
        } 
        catch( std::exception& e)
        {}
    }

    if ( d->m_ssl_response )
    {
        slog.WarningLog("~purchase_buy_session_some_is_wrong_go !" );
        d->send_result( ec_ );
    }
}

void purchase_buy_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this())) ;
}
 

void purchase_buy_session::update_purchase()
{
    Purchase_T purch( oson_this_db  ) ;
    purch.update(p_info.id, p_info);
}

void purchase_buy_session::start()
{
    SCOPE_LOGD_C(slog);

    //1. init merchant
    ec_ = init_merchant();
    if (ec_) return ;

    //2. init fields
    ec_ = init_fields();
    if (ec_) return ;


    //3. init card-info.
    ec_ = init_card_info();
    if (ec_) return ;

    //4. init user-info
    ec_ = init_user_info();
    if (ec_) return ;

    //5. create purchase row in table.
    ec_ = create_purchase_info();
    if (ec_) return ;

    //6.trans some values.
    trans.param   = pay_field.value;
    trans.amount  = amount;
  
    p_info.status = TR_STATUS_ERROR; // a candidate !

    //7. check login
    ec_ = merchant_check_login();

    if (ec_ == Error_async_processing ) { 
        ec_ = Error_OK ;
        return ;
    }

    if (ec_) return ;


    //8. async card_info
    using namespace std::placeholders;
    oson_eopc -> async_card_info( card_info.pc_token, std::bind(&self_type::on_card_info_eopc, shared_from_this(), _1, _2, _3 )) ;
}

Error_T purchase_buy_session::init_merchant()
{
    SCOPE_LOG(slog);
    Error_T ec = Error_OK ;

    Merchant_T merchant_table( oson_this_db );

    this->merchant = merchant_table.get(merchant_id, ec );

    if(ec != Error_OK)  return ec;

    if (merchant.status == MERCHANT_STATUS_HIDE) 
    {
        if ( ! allow_master_code( d->m_uid ) )
        {
            slog.ErrorLog("Disabled merchant!!!. status = %d", (int)merchant.status);
            return Error_operation_not_allowed;
        }

        slog.WarningLog("Disabled merchant.");

    }
    
    ec = merchant.is_valid_amount(  amount )  ;
    
    if (ec) return ec;
    
    merchant_table.acc_info(merchant.id, this->acc );
    if(merchant.api_id == merchant_api_id::ums || 
       merchant.api_id == merchant_api_id::paynet)
    {
        merchant_table.api_info(merchant.api_id, acc);
    }
    
    this->commission =  merchant.commission( this->amount ) ; 

    if ( this->merchant.commission_subtracted() ) //merchant_identifiers::commission_subtracted(  merchant_id )  )
    {
        
        int64_t new_amount = this->amount - this->commission ;
        
        slog.WarningLog("client amount: %ld, new-amount: %ld, commission(from new-amount): %ld", amount, new_amount, commission ) ;

        this->amount = new_amount;
    }
    
    return Error_OK ;
}

Error_T purchase_buy_session::init_fields()
{
    SCOPE_LOG(slog);

    return oson::Merchant_api_manager::init_fields(merchant_id, oson_tr_id, list, /*out*/ trans, /*out*/ pay_field) ;
}

Error_T purchase_buy_session::init_card_info()
{
    SCOPE_LOG(slog);
    Error_T ec = Error_OK ;
    this->card_info = api_validate_card_info(d, card_id,  ec );
    if (ec) return ec;

    bool const bonus_card =  card_info.isbonus_card; 

    if (bonus_card)
    {
        if (card_info.deposit < amount + commission)
        {
            slog.ErrorLog("not enough amount on bonus card!");
            return Error_not_enough_amount;
        }
    }

    slog.DebugLog("3. end. card-info (DB): uid: %lld , bonus_card: %d ", (long long)d->m_uid, (int)bonus_card);

    return Error_OK ;
}

Error_T purchase_buy_session::init_user_info()
{
    SCOPE_LOG(slog);

    Error_T ec = Error_OK ;

    Users_T users( oson_this_db );

    this->user_info = users.get( d->m_uid, ec);

    if(ec != Error_OK)     return ec;

    slog.DebugLog("4. end user info (DB): phone: %s", user_info.phone.c_str());

    if (user_info.blocked)
    {
        slog.WarningLog("An user is blocked!");
        return Error_operation_not_allowed;
    }

    return Error_OK;
}

Error_T purchase_buy_session::create_purchase_info()
{
    SCOPE_LOG(slog);
        // Register request
    //this->p_info
    p_info.amount        = amount          ;
    p_info.mID           = merchant_id     ;
    p_info.uid           = d->m_uid        ;
    p_info.login         = pay_field.value ;
    p_info.eopc_trn_id   = "0"             ;
    p_info.pan           = card_info.pan   ;
    p_info.ts            = formatted_time_now("%Y-%m-%d %H:%M:%S") ;
    p_info.status        = TR_STATUS_REGISTRED ;
    p_info.oson_tr_id    = oson_tr_id      ;
    p_info.card_id       = card_info.id    ;
    p_info.commission    = commission      ;

    /********************************************************************/
    if (pay_field.value.empty() || pay_field.fID == 0){
        if (merchant.url.empty() || merchant.url == "0") // if this direct merchant
            p_info.login = user_info.phone ;
    }
    /***********************************************************/
    Purchase_T purchase( oson_this_db  );
    p_info.id =  purchase.add(p_info);
    p_info.receipt_id = p_info.id;
    int64_t const trn_id = p_info.id;

    trans.check_id = trn_id;
    slog.DebugLog("5. end create purchases row. trn-id: %lld", (long long)trn_id);

    return Error_OK ;
}

void purchase_buy_session::on_check_i(const Merch_trans_T& trans, const Merch_check_status_T& status,   Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_check, shared_from_this(), trans, status, ec ) ) ;
}

void purchase_buy_session::on_check(const Merch_trans_T& trans, const Merch_check_status_T& status,   Error_T ec)
{
    SCOPE_LOGD_C(slog);
    
    if ( ec != Error_OK )
    {
        slog.ErrorLog("error: %d", (int)ec);
        p_info.merch_rsp = "check: failed, ec: " + to_str( (int) ec) + "| " + status.status_text ;
        ec_ = ec;
        return ;
    }

    if( ! status.exist )
    {
        slog.ErrorLog("check status exist = false\n");
        p_info.merch_rsp = "check: login not found | " + status.status_text ;
        ec_ =  Error_parameters ; //Error_transaction_not_allowed;
        return ;
    }

    slog.DebugLog("CHECK SUCCESS ==> 6. end check login in merchant. trans.param: %s \n\t\t7. start EOPC card-info.", trans.param.c_str());

    //8. async card_info
    using namespace std::placeholders;
    oson_eopc -> async_card_info( card_info.pc_token, std::bind(&self_type::on_card_info_eopc, shared_from_this(), _1, _2, _3 )) ;
}

Error_T purchase_buy_session::merchant_check_login()
{
    SCOPE_LOGD_C(slog);

    //test async beeline check
    trans.merchant = merchant;
    trans.acc      = acc;

    if (   merchant.api_id == merchant_api_id::nonbilling )  
    {
        slog.WarningLog("No merchant check.") ;
        return Error_OK ;
    }
    
    if (merchant.api_id == merchant_api_id::paynet && trans.service_id_check > 0 )
    {
        trans.user_phone = oson::random_user_phone();
    }
    
    const bool is_developer = (d->m_uid == 7 || d->m_uid == 17 ) ;
    
    const bool has_async_version = 
                ( 
                    merchant.id == merchant_identifiers::Beeline               ||
                    merchant.id == merchant_identifiers::Beeline_test_dont_use ||
                    merchant.id == merchant_identifiers::Ucell_direct          ||
                    merchant.api_id == merchant_api_id::mplat ||
                    //merchant_identifiers::is_mplat(merchant.id)                ||
                    //::boost::algorithm::starts_with(merchant.url, "/etc/" )    ||  /* paynet test */
                    merchant.api_id == merchant_api_id::paynet || 
    
                    ( /*is_developer &&*/ merchant.id == merchant_identifiers::Webmoney_Direct ) ||
    
                    (  merchant.api_id == merchant_api_id::qiwi )  ||
    
                    ( /*is_developer  &&*/ merchant.id == merchant_identifiers::Ucell_direct ) ||
    
                    false // only need for '||' operator suffix.
                ) && !(  merchant.id == merchant_identifiers::UMS && is_developer) ;
    
    if ( has_async_version )
    {
        
        using namespace std::placeholders;
        oson_merchant_api -> async_check_status( trans, std::bind(&self_type::on_check_i, shared_from_this(), _1, _2, _3)   ) ;
        return Error_async_processing;
    }
    
    try
    {
        Merchant_T merch_table( oson_this_db ) ;
        trans.transaction_id = merch_table.next_transaction_id() ;//set auto generation id.

        Merch_check_status_T check_status;

        Merchant_api_T merch_api( merchant, acc);

        Error_T ec = merch_api.check_status(trans, check_status);

        if ( check_status.notify_push  && ! check_status.push_text.empty() )
        {
            Users_notify_T notifier{ oson_this_db } ;
            notifier.send2( d->m_uid, check_status.push_text, MSG_TYPE_BULK_MESSAGE, 5000 ) ;
        }
        
        if (ec != Error_OK)
        {
            slog.ErrorLog("error: %d", (int)ec);

            p_info.merch_rsp = "check: failed, ec: " + to_str( (int) ec) + "| " + check_status.status_text ;

            return ec;
        }

        if( ! check_status.exist )
        {
            slog.ErrorLog("check status exist = false\n");

            p_info.merch_rsp = "check: login not found | " + check_status.status_text;

            return Error_parameters ; //Error_transaction_not_allowed;
        }
    }
    catch(std::exception& e) 
    {
        slog.ErrorLog("exception: %s", e.what());
        return Error_merchant_operation;
    }
    slog.DebugLog("6. end check login in merchant. trans.param: %s", trans.param.c_str());
    return Error_OK ;
}


void purchase_buy_session::on_card_info_eopc(const std::string&id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_card_info, shared_from_this(), id, eocp_card, ec) ) ;
}

Error_T purchase_buy_session::validate_eopc_card(const oson::backend::eopc::resp::card& eocp_card)
{
    SCOPE_LOGD_C(slog);

    std::string& merch_rsp = p_info.merch_rsp;

    int64_t const total_amount = amount + commission ;

    bool const bonus_card = card_info.isbonus_card ;

    if ( ! bonus_card ) 
    {
        bool const card_phone_valid = ! card_info.owner_phone.empty() ;

        if ( card_phone_valid )
        {
            if (card_info.owner_phone != eocp_card.phone )
            {

                merch_rsp = "Card owner is different!";

                slog.ErrorLog("Owner is different: card-owner-phone: '%s', eocp-phone: '%s'", card_info.owner_phone.c_str(), eocp_card.phone.c_str());

                send_notify_card_owner_changed()( d->m_uid, eocp_card.pan ) ;

                return Error_card_owner_changed;
            }// else equal success
        }
        else 
        {
            card_info.owner_phone = eocp_card.phone;
            change_card_owner()( card_info.id, eocp_card.phone ) ;
        }

        if (eocp_card.status != VALID_CARD) 
        {
            slog.WarningLog("Pay from blocked card");

            merch_rsp = "Pay from blocked card. card-status: " + to_str(eocp_card.status);

            return Error_card_blocked;
        }
    } 
    else 
    { // this is bonus card

        //@Note: This is fatal error!!!
        if (eocp_card.status != VALID_CARD)  
        {
            slog.WarningLog("Pay from blocked card");

            return Error_card_blocked;
        }

        if ( eocp_card.balance < total_amount)
        {
            slog.WarningLog("Bonus card not enough amount!!!");

            return Error_not_enough_amount;
        }
    }

    // Check ballance
    if(eocp_card.balance < total_amount  ) 
    {
        slog.ErrorLog("Not enough amount");

        merch_rsp = "Not enough amount";

        return Error_not_enough_amount;
    }

    if ( ! bonus_card )
    {
        Error_T ec =  check_card_daily_limit(user_info, card_info, total_amount);
        if (ec) 
        {
            merch_rsp = "Limit exceeded daily foreign card!" ;
            return ec;
        }
    }

    return Error_OK ;
}


void purchase_buy_session::on_card_info(const std::string&, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    SCOPE_LOGD_C(slog);

    ec_ = ec;
    if ( ec != Error_OK ) { 
        slog.ErrorLog("EC: %d", (int)ec ) ;
        if (ec == Error_timeout || ec == Error_eopc_timout ) {
            p_info.merch_rsp = "EOPC connection timeout." ;
        } else {
            p_info.merch_rsp = "EOPC connection failed. code: " + to_str(ec) ;
        }
        return;
    }

    ec_ = validate_eopc_card(   eocp_card ) ; 
    if (ec_ ) return ;

    slog.DebugLog("7. end card-balance (EOCP).");
    //@Note: We must return there OK status to server.
    {

        d->send_result(Error_OK);
    }

    ec_ = trans_pay();
    
    if (ec_ == Error_async_processing)
    {
        ec_ = Error_OK ;
    }
}


Error_T purchase_buy_session::trans_pay()
{
    SCOPE_LOGD_C(slog);

    Purchase_T purchase( oson_this_db ) ;
    purchase.update_status(p_info.id, TR_STATUS_IN_PROGRESS);
    ///////////////////////////////////////////////////////////////////////////////////
    //==================  EOPC   TRANS   PAY   =======================================
    ////////////////////////////////////////////////////////////////////////////////
    int64_t const trn_id = p_info.id;

    EOPC_Debit_T debin;
    debin.amount      = amount + commission;
    debin.cardId      = card_info.pc_token;
    debin.ext         = num2string(trn_id);
    debin.merchantId  = merchant.merchantId;
    debin.terminalId  = merchant.terminalId;
    debin.port        = merchant.port;
    debin.stan        = make_stan(trn_id);
    using namespace std::placeholders;
    oson_eopc -> async_trans_pay(debin,  std::bind(&self_type::on_trans_pay_eopc, shared_from_this(), _1, _2, _3));
    return Error_async_processing ;
}

void purchase_buy_session::on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec )
{
    d->m_io_service->post( std::bind(&self_type::on_trans_pay, shared_from_this() ,debin, tran, ec ) )   ;
}

void purchase_buy_session::on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD_C( slog );

    ec_ = ec;

    p_info.eopc_trn_id = tran.refNum;

    if(ec != Error_OK) {
        slog.ErrorLog("Error: %d", (int)ec);

        Fault_T fault( oson_this_db );
        
        std::string msg = "Can't perform transaction in EOPC. purchase-id: " + to_str( p_info.id ) + ". \nerror-code = " + to_str(ec) + ", error-msg = " + oson::error_str( ec );
        
        Fault_info_T finfo { FAULT_TYPE_EOPC, FAULT_STATUS_ERROR,  msg } ;
        
        fault.add( finfo );
        
        if ( ec == Error_timeout || ec == Error_eopc_timout  )
        {
            p_info.merch_rsp = "EOPC connection timeout. " ;
        } else {
            p_info. merch_rsp  = "EOPC failed trans-pay.(code:  " + to_str(ec) + ") " + oson::error_str(ec)   ;
        }
        return ;
    }

    if( ! tran.status_ok() ) 
    {
        slog.ErrorLog("Status of transaction invalid");

        Fault_T fault( oson_this_db  );
        fault.add( Fault_info_T(FAULT_TYPE_EOPC, FAULT_STATUS_ERROR, "Wrong status for transaction. purchase-id: " + to_str(p_info.id)) );

        p_info.merch_rsp = "EOPC failed trans-pay. status = " + tran.status;
        ec_ =  Error_eopc_error;
        return ;
    }

    if ( tran.resp != 0 ){
        slog.ErrorLog("resp is invalid( not zero ). ");
        p_info.merch_rsp = "EOPC failed trans-pay. resp = " + to_str(tran.resp);
        ec_ =  Error_card_blocked;
        return ;
    }


    p_info.merch_rsp  = "EOPC trans-pay success.";


    p_info.status = TR_STATUS_IN_PROGRESS; 

    Purchase_T purchase( oson_this_db   ) ;
    purchase.update(p_info.id, p_info);

    slog.DebugLog("8. End Debit. ref-num: %s", tran.refNum.c_str());

    p_info.status = TR_STATUS_ERROR; // a candidate 
    
    ec_ = merchant_pay();
    
}

Error_T purchase_buy_session::merchant_pay()
{
    SCOPE_LOGD_C(slog);
    
    bool const can_pay = !( merchant.api_id == merchant_api_id::nonbilling ) ;
    
     // Pay through paynet|merchant.
    if (  can_pay  )
    {
        Error_T ec = merchant_pay_start();
    
        /////////////////////////////////////////////
        if (ec == Error_async_processing )
        {
            slog.WarningLog("Async processing!");
            return Error_OK ;
        }
        /////////////////////////////////////////////
        
        if (ec == Error_OK )
        {
            p_info.status = TR_STATUS_SUCCESS ; 
        } 
        else if (ec == Error_perform_in_progress )
        {
            p_info.status = TR_STATUS_IN_PROGRESS ;
        }
        else 
        {
            p_info.status = TR_STATUS_ERROR;
        }
    } // end payment to merchant.
    else
    {
       p_info.merch_rsp = "SUCCESS" ;  
       p_info.status = TR_STATUS_SUCCESS;
    }
   
    Purchase_T purchase( oson_this_db ) ;
    purchase.update(p_info.id, p_info);

    slog.DebugLog("9. end merchant pay, paynet-tr-id: %s", p_info.paynet_tr_id.c_str());

    
    if (p_info.status == TR_STATUS_ERROR) 
    {
        slog.WarningLog("===ERROR== reverse the amount to card === " ) ;
        using namespace std::placeholders;
        
        oson_eopc -> async_trans_reverse( p_info.eopc_trn_id, std::bind(&self_type::reverse_notify, shared_from_this(), _1, _2, _3 )  ) ;
        
        return Error_merchant_operation;
    }
    
    Purchase_info_T p_info_copy = p_info;  p_info.id = 0;//no update on destructor.

    bool const bonus_card = card_info.isbonus_card  != 0 ;

    if ( ! bonus_card )
    {
        d->m_io_service->post( std::bind(&ss_bonus_earns, p_info_copy) );
    } 
    else 
    {
        Users_bonus_T users_b( oson_this_db ); 
        users_b.reverse_balance(d->m_uid, amount + commission  );
    }

    slog.DebugLog("10. END PURCHASE %s", (bonus_card ? " with bonus card." : ".")) ;

    return Error_OK;
}

Error_T purchase_buy_session::merchant_pay_start()
{
    SCOPE_LOGD_C(slog);

    slog.DebugLog("merchant-id: %d, trans.service-id: %s ", merchant.id, trans.service_id.c_str() ) ;
    
    int64_t const trn_id = p_info.id; 

    trans.ts               =  p_info.ts       ;
    trans.transaction_id   =  trn_id          ;
    trans.user_phone       =  user_info.phone ;
    trans.acc              =  acc             ;
    trans.merchant         =  merchant        ;
    trans.merch_api_params["oson_eopc_ref_num"] = p_info.eopc_trn_id ;
    
    if (merchant.api_id == merchant_api_id::paynet )
    {
        trans.user_phone = oson::random_user_phone();
    }
    
    // For paynet get its transaction id from database paynet counter
    {
        Merchant_T merch( oson_this_db ) ;
        trans.transaction_id = merch.next_transaction_id( );
        p_info.oson_paynet_tr_id = trans.transaction_id;
    }
    
    const bool  is_developer = (d->m_uid == 7 || d->m_uid == 17);
    
    const bool has_async_version =  
        ( merchant.id == merchant_identifiers::Beeline ) ||
        ( merchant.id == merchant_identifiers::Beeline_test_dont_use && is_developer) ||
        ( merchant.id == merchant_identifiers::Ucell_direct  ) ||
        ( merchant.api_id == merchant_api_id::mplat ) || 
        ( merchant.api_id == merchant_api_id::paynet ) || 
        
        (  merchant.id == merchant_identifiers::Cron_Telecom) ||  /* oson api test*/
        (  merchant.id == merchant_identifiers::Webmoney_Direct ) || 
        (  merchant.api_id == merchant_api_id::qiwi  ) ||
        (  merchant.id == merchant_identifiers::Ucell_direct ) || 
        false
        ;
    
    
    if ( has_async_version ) 
    {
        using namespace std::placeholders;
        oson_merchant_api -> async_perform_purchase(trans, std::bind(&self_type::on_merchant_pay, shared_from_this(), _1, _2, _3 ) ) ;
        return Error_async_processing;
    }
    
    Merch_trans_status_T trans_status;
    Merchant_api_T merch_api(merchant, acc);
    Error_T ec = Error_OK;

    try
    { 
        ec = merch_api.perform_purchase(trans, trans_status);
    } 
    catch(std::exception& e)
    {
        slog.ErrorLog("Fatal exception: %s", e.what());

        Fault_T fault( oson_this_db  );
        fault.add( Fault_info_T(FAULT_TYPE_EOPC, FAULT_STATUS_ERROR, "purchase-id: " +to_str(p_info.id) +  ".\nMerchant perform purchase throws an exception: "  + e.what() ) );
    }

    /////////////////////////////////////////////////////////////////
    p_info.paynet_status = trans_status.merchant_status;
    p_info.paynet_tr_id  =  trans_status.merchant_trn_id ;
    p_info.merch_rsp     =  trans_status.merch_rsp;

    if (ec == Error_perform_in_progress)
    {
        slog.DebugLog(" == perform in progress == ") ;

        d->m_io_service->post( std::bind(&self_type::in_progress_notify, shared_from_this() ) ) ;
        
        return  Error_perform_in_progress ;
    }

    if(ec != Error_OK) {

        Fault_T fault( oson_this_db );
        fault.add(  Fault_info_T(FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p_info.id)) );
        return ec;
    }
    
    return Error_OK ;
}

void  purchase_buy_session::on_merchant_pay(const Merch_trans_T& trans, const Merch_trans_status_T& trans_status, Error_T ec) 
{
    d->m_io_service->post( std::bind(&self_type::on_merchant_pay_i, shared_from_this(), trans, trans_status, ec ) ) ;
}

void  purchase_buy_session::on_merchant_pay_i(const Merch_trans_T& trans, const Merch_trans_status_T& trans_status, Error_T ec) 
{
    SCOPE_LOGD_C(slog);
    
    p_info.paynet_status =  trans_status.merchant_status;
    p_info.paynet_tr_id  =  trans_status.merchant_trn_id ;
    p_info.merch_rsp     =  trans_status.merch_rsp;
    p_info.status        =  TR_STATUS_SUCCESS ; // by default, assumed all are success.
    
    if (ec == Error_OK )
    {
        slog.DebugLog(" == SUCCESS PAYED == ") ;
    }
    else if (ec == Error_perform_in_progress)
    {
        slog.DebugLog(" == perform in progress == ") ;

        d->m_io_service->post( std::bind(&self_type::in_progress_notify, shared_from_this() ) ) ;
        
        p_info.status = TR_STATUS_IN_PROGRESS ;
        
        ec = Error_OK ;
    }
    else
    {
        slog.ErrorLog("=== FAILURE PAYED === " );
        
        Fault_T fault( oson_this_db );
        
        fault.add(  Fault_info_T(FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p_info.id) ) );
        
        p_info.status = TR_STATUS_ERROR ;
    }
    
    Purchase_T purchase( oson_this_db ) ;
    purchase.update(p_info.id, p_info);

    slog.DebugLog("9. end merchant pay, paynet-tr-id: %s", p_info.paynet_tr_id.c_str());

    
    if (p_info.status == TR_STATUS_ERROR) 
    {
        using namespace std::placeholders;
        
        oson_eopc -> async_trans_reverse( p_info.eopc_trn_id, std::bind(&self_type::reverse_notify, shared_from_this(), _1, _2, _3 )  ) ;
        
        return ;
    }
    /////////////////////////////////////////////////////////////////////
    Purchase_info_T p_info_copy = p_info;  p_info.id = 0;//no update on destructor.

    bool const bonus_card = card_info.isbonus_card  != 0 ;

    if ( ! bonus_card )
    {
        d->m_io_service->post( std::bind(&ss_bonus_earns, p_info_copy) );
    } 
    else 
    {
        Users_bonus_T users_b( oson_this_db ); 
        users_b.reverse_balance(d->m_uid, amount + commission  );
    }

    slog.DebugLog("10. END PURCHASE %s", (bonus_card ? " with bonus card." : ".")) ;
}

    
void purchase_buy_session::reverse_notify(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) 
{
    d->m_io_service->post( std::bind(&self_type::reverse_notify_i, shared_from_this(), tranId, tran, ec) ) ;
}
    
void purchase_buy_session::reverse_notify_i(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) 
{
    SCOPE_LOGD_C(slog);

    if ( tran.status_reverse_ok() && tran.resp == 0 ) 
    {

        Users_notify_T user_n( oson_this_db ) ;

        int64_t uid = p_info.uid;

        int time_ms = 5 * 1000 ; // 5 seconds

        Msg_type__T type = MSG_TYPE_PURCHASE_MESSAGE;

        std::string msg = "Ваша оплата не осуществлена.\nСредство возращено на вашу карту.\nЧек №:" + to_str(p_info.id) + ".\nСумма: " + to_str( p_info.amount / 100 ) + " сум." ;

        user_n.send2(uid, msg, type, time_ms ) ;

        p_info.merch_rsp += " <EOPC reversed> " ;

    } 
    else 
    {
        p_info.merch_rsp += " <EOPC NOT reversed> " ;

    }

    if ( p_info.id != 0 )
    {
        update_purchase();

        p_info.id = 0;//no more needed update
    }
}

void purchase_buy_session::in_progress_notify() 
{
    d->m_io_service->post( std::bind( & self_type::in_progress_notify_i, shared_from_this() ) ) ;
}

void purchase_buy_session::in_progress_notify_i()
{
    SCOPE_LOGD_C(slog);

    Users_notify_T user_n( oson_this_db ) ;

    int64_t uid      = p_info.uid;

    int time_ms      = 5 * 1000; // next 5 seconds

    Msg_type__T type = MSG_TYPE_PURCHASE_MESSAGE ;

    std::string msg  = "Ваша оплата ещё не окончена, пожалуйста ждите(~5-10 минут).\nЧек №:" + to_str(p_info.id) 
                       +  ".\nЛогин: " + p_info.login 
                       +  ".\nCумма: " + to_str( p_info.amount / 100 ) + " сум." ;

    user_n.send2(uid, msg, type, time_ms );
}


static Error_T api_purchase_buy(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGF(slog);
    ///////////////////////////////////////////////////////////////////////////    
    const int32_t merchant_id = reader.readByte4();
    const int64_t amount      = reader.readByte8();
    const int64_t card_id     = reader.readByte8();
    
    std::vector<Merchant_field_data_T> list = parse_merchant_field_data(reader);
    
    const uint64_t oson_tr_id = reader.readByte8();
    ////////////////////////////////////////////////////////////////////////////////
    slog.DebugLog("1. read: merchant-id: %d, amount: %lld, card-id: %lld,  oson-tr-id: %lld, fields-count: %d", 
    (int)merchant_id, (long long)amount, (long long)card_id,  (long long)oson_tr_id, (int)list.size());
    
    typedef purchase_buy_session session;
    typedef std::shared_ptr< session > session_ptr;

    session_ptr s = std::make_shared< session > ( d, merchant_id, amount, card_id, oson_tr_id, list ) ;

    s->async_start();

    return Error_async_processing ;
}


Error_T ss_bonus_earns(Purchase_info_T p_info)
{
    SCOPE_LOG(slog);
    
    const int64_t uid         = p_info.uid;
    const int32_t merchant_id = p_info.mID;
    const int64_t amount      = p_info.amount ;
    
    DB_T& m_db = oson_this_db ;
    
    Merchant_T merch(m_db);
    Merchant_bonus_info_T search, bmerch;
    Merchant_bonus_list_T bmerch_list;
    search.merchant_id = merchant_id ;
    search.active      = true;
    Sort_T sort(0, 0);//no sort
    
    //add bonus balance to user.
    Error_T ec = merch.bonus_list(search, sort, bmerch_list);
    
    if (ec) return ec;
    
    if ( bmerch_list.list.empty() )
        return Error_not_found;

    bmerch = bmerch_list.list[0];

    if ( amount < bmerch.min_amount  )
        return Error_not_enough_amount;
    
    const int64_t bonus_amount = amount * ( bmerch.percent / 10000.0 ) ;

    //add this bonus_amount to user
    Users_bonus_T users_b(m_db);
    User_bonus_info_T uinfo;

    ec = users_b.bonus_info(uid, uinfo);
    if (ec != Error_OK )return ec ;

    {
        if (uinfo.block != 1){
            slog.WarningLog("This user is blocked.");
            return Error_card_blocked;
        }
        //update
        uinfo.earns   += bonus_amount;
        uinfo.balance += bonus_amount;
        users_b.bonus_edit(uinfo.id, uinfo);
    }

    p_info.bearns = bonus_amount;
    
    {
        std::string query = "UPDATE purchases SET bearn = " + escape(bonus_amount) + "  WHERE id = " + escape(p_info.id) ;
        DB_T::statement st( oson_this_db ) ;
        st.prepare(query);
    }
    
    std::string msg = "Поздравляем! Вы получили бонус в размере " + to_money_str(bonus_amount) + " сум." ;
    //SEND PUSH NOTIFICATION!!!
    {
        Users_notify_T users_n(m_db);
        users_n.send2(uid, msg, MSG_TYPE_BONUS_MESSAGE, 10 * 1000 ); // 10 seconds
    }
    return Error_OK;
}

static Error_T api_bonus_earns(api_pointer_cref d, Transaction_info_T  tr_info )    
{
    SCOPE_LOGD(slog);
    Error_T ec;
    
    const  int64_t uid = tr_info.uid;
    
    //// 1.  check user_bonus .
    User_bonus_info_T ub_info;
    Users_bonus_T users_b( oson_this_db );
    
    ec = users_b.bonus_info(uid, ub_info);
    
    if ( ec )return ec; // there no exists bonus card
    
    if ( ub_info.block != ub_info.ACTIVE_CARD)  // blocked
    {
        slog.WarningLog("bonus card is blocked!");
        return Error_card_blocked;
    }
    
    ///// 2. GET bank bonus
//    int64_t card_id = tr_info.srccard_id ;
//    
//    if (card_id == 0)
//    {
//        slog.WarningLog("There no card-id.");
//        return Error_card_not_found;
//    }
//    
//    Cards_T cards( oson_this_db );
//    Card_info_T c_search, c_info;
//    c_search.id = card_id;
//    ec = cards.info(c_search, c_info);
//    if ( ec ) return ec;

    if ( tr_info.dstcard.empty() ) {
        slog.ErrorLog("DST-CARD IS EMPTY ! " ) ;
        return Error_internal;
    }
    
    Bank_info_T bank_search; 
    bank_search.bin_code =  tr_info.dstcard ; //c_info.pan; // bin-code will extract within search.
    Bank_T bank( oson_this_db  );
    Bank_info_T bank_info = bank.info(bank_search, ec);
    if (ec) return ec;
    
    uint32_t bank_id = bank_info.id ;
    Bank_bonus_info_T bank_bonus_search, bank_bonus_info;
    bank_bonus_search.bank_id = bank_id;
    bank_bonus_search.status  = bank_bonus_search.ACTIVE_STATUS;
    ec = bank.bonus_info(bank_bonus_search, bank_bonus_info);
    if (ec)return ec;
    ////////////////////////////////////////////////
    
    /// 3. check minimum limit.
    if ( tr_info.amount < bank_bonus_info.min_amount)
    {
        slog.WarningLog("amount less than required minimum.");
        return Error_min_limit;
    }
    
    
    // 4. calculate bonus
    uint64_t amount = tr_info.amount ;
    uint64_t bonus_earn = (uint64_t) ( bank_bonus_info.percent / 10000.0 * amount) ; // percent - 100 multiplied actual percent.
    
    
    // 5. update tables.
    tr_info.bearn = bonus_earn;
    
    ub_info.balance += bonus_earn;
    ub_info.earns   += bonus_earn;
    
    Transactions_T trans( oson_this_db   );
    trans.transaction_edit(tr_info.id,  tr_info);
    
    users_b.bonus_edit(ub_info.id, ub_info);
    
 
    std::string msg = "Поздравляем! Вы получили бонус в размере " + to_money_str(bonus_earn) + " сум." ;
   
    Users_notify_T users_n(  oson_this_db    );
    users_n.send2(uid, msg, MSG_TYPE_BONUS_MESSAGE, 7 * 1000); // next 7 seconds
    
    return Error_OK ;
}

/*************************************************************************/
namespace 
{

class public_buy_start_session: public std::enable_shared_from_this< public_buy_start_session >
{
    
public:
    api_pointer d;
    std::string input_of_request;
    ////////////////////////////////////
    
    uint32_t merchant_id ;
    std::vector<Merchant_field_data_T> in_fields;
    int64_t amount, commission;
    
    std::string cardnumber;
    std::string cardexpiry;
    int64_t oson_tr_id;
    /////////////////////////////////////////
    Purchase_info_T p_info;
    Merchant_info_T merchant;
    
    Merchant_field_data_T pay_field;
    Merch_trans_T trans;

public:
    typedef public_buy_start_session self_type;
    typedef std::shared_ptr< self_type > pointer;
    
    static pointer create(api_pointer d);
    
    explicit public_buy_start_session(api_pointer d);
    
    void async_start();
    
    ~public_buy_start_session();
    
private:
    void update_purchase() ;
    
    void start() ;
    
    Error_T init_merchant();
    
    Error_T init_fields();
    Error_T merchant_check() ;
    Error_T create_purchase_row() ;
    Error_T save_input_of_request();
    Error_T start_i();
    void on_eopc_card_info(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec);
    void on_card_info(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec);    
    Error_T on_card_info_i(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& eocp_card, Error_T ec);
};   // end structure

////////////////////////////////////////////////////////////////////////////////////
public_buy_start_session::pointer public_buy_start_session::create(api_pointer d){
    return std::make_shared< self_type > ( d );
}

public_buy_start_session::public_buy_start_session(api_pointer d)
: d( d ) 
{
}

public_buy_start_session::~public_buy_start_session()
{
    SCOPE_LOGD_C(slog);
    if (p_info.id != 0){
        update_purchase();
    }
}


void public_buy_start_session::async_start()
{
    d->m_io_service->post( std::bind(&self_type::start, shared_from_this()  ) ) ;
}


void public_buy_start_session::update_purchase()
{
    Purchase_T table( oson_this_db ) ;
    table.update(p_info.id, p_info);
}

void public_buy_start_session::start()
{
    SCOPE_LOGD_C(slog);

    Error_T ec  = start_i();
    if (ec != Error_async_processing)
    {
        d->send_result(ec);
    }
}

Error_T public_buy_start_session::init_merchant()
{
    SCOPE_LOG_C(slog);

    Error_T ec;
    Merchant_T merch_table( oson_this_db  );
    this->merchant = merch_table.get(merchant_id,  /*OUT*/ec );
    if(ec != Error_OK)return ec;

    if (merchant.status == MERCHANT_STATUS_HIDE ){
        slog.WarningLog("This merchant is disabled!");
        return Error_operation_not_allowed;
    }

    ec = merchant.is_valid_amount(amount) ;
    if (ec) return ec;

    this->commission =  merchant.commission( amount ) ; //(merchant.rate / 10000.0 * amount ) + merchant.rate_money;
    
    if ( this->merchant.commission_subtracted() ) //merchant_identifiers::commission_subtracted(  merchant_id ) )
    {
        int64_t new_amount = this->amount - this->commission ;
        
        this->amount = new_amount;
    }
    return Error_OK ;
}


Error_T public_buy_start_session::init_fields()
{
    SCOPE_LOG_C(slog);

    const std::vector< Merchant_field_data_T > & list = this->in_fields ;

    return oson::Merchant_api_manager::init_fields( merchant_id, oson_tr_id, list,  /*out*/trans, /*out*/pay_field );
}

Error_T public_buy_start_session::merchant_check()
{
    SCOPE_LOG_C( slog );

    Merchant_T merchant_table( oson_this_db ) ;

    trans.amount = amount;
    trans.param  = pay_field.value;
    Merch_acc_T acc;

    merchant_table.acc_info(merchant_id, acc);
    if(merchant.api_id == merchant_api_id::ums || 
       merchant.api_id == merchant_api_id::paynet)
    {
        merchant_table.api_info(merchant.api_id, acc);
    }
    Merchant_api_T merch_api(merchant, acc);

    Merch_check_status_T check_status;

    bool const can_check = !( merchant.api_id == merchant_api_id::nonbilling);
    
    if (  can_check ) {
        trans.transaction_id = merchant_table.next_transaction_id();
        trans.user_phone     = oson::random_user_phone();
        
        
        Error_T ec = merch_api.check_status(trans, check_status);
        if (ec != Error_OK)return ec;
        if( ! check_status.exist )return Error_transaction_not_allowed;
    }

    return Error_OK ;
}

Error_T  public_buy_start_session::create_purchase_row()
{
    SCOPE_LOG_C(slog);

    std::string pan_mask = cardnumber ; 

    if ( pan_mask.length() >= 12 ) 
        std::fill_n(pan_mask.begin() + 6, 6, '*');

    // Register request | @Note simplify constructing Purchase_info_T structure!
   // Purchase_info_T p_info;   //== used this->p_info
    p_info.amount      = amount          ;
    p_info.mID         = merchant_id     ;
    p_info.uid         = 0 /*uid */      ;
    p_info.login       = pay_field.value ;
    p_info.eopc_trn_id = "0"             ;
    p_info.pan         = pan_mask        ;
    p_info.ts          = formatted_time_now_iso_S();
    p_info.status      = TR_STATUS_REGISTRED;
    p_info.receipt_id  = 0               ;
    p_info.commission  = commission       ;
    p_info.card_id     = 0;/*card_id*/   ;
    p_info.oson_tr_id  = oson_tr_id      ;

    Purchase_T purchase(  oson_this_db  );
    p_info.id = purchase.add(p_info);

    trans.check_id = p_info.id;
    return Error_OK ;
}

Error_T public_buy_start_session::save_input_of_request()
{
    if (oson_tr_id == 0) 
        return Error_OK ; //no save

    std::string b64 = oson::utils::encodebase64(input_of_request) ;

    DB_T::statement st( oson_this_db ) ;
    std::string query = "UPDATE purchase_info SET "
                        " input_of_request = " + escape( b64 ) + ", "
                        " request_type     = 2 "  // 2-public purchase
                        " WHERE oson_tr_id = " + escape(oson_tr_id) ;
    st.prepare( query ) ;
    if (st.affected_rows() == 0 ) {
        //there no row with oson_tr_id, so create it.
        query = "INSERT INTO purchase_info (oson_tr_id, trn_id, request_type, input_of_request) VALUES ( "
                " " + escape(oson_tr_id) + " , " 
                " " + escape(p_info.id)  + " , "
                " " + escape(2)          + " , "   // 2- public purchase
                " " + escape( b64 )      + " )  "
                ;
        st.prepare(query);
    }
    return Error_OK ;
}

Error_T public_buy_start_session::start_i()
{
    SCOPE_LOGD_C(slog);
    Error_T ec;

    ec = init_merchant();
    if (ec) return ec;

    ec = init_fields();
    if (ec) return ec;

    ec = create_purchase_row();
    if (ec) return ec;

    save_input_of_request();


    ec = merchant_check();
    if (ec) return ec;

    oson::backend::eopc::req::card in = { cardnumber, expire_date_rotate(cardexpiry) };
    oson_eopc -> async_card_new( in, std::bind(&self_type::on_eopc_card_info, shared_from_this(), std::placeholders::_1, std::placeholders::_2,std::placeholders::_3)  ) ;

    return Error_async_processing;
}

void public_buy_start_session::on_eopc_card_info(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec)
{   // send code to clientApi thread
    d->m_io_service->post( ::std::bind(&self_type::on_card_info, shared_from_this(), in, out, ec) ) ;
}

void public_buy_start_session::on_card_info(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec)
{
    SCOPE_LOGD_C(slog);


    ec = on_card_info_i(in, out, ec);

    return d->send_result( ec );
}

Error_T public_buy_start_session::on_card_info_i(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& eocp_card, Error_T ec)
{
    SCOPE_LOGD_C( slog );

    p_info.status = TR_STATUS_ERROR; // a potential candidate.

    if ( ec != Error_OK ) 
    {
        p_info.merch_rsp = "EOCP card_new failed with code: " + to_str(ec);
        slog.ErrorLog("error code: %d", (int)ec);
        return ec;
    }

    if (eocp_card.status != VALID_CARD)
    {
        p_info.merch_rsp = "This card is blocked. status: " + to_str(eocp_card.status); 
        slog.ErrorLog("This card is blocked. See 'status' field.");
        return Error_card_blocked;
    }

    if (eocp_card.phone.empty() )
    {
        p_info.merch_rsp = "EOCP card doesn't have a phone." ;
        // 90 356 78 99  : length = 9 minimum length
        slog.ErrorLog("phone is empty ");
        return Error_card_phone_not_found;
    }

    /////////////////////////////////////////////////////////////////////////////////////
    //                          CREATE PURCHASE ROW on DATABASE.
    //////////////////////////////////////////////////////////////////////////////////////
    int64_t uid = 0, card_id = 0;

    /// ======= get uid and card-id if possible!!
    {//@Note: we can more logical search,  iff there more than one account added this card, choose that where card_owner_phone == users.phone.
        
        std::string query = "SELECT uid, card_id FROM cards WHERE pc_token = " + escape( eocp_card.id ) + " LIMIT 12 " ;
        
        DB_T::statement st( oson_this_db ) ;
        
        st.prepare( query ) ;
        
        if (int r = st.rows_count() ) {
            if (r == 1 ) // a single
            {
                st.row(0) >> uid >> card_id; 
            } else { 
                query = "SELECT uid, card_id FROM cards WHERE pc_token = " + escape( eocp_card.id) + " AND owner_phone IN ( SELECT phone FROM users WHERE id = uid ) " ;
                st.prepare(query);
                r = st.rows_count();
                if (r == 1 ) {
                    st.row(0) >> uid >> card_id ;
                }
            }
        }
    }

    std::string code = oson::utils::generate_code( 5 );

    //@Note: simplify It.
    Activate_info_T act_i;
    act_i.phone    = eocp_card.phone                ;
    act_i.code     = code                           ;
    act_i.kind     = act_i.Kind_public_purchase_buy ;
    act_i.other_id = p_info.id                      ;
    act_i.dev_id   = "public_purchase"              ;
    act_i.add_ts   = formatted_time_now_iso_S() ;

    Activate_table_T act_table( oson_this_db );
    act_table.add(act_i);

    p_info.status     = TR_STATUS_REGISTRED  ; // back it again
    p_info.receipt_id = p_info.id   ;
    p_info.uid        = uid         ;
    p_info.card_id    = card_id     ;

    update_purchase();

    std::string cphone = eocp_card.phone;
    // 012  34 567  89  11
    // 998 [97 123  45] 67  : replace middle 5 symbols to '*' (star).
    if (cphone.length() >= 9){
        std::fill_n(cphone.begin() + 3, 5, '*');
    }
    /////////////////////////////////////////////////////////////
    d->m_writer << b8(p_info.id) << cphone ;
    /////////////////////////////////////////////////////////////

    p_info.id = 0;//no update needed
    std::string msg = "www.oson.uz: код подтверждения для оплаты без регистрации " + code +"\nСумма: " + to_money_str(this->amount, ',') ;
    SMS_info_T sms_info( eocp_card.phone, msg, SMS_info_T::type_public_purchase_code_sms) ;
    oson_sms -> async_send(sms_info);


    return Error_OK;        
} // end handle


}   // end namespace




static Error_T api_public_purchase_buy_start(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    ////////////////////////////////////////////////////////////////////////////
    typedef public_buy_start_session session;
    d->m_uid = 0;
    session::pointer s = session::create( d ) ;
    
    s->merchant_id = reader.readByte4();
    s->in_fields   = parse_merchant_field_data(reader);
    s->amount      = reader.readByte8();
    s->cardnumber  = reader.readAsString( reader.readByte2() ) ;
    s->cardexpiry  = reader.readAsString( reader.readByte2() ) ;
    
    s->oson_tr_id  = reader.readByte8();
    
    //////////////////////////
    reader.reset();
    s->input_of_request = reader.readAsString(reader.remainBytes()); 
    
    s->async_start(); 
    
    return Error_async_processing ;
}


namespace
{

class public_buy_confirm_session : public std::enable_shared_from_this< public_buy_confirm_session >
{
public:
    typedef public_buy_confirm_session     self_type ;
    typedef std::shared_ptr< self_type >  pointer   ;
    
    
    static pointer create(api_pointer d){  return std::make_shared< self_type >(d); }
    
public:
    api_pointer       d       ;
    Purchase_info_T   p_info  ;
    Merch_trans_T     trans   ;
    std::string       phone   ;
    Merchant_field_data_T pay_field;
    //////////////////////////////////////////
    int64_t trn_id ; 
    std::string sms_code, cardnumber, cardexpiry;
    /////////////////////////////////////////
    
public:
    explicit public_buy_confirm_session(api_pointer d) ;
    
    ~public_buy_confirm_session() ;
    
    void async_start();
private:
    void start() ;
    Error_T verify_sms_code() ;
    Error_T  get_purchase_info() ;
    
    Error_T init_fields() ;
    
    Error_T start_i() ;
     
    void on_eopc_card_info(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& out, Error_T ec) ;
    
    void on_card_info(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& out, Error_T ec) ;
    Error_T card_info_handle(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& eocp_card, Error_T ec);
    
    void trans_pay_handler_eopc_thread(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec) ;
    void on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec) ;
    Error_T trans_pay_handler_ii(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec) ;
};
    
} // end noname namespace


public_buy_confirm_session::public_buy_confirm_session(api_pointer d)
        : d( d )
{
    SCOPE_LOGF_C(slog);
}

public_buy_confirm_session::~public_buy_confirm_session()
{
    SCOPE_LOGF_C(slog);
}

void public_buy_confirm_session::async_start()
{
    d->m_io_service->post( std::bind( &self_type::start, shared_from_this() ) ) ;
}

void public_buy_confirm_session::start()
{
    SCOPE_LOGD_C(slog);
    Error_T ec = start_i();
    d->send_result( ec );
}

Error_T public_buy_confirm_session::verify_sms_code()
{
    SCOPE_LOG_C(slog);

    if (sms_code.empty()){
        slog.ErrorLog("sms code is empty!");
        return Error_parameters;
    }

    Activate_info_T act_s, act_i;
    act_s.kind     = act_s.Kind_public_purchase_buy;
    act_s.code     = sms_code ;
    act_s.other_id = trn_id;

    Activate_table_T act_table(  oson_this_db  );
    act_i = act_table.info( act_s ) ;

    bool const matched = act_i.id != 0 && act_i.code == sms_code ;

    if ( ! matched  ){
            slog.ErrorLog("sms code not found ") ;
            return Error_sms_code_not_verified;
    }

    act_table.deactivate( act_i.id );

    return Error_OK ;
}

Error_T  public_buy_confirm_session::get_purchase_info()
{
    SCOPE_LOG_C(slog);
    //Purchase_info_T& p_info = this->p_info;
    Purchase_T purchase(  oson_this_db   );

    Error_T ec = Error_OK;
    ec = purchase.info(trn_id, this->p_info);
    if (ec)return ec;

    if (p_info.status != TR_STATUS_REGISTRED)
    {
        slog.ErrorLog("status is not registered");
        return Error_sms_code_not_verified;
    }
    //set to status in progress.
    purchase.update_status(p_info.id, TR_STATUS_IN_PROGRESS);

    return Error_OK ;
}

Error_T public_buy_confirm_session::init_fields()
{
    SCOPE_LOG_C(slog);
    ////////////////////////////////////////////////
    int64_t const oson_tr_id =   p_info.oson_tr_id ;

    if (oson_tr_id == 0){
        //there no purchase-info, skip this
        slog.WarningLog("  =>>>> oson_tr_id = 0");
        return Error_OK ;
    }

    DB_T::statement st( oson_this_db ) ;

    st.prepare("SELECT input_of_request FROM purchase_info WHERE ( request_type = 2 ) AND oson_tr_id = " + escape(oson_tr_id) ) ;

    if (st.rows_count() == 0 ) {
        //there no purchase-info, but it must be exist!
        slog.WarningLog("There no purchase info for oson_tr_id = %ld", oson_tr_id);
        return Error_internal ;
    }

    std::string input_of_request, b64 ;
    st.row(0) >> b64;

    input_of_request = oson::utils::decodebase64(b64);

    ByteReader_T reader( (const byte_t*)input_of_request.data(), input_of_request.size() ) ;

    int32_t merchant_id                                = reader.readByte4();
    std::vector< Merchant_field_data_T> in_fields      = parse_merchant_field_data(reader);
    //////////////////////////////////////////////////////////////

    return oson::Merchant_api_manager::init_fields(merchant_id, oson_tr_id, in_fields, /*out*/ trans, /*out*/ pay_field);
}

Error_T public_buy_confirm_session::start_i()
{
    SCOPE_LOG_C(slog);

    Error_T ec;
    //////////////////////
    ec = verify_sms_code();
    if (ec) return ec;

    ec = get_purchase_info();
    if (ec) return ec;

    ec = init_fields();
    if (ec) return ec;

    oson::backend::eopc::req::card in = { cardnumber, expire_date_rotate( cardexpiry ) } ;

    oson_eopc -> async_card_new(in, std::bind(&self_type::on_eopc_card_info, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;

    return Error_async_processing;
}


void public_buy_confirm_session::on_eopc_card_info(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& out, Error_T ec)
{
     //this operator works in EOCP thread, move handle to ClientApi thread.
    d->m_io_service-> post(  ::std::bind( &self_type::on_card_info, shared_from_this(), in, out, ec) ) ;
}

void public_buy_confirm_session::on_card_info(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& out, Error_T ec)
{
    SCOPE_LOGD_C(slog);

    ec = card_info_handle(in, out, ec);

    return d->send_result( ec );

}

Error_T public_buy_confirm_session::card_info_handle(const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)
{
    SCOPE_LOG_C(slog);

    this->phone = eocp_card.phone ;

    int64_t const trn_id = p_info.id; 

    Purchase_T purchase( oson_this_db  ) ;

    int const old_status = p_info.status;
    slog.InfoLog("old_status: %d", old_status);

    p_info.status = TR_STATUS_ERROR ;
    bool p_info_commit = false;
    BOOST_SCOPE_EXIT(&p_info, &purchase, &p_info_commit){
        if ( ! p_info_commit) purchase.update( p_info.id, p_info );
    }BOOST_SCOPE_EXIT_END ;

    const int64_t amount     = p_info.amount;
    const int64_t commission = p_info.commission ;


    if(ec != Error_OK){
        p_info.merch_rsp = "EOCP card-info FAILED. error: " + to_str(ec);
         return ec;
    }
    if (eocp_card.status != VALID_CARD)
    {
        slog.ErrorLog("This card is invalid. See 'status'.");
        p_info.merch_rsp = "This card is not valid. status: " + to_str( eocp_card.status );
        return Error_card_blocked;
    }
    if (eocp_card.balance <  amount + commission)
    {
        slog.ErrorLog("Not enough amount");
        p_info.merch_rsp = "Not enough amount" ;
        return Error_not_enough_amount;
    }


    Merchant_T merch(  oson_this_db   );
    Merchant_info_T merchant = merch.get(p_info.mID, ec );
    if (ec) return ec;

    {
        // Perform transaction to EOPC
          EOPC_Debit_T debin;
          debin.amount      = amount + commission;
          debin.cardId      = eocp_card.id;
          debin.ext         = num2string(trn_id);
          debin.merchantId  = merchant.merchantId;
          debin.terminalId  = merchant.terminalId;
          debin.port        = merchant.port;
          debin.stan        = make_stan(trn_id);

          oson_eopc -> async_trans_pay( debin,  std::bind(&self_type::trans_pay_handler_eopc_thread, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;

          p_info.status = old_status;
          p_info_commit = true;

          return Error_async_processing ;
    }
}

void public_buy_confirm_session::trans_pay_handler_eopc_thread(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    //this method works on EOCP thread, move trans_pay_handler to ClientApi thread.
    d->m_io_service-> post(  ::std::bind( &self_type::on_trans_pay, shared_from_this(), debin, tran, ec) ) ;
}

void public_buy_confirm_session::on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD_C(slog);


    ec = trans_pay_handler_ii(debin, tran, ec);

    return d->send_result( ec );
}

Error_T public_buy_confirm_session::trans_pay_handler_ii(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOG_C(slog);

    int64_t const trn_id = p_info.id; 

    Purchase_T purchase( oson_this_db  ) ;

    int const old_status = p_info.status;
    slog.InfoLog("old_status: %d", old_status);

    p_info.status = TR_STATUS_ERROR ;
    bool p_info_commit = false;
    BOOST_SCOPE_EXIT(&p_info, &purchase, &p_info_commit){
        if ( ! p_info_commit) purchase.update( p_info.id, p_info );
    }BOOST_SCOPE_EXIT_END ;

    p_info.eopc_trn_id = tran.refNum;
    p_info.pan         = tran.pan;

    if(ec != Error_OK) {
        slog.ErrorLog("Error: %d", (int)ec);

        Fault_T fault{ oson_this_db  } ;

        std::string msg = "Can't perform transaction in EOPC. purchase-id: " + to_str(p_info.id) + ". \nerror-code=" + to_str(ec) + ", error-msg: " + oson::error_str(ec) + "." ;

        Fault_info_T finfo{  FAULT_TYPE_EOPC, FAULT_STATUS_ERROR,  msg } ;

        fault.add( finfo );

        p_info.merch_rsp = "EOPC trans_pay get error: " + to_str( (long long)ec);  

        purchase.update(p_info.id, p_info);

        p_info_commit = false;

        return ec;
    }


    if( ! tran.status_ok() ) 
    {
        slog.ErrorLog("Status of transaction invalid");

        Fault_T fault( oson_this_db  );
        fault.add( Fault_info_T(FAULT_TYPE_EOPC, FAULT_STATUS_ERROR, "Wrong status for transaction. purchase-id: " + to_str(p_info.id) ) );

        p_info.merch_rsp = "EOPC tran.status = " + tran.status;


        purchase.update(p_info.id, p_info);
        p_info_commit = false;

        return Error_internal;
    }

    if ( tran.resp != 0 )
    {
        slog.ErrorLog("resp is invalid( not zero ). ");
        p_info.merch_rsp = "EOPC tran.resp = " + to_str(tran.resp);

        purchase.update(p_info.id, p_info);
        p_info_commit = false;

        return Error_card_blocked;
    }


    slog.DebugLog("Purchase success: ref-num: '%s' ", tran.refNum.c_str());

    p_info.merch_rsp = "eopc_trans_pay success!";

    /*****************************************************/        

    Merchant_T merchant_table(  oson_this_db   );
    Merchant_info_T merchant = merchant_table.get(p_info.mID, ec );


    Merch_acc_T acc;
    merchant_table.acc_info(p_info.mID, acc);
    if(merchant.api_id == merchant_api_id::ums || 
       merchant.api_id == merchant_api_id::paynet)
    {
        merchant_table.api_info(merchant.api_id, acc);
    }
    Merchant_api_T merch_api( merchant, acc);



    // Pay through paynet|merchant.
    {
        trans.uid             =  p_info.uid ;
        trans.amount          =  p_info.amount;
        trans.param           =  p_info.login;
        trans.ts              =  p_info.ts ;
        trans.transaction_id  =  trn_id;
        //    trans.service_id      =  merchant.extern_service;
        trans.user_phone       =  this->phone ; 

        trans.merch_api_params["oson_eopc_ref_num"] = p_info.eopc_trn_id ;
        
        if (merchant.api_id == merchant_api_id::paynet  ) 
        {
            trans.user_phone = oson::random_user_phone();
        }

        // For paynet get its transaction id from database paynet counter
        {
            trans.transaction_id = merchant_table.next_transaction_id( );
            p_info.oson_paynet_tr_id = trans.transaction_id;
        }

        Merch_trans_status_T trans_status;

        ec = merch_api.perform_purchase(trans, trans_status);

        p_info.paynet_status = trans_status.merchant_status;
        p_info.paynet_tr_id  = trans_status.merchant_trn_id ;
        p_info.merch_rsp     = trans_status.merch_rsp;


        if (ec == Error_perform_in_progress)
        {
            slog.DebugLog("=== Perform in progress === " );
            p_info.status = TR_STATUS_IN_PROGRESS;

            return Error_OK ;
        }

        if(ec != Error_OK) 
        {
            //reverse from EOCP.
            oson_eopc -> async_trans_reverse( p_info.eopc_trn_id, oson::trans_reverse_handler() ) ;

            Fault_T fault(  oson_this_db  );
            fault.add(   Fault_info_T( FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p_info.id) ) );
            return ec;
        }
    }
    p_info.status = TR_STATUS_SUCCESS;
    purchase.update(p_info.id, p_info);
    p_info_commit = true;

    return Error_OK ;
}


static Error_T api_public_purchase_buy_confirm(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////////
    int64_t trn_id = 0 ; 
    std::string sms_code, cardnumber, cardexpiry;
    reader >> r8(trn_id) >> r2(sms_code) >> r2(cardnumber) >> r2(cardexpiry)  ;
    //////////////////////////////////////////////////////////////////////////
    typedef public_buy_confirm_session  session;
    d->m_uid = 0;
    session::pointer  s =  session::create( d ) ;
    
    s->trn_id       =  trn_id      ;
    s->sms_code     =  sms_code    ;
    s->cardnumber   =  cardnumber  ;
    s->cardexpiry   =  cardexpiry  ;
    
    s->async_start();
    
    return Error_async_processing ;
}

/*************************************************************************/

static Error_T api_favorite_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////
    Favorite_info_T search;
    Sort_T sort;
    std::string sort_field, sort_order; 
    reader >> r4( search.merchant_id ) >> r4( sort.offset ) >> r2( sort.limit ) >> r2(sort_field) >> r2(sort_order);
    
    search.uid         = d->m_uid;
    
    sort.offset = oson::utils::clamp(sort.offset, 0, INT_MAX ) ;
    sort.limit  = oson::utils::clamp(sort.limit,  0, 100     ) ;

    sort.order.field   = 2 ; //always user by fav_id
    sort.order.field2  = 3 ; //field-id.
    
    sort.order.order  = make_order_from_string(sort_order);
    
    
    Purchase_T purch(  oson_this_db   );
    Favorite_list_T  out;
    purch.favorite_list( search, sort, out ); // never get error
    /////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4( out.total_count ) <<  b4( out.list.size() ) ;
    
    for(size_t i = 0, n = out.list.size(); i < n ; )
    {
        size_t j = i;
        while( j < n && out.list[ j ].fav_id == out.list[ i ] .fav_id ) 
            ++j ;
        
        int32_t sz_fav_id = j - i ;
        d->m_writer << b4( sz_fav_id )  << b4(out.list[i].fav_id );
        
        for( ; i < j; ++i )
        {
             const Favorite_info_T& fav = out.list[ i ];
             
             d-> m_writer << b4(fav.merchant_id) << b4(fav.field_id) << b4(fav.key)
                          << fav.value << fav.prefix << fav.name ;
        }
    }
    //////////////////////////////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_favorite_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////////////////////
    size_t count;
    Favorite_info_T info;
    
    reader >> r4(info.merchant_id) >> r2( info.name ) >> r2( count );
   
    std::vector<Favorite_info_T> list(count, info); // copy info to every element. 

    for(Favorite_info_T& fav : list){
        reader >> r4(fav.field_id) >> r4(fav.key) >> r2(fav.value) >> r2(fav.prefix);
    }

    Purchase_T purch(  oson_this_db   );
    return purch.favorite_add( d->m_uid, list);
}

static Error_T api_favorite_del(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    const uint32_t fav_id = reader.readByte4();
    if (fav_id == 0){
        slog.WarningLog("fav_id is zero!");
        return Error_parameters;
    }
    Purchase_T purch(  oson_this_db   );
    return purch.favorite_del(fav_id);
}
 
static Error_T api_periodic_bill_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    /////////////////////////////////
    Periodic_bill_data_T search;
    search.uid = d->m_uid;

    Periodic_bill_T per_bill(  oson_this_db  );
    Periodic_bill_list_T plist;
    Sort_T sort;
    per_bill.list(search, sort, plist); // it never get error. But, may throw
    //////////////////////////////////////////////////////////////////////////////
    d-> m_writer << b2(plist.count) << b2(plist.list.size());
    
    for(const Periodic_bill_data_T & fav : plist.list)
    {
        d-> m_writer << b4(fav.id) << b4(fav.merchant_id) << b2(fav.status) << b8(fav.amount)
                     << fav.name   << fav.fields          << fav.prefix     << fav.periodic_ts;
    }
    //////////////////////////////////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_periodic_bill_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    /////////////////////////////////////
    Periodic_bill_data_T idata;
    reader >> r4(idata.id)     >> r4(idata.merchant_id) >> r8(idata.amount) >> r2( idata.status        ) 
           >> r2(idata.prefix) >> r2(idata.fields)      >> r2(idata.name)   >> r2( idata.periodic_ts   ) ;

    idata.uid = d->m_uid;

    Periodic_bill_T per_bill(  oson_this_db  );
    return per_bill.add(idata);
}

static Error_T api_periodic_bill_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    ////////////////////////////////
    Periodic_bill_data_T idata;
    reader >> r4(idata.id)     >> r4(idata.merchant_id) >> r8(idata.amount) >> r2( idata.status        ) 
           >> r2(idata.prefix) >> r2(idata.fields)      >> r2(idata.name)   >> r2( idata.periodic_ts   ) ;

    idata.uid = d->m_uid;
    
    Periodic_bill_T per_bill(  oson_this_db   );
    
    return per_bill.edit(idata.id, idata);
}

static Error_T api_periodic_bill_del(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);

    const uint32_t id = reader.readByte4();
    //@Note: check id - existing, and it is uid's bill.
    Periodic_bill_T per_bill(  oson_this_db   );
    return per_bill.del(id);
    
}

static Error_T api_news_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    std::string order_str, order_column;
    
    News_info_T search; 
    
    Sort_T sort{ 0 ,0, Order_T{ 1, 0, Order_T::ASC } }; // set order by id asc by default.
    
    reader >> r4(search.id) >> r2(order_column) >> r2(order_str) >> r4(sort.offset) >> r2(sort.limit) ;

    if(sort.offset == 0 && sort.limit == 0 )
    {
        sort.limit = 1024; // last 50 item's enough.
    }
    
    slog.DebugLog("order-str: '%s' ,  order-column: '%s'", order_str.c_str(), order_column.c_str()) ;
    
    boost::algorithm::to_lower(order_str);
    boost::algorithm::to_lower(order_column);
    
    if (order_column == "ts" )
    {
        sort.order = 3;
    }
    
    if ( order_str == "asc" )
    {
        sort.order.order= Order_T::ASC ;
    } 
    else if (order_str == "desc")
    {
        sort.order.order = Order_T::DESC ;
    }
    else
    {
        //use default:
        sort.order.order = Order_T::ASC ;
    }
    
    
    search.uid  = d->m_uid;
    Users_T users(  oson_this_db   ) ;
    search.lang = users.user_language(search.uid);
    
    News_T news(  oson_this_db   );
    News_list_T list;
    news.news_list(search, sort, list);
    ////////////////////////////////////////////////////////////////////
    //::std::reverse( ::std::begin(list.list), ::std::end(list.list)) ; // reverse , because add_ts must be ASC order.
    
    d->m_writer << b2(list.count) << b2(list.list.size());
    for(const News_info_T& info : list.list )
    {
       d-> m_writer << b4(info.id) << info.msg << info.add_time ;
    }
    /////////////////////////////////////////////////////////

    return Error_OK;
}

/******************************** E-Wallet *****************************/
namespace 
{
    
class topup_trans_req_session : public std::enable_shared_from_this< topup_trans_req_session > 
{
public:
    typedef topup_trans_req_session self_type;
    
    topup_trans_req_session();
    ~topup_trans_req_session();
    
    void set_data(api_pointer d, int32_t ewallet_id, int64_t card_id, int64_t amount ) ;
    
    void async_start();
    
private:
    void start();
    
    void on_card_info_eopc(std::string id, oson::backend::eopc::resp::card eocp_card, Error_T ec);
    void on_card_info(std::string id, oson::backend::eopc::resp::card eocp_card, Error_T ec);
    
    void on_topup_req_i( const oson::topup::webmoney::wm_request & wm_req , const oson::topup::webmoney::wm_response& wm_resp  ) ;
    void on_topup_req  ( const oson::topup::webmoney::wm_request & wm_req , const oson::topup::webmoney::wm_response& wm_resp    ) ; 
 
    void exit_error(Error_T ec  ) ;
    
    
    Error_T init_user();
    
    Error_T init_card();
    
    Error_T init_topup();
    
    Error_T send_req();
    
private:
    api_pointer d          ;
    int32_t     m_topup_id ;
    int64_t     m_card_id    ;
    int64_t     m_amount     ;
    
    
    std::string m_err_str;
    /////////////////////////////////////
    User_info_T                m_user_info      ;
    oson::Users_full_info      m_user_full_info ;
    Card_info_T                m_card_info      ;
  //  oson::ewallet::wallet_info m_wallet_info    ;
    
    
    oson::topup::info m_topup_info ;
    oson::topup::trans_info m_wll_trans ;
};

} // end noname namespace 

topup_trans_req_session::topup_trans_req_session()
   : d(), m_topup_id (), m_card_id(), m_amount()
{
       SCOPE_LOGD(slog);
}

topup_trans_req_session::~topup_trans_req_session()
{
    SCOPE_LOGD(slog);
    
    if ( static_cast< bool >  ( d->m_ssl_response ) ) {
        slog.ErrorLog("Something is wrong!");
        d->send_result(Error_internal);
    }
}

void topup_trans_req_session::exit_error(Error_T ec  ) 
{
    SCOPE_LOG(slog);
    
    slog.WarningLog("ec = %d, msg = %s ", (int)ec, m_err_str.c_str()); 
     
    if ( ! m_wll_trans.id ) // not created yet
    {
        slog.WarningLog("top-up trans row not created, yet!") ;
        return d->send_result( ec );
    }
    
    if (ec != Error_OK )
    {
        if (ec == Error_async_processing )
            m_wll_trans.status = TR_STATUS_IN_PROGRESS ;
        else    
            m_wll_trans.status = TR_STATUS_ERROR ;
        

        m_wll_trans.status_text = m_err_str;
    } else {
        m_wll_trans.status = TR_STATUS_SUCCESS ;
        m_wll_trans.status_text = "Успешно";
    }
    
    m_wll_trans.tse   = formatted_time_now_iso_S() ;
    
    oson::topup::trans_table table{ oson_this_db } ;
    
    table.update( m_wll_trans.id, m_wll_trans );
    
    
    return d->send_result( ec ); 
}

void  topup_trans_req_session::set_data(api_pointer d, int32_t topup_id, int64_t card_id, int64_t amount)
{
    SCOPE_LOG(slog);
    
    this->d            = d          ;
    this->m_topup_id   = topup_id   ; 
    this->m_card_id    = card_id    ;
    this->m_amount     = amount     ;
    
    slog.InfoLog("topup-id: %d, card-id: %lld, amount: %lld", (int)topup_id, (long long)card_id, (long long)amount);
}

void topup_trans_req_session::async_start()
{
    d->m_io_service->post(  std::bind(&self_type::start, shared_from_this() ) )  ;
}

void topup_trans_req_session::start()
{
    SCOPE_LOGD(slog);
    Error_T ec ;
    
    //1. check user
    ec = init_user();
    if ( ec ) return exit_error(ec ) ;// d->send_result(ec);

    //2. check card
    ec = init_card();
    if (ec  ) return exit_error(ec  ) ;//d->send_result(ec);

    //3. check webmoney database  -- always create row on table early than other operations.
    ec = init_topup();
    if (ec) return exit_error(ec ) ; 
    
    
    
    //4. check card-info
    using namespace std::placeholders;
    oson_eopc -> async_card_info( m_card_info.pc_token,  
            std::bind(&self_type::on_card_info_eopc, shared_from_this(), _1, _2, _3 ) ) ; 
}

Error_T topup_trans_req_session::init_user()
{
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    Users_T user_table(  oson_this_db ) ;
    
    m_user_info = user_table.get(d->m_uid, ec ) ; 
    if (ec) {
        m_err_str = "An user not found in the users table.";
        return ec ;
    }
    
    if ( m_user_info.blocked )
    {
        slog.WarningLog( "This user is blocked!" ) ;
        m_err_str = "This user is blocked.";
        return Error_blocked_user ;
    }
    
    oson::Users_full_info_table full_table{ oson_this_db } ;
    
    m_user_full_info = full_table.get_by_uid( d -> m_uid ) ;
    
    if ( ! m_user_full_info.id ) 
    {
        m_err_str = "This user does not exist in FULL REGISTERED USER LIST.";
        return Error_user_not_found ;
    }
    
    if (m_user_full_info.status  != oson::Users_full_info::Status_Enable ) 
    {
        slog.WarningLog("This FULL REGISTERED user status is not enabled. status = %d", m_user_full_info.status ) ;
        m_err_str = "This FULL REGISTERED user status is not enabled. status = " +to_str(m_user_full_info.status) ;
        return Error_operation_not_allowed ; 
    }
    
    // seems all are fine.
    
    return Error_OK ;
}
    
Error_T topup_trans_req_session::init_card()
{
    SCOPE_LOG(slog);
    
    Cards_T card_table(  oson_this_db ) ;
    
    Card_info_T c_search;
    c_search.uid = d -> m_uid ;
    c_search.id  = m_card_id    ;
    
    Error_T ec = card_table.info(c_search,  /*out*/ m_card_info ) ;
    if (ec) {
        m_err_str = "Card not found in Cards table." ;
        return Error_card_not_found;
    }
    
    if (m_card_info.foreign_card != FOREIGN_NO )
    {
        slog.ErrorLog("Card is foreign, and not activated.");
        m_err_str = "Card is foreign." ;
        return Error_card_foreign ;
    }
    
    if (m_card_info.admin_block || m_card_info.user_block )
    {
        slog.ErrorLog("This card is blocked in OSON service.") ;
        m_err_str = "Card is blocked in OSON service." ;
        return Error_card_blocked;
    }
    
    
    return Error_OK ;
}

Error_T topup_trans_req_session::init_topup()
{    
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    /** 2. check supported wallet*/
    namespace wll = oson::topup;
    
    if ( ! wll::supported( m_topup_id ) ) 
    {
        slog.ErrorLog("Unsupported wallet-id: %d", (int)m_topup_id ) ;
        
        m_err_str = "Unsupported wallet-id: " + to_str(m_topup_id) ;
        
        return Error_operation_not_allowed ;
    }
    ;
    wll::table wtable{ oson_this_db } ;
    
    wll::info winfo = wtable.get( m_topup_id, ec);
    if ( ec ) return ec;
    
    if ( ! ( m_amount >= winfo.min_amount && m_amount <= winfo.max_amount) )
    {
        slog.WarningLog("amount limit exceeded. min-amount: %lld, max-amount: %lld, amount: %lld", 
                (long long)(winfo.min_amount), (long long)(winfo.max_amount), (long long)(m_amount));
        
        return (m_amount < winfo.min_amount) ? Error_amount_is_too_small : Error_amount_is_too_high ;
    }
    
    Currency_info_T ci = oson_merchant_api -> currency_now_or_load( Currency_info_T::Type_Uzb_CB ) ; 
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        m_err_str = "Can't take currency";
        return Error_internal;
    }
    
    const ::std::int64_t amount_with_comission = static_cast< ::std::int64_t >( m_amount + winfo.rate * 1.0 * m_amount / 10000.00 );
    
    const double amount_req = ::std::ceil( ci.usd( amount_with_comission ) * 100.0  ) / 100.0 ;//convert it to dollar , and truncate 2 digits after dot.
    
    slog.DebugLog("amount: %lld, amount-with-commission: %lld, amount-req: %.2f", 
                    (long long)(m_amount), (long long)(amount_with_comission), amount_req);
    
    if ( amount_req <  1.0E-2 ){ // less than 1 cent, can't
        
        slog.ErrorLog("amount too small");
        
        m_err_str = "amount too small: " + to_str(m_amount/100.0, 2, false) + " sum." ;
        
        return Error_amount_is_too_small ;
    }
    
    
    wll::trans_info wt_info;
    wt_info.id             = 0             ;
    wt_info.topup_id       = m_topup_id    ;
    wt_info.amount_sum     = m_amount        ;     
    wt_info.amount_req     = amount_req    ;
    wt_info.currency       = static_cast< wll::trans_info::integer >( wll::Currency::usd );
    wt_info.uid            = d->m_uid      ;
    wt_info.login          = "0";
    wt_info.pay_desc       = "Попольнение узкарт из кошелька" ;
    wt_info.ts             = formatted_time_now_iso_S()    ;
    wt_info.tse            = wt_info.ts ;
    wt_info.status         = TR_STATUS_REGISTRED           ;
    wt_info.status_text    = "registered"    ;
    wt_info.card_id        = m_card_id       ;
    wt_info.card_pan       = m_card_info.pan ;
    wt_info.eopc_trn_id    = "0"             ;
    wt_info.oson_card_id   = winfo.card_id   ;
    
    wll::trans_table wt_table{ oson_this_db } ;
    
    wt_info.id = wt_table.add( wt_info ) ;
    
    m_topup_info  = winfo ;
    m_wll_trans   = wt_info ;
    
    return Error_OK ;
}


void topup_trans_req_session::on_card_info_eopc( std::string id, oson::backend::eopc::resp::card eocp_card, Error_T ec )
{
    d->m_io_service->post( std::bind(&self_type::on_card_info,  shared_from_this(), id, eocp_card, ec ) ) ;
}

void topup_trans_req_session::on_card_info(std::string id, oson::backend::eopc::resp::card eocp_card, Error_T ec)
{
    SCOPE_LOGD(slog);
    
    /***************************************************************/
    if ( ec ) 
    {
        slog.ErrorLog("Can't take card -info") ;
        m_err_str = "Can't take card-info. ec = " + to_str(ec) + ", msg = " + oson::error_str(ec) ;
        
        return exit_error(ec);
    }
    
    if (eocp_card.status != 0 ) 
    {
        slog.ErrorLog("Card status is not zero: %d", eocp_card.status ) ;
        m_err_str = "Card is blocked. status from EOPC = " + to_str(eocp_card.status) ;
        return exit_error(Error_card_blocked );
    }
    
    if ( eocp_card.phone.empty() )
    {
        slog.ErrorLog("Card owner phone not found");
        
        m_err_str = "EOPC card owner phone is not set.";
        
        return exit_error(Error_card_blocked) ;
    }
    
    if (eocp_card.phone  != m_card_info.owner_phone )
    {
        slog.ErrorLog("Owner phone changed.") ;
        
        m_card_info.foreign_card = FOREIGN_YES ;
        Cards_T card_table{ oson_this_db } ;
        card_table.card_edit(m_card_id, m_card_info ) ;

        m_err_str = "Card owner phone changed." ;
        return exit_error( Error_card_owner_changed ) ;
    }
    /***************************************************************/
    
    //Continue where we stop;
    
    ec = send_req();
    
    if ( ec && ec != Error_async_processing )
    {
        return exit_error(ec);
    }
    
    return d->send_result(ec);
}


Error_T topup_trans_req_session::send_req()
{
    SCOPE_LOG(slog);
    
    //Error_T ec = Error_OK ;
    
    namespace wll = oson::topup ;
    
    wll::webmoney::wm_request  wm_req;
    wm_req.url       = "https://psp.paymaster24.com/Payment/Init" ;
    wm_req.amount    = to_str( m_wll_trans.amount_req, 2, true ) ;
    wm_req.currency  = "USD"      ;
    wm_req.test_mode = "2"        ;
    wm_req.pay_desc  = "OSON.UZ"  ;
    wm_req.trn_id    = to_str( m_wll_trans.id ) ;
    
    
    wll::webmoney::wm_access  wm_acc;
    
    wm_acc.id_company = "ac15a23f-22a6-4fde-b0b1-572ef2e4192c" ;
    
    wll::webmoney::wm_manager wm_mng{ wm_acc } ;
    
    /****************************************************************************************/
    //wll::webmoney::wm_response wm_resp   =  wm_mng.payment_request(wm_req,  /*out*/ ec ) ;
    /*******************************************************************************************/
    auto wm_handler = std::bind(& self_type::on_topup_req_i, shared_from_this(), std::placeholders::_1, std::placeholders::_2 ) ;
    
    wm_mng.async_payment_request ( wm_req, wm_handler ) ;
    
    return Error_async_processing ;
}
     
void topup_trans_req_session::on_topup_req_i( const oson::topup::webmoney::wm_request & wm_req, const oson::topup::webmoney::wm_response& wm_resp )
{
    d->m_io_service->post( std::bind(&self_type::on_topup_req, shared_from_this(), wm_req, wm_resp ) ) ;
}

void topup_trans_req_session::on_topup_req(const oson::topup::webmoney::wm_request & wm_req, const oson::topup::webmoney::wm_response& wm_resp)
{
    SCOPE_LOGD(slog);
    
    if ( ! wm_resp.is_ok() ) 
    {
        slog.ErrorLog(" webmoney resp status = %lld, text: %s", (long long)wm_resp.status_value, wm_resp.status_text.c_str());
        
        m_err_str = wm_resp.status_text ;
        
        return exit_error( Error_wallet_operation ) ; 
    }
    
    d->m_writer << wm_resp.url ; 
    
    //@Note: There needn't update ewallet_trans table.
    return d->send_result( Error_OK );
}



static Error_T api_topup_trans_request(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN( d, reader ) ;
    ///////////////////////////////
    int32_t const topup_id = reader.readByte4() ;
    int64_t const card_id    = reader.readByte8() ;
    int64_t const amount     = reader.readByte8() ;
    
    ////////////////////////////////////////
    slog.InfoLog("ewallet-id: %d, card-id: %lld, amount: %lld\n", (int)topup_id, (long long)card_id, (long long)amount);
    
    if ( ! topup_id || !card_id || !amount )
    {
        slog.WarningLog("parameters are zero!");
        return Error_parameters ;
    }
    
    auto session  = std::make_shared< topup_trans_req_session > () ;

    session->set_data(d, topup_id, card_id, amount ); 

    session->async_start() ;

    return Error_async_processing ;
}

static Error_T  api_topup_webmoney_invoice_confirmation(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    std::string lmi_prerequest,
            lmi_merchant_id,
            lmi_payment_no,
            lmi_payment_amount,
            lmi_currency,
            lmi_paid_amount,
            lmi_paid_currency,
            lmi_payment_system,
            lmi_sim_mode,
            lmi_payment_desc;
    
    reader >> r2(lmi_prerequest)   >> r2(lmi_merchant_id)   >> r2(lmi_payment_no)     >> r2(lmi_payment_amount) >> r2( lmi_currency     )
           >> r2(lmi_paid_amount)  >> r2(lmi_paid_currency) >> r2(lmi_payment_system) >> r2(lmi_sim_mode)       >> r2( lmi_payment_desc ) 
            ;
    
    slog.DebugLog("\nLMI_PREREQUEST: %s,\nLMI_MERCHANT_ID: %s,\nLMI_PAYMENT_NO: %s,\nLMI_PAYMENT_AMOUNT: %s,\nLMI_CURRENCY: %s,\nLMI_PAID_AMOUNT: %s,\nLMI_PAID_CURRENCY: %s,\nLMI_PAYMENT_SYSTEM: %s,\nLMI_SIM_MODE: %s,\nLMI_PAYMENT_DESC: %s\n",
                 lmi_prerequest.c_str(), 
                 lmi_merchant_id.c_str(), 
                 lmi_payment_no.c_str(), 
                 lmi_payment_amount.c_str(), 
                 lmi_currency.c_str(), 
                 lmi_paid_amount.c_str(), 
                 lmi_paid_currency.c_str(), 
                 lmi_payment_system.c_str(), 
                 lmi_sim_mode.c_str(), 
                 lmi_payment_desc.c_str() 
    ) ;
    
    std::string no_ans = "NO", yes_ans = "YES";
    
    std::int64_t trn_id = string2num(lmi_payment_no);
    if (trn_id <= 0 ){
        slog.ErrorLog("Invalid  payment no!");
        d->m_writer << no_ans;
        return Error_OK ;
    }
    
    namespace wll = oson::topup;
    wll::trans_table  trans_table(oson_this_db);
    
    Error_T ec = Error_OK ;
    wll::trans_info trans_info = trans_table.get_by_id(trn_id, ec);
    
    if ( ! trans_info.id ) {
        slog.ErrorLog("Not found!");
        d->m_writer << no_ans;
        return Error_OK ;
    }
    
    if (  trans_info.status != TR_STATUS_REGISTRED )
    {
        slog.ErrorLog("status is not TR_STATUS_REGISTRED.  status: %d ", trans_info.status ) ;
        d->m_writer << no_ans;
        return Error_OK ;
    }
    
    trans_info.status       = TR_STATUS_IN_PROGRESS;
    trans_info.status_text  = "invoice confirmed";
    trans_info.tse          = formatted_time_now_iso_S();
    
    trans_table.update(trans_info.id, trans_info ) ;
    
    
    d->m_writer << yes_ans;
    
    return Error_OK ;
}



struct topup_transaction_session: public  std::enable_shared_from_this< topup_transaction_session > 
{
    typedef topup_transaction_session  self_t;
    ///////////////////////////////////////////
    api_pointer d;
    oson::topup::trans_info trans_info;
    
    Card_info_T       user_card;
    Card_topup_info_T oson_card;
    Bank_info_T       bank_info;
    ////////////////////////////////////////
    
    topup_transaction_session(api_pointer d, oson::topup::trans_info info)
            ;
    ~topup_transaction_session();
    
    void async_start();
    
    void start();
    
    void exit_trans(Error_T ec, const std::string& msg_err ) ;
    
    void on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& tran, Error_T ec);
    void on_card_info(const std::string& id, const oson::backend::eopc::resp::card& tran, Error_T ec);
    
    void on_debit_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    void on_debit(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    
    
    void on_p2p_credit_eopc(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& tran, Error_T ec);
    void on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& tran, Error_T ec);
    
    void reverse_debit();
    
    
};


static Error_T api_topup_webmoney_payment_noficiation(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    std::string lmi_merchant_id,
            lmi_payment_no,
            lmi_sys_payment_id,
            lmi_sys_payment_date,
            lmi_payment_amount,
            lmi_currency,
            lmi_paid_amount,
            lmi_paid_currency,
            lmi_payment_system,
            lmi_sim_mode,
            lmi_payment_desc,
            lmi_hash,
            lmi_payer_identifier,
            lmi_payer_country,
            lmi_payer_passport_country,
            lmi_payer_ip_address ;
    
    reader >> r2 ( lmi_merchant_id)      >> r2(lmi_payment_no)     >> r2(lmi_sys_payment_id) >> r2(lmi_sys_payment_date)
           >> r2 ( lmi_payment_amount)   >> r2(lmi_currency)      >> r2(lmi_paid_amount)    >> r2(lmi_paid_currency)
           >> r2 ( lmi_payment_system  ) >> r2(lmi_sim_mode)      >> r2(lmi_payment_desc)   >> r2(lmi_hash)
           >> r2 ( lmi_payer_identifier) >> r2(lmi_payer_country) >> r2(lmi_payer_passport_country) >> r2(lmi_payer_ip_address ) 
           ;
    
    slog.InfoLog("LMI_MERCHANT_ID: %s\nLMI_PAYMENT_NO: %s\nLMI_SYS_PAYMENT_ID: %s\nLMI_SYS_PAYMENT_DATE: %s\nLMI_PAYMENT_AMOUNT: %s\nLMI_CURRENCY: %s\nLMI_PAID_AMOUNT: %s\n"
                "LMI_PAID_CURRENCY: %s\nLMI_PAYMENT_SYSTEM: %s\nLMI_SIM_MODE: %s\nLMI_PAYMENT_DESC: %s\nLMI_HASH: %s\nLMI_PAYER_IDENTIFIER: %s\n"
                "LMI_PAYER_COUNTRY: %s\nLMI_PAYER_PASSPORT_COUNTRY: %s\nLMI_PAYER_IP_ADDRESS: %s\n",
                lmi_merchant_id.c_str(), 
                lmi_payment_no.c_str(),
                lmi_sys_payment_id.c_str(),
                lmi_sys_payment_date.c_str(),
                lmi_payment_amount.c_str(),
                lmi_currency.c_str(),
                lmi_paid_amount.c_str(),
                lmi_paid_currency.c_str(),
                lmi_payment_system.c_str(),
                lmi_sim_mode.c_str(),
                lmi_payment_desc.c_str(),
                lmi_hash.c_str(),
                lmi_payer_identifier.c_str(),
                lmi_payer_country.c_str(),
                lmi_payer_passport_country.c_str(),
                lmi_payer_ip_address.c_str()
            );
    /***1. check hash*/
    char        const delimiter = ';';
    std::string const secret_key = "Oson2018";
    
    std::string hash_raw = 
            lmi_merchant_id      + delimiter + 
            lmi_payment_no       + delimiter +
            lmi_sys_payment_id   + delimiter +
            lmi_sys_payment_date + delimiter + 
            lmi_payment_amount   + delimiter + 
            lmi_currency         + delimiter + 
            lmi_paid_amount      + delimiter + 
            lmi_paid_currency    + delimiter + 
            lmi_payment_system   + delimiter + 
            lmi_sim_mode         + delimiter + 
            secret_key ;
    
    std::string hash_calc =   oson::utils::encodebase64(  oson::utils::sha256_hash(hash_raw, true) ) ;
    slog.DebugLog("hash-raw: %s\nhash-calc: %s\n", hash_raw.c_str(), hash_calc.c_str());
    bool const check_hash = hash_calc == lmi_hash ;
    
    if ( ! check_hash )
    {
       slog.WarningLog("hash not verified!");
    }
    else {
       slog.DebugLog("hash is valid!");
    } 
   
    
    std::int64_t trn_id = string2num(lmi_payment_no);
    
    if (trn_id <= 0 ){
        slog.ErrorLog("Invalid  payment no!");
        return Error_not_found ;
    }
    
    namespace wll = oson::topup;
    wll::trans_table  trans_table(oson_this_db);
    
    Error_T ec = Error_OK ;
    wll::trans_info trans_info = trans_table.get_by_id(trn_id, ec);
    
    if ( ! trans_info.id ) {
        slog.ErrorLog("Not found!");
        return Error_not_found ;
    }
    
    if ( ! check_hash  )
    {
        trans_info.status       = TR_STATUS_ERROR;
        trans_info.status_text  = "payment notified. hash incorrect.";
        trans_info.tse          = formatted_time_now_iso_S();
        trans_info.login        = lmi_payer_identifier;
        trans_info.topup_trn_id = string2num(lmi_sys_payment_id);

        trans_table.update(trans_info.id, trans_info ) ;

        return Error_not_found ;
    }
    
    trans_info.status       = TR_STATUS_IN_PROGRESS;
    trans_info.status_text  = "payment notified";
    trans_info.tse          = formatted_time_now_iso_S();
    trans_info.login        = lmi_payer_identifier;
    trans_info.topup_trn_id = string2num(lmi_sys_payment_id);
    
    trans_table.update(trans_info.id, trans_info ) ;
    
    
    auto session = std::make_shared< topup_transaction_session >(d, trans_info ) ;
    session->async_start();

    
    /****************** NO WAIT **********************/
    d->m_writer << std::string("YES") ;
    
    return Error_OK ;
}

/***************************************************************************************************/
topup_transaction_session::topup_transaction_session( api_pointer d, oson::topup::trans_info info)
: d(d), trans_info(info)
{
    SCOPE_LOG(slog);
}

topup_transaction_session::~topup_transaction_session()
{
    SCOPE_LOG(slog);
    if (trans_info.id != 0 || static_cast< bool >(d->m_ssl_response) ) {
        slog.WarningLog("something is wrong!");
        exit_trans(Error_internal, "Unexpected inner error!");
    }
}

void topup_transaction_session::async_start()
{
    d->m_io_service->post( std::bind(&self_t::start, shared_from_this() ) ) ;
}

void topup_transaction_session::start()
{
    SCOPE_LOGD(slog);
    
    Error_T ec = Error_OK ;
    //1. get user card
    Cards_T card_table ( oson_this_db ) ;
    this->user_card = card_table.get(trans_info.card_id, ec ) ;
    if (ec) return exit_trans(ec, "User card not found!");
     
    this->oson_card = card_table.get_topup_by_id(trans_info.oson_card_id, ec ) ;
    if (ec) return exit_trans(ec, "oson-topup-card not found");
    
    
    Bank_T bank_table(oson_this_db);
    Bank_info_T bank_search;
    
    bank_search.bin_code = user_card.pan;
    bank_search.status   = Bank_info_T::ES_active ;
    
    this->bank_info = bank_table.info(bank_search, ec ) ;
    if (ec) return exit_trans(ec, "bank info not found for " + user_card.pan + " pan!" );
     
    auto eopc_api = oson_eopc;

    //1. card-info oson_card
    using namespace std::placeholders;
    eopc_api ->async_card_info(oson_card.pc_token, std::bind(&self_t::on_card_info_eopc, shared_from_this(), _1,_2,_3));
}

void topup_transaction_session::on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& tran, Error_T ec)
{
    d->m_io_service->post( std::bind( &self_t::on_card_info, shared_from_this(), id, tran, ec) ) ;
}

void topup_transaction_session::on_card_info(const std::string& id, const oson::backend::eopc::resp::card& tran, Error_T ec)
{
    SCOPE_LOGD(slog);
    
//    if (ec) return exit_trans(ec, "EOPC error on-card-info: " + to_str(ec) );
//    if (tran.status != 0 ){
//        slog.WarningLog("tran.status <> 0:   %d ", tran.status);
//        return exit_trans(Error_card_blocked, "EOPC on-card-info: card status not zero: " + to_str(tran.status) );
//    }
    
//    if (tran.balance < trans_info.amount_sum )
//    {
//        slog.WarningLog("balance not enough! required sum: %lld (TIYIN),  card-balance: %lld (TIYIN)", (long long)trans_info.amount_sum, (long long)tran.balance);
//        return exit_trans(Error_not_enough_amount, "Not enough amount!");
//    }
    
    ////////////////////////////////////////////////////////////////////////////////////////
    //            Debit
     ////////////////////////////////////////////////////////////////////////////////////////
    int64_t const trn_id = trans_info.id; 
    EOPC_Debit_T in;

    in.amount      = trans_info.amount_sum  + bank_info.commission(trans_info.amount_sum);
    in.cardId      = oson_card.pc_token    ;
    in.merchantId  = bank_info.merchantId  ;
    in.terminalId  = bank_info.terminalId  ;
    in.port        = bank_info.port        ;
    in.ext         = num2string(trn_id)    ;
    in.stan        = make_stan(trn_id)     ;

    using namespace std::placeholders;
    oson_eopc ->async_trans_pay( in,  std::bind(&self_t::on_debit_eopc, shared_from_this(),  _1, _2, _3 )   );
    
}
    

void topup_transaction_session::on_debit_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_t::on_debit, shared_from_this(), debin, tran, ec ) ) ;
}

void topup_transaction_session::on_debit(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD(slog);
    
    //@Note refNum  available anyway. so save it before handle errors.
    trans_info.eopc_trn_id = tran.refNum ;
    
    if (ec) return exit_trans(ec, "EOPC on debit error: " + to_str(ec) );
    if ( ! tran.status_ok() || tran.resp != 0 ) 
    {
        slog.WarningLog("Can't debit!");
        return exit_trans(Error_transaction_not_allowed, "EOPC on debit no OK or resp <> 0 :  " );
    }
    
    /****************************************************/
    trans_info.status      = TR_STATUS_IN_PROGRESS ;
    trans_info.status_text = "success debit";
    trans_info.tse         = formatted_time_now_iso_S();
    
    namespace wll = oson::topup;
    wll::trans_table trans_table(oson_this_db);
    trans_table.update(trans_info.id, trans_info);
    /************************************************/
    
    
    
    int64_t const trn_id = trans_info.id; 

    EOPC_Credit_T credit      ;
    credit.amount      = trans_info.amount_sum;
    credit.merchant_id = bank_info.merchantId;
    credit.terminal_id = bank_info.terminalId;
    credit.port        = bank_info.port;
    credit.ext         = num2string(trn_id);
    credit.card_id     = user_card.pc_token;
    
 
    using namespace std::placeholders;
    oson_eopc ->async_p2p_credit( credit, std::bind(&self_t::on_p2p_credit_eopc, shared_from_this(), _1, _2, _3 ) ) ;
    
}
void topup_transaction_session::on_p2p_credit_eopc(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& tran, Error_T ec)
{
    d->m_io_service->post( std::bind( &self_t::on_p2p_credit,shared_from_this(), credit, tran, ec));
}

void topup_transaction_session::on_p2p_credit(const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& tran, Error_T ec)
{
    SCOPE_LOGD(slog);
    const bool success = ec == Error_OK && tran.resp == 0 ;
    
    if ( ! success ){
        reverse_debit();
    }

    //@Note refNum available anyway. so save it before handle error.
    if ( ! tran.refNum.empty() ) {
        trans_info.eopc_trn_id += ";" + tran.refNum;
    }
    
    if (ec) return exit_trans(ec, "EOPC p2p-credit error: " + to_str(ec) );
    if (tran.resp != 0) return exit_trans(Error_eopc_error, "tran.resp not zero: " + to_str(tran.resp));
    
    
    trans_info.status      = TR_STATUS_SUCCESS;
    trans_info.status_text += ", success credit";
    
    trans_info.tse         = formatted_time_now_iso_S();
    
    
    //finish with success.
    return exit_trans ( Error_OK, "SUCCESS" );
}

void topup_transaction_session::reverse_debit()
{
    SCOPE_LOGD(slog);
    using namespace std::placeholders;
    
    auto eopc_api = oson_eopc;
    
    eopc_api->async_trans_reverse(trans_info.eopc_trn_id, oson::trans_reverse_handler() ) ;//needn't any handler.
}
            
void topup_transaction_session::exit_trans(Error_T ec, const std::string& msg_err)
{
    SCOPE_LOG(slog);
    
    oson::topup::trans_table table(oson_this_db);
    
    if (ec)
    {
        trans_info.status = TR_STATUS_ERROR ;
        
        if (!trans_info.status_text.empty())
            trans_info.status_text += ", " ;
        
        trans_info.status_text +=  msg_err;
        trans_info.tse = formatted_time_now_iso_S();
    } 
    else 
    {
        trans_info.status = TR_STATUS_SUCCESS; 
    }
    
    table.update(trans_info.id, trans_info);
    
    trans_info.id = 0;//no update
}


Error_T api_topup_webmoney_payment_finish_trans(api_pointer_cref d , ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
     //LMI_MERCHANT_ID
        //LMI_PAYMENT_NO
        //LMI_SYS_PAYMENT_ID
        //LMI_SYS_PAYMENT_DATE
        //LMI_PAYMENT_AMOUNT
        //LMI_CURRENCY
       
    std::int32_t success = 0;
    
    std::string lmi_merchant_id      ,  
                lmi_payment_no       ,
                lmi_sys_payment_id   ,
                lmi_sys_payment_date ,
                lmi_payment_amount   ,
                lmi_currency         ;
    
    reader >> r4(success) >> r2(lmi_merchant_id) >> r2(lmi_payment_no) >> r2(lmi_sys_payment_id) >> r2(lmi_sys_payment_date ) >> r2(lmi_payment_amount) >> r2(lmi_currency);
    
    slog.InfoLog("success: %d,\n LMI_MERCHANT_ID: %s\nLMI_PAYMENT_NO: %s\nLMI_SYS_PAYMENT_ID: %s\nLMI_SYS_PAYMENT_DATE: %s\nLMI_PAYMENT_AMOUNT: %s\nLMI_CURRENCY: %s\n", 
    (int)success, lmi_merchant_id.c_str(), lmi_payment_no.c_str(), lmi_sys_payment_id.c_str(), lmi_sys_payment_date.c_str(), lmi_payment_amount.c_str(), lmi_currency.c_str());
    
    return Error_OK ;
}
 

 Error_T api_topup_merchant_list (api_pointer_cref d, ByteReader_T& reader )
 {
     SCOPE_LOGD(slog);
     OSON_PP_USER_LOGIN(d, reader);
     
     
     int32_t id = 0, status = 0 , offset, limit ;
     reader >> r4( id )  >> r4( status ) >> r4(offset) >> r2(limit);
     
     
     /////////////////////////
     namespace topup = oson::topup;
     
     struct topup::search search;
     search.id = id;
     search.status = status;
     
     Sort_T sort(offset, limit, Order_T(8, 0, Order_T::DESC ) ) ;//8 - position.
     
     topup::table  table(  oson_this_db ) ;
     
     topup::info_list list = table.list(search, sort);
     
     oson::icons::table icon_table( oson_this_db ) ;
     
     /////////////////////
     d->m_writer << b4( list.size() ) ;
     for(const topup::info& e  : list ) 
     {
         std::string icon_url;
         /**@Note: Optimize it later! */
         if ( e.icon_id != 0 ) {
             icon_url = icon_table.get( e.icon_id ).path_hash ;
         }
         
         d->m_writer << b4(e.id) << e.name << b4(e.status) << b4(e.option) << b8(e.min_amount)
                     << b8(e.max_amount)   << b4(e.rate  ) << b4(e.position) << icon_url ;
     }
     
     return Error_OK ;
 }

/******************************* END E-WALLET ***************************************/

static Error_T api_bill_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    ////////////////////////////////////////////////////////////////////////////////
    Bill_data_search_T search_data;
    search_data.uid     = d->m_uid;
    search_data.status  = BILL_STATUS_REGISTRED;
    Sort_T sort;
    std::string sort_column, sort_order;
    reader >> r4(sort.offset) >> r2(sort.limit) >> r2(sort_column) >> r2(sort_order);
    
    sort.order.order = make_order_from_string(sort_order);
    sort.offset = oson::utils::clamp(sort.offset, 0, INT_MAX);
    
    if (sort.limit <= 0 || sort.limit > 100 ) 
    {
        sort.limit = 100;
    }
    if (d->m_uid == 0 || d->m_uid == -1 ) {
        slog.ErrorLog("uid is zero");
        return Error_login_failed;
    }
    Bills_T bills(  oson_this_db  );
    Bill_data_list_T blist = bills.list(search_data, sort  );
    ////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(blist.count) << b4(blist.list.size());
    
    for( const Bill_data_T & bill :  blist.list )
    {
         d->m_writer << b8(bill.id)  << b8(bill.amount) << b4(bill.merchant_id) << bill.fields
                     << bill.add_ts  << bill.phone      << bill.comment ;
    }
    ///////////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_bill_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    ///////////////////////////////////////
    Bill_data_T input_data;

    reader >> r8(input_data.id) >> r8(input_data.amount) >> r4(input_data.merchant_id) >> r2(input_data.phone) >> r2(input_data.fields);
    
    input_data.uid2 = d->m_uid;
    input_data.status = BILL_STATUS_REGISTRED;

    Error_T ec = Error_OK;
    Users_T users(  oson_this_db   ) ;
    User_info_T dst_user = users.info(input_data.phone, /*OUT*/ ec ) ;
    
    if( ec ) return ec;
    
    input_data.uid = dst_user.id;

    //add min/max amount check
    Merchant_T merch(  oson_this_db   );
    Merchant_info_T merch_info = merch.get(input_data.merchant_id, ec);
    if (ec) return ec;
    
    ec = merch_info.is_valid_amount(input_data.amount);
    if (ec) return ec;
    
    Bills_T bills(  oson_this_db   );
    int64_t  bill_id = bills.add( input_data );
    
    std::string msg = "Оплатите мой счёт, пожалуйста.";
    Users_notify_T users_n(  oson_this_db   );
    users_n.notification_send(dst_user.id, msg, MSG_TYPE_REQUEST);
    
    d->m_writer << b8( bill_id );
    return Error_OK ;
}

namespace 
{
class bill_edit_session: public  std::enable_shared_from_this< bill_edit_session >
{
private:
    typedef bill_edit_session self_type;
    
    api_pointer d;
    int64_t     bill_id ; 
    int16_t     accept  ;
    int64_t     card_id ;
    
    /////////////////////////
    Merchant_info_T merchant  ;
    Purchase_info_T p_info    ;
    Bill_data_T     bill      ;
    User_info_T     user_info ;
    Card_info_T     card_info ;
    Merch_trans_T   trans     ;
    
public:
    bill_edit_session(api_pointer d, int64_t bill_id, int16_t accept, int64_t card_id);
    
    ~bill_edit_session();
    
    void  async_start();
    
private:
    
    void update_purchase();
    
    void start() ;
    
    Error_T handle_start() ;
    
    void on_card_info_eopc(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec) ;
    
    void on_card_info(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec) ;
    
    Error_T handle_card_info(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec);
    
    void on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    
    void on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
    
    Error_T handle_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec);
};
    
} //end noname namespace

bill_edit_session::bill_edit_session(api_pointer d, int64_t bill_id, int16_t accept, int64_t card_id)
: d(d), bill_id(bill_id), accept(accept), card_id(card_id)
{
    SCOPE_LOGF_C(slog);
   
}

bill_edit_session::~bill_edit_session()
{
    SCOPE_LOGF_C(slog);
    
    if ( p_info.id != 0 )
    {
        update_purchase();
    }

    if ( static_cast< bool > ( d->m_ssl_response )  ) // something is wrong, send error internal.
    {
        slog.WarningLog("Something is wrong with ClientApi, send error internal!");
        d->send_result( Error_internal );
    }
}

void bill_edit_session::async_start()
{
    d->m_io_service->post( ::std::bind(&self_type::start, shared_from_this())) ;
}


void bill_edit_session::update_purchase()
{
    Purchase_T purch(  oson_this_db  ) ;
    purch.update(p_info.id, p_info);
}

void bill_edit_session::start()
{
    SCOPE_LOGD_C(slog);
    Error_T ec = handle_start();
        
    d->send_result(ec);
}

Error_T bill_edit_session::handle_start()
{
    SCOPE_LOGD_C(slog);
    Error_T ec;
    Bills_T bills(  oson_this_db  );
    this->bill = bills.get( bill_id, ec);
    if( ec )return ec;

    if(accept == 0) 
    {
        ec = bills.set_status(bill_id, BILL_STATUS_DECLINED);

        if ( ! bill.is_business_bill() ) 
        {
            std::string msg =  "Ваша просьба оплатить счёт отклонена.";
            Users_notify_T users_n(  oson_this_db  );
            users_n.notification_send( bill.uid2, msg, MSG_TYPE_REQUEST );
        }

        return ec;
    }

    const int32_t merchant_id = bill.merchant_id;
    /*const*/ int64_t amount      = bill.amount;
    //////////////////////////////////////////////////////////
    ///   5. merchant info
    //////////////////////////////////////////////////////////
    Merchant_T merchant_table(  oson_this_db  );
    this->merchant = merchant_table.get(merchant_id, ec);
    if(ec) return ec; // may be merchant not found OR merchant_id - invalid value.

    if (merchant.status == MERCHANT_STATUS_HIDE ){
        if ( ! allow_master_code( d->m_uid )){
            slog.ErrorLog("This merchant is disabled!");
            return Error_operation_not_allowed;
        }
    }
    ///////////////////////////////////////////////////////////////////
    const int64_t commission  = merchant.commission( amount ) ; 
    if ( merchant.commission_subtracted() )  //merchant_identifiers::commission_subtracted(  merchant_id ) )
    {
        int64_t new_amount = amount - commission;

        amount = new_amount;
    }
    //////////////////////////////////////////////////////////
    ///   6. card info
    //////////////////////////////////////////////////////////

    Cards_T card(  oson_this_db  );
    Card_info_T card_search     ;
    card_search.id         = card_id       ;
    card_search.is_primary = PRIMARY_UNDEF ;
    card_search.uid        = d->m_uid      ;

    ec = card.info(card_search, this->card_info);
    if(ec != Error_OK) return ec;

    if (card_info.foreign_card == FOREIGN_YES ) {
        slog.WarningLog("Foreign card!");
        return Error_card_foreign;
    }

    //////////////////////////////////////////////////////////
    ///   7. user- info
    //////////////////////////////////////////////////////////
    Users_T users(  oson_this_db  ) ;
    this->user_info = users.get( d->m_uid, ec );
    if(ec != Error_OK) return ec;

    if (user_info.blocked){
        slog.WarningLog("An user is blocked!");
        return Error_operation_not_allowed;
    }
    ///////////////////////////////////////////////////////////
    const std::string login = bill.get_login();
    //////////////////////////////////////////////////////////
    ///   8. check login in merchant
    //////////////////////////////////////р////////////////////

    //Merch_trans_T trans;
    this->trans.amount = amount;
    this->trans.param = login;
    
    bool const can_check = ! ( merchant.api_id   ==  merchant_api_id::nonbilling ) ;
    if (  can_check   ) 
    {
        Merch_acc_T acc;
        merchant_table.acc_info(merchant.id, acc);
        if(merchant.api_id == merchant_api_id::ums || 
           merchant.api_id == merchant_api_id::paynet)
        {
            merchant_table.api_info(merchant.api_id, acc);
        }
        trans.transaction_id = merchant_table.next_transaction_id();
        trans.user_phone =   user_info.phone;
        if (merchant.api_id == merchant_api_id::paynet && trans.service_id_check > 0 )
        {
            trans.user_phone = oson::random_user_phone();
        }


        Merchant_api_T merch_api( merchant, acc);
        Merch_check_status_T check_status;
        ec = merch_api.check_status(trans, check_status);
        if(ec != Error_OK)
             return ec;

         if( !check_status.exist ) 
         {
             return Error_transaction_not_allowed;
         }
    }
    // Register request, this->p_info
    p_info.amount      = amount        ;
    p_info.mID         = merchant_id   ;
    p_info.uid         = d->m_uid      ;
    p_info.login       = login         ;
    p_info.eopc_trn_id = "0"           ;
    p_info.pan         = card_info.pan ;
    p_info.ts          = formatted_time_now_iso_S();
    p_info.status      = TR_STATUS_REGISTRED;
    p_info.receipt_id  = bill.id       ;
    p_info.commission  = commission    ;
    p_info.card_id     = card_id       ;

     /********************************************************************/
    if ( login.empty() ){
        if (   merchant.api_id == merchant_api_id::nonbilling  ) // if this direct merchant
            p_info.login = user_info.phone ;
    }
    /***********************************************************/


    Purchase_T purchase(  oson_this_db  );

    p_info.id = purchase.add(p_info );

    p_info.status = TR_STATUS_ERROR ; // candidate.

    trans.check_id = p_info.id;
    
    oson_eopc -> async_card_info( card_info.pc_token, std::bind(&self_type::on_card_info_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)) ;

    return Error_async_processing ;
}

void bill_edit_session::on_card_info_eopc(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec)
{
    d->m_io_service -> post( std::bind(&self_type::on_card_info, shared_from_this(), id, eocp_card, ec ) ) ;
}

void bill_edit_session::on_card_info(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec)
{
    SCOPE_LOGD_C(slog);
    ec = handle_card_info(id, eocp_card, ec);

    return d->send_result(ec);
}

Error_T bill_edit_session::handle_card_info(std::string const& id, oson::backend::eopc::resp::card const& eocp_card, Error_T ec)
{
    SCOPE_LOGD_C(slog);
    if (ec != Error_OK)
        return ec;
    ///////////////////////////////////////////////////////////////////////////////////
    if ( card_info.owner_phone.size() > 0 ){
        if (eocp_card.phone.size() > 0 && card_info.owner_phone != eocp_card.phone){
            slog.ErrorLog("card-owner-phone: %s,  eopc-card-phone: %s", card_info.owner_phone.c_str(), eocp_card.phone.c_str());
            p_info.merch_rsp = "card-owner changed";
            return Error_card_owner_changed;
        }
    } else {
        if (eocp_card.phone.size() > 0 ) {
            card_info.owner_phone = eocp_card.phone;
            Cards_T card( oson_this_db ) ;
            card.card_edit_owner(card_info.id, eocp_card.phone);
        }
    }

    if (eocp_card.status != VALID_CARD) {
        slog.WarningLog("Pay from blocked card");
        p_info.merch_rsp = "Blocked EOCP card. status = " + to_str( eocp_card.status );
        return Error_card_blocked;
    }

    int64_t const amount      =  p_info.amount;
    int64_t const commission  =  p_info.commission;

    if( eocp_card.balance <  (amount + commission) ) {
        slog.ErrorLog("Not enough amount");
        p_info.merch_rsp = "Not enough amount";
        return Error_not_enough_amount;
    }

    ec = check_card_daily_limit(user_info, card_info, amount  + commission );
    if (ec) return ec;
    /////////////////////////////////////////////////////////////////////////////////////
    //                        PERFORM TRANSACTION TO MERCHANT
    //////////////////////////////////////////////////////////////////////////////////////
    // Perform transaction to EOPC  @Note: simplify construct EOPC_Debit_T structure.
    int64_t const trn_id = p_info.id;

    EOPC_Debit_T debin;
    debin.amount      = amount + commission;
    debin.cardId      = card_info.pc_token;
    debin.ext         = num2string(trn_id);
    debin.merchantId  = merchant.merchantId;
    debin.terminalId  = merchant.terminalId;
    debin.port        = merchant.port;
    debin.stan        = make_stan(trn_id);

    oson_eopc -> async_trans_pay(debin, std::bind(&self_type::on_trans_pay_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) ) ;
    return Error_async_processing ;
}

void bill_edit_session::on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    d->m_io_service->post( std::bind(&self_type::on_trans_pay, shared_from_this(), debin, tran, ec));
}

void bill_edit_session::on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD_C(slog);

    ec = handle_trans_pay(debin, tran, ec);

    return d->send_result(ec);
}

Error_T bill_edit_session::handle_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)
{
    SCOPE_LOGD_C(slog);

    p_info.eopc_trn_id = tran.refNum;
    p_info.pan         = tran.pan;

    if(ec != Error_OK) {
        slog.ErrorLog("Error: %d", (int)ec);

        Fault_T fault( oson_this_db  );
        std::string msg =  "Can't perform transaction in EOPC. purchase-id: "  + to_str(p_info.id) + ". \nerror-code:" + to_str(ec) + ", error-msg : " + oson::error_str(ec) + ".";
        Fault_info_T finfo{  FAULT_TYPE_EOPC, FAULT_STATUS_ERROR, msg } ;
        fault.add( finfo );

        p_info.merch_rsp = "EOPC trans_pay get error: " + to_str( (long long)ec);  
        return ec;
    }

    if( ! tran.status_ok() ) {
        slog.ErrorLog("Status of transaction invalid");

        Fault_T fault( oson_this_db );
        fault.add( Fault_info_T(FAULT_TYPE_EOPC, FAULT_STATUS_ERROR, "Wrong status for transaction. purchase-id: " + to_str(p_info.id) )  );

        p_info.merch_rsp = "EOPC tran.status = " + tran.status;
        return   Error_eopc_error ;
    }

    if ( tran.resp != 0 ){
        slog.ErrorLog("resp is invalid( not zero ). ");
        p_info.merch_rsp = "EOPC tran.resp = " + to_str(tran.resp);
        return Error_card_blocked;
    }

    slog.DebugLog("Purchase success: ref-num: '%s' ", tran.refNum.c_str()); 

    p_info.merch_rsp = "eopc_trans_pay success!";


    Purchase_T purchase( oson_this_db ) ;

    bool const has_merchant_pay = !( merchant.api_id == merchant_api_id::nonbilling ) ; 

    if ( has_merchant_pay) 
    {
        int const old_status =  p_info.status; 

        p_info.status = TR_STATUS_EOPC_PAY_SUCCESS;

        purchase.update(p_info.id, p_info);

        p_info.status = old_status;
    }

    int a_new_purch_status = TR_STATUS_SUCCESS ;

    if( has_merchant_pay ) 
    {
        int64_t const trn_id = p_info.id;
        Merchant_T merchant_table( oson_this_db  ) ;

        Merch_acc_T acc;
        merchant_table.acc_info(merchant.id, acc);
        if(merchant.api_id == merchant_api_id::ums ||
           merchant.api_id == merchant_api_id::paynet)
        {
            merchant_table.api_info(merchant.api_id, acc);
        }
        
        this->trans.merch_api_params["oson_eopc_ref_num"] = p_info.eopc_trn_id ;
        
        this->trans.ts             = p_info.ts ;
        this->trans.transaction_id = trn_id;
        this->trans.user_phone      = user_info.phone;

        if (merchant.api_id == merchant_api_id::paynet )
        {
            this->trans.user_phone = oson::random_user_phone();
        }

        // For paynet get its transaction id from database paynet counter
        {
            trans.transaction_id = merchant_table.next_transaction_id( );
            p_info.oson_paynet_tr_id = trans.transaction_id;
        }

        Merch_trans_status_T trans_status;
        Merchant_api_T merch_api(merchant, acc);
        ec = merch_api.perform_purchase(trans, trans_status);

        p_info.paynet_status  = trans_status.merchant_status;
        p_info.paynet_tr_id   = trans_status.merchant_trn_id ;
        p_info.merch_rsp      = trans_status.merch_rsp ;

        if(ec == Error_perform_in_progress){
            slog.DebugLog(" === Purchase in progress === ");
            a_new_purch_status = TR_STATUS_IN_PROGRESS;
            ec =  Error_OK ;
        }
        else
        if(ec != Error_OK) {
            oson_eopc -> async_trans_reverse( p_info.eopc_trn_id,  oson::trans_reverse_handler() ) ;
            Fault_T fault(  oson_this_db  );
            fault.add( Fault_info_T(  FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p_info.id) ) );
            return ec;
        }
    }  

    p_info.status = a_new_purch_status;
    purchase.update(p_info.id, p_info);

    Bills_T bills( oson_this_db ) ;
    bills.set_status(bill_id, BILL_STATUS_REPAID);

    if ( ! bill.is_business_bill() ) 
    {
        std::string msg = "Ваша просьба оплатить счёт принята.";
        Users_notify_T users_n(  oson_this_db  );
        users_n.notification_send(bill.uid2, msg, MSG_TYPE_REQUEST);
    }

    Purchase_info_T p_info_copy = p_info;
    p_info.id = 0;//no update more.

    d->m_io_service->post( std::bind(&ss_bonus_earns, p_info_copy)) ;

    return Error_OK ;
}

static Error_T api_bill_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /**************========== LOGIN =============== */
    OSON_PP_USER_LOGIN(d, reader); 
    //////////////////////////////////////////////////
    const uint64_t id       = reader.readByte8();
    const uint16_t accept   = reader.readByte2();
    const uint64_t card_id  = reader.readByte8();
    //////////////////////////////////////////////////////////
    typedef bill_edit_session session;
    typedef std::shared_ptr< session > session_ptr;
    session_ptr s  = std::make_shared<session>(d, id, accept, card_id);
    s->async_start();
    
    slog.DebugLog("id = %lld, accept = %d, card_id = %lld  uid: %lld", id, accept, card_id, d->m_uid);
    return Error_async_processing ;
}

static Error_T api_bank_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    Error_T ec;
    
    uint32_t bank_id = reader.readByte4();
    
    Sort_T sort(0, 0); // no limit, no offset
    
    Bank_info_T search;
    search.id = bank_id;
    search.status = 1;
    Bank_list_T list;
    Bank_T bank(  oson_this_db  );
    ec = bank.list( search, sort , list);
    if (ec) return ec;
     
    oson::icons::table  icon_table(oson_this_db);
    
   ////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4( list.list.size() ) ;
    
    for(const Bank_info_T& f : list.list ) 
    {
        std::string icon_path; 
        //always check not equal to zero, because it's possible situation.
        if (f.icon_id != 0 ) {
            icon_path = icon_table.get( f.icon_id ).path_hash;
        }
        d->m_writer << b4(f.id)       << f.name            << b4(f.rate)    << b8(f.min_limit) 
                 << b8(f.max_limit)   << b8(f.month_limit) << f.offer_link  << f.bin_code << icon_path ;
    }
    ////////////////////////////////////////////////////////////

    return Error_OK;    

}

static Error_T api_bank_icon_get(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /********************************************/
    if ( !d->m_uid ){
        slog.ErrorLog("Not authorization access");
        return Error_login_failed;
    }
    /*****************************************/
    const uint32_t id = reader.readByte4();
    /*****************************************/
    Error_T ec = Error_OK ;
    Bank_T bank( oson_this_db  );
    Bank_info_T bank_info;
    ec = bank.info(id, bank_info);
    if (ec) return ec;
    
    oson::icons::content img;
    oson::icons::manager manager;
    int loaded = manager.load_icon(bank_info.icon_id, img);
    
    if ( ! loaded) return Error_not_found;
    
    d->m_writer << b4( img.image ) ;
    
    return Error_OK ;
}


static Error_T api_user_bonus_card_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
     if (d->m_uid == 0){
         slog.WarningLog("NO login the user.");
         return Error_login_failed;
     }
    //////////////////////////////////////////////////////////////////////
    std::string fio    = reader.readAsString( reader.readByte2() ) ;
    //////////////////////////////////////////////////////////////////////
     
     //@Note this condition will removed, after stable bonus going to.
     {
         //check existence
         Users_bonus_T users_b( oson_this_db  );
         User_bonus_info_T info;
         Error_T ec = users_b.bonus_info(d->m_uid, info);
         if ( ! ec && info.uid == d->m_uid )
         {
             return Error_a_new_card_already_exists;
         }
     }
     //////////////////////////////////////////////////////////////////
     std::string pan, expire;
     //generate pan and expire
     {
        char buf[ 32 ] = {};
        size_t sz = snprintf(buf, 30, "0504%08lld", (long long)(d->m_uid) );

        pan.assign(buf, sz); 
        //a next year from now.
        
        expire = formatted_time_now("%m%y"  ); 
        //increase year.
        if (expire.size() == 4) // MMYY
        {
            ++expire[3]; 
            if (expire[3] > '9' ){
                expire[3] = '0';
                ++expire[2];
                if (expire[2] > '9')
                    expire[2] = '0';
            }
        }
        slog.DebugLog("pan: %s, expire: %s", pan.c_str(), expire.c_str() );
     }
     ///////////////////////////////////////////////////////////////////
     
     //1. get bonus card
     uint64_t bonus_card_id = 0;
     {
         Cards_T cards( oson_this_db  );
         Card_bonus_list_T list;
         cards.bonus_card_list(0, list);
         
         if (list.empty())
             return Error_not_found ;
            
         bonus_card_id = 0; //list[0].card_id ;
         
         for(const auto& c: list)
         {
             if (c.xid == 1 ) // active
             {
                 bonus_card_id = c.card_id;
             }
         }
         
         if ( bonus_card_id == 0 ) 
         {
             slog.ErrorLog("There no active bonus card!!!");
             return Error_internal;
         }
     }
     
     User_bonus_info_T bonus_info;
     
     bonus_info.id       = 0;
     bonus_info.uid      = d->m_uid;
     bonus_info.balance  = 0;
     bonus_info.earns    = 0;
     bonus_info.bonus_card_id =  bonus_card_id;
     bonus_info.block    = User_bonus_info_T::ACTIVE_CARD; //1-active, 2 - disabled.
     bonus_info.fio      = fio;
     bonus_info.pan      = pan;
     bonus_info.expire   = expire;
     
     Users_bonus_T users_b( oson_this_db  );
     users_b.bonus_add(bonus_info, bonus_info.id);
     //////////////////////////////////////////////
     d->m_writer << b8(bonus_info.id) << pan << expire ;
     //////////////////////////////////////////////
     
     return Error_OK ;
}

 ///////////  ============== CLIENT CABINET ======================================

 static bool allowed_cabinet_password(const std::string& password ) 
 {
     struct special_symbols
     {
         bool operator()(int c ) const
         { 
             return  ( c == '-' || c == '$' || c == '@' || c == '#' || c == '%' || c == '_' ) ;
         }
     }cs;
     
     
     size_t const len = password.size();
     
     if ( ! ( len >= 8 && len <= 18 ) ) {
         return false;
     }
     
     
     bool has_digit = false , has_alpha = false /*, has_cs = false*/;
     bool has_big_letter = false, has_small_letter = false;
     for(int c : password )
     {
       
         if ( isdigit(c ) ) {
             has_digit = true;
         } else if (isalpha(c ) ) {
             has_alpha = true;
             if (islower(c)){
                 has_small_letter = true;
             } else {
                 has_big_letter = true;
             }
         } else if (cs(c) ) {
             /*has_cs = true;*/
         } else {
             return false;
         }
     }
     
     return has_digit && has_alpha && has_small_letter && has_big_letter ;
 }
 
 static Error_T api_client_cabinet_register(api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////
    std::string phone     = reader.readString();
    std::string password  = reader.readString();
    std::string device_id = reader.readString();
    //////////////////////////////////////////////////
    
    //1. check phone.
    if (boost::algorithm::starts_with(phone, "+")) {
        phone.erase(0, 1 ) ;
    }
    
    if ( ! valid_phone(phone )  /*||   phone.length() != 12*/ ) 
    {
        slog.WarningLog("Not valid phone: %s ", phone.c_str());
        return Error_not_valid_phone_number ;
    }
    
    //2. check password
    if ( ! allowed_cabinet_password(password ) ) {
        slog.WarningLog("Not valid password: %s ", password.c_str());
        return Error_week_password ;
    }
    
    //3. check device_id
    if ( device_id.empty() ) {
        slog.WarningLog("Device id empty!" ) ;
        return Error_parameters;
    }
    ///////////////////////////////////////////////////////////////
    
    Error_T ec = Error_OK ;
    Users_T users( oson_this_db  ) ;
    User_info_T user_info  = users.info(phone, /*OUT*/ ec ) ;
    
    if (ec == Error_OK && user_info.blocked) {
        slog.WarningLog("register blocked user");
        return Error_blocked_user ;
    }

    
    Activate_table_T act_table( oson_this_db  );
    
    Activate_info_T act_s;
    act_s.phone = phone;
    int cnt     = act_table.count(act_s) ;
    
    if ( cnt > 10) 
    {
        slog.WarningLog("Too many sms have been sent: %d times to phone: %s", cnt, phone.c_str());
        return Error_limit_exceeded_password_check ;
    }
    
    act_s.kind   = act_s.Kind_user_register ;
    act_s.dev_id = device_id ;
    act_s.add_ts = formatted_time_iso( std::time( 0 ) - 3 * 60 );// after 3 minutes.
    cnt = act_table.count(act_s ) ;
    
    if ( cnt >= 1 ) 
    {
        slog.WarningLog("Too many sms have been sent by cabinet register: %d times to phone: %s", cnt, phone.c_str());
        return Error_limit_exceeded_password_check ;
    }
     
    std::string code = oson::utils::generate_code( 7 ) ;
    Activate_info_T act_i;
    act_i.code   = code      ;
    act_i.phone  = phone     ;
    act_i.dev_id = device_id ;  //"cabinet" ;
    act_i.kind   = act_i.Kind_user_register;
    act_i.add_ts = formatted_time_now_iso_S();
    
    
    act_table.add( act_i );
    SMS_info_T sms_info (phone, "www.cabinet.oson.uz: код подтверждения регистрации в кабинете " + code , SMS_info_T::type_client_register_code_sms ) ;
    oson_sms -> async_send( sms_info );
      
    return Error_OK ;    
 }
 
 
static Error_T api_client_cabinet_checkcode(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////////
    std::string phone      = reader.readString();
    std::string password   = reader.readString();
    std::string code       = reader.readString();
    std::string dev_id     = reader.readString();
    //////////////////////////////////////////////////////////
    if (code.empty() || phone.empty() || dev_id.empty() ){
        slog.ErrorLog("code is empty!");
        return Error_checkcode_failed;
    }
    
    /*** check min-max length of password and symbols */
    if ( ! allowed_cabinet_password(password ) ) {
        slog.WarningLog("weak password: '%s' ", password.c_str()); 
        return Error_week_password ;
    }
    
    Activate_info_T act_i, act_s;
    act_s.phone  =  phone  ;
    act_s.dev_id =  dev_id ;
    act_s.code   =  code   ;
    act_s.kind   =  act_s.Kind_user_register;
    
    Activate_table_T act_table( oson_this_db );
    act_i = act_table.info(act_s);
    
    int64_t const code_id = act_i.id ;
    
    if (!code_id || code != act_i.code ) 
    {
        
        act_s.code.clear();
        act_table.kill_lives( act_s ) ;
        
        slog.DebugLog("Wrong check code");
        return Error_checkcode_failed;
        
    }
 
    DB_T::transaction transaction( oson_this_db  );
 
    act_table.deactivate(code_id);
    
    Users_T users( oson_this_db  ) ;
    Error_T ec = Error_OK ;
    User_info_T user_info  = users.info(phone,  /*OUT*/ ec );
    
    // If user not found
    if (user_info.id == 0 ) 
    {
        std::string qr_code = oson::utils::generate_password();
            
        std::string query = "INSERT INTO users (phone, qrstring) VALUES (" + escape(phone) + ", "
            " upper(encode(hmac('" + qr_code + "', gen_salt('md5'), 'sha256'), 'hex') ) ) RETURNING id";
        
        DB_T::statement st( oson_this_db );
        st.prepare(query);

        st.row(0) >> user_info.id ;
        slog.InfoLog("uid: %ld", user_info.id);
        
        users.create_deposit(user_info.id);
    }
    
    //now client exists, but check users_client
    {
        std::string query = "SELECT id FROM users_cabinet WHERE uid = " + escape(user_info.id) ;
        DB_T::statement st( oson_this_db  );
        st.prepare(query);
        int64_t id = 0;
        
        if (st.rows_count() == 0) // there no 
        {
            query = "INSERT INTO users_cabinet (id, uid) VALUES ( DEFAULT, " + escape(user_info.id)  + " ) RETURNING id; " ;
        
            st.prepare(query);
            
            st.row(0) >> id;
            slog.InfoLog("id: %ld", id);
        } else {
            st.row(0) >> id ;
        }
        
        //reset last_passwd_check and check_count
        {
            User_cabinet_info_T uc;
            uc.last_passwd_check = formatted_time_now_iso_S() ;
            uc.check_count = 0;
            
            Users_cabinet_T table{ oson_this_db } ;
            table.edit_last_password(id, uc ) ;
        }
        
    }
    
    d->m_uid    = user_info.id ;
    d->m_dev_id = 0;
    {
        Users_cabinet_T users_c( oson_this_db );
        users_c.add_client_password(d->m_uid, password);
    }
    transaction.commit();
    ////////////////////////////////////////////////////////////////////    

    //3. generate token and save it user online table
    std::string token = gen_token_and_add_users_online(d);
    ///////////////////////////////////////
    d->m_writer << b8(d->m_uid) << token ;
    //////////////////////////////////////
    return Error_OK ;
}

static Error_T api_client_cabinet_auth(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /******************************************/
    std::string phone     = reader.readString();
    std::string password  = reader.readString();
    std::string device_id = reader.readString();
    /******************************************/
     
    if (phone.empty() || password.empty() || device_id.empty() )
    {
        slog.WarningLog("Some parameters are empty");
        return Error_parameters;
    }
    
    
    Error_T ec = Error_OK ;
    
    //1. check exists phone.
    Users_T users( oson_this_db  ) ;
    User_info_T user_info = users.info(phone, /*out*/ ec ) ;
    
    if ( ec ) {
        slog.WarningLog("User not found with %s phone!", phone.c_str());
        return Error_user_not_found;
    }
    
    // save uid.
    d->m_uid = user_info.id; 
    
    
    //2. check 
    Users_cabinet_T users_c ( oson_this_db  );
    User_cabinet_info_T user_cinfo, csrch;
    
    csrch.uid = d->m_uid ;
    
    ec = users_c.get_client_cabinet( csrch, user_cinfo ) ;
    
    if ( ec != Error_OK  || user_cinfo.uid != d->m_uid )  
    {
        return Error_user_not_found ;
    }
    
    /******************************************/
    std::time_t  /*const*/ last_change  = ( user_cinfo.last_passwd_check.empty() )  ? 0 : str_2_time( user_cinfo.last_passwd_check.c_str() ) ;
    std::time_t  const now_time     = std::time( 0 );
    std::time_t  const diff_time    = user_cinfo.check_count < 20 ? 5 * 60  :  60 * 60 ;
    bool const increase_check_count = (last_change + diff_time > now_time ) ;
    
    slog.InfoLog("last_change( %s ) : %ld, now: %ld,  diff-time: %ld", user_cinfo.last_passwd_check.c_str(), (int64_t)last_change, (int64_t)now_time, (int64_t)diff_time ) ;
    
    if ( ! increase_check_count  )
    {  
        user_cinfo.check_count = 0;
    }
    
    ++user_cinfo.check_count ;
    
    if ( ! increase_check_count || user_cinfo.check_count == 10 )
    {
        user_cinfo.last_passwd_check = formatted_time_now_iso_S() ;
        last_change = now_time ;
    }

    
    if ( user_cinfo.check_count >= 10 ) 
    {
        // remove device token, if exists.
        std::string query = "DELETE  FROM user_devices WHERE (uid = " + escape(d->m_uid) + ") AND (dev_token = " + escape(device_id) + ") " ;
        DB_T::statement st( oson_this_db ) ;
        st.prepare(query);
    }
    
    users_c.edit_last_password( user_cinfo.id,  user_cinfo  );
    
    int64_t wait_time = 0; 
    if (user_cinfo.check_count >= 10 && user_cinfo.check_count < 20 ) 
        wait_time = std::max< int64_t > (0,  last_change + 5 * 60ll - now_time ) ;
    else if (user_cinfo.check_count >= 20 ) 
        wait_time = std::max< int64_t > (0,  last_change + 60 * 60ll - now_time ) ;
    else 
        wait_time = 0;
    
    d->m_writer << b4(user_cinfo.check_count)  << b8( wait_time ) ;
    
    if ( user_cinfo.check_count >= 10 ) 
    {
        return Error_limit_exceeded_password_check ;
    }
    /*******************************************************/
    bool test = users_c.check_client_password(d->m_uid, password);
    
    if( !test ) 
    {
        return Error_login_failed;
    }
    
    if ( ! allowed_cabinet_password( password ) ) {
        slog.WarningLog("password is weak") ;
        return Error_week_password ;
    }
    /**************************************************************/
    /************* SUCCESS auth**********************************/
    
    // check trusted device token
    {
        
        std::string query = "SELECT  dev_id FROM user_devices WHERE (uid = " + escape(d->m_uid) + ") AND (dev_token = " + escape(device_id) + 
                        ") AND ( add_ts > NOW() - interval '72 hour' )  LIMIT 4 " ;
        DB_T::statement st( oson_this_db ) ;
        st.prepare(query);
        
        if (st.rows_count() == 1 )
        {
            st.row(0) >> d->m_dev_id ;
        } else {
            d->m_dev_id = 0;
        }
        
        if ( d->m_dev_id > 0 ) 
        {
            user_cinfo.last_passwd_check = formatted_time_now_iso_S();
            user_cinfo.check_count       =  0;
            users_c.edit_last_password( user_cinfo.id,  user_cinfo  );
            
            std::string token = gen_token_and_add_users_online(d);
            int32_t sms_sent = 0;
            d->m_writer << b4(sms_sent) << b8(d->m_uid) << token ;
            return Error_OK ;
        }
    }
    
    Activate_table_T act_table( oson_this_db  );
    Activate_info_T act_search;
    act_search.phone = phone;
    act_search.kind  = Activate_info_T::Kind_cabinet_auth;
    act_search.add_ts = formatted_time_iso(std::time(0) - 3* 60 ) ;
    act_search.dev_id = device_id;
    
    int cnt_act = act_table.count(act_search);
    if (cnt_act){
        slog.ErrorLog("SMS has been sent already!");
        return Error_timeout;
    }
    
    //NEED SMS
    {
        std::string token; //an empty token.
        d->m_writer << b4(1) << b8(0) << token;
    }
    
    std::string sms_code = oson::utils::generate_code( 7 ) ;
    
    Activate_info_T act_i           ;
    act_i.code     = sms_code       ;
    act_i.phone    = phone          ;
    act_i.dev_id   = device_id      ;//"cabinet_auth" ;
    act_i.kind     = act_i.Kind_cabinet_auth ;
    act_i.add_ts   = formatted_time_now_iso_S() ;
    act_i.other_id = d->m_uid ;
    
    
    act_table.add( act_i );
    //www.oson.uz: код подтверждения авторизации в кабинете 0924158
    SMS_info_T sms_info (phone, "www.cabinet.oson.uz: код подтверждения авторизации в кабинете " + sms_code , SMS_info_T::type_client_register_code_sms ) ;
    oson_sms -> async_send(sms_info) ;
    
    return Error_OK ;
}


static Error_T api_client_cabinet_auth_sms_check(api_pointer_cref d, ByteReader_T& reader )
{    
    SCOPE_LOGD(slog);
    ////////////////////////////////////////////
    std::string phone  = reader.readString() ;
    std::string code   = reader.readString() ;
    std::string dev_id = reader.readString() ; //"cabinet_auth";
    int32_t const trust   = reader.readByte4() ;

    //////////////////////////////////////////////////////////
    if (code.empty() || phone.empty() || dev_id.empty() ){
        slog.ErrorLog("code is empty!");
        return Error_checkcode_failed;
    }
    
     
    Activate_info_T act_i, act_s;
    act_s.phone  =  phone;
    act_s.dev_id =  dev_id;
    act_s.code   =  code;
    act_s.kind   =  act_s.Kind_cabinet_auth ;
    
    Activate_table_T act_table( oson_this_db );
    act_i = act_table.info(act_s);
    
    int64_t const code_id = act_i.id ;
    
    if (!code_id || code != act_i.code ) 
    {
        act_s.code.clear();
        act_table.kill_lives( act_s ) ;
        
        slog.DebugLog("Wrong check code");
        return Error_checkcode_failed;
        
    }

    act_table.deactivate(code_id);
    /***************************************/
    d->m_uid = act_i.other_id ;
    
    
    if ( trust != 0 ) 
    {
        DB_T& db = oson_this_db ;
        
        //remove old devices.
        std::string query = "DELETE FROM user_devices WHERE uid = " + escape(d->m_uid) + " AND dev_token = " + escape(dev_id) ;
        DB_T::statement st( db ) ;
        st.prepare(query);
        
        
        //add this dev_id to users_devices table.
        Users_device_T device_table(db);
        Device_info_T dev_info ;
        dev_info.dev_token = dev_id   ;
        dev_info.uid       = d->m_uid ;
        dev_info.password  = oson::utils::md5_hash( phone );  
        
        device_table.device_register( dev_info, /*out*/ d->m_dev_id ) ;
        
        
    }
    
    //3. generate token and save it user online table
    std::string token = gen_token_and_add_users_online( d );
    ///////////////////////////////////////
    d->m_writer << b8(d->m_uid) << token ;
    //////////////////////////////////////
    return Error_OK ;
}

static Error_T api_client_cabinet_checkpassword(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    ////////////////////////////////////////////////////////////////////////////
    std::string password    = reader.readString();
    ////////////////////////////////////////////////////////////////////////
    Users_cabinet_T users_c( oson_this_db );
    bool test = users_c.check_client_password(d->m_uid, password ) ;
    
    if ( ! test )
        return Error_login_failed;
    return Error_OK ;
}

static Error_T api_client_cabinet_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /*****  1-step:  read  ***************/
    OSON_PP_USER_LOGIN(d, reader);
    ///////////////////////////////////////////////
    std::string oldpwd = reader.readString();
    std::string newpwd = reader.readString();
    uint32_t checkpwd  = reader.readByte4();
    ///////////////////////////////////////////
    /***** 2-step: edit  *************/
    User_cabinet_info_T search, info;
    search.uid      = d->m_uid;
    search.password = oldpwd;
    
    info.checkpassword = checkpwd;
    info.password      = newpwd;
    
    Users_cabinet_T users_c( oson_this_db  );
    return users_c.edit_client_cabinet(search, info);
}

static Error_T api_client_cabinet_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);

    User_cabinet_info_T search, info;
    search.uid = d->m_uid;
    
    Users_cabinet_T users_c( oson_this_db  );
    Error_T ec = users_c.get_client_cabinet(search, info);
    if ( ec )  return ec;
    /////////////////////////////////////////
    d->m_writer << b8(info.uid) << b4(info.checkpassword);
    ///////////////////////////////////////////
    return Error_OK ;
}
/********************************** END CLIENT CABINET IMPLEMENTATION ************/
static Error_T api_hello_test(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    std::string email_from = "kh.normuradov@oson.uz";
    std::string email_to   = "security@oson.uz";
    std::string subject = "test oson linux email sending";
    std::string message = "hello link: https://core.oson.uz:9443/api/purchase/status/374707?token=xabs.\n";
    
    oson::utils::send_email(email_to, email_from, subject, message);
    
    return Error_OK ;
}

static Error_T api_oson_app_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_USER_LOGIN(d, reader);
    
    std::string os;
    reader >> r2(os);
    
    ///////////////////
    boost::trim(os);
    boost::to_lower(os);
    ////////////////////
    
    oson::App_info_table_T table( oson_this_db ) ;
    
    oson::App_info_T info  = table.get_last( os );
    
    if (info.id == 0 ) {
        slog.WarningLog("Not found");
        return Error_not_found ;
    } 
    
    
    d->m_writer << b4(info.id ) << info.version << info.os << info.release_date << info.expiry_date ;
    
    return Error_OK ;
}

static Error_T api_client_data_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    //1.user_info
    Error_T ec = api_client_info(d, reader );
    if (ec)return ec;
    
    //2.1 add monitoring - flag.
    {
        Cards_cabinet_table_T table(oson_this_db);
        Card_cabinet_info_T info;
        ec= table.last_info(d->m_uid, info);
        
        int32_t monitoring_flag = MONITORING_OFF ;
        if ( ec == Error_OK ) {
            monitoring_flag = info.monitoring_flag ;
        }
        
        d->m_writer << b4(monitoring_flag);
    }
    
    //2. transaction_bill
    ec = api_transaction_bill_list(d, reader.reset() );
    if (ec)return ec;

    return api_client_card_list(d, reader.reset() );
}

static Error_T api_get_user_full_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    if (d->m_uid == 0)
    {
        slog.WarningLog("Unauthorized access!");
        return Error_login_failed ;
    }
    
    oson::Users_full_info_table table(  oson_this_db ) ;
    
    oson::Users_full_info info = table.get_by_uid( d-> m_uid ) ;
    
    if ( info.id == 0 )
    {
        return Error_not_found ;
    }
    
    d->m_writer << b8( info.id )  << b8( info.uid ) << b4( info.status ) << info.fio << info.date_of_birth << info.nationality << info.citizenship 
                << info.passport_number << info.passport_serial <<info.passport_start_date << info.passport_end_date << info.passport_image_path ;
    
    return Error_OK ;
}

static Error_T api_add_user_full_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    if (d->m_uid == 0)
    {
        slog.WarningLog("Unauthorized access!");
        return Error_login_failed ;
    }
    
    oson::Users_full_info info;
    reader >> r2(info.fio) >> r2(info.date_of_birth) >> r2(info.nationality) >> r2(info.citizenship) >> r2(info.passport_number) 
           >> r2(info.passport_serial) >> r2(info.passport_start_date) >> r2(info.passport_end_date) >> r2(info.passport_image_path) ;
    
    info.id     = 0;
    info.uid    = d->m_uid;
    info.status = info.Status_Registered ;
    
    
    
    
    oson::Users_full_info_table  table( oson_this_db ) ;
    
    oson::Users_full_info tmp =  table.get_by_uid(d->m_uid);
    if (tmp.id != 0 )
    {
        slog.WarningLog("There already exists %lld  user!", ( long long ) d->m_uid);
        return Error_operation_not_allowed ;
    }
    
    info.id  = table.add( info );
    
    d->m_writer << b8(info.id);
    
    return Error_OK ;
}

static Error_T api_edit_user_full_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    if (d->m_uid == 0)
    {
        slog.WarningLog("Unauthorized access!");
        return Error_login_failed ;
    }
    
    oson::Users_full_info info;
    reader >> r2(info.fio) >> r2(info.date_of_birth) >> r2(info.nationality) >> r2(info.citizenship) >> r2(info.passport_number) 
           >> r2(info.passport_serial) >> r2(info.passport_start_date) >> r2(info.passport_end_date) >> r2(info.passport_image_path) ;
    
    info.id     = 0;
    info.uid    = d->m_uid;
    //info.status = info.Status_Registered ;
    
    oson::Users_full_info_table  table( oson_this_db ) ;
    
    oson::Users_full_info tmp =  table.get_by_uid( d->m_uid ) ;
    
    if (tmp.id == 0 )
    {
        slog.WarningLog("There does not exists %lld  user!", ( long long ) d->m_uid);
        return Error_not_found ;
    }
    
    info.status = tmp.status ; //user itself DOES NOT  allow to edit the status.
    info.id     = tmp.id     ;
    // SET empty fields to original.
#define OSON_PP_INFO_UPD( name )   if (info . name . empty() ) info . name = tmp . name 
    OSON_PP_INFO_UPD(fio);
    OSON_PP_INFO_UPD(date_of_birth);
    OSON_PP_INFO_UPD(nationality);
    OSON_PP_INFO_UPD(citizenship);
    OSON_PP_INFO_UPD(passport_number);
    OSON_PP_INFO_UPD(passport_serial);
    OSON_PP_INFO_UPD(passport_start_date);
    OSON_PP_INFO_UPD(passport_end_date);
    OSON_PP_INFO_UPD(passport_image_path);

#undef OSON_PP_INFO_UPD

    table.edit( info.id, info ) ;
    
    return Error_OK ;
}
 
static std::string gen_token_and_add_users_online(api_pointer_cref d ) 
{
    std::string token = oson::utils::generate_token( 17 );
    
    Users_online_T  table{ oson_this_db } ;
    
    User_online_info_T info;
    info.uid    = d->m_uid;
    info.dev_id = d->m_dev_id;
    info.token  = token;
    
    table.add(info);
    
    return token;
}
//////////////////////////////////////////////////////////////////////////
//	Internal function

void show_header( const uint8_t * data, size_t length ) {
    CLog log;
    log.WriteArray( LogLevel_Debug, "H: ",  data ,   length );
}

void show_data(const uint8_t * data, size_t length )
{
    CLog log;
    log.WriteArray( LogLevel_Debug, "D: ",  data ,   length  );
}
/****************************************************************************************************************************/
namespace{
    
template< int cmd >  struct command_dispatcher{   
    Error_T operator()(api_pointer_cref d, ByteReader_T& reader)const { return api_null(d, reader); }
};

#define OSON_CLIENT_API_CMD(cmd_name, cmd_val, cmd_func)                      \
    template<> struct command_dispatcher< cmd_val >                           \
    {   enum{ cmd_value = cmd_val};                                           \
        Error_T operator()(api_pointer_cref d, ByteReader_T& reader)const     \
        { return cmd_func(d, reader) ; }                                      \
    };                                                                        \
    /****/
OSON_CLIENT_API_CMD(log-debug       , 0   , api_logging_debug     ) 
OSON_CLIENT_API_CMD(user-auth       , 1   , api_client_auth       ) 
OSON_CLIENT_API_CMD(auth-token      , 2   , api_client_auth_token )         
OSON_CLIENT_API_CMD(registration    , 3   , api_client_register   ) 
OSON_CLIENT_API_CMD(checkcode       , 4   , api_client_checkcode  ) 
OSON_CLIENT_API_CMD(logout          , 5   , api_client_logout     ) 
OSON_CLIENT_API_CMD(errors          , 7   , api_user_error_codes  ) 
OSON_CLIENT_API_CMD(errors_list     , 8   , api_user_error_codes_list ) 
OSON_CLIENT_API_CMD(data-info       , 9   , api_client_data_info  ) 
OSON_CLIENT_API_CMD(user-info       , 10  , api_client_info       ) 
OSON_CLIENT_API_CMD(change-info     , 11  , api_client_change_info ) 
OSON_CLIENT_API_CMD(set-avatar      , 12  , api_set_avatar        ) 
OSON_CLIENT_API_CMD(register_notify , 13  , api_client_notify_register ) 
OSON_CLIENT_API_CMD(balance         , 14  , api_client_balance    ) 
OSON_CLIENT_API_CMD(contacts        , 15  , api_client_contacts_list ) 
OSON_CLIENT_API_CMD(icons           , 16  , api_client_avatar        ) 
OSON_CLIENT_API_CMD(notifylist      , 17  , api_notification_list ) 
OSON_CLIENT_API_CMD(decode_qr       , 18  , api_decode_qr         ) 
OSON_CLIENT_API_CMD(user_qr_code    , 19  , api_user_qr_code      ) 
OSON_CLIENT_API_CMD(send_notify     , 20  , api_send_notify       ) 
OSON_CLIENT_API_CMD(card_mon_tarif  , 22  , api_card_monitoring_tarif ) 
OSON_CLIENT_API_CMD(card_monit_pay_m, 23  , api_card_monitoring_payed_months)
OSON_CLIENT_API_CMD(card_monitoring,  24  , api_card_monitoring_edit )
OSON_CLIENT_API_CMD(card_list       , 25  , api_client_card_list  ) 
OSON_CLIENT_API_CMD(card-add        , 26  , api_client_card_add   ) 
OSON_CLIENT_API_CMD(card-edit       , 27  , api_client_card_edit  ) 
OSON_CLIENT_API_CMD(card_delete     , 28  , api_client_card_delete) 
OSON_CLIENT_API_CMD(primary_card    , 29  , api_client_primary_card ) 
OSON_CLIENT_API_CMD(foreign_cards   , 30  , api_foreign_card_list      ) 
OSON_CLIENT_API_CMD(foreign_actva   , 31  , api_foreign_card_activate ) 
OSON_CLIENT_API_CMD(foreign_code_req, 32  , api_foreign_card_activate_req) 
OSON_CLIENT_API_CMD(card_history    , 33  , api_card_history_list ) 
OSON_CLIENT_API_CMD(card_info       , 34  , api_card_info)
OSON_CLIENT_API_CMD(trans_list      , 35  , api_transaction_list)
OSON_CLIENT_API_CMD(trans_perform   , 36  , api_transaction_perform  ) 
OSON_CLIENT_API_CMD(trans_req       , 37  , api_transaction_request  ) 
OSON_CLIENT_API_CMD(trans_check     , 38  , api_transaction_check    ) 
OSON_CLIENT_API_CMD(trans_bill      , 40  , api_transaction_bill_list) 
OSON_CLIENT_API_CMD(trans_bill_ac   , 41  , api_transaction_bill_accept) 
OSON_CLIENT_API_CMD(incoming_list   , 45  , api_incoming_list ) 
OSON_CLIENT_API_CMD(outgoing_list   , 46  , api_outgoing_list ) 
OSON_CLIENT_API_CMD(detail_info     , 47  , api_detail_info   ) 
    
OSON_CLIENT_API_CMD(merch_groups    , 50  , api_merchant_group_list) 
OSON_CLIENT_API_CMD(merch_list      , 51  , api_merchant_list  ) 
OSON_CLIENT_API_CMD(merch_info      , 52  , api_merchant_info  ) 
OSON_CLIENT_API_CMD(merch_field     , 55  , api_merchant_field ) 
OSON_CLIENT_API_CMD(merch_ico       , 56  , api_merchant_ico   ) 
OSON_CLIENT_API_CMD(bonus_merch_list, 57  , api_bonus_merchant_list)
OSON_CLIENT_API_CMD(bonus_purch_list, 58  , api_bonus_purchase_list)
OSON_CLIENT_API_CMD(purch_list      , 60  , api_purchase_list ) 
OSON_CLIENT_API_CMD(purch_buy       , 61  , api_purchase_buy  ) 
OSON_CLIENT_API_CMD(purch_info      , 62  , api_purchase_info_new_scheme ) 
OSON_CLIENT_API_CMD(top_merch_list  , 63  , api_client_top_merchant_list) 

OSON_CLIENT_API_CMD(pub_purch_buy1  , 66  , api_public_purchase_buy_start) 
OSON_CLIENT_API_CMD(pub_purch_buy2  , 67  , api_public_purchase_buy_confirm ) 
        
OSON_CLIENT_API_CMD(favorite_list   , 70  , api_favorite_list ) 
OSON_CLIENT_API_CMD(favorite_add    , 71  , api_favorite_add  ) 
OSON_CLIENT_API_CMD(favorite_delete , 72  , api_favorite_del  ) 
OSON_CLIENT_API_CMD(periodic_bill_l , 75  , api_periodic_bill_list ) 
OSON_CLIENT_API_CMD(periodic_bill_a , 76  , api_periodic_bill_add  ) 
OSON_CLIENT_API_CMD(periodic_bill_d , 77  , api_periodic_bill_del  ) 
OSON_CLIENT_API_CMD(periodic_bill_e , 78  , api_periodic_bill_edit ) 
    
OSON_CLIENT_API_CMD(news_list       , 80  , api_news_list  ) 

OSON_CLIENT_API_CMD(ewallet_trans   , 82  , api_topup_trans_request)
OSON_CLIENT_API_CMD(ewallet_trans2  , 83  , api_topup_webmoney_invoice_confirmation)
OSON_CLIENT_API_CMD(ewallet_trans3  , 84  , api_topup_webmoney_payment_noficiation)
OSON_CLIENT_API_CMD(success         , 85  , api_topup_webmoney_payment_finish_trans)


OSON_CLIENT_API_CMD(ewallet_list    , 88  , api_topup_merchant_list ) 

OSON_CLIENT_API_CMD(bill_list       , 90  , api_bill_list  ) 
OSON_CLIENT_API_CMD(bill_add        , 91  , api_bill_add   ) 
OSON_CLIENT_API_CMD(bill_edit       , 92  , api_bill_edit  ) 
OSON_CLIENT_API_CMD(bank_list       , 100 , api_bank_list  ) 
//OSON_CLIENT_API_CMD(bank_icon_link  , 101 , api_bank_icon_link_get)
OSON_CLIENT_API_CMD(bank_icon       , 102 , api_bank_icon_get ) 

//OSON_CLIENT_API_CMD(error_text      , 105 , api_get_error_text ) ;

OSON_CLIENT_API_CMD(bonus_card_a    , 110 , api_user_bonus_card_add ) 
OSON_CLIENT_API_CMD(cabinet_registr , 120 , api_client_cabinet_register )
OSON_CLIENT_API_CMD(cabinet_check   , 121 , api_client_cabinet_checkcode ) 
OSON_CLIENT_API_CMD(cabinet_auth    , 122 , api_client_cabinet_auth ) 
OSON_CLIENT_API_CMD(cabinet_chkpwd  , 123 , api_client_cabinet_checkpassword)
OSON_CLIENT_API_CMD(cabinet_edit    , 124 , api_client_cabinet_edit)
OSON_CLIENT_API_CMD(cabinet_info    , 125 , api_client_cabinet_info)
OSON_CLIENT_API_CMD(cabinet_auth2   , 126 , api_client_cabinet_auth_sms_check ) 

OSON_CLIENT_API_CMD(oson_app_info   , 131 , api_oson_app_info ) 

OSON_CLIENT_API_CMD(oson_hello_test,  133, api_hello_test);

OSON_CLIENT_API_CMD(client_full_info_g, 134, api_get_user_full_info )
OSON_CLIENT_API_CMD(client_full_info_a, 135, api_add_user_full_info )
OSON_CLIENT_API_CMD(client_full_info_e, 136, api_edit_user_full_info ) 


} // end noname namespace

static Error_T dispatch_command(int cmd, api_pointer_cref dptr, ByteReader_T& reader)
{
#define OSON_API_CASE_SWITCH(z,n,text)  case n  : {  return command_dispatcher< n > () (dptr, reader); }
 
    switch(cmd)
    {
        BOOST_PP_REPEAT(FUNC_COUNT, OSON_API_CASE_SWITCH, dptr )
        default: return api_null(dptr, reader);        
    }
#undef OSON_API_CASE_SWITCH
}

#undef OSON_CLIENT_API_CMD 
/******************************************************************************************************************************/