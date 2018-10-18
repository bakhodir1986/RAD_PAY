
#include <algorithm>
#include <stdexcept>
#include <exception>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <memory>

#include <thread>
#include <mutex>

#ifdef __unix__
#include <malloc.h>
#endif 


#define  BOOST_ERROR_CODE_HEADER_ONLY 1

#include <boost/system/error_code.hpp>

#include <boost/asio/io_service.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/utility/addressof.hpp>


#include <boost/scope_exit.hpp>
#include <boost/algorithm/string/trim.hpp>


#include "osond.h"
#include "types.h"
#include "queue.h"
#include "runtime_options.h"
#include "clientapi.h"
#include "adminapi.h"
#include "DB_T.h"
#include "ssl_server.hpp"
#include "users.h"
#include "admin.h"
#include "log.h"
#include "eocp_api.h"
#include "fault.h"
#include "sms_sender.h"
#include "transaction.h"
#include "purchase.h"
#include "eopc_queue.h"
#include "periodic_bill.h"
#include "http_request.h"

#include "cards.h"
#include "Merchant_T.h"
#include "merchant_api.h"
#include "eocp_api.h"
#include "eopc_queue.h"
#include "utils.h"
#include "exception.h"
#include "application.h"

#include "http_server.h"

#ifdef _WIN32
    #define CONFIG_FILE_LOCATION "osond.conf"
    #define PID_FILE "osond.pid"
    #define LOG_FILE_LOCATION "osond.log"
    #define IOS_CERT_FILE_LOCATION "production.pem"
#else
    #define CONFIG_FILE_LOCATION "/etc/oson/osond.conf"
    #define PID_FILE "/var/run/osond.pid"
    #define LOG_FILE_LOCATION "/var/log/osond.log"
    #define IOS_CERT_FILE_LOCATION "/etc/oson/ios/production.pem"
#endif

void show_header( const uint8_t * data, size_t length );
void show_data( const uint8_t * data, size_t length );

Error_T ss_bonus_earns(Purchase_info_T p_info );
Error_T check_card_daily_limit(const User_info_T& user_info, const Card_info_T& card_info, int64_t pay_amount);


struct startup_options_t
{
    
    std::string config  ;
    std::string pid     ;
    bool help           ;
    //bool daemonize      ;
    //bool debug          ;
    //bool checker        ;
};

static int print_osond_help()
{
    printf("Options:\n");
    printf("  -h [ --help ]                         Help\n");
    printf("  -c [ --config ] arg (=%s)\n", CONFIG_FILE_LOCATION );
    printf("                                        Config file location\n");
    printf("  -p [ --pid ] arg (=%s)\n", PID_FILE );
    printf("                                        Pid file\n");
    printf("  -d [ --daemonize ]                    Daemonize server\n");
    printf("  --debug                               Debug output to log\n");
    printf("  --checker                             Check osond running process\n");

    return 0;
}

static void print_startup_opts( const startup_options_t& opts)
{
    printf("config    : %s\n", opts.config.c_str());
    printf("pid       : %s\n", opts.pid.c_str());
    printf("help      : %s\n", opts.help      ? "true" : "false") ;
//    printf("daemonize : %s\n", opts.daemonize ? "true" : "false");
//    printf("debug     : %s\n", opts.debug     ? "true" : "false");
//    printf("checker   : %s\n", opts.checker   ? "true" : "false");
}

//An Expremental realization without boost program options.
static bool init_startup_options(int argc, char* argv[], startup_options_t& opts)
{
    opts.config    = CONFIG_FILE_LOCATION ; // default
    opts.pid       = PID_FILE             ; // default
    opts.help      = false ;
   // opts.daemonize = false ;
   // opts.debug     = false ;
   // opts.checker   = false ;
    
    // argv[0] - is name of program, skip it.
    for(int i = 1; i < argc ; ++i)
    {
        if (argv[i][ 0 ] != '-'){
            printf("undefined option: %s\n", argv[i]);
            continue;
        }
        
        if (argv[ i ][ 1 ] == '-' ) // '-- options'
        {
            if ( 0 == strcmp(argv[i] , "--help")) {
                opts.help = true;
            } else if (0 == strcmp(argv[i], "--daemonize") ) {
              //  opts.daemonize = true;
            } else if ( 0 == strcmp(argv[i], "--debug")) {
               // opts.debug = true;
            } else if ( 0 == strcmp(argv[i], "--checker" ) ) {
               // opts.checker = true;
            } else if ( 0 == strncmp(argv[i], "--config", 8 ) ) { 
                if (argv[i][8] == '\0') {
                    //take next token
                    if ( i + 1 < argc ) {
                        opts.config = static_cast< const char*>( argv[i+1] ) ;
                        ++i;
                    } else {
                        printf("--config next token not defined!");
                    }
                } else if (argv[i][8] == '=') {
                    opts.config = static_cast< const char*>(argv[i] + 9) ;
                } else {
                    printf("invalid options: %s\n", argv[i]);
                }
            } else if ( 0 == strncmp(argv[i], "--pid", 5) ) {
                if (argv[i][5] == '\0'){ 
                    if ( i + 1 < argc ) {
                        opts.pid = static_cast< const char*>(argv[i+1]);
                        ++i;
                    } else {
                        printf("--pid next token not defined!");
                    }
                } else if (argv[i][5] == '=') {
                    opts.pid = static_cast< const char*>( argv[i] + 6 ) ;
                } else {
                    printf("invalid options: %s\n", argv[i]);
                }
            }
        } else {
            bool im_exit = false ;
            
            for(int k = 1; argv[ i ][ k ] != '\0' && ! im_exit; ++k)
            {
                switch(argv[i][k])
                {
                    case 'h': opts.help      = true; break;
                 //   case 'd': opts.daemonize = true; break;
                    case 'c': 
                    {
                        if (argv[ i ][ k + 1 ] == '\0'){ // if this last symbol, next token is config file name
                            if ( i + 1 < argc ){
                                opts.config = static_cast< const char*> ( argv[ i + 1 ] );
                                ++i;
                                
                            } else {
                                    
                                printf("-c next token not defined\n");
                            }
                        } else { // otherwise next symbols is config file name
                            opts.config = static_cast< const char* >( argv[ i ] + k + 1 );
                        }
                        im_exit = true;
                    }
                    break;
                    case 'p':
                    {
                        if (argv[i][k+1] == '\0') { // take next token
                            if ( i + 1 < argc ) { 
                                opts.pid = static_cast< const char* > ( argv[ i + 1 ] );
                                ++i;
                            } else {
                                printf("-p next token not defined\n");
                            }
                        } else { // take other symbols
                            opts.pid = static_cast<  const char* >( argv[ i ] + k + 1);
                        }
                        im_exit = true;
                    }
                    break;     
                    default:
                    {
                        printf("undefined option: %c\n", argv[i][k]);
                    }
                    break;
                }
            }
        }
    }

    print_startup_opts(opts);

    return true;
}



static std::string value_or(const ::std::map< std::string, std::string>& vm, const std::string& name, const std::string& default_value)
{
    if (vm.count(name)){
        return vm.at(name);
    } else {
        return default_value;
    }
}

static int64_t value_or(const ::std::map< std::string, std::string> & vm, const std::string& name, int64_t default_value){
    if (vm.count(name)){
        return string2num( vm.at(name));
    } else {
        return default_value;
    }
}
static void trim_comment(std::string& line)
{
    size_t sep = std::string::npos;
    bool in_quot = false;
    char quot = '\0';
    for(size_t i = 0; i < line.size(); ++i)
    {
        if (line[i] == '\'' || line[i] == '\"' ) {
            if ( ! in_quot ){
                in_quot = true;
                quot = line[ i ];
            } else if (line[i] == quot ){
                in_quot = false;
            }
        }
        else if (/*line[i] == ';' ||*/ line[i] == '#') {
            if ( ! in_quot ){
                sep = i;
                break;
            }
        }
    }

    if (sep != std::string::npos) {
        line.erase( sep ); // erase all symbols starts from a sep.
        boost::trim( line );
    }
}

static int parse_bulk_sms(std::string const& x)
{
    if ( x == "off" ) return 0 ;
    if ( x == "on"  ) return 1 ;
    if ( x == "1"   ) return 1 ;
    if ( x == "0"   ) return 0 ;
    if ( x == "disable" || x == "disabled" ) return 0 ;
    if ( x == "enable"  || x == "enabled"  ) return 1 ;
    return 0 ;
}

static bool parse_ini_config( const std::string & config_file, oson::runtime_options_t & opts)
{
    //1.==========  init with default values =========================
    opts.main.log_file  = LOG_FILE_LOCATION ;
    opts.main.log_level = LogLevel_Error    ;
    opts.main.pid_file  = PID_FILE          ;
    

    opts.db.host =  "127.0.0.1"; // localhost
    opts.db.name =  "oson" ;
    opts.db.user =  "oson" ;
    opts.db.pwd  =  "oson" ; 

    opts.client.address   = "127.0.0.1" ;
    opts.client.port      = 0;
    opts.client.ssl_chain = "" ;
    opts.client.ssl_key   = "" ;
    opts.client.ssl_dh    = "" ;
    opts.client.ssl_pwd   = "" ;
    opts.client.online_timeout   = 15  ;
    opts.client.active_threads   = 7   ;
    opts.client.max_active_users = 128 ;
    opts.client.monitoring_off_on_timeout = 60;
    
    opts.admin.address   = "127.0.0.1" ;
    opts.admin.port      = 0;
    opts.admin.ssl_chain = "";
    opts.admin.ssl_key   = "";
    opts.admin.ssl_dh    = "";
    opts.admin.ssl_pwd   = "";
    opts.admin.phones    = "";

    opts.eopc.address     = "127.0.0.1" ;
    opts.eopc.authHash    = ""          ;
    opts.eopc.merchant_id = ""          ;
    opts.eopc.terminal_id = ""          ;
    opts.eopc.port        = 0           ;

    opts.ios_cert.certificate = "";
    opts.ios_cert.badge       = 1;
    opts.ios_cert.sandbox     = 0;
    
    opts.xmpp.ip   = "127.0.0.1" ;
    opts.xmpp.port = 8080;
    
    opts.sms.bulk_sms = 0; //"off"  ; // by default off.
    //////////////////////////////////////
    
    std::ifstream ifs(config_file.c_str());
    if ( ! ifs.is_open()) {
        return false;
    }
    
    ::std::map< std::string , std::string > vm;
    
    std::string line, section, variable, value;
    while( std::getline( ifs, line ) ) 
    {
        boost::trim( line );
        ////////////////////////////////////////
        if (line.empty() || line[ 0 ] == '#' || line[ 0 ] == ';' ){ // empty or comments
            continue;
        }
        /////////////////////////////////////
        if(line[0] == '[') // start section
        {
            size_t close_b = line.find(']') ;
            if (close_b == std::string::npos ){
                continue;//error section.
            }
            section = line.substr(1, close_b - 1);
            boost::trim( section );
            continue;
        }
        //////////////////////////////
        size_t sep = line.find( '=' ) ;
        if ( sep == std::string::npos )
        {
            continue;
        }
        
        variable = line.substr(0, sep);
        value    = line.substr(sep + 1);
        
        boost::trim( variable );
        boost::trim( value );
        
        trim_comment(value);
        
        if ( ! section.empty() ) {
            variable = section + "." + variable;
        }
        
        vm[ variable ] = value;
    }
    ///////////////////////////////////////////////////////////////////
    opts.main.log_file  = value_or( vm,  "main.log" , LOG_FILE_LOCATION )   ;
    opts.main.log_level = value_or( vm,  "main.log_level" , LogLevel_Error ) ;
    opts.main.pid_file  = value_or( vm,  "main.pid",  PID_FILE ) ;
 
    opts.db.host = value_or( vm,  "database.host", "127.0.0.1" ) ;
    opts.db.name = value_or( vm,  "database.name",  "oson" ) ;
    opts.db.user = value_or( vm,  "database.user",  "oson" ) ;
    opts.db.pwd  = value_or( vm,  "database.pass",  "oson" ) ;

    opts.client.address   = value_or( vm,  "client.listen_address", "127.0.0.1" )  ;
    opts.client.port      = value_or( vm,  "client.port",  8080) ;
    opts.client.ssl_chain = value_or( vm,  "client.ssl_chain" , "-") ;
    opts.client.ssl_key   = value_or( vm,  "client.ssl_key",  "-" ) ;
    opts.client.ssl_dh    = value_or( vm,  "client.ssl_dh",  "-") ;
    opts.client.ssl_pwd   = value_or( vm,  "client.ssl_password" , "-") ;
    opts.client.online_timeout   = value_or( vm,  "client.online_timeout", 15 );
    opts.client.active_threads   = value_or( vm,  "client.active_threads", 7  );
    opts.client.max_active_users = value_or( vm,  "client.max_active_users", 128 );
    opts.client.monitoring_off_on_timeout = value_or(vm, "client.monitoring_off_on_timeout", 60);
    
    opts.admin.address   = value_or( vm,  "admin.listen_address", "127.0.0.1") ;
    opts.admin.port      = value_or( vm,  "admin.port", 8080) ;
    opts.admin.ssl_chain = value_or( vm,  "admin.ssl_chain", "-") ;
    opts.admin.ssl_key   = value_or( vm,  "admin.ssl_key", "-") ;
    opts.admin.ssl_dh    = value_or( vm,  "admin.ssl_dh",  "-") ;
    opts.admin.ssl_pwd   = value_or( vm,  "admin.ssl_password", "-") ;
    opts.admin.phones    = value_or( vm,  "admin.phones", "-");
    
    opts.eopc.address     = value_or( vm, "eopc.address", "-") ;
    opts.eopc.authHash    = value_or( vm, "eopc.authHash", "-") ;
    opts.eopc.merchant_id = value_or( vm, "eopc.merchant_id" , "-");
    opts.eopc.terminal_id = value_or( vm, "eopc.terminal_id", "-") ;
    opts.eopc.port        = value_or( vm, "eopc.port", 8080) ;

    opts.ios_cert.certificate = value_or( vm,  "ios.certificate" , "-") ;
    opts.ios_cert.badge       = value_or( vm,  "ios.badge", 1);
    opts.ios_cert.sandbox     = value_or( vm,  "ios.sandbox", 0) ;
    
    opts.xmpp.ip   = value_or( vm,  "xmpp.ip",   "127.0.0.1");
    opts.xmpp.port = value_or( vm,  "xmpp.port" , 8080 ) ;
    
    opts.sms.bulk_sms      = parse_bulk_sms( value_or( vm, "sms.bulk_sms", "off") ) ;
    opts.sms.url           = value_or( vm, "sms.url", "");
    opts.sms.url_v2        = value_or( vm, "sms.url_v2", "");
    opts.sms.auth_basic_v2 = value_or( vm, "sms.auth_basic_v2", "");
    
    opts.certs.webmoney_private_key = value_or( vm, "certs.webmoney_private_key", "");
    opts.certs.ums_private_key      = value_or( vm, "certs.ums_private_key", "" ) ;
    opts.certs.ums_public_key       = value_or( vm, "certs.ums_public_key", "");
    opts.certs.ums_verify_cert      = value_or( vm, "certs.ums_verify_cert", "");
    
    return true;
    
}
 
