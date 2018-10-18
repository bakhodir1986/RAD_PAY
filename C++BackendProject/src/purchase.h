#ifndef PURCHASE_H
#define PURCHASE_H

#include "types.h"

//forward declarations.
class Sort_T;
class DB_T ;

struct Purchase_reverse_info_T
{
    typedef std::string text;
    
    int64_t id;
    int64_t pid;
    int64_t aid;
    int64_t uid;
    text   sms_code;
    int32_t status;
    text   ts_start;
    text   ts_confirm;
    text   phone;
    text   baz;
    
    Purchase_reverse_info_T(): id(), pid(), aid(), uid(), status(){}
};

class Purchase_reverse_T
{
public:
    explicit Purchase_reverse_T(DB_T& db);
    ~Purchase_reverse_T();
    
    Error_T  info(int64_t id, Purchase_reverse_info_T& out);
    Error_T  add(const Purchase_reverse_info_T& in,  int64_t& id);
    Error_T  upd(const Purchase_reverse_info_T& in);
    
private:
    DB_T& m_db;
};
 
struct Purchase_info_T 
{
    typedef int64_t      bigint  ;
    typedef int32_t      integer ;
    typedef std::string  text    ; 
    
    bigint  id                 ;
    bigint  uid                ;
    integer mID                ;
    text    login              ;
    text    ts                 ;
    bigint  amount             ;
    text    eopc_trn_id        ;
    text    pan                ;
    integer status             ;
    text    paynet_tr_id       ;
    integer paynet_status      ;
    bigint  receipt_id         ; // similar with id most case, but bill purchase this is id of bill table.
    bigint  oson_paynet_tr_id  ;
    bigint  oson_tr_id         ; // purchase_info table oson_tr_id.
    bigint  card_id            ; // need for bonus 
    bigint  bearns             ; 
    bigint  commission         ;
    text    merch_rsp          ;

//    ///////////////////////////////////////
//    //// not saved on db, used internal.//
//    text    from_date          ;
//    text    to_date            ;
    text    src_phone          ; 
//    text    m_merchant_ids     ;
//    bool    m_use_merchant_ids ;
//    int     flag_total_sum     ;
    Purchase_info_T();
};

struct Purchase_search_T 
{
    typedef int64_t      bigint  ;
    typedef int32_t      integer ;
    typedef std::string  text    ; 
    
    bigint  id                 ; // used
    bigint  uid                ; // used
    integer mID                ; // used
    text    eopc_trn_id        ; // used 
    integer status             ; // used
    integer bank_id            ;
    ///////////////////////////////////////
    //// not saved on db, used internal.//
    text    from_date          ; // used
    text    to_date            ; // used 
    text    m_merchant_ids     ; // used 
    bool    m_use_merchant_ids ; // used
    bool    in_merchants       ; // in or not in m_merchant_ids
    int     flag_total_sum     ;
    
    inline Purchase_search_T() 
           : id(0)
           , uid(0)
           , mID(0)
           , status(0)
           , bank_id(0)
           , m_use_merchant_ids(false)
           , in_merchants(true) // by default in
           , flag_total_sum(0)
    {}
};

struct Purchase_list_T {
    int64_t count;
    int64_t sum;
    int64_t commission_sum;
    std::vector<Purchase_info_T> list;
};

struct Purchase_export_T {
    uint64_t id;
    uint64_t uid;
    uint32_t mID;
    std::string m_name;
    std::string m_contract;
    std::string login;
    std::string ts;
    std::string from_date;
    std::string to_date;
    uint64_t amount;
    Purchase_export_T() {
        id = 0;
        uid = 0;
        mID = 0;
        amount = 0;
    }
};

struct Purchase_export_list_T {
    uint32_t count;
    std::vector<Purchase_export_T> list;
};

struct Purchase_state_T{
    std::string ts;
    uint64_t merchant_id;
    uint64_t total;
    uint64_t users;
    uint64_t sum;
};

struct Purchase_top_T {
    uint64_t sum;
    uint64_t count;
    uint64_t users;
    uint32_t merchant_id;
};


