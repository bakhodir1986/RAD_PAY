
#include <cstdio> // snprintf
#include <cstring>
#include <algorithm>
#include <cstring>

#include "log.h"
#include "types.h"
#include "DB_T.h"
#include "exception.h"

static Connect_info_T g_connect_info;

static std::string g_connect_info_str;

static std::string escape_text(const  std::string& text)
{
    const char quot = '\'';
    const char slash = '\\';
    const size_t n = text.size();
    std::string result;
    result.reserve(n + 2);
    
    result += quot;
    for(size_t i = 0; i != n ; ++i)
    {
        char c = text[i];
        if (c == '\0')
        {
            result += slash;
            result += slash;
            c = '0'; // \\0
        }

        if ( c == quot || c == slash)
            result += c ;
        result += c;
    }
    result += quot;
    
    return result;
}
int make_order_from_string(std::string const& order_str, int default_order ) 
{
    std::string const& o = order_str;
    
    if ( o == "1" ) return Order_T::ASC;
    
    if ( o == "0" ) return Order_T::DESC;
    
    if (o.length() == 3 && tolower(o[0]) == 'a' && tolower(o[1]) == 's' && tolower(o[2]) == 'c' ) 
        return Order_T::ASC;

    if (o.length() == 4 && tolower(o[0] == 'd' ) &&tolower(o[1] ) == 'e' && tolower(o[2] ) == 's' && tolower(o[3]) == 'c' )
        return Order_T::DESC;
    
    //by default ASC
    return default_order;
}
////////////////////////////////////////////////////////////////////////
//		External class
//////////////////////////////////////////////////////////////////////////
 std::string Connect_info_T::to_string()const{
     const char space = ' ';
     const size_t approximal_length = 4 * 8 + m_db_name.size() + m_user.size() + m_pass.size() + m_host.size();
     
     std::string res;
     res.reserve(approximal_length);
     
     res += "dbname=";
     res += m_db_name;
     res += space;
     res += "user=";
     res += m_user ;
     res += space;
     res += "password=";
     res += m_pass;
     
     if( ! m_host.empty() )
     {
         res += space;
         res += "host=";
         res += m_host;
     }
     return res;
 }
 
static PGconn* make_PGconnect( const std::string& connect_str, Error_T& ec)
{
    SCOPE_LOGD(slog);

    PGconn * conn = PQconnectdb(connect_str.c_str());
    ConnStatusType status = PQstatus( conn );
    if ( status == CONNECTION_BAD)
    {
        const char* err_msg =  PQerrorMessage( conn  );

        slog.ErrorLog( "DB connect failed: %s", err_msg  );

        PQfinish(conn);
        conn = NULL;
        
        ec = Error_DB_connection;
    }
    else
    {
        ec = Error_OK ;
    }
    
    return conn;
}

static PGresult * make_PGresult(PGconn * conn, const std::string& query, Error_T& ec )
{
    PGresult * res = PQexec(conn, query.c_str());
    ExecStatusType status = PQresultStatus(res);
    
    if ( status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK ) 
    {
        SCOPE_LOGD(slog);
        const char* err_msg =  PQerrorMessage( conn  );
        slog.ErrorLog( "\n%.*s command failed( status = %d):\n%s\n", ::std::min<int>(query.length(), 1024), query.c_str(),(int)status, err_msg );
        ec = Error_DB_exec ;
        
        if ( res != nullptr )
            PQclear( res );
        
        return nullptr;
    }
    
    //@check this situation, also.
    if (nullptr == res)
    {
        SCOPE_LOGD(slog);
        slog.ErrorLog("\n%.*s command exec return nullptr.\n", ::std::min<int>(query.length(), 1024), query.c_str());
        ec = Error_DB_exec;
        return nullptr;
    }
    
    return res;
}

static  PGresult* make_PGresult( PGconn* conn, const std::string & query)
{
    PGresult* res = PQexec(conn , query.c_str());
    ExecStatusType status = PQresultStatus(res) ;

    if (  status != PGRES_COMMAND_OK  && status != PGRES_TUPLES_OK)
    {
        SCOPE_LOGD(slog);
        std::string err_msg =  PQerrorMessage( conn  );
        slog.ErrorLog( "\n%.*s command failed( status = %d):\n%s\n", std::min( (int)query.length(), 1024), query.c_str(),(int)status, err_msg.c_str());
        throw db_exception(err_msg, (int)Error_DB_exec);
        //return oson::PGresult_ptr();
    }
    return res;//oson::PGresult_ptr(res, & ::PQclear);
}
  
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  
Sort_T::Sort_T(int offset /*= 0*/, int limit /*= 0*/, Order_T o /*= Order_T() */ )
 : offset(offset), limit(limit), order(o)
{}

