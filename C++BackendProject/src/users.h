#ifndef OSON_USERS_H_INCLUDED
#define OSON_USERS_H_INCLUDED 1

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "types.h"

#include "msg_android.h"

#include "DB_T.h"

/***************************************************/
namespace oson
{
    
struct App_info_T
{
    typedef int32_t integer;
    typedef std::string text;
    
    integer id;
    text    version;
    text    os;
    text    release_date;
    text    expiry_date;
    text    min_version;
    
    inline App_info_T(): id(0){}
};

typedef std::vector< App_info_T > App_info_list_T;

class App_info_table_T
{
public:
    explicit App_info_table_T(DB_T& db);
    ~App_info_table_T();
    
    
    App_info_list_T list( const App_info_T& search, const Sort_T& sort ) ;
    
    //add and return id
    int32_t add(const App_info_T& info);

    //edit and return number of edited rows: 0 - nothing edited, 1 - single element edited.
    int32_t edit(int32_t id, const App_info_T& info);

    //del and return number of deleted rows: 0 - nothing deleted, 1 - single element deleted.
    int32_t del(int32_t id);
    
    //return last released oson apk,  os - android  or ios.
    App_info_T get_last(const std::string& os);
    
private:
    DB_T& m_db;
}; // end App_info_table_T

} // end namespace oson

namespace oson
{
 
struct Users_full_info
{
    typedef int32_t     integer ;
    typedef int64_t     bigint  ;
    typedef std::string text    ;
    
    bigint id  ;
    bigint uid ;
    text   fio ;
    
    text   passport_number     ;
    text   passport_serial      ;
    text   passport_start_date ;
    text   passport_end_date   ;
    text   passport_image_path ;
    
    text  date_of_birth        ;
    text  nationality          ;
    text  citizenship          ;
    
    integer status             ;
    integer level              ;
    enum Status_E
    {
        Status_None       = 0,
        Status_Enable     = 1,
        Status_Registered = 2,
        Status_Blocked    = 3,
    };
    
    inline Users_full_info(): id( 0 ), uid( 0 ), status( 0 ), level ( 0 ) {}
};

struct Users_full_info_list
{
    int32_t total_count ; 
    std::vector< Users_full_info > list;
};
    
class Users_full_info_table
{
public:
    explicit Users_full_info_table(DB_T& db);
    ~Users_full_info_table() ;
    
    Users_full_info_list  list(const Users_full_info& search, const Sort_T& sort) ;
    Users_full_info  get_by_uid( int64_t uid) ;
    int64_t add(const Users_full_info & info);
    int32_t edit(int64_t id, const Users_full_info & info ) ;
    int32_t del(int64_t id);
    
private:
    DB_T& m_db;
};
    
} // end namespace oson

/**********************************************/
struct Activate_info_T
{
    typedef int64_t bigint;
    typedef int32_t integer;
    typedef std::string text;
    
    
    bigint  id       ;
    text    phone    ;
    text    code     ;
    text    dev_id   ;
    integer kind     ;
    bigint  other_id ;
    text    add_ts   ;
    integer lives    ;
    
    enum KindTypes
    {  
        Kind_none = 0, 
        Kind_user_register = 1, 
        Kind_foreign_card_register = 2, 
        Kind_public_purchase_buy = 3, 
        Kind_cabinet_auth = 4 
    };
    
    inline Activate_info_T(): id(0), phone(), code(),  dev_id(),  kind(0), other_id(0){}
};

class Activate_table_T
{
public:
    explicit Activate_table_T( DB_T& db ) ;
    ~Activate_table_T();
    
    int64_t add(const Activate_info_T& info);
    
    Activate_info_T info(const Activate_info_T& search);
    
    int count(const Activate_info_T& search);
    
    int deactivate(int64_t id);
    
    int kill_lives(const Activate_info_T& search ) ;
private:
    typedef std::reference_wrapper<DB_T> db_ref;
    db_ref m_db;
};

 
bool valid_phone( const std::string& phone);

struct Error_info_T
{
    typedef std::string text;
    typedef int32_t integer;
    
