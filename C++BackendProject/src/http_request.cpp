
#include <sstream>
#include <functional>
#include <memory>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "http_request.h"
#include "log.h"
#include "utils.h"

namespace
{
    struct empty_handler
    {
        void operator()(const std::string& , const boost::system::error_code& ){}
    };
}

static void encode_transfer_chunked(std::string& body);

namespace http = oson::network::http;


http::client::pointer http::client::get_pointer ()
{
    return  shared_from_this();
}

 oson::network::http::client::pointer   oson::network::http::client::create( io_service_ptr io_service, ssl_context_ptr ctx )
 {
     return std::make_shared< self_type >(io_service, ctx ) ; 
 }
 

oson::network::http::client::client(io_service_ptr io_service, ssl_context_ptr ctx)
    : resolver_(  *io_service )
    , socket_(  *io_service ,  *ctx )
    , timer_(  *io_service  ) 
    , handler_(empty_handler())
    , chunked_(false)
    , timeout_milliseconds_( 60 * 1000 )  // 60 seconds
    , verify_mode_(boost::asio::ssl::context::verify_none)
{
    //SCOPE_LOGD(slog);
}

http::client::~client()
{
    //SCOPE_LOGD(slog);
    
    if (static_cast<bool>(handler_)) {
        call_handler(content_, boost::system::error_code());
    }
}


     
void http::client::set_request(const request& req)
{
    SCOPE_LOG(slog);
    this->host = req.host;
    this->port = req.port;
    
    slog.DebugLog("host: '%s'  port: '%s'  path: '%s'",  req.host.c_str(), req.port.c_str(), req.path.c_str() );
    
    const bool hasContent = ! req.content.value.empty() ;
    
    const bool hasHeader  = ! req.headers.empty();

    if (hasHeader){
        for(auto const& header: req.headers) {
            slog.DebugLog("header: %s", header.c_str());
        }
    }
    std::string method = "GET";
    if (! req.method.empty() )
        method = req.method;
    
    std::string contentType = "application/text";
    
    if ( ! req.content.type.empty() )
    {
        if (req.content.type.find('/') == std::string::npos) 
            contentType = "application/" + req.content.type;  // only json, or xml --> application/json,  application/xml
        else
            contentType = req.content.type; // text/xml,  or text/json  -->>  text/xml,  text/json
    }
    
    std::string charset = "utf-8";
    
    if (! req.content.charset.empty() )
    {
        charset = req.content.charset;
    }
    
    std::ostream request_stream(&request_);
    
    
    request_stream << method << " " << req.path << " HTTP/1.1\r\n"; 
    request_stream << "Host: " << req.host << "\r\n";
    if ( hasContent ) request_stream << "Content-Length: " << req.content.value.length() << "\r\n";
    if (hasContent) request_stream << "Content-Type: " << contentType << "; charset=" << charset << " \r\n";
    if ( hasHeader ){
        for(const auto& header : req.headers) {
            request_stream << header << "\r\n";
        }
    }
    request_stream << "Accept: */*\r\n";
    request_stream << "Connection: close\r\n\r\n";
    if (hasContent) request_stream << req.content.value;

    {
        const char* header = boost::asio::buffer_cast< const char*>(request_.data() ) ;
        size_t size = request_.size();
        //@Note write log no more 2048 symbols.
        slog.DebugLog("REQUEST: %.*s\n", ::std::min<int>(size, 2048), header);
    }
}

void http::client::set_response_handler(const response_handler_t& h )
{
    if ( static_cast< bool > ( h ) ) {
        handler_ = h;
    } else {
        handler_ = empty_handler();
    }
}

void http::client::async_start()
{
    SCOPE_LOG(slog);
    
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    ::boost::asio::ip::tcp::resolver::query query( host, port );
 
    resolver_.async_resolve(query,
        std::bind(&client::handle_resolve, get_pointer(),
          std::placeholders::_1,
          std::placeholders::_2));
 
    start_timeout();
 
}
     
