
#include <utility> // C++11 std::swap.
#include <iterator> // std::begin, std::end
#include <algorithm> // std::swap, std::all_of (c++11).

#include "cards.h"
#include "log.h"
#include "utils.h"
#include "DB_T.h"

std::string expire_date_rotate(std::string expire)
{
    if (expire.length() < 4) {
        return expire;
    }
    
    // expire: 1423  -->  2314
    // index : 0123  --> 0<-->2 , 1<-->3
    
    using std::swap;
    swap(expire[0], expire[2]);
    swap(expire[1], expire[3]);
    return expire;
}
bool   is_valid_expire_now( std::string expire)
{
    if (expire.length() != 4)
        return false;
        
    expire = expire_date_rotate(expire);
    //MMYY --> YYMM format.
     
    std::string now_expire = formatted_time_now("%y%m")   ;
    
    //There expire  and now_expire are   YYMM format, we can compare they as string.
    return expire >= now_expire;

}


bool   is_bonus_card(DB_T& db, int64_t card_id)
{
    //////////////////////////////////////////////////////|-01234567899876543210-|/
    std::string query  = "SELECT 1 FROM card_bonus WHERE card_id = " + escape( card_id ) ;

    DB_T::statement st( db ) ;

    st.prepare(  query  ) ; 
    
    return st.rows_count() > 0 ;
}
/******************************************************************************************************/
 Cards_cabinet_table_T:: Cards_cabinet_table_T(DB_T& db) : m_db( db )
 {}

//return id
int64_t Cards_cabinet_table_T::add( const Card_cabinet_info_T & ci ) 
{
    SCOPE_LOG(slog);
    std::string query = "INSERT INTO card_monitoring_cabinet "
            " ( id, card_id, monitoring_flag, add_ts, start_date, end_date, purchase_id, status, off_ts, uid ) "
            " VALUES ( DEFAULT " 
            " , " + escape( ci.card_id )          +
            " , " + escape( ci.monitoring_flag )  + 
            " , " + escape( ci.add_ts )           +
            " , " + escape( ci.start_date )       + 
            " , " + escape( ci.end_date )         + 
            " , " + escape( ci.purchase_id )      + 
            " , " + escape( ci.status )           + 
            " , " + escape( ci.off_ts )           +
            " , " + escape( ci.uid    )           +
            " ) RETURNING id " 
            ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int64_t id = 0;

    st.row( 0 ) >> id ;
    
    slog.InfoLog("id = %ld ", id);
    
    return id;
}

//return number of rows
int  Cards_cabinet_table_T::edit(int64_t id, const Card_cabinet_info_T& ci ) 
{
    SCOPE_LOG(slog);
    std::string query = 
            "UPDATE card_monitoring_cabinet SET " 
            " monitoring_flag = " + escape(ci.monitoring_flag) + 
            ", purchase_id = "    + escape(ci.purchase_id )    +  
            ", status = "         + escape(ci.status)          + 
            ", off_ts = "         + escape(ci.off_ts )         +
            ", card_id = "        + escape(ci.card_id)         +
            " WHERE id = "        + escape(id) ;
    
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return st.rows_count() ;
}

int Cards_cabinet_table_T::del(int64_t id)
{
    SCOPE_LOG(slog);
    slog.WarningLog("There no required delete row!");
    return 0 ;
}

static std::string make_where(const Card_cabinet_info_T& search)
{
    std::string result = " ( 1 = 1 ) " ; 
    
    if (search.id != 0 ) {
        result += " AND ( id = " + escape(search.id ) + " ) " ;
    }
    
    if(search.card_id != 0 ) {
        result += " AND ( card_id = " + escape(search.card_id ) + " ) " ;
    }
    
    if (search.status != 0 ) { 
        result += " AND ( status = " + escape(search.status ) + " ) " ;
    }
    
    if (search.monitoring_flag != 0 ) {
        result += " AND ( monitoring_flag = " + escape(search.monitoring_flag) + " ) " ;
    }
    
    if (search.purchase_id != 0 ) {
        result += " AND ( purchase_id = " + escape(search.purchase_id) + " ) "  ;
    }
    
    if (search.uid != 0 ) {
        result += " AND ( uid = " + escape( search.uid ) + " ) " ;
    }
    
    return result;
}

