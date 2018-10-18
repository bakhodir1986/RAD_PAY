
#include <sstream>
#include <png.h>
#include <qrencode.h>


#include <fstream>
#include <ctime>
#include <map>
#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>

#include "Merchant_T.h"
#include "log.h"
#include "png_image.h"
#include "DB_T.h"
#include "utils.h"

/******************************************************************************************/
 Currency_info_T::Currency_info_T()
    : id( 0 )
    , initialized( false )
    , usd_uzs( 0.0 )
    , usd_rub( 0.0 )
    , usd_eur( 0.0 )
    , type(  1 )  // by default always get Uzbekistan Center bank.
    {}
     
double Currency_info_T::usd(double amount_uzs_tiyin ) const  
{
    if ( ! initialized || usd_uzs < 1.0E-6 ) return 0.0;

    double usz_sum = amount_uzs_tiyin / 100.0;
    return usz_sum / usd_uzs;
}

double Currency_info_T::rub( double amount_uzs_tiyin )const{
    if ( ! initialized || usd_rub < 1.0E-6 ) return 0.0;
    double usz_sum = amount_uzs_tiyin / 100.0;
    return usz_sum / usd_rub ;
    //return usd(amount_uzs_tiyin) /  usd_rub ;
}

double Currency_info_T::eur(double amount_uzs_tiyin ) const{
    if (! initialized || usd_eur < 1.0E-6 ) return 0.0;

    double usz_sum = amount_uzs_tiyin / 100.0;

    return usz_sum / usd_eur;
    //return usd(amount_uzs_tiyin ) / usd_eur ;
}

Currencies_table::Currencies_table(DB_T& db) :m_db(db){}

Currencies_table::~Currencies_table()
{}
    
Currency_info_T Currencies_table::get(int type)const
{
    SCOPE_LOGD(slog);
    Currency_info_T currency;
    std::string query = " SELECT id, usd_uzs, usd_rub, usd_eur, upd_ts, type FROM currencies "
                        " WHERE  ( upd_ts >= current_date ) AND ( type = " + escape( type ) + " ) "
                        " ORDER BY upd_ts DESC LIMIT 1 ; " ;
    DB_T::statement st(m_db);
    st.prepare(query);
    
    if (st.rows_count() == 1)
    {
        std::string upd_ts;
        st.row(0) >> currency.id >> currency.usd_uzs >> currency.usd_rub >> currency.usd_eur >> upd_ts >> currency.type ;

        currency.initialized = true;
    }
    
    return currency ;
}

Currency_info_T Currencies_table::last(int type)const
{
    SCOPE_LOGD(slog);
    Currency_info_T currency ;
    return currency;
}

void Currencies_table::update(const Currency_info_T& currency)
{
    SCOPE_LOGD(slog);
    
    std::string query = "UPDATE currencies SET "
                        "  usd_uzs = " + escape_d( currency.usd_uzs  ) + 
                        ", usd_rub = " + escape_d( currency.usd_rub  ) + 
                        ", usd_eur = " + escape_d( currency.usd_eur  ) +
                        ", type    = " + escape( currency.type       ) +
                        ", upd_ts  = NOW() " 
                        " WHERE id = " + escape( currency.id )  ;

    DB_T::statement st(m_db);
    st.prepare(query);
}

int32_t Currencies_table::add(const Currency_info_T& currency)
{
    SCOPE_LOGD(slog);
    
    std::string query = "INSERT INTO currencies (id, usd_uzs, usd_rub, usd_eur, upd_ts, type) VALUES ( DEFAULT, "  
                        + escape_d(currency.usd_uzs) + ", " 
                        + escape_d(currency.usd_rub) + ", " 
                        + escape_d(currency.usd_eur) + ", "
                        " NOW() , " 
                        + escape(currency.type) + " ) RETURNING id ; " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int32_t id = 0;
    if (st.rows_count() == 1){
        st.row(0) >> id;
    }
    return id;
}
/****************************************************************************************/
/****************************************************************************************/
std::string Merchant_info_T::make_icon_web_path(int32_t id, const std::string& file_path ) 
{
  std::string id_str = to_str(id);
  std::string last_modified = to_str( oson::utils::last_modified_time(file_path) ) ;  
  std::string link = "merchant_icon_" + oson::utils::md5_hash("merchant_icon_" + id_str + "_" + last_modified ) ;
  return link;  
}

std::string Merchant_info_T::make_icon_link(int32_t id, const std::string & file_path)
{
    std::string link  = make_icon_web_path( id, file_path ) ;
  
    return oson::utils::bin2hex( link ) + ".png";
}

 

Merchant_info_T::Merchant_info_T() 
: id(0)
, group(0)
, status(0)
, min_amount(0)
, max_amount(0)
, port(0)
, external(0)
, extern_service()
, bank_id(0)
, rate(0)
, rate_money(0)
, position(0)
, api_id(0)
{
}

Merchant_info_T::bigint Merchant_info_T::commission(bigint amount)const
{
    //rate - is 100 multiply percentage, so need divide 100*100 = 10000,  and add it rate_money
    return  (int64_t) ( rate / 10000.0 * amount + 0.5) + rate_money;
    
}

    // merchant max|min_amount given in 'sum'. amount -given in TIYIN. So need multiple 100  the min|max_amounts.