void http::client::set_timeout(int timeout_milliseconds)
{
    SCOPE_LOG(slog);
    this->timeout_milliseconds_ = oson::utils::clamp< int >(  timeout_milliseconds, 10, 100 * 1000 ) ;//no more 100 seconds
    slog.InfoLog("timeout %d ms, installed: %d ms. ", timeout_milliseconds, this->timeout_milliseconds_ );
}

void http::client::set_verify_mode(boost::asio::ssl::context::verify_mode verify_mode)
{
    this->verify_mode_ = verify_mode ;
}
     

std::string http::client::body()const
{
    return content_;
}

void oson::network::http::client::handle_resolve(const boost::system::error_code& err,
                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    if (!err)
    {
      //  SCOPE_LOG(slog);
        start_timeout();
        
        socket_.set_verify_mode( this->verify_mode_ );
        socket_.set_verify_callback(
                std::bind(&client::verify_certificate, std::placeholders::_1, std::placeholders::_2));
        
        boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                               std::bind(&client::handle_connect, get_pointer(),
                                           std::placeholders::_1));
    }
    else
    {
        SCOPE_LOG(slog); 

        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        return call_handler("", err); //std::cout << "Error: " << err.message() << "\n";
    }
}

// this is now static method.
bool http::client::verify_certificate(bool preverified,
                        boost::asio::ssl::verify_context& ctx)
{
    SCOPE_LOG(slog);

    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // In this example we will simply print the certificate's subject name.
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    
    slog.InfoLog("subject-name: %s", subject_name ) ;
    
    return preverified;
}


 void http::client::handle_connect(const boost::system::error_code& err)
 {
    if (!err)
    {
        start_timeout();

        socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                    std::bind(&client::handle_handshake, get_pointer(),
                                                std::placeholders::_1));
        
    }
    else
    {
        SCOPE_LOG(slog);

        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
     
        return call_handler("", err); //std::cout << "Error: " << err.message() << "\n";
    }
 }

