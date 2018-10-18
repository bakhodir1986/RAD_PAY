#include <cctype> // ::isdigit
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <memory>
#include <atomic> // std::atomic_int

#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl/context.hpp>

#include <boost/smart_ptr/enable_shared_from_this.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/predicate.hpp>



#include "log.h"
#include "fault.h"
#include "osond.h"
#include "application.h"
#include "eocp_api.h"
#include "utils.h"
#include "exception.h"
#include "config_types.h"

#include "http_request.h"
#include "sms_sender.h"
#include "runtime_options.h"

///////////////////////////////////////
#define CARD_NEW "cards.new"
#define CARD_GET "cards.get"
#define CARD_BLOCK "cards.block"
#define TRANS_PAY "trans.pay"
#define TRANS_CREDIT "trans.credit"
#define TRANS_EXT "trans.ext"
#define TRANS_SV "trans.sv"
#define TRANS_REVERSE "trans.reverse"
#define P2P_INFO "p2p.info"
#define P2P_ID2ID "p2p.id2id"
#define P2P_ID2PAN "p2p.id2pan"
#define P2P_PAN2PAN "p2p.pan2pan"
#define P2P_CREDIT  "trans.credit"
#define REPORT_SUM "report.sum"
#define REPORT_DETAIL "report.detail"
#define REPORT_SHORT "report.short"
#define CARD_HISTORY "trans.history.filter" 
///////////////////////////////////////////////
#define EOPC_ERROR_CONNECT (-32003)
#define EOPC_ERROR_NOTFOUND (-404)
#define P2P_TIMEOUT (30000)
/////////////////////////////////////////////

namespace pt = boost::property_tree ;

bool valid_card_pan(const std::string& pan){  
    if (pan.empty())
        return false;
    for(size_t i = 0; i < pan.size(); ++i)
        if (! isdigit(pan[i]))
            return false;
    return true;
}

std::string EOCP_card_history_resp::date_time()const
{
    //utime: HHMMSS
    //udate: YYYYMMDD
    int year  = udate / 10000;
    int month = (udate % 10000) / 100;
    int day   = udate % 100 ;
    
    
    int hour   =  utime / 10000 ;
    int minute = (utime % 10000) / 100;
    int second = utime % 100 ;
    
    char buf[64]  = {};
    
    size_t z = snprintf(buf, 64, "%04d-%02d-%02d %02d:%02d:%02d", year, month, day,   hour, minute, second);
    
    return std::string((const char*)buf, z );
}

std::string make_stan(uint64_t trn_id){
    static const std::string::size_type stan_fixed_length = 6;
    
    std::string result( ( stan_fixed_length ) , ('0')) ;
    
    for(std::string::size_type i = 0; i != stan_fixed_length; ++i)
        result[ stan_fixed_length - i - 1 ] = (trn_id % 10 + '0'), trn_id /= 10;
    
    return result;
}


static void fix_eocp_phone(std::string& phone)
{
    //1. remove spaces.
    phone.erase(std::remove_if(phone.begin(), phone.end(), ::isspace ), phone.end());

    if (phone.empty()) {
        return;
    }

    //there out.phone is not empty!!
    if (phone[0] == '+')
        phone.erase(0, 1); // remove '+' symbol
    
    if (phone.length() == 12 && phone.compare(0, 3, "998") == 0)
        return ;  //already fine!
    
    if (phone.length() == 9 ) { // there short form  : 97 422 17 77
        phone = "998" + phone;
        return ;
    }
    
    if ( phone.length() >= 9)
    {
        // 000 97 422 17 77
        phone.erase( 0, phone.length() - 9 );

        phone = "998" + phone;
    } else {
        SCOPE_LOGD(slog);
        slog.WarningLog("PHONE IS NOT VALID: phone: '%s'", phone.c_str());
        phone.clear(); 
    }
    
}

static Error_T parseJson_i(const pt::ptree& in, oson::backend::eopc::resp::card& out)
{
    out.id        = in.get< std::string > ("id", "0") ;
    out.pan       = in.get< std::string >("pan", "0") ;
    out.expiry    = in.get< std::string >("expire", "");
    out.status    = in.get< int > ("status", -222222);
    out.phone     = in.get< std::string >("phone", "");
    out.fullname  = in.get< std::string >("fullName", "");
    out.balance   = in.get< int64_t >("balance", 0);
    out.sms       = in.get< std::string > ("sms", "false") == "true";
    
    fix_eocp_phone(out.phone); 
    return Error_OK ;
}
static Error_T parseJson(const pt::ptree & in, oson::backend::eopc::resp::card& out)
{
    return parseJson_i(in, out);
}

