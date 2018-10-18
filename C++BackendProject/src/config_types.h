/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   config_types.h
 * Author: biocpp
 *
 * Created on December 21, 2017, 9:43 PM
 */

#ifndef OSON_CONFIG_TYPES_H
#define OSON_CONFIG_TYPES_H

#include <string>

////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
struct xmpp_network_info
{
    std::string address;
    unsigned short    port;
};
struct ios_notify_cert_info
{
    std::string certificate;
    bool isSandbox;
    int  badge;
};
/////////////////////////////////////////////////////////////////////////////
struct eopc_network_info
{
    std::string address;
    std::string authHash;
};

struct Server_head_T 
{
    enum{ length = 32, CMD_INNER_NONE = 0, CMD_INNER_CREATE = 1, CMD_INNER_DELETE = 2 };
	unsigned version  ; // 4    1
	unsigned cmd_id   ; // 4    2
    
    unsigned cmd_inner_id  ; // 4    3  :   1 - for create ,  2- for delete.
    unsigned inner_id_val  ; // 4    4
    
	unsigned data_size; // 4    5   offset 16, size 4.
    
  //  Uint32_T unused_3 ; // 4    6
  //  Uint32_T unused_4 ; // 4    7
  //  Uint32_T unused_5 ; // 4    8
};


Server_head_T parse_header(const unsigned char* data, size_t length);
void encode_header( const Server_head_T& head, unsigned char* data, size_t length);

#endif /* OSON_CONFIG_TYPES_H */

