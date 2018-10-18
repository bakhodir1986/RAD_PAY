#ifndef CARDS_H
#define CARDS_H

#include <string>
#include <vector>

#include "types.h"

//forward declarations.
class Sort_T;
class DB_T  ;

enum Card_primary_T 
{
    PRIMARY_UNDEF = 0,
    PRIMARY_NO    = 1,
    PRIMARY_YES   = 2,
    PRIMARY_BONUS = 99
};

enum Card_foreign_T 
{
    FOREIGN_UNDEF = 0,
    FOREIGN_NO    = 1,
    FOREIGN_YES   = 2
};

enum Card_monitoring_flag_T
{
    MONITORING_UNDEF = 0,
    MONITORING_OFF   = 1,
    MONITORING_ON    = 2,
};

struct Card_search_T
{
     int64_t id;
     int64_t uid;
     int32_t is_primary;
     int32_t foreign_card;
     int32_t activation_code;
};


struct Card_info_T 
{
	typedef int64_t      bigint   ;
    typedef int32_t      integer  ;
    typedef std::string  text     ;
    
    bigint  id           ;
    bigint  uid          ;
    bigint  deposit      ;
	text    pan          ;
    text    expire       ;
    text    name         ;
    integer is_primary   ;
    text    owner        ;
    text    owner_phone  ;
    text    pc_token     ;
    integer tpl          ;
    integer tr_limit     ;
    integer admin_block  ;
    integer user_block   ; //
    integer foreign_card ;
    integer isbonus_card      ; // bonus card or not: true - this card is bonus card, false - default user card.
    bigint  daily_limit  ;
    //integer monitoring_flag ;//0-none,  1-disabled  2-enabled, (default is 1 ).
    
	inline Card_info_T() 
     : id(0) 
     , uid(0) 
     , deposit(0) 
     , pan()
     , expire()
     , name()
     , is_primary(PRIMARY_UNDEF)
     , owner()
     , owner_phone()
     , pc_token()
     , tpl(0)
     , tr_limit(0)
     , admin_block(0)
     , user_block(0)
     , foreign_card(FOREIGN_UNDEF)
     , isbonus_card(0)
     , daily_limit(0)
    // , monitoring_flag(0)
    {
	}
};


std::string expire_date_rotate(std::string expire);
bool   is_valid_expire_now( std::string expire);
bool   is_bonus_card(DB_T& db, int64_t card_id);

struct Card_bonus_info_T
{
    
	typedef int64_t      bigint   ;
    typedef int32_t      integer  ;
    typedef std::string  text     ;
    
    bigint card_id ;
    text   number;
    text   expire;
    bigint xid;            // 1 - active bonus card,  2-deactive.
    text name;
    integer tpl;
    text pc_token;
    text owner;
    text password;
    bigint  balance;
    
    inline Card_bonus_info_T()
    : card_id(0)
    , number()
    , expire()
    , xid(0)
    , name()
    , tpl(0)
    , pc_token()
    , owner()
    {}
};

struct Card_topup_info_T
{
    typedef int64_t      bigint   ;
    typedef int32_t      integer  ;
    typedef std::string  text     ;
    
    bigint  card_id ;
    text    number;
    text    expire;
    integer status;
    text     name;
     
    text pc_token;
    text owner;
    text password;
    bigint  balance;
    
    Card_topup_info_T()
    : card_id(0)
    , number()
    , expire()
    , status(0)
    , name()
    , pc_token()
    , owner()
    {}
};

typedef std::vector< Card_bonus_info_T > Card_bonus_list_T;

struct Card_list_T 
{
	uint64_t count;
	std::vector<Card_info_T> list;
	Card_list_T() {
		count = 0;
	}
};

struct Card_cabinet_info_T
{
    typedef int64_t bigint;
    typedef int32_t integer;
    typedef std::string text;
    
    bigint  id              ;
    bigint  card_id         ;
    integer monitoring_flag ;
    text    add_ts          ;
    text    off_ts          ;
    text    start_date      ;
    text    end_date        ;
    integer status          ; // see purchases status
    bigint  purchase_id     ;
    bigint  uid             ;
    
    inline Card_cabinet_info_T():id(0), card_id(0), monitoring_flag(0), status(0), purchase_id(0), uid( 0 ) {}
};

struct Card_cabinet_list_T
{
    int32_t total_count;
    std::vector< Card_cabinet_info_T > list;
};

class Cards_cabinet_table_T
{
public:
    explicit Cards_cabinet_table_T(DB_T& db) ;
    
    //return id
    int64_t add( const Card_cabinet_info_T & ) ;
    
    //return number of rows
    int  edit(int64_t id, const Card_cabinet_info_T& ) ;
    
    int del(int64_t id);
    
    Error_T info(const Card_cabinet_info_T& search, Card_cabinet_info_T& out);
    
    Error_T last_info(int64_t uid,   Card_cabinet_info_T& out);
    
    // how many time payed from year/mon/day   , if day = 0, for year/mon 
    int payed_date_count( std::string const & date, int64_t uid);
    
    int total_payed(int64_t uid);
    