std::string Sort_T::to_string()const
{
    char buf[ 128 ] = {};
    size_t sz;
    char* pf = buf;
    
    if ( order.field != 0 )
    {
        if (order.order == order.ASC)
        {
           sz =  snprintf(pf, buf + sizeof(buf) - pf , " ORDER BY %d  ASC ", order.field);
            pf += sz;
        } 
        else 
        {
            sz = snprintf(pf, buf + sizeof(buf) - pf , " ORDER BY %d DESC ", order.field);
            pf += sz;
        }
        
        if (order.field2 != 0 )
        {
            if (order.order == order.ASC)
            {
                sz = snprintf(pf, buf + sizeof(buf) - pf, ", %d ASC ", order.field2);
                pf += sz;
            }
            else
            {
                sz = snprintf(pf, buf + sizeof(buf) - pf, ", %d DESC ", order.field2);
                pf += sz;
            }
        }
    }
    
    if (offset != 0){
        sz = snprintf(pf, buf + sizeof(buf) - pf, " OFFSET %d ", offset);
        pf += sz;
    }
    
    if (limit != 0){
        sz = snprintf(pf, buf + sizeof(buf) - pf, " LIMIT %d ", limit ) ;
        pf += sz;
    }
    sz = pf - buf;
    
    return std::string(buf, sz);
} 
 
int Sort_T::total_count(int rows)const
{
    //offset = 0 limit = 25 rows = 25
    bool const counted = (  ( offset == 0)            && ( ! limit || rows < limit )  ) ||
                         (  ( offset >  0 && rows> 0) && ( ! limit || rows < limit )  ) ;
    
    return ( counted ) ? offset + rows : -1;
    
}
 
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
DB_T::DB_T( DB_tag )
    : m_conn(NULL)
{
	SCOPE_LOG( slog );
}
   
DB_T::~DB_T()
{
	SCOPE_LOG( slog );
    disconnect();
}

Connect_info_T DB_T::connectionInfo(){ return g_connect_info;}
void DB_T::initConnectionInfo(const Connect_info_T& connect_info)
{
    SCOPE_LOG(slog);
    g_connect_info = connect_info;
    g_connect_info_str = connect_info.to_string();
}
 
bool DB_T::isconnected()const
{
    return ( m_conn != NULL ) &&  ( PQstatus( m_conn) == CONNECTION_OK ) ;
}

int DB_T::connect( )
{
    SCOPE_LOG(slog);
    if ( m_conn != NULL ){
        PQfinish(m_conn);
        m_conn = NULL ;
    }
    Error_T  ec = Error_OK ;
    m_conn = make_PGconnect(g_connect_info_str,  /*out*/ ec);
    return ec;
}

void DB_T::disconnect()
{
    SCOPE_LOG(slog);
    if (m_conn != NULL){
        PQfinish(m_conn);
        m_conn = NULL ;
    }
}

std::string DB_T::escape( const std::string& view)
{
    return escape_text(view);
}

std::string DB_T::escape_d(double d)
{
    char buffer[ 64 ] = {};
    snprintf(buffer, 64, "%.12f", d);
    return buffer;
}

//escape a integer is simple: quote around the number.
std::string DB_T::escape(long long number)
{
    char buffer[ 32 ] = {};
    int i = 31; // buffer[31] = '\0';
    long long na = number < 0 ? - number : number;
    bool sign = number < 0 ;
    
    buffer[--i] = '\'';
    do buffer[--i] = char(na%10 + '0'); while(na/=10);
    if(sign)
        buffer[--i] = '-';
    buffer[--i] = '\'';
    
    return std::string((const char*)( buffer + i ), (size_t)(31 - i));
    
}

