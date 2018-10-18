

#include <png.h>
#include <qrencode.h>

#include <fstream>
#include <sstream>

#include "admin.h"
#include "log.h"
#include "utils.h"
#include "png_image.h"
#include "DB_T.h"




Admin_info_T::integer Admin_info_T::to_flag(integer business, integer bus_login, integer bus_id, integer bill_form, integer bus_bank, integer bus_trans)
{
    return ( ( !! business   ) << 1  ) | //@Note:   !!a ==>  if(a != 0) 1  else 0 ;
           ( ( !! bus_login  ) << 2  ) |
           ( ( !! bus_id     ) << 3  ) |
           ( ( !! bill_form  ) << 4  ) |
           ( ( !! bus_bank   ) << 5  ) |
           ( ( !! bus_trans  ) << 6  ) ;

}

Admin_info_T::Admin_info_T() 
: aid( 0 )
, status( 0 )
, flag( 0 )
{
}

/*********************************/

Admin_list_T::Admin_list_T() 
 : count(0)
{
    
}

/***************************************/
Admin_permit_T:: Admin_permit_T(): view(0), add(0), edit(0), del(0){}
Admin_permit_T::Admin_permit_T(uint32_t flag){ from_flag(flag);}

uint32_t Admin_permit_T::to_flag()const
{ 
    //@Note:  !!  - converts any non zero value to 1, and zero to 0.
    return ( (!!view)  << 1  ) | 
           ( (!!add )  << 2  ) | 
           ( (!!edit)  << 3  ) | 
           ( (!!del )  << 4  ) ;

}

void Admin_permit_T::from_flag(uint32_t flag)
{
    view = (flag >> 1) & 0x01;
    add  = (flag >> 2) & 0x01;
    edit = (flag >> 3) & 0x01;
    del  = (flag >> 4) & 0x01;

}
/***********************************/

Admin_permissions_T::Admin_permissions_T(): aid(0), module(0), merchant(0), bank(0), flag(0) {}





/*******************************************************************************************************/
Admin_T::Admin_T(DB_T &db) : m_db(db)
{
}

Error_T Admin_T::login(Admin_info_T &info, bool &logged, uint32_t &aid)
{
    SCOPE_LOG( slog );
    logged = false;
    aid    = 0;
    
    std::string where_s = " ( login = " + escape(info.login) + ") AND ( password = crypt('" + info.password  + "', password ) ) " ;
    
    std::string query = "SELECT id, first_name, status, flag FROM admins WHERE " + where_s;

    DB_T::statement st(m_db);

    st.prepare(query);
     
    if ( st.rows_count() > 1           )   return Error_login_failed;
    if ( st.rows_count() == 0          )   return Error_OK;

    st.row(0) >> info.aid  >> info.first_name >> info.status >> info.flag ;
    
    aid = info.aid;
    
    if(info.status != ADMIN_STATUS_ENABLE) return Error_login_failed;
    
    query = "SELECT token FROM admin_online WHERE aid = " + escape(aid) ;
    
    st.prepare(query) ;
    
    std::string token = "";
    
    if (st.rows_count() == 1)
    {
        st.row(0) >> token; 
    } 
    else 
    {
        token = oson::utils::generate_token();
        query = "INSERT INTO admin_online ( aid, token) VALUES (" + escape( aid )+ ", " + escape( token ) + " ) " ;
    
        st.prepare(query);
    }
    
    info.token = token;
    logged = true;
    return Error_OK;
}


Error_T Admin_T::logged( const std::string  & token,  uint32_t &aid)
{
    SCOPE_LOG(slog);
    
    std::string query  = "SELECT aid FROM admin_online WHERE token = " + escape( token );
    
    aid = 0;
    
    DB_T::statement st(m_db);
    
    st.prepare( query );
    
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> aid;
    
    return Error_OK ;
}


Error_T Admin_T::logout( const std::string& token  )
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    
    std::string query = "DELETE FROM admin_online WHERE token = " + escape(token) ;
    
    st.prepare( query);
    
    return Error_OK ;
}

Error_T Admin_T::check_online()
{
    SCOPE_LOG(slog);
    
    DB_T::statement st(m_db);
    st.prepare(   "DELETE FROM admin_online WHERE ( last_ts < now() - INTERVAL '15 minute' ) ; "   );
    return Error_OK ;
}



static std::string make_where(const Admin_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.aid != 0 )  result += " AND (id = " + escape(search.aid) + " ) " ;
    result += "AND (status<> " + escape(ADMIN_STATUS_DELETED) + " ) " ;
    
    return result;
}

