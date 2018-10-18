
#include <ctime>
#include <numeric>
#include <cctype>
#include <map>
#include <set>
#include <algorithm>
#include <functional>
#include <memory>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/range/adaptor/transformed.hpp>

#include "adminapi.h"
#include "log.h"

#include "admin.h"
#include "users.h"
#include "transaction.h"
#include "Merchant_T.h"
#include "cards.h"
#include "purchase.h"
#include "news.h"
#include "eocp_api.h"
#include "fault.h"
#include "bank.h"
#include "bills.h"
#include "utils.h"
#include "sms_sender.h"
#include "exception.h"
#include "application.h"
#include "DB_T.h"
#include "topupmerchant.h"
#include "merchant_api.h"
#include "icons.h"
#include "runtime_options.h"


#define FUNC_COUNT 200
#define SRV_HEAD_LENGTH 32
#define SRV_RESP_HEAD_LENGTH 6


namespace 
{

typedef boost::asio::io_service io_service_t;
typedef std::shared_ptr< io_service_t > io_service_ptr;
    
struct admin_data
{
    io_service_ptr  m_io_service;
    bool m_is_logged;
    uint32_t m_aid;
    std::string m_token;
    ByteStreamWriter   m_writer;
    response_handler_type m_response_handler;
//    bool m_active ;
    
    /////////////////////////////////////////
    
    void send_result(Error_T ec);
    //////////////////////////////////////////
};

typedef admin_data  api_type;
typedef std::shared_ptr<admin_data>   api_pointer;

typedef const api_pointer&   api_pointer_cref;
     
} // end noname namespace

static void admin_exec_impl( api_pointer_cref d, const std::vector<uint8_t>& data);

static Error_T dispatch_command(int cmd, api_pointer_cref dptr, ByteReader_T& reader) ;
static std::string push_notify_msg_fix_long(std::string msg);

static bool  admin_has_module_permission(int32_t aid, int32_t module, uint32_t flag);
static bool  admin_has_merch_permission(int32_t aid, int32_t merch_id, uint32_t flag ) ;
static bool admin_has_bank_permission(int32_t aid, int32_t bank_id, uint32_t flag);


void show_header(const uint8_t * data, size_t length );
void show_data(const uint8_t * data, size_t length );


AdminApi_T::AdminApi_T( std::shared_ptr< boost::asio::io_service> io_service  )
: d_ptr(new admin_data)
{
	SCOPE_LOG( slog );
    admin_data* d = static_cast< admin_data*>(d_ptr);
    d->m_is_logged  = false;
    d->m_aid      = 0;
    //d->m_active   = false;
    d->m_io_service = io_service;
}

AdminApi_T::~AdminApi_T() {
	SCOPE_LOGD( slog );
    admin_data* d = static_cast< admin_data*>(d_ptr);
    delete d;
}

void AdminApi_T:: exec( const byte_array& data,  response_handler_type   response_handler )
{
    admin_data* d = static_cast< admin_data*>(d_ptr);
    d->m_response_handler.swap(   response_handler ) ;
    //d->m_active = true;
    
    api_pointer p = api_pointer( shared_from_this(), d );
    
    return admin_exec_impl(p, data);
    
}


/*************************************************************/
static void admin_exec_impl( api_pointer_cref d, const std::vector<uint8_t>& data)
{
    SCOPE_LOGD( slog );

    d->m_writer.clear();

    d->m_writer << b2(0) << b4(0);   // 2-bytes error code and 4-bytes length
    
    Error_T ec = Error_OK ;
    
    ////////////////////////////////////////////////////////////////////////////
    Server_head_T head = parse_header(data.data(), data.size());

    if( head.version != 1 ) {
        slog.ErrorLog("header version invalid(required 1): version = %d", head.version);
        return d->send_result( Error_SRV_version );
    }

    show_header( data.data(), SRV_HEAD_LENGTH  );
    show_data( data.data() + SRV_HEAD_LENGTH, head.data_size );

 
    try
    {
        ByteReader_T reader(data.data() + SRV_HEAD_LENGTH, head.data_size);
        
        
        Error_T error_code =   dispatch_command(head.cmd_id, d, reader ) ;

        return d->send_result( error_code ) ;

    }catch(std::exception& e){
        slog.ErrorLog("standard exception: msg = '%s'", e.what());
        ec =   Error_internal;
    }
        
    return d->send_result( ec );
}

void admin_data::send_result(Error_T ec)
{
    SCOPE_LOGD(slog);
    if (ec == Error_async_processing ) {
        slog.WarningLog("This is async error, skip this.");
        return ;
    }
    
    if (ec > Error_internal) 
           ec = Error_internal;
    //////////////////////////////////////////////////////////
    byte_array& buffer = m_writer.get_buf();
    
    if (buffer.size() < SRV_RESP_HEAD_LENGTH )
        buffer.resize(SRV_RESP_HEAD_LENGTH) ; // add necessary bytes.
    
    size_t buffer_length = buffer.size() - SRV_RESP_HEAD_LENGTH ;
    
    ByteWriter_T writer(buffer);
   
    writer.writeByte2(ec);
    writer.writeByte4(buffer_length);
    
    show_header( buffer.data(), SRV_RESP_HEAD_LENGTH );
    show_data( buffer.data() + SRV_RESP_HEAD_LENGTH, buffer_length   );
    /////////////////////////////////////////////////////////////////////
    if ( static_cast< bool > (m_response_handler ) /*&& m_active*/ )
    {
        response_handler_type rsp;
        rsp.swap(m_response_handler); // take it, it's save from double sending results.
        
        rsp( std::move( buffer ) );
    }
    else
    {
        slog.WarningLog("no ssl response!");
    }
    //m_active = false;
}

#define OSON_PP_ADMIN_CHECK_LOGGED(d)             \
    if ( ! d->m_is_logged )                       \
    {                                             \
        slog.WarningLog("Unauthorized access!");  \
        return Error_login_failed;                \
    }                                             \
/***/

#define OSON_PP_ADMIN_TOKEN_LOGIN(d, reader)   \
do{                                            \
    std::string token = reader.readAsString(reader.readByte2()); \
    api_access_permited(d, token);           \
    OSON_PP_ADMIN_CHECK_LOGGED(d);             \
}while(0);                                     \
/*****/

/****************************************************************************************/

static bool  admin_has_module_permission(int32_t aid, int32_t module, uint32_t flag)
{
    SCOPE_LOG(slog);
    Admin_T table{ oson_this_db } ;
    Error_T ec;
    Admin_permissions_T p;
    
    ec = table.permission_module(aid, module, /*out*/p ) ;
    if (ec) return false;
    
     // p.flag MUST contain all bits where flag is set.
    return ( flag & p.flag ) != (0) ;
}

static bool  admin_has_merch_permission(int32_t aid, int32_t merch_id, uint32_t flag ) 
{
    SCOPE_LOG(slog);
    Admin_T table{ oson_this_db } ;
    Error_T ec;
    Admin_permissions_T perm;
    ec = table.permission_merch(aid, merch_id, perm ) ;
    if (ec) return false;
    
    return  (  (perm.flag & flag)  != 0 ) ;
}

static bool admin_has_bank_permission(int32_t aid, int32_t bank_id, uint32_t flag)
{
    SCOPE_LOG(slog);
    
    Admin_T table{ oson_this_db } ;
    Error_T ec;
    Admin_permissions_T perm;
    ec = table.permission_bank(aid, bank_id, perm);
    if (ec) return false;
    
    return  ( ( perm.flag & flag ) != 0 ) ;
}


static Error_T api_admin_null(api_pointer d, ByteReader_T& reader)
{
	SCOPE_LOGD(slog);
	return Error_SRV_unknown_cmd;
}
/****************************************************************************************/


static Error_T api_logging_debug(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    const size_t  slen   =  std::min<int>(2048, reader.remainBytes());
    const std::string s  = reader.readAsString(slen);
    slog.DebugLog("\n%s\n",  s.c_str());
    return Error_OK ;
}

static bool api_access_permited(api_pointer_cref d, const std::string& token)
{
    SCOPE_LOGD(slog);
    static const std::string  PUBLIC_TOKEN = "Ft5LtRhD76_oson_8IKFyLgcSj" ;
    if (token == PUBLIC_TOKEN){
        d->m_aid = 0;
        d->m_is_logged = true;
        return true ;
    }
    
    if (token.empty())
    {
        slog.WarningLog("token is empty!");
        d->m_is_logged = false;
        return false;
    }
    
    DB_T& db = oson_this_db ;
    
    Admin_T admin( db );
    d->m_is_logged = false;
    
    Error_T ec = admin.logged(token,  /*out*/ d->m_aid);
    
    if (ec != Error_OK) {
        slog.WarningLog("access does not permit!");
        return false;
    }

    d->m_is_logged = d->m_aid > 0;
    d->m_token     = token;
    std::string query = "UPDATE admin_online SET last_ts = NOW() WHERE token = " + escape( token) ;
    DB_T::statement st( db ) ;
    st.prepare( query );
    
    return d->m_is_logged ;
}


static Error_T api_admin_auth(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////////////////////////
    Admin_info_T a_info;
    a_info.login = reader.readAsString(reader.readByte2()); 
    a_info.password = reader.readAsString(reader.readByte2());
    ///////////////////////////////////////////////////////////////
    
    Admin_T admin( oson_this_db  );
    uint32_t aid = 0;
    Error_T error = admin.login(a_info, d->m_is_logged, aid);
    if (error != Error_OK)
        return error;

    /////////////////////////////////////////////////////////////////////////////////
    uint16_t islogon =  static_cast< uint16_t >( d->m_is_logged );
    d->m_writer << b2(islogon);
    
    if (d->m_is_logged) {
        d->m_aid = aid;
        
        uint16_t bill_form   = !!(a_info.flag & ADMIN_FLAG_bill_form) 
                , bus_login  = !!(a_info.flag & ADMIN_FLAG_bus_login) 
                , bus_id     = !!(a_info.flag & ADMIN_FLAG_bus_id   ) 
                , bus_bank   = !!(a_info.flag & ADMIN_FLAG_bus_bank ) 
                , bus_trans  = !!(a_info.flag & ADMIN_FLAG_bus_trans) 
                ;
       d->m_writer << b4(aid) << a_info.token << b2(bill_form) << b2(bus_login) << b2(bus_id) << b2(bus_bank) << b2(bus_trans);
    }
    //////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_admin_auth_token(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////
    std::string token = reader.readAsString(reader.readByte2());
    ///////////////////////////////////////////////////////////////////////
    bool is_logged = api_access_permited(d, token);
    
    uint16_t islogon = static_cast< uint16_t > ( is_logged ); // true -> 1,  false -> 0
    d->m_aid = is_logged ? d->m_aid : 0;
    /////////////////////////////////////////////
    d->m_writer<< b2(islogon) << b4(d->m_aid);
    /////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_admin_logout(api_pointer_cref d, ByteReader_T & reader )
{
    SCOPE_LOGD(slog);
    ////////////////////////////////////////////////////////////////////////
    std::string token = reader.readAsString(reader.readByte2());
    ///////////////////////////////////////////////////////////////////////
    
    Admin_T admin( oson_this_db  );

    return admin.logout(token);
}


static Error_T api_business_qr_code(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    std::string img_data;
    
    uint64_t bill_id = reader.readByte8();

    Admin_T admin( oson_this_db  );

    if (Error_T ec = admin.generate_bill_qr_img(bill_id, img_data) )
        return ec;
    
    img_data = oson::utils::encodebase64(img_data);
    
    ////////////////////////////////////////
    d->m_writer<< b4(img_data);
    //////////////////////////////////////

    return Error_OK;
}


static Error_T api_merchant_qr_code(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    std::string img_data;
    uint32_t merchant_id = reader.readByte8();
    
    Merchant_T merch( oson_this_db  );
    if (Error_T ec = merch.qr_image(merchant_id, img_data))
    {
        if (ec == Error_not_found){
            if ( (ec = merch.generate_qr_image(merchant_id)) != Error_OK )
                return ec;
            if ((ec = merch.qr_image(merchant_id, img_data)) != Error_OK )
                return ec;
        }
        else 
        { // there is an another error.
            return ec;
        }
    }
    img_data = oson::utils::encodebase64( img_data );
    /////////////////////////////////////////
    d->m_writer << b4(img_data);
    ////////////////////////////////////////
    return Error_OK;
}

    
static Error_T api_admin_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    /////////////////////////////////////////////
    Admin_info_T search;
    Sort_T sort;
    reader >> r4(search.aid) >> r2(search.login ) >> r4( sort.offset )  >> r2( sort.limit ) ;
    /////////////////////////////////////////////
    Admin_list_T alist;
    Admin_T admin( oson_this_db  );
    
    
    Error_T error = admin.list(search, sort, alist);
    if(error)
        return error;
    /////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(alist.count ) << b4( alist.list.size() ); 
    
    for(Admin_info_T const& adm : alist.list)
    {
        uint16_t busines     = !!(adm.flag & ADMIN_FLAG_busines  )  , 
                 bill_form   = !!(adm.flag & ADMIN_FLAG_bill_form)  , 
                 bus_login   = !!(adm.flag & ADMIN_FLAG_bus_login)  , 
                 bus_id      = !!(adm.flag & ADMIN_FLAG_bus_id)     , 
                 bus_trans   = !!(adm.flag & ADMIN_FLAG_bus_trans)  ,
                 bus_bank    = !!(adm.flag & ADMIN_FLAG_bus_bank)   ;
                 
        d->m_writer << b4(adm.aid) << b2(adm.status) << adm.login << adm.first_name << adm.last_name << adm.phone
                    << b2(busines) << b2(bill_form) << b2(bus_login) << b2(bus_id) << b2(bus_trans) << b2(bus_bank);
    }
    /////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_admin_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Admin_info_T a_info;
    reader >> r2(a_info.status) >> r2(a_info.login) >> r2(a_info.password) >> r2(a_info.first_name) >> r2(a_info.last_name) >> r2(a_info.phone) ;
    
    int32_t busines =0, bus_login = 0, bus_id = 0, bill_form = 0, bus_bank = 0, bus_trans = 0;
    reader >> r2(busines) >> r2(bus_login) >> r2(bus_id) >> r2(bill_form) >> r2(bus_bank) >> r2(bus_trans) ;
    //////////////////////////////////////////////////////////////////////////
    //@Note: simplify it.
    a_info.flag = a_info.to_flag(  busines, bus_login, bus_id, bill_form, bus_bank, bus_trans  );
    
    /*********check login exists */
    
    Admin_T admin( oson_this_db   );
    if ( int32_t id = admin.search_by_login(a_info.login)   )
    {
        slog.WarningLog("already exists login: %s, with id: %d", a_info.login.c_str(), id) ;
        return Error_admin_already_exists;
    }
    return admin.add(a_info);
}


static Error_T api_admin_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Admin_info_T a_info;

    a_info.aid         = reader.readByte4();
    a_info.status      = reader.readByte2();
    a_info.login       = reader.readAsString( reader.readByte2() ) ;
    a_info.password    = reader.readAsString( reader.readByte2() ) ;
    a_info.first_name  = reader.readAsString( reader.readByte2() ) ;
    a_info.last_name   = reader.readAsString( reader.readByte2() ) ;
    a_info.phone       = reader.readAsString( reader.readByte2() ) ;
    
    
    const uint32_t busines   =   reader.readByte2() ;
    const uint32_t bus_login =   reader.readByte2() ;
    const uint32_t bus_id    =   reader.readByte2() ;
    const uint32_t bill_form =   reader.readByte2() ;
    const uint32_t bus_bank  =   reader.readByte2() ;
    const uint32_t bus_trans =   reader.readByte2() ;

    //@Note: simplify it.
    a_info.flag = a_info.to_flag(busines, bus_login, bus_id, bill_form, bus_bank, bus_trans);
    
    
    if ( ! a_info.aid ) 
        return Error_not_found;
    
    Admin_T admin( oson_this_db  );
    return admin.edit(a_info.aid, a_info);
}


Error_T api_admin_change_password(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    uint32_t id = reader.readByte4();
    std::string old_password = reader.readAsString(reader.readByte2());
    std::string new_password = reader.readAsString(reader.readByte2());
    
    Admin_T admin(oson_this_db );
     
    return admin.change_password(id, old_password, new_password);
}


