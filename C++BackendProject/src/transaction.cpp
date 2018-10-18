#include "transaction.h"
#include "log.h"
#include "utils.h"
#include "DB_T.h"



Transactions_T::Transactions_T( DB_T & db ): m_db(db) 
{

}

static std::string make_where_and(const Transaction_info_T & search)
{
    std::string result = "( 1 = 1 ) ";
    if (search.id  != 0            ) result += " AND (id = "      + escape(search.id)        + ") ";
    if (search.uid != 0            ) result += " AND (uid = "     + escape(search.uid)       + ") ";
    if (search.dst_uid != 0        ) result += " AND (dst_uid = " + escape(search.dst_uid)   + ") ";
    if (search.srccard.size() > 0  ) result += " AND (srccard = " + escape(search.srccard)   + ") ";
    if (search.dstcard.size() > 0  ) result += " AND (dstcard = " + escape(search.dstcard)   + ") ";
    if (search.from_date.size() > 0) result += " AND (ts >= "     + escape(search.from_date  + " 00:00:00") + ") " ;
    if (search.to_date.size() > 0  ) result += " AND (ts <= "     + escape(search.to_date    + " 23:59:59") + ") " ;
    if (search.status != 0         ) result += " AND (status = "  + escape(search.status)    + " ) ";
    
    //this bank_id  IS dstcard bank id.
    if (search.dstbank_id != 0        ) 
    {
        result += " AND (left(dstcard,6) IN ( SELECT bin_code FROM bank  WHERE  id = " + escape(search.dstbank_id) + ") )" ;
    }
    else if (search.aid != 0 )
    {
        result += " AND ( left(dstcard,6) IN \n"
                  " ( SELECT b.bin_code FROM bank b \n"
                  "    WHERE b.id IN \n"
                  "    ( SELECT bank FROM admin_permissions WHERE  ( bank > 0 ) AND (aid = " + escape(search.aid)+ " ) ) \n) \n) " ;
    }
    
    return result;
}

Error_T Transactions_T::transaction_list(const Transaction_info_T & search, const Sort_T &sort, Transaction_list_T & list)
{
	SCOPE_LOG(slog);
    
    std::string where_str = make_where_and( search ), sort_str = sort.to_string();
    
    std::string query = "SELECT id, uid, srccard, dstcard, srcphone, dstphone, amount::numeric::bigint, ts::timestamp(0), dst_uid, status, commision::numeric::bigint, ref_num "
                        "FROM transaction WHERE " + where_str + sort_str + "; " ;

    DB_T::statement st(m_db);
    st.prepare(query )  ;
    
    int rows = st.rows_count();  
    
    list.list.resize( rows ); 
    
    for (int i = 0; i < rows; i++) 
    {
        Transaction_info_T& info = list.list[i];
        st.row(i) >> info.id     >> info.uid >> info.srccard >> info.dstcard >> info.srcphone  >> info.dstphone
                  >> info.amount >> info.ts  >> info.dst_uid >> info.status  >> info.comission >> info.eopc_id ;
        
    }
    
    bool const counted = ( sort.offset == 0 &&               ( !sort.limit || rows < sort.limit ) ) ||
                         ( sort.offset > 0 &&  (rows > 0) && ( !sort.limit || rows < sort.limit ) ) ;
    if (counted){
        list.count = sort.offset + rows;
        return Error_OK ;
    }
    // get total count, i.e  without offset and limit.
    query = "SELECT count(*) FROM transaction WHERE " + where_str ;
    st.prepare( query) ;
    
    st.row(0) >> list.count;
    
    return Error_OK;
}

int64_t Transactions_T::transaction_add(const Transaction_info_T & data)
{
    SCOPE_LOG(slog);
    std::string src_card =  ::oson::utils::mask_pan(data.srccard);
    std::string dst_card =  ::oson::utils::mask_pan(data.dstcard);

    std::string query;
    query = "INSERT INTO transaction (uid, srccard, dstcard, srcphone, dstphone, amount, commision, dst_uid, status, ref_num) VALUES ( "
            + escape(data.uid)      + ", "
            + escape(src_card)      + ", "
            + escape(dst_card)      + ", "
            + escape(data.srcphone) + ", "
            + escape(data.dstphone) + ", "
            + escape(data.amount)   + ", "
            + escape(data.comission) + ", "
            + escape(data.dst_uid)   + ", "
            + escape(data.status)    + ", "
            + escape(data.eopc_id)   + "  "
            + ") RETURNING id " ;
    
   
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int64_t trn_id;
    st.row(0) >> trn_id;

    return trn_id;
}

