#ifndef OSOND_H
#define OSOND_H

#include <boost/asio/io_service.hpp>



struct scoped_register
{
    inline
    explicit scoped_register(boost::asio::io_service& ios): m_ptr(&ios)
    {
        unregister_io_service(m_ptr);
        register_io_service(m_ptr);
    }
    inline
    ~scoped_register(){ 
        unregister_io_service(m_ptr);
    }
private:
    static void register_io_service(boost::asio::io_service* ios);
    static void unregister_io_service(boost::asio::io_service* ios);
private:
    boost::asio::io_service* m_ptr;
};

inline
scoped_register make_scoped_register(boost::asio::io_service& ios){ return scoped_register(ios);}






#endif