void http::client::handle_handshake(const boost::system::error_code& ec)
{
   SCOPE_LOG(slog);
   if (!ec)
   {
       start_timeout();
       
       // The handshake was successful. Send the request.
       boost::asio::async_write(socket_, request_,
                                std::bind(&client::handle_write_request, get_pointer(),
                                            std::placeholders::_1));
       
   }
   else
   {
        cancel_timeout();
        slog.ErrorLog("error: code: %d ,  msg = %s", ec.value(), ec.message().c_str());
        return call_handler("", ec);
   }
}

 void http::client::handle_write_request(const boost::system::error_code& err)
 {
     if (!err)
     {
         start_timeout();
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            boost::asio::async_read_until(socket_, response_, "\r\n",
                                          std::bind(&client::handle_read_status_line, get_pointer(),
                                                      std::placeholders::_1));

            
     }
     else
     {
         cancel_timeout();
        SCOPE_LOG(slog);
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        
        return call_handler("", err);
     }
 }

 void http::client::handle_read_status_line(const boost::system::error_code& err)
 {
    cancel_timeout();//no more needed timer.
       
    if (!err)
    {
        
         // Check that response is OK.
        std::istream response_stream(&response_);
        std::string http_version;
        unsigned int status_code;


        response_stream >> http_version;

        response_stream >> status_code;

        std::string status_message;

        std::getline(response_stream, status_message);
        
        if ( ! response_stream || http_version.substr( 0, 5 ) != "HTTP/")
        {
                 SCOPE_LOG(slog);

            slog.ErrorLog("Invalid response");
            return call_handler("", boost::asio::error::make_error_code(boost::asio::error::basic_errors::not_connected) );
           

        }

        if (status_code == 302 )
        {
            // found redirect.
            SCOPE_LOG(slog);
            slog.WarningLog("Http error 302 Found . skip this.");
        }
        else
        if (status_code != 200)
        {
             SCOPE_LOG(slog);

            slog.ErrorLog("Response returned with status code %d", status_code);
            //std::cerr << "Response returned with status code " << status_code << "\n";
            return call_handler("", boost::asio::error::make_error_code(boost::asio::error::basic_errors::not_connected) );

        }

        // Read the response headers, which are terminated by a blank line.
        boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
            std::bind(&client::handle_read_headers, get_pointer(),
              std::placeholders::_1));
        
    }
    else
    {
        SCOPE_LOG(slog);

        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());

        return call_handler("", err);//std::cout << "Error: " << err << "\n";
    }
 }

 void http::client::handle_read_headers(const boost::system::error_code& err)
 {
    if (!err)
    {
      // Process the response headers.
      std::istream response_stream(&response_);
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
      {
          //@Note: Transfer-Encoding: chunked  
          //   Need transform chunked encoding.
          
          boost::algorithm::trim(header);
          if (boost::algorithm::istarts_with(header, "Transfer-Encoding:")){
              if (boost::algorithm::iends_with(header, "chunked")) {
                  SCOPE_LOG(slog);
                  slog.WarningLog("chunked transfer-encoding");
                  this->chunked_ = true;
              }
          }

      }

      // Write whatever content we already have to output.
      if (response_.size() > 0){
          std::stringstream strm;
          strm << &response_;
          content_ += strm.str();
          
      }

      // Start reading remaining data until EOF.
          boost::asio::async_read(socket_, response_,
              boost::asio::transfer_at_least(1),
              std::bind(&client::handle_read_content, get_pointer(),
                std::placeholders::_1));
          
          
    }
    else
    {
        SCOPE_LOG(slog);
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        
        return call_handler("", err);
    } 
 }


 void http::client::handle_read_content(const boost::system::error_code& err)
 {
    SCOPE_LOG(slog);
    
    if (!err)
    {
      // Write all of the data that has been read so far.
         if (response_.size() > 0)
         {
            std::stringstream stream ;
            stream <<   &response_  ;
            std::string s= stream.str();
            
            content_ += s;
            
            slog.DebugLog("content: %.*s", ::std::min<int>(512, s.length()),  s.c_str());
         }
       
      // Continue reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          std::bind(&client::handle_read_content, get_pointer(),
            std::placeholders::_1));
      
      
    }
    else if (err == boost::asio::error::eof)
    {
        if (this->chunked_)
            encode_transfer_chunked(content_);
        
        return call_handler(content_, boost::system::error_code() );
    }
    else if (err.value() ==  335544539 || err == boost::asio::error::operation_aborted )
    {
        slog.WarningLog("short read: code: %d", err.value());
        
        do_async_ssl_shutdown();
        
        //code_ = boost;
        if (this->chunked_)
            encode_transfer_chunked(content_);
        
        //short read - is actually normal situation.
        if (err .value() == 335544539 )
        {
            return call_handler(content_, boost::system::error_code() ) ;
        }
        
        return call_handler(content_,  err );//std::cout << "Error: " << err << "\n";
    }
    else
    {
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        //code_ = err;
        if (this->chunked_)
            encode_transfer_chunked(content_);
        
        return call_handler(content_, err);//std::cout << "Error: " << err << "\n";
    }
 }
 void http::client::call_handler(const std::string& s, const boost::system::error_code& ec)
 {
     SCOPE_LOG(slog);
     if (static_cast< bool >(handler_)) {
        response_handler_t h;
        h.swap(handler_);
        h(s, ec); // if there exception throws or not, anywhy handler_ will destroyed!
     } else {
         slog.WarningLog("handler is empty!");
     }
 }
 
void http::client::start_timeout()
{
    timer_.expires_from_now( boost::posix_time::milliseconds( this->timeout_milliseconds_ )) ;
    timer_.async_wait(std::bind(&client::on_timeout, get_pointer(), std::placeholders::_1 ) ) ;
}

void ::oson::network::http::client::on_timeout( const boost::system::error_code&  ec )
{
    if ( ec  == boost::asio::error::operation_aborted ) 
    {
        ; // do nothing.
    }
    else
    {
        SCOPE_LOGD(slog);
        boost::system::error_code ignore_ec;

        socket_.lowest_layer().cancel(ignore_ec);
        
        call_handler(content_,  boost::asio::error::make_error_code(boost::asio::error::timed_out)) ;
    }
}

