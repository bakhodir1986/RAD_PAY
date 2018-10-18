
#ifndef OSON_DB_T_H_INCLUDED
#define OSON_DB_T_H_INCLUDED 1

#include <string>

#include <postgresql/libpq-fe.h>

#include "types.h"

class DB_T;


#ifndef oson_this_db 

namespace oson{   DB_T& this_db(); } //forward declaration, so no more needed include users.h

#define oson_this_db ::oson::this_db()   

#endif 


struct Order_T
{
    enum Order
    { 
        ASC = 0, 
        DESC = 1
    };
    
    int field, field2, order;
    inline Order_T(int f = 1, int f2 = 0, int o = ASC): field(f), field2(f2), order(o){}
};
int make_order_from_string(std::string const& order_str , int default_order = Order_T::ASC) ;

struct Sort_T
{
	int      offset ;
	int      limit  ;
	Order_T  order  ;
    Sort_T(int offset = 0, int limit = 0, Order_T o = Order_T() );

    std::string to_string()const;
    int total_count(int rows)const;
};

class Connect_info_T
{
public:
	std::string m_host;
	std::string m_db_name;
	std::string m_user;
	std::string m_pass;
    
    std::string to_string()const;
};


struct DB_tag{};

class DB_T
{
private:
    DB_T(const DB_T&);// = delete
    DB_T& operator = (const DB_T&); // = delete
    
    DB_T();
public:
	//DB_T();
    explicit DB_T( DB_tag ) ;
	~DB_T();
    
    static Connect_info_T connectionInfo();
    static void initConnectionInfo(const Connect_info_T& connect_info);
    
    bool isconnected()const;
	int connect( );
    void disconnect();
    
    static std::string escape_d(double d);
    static std::string escape(long long number);
    static std::string escape( const std::string& view);
    static std::string escape( const std::string & view, int );
    /***
     * 
     * usage example:
     *  DB_T::transaction tr(db);// begin transaction
     *   .... operations ...
     *  if (fail)return error; //  rollback will accured on destructor of tr
     * 
     *  if (fatal)throw exception(); //  rollback will accured of destructor of tr.
     * 
     *  tr.commit();  
     */
    class transaction
    {
    private:
        PGconn* conn_;
        PGresult* res_;
        bool  commit_;
    public:
        explicit transaction(DB_T& ref);
        void commit();
        ~transaction();
    private:
        transaction(const transaction&); // = delete
        transaction& operator = (const transaction&); // = delete
    };
    
    /*
     * 
     * usage example:    
     *     DB_T::statement st(db);  
     *     Error_T ec = st.prepare("SELECT count(*) from mytable");
     *     if(ec)return ec;
     *     int cnt = st.get_int(0, 0);
     * 
     * disadvantage of DB_T is that, it's PGResult does not free until next query is performed.
     * But, statement solves this problem. It's destructor frees PGResult.
     * 
     *@Note: I recomended that, to remove all methods from DB_T  except connect and separate.
     */
    class statement
    {
    private:
        PGconn* conn_;
        PGresult* res_ ;
    public:
        explicit statement(DB_T& ref);
        
        ~statement();
        
        void prepare( const std::string& q);
        void prepare( const std::string& q, Error_T& ec ) ;
        
        std::string get_str(int row, int column)const;
        
        long long get_int(int row, int column)const;
        bool get_bool(int row, int column)const;
        
        int rows_count()const;
        int affected_rows()const;
    private:
        statement(const statement& );// = delete
        statement& operator = (const statement&); // = delete
    public:
        class row_cl
        {
        private:
            statement * ptr; 
            int row;
            int col;
            row_cl(const row_cl& ); // = delete
            row_cl& operator = (const row_cl& ); // = delete
        public:
            inline row_cl(): ptr(), row(),col(){}
            inline friend row_cl & operator >> ( row_cl & rcl, std::string& s){
                s = rcl.ptr->get_str( rcl.row, rcl.col++ ); return rcl; 
            }
            inline friend row_cl & operator >> (row_cl & rcl , bool & b ) {
                b = rcl.ptr->get_bool(rcl.row, rcl.col++); return rcl;
            }
#define OSON_DEFINE_OPERATOR(type)   inline friend row_cl& operator >> (row_cl& rcl, type & t){ t = rcl.ptr->get_int(rcl.row, rcl.col++); return rcl; }
            OSON_DEFINE_OPERATOR(int)
            OSON_DEFINE_OPERATOR(unsigned)
            OSON_DEFINE_OPERATOR(short)
            OSON_DEFINE_OPERATOR(unsigned short)
            OSON_DEFINE_OPERATOR(long)
            OSON_DEFINE_OPERATOR(long long)
            OSON_DEFINE_OPERATOR(unsigned long)
            OSON_DEFINE_OPERATOR(unsigned long long)
            OSON_DEFINE_OPERATOR(unsigned char)
#undef OSON_DEFINE_OPERATOR
            friend row_cl& operator >> (row_cl& rcl, double& t);
        public:
            inline void init(statement* p, int ro){ ptr = p; row = ro; col = 0; }
        }cl;
        
    public:
        inline row_cl&  row(int ro) { cl.init(this, ro); return cl; }
    };

public:
    inline PGconn* native_handle()const{ return m_conn; }
private:
    PGconn* m_conn;
};

inline std::string escape( long long number){ return DB_T::escape(number); }
inline std::string escape_d(double number) { return DB_T::escape_d(number); }

inline std::string escape( const std::string& text ){ return DB_T::escape(text ); }
inline std::string escape( const std::string& text, int no_content_check){ return DB_T::escape(text, no_content_check); }


#endif // end define