//no content check, guaranteed there no quot or slash in view, and view does not have '\0' .
std::string DB_T::escape( const std::string& view, int )
{
    std::string result;
    result.reserve(2 + view.size());
    result += '\'';
    result.append(view.c_str(), view.length());
    result += '\'';
    return result;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////
//           TRANSACTION    AND  STATEMENT classes
////////////////////////////////////////////////////////////////////////////////////////////////////

DB_T::transaction:: transaction(DB_T& ref)
   : conn_(ref.native_handle())
   , res_(NULL)
   , commit_( false )
{
    res_ = make_PGresult( conn_,  "BEGIN"  );
    PQclear(res_);
    res_ = NULL;
	commit_ = true;
}

void DB_T::transaction::commit()
{
    if(commit_)
    {
        res_ = make_PGresult( conn_,  "COMMIT"  );
        PQclear(res_);
        res_ = NULL ;
        commit_ = false;
    }
}

DB_T::transaction::~transaction()
{
    if(commit_)
    {
        res_ = make_PGresult(conn_,   "ROLLBACK" );
        PQclear(res_);
        res_ = NULL ;
        commit_ = false;
    }
}
 /////////////////////////////////////////////////////////////////////////////   
DB_T::statement::statement(DB_T& ref)
    : conn_( ref.native_handle() )
    , res_(NULL)
{
    cl.init(this, -1);
}

DB_T::statement:: ~statement()
{
    if (res_){
        PQclear(res_);
        res_ = NULL ;
    }
}

void DB_T::statement::prepare( const std::string& query, Error_T& ec ) 
{
    SCOPE_LOG(slog);
    int const len =  ::std::min<int>( query.length(), 2048);

    slog.InfoLog( "| %.*s |",  len, query.c_str() );

    if (res_){ // if this is another prepare.
        PQclear(res_);
        res_ = NULL ;
    }

    ec = Error_OK ;
    
    res_ = make_PGresult(conn_, query, ec );
    cl.init(this, 0);

    
    slog.InfoLog("rows: %d,  affected-rows: %d", rows_count(), affected_rows() );
}

void DB_T::statement::prepare( const std::string& query)
{ 
    SCOPE_LOG( slog );
    int const len =  ::std::min<int>( query.length(), 2048);

    slog.InfoLog( "| %.*s |",  len, query.c_str() );

    if (res_){ // if this is another prepare.
        PQclear(res_);
        res_ = NULL ;
    }

    res_ = make_PGresult(conn_, query );
    cl.init(this, 0);

    slog.InfoLog("rows: %d,  affected-rows: %d", rows_count(), affected_rows() );
}
 
 std::string DB_T::statement::get_str(int row, int column)const
 { 
     const char * v = PQgetvalue( res_, row, column );

     return std::string( ( (v != NULL) ? v : "") );
 }

 long long DB_T::statement::get_int(int row, int column)const
 { 
     const char * v = PQgetvalue( res_, row, column );
     
     if (v == NULL || v[0] == '\0' )return 0;
     
     long long result = 0;
     
     ::std::sscanf(v, "%lld", &result);
     
     return result;
 }
 
 bool DB_T::statement::get_bool(int row, int column)const
 {
     const char* v = PQgetvalue(res_, row, column);
     if (v == NULL || v[0] == '\0' ) return false;
     
     // 't'  before psql 9.5,   'true' since psql 9.5.
     if (v[0] == 't'  || v[0] == 'T' || v[0] == '1' ) 
         return true;
     else
         return false;
 }
 
 //@Note: I have been tested:  If m_res.get()==NULL, PQntuples will return 0.
 int DB_T::statement::rows_count()const
 { 
     int r =  PQntuples( res_ ); 
     return (r < 0 ? 0 : r);
 }
 
 int DB_T::statement::affected_rows()const
 {
     char* v;
     
     v = PQcmdTuples( res_ );
     
     if (!v || !v[0])
         return 0;
     
     int result = 0;
     
     ::std::sscanf(v, "%d", &result);
     
     return result;
 }
     
DB_T::statement::row_cl& operator >> ( DB_T::statement::row_cl& rcl, double& t)
{
    std::string s = rcl.ptr->get_str(rcl.row, rcl.col++);
    
    if (s.empty())
    {
        t = 0;
    }
    else
    {
        sscanf(s.c_str(), "%lf", &t);
    }
    
    return rcl;
}