Error_T Merchant_info_T::is_valid_amount(bigint amount)const
{
    if (min_amount != 0 && amount < min_amount * 100 )
        return Error_amount_is_too_small ;
    
    if (max_amount != 0 && amount > max_amount * 100 )
        return Error_amount_is_too_high ;
    
    return Error_OK ;
}

bool Merchant_info_T::commission_subtracted()const
{
    typedef merchant_identifiers  idfs;
    
    return idfs::is_webmoney ( id ) ||  idfs::is_nativepay(id) ||   // by old api
            api_id == merchant_api_id::qiwi  ||
            api_id ==   merchant_api_id::hermes_garant   ; // a by new api.
            
}


Merchant_group_T::Merchant_group_T(DB_T &db): m_db(db)
{
}
Merchant_group_T::~Merchant_group_T(){}

Error_T Merchant_group_T::add(const Merch_group_info_T & data)
{
    SCOPE_LOG(slog);
    std::string query = "INSERT INTO merchant_group (id, name, name_uzb, position) VALUES ( DEFAULT, " 
                        + escape(data.name)     + ", " 
                        + escape(data.name_uzb) + ", "
                        + escape(data.position) +  
                        " ) RETURNING id " ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Merchant_group_T::edit(uint32_t id, const Merch_group_info_T &info)
{
    SCOPE_LOG(slog);
    
    std::string query = "UPDATE merchant_group SET "
                        "  name     = " + escape(info.name) 
                      + ", name_uzb = " + escape(info.name_uzb)
                      + ", icon_id  = " + escape(info.icon_id) 
                      + ", position = " + escape(info.position)
                      + ", icon_path= " + escape(info.icon_path)
                      + "  WHERE id = " + escape(id) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    return Error_OK ;
}

Error_T Merchant_group_T::del(uint32_t id)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM merchant_group WHERE id = "+ escape(id) ;
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK;
}

Error_T Merchant_group_T::list(uint32_t id, std::vector<Merch_group_info_T> &group_list)
{
    SCOPE_LOG(slog);
    std::string where_s = " (1 = 1)" ;
    if ( id != 0 )   
        where_s +=  " AND  id = " + escape( id )   ; 
    std::string query = "SELECT id, name, name_uzb, icon_id, position, icon_path FROM merchant_group WHERE " + where_s + " ORDER BY position ";
    
    
    DB_T::statement st(m_db);
    st.prepare(query) ;
    
    int rows = st.rows_count();
    group_list.resize(rows);
    for(int i = 0; i < rows; ++i)
    {
        Merch_group_info_T& group = group_list[i];
        st.row( i ) >> group.id >> group.name >> group.name_uzb >> group.icon_id >> group.position >> group.icon_path ;
    }
    return Error_OK;
}

Merchant_T::Merchant_T(DB_T &db): m_db(db)
{
   
}

Error_T Merchant_T::add(const Merchant_info_T &info)
{
    SCOPE_LOG(slog);
    
    std::string query;
    DB_T::statement st(m_db);
    
    std::string contract_date = "NULL", position = "DEFAULT";
    
    if (! info.contract_date.empty() )
        contract_date = escape(info.contract_date) ;
    
    if (info.position != 0)
        position = escape(info.position);
    
    query = "INSERT INTO merchant (name, group_id, url, status, inn, mfo, ch_account, contract, contract_date, min_amount, max_amount, merchant_id, terminal_id , "
            " port, bank_id, external, ext_service_id, rate, rate_money, position, api_id ) VALUES ( " 
            + escape(info.name )            + ", "
            + escape(info.group)            + ", "
            + escape(info.url)              + ", "
            + escape(info.status)           + ", "
            + escape(info.inn)              + ", "
            + escape(info.mfo)              + ", "
            + escape(info.checking_account) + ", "
            + escape(info.contract)         + ", "
            + contract_date                 + ", "
            + escape(info.min_amount)       + ", "
            + escape(info.max_amount)       + ", "
            + escape(info.merchantId)       + ", "
            + escape(info.terminalId)       + ", "
            + escape(info.port)             + ", "
            + escape(info.bank_id)          + ", "
            + escape(info.external)         + ", "
            + escape(info.extern_service)   + ", "
            + escape(info.rate)             + ", "
            + escape(info.rate_money)       + ", "
            + position                      + ", "  
            + escape(info.api_id)           + " ) "
            ;
    
    st.prepare(query);
    return Error_OK ;
    
}