Error_T Cards_cabinet_table_T::info(const Card_cabinet_info_T& search, Card_cabinet_info_T& out)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT id, card_id, monitoring_flag, add_ts, start_date, end_date, purchase_id, status, off_ts, uid "
                        " FROM card_monitoring_cabinet WHERE " + make_where(search) + " LIMIT 32 " ;
 
    DB_T::statement st(m_db);
    st.prepare(query);
    if (st.rows_count() != 1 ) 
    {
        return Error_not_found;
    }
    
    st.row(0) >> out.id >> out.card_id >> out.monitoring_flag >> out.add_ts >> out.start_date >> out.end_date
              >> out.purchase_id >> out.status >> out.off_ts >> out.uid ;
    return Error_OK ;
}

Error_T Cards_cabinet_table_T::list(const Card_cabinet_info_T& search, const Sort_T& sort, /*out*/ Card_cabinet_list_T& out ) 
{
    SCOPE_LOG(slog);
    
    std::string where_s = " (status = 1 ) AND ( uid = " + escape(search.uid ) + " ) " ;
    
    if ( ! search.start_date.empty() ) {
        where_s += " AND ( start_date >= " + escape(search.start_date) + ") " ;
    }
    
    if ( ! search.end_date.empty() ) {
        where_s += " AND ( end_date <= " + escape(search.end_date) + " ) " ;
    }
    
    std::string query = "SELECT id, card_id, monitoring_flag, add_ts, start_date, end_date, purchase_id, status, off_ts, uid "
                        " FROM card_monitoring_cabinet WHERE " + where_s + sort.to_string();
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count() ;
    out.list.resize(rows);
    for(int i = 0; i< rows; ++i)
    {
        Card_cabinet_info_T& c = out.list[ i ] ;
        st.row(i) >> c.id >> c.card_id >> c.monitoring_flag >> c.add_ts >> c.start_date >> c.end_date >> c.purchase_id >> c.status >> c.off_ts >> c.uid ;
    }
    
    out.total_count = sort.total_count( rows ) ;
    
    if ( out.total_count < 0 ) {
        query = "SELECT COUNT(*) FROM card_monitoring_cabinet WHERE " + where_s ;
        st.prepare(query);
        st.row(0) >> out.total_count ;
    }
    
    return Error_OK ;
}

Error_T Cards_cabinet_table_T::last_info(int64_t uid,   Card_cabinet_info_T& out)
{
    SCOPE_LOG(slog);
    std::string query = " SELECT id, card_id, monitoring_flag, add_ts, start_date, end_date, purchase_id, status, off_ts , uid"
                        " FROM card_monitoring_cabinet WHERE uid = " + escape(uid) + 
                        " AND end_date >= now()::date AND start_date <= now()::date ORDER BY add_ts DESC LIMIT 1 " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (st.rows_count() != 1  ) return Error_not_found;
    
    st.row(0) >> out.id >> out.card_id >> out.monitoring_flag >> out.add_ts >> out.start_date >> out.end_date
              >> out.purchase_id >> out.status >> out.off_ts >> out.uid;
    
    return Error_OK ;
}