static Error_T parseJson(const pt::ptree& in, EOCP_card_history_resp & out )
{
//    "utrnno": 3165445240,
//    "hpan": "860014******9347",
//    "utime": 120553,
//    "udate": 20161220,
//    "bankDate": null,
//    "reqamt": 120000,
//    "resp": -1,
//    "reversal": false,
//    "orgdev": 556468,
//    "merchant": 0,
//    "terminal": 33047805,
//    "merchantName": "MCHJ UZ GAZ OIL",
//    "street": "qora tosh 1",
//    "city": "959 tashkent",
//    "credit": false
    out.utrnno = in.get< std::string > ("utrnno", "0" ) ;
    out.hpan   = in.get< std::string > ("hpan", "*");
    out.utime  = in.get< int32_t > ("utime", 0);
    out.udate  = in.get< int32_t > ("udate", 0);
    out.reqamt = in.get< int64_t > ("reqamt", 0);
    out.resp   = in.get< int32_t > ( "resp", 0);
    out.reversal     = in.get< std::string>("reversal", "false") == "true" ;
    out.orgdev       = in.get< int64_t > ("orgdev", 0);
    out.merchantId   = in.get< std::string>("merchant", "");
    out.terminalId   = in.get< std::string>("terminal", "");
    out.merchantName = in.get< std::string > ("merchantName", "");
    out.street       = in.get< std::string >("street", "");
    out.city         = in.get< std::string > ("city", "");
    out.credit       = in.get< std::string > ("credit", "false") == "true";
    
    return Error_OK ;
}
static Error_T parseJson( const pt::ptree & in, EOCP_card_history_list & out )
{
    out.last = ( in.get< std::string >("last", "false" ) == "true" ) ;
    out.first = (in.get< std::string > ("first", "false" ) == "true"  ) ;
    out.totalPages = in.get< int32_t>("totalPages", 0 );
    out.totalElements = in.get<int32_t>("totalElements", 0);
    out.size = in.get< int32_t > ("size", 0);
    out.number = in.get< int32_t > ("number", 0) ;
    out.numberOfElements = in.get<int32_t>("numberOfElements", 0 ) ;
    
    pt::ptree nil;
    for(const pt::ptree::value_type& c :  in.get_child("content", nil ) )
    {
        EOCP_card_history_resp cp;
        parseJson( c.second , cp);
        out.list.push_back( cp ) ;
    }
    return Error_OK ;
}

static std::string to_json_str(const oson::backend::eopc::req::card& c)
{
    return "{ \"pan\": \"" + c.pan + "\", \"expiry\": \"" + c.expiry + "\" } " ;
}

static std::string to_json_str(const EOPC_Debit_T& d)
{
    return "{ \"cardId\"     : \"" + d.cardId         + "\", \n"
           "  \"merchantId\" : \"" + d.merchantId     + "\", \n"
           "  \"terminalId\" : \"" + d.terminalId     + "\", \n"
           "  \"port\"       :   " + to_str(d.port)   + ",   \n"
           "  \"amount\"     :   " + to_str(d.amount) + ",   \n"
           "  \"ext\"        : \"" + d.ext            + "\", \n"
           "  \"stan\"       : \"" + d.stan           + "\", \n"
           "  \"date12\"     : \"" + formatted_time_now("%y%m%d%H%M%S") + "\" \n"
           " } "
            ;
}

static std::string to_json_str(const EOPC_Credit_T& c, boost::false_type is_card)
{
    return "{ \"cardId\"     : \"" + c.card_id        + "\", \n"
           "  \"merchantId\" : \"" + c.merchant_id    + "\", \n"
           "  \"terminalId\" : \"" + c.terminal_id    + "\", \n"
           "  \"port\"       :   " + to_str(c.port)   + ",   \n"
           "  \"amount\"     :   " + to_str(c.amount) + ",   \n"
           "  \"ext\"        : \"" + c.ext            + "\", \n"
           "  \"date12\"     : \"" + formatted_time_now("%y%m%d%H%M%S") + "\" \n"
           " } "
            ;
}

static std::string to_json_str(const EOPC_Credit_T& c, boost::true_type is_card)
{
    return "{ \"card\"       :   " + to_json_str(c.card) + ",   \n"
           "  \"merchantId\" : \"" + c.merchant_id       + "\", \n"
           "  \"terminalId\" : \"" + c.terminal_id       + "\", \n"
           "  \"port\"       :   " + to_str(c.port)      + ",   \n"
           "  \"amount\"     :   " + to_str(c.amount)    + ",   \n"
           "  \"ext\"        : \"" + c.ext               + "\", \n"
           "  \"date12\"     : \"" + formatted_time_now("%y%m%d%H%M%S") + "\" \n"
           " } "
            ;
    
}

static std::string to_json_str(const EOPC_Credit_T& c){
    if ( c.card_id.empty() )
        return to_json_str(c, boost::true_type() ) ;
    else
        return to_json_str(c, boost::false_type() ) ;
}

