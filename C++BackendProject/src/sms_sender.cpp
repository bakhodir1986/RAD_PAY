

#include <cstdio>
#include <cstring> // memset
#include <memory>
#include <utility>

#include <string>
#include <functional>

#include "sms_sender.h"
#include "log.h"
#include "utils.h"
#include "DB_T.h"
#include "http_request.h"

namespace
{

struct sms_info_db
{
    typedef int32_t integer;
    typedef int64_t bigint ;
    typedef std::string text;
    
    bigint   id    ;
    text     msg   ;
    text     ts    ;
    integer  send  ;
    integer  nphones ;
    text     phone ;
    integer  type  ;
};

class Sms_info_table
{
public:
    Sms_info_table(DB_T& db): db(db){}
    
    int64_t add( const sms_info_db& s)const
    {
        SCOPE_LOGD(slog);
        
        if ( ! db.isconnected() ) 
        {
            slog.WarningLog("DB is not connected!");
            return 0;
        }
        
        std::string query = "INSERT INTO sms_info (id, msg, ts, send, nphones, phone, type) VALUES ( DEFAULT, " + 
                escape(s.msg) + ", "+ escape(s.ts) + ", " + escape(s.send) + ", "+ escape(s.nphones) + ", " + escape(s.phone) + ", " + escape(s.type) + 
                ") RETURNING id ;" ;
        
        DB_T::statement st(db);
        Error_T ec = Error_OK ;
        st.prepare(query, ec );
        if ( ec ) return 0;
        
        int64_t id = 0;
        st.row(0) >> id;
        slog.InfoLog("id: %ld", id);
        return id;
    }
    
    int set_status(int64_t id, int sent)const
    {
        if (! db.isconnected())
            return 0;
        
        std::string query = "UPDATE sms_info SET send = " + escape(sent) + " WHERE id = " + escape(id);
        DB_T::statement st(db);
        
        Error_T ec;
        st.prepare(query, ec);
    
        return (int)ec;
    }
    
private:
    DB_T& db;
};

static int64_t add_table( const SMS_info_T& s)
{
    DB_T & db = oson_this_db ;
     
    Sms_info_table table ( db  ) ;
    
    sms_info_db d;
    d.id     = 0;
    d.msg    = s.text ;
    d.phone  = s.phone ;
    d.send   = 1;
    d.ts     = formatted_time_now_iso_S();
    d.type   = s.type;
    d.nphones    = 1 + std::count(d.phone.begin(), d.phone.end(), ';' ) ;
    
    return table.add(d);
}

static int set_status_sms(int64_t id, int sent)
{
    DB_T& db = oson_this_db ;
    Sms_info_table table(db);
    return table.set_status(id, sent);
}
}



static bool valid_phone_list(const std::string & phone_list)
{
    // <phone-1>;<phone-2>;...;
    enum Status{ ST_NONE = 0, ST_SEPARATOR = 1, ST_DIGIT = 3 } ;
    enum { PHONE_LENGTH = 12 } ;
    
    enum Status status = ST_NONE;
    int len_digit = 0;
    int n = 0;
    for(char c : phone_list)
    {
        switch(c)
        {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                if (status == ST_NONE || status == ST_SEPARATOR ) {
                    status = ST_DIGIT ;
                    len_digit = 0;
                    ++n;
                }
                ++len_digit;
                if (len_digit > PHONE_LENGTH ) return false;
                break;
            case ';':
            status = ST_SEPARATOR;//after a separator
            if (len_digit != PHONE_LENGTH) return false;
            break;
            default:
                return false;
        }
    }
    
    switch(status)
    {
        case ST_NONE: return false;
     
        case ST_SEPARATOR:
            return n > 0 ;
        
        case ST_DIGIT:
            return len_digit == PHONE_LENGTH;
        default:
            return false;
    }
}

static std::pair< std::string, int > make_request(const SMS_info_T& sms  )
{
    SCOPE_LOG(slog);
    long long ref_id = sms.id ;
    const std::string & msisdn = sms.phone;
    
    if ( ! ref_id )
        ref_id = 1;
    
    slog.DebugLog("msisdn: %.*s", std::min<int>(2048, msisdn.length()), msisdn.c_str() ) ;
    
    std::string request;
    size_t nphones = 0;
    
    // a single msisdn
    if (  msisdn.find(';')   == msisdn.npos )
    {
        request = 
        "<bulk-request  login=\"Oson\"  password=\"O$o54n!N\"  ref-id=\"" + num2string(ref_id) + "\"   "
        " delivery-notification-requested=\"false\"  "
        " version=\"1.0\">\n "
        "<message id=\"1\"  msisdn=\"" + msisdn + "\"  validity-period=\"1\"  priority=\"1\">\n "
        "<content type=\"text/plain\">" + sms.text + "</content> "
        "</message> "
        "</bulk-request>";
        
        nphones = 1;
    }
    else //  multiple msisdn (s)
    {
        std::string::size_type pos = 0, cur, id = 0;
        request = "<bulk-request  login=\"Oson\"  password=\"O$o54n!N\"  ref-id=\"" + num2string(ref_id) + "\"   "
                 " delivery-notification-requested=\"false\"  "
                 " version=\"1.0\">\n " ;
        
        //@Note: if there more than 32 phone - server may crashed!!!
        static const size_t max_limit_phones = 32;
        
        for( pos = 0 ; pos < msisdn.size(); pos = cur + 1 )
        {
            cur = msisdn.find(';', pos)  ;// != msisdn.npos
            if (cur == msisdn.npos)
                cur = msisdn.size();
            
            std::string phone = msisdn.substr(pos, cur - pos); 
            
            if ( ! phone.empty() )
            {
                request += "<message id=\"" + to_str(++id) + "\" msisdn=\"" + phone + "\" validity-period=\"1\" priority=\"1\">\n"
                           "<content type=\"text/plain\">" + sms.text + "</content> </message>\n" ;
                
                ++nphones;
                
                if (nphones >= max_limit_phones ) {
                    break;
                }
            }
        }
        
        request += "</bulk-request>";
    }
    
    return std::make_pair( request, nphones ) ;
}

