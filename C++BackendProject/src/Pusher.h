/*
 * @author: Normuradov Khurshid
 * 
 */

#ifndef OSON_PUSHER_H_
#define OSON_PUSHER_H_

#include <string>
#include <vector>
#include <memory>


#include <boost/asio/io_service.hpp>
#include "config_types.h"

namespace oson{ namespace core{

struct PusherContent
{
	std::string content;
	int badge;
	std::string sound;
	std::string userData;
};

struct PushInfo
{
    PusherContent content;
    
    std::vector< std::string > tokens;
    
    bool isSandBox;
};

typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;

typedef std::shared_ptr< boost::asio::ssl::context > ssl_ctx_ptr ;

class ios_push_session: public std::enable_shared_from_this< ios_push_session > 
{
public:
    ios_push_session(const io_service_ptr & io_service,
                     const ssl_ctx_ptr& ctx
                   );
    
    ~ios_push_session();
    
    void async_start( const PushInfo& info );
private:
    void on_finish(const std::string&, const boost::system::error_code& );
    
    void filler(std::vector<unsigned char>& out);
    
private:
    ios_push_session(const ios_push_session&); // = delete
    ios_push_session& operator = (const ios_push_session&); // = delete
private:
    io_service_ptr io_service ;
    ssl_ctx_ptr    ctx;
};

    
}} // end oson::core

#endif /* OSON_PUSHER_H_ */