static std::string to_json_str(const EOCP_card_history_req & cq ) 
{
    return  "{ \"criteria\" : { "
            " \"cardIds\": [ "
            "   \"" + cq.card_id + "\" "
            " ], "
            "  \"range\": { "
            " \"startDate\": \"" + cq.startDate + "\", "
            " \"endDate\": \"" + cq.endDate + "\" "
            " }, "
            " \"pageNumber\": " + to_str(cq.pageNumber) + ", "
            " \"pageSize\":  "  + to_str(cq.pageSize )  + ", "
            " \"isCredit\": 2 "
            "  }  } " 
          ;
}

static Error_T parseJson( const pt::ptree & in, EOPC_Tran_cred_T& this_ )
{
    SCOPE_LOG(slog);
    this_.amount    = in.get< int64_t >("amount", 0);
    this_.currency  = in.get< int64_t >("currency", 0);
    this_.date12    = in.get< std::string >("date12", "");
    this_.date7     = in.get< std::string >("date7", "");
    
    this_.expiry    = in.get< int64_t >("expiry", 0);
    
    this_.ext = in.get< std::string > ("ext", "");
    this_.field48 = in.get< std::string >("field48", "");
    this_.field91 = in.get< std::string >("field91", "");
    this_.merchantId = in.get<std::string>("merchantId", "");
    this_.pan        = in.get< std::string>("pan", "");
    this_.refNum     = in.get< std::string >("refNum", "");
    this_.resp       = in.get< int >("resp", -999999);
    this_.stan       = in.get< int >("stan", 0);
    this_.status     = in.get< std::string >("status", "" );
    this_.terminalId = in.get< std::string>("terminalId", "");
    this_.tranType   = in.get< std::string >("tranType", "");
    
    slog.DebugLog("resp: %d  status: %s ", this_.resp, this_.status.c_str() ) ;
    
    return Error_OK ;
}


static Error_T parseJson(const pt::ptree & in, EOPC_Tran_T& out)
{
    SCOPE_LOG(slog);
    out.id       = in.get< std::string>( "id" , "0" );
    out.pan      = in.get< std::string>( "pan" , "0");
    out.pan2     = in.get< std::string>( "pan2" , "0");
    out.tranType = in.get< std::string>( "tranType", "0" );
    out.amount   = in.get< int64_t>(  "amount", 0 );
    out.date7    = in.get< std::string>( "date7" , "0");
    out.stan     = in.get< int> ( "stan" , 0);
    out.date12   = in.get< std::string>( "date12" , "0");
    out.expiry   = in.get< std::string>(  "expiry", "0");
    out.refNum   = in.get< std::string>( "refNum" , "0");
    //autId    = in["authId"].asString();
    out.resp     = in.get< int >( "resp" , -9999999);
    out.ext      = in.get< std::string>( "ext", "0" );
    out.status   = in.get< std::string>( "status" , "");
    out.merchantId = in.get< std::string>( "merchantId", "" );
    out.terminalId = in.get< std::string>( "terminalId" , "");
    out.currency   = in.get< int > ( "currency", 0 );
    out.field48    = in.get< std::string>( "field48" , "");
    out.field91    = in.get< std::string>( "field91", "" );

    return Error_OK;
}

static Error_T parseJson(const pt::ptree & result, EOCP_Card_list_T& infos)
{
    typedef pt::ptree::value_type value_type;

    for(const value_type & e: result){
        EOCP_Card_list_T::value_type card;
        parseJson(e.second, card);
        infos.insert(card);
    }

    return Error_OK ;
}


static void fix_exp_dt(int exp, std::string& exp_str)
{
    //STANDARD way is  MMYY:   month's 2 digits after than year's last two digits.
    const int year  = (exp / 100 ) % 100 ; // 201905 => 2019 => 19
    const int month = exp % 100;           // 201905 =>  05

    // buf [ 4 ] = {  5/10 + '0', 5%10 + '0', 19/10 + '0' , 19%10 + '0'} 
    //          or {  0 + '0'   , 5 + '0'   , 1 + '0'     , 9 + '0' }
    //          or { '0',             '5',        '1',          '9' }
    //          or  "0519"
    exp_str.resize( 4 ) ;
    exp_str[0] = '0' + ( month / 10 ) ;
    exp_str[1] = '0' + ( month % 10 ) ;
    exp_str[2] = '0' + ( year  / 10 ) ;
    exp_str[3] = '0' + ( year  % 10 ) ;
}

