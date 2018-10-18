#ifndef EOCP_API_T_H
#define EOCP_API_T_H

#include <string>  // std::string
#include <utility> // std::pair
#include <vector>  // std::vector
#include <functional> // std::function
#include <memory>

#include <boost/asio/io_service.hpp>
#include <boost/asio/ssl.hpp>

#include "types.h"
#include "config_types.h"


struct eopc_network_info;
namespace oson{ namespace backend{ namespace eopc{
    
    namespace req
    {
        struct card
        {
            std::string pan;
            std::string expiry;
        };
    } 
 
    namespace resp
    {
        struct card
        {
            std::string id       ;
            std::string pan      ;
            std::string expiry   ;
            int         status   ; // EOCP_CARD_STATUS
            std::string phone    ;
            std::string fullname ;
            int64_t     balance  ;
            bool        sms      ;
            
            inline bool empty()const { return id.empty(); }
        };
    }
    
    
}}} // end oson::backend::eopc
//struct EOCP_C_Card_T 
//{
//    std::string pan;
//    std::string expiry;
//    EOCP_C_Card_T(){}
//    explicit EOCP_C_Card_T(std::string pan, std::string expiry): pan(pan), expiry(expiry){}
//};

struct EOCP_card_history_req
{
    std::string card_id   ;  // pc-token
    std::string startDate  ; //20160318   YYYYMMDD  format,
    std::string endDate    ;
    
    int32_t pageNumber     ; // 1,2,...
    int32_t pageSize       ;// from 1 to 50.
};

struct EOCP_card_history_resp
{
    std::string hpan ;  // mask PAN
    int32_t utime;     // h24mmss time of transaction
    int32_t udate;     // yyyymmdd date of transaction.
    int64_t reqamt;    // amount in tiyin.
    int64_t orgdev ;
    bool    reversal;   // reversed this transaction or not ? .
    std::string utrnno ; // reference number. Unique identifier of transaction in host machine.
    std::string merchantId; 
    std::string terminalId;
    std::string merchantName;
    std::string street;
    std::string city;
   
    int32_t resp;       //@Note: -1  success.
    bool credit;
    
    
    std::string date_time()const;
};

struct EOCP_card_history_list
{
    std::vector< EOCP_card_history_resp > list;
    bool    last             ;
    int32_t totalPages       ;
    int32_t totalElements    ;
    int32_t size             ;
    int32_t number           ;
    bool    first            ;
    int32_t numberOfElements ; 
};

bool valid_card_pan(const std::string& pan) ;

//@Note: Need add: namespace oson{ namespace eocp{ struct card_status{
enum EOCP_CARD_STATUS {
    EXPIRE_FIRE_CARD      = -1,
    VALID_CARD            = 0,
    CALL_ISSUER           = 1,
    WARM_CARD             = 2,
    DO_NOT_HONOR          = 3,
    HONOR_WITH_ID         = 4,
    NOT_PERMITED          = 5,
    LOST_CARD_CAPTURE     = 6,
    STOLEN_CARD_CAPTURE   = 7,
    CALL_SECURITY_CAPTURE = 8,
    INVALID_CARD_CAPTURE  = 9,
    PICK_UP_CARD_SPECIAL_CONDITION = 10,
    CALL_ACQUIRER_SECURITY = 11,
    TEMPORARY_BLOCKED_BY_USER = 12,
    PIN_ATTEMPTS_EXCEEDED = 13,
    FORCED_PIN_CHANGE = 14,
    CREDIT_DEBITS     = 15,
    UNKNOWN_CARD       = 16,
    PIN_ACTIVATION    = 17,
    INSTANT_CARD_PERSON_WAIT = 18,
    FRAUD_PREVENTION  = 19,
    PERMANENT_BLOCKED_BY_CLIENT = 20,
    TEMPORARY_BLOCKED_BY_CLIENT = 21,
};

//struct EOCP_Card_T  
//{
//    std::string id;
//    std::string pan;
//    std::string expiry;
//    int status;//EOCP_CARD_STATUS
//    std::string phone;
//    std::string fullname;
//    int64_t balance;
//    bool sms;
//    inline EOCP_Card_T()
//    : id(), pan(), expiry(), status(VALID_CARD), phone(), fullname(), balance(0)
//    {}
//    
//    inline bool empty()const{ return id.empty() ; }
//};

