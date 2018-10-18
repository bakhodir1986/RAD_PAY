#ifndef OSON_BILLS_T_H
#define OSON_BILLS_T_H

#include <string>
#include <vector>

#include "types.h"

//forward declarations.
class Sort_T;
class DB_T  ;

enum Bills_status_T 
{
    BILL_STATUS_UNDEF     = 0,
    BILL_STATUS_REGISTRED = 1,
    BILL_STATUS_REPAID    = 2,
    BILL_STATUS_DECLINED  = 3,
};

struct Bill_data_T {
    typedef int32_t     integer ;
    typedef int64_t     bigint  ;
    typedef std::string text    ;
    
    bigint  id;
    bigint  uid;  // Who need pay (which user pay)
    bigint  uid2; // Who add this bill.  Note: if uid2 == 0, so bill to BUSINESS bill.
    bigint  amount;
    integer merchant_id;
    text    fields;
    text    phone;
    text    add_ts;
    text    comment;
    integer status;
    
    inline bool is_business_bill()const{ return uid2 == 0 ; }
    
    
    inline Bill_data_T()
        : id(0)
        , uid(0)
        , uid2(0)
        , amount(0)
        , merchant_id(0)
        , status( BILL_STATUS_UNDEF ) 
    {
    }
    
    text get_login()const;
    
    static text get_login( const text& fields);
};
struct Bill_data_search_T
{
    typedef int32_t     integer ;
    typedef int64_t     bigint  ;
    typedef std::string text    ;
    
    static const bigint UID_NONE = -1;
    
    bigint   id, uid, uid2    ;
    integer  status           ;
    bigint   merchant_id      ;
    text     merchant_id_list ;
    
    inline Bill_data_search_T()
    : id(0)
    , uid(UID_NONE)
    , uid2(UID_NONE)
    , status(0)
    , merchant_id(0)
    , merchant_id_list()
    {}
    
};
struct Bill_data_list_T {
    uint32_t count;
    std::vector<Bill_data_T> list;
};

class Bills_T
{
public:
    explicit Bills_T(DB_T &db);
    
    int64_t add(const Bill_data_T &new_data);
    Error_T del( int64_t id);
    
    Bill_data_list_T list(const Bill_data_search_T &search, const Sort_T & sort );
    Bill_data_T get( int64_t id,  Error_T& ec);
    Error_T set_status(int64_t id, uint16_t status);

private:
    
    DB_T& m_db;
};

#endif // OSON_BILLS_T_H
