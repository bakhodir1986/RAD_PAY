
#include "fault.h"
#include "log.h"
#include "DB_T.h"
#include "utils.h" // to_str


Fault_info_T::Fault_info_T() 
  : id(0)
  , type(FAULT_TYPE_UNDEF)
  , status(FAULT_STATUS_UNDEF)
  , ts_days(0)
{}
    
Fault_info_T::Fault_info_T(integer type, integer status, const text& description)
: id(0)
, type(type)
, status(status)
, description(description)
{}


bool Fault_info_T::empty() const
{
    return ( 0 == id ) &&  
           ( FAULT_TYPE_UNDEF == type )  &&  
           ( FAULT_STATUS_UNDEF == status ) && 
           ( ts_notify.empty() );
}
/***************************************************************/
Fault_T::Fault_T(DB_T & db) : m_db(db)
{
  
}

Error_T Fault_T::add(const Fault_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query = 
            "INSERT INTO fault (type, status, ts, description) VALUES( " 
            + escape(info.type )             + ", " 
            + escape(info.status)            + ", "
            "NOW(), "  
            + escape(info.description)       + " ) " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK; 
}

static std::string make_where(const Fault_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;
    
    if (search.id != 0)
        result += " AND ( id = " + escape(search.id) + " ) " ;
    
    if (search.type != FAULT_TYPE_UNDEF)
        result += " AND ( type = " + escape(search.type) + " ) ";
    
    if (search.status != FAULT_STATUS_UNDEF)
        result += " AND ( status = " + escape(search.status) + " ) ";
    
    if (search.ts_days != 0 )
        result += " AND ( ts >= NOW() - INTERVAL '"+to_str(search.ts_days) + " day' ) " ;
    
    if ( ! search.ts_notify.empty() )
        result += " AND ( (ts_notify IS NULL) OR ( ts_notify <= NOW() - INTERVAL '8 hour' ) )  " ;
    
    return result;
}

Error_T Fault_T::list(const Fault_info_T &search, const Sort_T& sort, Fault_list_T & list)
{
    SCOPE_LOG(slog);
    
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = "SELECT id, type, ts, status, description FROM fault WHERE " + where_s + sort_s + " ; ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query) ;
    
    int rows = st.rows_count();
    list.list.resize( rows );
    
    for(int i= 0; i < rows; ++i)
    {
        Fault_info_T& info = list.list[i];
        st.row(i) >> info.id >> info.type >> info.ts >> info.status >> info.description ;
    }
    
    int const total_cnt = sort.total_count(rows);
    
    if ( total_cnt >= 0 ) 
    {
        list.count = total_cnt;
    }
    else
    {
        query = "SELECT count(*) FROM fault WHERE " + where_s + " ; " ;
   
        st.prepare( query) ;

        st.row( 0 ) >> list.count;
    }

    return Error_OK ;
 
}

Error_T Fault_T::del( int32_t id)
{
    SCOPE_LOG(slog);
    std::string  query  = "DELETE FROM fault WHERE id = " + escape( id )  ;
     
    DB_T::statement st(m_db);
    
    st.prepare(    query   ) ;
    
    return Error_OK;
}

static bool empty(const Fault_info_T& fi)
{
    return  fi.empty(); 
}

Error_T Fault_T::edit(int32_t id, const Fault_info_T &new_data)
{
    SCOPE_LOG(slog);
    if (empty(new_data))
    {
        slog.WarningLog("There no update");
        return Error_OK;
    }
    std::string query = "UPDATE fault SET ";
    
    if(new_data.type != FAULT_TYPE_UNDEF)
        query += "type = " + escape(new_data.type) + ", ";
    
    if(new_data.status != FAULT_STATUS_UNDEF)
        query += "status = "+ escape(new_data.status) + ", ";
    
    if ( ! new_data.ts_notify.empty() )    
        query += " ts_notify = "+escape(new_data.ts_notify)+"  WHERE id = " + escape(id);

    DB_T::statement st(m_db);
    
    st.prepare(query);
    return Error_OK ;
}