static bool test_database( const struct oson::runtime_options_t::database_t & db_opts ) {
	Connect_info_T ci;

    ci.m_host    = db_opts.host  ;
    ci.m_db_name = db_opts.name  ;
    ci.m_pass    = db_opts.pwd   ;
    ci.m_user    = db_opts.user  ;

    DB_tag tag;
    DB_T db( tag );
    db.initConnectionInfo(ci);
    
    return Error_OK == db.connect( );
}

/////////////////////////////////////////////////////////////////////////
////  inner functions
/////////////////////////////////////////////////////////

static void sigintHandler(int signum);
static void sigusr1Handler(int signum) ;


std::string g_pid_filename;

std::mutex g_mutex_ios;
std::vector< boost::asio::io_service* > g_ios;

void scoped_register::register_io_service(boost::asio::io_service* ios)
{
    std::lock_guard< std::mutex > lock(g_mutex_ios);
    g_ios.push_back(ios);
}
void scoped_register::unregister_io_service(boost::asio::io_service* ios)
{
    std::lock_guard< std::mutex > lock(g_mutex_ios);
    g_ios.erase(std::remove(g_ios.begin(), g_ios.end(), ios), g_ios.end());
}

namespace
{
struct io_service_runner
{
    typedef std::shared_ptr< boost::asio::io_service > io_service_ptr;
   
    explicit io_service_runner(const io_service_ptr& ptr, std::string name): ios_(ptr), name( name ) {}

    void operator()()const
    {
        CLog::set_thread_name( name.c_str() );
        SCOPE_LOGD(slog);
        boost::asio::io_service::work w( *ios_ ) ;
        scoped_register ios_reg = make_scoped_register(*ios_);
        
        while(1)
        {
            try
            {
                boost::system::error_code ec;
                ios_->run(ec);
        
                if (ec == boost::asio::error::operation_aborted)
                    break;
            }
            catch(std::exception& e)
            {
                slog.ErrorLog("exception: '%s'\n", e.what());
                continue;
            }

            if (ios_->stopped())
                break;
        }
    }
private:
    io_service_ptr ios_;
    std::string name;
};
    
    
}



namespace // in C++11 mode there internal linkage
{
class pidfile
{
public:
#ifdef _WIN32
    typedef DWORD pid_int_t;
#else 
    typedef __pid_t  pid_int_t;
#endif 
    
    explicit pidfile(const std::string& filename);
    ~pidfile();
    
    pid_int_t readpid()const;
    int writepid(pid_int_t anewpid);
    static bool exists(pid_int_t);
    inline bool has_another_instance(){
        return exists( readpid() );
    }
private:
    pidfile(const pidfile&);//=deleted
    pidfile& operator = (pidfile&);//=deleted
private:
    const std::string filename; //not copyable
};
class scoped_pidfile
{
public:
    inline scoped_pidfile(const std::string& filename):filename(filename){
        pidfile pf(filename);
        pf.writepid(getpid()); // set a current 
    }
    inline ~scoped_pidfile(){ 
        pidfile pf(filename);
        pf.writepid(0);//clear it
    }
private:
    scoped_pidfile(const scoped_pidfile&); // =deleted
    scoped_pidfile& operator=(const scoped_pidfile&);//=deleted
private:
    std::string filename;
};
}



#ifdef _WIN32
    int fork(){ return -1; };
    void umask( int ){};
    void setsid(){};
    int chdir( char * ){ return 0; };
#endif 
    

pidfile::pidfile(const std::string& filename)
: filename(filename)
{}

pidfile::~pidfile()
{
}

pidfile::pid_int_t pidfile::readpid()const
{
    FILE* f = fopen(filename.c_str(), "r");
    if (!f){
        fprintf(stderr, "can't open %s file\n", filename.c_str());
        return 0;
    }
    pid_int_t pid = 0;
    int a = fscanf(f, "%d",&pid);
    fclose(f);
    (void)a;
    fprintf(stderr, "read %d pid from %s file\n", pid, filename.c_str());
    return pid;
}

int pidfile::writepid(pid_int_t anewpid)
{
    FILE * f = fopen(filename.c_str(), "w");
    if (!f){
        fprintf(stderr, "can't open %s file\n", filename.c_str());
        return -1;
    }
    (void)fprintf(f, "%d", anewpid);
    fclose(f);
    fprintf(stderr, "success write %d pid to %s file\n", anewpid, filename.c_str());
    return 0;
}

bool pidfile::exists(pid_int_t pid)
{
#ifdef _WIN32
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    DWORD ret = WaitForSingleObject(process, 0);
    CloseHandle(process);
    return ret == WAIT_TIMEOUT;
#else
    return 0 != pid  && 0 == kill(pid, 0);
#endif
}

static int stop_all_registered_io_services()
{
    {
        std::lock_guard< std::mutex > lock(g_mutex_ios);
        
        for(size_t i = 0; i < g_ios.size(); ++i)
            g_ios[i]->stop();
    }
    for(int i = 0; i < 100;++i)
    {
        // 50 milliseconds
        usleep(50000);
        
        bool ready = true;
        {
             std::lock_guard< std::mutex > lock(g_mutex_ios);
             ready = ready && g_ios.empty();
        }
        
        if (ready)
            return 0;
    }
    return -1;
}


static void sigintHandler(int signum)
{
    SCOPE_LOG( clog );
    int iret;
    clog.DebugLog( "Interrupt %d signal", signum );

    fprintf(stderr, "Interrupt sign ( %d ) received\n", signum);
    
    fprintf(stderr, "io-services stopped...\n");
    
    iret = stop_all_registered_io_services();
    
    fprintf(stderr, "io-services are stop!\n");
    
    static int loop = 0;
    ++loop;
    if (iret != 0 || loop > 1) //destroy LOG and pidfile handby.
    {
        clog.Destroy();
        pidfile pf(g_pid_filename);
        pf.writepid(0);
        exit(-1);
    }
    
 }

static void sigusr1Handler(int signum) 
{
    CLog log;
    log.Reinit();

//@Note: this removes|clears all unused memories. and back to OS.    
#ifdef __unix__
    malloc_trim(0);
#endif 

    
    signal(SIGUSR1, sigusr1Handler);
}

namespace
{
    
template< typename S >
class basic_numeric_range
{
public:
    typedef S size_type;
    
    static const size_type min_value;//  
    static const size_type max_value ;// 
    
    basic_numeric_range(): min_( min_value), max_(max_value){}
    
    basic_numeric_range(size_type a_min, size_type a_max): min_(a_min), max_(a_max){}
    
    explicit basic_numeric_range(size_type a_min): min_(a_min), max_(max_value){}
    
    size_type min()const{ return min_; }
    size_type max()const{ return max_; }
    
    bool empty()const{ return min_ > max_; }
    
    bool contains(size_type value)const{ return min_ <= value && value <= max_; }
private:
    size_type min_, max_;
};
    
template< typename S >
const typename basic_numeric_range<S>::size_type  basic_numeric_range< S >::min_value =  std::numeric_limits< size_type > ::min()  ;

template< typename S >
const typename basic_numeric_range<S>::size_type  basic_numeric_range< S >::max_value =  std::numeric_limits< size_type > ::max() ;

template< typename S >
inline bool operator < ( const basic_numeric_range< S > & lhs, const basic_numeric_range<S> & rhs ){
    return lhs.min() < rhs.min();
}

typedef basic_numeric_range< size_t >  numeric_range;


template< typename S >
class basic_numeric_set
{
public:
    typedef basic_numeric_range< S > range_type;
    typedef typename range_type::size_type size_type;
    typedef std::set< range_type > range_set;
    
    
    basic_numeric_set(){
        set_.insert( range_type() ) ;
    }
    
    explicit basic_numeric_set( range_type range )
    {
        set_.insert( range );
    }
    
    bool empty()const{ return set_.empty(); }
    
    size_type get( )
    {
        size_type result = range_type::max_value ;
        
        if ( ! set_.empty() )
            result = get_i() ;
        
        return result;
    }
    
    void set( size_type value )
    {
        range_type const new_range(value, value);
    
        
        // value <= low_it->min() 
        typename range_set::iterator low_it = set_.lower_bound(new_range);
        
        typename range_set::iterator prev_it = low_it;
        if (prev_it != set_.begin())
            --prev_it;
        
        
        bool const low_valid = low_it != set_.end();
        bool const prev_valid = prev_it != set_.end() && prev_it != low_it ;
        
        range_type const low_range  = low_valid  ? (*low_it) :  range_type() ;
        range_type const prev_range = prev_valid ? (*prev_it) : range_type() ;
         
        
        bool const low_eq  = low_valid  &&   low_range.min()  > value && value + ((size_type)1) == low_range.min() ;
        bool const prev_eq = prev_valid &&   prev_range.max() < value && value - ((size_type)1) == prev_range.max() ;
        
        //overlap
        if ( low_valid && low_range.contains(value) )
        {
            // already exists.
            return ;
        }
        
        if (prev_valid && prev_range.contains(value)  )
        {
            //already exists.
            return ;
        }
        
        // [ prev ] [value] [ low ]
        if (low_eq && prev_eq)
        {
            set_.erase(low_it);
            set_.erase(prev_it);
            
            set_.insert( range_type( prev_range.min(),  low_range.max() ) ) ;
            
        } 
        else if (low_eq) 
        {
            set_.erase(low_it);
            
            set_.insert( range_type( value, low_range.max() ) ) ;
        } 
        else if (prev_eq) 
        {
            set_.erase(prev_it);
            
            set_.insert(range_type( prev_range.min(), value ) ) ;
            
        } 
        else 
        {
            set_.insert(new_range);
        }
    }
    
private:
    size_type get_i()
    {
        assert( ! set_.empty() ) ;
        
        typename range_set::iterator it = set_.begin();
        
        size_type result = (*it).min();
        
        range_type new_range( result + 1,  (*it).max() ) ;
        
        set_.erase(it);
        
        if ( ! new_range.empty() ) {
            set_.insert(new_range);
        }
        
        return result;
        
    }
private:
    range_set set_;        
};

typedef basic_numeric_set < size_t > numeric_set;


} // end noname namespace

class ClientApiManager_T 
{
public:
    typedef ClientApiManager_T self_type;
    
    ClientApiManager_T(const std::shared_ptr< boost::asio::io_service > & ios,
                       
                       oson::runtime_options_t::client_t client_opts ); 
    
    
    //disable copy constructor and copy assignment.
    ClientApiManager_T(const ClientApiManager_T&) = delete;
    ClientApiManager_T& operator = (const ClientApiManager_T&) = delete;
    
    
    void start(); 

    ~ClientApiManager_T(); 
    
    void handle_request( byte_vector data,    ssl_response_handler  handler );
    
private:
    bool has_free_id()const;
    
    size_t get_free_id() ; 
    
