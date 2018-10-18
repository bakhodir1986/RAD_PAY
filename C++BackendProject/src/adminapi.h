#ifndef ADMINAPI_H
#define ADMINAPI_H

#include <vector>
#include <memory>
#include <functional>

#include <boost/asio/io_service.hpp>
#include "types.h"


typedef ::std::vector< uint8_t > byte_array;
typedef ::std::function< void(   byte_array  ) >  response_handler_type;

class AdminApi_T: public std::enable_shared_from_this< AdminApi_T > 
{
public:
	 explicit AdminApi_T( std::shared_ptr< boost::asio::io_service > io_service );
	~AdminApi_T();

    
    void exec( const byte_array& data,   response_handler_type   response_handler );
    
private:
    AdminApi_T(const AdminApi_T&); // = delete
    AdminApi_T& operator = (const AdminApi_T&); // = delete
    
private:
    void* d_ptr;
};

#endif