std::pair< std::string , int > make_request_v2( const SMS_info_T& sms)
{
    SCOPE_LOG(slog);
    //    {
    //      "messages":
    //      [
    //    {
    //       "recipient":"998977332585",
    //       "message-id":"10141",
    //
    //       "sms":{
    //
    //               "originator": "3700",
    //               "content": {
    //                   "text": "Вы получили бонус с размером -100 сум!!!"
    //               }
    //        }
    //    }
    //
    //    
    //      ]
    //  }
    //@Note: if there more than 32 phone - server may crashed!!!
    static const size_t max_limit_phones = 32;
    const std::string& msisdn = sms.phone;    
    std::vector< std::string > phones;
    std::size_t pos = 0, cur = 0;
    for( ; pos < msisdn.size(); pos = cur + 1 )
    {
        cur = msisdn.find(';', pos)  ;// != msisdn.npos
        if (cur == msisdn.npos)
            cur = msisdn.size();

        std::string phone = msisdn.substr(pos, cur - pos); 
        
        if (phone.empty())continue;
        
        if (phones.size() >= max_limit_phones)
            break;
        phones.push_back(phone);
    }
    
    std::string text; 
    //@Note convert sms.text to JSON string format.
    for(char c: sms.text)
    {
        switch(c)
        {
            case '\\':  text += '\\'; text += '\\'; break;
            case '"':   text += '\\'; text += '"' ; break;
            case '/' :  text += '\\'; text += '/' ; break;
            case '\b':  text += '\\'; text += 'b' ; break;
            case '\n':  text += '\\'; text += 'n' ; break;
            case '\r':  text += '\\'; text += 'r' ; break;
            case '\t':  text += '\\'; text += 't' ; break;
            default:
                text += c;
                break;
        }
    }
    std::string res = "{ \"messages\" : [   ";
    char comma = '\n';
    for(std::string const& phone: phones )
    {
        res += comma;
        res += "{  \"recipient\": \"" + phone + "\",   \"message-id\": \"" + to_str(sms.id ) +"\",  "
               "\"sms\": { \"originator\": \"3700\", \"content\": { \"text\": \"" + text + "\" } } } " ;
        
        comma = ',';
        
    }
    res += " ] } " ;
    
    return std::make_pair( res, (int)phones.size() ) ;
    
}

oson::SMS_manager::SMS_manager( const oson::io_service_ptr  & io_service)
: ios_(io_service)
{}

oson::SMS_manager::~SMS_manager()
{
    
}




namespace
{
    typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;
    
class sms_session : public std::enable_shared_from_this< sms_session > 
{
public:
    sms_session( const io_service_ptr & io_service)
    : io_service(io_service)
    {
        SCOPE_LOGD(slog);
    }

    ~sms_session()
    {
        SCOPE_LOGD(slog);
    }
    
    void async_start( SMS_info_T sms_info)
    {
        SCOPE_LOG(slog);
        
        if ( ! valid_phone_list(sms_info.phone ) ) {
            slog.WarningLog("NOT VALID DST: %s ", sms_info.phone.c_str());
            return ;
        }
        
        sms_info.id = add_table(sms_info);
        
        m_sms_info = sms_info;
        
        std::string request; 
        int nphones;
        
        std::tie(request, nphones) = ::make_request( sms_info  );
        
        slog.DebugLog("SMS Request: %.*s\n",  ::std::min<int> (1024,  request.length()), request.c_str());
    
        std::string query = options.url ;//"http://91.204.239.42:8081/re-smsbroker";

        oson::network::http::request req_  = oson::network::http::parse_url(query);
        req_.method           =  "POST"     ;
        req_.content.type     =  "text/xml" ;
        req_.content.charset  =  "UTF-8"    ;
        req_.content.value    =  request    ;
    
        typedef oson::network::http::client_http client_http;
        typedef client_http::pointer pointer;
        
        pointer client = std::make_shared< client_http >(  io_service );
        
        client->set_request(req_);
        client->set_response_handler( std::bind(&sms_session::on_finish, shared_from_this(), std::placeholders::_1, std::placeholders::_2 ) ) ;
        client->async_start();
    }
    
