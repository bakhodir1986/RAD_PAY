#ifndef SSL_SERVER_H
#define SSL_SERVER_H

#include <cstddef> 
#include <vector>  
#include <string>
#include <functional>
#include <memory>

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
 
typedef unsigned char byte_t;
typedef ::std::vector< byte_t > byte_vector;

typedef std::function< void (  byte_vector   )> ssl_response_handler;
typedef std::function< void (  byte_vector,  ssl_response_handler ) > ssl_request_handler;


struct ssl_session;
typedef std::shared_ptr< ssl_session> ssl_session_ptr;

struct ssl_server_runtime_options
{
    std::string cert_chain       ;
    std::string private_key_file ;
    std::string dh_file          ;
    std::string password         ;
    std::string ip               ;
    unsigned short  port         ;
};

class ssl_server_T
{
public:
 	ssl_server_T( std::shared_ptr< boost::asio::io_service   > io_service,
	              ssl_server_runtime_options   options   , 
                  ssl_request_handler          req_handler
                );

	~ssl_server_T() ;

	std::string get_password() const;

private:
	void handle_accept( ssl_session_ptr new_session, const boost::system::error_code& error );

private:
	std::shared_ptr< boost::asio::io_service > m_io_service;
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::asio::ssl::context m_context;
	std::string m_password;
    ssl_request_handler m_req_handler;
};




#endif