Error_T Merchant_T::edit(const Merchant_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query;
    DB_T::statement st(m_db);
    
    std::string contract_date = " contract_date ", position = " position ", api_id = " api_id "; // itself from DB.
    
    if (info.contract_date.length() > 0 ) 
        contract_date =  escape(info.contract_date) ;
    
    if (info.position != 0)
        position = escape(info.position);
    
    //@Note: merchant MUST be have a api_id!
    if (info.api_id != 0 ) 
        api_id = escape(info.api_id);
    
    query = "UPDATE merchant SET "
            " name = "           + escape(info.name)       + ", "
            " group_id = "       + escape(info.group)      + ", "
            " status = "         + escape(info.status )    + ", "
            " url  = "           + escape(info.url)        + ", "
            " inn = "            + escape(info.inn )       + ", "
            " mfo = "            + escape(info.mfo)        + ", "
            " ch_account = "     + escape(info.checking_account) + ", "
            " contract_date = "  + contract_date            +  ", "
            " min_amount = "     + escape(info.min_amount)  + ", "
            " max_amount = "     + escape(info.max_amount)  + ", "
            " merchant_id = "    + escape(info.merchantId)  + ", "
            " terminal_id = "    + escape(info.terminalId)  + ", "
            " port = "           + escape(info.port)        + ", "
            " external = "       + escape(info.external)    + ", "
            " ext_service_id = " + escape(info.extern_service) + ", "
            " bank_id = "        + escape(info.bank_id)     + ", "
            " rate    = "        + escape(info.rate)        + ", "
            " rate_money = "     + escape(info.rate_money)  + ", "
            " position =  "      +  position                + ", "
            " api_id = "         + api_id                   + "  "
            " WHERE id = "       + escape(info.id)          ;
    
    st.prepare(query);
    
    if (st.affected_rows() == 0)
        return Error_not_found;
    
    return Error_OK ;
}

Error_T Merchant_T::del(uint32_t id)
{
    SCOPE_LOG(slog);
    if(id == 0) {
        slog.WarningLog("Empty field");
        return Error_login_empty;
    }
    std::string query = "DELETE FROM merchant WHERE id = "+escape(id) ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK ;
}



Merchant_info_T Merchant_T::get( int32_t id, Error_T& ec)
{
    SCOPE_LOG(slog);
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string query  = " SELECT id, name, group_id, url, status, inn, mfo, ch_account, contract, contract_date, merchant_id  "
                  " , terminal_id, port, min_amount, max_amount, external, ext_service_id, bank_id, rate, rate_money, api_id    "
                  " FROM merchant WHERE id = "  + escape( id ) ;
    
    DB_T::statement st(m_db);
    st.prepare( query  ) ;
    
    Merchant_info_T data;
    if ( st.rows_count() != 1 ) 
    {
        ec = Error_not_found;
    } 
    else 
    {
        ec = Error_OK ;
        st.row(0) >> data.id               >> data.name     >> data.group         >> data.url         >> data.status   >> data.inn            >> data.mfo
                  >> data.checking_account >> data.contract >> data.contract_date >> data.merchantId 
                  >> data.terminalId       >> data.port     >> data.min_amount    >> data.max_amount  >> data.external >> data.extern_service >> data.bank_id
                >> data.rate             >> data.rate_money >> data.api_id ;
    }
    return data;
}

static std::string make_where(const Merchant_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;

    if (search.id != 0 ) 
        result += " AND (id = " + escape(search.id) + " ) " ;
    
    if (search.status != 0) 
        result += " AND ( status = " + escape(search.status) + " ) " ;
    
    if (search.merchantId.length()>0) 
        result += " AND ( merchant_id = " + escape(search.merchantId) + " ) " ;

    return result;
}

static bool empty_search(const Merchant_info_T& s)
{
    return (s.id == 0) && (s.status == 0) && (s.merchantId.empty());
}



Error_T Merchant_T::info(const Merchant_info_T &search, Merchant_info_T &data)
{
    SCOPE_LOG(slog);
    
    if (empty_search(search)){
        slog.WarningLog("search is emtpy!");
        return Error_not_found;
    }
    
    DB_T::statement st(m_db);
    std::string query, where_s = make_where(search);
    
    
    query = " SELECT id, name, group_id, url, status, inn, mfo, ch_account, contract, contract_date, merchant_id "
            " , terminal_id, port, min_amount, max_amount, external, ext_service_id, bank_id, rate, rate_money, api_id "
            " FROM merchant WHERE " + where_s;
    
    
    st.prepare(query);
    
    
    if (st.rows_count() != 1) 
        return Error_not_found;
    
    st.row(0) >> data.id               >> data.name     >> data.group         >> data.url         >> data.status   >> data.inn            >> data.mfo
              >> data.checking_account >> data.contract >> data.contract_date >> data.merchantId 
              >> data.terminalId       >> data.port     >> data.min_amount    >> data.max_amount  >> data.external >> data.extern_service >> data.bank_id
              >> data.rate             >> data.rate_money >> data.api_id;
    
    return Error_OK ;
}    

Error_T Merchant_T::acc_info(uint32_t merchant_id, Merch_acc_T& acc)
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT login, password, api_json FROM merchant_access_info WHERE merchant_id = " + escape(merchant_id) + "; " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
     
    if (st.rows_count() != 1)
        return Error_OK; //@note when we fill merchant_access_info, returns Error_not_found;
    
    st.row(0) >> acc.login >> acc.password >> acc.api_json ;
    
    return Error_OK;
}

