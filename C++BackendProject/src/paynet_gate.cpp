
#include <numeric>
#include <functional>
#include <memory>
#include <time.h> // localtime_r

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "paynet_gate.h"
#include "utils.h"
#include "types.h"
#include "log.h"
#include "http_request.h"
#include "osond.h"
#include "application.h"
#include "merchant_api.h"

namespace paynet = oson::backend::merchant::paynet;


paynet::manager::manager( const access_t& acc)
        : acc_(acc)
{}

paynet::manager::~manager()
{}

static std::string fill_account(const struct paynet::access_t& acc)
{
    	//return "USERNAME=$username&PASSWORD=$password&TERMINAL_ID=$terminal_id";
    return "USERNAME="+acc.username + "&PASSWORD="+acc.password + "&TERMINAL_ID="+acc.terminal_id;
}

static std::string fill_fields(const std::map<std::string, std::string>& fields)
{
    std::string res;
    for(const auto & f : fields)
    {
        res += "&" + f.first + "=" + f.second ; 
    }
    return res;
}

static boost::property_tree::ptree  parse_xml(const std::string& xml)
{
    boost::property_tree::ptree root;
    std::istringstream ss(xml);
    try{
        boost::property_tree::read_xml(ss, root);
    }catch(std::exception& e){
        SCOPE_LOG(slog);
        slog.ErrorLog("what: %s", e.what() );
    }
    return root;
}