    integer id;
    integer value;
    integer ex_id;
    text message_eng;
    text message_rus;
    text message_uzb;
    Error_info_T(): id(0), value(0), ex_id(0){}
};
struct Error_info_list_T
{
    int32_t count;
    std::vector< Error_info_T > list;
};

class Error_Table_T
{
public:
    Error_Table_T(DB_T& db);
    ~Error_Table_T();
    
    Error_T list(const Error_info_T& search, const Sort_T& sort, Error_info_list_T & out);
    
    int32_t add(const Error_info_T& info);
    Error_T edit(const Error_info_T& info);
    Error_T info(uint32_t id, Error_info_T& info);
    Error_T info_by_value(int32_t value, Error_info_T& info);
    Error_T del(uint32_t id);
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};

enum {
	SEX_none = 0,
	SEX_male = 1,
	SEX_female = 2,
};
enum LangCode{
    LANG_all = 0,
    LANG_rus  = 1,
    LANG_uzb  = 2,
};

struct User_search_info_T
{
    int64_t id;
    int32_t sex;
    std::string phone;
    std::string name ;
    std::string register_from_date;
    std::string register_to_date;
};

//1.2.79.8  converted each part to separated byte of int64_t type. 
// number of chunks shrinks to 7. i.e. 1.2.79.8 --> 1.2.79.8.0.0.0,   4.5 --> 4.5.0.0.0.0.0
struct app_version
{ 
    typedef ::std::int64_t value_type;
    typedef std::string inout_type;
    
    value_type value;
    
    app_version();
    
    explicit app_version( const std::string & in ) ;
    
    std::string to_str()const;
    
    value_type compare(const app_version & app)const;
};

inline bool operator < (app_version   a, app_version b){ return a.compare(b) < 0 ; } 
inline bool operator > (app_version   a, app_version b){ return a.compare(b) > 0 ; } 
inline bool operator == (app_version  a, app_version b){ return a.compare(b) == 0 ; } 
inline bool operator <= (app_version  a, app_version b){ return a.compare(b) <= 0 ; } 
inline bool operator >= (app_version  a, app_version b){ return a.compare(b) >= 0 ; } 
inline bool operator != (app_version  a, app_version b){ return a.compare(b) != 0 ; } 


struct User_info_T {
	int64_t id;
        uint32_t tr_limit;
	std::string phone;
	std::string name;
	std::string password;
	std::string registration;
        std::string email;
	std::string token;
        std::string notify_token;
        std::string qr_token;
        std::string dev_token;
	uint16_t sex;
        std::string avatar;
        uint16_t lang;
        uint16_t blocked;
        
        std::string platform;
        std::string app_version;

	User_info_T() {
		id = 0;
		sex = SEX_none;
        tr_limit = 0;
        lang = 0;
        blocked = 0;
	}
};

struct User_list_T {
    uint64_t count;
    std::vector<User_info_T> list;
    User_list_T() {
        count = 0;
    }
};

/******************************************/
enum{ CHECK_CODE_LENGTH  = 5 };
/**************************************/
struct Notify_T {
    uint64_t id;
    std::string msg;
    std::string ts;
    std::string type;
    bool is_send;
    uint64_t uid;
    
    Notify_T(uint64_t uid, std::string msg, std::string type, bool is_send)
    : id(0)
    , msg(msg)
    , type(type)
    , is_send(is_send)
    , uid(uid)
    {}
    
    Notify_T() 
    : id(0)
    , is_send(false)
    {
    }
};

struct Notify_list_T {
    uint16_t count;
    std::vector<Notify_T> list;
};


class Users_notify_T
{
public:
    
    explicit Users_notify_T(DB_T& db);
    ~Users_notify_T();
    
    Error_T notification_list(uint64_t uid, Notify_list_T & nlist);
    Error_T notification_readed(uint64_t uid)const;
    Error_T notification_add(const Notify_T &notification);

    Error_T notification_send(uint64_t uid,   std::string  msg, Msg_type__T type);
    Error_T notification_send(const std::string& phone, std::string msg, Msg_type__T type);
    Error_T send2(uint64_t uid, std::string msg, Msg_type__T type,  int timeout_ms ) ;
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};
/**********************************************/