    void set_free_id(size_t id);
    
private:
    ::std::vector< ::std::thread > thread_group ;
    std::shared_ptr< boost::asio::io_service > io_service, io_service_client ;
    std::map<size_t, std::shared_ptr< ClientApi_T >  > mp ;
    numeric_set                       numset  ;
    oson::runtime_options_t::client_t options ;
    ssl_server_T*                     server  ;
};


ClientApiManager_T:: ClientApiManager_T(const std::shared_ptr< boost::asio::io_service > & ios, oson::runtime_options_t::client_t client_opts )
 : io_service(ios)
 , io_service_client( std::make_shared< boost::asio::io_service > () )
 , mp()
 , numset( numeric_set::range_type( 1, client_opts.max_active_users ) ) 
 , options( client_opts )
 , server(nullptr)
 {
     SCOPE_LOGD(slog);
     io_service -> post( std::bind(&self_type::start, this ) ) ;
 }



 void ClientApiManager_T::start()
 {
     const auto& client_opts = this->options;

     ssl_server_runtime_options ssl_options;
     ssl_options.ip                = client_opts.address   ;
     ssl_options.port              = client_opts.port      ;

     ssl_options.cert_chain        = client_opts.ssl_chain ;
     ssl_options.private_key_file  = client_opts.ssl_key   ;
     ssl_options.dh_file           = client_opts.ssl_dh    ;
     ssl_options.password          = client_opts.ssl_pwd   ;


     int const max_threads =  ::oson::utils::clamp(client_opts.active_threads, 1, 10 ); 
     for(int i = 0; i < max_threads; ++i){
         std::string name = "client ";
         name[6] = i + '1';

         thread_group.emplace_back(   io_service_runner(io_service_client, name )  );
     }


     server  = new ssl_server_T( io_service, ssl_options, std::bind(&self_type::handle_request,  this , std::placeholders::_1, std::placeholders::_2) );
 }

 ClientApiManager_T::~ClientApiManager_T()
 {
     SCOPE_LOGD(slog);

     std::for_each(std::begin(thread_group), std::end(thread_group), std::mem_fn( & std::thread::join ) ) ;
     delete server;
 }

 void ClientApiManager_T::handle_request( byte_vector data,    ssl_response_handler  handler )
 {
     SCOPE_LOGD(slog);
     Server_head_T head = {};

     head = parse_header(data.data(), data.size());

     ::std::size_t const head_length    = std::min< ::std::size_t >( Server_head_T::length, data.size() );        
     //::std::size_t const payload_length = std::min< ::std::size_t > ( head.data_size, data.size() - head_length ) ;

     show_header( data.data(),   head_length );
     //show only first 128 bytes here.
     //show_data( data.data() + head_length, ::std::min< ::std::size_t >( payload_length, 128 ) );


     std::vector<unsigned char> fatal_v(6);
     {
         ByteWriter_T writer(fatal_v);
         writer.writeByte2(Error_login_failed);
         writer.writeByte4(0);
     }
     if (data.size() < Server_head_T::length ){
         slog.ErrorLog("data size invalid!");
         return handler(fatal_v);
     }

     switch(head.cmd_inner_id)
     {
         case Server_head_T::CMD_INNER_NONE:
         {
             size_t id = head.inner_id_val;

             if (!mp.count(  id ))
             {
                 slog.ErrorLog("can't found id: %u", id );
                 handler( fatal_v );
                 return;
             }

             // a common
             const std::shared_ptr< ClientApi_T >& api = mp[ id ];

             io_service_client->post( std::bind( &ClientApi_T::exec, api, std::move( data ), std::move(handler) ) ) ;

             slog.DebugLog("outer cmd: %u", head.cmd_id) ;
         }
         break;
         case Server_head_T::CMD_INNER_CREATE:
         {
             bool const has_id = has_free_id();

             //create
             size_t id = has_id ? get_free_id() : 0;

             if ( has_id ){
                 mp.insert( std::make_pair(id,  std::make_shared< ClientApi_T >( io_service )  ) );
             }

             slog.DebugLog("Create a new client api with inner id: %zu, total active clients: %zu", id, mp.size() );

             head.inner_id_val = id;

             std::vector<unsigned char> v(Server_head_T::length);
             encode_header(head, v.data(), v.size());

             handler(v);

         }
         break;
         case Server_head_T::CMD_INNER_DELETE:
         {
             size_t id = head.inner_id_val;

             if (id == 0) // do not use zero.
                 return;

             // a remove
             if (!mp.count( id ))
             {
                 slog.ErrorLog("can't found id: %zu", id );
                 return;
             } 

             mp.erase( id );

             set_free_id( id ) ;

             slog.DebugLog("Delete the client api with inner id: %zu, total current clients: %zu", id, mp.size() ) ;
         }
         break;
         default:
         {
             slog.ErrorLog("Unknown inner cmd.");
             handler(fatal_v);
         }
         break;
     }
 }

 bool ClientApiManager_T::has_free_id()const{ return ! numset.empty() ; }

 size_t ClientApiManager_T::get_free_id()
 {
     return numset.get();
 }
 void ClientApiManager_T::set_free_id(size_t id)
 {
     numset.set(id) ;
 }

 
/*****************************************************************************************/
class AdminApiManager_T
{
public:
    explicit AdminApiManager_T(std::shared_ptr< boost::asio::io_service > io_service,  oson::runtime_options_t::admin_t admin_opts);
    
    ~AdminApiManager_T();    
    
    //NON-COPYABLE, NON-ASSIGNABLE.
    AdminApiManager_T( const AdminApiManager_T& ) = delete ;
    AdminApiManager_T& operator = ( const AdminApiManager_T& ) = delete ;
    
    void start();
    
    void handle_request(  std::vector<unsigned char>  data,   ssl_response_handler  handler );
private:
    bool has_free_id()const;
    void set_free_id(size_t id);
    size_t get_free_id();
private:
    std::shared_ptr< boost::asio::io_service > m_io_service;
    std::map<size_t, std::shared_ptr<AdminApi_T>  > mp;
    numeric_set numset;
    oson::runtime_options_t::admin_t admin_opts;
    ssl_server_T* server;
};


AdminApiManager_T::AdminApiManager_T(std::shared_ptr< boost::asio::io_service > io_service,  oson::runtime_options_t::admin_t admin_opts)
 : m_io_service(io_service)
 , mp()
 , numset( numeric_set::range_type( 1, 1000000 ) )
 , admin_opts(admin_opts)
 , server(nullptr)
{
    io_service->post(std::bind(&AdminApiManager_T::start, this ) ) ; 
}
    
AdminApiManager_T::~AdminApiManager_T()
{
    delete server;
}
    
    
void AdminApiManager_T::start()
{
    SCOPE_LOGD(slog);
    ssl_server_runtime_options options;
    options.ip                = admin_opts.address   ;
    options.port              = admin_opts.port      ;
    options.cert_chain        = admin_opts.ssl_chain ;
    options.dh_file           = admin_opts.ssl_dh    ;
    options.password          = admin_opts.ssl_pwd   ;
    options.private_key_file  = admin_opts.ssl_key   ;

    server = new ssl_server_T ( m_io_service, options, std::bind(&AdminApiManager_T::handle_request, this , std::placeholders::_1, std::placeholders::_2) ) ;

}
    
void AdminApiManager_T::handle_request(  std::vector<unsigned char>  data,   ssl_response_handler  handler )
{


    Server_head_T head = {};
    head = parse_header(data.data(), data.size());


    std::vector<unsigned char> fatal_v(6);
    {
        ByteWriter_T writer(fatal_v);
        writer.writeByte2(Error_login_failed);
        writer.writeByte4(0);
    }        



    switch(head.cmd_inner_id)
    {
        case Server_head_T::CMD_INNER_NONE:
        {
            if (!mp.count(head.inner_id_val))
            {
                SCOPE_LOGD(slog);
                slog.ErrorLog("can't found id: %u", head.inner_id_val);
                handler( fatal_v );
                return;
            }

            // a common
            std::shared_ptr< AdminApi_T > api= mp[ head.inner_id_val ];
            api->exec( std::move( data ),  handler );
        }
        break;
        case Server_head_T::CMD_INNER_CREATE:
        {
            bool has_id = has_free_id();
            size_t id = has_id ? get_free_id() : 0;

            if (has_id) {
                //create
                mp.insert( std::make_pair( id,  std::make_shared< AdminApi_T > ( m_io_service ) ) );
            }

            //
            head.inner_id_val = id;

            std::vector<unsigned char> v(Server_head_T::length);
            encode_header(head, v.data(), v.size());

            handler(v);

        }
        break;
        case Server_head_T::CMD_INNER_DELETE:
        {
            size_t id = head.inner_id_val;

            if (!mp.count(id))
            {
                SCOPE_LOGD(slog);
                slog.ErrorLog("can't found id: %u",  id );
                handler( fatal_v );
                return;
            }

            // a remove
            mp.erase( id );

            set_free_id( id );
        }
        break;
        default:
        {
            SCOPE_LOGD(slog);
            slog.ErrorLog("Unknown inner cmd.");
            handler(fatal_v);
        }
        break;
    }
}

bool AdminApiManager_T::has_free_id()const{ return ! numset.empty(); }

void AdminApiManager_T::set_free_id(size_t id){
    numset.set(id);
}
size_t AdminApiManager_T::get_free_id()
{
    return numset.get();
}





namespace
{
    
typedef std::shared_ptr< boost::asio::io_service > io_service_ptr ;
    
class StartOnline_checker_T 
{
public:
    typedef StartOnline_checker_T self_type;
    
    StartOnline_checker_T( const io_service_ptr & io_service, int timeout_in_seconds ) ;
   
    ~StartOnline_checker_T();
    
private:
    StartOnline_checker_T(const StartOnline_checker_T&); // = deleted
    StartOnline_checker_T& operator = (const StartOnline_checker_T&); // = deleted
private:
    void start_async() ;
    void time_handle(const boost::system::error_code& ec) ;
    void del_old_devices( ) ;
    void check_demo_user( ) ;
    void do_job() ;
    void bonus_card_info_handler_i( Card_bonus_list_T  list, const std::vector< std::string > & ids, EOCP_Card_list_T const& infos, Error_T ec);
    void bonus_card_info_handler( Card_bonus_list_T  list, const std::vector< std::string > & ids, EOCP_Card_list_T const& infos, Error_T ec) ;
    void check_bonus_card_balance( ) ;
    
    static std::string find_demo_uid();
private: 
    io_service_ptr  m_ios;
    int m_timeout_in_seconds;
    boost::asio::deadline_timer m_deadline_timer;
    
