
#include "bank.h"
#include "log.h"
#include "DB_T.h"
#include "utils.h"



static const size_t BIN_LENGTH = 6 ;

Bank_T::Bank_T(DB_T & db): m_db(db)
{
}

Bank_T::~Bank_T()
{}

static std::string make_where_from_search(const Bank_info_T& search)
{
    std::string result = " ( 1 = 1 ) ";
    
    if ( search.id      != 0       ) result += " AND (  id = "      + escape(search.id)       + " ) " ;
    
    if ( search.status  != 0       ) result += " AND ( status = "   + escape(search.status)   + " ) " ;
    
    if ( ! search.bin_code.empty() ) { 
        std::string bin = search.bin_code;
        
        if (bin.length() > BIN_LENGTH ) {
            bin.erase( BIN_LENGTH ); // remove others
        }
        
        result += " AND ( bin_code = " + escape(bin) + " ) " ;
    }
    
    return result;
}

Error_T Bank_T::list(const Bank_info_T &search, const Sort_T &sort, Bank_list_T &list)
{
    SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where_from_search(search);
    std::string query = " SELECT id, name, min_limit, max_limit, rate, merchant_id, terminal_id, port, month_limit, offer_link  "
                        ", status, bin_code, icon_id FROM bank WHERE " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count() ;
    
    list.list.resize(rows);
    
    for(int i= 0; i < rows; ++i)
    {
        Bank_info_T & info = list.list[i];
        
        st.row(i) >> info.id   >> info.name        >> info.min_limit  >> info.max_limit     >> info.rate     >> info.merchantId >> info.terminalId 
                  >> info.port >> info.month_limit >> info.offer_link >> info.status        >> info.bin_code >> info.icon_id ;
    }
    
    int const total_cnt = sort.total_count( rows );
    
    if ( total_cnt >= 0){
        list.count = total_cnt;
        return Error_OK ;
    }
    else
    {
        query = "SELECT count(*) FROM bank WHERE " + where_s ;
            
        st.prepare(query);
    
        st.row(0) >> list.count;
    }
    
    return Error_OK;
}

Error_T Bank_T::info( int32_t id, Bank_info_T & info)
{
    SCOPE_LOG(slog);
    std::string query = " SELECT id, name, min_limit, max_limit, rate, merchant_id, terminal_id, port, month_limit, offer_link,  status, bin_code, icon_id "
                        " FROM bank  WHERE id = " + escape(id);

    DB_T::statement st(m_db);
    
    st.prepare( query);
    
    if (st.rows_count() != 1) return Error_not_found;
    
    st.row(0) >> info.id >> info.name >> info.min_limit >> info.max_limit >> info.rate >> info.merchantId >> info.terminalId >> info.port 
            >> info.month_limit >> info.offer_link   >> info.status >> info.bin_code >> info.icon_id ;
    
    return Error_OK;
}

Bank_info_T Bank_T::info(const Bank_info_T& search, Error_T & ec)
{
    SCOPE_LOG( slog );
    
    Bank_info_T info;
    ec = Error_OK ;
    
    std::string query = " SELECT id, name, min_limit, max_limit, rate, merchant_id, terminal_id, port, month_limit, offer_link, status, bin_code, icon_id "
                        " FROM bank  WHERE " + make_where_from_search( search );

    DB_T::statement st(m_db);
    
    st.prepare( query);
    
    if (st.rows_count() != 1 ) 
    { 
        ec =  Error_not_found; 
    }
    else 
    {
        st.row(0) >> info.id >> info.name >> info.min_limit >> info.max_limit >> info.rate >> info.merchantId >> info.terminalId >> info.port 
                >> info.month_limit >> info.offer_link       >> info.status >> info.bin_code >> info.icon_id ;

        ec =  Error_OK;
    }
    return info;
}

Error_T Bank_T::add(const Bank_info_T &b , /*out*/  uint32_t& id)
{
    SCOPE_LOG(slog);
    std::string query =
            "INSERT INTO bank (name, min_limit, max_limit, rate, merchant_id, terminal_id, port, month_limit, offer_link, status, bin_code) VALUES ( "
            + escape(b.name)       + ", " 
            + escape(b.min_limit ) + ", "
            + escape(b.max_limit)  + ", "
            + escape(b.rate)       + ", "
            + escape(b.merchantId) + ", "
            + escape(b.terminalId) + ", "
            + escape(b.port )      + ", "
            + escape(b.month_limit)+ ", "
            + escape(b.offer_link) + ", "
            + escape(b.status )    + ", "
            + escape(b.bin_code)   + ") RETURNING id " ;
    
    DB_T::statement st(m_db);

    st.prepare( query) ; 
    
    st.row(0) >> id;
    
    return Error_OK ;
}

