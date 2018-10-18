
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>

#include "Merchant_T.h"

#include "transaction.h"
#include "purchase.h"
#include "log.h"
#include "Merchant_T.h"
#include "DB_T.h"
#include "utils.h"






Purchase_reverse_T::Purchase_reverse_T(DB_T& db)
    :m_db( db )
{
}

Purchase_reverse_T::~Purchase_reverse_T()
{
}

Error_T  Purchase_reverse_T::info(int64_t id, Purchase_reverse_info_T& out)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT id, pid, aid, uid, sms_code, status, ts_start, ts_confirm, phone FROM purchases_reverse WHERE id = " + escape(id);
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> out.id >> out.pid >> out.aid >> out.uid >> out.sms_code >> out.status >> out.ts_start >> out.ts_confirm >> out.phone ;
    
    return Error_OK ;
}

Error_T  Purchase_reverse_T::add(const Purchase_reverse_info_T& in,  int64_t& id)
{
    SCOPE_LOG(slog);
    
    std::string query = "INSERT INTO purchases_reverse (pid, aid, uid, sms_code, status, ts_start, phone) VALUES ( "  
                         + escape(in.pid )     + ", "
                         + escape(in.aid)      + ", "
                         + escape(in.uid)      + ", "
                         + escape(in.sms_code) + ", "
                         + escape(in.status)   + ", "
                         + escape(in.ts_start) + ", "
                         + escape(in.phone)    + " ) "
                         "RETURNING id " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> id;
    
    return Error_OK ;
}

Error_T  Purchase_reverse_T::upd(const Purchase_reverse_info_T& in)
{
    SCOPE_LOG(slog);
    
    std::string ts_confirm = " ts_confirm ";
    
    if ( ! in.ts_confirm.empty() )
        ts_confirm = escape(in.ts_confirm);
    
    std::string query = "UPDATE purchases_reverse SET status = " + escape(in.status) + ", ts_confirm = " +  ts_confirm  + " WHERE id = " + escape(in.id);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    return Error_OK ;
}
    

Purchase_info_T::Purchase_info_T() 
{
        id  = 0;
        uid = 0;
        mID = 0;
        amount = 0;
        status = TR_STATUS_UNDEF;
        paynet_status = 0;
        receipt_id = 0;
        oson_paynet_tr_id = 0;
        oson_tr_id = 0;
        bearns = 0;
        card_id = 0;
        commission= 0;
}



Purchase_T::Purchase_T(DB_T &db): m_db(db)
{
   
   // SCOPE_LOG(slog);
}
Purchase_T::~Purchase_T()
{
    
}

    

Error_T Purchase_T::add_detail_info(const Purchase_details_info_T& info)
{
    SCOPE_LOG(slog);
    std::string json_text = oson::utils::encodebase64(info.json_text);
    
    std::string query = "INSERT INTO purchase_info (oson_tr_id, trn_id, json_text) VALUES ( " 
                + escape(info.oson_tr_id) + ", " + escape(info.trn_id) + ", " + escape(json_text) + ") ";
    
    DB_T::statement st( m_db );
     
    st.prepare( query) ;
    return Error_OK ;
}

Error_T Purchase_T::get_detail_info(uint64_t oson_tr_id, Purchase_details_info_T& info)
{
    SCOPE_LOG(slog);
    std::string query  = "SELECT oson_tr_id, trn_id, json_text FROM purchase_info WHERE oson_tr_id = " + escape(oson_tr_id)  ;
     
    DB_T::statement st(m_db);
    st.prepare(  query   );
    
    if (st.rows_count() != 1)  {  return Error_not_found; }

    st.row(0) >> info.oson_tr_id >> info.trn_id >> info.json_text ;
    if ( ! info.json_text.empty() && oson::utils::is_base64(info.json_text) ) {
        info.json_text = oson::utils::decodebase64(info.json_text);
    }
    return Error_OK;
}

