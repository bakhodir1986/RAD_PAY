#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <string>
#include <vector>
#include "types.h"

//forward declarations
class Sort_T;
class DB_T  ;


enum Transaction_status_T {
    TR_STATUS_UNDEF = 0,
    TR_STATUS_SUCCESS     = 1, // FULLY SUCCESS of current transaction, must set EOPC ( current logic).
    TR_STATUS_CANCEL      = 2,
    TR_STATUS_REVERSED    = 3,
    TR_STATUS_REGISTRED   = 4,
    TR_STATUS_REVERSE     = 5,

    TR_STATUS_IN_PROGRESS = 6,
    TR_STATUS_REQUEST     = 10,
    TR_STATUS_REQUEST_DST = 11,
    TR_STATUS_DECLINE     = 12,
    TR_STATUS_TIMEOUT     = 13,

    TR_STATUS_ERROR       = 15,
    
    TR_STATUS_EOPC_PAY_SUCCESS = 17,
    TR_STATUS_MERCHANT_PAY_SUCCESS = 27, // Pay to merchant (Paynet, or Beeline, Ucell, ...) is SUCCESS, but WAIT debit from EOPC.
    TR_STATUS_ERROR_EOPC_PURCHASE = 29,
};


struct Transaction_info_T {
    typedef std::string text;
    typedef int64_t bigint;
    typedef int32_t integer;
    
	bigint id;
	bigint uid;
    bigint srccard_id;
    bigint dstcard_id;
	text srccard;
	text dstcard;
    text dstcard_exp;
	text srcphone;
	text dstphone;
    bigint amount;
    bigint comission;
	text ts;
    text from_date;
    text to_date;
    bigint dst_uid;
    text dst_name;
    text eopc_id;
    text temp_token;
    integer status;
    text    status_text;
    bigint bearn; // bonus earn.
    
    integer bank_id;  //need for search - src bank_id
    integer dstbank_id ; // need for search.
    
    integer aid; // used for search.
    
    Transaction_info_T() {
		id = 0;
		uid = 0;
        srccard_id = 0;
        dstcard_id = 0;
        dst_uid = 0;
		amount = 0;
        comission = 0;
        status = TR_STATUS_UNDEF;
        bearn = 0;
        bank_id = 0;
        dstbank_id = 0;
        
        aid = 0;
	}
};

struct Transaction_list_T {
	uint64_t count;
	std::vector<Transaction_info_T> list;
	Transaction_list_T() {
		count = 0;
	}
};

struct Outgoing_T 
{
    typedef int32_t integer;
    typedef int64_t bigint;
    typedef std::string text;
    
    integer type;
    bigint  id;
    text    ts;
    bigint  amount;
    bigint  commision;
    integer status;
    integer merchant_id;
    text    login;
    text    dst_card;
    text    dstphone;
    text    pan;
    bigint  oson_tr_id;
    bigint  card_id;
    bigint  bearn;
  
    
    text from_date, to_date; //for search;
    Outgoing_T() 
    : type(0)
    , id(0)
    , amount(0)
    , commision(0)
    , status(0)
    , merchant_id(0)
    , oson_tr_id(0)
    , card_id(0)
    , bearn(0) 
    {}
};

struct Outgoing_list_T {
    uint32_t count;
    std::vector<Outgoing_T> list;
};

struct Incoming_T 
{
    typedef int32_t integer;
    typedef int64_t bigint;
    typedef std::string text;
    
    integer type;
    bigint  id;
    text    ts;
    bigint  amount;
    bigint  commision;
    integer status;
    text    src_card;
    text    src_phone;
    text    dst_card;
    Incoming_T() : type(0), id(0), amount(0), commision(0), status(0) {
    }
};

struct Incoming_list_T {
    int32_t count;
    std::vector<Incoming_T> list;
};

struct Tr_stat_T {
    uint64_t total;
    uint64_t users;
    uint64_t sum;
    std::string ts;
};

struct Transaction_top_T {
    uint64_t sum;
    uint64_t count;
    std::string phone;
};

class Transactions_T
{
public:
	Transactions_T( DB_T & db );

    uint64_t m_transaction_id;

    Error_T transaction_list(const Transaction_info_T & search,  const Sort_T &sort, Transaction_list_T & list);
    
    int64_t transaction_add(const Transaction_info_T & data);
    
    Error_T transaction_edit(const uint64_t & trn_id, Transaction_info_T & data);
    Error_T transaction_cancel(const Transaction_info_T & which);
    Error_T transaction_del(const uint64_t &id);
    Error_T info(const uint64_t & id, Transaction_info_T &info);
    Error_T temp_transaction_add(const Transaction_info_T & data);
    Error_T temp_transaction_del(const std::string &token);
    Error_T temp_transaction_info(const std::string &token, Transaction_info_T & info);
    Error_T temp_transaction_info(uint64_t id, std::string &token);

    Error_T stat(uint16_t group, const Transaction_info_T & search, std::vector<Tr_stat_T> &stat);
    Error_T top(const Transaction_info_T & search, std::vector<Transaction_top_T> &tops);

    Error_T bill_accept(Transaction_info_T & info);

    Incoming_list_T incoming_list(int64_t uid, const Sort_T& sort) ;
 
    Outgoing_list_T outgoing_list(int64_t uid, const Outgoing_T& search, const Sort_T& sort) ;
private:
    DB_T & m_db;
};


#endif