    std::string demo_uid;
};

StartOnline_checker_T::StartOnline_checker_T( const io_service_ptr & io_service,
        int timeout_in_seconds 
)
: m_ios(io_service)
, m_timeout_in_seconds(timeout_in_seconds)
, m_deadline_timer( *io_service )
, demo_uid()
{
    start_async();
}


StartOnline_checker_T::~StartOnline_checker_T()
{}

void StartOnline_checker_T::start_async(){
    m_deadline_timer.expires_from_now(boost::posix_time::seconds(m_timeout_in_seconds));
    m_deadline_timer.async_wait(std::bind(&self_type::time_handle, this, std::placeholders::_1));
}

void StartOnline_checker_T::time_handle(const boost::system::error_code& ec)
{
    if (ec)
    {
        SCOPE_LOGD(slog);
        slog.ErrorLog("ec: %d, msg: %s ", ec.value(), ec.message().c_str());
        return;
    }

    m_ios->post( std::bind( &self_type::do_job                   , this ) ) ;
    m_ios->post( std::bind( &self_type::check_bonus_card_balance , this ) ) ;
    m_ios->post( std::bind( &self_type::check_demo_user          , this ) ) ;
    m_ios->post( std::bind( &self_type::del_old_devices          , this ) ) ;
    start_async();
}

void StartOnline_checker_T::del_old_devices( )
{
    SCOPE_LOG(slog);
    std::string  query =  "DELETE FROM user_devices WHERE login_ts < Now() - interval '200 day' "   ;
    DB_T::statement st(  oson_this_db  );
    st.prepare( query );
}
std::string StartOnline_checker_T::find_demo_uid()
{
    SCOPE_LOG(slog);
    DB_T::statement st( oson_this_db ) ;
    st.prepare(   "SELECT id FROM users WHERE phone = '998000000000' "    ) ;
    
    if (st.rows_count() != 1) 
        return std::string();
    
    int64_t id = 0;
    st.row(0) >> id;

    return to_str( id ) ;
}

void StartOnline_checker_T::check_demo_user( )
{
    SCOPE_LOG(slog);
    
    if ( demo_uid.empty() ) {
        demo_uid = find_demo_uid();
        
        if (demo_uid.empty()) {
            return ;
        }
    }
    
    if (demo_uid == "0")
        return ;
    
    //1. delete bonus cards
    DB_T::statement st( oson_this_db );
    st.prepare(  "DELETE FROM user_bonus    WHERE uid = " + demo_uid ) ;
    st.prepare(  "DELETE FROM user_devices  WHERE uid = " + demo_uid ) ;
    st.prepare(  "DELETE FROM periodic_bill WHERE uid = " + demo_uid ) ;
    st.prepare(  "DELETE FROM favorite      WHERE uid = " + demo_uid ) ;
    st.prepare(  "DELETE FROM notification  WHERE uid = " + demo_uid ) ;
    st.prepare(  "DELETE FROM news          WHERE uid = " + demo_uid ) ;
}

void StartOnline_checker_T::do_job()
{
    SCOPE_LOGD(slog);
    //1. check online users.
    Users_online_T users_o(  oson_this_db );
    users_o.check_online();

    //2. check online administrators.
    Admin_T admins(  oson_this_db  );
    admins.check_online();


    std::string  query =  "DELETE FROM activate_code WHERE (valid = 'FALSE') OR ( add_ts <= Now() - INTERVAL '8 hour' ) " ;

    DB_T::statement st( oson_this_db );
    st.prepare(query) ;
}

void StartOnline_checker_T::bonus_card_info_handler_i( Card_bonus_list_T  list, const std::vector< std::string > & ids, EOCP_Card_list_T const& infos, Error_T ec)
{
    //This line runs on another thread, so we send  handler to online checker thread.
    m_ios->post(std::bind(&self_type::bonus_card_info_handler,  this,  list, ids, infos, ec ) ) ;
}

void StartOnline_checker_T::bonus_card_info_handler( Card_bonus_list_T  list, const std::vector< std::string > & ids, EOCP_Card_list_T const& infos, Error_T ec)
{
    SCOPE_LOG( slog );

    if ( ec ) {
        slog.ErrorLog("Error code: %d", ( int )ec );
        return  ;
    }
    DB_T::statement st(  oson_this_db  );

    int64_t total_balance = 0;

    for(size_t i = 0; i < list.size(); ++i)
    {
        Card_bonus_info_T bcard = list[i];

//        if ( ! infos.count(bcard.pc_token))
//            continue;

        oson::backend::eopc::resp::card eocp_card = infos.get(bcard.pc_token);
        if (eocp_card.empty())
            continue;
        
        int64_t new_balance = eocp_card .balance;

        if (new_balance !=  bcard.balance )
        {
            bcard.balance = new_balance;

            Cards_T cards(  oson_this_db  );
            cards.bonus_card_edit_balance(bcard.card_id, bcard) ;
        } else {
            slog.WarningLog("balance not changed. card-id: %lld", bcard.card_id);
        }

        total_balance += new_balance;
    }
    int64_t sum_balance = 0 ; // sum of all user earn  balances.
    {
        std::string query =   "SELECT sum(balance) FROM user_bonus"   ;

        st.prepare( query );
        st.row(0) >> sum_balance;
    }

    //long long previous_diff = previous_balance - sum_balance;
    int64_t diff_balance = total_balance     - sum_balance;
    const int64_t DIFF_ALLOW = 500 * 1000 * 100 ; // 500 ming sum.
    if (diff_balance < DIFF_ALLOW ) // 50 000 sum
    {
        std::string query = "SELECT count(*) FROM card_bonus  WHERE (notify_ts IS NULL) OR (notify_ts < Now() - interval '3 hour') " ;
        st.prepare(query);

        int64_t cnt = 0;
        st.row(0) >> cnt;

        if ( cnt > 0 )
        {
            SMS_info_T sms_info;
            sms_info.text = "Общая сумма в бонусах " + to_money_str(sum_balance) + 
                            " сум, баланс на корпоратинвой карте  " + to_money_str(total_balance) + 
                            " сум, разница " + to_money_str(diff_balance) + " сум. Пожалуйста пополните корпоративную карту!";

            sms_info.phone = oson_opts -> admin.phones ;
            sms_info.type = sms_info.type_bonus_admin_sms ;
            
            oson_sms -> async_send(sms_info);

            st.prepare(  "UPDATE card_bonus SET notify_ts = NOW();"  );
        } // end if

    } // end if
} // end method

void StartOnline_checker_T::check_bonus_card_balance( )
{
    SCOPE_LOG(slog);

    Cards_T cards(  oson_this_db  );

    Card_bonus_list_T list;

    //Get all bonus cards
    cards.bonus_card_list(0, list);

    if (list.empty()) {
        slog.WarningLog("There no bonus card!");
        return ;
    }
    std::vector< std::string > ids( (list.size() ) );
    for(size_t i = 0; i < ids.size(); ++i)
        ids[i] = list[i].pc_token ;

    oson_eopc -> async_card_info( ids,  std::bind(&self_type::bonus_card_info_handler_i, this, list, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) );
}


} // namespace


/****
 *                 ALGORITHM OF CARD MONITORING MONTH PAY .
 * 
 * 1)    ----> start-main checker --->  void start_job (2)
 * 2)    -----> start-read-database ---> check_card_monitoring (3)  and also start-main checker after 1 hour.
 * 3)    ------> read chunk of [16 items] from database, turn-off they from db.   Pack all (uid, card_id) to vector --->  start pay job (4) [ void do_pay ]
 *                @Note: if (3) step no item is available in database, stop processing. After 1 hour (1) repeated.
 * 
 * 4)    ------> pay last item from vector, and retry void do_pay for other items with 0.5 second wait ---> void do_pay (4)
 *                @Note: if vector is empty, stop pay, and go to (2) step  reading from database.
 */

/*@See clientapi.cpp file.*/
/*static*/ Error_T  global_card_monitoring_edit( int64_t uid, 
                                                 /*int64_t card_id, */
                                                 int32_t monitoring_flag,  
                                                 std::shared_ptr< boost::asio::io_service > io_service_ptr 
                                                ) ;

class Card_monitoring_month_checker
{
public:
    typedef Card_monitoring_month_checker self_t;
    
    typedef std::vector<  std::pair< std::int64_t, std::int64_t > > vec_pii_type;
    
    explicit Card_monitoring_month_checker(io_service_ptr );
    ~Card_monitoring_month_checker();
    
    
private:
    void start_timer_g(int wait_seconds);
    void start_timer_db(int wait_milliseconds ) ;
    
    
    void start_timer_pay(int wait_milliseconds, vec_pii_type v ) ;
    
    
    void start_job();
    
    void check_card_monitoring();
    
    void do_pay( int wait_milliseconds,  vec_pii_type v);
    
    void find_oson_pid();
private:
    io_service_ptr m_ios ;
    //timer_g   - every hour wackup
    //timer_db  - every 1 second by 16 rows.
    //timer_pay - every 0.5 second by pay.
    boost::asio::deadline_timer m_timer_g, 
                                m_timer_db, 
                                m_timer_pay ;
};

Card_monitoring_month_checker::Card_monitoring_month_checker(io_service_ptr io )
  : m_ios(io), m_timer_g(*io), m_timer_db(*io), m_timer_pay(*io)
{
    start_timer_g( 36  ) ;
}

Card_monitoring_month_checker::~Card_monitoring_month_checker()
{
}
void Card_monitoring_month_checker::start_timer_g(int wait_seconds)
{
    m_timer_g.expires_from_now( boost::posix_time::seconds(wait_seconds ) ) ;
    m_timer_g.async_wait( std::bind( &self_t::start_job , this ) ) ;
}

void Card_monitoring_month_checker::start_timer_db(int wait_milliseconds ) 
{
    m_timer_db.expires_from_now( boost::posix_time::milliseconds(wait_milliseconds) ) ;
    m_timer_db.async_wait( std::bind(&self_t::check_card_monitoring, this ) ) ;
}

void Card_monitoring_month_checker::start_timer_pay(int wait_milliseconds, vec_pii_type v ) 
{
    m_timer_pay.expires_from_now( boost::posix_time::milliseconds(wait_milliseconds) ) ;
    m_timer_pay.async_wait( std::bind( &self_t::do_pay, this, wait_milliseconds, v ) ) ;
}

void Card_monitoring_month_checker::start_job()
{
    SCOPE_LOGD(slog);
    
    start_timer_db( 100 );//after 100 milliseconds.
    
    start_timer_g( 3600 ); // after 1 hour.
    
    return find_oson_pid();
}

void Card_monitoring_month_checker::find_oson_pid()
{
    SCOPE_LOGD(slog);
#if 0    
    std::string query = "SELECT count(*) FROM card_monitoring_load WHERE status = 6 " ;
    DB_T::statement st(oson_this_db);
    
    st.prepare(query);
    
    int64_t total_cnt = 0;
    st.row(0) >> total_cnt ;
    
    if ( total_cnt == 0 ) {
        slog.WarningLog("There no in-progress monitoring-load data!" ) ;
        return ;
    }
    
    query = 
            "UPDATE card_monitoring SET oson_pid = ss.pid "
            "FROM ("
            "SELECT p.id as pid, c.id as cid "
            "  FROM purchases p INNER JOIN card_monitoring c ON "
            "  (  (p.transaction_id ~ '^[0-9]+$' ) AND ( p.transaction_id :: bigint = c.refnum) ) "
            "  WHERE c.oson_pid = 0 ) ss "
            " WHERE  ( id = ss.cid )  AND  ( oson_pid = 0 ) ;" ;
    
    st.prepare(query);
    
    
    query = "UPDATE card_monitoring_load SET status = 1 WHERE status = 6 " ;
    
    st.prepare(query);
#endif 
}

void Card_monitoring_month_checker::check_card_monitoring()
{
    SCOPE_LOGD(slog);
    
    typedef std::int64_t bigint;
    typedef std::pair< bigint, bigint > pii;
    typedef std::vector< pii  > vec_pii;
    
    vec_pii res;
    DB_T& db = oson_this_db ;
    if ( ! db.isconnected() ) {
        slog.WarningLog("Database connection failed");
        return ;
    }
    //////////////////////////////////////////////////////////////////////////
    {
        //GET  active  and ON  and not PAY for current month.
        std::string query = " SELECT id,  uid FROM card_monitoring_cabinet  "
                            " WHERE ( status = 1 ) AND (monitoring_flag = 2 ) AND "
                            " ( date_part('month', add_ts ) <> date_part( 'month', NOW() ) ) "
                            "  ORDER BY  id  LIMIT 16  " ;

        DB_T::statement st( db ) ;
        st.prepare(query);
        int rws = st.rows_count() ;


        for(int i = 0; i < rws; ++i)
        {
            bigint id = 0 , uid ;
            st.row(i) >> id >>  uid ;

            res.push_back( pii( id, uid )  ) ;
        }
    }
    /////////////////////////////////////////////////////////////////
    if ( res.empty() ) 
    {
        return ;//if there no rows, no start_timer_db
    }
    
    for(pii p: res)
    {
        bigint id = p.first;
        /* uid = p.second */
        
        DB_T::statement st(oson_this_db);
        
        //2. turn-off current monitoring cabinet.
        std::string off_ts = formatted_time_iso( std::time( 0 ) - 2 * 60 * 60 ) ;// 2 hours before, for avoid very often change error.
        std::string query = "UPDATE card_monitoring_cabinet SET monitoring_flag = 1, off_ts = " + escape(off_ts ) + " WHERE id = " + escape(id);
        st.prepare(query);
    }
    
    start_timer_pay( 500 /*milliseconds*/, res  ) ;
}

void Card_monitoring_month_checker::do_pay( int wait_milliseconds,  vec_pii_type v)
{
    SCOPE_LOGD(slog);
    slog.InfoLog("v size: %zu " , v.size() ) ;
    
    if ( v.empty() )
    {
        //if there global time expires time less than 16 seconds, not start db-timer!
        if (m_timer_g.expires_from_now().total_seconds() < 16 )
        {   ;
        }
        else
        {
            //finished. so start read from Database
            start_timer_db( wait_milliseconds );
        }
        return ;
    }
    
    std::pair< int64_t, int64_t> last = v.back();
    int64_t uid = last.second ;//first is id of card_monitoring_cabinet.
    
    v.pop_back() ;
    
    global_card_monitoring_edit(uid, /*card_id,*/ MONITORING_ON, m_ios ) ;
    
    //wait 0.5 seconds and pay next item.
    start_timer_pay(wait_milliseconds, v ) ;
}


/*******************************************************************************************************************/
namespace 
{
    typedef std::shared_ptr< boost::asio::io_service > io_service_ptr ;
    
class FaultInformer_T 
{
public:
    typedef FaultInformer_T self_type;
    
    FaultInformer_T( const io_service_ptr & io_service, const std::string& admin_phones);
    ~FaultInformer_T();
    
    void async_start(int seconds);
    void on_timer(boost::system::error_code const& error);
    void do_job();
    
    void read_fault(Sort_T sort);
    