static Error_T api_admin_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint32_t id = reader.readByte4();
    if (!id){
        return Error_not_found;
    }
    Admin_T admin( oson_this_db  );
    
    Error_T ec;
    ec = admin.del(id);
    
    return ec;
}

static Error_T api_admin_permission_info(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    ///////////////////////////////////////////////
    std::string token  = reader.readAsString(reader.readByte2());
    uint64_t aid             = reader.readByte8();
    ///////////////////////////////////////////////
    
    bool is_logged = api_access_permited( d, token);
    if (!is_logged) {
        slog.WarningLog("Unauthorized access");
        return Error_login_failed;
    }

    if ( aid == 0 )
        aid = d->m_aid;

    Error_T ec = Error_OK;
    Admin_T admin( oson_this_db  );
    std::vector<Admin_permissions_T> permits;
    ec = admin.permissions_list(aid, permits);
    if (ec != Error_OK){
        return ec;
    }

   ///////////////////////////////////////////////////////
    
    //m_writer.writeByte2(permits.size());
    d->m_writer << b2(permits.size());
    
    for(const Admin_permissions_T& per: permits )
    {
        Admin_permit_T permit(per.flag);
        d->m_writer << b4(per.module) << b4(per.merchant) << b4(per.bank) << b1(permit.view) << b1(permit.add) << b1(permit.edit) << b1(permit.del);
    }
    /////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_admin_permission_change(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);

    std::vector< Admin_permissions_T > permits;
 
    const uint64_t aid = reader.readByte8();
 
    const size_t count = reader.readByte2();

    for (size_t i = 0; i != count; i++)
    {
        Admin_permissions_T permission;
        Admin_permit_T permit;
        
        permission.module   = reader.readByte4();
        permission.merchant = reader.readByte4();
        permission.bank     = reader.readByte4();
        
        permit.view     = reader.readByte();
        permit.add      = reader.readByte();
        permit.edit     = reader.readByte();
        permit.del      = reader.readByte();
        
        permission.aid = aid;
        permission.flag = permit.to_flag();
        
        bool const has_permission =  permission.flag != 0; 
        
        if (has_permission)
            permits.push_back(permission);
    }

    Admin_T admin(oson_this_db );
    
    //@Note: transaction automatic rollback when exit failure.
    DB_T::transaction transaction( oson_this_db  );//m_db.begin();
    
    admin.permissions_del(aid);
    
    admin.permissions_add(permits);
    
    transaction.commit();

    return Error_OK;
}

static Error_T api_user_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    User_search_info_T search;
    reader >> r8(search.id) >> r2(search.sex) >> r2(search.phone) >> r2(search.name) >> r2(search.register_from_date) >> r2(search.register_to_date) ;

    Sort_T sort;
    reader >> r4(sort.offset) >> r2(sort.limit);
    
    
    Users_T users( oson_this_db );
    User_list_T u_list;
    Error_T ec = users.list(search, sort, u_list);
    if (ec != Error_OK){
        return ec;
    }

    //////////////////////////////////////////////////////////////
    d->m_writer << b4(u_list.count) << b4(u_list.list.size());
    for(const User_info_T & u: u_list.list)
    {
        d->m_writer << b8(u.id) << b2(u.sex) << b4(u.tr_limit) << u.phone << u.name << u.registration ;
    }
    //////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_user_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    const uint64_t uid = reader.readByte8();
    User_info_T u_info;
    
    u_info.sex      = reader.readByte2();
    u_info.tr_limit = reader.readByte4();
    
    const size_t name_size         = reader.readByte2();
    const size_t registration_size = reader.readByte2();
    
    u_info.name         = reader.readAsString(name_size);
    u_info.registration = reader.readAsString(registration_size);

    Users_T users(oson_this_db );
    return users.change(uid, u_info);
}


static Error_T api_user_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    slog.WarningLog(" DELETE AN USER  NOT ALLOWED!!!");
    return Error_not_found;
    
    //const uint64_t uid = reader.readByte8();

    //Users_T user(m_db);
    //return user.del(uid);
}

static Error_T api_user_block(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    uint64_t uid = reader.readByte8(); // user id
    uint32_t block_value = reader.readByte4();// 0 - nonblocking, 1 - blocking.
    
    Users_T users( oson_this_db  );
    return users.edit_block( uid, block_value);
}

static Error_T api_user_bonus_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    uint64_t uid    = reader.readByte8();
    uint32_t offset = reader.readByte4();
    uint32_t limit  = reader.readByte4();
    
    User_bonus_info_T search;
    search.uid = uid;
    
    Sort_T sort(offset, limit, Order_T(2));//order by uid
    
    User_bonus_list_T list;
    Users_bonus_T users_b( oson_this_db  );
    
    Error_T ec = users_b.bonus_info_list(search, sort, list);
    if (ec)
        return ec;
    //////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    
    for(const User_bonus_info_T& info : list.list )
    {
        d->m_writer << b8(info.id) << b8(info.uid) << b8(info.balance) << b8(info.earns) << b8(info.bonus_card_id) << b4(info.block)
                 << info.phone  << info.name    << info.fio         << info.pan       << info.expire ;
    }
    ///////////////////////////////////////////////
    
    return Error_OK ;
}

static Error_T api_user_bonus_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    
    uint64_t uid = reader.readByte8();
    uint32_t block = reader.readByte4() ;
    
    if (uid == 0 || !( block == 1 || block == 2) )
    {
        slog.WarningLog("Invalid parameters.");
        return Error_parameters;
    }
    
    Users_bonus_T users_b( oson_this_db  );
    
    Error_T ec = users_b.bonus_edit_block(uid, block);
    
    if (ec)
        return ec;
    
    return Error_OK ;
}

static Error_T api_user_notify( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    Error_T ec = Error_OK ;
    
    if(! d->m_is_logged) {
        slog.WarningLog("Access denied");
        //return Error_login_failed;  /** NOTE user notify not required to login  */
    }
    
    /*const*/ uint64_t uid        = reader.readByte8();
    const size_t phone_length = reader.readByte2();
    const size_t msg_length   = reader.readByte2();
    const std::string  phone  = reader.readAsString(phone_length);
    const std::string  msg    = reader.readAsString(msg_length);

    
    
    if ( ! uid && !phone_length)
        return Error_parameters;
    
    if(uid == 0) {
        Users_T user( oson_this_db  );
        User_info_T u_info = user.info(phone, /*OUT*/ ec);

        if (ec != Error_OK)
            return ec;

        uid = u_info.id;
    }
    
    std::string msg_2 = msg;
    
    msg_2 = push_notify_msg_fix_long(msg);
    
    Users_notify_T users_n( oson_this_db  );
    ec = users_n.notification_send(uid, msg_2, MSG_TYPE_MESSAGE);
    if (ec != Error_OK){
        return ec;
    }

    return Error_OK;
}

static Error_T api_user_generate_qr( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint64_t uid = reader.readByte8();
    std::string img_location;
    Users_T user( oson_this_db  );
    return user.generate_img(uid, img_location);
}


static void on_transaction_refnum_check(api_pointer d, const std::string& refnum, const EOPC_Tran_T& tran, Error_T ec )
{
    SCOPE_LOGD(slog);
    d->m_writer << tran.raw_rsp ;

    return d->send_result(ec ) ;
}

static Error_T api_transaction_refnum_check(api_pointer_cref d, ByteReader_T& reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    std::string refnum;
    reader >> r2(refnum);
    
    slog.DebugLog("refnum: %s", refnum.c_str());
    
     
    auto eopc = oson_eopc ;
    
    eopc->async_trans_sv(refnum, std::bind(&on_transaction_refnum_check, d, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
    
    return Error_async_processing ;
}

static Error_T api_transaction_list( api_pointer_cref d , ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    //////////////// 1. Parse Input data ////////////////////////////
    /////////////////////////////////////////////////////////////////
    Transaction_info_T tr_search;
    Sort_T sort( 0, 0, Order_T( 8, 0, Order_T::DESC ) ) ;
    
    reader >> r8(tr_search.id)      >> r8(tr_search.uid)      >> r8(tr_search.amount)   >> r2( tr_search.srccard    )
           >> r2(tr_search.dstcard) >> r2(tr_search.srcphone) >> r2(tr_search.dstphone) >> r2( tr_search.from_date  )
           >> r2(tr_search.to_date) >> r4(tr_search.status)   >> r4(tr_search.bank_id)  >> r4( tr_search.dstbank_id )
           >> r4( sort.offset )     >> r2( sort.limit) ;
    

    if (tr_search.dstbank_id == 0 )
        tr_search.aid = d->m_aid;
    
    ///////////  2. Get Transaction List  ///////////////////////////////
    /////////////////////////////////////////////////////////////////////
    Transactions_T tr( oson_this_db  );
    Transaction_list_T tr_list;
    Error_T error = tr.transaction_list(tr_search, sort, tr_list);
    if (error != Error_OK){
        return error;
    }

    /////////// 3. Writer It buffer. ////////////////////////////////////
    ////////////////////////////////////////////////////////////////////
    d->m_writer << b4(tr_list.count) << b4(tr_list.list.size());
    for(const Transaction_info_T& tr: tr_list.list)
    {
        d->m_writer << b8(tr.id)   << b8(tr.uid)  << b8(tr.amount) << tr.srccard    << tr.dstcard 
                    << tr.srcphone << tr.dstphone << tr.ts         << b4(tr.status) << b8(tr.comission) << tr.eopc_id;
    }
    /////////////////////////////////////////////////
    
    return Error_OK;
}

namespace
{
struct on_purchase_reverse
{
    api_pointer d;
    
    explicit on_purchase_reverse(api_pointer d)
    : d(d)
    {
        //d->m_active = false;
    }
    
    struct inner_trans_sv
    {
        void operator()(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec)const
        {
            SCOPE_LOGD(slog);
            slog.DebugLog("tranId: %s, tran.status: %s,  ec : %d", tranId.c_str(), tran.status.c_str(), (int)ec);
        }
    };
    //this is EOPC thread.
    void operator()(const std::string & ref_num, const EOPC_Tran_T& tran, Error_T ec)
    {
        SCOPE_LOGD(slog);
        on_purchase_reverse  self_copy(*this);
        
        d->m_io_service->post( std::bind(&on_purchase_reverse::do_handle, self_copy, ref_num, tran, ec) ) ;
        
        
    }

    //this is Admin thread.
    void do_handle(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T ec)
    {
        SCOPE_LOGD(slog);
       // d->m_active = true;
        ec = handle(ref_num, tran, ec);
        d->send_result(ec);
    }
    
    Error_T handle(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T )
    {
        SCOPE_LOGD(slog);
        
        if(  ! tran.status_reverse_ok() ) {
            slog.ErrorLog("Reverse transaction failed");
            
            return   Error_internal ;
        }
        
        Purchase_T purch(  oson_this_db  );
        Purchase_search_T search;
        search.eopc_trn_id = ref_num;
        
        Purchase_list_T list;
        Sort_T sort;
        Error_T ec = purch.list(search, sort, list); 
        
        if (!ec && list.list.size() == 1)
        {
            const Purchase_info_T& p_info = list.list[0];
            std::string query;
            DB_T::statement st( oson_this_db   );
            
            //1. set status
            query = "UPDATE purchases SET status = " + escape(TR_STATUS_CANCEL) + " WHERE id = " +escape(p_info.id);

            st.prepare(query);
            
            //2. check bearn
            if (p_info.bearns > 0)
            {
                //cancel bonus
                query = 
                        "UPDATE user_bonus SET "
                        "balance = balance - " + escape(p_info.bearns) + ", "
                        "earns   = earns - "   + escape(p_info.bearns) + " "
                        "WHERE uid = "         + escape(p_info.uid);
                
 
                st.prepare(query);
                
                //3. update bearn also
                query = "UPDATE purchases SET bearn = 0 WHERE id = " + escape(p_info.id);

                st.prepare(query);
            }
            
            
        }
        
        return Error_OK ;
    }
};
    

class transaction_del_session: public std::enable_shared_from_this< transaction_del_session >
{
private:
    typedef transaction_del_session self_type;
    
    api_pointer d;
    int64_t     id;
    std::string ref_num;
    
    /////////////////////
    Transaction_info_T info;
    int n_refs;
public:
    transaction_del_session(api_pointer_cref d, int64_t id, const std::string& ref_num)
    : d(d), id(id), ref_num(ref_num), n_refs(  0 )
    {
        SCOPE_LOGF(slog);
       // d->m_active = false;
    }
    
    ~transaction_del_session()
    {
        SCOPE_LOGF(slog);
        if ( static_cast< bool > ( d->m_response_handler )  ){
            slog.WarningLog("~transaction_del_session_some_is_wrong_go !");
            d->send_result(Error_internal);
        }
    }
    
    void async_start()
    {
        d->m_io_service->post( std::bind(&self_type::start, shared_from_this())) ;
    }
    
private:
    void start()
    {
        SCOPE_LOGD(slog);
        //d->m_active = true;
        Error_T ec =  handle_start();
        //if (d->m_active){
            return d->send_result(ec);
        //}
    }
    
    Error_T handle_start()
    {
        SCOPE_LOGD(slog);

        Transactions_T tr( oson_this_db  );

        

        Error_T ec = tr.info(id, info);

        if(ec  ) return ec;  // not found

        const std::string eopc_ref_num = info.eopc_id;
        const std::size_t index = eopc_ref_num.find(',');
        
        if (index == std::string::npos){
            slog.WarningLog("eopc_ref_num doesnot have a comma: '%s'", eopc_ref_num.c_str());
            
            return Error_internal;
        }
        
        const std::string ref_num_debit = eopc_ref_num.substr(0, index);
        const std::string ref_num_credit = eopc_ref_num.substr(index + 1 );
        
        this->n_refs = 2 ;
        oson_eopc -> async_trans_reverse( ref_num_debit, std::bind(&self_type::on_trans_rev_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
        oson_eopc -> async_trans_reverse( ref_num_credit, std::bind(&self_type::on_trans_rev_eopc, shared_from_this(), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
        
        //d->m_active = false;
        return Error_async_processing ;
    }
    
    //this is eopc thread.
    void on_trans_rev_eopc(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T ec)
    {
        d->m_io_service->post( std::bind(&self_type::on_trans_rev, shared_from_this(), ref_num, tran, ec ) ) ;
    }
    
    //this is admin thread
    void on_trans_rev(const std::string& ref_num, const EOPC_Tran_T& tran, Error_T ec)
    {
        SCOPE_LOGD(slog);
        
        if ( ec ) {
            slog.ErrorLog("EOPC reverse failed with  error code : %d", (int)ec);
            return ;
        }
        
        if ( ! --n_refs ) 
        {
           // d->m_active = true;
            if (info.bearn > 0)
            {
                std::string query = " UPDATE user_bonus SET "
                                    "  balance = balance - " + escape(info.bearn) + 
                                    ", earns = earns - " + escape(info.bearn) + 
                                    "  WHERE uid = " + escape(info.uid) ;

                DB_T::statement st( oson_this_db  );
                st.prepare(query);
            }
            
            Transactions_T tr( oson_this_db  );

            tr.transaction_del(id);
            
            return d->send_result(Error_OK );
        }
    }
    
    
};
} // end noname namespace

static Error_T api_transaction_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint64_t    id         = reader.readByte8();
    const std::string ref_num    = reader.readAsString( reader.readByte2() );

    if ( id != 0 ){
        typedef transaction_del_session session;
        typedef std::shared_ptr< session > session_ptr;

        session_ptr s = std::make_shared< session >(d, id, ref_num);

        s->async_start();
    }
    else  // reverse the purchase
    {
        oson_eopc -> async_trans_sv(ref_num, on_purchase_reverse::inner_trans_sv() ) ;
        oson_eopc -> async_trans_reverse( ref_num, on_purchase_reverse( d ) ) ;
        

        Purchase_T purch(  oson_this_db  );
        Purchase_search_T search;
        search.eopc_trn_id = ref_num;
        
        Purchase_list_T list;
        Sort_T sort;
        Error_T ec = purch.list(search, sort, list); 
        if (ec ||list.list.size() != 1 ) return Error_async_processing;
        
        const Purchase_info_T& p_info = list.list[0];
        
        Merchant_T merch_table(oson_this_db);
        Merchant_info_T merchant = merch_table.get(p_info.mID, ec);
        if (merchant.api_id == merchant_api_id::ums)
        {
            Merch_acc_T acc;
            merch_table.api_info(merchant.api_id, acc);
            
            Merchant_api_T api_m(merchant, acc);
            Merch_trans_T trans;
            
            trans.user_phone = p_info.login;
            trans.transaction_id = p_info.oson_paynet_tr_id;
            trans.merch_api_params["paynet_tr_id"] = p_info.paynet_tr_id ; 
            
            std::vector< std::string > results;
            boost::algorithm::split(results, p_info.merch_rsp, boost::is_any_of(";") ) ;
            //11101;4600.40;2018-09-05T13:59:04
            if (results.size()>=2)
                trans.ts = results[2];
            else
                trans.ts = p_info.ts;
            
            trans.amount = p_info.amount ;
            
            Merch_trans_status_T response;
            api_m.cancel_pay(trans, response);
        }

    }

    
    return Error_async_processing;
}

static Error_T api_transaction_statistics( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Transaction_info_T search;
    
    search.uid = reader.readByte8(); 
    
    const uint16_t group           = reader.readByte2();
    const size_t   src_card_length = reader.readByte2();
    const size_t   dst_card_length = reader.readByte2();
    const size_t   from_length     = reader.readByte2();
    const size_t   to_length       = reader.readByte2();
    
    search.srccard = reader.readAsString(src_card_length);
    search.dstcard = reader.readAsString(dst_card_length);
    
    search.from_date = reader.readAsString(from_length);
    search.to_date = reader.readAsString(to_length);
    
    Transactions_T tr( oson_this_db  );
    std::vector<Tr_stat_T> stats;
    Error_T error = tr.stat(group, search, stats);
    if(error != Error_OK)
        return error;

    ///////////////////////////////////////////////////////
    d->m_writer << b2(stats.size()); 
    for(Tr_stat_T const& s: stats)
    {
        d->m_writer << b8(s.total) << b8(s.users) << b8(s.sum) << s.ts ;
    }
    ///////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_transaction_top(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Transaction_info_T search;
    
    const size_t from_length = reader.readByte2();
    const size_t to_length   = reader.readByte2();
   
    search.from_date = reader.readAsString(from_length);
    search.to_date   = reader.readAsString(to_length);
    
    Transactions_T tr( oson_this_db  );
    std::vector<Transaction_top_T> tops;
    Error_T error = tr.top(search, tops);
    if(error != Error_OK)
        return error;

    ///////////////////////////////////////////////////////
    d->m_writer << b2(tops.size()); 
    
    for(Transaction_top_T const& top: tops)
    {
        d->m_writer << b8(top.count) << b8(top.sum) << top.phone;
    }
    ////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_merchant_group_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint32_t id = reader.readByte4();

    Merchant_group_T m_group( oson_this_db  );
    std::vector<Merch_group_info_T> group_list;
    Error_T error = m_group.list(id, group_list);
    if(error != Error_OK)
        return error;

    //////////////////////////////////////////////////////////////
    d->m_writer << b2(group_list.size()) << b2(group_list.size());
    for(const Merch_group_info_T& g : group_list)
    {
        d->m_writer << b4(g.id) << b4(g.position) << g.name << g.name_uzb << g.icon_path;
    }
    ////////////////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_merchant_group_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Merch_group_info_T info;
    
    info.position = reader.readByte4();
    
    info.name = reader.readAsString(reader.readByte2());
    info.name_uzb = reader.readAsString(reader.readByte2());
    
    Merchant_group_T m_group( oson_this_db  );
    return m_group.add(info);
}

static Error_T api_merchant_group_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    /////////////////////////////////////////////
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    /////////////////////////////////////////
    Merch_group_info_T info;
    reader >> r4(info.id) >> r4(info.position) >> r2(info.name) >> r2(info.name_uzb) >> r2(info.icon_path) ;
    
    if ( ! info.id ) {
        return Error_parameters;
    }
    
    Merchant_group_T m_group( oson_this_db );
    return m_group.edit(info.id, info);
}

static Error_T api_merchant_group_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint32_t id = reader.readByte4();
    if ( ! id ){
        slog.WarningLog("not id set!");
        return Error_parameters;
    }
    
    Merchant_group_T m_group( oson_this_db   );
    return m_group.del(id);
}

static Error_T api_merchant_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    std::string token = reader.readAsString(reader.readByte2());

    bool const is_logged = api_access_permited( d, token );

    if (!is_logged) {
        slog.WarningLog("Unauthorized access");
        return Error_login_failed;
    }
  
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_MERCHANTS, Admin_permit_T::VIEW_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Merchant_info_T search;
    reader >> r4(search.id) >> r4(search.bank_id) >> r4(search.group) >> r2(search.name) ;

    Sort_T sort;
    reader >> r4( sort.offset ) >> r2( sort.limit )  ;
    
    
    Merchant_list_T list;
    Merchant_T merchant( oson_this_db   );
    
    Error_T ec = merchant.list(search, sort, list);
    if(ec != Error_OK)
        return ec;

    
    /////////////////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    for(const Merchant_info_T& merch : list.list)
    {
    
        d->m_writer << b4(merch.id )        << b4(merch.group)    << b2(merch.status)      << b8(merch.min_amount)
                    << b8(merch.max_amount) << b2(merch.port)     << b2(merch.external)    << merch.extern_service 
                    << merch.name           << merch.inn          << merch.mfo             << merch.checking_account 
                    << merch.url            << merch.contract     << merch.contract_date   << merch.merchantId
                    << merch.terminalId     << b4(merch.bank_id)  << b4(merch.rate ) 
                    << b8(merch.rate_money) << b4(merch.position) << b4(merch.api_id ) ;
    }
    /////////////////////////////////////////
    return Error_OK;
}

static Error_T api_merchant_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader );

    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_MERCHANTS, Admin_permit_T::ADD_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Merchant_info_T mi ;
    
    reader  >> r4(mi.id)        >> r4( mi.group )        >> r4(mi.status)     >> r8(mi.min_amount)  >> r8(mi.max_amount)  >> r4( mi.port ) 
            >> r4(mi.external)  >> r2(mi.extern_service) >> r2(mi.name)       >> r2(mi.inn)         >> r2(mi.mfo )        >> r2( mi.checking_account )
            >> r2(mi.contract)  >> r2(mi.contract_date ) >> r2(mi.url)        >> r2(mi.merchantId)  >> r2(mi.terminalId)  >> r4(mi.bank_id)    
            >> r4(mi.rate)      >> r8(mi.rate_money)     >> r4(mi.position)   >> r4(mi.api_id) ;
 
    Merchant_T table{ oson_this_db } ;
    
    return table.add( mi ) ;
}

