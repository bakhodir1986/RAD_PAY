#ifndef SMS_SENDER_H
#define SMS_SENDER_H

#include <string>
#include <memory>
#include <functional>

#include <boost/asio/io_service.hpp>

#include "types.h" // Error_T

struct SMS_info_T 
{
    enum Type_E
    { 
            type_none                    = 0, 
            type_bulk_sms                = 1, 
            type_fault_sms               = 2, 
            type_bonus_admin_sms         = 3, 
            type_card_owner_changed_sms  = 4, 
            type_card_limit_exceeded_sms = 5,
            type_purchase_fail_sms       = 6,
            type_EOPC_222_error_sms      = 7,
            type_auth_error_sms          = 8,
            type_oson_restarted_sms      = 9,
            type_mplat_91_status_sms     = 10,
            type_mplat_status_fail_sms   = 11,
            
            type_business_reverse_sms    = 12,
            
            type_client_register_code_sms = 14,
            type_public_purchase_code_sms = 15,
            type_client_card_add_code_sms = 16,
            
    };
    
	std::string phone;
	std::string text;
    int  type;
    int64_t id;//used only inner purpose.
    
    inline SMS_info_T()
    : type(0), id(0)
    {}
    
    inline ~SMS_info_T(){}
    
    inline SMS_info_T(const std::string& phone, const std::string& msg, int type = 0)
            : phone(phone)
            , text(msg)
            , type(type)
            , id(0)
    {}
    
    
};

namespace oson
{

typedef std::shared_ptr < boost::asio::io_service > io_service_ptr;

struct sms_runtime_options_t
{
    std::string url, url_v2, auth_basic_v2;
};

class SMS_manager
{
public:
    
    explicit SMS_manager( const io_service_ptr & io_service);
    ~SMS_manager();
    
    void async_send(  SMS_info_T  sms);
    void async_send_v2( SMS_info_T sms);
    
    const io_service_ptr&  io_service()const;
    
    
    void get_status(std::string message_id, std::function<void(std::string const&)> handler);
    
private:
    SMS_manager(const SMS_manager&);
    SMS_manager& operator = (const SMS_manager&);
    
    friend class application;
    
    void set_runtime_options( const sms_runtime_options_t options ) ;
private:
    io_service_ptr  ios_;
    struct sms_runtime_options_t options_;
};

} // end oson


#endif
