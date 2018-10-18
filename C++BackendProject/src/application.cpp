/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include "application.h"

#include <cstdio> // fprintf, 
#include <cstdlib> // exit
#include <cassert>

static oson::application * g_app = NULL;

oson::application* oson::main_app(){ return g_app; }

static void set_app(oson::application* app)
{
    assert( NULL == g_app  ) ;
    g_app = app;
}

/*****************************************************************************************************/
struct oson::application::impl
{
    int argc;
    char** argv ;

    class SMS_manager  * sms_manager  ;
    class PUSH_manager * push_manager ;
    class XMPP_manager * xmpp_manager ;
    class EOPC_manager * eopc_manager ;
    class Merchant_api_manager* merchant_api ;
    struct runtime_options_t * runtime_options;
}; 

oson::application::application(int argc, char* argv[])
: p ( new impl )
{
    p->argc = argc;
    p->argv = argv;
    p->sms_manager  = NULL ;
    p->push_manager = NULL ;
    p->xmpp_manager = NULL ;
    p->eopc_manager = NULL ;
    p->runtime_options = NULL ;
    
    ::set_app(this);
}

oson::application::~application()
{
    delete p;
    fprintf(stderr, "~application()\n");
}



oson::SMS_manager*  oson::application::sms_manager () const
{
    return p->sms_manager;
}

oson::PUSH_manager*  oson::application::push_manager() const
{
    return p->push_manager;
}

oson::XMPP_manager* oson::application::xmpp_manager()const
{
    return p->xmpp_manager;
}

oson::EOPC_manager* oson::application::eopc_manager()const
{
    return p->eopc_manager;
}

oson::Merchant_api_manager * oson::application::merchant_api_manager() const
{
    return p->merchant_api ;
}

const struct oson::runtime_options_t*  oson::application::runtime_opts() const{
    return  p->runtime_options ;
}
/*****************************************************************************************************/