struct Favorite_info_T 
{
    uint32_t id;
    uint32_t fav_id;
    uint32_t field_id;
    uint64_t uid;
    uint32_t merchant_id;
    uint32_t key;
    std::string name;
    std::string value;
    std::string prefix;

    inline Favorite_info_T() 
          : id(0), fav_id(0), field_id(0), uid(0), merchant_id(0), key(0)
    {}
};

struct Favorite_list_T
{
    int32_t total_count ;
    std::vector< Favorite_info_T > list ;
};

struct Beeline_merch_response_T
{
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
    
    
//////12. FILE_NAME = oson_dd.mm.yyyy  , masalan  oson_26.12.2017
    
    std::string login;
    std::string msisdn;
    std::string amount;
    std::string currency;
    std::string pay_id;
    std::string receipt_num;
    std::string date_stamp;
    std::string commit_date;
    std::string magic_number;
    std::string partner_pay_id;//PARTNER_PAY_ID;
    std::string branch;
    std::string trade_point;
    //std::string file_name;
};
struct Beeline_response_list_T
{
    std::vector<Beeline_merch_response_T> list;
    size_t total_count;
    int64_t total_amount;
    
    inline Beeline_response_list_T(): list{}, total_count(0), total_amount(0){}
};

struct Sverochniy_header_T
{
    struct node_t{
        std::string field_name;
        std::string description;
        std::string format;
        inline node_t(){}
        inline node_t(std::string f, std::string d, std::string fmt): field_name(f), description(d), format(fmt){}
    };
    
    std::vector<node_t> nodes;
};

//@Note: purchase_info table.
struct Purchase_details_info_T
{
    int64_t oson_tr_id;
    int64_t trn_id;
    std::string json_text;
    inline Purchase_details_info_T(): oson_tr_id(0), trn_id(0){}
};
std::string extract_sysinfo_sid(std::string json);

class Purchase_T
{
public:
    explicit Purchase_T( DB_T & db );
    ~Purchase_T();
    
    Error_T make_beeline_merch_response(const Purchase_search_T& search, const Sort_T& sort, Beeline_response_list_T & rsp);
    
   // Error_T details_info(const Purchase_details_info_T& search, Purchase_details_info_T& out_info);
    Error_T add_detail_info(const Purchase_details_info_T& info);
    Error_T get_detail_info(uint64_t oson_trn_id, Purchase_details_info_T& info);
    
    Error_T list_admin(const Purchase_search_T & search, const Sort_T &sort, Purchase_list_T & list);
    
    static std::string list_admin_query(const Purchase_search_T & search, const Sort_T& sort )  ;
    
    Error_T list(const Purchase_search_T & search, const Sort_T &sort, Purchase_list_T & list);
    Error_T list(const Purchase_search_T & search, const Sort_T &sort, Purchase_export_list_T & list);
    
    int64_t add(const Purchase_info_T & data);
    
    Error_T info(const uint64_t trn_id, Purchase_info_T & p_info);
    Error_T update(const uint64_t trn_id, const Purchase_info_T & new_data);
    Error_T cancel(const uint64_t &id);
    Error_T update_status( int64_t trn_id, int status);
    Error_T update_status( int64_t trn_id, const Purchase_info_T& new_data);
    
    Error_T stat(uint16_t group, const Purchase_search_T & search, std::vector<Purchase_state_T> &statistics);
    Error_T top(const Purchase_search_T &search, std::vector<Purchase_top_T> &tops);

    Error_T favorite_list(const Favorite_info_T &search, const Sort_T& sort,  /*out*/Favorite_list_T  &list);
    Error_T favorite_add(uint64_t uid, const std::vector<Favorite_info_T>& list);
    Error_T favorite_del(uint32_t fav_id);

    Error_T bonus_list(const Purchase_search_T& search, const Sort_T& sort, Purchase_list_T& list);
    Error_T bonus_list_client(const Purchase_search_T&search, const Sort_T& sort, Purchase_list_T& list);
    
private:
    DB_T & m_db;
};

#endif