int Cards_cabinet_table_T::total_payed(int64_t uid)
{
    SCOPE_LOG(slog); 
    std::string query = " SELECT count(*) FROM card_monitoring_cabinet WHERE purchase_id > 0 AND status = 1 AND  uid = " + escape(uid)  ;

   DB_T::statement st(m_db);

   Error_T ec = Error_OK ;

   st.prepare(query, ec );

   int res = 0;

   if (ec == Error_OK && st.rows_count() == 1 ) 
   {
       st.row(0) >> res;
   }
   slog.InfoLog("count = %d ", res);
   return res;

}

 int Cards_cabinet_table_T::payed_date_count( std::string const & date, int64_t uid )
 {
     SCOPE_LOG(slog); 
     std::string query = " SELECT count(*) FROM card_monitoring_cabinet WHERE purchase_id > 0 AND status = 1 AND "
                         " uid = " + escape(uid) + " AND   start_date  <= " + escape( date )  + " AND end_date >= " + escape( date )  ;
     
     DB_T::statement st(m_db);
     
     Error_T ec = Error_OK ;
     
     st.prepare(query, ec );
     
     int res = 0;
     
     if (ec == Error_OK && st.rows_count() == 1 ) 
     {
         st.row(0) >> res;
     }
     slog.InfoLog("count = %d ", res);
     return res;
 }
 
/*****************************************************************************************************/
 
Cards_monitoring_tarif_table_T::Cards_monitoring_tarif_table_T(DB_T& db)
 : m_db(db)
{}

static std::string make_where (const Card_monitoring_tarif_info_T& search)
{
    std::string result = "(1=1)";
    if (search.status != 0)
    {
        result += " AND ( status = " + escape(search.status) + " ) " ;
    }
    if (search.id != 0 ) {
        result += " AND ( id = " + escape(search.id) + ") " ;
    }
    if (search.mid != 0 ) {
        result += " AND ( mid = " + escape(search.mid ) + " ) " ;
    }
    return result;
}

Error_T Cards_monitoring_tarif_table_T::info(const Card_monitoring_tarif_info_T& search,   /*out*/ Card_monitoring_tarif_info_T& out ) 
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT id, amount, mid, status FROM card_monitoring_tarif WHERE  " + make_where(search)+ "  LIMIT 32 " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    if (st.rows_count() != 1 ) {
        return Error_not_found ;
    }
    st.row(0) >> out.id >> out.amount >> out.mid >> out.status ;
    return Error_OK ;
}
/*****************************************************************************************************/ 

 Cards_monitoring_table_T::Cards_monitoring_table_T(DB_T& db) : m_db(db){}
 
    
int64_t Cards_monitoring_table_T::add(const Card_monitoring_data_T& data)
{
    SCOPE_LOG(slog);

    std::string query = "INSERT INTO card_monitoring ( id, card_id, uid, pan, ts, amount, reversal, credit, refnum, status, oson_pid, oson_tid, merchant_name"
                        ", epos_merchant_id, epos_terminal_id, street, city ) "
                        " VALUES ( DEFAULT " 
                        ", " + escape(data.card_id )  + 
                        ", " + escape(data.uid )      + 
                        ", " + escape(data.pan )      + 
                        ", " + escape(data.ts )       +
                        ", " + escape(data.amount )   +
                        ", " + escape(data.reversal)  + 
                        ", " + escape(data.credit )   +
                        ", " + escape(data.refnum )   +
                        ", " + escape(data.status )   + 
                        ", " + escape(data.oson_pid ) +
                        ", " + escape(data.oson_tid ) +
                        ", " + escape(data.merchant_name) + 
                        ", " + escape(data.epos_merchant_id) + 
                        ", " + escape(data.epos_terminal_id) + 
                        ", " + escape(data.street) + 
                        ", " + escape(data.city)   +
                        " ) RETURNING id " 
             ;
    
    DB_T::statement st(m_db);
    Error_T ec = Error_OK;
    st.prepare(query, /*out*/ec);
    if (ec) return 0;
    int64_t id = 0;
    st.row(0)>>id;
    slog.InfoLog("id = %ld ", id);
    
    return id;
}
 
 