static Error_T parseJson(const pt::ptree & in, EOPC_p2p_info_T& p)
{
    if ( ! in.count("EMBOS_NAME"))
        return Error_parameters;
    
    p.owner = in.get< std::string >("EMBOS_NAME") ;
    
    int exp =     in.get< int >("EXP_DT", 0)   ; 
    
    if (exp == 0 ) {//@fix EOPC ERROR:  2.0201E+5
        exp = static_cast< int > ( in.get< double >("EXP_DT", 0.0) ) ;
    }
    
    fix_exp_dt(exp, p.exp_dt);
    
    p.card_type = in.get< std::string > ("CARDTYPE");
    
    
    return Error_OK ;
}

static std::string to_json_str(const EOPC_P2P_in_data & in )
{
    return "{ \"merchantId\" : \"" + in.merchant_id            + "\", "
            " \"terminalId\" : \"" + in.terminal_id            + "\", "
            " \"port\"       : "   + to_str(in.port)           + ", "
            " \"sender\"     : "   + to_json_str(in.sender)    + " , "
            " \"recipient\"  : "   + to_json_str(in.recipient) + ", "
            " \"amount\"     : "   + to_str(in.amount)         + " , "
            " \"ext\"        : \"" + in.tran_id                + "\" , "
            " \"date12\"     : \"" +  formatted_time_now("%y%m%d%H%M%S") + "\" "
            " } "
            ;
}

#define EOPC_TRANS_STATUS_OK "OK"
#define EOPC_TRANS_STATUS_ERR "ERR"
#define EOPC_TRANS_STATUS_ROK "ROK"
#define EOPC_TRANS_STATUS_RER "RER"

bool EOPC_Tran_T::status_ok() const { return status == EOPC_TRANS_STATUS_OK ; }
bool EOPC_Tran_T::status_reverse_ok() const{ return status == EOPC_TRANS_STATUS_ROK; }

////////////////////////////////////////////////////////////////////////////////

namespace eopc = oson::backend::eopc;





namespace
{
template< eopc::Commands > struct int_{};

struct json_maker
{
    std::string operator()(const oson::backend::eopc::req::card& in, int_< eopc::cmd_card_new> )const
    {
        std::string params = "{ \"card\" : " + to_json_str(in) + " } " ;
        return append_rpc_header(params, CARD_NEW ) ;
    }
    
    std::string operator()(const std::vector< std::string> &ids, int_< eopc::cmd_card_info> )const
    {
        std::string params = "{ \"ids\" : [ " ;
        char comma = ' ';

        for(size_t i = 0, n = ids.size(); i != n; ++i) 
        {
            params += comma;
            params += "  \"" + ids[ i ] + "\" " ;
            comma = ',';
        }
        params += " ] } " ;
        
        return append_rpc_header(params, CARD_GET ) ;
    }
    std::string operator()(const std::string& id, int_< eopc::cmd_card_info_single> )const
    {
        std::string params = " {  \"ids\": [ \"" + id + "\" ] } " ;
        return append_rpc_header(params, CARD_GET);
    }
    
    std::string operator()(const std::string& id, int_< eopc::cmd_card_block> )const
    {
        std::string params = "{ \"id\" : \"" + id + "\" } " ;
        return append_rpc_header(params, CARD_BLOCK );
    }
    
    std::string operator()(const EOPC_Debit_T& debin, int_< eopc::cmd_trans_pay> )const
    {
        std::string params = "{  \"tran\" : " + to_json_str(debin) + "  } " ;
         return append_rpc_header(params, TRANS_PAY );
    }
    
    std::string operator()(const std::string& extId, int_< eopc::cmd_trans_extId> )const
    {
        std::string params = "{ \"extId\" : \"" + extId + "\" } " ;
        return append_rpc_header(params, TRANS_EXT);
    }
    
    std::string operator()(const std::string& tranId, int_< eopc::cmd_trans_sv> )const
    {
        std::string params = "{ \"svId\" : \"" + tranId + "\" } " ;
        return append_rpc_header(params, TRANS_SV);
    }
    
    std::string operator()(const std::string& tranId, int_< eopc::cmd_trans_reverse> )const
    {
        std::string params = "{ \"tranId\" : \"" + tranId + "\" } " ;
        return append_rpc_header(params, TRANS_REVERSE ) ;
    }
    
    std::string operator()(const std::string& hpan, int_< eopc::cmd_p2p_info> )const
    {
        std::string params = "{ \"hpan\" : \"" + hpan + "\" } " ;
        return append_rpc_header(params, P2P_INFO ) ;
    }
    std::string operator()(const EOPC_P2P_in_data& in, int_< eopc::cmd_p2p_id2id> )const
    {
        std::string params = "{ \"p2p\" : " + to_json_str(in) + " } " ;
        return append_rpc_header(params, P2P_ID2ID ) ;
    }

    std::string operator()(const EOPC_P2P_in_data &in, int_< eopc::cmd_p2p_id2pan> )const
    {
        std::string params = "{ \"p2p\" : " + to_json_str(in) + " } " ;
        return append_rpc_header(params, P2P_ID2PAN ) ;
        
    }
    
