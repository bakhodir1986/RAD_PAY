#ifndef OSON_UTILS_H
#define OSON_UTILS_H

#include <stdint.h>

#include <cstddef>
#include <ctime>
#include <string> 
#include <vector>


namespace oson{ namespace utils{

    enum { MASK_PAN_BEGIN = 6 };
    enum { MASK_PAN_LENGTH = 6 };
std::string mask_pan(std::string pan);    
std::string generate_token(size_t length = 10);
std::string generate_code(size_t length);

bool valid_ascii_text( const std::string& text ) ;


bool send_email(const std::string& to, const std::string& from, const std::string& subject, const std::string & message);

//generated randomly 'length' digits.
//note: if length > 16 , will generate only 16 digits.
std::string generate_password(size_t length = 10);

std::string encodebase64(const unsigned char* d,  size_t dsize);
std::string encodebase64(const std::string& data);
std::string decodebase64(  std::string  data);
bool is_base64(const std::string& text);

std::string make_zip(const std::string& text);

//return raw byte stream
std::vector< unsigned char> encryptRSA(const std::string& hash, const std::string& modulus, const std::string& exponenta);

std::string md5_hash(const std::string& data, bool raw = false);
std::string sha1_hash(const std::string& data, bool raw = false );
std::string sha512_hash(const std::string& data, bool raw = false );
std::string sha256_hash(const std::string& data, bool raw = false );

std::string bin2hex(const std::string &data);
std::string hex2bin(const std::string& data);

// int  - for error-code
std::pair< std::string, int > sign_sha1(const std::string& data, const std::string& key_file_path, const std::string& password);
std::pair< std::string, int > sign_md5(const std::string& data, const std::string& key_file_path, const std::string& password ) ;

bool is_iso_date( const std::string& date );

//std::string to_hex(oson::string_view raw);
std::string to_hex(const unsigned char r[], size_t l);

time_t  last_modified_time(const std::string& path);

std::string prettify_json(const std::string& json);

std::string load_file_contents(const std::string& filepath );

bool file_exists(const std::string& path);

template<typename T >
inline T const& clamp(T const& v, T const& lo, T const& hi){
    if ( v < lo ) return lo;
    if ( v > hi ) return hi;
    return v;
}

template<typename T >
inline size_t  number_of_digits(T d, T base = T(10) ){ size_t r = 0; do ++r; while( d /= base ); return r; }


}} // oson::utils

typedef unsigned char byte_t;
typedef std::vector< uint8_t> byte_array;
//typedef unsigned long long ull;

// no defined
class ByteReader_T{
private:
    ByteReader_T(const ByteReader_T& );
    ByteReader_T& operator = (const ByteReader_T&);
public:
    explicit ByteReader_T(const byte_t* data, size_t size);
    ~ByteReader_T();
    ByteReader_T& reset();
    uint8_t readByte();
    uint16_t readByte2();
    uint32_t readByte4();
    uint64_t readByte8();
    std::string readString();
    std::vector<byte_t> readAsVector(size_t len);
    std::string readAsString(size_t len);
    
//    oson::string_view readAsStringView(size_t len);
    
    size_t remainBytes()const;
private:
    const byte_t * m_curPos;
    const byte_t * m_begPos;
    const byte_t * m_endPos;
};

/******************************************************************/
template< int n, typename T > struct rn{ T* p; enum{ value = n }; };
template<typename T > 
ByteReader_T& operator >> (ByteReader_T& in, struct rn<1,T> r){
    *(r.p) = in.readByte();
    return in;
}
template<typename T > 
ByteReader_T& operator >> (ByteReader_T& in, struct rn<2,T> r){
    *(r.p) = in.readByte2();
    return in;
}
template<typename T > 
ByteReader_T& operator >> (ByteReader_T& in, struct rn<4,T> r){
    *(r.p) = in.readByte4();
    return in;
}
template<typename T > 
ByteReader_T& operator >> (ByteReader_T& in, struct rn<8,T> r){
    *(r.p) = in.readByte8();
    return in;
}


//inline ByteReader_T& operator >> (ByteReader_T& in, std::string& s){
//    s = in.readAsString(in.readByte2());
//    return in;
//}

template< typename T>
inline struct rn<1,T> r1(T& t){ struct rn<1,T> r = { &t }; return r; }
template< typename T>
inline struct rn<2,T> r2(T& t){ struct rn<2,T> r = { &t }; return r; }
template< typename T>
inline struct rn<4,T> r4(T& t){ struct rn<4,T> r = { &t }; return r; }
template< typename T>
inline struct rn<8,T> r8(T& t){ struct rn<8,T> r = { &t }; return r; }

template< int n > struct str_ref{ std::string* s; };
inline str_ref<2> r2(std::string&s){ struct str_ref<2> ref = {&s}; return ref; }
inline str_ref<4> r4(std::string&s){ struct str_ref<4> ref = {&s}; return ref; }

inline ByteReader_T& operator >> (ByteReader_T& in, str_ref<2> s){
    *(s.s) = in.readAsString(in.readByte2());
    return in;
}

inline ByteReader_T& operator >> (ByteReader_T& in, str_ref<4> s){
    *(s.s) = in.readAsString(in.readByte4());
    return in;
}

//string_view also need
//template< int n > struct str_view_ref{ oson::string_view* s; };
//inline str_view_ref<2> r2(oson::string_view& s){ struct str_view_ref<2> ref = {&s}; return ref; }
//inline str_view_ref<4> r4(oson::string_view& s){ struct str_view_ref<4> ref = {&s}; return ref; }