Error_T Purchase_T::bonus_list_client(const Purchase_search_T&search, const Sort_T& sort, Purchase_list_T& list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string() ;
    std::string where_s = " (    bearn > 0    ) AND  (uid = " + escape( search.uid ) + " ) "  ;
    if (search.mID != 0)
        where_s += " AND (merchant_id = " + escape(search.mID) + " ) " ;
    
    
    std::string query = 
            "SELECT id, uid, merchant_id, login, ts::timestamp(0), amount, transaction_id, pan, status, paynet_tr_id, paynet_status, receipt, card_id, bearn "
            "FROM purchases WHERE " + where_s + sort_s;
    
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();
    
    list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Purchase_info_T& p = list.list[i];
        st.row(i) >> p.id >> p.uid >> p.mID >> p.login >> p.ts >> p.amount >> p.eopc_trn_id >> p.pan >> p.status >> p.paynet_tr_id >> p.paynet_status 
                >> p.receipt_id >> p.card_id >> p.bearns ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0) {
        list.count = total_cnt;
        return Error_OK ;
    }
    
    query = "SELECT COUNT(*) FROM purchases WHERE " + where_s ;
    st.prepare( query  ) ;
    
    st.row(0) >> list.count ;
    
    return Error_OK ;
}


Error_T Purchase_T::bonus_list(const Purchase_search_T& search, const Sort_T& sort, Purchase_list_T& list)
{
    SCOPE_LOG(slog);

    std::string sort_s = sort.to_string() ;
    std::string where_s = " ( ( card_id IN (SELECT card_id FROM card_bonus)  ) OR  (  bearn > 0  ) ) " ;
    
    if ( ! search.from_date.empty() )
        where_s += " AND ( ts >= " + escape(search.from_date) + ") ";
    
    if ( ! search.to_date.empty() )
        where_s += " AND ( ts <= " + escape(search.to_date) + " ) " ;
    
    if ( search.status != 0 )
        where_s += " AND ( status = " + escape(search.status ) + ") " ;
    
    std::string query = 
            "SELECT id, uid, merchant_id, login, ts, amount, transaction_id, pan, status, paynet_tr_id, paynet_status, receipt, card_id, bearn "
            "FROM purchases WHERE " + where_s + sort_s;
    
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();
    
    list.list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Purchase_info_T& p = list.list[i];
        st.row(i) >> p.id >> p.uid >> p.mID >> p.login >> p.ts >> p.amount >> p.eopc_trn_id >> p.pan >> p.status >> p.paynet_tr_id >> p.paynet_status 
                >> p.receipt_id >> p.card_id >> p.bearns ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0 ){
        list.count = total_cnt;
        return Error_OK ;
    }
    query = "SELECT COUNT(*) FROM purchases WHERE " + where_s ;
    st.prepare( query) ;
    
    st.row(0) >> list.count ;
    
    return Error_OK ;
}

static std::string make_where_from_search(const Purchase_search_T& search){
    std::string result = " (1 = 1) ";
    
    if ( search.id        != 0 )result += " AND (p.id = "          + escape(search.id)     + ") ";
    if ( search.uid       != 0) result += " AND (p.uid = "         + escape(search.uid)    + ") ";
    if ( search.mID       != 0) result += " AND (p.merchant_id = " + escape(search.mID)    + ") ";
    if ( search.status    != 0){
        if (search.status == TR_STATUS_REVERSED || search.status == TR_STATUS_CANCEL ) 
            result  += " AND (p.status = '2' OR p.status = '3' ) " ;
        else
            result += " AND (p.status = "      + escape(search.status) + ") ";
    }
    
    if (search.bank_id != 0 ) {
        result += " AND ( EXISTS( SELECT 1 FROM merchant m WHERE m.id = p.merchant_id AND m.bank_id = " + escape(search.bank_id) + " ) ) " ;
    }
    
    
    if ( ! search.from_date.empty()   ) result += " AND (p.ts >= " +  escape(search.from_date + " 00:00:00") + ") ";
    if ( ! search.to_date.empty()     ) result += " AND (p.ts <= " +  escape(search.to_date   + " 23:59:59") + ") ";
    if ( ! search.eopc_trn_id.empty() ) result += " AND (p.transaction_id=" +  escape(search.eopc_trn_id)    + ") ";
    
    if (search.m_use_merchant_ids) {
        if ( ! search.m_merchant_ids.empty() ) 
        {
            if (search.in_merchants)
            {
                result  += " AND (p.merchant_id IN (" + search.m_merchant_ids + ") ) " ;
            } else {
                result += " AND NOT( p.merchant_id IN ( " + search.m_merchant_ids + " ) ) " ;
            }
        }
        else 
        {
           if ( search.in_merchants ) 
               result += " AND (1 = 0 ) " ; // always false :)))
           else
               result += " " ;//nothing do add.
        }
    }
    return result;
}