    std::string operator()(const EOPC_P2P_in_data & in, int_< eopc::cmd_p2p_pan2pan> )const
    {
        std::string params = "{ \"p2p\" : " + to_json_str(in) + " } " ;
        return append_rpc_header(params, P2P_PAN2PAN ) ;
    }
    std::string operator()(const EOPC_Credit_T& credit, int_< eopc::cmd_p2p_credit> )const
    {
        std::string params = "{ \"tran\" : " + to_json_str(credit) + " }  " ;
        return append_rpc_header( params  , P2P_CREDIT ) ;
    }
    std::string operator()(const EOCP_card_history_req& req, int_< eopc::cmd_card_history> ) const
    {
        std::string params = to_json_str(req);
        return append_rpc_header( params, CARD_HISTORY ) ;
    }
    std::string append_rpc_header( const std::string& param, std::string method)const
    {
        return "{ "
              "    \"jsonrpc\" : \"2.0\",  "
              "    \"id\"      :  1,       "
              "    \"method\"  : \"" + method + "\", "
              "    \"params\"  :   " + param  + "    "
              " } "
                ;
    }
    
};


struct eopc_result_maker
{
    std::string req_s;
    
    inline explicit eopc_result_maker( std::string r ): req_s( r )
    {}
    
    template< eopc::Commands cmd, typename T>
    Error_T operator()(const std::string& response, T& out, int_< cmd > cmd_int )const
    {
            return new_api(response, out, cmd_int);
    }

    template< eopc::Commands cmd, typename T >
    Error_T new_api(const std::string& response, T& out, int_< cmd > cmd_int ) const
    {
        //SCOPE_LOG(slog);
        if ( response.empty() )
            return Error_EOPC_connect;
        
        pt::ptree resp, nil;
        std::istringstream ss(response);
        try
        {
            pt::read_json(ss, resp);
        }
        catch(std::exception& e)
        {
            SCOPE_LOGD(slog);
            slog.ErrorLog("Exception: %s", e.what());
            return Error_internal;
        }
        
        Error_T ec = test_error(resp, (int)cmd, response);
        if (ec) return ec;
        
        if ( ! resp.count("result"))
            return Error_EOPC_connect;

        return parse_result(resp.get_child("result"), out, cmd_int);
    }
    
    template< eopc::Commands cmd, typename T >
    Error_T parse_result(const pt::ptree& result, T& out, int_< cmd > cmd_int ) const
    {
        return ::parseJson(result, out);
    }
    
    template< typename T >
    Error_T parse_result(const pt::ptree& result, T& out, int_< eopc::cmd_card_info_single > )const
    {
        return ::parseJson( result.front().second, out);
    }
    
    Error_T test_error(const pt::ptree & resp, int cmd, const std::string& resp_s )const
    {
        if (resp.empty()) {
            return Error_EOPC_connect;
        }
        if (int code = resp.get<int>("error.code", 0 )){
            if (code == -404 || code == 404 ) return Error_card_not_found ;
            if (code == -2222222) {
                if ( cmd != eopc::cmd_card_block ) { // block always gets this error
                    send_sms(code, resp_s );
                }
                return Error_EOPC_connect;
            }
            return Error_internal;
        }
        if (int code = resp.get<int>("result.error.code", 0 ) ) {
            if (code == -404 || code == 404 ) return Error_card_not_found;
            if (code == -2222222) {
                if (cmd  != eopc::cmd_card_block ) { // block card always gets this error.
                    send_sms(code, resp_s );
                }
                return Error_EOPC_connect;
            }
            return Error_internal;
        }
        return Error_OK  ;
    }
    
    void send_sms(int code, const std::string &resp_s )const
    {
        static time_t last_send = 0;
        
        const time_t now_time = time(NULL) ;
        
        if (!last_send || last_send < now_time - 1 * 10 * 60 ) // 10 mintes
        {
            last_send = now_time;
            SMS_info_T sms_info;

            sms_info.text = "EOPC может не работает, свяжитесь с админ. error-code: " + to_str(code) 
                    + "\nREQUEST : " + req_s  
                    + "\nRESPONSE: " + resp_s;


            sms_info.phone = oson_opts -> admin.phones;

            oson_sms -> async_send(sms_info);
        }
    }
};

} // end namespace