Error_T Merchant_T::api_info(int32_t api_id, Merch_acc_T& acc)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT id, name, url, login, password, api_json, options, status, api_id FROM merchant_api WHERE api_id = " + escape(api_id);
    DB_T::statement st(m_db);
    
    Error_T ec = Error_OK ;
    st.prepare(query, ec);
    if (ec) return ec;
    if (st.rows_count() != 1 )
        return Error_not_found ;
    st.row(0) >> acc.id >> acc.name >> acc.url >> acc.login >> acc.password >> acc.api_json >> acc.options >> acc.status >> acc.api_id ;
    return Error_OK ;
}

static std::string make_where_from_search(const Merchant_info_T& search){
    std::string result = " ( 1 = 1 ) " ;
    if (search.id      != 0 ) result += " AND ( id =      " + escape(search.id     ) + ") " ;
    if (search.status  != 0 ) result += " AND ( status =  " + escape(search.status ) + ") " ;
    if (search.bank_id != 0 ) result += " AND ( bank_id = " + escape(search.bank_id) + ") " ;
    if (search.group   != 0 ) result += " AND ( group_id = " +escape(search.group) + ") " ;
    
    if ( ! search.name.empty() ){
        std::string like_name = "%" + search.name + "%";
        result += " AND ( LOWER(name)  LIKE LOWER( " + escape(like_name) + " ) ) "  ;
    }
    
    if ( ! search.id_list.empty() ) {
        result += " AND ( id IN ( " + search.id_list + " ) ) " ;
    }
    return result;
}


Error_T Merchant_T::list(const Merchant_info_T &search, const Sort_T &sort, Merchant_list_T &out_list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where_from_search(search);
    std::string query = " SELECT id, name, group_id, status, inn, mfo, ch_account, contract, contract_date  "
                        " ,merchant_id, terminal_id, port, min_amount, max_amount, url, external, ext_service_id, bank_id "
                        " , rate, rate_money, position, api_id FROM merchant WHERE " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int const rows = st.rows_count();
    out_list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Merchant_info_T& info = out_list.list[ i ] ;
        
        st.row(i) >> info.id         >> info.name          >> info.group      >> info.status     >> info.inn  >> info.mfo >> info.checking_account 
                  >> info.contract   >> info.contract_date >> info.merchantId >> info.terminalId >> info.port 
                  >> info.min_amount >> info.max_amount    >> info.url        >> info.external   >> info.extern_service   >> info.bank_id
                  >> info.rate       >> info.rate_money    >> info.position   >> info.api_id ;
        
    }
    
    bool const counted = (sort.offset == 0 &&               ( !sort.limit || rows < sort.limit ) ) ||
                         (sort.offset  > 0 && (rows > 0) && ( !sort.limit || rows < sort.limit ) ) ;
    if ( counted )
    {
        out_list.count = sort.offset + rows;
    }
    else
    {
        query = "SELECT count(*) FROM merchant WHERE " + where_s ;
    
        //total count
        st.prepare(query);

        out_list.count = st.get_int(0, 0);
    }
    
    return Error_OK ;
}  

namespace
{
    struct not_valid_merchant
    {
        bool operator()(const Merchant_info_T& m)const{ return m.id == 0 ; }
    };
} // end


Error_T Merchant_T::top_list(uint64_t uid, const Sort_T& sort, Merchant_list_T& out_list)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT DISTINCT(merchant_id), COUNT(*), MAX(ts) "
                        "FROM purchases "
                        "WHERE (uid = " + escape(uid) + ") AND ( status = 1 ) AND ( merchant_id IN (SELECT m.id FROM merchant m WHERE m.status = 2 ) )" // 2-show status 
                        "GROUP BY merchant_id "
                        "ORDER BY 2 DESC, 3 DESC  "
                        "LIMIT " + to_str(sort.limit);
    
    std::map< long long, int > index_map;
    
    
    std::string agg ;
    DB_T::statement st(m_db);
    st.prepare(query);
    int rows = st.rows_count();
    for(int i = 0; i < rows; ++i)
    {
        long long merchant_id = st.get_int(i, 0);
        if (i > 0)
            agg += ", ";
        
        agg += escape(merchant_id);
        
        index_map[ merchant_id ] = i;
    }
    
    if ( rows < sort.limit )
    {
        typedef merchant_identifiers ids;
        
        //There no more 20 TOP merchants
        static const ids::values mobile_merchants[ 20 ] = 
        { 
            ids::Beeline,        ids::Ucell,        ids::UMS,        ids::UzMobile_GSM,    ids::Perfectum,     ids::UzMobile_CDMA,
            ids::ISTV_I,         ids::TPS_I,        ids::My_Taxi,    ids::Munis_HOT_WATER, ids::Munis_Gaz,     ids::Munis_COLD_WATER,
            ids::Sarkor_Telecom, ids::Cron_Telecom, ids::Ars_Inform, ids::EVO_LTE,         ids::Sharq_Telecom, ids::Beeline_Internet,
            ids::Uzonline_I,     ids::Comnet_I
        };
        
        for(int i = 0; i < 20 && rows < sort.limit; ++i)
        {
            int const id = static_cast< int >( mobile_merchants[ i ] );
            
            if (! index_map.count( id )){
                //add it
                if (rows > 0)
                    agg += ", ";
                agg += escape( id );
                index_map[ id ] = rows++; 
            }
        }
    }
    
    
    //////////////////////////////////
    query = " SELECT id, name, group_id, status, inn, mfo, ch_account, contract, contract_date          "
            " ,merchant_id, terminal_id, port, min_amount, max_amount, url, external, ext_service_id, bank_id "
            " , rate, rate_money FROM merchant WHERE id IN ( " + agg + ") ";

    
    st.prepare(query);
   
    int rows_m = st.rows_count();
   
    out_list.list.resize(rows_m);
    
    
    for(int i = 0; i < rows_m; ++i)
    {
        Merchant_info_T  info;
        
        st.row(i) >> info.id         >> info.name          >> info.group      >> info.status     >> info.inn  >> info.mfo >> info.checking_account 
                  >> info.contract   >> info.contract_date >> info.merchantId >> info.terminalId >> info.port 
                  >> info.min_amount >> info.max_amount    >> info.url        >> info.external   >> info.extern_service >> info.bank_id
                  >> info.rate          >> info.rate_money ;
        
        //@Note only debug purpose, because by theoritic this NEVER be acurred.
        if ( ! index_map.count(info.id)){
            slog.WarningLog("Cannot find id: %lld", static_cast<long long>(info.id));
            continue;
        }
        
        size_t const idx = index_map[info.id];
        if (idx >= out_list.list.size()){
            slog.WarningLog("idx: not found There(%u), merchant-id: %lld", static_cast< unsigned > (idx), static_cast< long long > (info.id));
            continue;
        }
        
        out_list.list[ idx ] = info;
        
        
    }
    
    //remove uninitialized merchants.
    out_list.list.erase( 
        std::remove_if( 
            out_list.list.begin(),
            out_list.list.end(),
            not_valid_merchant()
        ),
        out_list.list.end()
    );
    
    
    
    out_list.count = out_list.list.size();
    
    return Error_OK ;
    //////////////////////////////////
}