static Error_T api_merchant_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_MERCHANTS, Admin_permit_T::EDIT_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Merchant_info_T mi ;
    
    reader  >> r4(mi.id)        >> r4( mi.group )        >> r4(mi.status)     >> r8(mi.min_amount)  >> r8(mi.max_amount)  >> r4( mi.port ) 
            >> r4(mi.external)  >> r2(mi.extern_service) >> r2(mi.name)       >> r2(mi.inn)         >> r2(mi.mfo )        >> r2( mi.checking_account )
            >> r2(mi.contract)  >> r2(mi.contract_date ) >> r2(mi.url)        >> r2(mi.merchantId)  >> r2(mi.terminalId)  >> r4(mi.bank_id)    
            >> r4(mi.rate)      >> r8(mi.rate_money)     >> r4(mi.position)   >> r4(mi.api_id) ;
 
    Merchant_T table{  oson_this_db    };
    
    return table.edit( mi ) ;
}

static Error_T  api_merchant_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;

    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_MERCHANTS, Admin_permit_T::DEL_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    const uint32_t id = reader.readByte4();
    
    if ( ! admin_has_merch_permission(d->m_aid,  id, Admin_permit_T::DEL_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Merchant_T merch( oson_this_db );
    
    return merch.del(id);
}

static Error_T api_merchant_api_list(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);
     
    DB_T::statement st(oson_this_db);
    
    Error_T ec = Error_OK ;
    st.prepare("SELECT id, name, status, api_id FROM merchant_api ORDER BY api_id;", /*out*/ ec );
    
    if (ec) return ec;
    
    int rows = st.rows_count() ;
    
    d->m_writer << b4(rows)  << b4(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        int32_t id, status, api_id;
        std::string name;
        st.row(i) >> id >> name >> status >> api_id;
        
        d->m_writer << b4(api_id) <<  b4(status) << name;
        
        oson::ignore_unused(id);
    }
    
    return Error_OK ;
}



static Error_T  api_merchant_icon_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_CHECK_LOGGED( d );
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_MERCHANTS, Admin_permit_T::EDIT_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    const uint32_t id = reader.readByte4();
    
    //@Note: PNG icon size may be greater than 64 KByte!!!
    const size_t png_length = reader.readByte2();
    std::string img = reader.readAsString(png_length);
    /*************************************/
    Error_T ec = Error_OK ;
    
    std::string query = "SELECT icon_id FROM merchant WHERE id = " + escape(id);
    DB_T::statement st(oson_this_db);
    
    st.prepare(query, ec);
    if (ec) return ec;
    if (st.rows_count() != 1 ) return Error_not_found;
    
    int64_t old_icon_id = 0;
    st.row(0) >> old_icon_id ;
    
    
    oson::icons::content icon_content;
    icon_content.image = std::move(img);
    
    oson::icons::manager  icon_mng;
    
    auto icon_info  = icon_mng.save_icon(icon_content, oson::icons::Kind::merchant, old_icon_id ) ;
    
    if (icon_info.id == 0 ) {
        slog.ErrorLog("Can't put icon!");
        return Error_not_found;
    }
    
    if (icon_info.id == old_icon_id )
    {
       slog.WarningLog("icon not changed!");
       return Error_OK ;
    }
    
    query = "UPDATE merchant SET icon_id = " + escape(icon_info.id ) + " WHERE id = " + escape(id);
    st.prepare(query, ec);

    return ec;
}

static Error_T  api_merchant_icon_get( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint32_t id = reader.readByte4();
   
    std::string query = "SELECT icon_id FROM merchant where id = " + escape(id);
    DB_T::statement st(oson_this_db);
    st.prepare(query);
    if (st.rows_count() != 1 ) return Error_not_found;
    
    int64_t icon_id = 0;
    st.row(0) >> icon_id; 
    std::string img;
    
    oson::icons::content icon_content;
    oson::icons::manager icon_mng;
    int ret = icon_mng.load_icon(icon_id, icon_content ) ;

    if (!ret)return Error_not_found;

    img = std::move(icon_content.image);
    
    img = oson::utils::encodebase64(img); // convert it to base64 codec.
    //////////////////////////////////////////////
    d->m_writer << b4(img);
    ///////////////////////////////////////////////
    return Error_OK;
}

/************************************************************************************/

/**************************************************************************************/
static Error_T  api_merchant_field_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );
    //////////////////////////////////////////////
    int32_t const merchant_id    = reader.readByte4();
    int32_t const fID            = reader.readByte4();
    
    int32_t const offset         = reader.readByte4();
    int32_t const limit          = reader.readByte4();
    ////////////////////////////////////////////////
    
    Sort_T sort(offset, limit, Order_T(4)); // order by position

    Merchant_field_T search;
    search.merchant_id = merchant_id;
    search.fID         = fID ;
    search.usage       = search.USAGE_UNDEF;
    
    Merchant_T merch( oson_this_db  );
    Merchant_field_list_T  fields;
    Error_T error = merch.fields(search, sort, fields);

    if(error != Error_OK)
        return error;

    //////////////////////////////////////////////////////////////////////
    d->m_writer << b4(fields.count) << b4(fields.list.size());
    
    for(Merchant_field_T const& f : fields.list )
    {
        d->m_writer << b2(f.position)    << b4(f.fID)          << b4(f.parent_fID) << b2(f.type)
                    << b2(f.input_digit) << b2(f.input_letter) << b2(f.min_length) << b2(f.max_length)
                    << f.label           << f.prefix_label     << f.param_name     << b2(f.usage) 
                    << f.label_uz1 ;
    }
    //////////////////////////////////////////////////////////////////////////
    return Error_OK;
}


static Error_T  api_merchant_field_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );
    
    Merchant_field_T m_info;
    m_info.merchant_id    = reader.readByte4();
    m_info.parent_fID     = reader.readByte4();
    m_info.position       = reader.readByte2();
    m_info.type           = reader.readByte2();
    m_info.input_digit    = reader.readByte2();
    m_info.input_letter   = reader.readByte2();
    m_info.min_length     = reader.readByte2();
    m_info.max_length     = reader.readByte2();

    m_info.label        = reader.readString();
    m_info.prefix_label = reader.readString();
    m_info.param_name   = reader.readString();
    m_info.usage        = reader.readByte2() ;
    m_info.label_uz1    = reader.readString();
    
    Merchant_T merch( oson_this_db  );
    return merch.field_add(m_info);
}


static Error_T api_merchant_field_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Merchant_field_T m_info;
    const uint32_t field_id = reader.readByte4();

    m_info.parent_fID   = reader.readByte4();
    m_info.position     = reader.readByte2();
    m_info.type         = reader.readByte2();
    m_info.input_digit  = reader.readByte2();
    m_info.input_letter = reader.readByte2();
    m_info.min_length   = reader.readByte2();
    m_info.max_length   = reader.readByte2();

    m_info.label        = reader.readString();
    m_info.prefix_label = reader.readString();
    m_info.param_name   = reader.readString();
    m_info.usage        = reader.readByte2();
    m_info.label_uz1    = reader.readString();
    
    Merchant_T merch( oson_this_db  );
    return merch.field_edit(field_id, m_info);
}

static Error_T api_merchant_field_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint32_t field_id = reader.readByte4();
   
    Merchant_T merch( oson_this_db );
    return merch.field_delete(field_id);
}

static Error_T api_merchant_field_data_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint32_t field_id = reader.readByte4();
    const uint32_t id       = reader.readByte4();

    int const offset = reader.readByte4();
    int const limit  = reader.readByte4();
    
    Sort_T sort(offset, limit);
    
    Merchant_T merch( oson_this_db  );
    Merchant_field_data_list_T fields;
    
    if (id != 0) 
    {
        Merchant_field_data_T f_data, search;
        search.id = id;
        
        Error_T error = merch.field_data_search(search, sort, f_data);
        if(error != Error_OK)
            return error;
        
        fields.list.push_back(f_data);
        fields.count = 1;
        
    }
    else 
    {
        Merchant_field_data_T search;
        search.fID = field_id;
        Error_T error = merch.field_data_list(search, sort, fields );
        if(error != Error_OK)
            return error;
    }

    /////////////////////////////////////////////////////
    d->m_writer << b4(fields.count) << b4(fields.list.size());
    for(const Merchant_field_data_T& f : fields.list)
    {
        d->m_writer << b4(f.id)       << b4(f.fID)        << b4(f.key)              << b4(f.parent_key)
                    << b4(f.extra_id) << b4(f.service_id) << b4(f.service_id_check) << f.prefix
                    << f.value;
    }
    ///////////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_merchant_field_data_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader ) ;
    
    const uint32_t id  = reader.readByte4(); // id will is zero .
    const uint32_t fid = reader.readByte4();
    const uint32_t key = reader.readByte4();
    const uint32_t parent_key = reader.readByte4();
    const uint32_t extra_id   = reader.readByte4();
    const uint32_t service_id = reader.readByte4();
    const uint32_t service_id_check = reader.readByte4();

    const size_t value_length  = reader.readByte2();
    const size_t prefix_length = reader.readByte2();
   
    const std::string value  = reader.readAsString(value_length);
    const std::string prefix = reader.readAsString(prefix_length);
    
    slog.DebugLog("id = %u key = %u  value = %s  prefix = %s\n", fid, key, value.c_str(), prefix.c_str());
    
    Merchant_field_data_T field_data;
    field_data.id  = id;
    field_data.fID = fid;
    field_data.id = 0;
    field_data.key = key;
    field_data.value = value;
    field_data.prefix = prefix;
    field_data.extra_id = extra_id;
    field_data.service_id = service_id;
    field_data.service_id_check = service_id_check;
    field_data.parent_key = parent_key;

    Merchant_T merch( oson_this_db  );
    Error_T error = merch.field_data_add(field_data);
    return error;
}