Error_T Admin_T::list(const Admin_info_T &search, const Sort_T& sort, Admin_list_T &alist)
{
    SCOPE_LOG(slog);
    std::string sort_str = sort.to_string(), where_str = make_where( search );
    //add login LIKE search.
    if ( ! search.login.empty() ) {
        std::string like_name = "%" + search.login + "%";
        where_str += " AND ( LOWER(login)  LIKE LOWER( " + escape(like_name) + " ) ) "  ;
        
    }
    
    std::string query    = " SELECT id, login, first_name, last_name, status, flag, phone FROM admins WHERE " + where_str + sort_str ;
    //;
    
    DB_T::statement st(m_db);
    
    
    st.prepare( query);
    
    int rows = st.rows_count();      
    alist.list.resize(rows); // a little optimization.
    
    for(int i = 0; i < rows; ++i)
    {
        Admin_info_T& info = alist.list[i];
        st.row(i) >> info.aid >> info.login >> info.first_name >> info.last_name >> info.status >> info.flag >> info.phone ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0)
    {
        alist.count  = total_cnt;
        return Error_OK ;
    }
    
    //@Note: optimize it!!
    query = "SELECT count(*) FROM admins WHERE " + where_str;
    
    
    
    st.prepare(query);
    
    st.row(0) >> alist.count;
    
    return Error_OK;
}


Error_T Admin_T::info(const Admin_info_T &search, Admin_info_T &data)
{
    SCOPE_LOG(slog);
    std::string where_s = make_where(search);
    std::string query = " SELECT id, login, first_name, last_name, status, flag, phone FROM admins WHERE " + where_s;
    
    DB_T::statement st(m_db);
    
    
    st.prepare(query);
    
    int rows = st.rows_count();
    
    if (rows != 1) return Error_not_found;
    
    st.row(0) >> data.aid >> data.login >> data.first_name >> data.last_name >> data.status >> data.flag >> data.phone;
    
    return Error_OK;
}


Error_T Admin_T::add(Admin_info_T &data)
{
    SCOPE_LOG(slog);
    std::string query = 
            "INSERT INTO admins (login, password, first_name, last_name, status, flag, phone) VALUES ("
            + escape(data.login)      + ", "
            + "crypt( '" + data.password + "', gen_salt('md5') ) , "
            + escape(data.first_name) + ", "
            + escape(data.last_name)  + ", "
            + escape(data.status)     + ", " 
            + escape(data.flag)       + ", "
            + escape(data.phone )     + " ) RETURNING id " ;
    
    DB_T::statement st(m_db);
    
    
    st.prepare(query);
    
    st.row(0) >> data.aid ;
    return Error_OK ;
}

Error_T Admin_T::edit(uint32_t id, const Admin_info_T &data)
{
    SCOPE_LOG(slog);
    
    std::string pwd ;
    
    if ( ! data.password.empty())
        pwd = "password = crypt( '" + data.password + "' , gen_salt('md5')), ";
    
    std::string query = 
            " UPDATE admins SET "
            " login      = " + escape(data.login)      + ", "
            + pwd +  
            " first_name = " + escape(data.first_name) + ", "
            " last_name  = " + escape(data.last_name)  + ", "
            " status     = " + escape(data.status)     + ", "
            " flag       = " + escape(data.flag )      + ", "
            " phone      = " + escape(data.phone )     + "  "
            " WHERE id   = " + escape(id);

    DB_T::statement st(m_db);
    
    st.prepare( query  );
    
    return Error_OK ;
}

Error_T Admin_T::del(uint32_t id)
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    std::string query = "UPDATE admins SET status = " + escape(ADMIN_STATUS_DELETED) + " WHERE id = " + escape(id) ;
    
    st.prepare(query);
    return Error_OK ;
}

