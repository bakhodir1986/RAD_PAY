#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace oson
{
    
namespace network
{
      typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;
    
      typedef std::shared_ptr< boost::asio::ssl::context  > ssl_context_ptr; 
  
namespace http
{

typedef  ::std::function< void(const std::string& response, const ::boost::system::error_code& error) >  response_handler_t;
typedef  ::boost::asio::ssl::stream< ::boost::asio::ip::tcp::socket > ssl_socket_t;

// HTTP  GET function.
struct http_content
{
    std::string value   ; // content value.  default is empty, no content provided.
    std::string type    ; // 'text', 'xml', 'json', etc..  Default is 'text'
    std::string charset ; // 'utf-8', 'windows-1251', 'utf-16', etc.  Default is 'utf-8'.
};
struct request
{
    std::string host;      // IP  or DNS without protocol http, https, ftp, smpt, and etc.
    std::string port;      // concrete ports, '80', '8080', '443', or 'http', https, ..
    std::string path;      // '/oson.php' 
    std::string method;    // GET, POST, DELETE, PUT.  Default is 'GET'
    std::vector<std::string> headers;   // 'Authorization: Basic authHash' 
    http_content content;  // 
    bool no_connection_close;
    inline request(): no_connection_close(false){}
};

request parse_url( const std::string& url );// parse host, port, path

    
 class client: public std::enable_shared_from_this< client > 
 {
 public:
     typedef client self_type;
     typedef std::shared_ptr< client > pointer;
     
     pointer get_pointer ();
     
     static pointer create(io_service_ptr io_service, ssl_context_ptr ctx );
     
 public:
     client (io_service_ptr io_service, ssl_context_ptr ctx ) ;
     ~client();
     
     
     void async_start();
     
     void set_request(const request& req);
     void set_response_handler(const response_handler_t& h );
     
     void set_timeout(int timeout_milliseconds);
     //void set_timeout(int timeout_milliseconds);
     
     void set_verify_mode(boost::asio::ssl::context::verify_mode verify_mode);
     
     std::string body()const;
     //boost::system::error_code error_code()const;
     
 private:
     void handle_resolve(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
     
     
     static bool verify_certificate(bool preverified,
                        boost::asio::ssl::verify_context& ctx);

      void handle_connect(const boost::system::error_code& err);
      
      void  handle_handshake(const boost::system::error_code& error);
      void handle_write_request(const boost::system::error_code& err);
      
      void handle_read_status_line(const boost::system::error_code& err);
      
      void handle_read_headers(const boost::system::error_code& err);
      
      void handle_read_content(const boost::system::error_code& err);
      
      
      void on_shutdown( boost::system::error_code );
     // void on_shutdown_write(const boost::system::error_code&);
      
      void call_handler(const std::string& s, const boost::system::error_code& ec);
      
      void start_timeout();
      void on_timeout( const boost::system::error_code&  ec );
      void cancel_timeout();
      
      void do_async_ssl_shutdown();
 private:
     boost::asio::ip::tcp::resolver resolver_ ;
     ssl_socket_t            socket_          ;
     boost::asio::deadline_timer timer_          ;
     boost::asio::streambuf request_          ;
     boost::asio::streambuf response_         ;
     response_handler_t     handler_          ;
     std::string            content_          ;
     //boost::system::error_code code_          ;
     bool chunked_;
     
     int timeout_milliseconds_ ;
     std::string host, port;
     boost::asio::ssl::context_base::verify_mode verify_mode_;
     
 };
 
 class client_http : public std::enable_shared_from_this< client_http > 
 {
 public:
     typedef client_http self_type;
     typedef std::shared_ptr< client_http > pointer;
     
     pointer get_pointer();
     
 public:
     
     explicit client_http( io_service_ptr  io_service );
     
     ~client_http();
     
     void set_response_handler( const response_handler_t& h);
     
     void set_request(const request& req);
     
     void async_start(  );
     
     
      
     std::string body()const;
     boost::system::error_code error_code()const;
 private:
     void handle_resolve(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
      void handle_connect(const boost::system::error_code& err);
      
      void handle_write_request(const boost::system::error_code& err);
      
      void handle_read_status_line(const boost::system::error_code& err);
      
      void handle_read_headers(const boost::system::error_code& err);
      
      void handle_read_content(const boost::system::error_code& err);
 private:
     boost::asio::ip::tcp::resolver resolver_ ;
     boost::asio::ip::tcp::socket   socket_   ;
     boost::asio::streambuf  request_ ;
     boost::asio::streambuf  response_;
     
     std::string content_ ;
     boost::system::error_code code_;
     
     bool chunked_;
     
     response_handler_t handler_;
     
     std::string host, port;
 };
 
} // end http
    
} // end network
    
} // end oson

namespace oson{ namespace network{ namespace tcp{
    
typedef std::function< void (const std::string&, const boost::system::error_code&)> response_handler;
    
class client: public std::enable_shared_from_this<  client > 
{
public:
    typedef client self_type;
    typedef std::shared_ptr< client > pointer;
    
    pointer get_pointer();
public:
    explicit client( io_service_ptr io_service);
            
     
    void set_address(const std::string& address, unsigned short port);
    void set_request(const std::string& req);
    
    void async_start();
    
     ~client();
     
     void set_response_handler(const response_handler& h);
     
//     std::string body()const;
//     boost::system::error_code error_code()const;
 private:
     void handle_resolve(const boost::system::error_code& err,
                        boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
      void handle_connect(const boost::system::error_code& err);
      
      void handle_write_request(const boost::system::error_code& err);
      void handle_read_content(const boost::system::error_code& err);
 private:
     boost::asio::ip::tcp::resolver resolver_ ;
     boost::asio::ip::tcp::socket   socket_   ;
     boost::asio::streambuf  request_ ;
     boost::asio::streambuf  response_;
     
     std::string content_ ;
     boost::system::error_code code_;
     response_handler handler_; 

     std::string address_;
     unsigned short port_;
     
     std::string req_content_;
};

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

typedef std::function< void(std::vector< unsigned char>& out ) > request_filler;

class client_ssl: public std::enable_shared_from_this< client_ssl >
{
public:
    typedef client_ssl self_type;
    typedef std::shared_ptr< client_ssl > pointer;
    
    pointer get_pointer();
    
public:
    
    client_ssl( io_service_ptr io_service, ssl_context_ptr ctx );
    
    
    void async_start( );
    
    ~client_ssl();
    
    void set_address(const std::string& address, unsigned short port);
    void set_request_filler( request_filler filler ) ;
    void set_response_handler( response_handler   h);
    
private:
    void handle_resolve(const boost::system::error_code& err,
                   boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
    
    static bool verify_certificate(bool preverified,
                        boost::asio::ssl::verify_context& ctx);
    
    void handle_connect(const boost::system::error_code& err);
     
    void  handle_handshake(const boost::system::error_code& error);
     
    void handle_write_request(const boost::system::error_code& err);
     
    
private:
	 boost::asio::ip::tcp::resolver resolver_ ;
     ssl_socket             socket_           ;
     boost::asio::streambuf response_         ;
     response_handler       handler_          ;
     std::string            content_          ;
     request_filler         filler_;
     std::string            address_;
     unsigned short         port_;
     std::vector< unsigned char > req_out_;
};
    
}}} // end oson::network::tcp

#endif // HTTP_REQUEST_H