Error_T  Cards_monitoring_table_T::list( const Card_monitoring_search_T& search, const Sort_T& sort,  Card_monitoring_list_T& out ) 
{
    SCOPE_LOG(slog);
    std::string where_s = "card_id = " + escape(search.card_id) +  " AND  ts::date >= " + escape(search.from_date) + " AND ts::date <= " + escape(search.to_date) 
              , sort_s = sort.to_string();
    
    std::string query = "SELECT id, card_id, uid, pan, ts, amount, reversal, credit, refnum, status, oson_pid, oson_tid, merchant_name"
                        ", epos_merchant_id, epos_terminal_id, street, city "
                        " FROM card_monitoring  WHERE  " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rws = st.rows_count() ;
    
    out.list.resize( rws );
    for(int i = 0; i < rws; ++i)
    {
        Card_monitoring_data_T& o = out.list[i];
        st.row(i) >> o.id >> o.card_id >> o.uid >> o.pan >> o.ts >> o.amount >> o.reversal >> o.credit >> o.refnum >> o.status >> o.oson_pid >> o.oson_tid >> o.merchant_name 
                  >> o.epos_merchant_id >> o.epos_terminal_id >> o.street >> o.city ;
        
    }
    
    out.total_count = sort.total_count(rws);
    
    if (out.total_count == -1 ){
        query = "SELECT count(*) FROM card_monitoring WHERE " + where_s ;
        st.prepare(query);
        st.row(0) >> out.total_count ;
    }
    return Error_OK ;
}
/******************************************************************************************************/    
Cards_monitoring_load_table_T:: Cards_monitoring_load_table_T(DB_T& db) : m_db(db){}

//return a new created row id.
int64_t Cards_monitoring_load_table_T::add(const Card_monitoring_load_data_T& data)
{
    SCOPE_LOG(slog);
    
    std::string query = "INSERT INTO card_monitoring_load ( id, card_id, from_date, to_date, ts, status ) VALUES ( DEFAULT, "  + 
            escape(data.card_id )  + ", " + 
            escape(data.from_date) + ", " + 
            escape(data.to_date)   + ", " +
            escape(data.ts)        + ", " + 
            escape(data.status)    + 
            ") RETURNING id " ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int64_t id = 0;
    
    st.row(0) >> id ;
    
    slog.InfoLog("id = %ld ", id ) ;
    
    return id; 
}

int Cards_monitoring_load_table_T::set_status(int64_t id, int32_t status)
{
    SCOPE_LOG(slog);
    
    std::string query = "UPDATE card_monitoring_load SET status = " + escape(status) + " WHERE id = " + escape(id) ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return st.affected_rows();
}

int Cards_monitoring_load_table_T::del(const Card_monitoring_load_data_T& search)
{
    SCOPE_LOG(slog);
    std::string query = " DELETE from card_monitoring_load WHERE ( card_id = " 
            + escape(search.card_id) + ") AND ( status = 15 ) AND ( from_date = "
            + escape(search.from_date) + " ) AND ( to_date =  " 
            + escape(search.to_date) + ") " ;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return st.affected_rows();
}
        
int Cards_monitoring_load_table_T::loaded( const Card_monitoring_load_data_T & search )
{
    SCOPE_LOG(slog);
    
    //status = 1 success, 6 - in-progress i.e. there no determined oson_pid and oson_tid.
    // status = 15 - error load.
    std::string query = "SELECT count(*) FROM card_monitoring_load WHERE ( card_id = " + escape(search.card_id) + 
              " ) AND  (status = 1 OR status = 6 ) AND ( from_date <= " + escape(search.from_date) + " ) AND  ( to_date >= " + escape(search.to_date) + " ) " ;
    
    DB_T::statement st(m_db);
     
    st.prepare(query );
    
    int res = 0;
    st.row(0) >> res;
    
    slog.InfoLog("res = %d ", res);
    return res;
    
}


 
/*****************************************************************************************************/
Cards_T::Cards_T( DB_T & db ): m_db(db) {
	
}

Error_T Cards_T::card_count( uint64_t uid, std::string pc_token, size_t& count)
{
    SCOPE_LOG(slog);
    std::string query = "SELECT count(*) FROM cards WHERE ( uid = " + escape( uid ) + ") AND ( pc_token = " + escape(pc_token) + ") " ;
    
    DB_T::statement st(m_db);
    
    st.prepare( query) ;
    
    st.row( 0 ) >> count;
   
    return Error_OK;
}