void Admin_T::permissions_add(const std::vector< Admin_permissions_T >& permission_list)
{
    SCOPE_LOG(slog);
    
    slog.InfoLog("permission-list-size: %zu ", permission_list.size() ) ;
    
    if (permission_list.empty())
        return ;//there no permissions.
    
    const size_t chunk_size = std::min<size_t>(64, permission_list.size());
    const size_t chunk_number = ( permission_list.size() + chunk_size - 1) / chunk_size ;
    
    for(size_t ichunk = 0; ichunk != chunk_number; ++ichunk ) 
    {
        
        slog.InfoLog("chunk-#: %zu " , ichunk ) ;
         
        
        std::string query ;

        query = "INSERT INTO admin_permissions (aid, modules, merchant, bank,  permit_flag) VALUES "  ;

        const size_t start_pos = ichunk * chunk_size ;
        const size_t end_pos   = std::min< size_t > ( permission_list.size(), start_pos + chunk_size ) ;
        
        for(size_t i = start_pos ; i != end_pos ; ++i)
        {
            const Admin_permissions_T& p = permission_list[i];
            if( i > start_pos )
                query += ',';

            query += '(' ;

            query += to_str( static_cast< long long >( p.aid ) ); 
            query += ',';

            query += to_str( static_cast< long long >( p.module ) ); // '0'  3 symbols, ',' - 1
            query += ',';

            query += to_str( static_cast< long long > (p.merchant) );
            query += ',';

            query += to_str( static_cast< long long >( p.bank ) );
            query += ',';


            query += to_str( static_cast<  long long > ( p.flag) );
            query += ' ';

            query += ')';
        }

        DB_T::statement st(m_db);
        st.prepare(query);
    }
}

Error_T Admin_T::permissions_add(const Admin_permissions_T &permission)
{
    SCOPE_LOG(slog);
    
    std::string query = 
            "INSERT INTO admin_permissions (aid, modules, merchant, bank,  permit_flag) VALUES ("
            + escape(permission.aid)      + ", "
            + escape(permission.module)   + ", "
            + escape(permission.merchant) + ", "
            + escape(permission.bank)     + ", "
            + escape(permission.flag)     + ") "
            ;
    
    DB_T::statement st(m_db);
    // (query.c_str(), query.size());
    
    st.prepare(query);
    
    return Error_OK ;
}

Error_T Admin_T::permissions_del(uint64_t aid)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM admin_permissions WHERE ( aid = "+ escape(aid) + " ); " ;
    //(query.c_str(), query.size());

    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Admin_T::permissions_merchant_ids(uint64_t aid, std::string& ids)
{
    SCOPE_LOG(slog);
    
    std::string where_s = "( p.aid = " + escape(aid) + ") AND ( p.merchant <> 0 ) AND ( p.permit_flag <> 0 ) " ;
    std::string query = " SELECT  string_agg( p.merchant ::text, ',') FROM admin_permissions p WHERE " + where_s ;
    
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    st.row(0) >> ids;
    
    return Error_OK;
}

Error_T Admin_T::not_permissions_merchant_ids(uint64_t aid, std::string& ids)
{
    SCOPE_LOG(slog);

    std::string query = " SELECT string_agg( m.id ::text , ',') "
                        " FROM merchant m "
                        " WHERE NOT EXISTS( SELECT 1 "
                        "   FROM admin_permissions p "
                        "   WHERE p.aid = " + escape(aid) + 
                        "  AND p.merchant = m.id AND p.permit_flag <> 0 ) " ;
    
    
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    st.row(0) >> ids;
    
    return Error_OK;
    
}
int32_t Admin_T::search_by_login(std::string const& login  ) 
{
    SCOPE_LOG(slog);
    std::string query = " SELECT id FROM admins WHERE login = " + escape(login )  ;
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() == 0 ) {
        return 0;
    }
    
    int32_t id= 0;
    st.row(0) >> id;
    slog.InfoLog("id = %d", id);
    return id;
}


Error_T Admin_T::permissions_list(uint64_t aid, std::vector<Admin_permissions_T> &permissions)
{
    SCOPE_LOG(slog);
    std::string query = " SELECT aid, modules, merchant, bank,  permit_flag FROM admin_permissions WHERE aid = " + escape(aid);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();

    permissions.resize(rows);

    for(int i = 0; i < rows; ++i)
    {
        Admin_permissions_T& info = permissions[i];
        st.row(i) >> info.aid >> info.module >> info.merchant >> info.bank >> info.flag;
    }

    return Error_OK;
}

Error_T Admin_T::permission_module(uint64_t aid, uint32_t module,  /*OUT*/ Admin_permissions_T& per)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT aid, modules, merchant, bank, permit_flag FROM admin_permissions WHERE aid = "  + escape(aid) + " AND modules = " + escape(module) ; 
    
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> per.aid >> per.module >> per.merchant >> per.bank >> per.flag ;
    
    return Error_OK ;
}