void http::client::cancel_timeout()
{
    timer_.cancel();
}

void ::oson::network::http::client::do_async_ssl_shutdown()
{
    //socket_.async_shutdown( std::bind(&client::on_shutdown, get_pointer(), std::placeholders::_1) ) ;
}
//
void ::oson::network::http::client::on_shutdown(boost::system::error_code ec)
{
    SCOPE_LOGD(slog);
    if ( ec == boost::asio::error::eof){
        ec.assign(0, ec.category());
    }
    
    if ( ! ec ) {
        boost::system::error_code ignore_ec;
        socket_.lowest_layer().close( ignore_ec );
    }
    
    
}
//      
       
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 

static int hex_digit(char c)
{
    if (c >= '0' && c <='9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static void encode_transfer_chunked(std::string& body)
{
    SCOPE_LOG(slog);
    size_t const n = body.length();
    size_t i = 0, pos = 0;
    std::string res;
    res.reserve( body.size() );
    while( i < n )
    {
        // 1. read chunk size
        size_t hex_sz = 0;
        while(i < n && (body[i]) != '\r')
        {
            int digit = hex_digit( body[ i++ ] );
            
            if (digit != -1) 
            {
                hex_sz = hex_sz * 16 + digit;
            }
        }
        slog.InfoLog("hex_sz: %zu, i: %zu", hex_sz, i);
        if (hex_sz == 0)
            break;
        
        if ( i < n && ( (body[i]) == '\r') )
            ++i;
        if ( i < n && ( (body[i]) == '\n'))
            ++i;
        
        //fix overflow.
        if (i + hex_sz > n)
            hex_sz = n - i;
        //2. read chunk body
        ////// append(const basic_string& __str, size_type __pos, size_type __n)
        //body.append(raw, i, hex_sz);
        //////////////////////////////////
        //  need move  [i... i + hex_sz]  from body  to [pos..pos+hex_sz]
        //------------------------------------------------
        //memmove(&body[0] + pos, &body[0] + i, hex_sz * sizeof( body[ 0 ] ) );
        res.append(body ,   i,  hex_sz );
        pos += hex_sz;
        ////////////////////////////////
        i += hex_sz;
        
        if ( i < n && ( (body[i]) == '\r') )
            ++i;
        if ( i < n && ( (body[i]) == '\n'))
            ++i;

        
    }
    
    body.swap( res );
}

http::request http::parse_url( const std::string& url ) // parse host, port, path
{
    std::string host, port, path;
    std::string::size_type idx;
    
    //By default host is URL.
    host = url;
    
    // url = "https://185.8.212.69:8443/api/admin/
    
    // cut protocol
    idx = host.find("://");
    
    if (idx != host.npos)
    {
        // port = "https"
        port = host.substr(0, idx);
        
        // host = 185.8.212.69:8443/api/admin/
        host = host.substr(idx + 3);
    }
    
    // cut path
    idx = host.find('/');
    if (idx != host.npos)
    {
        // path = "/api/admin/"
        path = host.substr(idx);
        
        //host = "185.8.212.69:8443
        host = host.substr(0, idx);
    }
    
    // cut port
    idx = host.find_last_of(':');
    if (idx != host.npos)
    {
        // port = "8443"
        port = host.substr(idx + 1);
        
        // host = "185.8.212.69
        host = host.substr(0, idx);
    }
    
    if (path.empty())
    {
        path = "/";
    }
    
    request result;
    result.host = host;
    result.port = port;
    result.path = path;
    return result;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////**********************************************************///////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

http::client_http::pointer http::client_http::get_pointer()
{
    return shared_from_this();
}

http::client_http::client_http( io_service_ptr  io_service )
 : resolver_(  *io_service  ) 
 , socket_ (  *io_service  )
 , chunked_(false)
 , handler_(empty_handler())
{
   // SCOPE_LOGD(slog);
}



void http::client_http::set_response_handler( const response_handler_t& h)
{
    if (static_cast< bool > (h ) ) {
        handler_ = h;
    } else {
        handler_ = empty_handler();
    }
}


void http::client_http::set_request(const request& req)
{
    SCOPE_LOGD(slog);
    this->host = req.host ;
    this->port = req.port ;
    
    //slog.DebugLog("host: '%s'  port: '%s'  path: '%s'",  req.host.c_str(), req.port.c_str(), req.path.c_str());
    
    const bool hasContent = ! req.content.value.empty() ;
    
    const bool hasHeader  = ! req.headers.empty();
    
    std::string method = "GET";
    if (! req.method.empty() )
        method = req.method;
    
    std::string contentType = "application/text";
    
    if ( ! req.content.type.empty() )
    {
        if(req.content.type.find('/') == std::string::npos)
            contentType = "application/" + req.content.type;
        else
            contentType = req.content.type ;
    }
    
    std::string charset = "utf-8";
    
    if (! req.content.charset.empty() )
    {
        charset = req.content.charset;
    }
    
    std::string path = req.path;
    if (path.empty())
        path = "/" ;
    
    slog.DebugLog("host: '%s'  port: '%s'  path: '%s'  method: '%s'  contentType: '%s'  charset = '%s' ",  
            req.host.c_str(), req.port.c_str(), path.c_str(), method.c_str(), contentType.c_str(), charset.c_str());
    

    std::ostream request_stream(&request_);
    
    
    request_stream << method << " " << path << " HTTP/1.1\r\n"; 
    request_stream << "Host: " << req.host << "\r\n";
    request_stream << "Accept: */*\r\n";
    if (hasContent)request_stream << "Content-Length: " << req.content.value.length() << "\r\n";
    request_stream << "Content-Type: " << contentType << "; charset=" << charset << "\r\n";
    if ( hasHeader ){
        for(const auto& header: req.headers) {
            request_stream <<  header  << "\r\n";
        }
    }
    
    if ( ! req.no_connection_close ){
        request_stream << "Connection: close\r\n\r\n";
    } else {
        request_stream << "\r\n";
    }
    
    if (hasContent)request_stream << req.content.value;

    //write to log.
    {
        const char* header=boost::asio::buffer_cast<const char*>(request_.data());
        slog.DebugLog("Request: %s\n",  header);
    }
}

void http::client_http::async_start()
{
    //SCOPE_LOGD( slog ); 
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    ::boost::asio::ip::tcp::resolver::query query( host,  port ) ;
    resolver_.async_resolve(query,
        std::bind(&client_http::handle_resolve, get_pointer(),
          std::placeholders::_1,
          std::placeholders::_2));

}

http::client_http::~client_http()
{
}

std::string http::client_http::body()const
{
    return content_;
}

boost::system::error_code http::client_http::error_code()const
{
    return code_;
}

void http::client_http::handle_resolve(const boost::system::error_code& err,
                    boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    
    if (!err)
    {
      //  slog.DebugLog("Resolve OK");
        boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                               std::bind(&client_http::handle_connect, get_pointer(),
                                           std::placeholders::_1));
    }
    else
    {
        SCOPE_LOG(slog);
        code_ = err;
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        handler_(content_, err);
    }
   
}

void http::client_http::handle_connect(const boost::system::error_code& ec)
{
  
   if (!ec)
   {
//       const char* header=boost::asio::buffer_cast<const char*>(request_.data());
       //slog.DebugLog("Request: %s\n",  header);
       
       // The handshake was successful. Send the request.
       boost::asio::async_write(socket_, request_,
                                std::bind(&client_http::handle_write_request, get_pointer(),
                                            std::placeholders::_1));
   }
   else
   {
        SCOPE_LOG(slog);
        code_ = ec;
        slog.ErrorLog("error: code: %d ,  msg = %s", ec.value(), ec.message().c_str());
        handler_(content_, ec);
   }
    
}

void http::client_http::handle_write_request(const boost::system::error_code& err)
{
     //SCOPE_LOG(slog);
     if (!err)
     {
                 // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            boost::asio::async_read_until(socket_, response_, "\r\n",
                                          std::bind(&client_http::handle_read_status_line, get_pointer(),
                                                      std::placeholders::_1));

     }
     else
     {
        SCOPE_LOG(slog);
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        code_ = err;
        handler_(content_, err);
     }
}

void http::client_http::handle_read_status_line(const boost::system::error_code& err)
{
    SCOPE_LOG(slog);
     if (!err)
     {
        // Check that response is OK.
        std::istream response_stream(&response_);
        std::string http_version;
        unsigned int status_code;


        response_stream >> http_version;

        response_stream >> status_code;

        std::string status_message;

        std::getline(response_stream, status_message);
        slog.DebugLog("status message: %s", status_message.c_str());
        if ( ! response_stream || http_version.substr( 0, 5 ) != "HTTP/")
        {
            slog.DebugLog("Invalid response");
            handler_("", boost::asio::error::make_error_code(boost::asio::error::basic_errors::not_connected) );
            return ;

        }

        if (status_code != 200)
        {
            slog.DebugLog("Response returned with status code %d", status_code);
            handler_("",  boost::asio::error::make_error_code(boost::asio::error::basic_errors::not_connected) );
            //std::cerr << "Response returned with status code " << status_code << "\n";
            return;// handler_("", err);

        }

        // Read the response headers, which are terminated by a blank line.
        boost::asio::async_read_until(socket_, response_, "\r\n\r\n",
            std::bind(&client_http::handle_read_headers, get_pointer(),
              std::placeholders::_1));
    }
    else
    {
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        code_ = err;
        handler_( content_, err);
    }
    
}

void http::client_http::handle_read_headers(const boost::system::error_code& err)
{
    SCOPE_LOG(slog);
    if (!err)
    {
      // Process the response headers.
      std::istream response_stream(&response_);
      std::string header;
      while (std::getline(response_stream, header) && header != "\r")
      {
          slog.DebugLog("header: %s", header.c_str());
          
          //@Note: Transfer-Encoding: chunked  
          //   Need transform chunked encoding.
          boost::algorithm::trim(header);
          if (boost::algorithm::istarts_with(header, "Transfer-Encoding:")){
              if (boost::algorithm::iends_with(header, "chunked"))
                  this->chunked_ = true;
          }
      }

      // Write whatever content we already have to output.
      if (response_.size() > 0){
          std::stringstream strm;
          strm << &response_;
          content_ += strm.str();
          slog.DebugLog("header(last): %s", strm.str().c_str());
        
      }

      // Start reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          std::bind(&client_http::handle_read_content, get_pointer(),
            std::placeholders::_1));
    }
    else
    {
       slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
       code_ = err;
       handler_(content_, err);
    } 

}

