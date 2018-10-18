
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include "bills.h"
#include "log.h"
#include "DB_T.h"
#include "utils.h"

std::string Bill_data_T::get_login( const std::string& fields)
{
    SCOPE_LOGD(slog);
    
    slog.InfoLog("fields: '%s'", fields.c_str());
    
    typedef boost::property_tree::ptree ptree;
    // [ { "value" : "value",  "prefix" : "<prefix>" }  ]
    ptree root_tree;
    
    try
    {
        std::istringstream ostr(fields);
        boost::property_tree::read_json(ostr, root_tree);
    }
    catch(std::exception& e)
    {
        slog.WarningLog("exception: %s", e.what());
        return "";
    }
    
    if ( root_tree.empty() ) {
        slog.WarningLog("tree is empty");
        return "";
    }
    
    ptree field = (*root_tree.begin()).second;
    
    std::string value = field.get< std::string > ("value") ;
    std::string prefix = field.get< std::string >("prefix");
    std::string login = prefix + value;
    
    return login;
}

std::string Bill_data_T::get_login() const
{
    return get_login(fields);
}

Bills_T::Bills_T(DB_T &db)
    :m_db(db)
{
}


int64_t Bills_T::add(const Bill_data_T &b)
{
    SCOPE_LOG(slog);
    std::string query = 
         "INSERT INTO user_bills (id, uid, uid2, amount, merchant_id, value, status, add_ts, comment) VALUES ( "
           " DEFAULT , " 
           + escape(b.uid)         + ", "
           + escape(b.uid2)        + ", "
           + escape(b.amount)      + ", "
           + escape(b.merchant_id) + ", "
           + escape(b.fields)      + ", "
           + escape(b.status )     + ", "
           " NOW() , "
           + escape(b.comment)     + " )"
           " RETURNING id ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query) ;
    int64_t id = 0;
    st.row(0) >> id;
    slog.InfoLog("id = %ld", id);
    return id;
}

Error_T Bills_T::del(int64_t id)
{
    SCOPE_LOG(slog);
    ///////////////////////////////////////////////////////////////////////////
    std::string query = "DELETE FROM user_bills WHERE id = " + escape(id)  ;
    
    //////////////////////////////////////////////////////////////////////////
    DB_T::statement st(m_db);
    st.prepare(  query  );
    return Error_OK ;
}

static std::string make_where_from_search(const Bill_data_search_T & search){
    std::string result = " ( 1 = 1 ) " ;
    if ( static_cast< bool >( search.id   ) ) result += "AND ( ub.id   = " + escape( search.id  )   + " ) " ;
    if (   search.uid   != search.UID_NONE  ) result += "AND ( ub.uid  = " + escape( search.uid )  + " ) " ;
    if (   search.uid2  != search.UID_NONE  ) result += "AND ( ub.uid2 = " + escape( search.uid2) + " ) " ;
    
    
    if (   search.status != 0 ) result += "AND (ub.status = " + escape( search.status) + ") ";
    if (   search.merchant_id != 0 ) result += "AND (ub.merchant_id = " + escape( search.merchant_id) + ") ";
    if ( ! search.merchant_id_list.empty() ) result += "AND (ub.merchant_id IN ( " + search.merchant_id_list + ") ) ";
    
    return result;
}

Bill_data_list_T Bills_T::list(const Bill_data_search_T &search, const Sort_T &sort)
{
    SCOPE_LOG(slog);
    
    Bill_data_list_T  bill_list ;
    
    std::string sort_str = sort.to_string(), where_str = make_where_from_search(search);
    std::string query = " SELECT ub.id, ub.uid, ub.uid2, ub.amount, ub.merchant_id, ub.value, ub.add_ts, ub.comment, users.phone "
                        " FROM user_bills ub LEFT OUTER JOIN users ON (users.id = ub.uid2) WHERE " + where_str + sort_str + "; " ;

    DB_T::statement st(m_db);

    st.prepare(query);
    
    size_t rows = st.rows_count();     
    bill_list.list.resize(rows); // a little optimization.

    for( size_t i = 0; i < rows; i++ ) {
        Bill_data_T& info = bill_list.list[i];
        st.row(i) >> info.id >> info.uid >> info.uid2 >> info.amount >> info.merchant_id >> info.fields >> info.add_ts >> info.comment >> info.phone;
    }
    
    int total_cnt = sort.total_count(rows);
    if( total_cnt >= 0){
        bill_list.count = total_cnt;
    }
    else
    {
        query = "SELECT count(*) FROM user_bills ub WHERE " + where_str ;
        st.prepare(query) ;
        st.row(0) >> bill_list.count;
    }
    return bill_list ;
} 

Bill_data_T Bills_T::get(int64_t id,  Error_T& ec)
{
    SCOPE_LOG(slog);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string query = "SELECT id, uid, uid2, amount, merchant_id, value, add_ts FROM user_bills WHERE id = " + escape( id ) ;

    DB_T::statement st(m_db);
    
    st.prepare(  query   );
    
    if (st.rows_count() != 1){ 
        slog.WarningLog("bill not found: %d entry", st.rows_count() ); 
        ec =  Error_not_found; 
        return Bill_data_T();
    }
    
    Bill_data_T out;
    st.row(0) >> out.id >> out.uid >> out.uid2 >> out.amount >> out.merchant_id >> out.fields >> out.add_ts;
    ec =  Error_OK;
    return out;
}

Error_T Bills_T::set_status(int64_t id, uint16_t status)
{
    SCOPE_LOG(slog);
    ////////////////////////////////////////////////////////////////////////////////////////////
    std::string query  = "UPDATE user_bills SET status = "+  escape(status)  +" WHERE id = " + escape( id ) ;
    
    DB_T::statement st(m_db);
    st.prepare( query );
    
    return Error_OK ;
}