    Error_T list(const Card_cabinet_info_T& search, const Sort_T& sort, /*out*/ Card_cabinet_list_T& out ) ;
private:
    DB_T& m_db;
};


struct Card_monitoring_tarif_info_T
{
    typedef std::string text;
    typedef int64_t bigint;
    typedef int32_t integer;
    
    bigint id;
    bigint amount;
    integer mid;
    integer status;
    
    inline Card_monitoring_tarif_info_T(): id(0), amount(0), mid(0), status(0){}
};

class Cards_monitoring_tarif_table_T
{
public:
    explicit Cards_monitoring_tarif_table_T(DB_T& db);
    
    Error_T info(const Card_monitoring_tarif_info_T& search,   /*out*/ Card_monitoring_tarif_info_T& out ) ;
private:
    DB_T& m_db;
};


struct Card_monitoring_data_T
{
    typedef int64_t bigint;
    typedef int32_t integer;
    typedef ::std::string text;
    
    bigint  id       ;
    bigint  card_id  ;
    bigint  uid      ;
    text    pan      ;
    text    ts       ;
    bigint  amount   ;
    bool reversal ;
    bool credit   ;
    text    refnum   ;
    integer status   ;
    bigint  oson_pid ;
    bigint  oson_tid ;
    text    merchant_name;
    text    city;
    text    street;
    text    epos_merchant_id;
    text    epos_terminal_id;
    
    inline Card_monitoring_data_T()
    : id(0)
    , card_id(0)
    , uid(0)
    , pan()
    , ts()
    , amount(0)
    , reversal(false)
    , credit(false)
    , refnum()
    , status(0)
    , oson_pid(0)
    , oson_tid(0)
    {}
};

struct Card_monitoring_search_T
{
    typedef int64_t bigint;
    typedef int32_t integer;
    typedef ::std::string text;
    
    bigint card_id;
    text   from_date;
    text   to_date;
};

struct Card_monitoring_list_T
{
    int32_t total_count;
    std::vector< Card_monitoring_data_T> list;
};

class Cards_monitoring_table_T
{
public:
    explicit Cards_monitoring_table_T(DB_T& db);
    
    int64_t add(const Card_monitoring_data_T& data);
    //int edit(int64_t id, const)
    
    //0 - if not found, total count 
    Error_T  list(const Card_monitoring_search_T& search, const Sort_T& sort,  Card_monitoring_list_T& out ) ;
    
private:
    DB_T& m_db;
};


struct Card_monitoring_load_data_T
{
    typedef int64_t bigint;
    typedef int32_t integer;
    typedef std::string text;
    
    bigint id        ;
    bigint card_id   ;
    text   from_date ;
    text   to_date   ;
    text   ts        ;
    integer status   ;
    
    inline Card_monitoring_load_data_T()
    : id(0)
    , card_id(0)
    , from_date( )
    , to_date( )
    , status(0)
    {}
};

class Cards_monitoring_load_table_T
{
public:
    explicit Cards_monitoring_load_table_T(DB_T& db);
    
    //return a new created row id.
    int64_t add(const Card_monitoring_load_data_T& data);
    
    int set_status(int64_t id, int32_t status);
    
    int del(const Card_monitoring_load_data_T& search);
    
    int loaded( const Card_monitoring_load_data_T & search);
private:
    DB_T& m_db;
};

class Cards_T
{
public:
	explicit Cards_T( DB_T & db );

    Error_T card_count( uint64_t uid, std::string pc_token, size_t& count);
    Error_T card_list(const Card_info_T & search, const Sort_T &sort, Card_list_T & list);
    std::vector< Card_info_T> card_list( int64_t uid );
    size_t card_count( int64_t uid ) ;
    
    Error_T make_bonus_card( uint64_t uid,  Card_info_T& out);
    
    Error_T bonus_card_add(Card_bonus_info_T& info);
    Error_T bonus_card_list(uint64_t card_id, Card_bonus_list_T & list);
    Error_T bonus_card_info(uint64_t card_id, Card_bonus_info_T& bonus_info);
    Error_T bonus_card_edit(uint64_t card_id, const std::string& passwd, Card_bonus_info_T& bonus_info);
    Error_T bonus_card_edit_balance(uint64_t card_id, const Card_bonus_info_T& bonus_info);
    
    Error_T bonus_card_delete(uint64_t card_id, const std::string& passwd);
    
    Card_info_T get(int64_t card_id, Error_T& ec);
    
    
    Card_topup_info_T get_topup_by_id(int64_t card_id, Error_T& ec ) ;
    
    //return id
    int64_t card_add(const Card_info_T & data );
    
    Error_T info(const Card_info_T &search, Card_info_T &info);
    Error_T card_edit(uint64_t id, const Card_info_T &info);
    Error_T card_edit_owner(uint64_t id, const std::string& owner_phone);
    Error_T card_admin_edit(uint64_t id, const Card_info_T &info);
    Error_T card_delete( int64_t id);
    Error_T unchek_primary(uint64_t id);
    Error_T set_primary( int64_t id);
    
private:
	DB_T & m_db;
};



#endif