std::string Purchase_T::list_admin_query(const Purchase_search_T & search, const Sort_T& sort )  
{
    std::string sort_str = sort.to_string(),  where_str = make_where_from_search(search);
    std::string query = 
        " SELECT  p.id, p.uid, p.merchant_id, p.login, p.ts::timestamp(0), p.amount, p.transaction_id, p.pan, p.status,"
        "        p.paynet_tr_id, p.paynet_status, p.oson_paynet_tr_id, p.receipt, p.card_id, p.bearn, p.commission, p.merch_rsp"
        " FROM purchases p WHERE " + where_str + sort_str + " ";
    return query ;
}
Error_T Purchase_T::list_admin(const Purchase_search_T & search, const Sort_T &sort, Purchase_list_T & list)
{
    SCOPE_LOG(slog);
    std::string query = list_admin_query(search, sort) ;
    DB_T::statement st(m_db);
    
    st.prepare(query);  
    
    const size_t rows = st.rows_count();   
    list.list.resize(rows); // a little optimization
    
    for(size_t i= 0; i < rows; ++i){
        Purchase_info_T& info = list.list[i];
        st.row(i) >> info.id >> info.uid >> info.mID >> info.login >> info.ts >> info.amount >> info.eopc_trn_id >> info.pan 
                  >> info.status >> info.paynet_tr_id >> info.paynet_status >> info.oson_paynet_tr_id >> info.receipt_id 
                  >> info.card_id >> info.bearns >> info.commission >> info.merch_rsp ;
        
    }
    
    if ( search.flag_total_sum ) 
    {
        std::string where_str = make_where_from_search(search);
        query = "SELECT count(*), sum(p.amount), sum(p.commission) FROM purchases p WHERE " + where_str ;
        st.prepare(query); 
        st.row(0) >> list.count >> list.sum  >> list.commission_sum ;
    } 
    else 
    {
        list.count          = 0;
        list.sum            = 0;
        list.commission_sum = 0;
    }
    
    return Error_OK;
}

Error_T Purchase_T::list(const Purchase_search_T &search, const Sort_T &sort, Purchase_list_T &list)
{
    SCOPE_LOG(slog);
    std::string sort_str = sort.to_string(),  where_str = make_where_from_search(search);
    std::string query = 
        " SELECT  p.id, p.uid, p.merchant_id, p.login, p.ts::timestamp(0), p.amount, p.transaction_id, p.pan, p.status,"
        "        p.paynet_tr_id, p.paynet_status, p.oson_paynet_tr_id, p.receipt, p.card_id, p.bearn, p.commission, p.merch_rsp, users.phone"
        " FROM purchases p LEFT JOIN users ON (p.uid=users.id) WHERE " + where_str + sort_str + "; ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query);  
    
    const size_t rows = st.rows_count();   
    list.list.resize(rows); // a little optimization
    
    for(size_t i= 0; i < rows; ++i){
        Purchase_info_T& info = list.list[i];
        st.row(i) >> info.id >> info.uid >> info.mID >> info.login >> info.ts >> info.amount >> info.eopc_trn_id >> info.pan 
                  >> info.status >> info.paynet_tr_id >> info.paynet_status >> info.oson_paynet_tr_id >> info.receipt_id 
                  >> info.card_id >> info.bearns >> info.commission >> info.merch_rsp >> info.src_phone ;
        
    }
    
    if ( search.flag_total_sum ) 
    {
        query = "SELECT count(*), sum(p.amount), sum(p.commission) FROM purchases p WHERE " + where_str ;
        st.prepare(query); 
        st.row(0) >> list.count >> list.sum  >> list.commission_sum ;
    } 
    else 
    {
        list.count          = 0;
        list.sum            = 0;
        list.commission_sum = 0;
    }
    
    return Error_OK;
}