void http::client_http::handle_read_content(const boost::system::error_code& err)
{
    SCOPE_LOG(slog);
    if (!err)
    {
      // Write all of the data that has been read so far.
         if (response_.size() > 0)
         {
            std::stringstream stream ;
            stream <<   &response_  ;
            std::string s= stream.str();
            content_ += s;
            slog.DebugLog("content: %s",s.c_str());
         }
       
      // Continue reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          std::bind(&client_http::handle_read_content, get_pointer(),
            std::placeholders::_1));
    }
    else if (err == boost::asio::error::eof)
    {
        if (chunked_)
            encode_transfer_chunked(content_);

        handler_(content_, boost::system::error_code() );
    }
    else if (err.value() == 335544539)
    {
        slog.WarningLog("short read: code: %d", err.value());
        if (chunked_)
             encode_transfer_chunked(content_);

        handler_(content_, boost::system::error_code() );
    }
    else
    {
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        code_ = err;
        if (chunked_)
            encode_transfer_chunked(content_);

        handler_(content_, err);
    }
 
}
/**********************************************************************************************/ 
namespace tcp = oson::network::tcp;


tcp::client::pointer  tcp::client::get_pointer()
{
    return this->shared_from_this();
}

tcp::client::client( io_service_ptr io_service )
 : resolver_(  *io_service  ) 
 , socket_(  *io_service  ) 
 , handler_(empty_handler())
{
    SCOPE_LOGD(slog);
}

