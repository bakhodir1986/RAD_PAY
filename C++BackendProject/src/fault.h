#ifndef OSON_FAULT_T_H
#define OSON_FAULT_T_H

#include <string>
#include <vector>
#include "types.h"

//forward declarations.
class Sort_T;
class DB_T  ;

enum Fault_status_T 
{
    FAULT_STATUS_UNDEF = 0,
    FAULT_STATUS_OK = 1,
    FAULT_STATUS_ERROR = 2,
    FAULT_STATUS_CORRECTED = 3,
};

enum Fault_type_T 
{
    FAULT_TYPE_UNDEF = 0,
    FAULT_TYPE_MERCHANT = 1,
    FAULT_TYPE_EOPC = 2,
};

struct Fault_info_T 
{
    typedef int32_t     integer ;
    typedef std::string text    ;
    
    integer id          ;
    integer type        ;
    integer status      ;
    text    description ;
    text    ts          ;
    text    ts_notify   ;
    
    integer ts_days     ; // need for search
    
    Fault_info_T();
    Fault_info_T( integer type, integer status, const text& description);
    
    bool empty()const;
};

struct Fault_list_T 
{
    int32_t count;
    std::vector<Fault_info_T> list;

    inline Fault_list_T()
    : count(0)
    , list()
    {
        
    }
};

class Fault_T
{
public:
    Fault_T(DB_T & db);

    Error_T add(const Fault_info_T & info);
    Error_T list(const Fault_info_T & search, const Sort_T& sort, Fault_list_T & list);
    Error_T del(int32_t id);
    Error_T edit(int32_t id, const Fault_info_T &new_data);

private:
    DB_T & m_db;
};

#endif // OSON_FAULT_T_H