static Error_T api_merchant_field_data_edit(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader );
    

    Merchant_field_data_T field_data;
    field_data.id  = reader.readByte4();
    field_data.fID = reader.readByte4();
    field_data.key = reader.readByte4();
    field_data.parent_key = reader.readByte4();
    field_data.extra_id   = reader.readByte4();
    field_data.service_id = reader.readByte4();
    field_data.service_id_check = reader.readByte4();

    const size_t value_length  = reader.readByte2();
    const size_t prefix_length = reader.readByte2();

    field_data.value  = reader.readAsString(value_length);
    field_data.prefix = reader.readAsString(prefix_length);

    Merchant_T merch( oson_this_db  );
    Error_T error = merch.filed_data_edit(field_data);
    return error;
}

static Error_T api_merchant_field_data_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);

    const uint32_t id = reader.readByte4();

    Merchant_T merch( oson_this_db  );

    Error_T error = merch.field_data_delete(id);

    return error;
}


static void str_set_intersect(std::string& in_mids,  const std::string& per_mids)
{
    SCOPE_LOG(slog);
    //////////////////////////////////////////////////
    if (in_mids.empty() || per_mids.empty() ) 
    {
        in_mids =  per_mids; 
        return;
    }
    
    std::set< int32_t > per_mchs; 
    int32_t mid = 0;
    // permitted_merchants:  1, 132, 324, 32423, ...
    for(char c: per_mids)
    {
        if ( std::isdigit( c ) ) 
        { 
            mid = mid * 10 + c - '0';
        } 
        else if (mid > 0) 
        {
            per_mchs.insert( mid );
            mid = 0;
        }
    }
    if (mid > 0) per_mchs.insert( mid );
    //////////////////////////////////////////////

    std::string out;
    char comma = ' ';
    
    char mid_arr[ 32 ] = {} ;
    size_t sz_arr      = 0 ;
    
    out.reserve( in_mids.size() );
    mid = 0;

    for(char c: in_mids)
    {
        if ( std::isdigit(c))
        {
            if(mid == 0 && c == '0') // leading zero, skip it
                continue;

            if ( INT32_MAX / 10 <  mid) {
                slog.WarningLog("number is very big");
                break;
            }
            mid = mid * 10 + c - '0';
            mid_arr[sz_arr++] = c;
        } 
        else if (mid > 0 ) 
        {
            if (per_mchs.count(mid)) 
            {
                out += comma;
                out.append(mid_arr, sz_arr);
                comma = ',';
            }
            mid = 0;
            sz_arr = 0;
        }
    } 
    
    if (mid > 0 && per_mchs.count(mid) > 0 )
    {
        out += comma;
        out.append(mid_arr, sz_arr);
    }
    
    in_mids.swap( out );
}

static std::pair< std::string, bool >  filter_merchant_list_by_permissions(api_pointer_cref d, std::string   merchant_list)
{
    SCOPE_LOG(slog);
    
    bool revert = false;
    
    /********* check permissions for this admin ***********/
    Admin_T admin( oson_this_db  );
    std::string permitted_merchants;
    
    admin.permissions_merchant_ids( d->m_aid,  permitted_merchants ) ;
    
    boost::algorithm::trim(merchant_list);
    
    str_set_intersect( /*IN-OUT*/merchant_list, /*IN*/permitted_merchants );

    const std::size_t cnt = std::count(merchant_list.begin(), merchant_list.end(), ',' ) ;
    if ( cnt > 4 ) // about, more than 4 merchants
    {
       permitted_merchants.clear();
       admin.not_permissions_merchant_ids(d->m_aid , permitted_merchants);
       
       const std::size_t cnt_rev = std::count(permitted_merchants.begin(), permitted_merchants.end(), ',' ) ;
       
       if ( cnt_rev < cnt  ) 
       {
           merchant_list.swap(permitted_merchants);
           revert = true;
       }
    }
     
    return std::make_pair( merchant_list, revert ) ;
}

static Error_T api_purchase_list_zip(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);
    if(d->m_aid == 0 ) {
        slog.WarningLog("Not allowed public token!");
        return Error_access_denied;
    }
    /**************************************/
    Sort_T sort(0,0, Order_T(1,0, Order_T::DESC)); //by default: no offset, no limit, order by 1 DESC , where 1- id.
    
    Purchase_search_T search;
    
    reader >> r8(search.id) >> r8(search.uid)    >> r2(search.m_merchant_ids) >> r2(search.from_date) >> r2(search.to_date) 
           >> r4(search.status) >> r4( search.bank_id )  >> r4(sort.offset ) >> r2(sort.limit);


    std::string permitted_merchants; bool reverted;
    std::tie(permitted_merchants, reverted ) = filter_merchant_list_by_permissions(d, search.m_merchant_ids ) ;
    search.m_merchant_ids     = permitted_merchants;
    search.m_use_merchant_ids = true       ;//always use !!
    search.in_merchants       = ! reverted ;
       
     //from root
    // su - postgres && PGPASSWORD= LcR7nhqtQkZgjqTD psql -h 185.8.212.69 -U oson -W -c "COPY (select * from admins order by id ) TO stdout DELIMITER ',' CSV HEADER"  | gzip > /tmp/admins.csv.gz && exit && chown www-data:www-data /tmp/admins.csv.gz
    //(PGPASSWORD=LcR7nhqtQkZgjqTD psql -h 185.8.212.69 -U oson -w -c "COPY (select * from admins order by id ) TO stdout DELIMITER ',' CSV HEADER" |gzip > /tmp/admins.csv.gz) && ( chown www-data:www-data /tmp/admins.csv.gz ) 
    
    std::string query = Purchase_T::list_admin_query( search, sort ) ;
    std::string filename = "/tmp/purchase_" + oson::utils::md5_hash( to_str( time( 0  ) ) ) + ".csv.gz";
    auto ci = DB_T::connectionInfo() ;
    std::string pwd = ci.m_pass, host = ci.m_host, user = ci.m_user ;
    
    std::string cmd = "(PGPASSWORD=" + pwd +  " psql -h " + host + " -U " + user + " -w -c "
                      " \"COPY ( " + query + " ) TO stdout DELIMITER ',' CSV HEADER\" "
                      " | gzip > " + filename + ") && ( chown www-data:www-data " + filename + " ) " ;

    
    int ret = system(cmd.c_str());
    
    slog.InfoLog("system '%s'  \n\t\treturn: %d   code ", cmd.c_str(), ret ) ;
    
    d->m_writer << filename ;
    return Error_OK ;
}

static Error_T api_purchase_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    if(d->m_aid == 0 || d->m_aid < 0) {
        slog.WarningLog("Not allowed public token!");
        return Error_access_denied;
    }
    //////////////////////////////////////////////
    Sort_T sort(0,0, Order_T(1, 0, Order_T::DESC));
   // int32_t bank_id = 0;
    Purchase_search_T search;
    
    search.flag_total_sum = 1;//always get count and sum. yet.
    
    reader >> r8(search.id) >> r8(search.uid)    >> r2(search.m_merchant_ids) >> r2(search.from_date) >> r2(search.to_date) 
           >> r4(search.status) >> r4( search.bank_id )  >> r4(sort.offset ) >> r2(sort.limit);
    
    std::string hash_raw = to_str(d->m_aid)         + 
                           d->m_token               + 
                           to_str(search.id)        + 
                           to_str(search.uid)       + 
                           search.m_merchant_ids    + 
                           search.from_date         + 
                           search.to_date           + 
                           to_str(search.status)    + 
                           to_str(search.bank_id)   ;
    
    std::string hash = oson::utils::md5_hash( hash_raw );
    slog.DebugLog("hash: %s ", hash.c_str());
    struct cache_node{
        int64_t count, sum, comission_sum;
        std::time_t ts;
    };
    static std::map< std::string, cache_node > cache_total_sum_and_count;
    ////remove old caches
    {
        
        for(auto it = cache_total_sum_and_count.begin() ; it != cache_total_sum_and_count.end();  /****/ )
        {
            if ( std::time( 0 ) - ( *it ).second.ts    >= 60  ) // more than 1 minutes
            {
                it = cache_total_sum_and_count.erase(it);
            } else {
                ++it;
            }
        }
    }
    ////////////////////////
    const bool has_cache = cache_total_sum_and_count.count(hash);
    cache_node cache_value  = {};
    if (has_cache){
        
        search.flag_total_sum = 0;
        cache_value = cache_total_sum_and_count[hash];
    }
    
    /////////////////////////////////////////////////////////////////
    if (sort.limit == 0 || sort.limit > 1024 ) {
        sort.limit = 1024;
    }
    
    /********* check permissions for this admin ***********/
    std::string permitted_merchants; bool reverted;
    std::tie(permitted_merchants, reverted ) = filter_merchant_list_by_permissions(d, search.m_merchant_ids ) ;
    search.m_merchant_ids     = permitted_merchants;
    search.m_use_merchant_ids = true       ;//always use !!
    search.in_merchants       = ! reverted ;
    /******************************************/
    Purchase_T purchases( oson_this_db  );
    Purchase_list_T plist;
    Error_T ec = purchases.list_admin(search, sort, plist);
    if(ec != Error_OK)
        return ec;

    
    if (search.flag_total_sum == 0 )  // so take from cache values
    {
        plist.count = cache_value.count ;
        plist.sum   = cache_value.sum   ;
        plist.commission_sum = cache_value.comission_sum ;
    } 
    else   // otherwise cache they.
    {
        cache_value.count          = plist.count          ;
        cache_value.sum            = plist.sum            ;
        cache_value.comission_sum  = plist.commission_sum ;
        cache_value.ts             = std::time(0)         ;
        cache_total_sum_and_count[hash] = cache_value     ;
    }
    
    //////////////////////////////////////////////
    d->m_writer << b8(plist.count) << b8( plist.sum  ) << b8(plist.commission_sum   ) << b4(plist.list.size());
    for( const Purchase_info_T& p : plist.list)
    {
        std::string merch_rsp = p.merch_rsp;
        
        if ( p.status == TR_STATUS_SUCCESS ) {
             merch_rsp = "Успешно";
        } else if ( merch_rsp.empty() ) { 
            switch(p.status){
                case TR_STATUS_ERROR     : merch_rsp = "Ошибка"          ; break;
                case TR_STATUS_REGISTRED : merch_rsp = "Зарегистрирован" ; break;
                case TR_STATUS_CANCEL    : merch_rsp = "Отменён"         ; break;
                case TR_STATUS_REVERSED  : merch_rsp = "Аннулированный"  ; break;
            }
        }
        
        d->m_writer << b8(p.id)       << b8(p.uid)     << b4(p.mID)      << b8(p.amount) << b8(p.commission)
                 << b4(p.status)      << p.login       << p.ts           << p.src_phone
                 << b8(p.receipt_id)  << p.eopc_trn_id <<  b8(p.card_id) <<  merch_rsp << p.pan ;
    }
    ////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_purchase_stat( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );
    
    Purchase_search_T search;
    search.uid = reader.readByte8();
    search.mID = reader.readByte4();
    
    const uint16_t group = reader.readByte2();
    
    const size_t from_length = reader.readByte2();
    const size_t to_length = reader.readByte2();
    
    search.from_date = reader.readAsString(from_length);
    search.to_date = reader.readAsString(to_length);
   
    Purchase_T purchases( oson_this_db  );
    std::vector<Purchase_state_T> stats;
    Error_T error = purchases.stat(group, search, stats);
    if(error != Error_OK)
        return error;

    /////////////////////////////////////////////////
    d->m_writer << b2(stats.size());
    for(const Purchase_state_T& s : stats  )
    {
        d->m_writer << b8(s.total) << b8(s.users) << b8(s.sum ) << s.ts ;
    }
    ///////////////////////////////////////////
    return Error_OK;
}

static Error_T api_purchase_top( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );
    
    Purchase_search_T search;
    
    const size_t from_length = reader.readByte2();
    const size_t to_length = reader.readByte2();
    
    search.from_date = reader.readAsString(from_length);
    search.to_date = reader.readAsString(to_length);
    
    Purchase_T purch( oson_this_db  );
    std::vector<Purchase_top_T> tops;
    Error_T error = purch.top(search, tops);
    if(error != Error_OK)
        return error;

    ///////////////////////////////////////////////
    d->m_writer << b2(tops.size());
    for(const Purchase_top_T& t: tops)
    {
        d->m_writer << b8(t.count) << b8(t.sum) << b4(t.merchant_id);
    }
    //////////////////////////////////////////
    return Error_OK;
}
namespace
{
struct purchase_reverse_session
{
    typedef purchase_reverse_session self_type;
    api_pointer d;
    Purchase_info_T p_info;
    Purchase_reverse_info_T rev_info;

    purchase_reverse_session(api_pointer d, Purchase_info_T p_info, Purchase_reverse_info_T rev_info)
            : d(d), p_info(p_info), rev_info(rev_info)
    {
        SCOPE_LOGF(slog);
        //d->m_active = false;
    }

    void operator()(const std::string&, const EOPC_Tran_T& tran_cancel, Error_T ec)const
    {
        self_type self_copy(*this);
        d->m_io_service->post(std::bind(&self_type::on_reverse, self_copy, tran_cancel, ec) ) ;
    }

    void on_reverse(const EOPC_Tran_T& tran_cancel, Error_T ec)
    {
        SCOPE_LOGD(slog);
        //d->m_active = true;
        ec = handle_on_reverse(tran_cancel, ec);
        return d->send_result(ec);
    }