void tcp::client::set_address(const std::string& address, unsigned short port)
{
    address_ = address;
    port_    = port;
}

void tcp::client::set_request(const std::string& req)
{
    req_content_ = req;
}



 void tcp::client::set_response_handler(const response_handler& h)
 {
     if (static_cast< bool >( h ) ) {
         handler_ = h;
     } else {
         handler_ = empty_handler();
     }
 }
    


void tcp::client::async_start()
{
    SCOPE_LOG(slog);
    slog.DebugLog("address: '%s', port: %d,  request: '%.*s'", address_.c_str(), (int)port_, (std::min)(1024, (int)req_content_.length()), req_content_.c_str());
    
    std::ostream request_stream(&request_);
    request_stream << req_content_;
    
    
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    ::boost::asio::ip::tcp::resolver::query query(address_,  to_str(port_) );
    
    resolver_.async_resolve(query,
        std::bind(&client::handle_resolve, get_pointer(),
          std::placeholders::_1,
          std::placeholders::_2));

}
    
tcp::client::~client()
{
    SCOPE_LOGD(slog); 
}
     
void tcp::client:: handle_resolve(const boost::system::error_code& err,
                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    SCOPE_LOG(slog);
    if (!err)
    {
        slog.DebugLog("Resolve OK");
        boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                               std::bind(&client::handle_connect, get_pointer(),
                                           std::placeholders::_1));
    }
    else
    {
        code_ = err;
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
    }
   
    
}

