#ifndef ADMIN_H
#define ADMIN_H

#include <string>
#include <vector>

#include "types.h"

//forward declarations.
class Sort_T;
class DB_T ;

enum Admin_status_T 
{
    ADMIN_STATUS_UNKNOWN = 0,
    ADMIN_STATUS_ENABLE  = 1,
    ADMIN_STATUS_DISABLE = 2,
    ADMIN_STATUS_DELETED = 3,
    ADMIN_STATUS_MAX
};

enum Admin_flag_T
{
    ADMIN_FLAG_none = 0,
    ADMIN_FLAG_busines   = 1 << 1,
    ADMIN_FLAG_bus_login = 1 << 2,
    ADMIN_FLAG_bus_id    = 1 << 3,
    ADMIN_FLAG_bill_form = 1 << 4,
    ADMIN_FLAG_bus_bank  = 1 << 5,
    ADMIN_FLAG_bus_trans = 1 << 6,
    
    ADMIN_FLAG_MAX       = 1 << 30
};

enum Admin_Module_T
{
    ADMIN_MODULE_NONE          = 0 ,
    ADMIN_MODULE_USERS         = 1 ,
    ADMIN_MODULE_CARDS         = 2 ,
    ADMIN_MODULE_MERCHANTS     = 3 ,
    ADMIN_MODULE_PURCHASES     = 4 ,
    ADMIN_MODULE_TRANSACTIONS  = 5 ,
    ADMIN_MODULE_ADMINS        = 6 ,
    ADMIN_MODULE_NEWS          = 7 ,
    ADMIN_MODULE_BANKS         = 8 ,
    ADMIN_MODULE_CARD_BONUS    = 9 ,
    ADMIN_MODULE_MERCHANT_BONUS = 10,
    ADMIN_MODULE_USER_BONUS_LIST = 11,
    ADMIN_MODULE_PURCHASE_BONUS_LIST = 12, 
    ADMIN_MODULE_SENDMSG         = 13,
    ADMIN_MODULE_ALLOW_PAN       = 14,
//    ADMIN_MODULE_ERROR_CODES     = 14,
//    ADMIN_MODULE_USER_PROFILE    = 15,
//    ADMIN_MODULE_QIWI_BALANCE    = 16,
    ADMIN_MODULE_TOPUP_MERCHANTS = 15,
    ADMIN_MODULE_BUSINESS_BALANCE = 16,
};

struct Admin_info_T 
{
    typedef std::string text;
    typedef uint32_t integer;
    
    integer aid        ;
    text    login      ;
    text    first_name ;
    text    last_name  ;
    text    password   ;
    text    phone      ;
    integer status     ;
    
    //@Note: this flag replaced business, bus_login, bus_id, bill_form, bus_bank, bus_trans.
    integer flag;
    
    
    text token;//Why this is necessary?
    
    
    static integer to_flag(integer business, integer bus_login, integer bus_id, integer bill_form, integer bus_bank, integer bus_trans);
    
    Admin_info_T() ;
};

struct Admin_list_T 
{
    int32_t count;

    std::vector<Admin_info_T> list;

    Admin_list_T();
};

//@Note: A helper structure convert to|from flag
struct Admin_permit_T
{
    uint8_t view;
    uint8_t add;
    uint8_t edit;
    uint8_t del;
    
    Admin_permit_T();
    
    explicit Admin_permit_T(uint32_t flag);

    uint32_t to_flag()const;
    
    void from_flag(uint32_t flag);
    
    enum PermitValues
    {
        VIEW_VALUE = 1 << 1,
        ADD_VALUE  = 1 << 2,
        EDIT_VALUE = 1 << 3,
        DEL_VALUE  = 1 << 4,
    };
};

struct Admin_permissions_T 
{
    uint64_t aid;
    uint32_t module;
    uint32_t merchant;
    uint32_t bank;
    uint32_t flag;
    
    
    Admin_permissions_T();
};

class Admin_T
{
public:
    Admin_T(DB_T & db);
    Error_T login(Admin_info_T &info, bool &logged, uint32_t &aid);
    Error_T logged( const std::string& token, uint32_t &aid);
    Error_T logout( const std::string& token );
    Error_T check_online();

    Error_T list(const Admin_info_T & search, const Sort_T& sort, Admin_list_T &alist);
    Error_T info(const Admin_info_T &search, Admin_info_T &data);
    Error_T add(Admin_info_T &data);
    Error_T edit(uint32_t id, const Admin_info_T &data);
    Error_T del(uint32_t id);

    Error_T generate_bill_qr_img(uint64_t bill_id, std::string& img_data);
    Error_T change_password(uint32_t aid, const std::string & old_password, const std::string& new_password);
    
    void permissions_add(const std::vector< Admin_permissions_T >& permission_list);
    Error_T permissions_add(const Admin_permissions_T &permission);
    Error_T permissions_del(uint64_t aid);
    Error_T permissions_list(uint64_t aid, std::vector<Admin_permissions_T> &permissions);
    
    Error_T permission_module(uint64_t aid, uint32_t module,  /*OUT*/ Admin_permissions_T& per);
    Error_T permission_merch(uint64_t aid, uint32_t merch_id, /*OUT*/Admin_permissions_T& per);
    Error_T permission_bank( uint64_t aid, uint32_t bank_id,  /*OUT*/Admin_permissions_T& per);
    
    Error_T permissions_merchant_ids(uint64_t aid, std::string& ids);
    Error_T not_permissions_merchant_ids(uint64_t aid, std::string& ids);
    
    //return admin id. or 0 if not found.
    int32_t search_by_login(std::string const& login  ) ;
private:
    DB_T & m_db;
};

#endif