    int wakeup();
    
private:
    FaultInformer_T(const FaultInformer_T&); // = delete
    FaultInformer_T& operator = (const FaultInformer_T&); // = delete
private:
    io_service_ptr   m_ios;
    boost::asio::deadline_timer m_deadline_timer;
    std::string m_phones; // comma separated phones.
};

FaultInformer_T::FaultInformer_T( const io_service_ptr & io_service, const std::string& admin_phones)
: m_ios(io_service)
, m_deadline_timer( *io_service)
, m_phones(admin_phones)

{
    SCOPE_LOG(slog);
    slog.InfoLog("amdin-phones: %s", admin_phones.c_str());
    
    /** remove spaces and quotes around of phones */
    boost::trim_if(m_phones, [](char c){ return std::isspace(c) || c == '\'' || c == '\"'; } ); 
    
    slog.InfoLog("m_phones: %s", m_phones.c_str());
    
    async_start( 1 );
}

FaultInformer_T::~FaultInformer_T()
{
    SCOPE_LOG(slog);
}

void FaultInformer_T::async_start(int seconds){
    m_deadline_timer.expires_from_now( boost::posix_time::seconds( seconds ) ) ;
    m_deadline_timer.async_wait( std::bind(&self_type::on_timer, this, std::placeholders::_1));
}

void FaultInformer_T::on_timer(boost::system::error_code const& error){
    SCOPE_LOGD(slog);
    if (error){
        slog.ErrorLog("error: code = %d, msg = %s",error.value(), error.message().c_str());
        return ;
    }
    
    do_job();
    async_start(1800);
}

int FaultInformer_T::wakeup()
{
    async_start(0);//now.
    return 0;
}

void FaultInformer_T::do_job()
{
    SCOPE_LOG(slog);
    
    Sort_T sort(0, 20);

    m_ios ->post( ::std::bind(&self_type::read_fault, this, sort) ) ;
}

void FaultInformer_T::read_fault(Sort_T sort)
{
    SCOPE_LOG(slog);
    
    DB_T& db =  oson_this_db  ;
    if ( ! db.isconnected() )
        return ;

    BOOST_STATIC_ASSERT((FAULT_STATUS_ERROR == 2)) ;
    BOOST_STATIC_ASSERT((FAULT_TYPE_EOPC == 2)) ;
    
    std::string query = " SELECT id, type, ts::timestamp(0), status, description"
                        " FROM fault "
                        " WHERE type = 2 AND status = 2 AND (ts >= NOW() - INTERVAL '8 hour' ) AND   "
                        " ( (ts_notify IS NULL) OR ( ts_notify <= NOW() - INTERVAL '8 hour' ) )  " + sort.to_string();
             
    
    DB_T::statement st(db);
    st.prepare( query );//this may took 10-20 ms time
    
    int rows = st.rows_count() ;
    
    if ( ! rows )
        return ;
    
    std::string id_set = "0";
    
     
    for(int i = 0; i < rows; ++i)
    {
        Fault_info_T fault;
        
        st.row(i) >> fault.id >> fault.type >> fault.ts >> fault.status >> fault.description ;
        
        //needn't send every fault!!!
        //if (i < 4 )
        {
            std::string msg = "id = " + num2string(fault.id) + ", ts: " + fault.ts + ", " +  fault.description;
            SMS_info_T  sms_info ( m_phones, msg, SMS_info_T::type_fault_sms ) ;
            oson_sms->async_send(  sms_info  );
        }
           
        id_set += ',';
        id_set += num2string(fault.id);
    }
    
    std::string ts_notify = formatted_time_now_iso_S();
    st.prepare("UPDATE fault SET ts_notify  = " + escape(ts_notify) + " WHERE id IN ( " + id_set + " ) ") ;
    
    sort.offset += sort.limit;
    
    m_ios ->post( ::std::bind(&self_type::read_fault, this, sort) ) ;
}


} // end noname namespace


namespace {

struct Payment_info 
{
    typedef std::shared_ptr< Payment_info> pointer_type ;
    
    typedef std::function< void(pointer_type p, Error_T ec)> response_type;

    Periodic_bill_data_T    bill_data;
    //////////////////////////
    int64_t     uid         ;
    int64_t     amount      ;
    
    int64_t     card_id     ;
    int32_t     merchant_id ;
    std::string login       ;
    ///////////////////////////////////////////
    int64_t         commission    ;
    Merchant_info_T merchant      ;
    Card_info_T     card_info     ;
    User_info_T     user_info     ;
    Merch_trans_T   trans         ;
    Merch_acc_T     acc           ;     
    Purchase_info_T tr_info       ;
    
    response_type   rsp ;
    
    inline Payment_info() : uid(0), amount(0), card_id(0), merchant_id(0), login(), commission(0)
    {}
};

class periodic_purchase_session
{
public:
    typedef periodic_purchase_session self_type;
    
    typedef Payment_info  value_type;
 
    typedef std::shared_ptr< value_type > pointer_type;
    
    typedef boost::asio::io_service service_type;
    
    typedef std::shared_ptr< service_type> service_ptr;
    

    
    explicit periodic_purchase_session(const pointer_type& p_info, service_ptr & s) ;
    ~periodic_purchase_session();
    
    void async_start() ;
    
    void update_tr_info(int tr_status) const;
    void finish(Error_T ec) const;
    void start()const;
    Error_T init_info()const;
    Error_T check_login() const;
    void pay_merchant() const;
    
    void on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) const;
    void on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)const;  
    void on_trans_reverse_eopc(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) const;

    
    void on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) const;
    void on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)const;  
    void on_trans_reverse(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) const;

private:
     pointer_type    p ;
     service_ptr     s ;
};


periodic_purchase_session::periodic_purchase_session(const pointer_type& p_info, service_ptr & s)
: p(    p_info   ), s( s ) 
{
    SCOPE_LOGF_C(slog);
}

periodic_purchase_session::~periodic_purchase_session()
{
    SCOPE_LOGF_C(slog);
}

void periodic_purchase_session::async_start()
{
    SCOPE_LOG_C(slog);
    
    self_type self_copy(*this);
    s->post( std::bind( &self_type::start, self_copy )  ) ;
}

void periodic_purchase_session::update_tr_info(int tr_status)const
{
    SCOPE_LOG_C(slog);
    
    Purchase_T purch( oson_this_db );
    p->tr_info.status = tr_status;
    purch.update(p->tr_info.id, p->tr_info);

}
void periodic_purchase_session::finish(Error_T ec)const
{
    SCOPE_LOG_C(slog);
    if (   p->tr_info.id != 0 ) {
        int status = ( ec == Error_OK) ? TR_STATUS_SUCCESS : ( ec == Error_perform_in_progress) ? TR_STATUS_IN_PROGRESS : TR_STATUS_ERROR ;
        update_tr_info( status ) ;
    }

    if (static_cast<bool>( p->rsp ) ) {
        p->rsp( p, ec );
    }
}

void periodic_purchase_session::start()const
{
    SCOPE_LOGD_C(slog);

    //1. initialize all data .
    Error_T ec = init_info();
    if (ec) return finish( ec );


    //2. add purchase to database!
    DB_T& db = oson_this_db  ;

    Purchase_T purchase(db);
    p->tr_info.id = purchase.add( p->tr_info );
    
    p->trans.check_id = p->tr_info.id;
    //3. check login
    ec = check_login();
    if (ec) return finish( ec );

     ////////////////////////////////////////////////////////////////////////////////////
    //////  test card balance.
    self_type self_copy(*this);
    oson_eopc -> async_card_info( p->card_info.pc_token,  ::std::bind(&self_type::on_card_info_eopc, self_copy, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;
}

Error_T periodic_purchase_session::init_info()const
{
    SCOPE_LOG_C(slog);

    Error_T ec = Error_OK;
    DB_T & db = oson_this_db  ; //get current thread db.
    //1. user login needn't

    // 5. merchant info
    Merchant_T merch_table(db);
    p->merchant = merch_table.get(p->merchant_id, ec);

    if( ec ) return ec; // merchant-id invalid, or inactive.

    ec = p->merchant.is_valid_amount(p->amount) ;

    if ( ec  )
    { 
        if ( p->uid == 7 ) 
            ; 
        else return ec; 
    }


    p->commission = p->merchant.commission( p->amount );
    
    //for webmoney pay amount - commission to webmoney :)
    if(  p->merchant.commission_subtracted() )  //merchant_identifiers::commission_subtracted( p->merchant_id )   )
    {
        p->amount = p->amount - p->commission;
    }

    // Get card info
    Cards_T card_table( db );
    Card_info_T card_search;

    if(p->card_id != 0) {
        card_search.id = p->card_id;
        card_search.is_primary = PRIMARY_UNDEF;
    }
    else {
        card_search.is_primary = PRIMARY_YES;
    }
    card_search.uid = p->uid;
    ec = card_table.info(card_search, p->card_info);
    if( ec )return ec;


    if ( p->card_info.foreign_card == FOREIGN_YES ) {
        slog.WarningLog("Foreign card!");
        return Error_card_foreign;
    }

    if (p->card_info.user_block || p->card_info.admin_block ) {
        slog.WarningLog("Blocked card!");
        return Error_card_blocked ;
    }
    
    Users_T users( db );
    p->user_info = users.get(p->uid, ec);

    if(ec)return ec;

    if ( p->user_info.blocked ) {
        slog.WarningLog("An user is blocked!");
        return Error_operation_not_allowed;
    }
    // Check login in merchant

    p->trans.amount  = p->amount;
    p->trans.param   = p->login;

    //Merch_acc_T acc;
    merch_table.acc_info( p->merchant.id, p->acc);
    if( p->merchant.api_id == merchant_api_id::ums ||
        p->merchant.api_id == merchant_api_id::paynet)
    {
        merch_table.api_info(p->merchant.api_id, p->acc);
    }
        // Register request
    Purchase_info_T& tr_info = p->tr_info;
    tr_info.amount      = p->amount;
    tr_info.mID         = p->merchant_id;
    tr_info.uid         = p->uid;
    tr_info.login       = p->login;
    tr_info.eopc_trn_id = "0";
    tr_info.pan         = p->card_info.pan ;
    tr_info.ts          = formatted_time_now("%Y-%m-%d %H:%M:%S");
    tr_info.status      = TR_STATUS_REGISTRED;
    tr_info.commission  = p->commission;
    tr_info.card_id     = p->card_info.id; 

    return Error_OK ;
}


Error_T periodic_purchase_session::check_login()const
{
    SCOPE_LOG_C(slog);
    Merchant_api_T merch_api( p->merchant, p->acc);

    
    
    Merchant_T merch_table( oson_this_db ) ;
    p->trans.user_phone     = p->user_info.phone ;
    p->trans.transaction_id = merch_table.next_transaction_id();
    
    /**** IF this is PAYNET , and there is service-id-check, set random phone.*/
    if (p->merchant.api_id == merchant_api_id::paynet && p->trans.service_id_check > 0)
    {
        p->trans.user_phone = oson::random_user_phone();
    }
    
    Merch_check_status_T check_status;
    Error_T ec = merch_api.check_status( p->trans, check_status);

    if (ec) {
        p->tr_info.merch_rsp = "Check login FAILED with '" + to_str(ec) + "'  error code. " ;
        return ec; 
    }
    if(!check_status.exist){ 
        p->tr_info.merch_rsp = "Check: login not found!" ;
        return Error_transaction_not_allowed;
    }

    return Error_OK ;
}

void periodic_purchase_session::on_card_info_eopc(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec) const
{
    self_type self_copy(*this);
    s->post( std::bind(&self_type::on_card_info, self_copy, id, eocp_card, ec)) ;
}

void periodic_purchase_session::on_trans_pay_eopc(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)const
{
    self_type self_copy(*this);
    s->post( std::bind(&self_type::on_trans_pay, self_copy, debin, tran, ec)  ) ;
}

void periodic_purchase_session::on_trans_reverse_eopc(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) const
{
    self_type self_copy(*this);
    s->post( std::bind(&self_type::on_trans_reverse, self_copy, tranId, tran, ec)) ;
}


void periodic_purchase_session::on_card_info(const std::string& id, const oson::backend::eopc::resp::card& eocp_card, Error_T ec)const
{
    SCOPE_LOGD_C(slog);
    ///////////////////////////////////////////////////////////////////////////////////
    if (ec) {
        p->tr_info.merch_rsp = "EOPC card-info FAILED with '" + to_str(ec) + " error code!" ;
        return finish( ec );
    }

    if ( eocp_card.phone.empty())
    {
        //I'm not sure what we MUST do there!
        slog.WarningLog("Card_info_EOCP no phone determined!!!!!");
    }
    else if ( p->card_info.owner_phone.empty() ) {
        p->card_info.owner_phone = eocp_card.phone;
        
        Cards_T card_table(  oson_this_db  ) ;
        card_table.card_edit_owner( p->card_info.id, eocp_card.phone);
    } 
    else  if ( eocp_card.phone == p->card_info.owner_phone)
    {
        ;//do nothing!
        slog.DebugLog("OK: card owner phones identical!");
    } else { // there eocp.phone != owner_phone
        slog.ErrorLog("card-owner-phone: %s,  eopc-card-phone: %s", p->card_info.owner_phone.c_str(), eocp_card.phone.c_str());
        p->tr_info.merch_rsp = "Card owner changed" ;
        return finish( Error_card_owner_changed);
    }

    if (eocp_card.status != VALID_CARD) {
        slog.WarningLog("Pay from blocked card");
        p->tr_info.merch_rsp = "EOPC Card invalid status: " + to_str(eocp_card.status) ;
        return finish( Error_card_blocked);
    }

    if(eocp_card.balance < p->amount + p->commission) {
        slog.ErrorLog("Not enough amount");
        p->tr_info.merch_rsp = "Not enough amount" ;
        return finish( Error_not_enough_amount ) ;
    }

    ec = check_card_daily_limit( p->user_info, p->card_info, p->amount  + p->commission );
    if (ec) { 
        p->tr_info.merch_rsp = "limit exceeded daily foreign card amount." ; 
        return finish( ec );
    }

    /////////////////////////////////////////////////////////////////////////////////////
    //                        PERFORM TRANSACTION TO MERCHANT
    //////////////////////////////////////////////////////////////////////////////////////
    int64_t trn_id = p->tr_info.id ;
    // Perform transaction to EOPC
    EOPC_Debit_T debin;
    debin.amount     = p->amount + p->commission;
    debin.cardId     = p->card_info.pc_token;
    debin.ext        = num2string(trn_id);
    debin.merchantId = p->merchant.merchantId;
    debin.port       = p->merchant.port;
    debin.terminalId = p->merchant.terminalId;
    debin.stan       = make_stan(trn_id);
    //EOPC_Tran_T tran;

    self_type self_copy(*this);
    oson_eopc -> async_trans_pay( debin, std::bind(&self_type::on_trans_pay_eopc, self_copy, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) ) ;
}

void periodic_purchase_session::on_trans_pay(const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)const
{
    SCOPE_LOGD_C(slog);
    
    DB_T& db = oson_this_db ;
    
    if(ec != Error_OK) {
        Fault_T fault(db);
        std::string msg = "Can't perform transaction in EOPC. purchase-id: " + to_str(p->tr_info.id) +". \nerror-code: " + to_str(ec) + ", error-msg: "+ oson::error_str(ec) + ".";
        Fault_info_T finfo{  FAULT_TYPE_EOPC, FAULT_STATUS_ERROR,   msg } ;
        fault.add( finfo  );
        
        p->tr_info.merch_rsp = "Can't perform transaction in EOPC" ;
        return finish( ec );
    }
    if( ! tran.status_ok() ) {
        slog.WarningLog("Status of transaction invalid");
        Fault_T fault(db);
        fault.add(  Fault_info_T( FAULT_TYPE_EOPC,  FAULT_STATUS_ERROR , "Wrong status for transaction. purchase-id: " + to_str(p->tr_info.id) ));
        
        p->tr_info.merch_rsp = "EOPC: Wrong status for transaction: " + tran.status ;
        return finish( Error_internal ) ;
    }

    if ( tran.resp != 0 )
    {
        slog.ErrorLog("resp invalid ( not zero)");
        p->tr_info.merch_rsp = "EOPC resp invalid: " + to_str(tran.resp);
        return finish( Error_card_blocked ) ;
    }

    p->tr_info.eopc_trn_id = tran.refNum;
    p->tr_info.pan         = tran.pan;
    
    p->trans.merch_api_params["oson_eopc_ref_num"] = tran.refNum ;
    
    update_tr_info(TR_STATUS_SUCCESS) ;
   
    return pay_merchant();
}
void periodic_purchase_session::on_trans_reverse(const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec) const
{
    SCOPE_LOGD_C(slog);
    if (ec){
        slog.ErrorLog("error code: %d", (int)ec);
    }
}

void periodic_purchase_session::pay_merchant()const
{
    SCOPE_LOGD_C(slog);

    if ( p->merchant.url.empty() ) {
        return finish( Error_OK );
    }

    p->trans.ts             =  p->tr_info.ts ;
    p->trans.transaction_id =  p->tr_info.id; 
    p->trans.service_id     =   p->merchant.extern_service  ;
    p->trans.user_phone      =  p->user_info.phone;

    if (p->merchant.api_id == merchant_api_id::paynet )
    {
        p->trans.user_phone = oson::random_user_phone();
    }
    
    Error_T ec;
   // For paynet get its transaction id from database paynet counter
    {
        Merchant_T merch( oson_this_db  ) ;
        p->trans.transaction_id = merch.next_transaction_id( );
        p->tr_info.oson_paynet_tr_id = p->trans.transaction_id;
    }

    Merch_trans_status_T trans_status;
    Merchant_api_T merch_api(p->merchant, p->acc);
    ec = merch_api.perform_purchase ( p->trans, trans_status);

    p->tr_info.paynet_tr_id =  trans_status.merchant_trn_id ;
    p->tr_info.merch_rsp    =  trans_status.merch_rsp;
    p->tr_info.paynet_status = trans_status.merchant_status ;

    if (ec == Error_perform_in_progress)
    {
       // ec= Error_OK ;
    } else if(ec != Error_OK)  {
        //reverse from EOCP.
        self_type self_copy(*this);
        oson_eopc -> async_trans_reverse( p->tr_info.eopc_trn_id,  std::bind(&self_type::on_trans_reverse_eopc, self_copy, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) ) ;

        Fault_T fault( oson_this_db );
        fault.add(  Fault_info_T( FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p->tr_info.id) ));


        return finish( ec );
    }

    //asynchronously call ss_bonus_earns
    s->post( std::bind(&ss_bonus_earns, p->tr_info  ) )  ;
   
    return finish( ec );
}



} // end noname namespace