Error_T Admin_T::permission_merch(uint64_t aid, uint32_t merch_id, /*OUT*/Admin_permissions_T& per)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT aid, modules, merchant, bank, permit_flag FROM admin_permissions WHERE aid = " + escape(aid) + " AND merchant = " + escape(merch_id) ; 
    
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> per.aid >> per.module >> per.merchant >> per.bank >> per.flag ;
    
    return Error_OK ;
    
}
Error_T Admin_T::permission_bank( uint64_t aid, uint32_t bank_id,  /*OUT*/Admin_permissions_T& per)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT aid, modules, merchant, bank, permit_flag FROM admin_permissions WHERE aid = " + escape(aid) + " AND bank = " + escape(bank_id) ; 
    
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> per.aid >> per.module >> per.merchant >> per.bank >> per.flag ;
    
    return Error_OK ;
    
}
    


Error_T Admin_T::change_password(uint32_t aid, const std::string & old_password, const std::string& new_password)
{
    SCOPE_LOG(slog);
    std::string aid_s = escape(aid);
    
    std::string query = "SELECT 1 FROM admins WHERE id = " + aid_s + " AND password = crypt( '" + old_password + "', password )" ; 

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1) return Error_not_found;
    
    
    query = "UPDATE admins SET password = crypt( '" + new_password + "', gen_salt('md5')) WHERE id = " +  aid_s;
    

    st.prepare(query) ;
    
    return Error_OK;
}


Error_T Admin_T::generate_bill_qr_img(uint64_t bill_id, std::string& img_data)
{
    SCOPE_LOG(slog);
    enum{QR_SIZE = 8};

    // prefix 'business_bill_add:' need for determine.
    const std::string qr_token = "business-bill:" + num2string(bill_id);
    
    Error_T error;
    
    //2. make qrcode from qr_token
    QRcode *qrcode;
    int version = 5;
    int casesensitive = 1;
    QRecLevel level = QR_ECLEVEL_H;
    QRencodeMode hint = QR_MODE_8;
    slog.DebugLog("Encode: %s", qr_token.c_str());

    qrcode = QRcode_encodeString(qr_token.c_str(), version, level, hint, casesensitive);
    if(qrcode == NULL) {
        slog.ErrorLog("Can't create qrcode");
        return Error_internal;
    }
    
    //scoped free.
    struct qrcode_free_t{ QRcode* qrcode; ~qrcode_free_t(){QRcode_free(qrcode);} } qrcode_free_e = {qrcode};
    
    //3. make PNG file from qrcode.
    static const std::string img_location = "/etc/oson/img/qr/";
    
    PNG_image_T qr_image;
    // Red: 47, Green: 60, blue143, alpha: 255
    png_byte base_color[4] = {47, 60, 143, 255};//{5, 87, 152, 255};

    error = qr_image.fill_qr( qrcode->width, qrcode->data, QR_SIZE, base_color);
    if(error != Error_OK) {
        slog.ErrorLog("Failed to generate qr_omage");
        return error;
    }

    // Set angle
    std::string qr_angle_file = img_location + "qr8_angle.png";
    PNG_image_T angle_image;
    //  Reader qr_angle_file to angle_image content.
    error = angle_image.read_file(qr_angle_file);
    if(error != Error_OK) {
         slog.ErrorLog("Failed to read angle image");
        return error;
    }

    error = qr_image.set_qr_angle(angle_image, qrcode->width, QR_SIZE);
    if(error != Error_OK) {
        slog.ErrorLog("Failed to set angle for qr image");
        return error;
    }

    // Set ico
    std::string ico_file = img_location + "oson_ico.png";
    PNG_image_T ico_image;
    error = ico_image.read_file(ico_file);
    if(error != Error_OK) {
        slog.ErrorLog("Failed to read angle image");
        return error;
    }
    error = qr_image.add_top_image(ico_image, 0);
    if(error != Error_OK) {
        slog.ErrorLog("Failed to set angle for qr image");
        return error;
    }

    
    const std::string location = img_location +"bill_temp_" + num2string(bill_id) +  ".png";

    error = qr_image.write_file(location);
    if(error != Error_OK) {
        slog.ErrorLog("Failed to save png file \"%s\"", location.c_str());
        return error;
    }
    
    //read from file
    {      
        std::ifstream fin(location.c_str(), std::ios::in | std::ios::binary);
        std::ostringstream oss;
        oss << fin.rdbuf();
        img_data = std::string(oss.str());
        
        //remove file, because it no more needed.
        int ret = ::std::remove(location.c_str());
        if ( ret != 0 )//error
        {
            slog.WarningLog("can't remove %s file\n", location.c_str());
        }
    }
    
    return Error_OK;
}