Error_T Purchase_T::list(const Purchase_search_T &search, const Sort_T &sort, Purchase_export_list_T &list)
{
    SCOPE_LOG(slog);
    std::string sort_str = sort.to_string(), where_str = make_where_from_search( search );
    std::string query = "SELECT p.id, p.uid, p.merchant_id, p.login, p.ts::timestamp(0), p.amount, merchant.name, merchant.contract "
            "FROM purchases p LEFT OUTER JOIN merchant ON ( merchant.id = p.merchant_id ) WHERE " + where_str + sort_str ;

    //(query.c_str(), query.size());
    DB_T::statement st(m_db);

    st.prepare(query);

    int rows = st.rows_count();  
    list.list.reserve(rows); // a little optimization.

    for( int i = 0; i < rows; i++ )
    {
        Purchase_export_T info;
        info.id         = st.get_int( i, 0 );
        info.uid        = st.get_int( i, 1 );
        info.mID        = st.get_int( i, 2 );
        info.login      = st.get_str( i, 3 );
        info.ts         = st.get_str( i, 4 );
        info.amount     = st.get_int( i, 5 );
        info.m_name     = st.get_str( i, 6 );
        info.m_contract = st.get_str( i, 7 );

        list.list.push_back( info );
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0 ){
        list.count = total_cnt;
        return Error_OK ;
    }
    
    query = "SELECT count(*) FROM purchases p WHERE " + where_str ;
    st.prepare(query);
    list.count = st.get_int(0, 0);
    return Error_OK;
}

int64_t Purchase_T::add(const Purchase_info_T &data)
{
    SCOPE_LOGD(slog);
    std::string query;
    query = "INSERT INTO purchases (uid, merchant_id, login, ts, amount, transaction_id, pan, status, receipt, oson_tr_id, card_id, commission) VALUES "
            " ( " + escape(data.uid)        + ", " + escape(data.mID)         + ", "  + escape(data.login)   +  ", " + escape(data.ts)         +  
            ",  " + escape(data.amount)     + ", " + escape(data.eopc_trn_id) + ", "  + escape(data.pan)     +  ", " + escape(data.status)     + 
            ",  " + escape(data.receipt_id) + ", " + escape(data.oson_tr_id)  +  ", " + escape(data.card_id) +  ", " + escape(data.commission) +
            ") RETURNING id " ;

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int64_t id = 0;
    st.row(0) >> id;
    slog.InfoLog("id: %ld", id);
    return id; 
    
}

Error_T Purchase_T::info(const uint64_t trn_id, Purchase_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query;
    query = "SELECT id, uid, merchant_id, login, ts::timestamp(0), amount, transaction_id, pan, status, paynet_tr_id, paynet_status, receipt, oson_paynet_tr_id, "
            "oson_tr_id, commission, merch_rsp FROM purchases WHERE id = " +escape(trn_id);
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if(st.rows_count() != 1){
        slog.WarningLog("Not found transaction with id %ull", trn_id);
        return Error_not_found;
    }
    
    st.row(0) >> info.id >> info.uid >> info.mID >> info.login >> info.ts >> info.amount >> info.eopc_trn_id >> info.pan >> info.status 
              >> info.paynet_tr_id   >> info.paynet_status     >> info.receipt_id >> info.oson_paynet_tr_id >> info.oson_tr_id 
              >> info.commission >> info.merch_rsp;
    
    
    
    
    return Error_OK;
}
Error_T Purchase_T::update_status( int64_t trn_id, int status)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE purchases SET status = " + escape(status) + " WHERE id = " + escape(trn_id ) ; 
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK ;
}

