#ifndef MSG_ANDROID_T_H
#define MSG_ANDROID_T_H


#include <string> // std::string
#include <memory>
#include <functional>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>


#include "types.h"

enum Msg_type__T {
    MSG_TYPE_REQUEST     = 0,
    MSG_TYPE_TRANSACTION = 1,
    MSG_TYPE_MESSAGE     = 2,
    MSG_TYPE_PERIODIC_BILL_MESSAGE = 3,
    MSG_TYPE_BULK_MESSAGE = 4,
    MSG_TYPE_PURCHASE_MESSAGE = 5,
    MSG_TYPE_BONUS_MESSAGE = 6,
};

struct Msg_android_content_T 
{
    std::string msg;
    int type;

    Msg_android_content_T() ;
    Msg_android_content_T(const std::string& msg, int type);
};


namespace oson{ namespace core{
    
typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;

typedef std::shared_ptr< boost::asio::ssl::context > ssl_ctx_ptr;

class android_push_session: public std::enable_shared_from_this< android_push_session >
{
public:
    
    struct empty_handler{  void operator()(const std::string&, const boost::system::error_code&, uint64_t ) const {} } ;
    
    typedef std::function< void(const std::string&, const boost::system::error_code&,  uint64_t  ) > response_handler_t;
    
    explicit android_push_session( const io_service_ptr & io_service , const ssl_ctx_ptr& ctx );
             
    
    void async_start( const Msg_android_content_T & content,
                        const std::string & token, uint64_t dev_id
                     );
    ~android_push_session();
    
    void set_response_handler( const  response_handler_t & h);
    
private:
    android_push_session(const android_push_session&); // = delete
    android_push_session& operator = (const android_push_session& ); // = delete
private:
    void on_finish(const std::string& content, const boost::system::error_code & ec);

    
private:
    io_service_ptr      io_service;
    ssl_ctx_ptr         ctx;
    uint64_t dev_id;
    response_handler_t  handler_;
    
};    
}} // end oson::core

#endif // MSG_ANDROID_T_H