Error_T Transactions_T::transaction_edit(const uint64_t &trn_id, Transaction_info_T &data)
{ 
    SCOPE_LOG(slog);
    std::string src_card =  ::oson::utils::mask_pan( data.srccard ) ;
    std::string dst_card =  ::oson::utils::mask_pan( data.dstcard ) ;
    
    std::string query;
    query = "UPDATE transaction SET ref_num = " + escape(data.eopc_id) + 
            ", status    = " + escape(data.status)    + 
            ", commision = " + escape(data.comission) + 
            ", dst_uid   = " + escape(data.dst_uid)   ;
    
    if(! src_card.empty() )
        query += ", srccard = " + escape(src_card);

    if( ! dst_card.empty())
        query += ", dstcard = " + escape(dst_card);
    
    if (data.bearn > 0)
        query += ", bearn = " + escape(data.bearn);
    
    if (data.status_text.size() > 0)
        query += ", status_text = " + escape(data.status_text);
    
    query += " WHERE id = " + escape(trn_id);

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.affected_rows() == 0) return Error_not_found;
    
    return Error_OK;
}

Error_T Transactions_T::transaction_cancel(const Transaction_info_T &which)
{
    SCOPE_LOG(slog);

    std::string query;
    query = "UPDATE transaction SET status = " + escape(TR_STATUS_CANCEL) + " WHERE id = " + escape(which.id); 

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    
    if (st.affected_rows() == 0) return Error_not_found;
    
    return Error_OK;
}

Error_T Transactions_T::transaction_del(const uint64_t &id)
{
    SCOPE_LOG(slog);

    std::string query;
    query = " UPDATE transaction SET "
            " status =   " + escape(TR_STATUS_REVERSED) + 
            ", bearn = 0 "
            " WHERE id = " + escape(id); 
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.affected_rows() == 0) return Error_not_found;
    
    return Error_OK;
}

Error_T Transactions_T::info(const uint64_t &id, Transaction_info_T &info)
{
    SCOPE_LOG(slog);

    std::string query = " SELECT id, uid, srccard, dstcard, srcphone, dstphone, amount::numeric::bigint, ts::timestamp(0), dst_uid, status, ref_num, bearn "
                        " FROM transaction WHERE id = " + escape(id);

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1){
        slog.WarningLog("Not found transaction with id: %llu", id);
        return Error_not_found;
        
    }
    st.row(0) >> info.id >> info.uid >> info.srccard >> info.dstcard >> info.srcphone >> info.dstphone 
              >> info.amount >> info.ts >> info.dst_uid >> info.status >> info.eopc_id >> info.bearn  ;
    return Error_OK;
}

Error_T Transactions_T::temp_transaction_add(const Transaction_info_T &data)
{
    SCOPE_LOG(slog);

    std::string random = oson::utils::generate_token();
    std::string query = 
            "INSERT INTO temp_transaction (dst_uid, dstcard, amount, token) VALUES( "
            + escape(data.dst_uid) + ", "
            + escape(data.dstcard) + ", "
            + escape(data.amount)  + ", "
            + "substring(encode(hmac('" + random + "', gen_salt('md5'), 'sha1'), 'hex') from 0 for 25)"
            " ) RETURNING id " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> m_transaction_id ;
    
    return Error_OK ;
    
    
}

Error_T Transactions_T::temp_transaction_del(const std::string &token)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM temp_transaction WHERE token = " + escape(token);

    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Transactions_T::temp_transaction_info(const std::string &token, Transaction_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query;
    query = "SELECT dst_uid, dstcard, amount FROM temp_transaction WHERE token = " + escape(token);

    DB_T::statement st(m_db);
    st.prepare(query);
    
    if (st.rows_count() != 1)
    {
        slog.WarningLog("No item");
        return Error_not_found;
    }
    
    st.row(0) >> info.dst_uid >> info.dstcard >> info.amount ;
    
    return Error_OK;
}