    Error_T handle_on_reverse(const EOPC_Tran_T& tran_cancel, Error_T ec)
    {
        SCOPE_LOGF(slog);
        Purchase_reverse_T rev( oson_this_db  );
     
        rev_info.ts_confirm = formatted_time_now("%Y-%m-%d %H:%M:%S");

        if(ec != Error_OK || !tran_cancel.status_reverse_ok() ) {
            rev_info.status = 2;// error
            rev.upd(rev_info);
            return ec;
        }

        Purchase_T purch( oson_this_db   );
        
        p_info.status = TR_STATUS_REVERSED ;
        purch.update_status( p_info.id, p_info.status );

        rev_info.status = 1; // success

        rev.upd(rev_info);

        return Error_OK;
    }
};    
}
static Error_T api_purchase_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);
    /***************************************************/
    uint32_t tp             = reader.readByte4() ;
    int64_t id              = reader.readByte8() ;
    int64_t rev_id          = reader.readByte8() ;
    std::string code        = reader.readAsString(reader.readByte2());
    /****************************************************/
    if (!(tp == 1 || tp == 2)){
        slog.ErrorLog("Unknown type: %u", (unsigned)tp);
        return Error_SRV_data_length;
    }
    /*************************************************************/
    Error_T ec;

    //@Note: transaction automatic rollback when failure exit.
    Purchase_T purch( oson_this_db   );
    Purchase_info_T p_info;
    ec = purch.info(id, p_info);
    if (ec) return ec;
    
    if (p_info.status != TR_STATUS_SUCCESS )
    {
        slog.ErrorLog("error purchase can't reverse! status = %d", ( int ) p_info.status );
        return Error_not_found;
    }
    
    
    Merchant_T merch( oson_this_db   );
    Merchant_info_T merch_info = merch.get(p_info.mID, ec);
    if (ec) return ec;
    
    
    std::string const& url =  merch_info.url;
    if ( ! ( url.empty() || url == "0" ) ) //
    {
        slog.ErrorLog("Now allowed reverse this merchant!");
        return Error_operation_not_allowed;
    }
    
    /************************************************************/
    Admin_T admin( oson_this_db   );
    Admin_permissions_T per_module, per_merchant, per_bank ; 
    
    ec = admin.permission_merch( d->m_aid, p_info.mID, per_merchant);
    if(ec) return ec;
    ec = admin.permission_module( d->m_aid, ADMIN_MODULE_PURCHASES, per_module);
    if (ec) return ec;
    
    
    Admin_permit_T pm(per_module.flag), mt(per_merchant.flag)  ;
    bool const allow_reverse =  ( pm.del != 0 && mt.del != 0  ) ;
    if (! allow_reverse )
    {
        slog.ErrorLog("not allowed!");
        return Error_operation_not_allowed;
    }


    Admin_info_T ad_info;
    ad_info.aid = d->m_aid;
    ec = admin.info(ad_info, ad_info);
    if (ec)
        return ec;
    
    if (ad_info.phone.empty())
    {
        slog.ErrorLog("no phone to admin");
        return Error_not_found;
    }
    //1. module = 4  can delete
    //2. merchant = p_info.merchant_id  can delete

    if (tp == 1)//start
    {
        //////////////////////////////////////////////////////////////////////////
        std::string code = oson::utils::generate_code( 7 );

        Purchase_reverse_info_T rev_info;
        rev_info.aid      = d->m_aid;
        rev_info.pid      = p_info.id;
        rev_info.sms_code = code;
        rev_info.phone    = ad_info.phone;
        rev_info.status   = 0;
        rev_info.ts_start = formatted_time_now("%Y-%m-%d %H:%M:%S");
        
        Purchase_reverse_T rev( oson_this_db  );
        rev.add(rev_info, rev_id);
        ///////////////////////////////////////////////////////////////////////
        d->m_writer << b8(rev_id);
        std::string msg = "Номер оплтаы: " + to_str(p_info.id ) + "\nКод подтверждения для отмены: " + code ;
        SMS_info_T sms(ad_info.phone, msg, SMS_info_T::type_business_reverse_sms ) ;
        
        oson_sms->async_send(sms);
        
        return Error_OK ;
    }
    else // a confirm
    {
        
        Purchase_reverse_info_T rev_info;
        Purchase_reverse_T rev( oson_this_db  );
        ec = rev.info(rev_id, rev_info);
        if (ec)
            return ec;
        
        if ( !( rev_info.status == 0 && rev_info.sms_code == code )  )
        {
            slog.ErrorLog("SMS code not verified");
            return Error_sms_code_not_verified;
        }
        
        rev_info.status = 3;//in- progress
        rev.upd(rev_info);
        
        
        oson_eopc -> async_trans_reverse(p_info.eopc_trn_id, purchase_reverse_session(d, p_info, rev_info) );
        
        return Error_async_processing ;
    }
}
static Error_T sverochnogo_list_tps(api_pointer_cref d, Purchase_search_T search, Sort_T sort )
{
    SCOPE_LOG(slog);
//    <txn_id> <дата и время> <идентификатор абонента> <сумма> 
//    ........................................................
//    <txn_id> <дата и время> <идентификатор абонента> <сумма> 
//    Total: <кол-во платежей> <общая сумма> 
    
    Purchase_list_T list;
    Purchase_T purch_table( oson_this_db ) ;
    
    Error_T ec = purch_table.list(search, sort, list);
    if (ec) return ec;
   
    ///////////////////////////////////////////////////////////////
    struct Sverochniy_header_T header ;
    typedef Sverochniy_header_T::node_t node_t;
    
    header.nodes.push_back(node_t("txn_id", "Транзакция идентификатор", "number"));
    header.nodes.push_back(node_t("date_time","дата и время", "datetime" ));
    header.nodes.push_back(node_t("account client", "идентификатор абонента", "text"));
    header.nodes.push_back(node_t("amount", "сумма", "number"));
    
     
     ///////////////////////////////////////////////
    std::string filename = "oson_TPS_" + search.from_date + ".txt";
    std::replace(filename.begin(), filename.end(), '-', '.');
    
    /*************************************************************/
    //0. filename
    d->m_writer << filename;
    
    
    //1. headers
    d->m_writer << b4(header.nodes.size());
    for(const node_t& nd : header.nodes )
    {
        d->m_writer << nd.field_name << nd.description << nd.format;
    }
    
    d->m_writer << b8(0); // terminator zero.
   
    //2. data
    d->m_writer << b4(list.count) << b4(list.list.size() )  << b8( list.sum);
    for(const auto& p: list.list )
    {
        d->m_writer << to_str(p.oson_paynet_tr_id) << p.ts << p.login <<  to_str(p.amount / 100 ) ; // TPS required amount as sum.
    }
    
    return Error_OK ;
}

static Error_T api_purchase_sverochnogo_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    ////////////////////////////////////////////////////////////
    Sort_T sort( 0, 0, Order_T( 5, 0, Order_T::DESC ) ) ;
    Purchase_search_T search;

    reader >> r8(search.id) >> r8(search.uid) >> r4(search.mID) >> r2(search.from_date) >> r2(search.to_date) >> r4( search.status ) 
           >> r4(sort.offset) >> r2(sort.limit);

    if (search.mID == merchant_identifiers::TPS_I_direct )
    {
        return sverochnogo_list_tps(d, search, sort);
    }
    
    if (search.mID != merchant_identifiers::Beeline)
    {
        slog.WarningLog("Support only Beeline sverochnogo list.");
        return Error_not_found;
    }
    
    Beeline_response_list_T beeline_list;
    Purchase_T purch( oson_this_db  );
    
    purch.make_beeline_merch_response(search, sort, beeline_list);
    
    //////    1. LOGIN       = "oson"
//////2. MSISDN      = purchases.login
//////3. AMOUNT      = purchases.amount div 100
//////4. CURRENCY    = 2. (1-dollar, 2- sum)
//////5. PAY_ID      = purchases.paynet_tr_id
//////6. RECEIPT_NUM = purchases.merch_resp[0]
//////7. DATE_STAMP  = purchases.merch_resp[1]
//////8. COMMIT_DATE = purchases.merch_resp[2]  
//////9. PARTNER_PAY_ID = purchases.id.
//////10. BRANCH = "OSON"
//////11.TRADE_POINT = "OSON"
    
    
    struct Sverochniy_header_T header;
    
    typedef Sverochniy_header_T::node_t node_t;
    
    header.nodes.push_back(node_t("LOGIN", "Логин Терминала", "text"));
    header.nodes.push_back(node_t("MSISDN", "Номер абонента", "text"));
    header.nodes.push_back(node_t("AMOUNT", "Сумма платежа в валюте транзакции", "number"));
    header.nodes.push_back(node_t("CURRENCY","Код валюты транзакции" , "number" ));
    header.nodes.push_back(node_t("PAY_ID", "Идентификатор транзакции", "text"));
    header.nodes.push_back(node_t("RECEIPT_NUM", "Номер платежного документа", "text"));
    header.nodes.push_back(node_t("DATE_STAMP", "Дата/Время создания транзакции", "datetime"));
    header.nodes.push_back(node_t("COMMIT_DATE", "Дата подтверждения транзакции", "datetime"));
    header.nodes.push_back(node_t("MAGIC_NUMBER", "Пустое поля пока незнаем", "number"));
    header.nodes.push_back(node_t("PARTNER_PAY_ID", "Уникальный номер транзакции в системе Партнера (до 255 символов)", "text"));
    header.nodes.push_back(node_t("BRANCH", "Идентификатор филиала", "text"));
    header.nodes.push_back(node_t("TRADE_POINT", "Код торговой точки", "text"));
    
    //ХХХ_ DD.MM.YYYY
    std::string filename = "oson_" + formatted_time_now("%d.%m.%Y");
    if ( ! search.from_date.empty() ){
        //2018-01-05  --> 05.01.2018
        int year, month, day;
        sscanf(search.from_date.c_str(), "%d-%d-%d", &year, &month, &day);
        char buf[64];
        snprintf(buf, 64, "oson_%02d.%02d.%04d", day, month, year);
        filename =  buf;
    }
    //////////////////////////////////////////////////////////////
    d->m_writer << filename;
    //1. header
    
    
    d->m_writer << b4(header.nodes.size());
    for(const node_t& nd : header.nodes )
    {
        d->m_writer << nd.field_name << nd.description << nd.format;
    }
    
    d->m_writer << b8(0); // terminator zero.
    
    
    //2. data
    int64_t total_sum = 0 ;
    d->m_writer << b4(beeline_list.total_count) << b4(beeline_list.list.size() ) << b8(total_sum) ;
    
    for(const Beeline_merch_response_T& b : beeline_list.list )
    {
        d->m_writer << b.login        << b.msisdn          << b.amount      <<  b.currency 
                    << b.pay_id       << b.receipt_num     << b.date_stamp  <<  b.commit_date
                    << b.magic_number << b.partner_pay_id  << b.branch      <<  b.trade_point;
    }
    
    /////////////////////////////////////////////////////////////
     
     return Error_OK ;
 }
 
static Error_T api_card_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    Card_info_T search;
    reader >> r8(search.id) >> r8(search.uid) >> r2(search.pan ) >> r2(search.owner_phone) ;
    
    Sort_T sort;
    reader >> r4(sort.offset) >> r2(sort.limit) ; 
    
    
    Cards_T cards( oson_this_db  );
    Card_list_T card_list;
    Error_T error = cards.card_list(search, sort, /*OUT*/card_list);
    if(error != Error_OK)
        return error;

    ///////////////////////////////////////////////////////////
    d->m_writer << b4(card_list.count) << b4(card_list.list.size());
    
    for(Card_info_T const& c : card_list.list)
    {
        d->m_writer << b8(c.id) << b8(c.uid) << b4(c.tr_limit) << b2(c.admin_block)
                    << c.pan << c.owner_phone ;
    }
    ////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_card_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint64_t id = reader.readByte8();
    
    Card_info_T info;
    info.tr_limit = reader.readByte4();
    info.admin_block = reader.readByte2();
    
    Cards_T card( oson_this_db );
    return card.card_admin_edit(id, info);
}

static Error_T api_card_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint64_t id = reader.readByte8();
    
    Cards_T card( oson_this_db );
    return card.card_delete(id);
}

 static Error_T  api_ums_sverka_start( api_pointer_cref d, ByteReader_T& reader )
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);
    
    std::string date;
    reader >> r2(date);
    if (date.empty()){
        slog.WarningLog("date is empty!");
        return Error_parameters;
    }
    //%Y-%m-%d 
    if (! oson::utils::is_iso_date(date) ) {
        slog.WarningLog("date is not valid( expected format : YYYY-MM-DD ). date = %.*s ", ::std::min<int>(128, date.length()), date.c_str());
        return Error_parameters;
    }
    
    auto app = oson_app ;
    app->start_ums_sverka(date.c_str());
//    Merchant_info_T merch_info;
//    Merch_acc_T acc;
//    Merchant_T merch_table(oson_this_db);
//    
//    Error_T ec  = Error_OK ;
//    
//    const int32_t merchant_id = merchant_identifiers::UMS_direct;
//    
//    merch_info = merch_table.get( merchant_id, ec);
//    
//    if (ec) return ec;
//    
//    
//    merch_table.api_info( (int32_t)merchant_api_id::ums, acc ) ;
//    
//    Merchant_api_T merch_api(merch_info, acc);
//    
//    Merch_trans_T trans;
//    trans.transaction_id = merch_table.next_transaction_id();
//    trans.param   = date;
//    Merch_trans_status_T response;
//    ec = merch_api.sverka_ums(trans, response);
//    if (ec) return ec;
//    
//    std::string sverka_id = response.merchant_trn_id ;
//    d->m_writer << sverka_id  ; 
//    
    return Error_OK ;
 }
 
 static Error_T  api_ums_sverka_result(api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader);
    
    
    std::string sverka_id;
   
    reader >> r2(sverka_id);
    if (sverka_id.empty()){
        slog.WarningLog("Empty sverka-id.");
        return Error_parameters;
    }
    
    
    Merchant_info_T merch_info;
    Merch_acc_T acc;
    Merchant_T merch_table(oson_this_db);
    
    Error_T ec  = Error_OK ;
    
    const int32_t merchant_id = merchant_identifiers::UMS_direct;
    
    merch_info = merch_table.get( merchant_id, ec);
    
    if (ec) return ec;
    
    
    //(void) merch_table.acc_info( merchant_id, acc);
    merch_table.api_info((int32_t)merchant_api_id::ums, acc );
    
    Merchant_api_T merch_api(merch_info, acc);
    
    Merch_trans_T trans;
    trans.param   = sverka_id;
    Merch_trans_status_T response;
    ec = merch_api.sverka_ums_result(trans, response);
    //if (ec) return ec;
    d->m_writer << response.merch_rsp ;
    
    return Error_OK ;
 }





static Error_T api_news_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    News_info_T search;
    search.id = reader.readByte4();
    Sort_T sort;
    sort.offset = reader.readByte4();
    sort.limit = reader.readByte2();
    
    News_T news( oson_this_db   );
    News_list_T list;
    news.news_list(search, sort, list);
    
    ////////////////////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    for(const News_info_T& n : list.list)
    {
        d->m_writer << b4(n.id) << n.msg << n.add_time ;
    }
    ///////////////////////////////////////////////////////
    return Error_OK;
}

static std::string push_notify_msg_fix_long(std::string msg)
{
    static const size_t   msg_max_size = 1576  ;
    
    if (msg.size() > msg_max_size)
    {
        size_t i = msg_max_size;
        
        //remove until space. char - may be signed or unsigned.
        while(  i > msg_max_size / 2 && ( (unsigned)msg[i]) > '\x20' )
            --i;
        
        msg.erase(i, msg.size());
        msg += "\n...\n";
    }
    
    return msg;
}

namespace
{
    
class sms_send_to_all: public std::enable_shared_from_this< sms_send_to_all >
{
public:
    typedef sms_send_to_all self_type;
    typedef std::shared_ptr< self_type > pointer;
    
    static pointer create( oson::news_in in ) 
    { 
        return std::make_shared< self_type>(in); 
    }
    
private:
    std::string msg;
    
    std::string where_s;
    
    Sort_T sort;
    
    boost::asio::deadline_timer m_timer;
    
public:
    
    explicit sms_send_to_all(oson::news_in in )
    : msg(in.message)
    , where_s( make_where(in) ) 
    , sort(0, 16, Order_T(1, 0, Order_T::ASC))
    , m_timer(  *(oson_sms -> io_service() ) )
    {
        SCOPE_LOGD(slog);
    }
    
    ~sms_send_to_all(){
        SCOPE_LOGD(slog);
    }
    
    
    void async_start()
    {
        async_wait( 2 );
    }
    
private:
    
    
    
    void async_wait(int seconds)
    {
        m_timer.expires_from_now(boost::posix_time::seconds(seconds)) ;
        m_timer.async_wait(std::bind(&self_type::start, this->shared_from_this())) ;
        
    }
    
    static std::string make_where( oson::news_in in )
    {
        std::string where_s = " ( sms_allow = 'TRUE' ) " ;
        
        //1. uids
        std::string uids = in.uids;
        boost::trim(uids);
        
        if (! uids.empty() ) where_s += " AND ( id IN ( " + uids + " ) ) " ;
        
        //2. language
        if (in.language != in.lang_all ) where_s += " AND (lang = " + escape(in.language) + ") " ;
        
        //3. operation system
        if (in.operation_system == in.os_android )
            where_s += "AND ( id IN (SELECT uid FROM user_devices WHERE uid=id and ( ( os = 'android' ) OR ( os IS NULL ) ) ) ) " ; 
        else if (in.operation_system == in.os_ios )
            where_s += "AND ( id IN (SELECT uid FROM user_devices WHERE uid=id and ( ( os = 'ios' ) OR ( os IS NULL ) ) ) ) " ;
        
        return where_s;
        
    }
    
    void start()
    {
        try
        {
            bool has = send();
            
            if (has){
                sort.offset += sort.limit;
                
                return async_wait( 1 );
            }
        }
        catch(std::exception& e)
        {
            
        }
    }
    
    
    bool send()
    {
        SCOPE_LOGD(slog);
        
        std::string sort_s = sort.to_string();
        std::string query = "SELECT id, phone FROM users WHERE " + where_s + sort_s;

        DB_T::statement st( oson_this_db   );
        st.prepare(query);

        if ( st.rows_count() == 0) 
            return false;


        std::string phones;
        int nrows = st.rows_count() ;
        for(int i = 0; i  < nrows; ++i){
            int64_t id;
            std::string p;
            st.row(i) >> id >> p;
            if ( ! phones.empty())
                phones += ';';
            phones += p;
        }

        SMS_info_T sms_info;
        sms_info.phone =  phones  ;
        sms_info.text = msg;
        sms_info.type = sms_info.type_bulk_sms ;
        

        oson_sms -> async_send(sms_info);

        return true;
    }
};
    
} // end noname namespace
 
namespace 
{
    

class push_notify_to_all: public std::enable_shared_from_this< push_notify_to_all > 
{
public:
    typedef push_notify_to_all self_type;
    typedef std::shared_ptr< self_type > pointer;
    
    static pointer create( oson::news_in in ) 
    { 
        return std::make_shared< self_type> (in )  ; 
    } 
    
private:
    std::string msg;
    std::string where_s;
    Sort_T sort;
    ////////////////////
    boost::asio::deadline_timer m_timer;
public:
    explicit push_notify_to_all( oson::news_in in )
    : msg( push_notify_msg_fix_long( in.message ) )
    , where_s( make_where(in) )
    , sort(0, 16, Order_T(0, 0)) 
    , m_timer( *(oson_push ->io_service()) )
    {
        SCOPE_LOGD(slog);
    }
    
    
    ~push_notify_to_all()
    {
        SCOPE_LOGD(slog);
    }
    
