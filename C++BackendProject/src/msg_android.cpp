
#include <functional>
#include <memory>

#include "msg_android.h"

#include "http_request.h"

#include "log.h"


const char* get_intent_name(int type){
    switch(type)
    {
        case MSG_TYPE_REQUEST               : return  "intent_notification_bill_request" ;
        case MSG_TYPE_TRANSACTION           : return  "intent_notification_transaction"  ;
        case MSG_TYPE_MESSAGE               : return  "intent_new_xmpp_message"          ;
        case MSG_TYPE_PERIODIC_BILL_MESSAGE : return  "intent_periodic_bill_message"     ;
        case MSG_TYPE_BULK_MESSAGE          : return  "intent_bulk_message"              ;
        case MSG_TYPE_PURCHASE_MESSAGE      : return  "intent_purchase_message"          ;
        case MSG_TYPE_BONUS_MESSAGE         : return  "intent_bonus_message"             ;
    }
    return "intent_unknown_message";
}
Msg_android_content_T::Msg_android_content_T() {
    type = 0;
}
Msg_android_content_T::Msg_android_content_T(const std::string& msg, int type)
        : msg(msg), type(type){}

oson::core::android_push_session::android_push_session( const io_service_ptr & io_service, const ssl_ctx_ptr& ctx)
: io_service(io_service),   ctx(ctx), dev_id(), handler_(empty_handler())
{}

void oson::core::android_push_session::async_start(
                    const Msg_android_content_T& content,
                    const std::string & token, uint64_t dev_id
                )
{

    SCOPE_LOG(slog);
    
    this->dev_id = dev_id;
    
    std::string server = "https://fcm.googleapis.com/fcm/send";

    oson::network::http::request req_ = oson::network::http::parse_url(server);
    req_.method = "POST";
    req_.headers.push_back( "Authorization: key=AAAAzjjDzJE:APA91bFnhy75VOCw0VrqbHuezWmDHRTm7vJzFKcO4sq1uElpwwjlJzREAxAAdKx77DInAz5wW7sHQmy5gEVqBqDDPdTdTilfPCuO2iBNpHCVTfSFbmcnKT6GOPBVhen1TBU0yO_3m5sO" );
    req_.content.charset =  "utf-8";
    req_.content.type    =  "application/json";
    req_.content.value   =  "{\"to\": \"" +token+"\",  \"data\": { \"intent_name\": \"" + get_intent_name( (int) content.type )  + 
                            "\", \"message\": \"" + content.msg + "\", \"title\": \"Oson\"} } " ;

    
    typedef oson::network::http::client client_t;
    typedef client_t::pointer pointer;
    
    pointer c = std::make_shared< client_t >(io_service, ctx ) ;
    
    c->set_request(req_);
    
    c->set_response_handler(std::bind(&android_push_session::on_finish, shared_from_this(), std::placeholders::_1, std::placeholders::_2) ) ;
    
    c->async_start();
    
}
oson::core::android_push_session::~android_push_session()
{
    SCOPE_LOG(slog);
}

void oson::core::android_push_session::set_response_handler( const response_handler_t & h)
{
    if (static_cast< bool > (h) ){
        handler_ = h;
    } else {
        handler_ = empty_handler();
    }
}

void oson::core::android_push_session::on_finish(const std::string& content, const boost::system::error_code & ec) 
{
    SCOPE_LOG(slog);
    slog.DebugLog("Android PUSH RESPONSE: %.*s", (int)(content.length() > 2048 ? 248 : content.length()), content.c_str());

    handler_(content, ec, dev_id);
}
     