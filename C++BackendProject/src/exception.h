/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   exception.h
 * Author: biocpp
 *
 * Created on December 21, 2017, 9:17 PM
 */

#ifndef OSON_EXCEPTION_H
#define OSON_EXCEPTION_H

#include <exception>

namespace oson
{

class exception: public std::exception
{
public:
    
    virtual ~exception()throw(){}
    
    explicit exception(const std::string& msg): m_error_code(Error_internal), m_error_msg(msg) 
    {}
    explicit exception(const std::string& msg, int error_code): m_error_code(error_code), m_error_msg(msg)
    {}
    
    virtual const char* what()const throw() { return m_error_msg.c_str(); }
    
    int error_code()const{ return m_error_code; }
private:
    int m_error_code;
    std::string m_error_msg;
};
    
} // oson

#define OSON_DECLARE_EXCEPTION(name)   class name : public oson::exception{ public: virtual ~name() throw(){}; name ( const std::string& msg, int ec): oson::exception(msg, ec){} };

OSON_DECLARE_EXCEPTION( db_exception ) 


#endif /* OSON_EXCEPTION_H */