Error_T Purchase_T::update_status( int64_t trn_id, const Purchase_info_T& new_data)
{
    //SCOPE_LOG(slog);
    return update_status(trn_id, new_data.status);
}

    
Error_T Purchase_T::update(const uint64_t trn_id, const Purchase_info_T &p)
{
    SCOPE_LOG(slog);
    std::string query, paynet_tr_id = " paynet_tr_id ";
    
    if ( ! p.paynet_tr_id.empty() )
        paynet_tr_id = escape(p.paynet_tr_id);
    
    std::string merch_rsp = " merch_rsp " ;
    if (p.merch_rsp.length() > 0)
        merch_rsp = escape(p.merch_rsp);
    
    query = "  UPDATE purchases SET "
            "  uid = "            + escape(p.uid)              + 
            ", merchant_id = "    + escape(p.mID)              + 
            ", login = "          + escape(p.login)            + 
            ", amount = "         + escape(p.amount )          + 
            ", transaction_id = " + escape(p.eopc_trn_id)      + 
            ", pan = "            + escape(p.pan)              + 
            ", status = "         + escape(p.status)           + 
            ", paynet_tr_id = "   +  paynet_tr_id              +  
            ", paynet_status = "  + escape(p.paynet_status)    + 
            ", receipt = "        + escape(p.receipt_id)       + 
            ", oson_paynet_tr_id = " + escape(p.oson_paynet_tr_id) + 
            ", oson_tr_id = "     + escape(p.oson_tr_id)       + 
          ( p.bearns ? ", bearn  = " + escape(p.bearns)  : " ")+ 
            //", commission =  "    + escape(p.commission)       +  // commision needn't update, because it MUST NOT CHANGE.
            ", merch_rsp = "      + merch_rsp                 + 
            "  WHERE id = "       + escape(trn_id)             ;
    
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query);
    return Error_OK ;
}

Error_T Purchase_T::cancel(const uint64_t &id)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE purchases SET status = " + escape(TR_STATUS_CANCEL) + " WHERE id = " + escape(id) ;
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query);
    return Error_OK ;
}

static std::string make_where(const Purchase_search_T& search)
{
    std::string result = " ( 1 = 1 ) ";
    
    if ( search.id != 0 )
        result += " AND ( id = " + escape( search.id ) + " ) ";
    
    if ( search.uid != 0 )
        result += " AND ( uid = " + escape( search.uid ) + " ) ";
    
    if ( search.mID != 0 )
        result += " AND ( merchant_id = " + escape( search.mID ) + " ) " ;
    
    if ( search.from_date.length() > 0 )
        result += " AND ( ts >= " + escape( search.from_date + " 00:00:00" ) + " ) " ;
    
    if ( search.to_date.length() > 0 )
        result += " AND ( ts <= " + escape( search.to_date + " 23:59:59" ) + " ) " ; 
    
    return result;
}

Error_T Purchase_T::stat(uint16_t group, const Purchase_search_T &search, std::vector<Purchase_state_T> &statistics)
{
    SCOPE_LOG(slog);
    std::string where_s = make_where(search);
    std::string query, group_s = (group == 2 ? "month" : "day");
    if(group > 0)
        query = " SELECT count(distinct(uid)), count(*), SUM(amount)::numeric::bigint, date_trunc('"+group_s+"', ts) FROM purchases WHERE " + where_s + " GROUP BY 4  ";
    else
        query = " SELECT count(distinct(uid)), count(*), SUM(amount)::numeric::bigint FROM purchases WHERE " + where_s  ; //without ts and group by
    
    
    //(query.c_str(), query.size());
    DB_T::statement st(m_db);
    
    st.prepare(query) ;
    
    int rows = st.rows_count();
    
    statistics.resize(rows);
    for(int i = 0; i < rows; ++i)
    {
        Purchase_state_T& state = statistics[i];
        if(group > 0)
        {
            st.row(i) >> state.users >> state.total >> state.sum >> state.ts;
            
        }
        else
        {
            st.row(i) >> state.users >> state.total >> state.sum ; // without ts.
        }
    }
    return Error_OK;
}


