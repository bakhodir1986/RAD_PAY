/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   runtime_options.h
 * Author: briocpp
 *
 * Created on August 11, 2018, 12:27 AM
 */

#ifndef RUNTIME_OPTIONS_H
#define RUNTIME_OPTIONS_H

#include <cstdint>
#include <string>

namespace oson
{

struct runtime_options_t
{
    struct main_opt_t
    {
        std::string log_file;
        int log_level;
        std::string pid_file;
    }main;
    
    struct database_t
    {
        std::string host;
        std::string name;
        std::string user;
        std::string pwd ;
    }db;
    
    struct client_t
    {
        std::string address;
        uint16_t port;
        std::string ssl_chain;
        std::string ssl_key;
        std::string ssl_dh;
        std::string ssl_pwd;
        
        int online_timeout   ;
        int max_active_users ;
        int active_threads   ;
        
        int monitoring_off_on_timeout;
    }client;
    
    struct admin_t
    {
        std::string address;
        uint16_t port;
        std::string ssl_chain;
        std::string ssl_key;
        std::string ssl_dh;
        std::string ssl_pwd;
        
        std::string phones;
    }admin;

    struct eopc_t
    {
        std::string address;
        std::string authHash;
        std::string merchant_id;
        std::string terminal_id;
        
        uint16_t port;
    }eopc;
    
    struct ios_cert_t
    {
        std::string certificate;
        int badge;
        int sandbox;
    }ios_cert;
    
    struct xmpp_t
    {
        std::string ip;
        uint16_t port;
        
    }xmpp;    
    
    struct sms_t
    {
        int         bulk_sms      ; // 0, off, disable, disabled - disable, 1, on, enable, enabled - enable
        std::string url           ;
        std::string url_v2        ;
        std::string auth_basic_v2 ;
    }sms ;
    
    struct certs_t
    {
        std::string webmoney_private_key;
        std::string ums_private_key;
        std::string ums_public_key;
        std::string ums_verify_cert;
    }certs;
};


} // end oson

#endif /* RUNTIME_OPTIONS_H */