/*******************************************************************************************************************/
namespace
{
    
template< ::oson::backend::eopc::Commands > struct handler_dispatcher;

template<> struct handler_dispatcher< eopc::cmd_card_new >
{
    typedef oson::card_new_handler handler_type;
    typedef oson::backend::eopc::req::card  input_type;
    typedef oson::backend::eopc::resp::card    output_type;
};
template<> struct handler_dispatcher< eopc::cmd_card_info >
{
    typedef oson::card_info_handler     handler_type;
    typedef std::vector< std::string >  input_type;
    typedef EOCP_Card_list_T             output_type;
};
template<> struct handler_dispatcher< eopc::cmd_card_info_single >
{
    typedef oson::card_info_single_handler handler_type;
    typedef std::string input_type;
    typedef oson::backend::eopc::resp::card output_type;
};
template<> struct handler_dispatcher< eopc::cmd_card_block >
{
    typedef oson::card_block_handler handler_type;
    typedef std::string input_type;
    typedef oson::backend::eopc::resp::card output_type;
};
template<> struct handler_dispatcher< eopc::cmd_trans_pay >
{
    typedef oson::trans_pay_handler handler_type;
    typedef EOPC_Debit_T input_type;
    typedef EOPC_Tran_T  output_type;
};
template<> struct handler_dispatcher< eopc::cmd_trans_extId >
{
    typedef oson::trans_extId_handler handler_type;
    typedef std::string input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_trans_sv >
{
    typedef oson::trans_sv_handler handler_type;
    typedef std::string input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_trans_reverse >
{
    typedef oson::trans_reverse_handler handler_type;
    typedef std::string input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_p2p_info >
{
    typedef oson::p2p_info_handler handler_type;
    typedef std::string input_type;
    typedef EOPC_p2p_info_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_p2p_id2id >
{
    typedef oson::p2p_id2id_handler handler_type;
    typedef EOPC_P2P_in_data input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_p2p_id2pan >
{
    typedef oson::p2p_id2pan_handler handler_type;
    typedef EOPC_P2P_in_data input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_p2p_pan2pan >
{
    typedef oson::p2p_pan2pan_handler handler_type;
    typedef EOPC_P2P_in_data input_type;
    typedef EOPC_Tran_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_p2p_credit >
{
    typedef oson::p2p_credit_handler handler_type;
    typedef EOPC_Credit_T input_type;
    typedef EOPC_Tran_cred_T output_type;
};
template<> struct handler_dispatcher< eopc::cmd_card_history > 
{
    typedef oson::card_history_handler handler_type;
    typedef EOCP_card_history_req input_type;
    typedef EOCP_card_history_list output_type;
};

static const char* eopc_cmd_names( eopc::Commands cmd)
{
    using namespace eopc ;
    
    switch(cmd)
    {
        case cmd_card_new : return "cmd_card_new";
        case cmd_card_info: return "cmd_card_info";
        case cmd_card_info_single: return "cmd_card_info_single";
        case cmd_card_block: return "cmd_card_block";
    
        case cmd_trans_pay: return "cmd_trans_pay";
        case cmd_trans_extId: return "cmd_trans_extId";
        case cmd_trans_sv: return "cmd_trans_sv";
        case cmd_trans_reverse: return "cmd_trans_reverse";
    
        case cmd_p2p_info: return "cmd_p2p_info";
        case cmd_p2p_id2id: return "cmd_p2p_id2id";
        case cmd_p2p_id2pan: return "cmd_p2p_id2pan";
        case cmd_p2p_pan2pan: return "cmd_p2p_pan2pan";
        case cmd_p2p_credit: return "cmd_p2p_credit";
        
        case cmd_card_history: return "cmd_card_history";
    }
    return "<unknown cmd>" ;
}

template<  eopc::Commands cmd >
struct eopc_session: public std::enable_shared_from_this< eopc_session< cmd > > 
{
    typedef eopc_session self_type;
public:
    typedef typename handler_dispatcher< cmd > ::handler_type handler_type;
    typedef typename handler_dispatcher< cmd > ::input_type   input_type;
    typedef typename handler_dispatcher< cmd > ::output_type  output_type;
    
    
    eopc_session(const std::shared_ptr< boost::asio::io_service> & io_service, 
                 const std::shared_ptr< boost::asio::ssl::context > & context,
                 const eopc_network_info& net,
                 const input_type & in, 
                 const handler_type& h)
    : io_service(io_service)
    , ctx(context)
    , net_(net)
    , input(in)
    , handler(h)
    {
        //SCOPE_LOGD(slog);
    }
    
    void async_start()
    {
       // SCOPE_LOGD(slog);
        io_service->post(std::bind(&eopc_session::start_i, this->shared_from_this() ) );
        
    }
    ~eopc_session(){
        //SCOPE_LOGD(slog);
        
        if (static_cast< bool > (handler ) ) {
            SCOPE_LOGD_C(slog);
            slog.WarningLog("~eopc_session [%s] is wrong go", eopc_cmd_names(cmd) ) ;
            
            call_handler(output_type(), Error_internal);
        }
    }
    
private:
    void start_i()
    {
        SCOPE_LOGD_C(slog);
                
        typedef int_< cmd > curr_cmd;
    
        std::string req_s = json_maker()(input, curr_cmd() );
        
        this->req_time_    = std::time(0);
        this->req_pretty_s = oson::utils::prettify_json(req_s);
        
        slog.DebugLog("Request ( %s ) : %s\n", eopc_cmd_names( cmd ), this->req_pretty_s.c_str());
        
        oson::network::http::request req_http = oson::network::http::parse_url(net_.address);

        req_http.method          = "POST";
        req_http.content.charset = "utf-8";
        req_http.content.type    = "json";
        req_http.content.value   = req_s;
        if ( ! net_.authHash.empty() ) {
            req_http.headers.push_back(  "Authorization: Basic " + net_.authHash );
        }

        typedef oson::network::http::client client_t;
        typedef client_t::pointer pointer;
        
        pointer cl = std::make_shared< client_t >(io_service, ctx ) ;

        cl->set_response_handler (std::bind(&eopc_session::on_finish, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2) ) ;
        
        cl->set_request(req_http);
       
        //@Note for card history 5 seconds enough, because  very often timeout is received.
        const int time_out = ( cmd == eopc::cmd_card_history ) ?  5 * 1000 : 30 * 1000 ;
        
        cl->set_timeout( time_out ); 
        
        cl->async_start();
    }
    
    void on_finish(const std::string& resp_s, const boost::system::error_code& ec)
    {
        SCOPE_LOGD_C(slog);
        output_type out;
        
        if (ec == boost::asio::error::timed_out){
            slog.WarningLog("Timeout");
            
            if ( cmd != eopc::cmd_card_history ) 
            {
                this->send_sms_timeout(req_pretty_s);
            }
            
            return call_handler( out , Error_timeout );
        }
        //input_type in = input;//take a copy
        ///////////////////////////////////////////////////////////////////////
        slog.DebugLog("Response ( %s ) : %s\n",  eopc_cmd_names( cmd ), oson::utils::prettify_json(resp_s).c_str());
        typedef int_< cmd > curr_cmd;
    
        eopc_result_maker maker(req_pretty_s);
        Error_T ec_ = maker (resp_s, out, curr_cmd());
        /////////////////////////////////////////////////////////////////
        return call_handler( out, ec_  );
    }
    
    void call_handler(output_type const& out, Error_T ec )
    {
        if ( static_cast< bool > (handler) ) {
            handler_type h_tmp;
            h_tmp.swap(handler); // noexcept no throws, cheap
            
            h_tmp ( input, out, ec )   ;
        } else {
            SCOPE_LOGD_C(slog);
            slog.WarningLog("Response handler is empty!");
        }
    }
    
    
    void send_sms_timeout(std::string const& req_s)
    {
        SCOPE_LOGD( slog );
        static time_t last_send = 0;
        
        const time_t now_time = time( 0 ) ;
        
        if ( ! last_send || last_send < now_time - 1 * 10 * 60 ) // 10 minutes
        {
            auto g_opts = oson_opts ;
            
            last_send = now_time;
            SMS_info_T sms_info;
            sms_info.text = "EOPC возвращает таймоут. \n(" + formatted_time_iso( this->req_time_ ) + ")REQUEST : " + req_s + "\n(" + formatted_time_now_iso_S() + ")RESPONSE: TIMEOUT." ;
            sms_info.phone = g_opts->admin.phones ; 
            oson_sms -> async_send(sms_info);
        }
    }
private:
    eopc_session& operator = (eopc_session const&) ; // = delete
    eopc_session(const eopc_session& ); // = delete
    
private:
    std::shared_ptr< boost::asio::io_service > io_service;
    std::shared_ptr< boost::asio::ssl::context>   ctx;
    
    eopc_network_info net_;
    input_type input;
    handler_type handler;
    std::string req_pretty_s;
    std::time_t req_time_;
};
} // end

oson::EOPC_manager::EOPC_manager(const std::shared_ptr< boost::asio::io_service >& io_service , const eopc_network_info& info)
 : io_service_(io_service)
 , context_(std::make_shared< boost::asio::ssl::context>(boost::asio::ssl::context::sslv3))
 , net_(info)
{
    SCOPE_LOGD(slog);
    
    namespace ssl = boost::asio::ssl;
    
    context_->set_default_verify_paths();
    
    context_->set_verify_mode( ssl::context::verify_peer | ssl::context::verify_fail_if_no_peer_cert );
}

oson::EOPC_manager::~EOPC_manager()
{
    SCOPE_LOGD(slog);
}

void oson::EOPC_manager::async_card_new(const oson::backend::eopc::req::card& in, const card_new_handler& h)
{
    typedef eopc_session<eopc::cmd_card_new> session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    
    session_ptr   s =  std::make_shared< session_t > (io_service_, context_, net_, in, h);
    s->async_start();
}

namespace
{
    
struct multi_card_info_helper
{
    struct node
    {
        std::atomic_int_fast32_t      pos       ;
        std::vector < std::string >   ids       ;
        oson::card_info_handler       handler   ;
        EOCP_Card_list_T              result    ;
    };
    
    std::shared_ptr< node > d;
    
    void operator()(const std::vector< std::string> & ids, const EOCP_Card_list_T& info, Error_T ec)
    {
        d->result.insert( info  );
       
        if ( --(d->pos)   <= 0 )
        {
            if (static_cast< bool >(d->handler) ) {
                return d->handler( d->ids, d->result, ec );
            }
        }
    }
};

}

void oson::EOPC_manager::async_card_info(const std::vector< std::string > & ids, const card_info_handler & h)
{
    typedef eopc_session< eopc::cmd_card_info > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    
    size_t const max_ids = 3;
    
    if (ids.size() <= max_ids) // a very simple
    {
        session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_, ids, h);
        s->async_start();
    }
    else // every 3 card info will be a session.
    {
        size_t nsession = std::min< size_t >( 10,  (ids.size() + max_ids - 1) / max_ids );
        
        multi_card_info_helper helper;
        helper.d          = std::make_shared< multi_card_info_helper::node > ();
        helper.d->pos     = nsession;
        helper.d->ids     = ids;
        helper.d->handler = h;
        //helper.d->result = {};
        
        for(size_t pos = 0, i = 0; i < nsession; ++i)
        {
            std::vector< std::string > d;
            for( ; pos < ids.size() && d.size() < max_ids;  ++pos)
                d.push_back(ids[pos]);
            
            session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_, d, helper);
            s->async_start();
        }
    }
    
}

void oson::EOPC_manager::async_card_info(const std::string& id, const card_info_single_handler& h)
{
    typedef eopc_session< eopc::cmd_card_info_single > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,id, h);
    s->async_start();
}

void oson::EOPC_manager::async_card_block(const std::string& id, const card_block_handler & h)
{
    typedef eopc_session< eopc::cmd_card_block > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,id, h);
    s->async_start();
}
void oson::EOPC_manager::async_trans_pay(const EOPC_Debit_T& debin, const trans_pay_handler& h)
{
    typedef eopc_session< eopc::cmd_trans_pay > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,debin, h);
    s->async_start();
}

void oson::EOPC_manager::async_trans_extId(const std::string &extId, const trans_extId_handler& h )
{
    typedef eopc_session< eopc::cmd_trans_extId > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,extId, h);
    s->async_start();
    
}

void oson::EOPC_manager::async_trans_sv(const std::string &tranId,  const trans_sv_handler& h)
{
    typedef eopc_session< eopc::cmd_trans_sv > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,tranId, h);
    s->async_start();
    
}

