


#ifndef OSON_EWALLET_H_INCLUDED
#define OSON_EWALLET_H_INCLUDED 1

#include <string>
#include <vector>
#include <functional>

#include "types.h"
#include "DB_T.h"

namespace oson{ namespace topup{

struct info
{
    typedef  ::std::string   text; 
    typedef  ::std::int32_t  integer;
    typedef  ::std::int64_t  bigint ;
    
    integer id     ;
    text    name   ;
    integer status ;
    integer option ;
    bigint  min_amount;
    bigint  max_amount;
    integer rate;
    integer position;
    bigint  card_id;
    bigint  icon_id;
    info();
};

struct search
{
    typedef ::std::string  text    ;
    typedef ::std::int32_t integer ;
    typedef ::std::int64_t bigint  ;
    
    integer id;
    text    name;
    integer status;
    integer option;
    integer position;
    bigint card_id;
    bigint icon_id ;
    bool   with_icon_location;
    
    search();
    
};

typedef ::std::vector<  info >  info_list;

struct topup_id
{
    enum value
    {
        none = 0,
        webmoney = 1,
        yandex_money = 99999, // didn't support yet
        qiwi         = 99999, // didn't support yet
        paypal       = 99999, // didn't support yet
        wallet_one   = 99999, // didn't support yet
    };
};


bool supported (int32_t  id);

struct  status
{
    enum value
    {
        none      = 0 ,
        active    = 1 ,
        deactive  = 2 ,
    };
};

struct  option
{
    enum value
    {
        none = 0,
    };
};


class table
{
public:
    explicit table(DB_T& db);
    ~ table();
    
     info get( ::std::int32_t id,  /*out*/ Error_T & ec ) ;
    
     info_list list( const  struct search& search, const Sort_T& sort) ;
    
    int32_t add(const  struct info & info);
    
    int edit(  int32_t id, const  struct info & info ) ;
    
    int del(int32_t id ) ;
    
    int edit_icon(int32_t id,  int64_t icon_id);
    
private:
    
    DB_T& m_db;
};


enum class Currency
{
    usd     = 840 ,
    uzs     = 860 ,
    rub_810 = 810 ,
    rub_643 = 643 , 
    eur     = 978 ,
};

struct trans_info
{
    typedef int32_t     integer ;
    typedef int64_t     bigint  ;
    typedef std::string text    ;
    
    bigint  id             ;
    integer topup_id       ;
    bigint  amount_sum     ;
    double  amount_req     ;
    integer currency       ;
    bigint  uid            ;
    text    login          ;
    text    pay_desc       ;
    text    ts             ;
    text    tse            ;
    integer status         ;
    text    status_text    ;
    bigint  card_id        ;
    text    card_pan       ;
    text    eopc_trn_id    ;
    bigint  oson_card_id   ;
    bigint  topup_trn_id   ;
    trans_info();
    
    bool empty()const;
};

struct  trans_list
{
    int64_t total_cnt ;
    int64_t total_sum ;
    std::vector< trans_info > list;
};


class  trans_table
{
public:
    explicit  trans_table(DB_T& db);
    ~ trans_table();
    
    int64_t add(const  trans_info& ) ;
    
    int update(int64_t id,  trans_info& );
    
    int del(int64_t id);
    
    trans_info  get_by_id(int64_t id, Error_T& ec );
    trans_info  get_by_search(const  trans_info& search, Error_T& ec ) ;
    
     trans_list  list(const  trans_info& search, const Sort_T& sort, Error_T& ec );
    
    
private:
    DB_T& m_db ;
};




class   manager 
{
public:
    
private:
    
};


/// API part
namespace webmoney
{

struct wm_access
{
    std::string id_company; // LMI_MERCHANT_ID
    
    
};

struct wm_request
{
    std::string amount   ;  // amount  5.84
    std::string currency ; // currency 'USD', 'RUB', 'EURO'.
    std::string trn_id   ;
    std::string pay_desc ;
    std::string test_mode;
    
    std::string url;
};

struct wm_response
{
    int64_t     status_value  ;
    std::string status_text   ;
    
    std::string url;
    
    inline wm_response(): status_value{ -1 } {} ;
    
    inline bool is_ok()const{ return status_value == 0 ; } 
};

typedef std::function< void(const wm_request&, const wm_response& ) >  payment_handler ;

class  wm_manager
{
public:
    explicit wm_manager(  wm_access  ac) ;
    ~wm_manager() ;
    
    wm_response payment_request(const wm_request & req, /*out*/ Error_T& ec)  ;
    
    void async_payment_request(const wm_request& req, payment_handler h ) ;
    
private:
    wm_access m_acc;
    
};

} // end webmoney

}} // end oson::topup

#endif // OSON_EWALLETE_H_INCLUDED