void tcp::client:: handle_connect(const boost::system::error_code& ec)
{
  SCOPE_LOG(slog);
   if (!ec)
   {
       boost::asio::async_write(socket_, request_,
                                std::bind(&client::handle_write_request, get_pointer(),
                                            std::placeholders::_1));
   }
   else
   {
        slog.ErrorLog("error: code: %d ,  msg = %s", ec.value(), ec.message().c_str());
        return handler_( content_, ec);
   }
    
    
}

void tcp::client:: handle_write_request(const boost::system::error_code& err)
{
     SCOPE_LOG(slog);
     if (!err)
     {
            boost::asio::async_read(socket_, response_, boost::asio::transfer_at_least(1),
                                          std::bind(&client::handle_read_content, get_pointer(),
                                                      std::placeholders::_1));
     }
     else
     {
          slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
          return handler_(content_, err);
     }
    
}

void tcp::client:: handle_read_content(const boost::system::error_code& err)
{
    SCOPE_LOG(slog);
    if (!err)
    {
      // Write all of the data that has been read so far.
         if (response_.size() > 0)
         {
            std::stringstream stream ;
            stream <<   &response_  ;
            std::string s= stream.str();
            content_ += s;
            slog.DebugLog("content: %s",s.c_str());
         }
       
      // Continue reading remaining data until EOF.
      boost::asio::async_read(socket_, response_,
          boost::asio::transfer_at_least(1),
          std::bind(&client::handle_read_content, get_pointer(),
            std::placeholders::_1));
    }
    else if (err == boost::asio::error::eof)
    {
        return handler_(content_, boost::system::error_code() );
    }
    else if (err.value() == 335544539)
    {
        slog.WarningLog("short read: code: %d", err.value());
        return handler_( content_, boost::system::error_code() );
    }
    else
    {
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
        return handler_( content_, err);
    }
    
}

/*************************************************************************************************/

tcp::client_ssl::client_ssl( io_service_ptr io_service, ssl_context_ptr ctx )
 : resolver_(  *io_service  ) 
 , socket_(  *io_service ,  *ctx  ) 
 , response_(  )
 , handler_(empty_handler() )
 , content_()
 , filler_()
{
    SCOPE_LOGD(slog);
}