struct EOCP_Card_list_T
{
    typedef  oson::backend::eopc::resp::card  value_type;
    typedef ::std::vector< value_type > array_type;
    
    array_type list;
    
    inline oson::backend::eopc::resp::card get( const std::string & pc_token )const
    {
        oson::backend::eopc::resp::card result;
        for(size_t i = 0 ; i < list.size(); ++i)
        {
            if (pc_token == list[i].id )
                return list[i];
        }
        return result;
    }

    void insert( const value_type & val){  list.push_back(val); }
    void insert( const EOCP_Card_list_T& other){ list.insert( list.end(), other.list.begin(), other.list.end() ) ; }
};

std::string make_stan(uint64_t trn_id);

struct EOPC_Debit_T  
{
    typedef std::string text;
    text     cardId     ;
    text     merchantId ;
    text     terminalId ;
    uint16_t port       ;
    int64_t  amount     ;
    text     ext        ;
    text     date12     ;
    text     stan       ;
    
    inline EOPC_Debit_T(): port(0), amount(0){}
};

struct EOPC_Credit_T  
{
    std::string card_id;
    oson::backend::eopc::req::card card;
    std::string merchant_id;
    std::string terminal_id;
    uint16_t port;
    long long int amount;
    std::string ext;
    std::string date12;
};

enum EOPC_TRAN_TYPE {
    EOPC_TRAN_P2P = 1,
    EOPC_TRAN_DEBIT = 2,
    EOPC_TRAN_REVERSAL = 3,
    EOPC_TRAN_SMS = 4,
};

enum EOPC_TRAN_STATUS {
    EOPC_TRAN_CRED = 1,
    EOPC_TRAN_SENT,
    EOPC_TRAN_OK,
    EOPC_TRAN_ERR,
    EOPC_TRAN_ROK,
    EOPC_TRAN_RER,
};

struct EOPC_Tran_cred_T  
{
    std::string pan;
    std::string tranType;
    long long int amount;
    std::string date7;
    int stan;
    std::string date12;
    int expiry;
    std::string refNum;
    int authId;
    std::string merchantId;
    std::string terminalId;
    int currency;
    std::string field48;
    std::string field91;
    int resp;
    std::string status;
    std::string ext;
};

struct EOPC_Tran_T 
{
    std::string id;
    std::string pan;
    std::string pan2;
    std::string expiry;
    std::string tranType;
    long long int amount;
    std::string date7;
    uint64_t stan;
    std::string date12;
    std::string refNum;
    std::string autId;
    std::string merchantId;
    std::string terminalId;
    int currency;
    std::string field48;
    std::string field91;
    int resp;
    std::string ext;
    std::string status;
    
    std::string raw_rsp;
    
    bool status_ok()const;
    bool status_reverse_ok()const;//reverse OK
};

struct EOPC_ShortTran_T  
{
    long long amount;
    std::string date;
    std::string extId;
    std::string svId;
};

//#ifndef OSON_EOCP_REPORT_DISABLE
//struct EOPC_Time_range_T {
//    bp::ptime from;
//    bp::ptime to;
//};
//
//
//
//struct EOPC_ReportSum_T {
//    std::string from;
//    std::string to;
//    std::string count;
//    std::string sum;
//};
//
//struct EOPC_ReportDetail_T {
//    std::string from;
//    std::string to;
//    std::vector<EOPC_Tran_T> trans;
//};
//
//struct EOPC_ReportShort_T {
//    std::string from;
//    std::string to;
//    std::vector<EOPC_ShortTran_T> trans;
//};
//#endif 

struct EOPC_p2p_info_T  
{
    std::string owner;
    std::string exp_dt;
    std::string card_type;
};


struct EOPC_P2P_in_data 
{
    std::string cardId;
    std::string recipientId;
    oson::backend::eopc::req::card sender;
    oson::backend::eopc::req::card recipient;
    uint64_t     amount;
    std::string  tran_id;
    std::string  merchant_id;
    std::string  terminal_id;
    uint16_t     port;

    inline EOPC_P2P_in_data() {
        amount = 0;
        tran_id = "0";
        port = 0;
    }
};

namespace oson{ namespace backend{ namespace eopc{

enum Commands
{
    cmd_card_new         = 0,
    cmd_card_info        = 1,
    cmd_card_info_single = 2,
    cmd_card_block       = 3,
    