static std::string make_where(const Card_info_T& search){
    std::string result =  " ( 1 = 1 ) " ;
    
    if (search.uid != 0) 
        result += " AND ( uid = " + escape(search.uid) + ") " ;
    
    if (search.id  != 0) 
        result += " AND ( card_id  = " + escape(search.id ) + ") " ;
    
    if (search.pan.size() != 0) 
        result += " AND ( number = " + escape(search.pan) + ") ";
    
    if (search.is_primary != PRIMARY_UNDEF) 
        result += " AND (is_primary = " + escape(search.is_primary) + ") ";
    
    if (search.foreign_card != FOREIGN_UNDEF) 
        result += " AND ( foreign_card = " + escape(search.foreign_card) + ") ";
    
    if (search.pc_token.size() > 0 )
        result += " AND ( pc_token  = " + escape(search.pc_token) + ") " ;
        
    if (search.owner_phone.size() > 0) {
        
        std::string phone = search.owner_phone;
        
        std::replace( phone.begin(), phone.end(), '*', '%');//postresql uses '%' for *
        std::replace( phone.begin(), phone.end(), '?', '_');//postresql uses '_' for ?

        if (phone.length() == 12 && std::all_of( std::begin(phone), std::end(phone), ::isdigit) ) { // a full version 
            result += " AND ( owner_phone	= " + escape(phone) + ") " ;
        } 
        else if (phone.length() <= 12 ) // short version
        {
            if (phone[ 0 ]  != '%'  )
                phone.insert(phone.begin(), '%');
            
            result += " AND ( owner_phone LIKE " + escape( phone) + " ) " ;
        } 
        else { // length is greater thatn 12 - incorrect phone
            result += " AND ( 0 = 1 ) " ;//always false
        }
    }
    
    return result;
}

size_t Cards_T::card_count( int64_t uid ) 
{
    SCOPE_LOG(slog);
    ////////////////////////////////////////////////////////////////////////////
    std::string query = "SELECT COUNT(*) FROM cards WHERE uid = " + escape(uid) ;
    
    //////////////////////////////////////////////////////////////////////////////
    
    DB_T::statement st(m_db);
    st.prepare(  query );
    return st.get_int( 0, 0 );
}

std::vector< Card_info_T> Cards_T::card_list( int64_t uid )
{
    SCOPE_LOG(slog);
    
    std::string query = "SELECT card_id, number, expire, uid, is_primary, name, tpl, "
                 "tr_limit, block, user_block, pc_token, owner, foreign_card, "
                 "owner_phone, daily_limit  FROM cards WHERE uid = " + escape(uid) + 
                 "  ORDER BY card_id  LIMIT 32  " ;//no more 32 cards allowed by user

    DB_T::statement st(m_db);

    st.prepare(  query  );
    
    std::vector< Card_info_T> result;
    
    size_t rows = st.rows_count() ;
    result.resize(rows);
    for(size_t i = 0; i != rows; ++i){
        Card_info_T& info = result[i];
        st.row(i) >> info.id         >> info.pan         >> info.expire     >> info.uid            >> info.is_primary 
                  >> info.name       >> info.tpl         >> info.tr_limit   >> info.admin_block 
                  >> info.user_block >> info.pc_token    >> info.owner      >> info.foreign_card   >> info.owner_phone
                  >> info.daily_limit ;
    }
    return result;
}


