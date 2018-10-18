#ifndef MERCHANT_API_T_H
#define MERCHANT_API_T_H

#include <string>
#include <map>
#include <memory>
#include <functional>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>

#include "types.h"
#include "Merchant_T.h"
#include "purchase.h"

struct Merch_trans_T 
{
    typedef int32_t      integer;
    typedef int64_t      bigint ;
    typedef std::string  text   ;
    
    text        param            ;  // field data `value`.
    bigint      amount           ;  // amount of money by tiyin.
    text        ts               ;  // time stamp of transaction start.
    bigint      transaction_id   ;  // id of transaction|purchase table.
    bigint      check_id         ;  // id of purchases table.
    text        service_id       ;  // merchant external service
    text        user_phone       ;  // user phone.
    integer     service_id_check ;
    Purchase_details_info_T info_detail;
    typedef std::map< std::string, std::string > merch_api_map_t;
    
    merch_api_map_t merch_api_params;
    
    bigint       uid;//for test only.
    integer      uid_lang;

    Merchant_info_T  merchant;
    Merch_acc_T      acc ;
    Merch_trans_T() 
    : param(), amount(0), ts(), transaction_id(), check_id(0)
    , user_phone(), service_id_check(0)
    , merch_api_params(), uid(0), uid_lang(0)
    {}
};


struct Merch_check_status_T 
{
    bool        exist        ;
    int64_t     status_value ;
    std::string status_text  ;
    bool        notify_push  ;
    std::string push_text    ;
    inline Merch_check_status_T()
    : exist( false )
    , status_value( 0 ) 
    , status_text()
    , notify_push(false)
    {}
};

struct Merch_trans_status_T 
{
    std::string ts;
    std::string merchant_trn_id;
    std::string merch_rsp;
    int32_t merchant_status;
    
    std::map<std::string, std::string> kv;
    std::string kv_raw;
    
    inline Merch_trans_status_T () 
     :  merchant_trn_id("0")
     , merchant_status(0)
    {
    }
};

struct Merchant_query_T
{
    int32_t merchant_id; 
    std::string field_name;
    std::string value;
};

namespace oson
{

class Merchant_api_manager
{
private:
    Merchant_api_manager(const Merchant_api_manager&); // = deleted
    Merchant_api_manager& operator = (const Merchant_api_manager&); // = deleted
public:
    typedef Merchant_api_manager self_t;
    
    enum commands
    {
        command_query_field   ,
        command_purchase_info ,
        command_check_status  ,
        command_perform_purchase  ,
        command_perform_status,
    };
    
    explicit Merchant_api_manager( std::shared_ptr< boost::asio::io_service > );
    ~Merchant_api_manager();
    
    std::shared_ptr< boost::asio::io_service >  get_io_service()const ;
    std::shared_ptr< boost::asio::ssl::context> get_ctx_sslv23()const ;
    std::shared_ptr< boost::asio::ssl::context> get_ctx_sslv3 ()const ;
public:
    
    //typedef std::function< void (const Merchant_query_T& q, const Merch_trans_status_T& response,  Error_T ec) > query_field_handler;
    typedef std::function< void (const Merch_trans_T& trans, const Merch_check_status_T& status,   Error_T ec) > check_status_handler;
    typedef std::function< void (const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) > purchase_info_handler;
    typedef std::function< void (const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) > perform_purchase_handler;
    typedef std::function< void (const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) > perform_status_handler;

    //@Note this may be changed.
    static Error_T init_fields(int32_t merchant_id, int64_t oson_tr_id, const std::vector< Merchant_field_data_T> & list,
      /*out*/ Merch_trans_T& trans, /*out*/Merchant_field_data_T & pay_field
    );
    static Currency_info_T currency_now_or_load( int type );
    
public:
//    void query_field(const Merchant_query_T& q,  query_field_handler handler);
    
    void async_check_status(const Merch_trans_T& trans, check_status_handler handler);
    
    void async_purchase_info(const Merch_trans_T& trans, purchase_info_handler handler);
    