    void async_start()
    {
        async_wait( 2 );
    }
    
private:
    
    void async_wait(int seconds){
        m_timer.expires_from_now(boost::posix_time::seconds(seconds)) ;
        m_timer.async_wait( std::bind(&self_type::start, this->shared_from_this() ) ) ;
    }
    
    static std::string make_where(oson::news_in in)
    {
        std::string where_s;

        where_s = "(os IS NOT NULL) AND (notify_token IS NOT NULL) " ;

        std::string uids = in.uids;
        boost::trim(uids);
        
        if ( ! uids.empty() ) where_s += " AND ( uid in ( " + uids + " ) ) " ;

        if (in.language != in.lang_all )
        {
            where_s += " AND ( uid IN ( SELECT id FROM users u WHERE lang = " + escape(in.language) + " )  ) " ;
        }

        if (in.operation_system == in.os_android )
            where_s += " AND (os = 'android' ) " ;
        else if(in.operation_system == in.os_ios ) 
            where_s += " AND (os = 'ios' ) "  ;
        
        return where_s;
    }

   
    void start()
    {
       try
       {  
          bool has =  send( ); 
          
          if ( has ){
              
              sort.offset += sort.limit;
              
              return async_wait( 4 );
          }
       } 
       catch(std::exception& e)
       {}
       
       
    }
    
    
    bool send()
    {
        SCOPE_LOGD(slog);
        
        std::string sort_s =  "ORDER BY uid OFFSET  " + num2string(sort.offset) + " LIMIT  " + num2string(sort.limit) ;
        
        
        std::string query = " SELECT dev_id, uid,  notify_token, os, login_ts::timestamp(0)  FROM user_devices WHERE " + where_s + sort_s;

        DB_T::statement st( oson_this_db  );
        st.prepare(query);
        
        int rows = st.rows_count();
        
        if (!rows)
            return false;
        
        std::vector< Device_info_T > devices ;
        
        for(int i = 0; i < rows;++i){
            Device_info_T dev;
            
            st.row(i) >>dev.dev_id >> dev.uid >> dev.notify_token >> dev.os >> dev.login_ts;
            
            devices.push_back(dev);
        }
        oson_push -> async_send_push(devices, msg, MSG_TYPE_BULK_MESSAGE);

        return true;
    }
};

} // end noname namespace 

static Error_T api_news_add_v2(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD( slog );
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    oson::news_in in;
    
    reader >> r2(in.message) >> r4(in.type) >> r2(in.uids)  >> r4(in.language) >> r4(in.operation_system) ;
    
    bool const is_push = (in.type & in.type_push) != 0 ;
    bool const is_sms  = (in.type & in.type_sms)  != 0 ;
    
    if (is_push )
    {
        typedef push_notify_to_all session;
        session::pointer  s = session::create(in);
        s->async_start();
    }
    
    if ( is_sms && oson_opts -> sms.bulk_sms  ) //@Note: bulk sms are disabled. 
    {
        typedef sms_send_to_all session;
        session::pointer s = session::create(in);
        s->async_start();
    }
    
    if ( !is_push && ! is_sms ) // no push no sms
    {
        slog.WarningLog("Type is invalid!") ;

        return Error_parameters;
    }
    
    /************************************************/
    auto extract_uid = [](std::string s)-> int64_t
    {
        if ( s.empty() )
        {
            return 0;
        }
        
        int64_t u  = 0 ;
        
        int i = 0;
        
        while( i < (int)s.size() && ! isdigit( s[ i ] ) )
        {
            ++i;
        }
        
        if ( i >= (int)s.size() )
        {
            return 0;
        }
        
        while( i < (int)s.size() && isdigit( s[ i ] ) ) 
        {
            u = u * 10 + s[ i++ ] - '0' ;
        }
        
        return u;
    };
    /***************************************************/
    News_T table( oson_this_db ) ;
    
    News_info_T info;
    
    info.msg  = in.message;
    
    info.lang = in.language;
    
    info.uid  = extract_uid( in.uids ) ;
    
    table.news_add( info ) ;
    
    
    return Error_OK ;
}

static Error_T api_news_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint32_t id = reader.readByte4();

    News_info_T info;
    reader >> r2(info.msg);
    
    News_T news( oson_this_db );
    return news.news_edit(id, info);
}

static Error_T api_news_delete(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    const uint32_t id = reader.readByte4();
    
    News_T news( oson_this_db  );
    return news.news_delete(id);
}

static Error_T api_notification_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Sort_T sort; 
    reader >> r4(sort.offset) >> r2(sort.limit) ;
    
    Fault_list_T list;
    Fault_info_T search;
    Fault_T fault( oson_this_db  );
     
    Error_T error = fault.list(search, sort, list);
    if(error != Error_OK) {
        return error;
    }
    
    /////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    
    for(Fault_info_T const& fault : list.list)
    {
        d->m_writer << b4(fault.id) << b2(fault.type) << b2(fault.status) << fault.ts << fault.description;
    }
    ////////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_purchase_status(api_pointer_cref d, ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    int64_t trn_id, espp_trn_id;
    reader >> r8(trn_id) >> r8(espp_trn_id);
    
    if (trn_id == 0 && espp_trn_id == 0 ) {
        slog.WarningLog("Both trn-id and espp-trn-id are zero!");
        return Error_parameters;
    }
    
    if (espp_trn_id != 0 ) 
    {
        
        Merch_trans_T trans;
        trans.merch_api_params["ums_espp_trn_id"] = to_str(espp_trn_id);
        
        Merch_acc_T acc;
        Merchant_T merchant_table(oson_this_db);
        merchant_table.api_info((int32_t)merchant_api_id::ums, acc);

        Merchant_info_T merchant;
        merchant.id = merchant_identifiers::UMS_direct ;
        Merchant_api_T api(merchant, acc);

        Merch_trans_status_T response;

        api.pay_status(trans, response);


        d->m_writer << response.kv_raw << response.ts << response.merchant_trn_id << response.merch_rsp << b4(response.merchant_status) ;
        return Error_OK ;
    }
    
    
    Merch_trans_T trans;
    Purchase_info_T info;
    
    Purchase_T table( oson_this_db ) ;
    Error_T ec = table.info(trn_id, info);
    if (ec) return ec;
 
    trans.merch_api_params["ums_espp_trn_id"] = info.paynet_tr_id ;

    
    Merch_acc_T acc;
    Merchant_T merchant_table(oson_this_db);
    Merchant_info_T merchant = merchant_table.get(info.mID, ec);
    if (ec) return ec; 
    
    merchant_table.api_info(merchant.api_id, acc);
    
    Merchant_api_T api(merchant, acc);
    
    Merch_trans_status_T response;
    
    if (merchant.api_id == merchant_api_id::ums ) {
        
        api.pay_status(trans, response);
    
    } else {
        response.kv_raw           = info.status == TR_STATUS_SUCCESS ? "SUCCESS PAYMENT " : "FAILURE PAYMENT "; 
        response.ts               = info.ts            ;
        response.merchant_trn_id  = info.paynet_tr_id  ;
        response.merch_rsp        = info.merch_rsp     ;
        response.merchant_status  = info.paynet_status ;
    }
    
    d->m_writer << response.kv_raw << response.ts << response.merchant_trn_id << response.merch_rsp << b4(response.merchant_status) ;
    return Error_OK ;
}
/************************************** EWALLET OPERATIONS *********************************************/
 
Error_T api_topup_merchant_icon_add( api_pointer_cref d, ByteReader_T& reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_TOPUP_MERCHANTS, Admin_permit_T::EDIT_VALUE ) )  
    {
        slog.WarningLog("Permission denied a view. aid: %d", d->m_aid);
        return Error_access_denied ;
    }
    
    int32_t topup_id = 0;
    std::string image;
    
    reader >> r4(topup_id) >> r2(image);
    
    if (topup_id == 0 || image.empty()){
        slog.WarningLog("Unexpected parameters!");
        return Error_parameters;
    }
    
    Error_T ec = Error_OK ;
    oson::topup::table table(oson_this_db);
    oson::topup::info topup_info = table.get(topup_id, ec);
    
    if (ec) return ec;//NOT FOUND 
    
    oson::icons::content icon;
    icon.image = oson::utils::decodebase64(image);
    
    oson::icons::manager  manager;
    
    
    struct oson::icons::info icon_info  = manager.save_icon(icon, oson::icons::Kind::topup_merchant, /*current icon id*/ topup_info.icon_id ) ;
    
    
    if (icon_info.id == 0)
    {
        slog.WarningLog("Can't save icon!");
        return Error_internal;
    }
    
    
    if (icon_info.id  == topup_info.icon_id ) 
    {
        slog.WarningLog("icon not changed!");
        return Error_OK ;//nobody are changed.
    }
    
    
    table.edit_icon(topup_id, icon_info.id ) ; 
    
    
    if (topup_info.icon_id != 0 ) 
    {
        manager.remove_icon(topup_info.icon_id);
    }
    
    
    return Error_OK ;
}

Error_T api_topup_merchant_list (api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_TOPUP_MERCHANTS, Admin_permit_T::VIEW_VALUE ) )  
    {
        slog.WarningLog("Permission denied a view. aid: %d", d->m_aid);
        return Error_access_denied ;
    }
    
    struct oson::topup::search search;
    Sort_T sort;
    reader >> r4(search.id) >> r4(search.status) >> r4(sort.offset) >> r2(sort.limit) ;
    
    oson::topup::table table(  oson_this_db );
    
    auto list = table.list(search, sort);
    
    oson::icons::table icon_table(oson_this_db ) ;
     
    /***************************************************************/
    d->m_writer << b4(list.size()) << b4(list.size()) ;
    
    for(const auto& w : list )
    {
        std::string icon_path_hash ;
        if ( 0 != w.icon_id ){
            auto icon_info = icon_table.get(w.icon_id);
            icon_path_hash = icon_info.path_hash;
        }
        
        d->m_writer << b4(w.id ) << w.name << b4(w.status) << b4(w.option) << b8(w.min_amount)
                    << b8(w.max_amount) << b4(w.position) << b4(w.rate) << b8(w.card_id ) << icon_path_hash;
    }
    /***********************************************************************/
    return Error_OK;
 }
 
 Error_T api_topup_merchant_add  (api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_TOPUP_MERCHANTS, Admin_permit_T::ADD_VALUE ) ) 
    {
        slog.WarningLog("Permission denied to add. aid: %d", d->m_aid ) ;
        return Error_access_denied ;
    }
    
    struct oson::topup::info info;
    
    reader >> r2(info.name) >> r4(info.status) >> r4(info.option) >> r8(info.min_amount)
           >> r8(info.max_amount) >> r4(info.position) >> r4(info.rate) >> r8(info.card_id) ;
    
    oson::topup::table table{ oson_this_db } ;
    
    info.id = table.add(info);
    
    
    d->m_writer << b4(info.id) ;
    
    return Error_OK;
 }
 
 Error_T api_topup_merchant_edit  (api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_TOPUP_MERCHANTS, Admin_permit_T::EDIT_VALUE ) ) 
    {
        slog.WarningLog("Permission denied to edit. aid: %d", d->m_aid ) ;
        return Error_access_denied ;
    }
    
    
    struct oson::topup::info info;
    
    reader >> r4(info.id) >>  r2(info.name) >> r4(info.status) >> r4(info.option) >> r8(info.min_amount)
           >> r8(info.max_amount) >> r4(info.position) >> r4(info.rate) >> r8(info.card_id) ;
    
    
    oson::topup::table table(  oson_this_db ) ;
    
    Error_T ec = Error_OK ;
    struct oson::topup::info tmp_info = table.get(info.id, ec ) ;
    
    if ( ec ) return ec ;
    
    if (tmp_info.id != info.id){
        slog.ErrorLog("Not found!");
        return Error_not_found;
    }
    
    if (info.name.empty())
        info.name = tmp_info.name;
    
    if (info.status == 0)
        info.status = tmp_info.status;
    
    if(info.option == 0)
        info.option = tmp_info.option;
    
    if (info.min_amount == 0 )
        info.min_amount = tmp_info.option;
    
    if(info.max_amount == 0 )
        info.max_amount = tmp_info.max_amount;
    
    if (info.position == 0)
        info.position = tmp_info.position;
    
    if (info.rate == 0)
        info.rate = tmp_info.rate;
    
    if (info.card_id == 0)
        info.card_id = tmp_info.card_id;
    
    const bool equal = ( info.name       == tmp_info.name       ) && 
                       ( info.status     == tmp_info.status     ) && 
                       ( info.option     == tmp_info.option     ) &&
                       ( info.min_amount == tmp_info.min_amount ) && 
                       ( info.max_amount == tmp_info.max_amount ) && 
                       ( info.position   == tmp_info.position   ) &&
                       ( info.rate       == tmp_info.rate       ) && 
                       ( info.card_id    == tmp_info.card_id    ) ;
    
    if (equal)
    {
        slog.WarningLog("wallet info identical with database version. No update required!");
        return Error_OK ;
    }
    
    table.edit(info.id, info);
    
    return Error_OK;
 }
 
 Error_T api_topup_merchant_del   (api_pointer_cref d, ByteReader_T& reader)
 {
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_TOPUP_MERCHANTS, Admin_permit_T::DEL_VALUE ) ) 
    {
        slog.WarningLog("Permission denied to del. aid: %d", d->m_aid ) ;
        return Error_access_denied ;
    }
    
    int32_t id = 0 ;
    reader >> r4(id);
    
    
    oson::topup::table table(  oson_this_db ) ;
    
    table.del( id );
    
    return Error_OK;
 }

 Error_T api_qiwi_balance(api_pointer_cref d , ByteReader_T& reader)
 {
     SCOPE_LOGD(slog);
     OSON_PP_ADMIN_CHECK_LOGGED(d);
     
     int32_t api_id = 0;
     reader >> r4(api_id);
     
     const bool possible_balances = (api_id == merchant_api_id::qiwi) || (api_id == merchant_api_id::hermes_garant);
     if ( ! possible_balances){
         slog.WarningLog("Unknown api_id: %d", api_id);
         return Error_parameters;
     }
     
     if ( ! admin_has_module_permission( d->m_aid,  ADMIN_MODULE_BUSINESS_BALANCE  , Admin_permit_T::VIEW_VALUE ) ) {
         slog.WarningLog("Permission denied to view qiwi|hg balance: aid = %d", d->m_aid ) ;
         return Error_access_denied ;
     }
     
     Merch_acc_T acc ;
     
     Merchant_T table{ oson_this_db } ;
     
     table.api_info( api_id , acc ) ;
     
     Merchant_info_T info;
     info.api_id = api_id;
     
     
     
     Merchant_api_T mapi(  info, acc  );
     
     Merch_trans_T trans;
     Merch_trans_status_T status;
     
     mapi.get_balance(trans, status) ;
     
     
     const size_t nkv = status.kv.size() ;
      
     d->m_writer << b4( nkv  ) ;
     for(auto p : status.kv ) {
         d->m_writer << p.first << p.second ;
     }
     
     //d->m_writer << std::string("status-value" ) << to_str(status.merchant_status ) ;
     //d->m_writer << std::string("status-text" )  << status.merch_rsp ;
     
     
     return Error_OK ; 
 }
