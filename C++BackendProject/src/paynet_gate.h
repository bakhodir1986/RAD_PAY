#ifndef PAYNET_GATE_H
#define PAYNET_GATE_H

#include <string>
#include <map>
#include <memory>
#include <cstdint>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl/context.hpp>

namespace oson{ namespace backend{ namespace merchant{ namespace paynet{
    
enum ACT_ENUM
{
    ACT_REQUEST_MERCHANT_LIST        = 1 ,
    ACT_REQUEST_PERFORM              = 2 ,
    ACT_REQUEST_LAST_PERFORM         = 3 ,
    ACT_REQUEST_DAILY_REPORT         = 4 ,
    ACT_REQUEST_CHECK_STATUS_PERFORM = 6
};
    
struct request_t
{
    int         act              ;
    int64_t     service_id       ;
    int64_t     transaction_id   ;
    std::string fraud_id         ; // phone of caller
    int64_t     amount           ; // in sum.
    std::string client_login     ; // login
    
    //std::string amount_param_name;
    std::string client_login_param_name;
    
    std::map< std::string, std::string> param_ext;
};
    
struct response_t
{
    int status;
    std::string status_text;
    std::string transaction_id;// transaction_id of paynet
    
    std::string ts;
    std::map<std::string, std::string>kv;
};

struct access_t
{
    std::string terminal_id  ;//'4119362';
    std::string merchant_id  ;//????????
    std::string password     ;//'NvIR4766a';
    std::string username     ;//'akb442447';
    std::string url          ;//'https://213.230.106.115:8443/PaymentServer';
};

struct info_t
{
    std::string transaction_id;
    std::string ts;
    std::map<std::string, std::string>kv;
    int status;
    std::string status_text;
    //std::string raw;
};
struct param_t
{
    std::map< std::string, std::string> pm;
};

typedef std::function< void (const request_t& , const response_t& ) > handler_type;

typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;
typedef std::shared_ptr< boost::asio::ssl::context > context_ptr;


class manager
{
public:
    explicit manager(const access_t&);
    ~manager();
    
    int check( const request_t& req,  response_t& resp);
    
    int get_param(const request_t& req, param_t& param);
    
    int get_info(const request_t& req, info_t& info);
    //return 0 if success, non zero if failed. see resp.status for more information.
    int perform(  const request_t& req,  response_t& resp);

    static const char*  invalid_transaction_value();

    void async_get_info(const request_t& req, handler_type handler);
    void async_perform(const request_t& req, handler_type);
    
private:
    manager(const manager&); // = deleted
    manager& operator = (const manager&); // =deleted
private:
    access_t acc_ ;
};



}}}} // end oson::backend::merchant::paynet

#endif // PAYNET_GATE_H
