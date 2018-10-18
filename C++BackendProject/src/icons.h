/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   icons.h
 * Author: briocpp
 *
 * Created on August 2, 2018, 1:33 PM
 */

#ifndef OSON_ICONS_H
#define OSON_ICONS_H

#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <string>


 
class DB_T; //forward declaration


namespace oson
{
    
namespace icons
{
 
enum class Kind
{
    none              = 0,
    bank              = 1,
    merchant          = 2,
    user_logo         = 3,
    topup_merchant    = 4,
    merchant_qr_image = 5,
    user_qr_image     = 6,
    
    max_value         = 0xffff, // 2^16 - 1
};
    
struct info
{
    typedef ::std::int64_t bigint  ;
    typedef ::std::int32_t integer ;
    typedef ::std::string  text    ;
    
    bigint   id        ;
    text     location  ;
    text     path_hash ;
    integer  kind      ;
    text     ts        ;
    bigint   size      ;
    text     sha1_sum  ;
    
    info();
    
    static text make_path_hash(const text& name ) ;
};


struct content
{
    std::string image ;
};

class table
{
public:
    table(const table&) = delete;
    table& operator = (const table&) = delete;
    
    explicit table(DB_T& db ) ;
    ~table();
    
    std::int64_t   add (const info& icon);
    int edit( std::int64_t id,  const info& icon);
    int del(std::int64_t id);
    
    info get( std::int64_t id ) ;
    
private:
    DB_T& m_db;
}; //end table



class manager
{
public:
    manager(const manager&) = delete;
    manager& operator = (const manager&) = delete;
    
    manager();
    ~manager();
    
    int remove_icon(std::int64_t icon_id ) ;
    
    //@Return  a new created icon info.
    // or 0  if there some error occurred.
    info save_icon(const content& icon_content,  Kind kind , std::int64_t old_icon_id  ) ;
    
    //@return 1 if success, 0 - if fail.
    int load_icon( std::int64_t icon_id,  /*out*/ content& icon_content);
private:
    
};

} // namespace icons
    
} // namespace oson

#endif /* OSON_ICONS_H */

