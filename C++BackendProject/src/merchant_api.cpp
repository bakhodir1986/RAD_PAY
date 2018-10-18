
#include <stdexcept>
#include <string>
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>


#include <boost/algorithm/string/predicate.hpp>  // boost::algorithm::contains
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>


#include "merchant_api.h"

#include "log.h"
#include "http_request.h"
#include "utils.h"
#include "osond.h"
#include "paynet_gate.h"
#include "users.h"
#include "application.h"
#include "purchase.h"
#include "runtime_options.h"
#include "transaction.h"


static std::string authorization_base_auth(const std::string& login, const std::string& password );
static std::string sync_http_request( oson::network::http::request http_rqst );
static std::string sync_http_ssl_request(oson::network::http::request req, boost::asio::ssl::context::method ssl_method = boost::asio::ssl::context::sslv23 );
static bool text_is_similar( std::string text, std::string orig );
static std::string map_to_json_string(const std::map< std::string , std::string> & obj);

////////////////////////////////////////////////////////////////////////////////
///////////////////////////// INNER FUNCTIONS   ///////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static std::string to_json_str(const  Merch_acc_T& acc)
{
    return "{ \"login\": \"" + acc.login + + "\" , \"password\": \"" + acc.password + "\"  } " ; 
}
////////////////////////////////////////////////////////////////////////////////
static std::string prettify_phone_number_uz( std::string  phone  )
{
    if (phone.empty())return phone;
    if (phone[0] == '+' )phone.erase(0,1);
    if (phone.length() == 9) phone = "998" + phone;
    if(phone.length() != 12) return phone;
    
    static const int group[5] = { 3, 2, 3, 2, 2 } ;
    
    std::string result = "+";
    
    for(int i = 0, p = 0;  i < 5; ++i){
        for(int j = 0; j < group[i]; ++j){
            result += phone[p++];
        }
        
        result += ' ';
    }
    return result;
}
/**********************************************************************************************/    
oson::Merchant_api_manager::Merchant_api_manager(  std::shared_ptr< boost::asio::io_service > io_service )
 : io_service_( io_service )
{
    namespace ssl = boost::asio::ssl;
    
    ctx_sslv23 = std::make_shared< ssl::context>( ssl::context::sslv23 );
    ctx_sslv23->set_default_verify_paths();
    ctx_sslv23->set_verify_mode( ssl::context::verify_fail_if_no_peer_cert | ssl::context::verify_peer);
    
    
    
    ctx_sslv3 = std::make_shared< ssl::context>( ssl::context::sslv3 );
    ctx_sslv3->set_default_verify_paths();
    ctx_sslv3->set_verify_mode( ssl::context::verify_fail_if_no_peer_cert | ssl::context::verify_peer);
    
}

oson::Merchant_api_manager::~Merchant_api_manager()
{}

std::shared_ptr< boost::asio::io_service >  oson::Merchant_api_manager::get_io_service()const{ return io_service_; }
std::shared_ptr< boost::asio::ssl::context> oson::Merchant_api_manager::get_ctx_sslv23()const{ return ctx_sslv23; }
std::shared_ptr< boost::asio::ssl::context> oson::Merchant_api_manager::get_ctx_sslv3 ()const{ return ctx_sslv3;  }


static std::string get_munis_sysinfo_sid(int64_t oson_tr_id)
{
    SCOPE_LOG(slog);
    std::string result;
    
    Purchase_details_info_T purch_info;
    Purchase_T purch( oson_this_db  );
    Error_T ec = purch.get_detail_info(oson_tr_id, purch_info);
    
    if ( ec ){
         slog.WarningLog("can't find purchase info");
         return result;
    }
    result = extract_sysinfo_sid( purch_info.json_text );
    
    return result;

}

Error_T oson::Merchant_api_manager::init_fields(int32_t merchant_id, int64_t oson_tr_id, const std::vector< Merchant_field_data_T> & list,
      /*out*/ Merch_trans_T& trans, /*out*/Merchant_field_data_T & pay_field
    ) 
{
    SCOPE_LOG(slog);
    
    Error_T ec = Error_OK ;
    
    Merchant_T merch( oson_this_db ) ;
    
    
    std::vector<Merchant_field_T> fields = merch.request_fields(merchant_id);

    std::map<std::string, std::string> merch_api_params;

    bool pay_field_initialized = false;

    for(const Merchant_field_T& field : fields)
    {
        size_t const idx_list   =  std::find_if( list.begin(), list.end(), fid_comparator::equal( field.fID ) )  - list.begin() ;
        if (  ! ( idx_list < list.size() )  ) {
            slog.WarningLog("No in input");
            continue;
        }

        switch( field.type)
        {
            case M_FIELD_TYPE_INPUT:
            {
                
                std::string value  = list[idx_list].value ;
                std::string prefix = list[idx_list].prefix;
                
                if ( ! oson::utils::valid_ascii_text( value ) )
                {
                    slog.ErrorLog("value NOT ASCII formatted: '%s'", value.c_str());
                    return Error_parameters;
                }
                
                merch_api_params[field.param_name] = prefix + value;
                
                if (merchant_id == merchant_identifiers::Electro_Energy)
                {
                    merch_api_params[field.param_name] = value; // without prefix.
                }
                
                //check pay_field contraction.
                if ( ! pay_field_initialized )
                {
                    pay_field = list[ idx_list ];

                    int32_t const value_size = pay_field.value.size();
                    if ( ! (value_size  >= field.min_length &&  value_size  <=  field.max_length  ) ) {
                        slog.WarningLog("Parameter length incorrect %d <= %d <= %d",  field.min_length, value_size, field.max_length);
                        return Error_parameters;
                    }
                    slog.DebugLog("pay_field.prefix: '%s'\tvalue: '%s'", pay_field.prefix.c_str(), pay_field.value.c_str());

                    pay_field.value       = pay_field.prefix + pay_field.value;
                    pay_field.max_size    = field.max_length;
                    pay_field_initialized = true;
                }
                else {
                    //this only texnomart, yet.
                   
                    const bool concate_needed = 
                                merchant_identifiers::is_texnomart(merchant_id )            ||
                                merchant_id == merchant_identifiers::Gross_insurance        ||
                                merchant_id == merchant_identifiers::Insurance_TEZ_Leasing  ||
                                merchant_id == merchant_identifiers::Insurance_Avant_Avto   ||
                                merchant_id == merchant_identifiers::AsiaTV                 ||
                                // allow all merchants concatenation login's.
                                true ;
                    
                    if (concate_needed)
                    {
                        pay_field.value += "; " + list[idx_list].value ;
                    }
                }
                //find service_id_check
                {
                    Merchant_field_data_T fdata, search;
                    search.fID    = field.fID;
                    search.prefix = prefix;

                    Error_T ec = merch.field_data_search(search, Sort_T(0, 0), fdata);
                    if ( ec == Error_OK && fdata.service_id_check > 0)
                    {
                        trans.service_id_check = fdata.service_id_check;
                    }
                }
            }
            break;
            case M_FIELD_TYPE_LIST:
            {
                std::string value = list[idx_list].value;
                
                Merchant_field_data_T fdata;
                fdata.fID = field.fID;
                fdata.key = string2num(value);
                Error_T ec = merch.field_data_search(fdata, Sort_T(0, 0), fdata);
                if(ec == Error_OK) 
                {
                    if ( fdata.service_id != 0 )
                    {
                        trans.service_id = to_str( fdata.service_id ) ;
                        slog.DebugLog("trans.service_id: %s", trans.service_id.c_str());
                    }

                    if (fdata.service_id_check != 0 )
                    {
                        trans.service_id_check = fdata.service_id_check;
                    }
                    
                    //@Note: Я пока не уверен других мерчантах
                    if(merchant_id == merchant_identifiers::TPS_I ||
                       merchant_id == merchant_identifiers::TPS_I_direct)
                    {
                        value = to_str(fdata.extra_id);
                    }
                    else if (merchant_id == merchant_identifiers::Elmakon_Direct   )
                    {
                        value = fdata.value ;
                    }
                    else
                    {
                        slog.WarningLog("(line: %d) param_name: '%s'  is not set", __LINE__, field.param_name.c_str());
                    }
                }
                slog.DebugLog("param[%s]: '%s'", field.param_name.c_str(), value.c_str());
                merch_api_params[ field.param_name ] = value; 
            }
            break;
            default: break;
        }//end switch
    }//end for
    //@Note: Munis services. 
    
    if (merchant_id == merchant_identifiers::Elmakon_Direct  ) 
    {
        pay_field.value += ";" + merch_api_params["oson_merch_filial"];
    }
    
    if ( merchant_identifiers::is_munis(merchant_id)  ) 
    {
        if ( oson_tr_id != 0 )
        {
            try{
                merch_api_params["sysinfo_sid"] = get_munis_sysinfo_sid( oson_tr_id );
            } catch( std::exception& e ) {
                slog.ErrorLog("Can't parse sysinfo_sid!");
                return Error_parameters;
            }
        } else 
        {
            slog.WarningLog("oson_tr_id required parameter is zero!");
            return Error_parameters;
        }
    }
    
    if (oson_tr_id != 0 ) {
        
        Purchase_details_info_T purch_info;
        Purchase_T purch( oson_this_db  );
        Error_T ec = purch.get_detail_info(oson_tr_id, purch_info);
        if ( ! ec ) 
        {
            trans.info_detail = purch_info ;
        } else {
            slog.WarningLog("Not found oson_tr_id = %ld in DB", oson_tr_id ) ;
        }
    }
    
    slog.InfoLog("trans.service_id: %s ", trans.service_id.c_str());
    
    //@Note: swap - is very lightweight operation than assign. 
    trans.merch_api_params.swap( merch_api_params );

    if ( trans.service_id.empty() )
    {
        Merchant_info_T minfo = merch.get(merchant_id, /*out*/ ec ) ;
        
        if (   ec == Error_OK ) {
            if ( ! minfo.extern_service.empty() &&  minfo.extern_service != "0" ) { 
                trans.service_id = minfo.extern_service ;
            }
        }
    }
    slog.InfoLog("trans.service_id: %s ", trans.service_id.c_str());
    
   return Error_OK ; 
}

Currency_info_T oson::Merchant_api_manager::currency_now_or_load( int type )
{
    SCOPE_LOG(slog);
    
    Currency_info_T currency;
    
    Currencies_table table( oson_this_db ) ;
    
    currency = table.get( type ) ;
    
    if ( ! currency.initialized ) // there today's currency does not exist.
    {
        //////////////////////
        Merchant_info_T info;
        Merch_acc_T acc;
        Merchant_api_T api(info, acc);

        currency = api.get_currency_now_cb(type);

        if (currency.initialized) 
        {
            currency.id = table.add( currency );
        }
    }
    
    slog.InfoLog("currency(%s): usd-uzs: %.12f, usd-rub: %.12f, usd-eur: %.12f type: %d", 
            ( currency.initialized ? "initialized" : "NOT initialized"), currency.usd_uzs, currency.usd_rub, currency.usd_eur, currency.type  ) ;

    return currency;
}

void oson::Merchant_api_manager::async_check_status(const Merch_trans_T& trans, check_status_handler handler)
{
    io_service_->post( boost::bind( &self_t::check_status, this, trans, handler) ) ;
}

void oson::Merchant_api_manager::check_status(const Merch_trans_T& trans, check_status_handler handler)
{
    SCOPE_LOGD(slog);
    typedef merchant_identifiers mis;
    
    const int32_t merchant_id = trans.merchant.id ;

    try
    {
        if ( trans.merchant.api_id == merchant_api_id::mplat  )
        {
            return check_status_mplat(trans, handler ) ;
        }
        
        {
            const bool is_paynet_script =  trans.merchant.api_id == merchant_api_id::paynet ;
            if (is_paynet_script)
            {
                return this->check_status_paynet( trans, handler ) ;
            }
        }
        
        if  ( trans.merchant.api_id == merchant_api_id::qiwi ) 
        {
            return check_status_qiwi(trans, handler);
        }
        
        switch(merchant_id)
        {
            case mis::Beeline_test_dont_use:
            case mis::Beeline :  return check_status_beeline(trans, handler);
            case mis::Webmoney_Direct: return check_status_webmoney(trans, handler);
            case mis::Ucell_direct: return check_status_ucell( trans, handler);
            default: return check_status_oson_api(trans, handler);
        }
    }
    catch(std::exception& e)
    {
        slog.ErrorLog("exception: %s", e.what());
        return handler(trans, {}, Error_internal);
    }
}

void oson::Merchant_api_manager::async_purchase_info(const Merch_trans_T& trans, purchase_info_handler handler)
{
    io_service_->post( std::bind(&self_t ::purchase_info, this, trans, handler ) ) ;
}

void oson::Merchant_api_manager::purchase_info(const Merch_trans_T& trans, purchase_info_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    typedef merchant_identifiers mis;
    
    const int32_t merchant_id = trans.merchant.id ;
    
    try
    {
        if (  trans.merchant.api_id == merchant_api_id::mplat  )
        {
            return this->purchase_info_mplat(trans, handler);
        }
         
        if (merchant_id == mis::Electro_Energy || merchant_id == mis::Musor)
        {
            return this->purchase_info_paynet( trans, handler ) ;
        }
        
        if (merchant_id == mis::Webmoney_Direct ) 
        {
            return this->purchase_info_webmoney(trans, handler ) ;
        }
        
        if ( trans.merchant. api_id == merchant_api_id::qiwi  ) 
        {
            return  purchase_info_qiwi(trans, handler);
        }
        
        if ( mis::Ucell_direct == merchant_id )
        {
            return purchase_info_ucell(trans, handler ) ;
        }
        
        {
            slog.WarningLog("There no purchase info API implemented for this merchant. merchant-id: %d", (int)merchant_id);
            return handler(trans, {}, Error_internal);
        }
    }
    catch( std::exception& e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        return handler(trans, {}, Error_internal);
    }
}

void oson::Merchant_api_manager::async_perform_purchase(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    io_service_->post( std::bind(& self_t::perform_purchase, this, trans, handler ) ) ;
}


void oson::Merchant_api_manager::perform_purchase(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    SCOPE_LOGD(slog);
    typedef merchant_identifiers mis;
    
    const int32_t merchant_id = trans.merchant.id ; // this is always noexcept

    try
    {
        if ( trans.merchant.api_id == merchant_api_id::mplat  )
        {
            return perform_purchase_mplat(trans, handler);
        }
        //check it paynet
        {
            const bool is_paynet_script =   trans.merchant.api_id == merchant_api_id::paynet ;
            if (is_paynet_script)
            {
                return this->perform_purchase_paynet( trans, handler ) ;
            }
        }
        
        if  ( trans.merchant.api_id == merchant_api_id::qiwi  ) 
        {
            return perform_purchase_qiwi(trans, handler);
        }
        switch(merchant_id)
        {
            case mis::Beeline_test_dont_use:
            case mis::Beeline: return perform_purchase_beeline(trans, handler);
            case mis::Ucell_direct: return perform_purchase_ucell(trans, handler) ;
            case mis::Webmoney_Direct: return perform_purchase_webmoney(trans, handler ) ;
            default: return perform_purchase_oson_api(trans, handler);
        }
    }
    catch( std::exception& e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        return handler(trans, {}, Error_internal);
    }
}

void oson::Merchant_api_manager::async_perform_status(const Merch_trans_T& trans, perform_status_handler handler )
{
    io_service_->post( std::bind(&self_t::perform_status, this, trans, handler)) ;
}

void oson::Merchant_api_manager::perform_status(const Merch_trans_T& trans, perform_status_handler handler ) 
{
    
    SCOPE_LOGD(slog);
    
    const int32_t merchant_id = trans.merchant.id ; // this is always noexcept.
     
    try
    {
        if ( trans.merchant.api_id == merchant_api_id::mplat  )
        {
             return perform_status_mplat(  trans, handler )   ; 
        }
        else if( trans.merchant. api_id == merchant_api_id::qiwi  ) 
        {
            return perform_status_qiwi( trans, handler ) ;
        }
        else if ( trans.merchant.api_id == merchant_api_id::hermes_garant )  //mis::is_HermesGarant(merchant_id) ) 
        {
            return perform_status_hg(trans, handler ) ;
        }
        else
        {
            slog.WarningLog("unknown merchant-id: %d", (int)merchant_id);
            return handler(trans, {}, Error_internal);
        }
    }
    catch(std::exception& e)
    {
        slog.ErrorLog("exception: %s", e.what());
        return handler(trans, {}, Error_internal);
    }
}

void oson::Merchant_api_manager::check_status_beeline(const Merch_trans_T& trans, check_status_handler handler)
{
    SCOPE_LOGD(slog);
    
    std::string phone    = trans.param;
    std::string msisdn   = phone; 
    std::string amount   = num2string( trans.amount / 100 ); // convert to sum.
    std::string login    = trans.acc.login;
    std::string password = trans.acc.password;

    //https://37.110.208.11:8444/work.html?ACT=0&USERNAME=oson&PASSWORD=Oson1505&MSISDN=901093865&PAY_AMOUNT=500&CURRENCY_CODE=2BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON
    std::string address = trans.merchant.url + "/work.html?ACT=7&USERNAME="+login+"&PASSWORD="+password+"&MSISDN="+msisdn+"&PAY_AMOUNT="+amount;
    
    auto req_ = oson::network::http::parse_url( address );
    
    struct on_finish
    {
        Merch_trans_T trans;
        check_status_handler handler;
        
        void operator()(const std::string & xml, const boost::system::error_code& ec)const
        {
            SCOPE_LOGD(slog);

            Merch_check_status_T status;

            //@Note: add there more error processing.
            if (  ec   )
            {
                slog.WarningLog("ec.value: %d, ec.message: %s", ec.value(), ec.message().c_str());
                return handler( trans, status, Error_merchant_operation ) ;
            }

            slog.DebugLog("response: xml:%s", xml.c_str());

            try
            {
               namespace pt =   boost::property_tree;

               std::istringstream stream( xml );

               pt::ptree  root;
               pt::read_xml(stream, root);
               
               status.status_value  = root.get< int64_t >("pay-response.status_code", -999);
            }
            catch( std::exception& e )
            {
               slog.ErrorLog("Can't parse xml. exception: %s", e.what());
               return handler(trans, status, Error_merchant_operation );
            }

            static int64_t const  BEELINE_STATUS_OK = 21  ;

            status.exist = ( BEELINE_STATUS_OK ==  status.status_value  ) ;
            
            return handler(trans, status, Error_OK ) ;
        }
    }http_handler = { trans, handler } ; // c++11 supported this!
    
    ///////////////////////////////////////////////////////////////
    //now we used  c++11 compatible compiler!  gcc-4.8 or higher.
    auto c = std::make_shared< oson::network::http::client >( io_service_, ctx_sslv23 );

    c->set_request( req_ );

    c->set_response_handler( http_handler );

    c->async_start();
    //////////////////////////////////////////////////////////
}

void oson::Merchant_api_manager::perform_purchase_beeline(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    std::string phone    = trans.param;
    std::string msisdn   = (phone.size() >= 9) ? phone.substr(phone.size() - 9) : phone; // last 9 digits
    std::string amount   = num2string( trans.amount / 100 ); // convert to sum.
    std::string login    = trans.acc.login;
    std::string password = trans.acc.password;
    
    //https://37.110.208.11:8444/work.html?ACT=0&USERNAME=oson&PASSWORD=Oson1505&MSISDN=901093865&PAY_AMOUNT=500&CURRENCY_CODE=2BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON
    std::string path = trans.merchant.url + "/work.html?ACT=0&USERNAME="+login+"&PASSWORD="+password+"&MSISDN="+msisdn+"&PAY_AMOUNT="+amount+"&CURRENCY_CODE=2&BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON";
    
    struct http_handler_t
    {
        oson::Merchant_api_manager * this_ ;
        
        Merch_trans_T trans;
        oson::Merchant_api_manager::perform_purchase_handler handler;
        Merch_trans_status_T result;
        
        std::string pay_id;
        std::string receipt;
        std::string ts_create, ts_commit;
        
        void on_transaction_create(const std::string& xml, const boost::system::error_code & ec )
        {
            SCOPE_LOGD(slog);
            
            if(ec)
            {
                slog.WarningLog("error value: %d, error-message: %s", ec.value(), ec.message().c_str());
                return handler(trans, result, Error_merchant_operation);
            }
            
            slog.DebugLog("RESPONSE: %s", xml.c_str());
            
            int status_code = 0;
            
            try
            {
                namespace pt =   boost::property_tree;

                std::istringstream stream( xml );

                pt::ptree  root;
                pt::read_xml(stream, root);
                pt::ptree resp = root.get_child("pay-response");

                status_code = resp.get<int>("status_code", -999999);

                pay_id = resp.get<std::string>("pay_id", std::string() );

                ts_create = resp.get< std::string > ("time_stamp", std::string("2000-01-01 00:00:00"));

            } 
            catch(std::exception & e)
            {
                slog.ErrorLog("Exception: %s", e.what());
                return handler(trans, result, Error_internal);
            }
            
            result.merchant_status  = status_code;
    
            if (status_code < 0) 
            {
                slog.ErrorLog("status-code: %d", status_code ) ;
                return handler(trans, result, Error_merchant_operation);
            }
            
            
            //pay_id - идентификационный номер транзакции.
            //status_code – результат операции
            //time_stamp – дата проведения операции (DD.MM.YYYY HH24:MI:SS)
            std::string login    = trans.acc.login;
            std::string password = trans.acc.password;

            std::string path = trans.merchant.url + "/work.html?ACT=1&USERNAME="+login+"&PASSWORD="+password+"&PAY_ID="+ pay_id ;
            {
                 auto req_ = oson::network::http::parse_url(path);
                 auto c    = std::make_shared< oson::network::http::client >( this_->io_service_, this_->ctx_sslv23);
                 c->set_request(req_);
                 c->set_response_handler(  std::bind(&http_handler_t::on_transaction_confirm, *this, std::placeholders::_1, std::placeholders::_2 ) ) ;
                 c->async_start();
            }
        }
        
        void on_transaction_confirm(const std::string& xml, const boost::system::error_code& ec)
        {
            SCOPE_LOGD(slog);
            if (ec)
            {
                slog.WarningLog("error value: %d, error-message: %s", ec.value(), ec.message().c_str());
                return handler(trans, result, Error_merchant_operation);
            }

            slog.DebugLog("RESPONSE: %s", xml.c_str());

            int status_code = 0;
            try
            {
                namespace pt =   boost::property_tree;

                std::istringstream stream( xml );

                pt::ptree  root;
                pt::read_xml(stream, root);
                pt::ptree resp = root.get_child("pay-response");

                status_code = resp.get<int>("status_code", -999999);

                receipt = resp.get<std::string>("receipt", std::string() );

                ts_commit = resp.get< std::string > ("time_stamp", std::string("2000-01-01 00:00:00"));

            }
            catch(std::exception& e)
            {
                slog.ErrorLog("Exception: %s", e.what());
                return handler(trans, result, Error_internal);
            }

            result.merchant_trn_id =  receipt;

            result.merchant_status  = status_code;

            result.merch_rsp =  pay_id + "," + ts_create + "," + ts_commit;

            //receipt - номер чека	
            //status_code – результат операции
            //time_stamp – дата проведения операции (DD.MM.YYYY HH24:MI:SS)

            result.ts = ts_commit;
            
            if (status_code < 0) 
            {
                slog.ErrorLog("status-code: %d", status_code ) ;
                return handler(trans, result, Error_merchant_operation);
            }
            
            return handler(trans, result, Error_OK ) ;
        }
    } http_handler = { this, trans, handler } ; // c++11 allowed that.
    
    //////////////////////////////////////////////////////////////////////////////
    {
        auto req_ = oson::network::http::parse_url(path);
        auto c    =  std::make_shared< oson::network::http::client >(io_service_, ctx_sslv23);
        
        c->set_request(req_);
        c->set_response_handler( std::bind( &http_handler_t::on_transaction_create, http_handler, std::placeholders::_1, std::placeholders::_2) );
        c->async_start();
    }
    ///////////////////////////////////////////////////////////////////////////////
}


void oson::Merchant_api_manager::check_status_oson_api(const Merch_trans_T& trans, check_status_handler handler)
{
    SCOPE_LOGD(slog);
     
    const auto& acc      = trans.acc;
    const auto& merchant = trans.merchant;
    
    // { "id" : 1, "jsonrpc" : "2.0" , "method" : "user.check", "params" : acc } 
    std::string root = 
            "{  \"jsonrpc\" : \"2.0\",  \"id\" : 1, \"method\" : \"user.check\", \"params\" : { \"acc\": " + to_json_str(acc) + 
            ", \"info\": { \"login\" : \"" + trans.param + "\" } }  } " ;
    
    auto req_http            = oson::network::http::parse_url(merchant.url);
    req_http.method          = "POST"; // jsonrpc works only POST method.
    req_http.content.type    = "application/json";
    req_http.content.charset = "UTF-8";
    req_http.content.value   = root;
    
    slog.DebugLog("Request: %s", root.c_str() );
    
    auto http_handler = [trans, handler](const std::string& resp_s,  boost::system::error_code ec)
    {
        SCOPE_LOGD(slog);
        
        Merch_check_status_T response;
        slog.DebugLog("Response: %s",  resp_s.c_str());
        if (static_cast<bool>(ec) || resp_s.empty())
        {
            slog.WarningLog("ec = %d, mesg: %s", ec.value(), ec.message().c_str());
            return handler(trans, response, Error_merchant_operation);
        }
        
        namespace pt = boost::property_tree;

        pt::ptree resp_json;

        std::istringstream ss(resp_s);
        
        try
        {
            pt::read_json(ss, resp_json);
        }
        catch( std::exception& e)
        {
            slog.WarningLog("exception: %s", e.what());
            
            return handler(trans, response, Error_merchant_operation);
        }

        pt::ptree resp_result = resp_json.get_child("result" ) ;

        if ( resp_result.count("error") ) 
        {
            pt::ptree resp_err = resp_result.get_child("error");
            int code = resp_err.get< int >("code") ;
            
            if (code != 0)
            {
                slog.WarningLog("code is not zero: %d", code);
                return handler(trans, response, Error_merchant_operation);
            }
        }

        std::string ex = resp_result.get< std::string > ("exist", "false");

        response.exist = ( ex == "true" ) ; 
        
        return handler( trans, response, Error_OK );

    };
   
    ////////////////////////////////////////
    auto client_http = oson::network::http::client::create(io_service_, ctx_sslv23 ) ;

    client_http->set_request(req_http);

    client_http->set_response_handler(http_handler);

    client_http->async_start();

}

static std::string to_json_str(const Merch_trans_T& trans){
    return "{ \"amount\" : " + to_str(trans.amount) 
            + ", \"login\": \"" + trans.param 
            + "\", \"time\": \"" + trans.ts 
            + "\" , \"transID\": " + to_str(trans.transaction_id) 
            + "} " ;
}

void oson::Merchant_api_manager::perform_purchase_oson_api(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    const auto& acc      = trans.acc;
    const auto& merchant = trans.merchant;
    
    std::string req_s = "{ \"jsonrpc\" : \"2.0\", \"id\" : 1, \"method\" : \"transaction.perform\", \"params\" : { \"acc\": " 
            + to_json_str(acc) + " , \"trans\" : " + to_json_str( trans ) +   " } } " ;
    
    auto req_http            = oson::network::http::parse_url(merchant.url);
    req_http.method          = "POST";
    req_http.content.charset = "UTF-8";
    req_http.content.type    = "application/json";
    req_http.content.value   = req_s;
    
    slog.DebugLog("Request: %s",   req_s .c_str());
    //std::string resp_s = sync_http_request(req_);// = sync_http_ssl_request(req_,  boost::asio::ssl::context::sslv3_client);
    
    auto http_handler = [trans, handler](const std::string& resp_s, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog);
        Merch_trans_status_T response;
        
        slog.DebugLog("Response: %s",  resp_s .c_str());
        
        if (static_cast<bool>(ec) || resp_s.empty())
        {
            response.merchant_status = ec.value();
            response.merch_rsp       = ec.message();
            return handler(trans, response, Error_merchant_operation);
        }

    // {
    //   "jsonrpc" : "2.0",
    //   "id" : "1",
    //   "result" : {
    //      "status" : 5,
    //      "message" : "\u041a\u043b\u0438\u0435\u043d\u0442 \u043d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d",
    //      "providerTrnId" : 1,
    //      "ts" : "2018-03-08 01:03:35"
    //   }
    //}


        namespace pt = boost::property_tree ;

        pt::ptree resp_json;
        std::istringstream ss(resp_s);

        try
        {
            pt::read_json(ss, resp_json);
        }
        catch( std::exception & e )
        {
            slog.ErrorLog("exception: %s", e.what());
            response.merch_rsp = e.what();
            return handler(trans, response, Error_merchant_operation);
        }

        if ( ! resp_json.count("result") ) 
        {
            slog.ErrorLog("There no 'result in response!!!!");
            response.merch_rsp = "There no 'result' in response!!!" ;
            return handler( trans, response, Error_merchant_operation ) ;
        }

        pt::ptree result_json = resp_json.get_child("result");
        if (result_json.count("error") ) {
            int code = result_json.get< int >("result.code", -999);
            if (code != 0){
                slog.ErrorLog("code is not zero : %d", code);
                response.merch_rsp = "code is not zero: " + to_str(code);
                return handler(trans, response, Error_merchant_operation ) ;
            }
        }

        pt::ptree const& in = result_json;

        response.merchant_status = in.get<int>("status",  -1 );
        response.ts = in.get< std::string > ("ts", "1977-07-07 07:07:07");
        response.merchant_trn_id = in.get< std::string >("providerTrnId", "0");
        response.merch_rsp = in.get< std::string >("message", "-.-.-"); //@Note: cron-telecom  message uses KOI-7, where absolute couldn't read.
        response.merch_rsp = "base64(" + oson::utils::encodebase64(response.merch_rsp) + ")";

        if (response.merchant_status != 0)
        {
            slog.WarningLog("status is not zero: %d", response.merchant_status);
            return handler( trans, response, Error_merchant_operation ) ;
        }

        
        return handler( trans, response, Error_OK) ;
    };
    
    /////////////////////
    
    auto client_http = oson::network::http::client::create(io_service_, ctx_sslv23);
    
    client_http->set_request(req_http);
    
    client_http->set_response_handler(http_handler);
    
    client_http->async_start();
    
}


/*********************************  UCELL     ***********************************/
void oson::Merchant_api_manager::purchase_info_ucell(const Merch_trans_T& trans, purchase_info_handler handler)
{
    SCOPE_LOGD(slog);
    Merch_trans_status_T response;
    std::string phone;
    ///////////////////////////////////////////
    {
        auto const& pms = trans.merch_api_params;
        auto it_ph      = pms.find("clientid");

        if ( it_ph == pms.end() || (*it_ph).second.empty() ) 
        {
            slog.WarningLog("clientid not found!");
            return handler(trans, response, Error_parameters ) ;
        }

        phone = (*it_ph).second;
        
        //add 998
        if (phone.length() == 9) {
            phone = "998" + phone;
        }
    }
    /////////////////////////////////////////
    /////////////////////////////////////////
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = trans.acc.login    ;
    ucell_acc.password    = trans.acc.password ;
    ucell_acc.api_json    = trans.acc.api_json ;
    ucell_acc.url         = trans.merchant.url ;
    ucell_acc.merchant_id = trans.merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    
    ucell_req.timeout_millisec = 3854 ;// ~4 seconds.
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    auto ucell_handler = [response, trans, handler, phone ](Ucell::request_t const&, Ucell::response_t const & ucell_resp) mutable
    {
        SCOPE_LOGD(slog);
        int r  = ucell_resp.status_value;
        std::string pretty_phone = prettify_phone_number_uz(phone);
        if (r < 0)
        {
            slog.WarningLog("ucell check failed");

            response.merchant_status = 0;

            response.merch_rsp = ucell_resp.status_text;

            response.kv["clientid"] = pretty_phone;

            return handler(trans, response, Error_OK ) ;
        }


        response.merchant_status = ucell_resp.status_value;
        response.merch_rsp       = ucell_resp.status_text ;

        if ( std::abs( ucell_resp.available_balance ) > 0.5E-4 )
        {
            response.kv["AvailableBalance"] = "$" + to_str( ucell_resp.available_balance, 4, false );
        }
        else
        {
            response.kv["AvailableBalance"] = "$0.00" ;
        }

        response.kv["StateValue" ]      = ucell_resp.status_text ;
        response.kv["clientid"]         = pretty_phone;

        return handler(trans, response, Error_OK );
    };
    //////////////////////////////////////////////////
    ucell_mng.async_info(ucell_req, ucell_handler ) ;
    
}

void oson::Merchant_api_manager::check_status_ucell(const Merch_trans_T& trans, check_status_handler handler ) 
{
    SCOPE_LOGD(slog);
     std::string phone;
    ///////////////////////////////////////////
    {
        const auto& pms = trans.merch_api_params;
        auto iter      = pms.find("clientid");

        if ( iter == pms.end() || (*iter).second.empty() ) {
            slog.WarningLog("clientid not found!");
            
            Merch_check_status_T status;
            status.exist = false;
            return handler(trans, status, Error_parameters);
        }

        phone = (*iter).second;
    }
    /////////////////////////////////////////
    const auto& acc      = trans.acc;
    const auto& merchant = trans.merchant ;
    
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = acc.login    ;
    ucell_acc.password    = acc.password ;
    ucell_acc.api_json    = acc.api_json ;
    ucell_acc.url         = merchant.url ;
    ucell_acc.merchant_id = merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    struct ucell_handler_t
    {
        Merch_trans_T trans;
        check_status_handler handler;
        
        void operator()(const Ucell::request_t&, const Ucell::response_t& resp)
        {
            SCOPE_LOGD(slog);
            Merch_check_status_T status;
            status.exist = ( 0 == resp.status_value ) ;

            status.status_value = resp.status_value;
            status.status_text  = resp.status_text ;

            handler(trans, status, Error_OK );
        }
    } ucell_handler = { trans, handler } ;
    
    ucell_mng.async_check(ucell_req, ucell_handler);
}

void oson::Merchant_api_manager::perform_purchase_ucell(const Merch_trans_T& trans, perform_purchase_handler handler )
{
    SCOPE_LOGD(slog);
    
    std::string phone;
    {
        auto const& mps = trans.merch_api_params;
        auto it = mps.find("clientid");
        if (it == mps.end() || (*it).second.empty() ) 
        {
            slog.WarningLog("There no clientid parameter!");
            Merch_trans_status_T response;
            response.merchant_status = -1;
            response.merch_rsp = "There no clientid parameter!";
            
            return handler( trans, response, Error_parameters ) ;
        }
        phone = (*it).second;
    }
    
    const auto& acc       =   trans.acc      ;
    const auto& merchant  =   trans.merchant ;
    
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = acc.login    ;
    ucell_acc.password    = acc.password ;
    ucell_acc.api_json    = acc.api_json ;
    ucell_acc.url         = merchant.url ;
    ucell_acc.merchant_id = merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    ucell_req.amount    = trans.amount;
    
    auto ucell_handler = [=](const Ucell::request_t&, const Ucell::response_t & ucell_resp)
    {
        SCOPE_LOGD(slog);
        Merch_trans_status_T response;

        response.merchant_status = ucell_resp.status_value;
        response.merch_rsp       = ucell_resp.status_text ;
        response.merchant_trn_id = ucell_resp.provider_trn_id;
        response.ts              = ucell_resp.timestamp ;

        Error_T ec = Error_OK ;

        if (ucell_resp.status_value != 0 ) {
            slog.WarningLog("status is not zero!");
            ec =  Error_merchant_operation;
        }

        response.merch_rsp += " " + response.ts; 

        return handler(trans, response, ec ) ;
    };
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    ucell_mng.async_pay( ucell_req,  ucell_handler );
} // end perform_ucell


void oson::Merchant_api_manager::purchase_info_mplat(const Merch_trans_T& trans, purchase_info_handler handler)
{
    SCOPE_LOGD(slog);
    namespace Mplat = oson::backend::merchant::Mplat;
    
    std::string account;
    
    if (trans.merch_api_params.count("account")) 
    {
        account = trans.merch_api_params.at( "account" );
    }
    else if (! trans.param.empty() ) 
    {
        account = trans.param;
    }
    else if (trans.merch_api_params.count("login"))
    {
        account = trans.merch_api_params.at("login") ;
    }
    else
    {
        account = "< ???? >" ;
    }
    
    
    slog.DebugLog("account: %.*s\n",  ::std::min<int>(2048, account.size() ), account.c_str());
    
    Mplat::request_t mplat_req;
    mplat_req.auth.login    = "oson_uz_xml";
    mplat_req.auth.agent    = "39";
    mplat_req.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    mplat_req.body.account = account;
    mplat_req.body.date    = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    mplat_req.body.service = string2num( trans.service_id ) ;
    mplat_req.body.type    = "CHECK";
    
    //request.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t mplat_acc;
    
    mplat_acc.login   = "oson_uz_xml";
    mplat_acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    mplat_acc.agent   = "39";
    mplat_acc.url     = trans.merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    mplat_acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.

    ///////////////////////////////////
    Merch_trans_status_T response;

    //response.merchant_trn_id  = mplat_response.txn ;
    //response.kv["balance"] = mplat_response.balance ;
    struct fix_client_rate_t
    {
        std::string operator()(const std::string& value)const
        {
            std::string::size_type dot = value.find('.');
            
            if (dot == value.npos  ) // there no dot.
                return value + ".00";
            
            if (dot + 1 == value.size()) // 1799. 
                return value + "00"; // 1799.00
            
            if (dot + 2 == value.size() ) // 1799.7
                return value + "0"; // 1799.70
            
            std::string::size_type i = value.size();
            
            //strip last zero.
            while( i > dot + 3 && value[ i - 1 ] == '0')
                --i;
            
            if (i == value.size())
                return value;
            
            //dot + 2 < i < value.size()
            return value.substr( 0, i );

        }
    }fix_client_rate;

    //if (trans.uid != 0 )
    {
        std::string oson_note ;
      
        if ( trans.uid != 0 )
        {
            if (account == trans.user_phone)
            {
                //if(trans.uid_lang == LANG_uzb)
                //    oson_note = "To'g'ri" ;
                //else
               //     oson_note = "Правильно" ;
            }
            else if ( text_is_similar(account, trans.user_phone)){
                if (trans.uid_lang == LANG_uzb)
                    oson_note = "Login va telefon raqamingiz mos kelmaydi, davom etasizmi?" ;
                else
                    oson_note = "Логин и номер вашего телефона не совпадают, продолжить?" ;
            } else {
                if (trans.merch_api_params.count("oson_last_purchase_login")){
                    std::string last_login = trans.merch_api_params.at("oson_last_purchase_login") ;

                    if (account == last_login)
                    {
                       // if(trans.uid_lang == LANG_uzb)
                       //     oson_note = "To'g'ri" ;
                       // else
                       //     oson_note = "Правильно" ;
                    }
                    else
                    if (text_is_similar(account, last_login)){
                        if (trans.uid_lang == LANG_uzb)
                            oson_note= "Login oxirgi tulov bilan mos kelmaydi, davom etasizmi?" ;
                        else
                            oson_note = "Логин не совпадает с последного оплату, продолжить?" ;
                    }
                }
            }

            if ( ! oson_note.empty()  ) 
                oson_note += "\n";
        }
        
        if (trans.uid_lang == LANG_uzb){
            oson_note += "Ushbu to'lov to'liq amalga oshishi uchun 5-10 minut vaqt talab etiladi!" ;
        } else {
            oson_note += "Этот платеж займет около 5-10 минут!" ;
        }
        
        slog.InfoLog("oson_note: %s", oson_note.c_str());
        
        response.kv["oson_note"] = oson_note;
        response.kv["login"] = account;
        
    }
    
    /////////////////////////////////////////////
    auto mplat_handler = [=](const Mplat::request_t& mplat_req, const Mplat::response_t& mplat_resp, Error_T ec)
    {
        SCOPE_LOGD(slog);
        slog.DebugLog("mplat-account: %s", mplat_req.body.account.c_str());
        Merch_trans_status_T status = response;
        if ( ec )
        {
            slog.WarningLog("ec: %d", (int)ec);
            return handler(trans, status, ec);
        }

        //example: <r type="CHECK" result="0" client_rate="166.0830000" currency="RUR" message="OK" info="Extra test field"/>
    
        status.merchant_status = mplat_resp.result ; //@Note, result actually status code.
        status.merch_rsp       = mplat_resp.message ; // status - text status description
        //response.ts              = mplat_response.message
        

        status.kv["client_rate"] = fix_client_rate( mplat_resp.client_rate ) ;
        status.kv["currency"]    = mplat_resp.currency ;
        
        ec = (Error_T) mplat_resp.result_to_oson_error();

        return handler( trans, status, ec ) ;
    };
    
    ////////////////////////////////////////
    Mplat::manager_api mplat_api(mplat_acc);
    
    mplat_api.async_check(mplat_req, mplat_handler );
}

void oson::Merchant_api_manager::perform_status_mplat(const Merch_trans_T& trans, perform_status_handler handler)
{
    SCOPE_LOGD(slog);
    
    const auto& merch_params = trans.merch_api_params;
    
    if( ! merch_params.count("txn"))
    {
        slog.WarningLog("Not found 'txn' parameter!");
        return handler(trans, Merch_trans_status_T{}, Error_parameters);
    }
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    Mplat::request_t mplat_req;
    mplat_req.auth.login    = "oson_uz_xml";
    mplat_req.auth.agent    = "39";
    mplat_req.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    mplat_req.body.type     = "STATUS";
    mplat_req.body.txn      = merch_params.at("txn") ; //merchant_trn_id ;
    
    Mplat::acc_t mplat_acc;
    
    mplat_acc.login   = "oson_uz_xml";
    mplat_acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    mplat_acc.agent   = "39";
    mplat_acc.url     = trans.merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    mplat_acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.

    struct mplat_perform_status_handler
    {
         Merch_trans_T  trans;
         perform_status_handler handle;
         
         void operator()(const struct Mplat::request_t& mplat_req,  const struct Mplat::response_t& mplat_response, Error_T ec)
         {
             SCOPE_LOGD(slog);
             
             slog.DebugLog("  == mplat_perform_status_handler == ");
             
             Merch_trans_status_T  response ;
         
             if ( ec ) 
             {
                 slog.WarningLog("ec: %d", (int)ec ) ;
                 return handle(trans, response, ec);
             }
             
            response.merchant_status = mplat_response.result ;
            response.merch_rsp       = mplat_response.message ;
            bool const in_progress = (response.merchant_status == 26 || response.merchant_status == 90 || response.merchant_status == 91) ;

            if (in_progress)
            { 
                slog.WarningLog(" ==== in progress ==== " );
                return handle(trans, response, Error_perform_in_progress) ;
            }


            if ( !( response.merchant_status  ==  0 || // OK/SUCCESS
                    response.merchant_status  == 25 || // New transaction NOT FATAL
                    response.merchant_status  == 26 || // Payment in progress NOT FATAL
                    response.merchant_status  == 90 || // Conducting the payment is not completed NOT FATAL
                    response.merchant_status  == 91    // Suspicious payment NOT FATAL
                  ) 
              )
            {
                ec = (Error_T)mplat_response.result_to_oson_error() ;
                
                return handle(trans, response,  ec  );
            }

            return handle(trans, response, Error_OK);
         }
    }mplat_handler = {trans, handler};

    ////////////////////////////////////////////////
    Mplat::manager_api mplat_api(mplat_acc);
    
    mplat_api.async_status(mplat_req, mplat_handler);
}

void oson::Merchant_api_manager::check_status_mplat(const Merch_trans_T& trans, check_status_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    Mplat::request_t mplat_req;
    mplat_req.auth.login    = "oson_uz_xml";
    mplat_req.auth.agent    = "39";
    mplat_req.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    mplat_req.body.account = trans.param ;
    mplat_req.body.date    = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    mplat_req.body.service = string2num( trans.service_id ) ;
    mplat_req.body.type    = "CHECK";
    
    mplat_req.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t mplat_acc;
    
    mplat_acc.login   = "oson_uz_xml";
    mplat_acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    mplat_acc.agent   = "39";
    mplat_acc.url     = trans.merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    mplat_acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
    
    auto mplat_handler = [ = ](const Mplat::request_t& , const Mplat::response_t& mplat_resp, Error_T ec )
    {
        Merch_check_status_T status;
        
        status.exist        = ( Error_OK ==  ec ) &&  ( 0 == mplat_resp.result )  ;
        status.status_value = mplat_resp.result;
        status.status_text  = mplat_resp.message;
        
        if ( ! ec )
            ec = (Error_T)mplat_resp.result_to_oson_error() ;
        
        return handler(trans, status, ec);
    };
    
    ////////////////////////////////////////////////
    Mplat::manager_api mplat_api(mplat_acc);
    
    
    mplat_api.async_check(mplat_req, mplat_handler);
    
}

void oson::Merchant_api_manager::perform_purchase_mplat(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    Mplat::request_t mplat_req;
    mplat_req.auth.login    = "oson_uz_xml";
    mplat_req.auth.agent    = "39";
    mplat_req.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    mplat_req.body.account  = trans.param ;
    mplat_req.body.date     = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    mplat_req.body.service  = string2num( trans.service_id)  ;
    mplat_req.body.type     = "PAY";
    mplat_req.body.id       = to_str(trans.transaction_id );
    mplat_req.body.amount   = to_money_str( trans.amount, '.' );
    
    //request.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t mplat_acc ;
    
    mplat_acc.login   = "oson_uz_xml";
    mplat_acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    mplat_acc.agent   = "39";
    mplat_acc.url     = trans.merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    mplat_acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
    
    auto mplat_handler = [trans, handler](const Mplat::request_t&, const Mplat::response_t& mplat_resp, Error_T ec )->void
    {
        Merch_trans_status_T response ;

        if (ec)
        {
            response.merch_rsp = mplat_resp.message;
            response.merchant_status = mplat_resp.result ;
        
            return handler(trans, response, ec);
        }
            
        
        response.merchant_status = mplat_resp.result;
        response.merch_rsp       = mplat_resp.message;
        response.merchant_trn_id = mplat_resp.txn;
        //response.ts              = mplat_resp.ts ;

        if (mplat_resp.result == 91)// Podozritelniy
        {
        }

        if (mplat_resp.result == 26 || 
            mplat_resp.result == 90 || 
            mplat_resp.result == 91
           )
        {
            return handler(trans, response, Error_perform_in_progress ) ;
        }
        
        if ( !( response.merchant_status  ==  0 || // OK/SUCCESS
                response.merchant_status  == 25 || // New transaction NOT FATAL
                response.merchant_status  == 26 || // Payment in progress NOT FATAL
                response.merchant_status  == 90 || // Conducting the payment is not completed NOT FATAL
                response.merchant_status  == 91    // Suspicious payment NOT FATAL
              ) 
          )
        {
            ec = (Error_T)mplat_resp.result_to_oson_error();
            return handler( trans, response, ec ) ;
        }

        return handler(trans, response, Error_OK );
    };
    //////////////////////////////////////////////////
    Mplat::manager_api mplat_api( mplat_acc );
    
    mplat_api.async_pay(mplat_req, mplat_handler );
    
}
/*************  PAYNET    *****************************************************************/
    
void oson::Merchant_api_manager::purchase_info_paynet(const Merch_trans_T& trans, purchase_info_handler handler)
{
    SCOPE_LOGD(slog);
    //////////////////////////////////////////////////////////////////////////////////////////
    namespace paynet = oson::backend::merchant::paynet;
    paynet::request_t req;
    req.act    = 0  ; // needn't
    req.amount = trans.amount / 100 ;
   // req.client_login;//needn't
   // req.client_login_param_name;//needn't
    req.fraud_id       = trans.user_phone       ;
    req.param_ext      = trans.merch_api_params ;
    req.service_id     = string2num( trans.service_id )      ;
    req.transaction_id = trans.transaction_id   ;

    paynet::access_t acc;
    acc.merchant_id  = trans.merchant.merchantId;
    acc.password     = trans.acc.password ; //"NvIR4766a";
    acc.terminal_id  = trans.acc.options  ; //"4119362";
    acc.url          = trans.acc.url      ; //"https://213.230.106.115:8443/PaymentServer";
    acc.username     = trans.acc.login    ; //"akb442447";

    paynet::manager paynet_api(acc);

    paynet::info_t info;

    auto paynet_handler = [trans, handler](const paynet::request_t&, const paynet::response_t& info)
    {
        SCOPE_LOGD(slog);
        slog.DebugLog("resp.status: %d, text: %s", info.status, info.status_text.c_str());
        Merch_trans_status_T response;
        response.kv              = info.kv;
        response.merch_rsp       = info.status_text;
        response.merchant_status = info.status;
        response.merchant_trn_id = info.transaction_id;
        response.ts              = info.ts;

        response.kv_raw          = map_to_json_string(info.kv);
        
        Error_T ec = info.status == 0 ? Error_OK : Error_merchant_operation;
        return handler(trans, response, ec);
    };
    
    ///////////////////////////////////////////////
    paynet_api.async_get_info(req, paynet_handler);
}

void oson::Merchant_api_manager::check_status_paynet(const Merch_trans_T& trans, check_status_handler handler ) 
{
    SCOPE_LOGD(slog);
 
    Merch_check_status_T status;//result
    
    if ( ! trans.service_id_check   ) // no service id for check
    {
        slog.WarningLog("There no service_id_check!!");
        status.exist = true ;
        return handler(trans, status, Error_OK) ;
    }
    
    bool const paynet_extern_service =  string2num( trans.merchant.extern_service ) > 0   ;
    bool const paynet_service        =  string2num( trans.service_id  ) > 0  ; // extention fields need.
    
    namespace paynet = oson::backend::merchant::paynet;
    
    paynet::access_t acc;
    acc.merchant_id  = trans.merchant.merchantId;
    acc.password     = trans.acc.password ; // "NvIR4766a";
    acc.terminal_id  = trans.acc.options  ; // "4119362";
    acc.url          = trans.acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = trans.acc.login    ; // "akb442447";

    paynet::request_t req;
    req.act            = 0;//needn't .
    req.amount         = trans.amount / 100; // sum
    req.fraud_id       = trans.user_phone ;//oson::random_user_phone();
    req.client_login   = trans.param;
    req.service_id     = trans.service_id_check;
    req.transaction_id = trans.transaction_id;
    

    size_t len_match = 0;
    for( const auto& e :  trans.merch_api_params )
    {
        
        slog.DebugLog("merch_param[%s]: '%s'", e.first.c_str(), e.second.c_str());
        
        if ( e.second.length() > len_match && boost::ends_with(req.client_login, e.second)  )
             req.client_login_param_name = e.first, len_match = e.second.length(); // max match suffix will choose.
    }

    
    if (!paynet_extern_service && paynet_service) // extra
    {
        req.client_login.clear();
        req.client_login_param_name.clear();
        req.param_ext = trans.merch_api_params;
    }
    
    
    paynet::response_t resp;
    resp.status = 0;
    
    paynet::manager manager(acc);
    
    
    auto paynet_handler = [=](const paynet::request_t&, const paynet::response_t& resp)
        {
            SCOPE_LOGD(slog);
            Merch_check_status_T status_copy = status;
            slog.DebugLog("resp.status = %d, resp.status-text: %s", resp.status, resp.status_text.c_str());
            status_copy.exist =   ( resp.status == 0 );
            return handler(trans, status_copy, Error_OK ) ;
        };
            
    /////////////////////////////////////////////        
    manager.async_perform(req, paynet_handler ) ;
}


void oson::Merchant_api_manager::perform_purchase_paynet(const Merch_trans_T& trans, perform_purchase_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    bool const paynet_extern_service =  string2num(  trans.merchant.extern_service  )  > 0 ;
    bool const paynet_service        =  string2num(  trans.service_id  ) > 0 ; // extention fields need.
    
    
    namespace paynet = oson::backend::merchant::paynet;
    
    paynet::access_t acc;
    acc.merchant_id  = trans.merchant.merchantId;
    acc.password     = trans.acc.password ; // "NvIR4766a";
    acc.terminal_id  = trans.acc.options  ; // "4119362";
    acc.url          = trans.acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = trans.acc.login    ; // "akb442447";

    
    paynet::request_t req;
    req.act            = 0;//needn't .
    req.amount         = trans.amount / 100; // sum
    req.fraud_id       = trans.user_phone ;//oson::random_user_phone();
    req.client_login   = trans.param;
    req.service_id     = string2num( trans.service_id ) ;
    req.transaction_id = trans.transaction_id;
    
    
    size_t len_match = 0;
    for(const auto & e : trans.merch_api_params )
    {
        slog.DebugLog("merch_param[%s]: '%s'", e.first.c_str(), e.second.c_str());
        
        if ( e.second.length() > len_match && boost::ends_with(req.client_login, e.second)  )
             req.client_login_param_name = e.first, len_match = e.second.length(); // max match suffix will choose.
    }
    
    if (!paynet_extern_service && paynet_service) // extra
    {
        slog.WarningLog("take extra parameters");
        req.client_login.clear();
        req.client_login_param_name.clear();
        req.param_ext = trans.merch_api_params;
    }
    
    
    paynet::response_t resp;
    resp.status = 0;
    
    auto paynet_handler = [=](const paynet::request_t& , const paynet::response_t& resp)
    {
        SCOPE_LOGD(slog);
        
        Merch_trans_status_T response;
        
        response.merchant_status = resp.status;
        response.merch_rsp       = resp.status_text ;
        response.merchant_trn_id = resp.transaction_id;
        
        slog.DebugLog("resp.status = %d, status-text: %s", resp.status, resp.status_text.c_str());
        
        Error_T ec = Error_OK ;
        
        if ( resp.status !=  0 ) // zero is success.
            ec = Error_merchant_operation;
        else
        if (resp.transaction_id == paynet::manager::invalid_transaction_value() )
            ec = Error_merchant_operation;
    
        return handler(trans, response, ec);
    };
    
    paynet::manager manager(acc);
    
    manager.async_perform(req, paynet_handler );
}

/******************************************** WEBMONEY   ******************************************************/
void oson::Merchant_api_manager::purchase_info_webmoney(const Merch_trans_T& trans, purchase_info_handler handler)
{
    SCOPE_LOGD(slog);
    
    Merch_trans_status_T response;
    
    namespace webmoney = ::oson::backend::merchant::webmoney;
     
     webmoney::request_t req;
     webmoney::response_t resp;
     webmoney::acc_t  acc;
     
     webmoney::manager_t mng( acc );
     
     if (trans.merch_api_params.count("purse") == 0){
         slog.WarningLog("there no purse !");
         return handler(trans, response, Error_merchant_operation ) ;
     }
     
     req.payment.purse    = trans.merch_api_params.at("purse");
     
     boost::algorithm::trim(req.payment.purse); // remove spaces.
     
     if (req.payment.purse.empty()) {
         slog.WarningLog("purse is empty!");
         return handler(trans, response, Error_merchant_operation );
     }
     
     req.id = trans.transaction_id;
     req.wmid               = "527265668883" ;
     req.sign.key_file_path = req.sign.key_file_from_config();  //"/etc/oson/CA/6696852.key"; //@Note please add this path to config file.
     req.sign.type          = 2;
     req.regn               = 0;
     req.payment.currency = "RUB" ;//"EUR" ;
    char const cur_let = req.payment.purse[0]; // there purse not empty!

    switch(cur_let)
    {
        case 'E': req.payment.currency = "EUR"; break;
        case 'R': req.payment.currency = "RUB"; break;
        case 'Z': req.payment.currency = "USD"; break;
        default: slog.WarningLog("purse is invalid!"); break;
    }
    req.test  = 0;
    Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load( Currency_info_T::Type_Uzb_CB );
    if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency!");
         return handler(trans, response, Error_merchant_operation ) ;
    }
    //slog.DebugLog("ci.eur: %.8f", ci.usd_eur ) ;
    std::string currency_str = "0.0" ;
    req.payment.price = 0.0 ;
     
     //@Note: for webmoney we get commission from trans.amount.
    int64_t amount = trans.amount - trans.merchant.commission(trans.amount);
    std::string cur_symbol;
    double cur_course = 0.0;
    switch(cur_let)
    {
        case 'E': 
             req.payment.price = ci.eur(amount);  
             currency_str = /*"EUR  " +*/ to_str( ci.usd_eur, 4, true ) /*+ "€"*/; 
             cur_symbol = "\u20AC"; 
             cur_course = ci.usd_eur; 
             break;
             
        case 'R': 
             req.payment.price = ci.rub(amount);  
             currency_str = /*"RUB  " +*/ to_str( ci.usd_rub, 4, true ) /*+ "R"*/; 
             cur_symbol = " руб."; 
             cur_course = ci.usd_rub ;
             break;
             
        case 'Z': 
             req.payment.price = ci.usd(amount);  
             currency_str = /*"USD  " +*/ to_str( ci.usd_uzs, 4, true ) /*+ "$"*/; 
             cur_symbol = "$" ; 
             cur_course = ci.usd_uzs;
             break;
             
        default: break;
    }
     
    const double payment_price = req.payment.price;
     
    double const rounded_price = ::std::floor( payment_price * 100.0) / 100.0;
    slog.InfoLog("price: %.8f =>(rounded) => %.2f cur_course: %.12f", req.payment.price, rounded_price , cur_course );

    req.payment.price = rounded_price;
    if (rounded_price < 1.0E-2 ) {
         slog.WarningLog("price is zero, set 0.01!");
         //return Error_not_enough_amount ;
         req.payment.price = 0.01;
    }
     
     
     
     auto webmoney_handler = [trans, handler, response, cur_symbol, currency_str](const webmoney::request_t& req, const webmoney::response_t& resp) mutable
     {
         SCOPE_LOGD(slog);
        response.merchant_status = resp.retval ;
        response.merch_rsp       = resp.retdesc ;
        response.merchant_trn_id = resp.payment.wmtranid ;

        if (resp.retval != 0)
        {
           slog.WarningLog("resp.retval = %ld", resp.retval);

           response.kv["purse" ]   = req.payment.purse ;
           if (resp.ec == boost::asio::error::timed_out || resp.ec == boost::asio::error::operation_aborted )
           {
               response.kv["oson_note"] = "Вебмоней временно недоступен, попробуйте позже!" ;
           }
           else {
            switch(resp.retval)
            {
                case -32: 
                case -38:
                {
                    response.kv["oson_note"] = "Неверный номер кошелька." ;
                }
                break;

                case -700:
                case -711:
                {
                    response.kv["oson_note"] = "Дневной лимит исчерпан." ;
                }
                break;
                case -811:
                case -821:
                {
                    response.kv["oson_note"] = "Месячный лимит исчерпан." ;
                }
                break;
                default:
                {

                    response.kv["oson_note"] = "Неверный номер кошелька ( код ош.ка:   " + to_str(resp.retval) + ")." ;
                }
                break;
            }
           }
           return handler( trans, response, Error_OK ) ;
        }

        std::string amount_credit, total_course;

        const double rounded_price = req.payment.price ;
        
        amount_credit = to_str( rounded_price, 2, false ) + cur_symbol ;

        //trans.amount - given in "TIYIN's, divide it to 100 for convert to 'SUM'.
        double tcourse =  ( trans.amount / 100.0)  / rounded_price ;

        total_course = to_str(tcourse, 2, true);

        //const bool allow_oson_kv = trans.uid < 200 ;

       // if (allow_oson_kv )
        {
           response.kv[ "oson_amount_credit"] = amount_credit ;
           response.kv[ "oson_total_course" ] = total_course  ;
        }

        response.kv[ "currency" ]       = currency_str;
        response.kv[ "purse"    ]       = resp.payment.purse ;
        response.kv[ "daily_limit"]     = to_str( resp.payment.limit.daily_limit, 4, true) + cur_symbol;
        response.kv[ "monthly_limit" ]  = to_str( resp.payment.limit.monthly_limit, 4, true) + cur_symbol;

        return handler( trans, response , Error_OK ) ;
     };
     
     mng.async_check_atm(req, webmoney_handler );
 
     
}

void oson::Merchant_api_manager::check_status_webmoney(const Merch_trans_T& trans, check_status_handler handler) 
{
    SCOPE_LOGD(slog);

    Merch_check_status_T status;
    
    namespace webmoney = ::oson::backend::merchant::webmoney;
    
    webmoney::request_t req;
    webmoney::response_t resp;
    webmoney::acc_t  acc;
     
    webmoney::manager_t mng( acc );
     
     
    if (trans.merch_api_params.count("purse") == 0)
    {
         slog.WarningLog("there no purse !");
         return handler( trans, status, Error_merchant_operation ) ;
    }
     
    req.payment.purse    = trans.merch_api_params.at("purse");
     
    boost::algorithm::trim(req.payment.purse); // remove spaces.
     
    if (req.payment.purse.empty()) {
         slog.WarningLog("purse is empty!");
         return handler( trans, status, Error_merchant_operation ) ;
    }
     
    req.id                 = trans.transaction_id;
    req.wmid               = "527265668883" ;
    req.sign.key_file_path = req.sign.key_file_from_config();//"/etc/oson/CA/6696852.key"; //@Note add this path to config file.
    req.sign.type          = 2;
    req.regn               = 0;
    req.payment.currency   = "RUB" ;//"EUR" ;
    char const cur_let = req.payment.purse[0]; // there purse not empty!

    switch(cur_let)
    {
        case 'E': req.payment.currency = "EUR"; break;
        case 'R': req.payment.currency = "RUB"; break;
        case 'Z': req.payment.currency = "USD"; break;
        default: slog.WarningLog("purse is invalid!"); break;
    }
     req.test  = 0;
     Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load(Currency_info_T::Type_Uzb_CB);
     if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency!");
         return handler(trans, status, Error_merchant_operation ) ;
     }
     //slog.DebugLog("ci.eur: %.8f", ci.usd_eur ) ;
     
     req.payment.price = 0.0 ;
     switch(cur_let)
     {
         case 'E': req.payment.price = ci.eur(trans.amount);break;
         case 'R': req.payment.price = ci.rub(trans.amount);break;
         case 'Z': req.payment.price = ci.usd(trans.amount);break;
         default: break;
     }
     
     double  rounded_price = ::floor(req.payment.price * 100.0) / 100.0;
     slog.InfoLog("price: %.8f =>(rounded) => %.2f", req.payment.price, rounded_price );
     
     
     if (rounded_price < 1.0E-2 ) {
         slog.WarningLog("price is zero, set 0.01!");
         rounded_price = 0.01;
     }
     
     req.payment.price = rounded_price;
     
     //resp = mng.check_atm(req);
     auto webmoney_handler = [ status, trans, handler ]( const webmoney::request_t& req, const webmoney::response_t & resp) mutable
     {
         status.exist        = ( 0 == resp.retval  ) ;
         status.status_value = resp.retval ;

         if (resp.retval != 0 )
         {
            switch(resp.retval )
            {
                case -32:
                case -38:
                {
                    status.status_text = "Неверный номер кошелька.";
                    break;
                }
                case -700:
                case -711:
                {
                    status.status_text = "Дневный лимит исчерпан." ;
                    status.notify_push = true;
                    status.push_text = resp.description + "\nПожалуйста, обратитесь к поставщику услуг."; 
                    break;
                }
                case -811:
                case -821:
                {
                    status.status_text = "Месячный лимит исчерпан." ; 
                    status.notify_push = true;
                    status.push_text   = resp.description + "\nПожалуйста, обратитесь к поставщику услуг.";
                    break;
                }
                default:
                {
                    status.status_text = "Код ошибка: " + to_str(resp.retval ) ;
                    if (resp.ec == boost::asio::error::operation_aborted || resp.ec == boost::asio::error::timed_out ) {
                        status.status_text = "Вебмоней временно недоступен, попробуйте немножка позже!" ;
                    }
                    break;
                }
            }
        }
        return handler( trans, status, Error_OK );
     };
     /************************/
     
     mng.async_check_atm(req, webmoney_handler);
}

void oson::Merchant_api_manager::perform_purchase_webmoney(const Merch_trans_T& trans, perform_purchase_handler handler)
{
    SCOPE_LOGD(slog);

    Merch_trans_status_T response; 
    
    namespace webmoney = ::oson::backend::merchant::webmoney;
     
     webmoney::request_t req;
     
     webmoney::response_t resp;
     
     webmoney::acc_t  acc;
     
     webmoney::manager_t mng( acc );
     
     if (trans.merch_api_params.count("purse") == 0){
         slog.WarningLog("there no purse !");
         return handler( trans, response, Error_merchant_operation ) ;
     }
     
     req.payment.purse    = trans.merch_api_params.at("purse");
     
     boost::algorithm::trim(req.payment.purse); // remove spaces.
     
     if (req.payment.purse.empty() ) {
         slog.WarningLog("purse is empty!");
         return handler( trans, response, Error_merchant_operation ) ;
     }
     
     req.id = trans.transaction_id;
     req.wmid               = "527265668883" ;  //@Note: add this wmid to database!
     req.sign.key_file_path = req.sign.key_file_from_config();//"/etc/oson/CA/6696852.key"; //@Note: add this path to config file
     req.sign.type          = 2;
     req.regn               = 0;
     req.payment.currency   =  "X" ;
     req.payment.exchange   =  ""    ;
     req.test               = 0;
     req.payment.pspdate    = formatted_time_now("%Y%m%d %H:%M:%S") ;
     req.payment.pspnumber  = "787979878987" ;//@Note add this value to database!
     
     char const c_let = req.payment.purse[ 0 ];
     
     Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load(Currency_info_T::Type_Uzb_CB);
     if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency info");
         return handler(trans, response, Error_merchant_operation ) ;
     }
     req.payment.price    = 0.00 ;
     
     switch(c_let)
     {
         case 'R': /*rub*/ req.payment.currency = "RUB"; req.payment.price = ci.rub(trans.amount); break;
         case 'Z': /*usd*/ req.payment.currency = "USD"; req.payment.price = ci.usd(trans.amount); break;
         case 'E': /*eur*/ req.payment.currency = "EUR"; req.payment.price = ci.usd(trans.amount); break;
         default: slog.WarningLog("purse invalid!");break;
     }
     
     double const rounded_price = ::floor(req.payment.price * 100.0) / 100.0;
     slog.InfoLog("amount: %.2f, price: %.8f =>(rounded) => %.2f", trans.amount / 100.0 , req.payment.price, rounded_price );
     
     req.payment.price = rounded_price;
     
     /************************************/
     //resp = mng.pay_atm(req);
     /************************************/
     auto webmoney_handler = [response, trans, handler](const webmoney::request_t & req, const webmoney::response_t & resp ) mutable
     {
        SCOPE_LOGD(slog);
        response.merchant_status = resp.retval ;
        response.merch_rsp       = resp.retdesc ;
        response.merchant_trn_id = to_str( resp.payment.wmtranid ) ;
        Error_T ec = Error_OK ;
        if (resp.retval != 0)
        {
            if (resp.ec == boost::asio::error::timed_out || resp.ec == boost::asio::error::operation_aborted )
            {
                slog.WarningLog("TIMEOUT OR aborted: %s ", resp.ec.message().c_str());
                ec = Error_perform_in_progress; /**** REVERSE MUST BE ONLY RUCHNOY **/
            }
            else
            {
                slog.WarningLog("operation failed");
                ec = Error_merchant_operation;
            }
        }
        return handler(trans, response, ec) ;
     } ;
     /************************/
     mng.async_pay_atm( req, webmoney_handler ) ;
}
/************************************************ QIWI ************************************************************/
void oson::Merchant_api_manager::perform_status_qiwi(const Merch_trans_T& trans, perform_status_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    Merch_trans_status_T status;
    
    if ( ! trans.merch_api_params.count("account" ) ) {
        slog.ErrorLog("'account' parameter not found!") ;
        return handler(trans, status, Error_parameters ) ;
    }
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.terminal_id = trans.acc.login    ;
    qiwi_acc.password    = trans.acc.password ;
    qiwi_acc.url         = trans.merchant.url ;
    
    
    qiwi::request_t qiwi_req;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    qiwi_req.trn_id  = trans.transaction_id ;
    
    auto qiwi_handler = [status, trans, handler]( const qiwi::request_t &, const qiwi::response_t & qiwi_resp )
    {
        SCOPE_LOGD(slog);
        
        slog.DebugLog("trn-id: %ld", trans.transaction_id ) ;
        
        Merch_trans_status_T status_resp = status;
        
        status_resp.merchant_status = qiwi_resp.result_code ;
        status_resp.merch_rsp       = qiwi_resp.status_text ;
        
        
        if ( qiwi_resp.final_status ) {
            //a finished 
            if ( ! qiwi_resp.fatal_error && qiwi_resp.success() ){
                //success finished
                slog.DebugLog("qiwi purchase success finished!");
                return handler(trans, status_resp, Error_OK ) ;
            }
            
            slog.WarningLog("qiwi purchase failure finished!") ;
            //failed finished
            return handler(trans, status_resp, Error_merchant_operation ) ;
        } else { // not final status, so in progress 
            slog.DebugLog("qiwi purchase in progress");
            return handler(trans, status_resp, Error_perform_in_progress ) ;
        }
    };
    
    qiwi::manager_t  qiwi_manager(qiwi_acc);
    
    qiwi_manager.async_status(qiwi_req, qiwi_handler ) ;
    
}

void oson::Merchant_api_manager::purchase_info_qiwi(const Merch_trans_T& trans, purchase_info_handler handler)
{
    SCOPE_LOGD(slog);
    Merch_trans_status_T response;
    
    if ( ! trans.merch_api_params.count("account") ) 
    {
        slog.ErrorLog("'account' parameter not found!");
        return handler( trans, response, Error_parameters ) ;
    }
    
    const int iso_code =  string2num( trans.merchant.extern_service ) ;
    
    slog.InfoLog("iso_code: %d", iso_code);
#if 0
    const auto ci_type = (iso_code == 643 /*RUB*/ || iso_code == 978 /*EURO*/ ) 
                         ? Currency_info_T::Type_Rus_CB 
                         : Currency_info_T::Type_Uzb_CB ; 
#endif 
    const auto ci_type = Currency_info_T::Type_Uzb_CB ;
    
    /*@Note:  There for RUB and EUR  used Russian CB currency,  and for USD  used Uzbekistan CB currency. */
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return handler(trans, response, Error_internal);
    }
    
    
    const int64_t amount = trans.amount ;
    const int64_t commission = trans.merchant.commission(trans.amount);
    const int64_t remain_amount = amount - commission ;

    std::string ccy = "x" ;
    double cb_currency = 0.0;
    double amount_credit = 0;
    switch(iso_code)
    {
        case 643 : 
            amount_credit = ci.rub(remain_amount); 
            ccy = "RUB" ;
            cb_currency = ci.usd_rub ;
            break;
        case 840:
            amount_credit = ci.usd(remain_amount);
            ccy = "USD";
            cb_currency  = ci.usd_uzs ;
            break;
        case 978:
            amount_credit = ci.eur(remain_amount );
            ccy = "EUR";
            cb_currency = ci.usd_eur ;
            break;
        default: break;
    }
    
    slog.DebugLog("ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    if (amount_credit < 1.0E-2 ){
        slog.WarningLog("amount_credit is zero! try to use 0.01 value.");
        amount_credit = 0.01;
    }
    
    response.kv["oson_amount_credit"] = to_str(amount_credit, 2, false );
    response.kv["currency"] = to_str(cb_currency, 2, true);
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password    = trans.acc.password ;
    qiwi_acc.terminal_id = trans.acc.login    ;
    qiwi_acc.url         = trans.merchant.url     ;
    
    qiwi::request_t qiwi_req ;
    qiwi_req.ccy     = ccy   ;
    //qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
   // qiwi_req.amount  = ci.rub( trans.amount ) ;
    
    
    
    //qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    //qiwi_manager.check(qiwi_req, qiwi_resp ) ;
    
    auto qiwi_handler = [trans, response, handler](const qiwi::request_t& qiwi_req, const qiwi::response_t& qiwi_resp)mutable
    {
        if ( qiwi_resp.success() && qiwi_resp.status_value == 1 )
        {
            response.kv["login"] = qiwi_req.account;
            return handler( trans, response, Error_OK) ;
        }

        return handler( trans, response, Error_purchase_login_not_found );
    };
    
    qiwi_manager.async_check( qiwi_req, qiwi_handler ) ;
    
}

void oson::Merchant_api_manager::check_status_qiwi(const Merch_trans_T& trans, check_status_handler handler) 
{
    SCOPE_LOGD(slog);
    
    Merch_check_status_T status;
    
    if ( ! trans.merch_api_params.count("account") ) {
        slog.ErrorLog("'account' parameter not found!");
        return handler(trans, status, Error_parameters ) ;
    }
    
    const int iso_code = string2num( trans.merchant.extern_service ) ;
    slog.InfoLog("iso_code: %d", iso_code);
    std::string ccy = "x";
    switch(iso_code)
    {
        case 643: 
            ccy = "RUB" ;
            break;
        case 840:
          //  amount_credit = ci.usd(remain);
            ccy = "USD";
            break;
        case 978:
           // amount_credit = ci.usd(remain_amount);
            ccy = "EUR";
            break;
        default:
            break;
    }
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password    = trans.acc.password ;
    qiwi_acc.terminal_id = trans.acc.login        ;
    qiwi_acc.url         = trans.merchant.url     ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.ccy     = ccy ;
    //qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    //qiwi_req.amount  = ci.rub( trans.amount ) ;
    
    
    
    qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    //qiwi_manager.check(qiwi_req, qiwi_resp ) ;
    auto qiwi_handler = [status, trans, handler]( const qiwi::request_t&, const qiwi::response_t & qiwi_resp)mutable
    {
        if ( qiwi_resp.success() && qiwi_resp.status_value == 1 )
        {
            status.exist = true ;
            return handler(trans, status, Error_OK) ;
        }


        status.exist = false;
        status.status_value = qiwi_resp.status_value ;
        status.status_text  = "Номер не существует" ;
        if (qiwi_resp.ec == boost::asio::error::timed_out || qiwi_resp.ec == boost::asio::error::operation_aborted ) 
        {
            status.status_text = "TIMEOUT";
            return handler(trans, status, Error_timeout) ;
        }
        return handler( trans, status, Error_purchase_login_not_found );
    };
    
    qiwi_manager.async_check(qiwi_req, qiwi_handler);
}

void oson::Merchant_api_manager::perform_purchase_qiwi(const Merch_trans_T& trans, perform_purchase_handler handler)
{
    SCOPE_LOGD(slog);
       
    Merch_trans_status_T response;
    
    if ( ! trans.merch_api_params.count("account") ) 
    {
        slog.ErrorLog("'account' parameter not found!");
        return handler(trans, response, Error_parameters ) ;
    }
    
    const int iso_code = string2num( trans.merchant.extern_service ) ;
    const auto ci_type = Currency_info_T::Type_Uzb_CB ; // always use uzb CB course.
    
    //@Note: There need Russian Central Bank  Currency for RUB and EUR.  FOR USD - used UZbekistan Central Bank Currency.
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return handler(trans, response, Error_internal) ;
    }
    
    
    std::string ccy             = "x";
    const int64_t amount        = trans.amount;
    const int64_t commission    = 0;//@Note commission already subtracted. m_merchant.commission(amount);
    const int64_t remain_amount = amount - commission;
    double amount_credit= 0;
    switch(iso_code)
    {
        case 643:
            ccy = "RUB";
            amount_credit = ci.rub( remain_amount );
            break;
        case 840:
            ccy = "USD";
            amount_credit = ci.usd(remain_amount);
            break;
        case 978:
            ccy = "EUR";
            amount_credit = ci.eur(remain_amount);
            break;
        default:
            slog.WarningLog("Unexpected iso_code: %d", iso_code);
            return handler(trans, response, Error_parameters ) ;
            //break;
    }
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    slog.DebugLog(" iso-code: %d ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", 
            iso_code, ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password    = trans.acc.password ;
    qiwi_acc.terminal_id = trans.acc.login    ;
    qiwi_acc.url         = trans.merchant.url ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.ccy     = ccy ;
    qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    qiwi_req.amount  = amount_credit;
    
    
    
    //qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    //qiwi_manager.pay(qiwi_req, qiwi_resp ) ;
    
    auto qiwi_handler = [trans, response, handler]( const qiwi::request_t& qiwi_req, const qiwi::response_t & qiwi_resp)mutable
    {
        SCOPE_LOGD( slog );
        response.ts               = qiwi_resp.txn_date ;
        response.merchant_status  = qiwi_resp.result_code ;
        response.merchant_trn_id  = qiwi_resp.txn_id ;
        response.merch_rsp        = qiwi_resp.status_text ;

        if (response.merchant_trn_id.empty() ) 
        {
            response.merchant_trn_id = "0";
        }

        slog.DebugLog("qiwi_resp:{ result-code: %ld, txn-id: %s, status_text: %s, fatal-error: %d, final-status: %d }", 
                qiwi_resp.result_code, 
                qiwi_resp.txn_id.c_str(), 
                qiwi_resp.status_text.c_str(),
                (int)qiwi_resp.fatal_error,
                (int)qiwi_resp.final_status
        ) ;

        Error_T ec = Error_OK ;
        if ( qiwi_resp.success() && qiwi_resp.fatal_error == false && qiwi_resp.final_status == true ) 
        {
            slog.DebugLog(" Error-OK ") ;
            ec =  Error_OK ;
        } 
        else if ( qiwi_resp.final_status == false ) {
            slog.WarningLog(" Error-perform-in-progress ") ;
            ec =  Error_perform_in_progress ;
        } 
        else // error and this is final status
        {
            slog.WarningLog(" Error-merchant-operation " ) ;
            ec =  Error_merchant_operation ;
        }


        return handler(trans, response, ec ) ;
    } ; // end qiwi handler
    
    qiwi_manager.async_pay( qiwi_req, qiwi_handler ) ;
}
/////////////////////////////////////////////////////////////
void oson::Merchant_api_manager::perform_status_hg(const Merch_trans_T& trans, perform_status_handler handler ) 
{
    SCOPE_LOG(slog);
    
    std::string account = trans.merch_api_params.count("account") ? trans.merch_api_params.at("account") : "" ;
    double amount_credit =  1.00;
    auto ci = Merchant_api_T::get_currency_now_cb(Currency_info_T::Type_Uzb_CB);
    if (ci.initialized){
        amount_credit =  trans.amount - trans.merchant.commission(trans.amount)  ;
        amount_credit = ci.usd( amount_credit ) ;
    }
    
    std::string ccy = "USD" ;
    
    namespace hg = oson::backend::merchant::HermesGarant;
    
    hg::acc_t acc ; 
    acc.agent_id        = "21";
    acc.agent_password  = "OsonUZ76";
    acc.url             = "https://hgg.kz:8802" ;
    
    hg::request_t req;
    
    req.account     = account              ;
    req.amount      = amount_credit        ;
    req.currency    = ccy                  ;
    req.service_id  = trans.service_id     ;
    req.trn_id      = trans.transaction_id ;
    req.ts          = trans.ts             ;
    
    hg::manager_t mgr( acc );
    
    hg::handler_t h  = [handler, trans](const struct hg::request_t & req, const struct hg::response_t& resp)
    {
        Merch_trans_status_T status_resp ;
        
        if ( resp.err_value != Error_OK )
        {
            status_resp.merch_rsp = "Error on connection";
            return handler(trans, status_resp, resp.err_value);
        }
        
        if (hg::error_codes::is_final(resp.resp_status)) 
        {
            status_resp.merchant_status = resp.resp_status;
            status_resp.merch_rsp = resp.message;
            bool const success = resp.resp_status == hg::error_codes::success;
            
            Error_T ec = success ? Error_OK : Error_merchant_operation;
            
            return handler(trans, status_resp, ec);
        } else {
            return handler(trans, status_resp, Error_perform_in_progress);
        }
    };
    
    mgr.async_pay(req, h );
    
}
    

///////////////////////////////////////////////////////////////////////////////////////
Merchant_api_T::Merchant_api_T( const Merchant_info_T & merchant, const Merch_acc_T & acc) 
  :  m_acc(acc), m_merchant(merchant)
{
}

Merchant_api_T::~Merchant_api_T()
{

}

Error_T Merchant_api_T::cancel_pay(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    switch( (merchant_api_id) m_merchant.api_id )
    {
        case merchant_api_id::ums : return cancel_ums(trans, response);
        default:
            slog.WarningLog("Non-supported operation!");
            return Error_internal;
    }
}
    
Error_T Merchant_api_T::perform_purchase( const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    
    typedef merchant_identifiers idents;
    
    if (  m_merchant.api_id == merchant_api_id::nonbilling ) 
    {
        slog.DebugLog( "merchant-id: %d", m_merchant.id );
        response.merchant_status = 0;
        response.merchant_trn_id = "0";
        return Error_OK;
    }
    
    typedef merchant_identifiers idents;
    
    if (  m_merchant.api_id == merchant_api_id::mplat )
    {
        return this->perform_mplat_merchants(trans, response);
    }
   
    if ( m_merchant. api_id == merchant_api_id::qiwi  )
    {
        return perform_qiwi(trans, response) ;
    }
    
    if ( idents::is_munis( m_merchant.id ) )
    {
        return this->perform_munis_merchants(trans, response);
    }
   
    if ( idents::is_money_mover(m_merchant.id) ) 
    {
        return perform_money_mover(trans, response);
    }
    
    if ( idents::is_webmoney(m_merchant.id) ) 
    {
        return perform_webmoney(trans, response);
    }
    
    if ( idents::is_nativepay(m_merchant.id ) ) {
        return perform_nativepay(trans, response ) ;
    }
    
    const bool isscript_plugin =  m_merchant.url.find("/etc/") != std::string::npos ; 
    
    
    if (m_merchant.id == idents::Comnet_I) // Comnet
    {
         return this->perform_comnet(trans, response);
    }
    
    if (m_merchant.id ==  idents::Sharq_Telecom  /*|| m_merchant.id == merchant_identifiers::Simus_I*/) // Sharq-telecom. Simus-does not work.
    {
        return this->perform_sharq_telecom(trans, response);
    }
    
    if (m_merchant.id ==  idents::Uzinfocom )
    { 
          return perform_uzinfocom_http(trans, response);
    }
    
    if (m_merchant.id == idents::Beeline  || m_merchant.id == idents::Beeline_Internet_test )
    {
        return this->perform_beeline(trans, response);
    }
    
    if (m_merchant.id == idents::Ucell_direct )
    {
        return perform_ucell(trans, response);
    }
    
    if ( m_merchant.id == idents::UzMobile_CDMA && !isscript_plugin ) 
    {
        return perform_uzmobile_CDMA(trans, response);
    }
    
    if ( m_merchant.id == idents::UzMobile_GSM && !isscript_plugin)
    {
        return perform_uzmobile_GSM(trans, response);
    }
    
    
    if (m_merchant.id == idents::Sarkor_Telecom  || 
        m_merchant.id == idents::Sarkor_TV       || 
        m_merchant.id == idents::Sarkor_HastimUz || 
        m_merchant.id == idents::Sarkor_IPCAM )
    {
        return perform_sarkor_telecom(trans, response);
    }
    
    if (m_merchant.id == idents::Cron_Telecom)
    {
        return perform_cron_telecom(trans, response);
    }
    
    
    if (m_merchant.id == idents::Kafolat_insurance)
    {
        return perform_kafolat_insurance(trans, response);
    }
    
    //TEST PAYNET ALL 
    if (isscript_plugin  ){
        return perform_paynet_api(trans, response);
    }

    //put it after paynet 
    if ( /*m_merchant.id == idents::Nano_Telecom ||*/ m_merchant.id == idents::Nano_Telecom_direct){
        return perform_nanotelecom(trans, response);
    }


    if ( m_merchant.id == idents::TPS_I_direct ||
         m_merchant.id == idents::East_Telecom_direct)
    {
        return perform_tps( trans, response );
    }

    if ( m_merchant.id == idents::Uzmobile_CDMA_new_api ||
         m_merchant.id == idents::Uzmobile_GSM_new_api ) 
    {
        return perform_uzmobile_new(trans, response ) ;
    }
    
    if (m_merchant.id == idents::UMS_direct)
    {
        return perform_ums(trans, response ) ;
    }

    if (  m_merchant.api_id ==  merchant_api_id::hermes_garant  )
    {
        return perform_hg(trans, response);
    }
    
    ////////////////////////////////////////////////
    {
        return perform_oson( trans, response ); 
    }
    //////////////////////////////////////////////
}


Error_T Merchant_api_T::check_status(const Merch_trans_T & trans, Merch_check_status_T & status)
{
    SCOPE_LOG(slog);
    
    typedef merchant_identifiers idents;
    
    if  (  m_merchant.api_id == merchant_api_id::nonbilling )
    {
        status.exist = true;
        return Error_OK;
    }
    
    typedef merchant_identifiers idents;
    
    if (  m_merchant.api_id == merchant_api_id::mplat  )
    {
        return this->check_mplat_merchants(trans, status);
    }
    
    if ( m_merchant. api_id == merchant_api_id::qiwi   ) {
        return check_qiwi(trans, status ) ;
    }
    
    if ( idents::is_munis( m_merchant.id )  )
    {
        return check_munis_merchants(trans, status);
    }
    
    if (idents::is_money_mover(m_merchant.id) ) 
    {
        return check_money_mover(trans, status);
    }
    
    if (idents::is_webmoney(m_merchant.id) ) 
    {
        return check_webmoney(trans, status);
    }
    
    if (idents::is_nativepay(m_merchant.id) ) 
    {
        return check_nativepay(trans, status ) ;
    }
    
    if (m_merchant.id == idents::Beeline || m_merchant.id == idents::Beeline_Internet_test )
    {
        return this->check_beeline(trans, status);
    }

    if (m_merchant.id == idents::Ucell_direct ) 
    {
        return check_ucell(trans, status);
    }
    
    if (m_merchant.id  == idents::Kafolat_insurance)
    {
        return check_kafolat_insurance(trans, status);
    }
    
    //@Note: test UMS direct check.
    if (m_merchant.id == idents::UMS_direct   )
    {
        return check_ums(trans, status ) ;
    }
    
    const bool ispaynet = m_merchant.url.find("/etc/") != std::string::npos;
    if (ispaynet)
    {
        return check_paynet_api(trans, status);
    }
    
    if (m_merchant.id == idents::Comnet_I) //38) // comnet
    {
        return check_comnet(trans, status);
    }
    
    if (m_merchant.id ==  idents::Sharq_Telecom || m_merchant.id == idents::Simus_I) //98 || m_merchant.id == 36) // sharq telecom or Simus
    {
        return check_sharq_telecom(trans, status);
    }
    
    if (m_merchant.id == idents::Uzinfocom ) //147) // uzinfocom
    {
        return check_uzinfocom(trans, status);
    }
    
    if (m_merchant.id == idents::UzMobile_CDMA) //19) // uzmobile CDMA
    {
        return check_uzmobile_CDMA(trans, status);
    }
    
    if (m_merchant.id == idents::UzMobile_GSM )
    {
        return check_uzmobile_GSM(trans, status);
    }
    
    
    
    if (m_merchant.id == idents::Cron_Telecom)
    {
        return check_cron_telecom(trans, status);
    }
    
     if (m_merchant.id == idents::Sarkor_Telecom  || 
        m_merchant.id == idents::Sarkor_TV       || 
        m_merchant.id == idents::Sarkor_HastimUz || 
        m_merchant.id == idents::Sarkor_IPCAM )
    {
        return check_sarkor_telecom(trans, status);
    }
    
    if ( /*m_merchant.id == idents::Nano_Telecom ||*/ m_merchant.id == idents::Nano_Telecom_direct)
    {
        return check_nanotelecom(trans, status);
    }

    if (m_merchant.id == idents::TPS_I_direct ||
        m_merchant.id == idents::East_Telecom_direct)
    {
        return check_tps(trans, status);
    }

    if (m_merchant.id == idents::Uzmobile_CDMA_new_api || m_merchant.id == idents::Uzmobile_GSM_new_api )
    {
        return check_uzmobile_new(trans, status);
    }

    if ( m_merchant.api_id ==  merchant_api_id::hermes_garant  ) 
    {
        return check_hg(trans, status);
    }
    
    ////////////////////////
    return check_oson(trans, status);
    //////////////////////
}

Error_T Merchant_api_T::query_new(const std::string &field_name, const std::string &value, std::map<std::string, std::string> &list)
{
    SCOPE_LOGD(slog);

    namespace paynet = oson::backend::merchant::paynet;
    paynet::request_t req;
    req.param_ext[field_name] = value;  

    paynet::access_t acc;
    acc.merchant_id  = m_merchant.merchantId;
    acc.password     = m_acc.password ; // "NvIR4766a";
    acc.terminal_id  = m_acc.options  ; // "4119362";
    acc.url          = m_acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = m_acc.login    ; // "akb442447";

    paynet::manager api(acc);

    paynet::param_t param;

    api.get_param(req, param);
    
    list.swap( param.pm );
    
    return Error_OK ;
}

Error_T Merchant_api_T::get_info(const Merch_trans_T &trans, Merch_trans_status_T &response)
{
    SCOPE_LOGD(slog);
    typedef merchant_identifiers idents;
    
    if (idents::is_munis(m_merchant.id))
    {
        return get_munis_info(trans, response);
    }
    
    if ( m_merchant.api_id == merchant_api_id::mplat )
    {
        return get_mplat_info(trans, response);
    }
    
    if (   m_merchant.api_id  == merchant_api_id::qiwi  ) {
        return get_qiwi_info(trans, response ) ;
    }
    //bool isscript_plugin = m_merchant.url.find("/etc") != std::string::npos;
    if (( m_merchant.id == idents::Electro_Energy || m_merchant.id == idents::Musor ) )
    {
        //paynet
        return get_info_paynet(trans, response);
    }
    
    if(idents::is_webmoney(m_merchant.id))
    {
        return get_webmoney_info(trans, response);
    }
    
    if (idents::is_nativepay(m_merchant.id) ) 
    {
        return get_nativepay_info(trans, response);
    }
    
    if (m_merchant.id == idents::Ucell_direct  ) 
    {
        return get_ucell_info(trans, response);
    }
    
    if (m_merchant.id == idents::TPS_I_direct ||
        m_merchant.id == idents::East_Telecom_direct)
    {
        return this->get_tps_info(trans, response);
    }
    
    if (m_merchant.id == idents::Uzmobile_CDMA_new_api ||
        m_merchant.id == idents::Uzmobile_GSM_new_api )
    {
        return get_info_uzmobile_new(trans, response ) ;
    }
    
    if (m_merchant.id == idents::UMS_direct)
    {
        return get_info_ums(trans, response);
    }
    
    if ( m_merchant.api_id ==  merchant_api_id::hermes_garant ) 
    {
        return get_info_hg(trans, response);
    }
    
    if (m_merchant.api_id == merchant_api_id::beeline ||
        m_merchant.api_id == merchant_api_id::uzmobile_old ||
        m_merchant.id     == idents::Perfectum
      )
    {
        response.merchant_status = 0;
        response.merchant_trn_id =  "0";
        response.kv["login"] = prettify_phone_number_uz( trans.param ) ;
        return Error_OK ;
    }
    
    {
        // general case
        response.merchant_status = 0;
        response.merchant_trn_id =  "0";
        response.kv["login"] = trans.param;
        return Error_OK ;
    }

}

Error_T Merchant_api_T::get_balance(const Merch_trans_T& trans,  Merch_trans_status_T& status) 
{
    SCOPE_LOGD(slog);
    if (  m_merchant.api_id == merchant_api_id::qiwi ) {
        return get_qiwi_balance( trans, status ) ;
    }
    else if (m_merchant.api_id == merchant_api_id::hermes_garant){
        return get_hg_balance(trans, status);
    }
    
    
    else 
    {
        slog.WarningLog("Unsupported merchant-id: %d", m_merchant.id) ;
        return Error_operation_not_allowed ;
    
    }
    
}

Error_T Merchant_api_T::pay_status(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    
    if (m_merchant.api_id == merchant_api_id::ums  ) 
    {
        return pay_status_ums(trans, response); 
    } else {
        slog.WarningLog("Not implemented");
        
        return Error_OK ;
    }
}

Error_T Merchant_api_T::make_detail_info(const std::string& json_text, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    typedef merchant_identifiers idents;
    
    if (json_text.empty() ) {
        slog.WarningLog("empty json_text");
        return Error_OK;
    }
    
    //@Note we support only Munis GUBDD AND HotWater yet!
    if ( idents::is_munis(   m_merchant.id ) )
    {
        
        typedef boost::property_tree::ptree ptree;
        std::istringstream stream(json_text);
        
        ptree json_response;
        boost::property_tree::read_json(stream, json_response);
        
        ptree nil;
        ptree json_data = json_response.get_child("data", nil);
        ptree json_keys = json_data.get_child("keys", nil);
        ptree json_vals = json_data.get_child("values", nil);
        
        std::map< std::string, std::string> kv;
        
        for( ptree::const_iterator it_key = json_keys.begin(), it_val = json_vals.begin()   ;  
             it_key != json_keys.end() && it_val != json_vals.end(); 
             ++it_key, ++it_val
                )
        {
            ptree::value_type const& k = (*it_key);
            ptree::value_type const& v = (*it_val);
            
            kv.insert( std::make_pair( k.second.data(), v.second.data() ) ) ;
        }

        response.kv.swap( kv );
    }
    else //if ( idents::is_mplat( m_merchant.id ) ) 
    {
        typedef boost::property_tree::ptree ptree;
        std::istringstream stream(json_text);
        
        ptree json_response;
        boost::property_tree::read_json(stream, json_response);

        std::map< std::string, std::string> kv;
        
        for(ptree::const_iterator it = json_response.begin(), it_e = json_response.end(); it != it_e; ++it)
        {
            ptree::value_type const& e = (*it); //  std::pair< std::string, ptree>
            
            kv.insert( std::make_pair( e.first, e.second.data() ) ) ;
        }
        
        response.kv.swap( kv );
    }
    return Error_OK ;
}

Currency_info_T  Merchant_api_T::get_currency_now()
{
    SCOPE_LOGD(slog);
    Currency_info_T currency;
    
    std::string address = "http://apilayer.net/api/live?access_key=c9121f0ce0f879b77bc1da06939b2ed7&currencies=uzs,rub,eur" ;
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    req_.method= "GET" ;
    
//    {
//      "success": true,
//      "terms": "https://currencylayer.com/terms",
//      "privacy": "https://currencylayer.com/privacy",
//      "timestamp": 1522405449,
//      "source": "USD",
//      "quotes": {
//        "USDUZS": 8120.000335,
//        "USDRUB": 57.393002,
//        "USDEUR": 0.811031
//      }
//    }
    slog.DebugLog("REQUEST: %s", address.c_str());
    std::string resp_str = sync_http_request(req_);
    slog.DebugLog("RESPONSE: %s", resp_str.c_str());
    
    if (resp_str.empty()){
        return currency;
    }
    
    namespace pt = boost::property_tree;

    try
    {
        std::stringstream ss(resp_str);
        pt::ptree root;
        
        pt::read_json(ss, root);
        
        currency.usd_uzs = root.get< double > ("quotes.USDUZS", 0.0);
        currency.usd_rub = root.get< double > ("quotes.USDRUB", 0.0);
        currency.usd_eur = root.get< double > ("quotes.USDEUR", 0.0);
        
        if (  currency.usd_uzs  < 1.0E-6 ||
              currency.usd_rub  < 1.0E-6 ||
              currency.usd_eur  < 1.0E-6 )
        {
            slog.WarningLog("currencies are wrong!");
            return currency;
        }
        
        //convert they to sum
        currency.usd_rub = currency.usd_uzs / currency.usd_rub ;
        currency.usd_eur = currency.usd_uzs / currency.usd_eur ;
        
        slog.DebugLog("USD: %.12f, RUB: %.12f, EUR: %.12f", currency.usd_uzs, currency.usd_rub, currency.usd_eur ) ;
        
        //all right
        currency.initialized = true;
    }
    catch(std::exception & e)
    {
        slog.ErrorLog("exception: %s", e.what());
    }
    
    return currency;
}

    
Currency_info_T  Merchant_api_T::get_currency_now_cb(int type)
{
    SCOPE_LOG(slog);
    Currency_info_T currency;
    
    switch(type)
    {
        case Currency_info_T::Type_Uzb_CB:  
            currency = get_cb_uzb() ; 
            //@Note: sometimes json based API does not work, try to a xml variant.
            if ( ! currency.initialized ) {
                currency =  get_cb_uzb_xml();
            }
            break;
        case Currency_info_T::Type_Rus_CB:  currency =  get_cb_rus() ; break;
        default: currency = Currency_info_T(); break;//an empty
    }
    
    return currency;
}

    
Currency_info_T  Merchant_api_T::get_cb_rus()
{
    SCOPE_LOGD(slog);
    Currency_info_T currency;
    
    currency.type = Currency_info_T::Type_Rus_CB ;
    
    std::string address = "http://www.cbr-xml-daily.ru/daily_utf8.xml" ;
    
    auto http_req = oson::network::http::parse_url(address);
    http_req.method = "GET" ;
    
    slog.DebugLog("REQUEST: %s", address.c_str());
    std::string xml_resp = sync_http_request(http_req );
    slog.DebugLog("RESPONSE: %s", xml_resp.c_str());
    if (xml_resp.empty() ) {
        slog.WarningLog("Can't take cb curse from russian center bank!");
        return currency;
    }
    
    namespace pt = boost::property_tree ;
    try
    {
        std::stringstream ss(xml_resp);
        pt::ptree root;
        
        pt::read_xml(ss, root);
        
        const pt::ptree& valcurs = root.get_child("ValCurs");
        
        for(const pt::ptree::value_type & vals : valcurs)
        {
            std::string const& name = vals.first ;
            
            slog.InfoLog("name: %s", name.c_str());
            
            if (name != "Valute") continue;
            
            int numcode = vals.second.get< int > ("NumCode");
            
            double ru_cb = 0;
            
            {
                double nominal = vals.second.get< double > ("Nominal");
                
                std::string rate_s    = vals.second.get< std::string > ("Value");
                
                // replace ','  to '.'
                std::replace( rate_s.begin(), rate_s.end(), ',', '.' ) ;
                
                double rate = 0;
                sscanf(rate_s.c_str(), "%lf", &rate ) ;
                
                if ( fabs(rate)> 1.0E-7 )
                    ru_cb = nominal / rate ;
            }
            
            if (numcode == 860 ) {// 860 - ISO UZS  code
                currency.usd_rub = ru_cb ;
            } else if (numcode == 840 ) {  // 840 - ISO USD code
                currency.usd_uzs = ru_cb ;
            } else if (numcode == 978 ) {
                currency.usd_eur = ru_cb ;
            }
            
        }
        
        
    }catch(std::exception& e ){
        slog.WarningLog("Exception: %s", e.what());
        return currency;
    }
    
    
    slog.DebugLog("RUSSIAN CB (relative RUB) : USD: %.12f, UZS: %.12f, EUR: %.12f", currency.usd_uzs, currency.usd_rub, currency.usd_eur ) ;
        
    if (  currency.usd_uzs  < 1.0E-6 ||
          currency.usd_rub  < 1.0E-6 ||
          currency.usd_eur  < 1.0E-6 )
    {
        slog.WarningLog("currencies are wrong!");
        return currency;
    }

    // make it   as initialized.
    currency.initialized = true;
    
    // 1 rubl -- usd_rub sum OK     1 dollar --> x sum,  1 dollar = 1/ usd_uzs rubl
    // 1 rubl -- usd_uzs  dollar -->   
    currency.usd_uzs = currency.usd_rub / currency.usd_uzs ;
    currency.usd_eur = currency.usd_rub / currency.usd_eur ;
    
    slog.DebugLog("RUSSIAN CB(relative UZS): USD: %.12f, UZS: %.12f, EUR: %.12f", currency.usd_uzs, currency.usd_rub, currency.usd_eur ) ;
    
    return currency;
}

Currency_info_T Merchant_api_T::get_cb_uzb_xml()
{
    SCOPE_LOG(slog);
    Currency_info_T currency;
    currency.type = Currency_info_T::Type_Uzb_CB;
    std::string address = "http://cbu.uz/uzc/arkhiv-kursov-valyut/xml/";
    oson::network::http::request http_req = oson::network::http::parse_url(address);
    http_req.method = "GET";
    
    slog.DebugLog("REQUEST: %s", address.c_str());
    std::string resp_xml = sync_http_request(http_req);
    slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());
    
    if (resp_xml.empty()){
        return currency;
    }
    
    
//    <CBU_Curr name="CBU Currency XML by ISO 4217">
//     <CcyNtry ID="840">
//        <Ccy>USD</Ccy>
//        <CcyNm_RU>Доллар США</CcyNm_RU>
//        <CcyNm_UZ>AQSH dollari</CcyNm_UZ>
//        <CcyNm_UZC>АҚШ доллари</CcyNm_UZC>
//        <CcyNm_EN>US Dollar</CcyNm_EN>
//        <CcyMnrUnts>2</CcyMnrUnts>
//        <Nominal>1</Nominal>
//        <Rate>7783.05</Rate>
//        <date>07.08.2018</date>
//    </CcyNtry>
//            
    
    namespace pt = boost::property_tree;
    try
    {
        
        std::stringstream ss(resp_xml);
        pt::ptree root;
        pt::read_xml(ss, root);
        
        const pt::ptree& cbu_curr = root.get_child("CBU_Curr");
        for(const pt::ptree::value_type& ccy : cbu_curr)
        {
            const std::string& name = ccy.first;
            if ( boost::algorithm::iequals( name ,"CcyNtry" ) == false ) 
                continue;
            
            const pt::ptree & ccyNtry = ccy.second;
            const int64_t code = ccyNtry.get<  int64_t >("<xmlattr>.ID", 0);
            
            if ( ! ( code == 840 || code == 643 || code == 978 ) )
                continue;
            
            double nominal = ccyNtry.get< double >("Nominal", 1.0);
            double rate    = ccyNtry.get< double > ("Rate", 0);
            double course = rate / nominal;
            
            switch(code)
            {
                case 840:  // a dollar
                {
                    currency.usd_uzs = course;
                }
                break;
                case 643: // russian ruble
                {
                    currency.usd_rub = course;
                }
                break;
                case 978: // euro
                {
                    currency.usd_eur = course;
                }
                break;
            }
        }
        
        slog.DebugLog("USD: %.12f, RUB: %.12f, EUR: %.12f", currency.usd_uzs, currency.usd_rub, currency.usd_eur ) ;
        
        if (  currency.usd_uzs  < 1.0E-6 ||
              currency.usd_rub  < 1.0E-6 ||
              currency.usd_eur  < 1.0E-6 )
        {
            slog.WarningLog("currencies are wrong!");
            return currency;
        }
        
        //all right
        currency.initialized = true;
        
    }catch(pt::ptree_error& e )
    {
        slog.ErrorLog("ptree-error: %s", e.what());
    } catch(std::exception& e )
    {
        slog.ErrorLog("std-exception: %s", e.what());
    }
    
    return currency;
}

Currency_info_T  Merchant_api_T::get_cb_uzb()
{
    SCOPE_LOGD(slog);
    
    Currency_info_T currency;
    
    currency.type = Currency_info_T::Type_Uzb_CB ;
    
    std::string address = "http://cbu.uz/uzc/arkhiv-kursov-valyut/json/" ;
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    req_.method= "GET" ;
    
    slog.DebugLog("REQUEST: %s", address.c_str());
    std::string resp_str = sync_http_request(req_);
    slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(128, resp_str.length()), resp_str.c_str());
    
    if (resp_str.empty()){
        return currency;
    }
    
    namespace pt = boost::property_tree;

    try
    {
        std::stringstream ss(resp_str);
        pt::ptree root;
        
        pt::read_json(ss, root);
        
        
        
        for( const pt::ptree::value_type& val : root)
        {
            const pt::ptree & obj = val.second ;
            std::string ccy = obj.get< std::string >("Ccy", "-");

            if (ccy == "USD"){
                currency.usd_uzs = obj.get< double >( "Rate", 0.0 );
            } else if (ccy == "RUB" ) {
                currency.usd_rub = obj.get< double >( "Rate", 0.0 );
            } else if (ccy == "EUR") {
                currency.usd_eur = obj.get< double >( "Rate", 0.0 );
            }

        }
        
        slog.DebugLog("USD: %.12f, RUB: %.12f, EUR: %.12f", currency.usd_uzs, currency.usd_rub, currency.usd_eur ) ;
        
        if (  currency.usd_uzs  < 1.0E-6 ||
              currency.usd_rub  < 1.0E-6 ||
              currency.usd_eur  < 1.0E-6 )
        {
            slog.WarningLog("currencies are wrong!");
            return currency;
        }
        
//        // 1 rub - currency.usd_rub  sum
//        // 1 dol - currency.usd_uzs  sum - 
//        // 1 dol - x rubl
//        // x = currency.usd_uzs / currency.usd_rub
//        currency.usd_rub = currency.usd_uzs / currency.usd_rub;
//        currency.usd_eur = currency.usd_uzs / currency.usd_eur; 
        
        //all right
        currency.initialized = true;
    }
    catch(std::exception & e)
    {
        slog.ErrorLog("exception: %s", e.what());
    }
    
    return currency;
    
}

static std::string map_to_json_string(const std::map< std::string , std::string> & obj)
{
    std::string result ;
    char const quot = '\"';
    bool first_el = true;
    result += "{\n" ;

    for(std::map< std::string, std::string>::const_iterator it = obj.begin(), it_e = obj.end();  it != it_e; ++it)
    {
        std::string const& key = (*it).first;
        std::string const& val = (*it).second;

        if ( ! first_el )
            result += ", \n";

        first_el = false;

        result += quot;
        result += key;
        result += quot;

        result += " : ";


        result += quot;
        result += val;
        result += quot;
    }
    result += "\n}";
    
    return result;
}

Error_T Merchant_api_T::get_info_paynet(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    //////////////////////////////////////////////////////////////////////////////////////////
    namespace paynet = oson::backend::merchant::paynet;
    paynet::request_t req;
    req.act = 0; // needn't
    req.amount = trans.amount/100;
   // req.client_login;//needn't
   // req.client_login_param_name;//needn't
    req.fraud_id = trans.user_phone;
    req.param_ext = trans.merch_api_params;
    req.service_id = string2num( trans.service_id) ;
    req.transaction_id = trans.transaction_id;

    paynet::access_t acc;
    acc.merchant_id  = m_merchant.merchantId;
    acc.password     = m_acc.password ; // "NvIR4766a";
    acc.terminal_id  = m_acc.options  ; // "4119362";
    acc.url          = m_acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = m_acc.login    ; // "akb442447";

    paynet::manager api(acc);

    paynet::info_t info;

    int ret = api.get_info(req, info);

    response.kv              = info.kv;
    response.merch_rsp       = info.status_text;
    response.merchant_status = info.status;
    response.merchant_trn_id = info.transaction_id;
    response.ts              = info.ts;

    response.kv_raw          = map_to_json_string(info.kv);

    if (ret != 0)
        return Error_merchant_operation;

    return Error_OK;
    
}
static std::string sync_http_request( oson::network::http::request http_rqst )
{
    SCOPE_LOG(slog); 
    
    std::shared_ptr< boost::asio::io_service >  io_service = std::make_shared< boost::asio::io_service > () ;

    scoped_register scope = make_scoped_register( *io_service );

    typedef oson::network::http::client_http client_http;
    typedef client_http::pointer pointer;

    pointer cl = std::make_shared< client_http >(io_service);

    cl->set_request(http_rqst);

    cl->async_start();

    boost::system::error_code ec;

    io_service->run(ec);

    std::string body = cl->body();
    
    return body;
}

Error_T Merchant_api_T::get_munis_info(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    // http://ip:port/fb-pay/services/payme/identification
    //HTTP Request Method: POST
    //content-type: application/json
    //
    //{ 
    //   "payment_system_id":"1234567890",
    //   "category":"MUNIS",
    //   "data":{ 
    //      "service_provider":"0101",
    //      "payment_type":"00",
    //      "keys":[ 
    //         "CUSTOMER_TYPE",
    //         "SOATO",
    //         "CUSTOMER"
    //      ],
    //      "values":[ 
    //         "2",
    //         "26266",
    //         "1235428"
    //      ]
    //   }
    //}
    
    //---------------------------------------------------------------------------------------------
    
    std::string service_provider, payment_type;
    
    ///////////////////////////////////////////////////
    {
        if (trans.service_id.empty() ) 
        {
            slog.DebugLog("service provider cannot found!");
            return Error_parameters;
        }
       
        //1. service_provider
        service_provider = trans.service_id ; 
       
        
        //2. payment_type
        if (trans.merch_api_params.count("payment_type")){
            
            payment_type = trans.merch_api_params.at("payment_type");
            switch(payment_type.length())
            {
                case 1: payment_type = "0" + payment_type;
                case 2: break;
                default:
                {
                    slog.WarningLog("invalid payment-type(used default '01'): '%s'", payment_type.c_str());
                    payment_type = "01";
                    break;
                }
            }
        }
        else
        {
            slog.DebugLog("payment type cannot found(used default '01')!");
            payment_type = "01";
            //return Error_parameters;
        }
    }
    ///////////////////////////////////////////////////////////
  //  {
        
      std::string json_keys = "[", json_values = "[";
        
        typedef Merch_trans_T::merch_api_map_t map_t;
        typedef map_t::value_type map_val_t;
        
        char comma = ' ';//first it must be a space
        
        for(const map_val_t & e : trans.merch_api_params)
        {
            std::string key  = e.first;
            std::string val  = e.second;
            
             //@Note payment_type skip
             if (key == "payment_type" || key.empty() )
                 continue;
            
            //@Note fix GAZ CODE_GP
            if (key ==  "CODE_GP" ){
                while (val.length() < 3){
                    val = "0" + val;
                }
            }
            
             //@Note: fix Sovuq suv fields
             if (m_merchant.id == merchant_identifiers::Munis_COLD_WATER )
             {
                 size_t const idx_semicolon = key.find(';');
                 slog.DebugLog("idx_semicolon: %zu", idx_semicolon);
                 if ( idx_semicolon != std::string::npos)
                 {
                     std::string lhs = key.substr(0, idx_semicolon);
                     std::string rhs = key.substr(idx_semicolon + 1);
                     if (payment_type == "01") // oplata po TARIFU
                         key = lhs;
                     else                    // oplata po schetchiku
                         key = rhs;
                     slog.DebugLog("lhs: '%s'\trhs: '%s'", lhs.c_str(), rhs.c_str());
                 }
             }
            json_keys += comma;
            json_keys += "  \"" + key + "\" " ;
            
            json_values += comma;
            json_values += "  \"" + val + "\" " ;
            
            comma = ',';
        }
        json_keys += "]" ;
        json_values += "]" ;
        
        std::string json_data = "{  \"service_provider\" : \"" + service_provider + "\",   \"payment_type\" : \"" + payment_type + "\" , "
                "  \"keys\" : " + json_keys + " ,   \"values\": " + json_values + " }  ";
//    }
    std::string json_request = " {  \"payment_system_id\" : \"" + m_acc.login + "\" ,  \"category\" : \"MUNIS\",  \"data\" : " + json_data + " } " ; 

    std::string const& json_value = json_request;
    slog.DebugLog("json_value: %s",   json_value.c_str());
    //---------------------------------------------------------------------------------------------
    
    std::string url = m_merchant.url; //  http://ip:port/fb-pay/services/payme/identification
    
    url += "/fb-pay/services/payme/identification";

    oson::network::http::request http_rqst = oson::network::http::parse_url(url);

    http_rqst.method          = "POST";
    http_rqst.content.charset = "UTF-8";
    http_rqst.content.type    = "json";
    http_rqst.content.value   = json_value;

    std::string body = sync_http_request(http_rqst) ;
    
    std::string text_response = body;
    
    slog.DebugLog("body : %s", body.c_str());
    if (text_response.empty())return Error_merchant_operation;
    
    
    //-------------------------------------------------------------------------
    namespace pt = boost::property_tree;
    pt::ptree json_response;
    std::istringstream ss(text_response);
    
    pt::read_json(ss, json_response);
    
//    {  
//   "payment_system_id":"1234567890",
//   "category":"MUNIS",
//   "status":"0",
//   "message":"Успешно",
//   "data":{
//      "sysinfo_sid":"5035823",
//      "sysinfo_data":"21.02.2017",
//      "sysinfo_time":"12:08:21",
//      "sysinfo_bid":"5035822",
//      "payer_branch":"12345",
//      "payer_account":"17409000100000110001",
//      "payer_name":"Пл-к к-талардан конунчиликда кузда тутилган бошка туловлар буйича ут-ши л/б-н ма",
//      "payer_inn":"200833707",
//      "payee_branch":"00123",
//      "payee_account":"22604000900123456001",
//      "payee_name":"эа электроэнергию население",
//      "payee_inn":"200835008",
//      "keys":[
//         "SOATO",
//         "CUSTOMER",
//         "NAME",
//         "ADDRESS",
//         "LAST_PAID_DATE",
//         "LAST_PAID_AMOUNT",
//         "TARIFF_PRICE",
//         "SALDO",
//         "PHONE"
//      ],
//      "values":[
//         "26266",
//         "1235428",
//         "Мухрриддинов К Р",
//         "ул. Юнусабад 565 кв-л, дом: 44, кв.: 155",
//         "06.06.2016",
//         "7500000",
//         "18200",
//         "11554620",
//         "998909712372"
//      ]
//   }
//}
    
    {
        response.merchant_status =  json_response.get< int >("status", -99999999 ) ;  //string2num( json_response["status"].asString() ) ;
        if (response.merchant_status != 0){
           // {
            //    "payment_system_id":"faa4a1c0-0d7a-4318-8050-24402af2a3b9",
            //    "category":"MUNIS",
            //    "status":"10134",
            // "message":"Ошибка! Постановление:GA18019310235 полностью оплачено:06.05.2018 22:51"}
            std::string message = json_response.get< std::string > ("message", "" ) ;  
            if ( ! message.empty() ) {
                response.kv["munis_error"] = message ;
                return Error_OK ;
            }
            return Error_merchant_operation;
        }
        pt::ptree nil;
        const pt::ptree& json_data   = json_response.get_child("data", nil) ;
        const pt::ptree& json_keys   = json_data.get_child("keys" , nil) ;
        const pt::ptree& json_values = json_data.get_child("values", nil) ;
        
        ////KEYS|VALUES
        
        for(pt::ptree::const_iterator it_k = json_keys.begin(), it_ke = json_keys.end(), it_v = json_values.begin(), it_ve = json_values.end();
                it_k != it_ke && it_v != it_ve; ++it_k, ++it_v)
        {
            std::string es = (*it_k).second.data();  
            std::string vs = (*it_v).second.data();  
            
            if (  (m_merchant.id == merchant_identifiers::Munis_Gaz) )
            {
                if (es == "SALDON" || es == "SALDOK" || es == "AKT"  || es == "RECEIPTS_ONLINE" || es == "RECEIPTS" || es == "CHARGE" ) // summa
                {
                    if (long long summa = string2num(vs) ) // if summa != 0
                    {
                        vs = to_money_str(summa, ',');
                    }
                }
            }
            
            response.kv.insert( std::make_pair( es, vs ) ); 
        }
 
        response.kv_raw = (text_response);
    }
    //-------------------------------------------------------------------------
    return Error_OK;
}

Error_T Merchant_api_T::get_bank_infin_kredit_info(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
     SCOPE_LOG(slog);
     
     //http://Url:port/is_loan/loan
     std::string address = m_merchant.url ;
     std::string path = "/is_loan/loan";
     std::string json_req = "{ \"terminal_id\" : \"" + m_merchant.terminalId + "\", \"un\" : \"" + m_acc.login + "\", \"pwd\" : \"" + m_acc.password + "\" } " ;

     std::string json_result;
     //2-step: get response from network
     {
         oson::network::http::request req = oson::network::http::parse_url(address + path);
         req.method          = "POST";
         req.content.charset = "utf-8";
         req.content.type    = "json";
         req.content.value   = json_req;

         json_result = sync_http_request( req );  

         if (json_result.empty())
             return Error_merchant_operation;
     }

     
     //3-step: parse response.
     {
         namespace pt = boost::property_tree;
         pt::ptree json_resp;
         std::istringstream ss(json_result);
         pt::read_json(ss, json_resp);

        response.kv_raw = json_result;
        typedef pt::ptree::value_type ptree_value_type;
        
        for( const ptree_value_type & e : json_resp){
            response.kv.insert( std::make_pair(e.first, e.second.data())) ;
        }
     }

     response.merchant_status = 0;
     return Error_OK ;
}
 
static size_t euclid_difference(std::string text, std::string orig)
{
    if (text.size() != orig.size())
        return (size_t)(-1);
    size_t n = 0;
    for(size_t i = 0; i != orig.size(); ++i)
        n += (text[i] != orig[i]) ? 1 : 0;
    return n;
}
static bool text_is_similar_helper(std::string text, std::string orig,  size_t max_possible_difference)
{
    if ( 0 == max_possible_difference )
        return ( text == orig ) ;
    
    if (text.size() == orig.size())
    {
        if ( euclid_difference(text, orig) <= max_possible_difference )
            return true;
    }
    
    if ( text.size() > orig.size() ) 
        text.swap(orig);
    
    if (text.size() +  max_possible_difference < orig.size())
       return false;
   
    // text.size() <= orig.size()
   for(size_t i = 0; i < orig.size(); ++i)
   {
       std::string orig_i = orig ;
       orig_i.erase(i, 1); // i-th symbol
       if (text_is_similar_helper(text, orig_i, max_possible_difference - 1) )
           return true;
   }
   return false;
}

static bool text_is_similar( std::string text, std::string orig )
{
    SCOPE_LOG(slog);
    slog.DebugLog("text: %s", text.c_str());
    slog.DebugLog("orig: %s", orig.c_str());
    //1. if empty no similar
    if (text.empty() || orig.empty())
        return false;
    if (text == orig)
        return false; // already equal, NOT similar
    if(text.size() > orig.size())
        text.swap(orig);
    
    // text.size() <= orig.size()
    size_t const max_possible_difference = 3;
    
    if (text.size() <= max_possible_difference)
        return false;
    
    return text_is_similar_helper(text, orig, max_possible_difference);
    
}

Error_T Merchant_api_T::get_mplat_info(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    std::string account;
    
    if (trans.merch_api_params.count("account")) 
    {
        account = trans.merch_api_params.at( "account" );
    }
    else if (! trans.param.empty() ) 
    {
        account = trans.param;
    }
    else if (trans.merch_api_params.count("login"))
    {
        account = trans.merch_api_params.at("login") ;
    }
    else
    {
        account = "< ???? >" ;
    }
    
    
    slog.DebugLog("account: %.*s\n", (::std::min)(2048, static_cast< int >(account.length())), account.c_str());
    
    Mplat::request_t request;
    request.auth.login    = "oson_uz_xml";
    request.auth.agent    = "39";
    request.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    request.body.account = account;
    request.body.date    = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    request.body.service = string2num( trans.service_id  ) > 0 ? string2num( trans.service_id ) : string2num( m_merchant.extern_service ) ;
    request.body.type    = "CHECK";
    
    //request.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t acc;
    
    acc.login   = "oson_uz_xml";
    acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    acc.agent   = "39";
    acc.url     = m_merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
    
    Mplat::manager_api mplat_api(acc);
    
    Mplat::response_t mplat_response;
    Error_T ec;
    ec = mplat_api.check(request, mplat_response);
    if (ec)
        return ec;
    
   //example: <r type="CHECK" result="0" client_rate="166.0830000" currency="RUR" message="OK" info="Extra test field"/>
    
    response.merchant_status = mplat_response.result ; //@Note, result actually status code.
    response.merch_rsp       = mplat_response.message ; // status - text status description
    //response.ts              = mplat_response.message
    
    //response.merchant_trn_id  = mplat_response.txn ;
    //response.kv["balance"] = mplat_response.balance ;
    struct fix_client_rate_t
    {
        std::string operator()(const std::string& value)const
        {
            std::string::size_type dot = value.find('.');
            
            if (dot == value.npos  ) // there no dot.
                return value + ".00";
            
            if (dot + 1 == value.size()) // 1799. 
                return value + "00"; // 1799.00
            
            if (dot + 2 == value.size() ) // 1799.7
                return value + "0"; // 1799.70
            
            std::string::size_type i = value.size();
            
            //strip last zero.
            while( i > dot + 3 && value[ i - 1 ] == '0')
                --i;
            
            if (i == value.size())
                return value;
            
            //dot + 2 < i < value.size()
            return value.substr( 0, i );

        }
    }fix_client_rate;
    
    response.kv["client_rate"] = fix_client_rate( mplat_response.client_rate ) ;
    response.kv["currency"] = mplat_response.currency ;
    response.kv["login"] = account;
    
    if (trans.uid != 0 ){
        std::string oson_note ;
        
        if (account == trans.user_phone)
        {
            //if(trans.uid_lang == LANG_uzb)
            //    oson_note = "To'g'ri" ;
            //else
           //     oson_note = "Правильно" ;
        }
        else if ( text_is_similar(account, trans.user_phone)){
            if (trans.uid_lang == LANG_uzb)
                oson_note = "Login va telefon raqamingiz mos kelmaydi, davom etasizmi?" ;
            else
                oson_note = "Логин и номер вашего телефона не совпадают, продолжить?" ;
        } else {
            if (trans.merch_api_params.count("oson_last_purchase_login")){
                std::string last_login = trans.merch_api_params.at("oson_last_purchase_login") ;
                
                if (account == last_login)
                {
                   // if(trans.uid_lang == LANG_uzb)
                   //     oson_note = "To'g'ri" ;
                   // else
                   //     oson_note = "Правильно" ;
                }
                else
                if (text_is_similar(account, last_login)){
                    if (trans.uid_lang == LANG_uzb)
                        oson_note= "Login oxirgi tulov bilan mos kelmaydi, davom etasizmi?" ;
                    else
                        oson_note = "Логин не совпадает с последного оплату, продолжить?" ;
                }
            }
        }
        
        if ( ! oson_note.empty()  ) 
            oson_note += "\n";
        
        if (trans.uid_lang == LANG_uzb){
            oson_note += "Ushbu to'lov to'liq amalga oshishi uchun 5-10 minut vaqt talab etiladi!" ;
        } else {
            oson_note += "Этот платеж займет около 5-10 минут!" ;
        }
        
        response.kv["oson_note"] = oson_note;
        slog.InfoLog("oson_note: %s", oson_note.c_str());
    }

    return Error_OK ;
}

static std::string sync_http_ssl_request(oson::network::http::request req, boost::asio::ssl::context::method ssl_method /*= boost::asio::ssl::context::sslv23*/ )
{
    SCOPE_LOGD(slog);
    
    std::shared_ptr< boost::asio::ssl::context> ctx = std::make_shared< boost::asio::ssl::context>( ssl_method  );
    ctx->set_default_verify_paths();
    ctx->set_verify_mode(ctx->verify_fail_if_no_peer_cert | ctx->verify_peer);

    std::shared_ptr< boost::asio::io_service> io_service = std::make_shared< boost::asio::io_service > () ;

    scoped_register scope = make_scoped_register( *io_service);

    typedef oson::network::http::client client_t;
    typedef client_t::pointer pointer;

    pointer c = std::make_shared< client_t >(io_service, ctx);

    c->set_request(req);

    c->async_start();

    boost::system::error_code ignored_ec;

    io_service->run( ignored_ec );

    std::string body =   c->body();
    return body;
}



///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
Error_T Merchant_api_T::check_comnet(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
//    Пример запроса на проверку возможности пополнения счёта:
//
//    https://94.158.48.4/paysys_check.cgi?command=check&account=testonly&sum=1000.00
//
//    где
//
//    account - логин абонента в системе Comnet
//    sum     - сумма платежа (сум), которую планируется зачислить на
//    счёт пользователя
    
    
    std::string param    = trans.param;
    std::string amount   = num2string( trans.amount / 100 ) ; // convert to sum.
    //std::string txn_date = formatted_time_now("%y%m%d%H%M%S");
    //std::string txn_id   = num2string(trans.transaction_id);

    // https://94.158.48.4/paysys_check.cgi?command=pay&txn_id=8379815&txn_date=20150820113000&account=testonly2&sum=1000.00
    //std::string server = "94.158.48.4:443";
    std::string path = m_merchant.url + "?command=check&account="+param+"&sum="+amount;
    oson::network::http::request req_ = oson::network::http::parse_url( path );
    
    std::string xml;
    int status_code;
    {
        
        xml = sync_http_ssl_request( req_ );
        
        //std::cerr << "comnet response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
        
        if ( xml.empty() )return Error_internal;
        //Fix xml string
        {
            std::string::size_type idx = xml.find("<?xml");
            if (idx != xml.npos && idx > 0)
                xml.erase(0, idx);

            idx = xml.find_last_of("</response>");
            if (idx != xml.npos)
                xml.erase(idx +  1);
        }
            
       slog.DebugLog("response(fixed): %s", xml.c_str());
         
    }
    
    //<?xml version="1.0" encoding="UTF-8"?>
    //<response>
    //<disable_paysys>1</disable_paysys>
    //<result>7</result>
    //</response>
    std::string pay_id;
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");

        status_code = resp.get<int>("result");
        slog.DebugLog("result: %d", status_code);
        //pay_id = resp.get<std::string>("osmp_txn_id");
    
    }
    status.exist = ( status_code == 0 ) ;
    return Error_OK ;
}

Error_T Merchant_api_T::check_sharq_telecom(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    //status.exist = true;
    //return Error_OK;
    
     SCOPE_LOG(slog);
    std::string param = trans.param;
    std::string amount = num2string(trans.amount / 100) + "." + num2string(trans.amount % 100);
    //   https://payment.st.uz/mypaysys.php?USERNAME=mypaysystem&PASSWORD=secret&ACT=0&CLIENTLOGIN=shsts1234567&PAY_AMOUNT=345.67&TRANTYPE=4
    std::string url = m_merchant.url;

    //std::string server = "payment.st.uz";
    std::string path   = url + "?USERNAME="+m_acc.login+"&PASSWORD="+m_acc.password+"&ACT=0&CLIENTLOGIN="+param+"&PAY_AMOUNT="+amount;

    
    std::string xml;
    int status_code;
    std::string pay_id;
    {
        oson::network::http::request req_ = oson::network::http::parse_url(path);
        
        xml = sync_http_ssl_request(req_);

//        std::cerr << "sharq-telecom response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
        
        if ( xml.empty() )return Error_internal;
    }
    
//    <?xml version="1.0" encoding="windows-1251" ?>
//<pay-response>
//	<status_code>20</status_code>
//	<pay_id>111111</pay_id>
//	<time_stamp>2013-03-27 11:49:39</time_stamp>
//	<client_name>Иванов Иван Иванович</client_name>
//</pay-response>
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code");
        
        if (status_code != 20){
            status.exist = false;
            return Error_merchant_operation;
        }
        
        pay_id = resp.get<std::string>("pay_id");
    
    }
    
    status.exist = true;
    
    //cancel created transaction.
    path   = url + "?USERNAME="+m_acc.login+"&PASSWORD="+m_acc.password+"&ACT=2&PAY_ID="+pay_id;
    {
        oson::network::http::request req_ = oson::network::http::parse_url(path);
        
        xml = sync_http_ssl_request(req_); //c->body();
//        std::cerr << "sharq-telecom response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
    }
    
    

    return Error_OK;
    
}
 
Error_T Merchant_api_T::check_uzinfocom(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);

    //unsigned long long time_params = milliseconds_since_epoch();
    std::string authHash = authorization_base_auth(m_acc.login, m_acc.password);
    //"T3NvbjpFd3lYS1YyamVUdE5HdmdzanowQjdqZ21IbzN6bThndHpxbmY=";//@Note this is generated from 'login:password'.
    slog.DebugLog("authHash: %s", authHash.c_str());
    std::string header = "Authorization: Basic " + authHash;
    
    std::string server = m_merchant.url;
                 
    std::string json_result;
    //1. createTransaction
    {
        std::string json_text = "{ \"jsonrpc\": \"2.0\",  \"id\": 1, \"method\": \"CheckPerformTransaction\", \"params\": "
                "{ \"account\": { \"login\": \""+trans.param       + "\" },"
                "  \"amount\": " + num2string(trans.amount)        + "}}    ";
                //"  \"id\": \""   + num2string(trans.transaction_id)+ "\",  "
                //"  \"time\": "   + num2string(time_params)         + " } } ";
        

                
        oson::network::http::request req_ = oson::network::http::parse_url(server);
        req_.headers.push_back( header );
        req_.method          = "POST";
        req_.content.type    = "json";
        req_.content.value   = json_text;
        req_.content.charset = "UTF-8";
        
        json_result = sync_http_ssl_request(req_);
        slog.DebugLog("Response: %s\n", oson::utils::prettify_json(json_result).c_str());
        if (json_result.empty()) return Error_internal;
    }
    
    
    //{
    //  "result":{
    //   "allow":true
    //}
    //}

    //parse-json
    {
        namespace pt = boost::property_tree;
        pt::ptree json_tree;
        std::istringstream ss(json_result);
        pt::read_json(ss, json_tree);
        
        if (int code = json_tree.get<int>("error.code", 0 ) ) {
//            boost::ignore_unused(code);
            slog.ErrorLog("code: %d", code);
            return Error_merchant_operation;
        }
        
        status.exist = json_tree.get< std::string >("result.allow", "false") == "true";
    }

    
    return Error_OK;
}

Error_T Merchant_api_T::check_cron_telecom(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    slog.DebugLog("cron telecom used OSON API.");

    return check_oson(trans, status);

}



Error_T Merchant_api_T::check_money_mover(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    int64_t service_id =   string2num( trans.service_id )  > 0 ? string2num( trans.service_id ) : string2num( m_merchant.extern_service ) ;
    int64_t trans_id   = trans.transaction_id ;
    
    namespace mm_llc = oson::backend::merchant::money_movers_llc ;
    
    mm_llc::acc_t acc;
    acc.agent = "BRIO_GROUP";
    acc.canal = "0";
    acc.url   = "https://222.235.235.248:13000" ;
    
    mm_llc::manager_t manager(acc);
    
    mm_llc::request_t req ;
    req.amount       = trans.amount / 150 ; //RUBL
    req.date_ts      = formatted_time_now_iso_T();
    req.service.id   = service_id ;
    req.service.sub_id         = 0 ;
    req.service.second_sub_id  = 0 ;
    req.service.third_sub_id   = 0 ;
    req.trn_id                 = trans_id ;
    req.user_login             = trans.param ;
    
    mm_llc::response_t resp = manager.info(req);
    if (resp.ec.code == 0 ){
        status.exist = true;
    } else {
        status.exist = false;
    }
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_money_mover(const Merch_trans_T& trans, Merch_trans_status_T& response) 
{
    SCOPE_LOG(slog);
    
    int64_t service_id = string2num( trans.service_id ) > 0 ? string2num ( trans.service_id ) : string2num( m_merchant.extern_service ) ;
    
    namespace mm_llc = oson::backend::merchant::money_movers_llc ;
    
    mm_llc::acc_t acc;
    acc.agent = "BRIO_GROUP";
    acc.canal = "0";
    acc.url   = "https://222.235.235.248:13000" ;
    
    mm_llc::manager_t manager(acc);
    
    mm_llc::request_t req ;
    req.amount       = trans.amount / 150 ; //RUBL
    req.date_ts      = formatted_time_now_iso_T();
    req.service.id   = service_id ;
    req.service.sub_id         = 0 ;
    req.service.second_sub_id  = 0 ;
    req.service.third_sub_id   = 0 ;
    req.trn_id                 = trans.transaction_id ;
    req.user_login             = trans.param ;
    
    mm_llc::response_t resp = manager.pay( req );
    
    response.merch_rsp       = resp.ec.en   ;
    response.merchant_status = resp.ec.code ;
    response.merchant_trn_id = resp.txn_id  ;

    if ( resp.ec.code  != 0 )
    {
        return Error_merchant_operation;
    }
    
    return Error_OK ;
}

Error_T Merchant_api_T::check_oson(const Merch_trans_T &trans, Merch_check_status_T &status)
{
    SCOPE_LOG(slog);
     
    // { "id" : 1, "jsonrpc" : "2.0" , "method" : "user.check", "params" : acc } 
    std::string root = "{  \"jsonrpc\" : \"2.0\",  \"id\" : 1, \"method\" : \"user.check\", \"params\" : { \"acc\": " + to_json_str(m_acc) + 
            ", \"info\": { \"login\" : \"" + trans.param + "\" } }  } " ;
    
    oson::network::http::request req_ = oson::network::http::parse_url(m_merchant.url);
    req_.method          = "POST"; // jsonrpc works only POST method.
    req_.content.type    = "application/json";
    req_.content.charset = "UTF-8";
    req_.content.value   = root;
    
    slog.DebugLog("Request: %s", oson::utils::prettify_json(req_.content.value).c_str());
    std::string resp_s = sync_http_ssl_request(req_);
    slog.DebugLog("Response: %s", oson::utils::prettify_json(resp_s).c_str());
            
    if (resp_s.empty())return Error_merchant_operation;
    
    //@Note: replace boost::property_tree to jsmn lightweight json parser
    namespace pt = boost::property_tree;
    pt::ptree resp_json;
    std::istringstream ss(resp_s);
    pt::read_json(ss, resp_json);
    
    pt::ptree resp_result = resp_json.get_child("result" ) ;
    
    if ( resp_result.count("error") ) 
    {
        pt::ptree resp_err = resp_result.get_child("error");
        int code = resp_err.get< int >("code") ;
        if (code != 0)
        {
            return Error_merchant_operation;
        }
    }
     
    std::string ex = resp_result.get< std::string > ("exist", "false");
    
    status.exist = ( ex == "true" ) ; 
    
    return Error_OK ;
}

Error_T Merchant_api_T::check_uzmobile_CDMA(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    status.exist = false;
    
//https://XX.XX.XX.XXX:443/PAY/pay_net_v2.php?[[имя_параметра=значение_параметра]&…]
    std::string server   = m_merchant.url; //"192.168.192.15:443";// "https://192.168.182.234:10449";
    std::string phone_s  = trans.param;
    std::string username = m_acc.login;
    std::string password = m_acc.password;
    std::string path     = "/pay_net_v2.php?act=4&username="+username+"&password="+password+"&phone="+phone_s;

    oson::network::http::request req_ = oson::network::http::parse_url(server + path);

    std::string xml =  sync_http_ssl_request(req_);  //c.body();
    if (xml.empty())
        return Error_internal;
   
    slog.DebugLog("uzmobile CDMA response: %s", xml.c_str());
    try
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");
    
        int status_code = resp.get<int>("status_code");
        
        status.exist  = ( 0 == status_code ) ;
  
    }catch(std::exception& e){
        slog.ErrorLog("exception: %s", e.what());
        return Error_internal;
    }
    return Error_OK;
}

Error_T Merchant_api_T::check_uzmobile_GSM(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    std::string url ;
    //1. make url
    {
        //https://XX.XX.XX.XXX:444/PAY/pay_net_v2.php?[[имя_параметра=значение_параметра]&…]
        std::string server =   m_merchant.url; //"http://192.168.182.234";//https://192.168.182.234:444";
        //std::string amount_s = to_str(trans.amount / 100);
        //std::string pay_id_s = to_str(trans.transaction_id);
        std::string phone_s  = trans.param;
        std::string username = m_acc.login;
        std::string password = m_acc.password;
        std::string path = "/oson/UzGsmOson.php?act=4&username="+username+"&password="+password+"&phone="+phone_s ;

        url = server + path;
    }
    
    //2. get xml response.
    oson::network::http::request request = oson::network::http::parse_url( url );
    std::string xml = sync_http_request(request);
    
    slog.DebugLog("uzmobile gsm response: %s", xml.c_str());
    if (xml.empty())
        return Error_merchant_operation;

    //3. parse xml
    try
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");
    
        int status_code = resp.get<int>("status_code");
        
        status.exist  = ( 0 == status_code ) ;
  
    }catch(std::exception& e){
        slog.ErrorLog("exception: %s", e.what());
        return Error_internal;
    }

    
    return Error_OK ;
}



Error_T Merchant_api_T::check_beeline(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    std::string phone    = trans.param;
    //std::string msisdn   = phone; 
    std::string amount   = num2string( trans.amount / 100 ); // convert to sum.
    std::string login    = m_acc.login;
    std::string password = m_acc.password;
    
    //90 109 38 65 - 9 ta
    if (phone.length() > 9 )
    {
        //remove prefix
        phone.erase( 0,  phone.length() - 9 );
    }
    
    //std::string url = m_merchant.url;
    
    //https://37.110.208.11:8444/work.html?ACT=0&USERNAME=oson&PASSWORD=Oson1505&MSISDN=901093865&PAY_AMOUNT=500&CURRENCY_CODE=2BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON
    std::string server = m_merchant.url;//"37.110.208.11:8444";
    std::string path = "/work.html?ACT=7&USERNAME="+login+"&PASSWORD="+password+"&MSISDN="+phone+"&PAY_AMOUNT="+amount;//+"&CURRENCY_CODE=2&BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON";
    
    oson::network::http::request req_ = oson::network::http::parse_url(server + path);
    
    std::string xml = sync_http_ssl_request(req_);
    int status_code;
    std::string pay_id;
    
    
    //std::cerr << "beeline response: \n" << xml << "\n";
    slog.DebugLog("response: xml:%s", xml.c_str());

    if ( xml.empty() )return Error_internal;
    
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code", -999);
    }
    
    enum{ BEELINE_STATUS_OK = 21 };
    
    if (status_code == BEELINE_STATUS_OK )
    {
        status.exist = true;
    }
    else
    {
        status.exist = false;
    }
    
    return Error_OK ;
}


Error_T Merchant_api_T::check_munis_merchants(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    status.exist = true;
    
    return Error_OK ;
}


Error_T Merchant_api_T::check_mplat_merchants(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    Mplat::request_t request;
    request.auth.login    = "oson_uz_xml";
    request.auth.agent    = "39";
    request.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    request.body.account = trans.param ;
    request.body.date = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    request.body.service = string2num( trans.service_id ) > 0 ? string2num( trans.service_id ): string2num( m_merchant.extern_service ) ;
    request.body.type = "CHECK";
    
    request.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t acc;
    
    acc.login   = "oson_uz_xml";
    acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    acc.agent   = "39";
    acc.url     = m_merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
    
    Mplat::manager_api mplat_api(acc);
    
    Mplat::response_t response;
    Error_T ec;
    
    ec = mplat_api.check(request, response);
    
    status.status_value = response.result;
    status.status_text  = response.message;
    
    if (ec)
        return ec;
    
    status.exist = ( response.result == 0 );
    
    return Error_OK ;
}



static std::string authorization_base_auth(const std::string& login, const std::string& password )
{
    std::string st = login + ":" + password;

    return oson::utils::encodebase64( st );
}
static unsigned long long milliseconds_since_epoch()
{
    unsigned long long result;
    boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970,1,1));
            
    boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
    boost::posix_time::time_duration diff = now - time_t_epoch;
    result = diff.total_milliseconds();
    
    return result;
}


Error_T Merchant_api_T::perform_uzinfocom_http(const Merch_trans_T & trans, Merch_trans_status_T &response)
{
    SCOPE_LOG(slog);
    
    unsigned long long time_params = milliseconds_since_epoch();
    std::string authHash = authorization_base_auth(m_acc.login, m_acc.password);
    //"T3NvbjpFd3lYS1YyamVUdE5HdmdzanowQjdqZ21IbzN6bThndHpxbmY=";//@Note this is generated from 'login:password'.
    slog.DebugLog("authHash: %s", authHash.c_str());
    std::string header  = "Authorization: Basic " + authHash;
    
    std::string server = m_merchant.url;
                 
    std::string json_result;
    //1. createTransaction
    {
        std::string json_text = "{ \"jsonrpc\": \"2.0\",  \"id\": 1, \"method\": \"CreateTransaction\", \"params\": "
                "{ \"account\": { \"login\": \""+trans.param       + "\" },"
                "  \"amount\": " + num2string(trans.amount)        + ",    "
                "  \"id\": \""   + num2string(trans.transaction_id)+ "\",  "
                "  \"time\": "   + num2string(time_params)         + " } } ";
        
        
        oson::network::http::request req_ = oson::network::http::parse_url(server);
        req_.method          = "POST"    ;
        req_.headers.push_back(header)   ;
        req_.content.charset = "UTF-8"   ;
        req_.content.type    = "json"    ;
        req_.content.value   = json_text ;

        json_result = sync_http_ssl_request(req_);  
        slog.DebugLog("Response: %s", oson::utils::prettify_json(json_result).c_str());
        if (json_result.empty()) return Error_internal;
    }
    
    //parse-json
    {
        namespace pt = boost::property_tree;
        pt::ptree json;
        std::istringstream ss(json_result);
        pt::read_json(ss, json);

        if (int code = json.get<int>("error.code", 0)){
            slog.WarningLog("code: %d", code);
            return Error_merchant_operation;
        }
        response.merchant_status = json.get< int >("result.state", 0 );  //res["state"].asInt();
        response.merchant_trn_id = json.get< std::string > ("result.transaction", "0") ; //res["transaction"].asString();
    }
    
    //2. PerformTransaction
    {
        std::string json_text = "{ \"jsonrpc\": \"2.0\",  \"id\": 1, \"method\": \"PerformTransaction\", \"params\": "
                "{ \"id\": \""+num2string(trans.transaction_id)+"\" } }";
        
        oson::network::http::request req_ = oson::network::http::parse_url(server);
        req_.method          = "POST"    ;
        req_.headers.push_back(  header )  ;
        req_.content.charset = "UTF-8"   ;
        req_.content.type    = "json"    ;
        req_.content.value   = json_text ;

        json_result = sync_http_ssl_request(req_); 
        slog.DebugLog("Reponse: %s", oson::utils::prettify_json(json_result).c_str()) ;
        if (json_result.empty()) return Error_merchant_operation;
    }
    
    //parse result
    {
        namespace pt = boost::property_tree;
        
        std::istringstream ss(json_result);
        pt::ptree json;
        pt::read_json(ss, json);
        
        if (int code = json.get< int > ("error.code", 0)) {
            slog.WarningLog("code: %d", code);
            return Error_merchant_operation;
        }
        
        response.merchant_status = json.get< int >("result.state" , 0);
        response.merchant_trn_id = json.get< std::string > ("result.transaction" , "0");
    }
        
    return Error_OK;
}

Error_T Merchant_api_T::perform_sharq_telecom(const Merch_trans_T & trans, Merch_trans_status_T &response)
{
    SCOPE_LOG(slog);
    std::string param = trans.param;
    std::string amount = num2string(trans.amount / 100) + "." + num2string(trans.amount % 100);
    //   https://payment.st.uz/mypaysys.php?USERNAME=mypaysystem&PASSWORD=secret&ACT=0&CLIENTLOGIN=shsts1234567&PAY_AMOUNT=345.67&TRANTYPE=4
    std::string url = m_merchant.url;

    //std::string server = "payment.st.uz";
    std::string path   = url + "?USERNAME="+m_acc.login+"&PASSWORD="+m_acc.password+"&ACT=0&CLIENTLOGIN="+param+"&PAY_AMOUNT="+amount;
    
    std::string xml;
    int status_code;
    std::string pay_id;
    {
        oson::network::http::request req_ = oson::network::http::parse_url(path);
        
        xml = sync_http_ssl_request(req_); 

//        std::cerr << "sharq-telecom response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
        
        if ( xml.empty() )return Error_merchant_operation;
    }
    
//    <?xml version="1.0" encoding="windows-1251" ?>
//<pay-response>
//	<status_code>20</status_code>
//	<pay_id>111111</pay_id>
//	<time_stamp>2013-03-27 11:49:39</time_stamp>
//	<client_name>Иванов Иван Иванович</client_name>
//</pay-response>
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code");
        
        if (status_code != 20){
            response.merchant_status = status_code;
            return Error_merchant_operation;
        }
        
        pay_id = resp.get<std::string>("pay_id");
    
    }
    
    // Подверждение транзакция
    
    //https://payment.st.uz/mypaysys.php?USERNAME=mypaysystem&PASSWORD=secret&ACT=1&PAY_ID=111111
    path = url + "?USERNAME="+m_acc.login+"&PASSWORD="+m_acc.password+"&ACT=1&PAY_ID=" + pay_id;
    {

        oson::network::http::request req_ = oson::network::http::parse_url(path);
       
        xml =  sync_http_ssl_request(req_);

        //std::cerr << "sharq-telecom response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
        
        if ( xml.empty() )return Error_merchant_operation;
    }
   
    // parse xml
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code", 0);
        //pay_id = resp.get<std::string>("pay_id");
    
    }
    
    response.merchant_status = status_code;
    response.merchant_trn_id = pay_id;
       
    // 22 - SUCCESS.
    if (status_code != 22)
    {
        return Error_merchant_operation;
    }
    return Error_OK; 
}

Error_T Merchant_api_T::perform_comnet(const Merch_trans_T & trans, Merch_trans_status_T &response)
{
    SCOPE_LOG(slog);
    
    std::string param    = trans.param;
    std::string amount   = num2string( trans.amount / 100 ); // convert to sum.
    std::string txn_date = formatted_time_now("%y%m%d%H%M%S");
    std::string txn_id   = num2string(trans.transaction_id);
    std::string server, url, path_prefix;
    url = m_merchant.url;

    //https://94.158.48.4/paysys_check.cgi?command=pay&txn_id=8379815&txn_date=20150820113000&account=testonly2&sum=1000.00
    //std::string server = "94.158.48.4:443";
    std::string path = url + "?command=pay&txn_id="+txn_id+"&txn_date="+txn_date+"&account="+param+"&sum="+amount;
    
    std::string xml;
    int status_code;
    {
        
        oson::network::http::request req_ = oson::network::http::parse_url(path);
       
        xml = sync_http_ssl_request(req_); //c.body();

        //std::cerr << "comnet response: \n" << xml << "\n";
        slog.DebugLog("response: %s", xml.c_str());
        
        if ( xml.empty() )return Error_internal;
        //Fix xml string
        {
            std::string::size_type idx = xml.find("<?xml");
            if (idx != xml.npos && idx > 0)
                xml.erase(0, idx);

            idx = xml.find_last_of("</response>");
            if (idx != xml.npos)
                xml.erase(idx +  1);
        }
            
       slog.DebugLog("response(fixed): %s", xml.c_str());
         
    }
    
//    <?xml version="1.0" encoding="UTF-8"?>
//    <response>
//    <osmp_txn_id>8376815</osmp_txn_id>
//    <prv_txn>26708</prv_txn>
//    <sum>1000.00</sum>
//    <result>0</result>
//    </response>
    std::string pay_id, status_text;
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");

        status_code = resp.get<int>("result");
        //pay_id = //resp.get<std::string>("osmp_txn_id");
        pay_id = resp.get< std::string > ("prv_txn", std::string());

   /*********** ERROR ANSWER   */        
//        <response>
//        <prv_txn></prv_txn>
//        <result>5</result>
//        <osmp_txn_id>20128</osmp_txn_id>
//        <comment>User not exist</comment>
//        <sum>120000</sum>
//        </response>

        if (resp.count("comment"))
            status_text = resp.get< std::string > ("comment");
    
    }
    response.merchant_trn_id = pay_id;
    response.merchant_status = status_code;
    response.merch_rsp       = status_text;
    
    //status != 0   so  error on purchase!!
    if ( status_code != 0 )
        return Error_merchant_operation;
    
    return Error_OK;
}

Error_T Merchant_api_T::check_paynet_api(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOGD(slog);
    
    if ( ! trans.service_id_check   ) // no service id for check
    {
        slog.WarningLog("There no service-id-check!");
        status.exist = true;
        return Error_OK ;
    }
    
    bool const paynet_extern_service = string2num( m_merchant.extern_service) > 0;
    bool const paynet_service = string2num( trans.service_id)  > 0; // extention fields need.
    
    namespace paynet = oson::backend::merchant::paynet;
    
    paynet::access_t acc;
    acc.merchant_id  = m_merchant.merchantId;
    acc.password     = m_acc.password ; // "NvIR4766a";
    acc.terminal_id  = m_acc.options  ; // "4119362";
    acc.url          = m_acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = m_acc.login    ; // "akb442447";

    paynet::request_t req;
    req.act            = 0;//needn't .
    req.amount         = trans.amount / 100; // sum
    req.fraud_id       = trans.user_phone  ; //oson::random_user_phone();
    req.client_login   = trans.param;
    req.service_id     = trans.service_id_check;
    req.transaction_id = trans.transaction_id;
    

    size_t len_match = 0;
    for(std::map<std::string, std::string>::const_iterator it = trans.merch_api_params.begin(), it_e = trans.merch_api_params.end(); it != it_e; ++it)
    {
        slog.DebugLog("merch_param[%s]: '%s'", (*it).first.c_str(), (*it).second.c_str());
        
        if ( (*it).second.length() > len_match && boost::ends_with(req.client_login, (*it).second)  )
             req.client_login_param_name = (*it).first, len_match = (*it).second.length(); // max match suffix will choose.
    }

    
    if (!paynet_extern_service && paynet_service) // extra
    {
        req.client_login.clear();
        req.client_login_param_name.clear();
        req.param_ext = trans.merch_api_params;
    }
    
    
    paynet::response_t resp;
    resp.status = 0;
    
    paynet::manager manager(acc);
    
    int res = manager.perform(req, resp);
    
    status.exist = (res == 0 )&& ( resp.status == 0 );
    
    //if (res != 0)
    //    return Error_merchant_operation;
    
    return Error_OK ;
    
}

Error_T Merchant_api_T::check_sarkor_telecom(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    std::string const address = m_merchant.url + "/payment/?type=json";
    //https://billing2.sarkor.uz/payment/?type=json 
    std::string reqt =  " { \"paysys_login\" : \"" + m_acc.login + 
                        "\" , \"paysys_pass\" : \"" + m_acc.password +
                        "\" ,  \"operation\" : 30 ,"
                        " \"client_id\" : \"" + trans.param + "\" } " ;

    slog.DebugLog("Request: %s\n", reqt.c_str());
    
    std::string resp_t;
    {
        oson::network::http::request req_ = oson::network::http::parse_url(address);
        //host, port, path determined.
        
        req_.method          = "POST"; // sarkor API required only POST method.
        req_.content.charset = "utf-8";
        req_.content.type    = "text/plain";
        req_.content.value   = reqt;
        
        resp_t =  sync_http_ssl_request(req_); //client.body();
    }
    
    slog.DebugLog("Response: %.2048s\n",  resp_t.c_str());
    
    if (resp_t.empty()) {
        return Error_merchant_operation;
    }
    
    namespace pt = boost::property_tree;
    pt::ptree respj;
    std::istringstream ss(resp_t);
    pt::read_json(ss, respj);
    
    status.exist = respj.get< int > ("status", 0) == 1 ;

    return Error_OK ;
}
    

Error_T Merchant_api_T::check_kafolat_insurance(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    //there no check API kafolat
    status.exist = true;
    return Error_OK ;
}

Error_T Merchant_api_T::check_nanotelecom(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    status.exist = false;
    
    if( ! trans.merch_api_params.count("account" ) )
    {
        slog.WarningLog("Required 'account' field not found!");
        status.status_text = "Required 'account' field parameter not found!" ;
        return Error_internal;
    }
    std::string account = trans.merch_api_params.at("account");
    std::string sum = to_str( trans.amount / 100.0, 2, false )   ;//@Note: convert it to SO"M .
//CHECK
//# curl -k -X GET 'https://91.240.12.128:8443/bgbilling/mpsexecuter/27/1/?command=check&txn_id=27&account=I/S-01&sum=1000' -H 'Authorization: Basic bmFubzpVc29oeGlyMA==' 
//<?xml version="1.0" encoding="UTF-8"?>
//<response>
//<result>0</result>
//<osmp_txn_id>27</osmp_txn_id>
//<comment>I/S-01 (test jeka)</comment>
//</response>
//

    std::string address = m_merchant.url; //https://91.240.12.128:8443/bgbilling/mpsexecuter/27/1/?
    std::string request_str = address + "command=check&txn_id="+ to_str(trans.transaction_id) + "&account="+ account  + "&sum=" + sum;
    oson::network::http::request req_ = oson::network::http::parse_url(request_str);
    req_.method = "GET";
    req_.headers.push_back("Authorization: Basic "+ authorization_base_auth(m_acc.login, m_acc.password)) ;
    
    slog.DebugLog("REQUEST: %s", request_str.c_str()) ;
    
    std::string response_str = sync_http_ssl_request( req_ );
    
    slog.DebugLog("RESPONSE: %s", response_str.c_str()) ;
    
    
    if (response_str.empty() ) {
        return Error_merchant_operation;
    }
    
    namespace pt = boost::property_tree;
    pt::ptree root;
    std::stringstream ss(response_str);
    pt::read_xml(ss, root);
    
    status.exist =  ( root.get< int > ("response.result", -999999 ) )  == 0 ;
    
    
    return Error_OK ;
}


/********************************************************************************************************/
Error_T Merchant_api_T::perform_paynet_api(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    bool const paynet_extern_service = string2num( m_merchant.extern_service) > 0;
    bool const paynet_service = string2num( trans.service_id ) > 0; // extention fields need.
    
    
    namespace paynet = oson::backend::merchant::paynet;
    
    paynet::access_t acc;
    acc.merchant_id  = m_merchant.merchantId;
    acc.password     = m_acc.password ; // "NvIR4766a";
    acc.terminal_id  = m_acc.options  ; // "4119362";
    acc.url          = m_acc.url      ; // "https://213.230.106.115:8443/PaymentServer";
    acc.username     = m_acc.login    ; // "akb442447";

    
    paynet::request_t req;
    req.act            = 0;//needn't .
    req.amount         = trans.amount / 100; // sum
    req.fraud_id       = trans.user_phone ; //oson::random_user_phone();
    req.client_login   = trans.param;
    req.service_id     = string2num ( trans.service_id ) ;
    req.transaction_id = trans.transaction_id;
    
    
    size_t len_match = 0;
    for(std::map<std::string, std::string>::const_iterator it = trans.merch_api_params.begin(), it_e = trans.merch_api_params.end(); it != it_e; ++it)
    {
        slog.DebugLog("merch_param[%s]: '%s'", (*it).first.c_str(), (*it).second.c_str());
        
        if ( (*it).second.length() > len_match && boost::ends_with(req.client_login, (*it).second)  )
             req.client_login_param_name = (*it).first, len_match = (*it).second.length(); // max match suffix will choose.
    }
    
    if (!paynet_extern_service && paynet_service) // extra
    {
        req.client_login.clear();
        req.client_login_param_name.clear();
        req.param_ext = trans.merch_api_params;
    }
    
    
    paynet::response_t resp;
    resp.status = 0;
    
    paynet::manager manager(acc);
    
    int res = manager.perform(req, resp);
    
    response.merchant_status = resp.status;
    response.merch_rsp       = resp.status_text ;
    response.merchant_trn_id = resp.transaction_id;
    
    if (res != 0)
        return Error_merchant_operation;
    
    if (resp.transaction_id == manager.invalid_transaction_value() )
        return Error_merchant_operation;
    
    return Error_OK ;
}




 

Error_T Merchant_api_T::perform_oson(const Merch_trans_T & trans, Merch_trans_status_T &response) 
{
    SCOPE_LOG(slog);
    
    std::string req_s = "{ \"jsonrpc\" : \"2.0\", \"id\" : 1, \"method\" : \"transaction.perform\", \"params\" : { \"acc\": " 
            + to_json_str(m_acc) + " , \"trans\" : " + to_json_str( trans ) +   " } } " ;
    
    oson::network::http::request req_ = oson::network::http::parse_url(m_merchant.url);
    req_.method = "POST";
    req_.content.charset = "UTF-8";
    req_.content.type = "application/json";
    req_.content.value = req_s;
    
    slog.DebugLog("Request: %s", oson::utils::prettify_json(req_s).c_str());
    std::string resp_s = sync_http_request(req_);// = sync_http_ssl_request(req_,  boost::asio::ssl::context::sslv3_client);
    slog.DebugLog("Response: %s", oson::utils::prettify_json(resp_s).c_str());
    if (resp_s.empty())
    {
        return Error_merchant_operation;
    }

// {
//   "jsonrpc" : "2.0",
//   "id" : "1",
//   "result" : {
//      "status" : 5,
//      "message" : "\u041a\u043b\u0438\u0435\u043d\u0442 \u043d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d",
//      "providerTrnId" : 1,
//      "ts" : "2018-03-08 01:03:35"
//   }
//}

    
    namespace pt = boost::property_tree ;
    
    pt::ptree resp_json;
    std::istringstream ss(resp_s);
    
    pt::read_json(ss, resp_json);

    if ( ! resp_json.count("result") ) {
        return Error_merchant_operation;
    }
    
    pt::ptree result_json = resp_json.get_child("result");
    if (result_json.count("error") ) {
        pt::ptree err_json = result_json.get_child("error");
        int code = err_json.get< int >("code");
        if (code != 0){
            return Error_merchant_operation;
        }
    }
    
    pt::ptree const& in = result_json;
    
    response.merchant_status = in.get<int>("status",  -1 );
    response.ts = in.get< std::string > ("ts", "1977-07-07 07:07:07");
    response.merchant_trn_id = in.get< std::string >("providerTrnId", "0");
    response.merch_rsp = in.get< std::string >("message", "-.-.-"); //@Note: cron-telecom  message uses KOI-7, where absolute couldn't read.
    response.merch_rsp = "base64(" + oson::utils::encodebase64(response.merch_rsp) + ")";
    
    if (response.merchant_status != 0)
        return Error_merchant_operation;
    
    return Error_OK;
}

//@Note: will delete it , We don't know what it is a fak!
Error_T Merchant_api_T::perform_cron_telecom(const Merch_trans_T &trans, Merch_trans_status_T & response)
{
    SCOPE_LOG(slog);
    slog.DebugLog("CRON used OSON API.");
    
    return perform_oson(trans, response);
    
//    std::string req_s = "{ \"jsonrpc\" : \"2.0\", \"id\" : 1, \"method\" : \"transaction.perform\", \"params\" : { \"acc\": " 
//            + to_json_str(m_acc) + " , \"trans\" : " + to_json_str( trans ) +   " } } " ;
//    
//    oson::network::http::request req_ = oson::network::http::parse_url(m_merchant.url);
//    req_.method = "POST";
//    req_.content.charset = "UTF-8";
//    req_.content.type = "application/json";
//    req_.content.value = req_s;
//    
//    slog.DebugLog("Request: %s", oson::utils::prettify_json(req_s).c_str());
//    std::string resp_s = sync_http_request(req_);// = sync_http_ssl_request(req_,  boost::asio::ssl::context::sslv3_client);
//    slog.DebugLog("Response: %s", oson::utils::prettify_json(resp_s).c_str());
//    if (resp_s.empty())
//    {
//        return Error_merchant_operation;
//    }
//
//// {
////   "jsonrpc" : "2.0",
////   "id" : "1",
////   "result" : {
////      "status" : 5,
////      "message" : "\u041a\u043b\u0438\u0435\u043d\u0442 \u043d\u0435 \u043d\u0430\u0439\u0434\u0435\u043d",
////      "providerTrnId" : 1,
////      "ts" : "2018-03-08 01:03:35"
////   }
////}
//
//    
//    namespace pt = boost::property_tree ;
//    
//    pt::ptree resp_json;
//    std::istringstream ss(resp_s);
//    
//    pt::read_json(ss, resp_json);
//
//    if ( ! resp_json.count("result") ) {
//        return Error_merchant_operation;
//    }
//    
//    pt::ptree result_json = resp_json.get_child("result");
//    if (result_json.count("error") ) {
//        pt::ptree err_json = result_json.get_child("error");
//        int code = err_json.get< int >("code");
//        if (code != 0){
//            return Error_merchant_operation;
//        }
//    }
//    
//    pt::ptree const& in = result_json;
//    
//    response.merchant_status = in.get<int>("status",  -1 );
//    response.ts = in.get< std::string > ("ts", "1977-07-07 07:07:07");
//    response.merchant_trn_id = in.get< std::string >("providerTrnId", "0");
//    response.merch_rsp = in.get< std::string >("message", "-.-.-"); //@Note: cron-telecom  message uses KOI-7, where absolute couldn't read.
//    response.merch_rsp = "base64(" + oson::utils::encodebase64(response.merch_rsp) + ")";
//    
//    if (response.merchant_status != 0)
//        return Error_merchant_operation;
//    
//    return Error_OK;
    
}


Error_T Merchant_api_T::perform_beeline(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    std::string phone = trans.param;
   // std::string msisdn =  (phone.size() >= 9) ? phone.substr(phone.size() - 9) : phone; // last 9 digits
    std::string amount = num2string( trans.amount / 100 ); // convert to sum.
    std::string login = m_acc.login;
    std::string password = m_acc.password;
    
    //phone number must be 9 digits
    if (phone.length() > 9 )
    {
        //remove prefix
        phone.erase(0, phone.length() - 9 ) ;
    }
    
    //https://37.110.208.11:8444/work.html?ACT=0&USERNAME=oson&PASSWORD=Oson1505&MSISDN=901093865&PAY_AMOUNT=500&CURRENCY_CODE=2BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON
    std::string server = m_merchant.url;//"37.110.208.11:8444";
    std::string path = server + "/work.html?ACT=0&USERNAME="+login+"&PASSWORD="+password+"&MSISDN="+phone+"&PAY_AMOUNT="+amount+"&CURRENCY_CODE=2&BRANCH=OSON&SOURCE_TYPE=1&TRADE_POINT=OSON";
    
    std::string xml;
    int status_code;
    std::string pay_id;
    std::string receipt;
    std::string ts_create, ts_commit;
    
    {
         oson::network::http::request req_ = oson::network::http::parse_url(path);
         xml = sync_http_ssl_request(req_);
        //std::cerr << "beeline response: \n" << xml << "\n";
        slog.DebugLog("response: xml:%s", xml.c_str());
        
        if ( xml.empty() )return Error_internal;
    }
    
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code", -999999);
        
        pay_id = resp.get<std::string>("pay_id", std::string() );
        
        ts_create = resp.get< std::string > ("time_stamp", std::string("2000-01-01 00:00:00"));
        
    }
    
    response.merchant_status  = status_code;
    
    if (status_code < 0) return Error_merchant_operation;
    
    
    //pay_id - идентификационный номер транзакции.
	//status_code – результат операции
	//time_stamp – дата проведения операции (DD.MM.YYYY HH24:MI:SS)
    
    path = server + "/work.html?ACT=1&USERNAME="+login+"&PASSWORD="+password+"&PAY_ID="+ pay_id ;
    {
         oson::network::http::request req_ = oson::network::http::parse_url(path);
         xml = sync_http_ssl_request(req_);
    
        slog.DebugLog("response: xml:%s", xml.c_str());
        
        if ( xml.empty() )return Error_internal;
    }
    
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("pay-response");

        status_code = resp.get<int>("status_code", -999999);
        
        receipt = resp.get<std::string>("receipt", std::string() );
        
        ts_commit = resp.get< std::string > ("time_stamp", std::string("2000-01-01 00:00:00"));
  
    }
    
    response.merchant_trn_id =  receipt;
    
    response.merchant_status  = status_code;
    
    response.merch_rsp =  pay_id + "," + ts_create + "," + ts_commit;
    
    //receipt - номер чека	
    //status_code – результат операции
    //time_stamp – дата проведения операции (DD.MM.YYYY HH24:MI:SS)
    
    response.ts = ts_commit;
    
    if (status_code < 0)return Error_merchant_operation;
  

    return Error_OK;
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////               UZMOBILE  API                            .......////////////////////////////////////////


Error_T Merchant_api_T::perform_uzmobile_CDMA(const Merch_trans_T & trans, Merch_trans_status_T &response)
{
    SCOPE_LOG(slog);
    
    //https://XX.XX.XX.XXX:443/PAY/pay_net_v2.php?[[имя_параметра=значение_параметра]&…]
    std::string server   =  m_merchant.url;  //"192.168.192.15:443"; //"https://84.54.74.246:444"; //"https://192.168.182.234:10449";
    std::string amount_s = to_str(trans.amount / 100);
    std::string pay_id_s = to_str(trans.transaction_id);
    std::string phone_s  = trans.param;
    std::string username = m_acc.login;
    std::string password = m_acc.password;
    std::string path = server+ "/pay_net_v2.php?act=0&username="+username+"&password="+password+"&amount="+amount_s+"&phone="+phone_s+"&pay_id="+pay_id_s;

    oson::network::http::request req_ = oson::network::http::parse_url(path);

    std::string xml =  sync_http_ssl_request(req_);  //c.body();
    if (xml.empty())
        return Error_internal;

    //std::cerr << "uzmobile response: \n" << xml << "\n";
    slog.DebugLog("uzmobile CDMA response: %s", xml.c_str());

    try
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");
        
        response.merchant_status = resp.get< int > ("status_code", -99);
        response.merchant_trn_id = resp.get< std::string > ("pay_id", "");
        response.ts              = resp.get< std::string > ("time_stamp", "");
        response.merch_rsp       = resp.get< std::string > ("error_text", "");
        
        if (response.merchant_status != 0 )
            return Error_merchant_operation;
        
        if (response.merch_rsp.empty())
            response.merch_rsp = "SUCCESS PAY UZMOBILE CDMA";
    }catch(std::exception& e){
        slog.ErrorLog("exception: %s", e.what());
        return Error_internal;
    }

    return Error_OK;
    
}

Error_T Merchant_api_T::perform_uzmobile_GSM(const Merch_trans_T & trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    std::string url ;
    //1. make url
    {
        //https://XX.XX.XX.XXX:444/PAY/pay_net_v2.php?[[имя_параметра=значение_параметра]&…]
        std::string server   = m_merchant.url; // "http://185.139.136.12:444"; //"http://192.168.182.234";//https://192.168.182.234:444";
        std::string amount_s = to_str(trans.amount / 100);
        std::string pay_id_s = to_str(trans.transaction_id);
        std::string phone_s  = trans.param;
        std::string username = m_acc.login;
        std::string password = m_acc.password;
        std::string path = "/oson/UzGsmOson.php?act=0&username="+username+"&password="+password+"&amount="+amount_s+"&phone="+phone_s+"&pay_id="+pay_id_s;

        url = server + path;
    }
    
    //2. get xml response.
    oson::network::http::request req_ = oson::network::http::parse_url(url);
    
    std::string xml = sync_http_request(req_);

    slog.DebugLog("uzmobile gsm response: %s", xml.c_str());

    if (xml.empty())
        return Error_merchant_operation;
    
   
    //3. parse xml
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("response");
        
        response.merchant_status = resp.get< int > ("status_code", -50499);
        response.merchant_trn_id = resp.get< std::string > ("pay_id", "");
        response.ts              = resp.get< std::string > ("time_stamp", "");
        response.merch_rsp       = resp.get< std::string > ("error_text", "");
        
        if (response.merchant_status != 0 )
            return Error_internal;
        
        if (response.merch_rsp.empty())
            response.merch_rsp = "SUCESS PAY UZMBILE GSM";
    }

    return Error_OK;
}

///////////////////////                END UZMOBILE API                        ..//////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
Error_T Merchant_api_T::get_info_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    if ( ! trans.merch_api_params.count("phone") ) {
        slog.WarningLog("Not found 'phone' parameter!");
        return Error_parameters;
    }
    std::string phone = trans.merch_api_params.at( "phone" );
    Merch_check_status_T status;
    Error_T ec = check_uzmobile_new(trans, status);
    if (ec) return ec;
    std::string pretty_phone = prettify_phone_number_uz(phone);
    if (status.exist){
        response.kv["login"] = pretty_phone;
    } else {
        response.kv["login"] = pretty_phone + " телефон не найден!" ;
    }
    return Error_OK ;
}

Error_T Merchant_api_T::check_uzmobile_new(const Merch_trans_T& trans, Merch_check_status_T& status ) 
{
    SCOPE_LOGD(slog);
    
    status.exist = false;
    
    if ( ! trans.merch_api_params.count("phone") ) {
        slog.WarningLog("Not found 'phone' parameter!");
        return Error_parameters;
    }
    
    std::string phone = trans.merch_api_params.at( "phone" );
    
    /***@Note:  REMOVE THIS CODE AFTER 1 October 2018.*/
    if ( m_merchant.id == merchant_identifiers::Uzmobile_CDMA_new_api )
    {
        
        Merch_acc_T acc;
        acc.login = "oson";
        acc.password = "1dXF50oqZ9Wki3Y";
        
        Merchant_info_T merchant;
        merchant.id = merchant_identifiers::UzMobile_CDMA ;
        merchant.url = "185.139.136.12:443";
        
        Merchant_api_T old_api( merchant, acc );
        Error_T ec = old_api.check_uzmobile_CDMA(trans, status);
        
      if (!ec && status.exist)
      {
          typedef Merch_trans_T::merch_api_map_t map_t;
          map_t * mp = const_cast<map_t*>(std::addressof(trans.merch_api_params)) ;
          mp->insert(std::make_pair("uzmobile_use_old_api", "on" ) ) ;
          return Error_OK ;
      } else {
          slog.WarningLog("Not found %s phone in standard API", phone.c_str() ) ;
      }
    }  else if ( m_merchant.id == merchant_identifiers::Uzmobile_GSM_new_api ) {
        
        Merch_acc_T acc;
        acc.login = "oson";
        acc.password = "1dXF50oqZ9Wki3Y";
        
        Merchant_info_T merchant;
        merchant.id = merchant_identifiers::UzMobile_GSM ;
        merchant.url = "185.139.136.12:444" ;
        
        Merchant_api_T old_api( merchant, acc );
        
        Error_T ec = old_api.check_uzmobile_GSM(trans, status);
        if (!ec && status.exist)
        {
           typedef Merch_trans_T::merch_api_map_t map_t;

            map_t * mp = const_cast<map_t*>(std::addressof(trans.merch_api_params)) ;
            mp->insert(std::make_pair("uzmobile_use_old_api", "on" ) ) ;
            return Error_OK;
        } else {
            slog.WarningLog("Not found %s phone in standard API", phone.c_str());
        }
    }
    /*********************************************************/

    /**** USE A NEW API OTHERWISE */
    //http://HOST:PORT/KKM_PG_GATE/HTTP_ALLOW_PAY_QUERY?P_LOGIN_NAME=Login&P_LOGIN_PASSWD=Password&P_MSISDN=9262104045
    //std::string query =      //"https://pays.uzmobile.uz:446/KKM_PG_GATE/HTTP_ALLOW_PAY_QUERY?P_LOGIN_NAME=oson&P_LOGIN_PASSWD=OgNV3HQahUg&P_MSISDN=" + phone ;
    
    //curl -ikvvv -X GET 'http://pays.uzmobile.uz:449/KKM_PG_GATE/HTTP_ALLOW_PAY_QUERY?P_LOGIN_NAME=oson&P_LOGIN_PASSWD=OgNV3HQahUg&P_MSISDN=951946000'
    std::string query = m_merchant.url + "/KKM_PG_GATE/HTTP_ALLOW_PAY_QUERY?P_LOGIN_NAME="+m_acc.login+"&P_LOGIN_PASSWD="+m_acc.password+"&P_MSISDN="+phone ;
    auto http_req = oson::network::http::parse_url(query);
    
    http_req.method = "GET";
    
    
    slog.DebugLog("REQUEST: %s", query.c_str());
    std::string resp_s = sync_http_request( http_req  );
    slog.DebugLog("RESPONSE: %s", resp_s.c_str());
    
    if (resp_s.empty()) return Error_communication ;
    
//    <KKM_PG_GATE>
//        <ERROR SQLCODE="0" SQLERRM="ORA-0000: normal, successful completion" /> 
//        <RESULT>YES</RESULT> 
//        <PAY_CAT>0</PAY_CAT> 
//        <PAY_CAT>1</PAY_CAT> 
//        <PAY_CAT>2</PAY_CAT> 
//        <PAY_CAT>3</PAY_CAT> 
//    </KKM_PG_GATE>
    
    namespace pt = boost::property_tree;
    pt::ptree root_tree;
    std::stringstream ss(resp_s);
    try{
        pt::read_xml(ss, root_tree);
    }catch(pt::ptree_error & e ) {
        slog.ErrorLog("exception: %s ", e.what());
        return Error_internal;
    }
    
    std::string sqlcode = root_tree.get<std::string>("KKM_PG_GATE.ERROR.<xmlattr>.SQLCODE", "-1" ) ;
    
    slog.InfoLog("sqlcode: %s ", sqlcode.c_str());
    
    status.exist =  string2num(sqlcode) == 0 ;
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    //http://host:port/KKM_PG_GATE/HTTP_ADD_PAYMENT?P_LOGIN_NAME=login&P_LOGIN_PASSWD=password&P_MSISDN=9262104045&P_PAY_AMOUNT=200
    
    if ( ! trans.merch_api_params.count("phone") ) {
        slog.WarningLog("Not found 'phone' parameter!");
        return Error_parameters;
    }
    
    std::string phone = trans.merch_api_params.at( "phone" );
    
    /***@Note:  REMOVE THIS CODE AFTER 1 October 2018.*/
    const bool use_old_api = trans.merch_api_params.count("uzmobile_use_old_api") && 
                       trans.merch_api_params.at("uzmobile_use_old_api") == "on" ;
    
    if (use_old_api) 
    {
        slog.WarningLog("use old uzmobile API");
        if (   m_merchant.id == merchant_identifiers::Uzmobile_CDMA_new_api) 
        {
            Merch_acc_T acc;
            acc.login = "oson";
            acc.password = "1dXF50oqZ9Wki3Y";

            Merchant_info_T merchant;
            merchant.id = merchant_identifiers::UzMobile_CDMA ;
            merchant.url = "185.139.136.12:443";

            Merchant_api_T old_api( merchant, acc );
            
            Merch_trans_status_T old_response;
            Error_T ec = old_api.perform_uzmobile_CDMA(trans, old_response );

            if ( ! ec )
            {
                response = old_response;
                return Error_OK ;
            } 
            else 
            {
                slog.WarningLog("Not found %s phone in standard API", phone.c_str() ) ;
                return ec;
            }
        }  else if ( m_merchant.id == merchant_identifiers::Uzmobile_GSM_new_api ) {

            Merch_acc_T acc;
            acc.login = "oson";
            acc.password = "1dXF50oqZ9Wki3Y";

            Merchant_info_T merchant;
            merchant.id = merchant_identifiers::UzMobile_GSM ;
            merchant.url = "185.139.136.12:444" ;

            Merchant_api_T old_api( merchant, acc );
            
            Merch_trans_status_T old_response;
            
            Error_T ec = old_api.perform_uzmobile_GSM(trans, old_response );
            
            if (!ec)
            {
                response = old_response ;
                return Error_OK;
            } 
            else 
            {
                slog.WarningLog("Not found %s phone in standard API", phone.c_str());
                return ec;
            }
        }
    }
    /*********************************************************/

    //curl  -X GET "http://pays.uzmobile.uz:449/KKM_PG_GATE/HTTP_ADD_PAYMENT?P_LOGIN_NAME=oson&P_LOGIN_PASSWD=OgNV3HQahUg&P_MSISDN=951775114&P_PAY_AMOUNT=100&P_RECEIPT_NUM=3&P_DATE=24.07.2018%2016:08:08"
    
    std::string query = m_merchant.url + "/KKM_PG_GATE/HTTP_ADD_PAYMENT?P_LOGIN_NAME=" + m_acc.login +
                        "&P_LOGIN_PASSWD=" + m_acc.password +
                        "&P_MSISDN="       + phone +
                        "&P_PAY_AMOUNT="   + to_str( trans.amount / 100 ) + // convert to sum
                        "&P_RECEIPT_NUM="  + to_str( trans.transaction_id ) +
                        "&P_DATE=" + formatted_time_now("%d.%m.%Y%%20%H:%M:%S") ;
                        //24.07.2018%2016:08:08
    
    auto http_req = oson::network::http::parse_url(query);
    
    http_req.method = "GET";
    
    
    slog.DebugLog("REQUEST: %s", query.c_str());
    std::string resp_s = sync_http_request( http_req  );
    slog.DebugLog("RESPONSE: %s", resp_s.c_str());
    
    if (resp_s.empty()) return Error_communication ;
 
    //<?xml version="1.0" encoding="utf-8"?>
    //<KKM_PG_GATE>
    //   <OPERATION_DATE>24.07.2018 15:18:53</OPERATION_DATE>
    //   <ERROR SQLCODE="0" SQLERRM="ORA-0000: normal, successful completion"/>
    //   <PAY_SALE_TAX>16.67</PAY_SALE_TAX>
    //   <RECEIPTS><RECEIPT>1-OS</RECEIPT></RECEIPTS>
    //</KKM_PG_GATE>
 
     
    namespace pt = boost::property_tree;
    pt::ptree root_tree;
    std::stringstream ss(resp_s);
    try{
        pt::read_xml(ss, root_tree);
    }catch(pt::ptree_error & e ) {
        slog.ErrorLog("exception: %s ", e.what());
        return Error_internal;
    }
    
    std::string sqlcode = root_tree.get<std::string>("KKM_PG_GATE.ERROR.<xmlattr>.SQLCODE", "-1" ) ;
    
    slog.InfoLog("sqlcode: %s ", sqlcode.c_str());
    
    response.merchant_status = string2num(sqlcode);
    response.merchant_trn_id = "0";
    response.merch_rsp       = "OPERATION_DATE: " + root_tree.get< std::string>("KKM_PG_GATE.OPERATION_DATE", "-") ;
    
    if (response.merchant_status != 0 ) 
    {
        return Error_merchant_operation;
    }
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_status_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    return Error_OK ;
}

Error_T Merchant_api_T::perform_cancel_uzmobile_new(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    return Error_OK ;
}
        
/******************************************************/


Error_T Merchant_api_T::perform_munis_merchants(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);


//    http://ip:port/fb-pay/services/payme/payment

//HTTP Request Method: POST
//content-type: application/json
    typedef Merch_trans_T::merch_api_map_t map_t;
    map_t::const_iterator mit = trans.merch_api_params.find("sysinfo_sid");
    if (mit == trans.merch_api_params.end())
    {
        slog.WarningLog("sysinfo_sid not found");
        return Error_parameters;
    }
    
    std::string sysinfo_sid = (*mit).second;
    
    std::string authorization_code, operation_number;
    mit = trans.merch_api_params.find("oson_eopc_ref_num");
    if (mit == trans.merch_api_params.end() ) {
        slog.WarningLog("eopc_ref_num parameter not found!");
        return Error_parameters;
    }
    std::string ref_num = (*mit).second;
    if ( ::boost::algorithm::starts_with(ref_num, "00") ) {
        ref_num.erase(0, 2 ) ;
    }
    
    authorization_code = ref_num;
    operation_number   = ref_num;
    
    if (authorization_code.length() > 6 ) {
        authorization_code.erase(0, authorization_code.length() - 6 ) ;
    }          
    slog.InfoLog("authorization-code: %s  \t operation-number: %s ", authorization_code.c_str(), operation_number.c_str());
    
    //---------------------------------------------------------------------------------------------
    std::string json_value = "{ \n"
        "     \"payment_system_id\" : \""  + m_acc.login + "\", \n"
        "     \"category\" : \"MUNIS\", \n" 
        "     \"data\" : {  \n"
        "           \"sysinfo_sid\" : " + sysinfo_sid + ", \n"
        "           \"amount_value\": " + to_str(trans.amount) + ", \n"
        "           \"pay_purpose\" : \"\" , \n"
        "           \"transaction_info\" : { \n"
        "                  \"authorization_code\": " + authorization_code                 +  ", \n"
        "                  \"operation_number\": "   + operation_number                   +  ", \n"
        "                  \"terminal\" : "          + m_merchant.terminalId              +  ", "
        "                  \"payment_date\": \""     + formatted_time_now("%y%m%d%H%M%S") +  "\" \n"
        "           } \n"
        "      } \n"
        "} " ;
    
//    Json::Value json_request(Json::objectValue);
//    json_request["payment_system_id"] =  m_acc.login; //1. OK
//    json_request["category"] = "MUNIS";               //2, OK
//    Json::Value json_data(Json::objectValue), json_tr_info(Json::objectValue);
//    
//    json_tr_info["payment_date"] = formatted_time_now("%y%m%d%H%M%S");
//    json_tr_info["terminal"]     = string2num(m_merchant.terminalId);
//    json_tr_info["operation_number"] =  static_cast< long long > ( trans.transaction_id  );
//    json_tr_info["authorization_code"] =  14044 ;
//    
//    json_data["sysinfo_sid"] = string2num(sysinfo_sid); 
//    json_data["amount_value"] =  static_cast< long long > (trans.amount) ;    //TIYIN
//    json_data["pay_purpose"]  = "";
//    json_data["transaction_info"] =  json_tr_info;
//    
//    json_request["data"] = json_data;
//    Json::FastWriter json_writer;
//    std::string json_value = json_writer.write(json_request);
    //---------------------------------------------------------------------------------------------
    
    std::string url = m_merchant.url+"/fb-pay/services/payme/payment"; //  http://ip:port/fb-pay/services/payme/identification
    
    oson::network::http::request http_rqst = oson::network::http::parse_url(url);
    
    http_rqst.method          = "POST";
    http_rqst.content.charset = "utf-8";
    http_rqst.content.type    = "json";
    http_rqst.content.value   = json_value;

    std::string body = sync_http_request(http_rqst);

    std::string text_response = body;
    
    if (text_response.empty())
        return Error_merchant_operation;
    
    {
        //logging
        const std::string& tex  =  text_response ;
        slog.DebugLog("response : %s ", tex .c_str());
    }
    
    //-------------------------------------------------------------------------
    namespace pt = boost::property_tree;
    pt::ptree json_response;
    std::istringstream ss(text_response);
    pt::read_json(ss, json_response);
    
    response.merchant_status = json_response.get< int > ("status", -1);
    response.merch_rsp       = json_response.get< std::string >("message", "-");
    response.merchant_trn_id = sysinfo_sid;
    if (response.merchant_status != 0) // 0 - success
        return Error_merchant_operation;
//    Json::Reader json_reader;
//    Json::Value json_response;
//    bool suc = json_reader.parse(text_response, json_response, false);
//    if(!suc)
//    {
//        slog.ErrorLog("Can't parse as json the response.");
//        return Error_merchant_operation;
//    }
//  
////    { 
////   "payment_system_id":"1234567890",
////   "category":"MUNIS",
////   "status":"0",
////   "message":"Успешно"
////}
//
//    
//    {
//        response.merchant_status = string2num( json_response["status"].asString() ) ;
//        response.merch_rsp       = json_response["message"].asString();
//        response.merchant_trn_id = sysinfo_sid;
//        if (response.merchant_status != 0){
//            return Error_merchant_operation;
//        }
//    }
    //-------------------------------------------------------------------------
    
    return Error_OK ;
}



Error_T Merchant_api_T::perform_bank_infin_kredit(const Merch_trans_T& trans,  Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    //http://Url:port/is_loan/payment
    std::string address = m_merchant.url;
    std::string path = "/is_loan/payment";
    
    std::string json_req;
    //1-step. make json
    {
        json_req = "{\"un\" : \""            + m_acc.login                  +  "\" , "
                   "  \"pwd\" : \""          + m_acc.password               +  "\" , "
                   "  \"terminal_id\": \""   + m_merchant.terminalId        +  "\" , "
                   "  \"request_id\" : "     + to_str(trans.transaction_id) +  "   , "
                   "  \"loan_id\" : \""      + trans.param                  +  "\" , "
                   "  \"amount\" : "         + to_str(trans.amount)         +  "   , "
                   "  \"intereset\" : 232 , "
                   "  \"nspc_trans_id\": 23324, "
                   "  \"payment_type\": 1 "
                   " } " ;
//        Json::Value json(Json::objectValue);
//
//        json["un"  ]          = m_acc.login;
//        json["pwd" ]          = m_acc.password;
//        json["terminal_id"]   = m_merchant.terminalId;
//        json["request_id"]    = static_cast< long long > ( trans.transaction_id ) ;
//        json["loan_id"]       = trans.param;
//        json["amount"]        = static_cast< long long > ( trans.amount ) ;
//        json["interest"]      = 232;
//        json["nspc_trans_id"] = 23324;
//        json["payment_type"]  = 1;
//
//        Json::FastWriter jsonwriter;
//        json_req = jsonwriter.write(json);
    }
    
    std::string json_result;
    //2-step: get response from network
    
     {
         oson::network::http::request req = oson::network::http::parse_url(address + path);
         req.method          = "POST";
         req.content.charset = "utf-8";
         req.content.type    = "json";
         req.content.value   = json_req;
       
         json_result = sync_http_request(req);  //c.body();
         if (json_result.empty())
             return Error_merchant_operation;
     }

     
     //3-step: parse response.
     {
//        Json::Reader jsonreader;
//        Json::Value json_resp;
//        jsonreader.parse(json_result, json_resp);

     }
     response.merchant_status = 0;
     response.merchant_trn_id = "07789888";
    
    return Error_OK ;
}

//Error_T Merchant_api_T::perform_status(const Merch_trans_T& trans, Merch_trans_status_T& response)
//{
//    SCOPE_LOG(slog);
//    if ( ! merchant_identifiers::is_mplat(m_merchant.id)){
//        // only mplat perform status is support
//        response.merchant_status = 0;
//        response.merch_rsp = "perform status not supported";
//        return Error_OK ;
//    }
//    
//    return perform_status_mplat(trans, response);
//    
//}
//
//Error_T Merchant_api_T::perform_status_mplat(const Merch_trans_T& trans, Merch_trans_status_T& response)
//{
//    SCOPE_LOG(slog);
//    
//    namespace Mplat = oson::backend::merchant::Mplat;
//    
//    Mplat::request_t request;
//    request.auth.login    = "oson_uz_xml";
//    request.auth.agent    = "39";
//    request.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
//    
//    request.body.type     = "STATUS";
//    request.body.txn      = response.merchant_trn_id ;
//    
//    Mplat::acc_t acc;
//    
//    acc.login   = "oson_uz_xml";
//    acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
//    acc.agent   = "39";
//    acc.url     = m_merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
//    acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
//    
//    Mplat::manager_api mplat_api(acc);
//    
//    Mplat::response_t mplat_response;
//    Error_T ec;
//    
//    ec = mplat_api.status(request, mplat_response);
//    if ( ec ) return ec;
//        
//        
//    response.merchant_status = mplat_response.result ;
//    
//    bool const in_progress = (response.merchant_status == 26 || response.merchant_status == 90 || response.merchant_status == 91) ;
//    
//    if (response.merch_rsp.empty() || ! in_progress)
//    {
//        response.merch_rsp += "; " + mplat_response.message ;
//    }
//    
//    if (in_progress)
//    { 
//        return Error_perform_in_progress ;
//    }
//    
//    
//    if ( !( response.merchant_status  ==  0 || // OK/SUCCESS
//            response.merchant_status  == 25 || // New transaction NOT FATAL
//            response.merchant_status  == 26 || // Payment in progress NOT FATAL
//            response.merchant_status  == 90 || // Conducting the payment is not completed NOT FATAL
//            response.merchant_status  == 91    // Suspicious payment NOT FATAL
//          ) 
//      )
//    {
//        return Error_merchant_operation;
//    }
//    
//    
//        
//    return Error_OK ;
//}
    
//namespace Mplat = oson::backend::merchant::Mplat;
//
//namespace
//{
//    
//struct mplat_perform_status_handler
//{
//     Merch_trans_T  trans;
//     Merch_trans_status_T  response ;
//     std::function< void(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) > handle;
//
//     void operator()(const struct Mplat::request_t& mplat_req,  const struct Mplat::response_t& mplat_response, Error_T ec)
//     {
//         if (ec)
//             return handle(trans, response, ec);
//
//         response.merchant_status = mplat_response.result ;
//
//        bool const in_progress = (response.merchant_status == 26 || response.merchant_status == 90 || response.merchant_status == 91) ;
//
//        if (response.merch_rsp.empty() || ! in_progress)
//        {
//            response.merch_rsp += "; " + mplat_response.message ;
//        }
//
//        if (in_progress)
//        { 
//            return handle(trans, response, Error_perform_in_progress) ;
//        }
//
//
//        if ( !( response.merchant_status  ==  0 || // OK/SUCCESS
//                response.merchant_status  == 25 || // New transaction NOT FATAL
//                response.merchant_status  == 26 || // Payment in progress NOT FATAL
//                response.merchant_status  == 90 || // Conducting the payment is not completed NOT FATAL
//                response.merchant_status  == 91    // Suspicious payment NOT FATAL
//              ) 
//          )
//        {
//            return handle(trans, response,  Error_merchant_operation);
//        }
//
//        return handle(trans, response, Error_OK);
//     }
//};
//
//}
//void Merchant_api_T::async_mplat_perform_status(const Merch_trans_T& trans, Merch_trans_status_T& response,
//                        std::shared_ptr< boost::asio::io_service> &ios_ptr,
//                        std::function< void(const Merch_trans_T& trans, const Merch_trans_status_T& response, Error_T ec) > handle 
//    ) 
//{
//    SCOPE_LOG(slog);
//        
//    namespace Mplat = oson::backend::merchant::Mplat;
//    
//    Mplat::request_t request;
//    request.auth.login    = "oson_uz_xml";
//    request.auth.agent    = "39";
//    request.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
//    
//    request.body.type     = "STATUS";
//    request.body.txn      = response.merchant_trn_id ;
//    
//    Mplat::acc_t acc;
//    
//    acc.login   = "oson_uz_xml";
//    acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
//    acc.agent   = "39";
//    acc.url     = m_merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
//    acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
//    
//    Mplat::async::manager_api mplat_api( acc, ios_ptr );
//    
//    Mplat::response_t mplat_response;
//    mplat_perform_status_handler mplat_handle;
//    mplat_handle.response = response;
//    mplat_handle.trans    = trans;
//    mplat_handle.handle   = handle;
//    
//    return mplat_api.status(request,  mplat_handle);
//}
    



Error_T Merchant_api_T::perform_mplat_merchants(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    namespace Mplat = oson::backend::merchant::Mplat;
    
    Mplat::request_t request;
    request.auth.login    = "oson_uz_xml";
    request.auth.agent    = "39";
    request.auth.password = "51ffc21eb953535c3a0096bfd6c04bf7" ;//md5 of password
    
    request.body.account  = trans.param ;
    request.body.date     = formatted_time_now("%Y-%m-%dT%H:%M:%S");//   "2017-12-22T15:38:48"
    request.body.service  = string2num( trans.service_id ) != 0  ? string2num( trans.service_id ) : string2num( m_merchant.extern_service ) ;
    request.body.type     = "PAY";
    request.body.id       = to_str(trans.transaction_id );
    request.body.amount   = to_money_str(trans.amount , '.' );
    
    //request.extra.ev_s.push_back( std::make_pair("ev_test", "0") ) ;
    
    Mplat::acc_t acc;
    
    acc.login   = "oson_uz_xml";
    acc.pwd_md5 = "51ffc21eb953535c3a0096bfd6c04bf7";
    acc.agent   = "39";
    acc.url     = m_merchant.url;//"https://gate.mplat.info/Gate/Default" ;//
    acc.sign    = "e2f8723abee4d0470ed86cf2ed48e087"; // OWN sign.
    
    Mplat::manager_api mplat_api(acc);
    
    Mplat::response_t mplat_response;
    Error_T ec;
    ec = mplat_api.pay(request, mplat_response);
    if (ec)
        return ec;
    
    //status.exist = ( response.result == 0 );
    response.merchant_status = mplat_response.result;
    response.merch_rsp       = mplat_response.message;
    response.merchant_trn_id = mplat_response.txn;
    //response.ts              = mplat_response.
    
    if (mplat_response.result == 91)// Podozritelniy
    {
    }
    
    if (mplat_response.result == 26 || mplat_response.result == 90 || mplat_response.result == 91)
        return Error_perform_in_progress ;
    
    if ( !( response.merchant_status  ==  0 || // OK/SUCCESS
            response.merchant_status  == 25 || // New transaction NOT FATAL
            response.merchant_status  == 26 || // Payment in progress NOT FATAL
            response.merchant_status  == 90 || // Conducting the payment is not completed NOT FATAL
            response.merchant_status  == 91    // Suspicious payment NOT FATAL
          ) 
      )
    {
        return Error_merchant_operation;
    }
    
    return Error_OK;
}

 Error_T Merchant_api_T::perform_sarkor_telecom(const Merch_trans_T& trans, Merch_trans_status_T& response)
 {
    SCOPE_LOG(slog);
    std::string const address = m_merchant.url + "/payment/?type=json"; // "https://billing2.sarkor.uz/payment/?type=json";
    //https://billing2.sarkor.uz/payment/?type=json
    std::string reqj = "{ \"paysys_login\" : \"" + m_acc.login 
                     + "\", \"paysys_pass\" : \"" + m_acc.password 
                     + "\", \"operation\" : 10, "
                       " \"client_id\": \"" + trans.param 
                     + "\", \"amount\" : " + to_str(trans.amount/100) 
                     + ", \"trans_id\" : " + to_str(trans.transaction_id) 
                     + "  }" ;

    const std::string & reqt = reqj;
    slog.DebugLog("Request: %s\n", reqt.c_str());
    
    std::string resp_t;
    {
        oson::network::http::request req_ = oson::network::http::parse_url(address);
        //host, port, path determined.
        
        req_.method          = "POST"; 
        req_.content.charset = "utf-8";
        req_.content.type    = "text/plain";
        req_.content.value   = reqt;
        
        
        resp_t =  sync_http_ssl_request(req_); 
    }
    
    slog.DebugLog("Response: %.*s\n", (::std::min)(2048, (int)resp_t.length()), resp_t.c_str());
    
    if (resp_t.empty())
        return Error_merchant_operation;

    namespace pt = boost::property_tree;
    pt::ptree respj;
    std::istringstream ss(resp_t);
    pt::read_json(ss, respj);
    
    
    response.merchant_status = respj.get< int >("status", 0);
    
    std::string message = respj.get< std::string>("message", "0");
    
    if (response.merchant_status  != 1) { // success
        response.merch_rsp = message;
        return Error_merchant_operation;
    }
    
    response.merchant_trn_id = message;
    response.merch_rsp = "Success";
    return Error_OK ;
 }

 
 Error_T Merchant_api_T::perform_kafolat_insurance(const Merch_trans_T& trans, Merch_trans_status_T& response)
 {
     SCOPE_LOG(slog);
     
     if (! trans.merch_api_params.count("ID") || ! trans.merch_api_params.count("NOTE1")) {
         //there no exist required parameters
         slog.WarningLog("There no exists required parameters");
         return Error_parameters;
     }
     
     std::string address      =   m_merchant.url                       ; //url: https://online.kafolat.uz:5080/online/ins/oson
     std::string id           =   trans.merch_api_params.at("ID")      ; // tel.nomer client's.
     std::string sum          =   to_str(trans.amount / 100 )          ; //in sum or tiyin, unknow yet.
     std::string pdate        =   formatted_time_now("%d.%m.%Y %H:%M") ; // buy date: 23.03.2018 15:22 format. 
     std::string note         =   to_str(trans.transaction_id)         ; // transaction number.
     std::string note1        =   trans.merch_api_params.at("NOTE1")   ; // tel.nomer register client's.
     std::string secret_key   =   m_acc.password ;//unknown yet
     std::string hash         =   oson::utils::md5_hash( id + sum + pdate + note + note1 + secret_key);
     
     
     oson::network::http::request req_ = oson::network::http::parse_url(address); // parsed: host, port, path 
     req_.method = "POST"; 
     req_.content.charset = "UTF-8";
     req_.content.type = "application/x-www-form-urlencoded" ;
     req_.content.value = "ID=" + id + "&SUM=" + sum + "&PDATE="+pdate + "&NOTE=" + note + "&NOTE1=" + note1 + "&HASH=" + hash ;
     
     std::string const& req_str = req_.content.value;
     slog.DebugLog("REQUEST: %s", req_str.c_str());
     
     std::string resp_str = sync_http_ssl_request(req_);
     slog.DebugLog("RESPONSE: %s", resp_str.c_str());
     
     if (resp_str.empty()){
         return Error_merchant_operation;
     }
     
     response.merchant_status = string2num(resp_str);
     switch(response.merchant_status)
     {
         case 0: response.merch_rsp = "Успешно проведено" ;break;
         case 1: response.merch_rsp = "HASH подпись не верно сформирован" ;break;
         case 2: response.merch_rsp = "Неправильный формат ID" ; break;
         case 3: response.merch_rsp = "Неправильный формат SUM"; break;
         case 4: response.merch_rsp = "Неправильный формат PDATE"; break;
         case 5: response.merch_rsp = "платёж с данным транзакционным номером уже введен" ;break;
         default: response.merch_rsp = "Unknown error"; break;
     }
     
     if (response.merchant_status != 0){
         return Error_merchant_operation;
     }
     
     return Error_OK ;
 }
 
 Error_T Merchant_api_T::perform_nanotelecom(const Merch_trans_T& trans, Merch_trans_status_T& response)
 {
     SCOPE_LOG(slog);
     if ( ! trans.merch_api_params.count("account") ){
         
         response.merchant_status = -1;
         response.merch_rsp = "Required 'account' field parameter not found!" ;
         return Error_internal;
     }
     std::string account = trans.merch_api_params.at("account");
     std::string sum   = to_str(trans.amount / 100.0, 2, false) ;
//     # curl -k -X GET 'https://91.240.12.128:8443/bgbilling/mpsexecuter/27/1/?command=pay&txn_id=27&account=I/S-01&sum=1000&txn_date=20180329181020' -H 'Authorization: Basic bmFubzpVc29oeGlyMA==' 
//<?xml version="1.0" encoding="UTF-8"?>
//<response>
//<result>0</result>
//<osmp_txn_id>27</osmp_txn_id>  // -> oson transaction_id
//<prv_txn>1</prv_txn>           // -> nanotelecom transaction_id
//<sum>1000.00</sum>
//<comment>Платеж принят на: I/S-01 (test jeka)</comment>
//</response>

     std::string address = m_merchant.url; //https://91.240.12.128:8443/bgbilling/mpsexecuter/27/1/?
     std::string request_str = address + "command=pay&txn_id=" + to_str(trans.transaction_id) + "&account=" +  account  + "&sum=" + sum + 
             "&txn_date=" + formatted_time_now("%Y%m%d%H%M%S") ;
     
     oson::network::http::request req_ = oson::network::http::parse_url(request_str);
     req_.method = "GET";
     req_.headers.push_back("Authorization: Basic " + authorization_base_auth(m_acc.login, m_acc.password)) ;
     
     slog.DebugLog("REQUEST: %s", request_str.c_str());
     
     std::string response_str = sync_http_ssl_request(req_);
     
     slog.DebugLog("RESPONSE: %s", response_str.c_str());
     
     if (response_str.empty()){
         return Error_merchant_operation;
     }
     
     namespace pt = boost::property_tree;
     
     pt::ptree root;
     std::stringstream ss(response_str);
     pt::read_xml(ss, root);
     
     response.merchant_status = root.get< int > ("response.result", -9999999 ) ;
     response.merch_rsp       = root.get< std::string > ("response.comment", "-.-");
     
     if ( response.merchant_status != 0 ) 
     {
         return Error_merchant_operation ;
     }
     
     response.merchant_trn_id = root.get< std::string > ("response.prv_txn", "0");
     
     return Error_OK;
 }
 
Error_T Merchant_api_T::get_webmoney_info(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
     namespace webmoney = ::oson::backend::merchant::webmoney;
     
     webmoney::request_t req;
     
     webmoney::response_t resp;
     
     webmoney::acc_t  acc;
     
     webmoney::manager_t mng( acc );
     
     
     if (trans.merch_api_params.count("purse") == 0){
         slog.WarningLog("there no purse !");
         return Error_merchant_operation;
     }
     
     req.payment.purse    = trans.merch_api_params.at("purse");
     
     boost::algorithm::trim(req.payment.purse); // remove spaces.
     
     if (req.payment.purse.empty()) {
         slog.WarningLog("purse is empty!");
         return Error_merchant_operation;
     }
     
     req.id = trans.transaction_id;
     req.wmid = "527265668883" ;
     req.sign.key_file_path = req.sign.key_file_from_config(); //"/etc/oson/CA/6696852.key";
     req.sign.type = 2;
     req.regn      = 0;
     req.payment.currency = "RUB" ;//"EUR" ;
     //{
         char const cur_let = req.payment.purse[0]; // there purse not empty!

         switch(cur_let)
         {
             case 'E': req.payment.currency = "EUR"; break;
             case 'R': req.payment.currency = "RUB"; break;
             case 'Z': req.payment.currency = "USD"; break;
             default: slog.WarningLog("purse is invalid!"); break;
         }
    // }
     req.test  = 0;
     Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load( Currency_info_T::Type_Uzb_CB );
     if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency!");
         return Error_merchant_operation;
     }
     //slog.DebugLog("ci.eur: %.8f", ci.usd_eur ) ;
     std::string currency_str = "0.0" ;
     req.payment.price = 0.0 ;
     
     //@Note: for webmoney we get commission from trans.amount.
     int64_t amount = trans.amount - trans.merchant.commission(trans.amount);
     std::string cur_symbol;
     double cur_course = 0.0;
     switch(cur_let)
     {
         case 'E': 
             req.payment.price = ci.eur(amount);  
             currency_str = /*"EUR  " +*/ to_str( ci.usd_eur, 4, true ) /*+ "€"*/; 
             cur_symbol = "\u20AC"; 
             cur_course = ci.usd_eur; 
             break;
             
         case 'R': 
             req.payment.price = ci.rub(amount);  
             currency_str = /*"RUB  " +*/ to_str( ci.usd_rub, 4, true ) /*+ "R"*/; 
             cur_symbol = " руб."; 
             cur_course = ci.usd_rub ;
             break;
             
         case 'Z': 
             req.payment.price = ci.usd(amount);  
             currency_str = /*"USD  " +*/ to_str( ci.usd_uzs, 4, true ) /*+ "$"*/; 
             cur_symbol = "$" ; 
             cur_course = ci.usd_uzs;
             break;
             
         default: break;
     }
     
     const double payment_price = req.payment.price;
     
     double const rounded_price = ::floor( payment_price * 100.0) / 100.0;
     slog.InfoLog("price: %.8f =>(rounded) => %.2f cur_course: %.12f", req.payment.price, rounded_price , cur_course );

     if (rounded_price < 1.0E-2 ) {
         slog.WarningLog("price is zero, set 0.01!");
         return Error_not_enough_amount ;
         
     }
     
     req.payment.price = rounded_price;
     
     resp = mng.check_atm(req);
 
     
     
     
     response.merchant_status = resp.retval ;
     response.merch_rsp       = resp.retdesc ;
     response.merchant_trn_id = resp.payment.wmtranid ;
    
     if (resp.retval != 0)
     {
        slog.WarningLog("resp.retval = %ld", resp.retval);

        //response.kv["currency"] = currency_str;
        response.kv["purse" ]   = req.payment.purse ;

        switch(resp.retval)
        {
            case -32: 
            case -38:
            {
                response.kv["oson_note"] = "Неверный номер кошелька." ;
            }
            break;

            case -700:
            case -711:
            {
                response.kv["oson_note"] = "Дневной лимит исчерпан." ;
            }
            break;
            case -811:
            case -821:
            {
                response.kv["oson_note"] = "Месячный лимит исчерпан." ;
            }
            break;
            default:
            {
                response.kv["oson_note"] = "Неверный номер кошелька ( код ош.ка:   " + to_str(resp.retval) + ")." ;
            }
            break;
        }
         
        return Error_OK ;
         //return Error_purchase_login_not_found;
     }
     
     std::string amount_credit, total_course;
     
     amount_credit = to_str(rounded_price, 2, false ) + cur_symbol ;
     
     //trans.amount - given in "TIYIN's, divide it to 100 for convert to 'SUM'.
     double tcourse =  ( trans.amount / 100.0)  / rounded_price ;
     
     total_course = to_str(tcourse, 2, true);
     
     //const bool allow_oson_kv = trans.uid < 200 ;
     
    // if (allow_oson_kv )
     {
        response.kv[ "oson_amount_credit"] = amount_credit ;
        response.kv[ "oson_total_course" ] = total_course  ;
        
//        boost::ignore_unused(cur_course);
     }
     
     response.kv[ "currency" ]       = currency_str;
     response.kv[ "purse"    ]       = resp.payment.purse ;
     response.kv[ "daily_limit"]     = to_str( resp.payment.limit.daily_limit, 4, true) + cur_symbol;
     response.kv[ "monthly_limit" ]  = to_str( resp.payment.limit.monthly_limit, 4, true) + cur_symbol;
     
     return Error_OK ;
 
}
 
 Error_T Merchant_api_T::check_webmoney(const Merch_trans_T& trans, Merch_check_status_T& status) 
 {
     SCOPE_LOG(slog);
     namespace webmoney = ::oson::backend::merchant::webmoney;
     
     webmoney::request_t req;
     
     webmoney::response_t resp;
     
     webmoney::acc_t  acc;
     
     webmoney::manager_t mng( acc );
     
     
     if (trans.merch_api_params.count("purse") == 0){
         slog.WarningLog("there no purse !");
         return Error_merchant_operation;
     }
     
     req.payment.purse    = trans.merch_api_params.at("purse");
     
     boost::algorithm::trim(req.payment.purse); // remove spaces.
     
     if (req.payment.purse.empty()) {
         slog.WarningLog("purse is empty!");
         return Error_merchant_operation;
     }
     
     req.id = trans.transaction_id;
     req.wmid = "527265668883" ;
     req.sign.key_file_path = req.sign.key_file_from_config(); //"/etc/oson/CA/6696852.key";
     req.sign.type = 2;
     req.regn      = 0;
     req.payment.currency = "RUB" ;//"EUR" ;
     //{
         char const cur_let = req.payment.purse[0]; // there purse not empty!

         switch(cur_let)
         {
             case 'E': req.payment.currency = "EUR"; break;
             case 'R': req.payment.currency = "RUB"; break;
             case 'Z': req.payment.currency = "USD"; break;
             default: slog.WarningLog("purse is invalid!"); break;
         }
    // }
     req.test  = 0;
     Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load(Currency_info_T::Type_Uzb_CB);
     if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency!");
         return Error_merchant_operation;
     }
     //slog.DebugLog("ci.eur: %.8f", ci.usd_eur ) ;
     
     req.payment.price = 0.0 ;
     switch(cur_let)
     {
         case 'E': req.payment.price = ci.eur(trans.amount);break;
         case 'R': req.payment.price = ci.rub(trans.amount);break;
         case 'Z': req.payment.price = ci.usd(trans.amount);break;
         default: break;
     }
     
     double  rounded_price = ::floor(req.payment.price * 100.0) / 100.0;
     slog.InfoLog("price: %.8f =>(rounded) => %.2f", req.payment.price, rounded_price );
     
     
     if (rounded_price < 1.0E-2 ) {
         slog.WarningLog("price is zero, set 0.01!");
         rounded_price = 0.01;
     }
     
     req.payment.price = rounded_price;
     
     resp = mng.check_atm(req);
     
     status.exist = ( 0 == resp.retval  ) ;
     status.status_value = resp.retval ;
     
     if (resp.retval != 0 )
     {
         switch(resp.retval )
         {
             case -32:
             case -38:
             {
                 status.status_text = "Неверный номер кошелька.";
                 break;
             }
             case -700:
             case -711:
             {
                 status.status_text = "Дневный лимит исчерпан." ;
                 status.notify_push = true;
                 status.push_text = resp.description + "\nПожалуйста, обратитесь к поставщику услуг."; 
                 break;
             }
             case -811:
             case -821:
             {
                 status.status_text = "Месячный лимит исчерпан." ; 
                 status.notify_push = true;
                 status.push_text   = resp.description + "\nПожалуйста, обратитесь к поставщику услуг.";
                 break;
             }
             default:
             {
                 status.status_text = "Код ошибка: " + to_str(resp.retval ) ;
                 break;
             }
         }
     }
     return Error_OK ;
 }
    
 Error_T Merchant_api_T::perform_webmoney(const Merch_trans_T& trans, Merch_trans_status_T& response)
 {
     SCOPE_LOG(slog);
     namespace webmoney = ::oson::backend::merchant::webmoney;
     
     webmoney::request_t req;
     
     webmoney::response_t resp;
     
     webmoney::acc_t  acc;
     
     webmoney::manager_t mng( acc );
     
     if (trans.merch_api_params.count("purse") == 0){
         slog.WarningLog("there no purse !");
         return Error_merchant_operation;
     }
     
     req.payment.purse    = trans.merch_api_params.at("purse");
     
     boost::algorithm::trim(req.payment.purse); // remove spaces.
     
     if (req.payment.purse.empty() ) {
         slog.WarningLog("purse is empty!");
         return Error_merchant_operation;
     }
     
     req.id = trans.transaction_id;
     req.wmid = "527265668883" ;
     req.sign.key_file_path = req.sign.key_file_from_config();//"/etc/oson/CA/6696852.key";
     req.sign.type = 2;
     req.regn      = 0;
     req.payment.currency =  "X" ;
     req.payment.exchange =  ""    ;
     req.test     = 0;
     req.payment.pspdate = formatted_time_now("%Y%m%d %H:%M:%S") ;
     req.payment.pspnumber = "787979878987" ;
     char const c_let = req.payment.purse[0];
     
     Currency_info_T ci   = oson::Merchant_api_manager::currency_now_or_load(Currency_info_T::Type_Uzb_CB);
     if ( ! ci.initialized ) {
         slog.WarningLog("Can't get currency info");
         return Error_merchant_operation;
     }
     req.payment.price    = 0.00 ;
     
     switch(c_let)
     {
         case 'R': /*rub*/ req.payment.currency = "RUB"; req.payment.price = ci.rub(trans.amount); break;
         case 'Z': /*usd*/ req.payment.currency = "USD"; req.payment.price = ci.usd(trans.amount); break;
         case 'E': /*eur*/ req.payment.currency = "EUR"; req.payment.price = ci.usd(trans.amount); break;
         default: slog.WarningLog("purse invalid!");break;
     }
     
     double const rounded_price = ::floor(req.payment.price * 100.0) / 100.0;
     slog.InfoLog("amount: %.2f, price: %.8f =>(rounded) => %.2f", trans.amount / 100.0 , req.payment.price, rounded_price );
     
     req.payment.price = rounded_price;
     std::time_t  before_timer = std::time(0);
     /************************************/
     resp = mng.pay_atm(req);
     /************************************/
     std::time_t  after_timer = std::time(0);
     int64_t diff_timer = (int64_t)after_timer - (int64_t)before_timer ;
     
     response.merchant_status = resp.retval ;
     response.merch_rsp       = resp.retdesc ;
     response.merchant_trn_id = to_str( resp.payment.wmtranid ) ;
     
     if (resp.retval != 0)
     {
         if (diff_timer >= 25 ) //timeout
         {
             slog.WarningLog("TIMEOUT");
             return Error_perform_in_progress;/**** REVERSE MUST BE ONLY RUCHNOY **/
         }
         slog.WarningLog("operation failed");
         
         return Error_merchant_operation ; 
     }
     
     return Error_OK ;
 }
 
 
 /*****************************************************************************************************************************/
 /************************************ UCELL  *********************************************************************************/
/******************************************************************************************************************************/    
Error_T Merchant_api_T::get_ucell_info(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    std::string phone;
    
    ///////////////////////////////////////////
    {
        auto const& pms = trans.merch_api_params;
        auto it_ph      = pms.find("clientid");

        if ( it_ph == pms.end() || (*it_ph).second.empty() ) {
            slog.WarningLog("clientid not found!");
            return Error_parameters;
        }

        phone = (*it_ph).second;
        
        //add 998
        if (phone.length() == 9) {
            phone = "998" + phone;
        }
    }
    /////////////////////////////////////////
    if (trans.uid > 200 ) // disable purchase-info for real users, because it's got timeout 40-50% situtation.
    {
        response.merchant_status = 0;
        response.kv["clientid"] = phone;
        return Error_OK ;
    }
    /////////////////////////////////////////
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = m_acc.login    ;
    ucell_acc.password    = m_acc.password ;
    ucell_acc.api_json    = m_acc.api_json ;
    ucell_acc.url         = m_merchant.url ;
    ucell_acc.merchant_id = m_merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    
    Ucell::response_t ucell_resp;
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    int r  =0 ;
    
    
    try
    {
        r = ucell_mng.info( ucell_req, ucell_resp);
    }
    catch(std::exception & e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        r = -9999;
    }
    
    
    if (r < 0)
    {
        slog.WarningLog("ucell check failed");
    
        response.merchant_status = 0;
        
        response.merch_rsp = ucell_resp.status_text;
        
        response.kv["clientid"] = phone;
        
        return Error_OK ;
    }
    
    
    response.merchant_status = ucell_resp.status_value;
    response.merch_rsp       = ucell_resp.status_text ;
    
    if ( std::abs( ucell_resp.available_balance ) > 0.5E-4 )
    {
        response.kv["AvailableBalance"] = "$" + to_str( ucell_resp.available_balance, 4, false );
    }
    else
    {
        response.kv["AvailableBalance"] = "$0.00" ;
    }
    
    response.kv["StateValue" ]      = ucell_resp.status_text ;
    response.kv["clientid"]         = phone;
    
    return Error_OK ;
}

Error_T Merchant_api_T::check_ucell(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOG(slog);
    
    std::string phone;
    
    status.exist = false;
    ///////////////////////////////////////////
    {
        auto const& pms = trans.merch_api_params;
        auto it_ph      = pms.find("clientid");

        if ( it_ph == pms.end() || (*it_ph).second.empty() ) {
            slog.WarningLog("clientid not found!");
            return Error_parameters;
        }

        phone = (*it_ph).second;
    }
    /////////////////////////////////////////
    
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = m_acc.login    ;
    ucell_acc.password    = m_acc.password ;
    ucell_acc.api_json    = m_acc.api_json ;
    ucell_acc.url         = m_merchant.url ;
    ucell_acc.merchant_id = m_merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    
    Ucell::response_t ucell_resp;
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    int r = ucell_mng.check( ucell_req, ucell_resp);
    if (r < 0){
        slog.WarningLog("ucell check failed");
        return Error_parameters;
    }
    
    status.exist = ( 0 == ucell_resp.status_value )  ;
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_ucell(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    std::string phone;
    {
        auto const& mps = trans.merch_api_params;
        auto it = mps.find("clientid");
        if (it == mps.end() || (*it).second.empty() ) 
        {
            slog.WarningLog("There no clientid parameter!");
            return Error_parameters;
        }
        phone = (*it).second;
    }
    
    
    namespace Ucell = ::oson::backend::merchant::Ucell;
    Ucell::acc_t ucell_acc;
    
    ucell_acc.login       = m_acc.login    ;
    ucell_acc.password    = m_acc.password ;
    ucell_acc.api_json    = m_acc.api_json ;
    ucell_acc.url         = m_merchant.url ;
    ucell_acc.merchant_id = m_merchant.id  ;
    
    
    Ucell::request_t ucell_req ;
    ucell_req.clientid  = phone;
    ucell_req.trn_id    = trans.transaction_id;
    ucell_req.amount    = trans.amount;
    
    Ucell::response_t ucell_resp;
    
    Ucell::manager_t ucell_mng ( ucell_acc );
    
    int r = ucell_mng.pay( ucell_req, ucell_resp);
    if (r < 0){
        slog.WarningLog("ucell check failed");
        return Error_parameters;
    }
     
    
    response.merchant_status = ucell_resp.status_value;
    response.merch_rsp       = ucell_resp.status_text ;
    response.merchant_trn_id = ucell_resp.provider_trn_id;
    response.ts              = ucell_resp.timestamp ;
    
    
    if (ucell_resp.status_value != 0 ) {
        slog.WarningLog("status is not zero!");
        return Error_merchant_operation;
    }
    
    response.merch_rsp += " " + response.ts; 
    
    return Error_OK ;
}
/********************************************************************************/

Error_T Merchant_api_T::get_tps_info(const Merch_trans_T& trans,  Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    
    const auto& merch_param = trans.merch_api_params ;
    
    
    if ( ! merch_param.count("account")  || ! merch_param.count("account_type"))
    {
        slog.WarningLog("TPS account not found!");
        return Error_parameters;
    }
    
    namespace tps = oson::backend::merchant::tps ;
    
    tps::request_t tps_req;
    tps_req.account      = merch_param.at("account");
    tps_req.account_type = merch_param.at("account_type");
    tps_req.trn_id       = trans.transaction_id  ;
    tps_req.summ         = trans.amount;
    
    tps::access_t tps_acc;
    tps_acc.username =  m_acc.login;
    tps_acc.password =  m_acc.password;
    tps_acc.url      =  m_merchant.url ;
    
    tps::manager_t  tps_manager( tps_acc ) ;
    tps::response_t resp = tps_manager.info( tps_req ) ;
    
    if (resp.result != 0)
    {
        slog.ErrorLog("TPS info failed with error: %lld", (long long)resp.result);
        return Error_merchant_operation;
    }
    ///////////////////////////////
    response.merchant_status = resp.result   ;
    response.merch_rsp       = resp.out_text ;
    response.ts              = resp.ts       ;
    response.merchant_trn_id = resp.oper_id  ;

//    "acc_saldo": "-42099.01",
//    "subject_name": "Юсупов Бахром Бахтиёрович"

    response.kv["acc_saldo"]    = resp.acc_saldo;
    response.kv["subject_name"] = resp.subject_name ;
    response.kv["login"]        = tps_req.account;
    return Error_OK ;
}

Error_T Merchant_api_T::check_tps(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOGD(slog);
    
    const auto& merch_param = trans.merch_api_params;
    
    if ( ! merch_param.count("account") )
    {
        slog.WarningLog("TPS account not found!");
        return Error_parameters;
    }
    
    
    
    namespace tps = oson::backend::merchant::tps;
    
    tps::request_t tps_req;
    tps_req.account =  merch_param.at("account");
    tps_req.trn_id  =  ( trans.transaction_id ) ;
    tps_req.summ    =  trans.amount;
    
    tps::access_t tps_acc;
    tps_acc.username =  m_acc.login ;
    tps_acc.password =  m_acc.password ;
    tps_acc.url      =  m_merchant.url ;
    
    tps::manager_t tps_manager(tps_acc);
    
    tps::response_t resp = tps_manager.check(tps_req);
    
    ///////////////////////////////////////
    
    status.exist        = ( 0 == resp.result ) ; 
    status.status_value = resp.result;
    status.status_text  = resp.out_text ;
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_tps(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    const auto& merch_param = trans.merch_api_params ;
    if ( ! merch_param.count("account") || ! merch_param.count("account_type") )
    {
        slog.WarningLog("TPS account not found!");
        return Error_parameters;
    }
    
    namespace tps = oson::backend::merchant::tps ;
    
    tps::request_t tps_req;
    tps_req.account  = merch_param.at("account");
    tps_req.account_type = merch_param.at("account_type");
    tps_req.trn_id   = ( trans.transaction_id );
    tps_req.summ     = trans.amount;
    tps_req.date     = formatted_time_now("%Y%m%d%H%M%S"); //date 20180409121000  --> YYYYMMDDHHMMSS
    
    tps::access_t tps_acc;
    tps_acc.username =  m_acc.login;
    tps_acc.password =  m_acc.password;
    tps_acc.url      =  m_merchant.url ;
    
    tps::manager_t tps_manager(tps_acc);
    
    tps::response_t resp = tps_manager.pay(tps_req);
    
    ///////////////////////////////////////
    response.merchant_status = resp.result ;
    response.ts              = resp.ts     ;
    response.merchant_trn_id = resp.oper_id ;
    response.merch_rsp       = resp.out_text ;
    
    if (resp.result != 0 )
    {
        slog.ErrorLog("TPS pay failed, result = %lld", (long long)resp.result);
        return Error_merchant_operation;
    }
    
    return Error_OK ;
}
/*************************** QIWI WALLET **************************************/
Error_T Merchant_api_T::get_qiwi_info(const Merch_trans_T& trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    if ( ! trans.merch_api_params.count("account") ) {
        slog.ErrorLog("'account' parameter not found!");
        return Error_parameters;
    }
    
    const int iso_code =  string2num( m_merchant.extern_service ) ;
    
    slog.InfoLog("iso_code: %d", iso_code);
#if 0
    const auto ci_type = (iso_code == 643 /*RUB*/ || iso_code == 978 /*EURO*/ ) 
                         ? Currency_info_T::Type_Rus_CB 
                         : Currency_info_T::Type_Uzb_CB ; 
#endif 
    const auto ci_type = Currency_info_T::Type_Uzb_CB ;
    
    /*@Note:  There for RUB and EUR  used Russian CB currency,  and for USD  used Uzbekistan CB currency. */
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return Error_internal;
    }
    
    
    const int64_t amount = trans.amount ;
    const int64_t commission = m_merchant.commission(trans.amount);
    const int64_t remain_amount = amount - commission ;

    std::string ccy = "x" ;
    double cb_currency = 0.0;
    double amount_credit = 0;
    switch(iso_code)
    {
        case 643 : 
            amount_credit = ci.rub(remain_amount); 
            ccy = "RUB" ;
            cb_currency = ci.usd_rub ;
            break;
        case 840:
            amount_credit = ci.usd(remain_amount);
            ccy = "USD";
            cb_currency  = ci.usd_uzs ;
            break;
        case 978:
            amount_credit = ci.eur(remain_amount );
            ccy = "EUR";
            cb_currency = ci.usd_eur ;
            break;
        default: break;
    }
    
    slog.DebugLog("ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    response.kv["oson_amount_credit"] = to_str(amount_credit, 2, false );
    response.kv["currency"] = to_str(cb_currency, 2, true);
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password = m_acc.password ;
    qiwi_acc.terminal_id = m_acc.login ;
    qiwi_acc.url  = m_merchant.url ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.ccy     = ccy ;
    //qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
   // qiwi_req.amount  = ci.rub( trans.amount ) ;
    
    
    
    qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    qiwi_manager.check(qiwi_req, qiwi_resp ) ;
    
    if ( qiwi_resp.success() && qiwi_resp.status_value == 1 )
    {
        response.kv["login"] = qiwi_req.account;
        return Error_OK ;
    }
    
    return Error_purchase_login_not_found ;
}

Error_T Merchant_api_T::check_qiwi(const Merch_trans_T& trans, Merch_check_status_T& status ) 
{
    SCOPE_LOGD(slog);
    if ( ! trans.merch_api_params.count("account") ) {
        slog.ErrorLog("'account' parameter not found!");
        return Error_parameters;
    }
    
//    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load() ;
//    if ( ! ci.initialized ){
//        slog.ErrorLog("Can't take currency!");
//        return Error_internal;
//    }
//  
    const int iso_code = string2num( m_merchant.extern_service ) ;
    slog.InfoLog("iso_code: %d", iso_code);
    std::string ccy = "x";
    switch(iso_code)
    {
        case 643: 
            ccy = "RUB" ;
            break;
        case 840:
          //  amount_credit = ci.usd(remain);
            ccy = "USD";
            break;
        case 978:
           // amount_credit = ci.usd(remain_amount);
            ccy = "EUR";
            break;
        default:
            break;
    }
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password = m_acc.password ;
    qiwi_acc.terminal_id = m_acc.login ;
    qiwi_acc.url  = m_merchant.url ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.ccy     = ccy ;
    //qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    //qiwi_req.amount  = ci.rub( trans.amount ) ;
    
    
    
    qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    qiwi_manager.check(qiwi_req, qiwi_resp ) ;
    
    if ( qiwi_resp.success() && qiwi_resp.status_value == 1 )
    {
        status.exist = true ;
        return Error_OK ;
    }
    
    
    status.exist = false;
    status.status_value = qiwi_resp.status_value ;
    status.status_text  = "Номер не существует" ;
    
    return Error_purchase_login_not_found ;
    
}

Error_T Merchant_api_T::perform_qiwi(const Merch_trans_T& trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    
    if ( ! trans.merch_api_params.count("account") ) {
        slog.ErrorLog("'account' parameter not found!");
        return Error_parameters;
    }
    
    const int iso_code = string2num( m_merchant.extern_service ) ;
#if 0
    const auto ci_type = (iso_code == 643 /*RUB*/ || iso_code == 978 /*EURO*/ ) 
                         ? Currency_info_T::Type_Rus_CB 
                         : Currency_info_T::Type_Uzb_CB ; 
#endif
    const auto ci_type = Currency_info_T::Type_Uzb_CB ; // always use uzb CB course.
    
    //@Note: There need Russian Central Bank  Currency for RUB and EUR.  FOR USD - used UZbekistan Central Bank Currency.
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized ){
        slog.ErrorLog("Can't take currency!");
        return Error_internal;
    }
    
    
    std::string ccy             = "x";
    const int64_t amount        = trans.amount;
    const int64_t commission    = 0;//@Note commission already subtracted. m_merchant.commission(amount);
    const int64_t remain_amount = amount - commission;
    double amount_credit= 0;
    switch(iso_code)
    {
        case 643:
            ccy = "RUB";
            amount_credit = ci.rub( remain_amount );
            break;
        case 840:
            ccy = "USD";
            amount_credit = ci.usd(remain_amount);
            break;
        case 978:
            ccy = "EUR";
            amount_credit = ci.eur(remain_amount);
            break;
        default:
            slog.WarningLog("Unexpected iso_code: %d", iso_code);
            return Error_parameters;
            //break;
    }
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    slog.DebugLog(" iso-code: %d ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", 
            iso_code, ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password    = m_acc.password ;
    qiwi_acc.terminal_id = m_acc.login    ;
    qiwi_acc.url         = m_merchant.url ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.ccy     = ccy ;
    qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    qiwi_req.amount  = amount_credit;
    
    
    
    qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    qiwi_manager.pay(qiwi_req, qiwi_resp ) ;
    
    
    response.ts               = qiwi_resp.txn_date ;
    response.merchant_status  = qiwi_resp.result_code ;
    response.merchant_trn_id  = qiwi_resp.txn_id ;
    response.merch_rsp        = qiwi_resp.status_text ;
    
    if (response.merchant_trn_id.empty() ) 
    {
        response.merchant_trn_id = "0";
    }
    
    slog.DebugLog("qiwi_resp:{ result-code: %ld, txn-id: %s, status_text: %s, fatal-error: %d, final-status: %d }", 
            qiwi_resp.result_code, 
            qiwi_resp.txn_id.c_str(), 
            qiwi_resp.status_text.c_str(),
            (int)qiwi_resp.fatal_error,
            (int)qiwi_resp.final_status
    ) ;
    
    Error_T ec = Error_OK ;
    if ( qiwi_resp.success() && qiwi_resp.fatal_error == false && qiwi_resp.final_status == true ) 
    {
        slog.DebugLog(" Error-OK ") ;
        ec =  Error_OK ;
    } 
    else if ( qiwi_resp.final_status == false ) {
        slog.WarningLog(" Error-perform-in-progress ") ;
        ec =  Error_perform_in_progress ;
    } 
    else // error and this is final status
    {
        slog.WarningLog(" Error-merchant-operation " ) ;
        ec =  Error_merchant_operation ;
    }
    
    
    return ec;
}

Error_T Merchant_api_T::pay_status_qiwi(const Merch_trans_T& trans,  Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    
    if ( ! trans.merch_api_params.count("account") ) {
        slog.ErrorLog("'account' parameter not found!");
        return Error_parameters;
    }
    
//    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load() ;
//    if ( ! ci.initialized ){
//        slog.ErrorLog("Can't take currency!");
//        return Error_internal;
//    }
//    
    namespace qiwi = oson::backend::merchant::QiwiWallet ;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password    = m_acc.password ;
    qiwi_acc.terminal_id = m_acc.login    ;
    qiwi_acc.url         = m_merchant.url ;
    
    qiwi::request_t qiwi_req;
    //qiwi_req.ccy     = "RUB" ;
    qiwi_req.trn_id  = trans.transaction_id ;
    qiwi_req.account = trans.merch_api_params.at("account") ;
    //qiwi_req.amount  = ci.rub( trans.amount ) ;
    
    
    
    qiwi::response_t qiwi_resp;
    qiwi::manager_t qiwi_manager(qiwi_acc);
    
    qiwi_manager.status( qiwi_req, qiwi_resp ) ;
    
    
    response.ts               = qiwi_resp.txn_date ;
    response.merchant_status  = qiwi_resp.result_code ;
    response.merchant_trn_id  = qiwi_resp.txn_id ;
    response.merch_rsp        = qiwi_resp.status_text ;
    
    if ( qiwi_resp.success() && qiwi_resp.fatal_error == false && qiwi_resp.final_status == true ) 
    {
        return Error_OK ;
    } else if ( qiwi_resp.final_status == false ) {
        return Error_async_processing ;
    } else { // error  and this is final status
        return Error_merchant_operation ;
    }
}

Error_T Merchant_api_T::get_qiwi_balance(const Merch_trans_T& trans,  Merch_trans_status_T& status)
{
    SCOPE_LOG(slog);
    
    namespace qiwi = oson::backend::merchant::QiwiWallet;
    
    qiwi::acc_t qiwi_acc;
    
    qiwi_acc.password     = m_acc.password ;
    qiwi_acc.terminal_id  = m_acc.login    ;
    qiwi_acc.url          = m_acc.url      ;
    
    qiwi::request_t qiwi_req;
    qiwi_req.trn_id = trans.transaction_id;
    qiwi_req.account  = "-";
    
    
    qiwi::response_t qiwi_resp;
    
    qiwi::manager_t  qiwi_manager(qiwi_acc ) ;
    
    qiwi_manager.balance(qiwi_req,  qiwi_resp ) ;
    
    //status.kv["QIWI-BALANCE-RESPONSE"] = qiwi_resp.raw_data ;
    //status.kv["Balance( 643 ) RUB"] = to_str(qiwi_resp.balance.rub, 2, false ) + " R " ;
    status.kv["Balance( 840 ) USD"] = to_str(qiwi_resp.balance.usd, 2, false ) + " $ " ;
    //status.kv["Balance( 978 ) EUR"] = to_str(qiwi_resp.balance.eur, 2, false ) + " \u20AC ";
//    
//    status.merchant_status = qiwi_resp.status_value ;
//    status.merch_rsp       = qiwi_resp.status_text  ;
    
    
    return Error_OK ;
}

/***************************** nativepay **************************************************/
Error_T Merchant_api_T::get_nativepay_info(const Merch_trans_T & trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    
    if ( ! trans.merch_api_params.count("account" ) ) {
        slog.ErrorLog("account  parameter not found");
        return Error_parameters;
    }
    std::string account = trans.merch_api_params.at("account");
    
    response.kv["account"] = account ;
    
    
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load(Currency_info_T::Type_Uzb_CB ) ;
    if ( ! ci.initialized ) {
        slog.ErrorLog("Can't get currency ! " ) ;
        return Error_internal;
    }
    
    const int64_t amount = trans.amount  ;
    const int64_t commission = m_merchant.commission(amount);
    const int64_t remain_amount = amount - commission ;
    
    const double amount_credit = ::std::floor( ci.usd(remain_amount) * 100.0 ) / 100.0 ;
    
    slog.InfoLog("amount: %ld, commission: %ld, remain-amount: %ld, amount-credit: %.2f, ccy: USD  rate: %.2f",
                 amount, commission, remain_amount, amount_credit, ci.usd_uzs ) ;
    
    //response.kv["oson_amount_credit"] = to_str(amount_credit, 2, false);
    //response.kv["currency"] = to_str(ci.usd_uzs, 2, false ) ;
    
    namespace nativepay = oson::backend::merchant::nativepay ;
    
    nativepay::acc_t  np_acc;
    np_acc.url = m_merchant.url ;
    
    nativepay::request_t  np_req          ;
    np_req.account   =  account           ;
    np_req.service   =  trans.service_id  ;
    np_req.sum       =  amount_credit     ;
    //np_req.txn_id    = trans.transaction_id ;
    //np_req.txn_date  = 
    
    
    nativepay::response_t np_resp;
    
    nativepay::manager_t  np_manager(np_acc ) ;
    
    np_manager.check(np_req, np_resp ) ;
    
    if (np_resp.status != 0 ) {
        slog.ErrorLog("Connection failed: status = %d, msg: %s\n", np_resp.status, np_resp.status_text.c_str() ) ;
        return Error_merchant_operation;
    }
    
    response.merchant_status = np_resp.result ;
    response.merchant_trn_id = np_resp.prv_txn;
    response.merch_rsp       = np_resp.comment ;
    
    if ( fabs(np_resp.rate) < 1.0E-6 ) {
        np_resp.rate     = ci.usd_uzs ;
        np_resp.amount   = np_req.sum ;
        np_resp.currency = "USD";
    }
    
    response.kv["currency"]            = to_str( np_resp.rate, 2, false) +   np_resp.currency ;
    response.kv["oson_amount_credit"]  = to_str( np_resp.amount, 2, false ) ;
    response.kv["comment"]             = np_resp.comment ;
    
//    if  (  ! ( np_resp.result == 0 || np_resp.result == 1 ) ) {
//        
//    }   
    
    return Error_OK ;
}

Error_T Merchant_api_T::check_nativepay(const Merch_trans_T& trans, Merch_check_status_T& status ) 
{
    SCOPE_LOGD(slog);
    if ( ! trans.merch_api_params.count("account" ) ) {
        slog.ErrorLog("account  parameter not found");
        return Error_parameters;
    }
    
    std::string account = trans.merch_api_params.at("account");
    
    namespace nativepay = oson::backend::merchant::nativepay ;
    
    nativepay::acc_t  np_acc;
    np_acc.url = m_merchant.url ;
    
    nativepay::request_t  np_req;
    np_req.account   =  account;
    np_req.service   =  trans.service_id    ;
    //np_req.txn_id    = trans.transaction_id ;
    //np_req.txn_date  = 
    
    
    nativepay::response_t np_resp;
    
    nativepay::manager_t  np_manager(np_acc ) ;
    
    np_manager.check(np_req, np_resp ) ;
    
    if (np_resp.status != 0 ) {
        slog.ErrorLog("Connection failed: status = %d, msg: %s\n", np_resp.status, np_resp.status_text.c_str() ) ;
        return Error_merchant_operation;
    }
    
    status.exist         = ( np_resp.result == 0 || np_resp.result == 1 ) ;
    status.status_value  = np_resp.result;
    status.status_text   = np_resp.comment ;
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    
    
    if ( ! trans.merch_api_params.count("account" ) ) {
        slog.ErrorLog("account  parameter not found");
        return Error_parameters;
    }
    
    std::string account = trans.merch_api_params.at("account");
    
    namespace nativepay = oson::backend::merchant::nativepay ;
    
    nativepay::acc_t  np_acc;
    np_acc.url = m_merchant.url ;
    
    nativepay::request_t  np_req;
    np_req.account   =  account;
    np_req.service   =  trans.service_id   ;
    np_req.txn_id    =  trans.transaction_id ;
    np_req.txn_date  =  formatted_time_now("%Y%m%d%H%M%S") ;
    np_req.sum       =  trans.amount ;
    
    nativepay::response_t np_resp;
    
    nativepay::manager_t  np_manager(np_acc ) ;
    
    np_manager.pay(np_req, np_resp ) ;
    
    
    if (np_resp.status != 0 ) {
        slog.ErrorLog("connection failed: np_resp=%d  msg: %s", np_resp.status, np_resp.status_text.c_str() ) ;
        response.merchant_status = np_resp.status;
        response.merch_rsp       = np_resp.status_text ;
        return Error_merchant_operation;
    }
    
    response.merchant_trn_id = np_resp.prv_txn ;
    response.merchant_status = np_resp.result  ;
    response.merch_rsp       = np_resp.comment ;
    
    if ( np_resp.result == 0 ) {
        slog.InfoLog("  =====  result = 0.  PERFROM SUCCESS === ... " ) ;
        return Error_OK ;
    } else {
        slog.WarningLog(" ==== result = %d   PERFORM IN PROGRESS === ... ", np_resp.result ) ;
        return Error_perform_in_progress ;
    }
     
}

Error_T Merchant_api_T::balance_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& status)
{
    SCOPE_LOGD(slog);
    
    return Error_OK ;
}

Error_T Merchant_api_T::pay_status_nativepay(const Merch_trans_T& trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOGD(slog);
    
    return Error_OK ;
}
/********************************  UMS (MTS) ********************************************/    

Error_T Merchant_api_T::get_info_ums(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);
    
    namespace MTS = oson::backend::merchant::MTS;
    
    MTS::request_t   mts_req;
    mts_req.check_id   = trans.check_id;
    mts_req.trn_id     = trans.transaction_id ;
    mts_req.phone      = "998" + trans.param;
    mts_req.espp_type  = MTS::request_t::ESPP_0104010 ;
    mts_req.sum        = trans.amount/ 100.00 ;
    mts_req.ts         = formatted_time_now_iso_T() ;
    
    MTS::response_t  mts_resp;

    MTS::acc_t       mts_acc;
    
    mts_acc.url = m_acc.url;//"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher"  ;
    mts_acc.load_cert_from_config();
    
    
    MTS::manager_t   mts_manager(mts_acc);
    
    mts_manager.send_request(mts_req, mts_resp);
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204010 ) {
        response.kv["login"]        = prettify_phone_number_uz(mts_req.phone);
        response.kv["ums_operator"] = mts_resp.name_operator;
        response.kv["ums_user_info"] = mts_resp.user_fio ;
        response.kv_raw              = mts_resp.raw;
    }
    else if (mts_resp.espp_type == MTS::response_t::ESPP_2204010 )
    {
        response.kv["ums_operator"] = mts_resp.desc_reject;
        response.kv_raw = mts_resp.raw;
    } else {
        slog.ErrorLog("Unexpected result!");
        return Error_merchant_operation;
    }
    
    return Error_OK ;
}


Error_T Merchant_api_T::check_ums(const Merch_trans_T& trans, Merch_check_status_T& status)
{
    SCOPE_LOGD(slog);
    
////    10.160.18.195  -- UMS TEST address.
////10.160.18.196  -- UMS Production address.
////
////test phone: 998973341649
////            998971579564
////	    998974468675
////            998974426487
////	    998977195247
//    
    namespace MTS = oson::backend::merchant::MTS ;
    
    if ( trans.info_detail.oson_tr_id != 0 ) 
    {
        MTS::response_t mts_resp;
        MTS::acc_t mts_acc;
        MTS::manager_t mts_manager(mts_acc);
        mts_manager.from_xml(trans.info_detail.json_text, mts_resp);
        status.exist = mts_resp.espp_type == MTS::response_t::ESPP_1204010 ;
        return Error_OK ;
    }

    status.exist = false;
    
    
    MTS::request_t   mts_req;
    mts_req.check_id   = trans.check_id;
    mts_req.trn_id     = trans.transaction_id ;
    mts_req.phone      = "998" + trans.param;
    mts_req.espp_type  = MTS::request_t::ESPP_0104010 ;
    mts_req.sum        = trans.amount/ 100.00 ;
    mts_req.ts         = formatted_time_now_iso_T() ;;
    
    MTS::response_t  mts_resp;

    MTS::acc_t       mts_acc;
    
    mts_acc.url = m_acc.url ;//"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher"  ;
    mts_acc.load_cert_from_config();
    
    MTS::manager_t   mts_manager(mts_acc);
    
    mts_manager.send_request(mts_req, mts_resp);
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204010 ) {

        status.exist = true;
        
        //@Note this isn't safe for C++. save raw output to trans.
        Purchase_details_info_T * p_info = const_cast< Purchase_details_info_T*>( std::addressof(trans.info_detail));
        p_info->json_text  = mts_resp.raw;
        p_info->oson_tr_id = 1;
        p_info->trn_id     = trans.transaction_id;
    }
    else if (mts_resp.espp_type == MTS::response_t::ESPP_2204010 )
    {
        status.exist =false;
    } else {
        slog.ErrorLog("Unexpected result!");
        status.exist = false;
        return Error_merchant_operation;
    }
    
    return Error_OK ;
}

#ifndef _GNU_SOURCE 
#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */
#endif 

#include <time.h>
#include <boost/algorithm/string/split.hpp>
#include <thread>

static std::string tz_my_location()
{
    time_t t = time(NULL);
    struct tm lt = {0};

    localtime_r(&t, &lt);

    char buf[64];
    char c = '+';//by default +

    long int h,m;

    long int val = lt.tm_gmtoff;

    if (val < 0)
    {
        c = '-';
        val = -val;
    }

    h = val / 3600 ;
    m = ( val % 3600) / 60 ;

    size_t z = snprintf(buf, 64, "%c%02ld:%02ld", c, h, m);

    return std::string((const char*)buf, z);
}


Error_T Merchant_api_T::perform_ums(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOGD(slog);

    
    namespace MTS = oson::backend::merchant::MTS;
    
    MTS::request_t   mts_req ;
    
    mts_req.check_id   = trans.check_id                ;
    mts_req.trn_id     = trans.transaction_id          ;
    mts_req.phone      = "998" + trans.param           ;
    mts_req.espp_type  = MTS::request_t::ESPP_0104090  ;
    mts_req.sum        = trans.amount/ 100.00          ;
    mts_req.ts         = formatted_time_now_iso_T()    ;
    mts_req.ts_tz      = formatted_time_now_iso_T() + tz_my_location();
    mts_req.raw_info   = trans.info_detail.json_text   ;
    
    MTS::response_t  mts_resp;

    MTS::acc_t       mts_acc;
    
    mts_acc.url = m_acc.url; //"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher"  ;
    mts_acc.load_cert_from_config();
    
    MTS::manager_t   mts_manager(mts_acc);
    
    mts_manager.send_request(mts_req, mts_resp);
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204090 )
    {
        response.merchant_trn_id = mts_resp.espp_trn_id ;
        response.merchant_status = 1;
        MTS::response_t tmp_resp;
        mts_manager.from_xml(mts_req.raw_info, tmp_resp);
        response.merch_rsp = mts_resp.code_operator + ";" + tmp_resp.oson_limit_balance + ";" + mts_resp.ts ;
        return Error_OK ;
    } 
    else if(mts_resp.espp_type == MTS::response_t::ESPP_2204090 ) 
    {
        slog.WarningLog("Purchase not allowed ! " ) ;
        response.merch_rsp = mts_resp.desc_reject;
        response.merchant_status = string2num( mts_resp.code_reject) ;
        return Error_merchant_operation;
    }
    else {
        slog.ErrorLog("No purchase perform allowed!");
        response.merch_rsp = mts_resp.raw;
        return Error_merchant_operation;
    }
    
}

Error_T Merchant_api_T::cancel_ums(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    if (! trans.merch_api_params.count("paynet_tr_id")){
        slog.WarningLog("not found 'paynet_tr_id' parameter!");
        return Error_parameters;
    }
    
    namespace MTS = oson::backend::merchant::MTS ;
    
    MTS::request_t mts_req;
    mts_req.espp_type = MTS::request_t::ESPP_0104213 ; //otmena
    mts_req.espp_trn_id = trans.merch_api_params.at("paynet_tr_id");
    mts_req.phone       = trans.user_phone ;
    mts_req.sum         = trans.amount  / 100.0 ; // in sum
    mts_req.ts_tz       = trans.ts ;
    
    if(mts_req.phone.length() == 9 ) 
        mts_req.phone = "998" + mts_req.phone;
    
    MTS::acc_t mts_acc;
    mts_acc.load_cert_from_config();
    mts_acc.login     = m_acc.login    ;
    mts_acc.password  = m_acc.password ;
    mts_acc.url       = m_acc.url      ;
    
    MTS::manager_t manager(mts_acc);
    
    MTS::response_t mts_resp;
    
    manager.send_request(mts_req,  mts_resp);
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204213) // success
    {
        slog.InfoLog("Success cancleded!");
        return Error_OK ;
    }
    else {
        return Error_merchant_operation;
    }
    
}

Error_T Merchant_api_T::pay_status_ums(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    if ( ! trans.merch_api_params.count("ums_espp_trn_id")){
        slog.WarningLog("'ums_espp_trn_id' parameter not found");
        return Error_parameters;
    }
    
    namespace MTS = oson::backend::merchant::MTS;
    
    MTS::request_t   mts_req ;
    
    mts_req.espp_type  = MTS::request_t::ESPP_0104085  ;
    mts_req.espp_trn_id = trans.merch_api_params.at("ums_espp_trn_id");
    MTS::response_t  mts_resp;

    MTS::acc_t       mts_acc;
    
    mts_acc.url                        = m_acc.url; //"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher"  ;
    mts_acc.load_cert_from_config();
    
    MTS::manager_t   mts_manager(mts_acc);
    
    mts_manager.send_request(mts_req, mts_resp);

    response.kv_raw          = mts_resp.raw    ;
    response.merchant_status = mts_resp.status ;
    response.ts              = mts_resp.ts     ;
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204085 ) 
    {
        response.merch_rsp = "Success" ;
        if (mts_resp.status == 3 ) {
            response.merch_rsp = "Успешно" ;
        } else if (mts_resp.status == 5 ) {
            response.merch_rsp = "Отменён" ;
        }
    } else {
        response.merch_rsp = "Failure";
    }
    
    return Error_OK ;
}

Error_T Merchant_api_T::sverka_ums( const Merch_trans_T& trans, Merch_trans_status_T& response )
{
    SCOPE_LOG(slog);
    std::string date = trans.param;
    
    Purchase_list_T list;
    ///////////////////////////////////////
    {
        Purchase_T table(oson_this_db);
        Purchase_search_T search ;
        
        search.mID       = m_merchant.id ;
        search.status    = TR_STATUS_SUCCESS;
        search.from_date = date ;
        search.to_date   = date ;
        search.flag_total_sum  = 1; // get total sum and count
        
        Sort_T sort( 0, 0  );
        Error_T ec;
        ec = table.list_admin(search, sort, list);
        if (ec) return ec;
    }
    //////////////////////////////////////////////
    namespace MTS = oson::backend::merchant::MTS;
    
    MTS::request_t   mts_req ;
    std::string from_date = date + "T00:00:00";
    std::string to_date   = date + "T23:59:59";
    mts_req.trn_id     = trans.transaction_id ;
    mts_req.espp_type  = MTS::request_t::ESPP_0104050  ;
    mts_req.reestr_b64 = mts_req.make_reestr_b64(list, from_date, to_date);
    
    MTS::response_t  mts_resp;

    MTS::acc_t       mts_acc;
    mts_acc.url         =  m_acc.url ; //"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher"  ;
    mts_acc.load_cert_from_config();
    

    MTS::manager_t manager(mts_acc);
    
    manager.send_request(mts_req, mts_resp);

    if ( mts_resp.espp_type  == MTS::response_t::ESPP_1204050 ) 
    {
        response.merchant_trn_id = mts_resp.reestr_id ;
    } 
    else 
    {
        response.merch_rsp = mts_resp.desc_reject ;
        
        return Error_merchant_operation;
    }
    
    return Error_OK;
}

Error_T Merchant_api_T::sverka_ums_result(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    
    std::string sverka_id = trans.param;
    
    namespace MTS = oson::backend::merchant::MTS;
    MTS::request_t mts_req;
    mts_req.espp_type = MTS::request_t::ESPP_0104051 ;
    mts_req.espp_trn_id  =  sverka_id ;
    
    MTS::acc_t mts_acc;
    mts_acc.url = m_acc.url; //"https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher" ;
    mts_acc.load_cert_from_config() ;
    
    MTS::manager_t manager(mts_acc);
    
    MTS::response_t  mts_resp;
    
    manager.send_request(mts_req,  mts_resp);
    
    if (mts_resp.espp_type == MTS::response_t::ESPP_1204051 )
    {
        response.merch_rsp = mts_resp.raw ;
        response.merchant_trn_id = mts_resp.reestr_id;
        response.kv_raw   = mts_resp.reestr_res ;
        //if (mts_resp.reestr_res)
        return Error_OK ;
    }
    else
    {
        response.merch_rsp = mts_resp.desc_reject;
        return Error_merchant_operation;
    }
    
}
////////////////////////////////////////////////////////////////////////////////
//////////////////// Hermes Garant ////////////////////////////////////////////
std::pair< Error_T, double >  Merchant_api_T::currency_convert_usd(int64_t amount)
{
    SCOPE_LOG(slog);
    Currency_info_T ci = this->get_currency_now_cb(Currency_info_T::Type_Uzb_CB);
    if(!ci.initialized)
        return std::make_pair(Error_internal, 0.0);
    
    double usd = ci.usd(amount);
    
    return std::make_pair(Error_OK, usd);
        
}

Error_T Merchant_api_T::get_info_hg(const Merch_trans_T& trans, Merch_trans_status_T& response ) 
{
    SCOPE_LOG(slog);
    
    if ( ! trans.merch_api_params.count("account"))
    {
        slog.ErrorLog("Not found 'account' parameter!");
        return Error_internal;
    }
    
    const auto ci_type = Currency_info_T::Type_Uzb_CB ;
    
    /*@Note:  There for RUB and EUR  used Russian CB currency,  and for USD  used Uzbekistan CB currency. */
    Currency_info_T ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return Error_internal;
    }
    
    
    const int64_t amount        = trans.amount ;
    const int64_t commission    = m_merchant.commission(trans.amount);
    const int64_t remain_amount = ::std::max< int64_t> ( 0, amount - commission) ;

    std::string ccy = "USD" ;
    double cb_currency = ci.usd_uzs ;
    double amount_credit = ci.usd(remain_amount);
    
    slog.DebugLog("ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    response.kv["amount"] = to_str(amount_credit, 2, false );
    response.kv["currency"] = to_str(cb_currency, 2, true);
    
    
    namespace hg = oson::backend::merchant::HermesGarant;
    
    
    hg::acc_t hg_acc;
    hg_acc.agent_id        = "21";
    hg_acc.agent_password  = "OsonUZ76";
    hg_acc.url             = "https://hgg.kz:8802" ;
    
    hg::request_t hg_req;
    hg_req.account    =  trans.merch_api_params.at("account");
    hg_req.amount     =  amount_credit;
    hg_req.currency   =  ccy;
    hg_req.service_id =  trans.service_id;
    hg_req.trn_id     =  trans.transaction_id;
    hg_req.ts         =  formatted_time_now_iso_S();
    
    hg::response_t hg_resp;
    
    hg::manager_t manager(hg_acc);

    manager.check(hg_req,  hg_resp);

    
    response.merch_rsp = hg_resp.message ;
    
    if ( ! hg::error_codes::is_final(hg_resp.resp_status ) )
    {
        slog.WarningLog("not final status!");
        return Error_merchant_operation;
    }
    
    if ( hg_resp.resp_status == hg::error_codes::success   ) {
        response.merchant_status = 0 ;
        response.merchant_trn_id = to_str( hg_resp.req_id  )    ;

        response.kv["account"]   = hg_req.account;  
    } else {

        response.merchant_status = -1;
        response.kv["account"] = hg_resp.message;
    }

    return Error_OK ;
}

Error_T Merchant_api_T::check_hg(const Merch_trans_T& trans, Merch_check_status_T& status ) 
{
    SCOPE_LOG(slog);
    
    if ( ! trans.merch_api_params.count( "account" ) )  
    {
        slog.ErrorLog( "'account' parameter not found." ) ;
        return Error_internal;
    }
    
    //Check if merch_api_params includes prefix param
    if ( trans.merch_api_params.count( "prefix" ) )  
    {
        slog.ErrorLog( "'prefix' parameter has prefix field" ) ;
        
        
    }
    
    
    const auto ci_type = Currency_info_T::Type_Uzb_CB ;
    
    const auto ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return Error_internal;
    }
    
    
    const int64_t amount        = trans.amount ;
    const int64_t commission    = m_merchant.commission(trans.amount);
    const int64_t remain_amount = ::std::max< int64_t >(0, amount - commission) ;

    std::string ccy = "USD";
    double amount_credit = ci.usd(remain_amount);
    
    slog.DebugLog("ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    
    namespace hg = oson::backend::merchant::HermesGarant;
    
    hg::acc_t hg_acc;
    hg_acc.agent_id        = "21";
    hg_acc.agent_password  = "OsonUZ76";
    hg_acc.url             = "https://hgg.kz:8802" ;
    
    hg::request_t hg_req;
    hg_req.account    =  trans.merch_api_params.at("account");
    hg_req.amount     =  amount_credit;
    hg_req.currency   =  ccy;
    hg_req.service_id =  trans.service_id;
    hg_req.trn_id     =  trans.transaction_id;
    hg_req.ts         =  formatted_time_now_iso_S();
    
    hg::response_t hg_resp;
    
    hg::manager_t manager(hg_acc);
    
    manager.check(hg_req,  hg_resp);
    
    status.exist = ( hg_resp.resp_status == hg::error_codes::success ) ;
    
    
    return Error_OK ;
}

Error_T Merchant_api_T::perform_hg(const Merch_trans_T& trans, Merch_trans_status_T& response)
{
    SCOPE_LOG(slog);
    if ( ! trans.merch_api_params.count( "account" ) )  
    {
        slog.ErrorLog( "'account' parameter not found." ) ;
        return Error_internal;
    }
    
    std::string account = trans.merch_api_params.at("account") ;
    
    const auto ci_type = Currency_info_T::Type_Uzb_CB ;
    
    const auto ci = oson::Merchant_api_manager::currency_now_or_load( ci_type ) ;
    if ( ! ci.initialized )
    {
        slog.ErrorLog("Can't take currency!");
        return Error_internal;
    }
    
    
    const int64_t amount        = trans.amount ;
    const int64_t commission    = m_merchant.commission(trans.amount);
    const int64_t remain_amount = ::std::max< int64_t >(0, amount - commission) ;

    std::string ccy = "USD";
    double amount_credit = ci.usd(remain_amount);
    
    slog.DebugLog("ccy: %s, amount: %lld, comission: %lld, remain-amount: %lld, amount-credit: %.12f ", ccy.c_str(), amount, commission, remain_amount, amount_credit ) ;
    
    
    amount_credit = ::std::floor(amount_credit * 100 ) / 100.0;
    
    
    
    namespace hg = oson::backend::merchant::HermesGarant;
    
    hg::acc_t hg_acc;
    hg_acc.agent_id        = "21";
    hg_acc.agent_password  = "OsonUZ76";
    hg_acc.url             = "https://hgg.kz:8802" ;
    
    hg::request_t hg_req;
    hg_req.account    =  account                    ;
    hg_req.amount     =  amount_credit              ;
    hg_req.currency   =  ccy                        ;
    hg_req.service_id =  trans.service_id           ;
    hg_req.trn_id     =  trans.transaction_id       ;
    hg_req.ts         =  formatted_time_now_iso_S() ;
    
    hg::response_t hg_resp;
    
    hg::manager_t manager(hg_acc);
    
    manager.pay(hg_req,  hg_resp);
    
    
    response.merchant_status = hg_resp.resp_status;
    response.merch_rsp       = hg_resp.message ;
    response.merchant_trn_id = to_str( hg_resp.req_id ) ;
    
    if (! hg::error_codes::is_final(hg_resp.resp_status ) ) 
    {
        slog.WarningLog("Payment in progress.");
        return Error_perform_in_progress;
    }
    
    return Error_OK ;
}

Error_T Merchant_api_T::get_hg_balance (const Merch_trans_T& trans,  Merch_trans_status_T& status)
{
    SCOPE_LOG(slog);
    
    namespace hg = oson::backend::merchant::HermesGarant;
    
    hg::acc_t hg_acc;
    hg_acc.agent_id        = m_acc.login    ; //"21";
    hg_acc.agent_password  = m_acc.password ; //"OsonUZ76";
    hg_acc.url             = m_acc.url      ; //"https://hgg.kz:8802" ;
    
    hg::request_t hg_req;
    hg_req.currency = "USD";
    
    hg::response_t hg_resp;
    
    hg::manager_t  manager(hg_acc);
    
    manager.balance(hg_req, hg_resp);
    
    if ( ! hg_resp.bc.list.empty() )
    {
        for( const auto& p: hg_resp.bc.list){
            status.kv[p.currency] = p.balance;
        }
    } else {
        status.kv["ERROR"] = hg_resp.message ;
    }
    return Error_OK ;
    
}
 ///=========================================================================

namespace Mplat = oson::backend::merchant::Mplat;

const char*  Mplat::cmd_type_str( Mplat::MplatCMD cmd)
{
    switch(cmd)
    {
        case CMD_STATUS : return "STATUS";
        case CMD_PAY    : return "PAY" ;
        case CMD_CHECK  : return "CHECK";
        case CMD_GET_BALANCE : return "BALANCE";
        case CMD_GET_PROVIDERS : return "PROVIDERS";
        case CMD_GROUP_PROVIDERS : return "GROUPPROVIDERS";
        case CMD_REGISTRY_PAYMENTS : return "REGISTRY";
    }
    return "Unknown";
}

std::string Mplat::make_signature_check_pay(const struct Mplat::acc_t& acc,  const std::string& account )
{
    //// Signature=SHA1(login+MD5(Password)+SHA512(account)+Sign)
    return ::oson::utils::sha1_hash( acc.login + acc.pwd_md5 +  ::oson::utils::sha512_hash( account ) + acc.sign );
}

std::string Mplat::make_signature_status( const struct Mplat::acc_t& acc, const std::string& txn)
{
    // Signature=SHA1(login+MD5(Password)+SHA512(txn)+Sign)
    return ::oson::utils::sha1_hash(acc.login + acc.pwd_md5 +  ::oson::utils::sha512_hash(txn) + acc.sign);
}

std::string Mplat::make_signature_others( const struct Mplat::acc_t& acc)
{
    static const char* const hashEmpty = "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e";
    
    // Signature=SHA1(login+MD5(Password)+hashEmpty+Sign)
    return ::oson::utils::sha1_hash(acc.login + acc.pwd_md5 + hashEmpty + acc.sign);
}

int Mplat::response_t::result_to_oson_error()const
{
    switch(this->result)
    {
        case 0: return Error_OK ;

        case 5:
        case 150: return Error_purchase_login_not_found;
        
        case 8 : 
        case 155:    return Error_provider_temporarily_unavailable;
        
        case 503: 
            return Error_parameters;

        default:
            return Error_merchant_operation;
    }
}

Mplat::manager_api::manager_api(const struct Mplat::acc_t& acc): acc_(acc) 
{
}

Mplat::manager_api::~manager_api()
{}

Error_T Mplat::manager_api::providerGroups(const struct Mplat::request_t& request, Mplat::provider_group_list_t& list)
{
    SCOPE_LOG(slog);
    
    std::string signature = request.header.signature;
    
    if (signature.empty())
        signature = make_signature_others(acc_);

    slog.DebugLog("signature: %s", signature.c_str());
    
//   <?xml version="1.0" encoding="utf-8" ?>    
//    <request>
//        <auth login="" password="" agent="" />
//        <body type="" />
//    </request>
    
    
    std::string xml_req;
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\" password=\"" + request.auth.password + "\" agent=\"" + request.auth.agent + "\" />\n";
    xml_req += "\t<body type=\"GROUPPROVIDERS\" />\n";
    xml_req += "</request>\n";
    
    slog.DebugLog("xml_req: %s\n", xml_req.c_str());
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back(  "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request(req);//c.body();
    }
    
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    
//    <r>
//        <group id="" name="" description="" icon="" typeLogo="" />
//        ..........................................................
//        <group id="" name="" description="" icon="" typeLogo="" />
//    </r>
    
    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("r");
        
        for(const pt::ptree::value_type& v : resp)
        {
            if (v.first == "group")
            {
                struct provider_group_t group;
                pt::ptree pt = v.second.get_child("<xmlattr>", pt::ptree());
                
                group.id           =  pt.get("id", "0") ;
                group.name         =  pt.get("name", "");
                group.description  =  pt.get("description", "");
                group.icon         =  pt.get("icon", "");
                group.typeLogo     =  pt.get("typeLogo", "");
                
                list.push_back(group);
            }
        }
        slog.DebugLog("Found %d groups", (int)list.size());
    }
    
    
    return Error_OK ;
}

Error_T Mplat::manager_api::providers(const struct Mplat::request_t& request, Mplat::provider_list_t& list)
{
    SCOPE_LOG(slog);
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_others(acc_);
    
    slog.DebugLog("signature: %s", signature.c_str());
//   <?xml version="1.0" encoding="utf-8" ?>    
//    <request>
//        <auth login="" password="" agent="" />
//        <body type="" />
//    </request>
    
    std::string xml_req;
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\" password=\"" + request.auth.password + "\" agent=\"" + request.auth.agent + "\" />\n";
    xml_req += "\t<body type=\"PROVIDERS\" />\n";
    xml_req += "</request>\n";
    
    slog.DebugLog("xml_req: %s\n", xml_req.c_str());
    
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back( "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request(req);//c.body();
    }
    
    
//    <r>
//        <provider id="" name="" currency="" regExp="" minSum="" maxSum="" header="" />
//        <provider id="" name="" currency="" regExp="" minSum="" maxSum="" header="" />
//        ...............................................................................
//        <provider id="" name="" currency="" regExp="" minSum="" maxSum="" header="" />
//    </r>

    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("r");
        
        for(const pt::ptree::value_type& v : resp)
        {
            if (v.first == "provider")
            {
                struct provider_t provider;
                pt::ptree pt = v.second.get_child("<xmlattr>", pt::ptree());
                
                provider.id       =  string2num( pt.get("id", "0") );
                provider.name     =  pt.get("name", "");
                provider.currency =  pt.get("currency", "");
                provider.regExp   =  pt.get("regExp", "");
                provider.minSum   =  string2num(pt.get("minSum", "0"));
                provider.maxSum   =  string2num(pt.get("maxSum", "0"));
                provider.header   =  pt.get("header", "");
                
                list.push_back(provider);
            }
        }
        
        slog.DebugLog("Found %d providers", (int)list.size());
    }
    
    return Error_OK ;
    
}

Error_T Mplat::manager_api::status(const struct Mplat::request_t& request, struct Mplat::response_t& response)
{
    SCOPE_LOG(slog);
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_status(acc_, request.body.txn);
    
    slog.DebugLog("signature: %s", signature.c_str());
    
//    <request>
//    <auth login="" password="" agent="" />
//    <body type="" txn="" />
//    </request>
    std::string xml_req;
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\" password=\"" + request.auth.password + "\" agent=\"" + request.auth.agent + "\" />\n";
    xml_req += "\t<body type=\"STATUS\" txn=\"" + request.body.txn + "\" />\n";
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %s\n", xml_req.c_str());
    
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back(  "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request(req);
    }
    slog.DebugLog("Response: %.*s\n", (::std::min)( 2048, static_cast< int >( xml_resp.length() ) ), xml_resp.c_str());
    //<r type="" result="" id="" txn="" message="" />
    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("r");
        pt::ptree attr = resp.get_child("<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            const std::string&  name = v.first ;
            std::string  value = v.second.data();
            
            if (name == "type")
                response.type = value;
            else if (name == "result")
                response.result = string2num(value);
            else if (name == "txn")
                response.txn = value;
            else if (name == "message")
                response.message = value;
            else if (name == "id")
                response.id = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
 
    return Error_OK ;
    
}

Error_T Mplat::manager_api::check(const struct Mplat::request_t& request, struct Mplat::response_t& response)
{
    SCOPE_LOG(slog);
        
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_check_pay(acc_, request.body.account);
    
    slog.DebugLog("signature: %s", signature.c_str());
    
//////////////<?xml version="1.0" encoding="utf-8" ?>    
//////////////<request>
//////////////  <auth login="" password="" agent="" />
//////////////  <body type="" service="" account="" date="" />
//////////////  <extra ev_name_1="" ev_name_2="" ev_name_N="" />
//////////////</request>
    
    std::string xml_req;
    
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" ;
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\"  password=\"" + request.auth.password + "\"  agent=\"" + request.auth.agent + "\" /> \n";
    xml_req += "\t<body type=\"CHECK\" ";
    //xml_req += "   id=\""       + request.body.id               ; 
    //xml_req +="\" currency=\"" + to_str(request.body.currency) ; 
    xml_req += " service=\""  + to_str(request.body.service) + "\" "  ; 
    xml_req += " account=\""  + request.body.account         + "\" " ;
    //xml_req += "\" amount=\""   + request.body.amount           ; 
    xml_req += " date=\""     + request.body.date            + "\" "  ; 
    xml_req += " /> \n" ;
    
    if ( ! request.extra.ev_s.empty() )
    {
        xml_req += "\t<extra ";
        for(size_t ix = 0, nx = request.extra.ev_s.size(); ix != nx;  ++ix)
        {
            
            xml_req += request.extra.ev_s[ix].first + "=\"" + request.extra.ev_s[ix].second + "\"  "; 
        }
        xml_req += "/>\n";
    }
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %s", xml_req.c_str());
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back(   "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request(req); 
    }
    
    slog.DebugLog("response: %s\n", xml_resp.c_str());
    
    if (xml_resp.empty())
        return Error_internal;
    /////////////////////////////////////////////////////////////////////////////////////////
    
    //<r type="" result="" currency="" client_rate="" message="" extra_N="" />
    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree attr = root.get_child("r.<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            const std::string&  name = v.first ;
            std::string  value = v.second.data();
            
            if (name != "message"){
                slog.DebugLog("name: %s\tvalue: %s", name.c_str(), value.c_str());
            } else {
                const std::string& m64 = /*oson::utils::encodebase64*/(value);
                slog.DebugLog("name: message\tvalue: %s", m64.c_str());
            }
            
            if (name == "type")
                response.type = value;
            else if (name == "result")
                response.result = string2num(value);
            else if (name == "currency")
                response.currency = value;
            else if (name == "client_rate")
                response.client_rate = value;
            else if (name == "message")
                response.message = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
    return Error_OK ;
}

Error_T Mplat::manager_api::balance(const struct Mplat::request_t& request, struct Mplat::response_t& response)
{
    SCOPE_LOG(slog);
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_others(acc_);
    
    slog.DebugLog("signature: %s", signature.c_str());
    
//    <request>
//    <auth login="" password="" agent="" />
//    <body type="" />
//    </request>
    
    std::string xml_req;
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\"  password=\"" + request.auth.password + "\"  agent=\"" + request.auth.agent + "\"  />\n";
    xml_req += "\t<body type=\"BALANCE\" />\n";
    xml_req += "</request>\n";
    
    
    slog.DebugLog("xml: %s", xml_req.c_str());
    
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back(     "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request(req); //c.body();
    }
    
    
//    <r>
//        <balance status="" currency="" credit="" balance="" />
//    </r>
    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree resp = root.get_child("r");
        resp = resp.get_child("balance");
        pt::ptree attr = resp.get_child("<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            std::string  name = v.first.data();
            std::string  value = v.second.data();
            
            if (name == "status")
                response.status = value;
            else if (name == "currency")
                response.currency = value;
            else if (name == "credit")
                response.credit = value;
            else if (name == "balance")
                response.balance = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
    
    return Error_OK ;
    
}

Error_T Mplat::manager_api::pay(const struct Mplat::request_t& request, struct Mplat::response_t& response)
{
    SCOPE_LOG(slog);
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_check_pay(acc_, request.body.account);
    
    slog.DebugLog("Signature: %s", signature.c_str());
    
//    <?xml version=\"1.0\" encoding=\"utf-8\" ?>
//    <request>
//        <auth login="" password="" agent="" />
//        <body type="" id="" currency="" service="" account="" amount="" date="" />
//        <extra ev_name_1="" ev_name_2="" ev_name_N="" />
//    </request>
   std::string xml_req;
    
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" ;
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\"  password=\"" + request.auth.password + "\"  agent=\"" + request.auth.agent + "\" /> \n";
    xml_req += "\t<body type=\"PAY\" "
               "   id = \""     + request.body.id               +
               "\" currency=\"" + to_str(request.body.currency) + 
               "\" service=\""  + to_str(request.body.service)  + 
               "\" account=\""  + request.body.account          + 
               "\" amount = \"" + request.body.amount           +
               "\" date=\""     + request.body.date             + 
               "\" /> \n" ;
    
    if ( ! request.extra.ev_s.empty() )
    {
        xml_req += "\t<extra ";
        for(size_t ix = 0, nx = request.extra.ev_s.size(); ix != nx;  ++ix)
        {
            xml_req += request.extra.ev_s[ix].first + "=\"" + request.extra.ev_s[ix].second + "\"  "; 
        }
        xml_req += "/>\n";
    }
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %.*s", (::std::min)(2048, static_cast< int >( xml_req.length() ) ), xml_req.c_str());
    
    
    std::string xml_resp;
    /////////////////////////////////////////////////////////////////////////////////////////
    {
        oson::network::http::request req = oson::network::http::parse_url(acc_.url);
        req.method          = "POST";
        req.headers.push_back( "Signature: " + signature ) ;
        req.content.charset = "utf-8";
        req.content.type    = "text/xml";
        req.content.value   = xml_req;

        xml_resp = sync_http_ssl_request( req );   //c.body();
    }
    
    //<r type="" result="" id="" txn="" message="" amount="" currency="" extra_N="" />
    slog.DebugLog("Response: %.*s", (::std::min)(2048, static_cast< int >( xml_resp.length()) ), xml_resp.c_str());
    
    if (xml_resp.empty())
        return Error_merchant_operation;
    
    //Parse xml_resp
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        
        pt::ptree attr = root.get_child("r.<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            const std::string&  name = v.first;
            std::string  value = v.second.data();
            
            if (name == "type")
                response.type = value;
            else if (name == "result")
                response.result = string2num(value);
            else if (name == "currency")
                response.currency = value;
            else if (name == "txn")
                response.txn = value;
            else if (name == "message")
                response.message = value;
            else if (name == "amount")
                response.amount = value;
            else if (name == "id")
                response.id = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
    
    
    
    return Error_OK ;
}

std::pair< std::string, std::string > Mplat::manager_api::make_status_xml(const struct Mplat::request_t& request)
{
    SCOPE_LOG(slog);
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_status(acc_, request.body.txn);
    
    slog.DebugLog("signature: %s", signature.c_str());
    
//    <request>
//    <auth login="" password="" agent="" />
//    <body type="" txn="" />
//    </request>
    std::string xml_req;
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n";
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\" password=\"" + request.auth.password + "\" agent=\"" + request.auth.agent + "\" />\n";
    xml_req += "\t<body type=\"STATUS\" txn=\"" + request.body.txn + "\" />\n";
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %s\n", xml_req.c_str());
    
    return std::make_pair( xml_req, signature ) ;
}

std::pair< std::string, std::string > Mplat::manager_api::make_check_xml(const struct request_t& request)
{
    SCOPE_LOG(slog);
        
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_check_pay(acc_, request.body.account);
    
    slog.DebugLog("signature: %s", signature.c_str());
    
//////////////<?xml version="1.0" encoding="utf-8" ?>    
//////////////<request>
//////////////  <auth login="" password="" agent="" />
//////////////  <body type="" service="" account="" date="" />
//////////////  <extra ev_name_1="" ev_name_2="" ev_name_N="" />
//////////////</request>
    
    std::string xml_req;
    
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" ;
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\"  password=\"" + request.auth.password + "\"  agent=\"" + request.auth.agent + "\" /> \n";
    xml_req += "\t<body type=\"CHECK\" ";
    //xml_req += "   id=\""       + request.body.id               ; 
    //xml_req +="\" currency=\"" + to_str(request.body.currency) ; 
    xml_req += " service=\""  + to_str(request.body.service) + "\" "  ; 
    xml_req += " account=\""  + request.body.account         + "\" " ;
    //xml_req += "\" amount=\""   + request.body.amount           ; 
    xml_req += " date=\""     + request.body.date            + "\" "  ; 
    xml_req += " /> \n" ;
    
    if ( ! request.extra.ev_s.empty() )
    {
        xml_req += "\t<extra ";
        for(size_t ix = 0, nx = request.extra.ev_s.size(); ix != nx;  ++ix)
        {
            
            xml_req += request.extra.ev_s[ix].first + "=\"" + request.extra.ev_s[ix].second + "\"  "; 
        }
        xml_req += "/>\n";
    }
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %s", xml_req.c_str());
    
    return std::make_pair( xml_req, signature ) ;
}

std::pair< std::string, std::string > Mplat::manager_api::make_pay_xml(const struct request_t & request ) 
{
    SCOPE_LOG(slog);
    
    std::string signature = request.header.signature;
    if (signature.empty())
        signature = make_signature_check_pay(acc_, request.body.account);
    
    slog.DebugLog("Signature: %s", signature.c_str());
    
//    <?xml version=\"1.0\" encoding=\"utf-8\" ?>
//    <request>
//        <auth login="" password="" agent="" />
//        <body type="" id="" currency="" service="" account="" amount="" date="" />
//        <extra ev_name_1="" ev_name_2="" ev_name_N="" />
//    </request>
    std::string xml_req;
    
    xml_req += "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n" ;
    xml_req += "<request>\n";
    xml_req += "\t<auth login=\"" + request.auth.login + "\"  password=\"" + request.auth.password + "\"  agent=\"" + request.auth.agent + "\" /> \n";
    xml_req += "\t<body type=\"PAY\" "
               "   id = \""     + request.body.id               +
               "\" currency=\"" + to_str(request.body.currency) + 
               "\" service=\""  + to_str(request.body.service)  + 
               "\" account=\""  + request.body.account          + 
               "\" amount = \"" + request.body.amount           +
               "\" date=\""     + request.body.date             + 
               "\" /> \n" ;
    
    if ( ! request.extra.ev_s.empty() )
    {
        xml_req += "\t<extra ";
        for(size_t ix = 0, nx = request.extra.ev_s.size(); ix != nx;  ++ix)
        {
            xml_req += request.extra.ev_s[ix].first + "=\"" + request.extra.ev_s[ix].second + "\"  "; 
        }
        xml_req += "/>\n";
    }
    xml_req += "</request>\n";
    
    slog.DebugLog("Request: %.*s", (::std::min)(2048, static_cast< int >( xml_req.length() ) ), xml_req.c_str());
    
    return std::make_pair(xml_req, signature);
}
    

void Mplat::manager_api::async_http_req(const std::string& xml_req, const std::string& signature, std::function< void(const std::string&, const boost::system::error_code& ) > handler ) 
{
    SCOPE_LOG(slog);
    auto http_req = oson::network::http::parse_url(acc_.url);
    http_req.method          = "POST";
    http_req.headers.push_back(  "Signature: " + signature ) ;
    http_req.content.charset = "utf-8";
    http_req.content.type    = "text/xml";
    http_req.content.value   = xml_req ;
    
    auto ioc = oson_merchant_api -> get_io_service();
    auto ctx = oson_merchant_api -> get_ctx_sslv23();
    
    auto cl = std::make_shared< oson::network::http::client > (ioc, ctx);

    cl->set_request(http_req);

    cl->set_response_handler(handler);

    cl->async_start();
}

void Mplat::manager_api::on_async_status_finish( const std::string& xml_resp, const boost::system::error_code& ec ) 
{
        SCOPE_LOGD(slog);
        
        const auto& handler   = this->cur_handler_ ;
        const auto& mplat_req = this->cur_req_ ;
        
        slog.DebugLog("Response: %.*s\n", (::std::min)( 2048, static_cast< int >( xml_resp.length() ) ), xml_resp.c_str());
        
        struct Mplat::response_t response;
        
        
        if (xml_resp.empty( ) || static_cast< bool >(ec) )
        {
            Error_T er = Error_internal;
            //1. host not found, or timeout
            if (ec == boost::asio::error::host_not_found || 
                ec == boost::asio::error::operation_aborted ||
                ec == boost::asio::error::connection_reset ||
                ec == boost::asio::error::timed_out ) 
            {
                er = Error_HTTP_host_not_found ;
            }
            
            slog.WarningLog("er: %d, boost-error-code: %d, msg: %s", (int)er, ec.value(), ec.message().c_str());
            return handler(mplat_req, response, er);
        }
        //<r type="" result="" id="" txn="" message="" />
        //Parse xml_resp
        try
        {
            namespace pt =   boost::property_tree;

            std::istringstream stream( xml_resp );

            pt::ptree  root;
            
            pt::read_xml(stream, root);
            
            pt::ptree resp = root.get_child("r");
            pt::ptree attr = resp.get_child("<xmlattr>", pt::ptree());

            for(const pt::ptree::value_type& v : attr)
            {
                const std::string&  name = v.first ;
                std::string  value = v.second.data();

                if (name == "type")
                    response.type = value;
                else if (name == "result")
                    response.result = string2num(value);
                else if (name == "txn")
                    response.txn = value;
                else if (name == "message")
                    response.message = value;
                else if (name == "id")
                    response.id = value;
                else 
                    response.extra.ev_s.push_back(std::make_pair(name, value));
            }
        } 
        catch( std::exception & e )
        {
            slog.WarningLog("Exception: %s", e.what());
            return handler(mplat_req, response, Error_internal);
        }
        
        return handler(mplat_req, response, Error_OK );
}

void Mplat::manager_api::on_async_check_finish( const std::string& xml_resp, const boost::system::error_code& ec  ) 
{
    SCOPE_LOGD(slog);
    
    const auto& handler   = this->cur_handler_;
    const auto& mplat_req = this->cur_req_ ;
    
    Mplat::response_t response;
    
    slog.DebugLog("response: %s\n", xml_resp.c_str());
    
    if (xml_resp.empty() || static_cast<bool>(ec))
    {
        return handler(mplat_req, response, Error_merchant_operation ) ;
    }
    /////////////////////////////////////////////////////////////////////////////////////////
    
    //<r type="" result="" currency="" client_rate="" message="" extra_N="" />
    //Parse xml_resp
    try
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        pt::ptree attr = root.get_child("r.<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            const std::string&  name = v.first ;
            std::string  value = v.second.data();
            
//            if (name != "message"){
//                slog.DebugLog("name: %s\tvalue: %s", name.c_str(), value.c_str());
//            } else {
//                const std::string& m64 = /*oson::utils::encodebase64*/(value);
//                slog.DebugLog("name: message\tvalue: %s", m64.c_str());
//            }
            
            if (name == "type")
                response.type = value;
            else if (name == "result")
                response.result = string2num(value);
            else if (name == "currency")
                response.currency = value;
            else if (name == "client_rate")
                response.client_rate = value;
            else if (name == "message")
                response.message = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
    catch(std::exception& e)
    {
        slog.WarningLog("Exception: %s", e.what() ) ;
        
        return handler( mplat_req, response, Error_internal );
    }
    
    return handler( mplat_req, response, Error_OK ) ;

}

void Mplat::manager_api::on_async_pay_finish( const std::string& xml_resp, const boost::system::error_code& ec  ) 
{
    SCOPE_LOGD(slog);
    
    const auto& handler = this->cur_handler_;
    
    struct Mplat::response_t response;
        
    const struct Mplat::request_t& mplat_req = this->cur_req_ ;
        
    //<r type="" result="" id="" txn="" message="" amount="" currency="" extra_N="" />
    slog.DebugLog("Response: %.*s", (::std::min)(2048, static_cast< int >( xml_resp.length()) ), xml_resp.c_str());
    
    if (xml_resp.empty() || static_cast<bool>( ec ) )
    {
        response.message = "Connection error: " + ec.message();
        response.result  = ec.value();
        
        Error_T er = Error_merchant_operation ;
        if (ec == boost::asio::error::timed_out )
            er = Error_timeout ;
        
        return handler(mplat_req, response, er );
    }
    
    //Parse xml_resp
    try
    {
        namespace pt =   boost::property_tree;
    
        std::istringstream stream( xml_resp );

        pt::ptree  root;
        pt::read_xml(stream, root);
        
        pt::ptree attr = root.get_child("r.<xmlattr>", pt::ptree());
        
        for(const pt::ptree::value_type& v : attr)
        {
            const std::string&  name = v.first;
            std::string  value = v.second.data();
            
            if (name == "type")
                response.type = value;
            else if (name == "result")
                response.result = string2num(value);
            else if (name == "currency")
                response.currency = value;
            else if (name == "txn")
                response.txn = value;
            else if (name == "message")
                response.message = value;
            else if (name == "amount")
                response.amount = value;
            else if (name == "id")
                response.id = value;
            else 
                response.extra.ev_s.push_back(std::make_pair(name, value));
        }
    }
    catch(std::exception& e)
    {
        slog.WarningLog("Exception: %s", e.what());
        return handler(mplat_req, response, Error_internal);
    }
    
    
    return handler(mplat_req, response, Error_OK );
}



void Mplat::manager_api::async_status(const struct Mplat::request_t& request, Mplat::handler_type handler)
{
    SCOPE_LOGD(slog);
    auto xml_req_sig = make_status_xml(request);;
    
    Mplat::manager_api copy_api = *this;
    
    copy_api.cur_handler_ = handler;
    copy_api.cur_req_     = request;
    
    //using namespace std::placeholders;
    async_http_req(xml_req_sig.first, xml_req_sig.second, 
            std::bind(&manager_api::on_async_status_finish, copy_api, std::placeholders::_1, std::placeholders::_2 ) ) ;
}

void Mplat::manager_api::async_check(const struct Mplat::request_t& request, Mplat::handler_type handler )
{
    SCOPE_LOGD(slog);
    
    auto xml_req_sig = make_check_xml(request);
    
    Mplat::manager_api copy_api = *this;
    
    copy_api.cur_handler_ = handler;
    copy_api.cur_req_     = request;
    
    //using namespace std::placeholders;
    async_http_req(xml_req_sig.first, xml_req_sig.second, 
            std::bind(&manager_api::on_async_check_finish, copy_api, std::placeholders::_1, std::placeholders::_2 )  ) ;
}


void Mplat::manager_api::async_pay(const struct Mplat::request_t& request, Mplat::handler_type handler ) 
{
    SCOPE_LOGD(slog);
    
    auto xml_req_sig = make_pay_xml(request);
    
    Mplat::manager_api copy_api = *this;
    
    copy_api.cur_handler_ = handler;
    copy_api.cur_req_     = request;
    
    //using namespace std::placeholders;
    async_http_req(xml_req_sig.first, xml_req_sig.second, 
            std::bind(&manager_api::on_async_pay_finish, copy_api, std::placeholders::_1, std::placeholders::_2) ) ;
}



/***********************************************************************************/
/**********************************************************************************/ 

namespace mm_llc = oson::backend::merchant::money_movers_llc;

mm_llc::manager_t::manager_t(const struct acc_t & acc)
    : acc(acc)
{}

mm_llc::manager_t::~manager_t()
{}

    
struct mm_llc::response_t    mm_llc::manager_t::info( const struct request_t& req)
{
    SCOPE_LOG(slog);
    
    response_t resp;
    resp.ec.code = -99999;
    
    acc.hash = make_hash( req ) ; 
    
    //https://{URL}/info.php?service=1&service_sub_id=0&service_second_sub_id=0&service_third_sub_id=0&amount=1&id=1
    //&user=593333333&date=2012-08-01+19%3A02%3A09&hash=
    //4a5952493501c40a0fb205433c7ca62f0cefc002703b2470b0bdaad11157348b&AGENT=USER&CANAL=0
    std::string request_str = acc.url + "/info.php?"
            "service="               + to_str(req.service.id )          + "&"
            "service_sub_id="        + to_str(req.service.sub_id)       + "&"
            "service_second_sub_id=" + to_str(req.service.second_sub_id)+ "&"
            "service_third_sub_id="  + to_str(req.service.third_sub_id) + "&"
            "amount=" + to_str(req.amount)     + "&"
            "id="     + to_str( req.trn_id )   + "&"
            "user="   + req.user_login         + "&"
            "date="   + req.date_ts            + "&"
            "hash="   + acc.hash               + "&"
            "AGENT="  + acc.agent              + "&"
            "CANAL= " + acc.canal              + "" ;
    
    
    slog.DebugLog("REQUEST: %s", request_str.c_str());
    
    oson::network::http::request http_req = oson::network::http::parse_url(request_str);
    http_req.method = "GET";
    
    std::string response_str = sync_http_ssl_request(http_req);
    slog.DebugLog("RESPONSE: %s", response_str.c_str());
    if (response_str.empty()){
        resp.ec.code = (int)Error_merchant_operation;
        resp.ec.en   = "Connection error";
        return resp;
    }
    
    std::istringstream ss(response_str);
    
    namespace pt = boost::property_tree;
    pt::ptree root;
    pt::read_xml(ss, root);
    
  
    /** SUCCESS RESPONSE */
//        <result>
//          <error>
//              <errorcode>0</errorcode>
//              <errorru>Удачно</errorru>
//              <errorge>წარმატებით</errorge>
//              <erroren>successfully</erroren>
//          </error>
//          <amount><gel>1</gel></amount>
//          <user>593333333</user>
//          <service>Geocell</service>
//          <data>
//              <currency>GEL</currency>
//              <rate>1</rate>
//              <GENERATED_AMOUNT>1</GENERATED_AMOUNT>
//          </date>
//        </result>

    /**** ERROR RESPONSE */
//        <result>
//          <error>
//              <errorcode>-5</errorcode>
//              <errorru>Не правильный параметр hash</errorru>
//              <errorge>არასწორი პარამეტრი hash</errorge>
//              <erroren>Incorect parameter hash</erroren>
//          </error>
//        </result>
    
    resp.ec.code = root.get< int >("result.error.errorcode", -1);
    resp.ec.en   = root.get< std::string >("result.error.erroren", "-.-");
    resp.ec.ru   = root.get< std::string >("result.error.errorru", ".-.");
    resp.ec.ge   = root.get< std::string >("result.error.errorge", "--.");
    
    if (resp.ec.code != 0 ){
        return resp;
    }
    
    resp.gel               = root.get< int64_t     > ("result.amount.gel", 0);
    resp.currency          = root.get< std::string > ("result.data.currency", "-");
    resp.rate              = root.get< double      > ("result.data.rate", 0.0);
    resp.generated_amount  = root.get< double      > ("result.data.GENERATED_AMOUNT", 0.0);
    resp.txn_id            = req.trn_id ;
    
    return resp;
}

struct mm_llc::response_t    mm_llc::manager_t::pay( const struct request_t &req)
{
    SCOPE_LOG(slog);
    
    response_t resp;
    
    acc.hash = make_hash(req);
    
    //https://{URL}/pay.php?service=77&service_sub_id=0&service_second_sub_id=0&service_third_sub_id=0&amount=1
    //&id=14&user=Z189404578252&date=2012-05-21+19%3A26%3A08&hash=fcc7196557825e7b612cdfcf5fb0d4444cf114a84c1a4d55d9c339c65307eb7e&AGENT=USER&
    //CANAL=0
    
    std::string request_str = acc.url + "/pay.php?"
            "service="               + to_str(req.service.id )          + "&"
            "service_sub_id="        + to_str(req.service.sub_id)       + "&"
            "service_second_sub_id=" + to_str(req.service.second_sub_id)+ "&"
            "service_third_sub_id="  + to_str(req.service.third_sub_id) + "&"
            "amount=" + to_str(req.amount)     + "&"
            "id="     + to_str( req.trn_id )   + "&"
            "user="   + req.user_login         + "&"
            "date="   + req.date_ts            + "&"
            "hash="   + acc.hash               + "&"
            "AGENT="  + acc.agent              + "&"
            "CANAL= " + acc.canal              + "" ;
    
    
    slog.DebugLog("REQUEST: %s", request_str.c_str());
    
    oson::network::http::request http_req = oson::network::http::parse_url(request_str);
    http_req.method = "GET";
    
    std::string response_str = sync_http_ssl_request(http_req);
    slog.DebugLog("RESPONSE: %s", response_str.c_str());
    if (response_str.empty()){
        resp.ec.code = (int)Error_merchant_operation;
        resp.ec.en   = "Connection error";
        return resp;
    }
    
    std::istringstream ss(response_str);
    
    namespace pt = boost::property_tree;
    pt::ptree root;
    pt::read_xml(ss, root);
    
  
    /** SUCCESS RESPONSE */

//        <result>
//          <error>
//              <errorcode>0</errorcode>
//              <errorru>Удачно</errorru>
//              <errorge>წარმატებით</errorge>
//              <erroren>successfully</erroren>
//          </error>
//          <amount><gel>1</gel></amount>
//          <user>Z189404578252</user>
//          <service>webmoney</service>
//          <operationid>1</operationid>
//          <data>
//              <currency>WMZ</currency>
//              <rate>1.6807</rate>
//              <GENERATED_AMOUNT>0.59</GENERATED_AMOUNT>
//          </data>
//        </result>
    
    /**** ERROR RESPONSE */
//        <result>
//          <error>
//              <errorcode>-5</errorcode>
//              <errorru>Не правильный параметр hash</errorru>
//              <errorge>არასწორი პარამეტრი hash</errorge>
//              <erroren>Incorect parameter hash</erroren>
//          </error>
//        </result>
    
    resp.ec.code = root.get< int         >("result.error.errorcode", -1);
    resp.ec.en   = root.get< std::string >("result.error.erroren", "-.-");
    resp.ec.ru   = root.get< std::string >("result.error.errorru", ".-.");
    resp.ec.ge   = root.get< std::string >("result.error.errorge", "--.");
    
    if (resp.ec.code != 0 ){
        return resp;
    }
    
    resp.gel               = root.get< int64_t     > ("result.amount.gel", 0);
    resp.currency          = root.get< std::string > ("result.data.currency", "-");
    resp.rate              = root.get< double      > ("result.data.rate", 0.0);
    resp.generated_amount  = root.get< double      > ("result.data.GENERATED_AMOUNT", 0.0);
    resp.txn_id            = root.get< int64_t     > ("result.operationid", 0);
    
    return resp;
    
}
  
std::string     mm_llc::manager_t::make_hash(const struct request_t & req)
{
    SCOPE_LOG(slog);
    //Agent.service.amount.id.user.secret  --> sha256
    std::string secret = "#!ZWkJy#gw!#";
    std::string h = oson::utils::sha256_hash(acc.agent + to_str(req.service.id) + to_str(req.amount) + to_str(req.trn_id) + req.user_login + secret);
    
    slog.DebugLog("hash: '%s'", h.c_str());
    return h;
}
/****************************************************************************************/

namespace webmoney = ::oson::backend::merchant::webmoney ;

webmoney::manager_t::manager_t( const struct acc_t& acc)
        : acc_( acc ) 
{}
  
static std::string win1251_to_utf8( const std::string& text )
{
    static const int wtable[64] = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
        0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
        0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x007F, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
        0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
        0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
        0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
        0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457};

    std::string utext; 
    
    for( size_t i = 0; i < text.size(); ++i)
    {
        int wc = (unsigned char)text[ i ] ;
        
        // Windows-1251 to Unicode
        if (wc>=0x80) {
          if (wc<=0xBF) /* Ђ-ї */
          {
            wc = wtable[wc-0x80];
          }
          else if (wc>=0xC0 && wc<=0xDF) // А-Я
            wc = wc - 0xC0 + 0x0410;
          else if (wc>=0xE0) // а-я
            wc = wc - 0xE0 + 0x0430;
        }
        
        // Unicode to UTF-8
        // 0x00000000 — 0x0000007F -> 0xxxxxxx
        // 0x00000080 — 0x000007FF -> 110xxxxx 10xxxxxx
        // 0x00000800 — 0x0000FFFF -> 1110xxxx 10xxxxxx 10xxxxxx
        // 0x00010000 — 0x001FFFFF -> 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        
        if ( wc < 0x80 )
        {
          utext  += (char)wc;
        }
        else if (wc<0x800)
        {
          utext  += (char)((wc>>6)   | 0xC0);
          utext  += (char)((wc&0x3F) | 0x80);
        }
        else if (wc<0x10000)
        {
          utext += (char)((wc>>12)  | 0xE0);
          utext += (char)((wc>>6)   | 0x80);
          utext += (char)((wc&0x3F) | 0x80);
        }
        else
        {
          utext += (char)((wc>>18)  | 0xF0);
          utext += (char)((wc>>12)  | 0x80);
          utext += (char)((wc>>6)   | 0x80);
          utext += (char)((wc&0x3F) | 0x80);
        }
    }
    
    return utext;
}
 
void webmoney::manager_t::async_check_atm(const request_t& req, handler_t h ) const
{
    SCOPE_LOG(slog);
//    <w3s.request lang="">
//    <wmid></wmid>
//    <sign type=""></sign>
//    <payment currency="" exchange="">
//     <purse></purse>
//     <price></price>
//    </payment> 
//   </w3s.request>

    std::string sign     = webmoney::make_check_sign(req);
    std::string exchange = ( ! req.payment.exchange.empty() )?  "exchange=\"" + req.payment.exchange + "\" "  : " ";
    
    std::string req_str = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
            "<w3s.request lang=\"en\">"
            "   <wmid>" + req.wmid + "</wmid>"
            "   <sign type=\"2\">" + sign + "</sign>"
            "   <payment currency=\"" + req.payment.currency +  "\"  >"
            "      <purse>" + req.payment.purse + "</purse>"
            "      <price>" + to_str( req.payment.price, 2, true ) + "</price>"
            "  </payment>"
            "</w3s.request>" ;
    
    oson::network::http::request http_req  = oson::network::http::parse_url("https://transfer.gdcert.com/ATM/Xml/PrePayment2.ashx") ;
    http_req.method          = "POST"     ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_str    ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    auto http_handler = [ req, h ](  std::string   resp_str, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog);
        slog.DebugLog("RESPONSE: %s", resp_str.c_str());
        struct webmoney::response_t resp;
        resp.ec = ec;
        if ( static_cast<bool>(ec) ) {
            resp.retval = ec.value() ;
            resp.retdesc = ec.message();
            slog.WarningLog("resp_str is empty!");
            return h( req, resp );
        }
        //fix dot, because boost::property_tree can't parse dot as name. Yes, there exists way to use different separator, but, I don't want it.
        {
            std::string::size_type dot_pos = resp_str.find("w3s.response");
            while(dot_pos != resp_str.npos){
                resp_str[dot_pos + 3] = '_';
                dot_pos = resp_str.find("w3s.response", dot_pos + 4 );
            }
        }

        namespace pt = boost::property_tree ;
        pt::ptree root;
        std::stringstream ss(resp_str);
        pt::read_xml(ss, root);

    //    <w3s.response>
    //      <retval></retval>
    //      <retdesc></retdesc>
    //      <payment currency="" exchange="">
    //          <purse></purse>
    //          <upexchange></upexchange>
    //          <course></course>
    //          <price></price>
    //          <amount></amount> 
    //          <rest></rest>
    //          <limit>
    //          <day></day>
    //          <month></month>
    //      </limit> 
    //  </payment> 
    //</w3s.response>

        const pt::ptree& w3s = root.get_child("w3s_response") ;


        resp.retval  = w3s.get< int64_t>("retval", -1);
        resp.retdesc = w3s.get< std::string>("retdesc", "-.-.-") ;


        if (resp.retval != 0){
            slog.WarningLog("retval IS NOT ZERO: %ld", resp.retval) ;

            resp.description = w3s.get< std::string > ("description", "");
            resp.description = win1251_to_utf8(resp.description);

            slog.DebugLog("description(utf8): %s", resp.description.c_str());

            return h(req, resp);
        }
        resp.payment.purse               = w3s.get< std::string > ("payment.purse", "-.-");
        resp.payment.price               = w3s.get< webmoney::response_t::amount_t >("payment.price", 0);
        resp.payment.amount              = w3s.get< webmoney::response_t::amount_t>("payment.amount", 0);
        resp.payment.rest                = w3s.get< webmoney::response_t::amount_t>("payment.rest", 0);
        resp.payment.limit.daily_limit   = w3s.get< webmoney::response_t::amount_t >("payment.limit.day", 0) ;
        resp.payment.limit.monthly_limit = w3s.get< webmoney::request_t::amount_t >("payment.limit.month", 0);

        return h(req, resp);
    };
    /**********************************************************/
    //std::string resp_str = sync_http_ssl_request(req_);
    auto io_service  = oson_merchant_api -> get_io_service () ;
    auto ssl_ctx     = oson_merchant_api -> get_ctx_sslv23 () ;
    
    auto http_client = oson::network::http::client::create(io_service, ssl_ctx ) ;
    http_client->set_request( http_req );
    http_client->set_response_handler(http_handler ) ;
    http_client->async_start() ;
    /***********************************************************/
}
 
struct webmoney::response_t webmoney::manager_t::check_atm(const struct request_t& request)const
{
    SCOPE_LOG(slog);

    struct webmoney::response_t resp;
    
//    <w3s.request lang="">
//    <wmid></wmid>
//    <sign type=""></sign>
//    <payment currency="" exchange="">
//     <purse></purse>
//     <price></price>
//    </payment> 
//   </w3s.request>

    std::string sign = webmoney::make_check_sign(request);
    std::string exchange = ( ! request.payment.exchange.empty() )?  "exchange=\"" + request.payment.exchange + "\" "  : " ";
    
    std::string req_str = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
            "<w3s.request lang=\"en\">"
            "   <wmid>" + request.wmid + "</wmid>"
            "   <sign type=\"2\">" + sign + "</sign>"
            "   <payment currency=\"" + request.payment.currency +  "\"  >"
            "      <purse>" + request.payment.purse + "</purse>"
            "      <price>" + to_str( request.payment.price, 2, true ) + "</price>"
            "  </payment>"
            "</w3s.request>" ;
    
    oson::network::http::request req_  = oson::network::http::parse_url("https://transfer.gdcert.com/ATM/Xml/PrePayment2.ashx") ;
    req_.method = "POST" ;
    req_.content.charset = "UTF-8";
    req_.content.type    = "text/xml";
    req_.content.value   = req_str   ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    std::string resp_str = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %s", resp_str.c_str());
    
    if ( resp_str.empty() ) {
        resp.retval = -9999999;
        slog.WarningLog("resp_str is empty!");
        return resp;
    }
    //fix dot, because boost::property_tree can't parse dot as name. Yes, there exists way to use different separator, but, I don't want it.
    {
        std::string::size_type dot_pos = resp_str.find("w3s.response");
        while(dot_pos != resp_str.npos){
            resp_str[dot_pos + 3] = '_';
            dot_pos = resp_str.find("w3s.response", dot_pos + 4 );
        }
    }
    
    namespace pt = boost::property_tree ;
    pt::ptree root;
    std::stringstream ss(resp_str);
    pt::read_xml(ss, root);
    
//    <w3s.response>
//      <retval></retval>
//      <retdesc></retdesc>
//      <payment currency="" exchange="">
//          <purse></purse>
//          <upexchange></upexchange>
//          <course></course>
//          <price></price>
//          <amount></amount> 
//          <rest></rest>
//          <limit>
//          <day></day>
//          <month></month>
//      </limit> 
//  </payment> 
//</w3s.response>
    
    const pt::ptree& w3s = root.get_child("w3s_response") ;
    
    
    resp.retval  = w3s.get< int64_t>("retval", -1);
    resp.retdesc = w3s.get< std::string>("retdesc", "-.-.-") ;
    
    
    if (resp.retval != 0){
        slog.WarningLog("retval IS NOT ZERO: %ld", resp.retval) ;
        
        resp.description = w3s.get< std::string > ("description", "");
        resp.description = win1251_to_utf8(resp.description);
        
        slog.DebugLog("description(utf8): %s", resp.description.c_str());
        
        return resp;
    }
    resp.payment.purse  = w3s.get< std::string > ("payment.purse", "-.-");
    resp.payment.price  = w3s.get< webmoney::response_t::amount_t >("payment.price", 0);
    resp.payment.amount = w3s.get< webmoney::response_t::amount_t>("payment.amount", 0);
    resp.payment.rest   = w3s.get< webmoney::response_t::amount_t>("payment.rest", 0);
    resp.payment.limit.daily_limit = w3s.get< webmoney::response_t::amount_t >("payment.limit.day", 0) ;
    resp.payment.limit.monthly_limit = w3s.get< webmoney::request_t::amount_t >("payment.limit.month", 0);
    
    return resp ;
}

void webmoney::manager_t::async_pay_atm  ( const request_t& req, handler_t h ) const
{
    SCOPE_LOG(slog);
    
//    struct webmoney::response_t resp;
    
    //<w3s.request lang="">
    // <wmid></wmid>
    // <sign type=""></sign>
    // <payment id="" currency="" test="" exchange="">
    //  <purse></purse>
    //  <price></price>
    //  <date></date>
    //  <point></point>
    // </payment> 
    //</w3s.request>

    std::string sign = webmoney::make_pay_sign(req);
    //std::string exchange = ( ! request.payment.exchange.empty() )?  "exchange=\"" + request.payment.exchange + "\" "  : " ";
    
    std::string req_str = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
            "<w3s.request lang=\"en\">"
            "   <wmid>" + req.wmid + "</wmid>"
            "   <sign type=\"2\">" + sign + "</sign>"
            "   <payment id = \"" + to_str(req.id) + "\"  currency=\"" + req.payment.currency + "\"  test=\"" + to_str(req.test) + "\" >"
            "      <purse>" + req.payment.purse + "</purse>"
            "      <price>" + to_str( req.payment.price, 2, true ) + "</price>"
            "      <date>"  + req.payment.pspdate + "</date>"
            "      <point>" + req.payment.pspnumber + "</point>"
            "  </payment>"
            "</w3s.request>" ;
    
    oson::network::http::request http_req  = oson::network::http::parse_url("https://transfer.gdcert.com/ATM/Xml/Payment2.ashx") ;
    http_req.method          = "POST"    ;
    http_req.content.charset = "UTF-8"   ;
    http_req.content.type    = "text/xml";
    http_req.content.value   = req_str   ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    //std::string resp_str = sync_http_ssl_request(http_req);
    auto http_handler = [req, h](std::string resp_str, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog);
        struct webmoney::response_t resp;
        slog.DebugLog("RESPONSE: %s", resp_str.c_str());
        resp.ec = ec ;
        if ( resp_str.empty() ) {
            resp.retval = -9999999;
            resp.retdesc = "resp_str is empty!" ;
            slog.WarningLog("resp_str is empty!");
            return h( req, resp ) ;
        }
        //fix dot, because boost::property_tree can't parse dot as name. Yes, there exists way to use different separator, but, I don't want it.
        {
            std::string::size_type dot_pos = resp_str.find("w3s.response");
            while(dot_pos != resp_str.npos){
                resp_str[dot_pos + 3] = '_';
                dot_pos = resp_str.find("w3s.response", dot_pos + 4 );
            }
        }

        namespace pt = boost::property_tree ;
        pt::ptree root;
        std::stringstream ss(resp_str);
        pt::read_xml(ss, root);


        //    <w3s.response>
        // <retval></retval>
        // <retdesc></retdesc>
        // <payment id="" currency="" test="">
        //  <purse></purse>
        //  <price></price>
        //  <amount></amount>
        //  <comiss></comiss>
        //  <rest></rest>
        //  <date></date>
        //  <point></point>
        //  <wmtranid></wmtranid>
        //  <dateupd></dateupd>
        //  <limit>
        //   <day></day>
        //   <month></month>
        //  </limit>
        // </payment> 
        //</w3s.response>

        pt::ptree w3s = root.get_child("w3s_response") ;


        resp.retval  = w3s.get< int64_t>("retval", -1);
        resp.retdesc = w3s.get< std::string>("retdesc", "-.-.-") ;


        if (resp.retval != 0){
            slog.WarningLog("retval IS NOT ZERO: %ld", resp.retval) ;
            return h( req, resp ) ;
        }
        resp.payment.purse  = w3s.get< std::string > ("payment.purse", "-.-");
        resp.payment.price  = w3s.get< webmoney::response_t::amount_t >("payment.price", 0);
        resp.payment.amount = w3s.get< webmoney::response_t::amount_t>("payment.amount", 0);
        resp.payment.rest   = w3s.get< webmoney::response_t::amount_t>("payment.rest", 0);
        resp.payment.limit.daily_limit = w3s.get< webmoney::response_t::amount_t >("payment.limit.day", 0) ;
        resp.payment.limit.monthly_limit = w3s.get< webmoney::request_t::amount_t >("payment.limit.month", 0);

        resp.payment.wmtranid  = w3s.get< int64_t> ("payment.wmtranid", 0);
        resp.payment.tranid    = w3s.get< int64_t> ("payment.tranid", 0);

        return h( req, resp ) ;
    };
    /*************************/
    auto io_service  = oson_merchant_api -> get_io_service () ;
    auto ssl_ctx     = oson_merchant_api -> get_ctx_sslv23 () ;
    
    auto http_client = oson::network::http::client::create(io_service, ssl_ctx ) ;
    http_client->set_request( http_req );
    http_client->set_response_handler(http_handler ) ;
    http_client->async_start() ;
    /***********************************************************/
}

struct webmoney::response_t webmoney::manager_t::pay_atm(const struct request_t& request)const
{
    SCOPE_LOG(slog);
    struct webmoney::response_t resp;
    
    //<w3s.request lang="">
    // <wmid></wmid>
    // <sign type=""></sign>
    // <payment id="" currency="" test="" exchange="">
    //  <purse></purse>
    //  <price></price>
    //  <date></date>
    //  <point></point>
    // </payment> 
    //</w3s.request>

    std::string sign = webmoney::make_pay_sign(request);
    //std::string exchange = ( ! request.payment.exchange.empty() )?  "exchange=\"" + request.payment.exchange + "\" "  : " ";
    
    std::string req_str = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>"
            "<w3s.request lang=\"en\">"
            "   <wmid>" + request.wmid + "</wmid>"
            "   <sign type=\"2\">" + sign + "</sign>"
            "   <payment id = \"" + to_str(request.id) + "\"  currency=\"" + request.payment.currency + "\"  test=\"" + to_str(request.test) + "\" >"
            "      <purse>" + request.payment.purse + "</purse>"
            "      <price>" + to_str( request.payment.price, 2, true ) + "</price>"
            "      <date>"  + request.payment.pspdate + "</date>"
            "      <point>" + request.payment.pspnumber + "</point>"
            "  </payment>"
            "</w3s.request>" ;
    
    oson::network::http::request req_  = oson::network::http::parse_url("https://transfer.gdcert.com/ATM/Xml/Payment2.ashx") ;
                                                                       
    req_.method = "POST" ;
    req_.content.charset = "UTF-8";
    req_.content.type    = "text/xml";
    req_.content.value   = req_str   ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    std::string resp_str = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %s", resp_str.c_str());
    
    if ( resp_str.empty() ) {
        resp.retval = -9999999;
        resp.retdesc = "resp_str is empty!" ;
        slog.WarningLog("resp_str is empty!");
        return resp;
    }
    //fix dot, because boost::property_tree can't parse dot as name. Yes, there exists way to use different separator, but, I don't want it.
    {
        std::string::size_type dot_pos = resp_str.find("w3s.response");
        while(dot_pos != resp_str.npos){
            resp_str[dot_pos + 3] = '_';
            dot_pos = resp_str.find("w3s.response", dot_pos + 4 );
        }
    }
    
    namespace pt = boost::property_tree ;
    pt::ptree root;
    std::stringstream ss(resp_str);
    pt::read_xml(ss, root);
    
    
//    <w3s.response>
// <retval></retval>
// <retdesc></retdesc>
// <payment id="" currency="" test="">
//  <purse></purse>
//  <price></price>
//  <amount></amount>
//  <comiss></comiss>
//  <rest></rest>
//  <date></date>
//  <point></point>
//  <wmtranid></wmtranid>
//  <dateupd></dateupd>
//  <limit>
//   <day></day>
//   <month></month>
//  </limit>
// </payment> 
//</w3s.response>
    
    pt::ptree w3s = root.get_child("w3s_response") ;
    
    
    resp.retval  = w3s.get< int64_t>("retval", -1);
    resp.retdesc = w3s.get< std::string>("retdesc", "-.-.-") ;
    
    
    if (resp.retval != 0){
        slog.WarningLog("retval IS NOT ZERO: %ld", resp.retval) ;
        return resp;
    }
    resp.payment.purse  = w3s.get< std::string > ("payment.purse", "-.-");
    resp.payment.price  = w3s.get< webmoney::response_t::amount_t >("payment.price", 0);
    resp.payment.amount = w3s.get< webmoney::response_t::amount_t>("payment.amount", 0);
    resp.payment.rest   = w3s.get< webmoney::response_t::amount_t>("payment.rest", 0);
    resp.payment.limit.daily_limit = w3s.get< webmoney::response_t::amount_t >("payment.limit.day", 0) ;
    resp.payment.limit.monthly_limit = w3s.get< webmoney::request_t::amount_t >("payment.limit.month", 0);
    
    resp.payment.wmtranid  = w3s.get< int64_t> ("payment.wmtranid", 0);
    resp.payment.tranid    = w3s.get< int64_t> ("payment.tranid", 0);
    
    return resp;
}

struct webmoney::response_t webmoney::manager_t::get_currency_atm(const struct request_t& request ) const
{
    SCOPE_LOG(slog);
    struct webmoney::response_t resp;
    
    return resp;
}

struct webmoney::response_t webmoney::manager_t::pay_provider_atm(const struct request_t& request ) const
{
    SCOPE_LOG(slog);
    struct webmoney::response_t resp;
    
    return resp;
}
    

webmoney::request_t::str_t   webmoney::request_t::sign_t::key_file_from_config()
{
    return oson_opts->certs.webmoney_private_key;
}

std::string  webmoney::make_check_sign(const struct request_t& request) 
{
    SCOPE_LOGD(slog);
    // wmid + currency + purse + price  (ehtimol base64 ).
    std::string price_s = to_str(request.payment.price, 2, true);// precision - 4, trim = true.
    std::string raw     = request.wmid + request.payment.currency + request.payment.purse + price_s;
    
    slog.DebugLog("raw: %s", raw.c_str());
    
    std::pair< std::string , int > sp = oson::utils::sign_sha1(raw, request.sign.key_file_path, "" /*password*/ ) ;
    
    if (sp.second != 0 ) 
    {
        slog.WarningLog("sign_sha1 error: %d", sp.second);
    }
    
    slog.DebugLog("sign:  %s ", sp.first.c_str());
    
    std::string sign = sp.first;
    
    return sign;
}


std::string webmoney::make_pay_sign(const struct request_t & request ) 
{
    SCOPE_LOGD(slog);
    
    //PAY SIGN: формируется из параметров: wmid + id + currency + test + purse + price + date + point
    std::string price_s = to_str(request.payment.price, 2, true); // precision : 2, trim: true
    std::string raw = request.wmid + to_str( request.id ) + request.payment.currency + to_str(request.test) +
                      request.payment.purse + price_s + request.payment.pspdate + request.payment.pspnumber ; 
    
    slog.DebugLog("raw: %s", raw.c_str());
    std::pair< std::string , int > sp = oson::utils::sign_sha1(raw, request.sign.key_file_path, "" /*password*/);
    
    if ( sp.second != 0 )
    {
        slog.WarningLog("sign_sha1 error: %d", sp.second);
    }
    
    slog.DebugLog("sign: %s ", sp.first.c_str());
    
    std::string sign = sp.first;
    
    return sign;
}

/**************************************************************************************************************************************/


namespace Ucell = oson::backend::merchant::Ucell ;

static Ucell::auth_info  parse_ucell_api_json( const Ucell::acc_t& acc )
{
    SCOPE_LOGD(slog);
    
    slog.DebugLog("acc.api_json: %s\n", acc.api_json.c_str());
    
    namespace pt = boost::property_tree;
    pt::ptree root;
    
    std::stringstream ss(acc.api_json);
    
    pt::read_json(ss, root);
    
    const pt::ptree & webApi = root.get_child("webApi");
    
    Ucell::auth_info info;
    
    info.url        = webApi.get< std::string > ( "url", "<>");
    info.app_name   = webApi.get< std::string > ( "ApplicationName", "<>");
    info.app_pwd    = webApi.get< std::string > ( "ApplicationPassword", "<>");
    info.app_key    = webApi.get< std::string > ( "ApplicationKey", "<>");
    
    std::string publicKey = webApi.get< std::string >("PublicKey", "");
    
    info.key_info.expiration = webApi.get< std::string >("PublicKeyExpiration", "");
    info.key_info.timestamp  = webApi.get< std::string > ("timeStamp", "");
    
    slog.DebugLog("url: %s\napp-name: %s\napp-pwd:%s\napp-key: %s\npublic-key: %s\n", 
        info.url.c_str(), 
        info.app_name.c_str(),
        info.app_pwd.c_str(),
        info.app_key.c_str(),
        publicKey.c_str());
    
    
    //// now publicKey
    pt::ptree xml_root;
    ss.str(publicKey);
    pt::read_xml(ss, xml_root);
    
    info.key_info.modulus   = xml_root.get< std::string > ("RSAKeyValue.Modulus", "");
    info.key_info.exponenta = xml_root.get< std::string > ("RSAKeyValue.Exponent", "");
    
    
    return info;
}



Ucell::manager_t::manager_t( const acc_t & acc ) 
  : acc_( acc )
  , info_( parse_ucell_api_json( acc ) )
{
    SCOPE_LOG(slog);
}

int Ucell::manager_t::check_expiration_auth_info()
{
    SCOPE_LOG( slog );
 
    const std::string& expiration = info_.key_info.expiration; 
    std::time_t now = std::time( 0 ) - 10 * 60 ; // 10 minutes early.
    
    bool const expired = expiration.empty() || str_2_time_T( expiration.c_str() ) < now ;
      
    if ( expired ) 
    {
        try
        {
            int er =  request_public_key( info_.key_info ) ;
       
            if (er != 0)
                return er;
        }
        catch( std::exception & e)
        {
            slog.ErrorLog("Exception: %s", e.what());
            return -1;
        }
       
        //save it to database.
        save_auth_info_to_db( info_ );

    }
    
    return 0;
}


Ucell::manager_t::~manager_t()
{
    SCOPE_LOG(slog);
    
}

std::string Ucell::public_key_info::to_xml_key() const
{
    return "<RSAKeyValue><Modulus>" + this->modulus + "</Modulus><Exponent>"+ this->exponenta + "</Exponent></RSAKeyValue>" ;
}

int Ucell::manager_t::info(const request_t& req, response_t& resp)
{
    SCOPE_LOG(slog);

//    User-Agent: Fiddler
//Host: 10.2.18.143:443
//Content-Length: 29
//Content-Type: application/json; charset=utf-8
//Authorization-Token: 156,3,16,65,180,230,66,181,212,157,159,238,27,146,85,83,141,179,38,245,209,121,182,65,86,237,216,69,80,76,41,37,55,228,23,180,210,16,207,218,95,244,184,82,229,222,41,111,181,151,110,34,202,13,171,250,99,27,27,10,240,143,172,179,103,234,181,113,110,139,14,135,201,68,167,10,139,98,66,243,143,48,207,181,220,209,139,26,40,19,145,206,71,199,51,240,168,174,165,250
//Authorization-Application: CoinTestApp123
//
//{
//	"Msisdn":"998935101400"
//}
    
    //http://IP/WebApi_deb/api/Tools/SubscriberGetBillingInfo 
    
    //188.113.225.128:8001
    int ret = check_expiration_auth_info();
    if ( 0 != ret ) {
        slog.WarningLog("auth info expiration can't update!");
        return ret;
    }
    
    ////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    std::string token = make_token("Tools/SubscriberGetBillingInfo");
    //https://188.113.225.128:8443 
    oson::network::http::request req_ = oson::network::http::parse_url(info_.url);
    req_.method = "POST";
    req_.port   = "8443";
    req_.path   =  "/api/Tools/SubscriberGetBillingInfo" ;
    req_.content.charset = "UTF-8";
    req_.content.type    = "application/json";
    req_.content.value   = "{ \"Msisdn\":\""+ req.clientid + "\" }  " ;
    req_.headers.push_back(  "Authorization-Token: " + token )  ;
    req_.headers.push_back(  "Authorization-Application: " + info_.app_name )  ; //SubscriberGetBillingInfo");
    
    
    slog.DebugLog("REQUEST: %s", req_.content.value.c_str());
    std::string resp_json = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %s", resp_json.c_str());
    
    if (resp_json.empty()){
        slog.WarningLog("No response!");
        return -1;
    }
    
    namespace pt = boost::property_tree;
    pt::ptree root;
    std::stringstream ss(resp_json);
    
    pt::read_json(ss, root);
    
        //{
        //	"StateId":"2",
        //	"StateValue":"Active",
        //	"PrimaryOfferId":"51005339",
        //	"PrimaryOfferValue":"OK",
        //	"IsPostpaid":"0",
        //	"AvailableBalance":"8.38",
        //	"SubscrNo":"36393807",
        //	"AccountNo":"33597529",
        //	"PreviousState":"Suspended(S1)",
        //	"Language":"Uzbek",
        //	"StateChangeDate":"2015.08.11 16:25:36",
        //	"CreationDate":"2015.08.03 14:15:01",
        //	"ActivationDate":"2015.08.03 14:15:16",
        //	"LastTransactionDate":"2015.09.02 11:29:21",
        //	"AccountExpirationDate":"2015.10.02 00:00:00",
        //	"LastRechargeDate":"",
        //	"Accruals":"Only for postpaid",
        //	"Result":"1",
        //	"code":0,
        //	"msg":"OK",
        //	"timeStamp":"2015-09-02T07:20:08.226911Z",
        //	"LastReregistrationDate":""
        //}

    resp.clientid          = req.clientid;
    resp.status_text       = root.get<std::string>("StateValue", "");
    resp.status_value      = 0;//root.get< int64_t > ( "code", 0) * 0   ; // alwasy 0 must be !).
    resp.timestamp         = root.get< std::string>("timeStamp", "");
    resp.available_balance = root.get< double >("AvailableBalance", 0.0);
    
    return 0;
}


int Ucell::manager_t::check(const request_t& req, response_t& resp )
{
    SCOPE_LOG(slog);   
#if 1
    std::string phone    = req.clientid  ;
    std::string login    = acc_.login    ; // "oson";
    std::string password = acc_.password ; // "oson";
    
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
            "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:uws=\"http://uws.provider.com/\"> \n"
            "     <soapenv:Header/> \n"
            "     <soapenv:Body>    \n"
            "         <uws:GetInformationArguments> \n"
            "              <password>"+login+"</password>     \n"
            "               <username>"+password+"</username> \n"
            "               <parameters> \n"
            "                   <paramKey>clientid</paramKey>       \n"
            "                    <paramValue>"+phone+"</paramValue> \n"
            "               </parameters> \n"
            "               <parameters>  \n"
            "                    <paramKey>getInfoType</paramKey>         \n"
            "                    <paramValue>CHK_PERFORM_TRN</paramValue> \n"
            "               </parameters> \n"
            "               <serviceId>1</serviceId>  \n"
            "          </uws:GetInformationArguments> \n"
            "     </soapenv:Body> \n"
            "</soapenv:Envelope>  \n"
            ;
    
    std::string address = acc_.url;
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    
    req_.method          = "POST"      ;
    req_.content.charset = "UTF-8"     ;
    req_.content.type    = "text/xml"  ;
    req_.content.value   = req_xml     ;
    
    
    
    slog.DebugLog("REQUEST: %s", req_xml.c_str());
    std::string resp_xml = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());
    
    if (resp_xml.empty() ) {
        slog.ErrorLog("response is empty!");
        return -1;
    }
    
    
    namespace pt = boost::property_tree;
        
    pt::ptree root;
        
    try
    {
        std::stringstream ss(resp_xml);
        
        pt::read_xml(ss, root);
        
    }
    catch(std::exception & e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        return -1;
    }
    
    
//    
//    <?xml version='1.0' encoding='UTF-8'?>
//<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
//	<soapenv:Body>
//		<ns2:GetInformationResult xmlns:ns2="http://uws.provider.com/">
//			<errorMsg>Success</errorMsg>
//			<status>0</status>
//			<timeStamp>2018-04-06T13:28:59.353+05:00</timeStamp>
//		</ns2:GetInformationResult>
//	</soapenv:Body>
//</soapenv:Envelope>
//
//root@oson2:/var/log# nano ucell_check.xml
//root@oson2:/var/log# curl -k -X POST 'https://188.113.225.77/ProviderWebService/ProviderWebService' -H 'Content-Type: text/xml; charset=utf-8' -d @ucell_check.xml
//
//<?xml version='1.0' encoding='UTF-8'?>
//<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
//	<soapenv:Body>
//		<ns2:GetInformationResult xmlns:ns2="http://uws.provider.com/">
//			<errorMsg>Subscriber is not found</errorMsg>
//			<status>302</status>
//			<timeStamp>2018-04-06T13:29:30.257+05:00</timeStamp>
//		</ns2:GetInformationResult>
//	</soapenv:Body>
//</soapenv:Envelope>root@oson2:/var/log# 

    pt::ptree nil;
    const pt::ptree & tree_result = root.get_child("soapenv:Envelope.soapenv:Body.ns2:GetInformationResult", nil) ;
    
    if ( tree_result.empty() ) 
    {
        slog.WarningLog("Can't get 'soapenv:Envelope.soapenv:Body.ns2:GetInformationResult'   ");
        return -1;
    }
    
    resp.status_value = tree_result.get< int64_t>("status",  -989898 );
    resp.status_text  = tree_result.get< std::string>("errorMsg", "<>");
    resp.timestamp    = tree_result.get< std::string>("timeStamp", "");
#else
    //Not supported yet, so simply ignore it.
    resp.status_value = 0;
    resp.status_text = "";
    resp.timestamp   = "";
#endif 
    return 0 ;    
}

int Ucell::manager_t::pay(const request_t& req, response_t & resp  )
{
    SCOPE_LOG(slog);

//    <?xml version="1.0" encoding="UTF-8"?>
//<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" xmlns:uws="http://uws.provider.com/">
//     <soapenv:Header/>
//     <soapenv:Body>
//          <uws:PerformTransactionArguments xmlns:uws ="http://uws.provider.com/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="uws:PerformTransactionArguments">
//               <password>oson</password>
//               <username>oson</username>
//               <amount>200000</amount>
//               <parameters>
//                    <paramKey>clientid</paramKey>
//                    <paramValue>946191400</paramValue>
//               </parameters>
//               <parameters>
//                    <paramKey>terminal_id</paramKey>
//                    <paramValue>654646464</paramValue>
//               </parameters>
//               <serviceId>1</serviceId>
//               <transactionId>47</transactionId>
//               <transactionTime>2018-04-06T11:24:30+05:00</transactionTime>
//          </uws:PerformTransactionArguments>
//     </soapenv:Body>
//</soapenv:Envelope>
    
    std::string phone = req.clientid;
    std::string amount = to_str( req.amount );
    
    std::string login    = acc_.login; // "oson";
    std::string password = acc_.password; //"oson";
    
    std::string address = acc_.url ;
    
    std::string req_xml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
        "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:uws=\"http://uws.provider.com/\"> \n"
        "     <soapenv:Header/> \n"
        "     <soapenv:Body>    \n"
        "          <uws:PerformTransactionArguments xmlns:uws =\"http://uws.provider.com/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:type=\"uws:PerformTransactionArguments\"> \n"
        "                <password>" + login + "</password>    \n"
        "                <username>" + password + "</username> \n"
        "                <amount>"   +  amount  + "</amount>   \n"  // tiyin
        "                <parameters> \n"
        "                     <paramKey>clientid</paramKey> \n"
        "                     <paramValue>"+phone+"</paramValue> \n"
        "                </parameters> \n"
        "                <parameters>  \n"
        "                     <paramKey>terminal_id</paramKey> \n"
        "                     <paramValue>654646464</paramValue> \n"
        "                </parameters> \n"
        "                <serviceId>1</serviceId> \n"
        "                <transactionId>" +  to_str( req.trn_id ) + "</transactionId> \n"
        "                <transactionTime>" + formatted_time_now_iso_T( ) + "+05:00</transactionTime> \n"
        "          </uws:PerformTransactionArguments> \n"
        "     </soapenv:Body> \n"
        "</soapenv:Envelope>  \n"
        ;
    
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    
    req_.method          = "POST"      ;
    req_.content.charset = "UTF-8"     ;
    req_.content.type    = "text/xml"  ;
    req_.content.value   = req_xml     ;
    
    slog.DebugLog("REQUEST: %s", req_xml.c_str());
    std::string resp_xml = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());
    
    if (resp_xml.empty() ) {
        slog.ErrorLog("response is empty!");
        return -1;
    }
    
    
    namespace pt = boost::property_tree;
        
    pt::ptree root;
        
    try
    {
        std::stringstream ss(resp_xml);
        
        pt::read_xml(ss, root);
        
    }
    catch(std::exception & e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        return -1;
    }
    
    //<?xml version='1.0' encoding='UTF-8'?>
    //<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
    //    <soapenv:Body>
    //        <ns2:PerformTransactionResult xmlns:ns2="http://uws.provider.com/">
    //            <errorMsg>Success</errorMsg>
    //            <status>0</status>
    //            <timeStamp>2018-04-06T11:50:05+05:00</timeStamp>
    //            <providerTrnId>4560789872</providerTrnId>
    //        </ns2:PerformTransactionResult>
    //    </soapenv:Body>
    //</soapenv:Envelope>
    
    pt::ptree nil;
    const pt::ptree & tree_result = root.get_child("soapenv:Envelope.soapenv:Body.ns2:PerformTransactionResult", nil) ;
    
    if (tree_result.empty() ) {
        slog.WarningLog("Can't get 'soapenv:Envelope.soapenv:Body.ns2:PerformTransactionResult'   ");
        return -1;
    }
    
    resp.status_value = tree_result.get< int > ("status",  -99989898 );
    resp.status_text  = tree_result.get< std::string >("errorMsg", "-");
    resp.timestamp    = tree_result.get< std::string >("timeStamp", "-");
    resp.provider_trn_id = tree_result.get< std::string >("providerTrnId", "0");
    resp.clientid     = phone;
    
    
    return 0;
}

int Ucell::manager_t::cancel(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    std::string req_xml = 
    "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:uws=\"http://uws.provider.com/\"> \n"
    "   <soapenv:Header/> \n"
    "    <soapenv:Body>   \n"
    "         <uws:CancelTransactionArguments> \n"
    "              <password>oson</password>   \n"
    "              <username>oson</username>   \n"
    "              <serviceId>1</serviceId>    \n"
    "              <transactionId>"+to_str(req.trn_id)+"</transactionId> \n"
    "              <transactionTime>" + req.ts + "</transactionTime> \n"
    "              <parameters> \n"
    "                   <paramKey>terminal_id</paramKey>  \n"
    "                  <paramValue>654646464</paramValue> \n"
    "              </parameters> \n"
    "              <parameters>  \n"
    "                   <paramKey>cancel_reason_code</paramKey> \n"
    "                   <paramValue>1</paramValue> \n"
    "              </parameters> \n"
    "              <parameters>  \n"
    "                   <paramKey>cancel_reason_note</paramKey> \n"
    "                   <paramValue>blabla</paramValue> \n"
    "              </parameters> \n"
    "         </uws:CancelTransactionArguments> \n"
    "    </soapenv:Body> \n"
    "</soapenv:Envelope> \n" ;

    std::string address = acc_.url;
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    
    req_.method          = "POST"      ;
    req_.content.charset = "UTF-8"     ;
    req_.content.type    = "text/xml"  ;
    req_.content.value   = req_xml     ;
    
    
    
    slog.DebugLog("REQUEST: %s", req_xml.c_str());
    std::string resp_xml = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());
    
    if (resp_xml.empty() ) 
    {
        slog.ErrorLog("response is empty!");
        return -1;
    }
    
    
    namespace pt = boost::property_tree;
        
    pt::ptree root;
        
    try
    {
        std::stringstream ss(resp_xml);
        
        pt::read_xml(ss, root);
        
    }
    catch(std::exception & e )
    {
        slog.ErrorLog("Exception: %s", e.what());
        return -1;
    }
    
    //<?xml version='1.0' encoding='UTF-8'?>
    //<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
    //  <soapenv:Body>
    //      <ns2:CancelTransactionResult xmlns:ns2="http://uws.provider.com/">
    //          <errorMsg>Success</errorMsg>
    //          <status>0</status>
    //          <timeStamp>2018-04-18T10:28:00+05:00</timeStamp>
    //         <transactionState>2</transactionState>
    //      </ns2:CancelTransactionResult>
    //  </soapenv:Body>
    //</soapenv:Envelope>
    pt::ptree nil;
    const pt::ptree& result_node = root.get_child("soapenv:Envelope.soapenv:Body:ns2:CancelTransactionResult", nil );
    
    resp.status_value    = result_node.get< int64_t >("status", -99998898);
    resp.status_text     = result_node.get< std::string> ("errorMsg", "-");
    resp.timestamp       = result_node.get< std::string > ("timeStamp", "-");
    resp.provider_trn_id = result_node.get<std::string>("transactionState", "0");
    
   
    return 0;
}

std::string Ucell::manager_t::method_hash(const std::string& method_name ) 
{
 //   SCOPE_LOG(slog);
    std::string name = method_name;
    
    boost::to_lower( name );
    
    std::string string_to_hash = info_.app_key + name;
    
 //   slog.DebugLog("app-key: %s, md5(app-key): %s", info_.app_key.c_str(), oson::utils::md5_hash(info_.app_key).c_str());
    
    return oson::utils::md5_hash( string_to_hash ) ;
}

std::string Ucell::manager_t::make_token(const std::string& path)
{
    SCOPE_LOG(slog);
    
    std::string hash = method_hash(path);
    
    slog.DebugLog("method_hash: %s", hash.c_str());
    
    std::string modulus_raw = oson::utils::decodebase64(info_.key_info.modulus);
    std::string exponenta_raw = oson::utils::decodebase64(info_.key_info.exponenta);
    
    slog.DebugLog("modulus: '%s' \n exponenta: '%s'", info_.key_info.modulus.c_str(), info_.key_info.exponenta.c_str());
    
    slog.DebugLog("modulus_raw length: %zu, exponenta_raw length: %zu", modulus_raw.size(), exponenta_raw.size());
    
    std::string method_hash_utf16_bytes;
    method_hash_utf16_bytes.reserve(hash.size() * 2 ) ;
    for(char c: hash)
    {
        method_hash_utf16_bytes += c;
        method_hash_utf16_bytes += '\0';
    }
    
    //we receive it with raw byte format
    std::vector<unsigned char> encrypted = oson::utils::encryptRSA(method_hash_utf16_bytes, modulus_raw, exponenta_raw ) ;
    
    if (encrypted.empty() ) {
        slog.WarningLog("oson::utils::encryptRSA can't encrypt!");
        return {};
    }
    
    slog.DebugLog("encryptRSA success encrypted. ");
    
    std::string token;
    
    token.reserve( encrypted.size() * 4 ) ;//every byte may up to 3 symbols  and plus  ',' symol
    
    bool end_semicolon = false ;
    for(unsigned char u: encrypted)
    {
        ////////////////////////////////
        end_semicolon = false;
        if (u < 10) {
            token += u + '0';
        } else if (u < 100 ) {
            token += (u / 10 ) + '0';
            token += (u % 10 ) + '0';
        } else { // 3 symbols, i.e.  100 <= u <= 255 
            token += (u / 100) + '0'; u %= 100; // now, u < 100
            token += (u / 10 ) + '0';
            token += (u % 10 ) + '0';
        }
        /////////////////////////////////
        token += ',';
        end_semicolon = true;
    }
    
    if ( end_semicolon ) 
    {
        token.pop_back();
    }
    
    
    return token;
}

int Ucell::manager_t::request_public_key(public_key_info& key_info)
{
    SCOPE_LOG(slog);

    /////////////////////////////////////////////
    std::string login       = info_.app_name ;
    std::string password    = info_.app_pwd  ;
    std::string app_key     = info_.app_key  ;
    /////////////////////////////////////////////
    
    std::string address =  info_.url ; //"https://188.113.225.128:8443/Api/Auth/getPublicKey" ;
    
    namespace http = oson::network::http;
    http::request req_   = http::parse_url( address ) ;
    req_.content.type    = "application/json" ;
    req_.content.value   = "{\"ApplicationName\":\"" + login + "\", \"ApplicationPassword\": \"" + oson::utils::md5_hash( password )+ "\"}" ;
    req_.method          = "POST" ;
    
    slog.DebugLog("REQUEST: %s", req_.content.value.c_str());
    std::string resp_json = sync_http_ssl_request(req_);
    slog.DebugLog("RESPONSE: %s", resp_json.c_str());
    
    if (resp_json.empty()){
        slog.WarningLog("no respnose!");
        return -1;
    }
    
    //////////////////
    
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    
    std::stringstream ss(resp_json);

    pt::read_json(ss, root);
    
    //////////////////////////
    std::string key_mod_exp = root.get< std::string > ("PublicKey" ) ;

    key_info.expiration = root.get< std::string>("PublicKeyExpiration");
    key_info.timestamp  = root.get< std::string>("timeStamp") ;
    
    ////////////////
    ss.str(key_mod_exp);
    pt::ptree xml_root;
    pt::read_xml(ss, xml_root);
        
    ///////////////////
    key_info.modulus = xml_root.get< std::string>("RSAKeyValue.Modulus");
    key_info.exponenta = xml_root.get< std::string>("RSAKeyValue.Exponent");
    
    return 0;
}

int Ucell::manager_t::save_auth_info_to_db(const auth_info& info)
{
    SCOPE_LOG(slog);

   
//{ "webApi" : 
//   {  
//      "url": "https://188.113.225.128:8443/Api/Auth/getPublicKey",	
//      "ApplicationName" : "Oson", 
//      "ApplicationPassword": "2O0l+1s8w0Pa3o1dN2p", 
//      "ApplicationKey": "d3878c4f783a381e", 
//      "PublicKey" : "<RSAKeyValue><Modulus>vqMNcprXcqc26qh90DKFM7FbcwdlQkW3UttsRbFiRnTTKpFl0nM+9PHfhzJoobuMi2pgYr3H/ulaY5utlCtmfbBAesIeI2UUlivpDhP5/GF8zmIwOkQphYkb+6tUMzB/BX5gDD+nB8+ixu9foGU/zdudW7D7LrvKwDzySqUTe+E=</Modulus><Exponent>AQAB</Exponent></RSAKeyValue>",
//      "PublicKeyExpiration": "2018-04-20T15:09:36" ,
//      "timeStamp"    : "2018-04-10T10:09:37.2504027Z"	
//   }
//}
    std::string key_str = "<RSAKeyValue><Modulus>"+info.key_info.modulus+"</Modulus><Exponent>"+info.key_info.exponenta+"</Exponent></RSAKeyValue>" ;
//    namespace pt = boost::property_tree;
//    pt::ptree root;
//    root.put("webApi.url", info.url ) ;
//    root.put("webApi.ApplicationName", info.app_name);
//    root.put("webApi.ApplicationPassword", info.app_pwd);
//    root.put("webApi.ApplicationKey", info.app_key);
//    root.put("webApi.PublicKey", key_str);
//    root.put("webApi.PublicKeyExpiration", info.key_info.expiration);
//    root.put("webApi.timeStamp", info.key_info.timestamp);
    
//    std::stringstream ss;
//    pt::write_json(ss, root);
    
    std::string json_str = 
    "{ \n"
    " \"webApi\" : \n "
    " {   \n"
    "     \"url\" : \""                 + info.url                 + "\" , \n"
    "     \"ApplicationName\" : \""     + info.app_name            + "\" , \n"
    "     \"ApplicationPassword\": \""  + info.app_pwd             + "\" , \n"
    "     \"ApplicationKey\" :  \""     + info.app_key             + "\" , \n"
    "     \"PublicKey\" : \""           + key_str                  + "\" , \n"
    "     \"PublicKeyExpiration\" : \"" + info.key_info.expiration + "\" , \n"
    "     \"timeStamp\" : \""           + info.key_info.timestamp  + "\"   \n"
    " } \n"
    "} " ;
    
            
            
    
    slog.DebugLog("json_str: %s\n", json_str.c_str());
    
    ////////////////////////////////////////
    
    std::string query = "UPDATE merchant_access_info SET api_json = " + escape( json_str ) + " WHERE merchant_id = " + escape(acc_.merchant_id);
    
    DB_T::statement st( oson_this_db ) ;

    st.prepare( query );
    ///////////////////////////////////////
    
    return 0;
}

void Ucell::manager_t::async_info(const request_t& req, handler_type h)
{
    SCOPE_LOG(slog);

    //    User-Agent: Fiddler
    //Host: 10.2.18.143:443
    //Content-Length: 29
    //Content-Type: application/json; charset=utf-8
    //Authorization-Token: 156,3,16,65,180,230,66,181,212,157,159,238,27,146,85,83,141,179,38,245,209,121,182,65,86,237,216,69,80,76,41,37,55,228,23,180,210,16,207,218,95,244,184,82,229,222,41,111,181,151,110,34,202,13,171,250,99,27,27,10,240,143,172,179,103,234,181,113,110,139,14,135,201,68,167,10,139,98,66,243,143,48,207,181,220,209,139,26,40,19,145,206,71,199,51,240,168,174,165,250
    //Authorization-Application: CoinTestApp123
    //
    //{
    //	"Msisdn":"998935101400"
    //}
    
    
    ///////////////////////////
    //{
    //	"StateId":"2",
    //	"StateValue":"Active",
    //	"PrimaryOfferId":"51005339",
    //	"PrimaryOfferValue":"OK",
    //	"IsPostpaid":"0",
    //	"AvailableBalance":"8.38",
    //	"SubscrNo":"36393807",
    //	"AccountNo":"33597529",
    //	"PreviousState":"Suspended(S1)",
    //	"Language":"Uzbek",
    //	"StateChangeDate":"2015.08.11 16:25:36",
    //	"CreationDate":"2015.08.03 14:15:01",
    //	"ActivationDate":"2015.08.03 14:15:16",
    //	"LastTransactionDate":"2015.09.02 11:29:21",
    //	"AccountExpirationDate":"2015.10.02 00:00:00",
    //	"LastRechargeDate":"",
    //	"Accruals":"Only for postpaid",
    //	"Result":"1",
    //	"code":0,
    //	"msg":"OK",
    //	"timeStamp":"2015-09-02T07:20:08.226911Z",
    //	"LastReregistrationDate":""
    //}
    ///////////////////////////////
    
    //http://IP/WebApi_deb/api/Tools/SubscriberGetBillingInfo 
    struct response_handler
    {
        Ucell::request_t    req   ;
        Ucell::response_t   resp  ;
        Ucell::handler_type h     ;
        
        void do_finish( int ret )
        {
            resp.status_value = ret; 
            h ( req, resp )   ;
        }
        
        void operator()(const std::string& resp_json, const boost::system::error_code& ec )
        {
            SCOPE_LOGD(slog);
            
            if (ec)
            {
                slog.WarningLog("Error-code: %d, msg: %s", ec.value(), ec.message().c_str());
                return do_finish( -1 ) ;
                
            }
            
            slog.DebugLog("RESPONSE: %s", resp_json.c_str());
    
            namespace pt = boost::property_tree;
            pt::ptree root;
            std::stringstream ss(resp_json);
            
            try
            {
                pt::read_json(ss, root);
            }
            catch( std::exception& e )
            {
                slog.ErrorLog("Exception: %s", e.what());
                return do_finish(-1);
            }

            
            resp.clientid          = req.clientid;
            resp.status_text       = root.get<std::string>("StateValue", "");
            resp.status_value      = 0;//root.get< int64_t > ( "code", 0) * 0   ; // alwasy 0 must be !).
            resp.timestamp         = root.get< std::string>("timeStamp", "");
            resp.available_balance = root.get< double >("AvailableBalance", 0.0);

            do_finish( 0 );
        }
    }http_h = { req , {}, h } ;
    
    //188.113.225.128:8001
    int ret = check_expiration_auth_info();
    if ( 0 != ret ) {
        slog.WarningLog("auth info expiration can't update!");
        
        return http_h.do_finish( ret );
    }
    
    ////////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    std::string token = make_token("Tools/SubscriberGetBillingInfo");
    //https://188.113.225.128:8443 
    auto req_ = oson::network::http::parse_url(info_.url);
    req_.method = "POST";
    req_.port   = "8443";
    req_.path   =  "/api/Tools/SubscriberGetBillingInfo" ;
    req_.content.charset = "UTF-8";
    req_.content.type    = "application/json";
    req_.content.value   = "{ \"Msisdn\":\""+ req.clientid + "\" }  " ;
    req_.headers.push_back(  "Authorization-Token:  " + token )  ;
    req_.headers.push_back(  "Authorization-Application:   " + info_.app_name )  ; 
    
    
    slog.DebugLog("REQUEST: %s", req_.content.value.c_str());
    std::string resp_json ;
    {
        auto io_service  = oson_merchant_api ->get_io_service() ;
        auto ssl_ctx     = oson_merchant_api ->get_ctx_sslv23() ;
        
        auto c =     oson::network::http::client ::create(io_service, ssl_ctx ) ;
        c->set_request(req_);
        c->set_response_handler( http_h ) ;
        if (req.timeout_millisec != 0 ){
            c->set_timeout( req.timeout_millisec ) ;
        }
        c->async_start() ;
    }
}

void Ucell::manager_t::async_check(const request_t& req, handler_type h)
{
    SCOPE_LOG(slog);
    
    std::string phone    = req.clientid  ;
    std::string login    = acc_.login    ; // "oson";
    std::string password = acc_.password ; // "oson";
    
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
            "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:uws=\"http://uws.provider.com/\"> \n"
            "     <soapenv:Header/> \n"
            "     <soapenv:Body>    \n"
            "         <uws:GetInformationArguments> \n"
            "              <password>"+login+"</password>     \n"
            "               <username>"+password+"</username> \n"
            "               <parameters> \n"
            "                   <paramKey>clientid</paramKey>       \n"
            "                    <paramValue>"+phone+"</paramValue> \n"
            "               </parameters> \n"
            "               <parameters>  \n"
            "                    <paramKey>getInfoType</paramKey>         \n"
            "                    <paramValue>CHK_PERFORM_TRN</paramValue> \n"
            "               </parameters> \n"
            "               <serviceId>1</serviceId>  \n"
            "          </uws:GetInformationArguments> \n"
            "     </soapenv:Body> \n"
            "</soapenv:Envelope>  \n"
            ;
    
    std::string address = acc_.url;
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    
    req_.method          = "POST"      ;
    req_.content.charset = "UTF-8"     ;
    req_.content.type    = "text/xml"  ;
    req_.content.value   = req_xml     ;
    
    
    
    slog.DebugLog("REQUEST: %s", req_xml.c_str());
    struct http_handler_t
    {
        Ucell::request_t    req  ;
        Ucell::handler_type h    ;
        Ucell::response_t   resp ;
        
        void operator()(const std::string& resp_xml, const boost::system::error_code & ec )
        {
            SCOPE_LOGD(slog);
            slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());

            if (  ec  ) 
            {
                slog.ErrorLog("Ec: %d, msg: %s", ec.value(), ec.message().c_str());
                resp.status_value = -1;
                resp.status_text = ec.message();
                return h(req, resp);
            }


            namespace pt = boost::property_tree;

            pt::ptree root;

            try
            {
                std::stringstream ss(resp_xml);

                pt::read_xml(ss, root);

            }
            catch(std::exception & e )
            {
                slog.ErrorLog("Exception: %s", e.what());
                resp.status_value = -1;
                resp.status_text  = e.what();
                return h(req, resp);
            }


        //    
        //    <?xml version='1.0' encoding='UTF-8'?>
        //<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
        //	<soapenv:Body>
        //		<ns2:GetInformationResult xmlns:ns2="http://uws.provider.com/">
        //			<errorMsg>Success</errorMsg>
        //			<status>0</status>
        //			<timeStamp>2018-04-06T13:28:59.353+05:00</timeStamp>
        //		</ns2:GetInformationResult>
        //	</soapenv:Body>
        //</soapenv:Envelope>
        //
        //root@oson2:/var/log# nano ucell_check.xml
        //root@oson2:/var/log# curl -k -X POST 'https://188.113.225.77/ProviderWebService/ProviderWebService' -H 'Content-Type: text/xml; charset=utf-8' -d @ucell_check.xml
        //
        //<?xml version='1.0' encoding='UTF-8'?>
        //<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
        //	<soapenv:Body>
        //		<ns2:GetInformationResult xmlns:ns2="http://uws.provider.com/">
        //			<errorMsg>Subscriber is not found</errorMsg>
        //			<status>302</status>
        //			<timeStamp>2018-04-06T13:29:30.257+05:00</timeStamp>
        //		</ns2:GetInformationResult>
        //	</soapenv:Body>
        //</soapenv:Envelope>root@oson2:/var/log# 

            pt::ptree nil;
            const pt::ptree & tree_result = root.get_child("soapenv:Envelope.soapenv:Body.ns2:GetInformationResult", nil) ;

            if ( tree_result.empty() ) 
            {
                slog.WarningLog("Can't get 'soapenv:Envelope.soapenv:Body.ns2:GetInformationResult'   ");
                resp.status_value = -1;
                resp.status_text  = "Can't get 'soapenv:Envelope.soapenv:Body.ns2:GetInformationResult'";
                return h(req, resp);
            }

            resp.status_value = tree_result.get< int64_t>("status",  -989898 );
            resp.status_text  = tree_result.get< std::string>("errorMsg", "<>");
            resp.timestamp    = tree_result.get< std::string>("timeStamp", "");
            
            return h(req, resp);
        }
    }http_handler = { req, h, {} };

    /////////////////
    {
        auto io_service =  oson_merchant_api -> get_io_service() ;
        auto ssl_ctx    =  oson_merchant_api -> get_ctx_sslv23() ;
        auto c          =  oson::network::http::client::create(io_service, ssl_ctx ) ;
        
        c->set_request( req_ );
        c->set_response_handler( http_handler );
        c->async_start();
    }
}

void Ucell::manager_t::async_pay(const request_t& req, handler_type h )
{
    SCOPE_LOG(slog);
    //    <?xml version="1.0" encoding="UTF-8"?>
//<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/" xmlns:uws="http://uws.provider.com/">
//     <soapenv:Header/>
//     <soapenv:Body>
//          <uws:PerformTransactionArguments xmlns:uws ="http://uws.provider.com/" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:type="uws:PerformTransactionArguments">
//               <password>oson</password>
//               <username>oson</username>
//               <amount>200000</amount>
//               <parameters>
//                    <paramKey>clientid</paramKey>
//                    <paramValue>946191400</paramValue>
//               </parameters>
//               <parameters>
//                    <paramKey>terminal_id</paramKey>
//                    <paramValue>654646464</paramValue>
//               </parameters>
//               <serviceId>1</serviceId>
//               <transactionId>47</transactionId>
//               <transactionTime>2018-04-06T11:24:30+05:00</transactionTime>
//          </uws:PerformTransactionArguments>
//     </soapenv:Body>
//</soapenv:Envelope>
    
    std::string phone    = req.clientid;
    std::string amount   = to_str( req.amount );
    
    std::string login    = acc_.login; // "oson";
    std::string password = acc_.password; //"oson";
    
    std::string address  = acc_.url ;
    
    std::string req_xml = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
        "<soapenv:Envelope xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:uws=\"http://uws.provider.com/\"> \n"
        "     <soapenv:Header/> \n"
        "     <soapenv:Body>    \n"
        "          <uws:PerformTransactionArguments xmlns:uws =\"http://uws.provider.com/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:type=\"uws:PerformTransactionArguments\"> \n"
        "                <password>" + login + "</password>    \n"
        "                <username>" + password + "</username> \n"
        "                <amount>"   +  amount  + "</amount>   \n"  // tiyin
        "                <parameters> \n"
        "                     <paramKey>clientid</paramKey> \n"
        "                     <paramValue>"+phone+"</paramValue> \n"
        "                </parameters> \n"
        "                <parameters>  \n"
        "                     <paramKey>terminal_id</paramKey> \n"
        "                     <paramValue>654646464</paramValue> \n"
        "                </parameters> \n"
        "                <serviceId>1</serviceId> \n"
        "                <transactionId>" +  to_str( req.trn_id ) + "</transactionId> \n"
        "                <transactionTime>" + formatted_time_now_iso_T( ) + "+05:00</transactionTime> \n"
        "          </uws:PerformTransactionArguments> \n"
        "     </soapenv:Body> \n"
        "</soapenv:Envelope>  \n"
        ;
    
    
    oson::network::http::request req_ = oson::network::http::parse_url(address);
    
    req_.method          = "POST"      ;
    req_.content.charset = "UTF-8"     ;
    req_.content.type    = "text/xml"  ;
    req_.content.value   = req_xml     ;
    
    slog.DebugLog("REQUEST: %s", req_xml.c_str());
    struct http_handler_t
    {
        Ucell::request_t     req  ;
        Ucell::response_t    resp ;
        Ucell::handler_type  h    ;
        
        void operator()(const std::string& resp_xml, const boost::system::error_code& ec )
        {
            SCOPE_LOGD(slog);
            if (ec)
            {
                slog.ErrorLog("Ec: %d, msg: %s", ec.value(), ec.message().c_str());
                resp.status_value = ec.value();
                resp.status_text  = ec.message();
                return h(req, resp);
            }
            
            slog.DebugLog("RESPONSE: %.*s", ::std::min<int>(1024, resp_xml.size()), resp_xml.c_str());

            
            namespace pt = boost::property_tree;

            pt::ptree root;

            try
            {
                std::stringstream ss(resp_xml);

                pt::read_xml(ss, root);

            }
            catch(std::exception & e )
            {
                slog.ErrorLog("Exception: %s", e.what());
                resp.status_value = -1;
                resp.status_text = e.what();
                return h(req, resp);
            }

            //<?xml version='1.0' encoding='UTF-8'?>
            //<soapenv:Envelope xmlns:soapenv="http://schemas.xmlsoap.org/soap/envelope/">
            //    <soapenv:Body>
            //        <ns2:PerformTransactionResult xmlns:ns2="http://uws.provider.com/">
            //            <errorMsg>Success</errorMsg>
            //            <status>0</status>
            //            <timeStamp>2018-04-06T11:50:05+05:00</timeStamp>
            //            <providerTrnId>4560789872</providerTrnId>
            //        </ns2:PerformTransactionResult>
            //    </soapenv:Body>
            //</soapenv:Envelope>

            pt::ptree nil;
            const pt::ptree & tree_result = root.get_child("soapenv:Envelope.soapenv:Body.ns2:PerformTransactionResult", nil) ;

            if (tree_result.empty() ) {
                slog.WarningLog("Can't get 'soapenv:Envelope.soapenv:Body.ns2:PerformTransactionResult'   ");
                resp.status_value = -1;
                resp.status_text = "Can't get 'soapenv:Envelope.soapenv:Body.ns2:PerformTransactionResult'   " ;
                
                return h(req, resp);
            }
  
            std::string phone = req.clientid;
  
            resp.status_value = tree_result.get< int > ("status",  -99989898 );
            resp.status_text  = tree_result.get< std::string >("errorMsg", "-");
            resp.timestamp    = tree_result.get< std::string >("timeStamp", "-");
            resp.provider_trn_id = tree_result.get< std::string >("providerTrnId", "0");
            resp.clientid     = phone;


            return h(req, resp);
        }
    }http_handler = { req, {}, h } ;
    
    //std::string resp_xml = sync_http_ssl_request(req_);
    {
        auto c = std::make_shared< oson::network::http::client>( oson_merchant_api -> get_io_service(), oson_merchant_api ->get_ctx_sslv23() ) ;
        c->set_request(req_);
        c->set_response_handler(http_handler);
        c->async_start() ;
    }
}// end Ucell::manager_t::async_pay

void Ucell::manager_t::async_cancel(const request_t& req, handler_type h ) 
{
    SCOPE_LOG(slog);
} 


/*************************************======= TPS ======= *************************************/

namespace tps = oson::backend::merchant::tps;


tps::manager_t::manager_t( const tps::access_t & acc)
  : acc_( acc )
{}

tps::manager_t::~manager_t()
{}

tps::response_t tps::manager_t::check( const tps::request_t & ) 
{
    SCOPE_LOGD(slog);
    slog.WarningLog("TPS does not have an API for check, use info request !");
    
    tps::response_t resp;
    resp.result = 0;
    resp.out_text = "OK";
    
    return resp;
}

tps::response_t tps::manager_t::info ( const tps::request_t & req ) 
{
    SCOPE_LOGD(slog);
    tps::response_t resp;
    
//    POST Request
//    Content-Type application/json
//    Body
//    {
//        "command":"Get_Information",
//        "user_name":"xxxxxx",
//        "password":"xxxxxxx",
//        "account":"tps2621248",
//        "account_type_id":2
//    }
    
   std::string body = "{  "
                       " \"command\": \"Get_Information\",           "
                       " \"user_name\": \"" + acc_.username + "\",   "
                       " \"password\": \""  + acc_.password + "\",   "
                       " \"account\": \""   + req.account   + "\",   "
                       " \"account_type\": " + req.account_type + "  "
                       "} " ;
                         
    oson::network::http::request http_req = oson::network::http::parse_url( acc_.url ); 
    http_req.port            = "http"    ;
    //http_req.path            = "/"     ;
    http_req.method          = "POST"  ;
    http_req.content.charset = "UTF-8" ;
    http_req.content.type    = "application/json" ;
    http_req.content.value   = body    ;
    
    slog.DebugLog("REQUEST: %s", body.c_str());
    
    std::string resp_s = sync_http_request(http_req);
    
    slog.DebugLog("RESPONSE: %s", resp_s.c_str());
    
    if (resp_s.empty() )
    {
        resp.result = -1;
        resp.out_text = "Connection error";
        return resp;
    }
    
//{
//    "result": "0",
//    "out_text": "OK",
//    "timestamp": "20180410144358",
//    "acc_saldo": "-42099.01",
//    "subject_name": "Юсупов Бахром Бахтиёрович"
//}

    namespace pt = boost::property_tree;
    pt::ptree root;
    std::stringstream ss(resp_s);
    
    try
    {
        pt::read_json(ss, root);
    }
    catch(pt::ptree_error & e)
    {
        slog.ErrorLog("read_json failed. ptree-error: %s", e.what());
        resp.result = -1;
        resp.out_text = e.what();
        return resp;
    }
    
    resp.result       = root.get< int64_t     >( "result", -9898998989);
    resp.out_text     = root.get< std::string >( "out_text", "");
    resp.ts           = root.get< std::string >( "timestamp", "");
    resp.acc_saldo    = root.get< std::string >( "acc_saldo", "");
    resp.subject_name = root.get< std::string >( "subject_name", "");
    
    return resp;
}

tps::response_t tps::manager_t::pay  ( const tps::request_t & req ) 
{
    SCOPE_LOGD(slog);
    tps::response_t resp;

//    POST Request
//    Content-Type application/json
//    Body
//    {
//        "command":"Add_Transaction",
//        "user_name":"XXXXXX",
//        "password":"XXXXXX",
//        "txn_id":"4",
//        "account":"tps2632400",
//        "account_type":2,
//        "summ":100000,
//        "data":"20180409121000",
//        "terminal":1
//    }
    
    std::string trn_id_str = to_str(req.trn_id);
    
    std::string body = "{ \n"
                       " \"command\": \"Add_Transaction\",          \n"
                       " \"user_name\": \"" + acc_.username + "\",  \n"
                       " \"password\": \""  + acc_.password + "\",  \n"
                       " \"txn_id\": \""    + trn_id_str    + "\",  \n"
                       " \"account\": \""   + req.account   + "\",  \n"
                       " \"account_type\": "+ req.account_type + ",\n"
                       " \"summ\": " + to_str( req.summ ) + ", \n"
                       " \"date\": \"" + req.date + "\", \n"    //@TPS error, date can't applied, 'data' used instead.
                       " \"data\": \"" + req.date + "\", \n"
                       " \"terminal\": 1 \n"
                       "} " ;
                         
    oson::network::http::request http_req = oson::network::http::parse_url( acc_.url ); 
    http_req.port            = "80"    ;
    http_req.method          = "POST";
    http_req.content.charset = "UTF-8";
    http_req.content.type    = "application/json";
    http_req.content.value   = body ;
    
    slog.DebugLog("REQUEST: %s", body.c_str());
    
    std::string resp_s = sync_http_request(http_req);
    
    slog.DebugLog("RESPONSE: %s", resp_s.c_str());
    
    if (resp_s.empty() )
    {
        resp.result = -1;
        resp.out_text = "Connection error";
        return resp;
    }
    
//    {
//        "result": "0",
//        "out_text": "OK",
//        "oper_id": "1092182",
//        "acc_saldo": "18557.13"
//    }


    namespace pt = boost::property_tree;
    pt::ptree root;
    std::stringstream ss(resp_s);
    
    try
    {
        pt::read_json(ss, root);
    }
    catch(pt::ptree_error & e)
    {
        slog.ErrorLog("read_json failed. ptree-error: %s", e.what());
        resp.result = -1;
        resp.out_text = e.what();
        return resp;
    }
    
    
    resp.result    = root.get< int64_t     > ("result", -99999989);
    resp.out_text  = root.get< std::string > ("out_text", "");
    resp.oper_id   = root.get< std::string > ("oper_id", "0");
    resp.acc_saldo = root.get< std::string > ("acc_saldo", "");
    
    
    return resp;
    
}

void tps::manager_t::async_check( const tps::request_t&, tps::handler_type ) 
{
    
}

void tps::manager_t::async_info ( const tps::request_t&, tps::handler_type ) 
{
    
}

void tps::manager_t::async_pay  ( const tps::request_t&, tps::handler_type ) 
{
    
}

/**********************************************************************************************/
namespace qiwi = oson::backend::merchant::QiwiWallet ;

 
qiwi::manager_t::manager_t( const acc_t & acc ) 
    : m_acc( acc )
{
    SCOPE_LOGD(slog);
}

qiwi::manager_t::~manager_t()
{
    SCOPE_LOGD(slog);
}

void qiwi::manager_t::info(const request_t& req,   response_t& resp)
{
    SCOPE_LOGD(slog);
    slog.WarningLog("Qiwi does not have info request ! ") ;
}

void qiwi::manager_t::check(const request_t& req,  response_t& resp)
{
    SCOPE_LOGD(slog);
//    <?xml version="1.0" encoding="utf-8"?>
//      <request>
//          <request-type>check-user</request-type>
//          <terminal-id>123</terminal-id>
//          <extra name="password">XXXXX</extra>
//          <extra name="phone">79031234567</extra>
//          <extra name="ccy">RUB</extra>
//      </request>
    slog.InfoLog("phone: %s, ccy: %s", req.account.c_str(), req.ccy.c_str() ) ;
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
            "  <request> \n"
            "      <request-type>check-user</request-type> \n"
            "      <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
            "      <extra name=\"password\">"+m_acc.password+"</extra> \n"
            "      <extra name=\"phone\">"+req.account+"</extra> \n"
            "      <extra name=\"ccy\">"+req.ccy+"</extra> \n"
            "  </request> "
            ;

    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    std::string resp_xml = sync_http_ssl_request(http_req);
    
    slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;
    
    if (resp_xml.empty() )
    {
        resp.txn_id = ""; // not determined yet
        resp.status_value = -2;//connection failed.
        resp.status_text  = "Connection failed." ;
        resp.result_code  = 300;
        return ;
    }

    /***  ON SUCCESS */
//    <response>
//    <result-code fatal="false">0</result-code>
//    <exist>1</exist>
//    </response>
    
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    std::stringstream ss( resp_xml ) ;
    
    try
    {
        pt::read_xml(ss, root);
        
    } catch( pt::ptree_error & e ){
        slog.ErrorLog("ptree error: %s", e.what() ) ;
        
        resp.status_value = -3;
        resp.status_text  = e.what() ;
        resp.result_code  = 300 ;
        return ;
    }
    
    const pt::ptree & resp_tree = root.get_child("response") ;
    
    resp.result_code  = resp_tree.get< int64_t >("result-code", 300 ) ;
    resp.fatal_error  = "true" == resp_tree.get< std::string > ("result-code.<xmlattr>.fatal", "false" ) ; 
    resp.status_value = resp_tree.get< int64_t >("exist", 0 ) ;
    resp.status_text  = resp_xml;
    
    
}

void qiwi::manager_t::pay(const request_t& req,    response_t& resp)
{
    SCOPE_LOGD(slog);
    
    
//    <?xml version="1.0" encoding="utf-8"?>
//    <request>
//        <request-type>pay</request-type>
//        <terminal-id>123</terminal-id>
//        <extra name="password">***</extra>
//        <auth>
//            <payment>
//              <transaction-number>12345678</transaction-number>
//              <from>
//                 <ccy>RUB</ccy>
//              </from>
//              <to>
//                <amount>15.00</amount>
//                <ccy>RUB</ccy>
//                <service-id>99</service-id>
//                <account-number>79181234567</account-number>
//              </to>
//           </payment>
//        </auth>
//   </request>
    
    
    slog.InfoLog("trn-id: %ld, ccy: %s, amount: %.12f, account: %s", req.trn_id, req.ccy.c_str(), req.amount, req.account.c_str()  ) ;
    
    std::string trn_id = to_str(req.trn_id);
    std::string amount = to_str(req.amount, 2, false);
    
    std::string req_xml = 
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
                    "<request> \n"
                    "    <request-type>pay</request-type> \n"
                    "    <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
                    "    <extra name=\"password\">"+m_acc.password+"</extra> \n"
                    "    <auth> \n"
                    "        <payment> \n"
                    "          <transaction-number>"+trn_id+"</transaction-number> \n"
                    "          <from> \n"
                    "             <ccy>USD</ccy> \n"
                    "          </from> \n"
                    "          <to> \n"
                    "            <amount>"+amount+"</amount> \n"
                    "            <ccy>"+req.ccy+"</ccy> \n"
                    "            <service-id>99</service-id> \n"
                    "            <account-number>"+req.account+"</account-number> \n"
                    "          </to> \n"
                    "       </payment> \n"
                    "    </auth> \n"
                    "</request> \n"
            ;

    
    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    std::string resp_xml = sync_http_ssl_request(http_req);
    
    slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;
    
    if (resp_xml.empty() )
    {
        resp.txn_id = ""; // not determined yet
        resp.status_value = -2;//connection failed.
        resp.status_text = "Connection failed." ;
        return ;
    }

    /*******  ON SUCCESS ********/
//        <response>
//          <payment status='60' txn_id='6060' transaction-number='12345678' result-code='0'
//                  final-status='true' fatal-error='false' txn-date='02.03.2011 14:35:46' >
//              <from>
//                  <amount>15.00</amount>
//                  <ccy>643</ccy>
//              </from>
//              <to>
//                  <service-id>99</service-id>
//                  <amount>15.00</amount>
//                  <ccy>643</ccy>
//                  <account-number>79181234567</account-number>
//              </to>
//          </payment>
//          <balances>
//              <balance code="428">0.00</balance>
//              <balance code="643">200</balance>
//              <balance code="840">12.20</balance>
//          </balances>
//        </response>
    
    /**** ON ERROR */
//    <response>
//        <result-code fatal="false">300</result-code>
//    </response>
    
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    std::stringstream ss( resp_xml ) ;
    
    try
    {
        pt::read_xml(ss, root);
        
    } catch( pt::ptree_error & e ){
        slog.ErrorLog("ptree error: %s", e.what() ) ;
        
        resp.status_value = -3;
        resp.status_text = e.what() ;
        return ;
    }
    
    const pt::ptree & resp_tree = root.get_child("response") ;
    
    if (resp_tree.count("result-code" ) ) 
    {
    
        const pt::ptree & rc_tree = resp_tree.get_child("result-code");
        
        resp.fatal_error  = "true" == rc_tree.get< std::string >("<xmlattr>.fatal", "false" )   ;
        resp.final_status = false;
        resp.result_code  = rc_tree.get_value< int64_t >();
        
        resp.status_value = resp.result_code  ;
        resp.raw_data  = resp_xml; //all response =).
        //return ;
        
    }
    
    if ( ! resp_tree.count("payment" ) ) 
    {
        slog.WarningLog(" 'payment' NOT FOUND! ") ;
        return ;
    }
    
    
    const pt::ptree & payment_tree = resp_tree.get_child("payment") ;
    
    resp.result_code  = payment_tree.get< int64_t > ("<xmlattr>.result-code", 300 ) ;
    resp.status_value = payment_tree.get< int64_t > ("<xmlattr>.status", -1);
    resp.status_text  = resp_xml ;
    resp.txn_id =   payment_tree.get< std::string >("<xmlattr>.txn_id", "0" ) ;
    resp.final_status = "true" ==  payment_tree.get< std::string > ("<xmlattr>.final-status", "false" ) ;
    resp.fatal_error  = "true" ==  payment_tree.get< std::string > ("<xmlattr>.fatal-error", "false" ) ;
    resp.txn_date     = payment_tree.get< std::string >("<xmlattr>.txn-date", "0");
    resp.status_text  = payment_tree.get< std::string > ("<xmlattr>.message", resp.raw_data ) ;
    
}

void qiwi::manager_t::status(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    
    
//    <?xml version="1.0" encoding="utf-8"?>
//      <request>
//          <request-type>pay</request-type>
//          <extra name="password">XXXXXX</extra>
//          <terminal-id>123</terminal-id>
//          <status>
//              <payment>
//                  <transaction-number>12345678</transaction-number>
//                  <to>
//                      <account-number>79181234567</account-number>
//                  </to>
//              </payment>
//          </status>
//      </request>
    
    slog.InfoLog("trn-id: %ld, account: %s", req.trn_id, req.account.c_str() ) ;
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
            "  <request> \n"
            "      <request-type>pay</request-type> \n"
            "      <extra name=\"password\">"+m_acc.password+"</extra> \n"
            "      <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
            "      <status> \n"
            "          <payment> \n"
            "              <transaction-number>"+to_str(req.trn_id)+"</transaction-number> \n"
            "              <to> \n"
            "                  <account-number>"+req.account+"</account-number> \n"
            "              </to> \n"
            "          </payment> \n"
            "      </status> \n"
            "  </request>"
            ;
            
    
    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    std::string resp_xml = sync_http_ssl_request(http_req);
    
    slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;
    
    if (resp_xml.empty() )
    {
        resp.txn_id        = ""; // not determined yet
        resp.status_value  = -2;//connection failed.
        resp.status_text   = "Connection failed." ;
        return ;
    }
    
    /************ ON SUCCESS */
//        <response>
//           <result-code fatal="false">0</result-code>
//          <payment status='60' transaction-number='12345678' txn_id='759640439' result-
//              сode='0' final-status='true' fatal-error='false' txn-date='12.03.2012 14:24:38'
//          />
//          <balances>
//              <balance code="643">90.79</balance>
//              <balance code="840">0.00</balance>
//          </balances>
//        </response>
    
    
    /*********** ON FAILURE */
//    <response>
//        <result-code fatal="false">300</result-code>
//    </response>
    
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    std::stringstream ss( resp_xml ) ;
    
    try
    {
        pt::read_xml(ss, root);
        
    } catch( pt::ptree_error & e ){
        slog.ErrorLog("ptree error: %s", e.what() ) ;
        
        resp.status_value = -3;
        resp.status_text = e.what() ;
        return ;
    }
    
    if ( ! root.count("response" ) ) {
        slog.ErrorLog("Not found 'response' !");
        return ;
    }
    
    const pt::ptree & resp_tree = root.get_child("response");
    
    resp.result_code = resp_tree.get< int64_t > ("result-code", -1) ;
    resp.fatal_error = "true" == resp_tree.get< std::string > ("result-code.<xmlattr>.fatal", "false" ) ;
    if (resp.result_code  != 0 ) {
        slog.ErrorLog("result-code is not zero: %ld", resp.result_code ) ;
        return ;
    }
    
    
    if ( ! resp_tree.count("payment") ) {
        slog.WarningLog("not found 'payment' sub-tree.") ;
        return ;
    }
    const pt::ptree & pay_tree = resp_tree.get_child("payment");
    
    resp.status_value = pay_tree.get< int64_t >("<xmlattr>.status", -999) ;
    resp.fatal_error  = "true" == pay_tree.get< std::string > ("<xmlattr>.fatal-error", "false");
    resp.final_status = "true" == pay_tree.get< std::string > ("<xmlattr>.final-status", "false" ) ;
    
}

void qiwi::manager_t::balance(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    
//    <request>
//      <request-type>ping</request-type>
//          <terminal-id>44</terminal-id>
//      <extra name="password">password</extra>
//    </request>
    
    std::string req_xml = 
        "<request>"
        "        <request-type>ping</request-type>"
        "        <terminal-id>"+m_acc.terminal_id+"</terminal-id>"
        "        <extra name=\"password\">"+m_acc.password+"</extra>"
        "</request>"
            ;

    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    std::string resp_xml = sync_http_ssl_request(http_req);
    
    slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;
    
    resp.raw_data = resp_xml;
    if (resp_xml.empty() )
    {
        resp.txn_id        = ""; // not determined yet
        resp.status_value  = -2;//connection failed.
        resp.status_text   = "Connection failed." ;
        return ;
    }
    
    /************ ON SUCCESS */
//    <response>
//       <result-code fatal="false">0</result-code>
//       <balances>
//         <balance code="428">100.00</balance>
//         <balance code="643">200.26</balance>
//         <balance code="840">300.00</balance>
//       </balances>
//    </response>
    
     
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    std::stringstream ss( resp_xml ) ;
    
    try
    {
        pt::read_xml(ss, root);
        
    } catch( pt::ptree_error & e ){
        slog.ErrorLog("ptree error: %s", e.what() ) ;
        
        resp.status_value = -3;
        resp.status_text = e.what() ;
        return ;
    }
    
    if ( ! root.count("response" ) ) {
        slog.ErrorLog("Not found 'response' !");
        return ;
    }
    
    const pt::ptree & resp_tree = root.get_child("response");
    
    resp.result_code = resp_tree.get< int64_t >("result-code", -99 ) ;
    
    resp.status_value = resp.result_code ;
    resp.status_text = resp_tree.get< std::string > ("result-code.<xmlattr>.message", "Ok") ;
    
    const pt::ptree & balance_tree = resp_tree.get_child("balances");
    
    for(const auto& bl :  balance_tree ) 
    {
        if (bl.first == "balance" ){
            std::string code = bl.second.get< std::string >("<xmlattr>.code", "0" ) ;
            double value = bl.second.get_value< double > () ;
            
            if (code == "643" ) {
                resp.balance.rub = value ;
            }  else if (code == "840" ) {
                resp.balance.usd = value ;
            } else if (code == "978" ) {
                resp.balance.eur = value ;
            } else {
                slog.WarningLog("Unknown code: '%s'", code.c_str());
            }
        }
    }
    
}

void qiwi::manager_t::async_info(const request_t& req,  handler_type h)
{
    SCOPE_LOGD(slog);
}

void qiwi::manager_t::async_check(const request_t& req, handler_type h)
{
    SCOPE_LOGD(slog);
//    <?xml version="1.0" encoding="utf-8"?>
//      <request>
//          <request-type>check-user</request-type>
//          <terminal-id>123</terminal-id>
//          <extra name="password">XXXXX</extra>
//          <extra name="phone">79031234567</extra>
//          <extra name="ccy">RUB</extra>
//      </request>
    slog.InfoLog("phone: %s, ccy: %s", req.account.c_str(), req.ccy.c_str() ) ;
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
            "  <request> \n"
            "      <request-type>check-user</request-type> \n"
            "      <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
            "      <extra name=\"password\">"+m_acc.password+"</extra> \n"
            "      <extra name=\"phone\">"+req.account+"</extra> \n"
            "      <extra name=\"ccy\">"+req.ccy+"</extra> \n"
            "  </request> "
            ;

    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method          = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
//    std::string resp_xml = sync_http_ssl_request(http_req);
    auto http_handler = [req, h](std::string resp_xml, boost::system::error_code ec ) 
    {
        SCOPE_LOGD(slog);
        qiwi::response_t resp;
        
        slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;
        resp.ec = ec;
        
        if ( static_cast<bool>(ec) )
        {
            resp.txn_id = ""; // not determined yet
            resp.status_value = -2;//connection failed.
            resp.status_text  = "Connection failed." ;
            resp.result_code  = 300;
            return h( req, resp ) ;
        }

        /***  ON SUCCESS */
    //    <response>
    //    <result-code fatal="false">0</result-code>
    //    <exist>1</exist>
    //    </response>

        namespace pt = boost::property_tree;

        pt::ptree root;
        std::stringstream ss( resp_xml ) ;

        try
        {
            pt::read_xml(ss, root);

        } catch( pt::ptree_error & e ){
            slog.ErrorLog("ptree error: %s", e.what() ) ;

            resp.status_value = -3;
            resp.status_text  = e.what() ;
            resp.result_code  = 300 ;
            return h(req, resp);
        }

        const pt::ptree & resp_tree = root.get_child("response") ;

        resp.result_code  = resp_tree.get< int64_t >("result-code", 300 ) ;
        resp.fatal_error  = "true" == resp_tree.get< std::string > ("result-code.<xmlattr>.fatal", "false" ) ; 
        resp.status_value = resp_tree.get< int64_t >("exist", 0 ) ;
        resp.status_text  = resp_xml;
        
        return h(req, resp);
    } ;
    
    /**************/
    auto io_service  = oson_merchant_api -> get_io_service();
    auto ssl_ctx     = oson_merchant_api -> get_ctx_sslv23();
    
    auto http_client = oson::network::http::client::create(io_service, ssl_ctx ) ;
    
    http_client->set_request(http_req);
    http_client->set_response_handler(http_handler);
    
    http_client->async_start() ;
    
}

void qiwi::manager_t::async_pay(const request_t& req, handler_type h ) 
{
    SCOPE_LOGD(slog);
    
    
//    <?xml version="1.0" encoding="utf-8"?>
//    <request>
//        <request-type>pay</request-type>
//        <terminal-id>123</terminal-id>
//        <extra name="password">***</extra>
//        <auth>
//            <payment>
//              <transaction-number>12345678</transaction-number>
//              <from>
//                 <ccy>RUB</ccy>
//              </from>
//              <to>
//                <amount>15.00</amount>
//                <ccy>RUB</ccy>
//                <service-id>99</service-id>
//                <account-number>79181234567</account-number>
//              </to>
//           </payment>
//        </auth>
//   </request>
    
    
    slog.InfoLog("trn-id: %ld, ccy: %s, amount: %.12f, account: %s", req.trn_id, req.ccy.c_str(), req.amount, req.account.c_str()  ) ;
    
    std::string trn_id = to_str(req.trn_id);
    std::string amount = to_str(req.amount, 2, false);
    
    std::string req_xml = 
                    "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
                    "<request> \n"
                    "    <request-type>pay</request-type> \n"
                    "    <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
                    "    <extra name=\"password\">"+m_acc.password+"</extra> \n"
                    "    <auth> \n"
                    "        <payment> \n"
                    "          <transaction-number>"+trn_id+"</transaction-number> \n"
                    "          <from> \n"
                    "             <ccy>USD</ccy> \n"
                    "          </from> \n"
                    "          <to> \n"
                    "            <amount>"+amount+"</amount> \n"
                    "            <ccy>"+req.ccy+"</ccy> \n"
                    "            <service-id>99</service-id> \n"
                    "            <account-number>"+req.account+"</account-number> \n"
                    "          </to> \n"
                    "       </payment> \n"
                    "    </auth> \n"
                    "</request> \n"
            ;

    
    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method          = "POST"     ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    //std::string resp_xml = sync_http_ssl_request(http_req);
    auto http_handler = [req, h](std::string resp_xml, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog); 
        qiwi::response_t resp;
        resp.ec = ec;
        slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;

        if ( static_cast<bool>(ec) )
        {
            resp.txn_id       = ""; // not determined yet
            resp.status_value = -2;//connection failed.
            resp.status_text  = ec.message();
            return h(req, resp);
        }

        /*******  ON SUCCESS ********/
    //        <response>
    //          <payment status='60' txn_id='6060' transaction-number='12345678' result-code='0'
    //                  final-status='true' fatal-error='false' txn-date='02.03.2011 14:35:46' >
    //              <from>
    //                  <amount>15.00</amount>
    //                  <ccy>643</ccy>
    //              </from>
    //              <to>
    //                  <service-id>99</service-id>
    //                  <amount>15.00</amount>
    //                  <ccy>643</ccy>
    //                  <account-number>79181234567</account-number>
    //              </to>
    //          </payment>
    //          <balances>
    //              <balance code="428">0.00</balance>
    //              <balance code="643">200</balance>
    //              <balance code="840">12.20</balance>
    //          </balances>
    //        </response>

        /**** ON ERROR */
    //    <response>
    //        <result-code fatal="false">300</result-code>
    //    </response>

        namespace pt = boost::property_tree;

        pt::ptree root;
        std::stringstream ss( resp_xml ) ;

        try
        {
            pt::read_xml(ss, root);

        } catch( pt::ptree_error & e ){
            slog.ErrorLog("ptree error: %s", e.what() ) ;

            resp.status_value = -3;
            resp.status_text = e.what() ;
            return h(req, resp);
        }

        const pt::ptree & resp_tree = root.get_child("response") ;

        if (resp_tree.count("result-code" ) ) 
        {

            const pt::ptree & rc_tree = resp_tree.get_child("result-code");

            resp.fatal_error  = "true" == rc_tree.get< std::string >("<xmlattr>.fatal", "false" )   ;
            resp.final_status = false;
            resp.result_code  = rc_tree.get_value< int64_t >();

            resp.status_value = resp.result_code  ;
            resp.raw_data  = resp_xml; //all response =).
            //return ;

        }

        if ( ! resp_tree.count("payment" ) ) 
        {
            slog.WarningLog(" 'payment' NOT FOUND! ") ;
            return h(req, resp);
        }

        const pt::ptree & payment_tree = resp_tree.get_child("payment") ;

        resp.result_code  = payment_tree.get< int64_t > ("<xmlattr>.result-code", 300 ) ;
        resp.status_value = payment_tree.get< int64_t > ("<xmlattr>.status", -1);
        resp.status_text  = resp_xml ;
        resp.txn_id       =   payment_tree.get< std::string >("<xmlattr>.txn_id", "0" ) ;
        resp.final_status = "true" ==  payment_tree.get< std::string > ("<xmlattr>.final-status", "false" ) ;
        resp.fatal_error  = "true" ==  payment_tree.get< std::string > ("<xmlattr>.fatal-error", "false" ) ;
        resp.txn_date     = payment_tree.get< std::string >("<xmlattr>.txn-date", "0");
        resp.status_text  = payment_tree.get< std::string > ("<xmlattr>.message", resp.raw_data ) ;
        
        return h(req, resp);
    };//end http_handler
    /*****************************/
    auto io_service = oson_merchant_api -> get_io_service();
    auto ssl_ctx    = oson_merchant_api -> get_ctx_sslv23();
    auto http_client = oson::network::http::client::create( io_service, ssl_ctx );
    
    http_client->set_request(http_req);
    http_client->set_response_handler(http_handler);
    
    http_client->async_start();
}


void qiwi::manager_t::async_status(const request_t& req, handler_type h)
{
    SCOPE_LOGD(slog);

//    <?xml version="1.0" encoding="utf-8"?>
//      <request>
//          <request-type>pay</request-type>
//          <extra name="password">XXXXXX</extra>
//          <terminal-id>123</terminal-id>
//          <status>
//              <payment>
//                  <transaction-number>12345678</transaction-number>
//                  <to>
//                      <account-number>79181234567</account-number>
//                  </to>
//              </payment>
//          </status>
//      </request>
    
    slog.InfoLog("trn-id: %ld, account: %s", req.trn_id, req.account.c_str() ) ;
    
    std::string req_xml = 
            "<?xml version=\"1.0\" encoding=\"utf-8\"?> \n"
            "  <request> \n"
            "      <request-type>pay</request-type> \n"
            "      <extra name=\"password\">"+m_acc.password+"</extra> \n"
            "      <terminal-id>"+m_acc.terminal_id+"</terminal-id> \n"
            "      <status> \n"
            "          <payment> \n"
            "              <transaction-number>"+to_str(req.trn_id)+"</transaction-number> \n"
            "              <to> \n"
            "                  <account-number>"+req.account+"</account-number> \n"
            "              </to> \n"
            "          </payment> \n"
            "      </status> \n"
            "  </request>"
            ;
            
    
    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST" ;
    http_req.content.charset = "UTF-8"    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.value   = req_xml    ;
    
    
    slog.DebugLog( "REQUEST: %s", req_xml.c_str() ) ;
    
    //std::string resp_xml = sync_http_ssl_request(http_req);
    auto io_service = oson_merchant_api -> get_io_service () ;
    auto ctx        = oson_merchant_api -> get_ctx_sslv23()  ;
    
    auto http_handler = [req, h](const std::string& resp_xml, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog);
        
        qiwi::response_t resp ;
        
        slog.DebugLog( "RESPONSE: %s", resp_xml.c_str() ) ;

        if ( static_cast< bool >( ec ) )
        {
            resp.txn_id        = ""; // not determined yet
            resp.status_value  = -2;//connection failed.
            resp.status_text   = "Connection failed." ;
            return h( req, resp );
        }

        /************ ON SUCCESS */
    //        <response>
    //           <result-code fatal="false">0</result-code>
    //          <payment status='60' transaction-number='12345678' txn_id='759640439' result-
    //              сode='0' final-status='true' fatal-error='false' txn-date='12.03.2012 14:24:38'
    //          />
    //          <balances>
    //              <balance code="643">90.79</balance>
    //              <balance code="840">0.00</balance>
    //          </balances>
    //        </response>


        /*********** ON FAILURE */
    //    <response>
    //        <result-code fatal="false">300</result-code>
    //    </response>

        namespace pt = boost::property_tree;

        pt::ptree root;
        std::stringstream ss( resp_xml ) ;

        try
        {
            pt::read_xml(ss, root);

        } catch( pt::ptree_error & e ){
            slog.ErrorLog("ptree error: %s", e.what() ) ;

            resp.status_value = -3;
            resp.status_text = e.what() ;
            return h(req, resp);
        }

        if ( ! root.count("response" ) ) {
            slog.ErrorLog("Not found 'response' !");
            
            return h(req, resp);
        }

        const pt::ptree & resp_tree = root.get_child("response");

        resp.result_code = resp_tree.get< int64_t > ("result-code", -1) ;
        resp.fatal_error = "true" == resp_tree.get< std::string > ("result-code.<xmlattr>.fatal", "false" ) ;
        if (resp.result_code  != 0 ) {
            slog.ErrorLog("result-code is not zero: %ld", resp.result_code ) ;
            return h(req, resp);
        }


        if ( ! resp_tree.count("payment") ) {
            slog.WarningLog("not found 'payment' sub-tree.") ;
            return h(req, resp);
        }
        const pt::ptree & pay_tree = resp_tree.get_child("payment");

        resp.status_value = pay_tree.get< int64_t >("<xmlattr>.status", -999) ;
        resp.fatal_error  = "true" == pay_tree.get< std::string > ("<xmlattr>.fatal-error", "false");
        resp.final_status = "true" == pay_tree.get< std::string > ("<xmlattr>.final-status", "false" ) ;
        
        return h(req, resp) ;
    };
    
    auto http_client = oson::network::http::client::create(io_service, ctx ) ;
    
    http_client->set_request( http_req ); 
    
    http_client->set_response_handler(http_handler);
    
    http_client->async_start() ;
}

/*************************************************************************************************************************************************/

namespace nativepay =   ::oson::backend::merchant::nativepay ;

nativepay::manager_t::manager_t(const acc_t& acc) 
  : m_acc( acc ) 
{
    SCOPE_LOG(slog);
}


nativepay::manager_t::~manager_t()
{
    SCOPE_LOG(slog);
} 

std::string nativepay::manager_t::get_http_request(const std::string& req_str ) 
{
    SCOPE_LOG(slog);
    
    auto http_req = oson::network::http::parse_url(req_str ) ;
    http_req.method  = "GET" ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    std::string resp_xml = sync_http_request(http_req);
    slog.DebugLog("RESPONSE: %s", resp_xml.c_str());
    
    return resp_xml ;
}

void nativepay::manager_t::parse_xml_check(const std::string & resp_xml, response_t& resp ) 
{
    SCOPE_LOG(slog);
    
//    <?xml version="1.0" encoding="UTF-8"?>
//    <response>
//    <prv_txn>1234567</prv_txn>
//    <result>0</result>
//    <comment>OK</comment>
//    <currency>USD</currency>
//    <rate>331.15</rate>
//    <amount>0.60</amount>
//    </response>
    resp.status = 0;
    resp.status_text = "OK";
    
    
    if ( resp_xml.empty() ) {
        resp.status = Error_communication;
        resp.status_text = "response text is empty";
        return ;
    }
    
    namespace pt = boost::property_tree ;
    pt::ptree root;
    std::stringstream ss(resp_xml);
    
    try
    {
        pt::read_xml(ss, root ) ;
        
        const pt::ptree& pt_resp = root.get_child("response");
        
        resp.prv_txn = pt_resp.get< std::string > ("prv_txn", "0");
        
        resp.result = pt_resp.get< int32_t > ("result" , 9999 ) ;
        
        resp.comment = pt_resp.get< std::string > ("comment", "");
        
        resp.currency = pt_resp.get< std::string > ("currency", "x");
        
        resp.rate = pt_resp.get< double > ("rate", 0.0);
        
        resp.amount = pt_resp.get< double > ("amount", 0.0) ;
        
    }catch(std::exception & e ) {
        resp.status = Error_internal;
        resp.status_text = e.what();
        return  ;
    }
}

void nativepay::manager_t::parse_xml_pay(const std::string & resp_xml, response_t& resp ) 
{
    SCOPE_LOG(slog);
    
//    <?xml version="1.0" encoding="UTF-8"?>
//    <response>
//    <txn_id>1234567</txn_id>
//    <prv_txn>2016</prv_txn>
//    <sum>180.00</sum>
//    <result>0</result>
//    <comment>OK</comment>
//    <currency>USD</currency>
//    <rate>331.15</rate>
//    <amount>0.54</amount>
//    </response>
    slog.DebugLog("pay and check responses are very similar, so call parse_xml_check ! " ) ;
    return parse_xml_check(resp_xml, resp ) ;
}

void nativepay::manager_t::parse_xml_balance(const std::string & resp_xml, response_t& resp) 
{
    SCOPE_LOG(slog);
    
//    <response>
//    <agent>XX</agent>
//    <agent_name>TestAgent</agent_name>
//    <agent_balance_sum>30488.3100</agent_balance_sum>
//    <currency_name>KZT</currency_name>
//    <result>OK. Операция прошла успешно</result>
//    </response>
    resp.status = 0;
    resp.status_text = "OK";
    
    
    if ( resp_xml.empty() ) {
        resp.status = Error_communication;
        resp.status_text = "response text is empty";
        return ;
    }
    
    namespace pt = boost::property_tree ;
    pt::ptree root;
    std::stringstream ss(resp_xml);
    
    try
    {
        pt::read_xml(ss, root ) ;
        
        const pt::ptree& pt_resp = root.get_child("response");
        
        resp.balance.balance   = pt_resp.get< double > ("agent_balance_sum", 0.0);
        resp.balance.currency  = pt_resp.get< std::string > ("currency_name", "x" ) ;
        resp.status_text       = pt_resp.get< std::string > ( "result" ,  resp.status_text ) ;
    }catch(std::exception & e ) {
        resp.status = Error_internal;
        resp.status_text = e.what();
        return  ;
    }
    
}

void nativepay::manager_t::parse_xml( const std::string & resp_xml, response_t& resp, int cmd )
{
    SCOPE_LOG(slog);
    slog.InfoLog(" cmd: %d ", cmd);
    switch(cmd)
    {
        case cmd_none   : break;
        case cmd_balance: return parse_xml_balance(resp_xml, resp ) ;
        case cmd_check  : return parse_xml_check(resp_xml, resp ) ;
        case cmd_pay    : return parse_xml_pay(resp_xml, resp ) ;
        default         : break;
    }
}

void nativepay::manager_t::check(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    
    //2.  Пример запроса на проверку состояния абонента
    // http://servername/api/gate2/service?command=check&account=0957835959
    std::string path = req.service == "tester" ? "/api/gate2"  : "/api/gate" ;
    std::string req_str  = m_acc.url + path + req.service + "?command=check&account="+req.account+"&sum="+to_str(req.sum, 2, false) ;
    std::string resp_xml = get_http_request( req_str ) ;
    
    parse_xml(resp_xml, resp , cmd_check ) ;
}

void nativepay::manager_t::balance(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    //http://servername/Finance/Agent/Balance?command=check&hash=XXXXXXXXXXX
    std::string req_str = m_acc.url + "/Finance/Agent/Balance?command=check&hash=" + req.hash ;
    std::string resp_xml = get_http_request(req_str);
    parse_xml(resp_xml, resp, cmd_balance ) ;
}

void nativepay::manager_t::pay(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
    std::string path = req.service == "tester" ? "/api/gate2"  : "/api/gate" ;
    //http://servername/api/gate2/service?command=pay&txn_id=1234567&txn_date=20170815120133&account=0957835959&sum=180.00&sum2=200.00&payer_info=Иванов_Иван_Удв_123456789
    std::string req_str = m_acc.url + path + req.service + "?command=pay&txn_id=" +to_str(req.txn_id) + 
                            "&txn_date="+req.txn_date + "&account="+req.account + 
                            "&sum=" + to_str(req.sum, 2, false)  ;
    std::string resp_xml = get_http_request(req_str);
    
    parse_xml(resp_xml, resp, cmd_pay);
    
}

void nativepay::manager_t::async_check(const request_t& req, handler_t h)
{
    SCOPE_LOGD(slog);
    
    
}

void nativepay::manager_t::async_balance(const request_t& req, handler_t h)
{
    SCOPE_LOGD(slog);
}

void nativepay::manager_t::async_pay(const request_t& req, handler_t h)
{
    SCOPE_LOGD(slog);
    
    std::string path = req.service == "tester" ? "/api/gate2"  : "/api/gate" ; 
    
    
    //http://servername/api/gate2/service?command=pay&txn_id=1234567&txn_date=20170815120133&account=0957835959&sum=180.00&sum2=200.00&payer_info=Иванов_Иван_Удв_123456789
    std::string req_str = m_acc.url + path + req.service + "?command=pay&txn_id=" +to_str(req.txn_id) + 
                            "&txn_date="+req.txn_date + "&account="+req.account + 
                            "&sum=" + to_str(req.sum, 2, false)  ;
    
    auto http_req = oson::network::http::parse_url(req_str ) ;
    http_req.method  = "GET" ;
    
    slog.DebugLog("REQUEST: %s", req_str.c_str());
    
    ///////////////////////////////////////////////////////////////////////////////
    auto http_handler = [ req, h ] (const std::string & resp_xml, boost::system::error_code ec ) 
    {
        SCOPE_LOGD(slog);
        slog.DebugLog("RESPONSE: %s", resp_xml.c_str());
        nativepay::response_t resp;
        if ( static_cast<bool>(ec) ) {
            
            resp.status = ec.value();
            resp.status_text = ec.message();
            slog.ErrorLog("ec: %d, msg: %s ", ec.value(), resp.status_text.c_str()) ;
            return h( req, resp ) ;
        }
        
        nativepay::manager_t::parse_xml(resp_xml, resp , cmd_pay ) ;
        
        return h(req, resp ) ;
    };
    
    /////////////////////////////////////
    auto io_service = oson_merchant_api -> get_io_service() ;
    
    auto http_client = std::make_shared< oson::network::http::client_http > ( io_service );
    
    http_client->set_request(http_req);

    http_client->set_response_handler(http_handler);
    
    http_client->async_start() ;
}




/**************************************************************************************************************/
/*********************************** MTS  *********************************************************************/

namespace MTS = oson::backend::merchant::MTS;

MTS::manager_t::manager_t(acc_t const& acc)
 : m_acc(acc)
{}


MTS::manager_t::~manager_t()
{
    
}

void MTS::manager_t::send_request(const request_t& req, response_t& resp)
{
    SCOPE_LOGD(slog);
     
    std::string req_xml = to_xml( req );
    if (req_xml.empty()){
        slog.WarningLog("Can't create request");
        resp.espp_type = response_t::ESPP_none;
        return ;
    }
    
    std::string resp_xml = sync_send_request(req_xml, req);
    
    from_xml(resp_xml, resp);
}

 
std::string MTS::manager_t::sync_send_request( std::string const& req_xml, const request_t& req ) 
{
    SCOPE_LOG(slog);
    
    const std::string& url  =  m_acc.url;//   "https://10.160.18.195/PaymentProcessingXMLEndpointTestProxy/TestPaymentProcessorDispatcher" ;
    auto http_req = oson::network::http::parse_url( url );
    http_req.method          = "POST"     ;
    http_req.content.value   = req_xml    ;
    http_req.content.type    = "text/xml" ;
    http_req.content.charset = "UTF-8"    ;
    
    if (req.espp_type == req.ESPP_0104050 ){
        http_req.headers.push_back("f_01: 5004"); //5001 - text format, 5004 XML format v5_01
        http_req.headers.push_back("f_02: 0");    // 0 - no archives, 1 - ZIP,  2 - GZIP.
        http_req.headers.push_back("f_03: " + to_str( req.trn_id ) ) ; // a unique id.
    }
    
    std::string resp_xml;
    {  
        auto ctx = std::make_shared< boost::asio::ssl::context>( boost::asio::ssl::context::sslv3  );
         
        //ctx->use_certificate_file( "/etc/oson/CA/UMS/ums_cert.csr",  boost::asio::ssl::context_base::file_format::pem   );
        boost::system::error_code ec;
        ctx->load_verify_file( m_acc.cert_info.verify_file, ec ) ;
        if (ec ) {
            slog.ErrorLog("load verify file failed. error-code: %d   message: %s ", ec.value(), ec.message().c_str());
        }
        
        ctx->use_certificate_file( m_acc.cert_info.public_key_file,  boost::asio::ssl::context_base::file_format::pem, ec );
        if (ec){
            slog.ErrorLog("use_certificate_chain_fail failed. error-code: %d  message: %s ", ec.value(), ec.message().c_str());
        }
        
        ctx->use_rsa_private_key_file( m_acc.cert_info.private_key_file, boost::asio::ssl::context_base::file_format::pem, ec  ) ;
        
        if (ec){
            slog.ErrorLog("exception when use_rsa_private_key_file: %s", ec.message().c_str());
            return resp_xml;
        }
        
        ctx->set_verify_mode(ctx->verify_fail_if_no_peer_cert | ctx->verify_peer);
        
        auto io_service = std::make_shared< boost::asio::io_service > () ;

        scoped_register scope = make_scoped_register( *io_service);


        auto http_client = std::make_shared< oson::network::http::client >(io_service, ctx);

        http_client->set_request(http_req);
        
        http_client->set_verify_mode(boost::asio::ssl::context::verify_peer | boost::asio::ssl::context::verify_fail_if_no_peer_cert);
        
        http_client->async_start();

        boost::system::error_code ignored_ec;

        io_service->run( ignored_ec );

        std::string body =   http_client->body();

        resp_xml = body;
        
        slog.InfoLog("RESPONSE: %s", body.c_str());
          
    }
    
    return resp_xml;
}

std::string MTS::manager_t::make_sign(const request_t& req ) 
{
    SCOPE_LOG(slog);
    
    std::string sign;
    if (req.espp_type == request_t::ESPP_0104090 ) 
    {
//        <Телефон>?<разделитель>
//        <Сумма><разделитель>
//        <Валюта><разделитель>
//        <Тип платежного инструмента><разделитель>
//        <Номер платежного инструмента>?<разделитель>
//        <Внешний номер платежа><разделитель>
//        <Дата операции><разделитель>
//        <ПЦ Представителя><разделитель>
//        <Номер терминала><разделитель>
//        <Тип терминала><разделитель>
//        <Дата платежа по терминалу><разделитель>
//        <Л/с><разделитель><Код провайдера>?
//            
//        <разделитель> = "&";
//      
        //1) <Телефон>  phone
        //2) <Сумма>   sum
        //3) <Валюта>  860
        //4) <Тип платежного инструмента>   21
        //5) <Номер платежного инструмента> <empty string>
        //6) <Внешний номер платежа> trn_id
        //7) <Дата операции>   date
        //8) <ПЦ Представителя> 86001561
        //9) <Номер терминала> Oson-Raiden.1234567
        //10) <Тип терминала>   1 
        //11) <Дата платежа по терминалу> date
        //12) <Л/с>   <empty strng>
        //13) <Код провайдера>  MTS
        
        std::string const& phone  = req.phone ;
        std::string const sum     = to_str( req.sum, 2, false ) ;
        std::string const trn_id = to_str( req.trn_id ) ;
        std::string const& date   = req.ts ;
        std::string const& date_tz = req.ts_tz;
        
        std::string raw = phone  + "&" +  // <Телефон>
                          sum    + "&" +  // <Сумма>
                          "860"  + "&" +  // <Валюта>
                          "21"   + "&" +  // <Тип платежного инструмента>
                          ""     + "&" +  //<Номер платежного инструмента> 
                          trn_id + "&" +  // <Внешний номер платежа>
                          date   + "&" +  // <Дата операции>
                          "86001561" + "&" +  //<ПЦ Представителя>
                          "Oson-Raiden.1234567" + "&"+ //<Номер терминала>
                          "1"    + "&"  +  //<Тип терминала>
                          date_tz+ "&" +  // <Дата платежа по терминалу>
                          ""     + "&" +  //<Л/с>
                          "MTS" ;         // <Код провайдера>
                          
        
        slog.InfoLog("raw: %s", raw.c_str());
        
        const std::string& private_key_file = m_acc.cert_info.private_key_file;  // "/etc/oson/CA/UMS/ums_privatekey.pem";
        
        //raw = oson::utils::md5_hash(raw);
        int err_code = 0;
        std::tie(sign, err_code)  = oson::utils::sign_md5(raw, private_key_file, "");//no password
        
        if (err_code )
        {
            slog.ErrorLog("sign md5 failed with %d code", err_code ) ;
        }
    }
    else 
    {
        slog.WarningLog("Not implemented yet!");
    }
    
    return sign;
}    
 

std::string MTS::manager_t::to_xml(const request_t& req)
{
    SCOPE_LOG(slog);
    std::string result;
    switch(req.espp_type)
    {
        case request_t::ESPP_none: ;break;
        case request_t::ESPP_0104010: // check
        {
            std::string const& phone = req.phone;
            std::string const sum = to_str(req.sum, 2, false);
            
            result = 
            "<?xml version = \"1.0\" encoding = \"UTF-8\"?> \n"
            "   <ESPP_0104010   xmlns = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" \n"
            "                   xmlns:espp-constraints = \"http://schema.mts.ru/ESPP/Core/Constraints/v5_01\" \n"
            "                   xmlns:xsi = \"http://www.w3.org/2001/XMLSchema-instance\" \n"
            "                   xsi:schemaLocation = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd \n"
            "                   http://schema.mts.ru/ESPP/Core/Constraints/v5_01 ESPP_Core_Constraints_v5_01.xsd\" \n"
            "        a_01 = \"60\"> \n"
            "                       \n"        
            "         <!--Телефон--> \n"
            "         <f_01 xsi:type = \"espp-constraints:PHN_CODE_fmt_02\">"+phone+"</f_01> \n"
            "         <!--Сумма-->  \n"
            "         <f_02>" + sum + "</f_02> \n"
            "         <!--Валюта--> \n"
            "         <f_03 xsi:type = \"espp-constraints:CUR_fmt_01\">860</f_03> \n"
            "         <!--Тип платежного инструмента--> \n"
            "         <f_04>21</f_04> \n"
            "         <!--Номер терминала--> \n"
            "         <f_05>Oson-Raiden.1234567</f_05> \n"
            "         <!--Тип терминала--> \n"
            "         <f_06>1</f_06> \n"
            "          <!--АС Агента--> \n"
            "         <f_07>86001561</f_07> \n"
            "         <!--Договор--> \n"
            "          <f_08>61</f_08> \n"
            //"         <!--Л/с абонента--> \n"
            //"         <!-- <f_09>123456789012345</f_09> --> \n"
            "         <!--Код провайдера абонента--> \n"
            "         <f_10>MTS</f_10> \n"
            "</ESPP_0104010> \n" 
            ;
        }
        break;
        case request_t::ESPP_0104090: // pay
        {
            std::string const& phone    = req.phone;
            std::string const sum       = to_str(req.sum, 2, false);
            std::string const  check_id = to_str( req.check_id ) ;
            std::string const  trn_id   = to_str( req.trn_id ) ;
            std::string const& date     = req.ts   ;
            std::string const& date_tz  = req.ts_tz ;
            std::string const sign      = make_sign(req);
            response_t tmp_resp;
            from_xml(req.raw_info, tmp_resp);
            if (tmp_resp.espp_type  != response_t::ESPP_1204010 )
            {
                slog.WarningLog("Not expected result. req.raw_info: %s ", req.raw_info.c_str());
                return result;
            }
            std::string const& code_operator = tmp_resp.code_operator;
            std::string const& ecpp_trn_id   = tmp_resp.espp_trn_id ;
            
            result = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
            "<ESPP_0104090	xmlns = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" \n"
            "			xmlns:espp-constraints = \"http://schema.mts.ru/ESPP/Core/Constraints/v5_01\" \n"
            "			xmlns:xsi = \"http://www.w3.org/2001/XMLSchema-instance\" \n"
            "			xsi:schemaLocation = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd "
            "								  http://schema.mts.ru/ESPP/Core/Constraints/v5_01 ESPP_Core_Constraints_v5_01.xsd\"> \n"
            "\n "
            "<!--Телефон--> \n"
            "<f_01 xsi:type = \"espp-constraints:PHN_CODE_fmt_02\">"+phone+"</f_01> \n"
            "<!--Сумма-->\n"
            "<f_02>"+sum+"</f_02>\n"
            "<!--Валюта-->\n"
            "<f_03 xsi:type = \"espp-constraints:CUR_fmt_01\">860</f_03>\n"
            "<!--Тип платежного инструмента-->\n"
            "<f_04>21</f_04>\n"
            "<!--Номер платежного инструмента не указан, т.к. платеж совершен наличными--> \n"
            "<!--Номер квитанции(чека)--> \n"
            "<f_06>" + check_id + "</f_06>   \n"
            "<!--Внешний номер платежа--> \n"
            "<f_07>"+trn_id+"</f_07> \n"
            "<!--Дата операции--> \n"
            "<f_08>"+date+"</f_08> \n"
            "<!--Комментарий не указан--> \n"
            "<!--Номер транзакции ЕСПП--> \n"
            "<f_10>"+ecpp_trn_id+"</f_10> \n"
            "<!--АС Представителя-->\n"
            "<f_11>86001561</f_11> \n"
            "<!--Номер терминала--> \n"
            "<f_12>Oson-Raiden.1234567</f_12> \n"
            "<!--Тип терминала--> \n"
            "<f_13>1</f_13> \n"
            //"<!--Идентификатор кассира--> \n"
            //"<f_14>99</f_14> \n"
            "<!--Код \"домашнего\" оператора--> \n"
            "<f_15>"+code_operator+"</f_15> \n"
            "<!--Дата платежа по терминалу--> \n"
            "<f_16>"+date_tz+"</f_16> \n"
            "<!--КК, подтверждающий прием платежа--> \n"
            "<f_18>"+sign+"</f_18> \n"
            "<!--Договор--> \n"
            "<f_19>61</f_19> \n"
            //"<!--Л/с абонента--> \n"
            //"<!-- <f_20>123456789012345</f_20> --> \n"
            "<!--Код провайдера--> \n"
            "<f_21>MTS</f_21> \n"
            "</ESPP_0104090> \n"
            ;

        }
        break;
        
        case request_t::ESPP_0104085: // pay_status
        {
            std::string const& espp_trn_id =req.espp_trn_id;
            result = 
            "<?xml version = \"1.0\" encoding = \"UTF-8\"?>\n"
            "<ESPP_0104085	xmlns = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" \n"
            "    xmlns:xsi = \"http://www.w3.org/2001/XMLSchema-instance\" \n"
            "    xsi:schemaLocation = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd\" a_01 = \"120\">\n"
            "     <!--Атрибут \"a_01\" - период обработки (в секундах)-->\n"
            "\n"
            "    <!--Номер транзакции ЕСПП. Поиск осуществляется по номеру транзакции ЕСПП-->\n"
            "    <f_02>"+espp_trn_id+"</f_02>\n"
            "    <!--ПЦ Представителя-->\n"
            "    <f_03>86001561</f_03>\n"
            "    <!--Договор-->\n"
            "    <f_04>61</f_04> \n"
            "</ESPP_0104085>"
                    ;
        }
        break;
        
        case request_t::ESPP_0104213: // cancel pay
        {
            result = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?> \n"
            "<ESPP_0104213 xmlns=\"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" "
            "   xmlns:ns2=\"http://schema.mts.ru/ESPP/Core/Protocol/Errors/v5_01\" "
            "   xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
            "   xsi:schemaLocation=\"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd http://schema.mts.ru/ESPP/Core/Constraints/v5_01 ESPP_Core_Constraints_v5_01.xsd\"> "
            "    <!-- Telefon --> \n"        
            "    <f_01 xmlns:ns4=\"http://schema.mts.ru/ESPP/Core/Constraints/v5_01\" xsi:type=\"ns4:PHN_CODE_fmt_02\">" + req.phone + "</f_01> \n"
            "    <!-- Transaction-id(MTS) --> \n"
            "    <f_02>"+req.espp_trn_id+"</f_02> \n"
            "    <!-- Date of operation --> \n"        
            "    <f_03>"+req.ts_tz+"</f_03> \n"
            "    <!-- Amount of payment --> \n"        
            "    <f_04>"+to_str(req.sum,2,false)+"</f_04> \n"
            "    <!-- Currency --> \n"        
            "    <f_05 xmlns:ns4=\"http://schema.mts.ru/ESPP/Core/Constraints/v5_01\" xsi:type=\"ns4:CUR_fmt_01\">860</f_05> \n"
            "    <!-- Type of payment --> \n"        
            "    <f_06>21</f_06> \n"
            "    <!-- Kod ASPS predstavitelya --> \n"        
            "    <f_07>86001561</f_07> \n"
            "    <!-- Kod dogovora --> \n"        
            "    <f_08>61</f_08> \n"
            "</ESPP_0104213> " ;
        }
        break;
        case request_t::ESPP_0104050: // register 
        {
            result = 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<ESPP_0104050	xmlns = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" \n"
            "    xmlns:xsi = \"http://www.w3.org/2001/XMLSchema-instance\" \n"
            "    xsi:schemaLocation = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd\"> \n"
            "     <!-- Реестр для проведения сверки --> \n"
            "     <f_04>" + req.reestr_b64 + "</f_04> \n"
            "</ESPP_0104050>"
                    ;
            
        }
        break;
        case request_t::ESPP_0104051:
        {
            std::string reestr_id = req.espp_trn_id ;
            
            result = 
            "<?xml version = \"1.0\" encoding = \"UTF-8\"?> "
            "<ESPP_0104051  xmlns = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01\" "
            "  xmlns:xsi = \"http://www.w3.org/2001/XMLSchema-instance\" "
            "  xsi:schemaLocation = \"http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd\"> \n"
            "  <f_01>5004</f_01> \n"
            "  <f_02>0</f_02> \n"
            "  <f_03>"+reestr_id+"</f_03> \n"
            "  <f_04>86001561</f_04> \n"
            "</ESPP_0104051> \n" ;
            
        }
        break;
        default: break;
    }     
    
    return result;
 }
 

std::string MTS::request_t::make_reestr_b64(const Purchase_list_T& list, const std::string & from_date, const std::string & to_date )
{
    SCOPE_LOG(slog);
    
    std::string b64, zip;
    std::string raw;
    std::time_t now = std::time(0);
    int64_t id = (int64_t)now;
    
    //@Remove it when real-mode.
    if ( 1 /*test-mode*/ ) 
        id = 10000000001 ;
    
    std::string id_s = to_str(id);
    
    
    raw = 
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" 
    "<comparePacket xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n" 
    "               xmlns:constraints=\"http://shemas.mts.ru/ESPP/StandardConstraints/v2_0\"\n" 
    "               xsi:schemaLocation=\"http://shemas.mts.ru/ESPP/StandardConstraints/v2_0 ESPP_StandardConstraints_v2_0.xsd  "
    " http://shemas.mts.ru/ESPP/ApplicationProtocol/Compare/v1_0 ESPP_ApplicationProtocol_Compare_v1_0.xsd\"\n" 
    "               xmlns=\"http://shemas.mts.ru/ESPP/ApplicationProtocol/Compare/v1_0\" id=\"" + id_s + "\">\n" 
    "<summary> \n "
    "        <contract>61</contract> \n"
    "        <comparePeriod>  \n"
    "            <from>"+from_date+"</from> \n "
    "            <to>"+to_date+"</to> \n " 
    "        </comparePeriod>  \n"
    "        <totalAmountOfPayments>"+to_str(list.count)+"</totalAmountOfPayments> \n"
    "        <currency xsi:type=\"constraints:CUR_fmt_01\">860</currency>\n"            
    "            <totalSum>"+to_str( list.sum / 100.0, 2, false ) + "</totalSum> \n"
    "            <totalDebt>0.00</totalDebt> \n"
    "</summary>  \n"
    "<payments> \n"
           ;
//    "    <p id = \"1\">138020050;17650201;20051201112151000;1000000000000944695;10000000000.00000001;9026475359;19029;102297;10.00;810;2;1.00</p>\n
//    "    <p id = \"2\">138020050;17650401;20051201112439000;1000000000000944696;10000000000.00000001;9026475359;19029;102297;10.00;810;2;1.00</p>\n
//    "    <p id = \"3\">138020050;17651401;20051201122026000;1000000000000944701;10000000000.00000001;9026475359;19029;102297;10.00;810;2;1.00</p>\n
    id = 0;
    static const char separator = ';';
    auto raw_p_add = [&raw](const std::string & s){ raw += s; raw += separator; } ;
    
    for(const auto & p : list.list )
    {
        ++id;
        raw += "<p id=\"" + to_str(id) + "\">" ;
//        <Код ПЦ Представителя><разделитель>
//        <Номер транзакции ЕСПП><разделитель>
//        <Дата операции><разделитель>
//        <Внешний номер платежа><разделитель>
//        <Номер терминала><разделитель>
//        <Телефон><разделитель>
//        <Код домашнего оператора><разделитель>
//        <Л/с абонента><разделитель>
//        <Сумма><разделитель>
//        <Валюта><разделитель>
//        <Тип платежного инструмента><разделитель>
//        <Сумма задолженности>
        
        std::string trn_id = to_str(p.oson_paynet_tr_id) ;
        
             
        
        std::string phone  = p.login;
        if (phone.length() == 9) phone ="998" + phone;
        
        std::string sum = to_str(p.amount/100.0, 2, false);
        std::string sum_debit;// = to_str(3.00, 2, false);
        std::string lc_user;
        std::string ts ;
        std::string code_op;
        
        std::vector< std::string > vec;
        
        boost::algorithm::split(vec, p.merch_rsp, boost::is_any_of(";"));
        if (vec.size()<3)vec.resize(3);
        
        code_op = vec[ 0 ] ;
        sum_debit     = vec[ 1 ] ;
        ts            = vec[ 2 ] ;
        
        //if(sum_debit.empty())
        sum_debit = "0.00";
        
        lc_user = "";//@Note litsevoy schet doljen bit pustoy!
        
        //remove all non-digits
        ts.erase( std::remove_if(ts.begin(), ts.end(), [](int c){ return ! ::isdigit(c); } ), ts.end() ); 
        if  (ts.size() < 17)
            ts.append( static_cast< std::string::size_type>( 17 - ts.size() ) , '0' ) ;
        
        raw_p_add("86001561");            //1. <Код ПЦ Представителя>
        raw_p_add( p.paynet_tr_id ) ;     //2. <Номер транзакции ЕСПП>
        raw_p_add( ts ) ;                 //3. <Дата операции>
        raw_p_add( trn_id ) ;             //4. <Внешний номер платежа>
        raw_p_add("Oson-Raiden.1234567"); //5. <Номер терминала>
        raw_p_add(phone);                 //6. <Телефон>
        raw_p_add(code_op);               //7. <Код домашнего оператора>
        raw_p_add(lc_user);               //8. <Л/с абонента>
        raw_p_add(sum)  ;                 //9. <Сумма>
        raw_p_add("860") ;                //10. <Валюта>
        raw_p_add("21");                  //11. <Тип платежного инструмента>
        raw_p_add(sum_debit) ;            //12. <Сумма задолженности>
        
        //@Note needn't last separator.
        if(! raw.empty() && raw.back()== separator)
            raw.pop_back();
        
        raw += "</p>\n" ;
    }
    raw += 
    "</payments> \n"
    "</comparePacket>\n"
            ;
    
    slog.InfoLog("raw: %.*s \n", ::std::min<int>(4096, raw.length()), raw.c_str());
    //zip = oson::utils::make_zip(raw);
    b64 = oson::utils::encodebase64( raw );
    return b64;
}

void MTS::manager_t::from_xml( std::string const& resp_xml, response_t& resp)
{
    SCOPE_LOG(slog);
    
    resp.espp_type = response_t::ESPP_none ;
    resp.raw = resp_xml;
    
    namespace pt = boost::property_tree ;
    
    if (resp_xml.empty())
    {
        slog.WarningLog("empty resp_xml!");
        return ;
    }
    
    pt::ptree root_tree;
    std::stringstream ss(resp_xml);
    try
    {
        pt::read_xml(ss, root_tree);
    }
    catch(pt::ptree_error& e )
    {
        slog.WarningLog("Parse error: %s", e.what());
        return ;
    }
    
    if (root_tree.count("ESPP_1204010"))
    {
        //	<!--Код "домашнего" оператора-->
        //	<f_01>138020</f_01>
        //	<!--Название "домашнего" оператора-->
        //	<f_02>МТС-Москва</f_02>
        //	<!--Номер л/с абонента-->
        //	<f_03>123456789012345</f_03>
        //	<!—Данные об абоненте-->
        //	<f_04>И.И.И.</f_04>
        //	<!—Номер транзакции ЕСПП-->
        //	<f_05>525068723001</f_05>
        //	<!--Сумма текущего лимита-->
        //	<f_06>1000000.00</f_06>
        //	<!--Валюта текущего лимита-->
        //	<f_07 xsi:type = "espp-constraints:CUR_fmt_01">810</f_07>
        //	<!--Дата текущего лимита-->
        //	<f_08>2005-02-28T15:00:00</f_08>
        //</ESPP_1204010>
        
        const pt::ptree & espp = root_tree.get_child("ESPP_1204010") ;
        
        resp.espp_type  = response_t::ESPP_1204010 ;
        
        resp.code_operator = espp.get< std::string > ("f_01", "");
        resp.name_operator = espp.get< std::string > ("f_02", "");
        resp.lc_user       = espp.get< std::string > ("f_03", "");
        resp.user_fio      = espp.get< std::string > ("f_04", "");
        resp.oson_limit_balance  = espp.get< std::string > ("f_06", "");
        resp.espp_trn_id   = espp.get< std::string > ("f_05", "0");
        
        
    }else if (root_tree.count("ESPP_2204010"))
    {
        /***2. otkaz */
        //    <?xml version="1.0" encoding="UTF-8"?>
        //<ESPP_2204010 xmlns="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
        //    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        //xsi:schemaLocation="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
        //    <!--Код отказа-->
        //    <f_01>204</f_01>
        //    <!--Описание отказа-->
        //    <f_02>VIP счет. Прием платежа запрещен (невозможен)</f_02>
        //</ESPP_2204010>
        const pt::ptree & espp = root_tree.get_child("ESPP_2204010") ;
        
        resp.espp_type  = response_t::ESPP_2204010;
        
        resp.code_reject = espp.get< std::string >("f_01", "0");
        resp.desc_reject = espp.get< std::string >("f_02", "");
                 
    }
    else if (root_tree.count("ESPP_1204090"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_1204090") ;
        
        resp.espp_type  = response_t::ESPP_1204090;
                ;
        
        //         <?xml version = "1.0" encoding = "UTF-8"?>
        //<ESPP_1204090 	xmlns = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
        //				xmlns:espp-constraints = "http://schema.mts.ru/ESPP/Core/Constraints/v5_01"
        //				xmlns:xsi = "http://www.w3.org/2001/XMLSchema-instance"
        //				xsi:schemaLocation = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd
        //http://schema.mts.ru/ESPP/Core/Constraints/v5_01 ESPP_Core_Constraints_v5_01.xsd">
        //	<!--Телефон-->
        //	<f_01 xsi:type = "espp-constraints:PHN_CODE_fmt_01">0952321731</f_01>
        //	<!--Сумма-->
        //	<f_02>12.00</f_02>
        //	<!--Валюта-->
        //	<f_03 xsi:type = "espp-constraints:CUR_fmt_01">810</f_03>
        //	<!--Тип платежного инструмента-->
        //	<f_04>1</f_04>
        //	<!--Номер платежного инструмента не указан-->
        //	<!--Номер транзакции ЕСПП-->
        //	<f_06>525068723001</f_06>
        //	<!--Внешний номер платежа-->
        //	<f_07>999990</f_07>
        //	<!--Дата операции-->
        //	<f_08>2005-02-28T15:01:19</f_08>
        //	<!--Л/с абонента-->
        //	<f_09>123456789012345</f_09>
        //	<!--АС Представителя-->
        //	<f_10>13802000501</f_10>
        //	<!--Номер терминала-->
        //	<f_11>ELECSNET.1234567</f_11>
        //	<!--Тип терминала-->
        //	<f_12>4</f_12>
        //	<!--Дата акцептования-->
        //	<f_13>2005-02-28T15:01:55</f_13>
        //	<!--Дата платежа по терминалу-->
        //	<f_14>2005-02-28T16:00:02+03:00</f_14>
        //	<!--КК-->
        //	<f_16>КК_base64Encoding</f_16>
        //	<!--Код "домашнего" оператора-->
        //	<f_17>138020</f_17>
        //	<!--Название "домашнего" оператора-->
        //	<f_18>МТС-Москва</f_18>
        //	<!--Договор-->
        //	<f_19>13802001</f_19>
        //</ESPP_1204090>       
        
        resp.espp_trn_id   = espp.get< std::string>("f_06", "0");        
        resp.lc_user       = espp.get< std::string >("f_09", "");
        resp.code_operator = espp.get < std::string > ("f_17", "");
        resp.ts            = espp.get< std::string > ("f_08", "");
        
    }
    else if (root_tree.count("ESPP_2204090"))
    {
        
        const pt::ptree & espp = root_tree.get_child("ESPP_2204090") ;
        
        resp.espp_type  = response_t::ESPP_2204090;
        
        //        <?xml version = "1.0" encoding = "UTF-8"?>
        //<ESPP_2204090 xmlns="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
        //    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"    xsi:schemaLocation="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
        //	<!--Код отказа-->
        //	<f_01>359</f_01>
        //	<!--Описание отказа-->
        //	<f_02>Нет разрешения приема платежа</f_02>
        //</ESPP_2204090>
        
        resp.code_reject = espp.get< std::string > ("f_01", "0");
        resp.desc_reject = espp.get< std::string > ("f_02", "");
    }
    else if (root_tree.count("ESPP_1204085"))
    {
        
        const pt::ptree & espp = root_tree.get_child("ESPP_1204085") ;
        
        resp.espp_type  = response_t::ESPP_1204085;
        
//            <?xml version = "1.0" encoding = "UTF-8"?>
//    <ESPP_1204085 	xmlns = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
//        xmlns:espp-constraints = "http://schema.mts.ru/ESPP/Core/Constraints/v5_01"
//        xmlns:xsi = "http://www.w3.org/2001/XMLSchema-instance"
//        xsi:schemaLocation = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd http://schema.mts.ru/ESPP/Core/Constraints/v5_01 ESPP_Core_Constraints_v5_01.xsd">
//        <!--Состояние обработки платежа-->
//        <f_01>2</f_01>
//        <!--Телефон-->
//        <f_02 xsi:type = "constraints:PHN_CODE_fmt_01">0952321731</f_02>
//        <!--Сумма-->
//        <f_03>12.00</f_03>
//        <!--Валюта-->
//        <f_04 xsi:type = "constraints:CUR_fmt_01">810</f_04>
//        <!--Тип платежного инструмента-->
//        <f_05>1</f_05>
//        <!--Номер платежного инструмента не указан-->
//        <!--Номер транзакции ЕСПП-->
//        <f_07>101</f_07>
//        <!--Внешний номер платежа-->
//        <f_08>999990</f_08>
//        <!--Дата операции-->
//        <f_09>2005-02-28T15:01:19</f_09>
//        <!--Л/с абонента-->
//        <f_10>1234567890</f_10>
//        <!--ПЦ Представителя-->
//        <f_11>13802000501</f_11>
//        <!--Номер терминала-->
//        <f_12>ELECSNET   .1234567 </f_12>
//        <!--Тип терминала-->
//        <f_13>4</f_13>
//        <!--Дата акцептования-->
//        <f_14>2005-02-28T15:01:55</f_14>
//        <!--Дата платежа по терминалу-->
//        <f_15>2005-02-28T16:00:02+05:00</f_15>
//        <!--КК-->
//        <f_17>КК_base64Encoding</f_17>
//        <!--Код "домашнего" оператора-->
//        <f_18>138020</f_18>
//        <!--Название "домашнего" оператора-->
//        <f_19>МТС-Москва</f_19>
//        <!--Договор-->
//        <f_20>13802001</f_20>
//    </ESPP_1204085>
        
        resp.status = espp.get< int64_t >("f_01", -1 ) ;
        resp.ts     = espp.get< std::string > ("f_09", "");
        
    }
    else if(root_tree.count("ESPP_2204085"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_2204085") ;
        
        resp.espp_type  = response_t::ESPP_2204085;
        
//        <?xml version = "1.0" encoding = "UTF-8"?>
//<ESPP_2204085 xmlns="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
//    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
//xsi:schemaLocation="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
//	<!--Код отказа-->
//	<f_01>650</f_01>
//	<!--Описание отказа-->
//	<f_02>Не определено состояние платежа (без детализации)</f_02>
//</ESPP_2204085>
        resp.code_reject = espp.get< std::string > ("f_01", "0");
        resp.desc_reject = espp.get< std::string > ("f_02", "");
        
    }
    else if (root_tree.count("ESPP_1204050"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_1204050") ;
        
        resp.espp_type  = response_t::ESPP_1204050;

//        <?xml version = "1.0" encoding = "UTF-8"?>
//        <ESPP_1204050 	xmlns = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
//            xmlns:xsi = "http://www.w3.org/2001/XMLSchema-instance"
//            xsi:schemaLocation = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
//            <!-- Идентификатор реестра для проведения сверки -->
//            <f_01>12111123315</f_01>
//        </ESPP_1204050>
        resp.reestr_id = espp.get< std::string > ("f_01", "");
    }
    else if (root_tree.count("ESPP_2204050"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_2204050") ;
        
        resp.espp_type  = response_t::ESPP_2204050;

        //        <?xml version = "1.0" encoding = "UTF-8"?>
        //<ESPP_2204050 xmlns="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
        //    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
        //xsi:schemaLocation="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
        //	<!—-Код отказа -->
        //	<f_01>801</f_01>
        //	<!—-Описание причины отказа -->
        //<f_02>Неизвестный формат реестра</f_02>
        //</ESPP_2204050>
        resp.code_reject = espp.get< std::string > ("f_01", "0");
        resp.desc_reject = espp.get< std::string > ("f_02", "");
        
    }
    else if (root_tree.count("ESPP_1204051"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_1204051") ;
        
        resp.espp_type  = response_t::ESPP_1204051;

//                <?xml version = "1.0" encoding = "UTF-8"?>
//        <ESPP_1204051 	xmlns = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
//            xmlns:xsi = "http://www.w3.org/2001/XMLSchema-instance"
//            xsi:schemaLocation = "http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd ">
//            <!—- Код результата сверки -->
//            <f_04>2</f_04>
//            <!—- Описание результата сверки -->
//            <f_05>Имеются расхождения</f_05>
//            <!—- Реестр с результатами сверки -->
//            <f_06>Результаты сверки</f_06>
//        </ESPP_1204051>
        
        resp.reestr_id  = espp.get< std::string > ( "f_04", "0" )  ;
        resp.reestr_res = espp.get< std::string  > ( "f_06", ""  )  ;
        
    }
    else if (root_tree.count("ESPP_2204051"))
    {
        const pt::ptree & espp = root_tree.get_child("ESPP_2204051");
        resp.espp_type = response_t::ESPP_2204051 ;
        
//        <?xml version = "1.0" encoding = "UTF-8"?>
//        <ESPP_2204051 xmlns="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01"
//            xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
//        xsi:schemaLocation="http://schema.mts.ru/ESPP/AgentPayments/Protocol/Messages/v5_01 ESPP_AgentPayments_Protocol_Messages_v5_01.xsd">
//            <!—- Код отказа -->
//            <f_01>804</f_01>
//            <!—- Описание причины отказа -->
//            <f_01>Идентификатор реестра для проведения сверки не найден</f_01>
//        </ESPP_2204051>
        resp.code_reject = espp.get< std::string > ("f_01", "0");
        resp.desc_reject = espp.get< std::string > ("f_02", "");
    }
    else if (root_tree.count("ESPP_2204213"))
    {
        //otkaz otmenu
        const pt::ptree & espp = root_tree.get_child("ESPP_2204213");
        resp.espp_type = response_t::ESPP_2204213;
        
        resp.code_reject = espp.get< std::string > ("f_01", "0");
        resp.desc_reject = espp.get< std::string > ("f_02", "");
    }
    else if (root_tree.count("ESPP_1204213"))
    {
        //prinata otmenu
        const pt::ptree & espp = root_tree.get_child("ESPP_1204213");
        resp.espp_type = response_t::ESPP_1204213;
        
        resp.espp_trn_id = espp.get< std::string >("f_02", "");
    }
    else{
        slog.WarningLog("Not implemented yet");
    }
    
}


void MTS::acc_t::load_cert_from_config()
{
    const struct runtime_options_t * opt = oson_opts  ;
    
    cert_info.private_key_file = opt->certs.ums_private_key ;  //"/etc/oson/CA/UMS/ums_privatekey.pem" ;
    cert_info.public_key_file  = opt->certs.ums_public_key  ;  //"/etc/oson/CA/UMS/oson_public.pem" ;
    cert_info.verify_file      = opt->certs.ums_verify_cert ;  //"/etc/oson/CA/UMS/ums_pc.cer" ;
}


/*********************************************************************************************************/
namespace hg = oson::backend::merchant::HermesGarant;


hg::manager_t::manager_t(const struct acc_t& acc)
    : m_acc( acc )
{
    SCOPE_LOGD(slog);
}

hg::manager_t::~manager_t()
{
    SCOPE_LOGD(slog);
}

std::string hg::manager_t::make_body(const request_t& req, int cmd )
{
    SCOPE_LOG(slog);
    
    //assert(cmd > 0 && cdm < 4 ) ;
    
    typedef const char* pchar;
    
    static const pchar request_types[ ] = 
    {
        "none", "AccountCheck", "Payment", "CheckBalance", "TerminalCheck"
    };
    
    std::string account = req.account;
    int const service_id = string2num(req.service_id);
    
    static const std::map< int, size_t > field_sizes = 
    {
        { 18,  10}, // Beeline Russia
        { 19,  10}, // Megafon Russia
        { 20,  10}, // MTS     Russia
        { 21,  10}, // Tele-2  Russia
        
        { 46, 10}, // Active or Altel Kazakhistan
        { 45, 10}, // Kcell Kazakhistan
        { 44, 10}, // Beeline Kazakhistan
        { 43, 10}, // Tele-2 Kazakhistan
        
        { 10, 9}, // Beeline Kirgizistan
        { 11, 9}, // Megacom Kirgizistan
        { 12, 9}, // Nurtelecom Kirgizistan
        
        { 60,  9}, // Beeline Uzbekistan
        { 13,  9}, // Ucell Uzbekistan
        
        { 14,  9}, // Beeline Tadjikistan
        { 15,  9}, // Megafon Tadjikistan
        { 16,  9}, // Vavilon Mobile Tadjikistan
        { 17,  9}, // Indigo Tadjikistan
        
        { 126, 8}, // Ucom Orange Armeniya
        { 96,  8}, // Beeline Armeniya
        { 97,  8}, // MTS armeniya
        
        { 98,  9}, // Beeline Gruziya
        { 99,  9}, // GeoCell Gruziya
        { 100, 9}, // Global Ucell Gruziya
        
        { 89,  9}, // MTS Ukraina
        { 90,  9}, // Life Ukraina
        { 91,  9}, // Kyester Ukraina
    } ;
    
    if (field_sizes.count(service_id)){
        size_t size = field_sizes.at( service_id);
        if (account.size()  > size ) {
            //remove first unnecessary symbols.
            account.erase(0, account.size() - size ) ;
        }
    }
    
    std::string req_str = "AgentID="         + m_acc.agent_id       + 
                          "&AgentPassword="  + m_acc.agent_password + 
                          "&Service="        + req.service_id       + 
                          "&RequestDate="    + req.ts               + 
                          "&TransactionID="  + to_str(req.trn_id)   + 
                          "&Amount="         + to_str(req.amount,2, false) + 
                          "&RequestType="    + request_types[cmd]   +
                          "&account="        + account              + 
                          "&Currency="       + req.currency         ;
    
    
    slog.InfoLog("req: %.*s\n", ::std::min<int>(2048, req_str.length()), req_str.c_str());
    
    return req_str;
    
}

void hg::manager_t::check(request_t const& req,  response_t & resp)
{
    SCOPE_LOGD(slog);
    
    //AgentID=21&TransactionID=1&RequestDate=2018-08-13%2013:08:05&Service=1&Amount=100.00&RequestType=AccountCheck&AgentPassword=OsonUZ76&account=test&Currency=KZT
    
    std::string req_str = make_body(req,  hg::commands::cmd_terminal_check);
    
    auto http_req = oson::network::http::parse_url(m_acc.url);
    http_req.method = "POST";
    http_req.content.charset = "UTF-8";
    http_req.content.type = "application/x-www-form-urlencoded" ;
    http_req.content.value = req_str;
    
    std::string resp_json = sync_http_ssl_request( http_req ) ;
    
    parse_resp(resp_json, resp);
}

void hg::manager_t::pay(request_t const & req, response_t& resp)
{
    SCOPE_LOG(slog);
    
    std::string req_str = make_body(req,  hg::commands::cmd_pay );
    
    auto http_req = oson::network::http::parse_url( m_acc.url );
    http_req.method = "POST";
    http_req.content.charset = "UTF-8";
    http_req.content.type = "application/x-www-form-urlencoded" ;
    http_req.content.value = req_str;
    
    std::string resp_json = sync_http_ssl_request( http_req ) ;
    
    parse_resp(resp_json, resp);
    
}

void hg::manager_t::async_pay(const struct request_t& req,  handler_t h )
{
    SCOPE_LOG(slog);
    std::string req_str = make_body(req, hg::commands::cmd_pay ) ;
    auto http_req = oson::network::http::parse_url(m_acc.url);
    http_req.method           = "POST";
    http_req.content.charset  = "UTF-8";
    http_req.content.type     = "application/x-www-form-urlencoded" ;
    http_req.content.value    = req_str;
    
    auto io_service = oson_merchant_api -> get_io_service();
    auto ctx        = oson_merchant_api -> get_ctx_sslv23();
    auto http_client = oson::network::http::client::create(io_service, ctx ) ;
    
    auto http_handler = [=](const std::string& resp_ans, boost::system::error_code ec )
    {
    
        hg::response_t resp;
        if ( static_cast< bool >( ec )  ){
            resp.err_value = Error_internal;

            if (ec == boost::asio::error::host_not_found || 
                ec == boost::asio::error::operation_aborted ||
                ec == boost::asio::error::connection_reset ||
                ec == boost::asio::error::timed_out ) 
            {
                resp.err_value = Error_HTTP_host_not_found ;
            }
            return h(req, resp);
        }
        resp.err_value = Error_OK ;
        hg::manager_t::parse_resp(resp_ans, resp);
        
        return h(req, resp);
    };
    
    http_client->set_request(http_req);
    http_client->set_response_handler( http_handler );
    http_client->async_start();
}

void hg::manager_t::balance(request_t const& req, response_t& resp)
{
    SCOPE_LOG(slog);
    std::string req_str = make_body(req, hg::commands::cmd_balance);
    
    auto http_req = oson::network::http::parse_url(m_acc.url);
    http_req.method = "POST";
    http_req.content.charset = "UTF-8";
    http_req.content.type = "application/x-www-form-urlencoded" ;
    http_req.content.value = req_str;
    
    std::string resp_json = sync_http_ssl_request(http_req);
    
    parse_resp(resp_json, resp);
}

void hg::manager_t::parse_resp(const std::string& resp_json, response_t& resp)
{
    
    SCOPE_LOG(slog);
    
    slog.InfoLog("resp_json: %.*s\n", ::std::min< int > ( 2048, resp_json.length() ), resp_json.c_str() ) ;
    
    if (resp_json.empty()){
        slog.WarningLog("resp_json is EMPTY");
        
        resp.message = "Connection error";
        resp.resp_status = -9999;
        return ;
    }
    
    namespace pt = boost::property_tree;
    
    pt::ptree root;
    try
    {
        std::stringstream ss(resp_json);
        pt::read_json(ss, root);
        
        //{
        //        TransactinID":113,
        //	"RequestID":2,
        //	"ResponseType":"Payment",
        //	"ResponseStatus":5,
        //	"Message":"Запрос Обрабатывается",
        //	"TransactionContent":
        //	{
        //		"Service":1,
        //		"account":"test",
        //		"Amount":100.00,
        //		"Currency":"KZT"
        //	}
        //}
           resp.trn_id = root.get< int64_t >("TransactionID", 0);
           resp.req_id = root.get< int64_t >("RequestID", 0);
           resp.message = root.get< std::string >("Message", "");
           resp.resp_status = root.get < int64_t >("ResponseStatus", 0 );
           resp.resp_type = root.get< std::string>("ResponseType", "");
           
           if (root.count("TransactionContent")){
               
                const pt::ptree& tc = root.get_child("TransactionContent");
                resp.tc.service = tc.get< std::string>("Service", "0");
                resp.tc.account = tc.get< std::string>("account", "");
                resp.tc.amount  = tc.get< std::string>("Amount", "0.00");
                resp.tc.currency = tc.get< std::string>("Currency", "");
           }
           
           
           if (root.count("Balances"))
           {
               const pt::ptree& bc = root.get_child("Balances");
               for(const pt::ptree::value_type & c: bc )
               {
                   typedef response_t::balances::pair  pair;
                   pair p;
                   p.balance = c.second.get<std::string>("Balance", "0.00");
                   p.currency = c.second.get<std::string>("Currency", "");
                   
                   resp.bc.list.push_back( p );
               }
           }
    
    }
    catch(pt::ptree_error  & e )
    {
        slog.ErrorLog("Parse error: %s ", e.what());
        resp.message = e.what();
        resp.resp_status = -9999;
    }
}