/******************************************************************************************************/
/*******************  BANK   **************************************************************************/
static Error_T api_bank_info( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;
    
    const uint32_t id = reader.readByte4();
    
    if ( ! admin_has_bank_permission(d->m_aid,  id , Admin_permit_T::VIEW_VALUE ) ) {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    

    Bank_T bank( oson_this_db  );
    Bank_info_T info;
    Error_T ec = bank.info(id, info);
    if (ec != Error_OK)
        return ec;
    std::string icon_path_link;
    if (info.icon_id != 0 )
    {
        oson::icons::table icon_table( oson_this_db ) ;
        auto icon_info = icon_table.get(info.icon_id);
        if(icon_info.id != 0 ) {
            icon_path_link = icon_info.path_hash;
        }
    }
    /////////////////////////////////////////////////
    d->m_writer << b4(info.id)          << info.name       << b8(info.min_limit) << b8(info.max_limit)
                << b4(info.rate)        << b2(info.port)   << info.merchantId    << info.terminalId
                << b8(info.month_limit) << info.offer_link << icon_path_link << b4(info.status)
                << info.bin_code ;
    
    //////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_bank_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_BANKS, Admin_permit_T::VIEW_VALUE ) ) 
    {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Sort_T sort;
    reader >> r4(sort.offset) >> r2(sort.limit) ;
    
    Bank_T bank( oson_this_db  );
    Bank_info_T search;//get all
    //search.status = 1;
    Bank_list_T list;
    Error_T ec = bank.list(search, sort, list);
    if (ec != Error_OK)
        return ec;

    //////////////////////////////////////////////////////
    d->m_writer << b4( list.count ) << b4( list.list.size() );

    for(Bank_info_T const& bank : list.list)
    {
        d->m_writer << b4(bank.id)    << bank.name     << b8(bank.min_limit) << b8(bank.max_limit)
                    << b4(bank.rate)  << b2(bank.port) << bank.merchantId    << bank.terminalId
                    << b4(bank.status) << bank.bin_code ;
    }
    /////////////////////////////////////////////
    return Error_OK;
}

static Error_T api_bank_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_BANKS, Admin_permit_T::ADD_VALUE ) ) 
    {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Bank_info_T info;
    reader >> r4(info.id)         >> r2(info.name)       >> r8(info.min_limit)   >> r8(info.max_limit)  >> r4(info.rate)      >> r2(info.port) 
           >> r2(info.merchantId) >> r2(info.terminalId) >> r8(info.month_limit) >> r2(info.offer_link) >> r2(info.bin_code)  >> r4(info.status);
    
    Bank_T bank( oson_this_db  );
    uint32_t bank_id = 0;
    bank.add(info, /*OUT*/ bank_id); //never get error.

    ///////////////////////////////
    d->m_writer << b8( bank_id);
    //////////////////////////////////
    
    return Error_OK ;
}

static Error_T api_bank_edit ( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;

    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_BANKS, Admin_permit_T::EDIT_VALUE ) ) 
    {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    Bank_info_T info;
    reader >> r4(info.id)         >> r2(info.name)       >> r8(info.min_limit)   >> r8(info.max_limit)  >> r4(info.rate)      >> r2(info.port) 
           >> r2(info.merchantId) >> r2(info.terminalId) >> r8(info.month_limit) >> r2(info.offer_link) >> r2(info.bin_code)  >> r4(info.status);
     
    int32_t const bank_id = info.id; 

    //check permissions
    Admin_T admin( oson_this_db  );
    Admin_permissions_T permissions;
    Error_T ec = admin.permission_bank( d->m_aid, bank_id, /*out*/permissions) ;
    if (ec) return ec;
    
    Admin_permit_T p( permissions.flag );
    if ( p.edit == 0 ) // no permission to edit
    {
        slog.ErrorLog("No permissions to edit the bank!");
        return Error_operation_not_allowed;
    }

    
    Bank_T bank( oson_this_db  );
    return bank.edit(info);
}

static Error_T api_bank_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);

    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ;
    
    if ( ! admin_has_module_permission(d->m_aid, ADMIN_MODULE_BANKS, Admin_permit_T::DEL_VALUE ) ) 
    {
        slog.ErrorLog("Access denied!");
        return Error_access_denied;
    }
    
    const uint32_t bank_id = reader.readByte4();
    if ( ! bank_id ){
        slog.ErrorLog("id of bank not set!");
        return Error_not_found;
    }
    
    //check permissions
    Admin_T admin( oson_this_db  );
    Admin_permissions_T permissions;
    Error_T ec = admin.permission_bank( d->m_aid, bank_id, /*out*/permissions) ;
    if (ec) return ec;
    
    Admin_permit_T p( permissions.flag );
    if ( p.del == 0 ) // no permission to delete
    {
        slog.ErrorLog("No permissions to delete the bank!");
        return Error_operation_not_allowed;
    }
    

    Bank_T bank( oson_this_db  );
    return bank.del(bank_id);
}

static Error_T api_bank_icon_add( api_pointer_cref d, ByteReader_T& reader) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint32_t id = reader.readByte4();
    
    std::string img = reader.readAsString(  reader.readByte4() );
 
    
    Bank_T bank( oson_this_db  );
    Bank_info_T bank_info;
    Error_T ec;
    
    ec = bank.info(id, bank_info);
    if (ec) return ec;
    
    oson::icons::content icon;
    icon.image = std::move(img);
    
    oson::icons::manager icon_manager;
    auto icon_info = icon_manager.save_icon( icon, oson::icons::Kind::bank , /*current icon id */ bank_info.icon_id );
    
    if (!icon_info.id ) {
        return Error_not_found;
    }
    
    if (bank_info.icon_id == icon_info.id ) 
    {
        slog.WarningLog("icon not changed!");
        return Error_OK ;
    }
    
    int64_t old_icon_id = bank_info.icon_id;
    bank_info.icon_id = icon_info.id;
    
    bank.edit_icon_id(bank_info.id, bank_info.icon_id );
    
    icon_manager.remove_icon(old_icon_id);
    
    return Error_OK ;
}


static Error_T api_bank_icon_get( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    const uint32_t id = reader.readByte4();
    
    Bank_T bank_table( oson_this_db   );
    Bank_info_T bank_info;
    Error_T ec = Error_OK ;
    ec = bank_table.info(id, bank_info);
    if (ec) return ec;
    
    oson::icons::content icon;
    oson::icons::manager manager;
    int loaded = manager.load_icon(bank_info.icon_id, icon);
    if ( ! loaded )
        return Error_not_found;
    
    ////////////////////////////////////
    d->m_writer << b4(icon.image  );
    ///////////////////////////////////
    return Error_OK;
 
}
    
/****************************************************************************************************************************************************/
/***********************************************         BANK CODE             **********************************************************************/
/****************************************************************************************************************************************************/
static Error_T api_bill_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN( d, reader ) ; 
    
    Error_T ec = Error_OK ;
    
    Bill_data_T input_data;
    reader >> r8(input_data.id) >> r8( input_data.amount ) >> r4( input_data.merchant_id ) >> r2(input_data.phone )
           >> r2(input_data.fields) >> r2(input_data.comment) ;

    input_data.status      = BILL_STATUS_REGISTRED;
   // input_data.add_ts      = formatted_time_now_iso_S();
    
    bool const push_enabled = reader.readByte2() != 0;
    ///////////////////////////////////////////////////////////////////////
    
    // fix +998...
    if ( ! input_data.phone.empty()  && input_data.phone[0] == '+' )
    {
        //remove first element
        input_data.phone.erase( 0, 1 );
    }
    
    // Search user
    User_info_T dst_user;
    
    if(input_data.phone.length() > 0) {
        Users_T users( oson_this_db  );
        dst_user = users.info(input_data.phone,  /*OUT*/ ec );
        if( ec != Error_OK )
            return ec;
        input_data.uid = dst_user.id;
    }

    Bills_T bills( oson_this_db  );
    int64_t bill_id = bills.add( input_data );

    if ( push_enabled ){
        if(dst_user.id != 0) {
            std::string msg = "Вам выставлен счет #" + num2string(bill_id);
            Users_notify_T users_n( oson_this_db  );
            users_n.notification_send(dst_user.id, msg, MSG_TYPE_REQUEST);
        }
    }

    //////////////////////////////////
    d->m_writer << b8(bill_id);
    /////////////////////////////////
    return Error_OK;
}
namespace{
    struct valid_id_list{
        bool is_digit(int c)const{ return (c >= '0' && c <='9'); }
        bool operator()(int c)const{ return  is_digit(c) || (c == ',') || (c == '\t') || (c == '\n') || (c == ' '); } 
    };
}

static Error_T api_bill_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_TOKEN_LOGIN(d, reader ) ;
    /////////////////////////////////////////////////////////////////////////
    int32_t            merchant_id   = reader.readByte4();
    
    int32_t            offset        = reader.readByte4();
    int32_t            limit         = reader.readByte4();
    std::string        merchant_list = reader.remainBytes() >= 4 ? reader.readAsString(reader.readByte4()) : "";
    ///////////////////////////////////////////////////////////////////
    /************  AUTHORIZE *****************/
    boost::trim(merchant_list) ;//delete unnecessary spaces.
    //CHECK it. it must be a  "16, 15, 27, 19"
    if ( ! boost::algorithm::all(merchant_list, valid_id_list() ) )
    {
        slog.ErrorLog("merchant_list invalid: '%.*s'", ::std::min<int>(1024, merchant_list.length()), merchant_list.c_str());
        return Error_internal;
    }
    /////////////////////////////////////////////////////////////////////////
    Bill_data_search_T search;
    search.merchant_id       = merchant_id;
    search.uid2              = 0; 
    search.merchant_id_list  = merchant_list;
    
    Sort_T sort(offset, limit);
    
    Bills_T bills( oson_this_db  );
    Bill_data_list_T bill_list = bills.list(search, sort );
 
    /////////////////////////////////////////////////
    d->m_writer << b4(bill_list.count) << b4(bill_list.list.size());
    for(const Bill_data_T& b : bill_list.list)
    {
        d->m_writer << b8(b.id)  << b8(b.amount) << b4(b.merchant_id) << b2(b.status)
                    << b.comment << b.add_ts ;
    }
    ///////////////////////////////////////////
    return Error_OK;
}

static Error_T api_bill_cancel( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    return Error_internal;
}

////////////////////////////////////////////////////////////////////////////
//                            BONUS CARD 
////////////////////////////////////////////////////////////////////////////
namespace
{
struct bonus_card_new_handler
{
    api_pointer d;

    response_handler_type rsp;

    Card_bonus_info_T bonus;

    explicit bonus_card_new_handler(api_pointer d, Card_bonus_info_T bonus)
     : d( d ), rsp(), bonus(bonus)
    {
        rsp.swap(d->m_response_handler); // take it
    }
    
    Error_T handle(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec)
    {
        SCOPE_LOG(slog);
        if(ec != Error_OK)
            return ec;
    
        if (out.status != VALID_CARD)
        {
            slog.ErrorLog("This card is blocked, for more information see status.");
            return Error_card_blocked;
        }
        
        bonus.number    = out.pan;
        bonus.owner     = out.fullname;
        bonus.pc_token  = out.id;
        bonus.balance   = out.balance;

        Cards_T cards( oson_this_db  );

        cards.bonus_card_add( bonus );

        ///////////////////////////////////////////
        d->m_writer << b8(bonus.card_id);
        //////////////////////////////////////////

        return Error_OK;        
    }
    //@Note, this code work another thread than adminapi
    void operator()(oson::backend::eopc::req::card const& in, oson::backend::eopc::resp::card const& out, Error_T ec)
    {
        SCOPE_LOGD(slog);

        ec = handle(in, out, ec);

        rsp.swap(d->m_response_handler);//back it again.
        
        return d->send_result( ec );
    }
};
} // end noname namespace

static Error_T api_bonus_card_add(api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    Card_bonus_info_T bonus;
    reader >> r2( bonus.number ) >> r2( bonus.expire ) >> r8( bonus.xid ) >> r2( bonus.name ) >> r2( bonus.password );
    
    oson::backend::eopc::req::card card = {  bonus.number, expire_date_rotate( bonus.expire ) } ;
    
    oson_eopc -> async_card_new( card,  bonus_card_new_handler(d, bonus)  ) ;
    return Error_OK ;
}

static Error_T api_bonus_card_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    const uint64_t card_id = reader.readByte8();
    
    Cards_T cards( oson_this_db );
    Card_bonus_list_T list;
    Error_T ec = cards.bonus_card_list(card_id, list);
    
    if (ec) return ec;
    
    ///////////////////////////////////////////////
    d->m_writer << b4(list.size()); 
    
    for(const Card_bonus_info_T& info : list )
    {
        
        d->m_writer << b8(info.card_id) << info.number  << info.expire << b8(info.xid)
                    << info.name        << b4(info.tpl) << info.owner ;
    }
    //////////////////////////////////////////
    return Error_OK;
}

static Error_T  api_bonus_card_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Card_bonus_info_T bonus;

    reader >> r8( bonus.card_id ) >> r2( bonus.number ) >> r2( bonus.expire ) >> r8( bonus.xid ) >> r2( bonus.name ) >> r2( bonus.password ) ;

    Cards_T cards( oson_this_db );
    
    return cards.bonus_card_edit(bonus.card_id, bonus.password, bonus);
}


static Error_T api_bonus_card_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED( d );

    uint64_t card_id = reader.readByte8();
    std::string passwd = reader.readAsString(reader.readByte2());
    
    Cards_T cards( oson_this_db  );
    
    return cards.bonus_card_delete(card_id, passwd);
}
    

///////////////////////////////////////////////////////////////////////////////////////////////////

static Error_T api_bonus_merchant_add( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Merchant_bonus_info_T info;

    reader >> r4(info.merchant_id) >> r8(info.min_amount) >> r4(info.percent) >> r8(info.bonus_amount) >> r2(info.start_date) >> r2(info.end_date)
           >> r2(info.description) >> r4(info.status) >> r2(info.longitude) >> r2(info.latitude) ;
    
    Merchant_T merch( oson_this_db  );
    
    merch.bonus_add(info, info.id);
    
    /////////////////////////////////////
    d->m_writer << b8(info.id);
    /////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_bonus_merchant_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Sort_T sort(0, 0, Order_T(2, 0, Order_T::ASC)); // order by merchant-id
    
    Merchant_bonus_info_T search;
    
    reader >> r8(search.id) >> r4(search.merchant_id) >> r2(search.start_date) >> r2(search.end_date) >> r4(sort.offset) >> r4(sort.limit) ;
    //search.status       = 1; //active
    
    Merchant_bonus_list_T list;
    Merchant_T merch( oson_this_db  );
    
    Error_T ec = merch.bonus_list(search, sort, list);
    
    if (ec) return ec;
    
    /////////////////////////////////////////////////
    d->m_writer << b4(list.total_count) << b4(list.list.size());
    
    for(Merchant_bonus_info_T& info  : list.list )
    {
        d->m_writer << b8(info.id)      << b4(info.merchant_id) << b8(info.min_amount) << b8(info.bonus_amount)
                    << b4(info.percent) << info.start_date      << info.end_date       << info.description
                    << b4(info.status)  << info.longitude       << info.latitude ;
    }
    ////////////////////////////////////////////////////////
    
    return Error_OK ;
}

static Error_T api_bonus_merchant_edit( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Merchant_bonus_info_T info;
    reader >> r4(info.id)         >> r4(info.merchant_id) >> r8(info.min_amount)  >> r4(info.percent) >> r8(info.bonus_amount) 
           >> r2(info.start_date) >> r2(info.end_date)    >> r2(info.description) >> r4(info.status ) 
           >> r2(info.longitude ) >> r2(info.latitude) ;
 
    Merchant_T merch( oson_this_db  );
    
    return merch.bonus_edit(info.id, info);
}

static Error_T api_bonus_merchant_delete( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    uint64_t id = reader.readByte8();
    
    if (0 == id){
        slog.WarningLog("id not set!");
        return Error_parameters;
    }
    
    Merchant_T merch( oson_this_db );
    
    return merch.bonus_delete(id);
}

static Error_T api_bonus_purchase_list( api_pointer_cref d, ByteReader_T& reader)
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    uint32_t status = reader.readByte4();
    std::string from_date = reader.readAsString( reader.readByte2() );
    std::string to_date = reader.readAsString(reader.readByte2() );
    
    Sort_T sort;
    sort.offset = reader.readByte4();
    sort.limit  = reader.readByte4();
    
    
    if ( ! from_date.empty() && from_date.find(':') == from_date.npos){
        from_date += " 00:00:00";
    }
    
    if ( ! to_date.empty() && to_date.find(':') == to_date.npos){
        to_date += " 23:59:59";
    }
    
    Purchase_T purch( oson_this_db  );
    Purchase_search_T search;
    Purchase_list_T list;
    search.status = status;
    search.from_date = from_date;
    search.to_date   = to_date;
    
    Error_T ec;
    
    ec = purch.bonus_list(search, sort, list);
    if (ec)
        return ec;
    
    ////////////////////////////////////////////////////////
    d->m_writer << b4(list.count) << b4(list.list.size());
    
    for(size_t i = 0; i < list.list.size(); ++ i)
    {
        const Purchase_info_T& p = list.list[i];
        d->m_writer << b8(p.id) << b8(p.uid)    << b4(p.mID) << p.login 
                    << p.ts     << b8(p.amount) << p.pan     << b4(p.status)
                    << b8(p.receipt_id)     << b8(p.card_id) << b8(p.bearns);
    }
    
    ////////////////////////////////////////////
    
    return Error_OK;
}

