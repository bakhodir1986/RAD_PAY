
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <stdexcept>
#include <vector>
#include <functional>
#include <memory>

#include "http_request.h"
#include "Pusher.h"
#include "utils.h"
#include "log.h"
#include "types.h"



#define MAXPAYLOAD_SIZE 256
#define DEVICE_BINARY_SIZE 32
#define APNS_SANDBOX_HOST "gateway.sandbox.push.apple.com"
#define APNS_SANDBOX_PORT 2195

#define APNS_HOST "gateway.push.apple.com"
#define APNS_PORT 2195


static inline int charToHex_fast(char value) 
{
    return (value >= '0' && value <= '9') 
            ? ( value - '0' ) 
            : ( value >= 'a' && value <= 'f') 
                 ? (value - 'a' + 10) 
                 : (value >= 'A' && value <= 'F')  
                      ? (value - 'A' + 10) 
                      : 0 ;
	
}

static std::string binaryToken_fast(const std::string& input) 
{
    size_t len = input.length();
    std::string result(len/2, '\0');
    std::string::iterator it = result.begin();
	
    
    for (size_t i = 0; i < len; i += 2) {
		(*it) = ( charToHex_fast(input[i]) << 4 ) | charToHex_fast(input[i+1]);
        ++it;
	}
    return result;
}


static std::string make_request_text(const oson::core::PusherContent& pushContent)
{
    //convert the content to json string
    std::string s;
    s += "{\"aps\":{\"alert\":\"";
    s += pushContent.content;
    s += "\",\"badge\":";
    s += to_str(pushContent.badge) ;
    s += ",\"sound\":\"";
    s += pushContent.sound;
    s += "\"}" ;

    if ( ! pushContent.userData.empty() ) {
        s += ",";
        s += pushContent.userData;
    }
    s += "}" ;

    return s;
}

static std::vector<uint8_t> make_ssl_write_payload(const std::string& deviceTokenBinary, const std::string& payload, long expirationDate)
{
    const std::size_t frame_length = (deviceTokenBinary.size() + 3)  + (3 + payload.size()) + (3 + 4) + (3 + 1); 
    std::vector< uint8_t > result(frame_length + 5) ;// 5 = 1 + 4, where 1 - ID, 4 - frame_length itself.
    /****|ID|FRAME-LENGTH|{ITEMS}    */
    //    2 
    //  ITEMS:  |ITEM-ID|ITEM-LENGTH|ITEM-DATA|
    ByteWriter_T  writer(result);
    writer.writeByte(2); // ID
    writer.writeByte4(frame_length); // frame-length
    
    //1. ITEM-ID: 1   -- DEVICE TOKEN
    writer.writeByte(1);
    writer.writeByte2(deviceTokenBinary.length());
    writer.writeString(deviceTokenBinary);
    
    //2. ITEM-ID: 2   -- PAYLOAD
    writer.writeByte(2);
    writer.writeByte2(payload.size());
    writer.writeString(payload);
    
    //3. ITEM-ID: 4 -- EXPIRATION DATE
    writer.writeByte(4);
    writer.writeByte2(4);//4 bytes
    writer.writeByte4(expirationDate);//big endian
    
    //4. ITEM-ID: 5 -- PRIORITY
    writer.writeByte(5);
    writer.writeByte2(1);
    writer.writeByte(10);//immediately
    //============================================
    return result;
}

/****************************************************************************************************************************************/

oson::core::ios_push_session::ios_push_session(const io_service_ptr & io_service, const ssl_ctx_ptr& ctx)
  : io_service(io_service), ctx(ctx)
{
    SCOPE_LOGD(slog);
}

void oson::core::ios_push_session::async_start( const PushInfo& info )
{
    SCOPE_LOGD(slog);
    
    if (info.tokens.empty() )
    {
        io_service->post( std::bind(&ios_push_session::on_finish, shared_from_this(), std::string(), boost::system::error_code() ) ) ;
        return ;
    }
    
    

//    const long expiration =  time(0) + 86400  ;//next day
//    
//    std::string payload = ::make_request_text( info.content );
    
//    std::vector< std::vector<unsigned char> > requests(info.tokens.size());
//    
//    for(size_t i = 0; i < info.tokens.size() ; ++i)
//    {
//        std::string bin_token = binaryToken_fast( info.tokens[i]);
//        
//        requests[i] = ::make_ssl_write_payload(bin_token, payload, expiration);
//        
//    }
    
    struct filler_t
    {
        mutable size_t i;
        
        oson::core::PushInfo info;
        std::string payload;
        long expiration ;
        
        explicit filler_t(const oson::core::PushInfo& info)
        : i(0)
        , info(info)
        , payload(::make_request_text(info.content) )
        , expiration(time(0) + 86400 /*next day*/ ) 
        {}
        
        void operator()(std::vector<unsigned char>& out_req )const
        {
            const size_t token_size = info.tokens.size();
            if (i >= token_size){
                out_req.clear();
                return ;
            }
            std::string bin_token = ::binaryToken_fast(info.tokens[i]);
            out_req = ::make_ssl_write_payload(bin_token, payload, expiration);
            
            ++i;/*go to next*/
        }
    };
    
    std::string address =  info.isSandBox ? APNS_SANDBOX_HOST : APNS_HOST;
    unsigned short port =  info.isSandBox ? APNS_SANDBOX_PORT : APNS_PORT;
    
    
    typedef oson::network::tcp::client_ssl client_ssl;
    typedef client_ssl::pointer pointer;
    
    pointer cl = std::make_shared< client_ssl > (io_service, ctx);
    
    struct filler_t push_filler (info);
    
    cl->set_address(address, port);
    cl->set_request_filler(push_filler);
    cl->set_response_handler( std::bind(&ios_push_session::on_finish, shared_from_this(), std::placeholders::_1, std::placeholders::_2) )  ;
    
    cl->async_start();
    
}

oson::core::ios_push_session::~ios_push_session()
{
    SCOPE_LOGD(slog);
}

void oson::core::ios_push_session::on_finish(const std::string& content, const boost::system::error_code& ec)
{
    SCOPE_LOG(slog);
    slog.DebugLog("IO_PUSH_RESPONSE: %.*s\n",  ::std::min<int>(2048, content.length() ), content.c_str());
}
