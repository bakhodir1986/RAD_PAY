#include "news.h"
#include "log.h"
#include "DB_T.h"

News_T::News_T(DB_T &db): m_db(db)
{

}

News_T::~News_T()
{}

static std::string make_where(const News_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ;

    if (search.id != 0)
        result += " AND ( id = " + escape(search.id) + " ) " ;

    if (search.lang != 0)
        result += " AND ( lang  = 0 OR lang = " + escape(search.lang) + ") " ;

    if (search.uid != 0)
        result += " AND ( uid = 0  OR uid = " + escape(search.uid ) + " ) ";

    return result;
}

Error_T News_T::news_list(const News_info_T &search, const Sort_T &sort, News_list_T &list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = "SELECT id, msg, add_ts::timestamp(0) FROM news WHERE " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
 
    int rows = st.rows_count();
    list.list.resize(rows);
    
    for( int i = 0; i < rows; i++ ) 
    {
        News_info_T& info = list.list[i];
 
        st.row(i) >> info.id >> info.msg >> info.add_time ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt >= 0){
        list.count = total_cnt;
        return Error_OK ;
    }
    else
    {
        query = "SELECT count(*) FROM news WHERE " + where_s ;
    
        st.prepare(query);

        st.row(0) >> list.count;

        return Error_OK;
    }
}

Error_T News_T::news_add(const News_info_T &info)
{
    SCOPE_LOG(slog);
    
    std::string query = "INSERT INTO news (id, msg, lang, uid) VALUES ( DEFAULT, " + escape(info.msg) + ", " + escape(info.lang)+ ", " + escape(info.uid) +  ") " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    return Error_OK ;
}  


Error_T News_T::news_edit(uint32_t id, const News_info_T &newinfo)
{
    SCOPE_LOG(slog);
    if(id == 0) {
        slog.WarningLog("Empty field");
        return Error_login_empty;
    }

    std::string query = "UPDATE news SET msg = " + escape(newinfo.msg) + ", edit_ts = NOW() WHERE id = " + escape(id);

    //(query.c_str(), query.size());
        
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T News_T::news_delete(uint32_t id)
{
    SCOPE_LOG(slog);
    if(id == 0) {
        slog.WarningLog("Empty field");
        return Error_login_empty;
    }
    std::string query = "DELETE FROM news WHERE id = " + escape(id) ;
    //(query.c_str(), query.size());
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}