struct Device_info_T {
    uint64_t dev_id;
    uint64_t uid;
    std::string dev_token;
    std::string password;
    std::string add_ts;
    std::string notify_token;
    std::string os;
    std::string login_ts;
    std::string dev_imei;
    Device_info_T() {
        dev_id = 0;
        uid = 0;
    }
};

class Users_device_T
{
public:
    
    
    explicit Users_device_T(DB_T& db);
    ~Users_device_T();
    
    Error_T device_exist(const uint64_t uid, const std::string &dev_token, /*OUT*/int64_t &dev_id);
    
    Error_T device_list(const Device_info_T &search, std::vector<Device_info_T> &list);
    Error_T update_login_ts(uint64_t uid, uint64_t dev_id)const;
    Error_T device_last_login(uint64_t uid, std::vector<Device_info_T> &list);
    Error_T device_change_password(  uint64_t dev_id, const std::string& password);
    
    Error_T device_register(const Device_info_T& info, /*OUT*/int64_t& dev_id);

private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};
/***************************************************/


struct User_bonus_info_T
{
    enum{ACTIVE_CARD = 1, BLOCKED_CARD = 2}; // used block
    
    int64_t id;
    int64_t uid;
    int64_t balance;
    int64_t earns;
    int32_t bonus_card_id;
    int32_t block;
    
    std::string fio;
    std::string pan;
    std::string expire;
    
    
    std::string name;
    std::string phone;
    
    User_bonus_info_T()
    : id(), uid(), balance()
    , earns(), bonus_card_id(), block()
    {}
};

struct  User_bonus_list_T 
{ 
    uint32_t count; 
    std::vector< User_bonus_info_T >list ; 
};

class Users_bonus_T
{
public:
    explicit Users_bonus_T(DB_T& db);
    ~Users_bonus_T();
    
    Error_T bonus_info(uint64_t uid, User_bonus_info_T& info);
    Error_T bonus_add(const User_bonus_info_T& info,  int64_t & bid);
    Error_T bonus_edit(uint64_t  id, const User_bonus_info_T& info);
    Error_T bonus_info_list(const User_bonus_info_T& search, const Sort_T& sort, User_bonus_list_T& list);
    Error_T bonus_edit_block(uint64_t id, uint32_t block);
    
    Error_T reverse_balance(int64_t uid, int64_t amount);
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};

/*************************************************************************/
struct User_cabinet_info_T
{
    typedef int32_t     integer ;
    typedef int64_t     bigint  ;
    typedef std::string text    ;
    
    bigint   id                ;
    bigint   uid               ;
    integer  checkpassword     ; // 0 - undef, 1 - active (i.e. need check password)  2- disabled( i.e. needn't check password on purchase).
    text     password          ;
    text     last_passwd_check ;
    //text     checkcode_expiry  ;
    integer  check_count       ;
    
    inline User_cabinet_info_T(): id(), uid(), checkpassword(), password() /*, last_passwd_check(), check_count() */
    {}
};


class Users_cabinet_T
{
public:
    explicit Users_cabinet_T(DB_T& );
    ~Users_cabinet_T();
    
    void add_client_password(uint64_t uid, const std::string& password);
    Error_T get_client_cabinet(const User_cabinet_info_T& search, User_cabinet_info_T& info);
    bool check_client_password(uint64_t uid, const std::string& password);
    Error_T edit_client_cabinet(const User_cabinet_info_T& search, const User_cabinet_info_T& info);
    Error_T edit_last_password(int64_t id, const User_cabinet_info_T& info ) ;
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};
/**********************************************************************************************************************************/
struct User_online_info_T
{
    int64_t uid;
    int64_t dev_id;
    std::string token;
    User_online_info_T();
    //login_time, last_online will not used there!!!
    
    User_online_info_T(int64_t uid, int64_t dev_id, std::string token);
};

class Users_online_T
{
public:
    explicit Users_online_T(DB_T& db);
    
    Error_T info( const std::string& token, User_online_info_T& info)const;
    
    Error_T add(const User_online_info_T& info)const;
    
    //Error_T update_online_time( const std::string& token)const;
    
    Error_T del( const std::string & token)const;
    Error_T del(int64_t uid, int64_t dev_id);
    
    Error_T check_online()const;
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t ;
    ref_t m_db;
};

/**********************************************************************************************************************************/

