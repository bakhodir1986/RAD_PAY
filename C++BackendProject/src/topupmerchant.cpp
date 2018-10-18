
#include <memory> //std::addressof

#include "log.h"
#include "utils.h"
#include "merchant_api.h" 
#include "topupmerchant.h"

namespace topup = oson::topup;


bool topup::supported (int32_t id)
{
    //@Note: we have only webmoney support, yet.  
    switch( id)
    {
        case topup::topup_id::webmoney:  return true;
        
        default:
            return false;
    }
}

/*******************************/
topup::info::info()
   : id( 0 )
   , name( )
   , status( 0 )
   , option( 0 )
   , min_amount(0)
   , max_amount(0)
   , rate(0)
   , position(0)
   , card_id(0)
   , icon_id(0)
{}

topup::search::search()
  : id(0)
  , name()
  , status(0)
  , option(0)
  , position(0)
  , card_id(0)
  , icon_id(0)
  , with_icon_location(false)
{}

/****************************/

topup::table::table(DB_T& db)
: m_db(db)
{}

topup::table::~table()
{}

topup::info topup::table::get( ::std::int32_t id,  /*out*/ Error_T & ec ) 
{
    SCOPE_LOG(slog);
    
    topup::info result; 
    
    ec = Error_OK ;
    
    std::string query = "SELECT id, name, status, option, min_amount, max_amount, rate, position, card_id, icon_id FROM top_up_merchants WHERE id = " + escape(id) ;
    
    DB_T::statement st( m_db ) ;
    
    st.prepare(query, ec  ) ;
    
    //failed exec db query.
    if( ec ) 
        return result ;
    
    if (  st.rows_count() != 1)
    {
        slog.WarningLog("Not found !");
        ec = Error_not_found;
        return result;
    }
    
    st.row(0) >> result.id         >> result .name >> result.status   >> result .option >> result.min_amount 
              >> result.max_amount >> result.rate  >> result.position >> result.card_id >> result.icon_id ;
    
    return result;
}


static std::string make_where(const struct topup::search & search)
{
    std::string result =  " ( 1 = 1 ) ";
    
    if ( search.id != 0 )
    {
        result += " AND ( id = " + escape(search.id) + ") " ;
    }
    
    if (search.name.size() > 0 )
    {
        result += " AND ( name = " +escape(search.name ) + ") " ;
    }
    
    if (search.status !=  0 )
    {
        result += " AND ( status = " + escape(search.status ) + " ) " ;
    }
    
    if (search.option != 0 )
    {
        result += " AND ( option = " + escape(search.option) + " ) " ;
    }
    
    return result;
}


topup::info_list topup::table::list( const struct search& search, const Sort_T& sort) 
{
    SCOPE_LOG(slog);
    
    info_list result_list ;
    
    std::string query = "SELECT id, name, status, option, min_amount, max_amount, rate, position, card_id, icon_id FROM top_up_merchants WHERE " + make_where(search) + sort.to_string();
    
    DB_T::statement st (m_db ) ;
    
    st.prepare(query);
    
    int rows = st.rows_count() ;
    
    
    result_list.reserve(rows);
    
    for(int i = 0; i < rows; ++i)
    {
        topup::info r;
        st.row(i) >> r.id >> r.name >> r.status >> r.option >> r.min_amount >> r.max_amount >> r.rate >> r.position >> r.card_id >> r.icon_id ;
        
        result_list.push_back(r);
    }
    
    return result_list;
    
}

int32_t topup::table::add(const struct topup::info & info)
{
    SCOPE_LOGD(slog);
    
    std::string query = "INSERT INTO top_up_merchants (id, name, status, option, min_amount, max_amount, rate, position, card_id, icon_id) VALUES ( " 
                        " DEFAULT, " + 
                        escape(info.name)        + ", " + 
                        escape(info.status)      + ", " + 
                        escape(info.option)      + ", " + 
                        escape(info.min_amount)  + ", " +
                        escape(info.max_amount ) + ", " +
                        escape(info.rate)        + ", " +
                        escape(info.position)    + ", " +
                        escape(info.card_id)     + ", " +
                        escape(info.icon_id)     + "  " 
                        " ) RETURNING id " ;
    DB_T::statement st (  m_db ) ;
    
    st.prepare(query) ;
    
    int32_t id = 0;
    st.row(0) >> id;
    return id;
}

