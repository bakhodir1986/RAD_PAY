/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */


#include <functional>
#include <memory>

#include <boost/asio/ssl.hpp>
#include <boost/type_traits/aligned_storage.hpp>

#include "log.h"

#include "types.h"
#include "ssl_server.hpp"
#include "config_types.h"

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

// 1. create ssl_session
// 2. start
// 3. handshake
// 4. read header
// 5. read boyd
// 6. handle packet
// 7. write
// 8. goto step 4. read header.

namespace
{

class handler_memory
{
#if __cplusplus < 201103L
private:
    handler_memory(const handler_memory&  ) ;
    handler_memory& operator = (const handler_memory&) ;
    
#else 
public:
    handler_memory(const handler_memory&  ) = delete;
    handler_memory& operator = (const handler_memory&) = delete;
#endif 
    
public:
    handler_memory(): in_use_(false){}
    
    void* allocate( std::size_t size)
    {
        if ( ! in_use_ && size < storage_.size ) {
            in_use_ = true;
            return storage_.address();
        } else {
            return ::operator new(size);
        }
    }
    
    void deallocate(void * pointer)
    {
        if (pointer == storage_.address() ) {
            in_use_ = false;
        } else {
            ::operator delete( pointer );
        }
    }
    
private:
    boost::aligned_storage<1024> storage_;
    bool in_use_;
};

template< typename T >
class handler_allocator
{
public:
    typedef T value_type;
    explicit handler_allocator(handler_memory& mem): mem_( boost::addressof(mem) ){}
    
    template< typename U >
    handler_allocator(const handler_allocator<U>& other): mem_( other.mem_ ){}
    
    template< typename U >
    struct rebind{
        typedef handler_allocator<U> other;
    };
    
    bool operator == (const handler_allocator& other)const{ return mem_  == other.mem_ ; }
    bool operator != (const handler_allocator& other)const{ return mem_  != other.mem_ ; }
    
    T* allocate(std::size_t n)const
    {
        return static_cast<T*>(mem_->allocate( sizeof( T ) * n ) ) ;
    }
    void deallocate(T* p, std::size_t /*n */ )
    {
        return mem_->deallocate(p);
    }
//private:
    handler_memory* mem_;
};


template< typename Handler >
class custom_alloc_handler
{
public:
    typedef handler_allocator<Handler> allocator_type;
    
    custom_alloc_handler(handler_memory& m, Handler h)
    : memory_(m),
      handler_(h)
  {
  }

  allocator_type get_allocator() const
  {
    return allocator_type(memory_);
  }

#if __cplusplus < 201103L 
  template <typename Arg1>
  void operator()(Arg1 arg1)
  {
    handler_(arg1);
  }

  template <typename Arg1, typename Arg2>
  void operator()(Arg1 arg1, Arg2 arg2)
  {
    handler_(arg1, arg2);
  }
#else 
  template< typename ... Args  >
  void operator()(Args ... args ){
      handler_( args ... );
  }
#endif 

private:
  handler_memory& memory_;
  Handler handler_;
};



// Helper function to wrap a handler object to add custom allocation.
template <typename Handler>
inline custom_alloc_handler<Handler> make_custom_alloc_handler(
    handler_memory& m, Handler h)
{
  return custom_alloc_handler<Handler>(m, h);
}
 

} // end noname namespace


struct ssl_session: public std::enable_shared_from_this< ssl_session > 
{
public:
	ssl_session( const std::shared_ptr< boost::asio::io_service >& io_service,
	             boost::asio::ssl::context& context,
	             ssl_request_handler   req_handler 
               );
  	~ssl_session();

	ssl_socket::lowest_layer_type& socket();
    
    
	void start();
private:
	