class UMS_sverka_session_T
{
public:
    typedef UMS_sverka_session_T self_type;
    typedef boost::asio::io_service service_type;
    typedef std::shared_ptr< service_type > service_ptr;
    typedef boost::asio::deadline_timer timer_type;
    
    explicit UMS_sverka_session_T( service_ptr io_service ) ;
    ~UMS_sverka_session_T();
    
    
public:
    void async_start(   ) ;
    
    void start(boost::system::error_code ec );
    
    void sverka_start(const char* date);
    
    void sverka_result(std::string date, std::string reestr_id);
    
    void send_email(std::string message);

private:
    service_ptr s_;
    timer_type  t_;
    timer_type  t_2;
};

UMS_sverka_session_T::UMS_sverka_session_T( service_ptr io_service )
 : s_(io_service)
 , t_(*io_service )
 , t_2(*io_service ) 
 {
    SCOPE_LOG(slog);
    async_start();
 }

UMS_sverka_session_T::~UMS_sverka_session_T()
{
    SCOPE_LOG(slog);
}

void UMS_sverka_session_T::async_start()
{
    SCOPE_LOG(slog);
    
    boost::gregorian::date current_date(boost::gregorian::day_clock::local_day());
    boost::posix_time::ptime time (current_date, boost::posix_time::ptime::time_duration_type(5,0,0));// 5:00:00
    
    if (boost::posix_time::second_clock::local_time() >= time ){
        current_date += boost::gregorian::days(1);
        time = boost::posix_time::ptime(current_date, boost::posix_time::ptime::time_duration_type(5,0,0));
    }
    
    auto diff = time - boost::posix_time::second_clock::local_time();
    
    t_2.expires_from_now( diff ) ;
   
    
    t_2.async_wait(std::bind(&self_type::start, this, std::placeholders::_1 ) ) ;
}

void UMS_sverka_session_T::start(boost::system::error_code ec)
{
   SCOPE_LOGD(slog); 
   
   if (ec){
       if (ec == boost::asio::error::operation_aborted)
           return;
   }
 
   //start again
   async_start();
   
   
   std::string date = formatted_time("%Y-%m-%d", std::time(0) - 24*60*60);//1 day before
    
   sverka_start(date.c_str());
}

void UMS_sverka_session_T::sverka_start(const char*date_s_str)
{
    SCOPE_LOGD(slog);
    std::string reestr_id, date;
    date = date_s_str ;
    
    Merchant_info_T merch_info;
    Merch_acc_T acc;
    Merchant_T merch_table( oson_this_db ) ;
    
    Error_T ec  = Error_OK ;
    
    const int32_t merchant_id = merchant_identifiers::UMS_direct;
    
    merch_info = merch_table.get( merchant_id, ec);
    
    if ( ec ) return  ;
    
    
    merch_table.api_info( (int32_t)merchant_api_id::ums, acc ) ;
    
    Merchant_api_T merch_api(merch_info, acc);
    
    Merch_trans_T trans;
    trans.transaction_id = merch_table.next_transaction_id();
    trans.param   = date;
    Merch_trans_status_T response;
    ec = merch_api.sverka_ums(trans, response);
    if (ec) {

        std::string token = "Ft5LtRhD76_oson_8IKFyLgcSj" ; // a public token
        
        std::string message = "После исправление зайдите ссылку: \nhttps://core.oson.uz:9443/api/purchase/ums_sverka_start?token="+token+"&date="+date + "  \n\n " ;
        message = "UMS сверка(реестр) неудачный. \n" + response.merch_rsp + "\n\n" + message;
        send_email(message);
        return;
    }
   
    reestr_id = response.merchant_trn_id ;
    
    
    t_.expires_from_now(boost::posix_time::hours(1));
    t_.async_wait(std::bind(&self_type::sverka_result, this, date, reestr_id ));
}

void UMS_sverka_session_T::sverka_result( std::string date,  std::string reestr_id )
{
    SCOPE_LOGD(slog);
    
    Merchant_info_T merch_info;
    Merch_acc_T acc;
    Merchant_T merch_table(oson_this_db);
    
    Error_T ec  = Error_OK ;
    
    const int32_t merchant_id = merchant_identifiers::UMS_direct;
    
    merch_info = merch_table.get( merchant_id, ec);
    
    if (ec) return  ;
    
    
    //(void) merch_table.acc_info( merchant_id, acc);
    merch_table.api_info((int32_t)merchant_api_id::ums, acc );
    
    Merchant_api_T merch_api(merch_info, acc);
    
    Merch_trans_T trans;
    trans.param   = reestr_id;
    Merch_trans_status_T response;
    ec = merch_api.sverka_ums_result(trans, response);
    
    if(ec) 
    { 
        std::string token = "Ft5LtRhD76_oson_8IKFyLgcSj" ; // a public token
        
        std::string message = "После исправление зайдите ссылку: \nhttps://core.oson.uz:9443/api/purchase/ums_sverka_start?token="+token+"&date="+date + "  \n\n " ;
        message = "UMS сверка (получит результат) неудачный. \n" + response.merch_rsp + "\n" + message;
        send_email(message);
        return;
    }
    
    if ( string2num( response.merchant_trn_id ) ==  2  )// yest rasxojdeniye
    {
        std::string rw = response.kv_raw ;
        
        if (oson::utils::is_base64(rw))
            rw =  oson::utils::decodebase64(rw);
        
        std::string token = "Ft5LtRhD76_oson_8IKFyLgcSj" ; // a public token
        std::string message = "После исправление расхождение зайдите ссылку: \nhttps://core.oson.uz:9443/api/purchase/ums_sverka_start?token="+token+"&date="+date + "  \n\n " ;
        message += rw;
        
        send_email(message); 
    } else {
        std::string token = "Ft5LtRhD76_oson_8IKFyLgcSj" ; // a public token
        std::string message = "После исправление расхождение зайдите ссылку: \nhttps://core.oson.uz:9443/api/purchase/ums_sverka_start?token="+token+"&date="+date + "  \n\n " ;

        slog.InfoLog("email will be sent: %s  message\n", message.c_str());
    }
   
}

void UMS_sverka_session_T::send_email(std::string message)
{
    SCOPE_LOGD(slog);
    std::string email_from = "kh.normuradov@oson.uz";
    std::string email_to   = "security@oson.uz;fayzulla.isxakov@gmail.com";
    std::string subject    = "UMS sverka yest rasxojdeniye " + formatted_time_now("%Y-%m-%d");
    
    oson::utils::send_email(email_to, email_from, subject, message);
}


/**************************************************************/

static long long time_pattern_diff_from_now(const std::string& pattern);

class PeriodicOperation_T
{
public:
    
    typedef PeriodicOperation_T self_type;
    
    typedef boost::asio::io_service service_type;
    typedef std::shared_ptr< service_type > service_ptr;
    typedef boost::asio::deadline_timer timer_type;
    
    explicit PeriodicOperation_T(service_ptr & io_service) ;
    
private:    
    void async_start(int seconds) ;
    
    bool can_bill(const Periodic_bill_data_T& bill_data)const;
    
    inline bool cannot_bill(const Periodic_bill_data_T& bill_data)const{ return ! can_bill(bill_data); }
    
    bool  handle_bill( Periodic_bill_data_T bill_data) ;
    
    void on_timer(const boost::system::error_code& e) ;
    
    bool read_bills(Sort_T ) ;
    void on_purchase_finished(  Payment_info::pointer_type p, Error_T ec );
private:
    timer_type  t_  ;
    service_ptr s_ ;
};


PeriodicOperation_T::PeriodicOperation_T(service_ptr & io_service)
: t_( *io_service)
, s_( io_service )

{
    async_start(2);
}

