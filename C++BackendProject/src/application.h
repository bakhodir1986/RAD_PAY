/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   application.h
 * Author: biocpp
 *
 * Created on January 17, 2018, 1:11 PM
 */

#ifndef OSON_APPLICATION_H
#define OSON_APPLICATION_H

#include <cstddef>

namespace  oson
{
 
class SMS_manager  ;
class PUSH_manager ;
class XMPP_manager ;
class EOPC_manager;
class Merchant_api_manager;
struct runtime_options_t;

class application
{
public:
    application(int argc, char* argv[]);
    ~application();
    
    int run();

public:
    SMS_manager*  sms_manager () const ;
    PUSH_manager* push_manager() const ;
    XMPP_manager* xmpp_manager() const ;
    EOPC_manager* eopc_manager() const ;

    const struct runtime_options_t* runtime_opts()const;
    
    Merchant_api_manager* merchant_api_manager()const;
    
    void start_ums_sverka(const char* date);
    
#if defined(__cplusplus) && (__cplusplus >= 201103L)  // C++11 or later
public:
    application( const application& ) = delete;
    application& operator = (const application& ) = delete;
#else  // c++98
private:
    application(const application&); // = deleted
    application& operator = (const application&); // = deleted
#endif 
    
private:
    struct impl;
    impl* p;
};

application* main_app();

} //end  namespace oson

#define oson_app   ::oson::main_app()
#define oson_eopc  oson_app ->eopc_manager()
#define oson_sms   oson_app ->sms_manager()
#define oson_push  oson_app ->push_manager()
#define oson_xmpp  oson_app ->xmpp_manager()
#define oson_opts  oson_app ->runtime_opts() 

class DB_T ;
namespace oson{   DB_T& this_db(); } //forward declaration, so no more needed include users.h

#define oson_this_db ::oson::this_db()   



#define oson_merchant_api    oson_app -> merchant_api_manager() 

#endif /* OSON_APPLICATION_H */