static std::string make_where(const Merchant_field_T& search)
{
    std::string result = " ( 1 = 1 ) " ;

    if (search.merchant_id != 0)
        result += " AND ( mid = " + escape(search.merchant_id) + " ) " ;

    if (search.fID != 0)
        result += " AND ( fid = " + escape(search.fID) + " ) " ;
    
    if (search.usage != 0)
        result += " AND (usage = " + escape(search.usage) + ") " ;
    
    return result;
}

std::vector< Merchant_field_T> Merchant_T::request_fields( int32_t mid ) 
{
    SCOPE_LOG(slog);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string query  = " SELECT mid, fid, parent_fid, position, label, prefix_label, type, input_digit, label_uz1, input_letter, min_length, max_length, param_name, usage "
                        " FROM merchant_fields WHERE  usage =  1  AND  mid = " + escape( mid )  ;
    
    DB_T::statement st(m_db);
    st.prepare(  query );
    
    size_t rows = st.rows_count();
  
    std::vector< Merchant_field_T> fields(rows);

    for(size_t i = 0; i < rows; ++i){   
        Merchant_field_T& info = fields[i];
        st.row(i) >> info.merchant_id  >>  info.fID        >> info.parent_fID  >> info.position   >> info.label 
                  >> info.prefix_label >> info.type        >> info.input_digit >> info.label_uz1  
                  >> info.input_letter >> info.min_length  >> info.max_length  >> info.param_name >> info.usage;
    }
   
    return fields;
}


Error_T Merchant_T::fields(const Merchant_field_T &search, const Sort_T& sort,  Merchant_field_list_T & fields)
{
    SCOPE_LOG(slog);
    std::string where_s = make_where(search), sort_s = sort.to_string();
    
    std::string query = " SELECT mid, fid, parent_fid, position, label, prefix_label, type, input_digit, label_uz1, input_letter, min_length, max_length, param_name, usage "
                        " FROM merchant_fields WHERE " + where_s +  sort_s;//" ORDER BY mid, position " ;
    
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
 
    fields.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Merchant_field_T& info = fields.list[i];
        st.row(i) >> info.merchant_id  >>  info.fID        >> info.parent_fID  >> info.position   >> info.label 
                  >> info.prefix_label >> info.type        >> info.input_digit >> info.label_uz1  
                  >> info.input_letter >> info.min_length  >> info.max_length  >> info.param_name >> info.usage;
    }
    
    long long total_cnt = sort.total_count(rows);
    if (total_cnt < 0){
        query = "SELECT COUNT(*) FROM merchant_fields WHERE " + where_s ;
    
        st.prepare(query);
        
        st.row(0) >> total_cnt;
    }
    fields.count = total_cnt;
    
    return Error_OK;
}


Error_T Merchant_T::field_info(uint32_t field_id, Merchant_field_T &data)
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    std::string query;
    
    query = "SELECT fid, mid, parent_fid, position, \"label\", prefix_label, \"type\", input_digit, input_letter, min_length, max_length, param_name, usage " 
            " FROM merchant_fields WHERE fid = " + escape(field_id);
    
    st.prepare(query);
    
    
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> data.fID >> data.merchant_id >> data.parent_fID >> data.position >> data.label >> data.prefix_label >> data.type >> data.input_digit 
              >> data.input_letter >> data.min_length >> data.max_length >> data.param_name >> data.usage;
    
    return Error_OK;
}