    cmd_trans_pay        = 4,
    cmd_trans_extId      = 5,
    cmd_trans_sv         = 6,
    cmd_trans_reverse    = 7,
    
    cmd_p2p_info         = 8,
    cmd_p2p_id2id        = 9,
    cmd_p2p_id2pan       = 10,
    cmd_p2p_pan2pan      = 11,
    cmd_p2p_credit       = 12,
    
    cmd_card_history     = 13,
};

}}}

/******************************************************************************************************************************/

namespace oson
{
    
    typedef std::function< void (const oson::backend::eopc::req::card& in, const oson::backend::eopc::resp::card & card, Error_T ec) > card_new_handler;
    typedef std::function< void (const std::vector< std::string> & ids, const EOCP_Card_list_T& info, Error_T ec)> card_info_handler;
    typedef std::function< void (const std::string& id, const oson::backend::eopc::resp::card& info, Error_T ec)      > card_info_single_handler;
    typedef std::function< void (const std::string& id, const oson::backend::eopc::resp::card& card, Error_T ec)      > card_block_handler;
    
    typedef std::function< void (const EOPC_Debit_T& debin, const EOPC_Tran_T& tran, Error_T ec)  >  trans_pay_handler ;
    typedef std::function< void (const std::string& extId, const EOPC_Tran_T& tran, Error_T ec)   >  trans_extId_handler;
    typedef std::function< void (const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec)  >  trans_sv_handler;
    typedef std::function< void (const std::string& tranId, const EOPC_Tran_T& tran, Error_T ec)  >  trans_reverse_handler;
    
    typedef std::function< void (const std::string& hpan, const EOPC_p2p_info_T& info, Error_T ec) >  p2p_info_handler;
    typedef std::function< void (const EOPC_P2P_in_data& in, const EOPC_Tran_T& tran, Error_T ec ) >  p2p_id2id_handler;
    typedef std::function< void (const EOPC_P2P_in_data& in, const EOPC_Tran_T& tran, Error_T ec ) >  p2p_id2pan_handler;
    typedef std::function< void (const EOPC_P2P_in_data& in, const EOPC_Tran_T& tran, Error_T ec ) >  p2p_pan2pan_handler;
    typedef std::function< void (const EOPC_Credit_T& credit, const EOPC_Tran_cred_T& tran, Error_T ec) > p2p_credit_handler;
    
    typedef std::function< void (const EOCP_card_history_req& , const EOCP_card_history_list&, Error_T ec) > card_history_handler;
    
    //for application
class EOPC_manager
{
public:
    EOPC_manager(const std::shared_ptr< boost::asio::io_service > & , const eopc_network_info&);
    ~EOPC_manager();
    
    void async_card_new(const oson::backend::eopc::req::card& in, const card_new_handler&);
    void async_card_info(const std::vector< std::string > & ids, const card_info_handler &);
    void async_card_info(const std::string& id, const card_info_single_handler& );
    void async_card_block(const std::string& id, const card_block_handler &);
    
    void async_trans_pay(const EOPC_Debit_T& debin, const trans_pay_handler& );
    
    void async_trans_extId(const std::string &extId, const trans_extId_handler&  );
    void async_trans_sv(const std::string &tranId,  const trans_sv_handler& );
    void async_trans_reverse(const std::string &tranId,  const trans_reverse_handler& );

    void async_p2p_info   ( const std::string & hpan,    const p2p_info_handler&     ) ;
    void async_p2p_id2id  ( const EOPC_P2P_in_data &in,  const p2p_id2id_handler&    ) ;
    void async_p2p_id2pan ( const EOPC_P2P_in_data &in,  const p2p_id2pan_handler&   ) ;
    void async_p2p_pan2pan( const EOPC_P2P_in_data &in,  const p2p_pan2pan_handler&  ) ;
    void async_p2p_credit ( const EOPC_Credit_T &credit, const p2p_credit_handler&   ) ;
    
    
    void async_card_history( const EOCP_card_history_req& req, const card_history_handler & ) ; 
    
public:
    eopc_network_info const&  eopc_net_info()const;
private:
    EOPC_manager(const EOPC_manager&); // = delete
    EOPC_manager& operator = (const EOPC_manager&); // = delete
private:
    std::shared_ptr< boost::asio::io_service > io_service_;
    std::shared_ptr< boost::asio::ssl::context> context_;
    eopc_network_info net_;
};
}// namespace oson

#endif // EOCP_API_T_H