static Error_T api_error_codes_add( api_pointer_cref d, ByteReader_T&  reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    ///////////////////////////////////
    Error_info_T ei;
    reader >> r4(ei.value) >> r4(ei.ex_id) >> r2(ei.message_eng) >> r2(ei.message_rus) >> r2(ei.message_uzb) ;
    
    Error_Table_T table( oson_this_db  );
    ei.id = table.add( ei );
    
    d->m_writer << b4( ei.id );
    
    return Error_OK ;
}

static Error_T api_error_codes_list(api_pointer_cref d,  ByteReader_T&  reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    /////////////////////////////
    uint32_t id     = reader.readByte4();
    uint32_t offset = reader.readByte4();
    uint32_t limit  = reader.readByte4();
    //////////////////////////////
    Sort_T sort(offset, limit);
    Error_info_list_T ecs;
    Error_info_T search;   search.id = id;

    Error_Table_T table( oson_this_db  );
    table.list(search, sort, ecs);

    d->m_writer << b4(ecs.count) << b4(ecs.list.size());

    for(const Error_info_T& ec : ecs.list )
    {
        d->m_writer << b4(ec.id) << b4(ec.value) << b4(ec.ex_id) << ec.message_eng << ec.message_rus << ec.message_uzb ;
    }
    
    return Error_OK ;
}

static Error_T api_error_codes_edit( api_pointer_cref d,  ByteReader_T& reader )
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    Error_info_T info;
    reader >> r4(info.id) >> r4(info.value) >> r4(info.ex_id) >> r2(info.message_eng) >> r2(info.message_rus) >> r2(info.message_uzb);

    Error_Table_T table( oson_this_db  );
    
    return table.edit(info);
}

static Error_T api_error_codes_delete (api_pointer_cref d,  ByteReader_T&  reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);

    uint32_t id  = reader.readByte4();
    
    Error_Table_T table( oson_this_db   );
    
    table.del(id);
    
    return Error_OK ;
}
/******************************************************************************************************************************************************/
static Error_T api_get_user_auth_full_info (api_pointer_cref d,  ByteReader_T&  reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    Sort_T sort ;
    oson::Users_full_info search;
    
    reader >> r8(search.id) >> r8(search.uid) >> r4(search.status) >> r2(search.nationality) >> r2(search.citizenship)
           >> r4(sort.offset) >> r2(sort.limit) ;
    
    oson::Users_full_info_table table ( oson_this_db ) ;
    oson::Users_full_info_list list = table.list(search, sort);
    
    d->m_writer << b4(list.total_count)  << b4(list.list.size()) ;
    for(const oson::Users_full_info& info: list.list)
    {
        d->m_writer << b8( info.id )  << b8( info.uid ) << b4( info.status ) << b4( info.level ) << info.fio << info.date_of_birth << info.nationality << info.citizenship 
                    << info.passport_serial << info.passport_number << info.passport_start_date << info.passport_end_date << info.passport_image_path ;
    
    }
    
    return Error_OK ;
}

static Error_T api_edit_user_auth_full_info (api_pointer_cref d,  ByteReader_T&  reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    
    int64_t id = 0, uid = 0;
    int32_t status = 0, level = 0;
    reader >> r8(id) >> r8(uid) >> r4(status) >> r4(level); 
    
    if (id == 0 && uid == 0)
    {
        slog.WarningLog("Both id and uid are zero!");
        return Error_user_not_found;
    }
    
    oson::Users_full_info_table table( oson_this_db ) ;
    oson::Users_full_info search;
    
    search.id  = id; 
    search.uid = uid;
    Sort_T sort;
    oson::Users_full_info_list  list = table.list(search, sort ) ;
    
    if ( list.list.empty() ) {
        slog.WarningLog("Not found. id: %lld, uid: %lld", (long long)id, (long long)uid);
        return Error_user_not_found;
    }
    
    oson::Users_full_info info = list.list[ 0 ];
    
    bool can_edit = false;
    if ( status != 0  && status != info.status )
    {
        info.status = status;
        can_edit = true;
    }
    
    if (level != 0 && info.level != level)
    {
        info.level = level;
        can_edit = true;
    }
    
    if ( can_edit ) 
    {
        table.edit(info.id, info);
    }
    
    return Error_OK ;
}

static Error_T api_del_user_auth_full_info (api_pointer_cref d,  ByteReader_T&  reader ) 
{
    SCOPE_LOGD(slog);
    OSON_PP_ADMIN_CHECK_LOGGED(d);
    int64_t id = 0;
    reader >> r8(id);
    if (id == 0)
    {
        slog.WarningLog("id is zero !");
        return Error_not_found;
    }
    
    slog.WarningLog("Delete user full info is not allowed, yet!");
    return Error_OK ;
#if 0
    oson::Users_full_info_table table( oson_this_db ) ;
    
    int r = table.del(id);
    
    if ( ! r )
    {
        slog.WarningLog("id: %lld  not found !", (long long)id);
    }
    return Error_OK ;
#endif
}

/****************************************************************************************************************************************************/

namespace
{
    
template< int cmd >  struct command_dispatcher{   
    Error_T operator()(api_pointer_cref d, ByteReader_T& reader)const { return api_admin_null(d, reader); }
};

#define OSON_ADMIN_API_CMD(cmd_name, cmd_val, cmd_func)                       \
    template<> struct command_dispatcher< cmd_val >                           \
    {   enum{ cmd_value = cmd_val};                                           \
        Error_T operator()(api_pointer_cref d, ByteReader_T& reader)const     \
        { return cmd_func(d, reader) ; }                                      \
    };                                                                        \
    /****/

OSON_ADMIN_API_CMD( CMD_ADMIN_LOGGING           , 0  , api_logging_debug ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_AUTH              , 1  , api_admin_auth    ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_AUTH_TOKEN        , 2  , api_admin_auth_token ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_LOGOUT            , 3  , api_admin_logout)
OSON_ADMIN_API_CMD( CMD_ADMIN_LIST              , 5  , api_admin_list) 
OSON_ADMIN_API_CMD( CMD_ADMIN_ADD               , 6  , api_admin_add)
OSON_ADMIN_API_CMD( CMD_ADMIN_EDIT              , 7  , api_admin_edit ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_DELETE            , 8  , api_admin_delete ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_CHANGE_PASSWORD   , 9  , api_admin_change_password ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_PERMISSION_CHANGE , 11 , api_admin_permission_change ) 
OSON_ADMIN_API_CMD( CMD_ADMIN_PERMISSION_INFO   , 10 , api_admin_permission_info ) 

OSON_ADMIN_API_CMD(CMD_BUSINESS_QR_CODE, 19   , api_business_qr_code)
OSON_ADMIN_API_CMD(CMD_MERCHANT_QR_CODE, 18   , api_merchant_qr_code)
OSON_ADMIN_API_CMD(CMD_USERS_LIST,       20   , api_user_list)
OSON_ADMIN_API_CMD(CMD_USERS_EDIT,       22   , api_user_edit)
OSON_ADMIN_API_CMD(CMD_USERS_DELETE,     23   , api_user_delete)
OSON_ADMIN_API_CMD(CMD_USERS_BLOCK,      24   , api_user_block)
OSON_ADMIN_API_CMD(CMD_USERS_NOTIFY,     25   , api_user_notify)

OSON_ADMIN_API_CMD(CMD_USERS_BONUS_LIST, 26 , api_user_bonus_list)
OSON_ADMIN_API_CMD(CMD_USERS_BONUS_EDIT, 27 , api_user_bonus_edit)


OSON_ADMIN_API_CMD(CMD_TRANSACTION_LIST       , 30  , api_transaction_list)
OSON_ADMIN_API_CMD(CMD_TRANSACTION_REFNUM     , 31  , api_transaction_refnum_check)
OSON_ADMIN_API_CMD(CMD_TRANSACTION_STATISTICS , 35  , api_transaction_statistics)
OSON_ADMIN_API_CMD(CMD_TRANSACTION_TOP        , 36  , api_transaction_top)
OSON_ADMIN_API_CMD(CMD_TRANSACTION_DELETE     , 33  , api_transaction_delete)

OSON_ADMIN_API_CMD(CMD_USER_GENERATE_QR       , 28  , api_user_generate_qr)

OSON_ADMIN_API_CMD(CMD_MERCHANT_GROUP_LIST    , 40  , api_merchant_group_list)
OSON_ADMIN_API_CMD(CMD_MERCHANT_GROUP_ADD     , 41  , api_merchant_group_add)
OSON_ADMIN_API_CMD(CMD_MERCHANT_GROUP_EDIT    , 42  , api_merchant_group_edit)
OSON_ADMIN_API_CMD(CMD_MERCHANT_GROUP_DELETE  , 43  , api_merchant_group_delete)

OSON_ADMIN_API_CMD(CMD_MERCHANT_LIST,   45    , api_merchant_list)
OSON_ADMIN_API_CMD(CMD_MERCHANT_ADD,    46    , api_merchant_add)
OSON_ADMIN_API_CMD(CMD_MERCHANT_EDIT,   47    , api_merchant_edit)
OSON_ADMIN_API_CMD(CMD_MERCHANT_DELETE, 48    , api_merchant_delete)
OSON_ADMIN_API_CMD(CMD_MERCHANT_API,    49    , api_merchant_api_list);

OSON_ADMIN_API_CMD(CMD_MERCHANT_ICON_GET, 51  , api_merchant_icon_get)
OSON_ADMIN_API_CMD(CMD_MERCHANT_ICON_ADD, 50  , api_merchant_icon_add)

OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD,     55 , api_merchant_field_list)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_ADD, 56 , api_merchant_field_add)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_EDIT,57 , api_merchant_field_edit)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_DEL, 58 , api_merchant_field_delete)

OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_DATA_LIST, 60 , api_merchant_field_data_list)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_DATA_ADD,  61, api_merchant_field_data_add)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_DATA_EDIT, 62, api_merchant_field_data_edit)
OSON_ADMIN_API_CMD(CMD_MERCHANT_FIELD_DATA_DEL,  63, api_merchant_field_data_delete)

OSON_ADMIN_API_CMD(CMD_PURCHASE_LIST_ZIP, 64  , api_purchase_list_zip)
OSON_ADMIN_API_CMD(CMD_PURCHASE_LIST,    65   , api_purchase_list)
OSON_ADMIN_API_CMD(CMD_PURCHASE_DELETE,  66   , api_purchase_delete)
OSON_ADMIN_API_CMD(CMD_PURCHASE_STAT,    67   , api_purchase_stat)
OSON_ADMIN_API_CMD(CMD_PURCHASE_TOP,     68   , api_purchase_top)
OSON_ADMIN_API_CMD(CMD_PURCHASE_SVEROCHNOGO_LIST , 69, api_purchase_sverochnogo_list)


OSON_ADMIN_API_CMD(CMD_CARD_LIST,         80  , api_card_list)
OSON_ADMIN_API_CMD(CMD_CARD_EDIT,         82  , api_card_edit)
OSON_ADMIN_API_CMD(CMD_CARD_DELETE,       83  , api_card_delete)

OSON_ADMIN_API_CMD(CMD_UMS_SVERKA_START,  86,   api_ums_sverka_start);
OSON_ADMIN_API_CMD(CMD_UMS_SVERKA_RESULT, 87,   api_ums_sverka_result);

OSON_ADMIN_API_CMD(CMD_NEWS_LIST,         90  , api_news_list)
OSON_ADMIN_API_CMD(CMD_NEWS_ADD,          91  , api_news_add_v2)
OSON_ADMIN_API_CMD(CMD_NEWS_EDIT,         92  , api_news_edit)
OSON_ADMIN_API_CMD(CMD_NEWS_DELETE,       93  , api_news_delete)


OSON_ADMIN_API_CMD(CMD_NOTIFICATION_LIST, 100  , api_notification_list)

OSON_ADMIN_API_CMD(CMD_PURCHASE_STATUS,   102, api_purchase_status )

OSON_ADMIN_API_CMD(CMD_EWALLET_ICON, 104, api_topup_merchant_icon_add)
OSON_ADMIN_API_CMD(CMD_EWALLET_LIST, 105, api_topup_merchant_list)
OSON_ADMIN_API_CMD(CMD_EWALLET_ADD , 106, api_topup_merchant_add )
OSON_ADMIN_API_CMD(CMD_EWALLET_EDIT, 107, api_topup_merchant_edit)
OSON_ADMIN_API_CMD(CMD_EWALLET_DEL , 108, api_topup_merchant_del ) 
OSON_ADMIN_API_CMD(CMD_QIWI_BALANCE, 109, api_qiwi_balance)

OSON_ADMIN_API_CMD(CMD_BANK_LIST,         110  , api_bank_list)
OSON_ADMIN_API_CMD(CMD_BANK_ADD,          111  , api_bank_add)
OSON_ADMIN_API_CMD(CMD_BANK_EDIT,         112  , api_bank_edit)
OSON_ADMIN_API_CMD(CMD_BANK_DELETE,       114  , api_bank_delete)
OSON_ADMIN_API_CMD(CMD_BANK_INFO,         115  , api_bank_info)
OSON_ADMIN_API_CMD(CMD_BANK_ICON_ADD,     117  , api_bank_icon_add)
OSON_ADMIN_API_CMD(CMD_BANK_ICON_GET,     118  , api_bank_icon_get)


OSON_ADMIN_API_CMD(CMD_BILL_ADD,           130, api_bill_add)
OSON_ADMIN_API_CMD(CMD_BILL_LIST,          131 , api_bill_list)
OSON_ADMIN_API_CMD(CMD_BILL_CANCEL,        132 , api_bill_cancel)

OSON_ADMIN_API_CMD(CMD_BONUS_CARD_ADD    , 140 , api_bonus_card_add)
OSON_ADMIN_API_CMD(CMD_BONUS_CARD_LIST   , 141 , api_bonus_card_list)
OSON_ADMIN_API_CMD(CMD_BONUS_CARD_EDIT   , 142 , api_bonus_card_edit)
OSON_ADMIN_API_CMD(CMD_BONUS_CARD_DELETE , 143 , api_bonus_card_delete)

OSON_ADMIN_API_CMD(CMD_BONUS_MERCHANT_ADD    , 145 , api_bonus_merchant_add)
OSON_ADMIN_API_CMD(CMD_BONUS_MERCHANT_LIST   , 146 , api_bonus_merchant_list)
OSON_ADMIN_API_CMD(CMD_BONUS_MERCHANT_EDIT   , 147 , api_bonus_merchant_edit)
OSON_ADMIN_API_CMD(CMD_BONUS_MERCHANT_DELETE , 148 , api_bonus_merchant_delete)
OSON_ADMIN_API_CMD(CMD_BONUS_PURCHASE_LIST   , 149 , api_bonus_purchase_list)


OSON_ADMIN_API_CMD(CMD_ERROR_CODES_ADD    , 150  , api_error_codes_add)
OSON_ADMIN_API_CMD(CMD_ERROR_CODES_LIST   , 151  , api_error_codes_list)
OSON_ADMIN_API_CMD(CMD_ERROR_CODES_EDIT   , 152  , api_error_codes_edit)
OSON_ADMIN_API_CMD(CMD_ERROR_CODES_DELETE , 153  , api_error_codes_delete)

OSON_ADMIN_API_CMD(CMD_USER_AUTH_FULL_INFO_GET, 155, api_get_user_auth_full_info)
OSON_ADMIN_API_CMD(CMD_USER_AUTH_FULL_INFO_EDIT, 156, api_edit_user_auth_full_info)
OSON_ADMIN_API_CMD(CMD_USER_AUTH_FULL_INFO_DEL, 157, api_del_user_auth_full_info)


} // end noname namespace


static Error_T dispatch_command(int cmd, api_pointer_cref dptr, ByteReader_T& reader)
{
#define OSON_API_CASE_SWITCH(z,n,text)  case n : return command_dispatcher< n > () (dptr, reader);
 
    switch(cmd)
    {
        BOOST_PP_REPEAT(FUNC_COUNT, OSON_API_CASE_SWITCH, dptr )
        default: return api_admin_null(dptr, reader);        
    }
#undef OSON_API_CASE_SWITCH
}

#undef OSON_ADMIN_API_CMD 