tcp::client_ssl::~client_ssl()
{
    SCOPE_LOGD(slog);
    
}

void tcp::client_ssl::set_address(const std::string& address, unsigned short port)
{
    address_ = address;
    port_    = port;
}

void tcp::client_ssl::set_request_filler(request_filler filler)
{
    filler_ = std::move( filler ) ;
}

void tcp::client_ssl::set_response_handler(  response_handler   h)
{
    if ( static_cast< bool >( h ) ) {
        handler_ = std::move( h ) ;
    } else {
        handler_ = empty_handler();
    }
}
    
void tcp::client_ssl::async_start( )
{
    SCOPE_LOGD(slog);
    
    // Start an asynchronous resolve to translate the server and service names
    // into a list of endpoints.
    ::boost::asio::ip::tcp::resolver::query query(address_,  to_str(port_) );
    
    resolver_.async_resolve(query,
        std::bind(&client_ssl::handle_resolve, get_pointer(),
          std::placeholders::_1,
          std::placeholders::_2));

}
tcp::client_ssl::pointer   tcp::client_ssl::get_pointer()
{
    return this->shared_from_this();
}


void tcp::client_ssl::handle_resolve(const boost::system::error_code& err,
                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
{
    SCOPE_LOG(slog);
    if (!err)
    {
        socket_.set_verify_callback(
                std::bind(&client_ssl::verify_certificate,  std::placeholders::_1, std::placeholders::_2));

        boost::asio::async_connect(socket_.lowest_layer(), endpoint_iterator,
                               std::bind(&client_ssl::handle_connect, get_pointer(),
                                           std::placeholders::_1));

    }
    else
    {
        SCOPE_LOG(slog);

   
        slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
       return handler_("", err); //std::cout << "Error: " << err.message() << "\n";
    }
}

bool tcp::client_ssl::verify_certificate(bool preverified,
                        boost::asio::ssl::verify_context& ctx)
{
     SCOPE_LOG(slog);
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // In this example we will simply print the certificate's subject name.
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    //std::cerr << "Verifying " << subject_name << "\n";
    
    return preverified;
}

void tcp::client_ssl::handle_connect(const boost::system::error_code& err)
{
     SCOPE_LOG(slog);
     if (!err)
    {
        socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                    std::bind(&client_ssl::handle_handshake, get_pointer(),
                                                std::placeholders::_1));

    }
    else
    {
       // SCOPE_LOG(slog);

         slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());

       return handler_("", err); //std::cout << "Error: " << err.message() << "\n";
    }
}

void  tcp::client_ssl::handle_handshake(const boost::system::error_code& ec)
{
    SCOPE_LOG(slog);
    if (!ec)
    {
        req_out_.clear();

        if( filler_ )
            filler_( req_out_ ) ;
        
        if (req_out_.empty() ) // there no request to be send.
        {
            return handler_("", ec);
        }
        
       // The handshake was successful. Send the request.
       boost::asio::async_write(socket_,  boost::asio::buffer( req_out_ ),
                                std::bind(&client_ssl::handle_write_request, get_pointer(),
                                            std::placeholders::_1));
   }
   else
   {
        slog.ErrorLog("error: code: %d ,  msg = %s", ec.value(), ec.message().c_str());
        return handler_("", ec);
   }
}

void tcp::client_ssl::handle_write_request(const boost::system::error_code& err)
{
     SCOPE_LOG(slog);
    if (err){
         slog.ErrorLog("error: code: %d ,  msg = %s", err.value(), err.message().c_str());
    }
    
     req_out_.clear();
     if (filler_)filler_(req_out_);
     
      
    if ( ! req_out_.empty() )
    {
        boost::asio::async_write(socket_,  boost::asio::buffer( req_out_ ),
                           std::bind(&client_ssl::handle_write_request, get_pointer(),
                                       std::placeholders::_1));
    }
    else{
        return handler_("",  err );
    }
}
/********************************************************************************/