static std::string sync_http_ssl_request(oson::network::http::request req)
{
    SCOPE_LOGD(slog);
    
    std::shared_ptr< boost::asio::ssl::context> ctx = std::make_shared< boost::asio::ssl::context>( boost::asio::ssl::context::sslv23 );
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

static std::string get_request(std::string const& url)
{
     SCOPE_LOG(slog);

     std::string resp;
    oson::network::http::request req_ = oson::network::http::parse_url(url);
    req_.method = "GET";
    try
    {
        resp = sync_http_ssl_request(req_); 
    }
    catch(std::exception& e)
    {
        slog.ErrorLog("line(1)what: %s", e.what());
    }
    return resp;
}
 
static bool some_day_check(std::time_t t)
{
    static const int months[2][12] = {
        //Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
        { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
        { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    };
        // if this day  last two days of month of first two days of month , alos needn't retrieve
    struct tm lt_tm = {};
    localtime_r(&t, &lt_tm);
    
    // if hour not in [10..18], also load from file.
    if ( ! ( lt_tm.tm_hour >= 10 && lt_tm.tm_hour <= 18 ) )
        return true;
    
    int year = lt_tm.tm_year + 1900 ;
    int mon = lt_tm.tm_mon  ;
    int is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0 ) ) ? 1  : 0 ;
    
    if ( lt_tm.tm_mday == 1 || 
         lt_tm.tm_mday == 2 || 
         lt_tm.tm_mday == months[is_leap][mon] || 
         lt_tm.tm_mday == months[is_leap][mon] - 1  )
    {
        return true;
    }
    //saturday and sunday also , needn't 
    if (lt_tm.tm_wday == 0 || lt_tm.tm_wday == 6 ) 
        return true;
    
    return false;
     
}
static std::map< std::string, std::string> get_fields(const struct paynet::request_t& req, const struct paynet::access_t& acc)
{
    SCOPE_LOG(slog);
    
    std::map< std::string, std::string> fields;

//    fields["amount"] = to_str(req.amount);
//    //fields["summa"]  = to_str(req.amount);
//    
//    if ( ! req.client_login.empty() ){
//        if (! req.client_login_param_name.empty())
//        {
//            fields[req.client_login_param_name] = req.client_login;
//            return fields;
//        }
//    } else  { // client_login -- ignore
//        fields.insert(req.param_ext.begin(), req.param_ext.end());
//        return fields;
//    }
    std::string const paynet_services_file_name = "/etc/oson/merchants/paynet_services_list.txt" ;
    
    std::string resp_xml;
    bool load_from_file = false;
    ////////////////////////////////////////////////////////////////////////////
    do
    {
        bool exists = oson::utils::file_exists(paynet_services_file_name);
        
        if ( ! exists)
            break;
        
        std::time_t lt = oson::utils::last_modified_time(paynet_services_file_name);
        std::time_t now = std::time(NULL); 
        std::time_t diff = now - lt;
        std::time_t day_seconds = 24 * 60 * 60; // 24 hours, 60 minutes, 60 seconds
        if (diff > day_seconds  ) 
        {
            //load only after 24 hours, and not first two day of mont or last two days of month, or on saturday or on sunday.
            if ( ! some_day_check( now ) ) 
            {
                break; 
            }
        }
        
        std::ifstream fs(paynet_services_file_name.c_str(), std::ios::in | std::ios::binary ) ;
        
        if ( ! fs.is_open() ) // file not found
            break;
        
        std::ostringstream ss;
        ss << fs.rdbuf();
        resp_xml = ss.str();
        load_from_file = true;

        slog.DebugLog("loaded from file. resp_xml size: %zu", resp_xml.size());

    }while(0);       
    ////////////////////////////////////////////////////////////////////////////
    
    if ( ! load_from_file)
    {
        std::string address = acc.url + "?ACT=1&" + fill_account( acc ) ;

        resp_xml = get_request(address);
    }
    
    boost::property_tree::ptree resp_tree = parse_xml(resp_xml);

    boost::property_tree::ptree nil;
    boost::property_tree::ptree services = resp_tree.get_child("response", nil).get_child("services_list", nil);
    boost::property_tree::ptree serv_info;
    
    typedef boost::property_tree::ptree::value_type ptree_value_type;
    
    std::string service_id_s = to_str(req.service_id);// 104 - UCELL
    for(const ptree_value_type & val : services)
    {
        std::string id   = val.second.get< std::string>("<xmlattr>.id", "0");
        if (id == service_id_s)
        {
            slog.InfoLog("serv_info found: id = %s", id.c_str());
            serv_info = val.second;
            break;
        }
    }
    if(serv_info.empty())
    {
        slog.ErrorLog("not found service info");
        return fields;
    }
    
    std::string service_type_id = serv_info.get< std::string > ("<xmlattr>.service_type_id", "");
    
    if (service_type_id.empty()){
        slog.ErrorLog("not found service_type_id");
        return fields;
    }
    slog.DebugLog("service_type_id: %s", service_type_id.c_str()); // 386 - UCELL
    
    //2.my $fields = $resp->{response}->{service_type_list}->{service_type}->{$serv_info->{service_type_id}}->{details};
    boost::property_tree::ptree service_type_list = resp_tree.get_child("response", nil).get_child("service_type_list", nil);
    boost::property_tree::ptree fields_tree;
    
    for(const ptree_value_type& val : service_type_list)
    {
        std::string id = val.second.get< std::string > ("<xmlattr>.id", "0");
        if (id == service_type_id)
        {
            slog.InfoLog("field tree found: id = %s", id.c_str());
            fields_tree = val.second;
            break;
        }
    }
    
    if (fields_tree.empty())
    {
        slog.ErrorLog("Not found fields in service_type_list");
        return fields;
    }
    //3. make fields
    
    slog.DebugLog("fields_tree size: %d", (int)fields_tree.size());
    int step = 0;
    for(const ptree_value_type& val : fields_tree)
    {
        ++step;
        
        slog.DebugLog("step: %d, val.first: %s", step, val.first.c_str());
        if ( !(val.first == "details"))
            continue;
        
        const boost::property_tree::ptree& field = val.second;
        
        slog.DebugLog("step: %d, field size: %d, has(is_required): %s", step, (int)field.size(), (field.count("is_required") ? "YES": "NO") ) ;
        
       
        std::string  is_required = field.get<std::string>("is_required", "");
        
        if ( is_required == "true")
        {
            std::string field_name = field.get< std::string >("field_name");
            slog.DebugLog("field-name: %s", field_name.c_str());
            
            if (field_name == "summa" || field_name == "amount"){
                fields[field_name] = to_str( req.amount );
            } else  {
                std::string field_size_s = field.get< std::string > ("field_size");
                size_t field_size = string2num(field_size_s);
                
                if ( ! req.client_login.empty() )
                {
                    std::string client = req.client_login;

                    if (client.size() > field_size) // client: 998123456789  size: 12,  field_size = 7,  12-7=5 - first 5 symbols needn't
                        client = client.substr(client.size() - field_size);

                    fields[field_name ] = client;
                }
                else // use extra
                {
                    if (req.param_ext.count(field_name))
                    {
                        std::string value  = req.param_ext.at( field_name );
                        fields[ field_name ] = value;
                    }
                }
                slog.DebugLog("field-size: %d", (int)field_size);
            } 
        }
        else
        {
            if ( ! is_required.empty() )
                slog.WarningLog("is_required value: %s", is_required.c_str());
            else
                slog.WarningLog("is_required not found");
        }
    }
    
    slog.DebugLog("end parse fields.");
    
    if ( ! load_from_file ) // loaded from network, update file.
    {
        std::ofstream fo(paynet_services_file_name.c_str(), std::ios::out | std::ios::binary ) ;
        if (fo.is_open())
        {
            std::istringstream ss(resp_xml);
            fo << ss.rdbuf();
        }
    }
    //resp_xml
    
    return fields;
}

int paynet::manager::get_param(const request_t& req, param_t& param)
{
    SCOPE_LOG(slog);

//    
//    my $param = $ARGV[0];
//my $value = $ARGV[1];
//my $debug = $ARGV[2];
//
//

    //std::map< std::string, std::string>::const_iterator it = req.param_ext.begin();
    //std::string param = (*it).first ;
    //std::string value = (*it).second;
    
//my $url = 'https://213.230.106.115:8443/PaymentServer';
//my $username = 'akb442447';
//my $password = 'NvIR4766a';
//my $terminal_id = '4119362';
     /*----> m_acc have this informations*/
     
    
    
//my $resp = get_param($param, $value);
//sub get_param {
//        my ($param, $value) = @_;
//
//        my $uri = 'ACT=8';
//        $uri .= "&" . fill_account();
//        $uri .= "&$param=$value";
//        my ($resp, $raw) = get_request($uri);
//        return $resp;
//}
    namespace pt = boost::property_tree;
    pt::ptree resp_tree;
    {
        std::string address = acc_.url + "?ACT=8&" + fill_account(acc_) + fill_fields(req.param_ext);
        std::string resp_xml = get_request( address );
        slog.DebugLog("response: %.*s\n",  std::min< int >( 2048, resp_xml.size( ) ), resp_xml.c_str());
        if (resp_xml.empty()){
            return -1;
        }
        resp_tree = parse_xml(resp_xml);
    }
    
    //#print Dumper($list);
//my $list = $resp->{response}->{filials_list}->{filial};
//my $size = $resp->{response}->{filials_list}->{size};
//
//if ($size > 0) {
//        print "0\n";
//        foreach my $elem (@$list) {
//                print "$elem->{id}\n";
//                print "$elem->{name}\n";
//        }
//}
//else {
//        print "1\n";
//}
//
//exit 0;
//
//
    
    pt::ptree nil;
    const pt::ptree& filials = resp_tree.get_child("response.filials_list", nil);
    for(const pt::ptree::value_type& value : filials)
    {
        const std::string& key = value.first;
        if (key != "filial")
            continue;
        
        const pt::ptree& d = value.second;
        
        std::string id   = d.get< std::string >("<xmlattr>.id", "0");
        std::string name = d.get< std::string >("<xmlattr>.name", "<>");
        
        param.pm.insert( std::make_pair(id, name));
    }
    
    return 0 ;
}

int paynet::manager::get_info(const request_t& req, info_t& info)
{
    SCOPE_LOG(slog);
    namespace pt = boost::property_tree;
    
    std::map< std::string, std::string> fields = get_fields(req, acc_);
    pt::ptree resp_tree;
    //////////////////////////////////////////////////////////
    //////////////////////  GET-INFO   ////////////////////////
    /////////////////////////////////////////////////////////////
    {
        std::string address = acc_.url +  "?ACT=2&"+fill_account(acc_) + "&SERVICE_ID="+to_str(req.service_id);
        
        if (! req.fraud_id.empty()) 
            address  += "&FRAUD_CID="+req.fraud_id;
        
        address += "&EK_REQUEST_ID="+to_str(req.transaction_id) + "&CARD=1" + fill_fields(fields);
        
        std::string resp_xml = get_request(address);
        const std::string& b64 = /*oson::utils::encodebase64*/(resp_xml);
        slog.DebugLog("response(base64): %.*s\n",  std::min<int> (4096, b64.size() ), b64.c_str());
    
        if (resp_xml.empty() ) {
            return -1;
        }
        
        resp_tree = parse_xml(resp_xml);
    }
    
    ////////////////////////////////////////////////////////////////////
    //    2. CHECK STATUS
    boost::property_tree::ptree  nil;
    std::string status = resp_tree.get< std::string> ("response.status", "0");
    info.status = string2num(status);
    if ( info.status != 0)
        return -1;

    const pt::ptree& tran_tree = resp_tree.get_child("response.transaction", nil);

    status = tran_tree.get< std::string >("status", "-9879997") ;

    info.status_text = tran_tree.get< std::string>("status_text", "");
    info.status = string2num(status);

    if ( info.status != 0)
        return -1;
    
    ///////////////////////////////////////////////////////////////////////
    //    3. RECEITP
    ///////////////////////////////////////////////////////////////////////
    const pt::ptree& receipt_tree = tran_tree.get_child("receipt", nil);
    
    for(const pt::ptree::value_type& val : receipt_tree)
    {
        std::string key = val.first;
        std::string d    = val.second.data();

        info.kv.insert(std::make_pair(key, d));
    }
    
    return 0;
}



const char*  paynet::manager::invalid_transaction_value()
{
    return "--.-.-" ;
}

int paynet::manager::perform(/*IN*/ const request_t& req, /*OUT*/ response_t& resp)
{
    SCOPE_LOG(slog);
    namespace pt = boost::property_tree;
    
    //4.
    std::map< std::string, std::string> fields = get_fields(req, acc_);
    pt::ptree resp_tree;
    //PERFORM
    {
        std::string address = acc_.url +  "?ACT=2&"+fill_account( acc_ ) + "&SERVICE_ID="+to_str(req.service_id);
        
        if (! req.fraud_id.empty()) {
            address  += "&FRAUD_CID="+req.fraud_id;
        }
        
        address += "&EK_REQUEST_ID="+to_str(req.transaction_id) + "&CARD=1" + fill_fields(fields);
        
        std::string resp_xml = get_request(address);
        const std::string& b64 = /*oson::utils::encodebase64*/(resp_xml);
        slog.DebugLog("Response : %.*s\n",  std::min<int>(4096, b64.size()), b64.c_str());
        
        if (resp_xml.empty() ) {
            return -1;
        }
        
        resp_tree = parse_xml(resp_xml);
    }
    
    boost::property_tree::ptree  nil;
    std::string status = resp_tree.get< std::string> ("response.status", "0");
    resp.status = string2num(status);
    if ( resp.status != 0)
        return -1;
    
    const pt::ptree& tran_tree = resp_tree.get_child("response.transaction", nil);
    status = tran_tree.get< std::string >("status", "-9879997") ;
    resp.status_text = tran_tree.get< std::string>("status_text", "");
    resp.status = string2num(status);
    
    if ( resp.status != 0 && resp.status != -6) // -6 Платёж в очереди ждите!
        return -1;
        
    
    std::string tr_id = tran_tree.get<std::string>("receipt.transaction_id", invalid_transaction_value() );
    
    resp.transaction_id = tr_id;
    
    return 0;
}

int paynet::manager::check( /*IN*/ const request_t& req, /*OUT*/ response_t& resp)
{
    return -1;
}


/********************************************************************************************/
/*************************** ASYNC VERSION **************************************************/
namespace
{
    
enum paynet_cmd
{
    paynet_cmd_none       = 0 ,
    paynet_cmd_check      = 2 , 
    paynet_cmd_get_param  = 3 , 
    paynet_cmd_get_info   = 4 ,  
    paynet_cmd_perform    = 5 ,
};
 
namespace http = oson::network::http;
    

class paynet_session: public std::enable_shared_from_this< paynet_session   >
{
public:
    typedef paynet_session self_type;
    typedef std::shared_ptr< self_type > pointer;
    
    static pointer create( paynet::io_service_ptr ios, paynet::context_ptr ctx, paynet::access_t acc)
    { 
        return std::make_shared< self_type > (ios, ctx, acc ) ;
    }
    
    
    typedef paynet::handler_type handler_type;
    
    typedef paynet::response_t response_t;
        
    
    void set_request( const struct paynet::request_t& req , paynet_cmd cmd_id)
    {
        req_ = req;
        cmd_ = cmd_id;
    }
    
    void set_response_handler( const handler_type & h ) 
    {
        h_ = h;
    }
    
    void async_start()
    {
        ios_ -> post( std::bind(&self_type::start, this->shared_from_this() ) ) ;
    }
    
public:
    explicit paynet_session( paynet:: io_service_ptr ios, paynet:: context_ptr ctx, paynet::access_t acc)
    : acc_(acc)
    , ios_(ios)
    , ctx_(ctx)
    , req_()
    {
        SCOPE_LOG(slog);
    }
    
    ~paynet_session()
    {
        SCOPE_LOG(slog);
    }
    
    void start()
    {
        SCOPE_LOG(slog);

        http::request req = make_http_req();
        
        http::client::pointer c = http::client::create( ios_, ctx_ ) ;  

        c->set_request(req);
        c->set_response_handler(std::bind(&self_type::on_finish, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2 ) ) ;
        
        c->async_start();
    }
    
    void on_finish(const std::string& content, const ::boost::system::error_code& ec) 
    {
        SCOPE_LOGD( slog );
        
        if ( ! ec ) 
        {
            make_response(content ); 
            
        } else {
            response_t resp;
            resp.status = ec.value();
            resp.status_text  = ec.message();
            call_handler( resp ) ;
        }
    }
    
    void call_handler( const response_t& resp )
    {
        SCOPE_LOG(slog);
        
        if ( static_cast< bool >( h_ ) ) 
        {
            h_(req_, resp ); 
        } 
        else 
        {
            slog.WarningLog("No handler!") ;
        }
    }
    
    void make_response( const std::string& content) 
    {
        SCOPE_LOG(slog);
        
        response_t resp;
        
        boost::property_tree::ptree resp_tree;
        
        std::string resp_xml = content;
        {
            const std::string& b64 = /*oson::utils::encodebase64*/(resp_xml);
            slog.DebugLog("Response: %.*s\n",  std::min<int> (4096, b64.size() ), b64.c_str());
            resp_tree = parse_xml(resp_xml);
        }
        
        if (cmd_ == paynet_cmd_get_info)
        {
             

            ////////////////////////////////////////////////////////////////////
            //    2. CHECK STATUS
            boost::property_tree::ptree  nil;
            std::string status = resp_tree.get_child("response", nil).get< std::string> ("status", "0");
            resp.status = string2num(status);
            
            if ( resp.status != 0) 
            {
                return call_handler(resp);
                
            }

            boost::property_tree::ptree tran_tree = resp_tree.get_child("response", nil).get_child("transaction", nil);

            status = tran_tree.get< std::string >("status", "-9879997") ;

            resp.status_text = tran_tree.get< std::string>("status_text", "");
            resp.status = string2num(status);

            if ( resp.status != 0) {
                return call_handler(resp);
            }

            ///////////////////////////////////////////////////////////////////////
            //    3. RECEITP
            ///////////////////////////////////////////////////////////////////////
            boost::property_tree::ptree receipt_tree = tran_tree.get_child("receipt", nil);

            typedef boost::property_tree::ptree::value_type value_type;
            
            for(const value_type& val : receipt_tree)
            {
                std::string key = val.first;
                std::string d    = val.second.data();
                resp.kv.insert(std::make_pair(key, d));
            }
            resp.status = 0;
            return call_handler(resp);
        } 
        else if (cmd_ == paynet_cmd_perform) 
        {
            //PERFORM
            boost::property_tree::ptree  nil;
            std::string status = resp_tree.get_child("response", nil).get< std::string> ("status", "0");
            resp.status = string2num(status);
            if ( resp.status != 0) {
                return call_handler(resp);
            }

            boost::property_tree::ptree tran_tree = resp_tree.get_child("response", nil).get_child("transaction", nil);
            status = tran_tree.get< std::string >("status", "-9879997") ;
            resp.status_text = tran_tree.get< std::string>("status_text", "");
            resp.status = string2num(status);

            if ( resp.status != 0 && resp.status != -6) { // -6 Платёж в очереди ждите!
                return call_handler(resp);
            }

            std::string tr_id = tran_tree.get_child("receipt", nil).get<std::string>("transaction_id",  paynet::manager::invalid_transaction_value() );

            resp.transaction_id = tr_id;
            resp.status = 0;
            return call_handler(resp);
        }
        else if (cmd_ == paynet_cmd_get_param ) 
        {
            typedef boost::property_tree::ptree::value_type ptree_value_type;

            boost::property_tree::ptree nil;
            boost::property_tree::ptree filials = resp_tree.get_child("response", nil).get_child("filials_list", nil);
            
            for(const ptree_value_type& value : filials)
            {
                const std::string& key = value.first;
                if (key != "filial")
                    continue;

                std::string id = value.second.get< std::string >("<xmlattr>.id", "0");
                std::string name = value.second.get< std::string>("<xmlattr>.name", "<>");
                
                resp.kv.insert( std::make_pair(id, name));
            }
            resp.status = 0;
            return call_handler(resp);
        } 
        else 
        {
            resp.status = -1;
            resp.status_text  = "Unexpected command: " + to_str(cmd_);
            return call_handler(resp);
        }
    }
    
    http::request make_http_req( ) 
    {
        SCOPE_LOG(slog);
        if (cmd_ == paynet_cmd_get_info || cmd_ == paynet_cmd_perform ) 
        {
            std::map< std::string, std::string> fields = get_fields(req_, acc_);

            std::string address = acc_.url;

            address +=  "?ACT=2&"+fill_account(acc_) + "&SERVICE_ID=" + to_str( req_.service_id );

            if (! req_.fraud_id.empty() ) {
                address  += "&FRAUD_CID="+req_.fraud_id;
            }

            address += "&EK_REQUEST_ID=" + to_str(req_.transaction_id) + "&CARD=1" + fill_fields( fields ) ;

            http::request result = http::parse_url(address);
            result.method = "GET";

            return result;
        } 
        else if (cmd_ == paynet_cmd_get_param ) 
        {
            std::string address = acc_.url + "?ACT=8&" + fill_account(acc_) + fill_fields(req_.param_ext);
        
            http::request result = http::parse_url(address);

            result.method = "GET";

            return result;
        } else {
            return http::request();  // an empty!
        }
    }
     
    
private:
    
    paynet::access_t        acc_ ;
    paynet::io_service_ptr  ios_ ;
    paynet::context_ptr     ctx_ ;
    paynet::request_t       req_ ;
    paynet_cmd              cmd_ ;
    handler_type            h_   ;
};
    
}

void paynet::manager::async_get_info(const request_t& req, handler_type handler)
{
    SCOPE_LOGD(slog);
    
    auto api         = oson_merchant_api ;
    auto ctx_        = api->get_ctx_sslv23();
    auto io_service_ = api->get_io_service();
    
    auto session = paynet_session::create(io_service_, ctx_, acc_ );
    session->set_request(req, paynet_cmd_get_info);
    session->set_response_handler(handler);
    session->async_start();

}

void paynet::manager::async_perform(const request_t& req, handler_type handler)
{
    SCOPE_LOGD(slog);

    auto api         = oson_merchant_api ;
    auto ctx_        = api->get_ctx_sslv23();
    auto io_service_ = api->get_io_service();
    
    auto session = paynet_session::create(io_service_, ctx_, acc_ );
    session->set_request(req, paynet_cmd_perform );
    session->set_response_handler(handler);
    session->async_start();
    
}