int topup::table::edit(  int32_t id, const struct topup::info   & info ) 
{
    SCOPE_LOGD(slog);
    
    std::string query = " UPDATE top_up_merchants SET " 
                        "  name = "       + escape( info.name      )  + 
                        ", status = "     + escape( info.status    )  + 
                        ", option = "     + escape( info.option    )  +
                        ", min_amount = " + escape( info.min_amount)  + 
                        ", max_amount = " + escape( info.max_amount)  + 
                        ", rate = "       + escape( info.rate      )  + 
                        ", position = "   + escape( info.position  )  + 
                        ", card_id = "    + escape( info.card_id   )  + 
                        " WHERE  id = "   + escape ( id ) ;
 
    DB_T::statement st(  m_db ) ;
    st.prepare( query ) ;
    
    return st.affected_rows();
}
 
int topup::table::edit_icon(int32_t id,  int64_t icon_id)
{
    SCOPE_LOG(slog);
    std::string query = "UPDATE top_up_merchants SET icon_id = " + escape(icon_id) + " WHERE id = " +escape(id);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    return st.affected_rows();
}

int topup::table::del(int32_t id ) 
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM top_up_merchants WHERE id = " + escape(id) ;
    
    DB_T::statement st  (  m_db ) ;
    
    st.prepare(query);
    
    return st.affected_rows() ;
}

/************************************************/


topup::trans_info::trans_info()
   :  id(0)
   ,  topup_id(0)
   ,  amount_sum(0)
   ,  amount_req(0)
   ,  currency()
   ,  uid(0)
   ,  ts()
   ,  tse()
   ,  status(0)
   ,  status_text()
   ,  card_id(0)
   ,  card_pan()
   ,  eopc_trn_id()
   ,  oson_card_id(0)
   ,  topup_trn_id(0)
{
    
}

bool topup::trans_info::empty()const
{
    return ( 0 == id ) && (0 == topup_id) && ( 0 == amount_sum ) && ( 0 == uid ) && (0 == status ) && ( 0 == card_id )  && ( eopc_trn_id.empty() ) ;
}

topup::trans_table::trans_table( DB_T& db) 
: m_db( db )
{}

topup::trans_table::~trans_table()
{}

int64_t topup::trans_table::add( const struct topup::trans_info& info ) 
{
    SCOPE_LOGD(slog);
    std::string query = 
        "INSERT INTO top_up_transactions (id, topup_id, amount_sum, amount_req, currency, uid, login, "
        " pay_description, ts, tse, status, status_text, card_id, card_pan, eopc_trn_id, oson_card, topup_trn_id) "
        "VALUES (DEFAULT, "  
        + escape( info.topup_id    ) + ", "
        + escape( info.amount_sum    ) + ", "
        + escape( to_str(info.amount_req, 8, false ) ) + ", "
        + escape( info.currency      ) + ", "
        + escape( info.uid           ) + ", "
        + escape( info.login         ) + ", "
        + escape( info.pay_desc      ) + ", "
        + escape( info.ts            ) + ", "
        + escape( info.tse           ) + ", " 
        + escape( info.status        ) + ", "
        + escape( info.status_text   ) + ", "
        + escape( info.card_id       ) + ", "
        + escape( info.card_pan      ) + ", "
        + escape( info.eopc_trn_id   ) + ", "
        + escape( info.oson_card_id  ) + ", "
        + escape( info.topup_trn_id  ) + " ) RETURNING id "
            ;
    
    DB_T::statement st{ m_db } ;
    
    st.prepare( query ) ;
    
    int64_t id = 0;
    
    st.row( 0 ) >> id ;
    
    return id ;
}

