#ifndef OSON_TYPES_H
#define OSON_TYPES_H

#include <cstdint> // write confirm C++11 mode.

enum Error_T
{
    Error_OK               = 0,
    Error_not_modified     = 1,
    
    Error_operation_not_allowed = 3,
    
    Error_token_is_too_long = 4,
    
	Error_login_failed     = 5,
	Error_login_empty      = 6,
	Error_checkcode_failed = 7,
    
    Error_token_is_empty   = 8,
    
    Error_device_token_not_found = 9,
    
	Error_not_found        = 10,
    Error_src_undefined    = 11,
    Error_dst_undefiend    = 12,

    Error_parameters       = 14,

    Error_limit_exceeded   = 15,
    Error_card_blocked     = 17,
    
    Error_min_limit        = 18,
    Error_max_limit        = 19,

  
    Error_transaction_not_allowed = 20,

    Error_purchase_login_not_found = 21,
    
    Error_amount_is_too_small  = 22,
    Error_amount_is_too_high   = 23,
    
    Error_bonus_card           = 24,
    
    Error_not_enough_amount  = 25,

    Error_blocked_user       = 26,
    
    Error_perform_in_progress = 27,
    
    Error_timeout            = 28,
    
    Error_card_daily_limit_exceeded = 29,
    
    Error_bank_not_supported = 30,
    
    Error_user_not_found     = 31,
    
    Error_eopc_timout        = 32,
    Error_eopc_connection    = 33,
    Error_eopc_error         = 34,
    
    Error_card_foreign       = 35,
    
    Error_card_not_found     = 36,
    
    Error_a_new_card_already_exists = 37,
    
    Error_sms_code_not_verified = 38,
    Error_merchant_operation = 39,
    
    Error_card_owner_changed = 40,
    
    Error_card_phone_not_found = 41,
    
    Error_wallet_operation  = 42,
    
    Error_not_valid_phone_number = 44,
 
    Error_admin_already_exists = 45,
    
    Error_very_often_access = 46,
    
    Error_access_denied      = 48,
    
	Error_communication   = 70, // All more then that as API error
	Error_SRV_unknown_cmd = 71,
	Error_SRV_data_length = 72,
	Error_SRV_version     = 73,

    Error_provider_temporarily_unavailable = 80,
    
    Error_week_password = 87,
    Error_limit_exceeded_password_check = 88,
    
            
            
	Error_internal      = 99, // All errors more then Error_internal returned to user as Error_internal
	Error_DB_connection = 100,
	Error_DB_reconnect,
	Error_DB_exec,
	Error_DB_empty,

    Error_EOPC_connect  = 110,
    Error_EOPC_not_valid_phone = 113,
    
    Error_HTTP_host_not_found = 122,
    Error_http_connection = 125,
    //////////////////////////////////
    Error_async_processing = 99999,
};

namespace oson
{
    const char * error_str(Error_T e ); 
    
    
    template< typename T > inline void ignore_unused(const T&){}
}


//-------------------------------------------------------------------
//////////////////////////////////////////////////////////////////////////

#endif // end OSON_TYPES