Error_T Merchant_T::field_add(const Merchant_field_T &data)
{
    SCOPE_LOG(slog);
    DB_T::statement st(m_db);
    std::string query;
    
    query = "INSERT INTO merchant_fields (parent_fid, mid, position, \"type\", \"label\", label_uz1, prefix_label, "
            " input_digit, input_letter, min_length, max_length, param_name, usage ) VALUES ( "
            + escape(data.parent_fID)   + ", "
            + escape(data.merchant_id)  + ", "
            + escape(data.position)     + ", "
            + escape(data.type )        + ", "
            + escape(data.label)        + ", "
            + escape(data.label_uz1)    + ", "
            + escape(data.prefix_label) + ", "
            + escape(data.input_digit)  + ", "
            + escape(data.input_letter) + ", "
            + escape(data.min_length)   + ", "
            + escape(data.max_length)   + ", "
            + escape(data.param_name)   + ", "
            + escape(data.usage)        + " )"
            ;
    
    st.prepare(query);
    
    return Error_OK ;
    
}

Error_T Merchant_T::field_edit(uint32_t field_id, const Merchant_field_T &data)
{
    SCOPE_LOG(slog);
    
    std::string query;
    DB_T::statement st(m_db);
    
    query  =  "UPDATE merchant_fields SET "
            " parent_fid = " + escape(data.parent_fID)   + ", "
            " position   = " + escape(data.position )    + ", "
            " \"type\"   = " + escape(data.type )        + ", "
            " \"label\"  = " + escape(data.label)        + ", "
            " label_uz1  = " + escape(data.label_uz1)    + ", "
            " prefix_label=" + escape(data.prefix_label) + ", "
            " input_digit =" + escape(data.input_digit)  + ", "
            " input_letter=" + escape(data.input_letter) + ", "
            " min_length = " + escape(data.min_length)   + ", "
            " max_length = " + escape(data.max_length)   + ", "
            " param_name = " + escape(data.param_name)   + ", "
            " usage      = " + escape(data.usage)        + "  "
            " WHERE fid = "  + escape(field_id);
    
    st.prepare(query);
    
    if (st.affected_rows() == 0)
        return Error_not_found;
    
    return Error_OK ;
}

Error_T Merchant_T::field_delete(uint32_t field_id)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM merchant_fields WHERE fid = " + escape(field_id) ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

static std::string make_where_2(const Merchant_field_data_T& search)
{
    std::string where_s = " ( 1 = 1 ) " ;
   
   if (search.fID != 0)
       where_s += " AND ( fid = " + escape(search.fID) + ") " ;
   
   if (search.parent_key != 0)
       where_s += " AND ( \"parent_key\" = " + escape(search.parent_key) + ") " ;
   
   return where_s; 
}

Error_T Merchant_T::field_data_list(const Merchant_field_data_T& search, const Sort_T& sort,  Merchant_field_data_list_T & f_dlist)
{
   SCOPE_LOG(slog);
   std::string  where_s = make_where_2(search), sort_s = sort.to_string();
   
   std::string query = "SELECT id, fid, \"key\", \"parent_key\", value, prefix, extra_id, service_id, service_id_check "
           "FROM merchant_fields_data WHERE " + where_s + sort_s;
    
   DB_T::statement st(m_db);
   
   st.prepare(query);
   
   int rows = st.rows_count();
   f_dlist.list.resize(rows);
   for(int i=  0 ; i < rows; ++i)
   {
       Merchant_field_data_T& info = f_dlist.list[i];
       
       st.row(i) >> info.id >> info.fID >> info.key >> info.parent_key >> info.value >> info.prefix >> info.extra_id >> info.service_id >> info.service_id_check ;
   }
    
   long long  total_cnt = sort.total_count(rows);
   if (total_cnt < 0) // can't find
   {
       query = "SELECT COUNT(*) FROM merchant_fields_data WHERE " + where_s ;
    
       st.prepare(query);
       
       st.row( 0 ) >> total_cnt;
   }
   
   f_dlist.count = total_cnt;
   return Error_OK;
}

static std::string make_where(const Merchant_field_data_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    
    if (search.fID != 0 )
        result += " AND ( fid = " + escape(search.fID) + ") "  ;
    
    if (search.id != 0 )
        result += " AND ( id = " + escape(search.id)  + " ) " ;
    
    if (search.key != 0)
        result += " AND ( \"key\" = " + escape(search.key) + ") " ;
    
    if (search.parent_key != 0)
        result += " AND ( \"parent_key\" = " + escape(search.parent_key) + " ) " ;
    
    if ( ! search.prefix.empty() )
        result += " AND ( prefix = " + escape(search.prefix) + " ) " ;
    
    return result;
}

Error_T Merchant_T::field_data_search(const Merchant_field_data_T &search_param, const Sort_T& sort, Merchant_field_data_T &f_data)
{
    SCOPE_LOG(slog);

    std::string query, where_s, sort_s = sort.to_string();
    DB_T::statement st(m_db);
    
    where_s = make_where(search_param);
    
    query = "SELECT id, \"key\", \"parent_key\", value, prefix, extra_id, fid, service_id, service_id_check " 
            "FROM merchant_fields_data WHERE " + where_s + sort_s;
    
    st.prepare(query);
    
    if (st.rows_count() != 1){
        slog.WarningLog("Found %d rows", st.rows_count());
        return Error_not_found;
    }
    
    st.row(0) >> f_data.id >> f_data.key >> f_data.parent_key >> f_data.value >> f_data.prefix >> f_data.extra_id 
              >> f_data.fID >> f_data.service_id >> f_data.service_id_check ;

    return Error_OK;
}

