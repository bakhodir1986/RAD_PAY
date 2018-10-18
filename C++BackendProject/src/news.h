#ifndef NEWS_H
#define NEWS_H

#include <string>
#include <vector>

#include "types.h"

class Sort_T;
class DB_T  ;


struct News_info_T 
{
    typedef int32_t integer;
    typedef int64_t bigint;
    typedef std::string text;
    
    integer id;
    text    msg;
    text    add_time;
    text    modify_time;
    integer lang;
    bigint  uid;
    
    inline News_info_T() 
    : id(0), lang(0), uid(0)
    {}
};

struct News_list_T 
{
    uint32_t count;
    std::vector<News_info_T> list;
};

namespace oson
{ 

struct news_in
{
    typedef int32_t     integer ;
    typedef std::string text    ;
    
    enum type_e
    { 
        type_none = 0x00, 
        type_push = 0x02, 
        type_sms  = 0x04, 
        type_push_sms = type_push | type_sms 
    };
    
    enum lang_e
    { 
        lang_all = 0, 
        lang_rus = 1, 
        lang_uzb = 2 
    } ;//see users LangCode

    enum operation_system_e
    {
        os_all     = 0 ,
        os_ios     = 1 ,
        os_android = 2 ,
    };
    ////////////////////////////////
    text    message          ;
    integer type             ;
    text    uids             ;
    integer language         ; // lang_e
    integer operation_system ; //OS: Android or IOS
    //////////////////////////////////////
};
} // end namespace oson

class News_T 
{
public:
    explicit News_T( DB_T & db );
    ~News_T();
    Error_T news_list(const News_info_T &search, const Sort_T &sort, News_list_T & list);
    Error_T news_add(const News_info_T &info);
    Error_T news_edit(uint32_t id, const News_info_T &newinfo);
    Error_T news_delete(uint32_t id);

private:
    DB_T & m_db;
};


#endif // end NEWS_H