void PeriodicOperation_T::async_start(int seconds)
{
    t_.expires_from_now( boost::posix_time::seconds( seconds ) );
    t_.async_wait(std::bind(&self_type::on_timer, this, std::placeholders::_1));
}

void PeriodicOperation_T::on_timer(const boost::system::error_code& ec)
{
    SCOPE_LOGD(slog);
    if ( ec ){
        slog.ErrorLog("Error on handle: %s, code %d", ec.message().c_str(), ec.value());
        return ;//if there some error,
    }
    
    read_bills( Sort_T( 0, 20 ) ) ;
    
    int next_time =   15 * 60 ; // 15 minutes

    return async_start( next_time ); // next a minute;
}

bool PeriodicOperation_T::read_bills( Sort_T sort ) 
{
    SCOPE_LOG(slog);
     
    DB_T& db =  oson_this_db ;
    
    if ( ! db.isconnected()) {
        return false ;
    }
     
    Periodic_bill_list_T list;

    Periodic_bill_T bill(db);

    bill.list_need_to_bill( sort, list );

    if ( list.list.empty() ) {
        return false;
    }
    
    std::vector< Periodic_bill_data_T > bills_c;
    
    bills_c.swap ( list.list ) ; // we don't use list.list more!
    
    //skip  ! can_bill's.
    bills_c.erase( ::std::remove_if( bills_c.begin(), bills_c.end(),   std::bind( &self_type::cannot_bill, this, std::placeholders::_1 ) ), bills_c.end() ) ;
    
    
    std::for_each(bills_c.begin(), bills_c.end(), std::bind(&self_type::handle_bill, this, std::placeholders::_1 ) )    ;

    //@Note This is not optimal way, if there a huge number of bills !!! 
    sort.offset += sort.limit;
    s_->post ( std::bind(&self_type::read_bills, this, sort ) ) ;
    
    return true;
}

bool  PeriodicOperation_T::handle_bill(  Periodic_bill_data_T  bill_data)
{
    SCOPE_LOG(slog);
    
    Payment_info::pointer_type p = std::make_shared< Payment_info>();
    p->bill_data    = bill_data;
    p->uid          = bill_data.uid;
    p->amount       = bill_data.amount;  
    p->merchant_id  = bill_data.merchant_id;
    p->login        = bill_data.get_login();
    p->card_id      = bill_data.card_id ;

    p->rsp          = std::bind(&self_type::on_purchase_finished, this, std::placeholders::_1, std::placeholders::_2 ) ;

    periodic_purchase_session session( p, s_ ) ;
    session.async_start() ;
         
    return true;
}

bool PeriodicOperation_T::can_bill(const Periodic_bill_data_T& bill_data)const
{
    SCOPE_LOG(slog);

    const uint32_t status = bill_data.status;

    if ( static_cast<bool>(status & PBILL_STATUS_PAUSE) || static_cast<bool>(status & PBILL_STATUS_ERROR))
    {
        slog.WarningLog("stop or error status");
        return false;
    }

    long long diff_ts = time_pattern_diff_from_now(bill_data.periodic_ts);

    slog.DebugLog("periodic_ts: '%s' diff: %lld\n", bill_data.periodic_ts.c_str(), diff_ts);
    if (diff_ts >= LONG_LONG_MAX / 2 ){ // error
        slog.WarningLog("pattern error");
        return false;
    }

    

    Periodic_bill_T bill( oson_this_db );

    // diff_ts = periodic_ts - current_ts  ==> next_day  ==>  diff_ts - 24 * 60 * 60
    const long long  diff_next_day = llabs( diff_ts - 24 * 60 * 60 );
    static const long long  ten_minutes = 10 * 60;

    if ( diff_next_day  < ten_minutes  ){ // within 10 minutes

        bill.update_last_notify_ts(bill_data.id);
        std::string msg = "Автоплатёж " + bill_data.name + " на сумму " + num2string(bill_data.amount) + " будет произведён завтра";
        Users_notify_T users_n( oson_this_db );
        users_n.notification_send(bill_data.uid, msg, MSG_TYPE_PERIODIC_BILL_MESSAGE);
        return false;
    }

    if ( diff_ts   > 60  ) // a minute early.
    {
        slog.WarningLog("Too early ts : '%s' ", bill_data.periodic_ts.c_str());
        return false;
    }

    return true;
}

void PeriodicOperation_T::on_purchase_finished(  Payment_info::pointer_type p, Error_T ec )
{
    SCOPE_LOG(slog);
    
    async_start( 15 * 60 ) ; //move start time to next 15-th minute.
    
    if ( ! p ){
        slog.WarningLog("no payment-info set!!!");
        return ;
    }
    slog.DebugLog("payment end. trn_id: %lld", static_cast< long long > (p->tr_info.id) ) ;
    
    
    
    Periodic_bill_T bill( oson_this_db );
    
    const bool once_time =    p->bill_data.periodic_ts.find('*') == std::string::npos;//this is   once time billing.

    
    Users_notify_T users_n( oson_this_db );

    if (ec != Error_OK) {

        p->bill_data.status |= PBILL_STATUS_ERROR;
        
        if ( once_time )
            p->bill_data.status |= PBILL_STATUS_PAUSE ;
        
        bill.edit( p->bill_data.id, p->bill_data);

        std::string msg = "Автоплатёж запланированный вами не удалось осуществить";
        users_n.notification_send( p->bill_data.uid, msg, MSG_TYPE_PERIODIC_BILL_MESSAGE);
        return ;
    }

    
    if (once_time) 
    {

        p->bill_data.status |= PBILL_STATUS_PAUSE;

        bill.edit( p->bill_data.id, p->bill_data);
    }
    
    {
        ec = bill.update_last_bill_ts( p->bill_data.id);
        if (ec != Error_OK){
            slog.WarningLog("Can't update last billing timestamp.");
        }
    }

    {
        std::string msg = "Запланированный вами автоплатёж успешно совершён.";
        users_n.notification_send( p->bill_data.uid, msg, MSG_TYPE_PERIODIC_BILL_MESSAGE);
    }

}

//
//// Y-M-D H:M:S
//// H,M,S - concrete digits
//// Y -  digit  or *  for any match
//// M -  digit  or *  for any match
//// D -  digit  or L - for last day of month, or * for any match

static long long time_pattern_diff_from_now(const std::string& pattern)
{
    std::time_t nw = std::time(0);
    
    struct tm current_ts = {};
    localtime_r(&nw, &current_ts); // this is heavy operation, need avoid it.
    
    struct tm tm_pt = {};
    
    typedef int tm::*  tm_int_pointer;
    
    tm_int_pointer pt_tm_pt[ 6 ] = { &tm::tm_year, &tm::tm_mon, &tm::tm_mday, &tm::tm_hour, &tm::tm_min, &tm::tm_sec };
    bool digit = false;
    for( size_t i = 0, k = 0; i < pattern.length() && k < 6; ++i)
    {
        int c = pattern[i];
        
        switch(c)
        {
            case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8' : case '9':
            {
                tm_pt.*pt_tm_pt[k]  = tm_pt.*pt_tm_pt[k] * 10 + (c - '0');
                digit = true;
                break;
            }
            case ' ': case ':' : case '-' : case 'T' :  //separators. 'T' also allowed.
            {
                if (digit) // previous is digit
                {
                    if (k == 0) // a year
                    {
                        if (tm_pt.tm_year > 1900)
                            tm_pt.tm_year -= 1900; // year [ actual year - 1900]
                    }
                    else if (k == 1) // a mon
                    {
                        tm_pt.tm_mon -= 1;// mon [0..11]
                    }
                }
                
                ++k;
                digit = false;
                break;
            }
            case '*':
            {
                tm_pt.*pt_tm_pt[ k ] = current_ts.*pt_tm_pt[ k ]; //copy this field from tm.
                digit = false;
                break;
            }
            case 'L':
            {
                static const int last_mon_day[ 2 ][ 12 ] = 
                { 
                    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
                    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
                };
                
                if (k != 2)//day
                    return LONG_LONG_MAX ;
                
                int y = (tm_pt.tm_year + 1900) ;
                int isLeap = ( ( y % 4 == 0 ) && ( ( y % 100 != 0 ) || ( y % 400 == 0 ) ) ) ? 1 : 0;
                
                tm_pt.tm_mday = last_mon_day[ isLeap ][ tm_pt.tm_mon ] ;
                
                digit = false;
                break;
            }
            default:
                return LONG_LONG_MAX ;
            
        }
    }
    
    const std::time_t ct = nw;//mktime( &current_ts);  
    const std::time_t pt = mktime( &tm_pt     );
    
    return (long long)pt - (long long)ct;
}


class Status_in_progress_checker
{
public:
    typedef Status_in_progress_checker self_type;
    enum{ tm_seconds = 30 } ;// every 30 seconds
    
    explicit Status_in_progress_checker(const std::shared_ptr< boost::asio::io_service > & io_service )
     : m_io_service(io_service)
     , m_timer(*io_service)
     , m_sort(0, 16, Order_T(1, 0, Order_T::ASC))
    {
        
        async_start(tm_seconds);
     
    }
    
    void async_start(int tm){
        m_timer.expires_from_now(boost::posix_time::seconds(tm)) ;
        m_timer.async_wait( std::bind( &self_type::handler, this, std::placeholders::_1 ) )  ;
    }
    
    void handler(boost::system::error_code ec )
    {
        if (ec)
        {
            SCOPE_LOGD(slog);
            slog.ErrorLog("ec: %d, msg: %s", ec.value(), ec.message().c_str());
            return ;
        }
        
        try{ do_job(); }catch(std::exception& e){}
        
        async_start(tm_seconds);
    }
    
    void do_job()
    {
        SCOPE_LOGD(slog);

        std::string query = 
            "  SELECT  id, uid, merchant_id, login, ts, amount, transaction_id, status,"
            "          paynet_tr_id,  paynet_status, oson_paynet_tr_id,  card_id,   merch_rsp "
            "   FROM purchases"
            "   WHERE (status = 6 ) AND (ts < Now() - interval '1 minute' ) AND  "
            "         (paynet_tr_id IS NOT NULL) AND ( length(paynet_tr_id) > 0) "
            + m_sort.to_string();

        
        DB_T::statement st( oson_this_db );

        st.prepare(   query   );  

        const size_t rows = st.rows_count();   
        
        for(size_t i= 0; i < rows; ++i){
            Purchase_info_T  info  ;
            st.row(i) >> info.id >> info.uid >> info.mID >> info.login >> info.ts >> info.amount >> info.eopc_trn_id  
                      >> info.status >> info.paynet_tr_id >> info.paynet_status >> info.oson_paynet_tr_id >>  info.card_id 
                      >> info.merch_rsp  ;

            m_io_service->post( std::bind(&self_type::check_status, this, info )  ) ;
        }
        
        if (rows == 0)
        {
            m_sort.offset = 0 ;
        }
        else
        {
            m_sort.offset += 16 ;
        }
    }
    
    
    //this works another thread.
    Error_T check_status_handle_test(Purchase_info_T p_info, const Merch_trans_T& trans, const Merch_trans_status_T& trans_status, Error_T ec)
    {
        SCOPE_LOGD(slog);
        m_io_service->post( std::bind(&self_type::check_status_handle, this, p_info, trans, trans_status, ec ) );
       
        return Error_OK ;
    }
    
    Error_T check_status_handle(Purchase_info_T p_info, const Merch_trans_T& trans, const Merch_trans_status_T& trans_status, Error_T ec)
    {
        SCOPE_LOGD(slog);
        
        if ( ec == Error_perform_in_progress )
        {
            return Error_perform_in_progress;
        }
        
        if (ec == Error_HTTP_host_not_found)
        {
            return Error_HTTP_host_not_found;//retry it, against.
        }
        
        //OK, now this purchase finished.
        p_info.merch_rsp     += ", " + trans_status.merch_rsp;
        p_info.paynet_status = trans_status.merchant_status;
        p_info.status        = ( Error_OK == ec ) ? TR_STATUS_SUCCESS : TR_STATUS_ERROR ;
        
        //UPDATE IT.
        std::string query = " UPDATE purchases SET  "
                            " merch_rsp     = " + escape(p_info.merch_rsp)     + ",  "
                            " paynet_status = " + escape(p_info.paynet_status) + ",  "
                            " status        = " + escape(p_info.status)        + "   "
                            " WHERE id      = " + escape(p_info.id)     ;
        
        DB_T::statement st( oson_this_db  );
        st.prepare(query);

        if (ec != Error_OK)
        {
            //reverse it.            
            SMS_info_T sms;
            sms.phone = oson_opts -> admin.phones ;
            sms.text = "WARN:  txn_id = " + to_str( p_info.id ) + " FAILED!! DO YOU WANT REVERSE CARD AMOUNT???" ;
            sms.type = sms.type_mplat_status_fail_sms ;
            
            oson_sms->async_send(sms) ;
            
            Fault_T fault( oson_this_db );
            fault.add( Fault_info_T (FAULT_TYPE_MERCHANT, FAULT_STATUS_ERROR, "Can't perform transaction. purchase-id: " + to_str(p_info.id) ) );

            return Error_OK;
        }

         //1. bonus card
        bool const bonus_card = is_bonus_card( oson_this_db , p_info.card_id ) ;
       
        //success
        if ( ! bonus_card )
        {
            ss_bonus_earns(p_info );
        }
        
        return Error_OK ;
    }
    