Error_T Merchant_T::field_data_add(const Merchant_field_data_T& f_data)
{
    SCOPE_LOG(slog);
    
    std::string query;
    DB_T::statement st(m_db);
    
    query = "INSERT INTO merchant_fields_data (fid, \"key\", \"parent_key\", value, prefix, extra_id, service_id, service_id_check) VALUES ( "
            + escape(f_data.fID)              + ", "
            + escape(f_data.key)              + ", "
            + escape(f_data.parent_key)       + ", " 
            + escape(f_data.value)            + ", "
            + escape(f_data.prefix)           + ", "
            + escape(f_data.extra_id)         + ", "
            + escape(f_data.service_id)       + ", "
            + escape(f_data.service_id_check) + " ) " 
            ;
    
    st.prepare(query);
    return Error_OK;
}

Error_T Merchant_T::filed_data_edit(const Merchant_field_data_T &f_data)
{
    SCOPE_LOG(slog);

    std::string query;
    DB_T::statement st(m_db);
    
    query = " UPDATE merchant_fields_data SET "
            " \"key\" = "           + escape(f_data.key)              + 
            ", \"parent_key\" = "   + escape(f_data.parent_key)       + 
            ", value = "            + escape(f_data.value)            + 
            ", prefix = "           + escape(f_data.prefix)           + 
            ", extra_id = "         + escape(f_data.extra_id)         + 
            ", service_id = "       + escape(f_data.service_id)       + 
            ", service_id_check = " + escape(f_data.service_id_check) + 
            "  WHERE id = "          + escape(f_data.id)              ;
    
       
    st.prepare(query);
    if (st.affected_rows() == 0)
        return Error_not_found ;
    
    return Error_OK ;
}

Error_T Merchant_T::field_data_delete(const uint32_t id)
{
    SCOPE_LOG(slog);
    if (id == 0)
        return Error_not_found;
    
    std::string query = "DELETE FROM merchant_fields_data WHERE id = " + escape(id) ;
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

int64_t Merchant_T::next_transaction_id()
{
    SCOPE_LOG(slog);

    DB_T::statement st(m_db);
    
    std::string query =   "UPDATE paynet_info SET oson_tr_id = oson_tr_id + 1 WHERE id = 1 RETURNING oson_tr_id;" ;
    
    st.prepare(query );

    int64_t id = 0;
    st.row(0) >> id; 
    
    return id ;
}



Error_T Merchant_T::qr_image(uint32_t merchant_id, std::string& data)
{
    SCOPE_LOG(slog);
   
    std::string query = "SELECT location FROM merchant_qr_image WHERE merchant_id = " + escape(merchant_id)  + "; ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1)
        return Error_not_found ;
    
    std::string location = st.get_str(0, 0);
    if(! oson::utils::file_exists(location))   
        return Error_not_found;
    
    std::ifstream fin(location.c_str(), std::ifstream::binary | std::ifstream::in);
    std::stringstream ss;
    ss << fin.rdbuf();
    data = ss.str();
    
    return Error_OK;
}

Error_T Merchant_T::generate_qr_image(uint32_t merchant_id)
{
    SCOPE_LOG(slog);
    
    enum{QR_SIZE = 8};
    
    std::string qr_token = "merchant:" + to_str(merchant_id);
    
     
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
        printf("Failed to generate qr_omage");
        return error;
    }

    // Set angle
    std::string qr_angle_file = img_location + "qr8_angle.png";
    PNG_image_T angle_image;
    //  Reader qr_angle_file to angle_image content.
    error = angle_image.read_file(qr_angle_file);
    if(error != Error_OK) {
        printf("Failed to read angle image");
        return error;
    }

    error = qr_image.set_qr_angle(angle_image, qrcode->width, QR_SIZE);
    if(error != Error_OK) {
        printf("Failed to set angle for qr image");
        return error;
    }

    // Set ico
    std::string ico_file = img_location + "oson_ico.png";
    PNG_image_T ico_image;
    error = ico_image.read_file(ico_file);
    if(error != Error_OK) {
        printf("Failed to read angle image");
        return error;
    }
    error = qr_image.add_top_image(ico_image, 0);
    if(error != Error_OK) {
        printf("Failed to set angle for qr image");
        return error;
    }

    
    const std::string location = img_location +"merchant_" + num2string(merchant_id) +  ".png";
    ::std::remove(location.c_str());
    
    error = qr_image.write_file(location);
    if(error != Error_OK) {
        printf("Failed to save png file \"%s\"", location.c_str());
        return error;
    }
    
    //update or insert
    {
        std::string query = " UPDATE merchant_qr_image SET location = " + escape(location) + " WHERE merchant_id = " + escape(merchant_id) + "; " ;
        DB_T::statement st(m_db);
        
        st.prepare( query );
        
        //OR insert if not exists.
        query = " INSERT INTO merchant_qr_image (merchant_id, location) SELECT " + escape(merchant_id) + ", " + escape(location) + " "
                " WHERE NOT EXISTS (SELECT 1 FROM merchant_qr_image WHERE merchant_id = " + escape(merchant_id) + ") " ;
        st.prepare(query); 
        
    }
    return Error_OK;
}