class Users_T
{
public:
    explicit Users_T(DB_T & db);
    
    int user_language(uint64_t uid)const;
    User_info_T get(uint64_t uid, Error_T& ec);
    
    Error_T login_with_dev_token(const User_info_T& login_data,   uint64_t& uid, uint64_t & dev_id);
    Error_T login_without_dev_token(const User_info_T& login_data,  uint64_t& uid );
        
    
    Error_T add( User_info_T & data, const std::string &password);
	
    User_info_T info(const std::string & phone,  /*OUT*/ Error_T& ec );
    
	Error_T info(const User_info_T & search, User_info_T & data);
	Error_T change(const std::string & token, const User_info_T & data);
    Error_T change(uint64_t uid, const User_info_T & data);
    
    Error_T user_change_password(  uint64_t uid, const std::string &password);
    
    Error_T del(uint64_t uid);
    Error_T edit_block(uint64_t uid, uint32_t block_value);
    
    
    Error_T register_notify_token(const uint64_t uid, uint64_t dev_id, const std::string &notify_token, const std::string &os);

    Error_T deposit(const uint64_t uid, uint64_t & amount);
	Error_T create_deposit(const uint64_t uid);
    
    Error_T set_avatar(uint64_t uid, const std::string & avatar);

    Error_T list(const User_search_info_T &search, const Sort_T &sort, User_list_T &u_list);

    //@Note: all_phones  must be escaped with DB_T::escape (single quoted), example: '998971234567'
    std::string check_phone( const std::string & all_phones);

    Error_T  generate_img(uint64_t uid , std::string& file_location );
    
    std::string qr_code_location(uint64_t uid );
    
    
    std::string get_far_user_phone();
    
    
private:
    typedef ::std::reference_wrapper<DB_T> ref_t;
    ref_t m_db;
};

/***************************************************************************************************************/
#include <boost/asio/io_service.hpp>

#include "config_types.h"
#include "Pusher.h"
namespace oson
{
    
class XMPP_manager
{
public:
    XMPP_manager( std::shared_ptr< boost::asio::io_service > const & io_service, const xmpp_network_info& xmpp);
    ~XMPP_manager();
    
    void change_password(const std::string & login, const std::string& password );
private:
    
    void change_password_impl( const std::string& request );

#if __cplusplus >= 201103L
public:
    XMPP_manager(const XMPP_manager&) = delete;
    XMPP_manager& operator = (const XMPP_manager&) = delete;
#else     
private:
    XMPP_manager(const XMPP_manager&); // = delete
    XMPP_manager& operator = (const XMPP_manager& ) ; // = delete
#endif 
private:
    std::shared_ptr< boost::asio::io_service  > io_service_;
    xmpp_network_info xmpp_;
    
};    
    
} // end namespace oson
/***************************************************************************************************************/

namespace oson
{
    
typedef std::shared_ptr< boost::asio::io_service > io_service_ptr ;
typedef std::shared_ptr< boost::asio::ssl::context > ssl_ctx_ptr;

class PUSH_manager
{
public:
    
    PUSH_manager( const io_service_ptr & io_service, ios_notify_cert_info const& cert_info);
    ~PUSH_manager();
    
    
    void async_send_push(const std::vector< Device_info_T> & devices, const std::string& msg, Msg_type__T type ) ;
    
    void async_timed_send_push(const std::vector<Device_info_T  >& devices, const std::string& msg, Msg_type__T type, int milliseconds);
    
    const io_service_ptr& io_service()const;
    
#if __cplusplus >= 201103L
public:
    PUSH_manager(const PUSH_manager&) = delete;
    PUSH_manager&  operator = (const PUSH_manager&) = delete;
#else 
private:
    PUSH_manager(const PUSH_manager&);// = delete;
    PUSH_manager&  operator = (const PUSH_manager&);// = delete;

#endif 
    
private:
    
        
    io_service_ptr io_service_;
    ssl_ctx_ptr  ios_ctx_, and_ctx_;
    ios_notify_cert_info cert_info_;
};

    
} // end namespace oson
/***************************************************************************************************************/

namespace oson
{
    ::DB_T& this_db();
    
    std::string random_user_phone();

} // end  namespace oson

/***************************************************************************************************************/
#endif