    void async_start_v2( SMS_info_T  sms_info)
    {
        SCOPE_LOG(slog);
        
        if ( ! valid_phone_list(sms_info.phone ) ) {
            slog.WarningLog("NOT VALID DST: %s ", sms_info.phone.c_str());
            return ;
        }
        
        sms_info.id = add_table(sms_info);
        
        m_sms_info = sms_info;
        
        std::string request;
        int nphones;
        
        std::tie(request, nphones) = ::make_request_v2(sms_info);
        
        slog.DebugLog("SMS Request: %.*s\n",  ::std::min<int> (1024,  request.length()), request.c_str());
        //http://91.204.239.42:8083/broker-api/send
        //Authorization: Basic b3NvbjpPcyNuITUj
        //Content-Type: application/json;charset=UTF-8
    
        std::string url  = options.url_v2 ;
        std::string auth_basic = "Authorization: Basic " + options.auth_basic_v2;
        
        auto http_req = oson::network::http::parse_url(url);
        http_req.method = "POST";
        http_req.content.type= "application/json";
        http_req.content.charset = "UTF-8";
        http_req.content.value = request;
        http_req.headers.push_back( auth_basic );//@Note: take this value from config file.
        
        auto client = std::make_shared< oson::network::http::client_http>(io_service);
        client->set_request(http_req);
        client->set_response_handler(std::bind(&sms_session::on_finish_v2, shared_from_this(), std::placeholders::_1, std::placeholders::_2) ) ;
        client->async_start();
    }
    
    void set_options(oson::sms_runtime_options_t options)
    {
        this->options = options;
    }
    
    
    void async_get_status(std::string message_id, std::function<void(std::string const&)> handler )
    {
        SCOPE_LOGD(slog);
        
        std::string request = "{  \"message-id\": [ \"" + message_id + "\" ] } " ;
        std::string url  = options.url_v2 ;
        std::string auth_basic = "Authorization: Basic " + options.auth_basic_v2;
        
        auto http_req = oson::network::http::parse_url(url);
        http_req.method = "POST";
        http_req.content.type= "application/json";
        http_req.content.charset = "UTF-8";
        http_req.content.value = request;
        http_req.headers.push_back( auth_basic );//@Note: take this value from config file.
        
        auto client = std::make_shared< oson::network::http::client_http>(io_service);
        client->set_request(http_req);
        client->set_response_handler( std::bind(&sms_session::on_finish_status, shared_from_this(), handler, std::placeholders::_1, std::placeholders::_2) ) ;
        client->async_start();
    } 
        
    
private:
    void on_finish_status(std::function<void(std::string const&)> handler, std::string content, boost::system::error_code ec   )
    {
        SCOPE_LOGD(slog);
        
        slog.InfoLog("ec( %d ) : %s", ec.value(), ec.message().c_str());
        
        return handler(content);
    }
    
    void on_finish(const std::string& response, const ::boost::system::error_code& ec)
    {
        SCOPE_LOGD(slog);
        if (static_cast<bool>(ec) ) {
            slog.WarningLog("Cant sent sms.");
            ::set_status_sms(m_sms_info.id, 0/*does not send*/ ) ;
        }
        slog.DebugLog("Response: %.*s\n",  ::std::min<int>(2048,  response.length()), response.c_str() );
    }
    
    void on_finish_v2(const std::string& response, const ::boost::system::error_code& ec )
    {
        SCOPE_LOGD(slog);
        if (static_cast<bool>(ec)){
            slog.WarningLog("cant sent with new api!");
            ::set_status_sms(m_sms_info.id, 0/*does not send*/ ) ;
            return async_start(m_sms_info);
        }
        slog.DebugLog("Response: %.*s\n",  ::std::min<int>(2048,  response.length()), response.c_str() ); 
    }
private:
    io_service_ptr    io_service;
    oson::sms_runtime_options_t options;
    SMS_info_T m_sms_info;
};
    
} // end noname namespace


void oson::SMS_manager::async_send( SMS_info_T  sms)
{
        return async_send_v2(sms);
    
//    //@Note moved a new api.
//    SCOPE_LOG(slog);
//    auto s = std::make_shared< sms_session > (ios_);
//    s->set_options(options_);
//    s->async_start(sms);
}

void oson::SMS_manager::async_send_v2( SMS_info_T sms)
{
    SCOPE_LOG(slog);
    auto s = std::make_shared< sms_session > (ios_);
    s->set_options(options_);
    s->async_start_v2( sms );
}
    
const oson::io_service_ptr&  oson::SMS_manager::io_service()const
{
    return ios_;
}

void oson::SMS_manager::set_runtime_options( const sms_runtime_options_t options ) 
{
    options_ = options;
}

void oson::SMS_manager::get_status(std::string message_id, std::function<void(std::string const&)> handler)
{
    SCOPE_LOG(slog);
    auto s = std::make_shared< sms_session > (ios_);
    s->set_options(options_);
    s->async_get_status(message_id, handler);
}

/*************************************************************************************************************/