Error_T Cards_T::card_list(const Card_info_T & search, const Sort_T &sort, Card_list_T & list)
{
	SCOPE_LOG(slog);
    std::string sort_s = sort.to_string(), where_s = make_where(search);
    std::string query = " SELECT card_id, number, expire, uid, is_primary, name, tpl, tr_limit, block, user_block, pc_token, owner, foreign_card, owner_phone, daily_limit "
                        " FROM cards WHERE " + where_s + sort_s;
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    int rows = st.rows_count();
    list.list.resize(rows);
    for(int i= 0; i < rows; ++i){
        Card_info_T& info = list.list[i];
        st.row(i) >> info.id         >> info.pan         >> info.expire     >> info.uid            >> info.is_primary 
                  >> info.name       >> info.tpl         >> info.tr_limit   >> info.admin_block 
                  >> info.user_block >> info.pc_token    >> info.owner      >> info.foreign_card   >> info.owner_phone
                  >> info.daily_limit ;
    }
    
    int const total_cnt = sort.total_count(rows);
    if ( total_cnt  >= 0) {
        list.count = total_cnt;
        return Error_OK ;
    }
    
    query = "SELECT count(*) FROM cards WHERE "+ where_s ;

    st.prepare(query);
    
    st.row(0) >> list.count ;
    
	return Error_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///                          BONUS CARD 
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "users.h"
Error_T Cards_T::make_bonus_card( uint64_t uid,  Card_info_T& out)
{
    SCOPE_LOG(slog);

    /******************* 1. GET BONUS INFO FOR USER *********************/
    Users_bonus_T users_b( m_db);
    User_bonus_info_T binfo;
    Error_T ec = users_b.bonus_info( uid, binfo);
    if (ec)
        return ec;  // there no bonus card for this user.
    
    /********************* 2. GET BONUS CARD INFO ************************/
    Card_bonus_info_T bonus_card;
    ec = this->bonus_card_info( binfo.bonus_card_id, bonus_card ) ;
    if (ec)
        return ec;  // there no bonus card on database , seems binfo.bonus_card_id  invalid ID.
    
    out.id            = bonus_card.card_id;
    out.uid           = binfo.uid;
    out.tpl           = bonus_card.tpl;
    out.user_block    = (binfo.block == 1 ? 0 : 1); 
    out.foreign_card  = FOREIGN_NO;
    out.pan           = binfo.pan; 
    out.expire        = binfo.expire;
    out.name          = binfo.name;
    out.owner         = binfo.fio;
    out.deposit       = binfo.balance;
    out.isbonus_card       = 1; //1-this bonus card.
     
    return Error_OK ;
}

Error_T Cards_T::bonus_card_add(Card_bonus_info_T& info) 
{
    SCOPE_LOG(slog);
    
    std::string query = "INSERT INTO card_bonus (card_id, number, pc_token, owner, tpl, expire, xid, name, balance, password) VALUES ( DEFAULT, " 
            + escape(info.number)   + ", " 
            + escape(info.pc_token) + ", "
            + escape(info.owner)    + ", "
            + escape(info.tpl)      + ", "
            + escape(info.expire)   + ", " 
            + escape(info.xid)      + ", " 
            + escape(info.name)     + ", "
            + escape(info.balance)  + ", "
            + "crypt( '" + info.password +  "', gen_salt('md5') ) ) RETURNING card_id " ;
    
   
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    st.row(0) >> info.card_id  ;
    
    return Error_OK ;
}

Error_T Cards_T::bonus_card_edit(uint64_t card_id, const std::string& passwd, Card_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    
    if (card_id == 0){
        slog.WarningLog("card id is zero!");
        return Error_parameters;
    }
    
    std::string query = " UPDATE card_bonus SET "
                        " number = "        + escape(info.number) + 
                        ", expire = "       + escape(info.expire) + 
                        ", xid = "          + escape(info.xid)    + 
                        ", name = "         + escape(info.name)   + 
                        " WHERE card_id = " + escape(card_id)     + 
                        " AND  password = crypt( '" + passwd + "', password) ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if (! st.affected_rows() )
        return Error_not_found;
    
    return Error_OK ;
}
Error_T Cards_T::bonus_card_edit_balance(uint64_t card_id, const Card_bonus_info_T& bonus_info)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE card_bonus SET "
                        "balance = " + escape(bonus_info.balance) + " "
                        "WHERE card_id = " + escape(card_id);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    return Error_OK ;
}
    