void oson::EOPC_manager::async_trans_reverse(const std::string &tranId,  const trans_reverse_handler& h)
{
    typedef eopc_session< eopc::cmd_trans_reverse > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,tranId, h);
    s->async_start();
    
}

void oson::EOPC_manager::async_p2p_info   ( const std::string & hpan,    const p2p_info_handler&     h) 
{
    typedef eopc_session< eopc::cmd_p2p_info > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,hpan, h);
    s->async_start();
    
}
void oson::EOPC_manager::async_p2p_id2id  ( const EOPC_P2P_in_data &in,  const p2p_id2id_handler&    h) 
{
    typedef eopc_session< eopc::cmd_p2p_id2id > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,in, h);
    s->async_start();
    
}
void oson::EOPC_manager::async_p2p_id2pan ( const EOPC_P2P_in_data &in,  const p2p_id2pan_handler&   h) 
{
    typedef eopc_session< eopc::cmd_p2p_id2pan > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,in, h);
    s->async_start();
    
}
void oson::EOPC_manager::async_p2p_pan2pan( const EOPC_P2P_in_data &in,  const p2p_pan2pan_handler&  h) 
{
    typedef eopc_session< eopc::cmd_p2p_pan2pan > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,in, h);
    s->async_start();
    
}
void oson::EOPC_manager::async_p2p_credit ( const EOPC_Credit_T &credit, const p2p_credit_handler&   h) 
{
    typedef eopc_session< eopc::cmd_p2p_credit > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t> (io_service_, context_, net_,credit, h);
    s->async_start();
    
}

void oson::EOPC_manager::async_card_history( const EOCP_card_history_req& req, const card_history_handler & h ) 
{
    typedef eopc_session< eopc::cmd_card_history > session_t;
    typedef std::shared_ptr< session_t > session_ptr;
    session_ptr  s = std::make_shared< session_t > (io_service_, context_, net_, req, h);
    s->async_start();
}

eopc_network_info const&  oson::EOPC_manager::eopc_net_info()const
{
    return this->net_ ;
}

/******************************************************************************************************************/