int topup::trans_table::update(int64_t id, topup::trans_info& info)
{
    SCOPE_LOGD( slog ) ;
    
    //@Note: allowed edit only, pay-description, tse, status, status_text, eopc_trn_id.
    
    std::string query = 
    " UPDATE top_up_transactions SET pay_description = " + escape(info.pay_desc) + ", "
     + " tse = "          + escape(info.tse )         + ", "
     + " status = "       + escape(info.status )      + ", "
     + " status_text = "  + escape(info.status_text ) + ", "
     + " eopc_trn_id = "  + escape(info.eopc_trn_id ) + ", "
     + " login  = "       + escape(info.login)        + ", "
     + " topup_trn_id = " + escape(info.topup_trn_id) + "  "
     + " WHERE id = "     + escape(id) ;
    
    DB_T::statement st{ m_db } ;
    
    st.prepare(query);
    
    return st.affected_rows() ;
}

int topup::trans_table::del(int64_t id)
{
    SCOPE_LOGD(slog);
    
    slog.WarningLog("Delete topup_trans_table does not allowed!") ;
    
    return 0;
}

topup::trans_info  topup::trans_table::get_by_id(int64_t id, Error_T& ec )
{
    SCOPE_LOGD(slog);
    
    trans_info info;
    
    std::string query = "SELECT id, topup_id, amount_sum, amount_req, currency, uid, login, "
        " pay_description, ts, tse, status, status_text, card_id, card_pan, eopc_trn_id, oson_card, topup_trn_id " 
        " FROM top_up_transactions WHERE id = " + escape(id) ;
    
    
    DB_T::statement st(  m_db ) ;
    ec = Error_OK ;
    st.prepare(query, ec ) ;
    if (ec) return info;
    
    if (st.rows_count() != 1 )
    {
        slog.WarningLog("Not found!");
        ec = Error_not_found ;
        return info;
    }
    
    st.row(0) >> info.id       >> info.topup_id    >> info.amount_sum  >> info.amount_req   >> info.currency  >> info.uid 
              >> info.login    >> info.pay_desc    >> info.ts          >> info.tse          >> info.status    >> info.status_text 
              >> info.card_id  >> info.card_pan    >> info.eopc_trn_id >> info.oson_card_id >> info.topup_trn_id ;
    
    return info ;
}


static bool empty(const topup::trans_info& info )
{
    return info.empty() ;
}

static std::string make_where(const topup::trans_info & search )
{
    std::string result = " ( 1 = 1 ) " ;
    if (search.id != 0 )  result += " AND ( id = " + escape(search.id ) + " ) " ;
    if (search.uid != 0 ) result += " AND ( uid = " + escape(search.uid ) + " ) " ;
    if (search.status != 0 ) result += " AND (status = " + escape(search.status ) + " ) " ;
    if (search.card_id != 0 ) result += " AND (card_id = " + escape(search.card_id ) + " ) " ;
    if (search.eopc_trn_id.size() > 0 ) result += " AND (eopc_trn_id = " + escape(search.eopc_trn_id) + " ) "  ;
    
    return result ;
}


topup::trans_info  topup::trans_table::get_by_search(const struct topup::trans_info& search, Error_T& ec ) 
{
    SCOPE_LOGD(slog);
    
    topup::trans_info info;
    
    if (empty(search))
    {
        slog.WarningLog("search is empty ! " );
        ec = Error_not_found;
        return info;
    }
    
    
    std::string query = "SELECT id, topup_id, amount_sum, amount_req, currency, uid, "
        " pay_description, ts, tse, status, status_text, card_id, card_pan, eopc_trn_id, oson_card " 
        " WHERE  " + make_where(search)  + " LIMIT 32 " ; // no more 32 for avoid memory allocation.

    DB_T::statement st(  m_db ) ;
    
    ec = Error_OK  ;
    st.prepare( query, ec ) ;
    if (ec) return info;
    
    
    if ( st.rows_count() != 1 ) 
    {
        slog.WarningLog("Not found ! " ) ;
        ec = Error_not_found ;
        return info ;
    }
    
    st.row(0) >> info.id >> info.topup_id >> info.amount_sum >> info.amount_req >> info.currency >> info.uid 
              >> info.pay_desc >> info.ts >> info.tse >> info.status >> info.status_text >> info.card_id 
              >> info.card_pan >> info.eopc_trn_id >> info.oson_card_id ;
     
    return info ;
}