Error_T Transactions_T::temp_transaction_info(uint64_t id, std::string &token)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT token FROM temp_transaction WHERE id = " + escape(id) ;

    //(query.c_str(), query.size());

    DB_T::statement st(m_db);

    st.prepare(query);
    
    if (st.rows_count() != 1)
    {
        slog.WarningLog("No item");
        return Error_not_found;
    }

    st.row(0) >> token;

    return Error_OK;
}

std::string make_where(const Transaction_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    
    if (search.uid != 0)
        result += " AND ( uid = " + escape(search.uid) + " ) " ;
    
    if (search.srccard.size() != 0)
        result += " AND ( srccard = " + escape(search.srccard) + " ) ";
    
    if (search.dstcard.size() != 0)
        result += " AND ( dstcard = " + escape(search.dstcard) + " ) ";
    
    if (search.from_date.length() > 0)
        result += " AND ( ts >= " + escape(search.from_date + " 00:00:00") + " ) ";
    
    if (search.to_date.length() > 0)
        result += " AND ( ts <= " + escape(search.to_date + " 23:59:59") + " ) " ;
    
    return result;
}

Error_T Transactions_T::stat(uint16_t group, const Transaction_info_T &search, std::vector<Tr_stat_T> &statistics)
{
    SCOPE_LOG(slog);
    
    std::string where_s  = make_where(search);
    std::string group_ts = ( group == 2 ? "month" : "day" );
    std::string query;
    if (group > 0) 
        query = " SELECT count(distinct(uid)), count(*), sum(amount)::numeric::bigint, date_trunc( '" + group_ts + "', ts) FROM transaction WHERE " + where_s + " GROUP BY 4";
    else 
        query = " SELECT count(distinct(uid)), count(*), sum(amount)::numeric::bigint FROM transaction WHERE " + where_s; // without group by
    
    //(query.c_str(), query.size());

    DB_T::statement st(m_db);
    
    st.prepare( query  );
    
    int rows = st.rows_count();
    
    statistics.resize(rows);
    for(int i = 0; i < rows; ++i)
    {
        Tr_stat_T& stat = statistics[i];
        if (group > 0)
        {
            st.row(i) >> stat.users >> stat.total >> stat.sum >> stat.ts ;
        }
        else
        {
            st.row(i) >> stat.users >> stat.total >> stat.sum ; // without ts
        }
        
    }
    
    return Error_OK;
}   

Error_T Transactions_T::top(const Transaction_info_T &search, std::vector<Transaction_top_T> &tops)
{
    SCOPE_LOG(slog);

    std::string query = " SELECT u.phone, count(u.phone), SUM(p.amount)::numeric::bigint "
                        " FROM transaction p "
                        " LEFT JOIN users u ON u.id = p.uid "
                        " WHERE u.phone is NOT NULL "
                        " GROUP BY u.phone "
                        " ORDER BY 3 desc "
                        " LIMIT 10" 
            ;

    //(query.c_str(), query.size());

    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    tops.resize(rows);
    for(int i =  0; i < rows; ++i)
    {
        Transaction_top_T& t = tops[i];
        st.row(i) >> t.phone >> t.count >> t.sum ;
    }
    
    return Error_OK;
}

Error_T Transactions_T::bill_accept(Transaction_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE transaction SET status = " + escape(info.status) + " WHERE id = " + escape(info.id) ;

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    
    if (st.affected_rows() == 0) return Error_not_found;
    return Error_OK ;
}

Incoming_list_T Transactions_T::incoming_list(int64_t uid, const Sort_T& sort) 
{
    SCOPE_LOG(slog);

    std::string query;
    query = " SELECT 2 as type, id, ts::timestamp(0),  amount,  srccard,  srcphone,  dstcard,  commision,  status "
            " FROM transaction "
            " WHERE dst_uid = " + escape(uid) + " " + sort.to_string() ;

    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    
    Incoming_list_T  list;
    list.list.resize( rows );
    
    for(int i = 0; i< rows; ++i)
    {
        Incoming_T & f = list.list[i];
        st.row(i) >> f.type >> f.id >> f.ts >> f.amount >> f.src_card >> f.src_phone >>f.dst_card >> f.commision >> f.status ;
    }
    
    
    int total = sort.total_count(rows);
    
    if (total >= 0){
        list.count = total;
    } else {
        query = "SELECT count(*) FROM transaction WHERE dst_uid = " + escape(uid);
        st.prepare(query);
        st.row(0) >> list.count ;
    }
    
    return list;
}