    void async_perform_purchase(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    
    void async_perform_status(const Merch_trans_T& trans, perform_status_handler handler );
    
private:
    void check_status(const Merch_trans_T& trans, check_status_handler handler);
    void perform_purchase(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    void purchase_info(const Merch_trans_T& trans, purchase_info_handler handler ) ;
    void perform_status(const Merch_trans_T& trans, perform_status_handler ) ;
    
private:
    /**************** BEELINE  *******************************************************/
    void check_status_beeline(const Merch_trans_T& trans, check_status_handler handler);
    void perform_purchase_beeline(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    
    
    
    /***************** OSON API *********************************************************/
    void check_status_oson_api(const Merch_trans_T& trans, check_status_handler handler);
    void perform_purchase_oson_api(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    
    
    /***************** UCELL   *******************************************************/
    void check_status_ucell(const Merch_trans_T& trans, check_status_handler handler ) ;
    void perform_purchase_ucell(const Merch_trans_T& trans, perform_purchase_handler handler );
    void purchase_info_ucell(const Merch_trans_T& trans, purchase_info_handler handler);
    
    /**************** MPLAT    *****************************************************************/
    void purchase_info_mplat(const Merch_trans_T& trans, purchase_info_handler handler);
    void check_status_mplat(const Merch_trans_T& trans, check_status_handler handler ) ;
    void perform_purchase_mplat(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    void perform_status_mplat(const Merch_trans_T& trans, perform_status_handler handler);


    /*************  PAYNET    *****************************************************************/
    
    void purchase_info_paynet(const Merch_trans_T& trans, purchase_info_handler handler);
    void check_status_paynet(const Merch_trans_T& trans, check_status_handler handler ) ;
    void perform_purchase_paynet(const Merch_trans_T& trans, perform_purchase_handler handler ) ;
    

    /********************** WEBMONEY DIRECT *********************************************/
    void purchase_info_webmoney(const Merch_trans_T& trans, purchase_info_handler handler);
    void check_status_webmoney(const Merch_trans_T& trans, check_status_handler handler) ;
    void perform_purchase_webmoney(const Merch_trans_T& trans, perform_purchase_handler handler);

    
    /************************  Qiwi wallet **********************************************/
    void perform_status_qiwi(const Merch_trans_T& trans, perform_status_handler handler ) ;
    void purchase_info_qiwi(const Merch_trans_T& trans, purchase_info_handler handler);
    void check_status_qiwi(const Merch_trans_T& trans, check_status_handler handler) ;
    void perform_purchase_qiwi(const Merch_trans_T& trans, perform_purchase_handler handler);
    
    
    /*********************** Hermes Garant ********************************************/
    void perform_status_hg(const Merch_trans_T& trans, perform_status_handler handler ) ;
    
private:
    std::shared_ptr< boost::asio::io_service > io_service_  ;
    std::shared_ptr< boost::asio::ssl::context > ctx_sslv23 ;
    std::shared_ptr< boost::asio::ssl::context > ctx_sslv3  ; //tls
};

} // end namespace oson

class Merchant_api_T
{
    Merchant_api_T(const Merchant_api_T&); // = deleted
    Merchant_api_T& operator = (const Merchant_api_T&); // = deleted.
public:
    Merchant_api_T( const Merchant_info_T & address, const Merch_acc_T & acc);

    
    ~Merchant_api_T();

    Error_T query_new(const std::string &field_name, const std::string &value, std::map<std::string, std::string> &list);
    
    Error_T perform_purchase(const Merch_trans_T& trans, Merch_trans_status_T& response);

    Error_T check_status(const Merch_trans_T & trans, Merch_check_status_T & status);
    
    Error_T get_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    Error_T make_detail_info(const std::string& json_text, Merch_trans_status_T& response);
    
    
    Error_T get_balance(const Merch_trans_T& trans,  Merch_trans_status_T& status) ;
    
    Error_T pay_status(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    
    Error_T cancel_pay(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
     
    static Currency_info_T  get_currency_now();
    static Currency_info_T  get_currency_now_cb(int type);
private:
    static Currency_info_T  get_cb_uzb();
    static Currency_info_T  get_cb_rus();
    
    static Currency_info_T get_cb_uzb_xml();
    
private:
    /************************ PAYNET ************************************************/
    Error_T get_info_paynet(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_paynet_api(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_paynet_api(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    /********************  MUNIS ******************************************************/
    Error_T get_munis_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_munis_merchants(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_munis_merchants(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    
    /********************** BANK INFIN KREDIT *****************************************/
    Error_T get_bank_infin_kredit_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T perform_bank_infin_kredit(const Merch_trans_T& trans,  Merch_trans_status_T& response);
    
    
    /**********************  MPLAT  ***********************************************/
    Error_T get_mplat_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_mplat_merchants(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_mplat_merchants(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    /*********************  MONEY MOVER **********************************************/
    Error_T check_money_mover(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_money_mover(const Merch_trans_T& trans, Merch_trans_status_T& response) ;
    
    /********************* OSON API *************************************************/
    Error_T check_oson(const Merch_trans_T & trans, Merch_check_status_T & status);
    Error_T perform_oson(const Merch_trans_T & trans, Merch_trans_status_T &response) ;
    
    /********************** CRON TELECOM *********************************************/
    Error_T check_cron_telecom(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_cron_telecom(const Merch_trans_T & trans, Merch_trans_status_T &response);
    
    
    /************************* UZINFOCOM *********************************************/
    Error_T check_uzinfocom(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_uzinfocom_http(const Merch_trans_T & trans, Merch_trans_status_T &response);
    
    
    /************************  COMNET *********************************************/
    Error_T check_comnet(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_comnet(const Merch_trans_T & trans, Merch_trans_status_T &response);
    
    
    /***********************  SHARQ TELECOM ********************************************/
    Error_T check_sharq_telecom(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_sharq_telecom(const Merch_trans_T & trans, Merch_trans_status_T &response);
    
    /************************* UZMOBILE CDMA ****************************************/
    Error_T check_uzmobile_CDMA(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_uzmobile_CDMA(const Merch_trans_T & trans, Merch_trans_status_T &response);
    
    /************************** UZMOBILE GSM ********************************************/
    Error_T check_uzmobile_GSM(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_uzmobile_GSM(const Merch_trans_T & trans, Merch_trans_status_T& response);

    /*************************** UZMOBILE A NEW API (CDMA,GSM) TOGETHER******************/
    Error_T get_info_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_uzmobile_new(const Merch_trans_T& trans, Merch_check_status_T& status ) ;
    Error_T perform_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T perform_status_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T perform_cancel_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response); 
    
    /************************** BEELINE DIRECT ******************************************/
    Error_T check_beeline(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_beeline(const Merch_trans_T& trans, Merch_trans_status_T& response);

    /************************* SARKOR TELECOM **********************************************/
    Error_T check_sarkor_telecom(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_sarkor_telecom(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    
    /************************* KAFOLAT INSURANCE ********************************************/
    Error_T check_kafolat_insurance(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_kafolat_insurance(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    /************************  NANOTELECOM ************************************************/
    Error_T check_nanotelecom(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_nanotelecom(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    
    /********************** WEBMONEY DIRECT *********************************************/
    Error_T get_webmoney_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_webmoney(const Merch_trans_T& trans, Merch_check_status_T& status) ;
    Error_T perform_webmoney(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
    /*************************  UCELL DIRECT *****************************************/
    Error_T get_ucell_info(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_ucell(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_ucell(const Merch_trans_T& trans, Merch_trans_status_T& response);

    /************************** TPS DIRECT *******************************************/
    Error_T get_tps_info(const Merch_trans_T& trans,  Merch_trans_status_T& response);
    Error_T check_tps(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_tps(const Merch_trans_T& trans, Merch_trans_status_T& response);


    /*************************** QIWI WALLET **************************************/
    Error_T get_qiwi_info(const Merch_trans_T& trans, Merch_trans_status_T& response ) ;
    Error_T check_qiwi(const Merch_trans_T& trans, Merch_check_status_T& status ) ;
    Error_T perform_qiwi(const Merch_trans_T& trans, Merch_trans_status_T& response ) ;
    Error_T get_qiwi_balance (const Merch_trans_T& trans,  Merch_trans_status_T& status);
    
    Error_T pay_status_qiwi(const Merch_trans_T& trans,  Merch_trans_status_T& response ) ;
    
    
    
    /****************************  NATIVEPAY ************************************/
    Error_T get_nativepay_info(const Merch_trans_T & trans, Merch_trans_status_T& response ) ;
    Error_T check_nativepay(const Merch_trans_T& trans, Merch_check_status_T& status ) ;
    Error_T perform_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& response ) ;
    Error_T  balance_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& status);
    
    
    Error_T pay_status_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& response ) ;
    
    
    /****************************** UMS Direct ***************************************/
    Error_T get_info_ums(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T check_ums(const Merch_trans_T& trans, Merch_check_status_T& status);
    Error_T perform_ums(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T cancel_ums(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T pay_status_ums(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
public:
    Error_T sverka_ums(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T sverka_ums_result(const Merch_trans_T& trans, Merch_trans_status_T& response);
    
private:
    /********************************* Hermes Garant *************************************/
    Error_T get_info_hg(const Merch_trans_T& trans, Merch_trans_status_T& response ) ;
    Error_T check_hg(const Merch_trans_T& trans, Merch_check_status_T& status ) ;
    Error_T perform_hg(const Merch_trans_T& trans, Merch_trans_status_T& response);
    Error_T get_hg_balance (const Merch_trans_T& trans,  Merch_trans_status_T& status);
    
    
    
    std::pair< Error_T, double>  currency_convert_usd(int64_t amount);
    
private:
    const Merch_acc_T m_acc;
    const Merchant_info_T m_merchant;
};


namespace oson{ namespace backend{ namespace merchant{ namespace Mplat{

enum MplatCMD
{
   CMD_STATUS = 1,
   CMD_PAY ,
   CMD_CHECK,
   CMD_GET_BALANCE,
   CMD_GET_PROVIDERS,
   CMD_GROUP_PROVIDERS,
   CMD_REGISTRY_PAYMENTS,
};    

const char*  cmd_type_str(MplatCMD cmd);


struct acc_t
{
    std::string login;
    std::string pwd_md5;
    std::string sign;
    std::string url;
    std::string agent;
};

std::string make_signature_check_pay(const struct acc_t& acc,  const std::string& account );
std::string make_signature_status( const struct acc_t& acc, const std::string& txn);
std::string make_signature_others( const struct acc_t& acc);


struct header_t
{
    // method : POST
    // SIGNATURE: 
    std::string signature;
    // content-type: text/xml; utf-8
    
};
struct auth_t
{
    std::string login;
    std::string password;
    std::string agent;
};
struct body_t
{
    std::string type;
    int service;
    std::string account;
    std::string date;
    int currency;
    std::string id;
    std::string amount;
    
    std::string txn;
};
struct extra_t
{
    std::vector< std::pair<std::string, std::string> > ev_s;
};
struct request_t
{
    struct header_t header;
    struct auth_t auth;
    struct body_t body;
    struct extra_t extra;
};


struct response_t
{
    std::string type;
    int result;
    std::string currency;
    std::string client_rate;
    std::string message;
    std::string txn;
    std::string amount;
    std::string id;
    struct extra_t extra;
    
    std::string status;
    std::string credit;
    std::string balance;
    
    int result_to_oson_error()const;
};

struct provider_t
{
    int id;
    std::string name;
    std::string currency;
    std::string regExp;
    long long minSum;
    long long maxSum;
    std::string header;
    int groupId;
    std::string icon;
};

typedef std::vector< provider_t > provider_list_t;

struct provider_group_t
{
    std::string id; // Номер транзакции агента
    std::string name;
    std::string description;
    std::string typeLogo;
    std::string icon;
    
};
typedef std::vector<provider_group_t > provider_group_list_t;


typedef std::function< void(const struct request_t&,  const struct response_t& , Error_T )> handler_type;

class manager_api
{
public:
    explicit  manager_api(const struct acc_t & acc);
    ~manager_api();
    
    
    Error_T providerGroups(const struct request_t& request, provider_group_list_t& list);
    Error_T providers(const struct request_t& request, provider_list_t& list);
    
    Error_T status(const struct request_t& request, struct response_t& response);
    Error_T check(const struct request_t& request, struct response_t& response);
    Error_T balance(const struct request_t& request, struct response_t& response);
    Error_T pay(const struct request_t& request, struct response_t& response);
    
    void async_status(const struct request_t& req, handler_type handler);
    void async_check(const struct request_t& req, handler_type handler );
    void async_pay(const struct request_t& req, handler_type handler ) ;
    
private:
    
    void on_async_status_finish( const std::string& xml_resp, const boost::system::error_code& ec ) ;
    void on_async_check_finish( const std::string& xml_resp, const boost::system::error_code& ec  ) ;
    void on_async_pay_finish(const std::string& xml_resp, const boost::system::error_code& ec  ) ;
    
    std::pair< std::string, std::string > make_status_xml(const struct request_t& req);
    std::pair< std::string, std::string > make_check_xml(const struct request_t& req);
    std::pair< std::string, std::string > make_pay_xml(const struct request_t & req ) ;
    
    void async_http_req(const std::string& xml_req, const std::string& signature, std::function< void(const std::string&, const boost::system::error_code& ) > handler ) ;
    
private:
    
    struct acc_t acc_;
    
    struct request_t cur_req_;
    handler_type  cur_handler_;
};

//namespace async
//{
//    
//typedef std::function< void(const struct request_t&,  const struct response_t& , Error_T )> handler_type;
//typedef std::shared_ptr< boost::asio::io_service >    io_service_ptr;
//    
//class manager_api
//{
//public:
//    explicit manager_api(const struct acc_t& acc, io_service_ptr const& ptr);
//    ~manager_api();
//    
//    void status(const struct request_t& request, handler_type const& handler);
//private:
//    struct acc_t acc_;
//    io_service_ptr ios_;
//};
//
//} // end async

}}}}  // oson::backend::merchant::Mplat


namespace oson{ namespace backend{ namespace merchant{ namespace money_movers_llc{ 
 //Agent.service.amount.id.user.secret  --> sha256
    
    ////////////// REQUEST //////////////////////////////////////
    //4. Service - id of service
    //   4.1. Service_sub_id   -- inner id of sesrvice,   if not determined , '0' used
    //   4.2. Service_second_sub_id -- 2-nd inner id  ,   if not determined , '0' used    
    //   4.3. Service_third_sub_id  -- 3-rd inner id  ,   if not determined , '0' used
    //
    //5. User    -- user login, telefon, other...
    //6. Amount  -- amount of by GEL
    //7. Id     -- unique transaction id, from OSON
    //8. Date   transaction date, format is "Y-m-d H:M:S"
    //9. Hash  - secure hashing
    //10. AGENT - OSON
    //11. CANAL - canal handling operation, if not value, so '0' will used.
    
    
    /////////////// ANSWER //////////////////////
    //12. Errorcode - code of error, response
    //13. Errorru  - message of error in Russian language
    //14. Errorge  - message of error in Gruzin language
    //15. Erroren  - message of error in English language
    //16. Gel      - Gel na sum kurse valyuta.
    //17. User     - same as 5.User
    //18. Service  - same as 4. Service
    //19. Operationid - unique transaction id, from GeoCell.
    //20. Data  --  other additional information.
    
    
    ////////////////////////////////////////////////////
    // 1. Service           -  Integer     c++ int64_t
    // 2. Service_sub_id    -  Integer     c++ int64_t
    // 3. Service_second_sub_id - Integer  c++ int64_t
    // 4. Service_third_sub_id  - Integer  c++ int64_t
    // 5. User                 - varchar(25)  c++ string
    // 6. amount              - float       c++ double  ( or int64_t )
    // 7. id                  - integer     c++ int64_t
    // 8. Date               - Date('Y-m-d H:M:S')  c++ string
    // 9. AGENT              - varchar(32)    c++ string
    // 10. CANAL             - varchar(32)    c++ string
    // 11. hash              - varchar(64)    c++ string
    //
    
    
    
    ///////  INFO 
    
    //https://{URL}/info.php?service=1&service_sub_id=0&service_second_sub_id=0&service_third_sub_id=0&amount=1&id=1
    //      &user=593333333&date=2012-08-01+19%3A02%3A09&hash=
    //      4a5952493501c40a0fb205433c7ca62f0cefc002703b2470b0bdaad11157348b&AGENT=USER&CANAL=0
    
    
    //https://{URL}/pay.php?service=77&service_sub_id=0&service_second_sub_id=0&service_third_sub_id=0&amount=1
    // &id=14&user=Z189404578252&date=2012-05-21+19%3A26%3A08&hash=fcc7196557825e7b612cdfcf5fb0d4444cf114a84c1a4d55d9c339c65307eb7e&AGENT=USER&
    // CANAL=0
struct service_t
{
    int64_t id; // service - id
    int64_t sub_id;
    int64_t second_sub_id;
    int64_t third_sub_id;
};

struct acc_t
{
    std::string url   ;
    std::string hash  ;
    std::string agent ;
    std::string canal ;
};

struct request_t
{
    struct service_t service; 
    //struct acc_t     acc    ;

    int64_t     amount     ;
    int64_t     trn_id     ;
    std::string user_login ;
    std::string date_ts    ;
};

struct error_code
{
    int32_t code;
    std::string ru ; // russian message
    std::string ge ; // georgian message
    std::string en ; // english message
};
struct response_t
{
    error_code ec            ;
    int64_t   gel            ;
    int64_t   txn_id         ;
    std::string currency     ;
    double rate              ;
    double generated_amount  ;
};

class manager_t
{
public:
    explicit manager_t(const struct acc_t & acc);
    ~manager_t();
    
    struct response_t info( const struct request_t& req);
    struct response_t pay( const struct request_t &req);
    
private:
    std::string make_hash(const struct request_t & req);//no const
    
private:
    acc_t acc;
};


}}}} // end oson::backend::merchant::money_mover_llc

namespace oson{ namespace backend{ namespace merchant{ namespace webmoney{
    
struct request_t
{
    typedef int64_t     int_t    ;
    typedef std::string str_t    ;
    typedef double      amount_t ;
    
    int_t id   ;
    int_t regn ; // nomer zaprosa
    str_t wmid ;
    int_t test ;
    struct sign_t{
        int_t type;
        str_t content;
        str_t key_file_path;
        
        static str_t key_file_from_config();
    }sign;
    
    struct payment_t
    {
        str_t    currency  ;
        str_t    exchange  ;
        str_t    pspname   ; // FIO of user, win-1251 codec.
        str_t    pspcode   ; // KOD pasport of user's government: win1251 codec.
        str_t    pspnumber ; // seriya and number pasport
        str_t    pspdate   ; // date of pasport when given. YYYYMMDD format. YYYY - year, MM - month, DD - day.
        str_t    purse     ; // 13 symbols number webmoney. first symbol E or Z, others 12 - are digits. spaces are not allowed.
        amount_t price     ; // Euro Or Dollar,  10.5,  or 20   9.3 VALID.   9.30, 10.500  are NOT VALID.
    }payment;
};

struct response_t
{
    typedef int64_t      int_t ;
    typedef std::string  str_t ;
    typedef double       amount_t;
     
    int_t retval; //error code:  0 - success. 
    str_t retdesc; // error message; most 255 symbols, win1251 codec.
    str_t description;
    
    boost::system::error_code ec; 
    
    struct payment_t
    {
        str_t    pspname   ; // FIO of user, win-1251 codec.
        str_t    pspcode   ; // KOD pasport of user's government: win1251 codec.
        str_t    pspnumber ; // seriya and number pasport
        str_t    pspdate   ; // date of pasport when given. YYYYMMDD format. YYYY - year, MM - month, DD - day.
        str_t    purse     ; // 13 symbols number webmoney. first symbol E or Z, others 12 - are digits. spaces are not allowed.
        amount_t price     ; // Euro Or Dollar,  10.5,  or 20   9.3 VALID.   9.30, 10.500  are NOT VALID.
        amount_t rest      ; // remain amount
        amount_t amount    ;
        int_t    wmtranid  ;
        int_t    tranid    ;
        struct limit_t
        {
            amount_t daily_limit; // maximal possible daily limit
            amount_t monthly_limit;
        }limit;
    }payment;
};

struct acc_t
{
    std::string atm_prepay_url;
    std::string atm_pay_url;
};

std::string make_check_sign(const struct request_t& request) ;
std::string make_pay_sign(const struct request_t & request ) ;

typedef  ::std::function< void (const request_t&, const response_t&) > handler_t;

class manager_t
{
public:
    explicit manager_t(const struct acc_t &  acc) ;
    
    
//    struct response_t check_a(const struct request_t& request) const;
//    struct response_t pay_a(const struct request_t& request ) const;
//    struct response_t info_pay_a(const struct request_t& request) const;
//    
//    struct response_t check_wmc(const struct request_t& request)const;
//    struct response_t pay_wmc(const struct request_t& request) const;
//    struct response_t info_pay_wmc(const struct request_t& request)const;
//    struct response_t reverse_pay_wmc(const struct request_t& request)const;
//    
    struct response_t check_atm(const struct request_t& request)const;
    struct response_t pay_atm(const struct request_t& request)const;
    struct response_t get_currency_atm(const struct request_t& request ) const;
    struct response_t pay_provider_atm(const struct request_t& request ) const;
    
    void async_check_atm( const request_t& req, handler_t h ) const;
    void async_pay_atm  ( const request_t& req, handler_t h ) const;
    
private:
    acc_t acc_;
};

}}}} // end oson::backend::merchant::webmoney

namespace oson{ namespace backend{ namespace merchant{ namespace Ucell{
    
    
struct public_key_info
{
    std::string modulus    ;
    std::string exponenta  ;
    
    std::string expiration ;
    std::string timestamp  ;
    
    std::string to_xml_key()const;
};

struct auth_info
{
    std::string url      ;
    std::string app_name ;
    std::string app_pwd  ;
    std::string app_key  ;
    
    public_key_info key_info;
};

struct acc_t
{
    int32_t     merchant_id ;
    std::string url         ;
    std::string login       ;
    std::string password    ;
    
    std::string api_json    ;
};

struct request_t
{
    std::string clientid  ; // phone
    std::string ts        ; // need for cancel
    int64_t     trn_id    ;
    int64_t     amount    ;
    
    int         timeout_millisec; 
    
    inline request_t(): clientid(), ts(), trn_id(0), amount(0), timeout_millisec(0){}
};

struct response_t
{
    std::string clientid        ;  // phone
    std::string status_text     ;
    int64_t     status_value    ;
    double     available_balance;
    std::string timestamp       ;
    std::string provider_trn_id ;
    
   inline response_t(): status_value(-1){}
};

typedef std::function< void( const request_t&, const response_t& ) > handler_type;

class manager_t
{
public:
    explicit manager_t( const acc_t & acc ) ;
    ~manager_t();
    
    int info(const request_t& req, response_t& resp  );
    
    int check(const request_t& req, response_t& resp );
    
    int pay(const request_t& req, response_t & resp  );

    int cancel(const request_t& req, response_t& resp);
    
    
    void async_info(const request_t& req, handler_type h);
    void async_check(const request_t& req, handler_type h);
    void async_pay(const request_t& req, handler_type h );
    void async_cancel(const request_t& req, handler_type h ) ;
    
private:
    int request_public_key(public_key_info& key_info);
    std::string make_token( const std::string& path);
    std::string method_hash(const std::string& method_name ) ;
    
    int save_auth_info_to_db(const auth_info& info);
    int check_expiration_auth_info();
private:
    acc_t acc_;
    auth_info info_;
};



}}}} // end oson::backend::merchant::Ucell

namespace oson{ namespace backend{ namespace merchant{ namespace Uzmobile{
    
enum ErrorStatus
{
    ES_SUCCESS= 0,
    ES_REQUIRED_PARAMETER_NOT_FOUND = 1,
    ES_PARAMETER_VALUE = 2,
    ES_ACCESS_DENIED = 3,
    ES_UNKNOW_ERROR = 100,
};
    
struct request_t
{
    int64_t     amount ;  //  v tiyinax
    int64_t     trn_id ;  //  transaction-id
    std::string msisdn ;  //  phone number
    std::string ts     ;  //  date of payment
};

struct access_t
{
    std::string login    ;
    std::string password ;
};

struct response_t
{
    int64_t     status_value;
    std::string status_text;
    
    std::string ts;
    std::string txn_id; // remote side transaction-id
    
};

typedef std::function< void (const request_t&,  const response_t& ) > handler_type;


class manager_t
{
public:
    explicit manager_t( const access_t & acc );
    
    ~manager_t();
    
    response_t check( const request_t&  ) ;
    response_t pay  ( const request_t&  ) ;
    response_t info ( const request_t&  ) ;
    
    
    void async_check( const request_t& , handler_type handler ) ;
    void async_pay  ( const request_t& , handler_type handler ) ;
    void async_info ( const request_t& , handler_type handler ) ;
private:
    access_t acc_;
};

}}}} // end oson::backend::merchant::Uzmobile

namespace oson{ namespace backend{  namespace merchant{ namespace tps{
    
struct request_t
{
    std::string command ;
    int64_t trn_id  ;
    
    std::string account ;
    
    std::string account_type ;
    
    int64_t summ        ;
    std::string date    ;
    
    int terminal        ;
};

struct response_t
{
    int64_t     result    ;
    std::string out_text  ;
    std::string status    ;
    
    std::string ts        ;
    
    std::string oper_id   ;
    
    std::string tr_state  ;
    
    std::string acc_saldo ;
    std::string subject_name;
};

struct access_t
{
    std::string username;
    std::string password;
    std::string url;
};

typedef std::function< void(const request_t&, const response_t& ) > handler_type;

class manager_t
{
public:
    explicit manager_t( const access_t & acc);
    ~manager_t();
    
    response_t check( const request_t & ) ;
    response_t info ( const request_t & ) ;
    response_t pay  ( const request_t & ) ;
    
    void async_check( const request_t&, handler_type ) ;
    void async_info ( const request_t&, handler_type ) ;
    void async_pay  ( const request_t&, handler_type ) ;
private:
    access_t acc_;
};

}}}} // end oson::backend::merchant::tps



namespace oson{ namespace backend{ namespace merchant{ namespace QiwiWallet{
    
enum class CMD    
{
    none  = 0 ,
    check = 1 ,
    pay   = 2 ,
    status =3 ,
};

struct request_t
{
    int cmd             ; // One of CMD enum value.
    int64_t trn_id      ; // oson transaction id.
    double  amount      ; // amount  in RUB, USD or EUR  --- used only pay
    std::string ccy     ; // currency, 'RUB', 'USD', 'EUR'.  
    std::string account ; // client account-number:  phone number in global format,  
    
    inline request_t(): cmd(0), trn_id(0), amount(0), ccy(), account(){}
};

struct response_t
{
    int64_t status_value;
    std::string status_text;
    
    int64_t result_code ;
    
    std::string txn_id; //Qiwi transaction id
    
    bool final_status;
    bool fatal_error;
    
    std::string txn_date;
    
    std::string raw_data;
    struct balance_t
    {
        double rub ;
        double usd ;
        double eur ;
        
        inline balance_t(): rub(0), usd(0), eur(0){}
    } balance ;
    
    
    boost::system::error_code ec;
    
    inline bool success()const{  return result_code == 0; } 
    inline response_t(): status_value(-1), status_text(), result_code(300), txn_id(), final_status(false), fatal_error(false){}
};

struct acc_t
{
    std::string terminal_id ; // 1745
    std::string password    ; // rfhpntxeb6
    std::string url         ;
};

typedef std::function< void (const request_t& req,  const response_t& resp) >  handler_type;

//https://api.qiwi.com/xml/topup.jsp
class manager_t
{
public:
    explicit manager_t( const acc_t & acc ) ;
    ~manager_t();
    
    void info(const request_t& req,   response_t& resp);
    void check(const request_t& req,  response_t& resp);
    void pay(const request_t& req,    response_t& resp);
    void status(const request_t& req, response_t& resp);
    void balance(const request_t& req, response_t& resp);
    
    void async_info(const request_t& req,  handler_type h );
    void async_check(const request_t& req, handler_type h );
    void async_pay(const request_t& req, handler_type h      ) ;
    void async_status(const request_t& req, handler_type  h  ) ;
    void async_balance(const request_t& res, handler_type  h ) ;
    
private:
    acc_t m_acc;
};
    
    
} } } } // end oson::backend::merchant::QiwiWallet


namespace oson{ namespace backend{ namespace merchant{ namespace nativepay{
    
    
    //1. Запрос на получение информация о балансе агента
    // http://servername/Finance/Agent/Balance?command=check&hash=XXXXXXXXXXX
    
    //2.  Пример запроса на проверку состояния абонента
    // http://servername/api/gate2/service?command=check&account=0957835959
    
    //3. Пример запроса на пополнение лицевого счета
    // http://servername/api/gate2/service?command=pay&txn_id=1234567&txn_date=20170815120133&account=0957835959&sum=180.00&sum2=200.00&payer_info=Иванов_Иван_Удв_123456789
    
    
struct acc_t
{
    std::string login    ;
    std::string password ;
    std::string url      ;// http://servername   IP .
};


struct request_t
{
    std::string  service    ; // identifier of provider.
    int64_t      txn_id     ; // transaction ID from agent (oson).
    std::string  txn_date   ; // 20050815120133  YYYYMMDDHHmmss  format date.
    std::string  account    ; // abonent identifier - account (login).
    double       sum        ; // summa to abonent schet.
    double       sum2       ; // summa without percent(rate) -- optional parameter.
    std::string  payer_info ; // abonent name ( optional parameter ).
    
    std::string  hash       ; // need for balance.
    
    inline request_t(): sum(0.0), sum2(0.0){}
};


struct response_t
{
    std::string txn_id   ;   //  ID agent's
    std::string prv_txn  ;   //  ID system operator's, i.e.  ID from nativepay side.
    double      sum      ;   //  accepted sum
    int32_t     result   ;   //  result of response.
    std::string comment  ;   //  comment
    std::string currency ;   //  USD, RUB, EUR, ... ISO code.
    double      rate     ;   //  course conversation.
    double      amount   ;   //  conversion amount of currency format.
    
    
    //inner oson connection status and status-text.
    int32_t  status;
    std::string status_text; 
    
    struct balance_t
    {
        double balance;
        std::string currency;
        inline balance_t(): balance(0.0){}
    }balance;
    
    inline response_t(): sum(0.0), result(0), rate(0.0), amount(0.0), status(0){}
};



struct error_codes
{
    enum codes
    {
        OK                      = 0 ,
        REQUEST_ACCEPTED        = 1 ,
        WRONG_ACCOUNT_FORMAT    = 4 ,
        NOT_FOUND_ACCOUNT       = 5 ,
        LIMIT_BALANCE_EXCEEDED  = 7 ,
        PAYMENT_DISABLED_BY_TECHNICAL_SITUATION = 8 ,
        
        ABONENT_SCHET_NOT_ACTIVE = 79  ,
        SUMMA_TOO_SMALL          = 241 ,
        SUMMA_TOO_BIG            = 242 ,
        
        NOT_DETECT_SCHET         = 243 ,
        
        OTHER_ERROR_OPERATORS    = 300 ,
    };
};


enum cmd_codes
{
    cmd_none    = 0 ,
    cmd_balance = 1 ,
    cmd_check   = 2 ,
    cmd_pay     = 3 ,
};


typedef  ::std::function< void ( const request_t&, const response_t& ) > handler_t;

class manager_t
{
public:
    explicit manager_t( const acc_t & acc ) ;
    ~manager_t();
    
    void check( const request_t& req,   response_t& resp ) ;
    void balance(const request_t& req,  response_t& resp ) ;
    void pay(const request_t& req, response_t & resp ) ;
    
    
    void async_check   ( const request_t& req,  handler_t h);
    void async_balance ( const request_t& req, handler_t h ) ;
    void async_pay     ( const request_t& req, handler_t h );
private:
    std::string get_http_request(const std::string& req_str ) ;
    
    
    static void parse_xml( const std::string & resp_xml, response_t& resp, int cmd );
    
    static void parse_xml_check(const std::string & resp_xml, response_t& resp ) ;
    static void parse_xml_pay(const std::string & resp_xml, response_t& resp ) ;
    static void parse_xml_balance(const std::string & resp_xml, response_t& resp) ;
    
    
private:
    acc_t m_acc; 
};

    
}}}} // end oson::backend::merchant::nativepay


namespace oson{ namespace backend{ namespace merchant{ namespace MTS{
    
//1) запрос на платеж  
//2) провести платеж
//3) узнать статус платежа
//4) отмена платежа
//5) сверка
//6) запрос состояния сверки    

struct acc_t
{
    struct cert_info_t
    {
        std::string private_key_file ; // ums_privatekey.pem
        std::string public_key_file  ; // oson_public.pem
        std::string verify_file        ; // ums_pc.crt
    }cert_info ;
    
    void load_cert_from_config();
    
    std::string url ;// https://dlsdfld
    std::string login;
    std::string password;
};
 
    
    
struct request_t
{
    enum ESPP_Types
    {
        ESPP_none = 0,
        
        ESPP_0104010 = 1,  // Запрос разрешения приема платежа
        
        ESPP_0104090 = 2,  //   Принять документ о платеже. Двухэтапный процессинг
        
        ESPP_0104085 = 3, // Запросить состояние платежа
        
        ESPP_0104050 = 4, // Реестр принятых платежей для проведения сверки
        
        ESPP_0104051 = 5,  //Запрос результатов сверки
        
        ESPP_0104213 = 6,  //Запрос на  отмену платежа.
    };
    
    ESPP_Types espp_type ;
    std::string phone    ;
    int64_t check_id ;
    int64_t trn_id   ;
    std::string ts       ;
    std::string ts_tz ;
    std::string raw_info ;
    std::string espp_trn_id; //for paystatus
    double sum; // summa in 'SUM' (860 ISO code).
    
    std::string reestr_b64;
    
    static std::string make_reestr_b64(const Purchase_list_T& list, const std::string & from_date, const std::string & to_date );
};

struct response_t
{
    enum ESPP_Types
    {
        ESPP_none = 0,
        
        ESPP_1204010 = 1, // Разрешение приема платежа
        ESPP_2204010 = 2, // Отказ приёма платежа.
        
        ESPP_1204090 = 3, // Документ акцептован. Двухэтапный процессинг
        ESPP_2204090 = 4, // Документ не акцептован. Двухэтапный процессинг
        
        
        ESPP_1204085 = 5, // Состояние платежа
        ESPP_2204085 = 6, // Состояние платежа не определено
        
        ESPP_1204050 = 7, // Подтверждение приема реестра принятых платежей для проведения сверки
        ESPP_2204050 = 8, // Отказ в приеме реестра принятых платежей для проведения сверки
        
        
        ESPP_1204051 = 7, // Результаты сверки
        ESPP_2204051 = 8, // Отказ в предоставлении результатов сверки
        
        ESPP_1204213 = 9, // Подтверждение отмены платежа
        ESPP_2204213 = 10, //Отказ в отмене платежа
    };
    
    
    ESPP_Types espp_type;
    
    std::string espp_trn_id;
    std::string code_operator;
    std::string name_operator;
    std::string user_fio;
    std::string oson_limit_balance ;
    std::string lc_user;
    std::string ts;
    std::string code_reject;
    std::string desc_reject;
    
    std::string reestr_id;
    std::string reestr_res;
    int64_t status;
    std::string raw;
};

class manager_t
{
public:
    explicit manager_t(acc_t const& acc);
    ~manager_t();

    void send_request(const request_t& req, response_t& resp ) ;

    void from_xml( std::string const& resp_xml, response_t& resp);
    std::string to_xml(const request_t& req);

private:
    std::string sync_send_request( std::string const& req_xml, const request_t& req ) ;
    std::string make_sign(const request_t& req ) ;
    
private:
    acc_t m_acc;
};
    
    
}}}} // end oson::backend::merchant::MTS

namespace oson{ namespace backend{ namespace merchant{ namespace HermesGarant{

struct acc_t
{
    std::string agent_password;
    std::string agent_id;
    std::string url;
};

struct request_t
{
    int64_t trn_id ;
    double amount  ;
    std::string ts ;
    std::string service_id;
    std::string account;
    std::string currency;
    //std::string request_type;
};
struct commands
{
    enum
    {
        cmd_none    = 0,
        cmd_check   = 1,
        cmd_pay     = 2,
        cmd_balance = 3,
        cmd_terminal_check = 4,
    };
};

struct response_t
{
    int64_t trn_id;
    int64_t req_id;
    int64_t     resp_status;
    std::string message;
    std::string resp_type;
    
    Error_T err_value;
    
    struct transaction_content
    {
       std::string service;
       std::string account;
       std::string amount;
       std::string currency;
       std::string exchange_rate;
       std::string service_currency;
    }tc;
    
    struct balances
    {
       struct pair{  std::string currency; std::string balance; };
       std::vector< pair > list;
    }bc;
    
    
    inline response_t(): trn_id(0), req_id(0), err_value(Error_OK){}
};


struct error_codes
{
    enum
    {
        success                = 10,
        trans_processed        = 9,
        request_processed      = 5,
        trans_processed_v2     = 3,
        trans_created          = 1,
        
        auth_error             = -1,
        not_enough_amount      = -2,
        format_account_invalid = -100,
        account_not_found      = -105,
        service_access_deinied = -110,
        technical_error        = -115,
        account_not_active     = -120,
        amount_very_small      = -201,
        amount_very_big        = -202,
        Bill_cant_check        = -203,
        Unknown_service_error  = -300,
        Error_on_request       = -500,
    };
    static  inline bool is_final(int ec){ return ec == error_codes::success || ec < 0 ; } 
};


typedef  ::std::function< void (const struct request_t& req,  const struct response_t& resp) > handler_t;

class manager_t
{
public:
    explicit manager_t(const struct acc_t& acc);
    ~manager_t();
    
    void check(request_t const& req,  response_t & resp);
    void pay(request_t const & req, response_t& resp);
    void balance(request_t const& req, response_t& resp);
    

    void async_pay(const struct request_t& req,  handler_t h );
private:
    static void parse_resp(const std::string& resp_json, response_t& resp);
    
    std::string make_body(const request_t& req, int cmd );
    
private:
    struct acc_t m_acc;
};

    
}}}}// end oson::backend::merchant::HermesGarant




#endif // EOCP_API_T_H
