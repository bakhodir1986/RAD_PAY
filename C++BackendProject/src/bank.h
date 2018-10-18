#ifndef BANK_T_H
#define BANK_T_H

#include <string>
#include <vector>
#include "types.h"
class Sort_T;
class DB_T;//forward declaration



struct Bank_info_T 
{
    typedef int32_t      integer ;
    typedef int64_t      bigint  ;
    typedef std::string  text    ;
 
    enum EStatus{ ES_none = 0, ES_active = 1, ES_noactive = 2};
    
    integer  id              ;
    text     name            ;
    integer  rate            ;
    bigint   min_limit       ;
    bigint   max_limit       ;
    text     merchantId      ;
    text     terminalId      ;
    integer  port            ;
    bigint   month_limit     ;
    text     offer_link      ;
    bigint   icon_id         ;
    text     bin_code        ;
    integer  status          ; // 0->undef, 1-active, 2-no active
  
    inline Bank_info_T ()  
    : id(0), name()  ,  rate(0)      , min_limit(0) , max_limit(0)
    , merchantId()   ,  terminalId() , port()       , month_limit(0), offer_link()
    , icon_id(),  bin_code()   , status(0)
    {}
    
    inline  bigint commission(bigint amount)const{ return amount * (this->rate) / 10000 ; } //10000 - 1% is 100
    
    enum Constraint{ C_OK, C_LESS_MIN_LIMIT, C_EXCEED_MAX_LIMIT };
    
    inline Constraint check_amount(bigint amount_tiyin )const 
    {
        
        bigint min_tiyin = min_limit /* * 100 */;
        bigint max_tiyin = max_limit /* * 100 */;
        
        if (amount_tiyin < min_tiyin)
            return C_LESS_MIN_LIMIT ;
        
        if (amount_tiyin > max_tiyin)
            return C_EXCEED_MAX_LIMIT;
        
        return C_OK;
    }
    
    //std::string icon_actual_path()const;
    //std::string icon_link()const;
};

struct Bank_list_T {
    uint32_t count;
    std::vector<Bank_info_T> list;
};


struct Bank_bonus_info_T
{
    enum{ ACTIVE_STATUS = 1, DISACTIVE_STATUS = 2};
    
    int32_t     id           ;
    int32_t     bank_id      ;
    int64_t     min_amount   ;
    int32_t     percent      ;
    std::string start_date   ;
    std::string end_date     ;
    int32_t     status       ;
    std::string longitude    ;
    std::string latitude     ;
    std::string description  ;
    int64_t     bonus_amount ;
    
    //fill with zero all fields.
    Bank_bonus_info_T()
    : id(), bank_id(), min_amount(), percent(), start_date()
    , end_date(), status(), longitude(), latitude(), description(), bonus_amount()
    {}
};

typedef std::vector< Bank_bonus_info_T > Bank_bonus_list_T ;

class Bank_T
{
public:
    explicit Bank_T(DB_T &db);
    ~Bank_T();
    
    ////////////////////// BANK TABLE /////////////////////////////////
    Error_T list(const Bank_info_T & search, const Sort_T &sort, Bank_list_T &list);
    Error_T info( int32_t id, Bank_info_T &data);
    
    Bank_info_T info(const Bank_info_T& search, Error_T & ec);
    
    Error_T add(const Bank_info_T &info, /*out*/  uint32_t& id );
    Error_T edit(const Bank_info_T &info);
    Error_T del(uint32_t id);
    
    Error_T edit_icon_id(int32_t id,  int64_t icon_id);
    ///Error_T add_logo(uint32_t bank_id, const std::string &logo_data);
    //Error_T ico(uint32_t bank_id, const time_t &if_mod, std::string &data);
    
    ///////////////////// BANK_BONUS TABLE /////////////////////////////////////////////////
    Error_T bonus_list(const Bank_bonus_info_T& search, const Sort_T& sort, Bank_bonus_list_T& list);
    Error_T bonus_info(const Bank_bonus_info_T& search, Bank_bonus_info_T& info);
    Error_T bonus_add(const Bank_bonus_info_T& info, /*out*/ uint32_t& id);
    Error_T bonus_edit(const Bank_bonus_info_T& info);
    Error_T bonus_del(uint32_t id);
    
private:
    DB_T & m_db;
};

#endif // BANK_T_H