static std::string make_where_top(const Purchase_search_T& search)
{
    std::string result = " (1 = 1) ";
    
    if (search.from_date.length() > 0)
        result += " AND ( ts >= " + escape(search.from_date + " 00:00:00") + " ) ";
    
    if (search.to_date.length() > 0 )
        result += " AND ( ts <= " + escape(search.to_date + " 23:59:59") + " ) " ;
    
    return result;
}

Error_T Purchase_T::top(const Purchase_search_T &search, std::vector<Purchase_top_T> &tops)
{
    SCOPE_LOG(slog);
    std::string where_s = make_where_top(search);
    std::string query = " SELECT merchant_id, count(distinct(uid)), count(*), sum(amount)::numeric::bigint FROM purchases WHERE " + where_s + " GROUP BY 1 ";
    DB_T::statement st(m_db);
    
    st.prepare(query) ; 
    
    int rows = st.rows_count();
    tops.resize(rows);
    for(int i = 0 ; i < rows; ++i)
    {
        Purchase_top_T& top = tops[i];
        st.row(i) >> top.merchant_id >> top.users >> top.count >> top.sum ;
    }
    return Error_OK;
}    

Error_T Purchase_T::favorite_list(const Favorite_info_T &search,  const Sort_T& sort,  /*out*/ Favorite_list_T  &list)
{
    SCOPE_LOG(slog);

    std::string query;
    
    query = "SELECT id, fav_id, field_id, merchant_id, \"key\", value, prefix, name FROM favorite WHERE uid = " + escape(search.uid);
    
    if (search.merchant_id != 0)
        query += " AND merchant_id = " + escape(search.merchant_id);
    
    query += sort.to_string();
    
    DB_T::statement st(m_db);
    
    st.prepare( query);
    
    int rows = st.rows_count();
    list.list.resize(rows);
    for(int i = 0; i < rows; ++i)
    {
        Favorite_info_T& info = list.list[ i ];
        st.row(i) >> info.id >> info.fav_id >> info.field_id >> info.merchant_id >> info.key >> info.value >> info.prefix  >> info.name ;
    }

    list.total_count = sort.total_count(rows);
    
    if (list.total_count < 0 ) 
    {
        query = "SELECT COUNT(*) FROM favorite WHERE uid = " +escape(search.uid) ;
    
        if (search.merchant_id != 0 )
            query += " AND merchant_id = " + escape(search.merchant_id);
        
        st.prepare(query);
        st.row(0) >> list.total_count ;
    }
    
    return Error_OK;
}

Error_T Purchase_T::favorite_add(uint64_t uid, const std::vector<Favorite_info_T>& list)
{
    SCOPE_LOG(slog);

    DB_T::transaction tr(m_db);
    std::string query = "SELECT nextval(pg_get_serial_sequence('favorite', 'id')) as new_id";
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    uint32_t fav_id = 0;
    
    st.row(0) >> fav_id;
    
    std::vector<Favorite_info_T>::const_iterator i = list.begin();
    query = "INSERT INTO favorite (uid, field_id, \"key\", value, prefix, merchant_id, name, fav_id) VALUES ";
    
    //Add multiple rows.
    for(; i != list.end(); i++) 
    {
        if (i != list.begin())
            query += ", ";
        
        query += "\n( "
          + escape(uid)             + ", "
          + escape(i->field_id)     + ", "
          + escape(i->key)          + ", "
          + escape(i->value)        + ", "
          + escape(i->prefix)       + ", "
          + escape(i->merchant_id)  + ", "
          + escape(i->name)         + ", "
          + escape(fav_id)          + ") "
          ;
    }
    st.prepare(query);
    
    tr.commit();
    return Error_OK;
}