topup::trans_list  topup::trans_table::list(const struct trans_info& search, const Sort_T& sort, Error_T& ec )
{
    SCOPE_LOGD(slog);
    ec = Error_OK ;
    
    topup::trans_list list;
    
    if ( sort.limit > 4000 || sort.limit <= 0 ) 
    {
        slog.WarningLog("sort.limit too big, use a less value. limit: %d", sort.limit ) ;
        ec = Error_operation_not_allowed ;
        return list;
    }
    
    std::string where_s = make_where(search);
    std::string query = "SELECT id, topup_id, amount_sum, amount_req, currency, uid, "
        " pay_description, ts, tse, status, status_text, card_id, card_pan, eopc_trn_id, oson_card " 
        " WHERE  " + where_s  + sort.to_string() ;

    DB_T::statement st{ m_db } ;
    
    ec = Error_OK  ;
    st.prepare( query, ec ) ;
    if (ec) return list; // can't execute, may be sql text error, or may be DB disconnected.
    
    int rows = st.rows_count() , i = 0 ;
    
    list.list.resize(rows);
    
    for(auto& info : list.list) 
    {
        st.row(i) >> info.id >> info.topup_id >> info.amount_sum >> info.amount_req >> info.currency >> info.uid 
                  >> info.pay_desc >> info.ts >> info.tse >> info.status >> info.status_text >> info.card_id 
                  >> info.card_pan >> info.eopc_trn_id >> info.oson_card_id ;
        ++i;
    }
    /////////////////
    st.prepare("SELECT count(*), sum(amount_sum) WHERE " + where_s,  ec ) ;
    
    if (ec)return list;
    
    st.row(0 ) >> list.total_cnt >> list.total_sum ;
    
    return list ;
    
    
}
   
/***********************************************************/
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "http_request.h"
#include "osond.h"
#include "application.h" // scoped register

//@Note: when we implement asynchronous mode, this method will be deleted.

static std::string sync_http_ssl_request(oson::network::http::request req, boost::asio::ssl::context::method ssl_method = boost::asio::ssl::context::sslv23 )
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



namespace wm = oson::topup::webmoney;

wm::wm_manager :: wm_manager(  wm_access  acc ) 
 : m_acc ( acc)
{}

wm::wm_manager::~wm_manager() 
{}

static std::string parse_a_href(const std::string & html )
{
    std::string url;
    namespace pt = boost::property_tree ;
    
    pt::ptree root;
    std::stringstream ss(html);
    
    try
    {
        pt::read_xml(ss, root);
    }
    catch(std::exception& e )
    {
        SCOPE_LOG(slog);
        slog.WarningLog("Exception: %s\n", e.what());
        return url;
    }
    
    //   "url": 
    //   <html>
    //    <head><title>Object moved</title></head>
    //    <body>\r\n<h2>Object moved to <a href=\"https://psp.paymaster24.com/Payment/process/3c19cc6d-88d8-4f5c-b5cc-3800564e5867\">here</a>.</h2>\r\n</body>
    // </html>\r\n"
    
    
    struct find_a_href
    {
        const pt::ptree*  find( const pt::ptree & root)
        {
            for( const pt::ptree::value_type & e :  root)
            {
                // 
                if ( e.first == "a") 
                {
                    return std::addressof( e.second ) ;
                }
            }
            
            for(const pt::ptree::value_type & e : root)
            {
                if ( const pt::ptree* r = find( e.second) )
                {
                    return r;
                }
            }
            return nullptr;
        }
    }finder;
    
    //find <a href   within <html> <body> .
    const pt::ptree * p = finder.find( root.get_child("html.body", pt::ptree{} ) )   ;
    
    if ( ! p ) return url;
    
    url = p->get<std::string>("<xmlattr>.href", "") ;
    
    ::boost::algorithm::trim_if(url, [](char c){ return c == '\"' ; } ) ;
    
    return url;
}

