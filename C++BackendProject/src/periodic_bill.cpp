
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "periodic_bill.h"
#include "log.h"
#include "DB_T.h"


/*****************************************************************/
Periodic_bill_data_T::Periodic_bill_data_T() 
    : id(0)
    , uid(0)
    , merchant_id(0)
    , amount(0)
    , card_id(0)
    , periodic_ts()
    , name()
    , prefix()
    , fields()
    , status(0)
{
}

std::string Periodic_bill_data_T::get_login()const
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
/*********************************************************/
Periodic_bill_T::Periodic_bill_T(DB_T &db)
    :m_db(db)
{

}

Error_T Periodic_bill_T::add(Periodic_bill_data_T &b)
{
    SCOPE_LOG(slog);
    std::string query = 
            "INSERT INTO periodic_bill (uid, merchant_id, amount, name, value, periodic_ts, status)  VALUES ( "
            + escape(b.uid)         + ", "
            + escape(b.merchant_id) + ", "
            + escape(b.amount)      + ", "
            + escape(b.name)        + ", "
            + escape(b.fields)      + ", "
            + escape(b.periodic_ts) + ", "
            + escape(b.status)      + ") " 
            ;

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK ;
}


static std::string make_where(const Periodic_bill_data_T& search){
    std::string result = " ( 1 = 1 ) " ;
    if (search.id   != 0) result += " AND ( id = " + escape(search.id)   + ") ";
    if (search.uid  != 0) result += " AND ( uid = " + escape(search.uid ) + ") ";
    if (search.status != PBILL_STATUS_ACTIVE)result += " AND ( status = "+escape(search.status) + ") ";
    
    return result;
}
Error_T Periodic_bill_T::list(const Periodic_bill_data_T &search, const Sort_T& sort,  Periodic_bill_list_T &list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = "SELECT id, uid, merchant_id, amount, name, value, periodic_ts, status FROM periodic_bill WHERE "+where_s + sort_s ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query) ;
    int const n = st.rows_count();
    list.list.resize(n);
    for(int i=  0;  i< n ; ++i){
        Periodic_bill_data_T& info = list.list[i];
        st.row(i) >> info.id >> info.uid >> info.merchant_id >> info.amount >> info.name >> info.fields >> info.periodic_ts >> info.status ;
    }
    
    int const total_cnt = sort.total_count(n);
    if ( total_cnt >= 0) {
        list.count = total_cnt;
        return Error_OK ;
    }
    
    query = "SELECT count(*) FROM periodic_bill WHERE "+ where_s ;
    
    st.prepare(query);
    st.row(0) >> list.count ;
    return Error_OK ;
}

Error_T Periodic_bill_T::list_need_to_bill(const Sort_T & sort,  Periodic_bill_list_T& list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string();
    std::string query = " SELECT  id, uid, merchant_id, amount, name, value, periodic_ts, status "
                        " FROM periodic_bill "
                        " WHERE (status = 0) AND ( (last_bill_ts is NULL) OR ( Now() - last_bill_ts > '1 day') ) AND "
                         " ( (last_notify_ts is NULL) OR ( Now() - last_notify_ts > '8 hour') ) " + sort_s;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int const n = st.rows_count();
    list.list.resize(n);
    for(int i = 0; i < n ; ++i){
        Periodic_bill_data_T& info = list.list[i];
        st.row(i) >> info.id >> info.uid >> info.merchant_id >> info.amount >> info.name >> info.fields >> info.periodic_ts >> info.status ;
    }
    list.count = 0; 
    return Error_OK;
}

Error_T Periodic_bill_T::edit(uint32_t id, const Periodic_bill_data_T &data)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE periodic_bill SET status = " + escape(data.status) + " WHERE id = "+escape(id);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    return Error_OK ;
}

Error_T Periodic_bill_T::del(uint32_t id)
{
    SCOPE_LOG(slog);
    std::string query ="DELETE FROM periodic_bill WHERE id = "+ escape(id) ;
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    return Error_OK ;
}
Error_T Periodic_bill_T::update_last_bill_ts(uint32_t id)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE periodic_bill SET last_bill_ts = now() WHERE id = "+escape(id) ;
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

 Error_T Periodic_bill_T::update_last_notify_ts(uint32_t id)
 {
     SCOPE_LOG(slog);
     std::string query = "UPDATE periodic_bill SET last_notify_ts = now() WHERE id = "+escape(id) ;
     //(query.c_str(), query.size());
    
     DB_T::statement st(m_db);
     st.prepare(query);
     
     return Error_OK; 
 }
 
 