Error_T Cards_T::bonus_card_delete(uint64_t card_id, const std::string& passwd)
{
    SCOPE_LOG(slog);
    if (card_id == 0)
    {
        slog.WarningLog("card_id is zero!");
        return Error_parameters;
    }
    
    std::string query = " DELETE FROM card_bonus WHERE card_id = " + escape(card_id) + " AND password = crypt( '" + passwd  + "' , password) ";
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    if ( ! st.affected_rows() )
        return Error_not_found;
    
    return Error_OK ;
}
        
Error_T Cards_T::bonus_card_list(uint64_t card_id, Card_bonus_list_T & list)
{
    SCOPE_LOG(slog);
    std::string where_s = " (1 = 1 ) " ;
    if(card_id != 0) 
        where_s += " AND ( card_id = " + escape(card_id) + " ) " ;
    
    std::string query = " SELECT card_id, number, expire, xid, name, tpl, pc_token, owner, balance FROM card_bonus WHERE " + where_s;

    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    int rows = st.rows_count();
    
    list.resize(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        Card_bonus_info_T& info = list[i];
        st.row(i) >> info.card_id >> info.number >> info.expire >> info.xid >> info.name >> info.tpl >> info.pc_token >> info.owner >> info.balance;
    }
    
    
    return Error_OK;
}
    