void wm::wm_manager :: async_payment_request(const wm_request& req, payment_handler handler ) 
{
    SCOPE_LOGD(slog);
    
    
    
    if (req.url.empty() )
    {
        slog.ErrorLog("request url is empty!");
        wm_response result;
        result.status_value = -1 ;
        result.status_text  = "request url is empty!" ;
        
        return handler( req, result ) ;
    }
    
    
    std::string address = req.url  +
            "/?LMI_MERCHANT_ID="   + m_acc.id_company +
            "&LMI_PAYMENT_AMOUNT=" + req.amount       +
            "&LMI_CURRENCY="       + req.currency     +
            "&LMI_PAYMENT_NO="     + req.trn_id       +
            "&LMI_PAYMENT_DESC="   + req.pay_desc     +
            //"&LMI_SIM_MODE="       + req.test_mode    +
            "";
    
    namespace http = oson::network::http;
    
    auto http_req = http::parse_url ( address ) ; // https://psp.paymaster24.com/Payment/Init
    http_req.method = "GET" ;
         
    slog.DebugLog("REQUEST: %s\n", address.c_str());
    
    auto http_handler = [handler, req ](const std::string& http_resp, boost::system::error_code ec )
    {
        SCOPE_LOGD(slog);
        wm_response result;
        slog.DebugLog("RESPONSE: %s\n", http_resp.c_str());
    
        if ( static_cast<bool>( ec ) ) 
        {
            
            result.status_value = ec.value();
            result.status_text = ec.message();
            
            return handler(req, result) ;
        }

        //   "url": 
        //   <html>
        //    <head><title>Object moved</title></head>
        //    <body>\r\n<h2>Object moved to <a href=\"https://psp.paymaster24.com/Payment/process/3c19cc6d-88d8-4f5c-b5cc-3800564e5867\">here</a>.</h2>\r\n</body>
        // </html>\r\n"


        result.status_text = "OK";
        result.status_value = 0;
        result.url = parse_a_href( http_resp ) ;
        slog.DebugLog("url: %s\n", result.url.c_str());

        if (result.url.empty() )
        {
            result.status_value = -1;
            result.status_text = "URL not found!";
        }

        return handler( req, result ) ;
    };
    
    auto io_service = oson_merchant_api -> get_io_service();
    auto ctx        = oson_merchant_api -> get_ctx_sslv23 () ;
    
    auto client_http = oson::network::http::client::create( io_service, ctx  );
    client_http->set_request(http_req);
    client_http->set_response_handler(http_handler);
    
    client_http->async_start() ;
    
}

wm::wm_response  wm::wm_manager :: payment_request(const wm_request & req, /*out*/ Error_T& ec)  
{
    
    SCOPE_LOGD(slog);
    
    ec = Error_OK ; //reset it
    
    wm_response result;
    
    if (req.url.empty() )
    {
        slog.ErrorLog("request url is empty!");
        ec = Error_parameters;
        return result;
    }
    
    std::string address = req.url  +
            "/?LMI_MERCHANT_ID="   + m_acc.id_company +
            "&LMI_PAYMENT_AMOUNT=" + req.amount       +
            "&LMI_CURRENCY="       + req.currency     +
            "&LMI_PAYMENT_NO="     + req.trn_id       +
            "&LMI_PAYMENT_DESC="   + req.pay_desc     ;
           // "&LMI_SIM_MODE="       + req.test_mode    ;
    
    namespace http = oson::network::http;
    
    auto http_req = http::parse_url ( address ) ; // https://psp.paymaster24.com/Payment/Init
    http_req.method = "GET" ;
         
    slog.DebugLog("REQUEST: %s\n", address.c_str());
    
    std::string http_resp = sync_http_ssl_request(http_req);
    
    slog.DebugLog("RESPONSE: %s\n", http_resp.c_str());
    
    if (http_resp.empty() ) 
    {
        ec = Error_internal ;
        return result;
    }
  
    //   "url": 
    //   <html>
    //    <head><title>Object moved</title></head>
    //    <body>\r\n<h2>Object moved to <a href=\"https://psp.paymaster24.com/Payment/process/3c19cc6d-88d8-4f5c-b5cc-3800564e5867\">here</a>.</h2>\r\n</body>
    // </html>\r\n"
     
    
    result.status_text = "OK";
    result.status_value = 0;
    result.url = parse_a_href( http_resp ) ;
    slog.DebugLog("url: %s\n", result.url.c_str());
    
    if (result.url.empty() )
    {
        result.status_value = -1;
        result.status_text = "URL not found!";
    }
    
    return result;
}
    
 
  


 