Error_T Bank_T::edit_icon_id(int32_t id,  int64_t icon_id)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE bank SET icon_id = " + escape(icon_id) + " WHERE id = "+ escape(id);
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Bank_T::edit(const Bank_info_T &info)
{
    SCOPE_LOG(slog);
    if(info.id == 0) {
        slog.WarningLog("Empty field");
        return Error_login_empty;
    }

    std::string query;
    query = "  UPDATE bank SET \n"
            "  name        = " + escape(info.name)         + ", \n"
            "  min_limit   = " + escape(info.min_limit)    + ", \n"
            "  max_limit   = " + escape(info.max_limit)    + ", \n"
            "  rate        = " + escape(info.rate)         + ", \n"
            "  merchant_id = " + escape(info.merchantId)   + ", \n"
            "  terminal_id = " + escape(info.terminalId)   + ", \n"
            "  port        = " + escape(info.port)         + ", \n"
            "  month_limit = " + escape(info.month_limit)  + ", \n"
            "  offer_link  = " + escape(info.offer_link)   + ", \n"
            "  status      = " + escape(info.status )      + ", \n"
            "  bin_code    = " + escape(info.bin_code)     + "  \n" 
            "  WHERE  id   = " + escape(info.id)             ;

    DB_T::statement st(m_db);

    st.prepare(query);
    
    return Error_OK ;
}

Error_T Bank_T::del(uint32_t id)
{
    SCOPE_LOG(slog);
    //////////////////////////////////////////|==0123456789==|
    std::string query   = "DELETE FROM bank WHERE id = " + escape(id) ; 
         
    DB_T::statement st( m_db );
    st.prepare( query  );
    
    return Error_OK ;
 }

///////////////////// BANK_BONUS TABLE /////////////////////////////////////////////////

static std::string make_where(const Bank_bonus_info_T& search)
{
    std::string result = " ( 1 = 1) ";
    
    if ( search.id != 0 )
        result += " AND ( id = " + escape(search.id) + " ) " ;
    
    if ( search.bank_id != 0 )
        result += " AND ( bank_id = " + escape(search.bank_id) + " ) ";
    
    if ( search.status != 0 )
        result += " AND ( status = " + escape(search.status) + " ) " ;
    
    return result;
}

Error_T Bank_T::bonus_list(const Bank_bonus_info_T& search, const Sort_T& sort, Bank_bonus_list_T& list)
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT  id, bank_id, min_amount, percent, start_date, end_date, status, longitude, latitude, description, bonus_amount  "
                        "FROM bank_bonus WHERE " + make_where(search) + sort.to_string() ;

    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    list.resize(rows);
    for(int i = 0; i < rows; ++i)
    {
        Bank_bonus_info_T& info  = list[i];
        st.row(i) >> info.id     >> info.bank_id   >> info.min_amount >> info.percent     >> info.start_date   >> info.end_date 
                  >> info.status >> info.longitude >> info.latitude   >> info.description >> info.bonus_amount ;
    }
    return Error_OK ;
}

Error_T Bank_T::bonus_info(const Bank_bonus_info_T& search, Bank_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    std::string where_s =  make_where(search) ;
    std::string query = " SELECT id, bank_id, min_amount, percent, start_date, end_date, status, longitude, latitude, description, bonus_amount "
                        " FROM bank_bonus WHERE " + where_s;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    
    if (rows != 1)
        return Error_not_found;
    
    st.row(0) >> info.id     >> info.bank_id   >> info.min_amount >> info.percent     >> info.start_date   >> info.end_date 
              >> info.status >> info.longitude >> info.latitude   >> info.description >> info.bonus_amount ;

    return Error_OK ;
}

Error_T Bank_T::bonus_add(const Bank_bonus_info_T& info, /*out*/ uint32_t& id)
{
    SCOPE_LOG(slog);
    std::string query  =
    "INSERT INTO bank_bonus (id, bank_id, min_amount, percent, start_date, end_date, status, longitude, latitude, description, bonus_amount ) "
    " VALUES ( DEFAULT, " + escape(info.bank_id)      + 
                   ", "   + escape(info.min_amount)   + 
                   ", "   + escape(info.percent)      + 
                   ", "   + escape(info.start_date)   + 
                   ", "   + escape(info.end_date)     + 
                   ", "   + escape(info.status)       + 
                   ", "   + escape(info.longitude)    + 
                   ", "   + escape(info.latitude)     + 
                   ", "   + escape(info.description)  + 
                   ", "   + escape(info.bonus_amount) + 
                   " ) RETURNING id "  ;

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> id;
    
    return Error_OK ;
}

Error_T Bank_T::bonus_edit(const Bank_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    
    std::string query = 
            "UPDATE bank_bonus SET  "
            "  bank_id = "        + escape(info.bank_id)      + 
            ", min_amount = "     + escape(info.min_amount)   + 
            ", percent = "        + escape(info.percent)      + 
            ", start_date = "     + escape(info.start_date)   + 
            ", end_date = "       + escape(info.end_date)     + 
            ", status = "         + escape(info.status)       + 
            ", longitude = "      + escape(info.longitude)    + 
            ", latitude = "       + escape(info.latitude)     + 
            ", description  = "   + escape(info.description)  + 
            ", bonus_amount = "   + escape(info.bonus_amount) ; 
            
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return Error_OK;
}

Error_T Bank_T::bonus_del(uint32_t id)
{
    SCOPE_LOG(slog);
    
    std::string query   = "DELETE FROM bank_bonus WHERE id = " + escape( id ) ;
     
    DB_T::statement st( m_db );
    
    st.prepare(  query  );
    
    return Error_OK ;
}