//inline ByteReader_T& operator >> (ByteReader_T& in, str_view_ref<2> s){
//    *(s.s) = in.readAsStringView(in.readByte2());
//    return in;
//}
//
//inline ByteReader_T& operator >> (ByteReader_T& in, str_view_ref<4> s){
//    *(s.s) = in.readAsStringView(in.readByte4());
//    return in;
//}


/************************************************************************************/
class ByteWriter_T 
{
private:
    ByteWriter_T(const ByteWriter_T&);
    ByteWriter_T& operator = (ByteWriter_T&);
public:
    explicit ByteWriter_T(byte_t* data, size_t size);
    explicit ByteWriter_T(std::vector<byte_t>& buf);
    ~ByteWriter_T();
    void writeByte( uint8_t value);
    void writeByte2(uint16_t value);
    void writeByte4(uint32_t value);
    void writeByte8(uint64_t value);
    
    void writeVector( const std::vector<byte_t>& v);
    void writeString( const std::string & v);
    
    size_t remainBytes()const;
private:
    byte_t* m_curPos;
    byte_t* m_endPos;
};

class ByteStreamWriter
{
private:
    ByteStreamWriter(const ByteStreamWriter&);
    ByteStreamWriter& operator = (const ByteStreamWriter &);
public:
    ByteStreamWriter();
    ~ByteStreamWriter();
    const std::vector<byte_t>& get_buf()const;
    std::vector<byte_t>& get_buf();
    void writeByte(uint8_t value);
    void writeByte2(uint16_t value);
    void writeByte4(uint32_t value);
    void writeByte8(uint64_t value);
    
    void writeVector( const std::vector<byte_t>& v);
    void writeString( const std::string & v);
//    void writeStringView( oson::string_view view);
    inline void clear(){ m_buf.clear(); }
    
    inline void swap(ByteStreamWriter& other){ m_buf.swap(other.m_buf); }
private:
    std::vector<byte_t> m_buf;
};

inline void swap(ByteStreamWriter&a, ByteStreamWriter& b){ a.swap(b); }

template< int n > struct bn{ uint64_t value; };

inline bn<1>  b1(uint64_t value ){  bn<1> b = { value } ; return b; }
inline bn<2>  b2(uint64_t value ){  bn<2> b = { value } ; return b; }
inline bn<4>  b4(uint64_t value ){  bn<4> b = { value } ; return b; }
inline bn<8>  b8(uint64_t value ){  bn<8> b = { value } ; return b; }




inline ByteStreamWriter& operator << (ByteStreamWriter& bw, bn<1> b){  bw.writeByte(b.value);  return bw; }
inline ByteStreamWriter& operator << (ByteStreamWriter& bw, bn<2> b){  bw.writeByte2(b.value); return bw; }
inline ByteStreamWriter& operator << (ByteStreamWriter& bw, bn<4> b){  bw.writeByte4(b.value); return bw; }
inline ByteStreamWriter& operator << (ByteStreamWriter& bw, bn<8> b){  bw.writeByte8(b.value); return bw; }


template< int n > struct str_cref{ const std::string* s; } ;
inline str_cref<4> b4(const std::string& s){  str_cref<4> r = { &s } ; return r;}


inline ByteStreamWriter& operator << (ByteStreamWriter& bw, str_cref<4> r)
{
    bw.writeByte4(r.s->size());
    bw.writeString(*(r.s));
    return bw;
}

inline ByteStreamWriter& operator << (ByteStreamWriter& bw, const std::string& s){ 
    bw.writeByte2(s.size());
    bw.writeString(s);
    return bw;
}
//inline ByteStreamWriter& operator << (ByteStreamWriter& bw, oson::string_view view){
//    bw.writeByte2(view.size());
//    bw.writeStringView(view);
//    return bw;
//}

inline ByteStreamWriter& operator << (ByteStreamWriter& bw, const byte_array& ar){
    bw.writeByte2(ar.size());
    bw.writeVector(ar);
    return bw;
}

template<typename T>
inline ByteStreamWriter& operator << (ByteStreamWriter& bw, const std::vector<T> & v){
    bw.writeByte2( v.size() );
    for(size_t i = 0, n = v.size();  i != n ; ++i){
        bw << v[ i ];
    }
    return bw;
}

std::string formatted_time(const char* format, std::time_t t);
// %Y-%m-%d %H:%M:%S
// 2017-21-12 13:43:15
//
std::string formatted_time_now(const char* format);
inline std::string formatted_time_now_iso_S(){ return formatted_time_now("%Y-%m-%d %H:%M:%S") ; }
inline std::string formatted_time_now_iso_T(){ return formatted_time_now("%Y-%m-%dT%H:%M:%S") ; }
inline std::string formatted_time_iso( std::time_t t ) { return formatted_time( "%Y-%m-%d %H:%M:%S" , t ) ; }

//  YYYY-MM-DD hh:mm:ss   format --> convert to time_t 
std::time_t str_2_time(const char* str);
std::time_t str_2_time_T(const char* str);

std::string num2string( long long number );
std::string to_str( long long number);
std::string to_money_str(long long money_tiyin, char sep = ',' );

std::string to_str(double t, int precision , bool trim_leading_zero = true );

long long string2num(const std::string& s);

#endif // OSON_UTILS