static std::string make_outgoing_search_p( int64_t uid, const Outgoing_T& search ) 
{
    
    std::string result = " ( p.uid = " + escape(uid) + " ) ";
    if (search.type != 0 ){
        if (search.type != 1) // not a this table
        {
            result = " ( 'FALSE' ) "  ;// false
            return result;
        }
    }
    
    if (search.merchant_id != 0 ) { // only purchase table has a merchant_id
        result += " AND ( p.merchant_id = " + escape(search.merchant_id) + " ) " ; 
    }
    
    if (search.status != 0 ) {
        result += " AND (p.status = " + escape(search.status ) + ") " ;
    }
    
    if (search.card_id != 0 ) {
        result += " AND (p.card_id = " + escape(search.card_id) + " ) " ;
    }
    
    if ( ! search.from_date.empty() ) {
        result += " AND (p.ts >= " + escape(search.from_date + " 00:00:00" )  + " ) ";
    }
    
    if ( ! search.to_date.empty() ) {
        result += " AND (p.ts <= " + escape(search.to_date + " 23:59:59" ) + " ) " ;
    }
    
    return result;
}

static std::string make_outgoing_search_t( int64_t uid, const Outgoing_T& search ) 
{
    
    std::string result = " ( t.uid = " + escape(uid) + " ) ";
    if (search.type != 0 ){
        if (search.type != 2) // not a this table
        {
            result = " ( 'FALSE' ) "  ;// false
            return result;
        }
    }
    
    if (search.status != 0 ) {
        result += " AND (t.status = " + escape(search.status ) + ") " ;
    }
    
    
    if ( ! search.from_date.empty() ) {
        result += " AND (t.ts >= " + escape(search.from_date + " 00:00:00" )  + " ) ";
    }
    
    if ( ! search.to_date.empty() ) {
        result += " AND (t.ts <= " + escape(search.to_date + " 23:59:59" ) + " ) " ;
    }
    
    return result;
}



Outgoing_list_T Transactions_T::outgoing_list(int64_t uid, const Outgoing_T& search, const Sort_T& sort) 
{
    SCOPE_LOG(slog);
    
    slog.DebugLog("search.type = %d", search.type);
    std::string where_p = make_outgoing_search_p( uid, search);
    std::string where_t = make_outgoing_search_t( uid, search);
    
    std::string query = 
    "( ( SELECT 1 as type, p.id as id , p.ts::timestamp(0) as pts, p.amount, p.merchant_id, p.login, 'a' as dsccard, 'a' as dstphone, p.commission, "
    "       p.status as status, p.pan as pan,  p.oson_tr_id as oson_tr_id, p.card_id, p.bearn             \n" 
    " FROM purchases p\n" 
    " WHERE  " + where_p + ") \n" 
    " UNION \n "
    " (SELECT 2 as type, t.id as id, t.ts::timestamp(0) as pts, t.amount , 0, 'a', t.dstcard, t.dstphone, t.commision, "
    "       t.status as status, '*' as pan,  0 as oson_tr_id, 0, t.bearn                                             \n" 
    " FROM transaction t\n" 
    " WHERE  " + where_t +  " ) ) \n"  + sort.to_string();
            

    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    
    Outgoing_list_T list;
    list.list.resize(rows);
    
    
    for(int i = 0; i < rows; ++i)
    {
        Outgoing_T & f = list.list[i];
        
        st.row(i) >> f.type >> f.id >> f.ts >> f.amount >> f.merchant_id >> f.login 
                  >> f.dst_card >> f.dstphone >> f.commision >> f.status >> f.pan >> f.oson_tr_id
                  >> f.card_id >> f.bearn;
        
        if(f.pan.empty())
            f.pan = "*";
        
    }
    int total = sort.total_count(rows);
    if (total >= 0){
        list.count = total;
    } else {
        int p_count = 0, t_count = 0;
        query = " (SELECT count(*) FROM purchases p WHERE " + where_p + ") UNION (SELECT count(*) FROM transaction t WHERE " + where_t + " ) ";
        
        
        st.prepare(query);
        if (st.rows_count() == 2 ) {
            st.row( 0 ) >> p_count ;
            st.row( 1 ) >> t_count ;
            list.count = p_count + t_count;
        } else {
            slog.WarningLog("rows count != 2");
            list.count = rows;
        }
    }
    return list;
}