Error_T Merchant_T::bonus_add(const Merchant_bonus_info_T& info, /*OUT*/  int64_t & id)
{
    SCOPE_LOG(slog);
    
    std::string start_date = "'2000-01-01'", end_date = "'2099-12-31'";
    
    if ( ! info.start_date.empty() )
        start_date = escape(info.start_date);
    
    if ( ! info.end_date.empty() ) 
        end_date = escape(info.end_date);
    
    std::string query =
    "INSERT INTO merchant_bonus (merchant_id, min_amount, percent, description, start_date, end_date, bonus_amount, status, longitude, latitude ) VALUES "
    " ( " +  escape(info.merchant_id)  + 
    ", "  +  escape(info.min_amount)   + 
    ", "  +  escape(info.percent )     + 
    ", "  +  escape(info.description)  + 
    ", "  +  start_date                +  
    ", "  +  end_date                  + 
    ", "  +  escape(info.bonus_amount) + 
    ", "  +  escape(info.status)       + 
    ", "  +  escape(info.longitude)    +
    ", "  +  escape(info.latitude)     +
    " ) RETURNING id " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> id ;
    
    return Error_OK ;
}


static std::string make_where(const Merchant_bonus_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.id != 0) 
        result += " AND ( id = " + escape(search.id) + " ) " ;
    if (search.merchant_id != 0)
        result += " AND ( merchant_id = " + escape(search.merchant_id) + " ) " ;
    if (search.start_date.length() > 0)
        result += " AND ( start_date >= " + escape(search.start_date) + " ) " ;
    if (search.end_date.length() > 0 ) 
        result += " AND ( start_date <= " + escape(search.end_date)  + " ) " ;
    
    if (search.active)
        result += " AND (start_date <= Now() AND end_date >= Now() ) " ;
    
    if (search.status != 0)
        result += " AND (status = " + escape(search.status) + ") " ;
    
    return result;
}

Error_T Merchant_T::bonus_list(const Merchant_bonus_info_T& search, const Sort_T& sort, Merchant_bonus_list_T& list)
{
    SCOPE_LOG(slog);
    
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = " SELECT id, merchant_id, min_amount, percent, description, start_date, end_date, bonus_amount, status, longitude, latitude, group_id "
                        " FROM merchant_bonus WHERE " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    
    int rows = st.rows_count();
    list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Merchant_bonus_info_T& info = list.list[i];
        st.row(i) >> info.id       >> info.merchant_id >> info.min_amount   >> info.percent >> info.description 
                  >> info.start_date >> info.end_date      >> info.bonus_amount >> info.status  >> info.longitude 
                  >> info.latitude >> info.group_id;
    }
    
    //OPTIMIZED
    bool const counted = ( sort.offset == 0  &&               ( ! sort.limit || rows < sort.limit) ) || 
                         ( sort.offset >  0  && (rows > 0) && ( ! sort.limit || rows < sort.limit) ) ;
    
    if (counted)
    {
        list.total_count = sort.offset + rows;
        return Error_OK;
    }

    query = "SELECT count(*) FROM merchant_bonus WHERE " + where_s ;
    st.prepare(query);
    
    st.row(0) >> list.total_count;
    
    return Error_OK ;
}

Error_T Merchant_T::bonus_edit(uint64_t id, const Merchant_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    std::string start_date = "'2000-01-01'", end_date = "'2099-12-31'";
    
    if (! info.start_date.empty())
        start_date = escape(info.start_date);
    
    if (! info.end_date.empty() )
        end_date = escape(info.end_date);
    
    std::string query =
     "  UPDATE merchant_bonus SET "
     "  merchant_id =  " + escape(info.merchant_id)  + 
     ", min_amount =   " + escape(info.min_amount)   + 
     ", percent =      " + escape(info.percent)      + 
     ", description =  " + escape(info.description)  + 
     ", start_date =   " + start_date                + 
     ", end_date =     " + end_date                  + 
     ", bonus_amount = " + escape(info.bonus_amount) + 
     ", status =       " + escape(info.status)       + 
     ", longitude =    " + escape(info.longitude)    + 
     ", latitude  =    " + escape(info.latitude)     +
     "  WHERE id =     " + escape(id);
    
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.affected_rows() == 0)
        return Error_not_found;
    
    
    return Error_OK;
}

Error_T Merchant_T::bonus_delete(uint64_t id)
{
    SCOPE_LOG(slog);
    ////////////////////////////////////////////////////////////////////////
    std::string query  = "DELETE FROM merchant_bonus WHERE id = " + escape( id ) ;
     
    DB_T::statement st(m_db);
    st.prepare(  query );
    
    return st.affected_rows() == 0 ? Error_not_found : Error_OK ;
}

