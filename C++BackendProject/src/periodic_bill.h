#ifndef OSON_PERIODIC_BILL_T_H
#define OSON_PERIODIC_BILL_T_H

#include <string>

#include "types.h"

//forward declarations.
class Sort_T;
class DB_T  ;


struct Periodic_bill_data_T 
{
    typedef int32_t      integer ;
    typedef int64_t      bigint  ;
    typedef std::string  text    ;
    
    bigint  id           ;
    bigint  uid          ;
    integer merchant_id  ;
    bigint  amount       ;
    bigint  card_id      ;
    
    text    periodic_ts  ;
    text    name         ;
    text    prefix       ;
    text    fields       ;
    integer status       ;
    
    
    Periodic_bill_data_T();
    
    std::string get_login()const;
};

enum PBill_status {
    PBILL_STATUS_ACTIVE = 0,
    PBILL_STATUS_PAUSE  = 1,
    PBILL_STATUS_ERROR  = 4,
    PBILL_STATUS_IN_PROGRESS = 8,
};

struct Periodic_bill_list_T 
{
    std::vector<Periodic_bill_data_T> list;
    uint32_t count;
};

class Periodic_bill_T
{
public:
    Periodic_bill_T(DB_T & db);

    Error_T add(Periodic_bill_data_T & data);
    Error_T list(const Periodic_bill_data_T & search, const Sort_T& sort, Periodic_bill_list_T & list);
    Error_T edit(uint32_t id, const Periodic_bill_data_T &data);
    Error_T del(uint32_t id);
    Error_T update_last_bill_ts(uint32_t id);
    
    Error_T update_last_notify_ts(uint32_t id);
    
    /***
     * retried informations from periodic_bill table, 
     * where status is active and difference from now and last_bill_ts greater than 1 day.
     *  
     *  
     */
    Error_T list_need_to_bill( const Sort_T & sort, Periodic_bill_list_T& list);
    

private:
    DB_T &m_db;
};

#endif // OSON_PERIODIC_BILL_T_H