    void handle_start(const std::vector<unsigned char>& d);
    
    
    void handle_handshake  ( const boost::system::error_code& ec );
    void handle_read_header( const boost::system::error_code& ec, size_t bytes_transferred);
	void handle_read_body  ( const boost::system::error_code& ec, size_t bytes_transferred ) ;
    void handle_packet();
    void write_async( byte_vector d);
	void write_async_i();
    void handle_write( const boost::system::error_code& ec ) ;
    void start_read_body();
    void start_read_header();
    
    
    void finish();
    
private:
	ssl_socket  socket_   ;
    std::vector<unsigned char> data_     ;
    size_t      data_pos_ ;
    ssl_request_handler m_req_handler;
    size_t      id_       ;
    
    handler_memory handler_memory_ ;
};



/////////////////////////////////////////////////////////////////////////////////////////////////    
ssl_server_T::ssl_server_T( std::shared_ptr< boost::asio::io_service> io_service,
                            ssl_server_runtime_options options , 
                            ssl_request_handler req_handler
                        ):
    m_io_service( io_service ),
    m_acceptor( *io_service,
                boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v6() , options.port ) ),
    m_context( /* *io_service, */ boost::asio::ssl::context::sslv23),
    m_req_handler( req_handler )
    
{
    SCOPE_LOG( slog );

    m_password = options.password;
    
    m_context.set_options(
          boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);

    m_context.set_password_callback( std::bind( &ssl_server_T::get_password, this ) );
    m_context.use_certificate_chain_file( options.cert_chain );
    m_context.use_private_key_file( options.private_key_file, boost::asio::ssl::context::pem);
    m_context.use_tmp_dh_file( options.dh_file );

    
    ssl_session_ptr new_session  =  std::make_shared< ssl_session > ( m_io_service, std::ref( m_context ) , m_req_handler ) ;
    
    m_acceptor.async_accept( new_session->socket(),
                             std::bind( &ssl_server_T::handle_accept,
                                          this,
                                          new_session,
                                          std::placeholders::_1 ) );
}

ssl_server_T::~ssl_server_T() 
{}

std::string ssl_server_T::get_password() const 
{ 
    return m_password; 
}

void ssl_server_T::handle_accept( ssl_session_ptr new_session, const boost::system::error_code& error )
{
    if (error)
    {
        return;
    }
    
    new_session->start();
    
    new_session  =   std::make_shared< ssl_session >( m_io_service, std::ref( m_context ), m_req_handler ) ;
    
    m_acceptor.async_accept( new_session->socket(),
            std::bind( &ssl_server_T::handle_accept, 
            this, 
            new_session,
            std::placeholders::_1 ) );

}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////



ssl_session::ssl_session( const std::shared_ptr< boost::asio::io_service> & io_service,
                        boost::asio::ssl::context& context,
                        ssl_request_handler   req_handler 
                        )
: socket_( *io_service, context )
, data_()
, data_pos_(0) 
, m_req_handler(req_handler)
, id_ (0)
{
    SCOPE_LOGD(slog);
}


ssl_session::~ssl_session() 
{
    //need flush after scoped  end
    {
        SCOPE_LOGD(slog);
        finish();
    }
    
    CLog::flush();
}

void ssl_session::finish()
{
    Server_head_T h = {};
    h.version = 1;
    h.cmd_inner_id = Server_head_T::CMD_INNER_DELETE;
    h.inner_id_val = this->id_; 
    data_.resize(Server_head_T::length);
    
    encode_header(h, data_.data(), data_.size() );
    
    m_req_handler(data_, ssl_response_handler() ) ;
}

ssl_socket::lowest_layer_type& ssl_session::socket() 
{
    return socket_.lowest_layer();
}

void ssl_session::start() 
{
    Server_head_T h = {};
    h.version = 1;
    h.cmd_inner_id = Server_head_T::CMD_INNER_CREATE;
    data_.resize(Server_head_T::length);
    
    encode_header(h, data_.data(), data_.size() );
    
    m_req_handler(data_, std::bind(&ssl_session::handle_start, this->shared_from_this(),  std::placeholders::_1)) ;
    
    socket_.async_handshake( boost::asio::ssl::stream_base::server,
        std::bind(&ssl_session::handle_handshake, this->shared_from_this(),
        std::placeholders::_1) );
}

void ssl_session::handle_start(const std::vector<unsigned char>& d)
{
    Server_head_T h  = parse_header(d.data(), d.size());
    this->id_ = h.inner_id_val;
}
    