    Error_T check_status(Purchase_info_T p_info)
    {
        SCOPE_LOGD(slog);
        Error_T ec;
        const int32_t merchant_id = p_info.mID;
        //const int64_t card_id     = p_info.card_id;
        
        
        //bool                  bonus_card   ;
        Merchant_info_T       merchant     ;
        Merch_acc_T           acc          ;
        Merch_trans_T         trans        ;
        //Merch_trans_status_T  trans_status ;
        
        
        if (p_info.paynet_tr_id.empty())
        {
            slog.WarningLog("Paynet trn-id empty!");
            return Error_OK;
        }
        DB_T& db = oson_this_db   ;

        if ( ! db.isconnected() )
        {
            return Error_DB_connection ;
        }
        
        //2. merchant
        Merchant_T merchant_table(db);
        merchant = merchant_table.get(merchant_id, ec);
        if (ec) return ec;
        
        
        const bool possible_status_merchants = merchant.api_id ==  merchant_api_id::mplat         || 
                                               merchant.api_id == merchant_api_id::qiwi           ||
                                               merchant.api_id ==  merchant_api_id::hermes_garant ||
                                                false ;
        
        if ( ! possible_status_merchants ) {
            slog.WarningLog("This is not mplat merchant nor qiwi. merchant-id: %d", merchant_id);
            return Error_OK ; //needn't test not mplat merchants
        }
        
        
        
        //3. acc
        merchant_table.acc_info(merchant_id, acc);
        if(merchant.api_id == merchant_api_id::ums){
            merchant_table.api_info(merchant.api_id, acc);
        }    
        //3. trans
        trans.param           = p_info.login              ;
        trans.uid             = p_info.uid                ;
        trans.transaction_id  = p_info.oson_paynet_tr_id  ;
        trans.amount          = p_info.amount             ;
        trans.ts              = p_info.ts;
        //trans.service_id = ??
        //trans.service_check_id == ?? 
        
        
        //4. trans_status
        //trans_status.merchant_trn_id = p_info.paynet_tr_id ;
        //trans_status.merch_rsp       = p_info.merch_rsp    ;
        
        
        //5. check
       // Merchant_api_T merch_api(merchant, acc);
        
       // typedef std::function< void(const Merch_trans_T&, const Merch_trans_status_T&, Error_T) > handle_type;
       // handle_type handle = std::bind(&self_type::check_status_handle, this, p_info, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3) ;
        
       // merch_api.async_mplat_perform_status(trans, trans_status, m_io_service, handle);

        //5. check oson::Merchant_api_manager
        {
            auto merch_api = oson_merchant_api ;
            trans.merchant = merchant;
            trans.acc      = acc;
            
            if ( merchant.api_id == merchant_api_id::mplat ) {
                trans.merch_api_params["txn"] = p_info.paynet_tr_id ;
            } else if (  merchant.api_id == merchant_api_id::qiwi ) {
                trans.merch_api_params["account"] = trans.param ;
            } else if ( merchant.api_id == merchant_api_id::hermes_garant ){
                trans.merch_api_params["account"] = trans.param;
                trans.service_id = merchant.extern_service;
                
            }
            
            auto handle = std::bind(&self_type::check_status_handle_test, this, p_info, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
            
            merch_api->async_perform_status(trans, handle ) ;
        }
        
        return Error_OK ;
    }
    
private:
    std::shared_ptr< boost::asio::io_service > m_io_service;
    boost::asio::deadline_timer m_timer ;
    Sort_T m_sort;
};


/********************************************************************************************/


struct oson::application::impl
{
    int argc;
    char** argv ;

    class SMS_manager  * sms_manager  ;
    class PUSH_manager * push_manager ;
    class XMPP_manager * xmpp_manager ;
    class EOPC_manager * eopc_manager ;
    class Merchant_api_manager * merchant_api;
    struct runtime_options_t * runtime_options;
}; 

std::function< void(const char*)> g_ums_sverka;

void oson::application::start_ums_sverka(const char* date)
{
    if (g_ums_sverka)
        g_ums_sverka(date);
}

//Starting Application of osond
int oson::application::run()
{

    /******** 1. Initialize command line options **********************/
    startup_options_t start_opts;
    init_startup_options(p->argc, p->argv, start_opts) ;

    if ( start_opts.help  ) {
        return print_osond_help();
	}
    /******* 2. read and parse config file ****************************/
    const std::string& config_file =  start_opts.config ; 

    //1. FIRST p field.
    p->runtime_options = new runtime_options_t;
    
	runtime_options_t & run_opts = *(p->runtime_options);
	if( ! parse_ini_config(config_file, run_opts  ) ) {
		printf( "Config file initialize error\n" );
		return -1;
    }

    /***  3. run  help|checker|daemonize if such options have. ***************/
    const std::string& pid_filename = run_opts.main.pid_file; 
  
    
    /****  4. Initialize most common tools: signals, pid file, log, databases...****/
    g_pid_filename = pid_filename;
    
    srand(time(NULL));
    signal(SIGINT, sigintHandler);
    signal(SIGUSR1, sigusr1Handler);

    fprintf(stderr, "pid file: %s\n", pid_filename.c_str());
    
    if ( pidfile( pid_filename ).has_another_instance()   ){
        fprintf(stderr, "There already exists another instance of osond\n");
        return -1;
    }
    
    
    scoped_pidfile scoped_pidfile_e(pid_filename);
    
	// Init core
	const std::string& log_file  =  run_opts.main.log_file;  
	const LogLevel_T   log_level = static_cast<LogLevel_T>( run_opts.main.log_level  ) ;
    
    fprintf(stderr, "log_file: %s\tlog_level: %d\n", log_file.c_str(), (int)log_level);
    CLog::set_thread_name("main");
	CLog log;
	
    log.Initialize( log_file.c_str(), log_level );

    
	SCOPE_LOG( slog );
    
    //compare_runtime_opts(run_opts_test, run_opts ) ;
    
    if ( ! test_database( run_opts.db ) ) {
        fprintf(stderr, "ERROR: can't initialize database\n");
        return -1;
    }
    
    
    /*****  5. create service threads  *************************************************/
    typedef ::boost::asio::io_service  service_t;
    typedef ::std::shared_ptr< service_t > service_ptr;
    
    using ::std::make_shared ;
    
    
    
    service_ptr  ios_notify       =   make_shared< service_t > () ; // 1
    service_ptr  ios_eopc         =   make_shared< service_t > () ; // 2
    //service_ptr  ios_db           =   make_shared< service_t > () ; // 3
    service_ptr  ios_periodic     =   make_shared< service_t > () ; // 4
    service_ptr  ios_status_check =   make_shared< service_t > () ; // 5
  //  service_ptr  ios_fault        =   make_shared< service_t > () ; // 6
    service_ptr  ios_online_check =   make_shared< service_t > () ; // 7
    
    service_ptr  ios_client  = make_shared<service_t>(); // 8
    service_ptr   ios_admin   = make_shared<service_t>(); // 9
    service_ptr ios_main = make_shared< service_t > ( ); // 10
    
    service_ptr  ios_merchant_api =   make_shared< service_t > () ; // 11
    
    //Add SMS sender thread.
    //{
        //xmpp and sms will work together one thread!
        oson::sms_runtime_options_t sms_options;
        
        sms_options.url           = run_opts.sms.url;
        sms_options.url_v2        = run_opts.sms.url_v2;
        sms_options.auth_basic_v2 = run_opts.sms.auth_basic_v2;
        
        p->sms_manager  = new oson::SMS_manager(ios_notify);
        p->sms_manager->set_runtime_options( sms_options );
        //////////////////////////////////////////////////
        xmpp_network_info xmpp;
        xmpp.address =  run_opts.xmpp.ip;   
        xmpp.port    =  run_opts.xmpp.port;   

        p->xmpp_manager = new oson::XMPP_manager(ios_notify, xmpp);
        //////////////////////////////////////////////////////
        ios_notify_cert_info ios_cert;
    
        ios_cert.certificate =  run_opts.ios_cert.certificate;  
        ios_cert.badge       =  run_opts.ios_cert.badge;        
        ios_cert.isSandbox   =  run_opts.ios_cert.sandbox;      
        
        if ( ! oson::utils::file_exists(ios_cert.certificate) ) 
        {
            fprintf(stderr, "ERROR: ios certificate file not found: '%s'\n", ios_cert.certificate.c_str());
            return -1;
        }
        
        p->push_manager = new oson::PUSH_manager(ios_notify, ios_cert);
        ////////////////////////////////////////////////////////
        eopc_network_info eopc_net;
        eopc_net.address  =  run_opts.eopc.address  ;  
        eopc_net.authHash =  run_opts.eopc.authHash ;  

        p->eopc_manager = new oson::EOPC_manager(ios_eopc, eopc_net);
        /////////////////////////////////////////////////////////////////////////
        p->merchant_api = new oson::Merchant_api_manager( ios_merchant_api ) ;
        ////////////////////////////////////////////////////////////////////////
        
        
        Status_in_progress_checker   in_progress_checker ( ios_status_check ); 
        
        
        PeriodicOperation_T  periodic (ios_periodic);
        
        UMS_sverka_session_T ums_session(ios_periodic);
        
        g_ums_sverka = std::bind(&UMS_sverka_session_T::sverka_start, &ums_session, std::placeholders::_1);
        
//        FaultInformer_T faulter ( ios_main, run_opts.admin.phones ) ;
        
        //g_fault_informer = std::addressof(faulter);

        int timeout_by_seconds =  run_opts.client.online_timeout * 60 ; 
        StartOnline_checker_T  online_checker ( ios_online_check , timeout_by_seconds );

        Card_monitoring_month_checker card_monitor_checker( ios_online_check ) ;
        
        ClientApiManager_T* client_manager = new ClientApiManager_T(ios_client, run_opts.client ) ;
       
        AdminApiManager_T* admin_manager = new AdminApiManager_T(ios_admin, run_opts.admin ) ;
        
    //}
        
    typedef ::std::vector< ::std::thread > thread_group_t;
    thread_group_t threads;
    
    threads.emplace_back(    io_service_runner( ios_notify, "notify" )   );
    
    threads.emplace_back(    io_service_runner( ios_status_check, "s_check" )   );
    
    threads.emplace_back(    io_service_runner( ios_eopc, "eopc" )   ) ;
    
    
    usleep( 100000 ) ; // wait 10 ms, because io_service threads MUST first run.
    
	// Inti client interface
	threads.emplace_back ( io_service_runner(ios_client, "CLIENT") ) ; 
	
    threads.emplace_back(  io_service_runner(ios_admin, "ADMIN" )  );

    
//    threads.emplace_back(    io_service_runner( ios_fault, "fault" )   )  ;
//    
    threads.emplace_back(    io_service_runner( ios_periodic, "periodic" )   ) ;
    
    threads.emplace_back(    io_service_runner( ios_online_check, "o_check" )   ) ;

    threads.emplace_back( io_service_runner( ios_merchant_api, "merchant") ) ;

     
    http::server::runtime_option http_opt;
    http_opt.doc_root   = "/etc/oson/img";
    http_opt.port       = 8447;
    http_opt.ip         = run_opts.client.address;
    http_opt.cert_chain = run_opts.client.ssl_chain ;
    http_opt.dh_file    = run_opts.client.ssl_dh ;
    http_opt.password   = run_opts.client.ssl_pwd;
    http_opt.private_key_file = run_opts.client.ssl_key ;
    
    //auto http_io_server = std::make_shared< boost::asio::io_service > () ;
    
  //  http::server::server server (http_io_server, http_opt ) ;
    
//    threads.emplace_back( io_service_runner(http_io_server, "http(s)")  ) ;
    
    
    
    io_service_runner( ios_main , "main" ) ( );
    
    
    ///// WAIT threads //////////////////////
    ::std::for_each( ::std::begin(threads), ::std::end(threads), ::std::mem_fn(& ::std::thread::join ) ) ;  //threads.join_all();
    /////////////////////////////////////////////////////////////////////////////////////
    //                         6. FINISH RUN
    ///////////////////////////////////////////////////////////////////////////////////
//    g_fault_informer = nullptr;
//    delete in_progress_checker ;
    delete admin_manager;
    delete client_manager;
    delete p->sms_manager  ;  p->sms_manager  = NULL ;
    delete p->xmpp_manager ;  p->xmpp_manager = NULL ;
    delete p->push_manager ;  p->push_manager = NULL ;
    delete p->runtime_options; p->runtime_options = NULL ;
    
    log.Destroy();
    
    fprintf(stderr, "\n\n===============================\n\nend osond.cpp run()\n");
    fflush(stderr);
    
    return 0;
}