Error_T Cards_T::bonus_card_info(uint64_t card_id, Card_bonus_info_T& info)
{
    SCOPE_LOG(slog);
    
    std::string query = " SELECT card_id, number, expire, xid, name, tpl, pc_token, owner, balance FROM card_bonus WHERE card_id = "  + escape(card_id);

    DB_T::statement st(m_db);
    st.prepare(query);
    
    if (st.rows_count() != 1)
        return Error_not_found;
    
    st.row(0) >> info.card_id >> info.number >> info.expire >> info.xid >> info.name >> info.tpl >> info.pc_token >> info.owner >> info.balance ;
    
    return Error_OK;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////
Card_info_T Cards_T::get(int64_t card_id, Error_T& ec)
{
    SCOPE_LOG(slog);
    Card_info_T info;
    /////////////////////////////////////////////////////////
    std::string query =  "SELECT card_id, number, expire, uid, is_primary, name, tpl, tr_limit, block, user_block,  pc_token, owner, foreign_card, owner_phone, daily_limit "
                         " FROM cards WHERE card_id  = " + escape( card_id );

    DB_T::statement st(m_db);

    st.prepare(  query, ec );
    
    if (ec)
    {
        ;
    }
    else if (st.rows_count() != 1)
    {
        ec = Error_card_not_found ;
    } 
    else 
    {
        st.row(0) >> info.id     >> info.pan         >> info.expire     >> info.uid            >> info.is_primary 
              >> info.name       >> info.tpl         >> info.tr_limit   >> info.admin_block 
              >> info.user_block >> info.pc_token    >> info.owner      >> info.foreign_card   >> info.owner_phone
              >> info.daily_limit ;
        ec = Error_OK ;
    }
    return info;
}

Card_topup_info_T Cards_T::get_topup_by_id(int64_t card_id, Error_T& ec ) 
{
    SCOPE_LOG(slog);
    Card_topup_info_T info;
    /////////////////////////////////////////////////////////
    std::string query =  "SELECT card_id, number, expire, status,  name,   pc_token, owner, balance  "
                         " FROM card_topup_master WHERE card_id  = " + escape( card_id );

    DB_T::statement st(m_db);

    st.prepare(  query, ec );
    
    if (ec)
    {
        ;
    }
    else if (st.rows_count() != 1)
    {
        ec = Error_card_not_found ;
    } 
    else 
    {
        st.row(0) >> info.card_id  >> info.number    >> info.expire  >> info.status  
                  >> info.name     >> info.pc_token  >> info.owner   >> info.balance   ;
                   
              
        ec = Error_OK ;
    }
    return info;
}

int64_t  Cards_T::card_add(const Card_info_T & data)
{
	SCOPE_LOG(slog);
    std::string pan = oson::utils::mask_pan(data.pan);  
    std::string query =  
    "INSERT INTO cards (number, expire, uid, is_primary, name, tpl, owner, pc_token, foreign_card, owner_phone) VALUES ( "
            + escape(pan)                  + ", "
            + escape(data.expire)          + ", "
            + escape(data.uid)             + ", "
            + escape(data.is_primary)      + ", "
            + escape(data.name)            + ", "
            + escape(data.tpl)             + ", "
            + escape(data.owner)           + ", "
            + escape(data.pc_token)        + ", "
            + escape(data.foreign_card)    + ", "
            + escape(data.owner_phone)     + ") "
            " RETURNING card_id " ;
    
    

    DB_T::statement st(m_db);
    
    st.prepare(query);
    int64_t  id = 0;
    st.row(0) >> id;
    
    return id;
}

Error_T Cards_T::info(const Card_info_T &search, Card_info_T &info)
{
    SCOPE_LOG(slog);
    
    std::string  where_s = make_where(search);
    std::string query = 
            "SELECT card_id, number, expire, uid, is_primary, name, tpl, tr_limit, block, user_block,  pc_token, owner, foreign_card, owner_phone, daily_limit "
            " FROM cards WHERE " + where_s + " ORDER BY card_id LIMIT 32 "; 
                
    //@Note: if search is empty, there may be full table, to avoid it added 'LIMIT 32'.

    DB_T::statement st( m_db ) ;
    
    st.prepare( query);
    
    if( st.rows_count() != 1) {
        slog.WarningLog( "card not found: %d entry", st.rows_count() );
        return Error_card_not_found;
    }
    
    st.row(0) >> info.id     >> info.pan         >> info.expire     >> info.uid            >> info.is_primary 
          >> info.name       >> info.tpl         >> info.tr_limit   >> info.admin_block 
          >> info.user_block >> info.pc_token    >> info.owner      >> info.foreign_card   >> info.owner_phone
          >> info.daily_limit ;

    
    return Error_OK ;
}

Error_T Cards_T::card_edit(uint64_t id, const Card_info_T &info)
{
    SCOPE_LOG(slog);
    
    std::string query, block = " user_block " ; // DB itself value; 
    if (info.user_block != 0)
        block = escape( info.user_block    ) ;//convert it to DB value: 0 - non block, 1 - block
    
    query = "UPDATE cards SET "
            " name = "            + escape(info.name)            + ", "
            " tpl  = "            + escape(info.tpl)             + ", "
            " user_block = "      + block                        + ", "
            " foreign_card =    " + escape(info.foreign_card)    + " "
            " WHERE card_id =   " + escape(id);

    
    DB_T::statement st(m_db);

     st.prepare(query);
     return Error_OK ;
}

Error_T Cards_T::card_edit_owner(uint64_t id, const std::string& owner_phone)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE cards SET owner_phone =  " +   escape(owner_phone)    + "  WHERE card_id = " + escape(id) + ";" ;

    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Cards_T::card_admin_edit(uint64_t id, const Card_info_T &info)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE cards SET tr_limit = " + escape( info.tr_limit ) + ", block = " + escape( info.admin_block ) + " WHERE card_id = " + escape( id );

    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Cards_T::card_delete(int64_t id)
{
    SCOPE_LOG(slog);
    std::string query  = "DELETE FROM cards WHERE card_id = " + escape(id) ;
     
    DB_T::statement st(m_db);
    st.prepare( query  );
    return Error_OK ;
}

Error_T Cards_T::unchek_primary(uint64_t uid)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE cards SET is_primary = " + escape( PRIMARY_NO ) + " WHERE uid = " + escape(uid) + " ; "; 
    

    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

Error_T Cards_T::set_primary( int64_t id)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE cards SET is_primary = " + escape( PRIMARY_YES ) + " WHERE card_id = " + escape(id) + " ; "; 
 
    DB_T::statement st(m_db);
    st.prepare(query);
    return Error_OK ;
}

 