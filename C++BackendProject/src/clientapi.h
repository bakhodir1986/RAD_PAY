#ifndef CLIENTAPI_H
#define CLIENTAPI_H
 
#include <vector>
#include <memory>
#include <functional>

#include <boost/asio/io_service.hpp>
#include "types.h"

typedef  std::vector< uint8_t > byte_array;
typedef  std::function< void(  byte_array ) > rsp_handler_type;

class ClientApi_T: public std::enable_shared_from_this< ClientApi_T >
{
public: 
	explicit ClientApi_T( std::shared_ptr< boost::asio::io_service > io_service );
	~ClientApi_T();

    // data - will not copied, so const ref,  response_handler - will copied into d_ptr.
	void exec( const  byte_array&   data,   rsp_handler_type   response_handler );
 
    //void on_timeout();
private:
   ClientApi_T(const ClientApi_T&); // = delete
   ClientApi_T& operator = (const ClientApi_T&); // = delete
   
private:
    void* d_ptr;
public:
    int64_t uid()const ;
};

#endif