void ssl_session::handle_handshake( const boost::system::error_code& error ) 
{
    if (error)
    {
        return ;
    }

    return start_read_header();
}

void ssl_session::handle_read_header(const boost::system::error_code& ec, size_t bytes_transferred)
{
    if (ec)
    {
        if (ec != boost::asio::error::eof)
        {
            SCOPE_LOG(slog);
             slog.ErrorLog( "Read error: %s", ec.message().c_str() );
        }
        return ;
    }


    if (bytes_transferred >= Server_head_T::length)
    {
        Server_head_T head = parse_header(data_.data(), bytes_transferred);
        size_t data_size          = head.data_size + Server_head_T::length;
        
        //add inner values.
        head.cmd_inner_id = head.CMD_INNER_NONE;
        head.inner_id_val = this->id_;
        
        encode_header( head, data_.data(), Server_head_T::length );
        
        
        if(data_size > bytes_transferred) 
        {
            data_.resize(data_size);
            data_pos_ = Server_head_T::length;

            return start_read_body();
        }
        
    }
    else // broken header.
    {
        Server_head_T head = {};
        data_.resize(Server_head_T::length);
        encode_header(head, data_.data(), data_.size());
    }

    //packet is ready
     handle_packet();
}

void ssl_session::handle_read_body( const boost::system::error_code& ec, size_t bytes_transferred ) 
{
    if (ec) 
    {
        if (ec != boost::asio::error::eof)
        {
             SCOPE_LOG(slog);
             slog.ErrorLog( "Read error: %s", ec.message().c_str() );
        }
        return ;

    }

    if (bytes_transferred + data_pos_ < data_.size())
    {
        data_pos_ += bytes_transferred;
        return start_read_body();
    }

    // packet is ready, call it.
    handle_packet();
}

void ssl_session::handle_packet()
{
    //SCOPE_LOG(slog);
   // slog.DebugLog("data_.ptr: %p", data_.data());
    
    m_req_handler( std::move( data_ ) , std::bind(&ssl_session::write_async, this->shared_from_this(), std::placeholders::_1 ) ) ;
}


void ssl_session::write_async( byte_vector d)
{
    //SCOPE_LOG(slog);
    //slog.DebugLog("d.ptr: %p", d.data());
    
    data_.swap( d );
    // this code may work absolute another thread than ssl_session created thread!
    // so move it to ssl_session's thread.
    socket_.get_io_service().post(std::bind( &ssl_session::write_async_i, this->shared_from_this() ) ) ;
}

void ssl_session::write_async_i()
{
    using namespace std::placeholders;
    
    boost::asio::async_write(socket_,
                    boost::asio::buffer( data_ ),
                    make_custom_alloc_handler(  handler_memory_,
                            std::bind( &ssl_session::handle_write, this->shared_from_this(), _1   ) 
                        )
                    );
}

void ssl_session::handle_write( const boost::system::error_code& ec ) 
{
    if (   ec )
    {
        SCOPE_LOG( slog );
        slog.ErrorLog( "Write error: %s", ec.message().c_str() );
        return ;
    }

    start_read_header();
}

 //handle_read.
void ssl_session::start_read_body()
{
    using namespace std::placeholders;
    
    socket_.async_read_some( 
        ::boost::asio::buffer(data_.data() + data_pos_, data_.size() - data_pos_),
        make_custom_alloc_handler( handler_memory_,
                    ::std::bind(&ssl_session::handle_read_body, this->shared_from_this(), _1, _2)
            )
    );
}


/// there handle_read_header .
void ssl_session::start_read_header()
{
    data_.resize(Server_head_T::length);
    data_pos_ = 0;
    
    using namespace std::placeholders;
    
    socket_.async_read_some( 
            ::boost::asio::buffer(data_.data(), Server_head_T::length),
          make_custom_alloc_handler( handler_memory_,
                ::std::bind(&ssl_session::handle_read_header, this->shared_from_this(),   _1, _2 )
            )
    );
}