Error_T Purchase_T::favorite_del(uint32_t fav_id)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM favorite WHERE fav_id = " + escape(fav_id) ;
     
    DB_T::statement st(m_db);
    
    st.prepare(   query    );
    
    return Error_OK ;
}

//@Note optimize it without parsing data.
//    approximat elapsesd 300 us. very long, It must be 20-30 us.
std::string extract_sysinfo_sid( std::string json_text )
{
    boost::property_tree::ptree json_value, json_data, json_sid;
    std::istringstream ss(json_text);
    boost::property_tree::read_json(ss, json_value);
    
    json_data = json_value.get_child("data");
    json_sid  = json_data.get_child("sysinfo_sid");
    return json_sid.data();
}

Error_T Purchase_T::make_beeline_merch_response(const Purchase_search_T& search, const Sort_T& sort, Beeline_response_list_T & rsp)
{
    SCOPE_LOG(slog);
    
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

    if (search.mID != merchant_identifiers::Beeline)
        return Error_not_found;
    
    
    Merch_acc_T acc;
    Merchant_T merch(m_db);
    //merch.acc_info(search.mID, acc);
    merch.api_info((int32_t)merchant_api_id::beeline, acc);
    
    int64_t total_amount = 0;
    
    Purchase_list_T list;
    {
        std::string sort_str = sort.to_string(),  where_str = make_where_from_search(search);
        std::string query = 
            " SELECT  p.id, p.uid, p.merchant_id, p.login, p.ts::timestamp(0), p.amount, p.paynet_tr_id, p.paynet_status, p.merch_rsp"
            " FROM purchases p  WHERE " + where_str + sort_str + "; ";

        DB_T::statement st(m_db);

        st.prepare(query);  

        const size_t rows = st.rows_count();   
        list.list.resize(rows); // a little optimization

        for(size_t i= 0; i < rows; ++i){
            Purchase_info_T& info = list.list[i];
            st.row(i) >> info.id >> info.uid >> info.mID >> info.login >> info.ts >> info.amount >> info.paynet_tr_id >> info.paynet_status  >> info.merch_rsp ;
            
            total_amount += info.amount;
        }
        
        int total_cnt = sort.total_count(rows);
        if ( total_cnt >=0)
        {
            list.count = total_cnt;
        }
        else
        {
            query = "SELECT count(*) FROM purchases p WHERE " + where_str ;
            st.prepare(query); 
            list.count = st.get_int(0,0);
        }
    }
    
    rsp.total_count = list.count;
    rsp.total_amount = total_amount;
    rsp.list.resize(list.list.size());
    
    
    std::vector< std::string > merch_resp;
    for(size_t i = 0; i < rsp.list.size(); ++i)
    {
        const Purchase_info_T& p = list.list[i];

        Beeline_merch_response_T& beeline = rsp.list[i];

        std::string pay_id, date_stamp, commit_date;

        boost::algorithm::split(merch_resp, p.merch_rsp, boost::algorithm::is_any_of(",") );
        merch_resp.resize( 3 );

        pay_id         = merch_resp[ 0 ] ;
        date_stamp     = merch_resp[ 1 ] ;
        commit_date    = merch_resp[ 2 ] ;
        if (date_stamp.empty())
            date_stamp = p.ts;
        if (commit_date.empty())
            commit_date = p.ts;
        if (pay_id.empty())
            pay_id = "<none>";
        ////////////////////////////////////////////////////////////////
        beeline.login           =  "AloqaBank";//acc.login;
        beeline.msisdn          =  p.login;
        beeline.amount          =  to_str( p.amount / 100 );
        beeline.currency        =  "2";
        beeline.pay_id          =  pay_id ;
        beeline.receipt_num     =  p.paynet_tr_id;
        beeline.date_stamp      =  date_stamp;
        beeline.commit_date     =  commit_date;
        beeline.partner_pay_id  =  to_str(p.id);
        beeline.branch          =  "OSON" ;
        beeline.trade_point     =  "OSON" ;
        ////////////////////////////////////////////////////////////////
    }
    
    return Error_OK ;
}
