#include <cstdlib>  //  rand
#include <cstring> // memcpy
#include <algorithm>
#include <fstream>
#include <sstream>

#include <boost/scope_exit.hpp>

#include "utils.h"
#include "types.h"


static const char alphan_digit[] = "0123456789";

static const char alphan_letter[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
       
// result in [0..n)
static size_t rand_range(size_t n) 
{
    long long r = rand();
    size_t rz = ( r * n + RAND_MAX / 2 ) / (int)RAND_MAX;
    return  rz < n ? rz : n - 1;
}

std::string oson::utils::mask_pan(std::string pan)
{
    if (pan.length() >= 16)
        pan.replace(MASK_PAN_BEGIN, MASK_PAN_LENGTH, MASK_PAN_LENGTH, '*');
    return pan;
}
std::string oson::utils::generate_token(size_t length /* = 10 */)
{
    return oson::utils::generate_password(length);
}

std::string oson::utils::generate_code(size_t length)
{
    std::string result( (length), ('\0') );

    for(size_t i = 0;  i != length; ++i)
        result[ i ] = alphan_digit[ rand_range(10) ] ;
        
    return result;
}


std::string oson::utils::generate_password(size_t length /* = 10 */)
{
    
    std::string result( (length), ('\0') );
    
    for(size_t i = 0; i  != length; ++i)
        result[i] = alphan_letter[ rand_range(62) ];
    
    return result;
}

bool oson::utils::send_email(const std::string& to, const std::string& from, const std::string& subject, const std::string & message)
{
    
    bool retval = false;
    
    FILE *mailpipe = popen("/usr/lib/sendmail -t", "w");
    
    if (mailpipe != NULL) 
    {
        fprintf(mailpipe, "To: %s\n", to.c_str());
        fprintf(mailpipe, "From: %s\n", from.c_str());
        fprintf(mailpipe, "Subject: %s\n\n", subject.c_str());
        fwrite(message.data(), 1, message.size(), mailpipe);
        fwrite(".\n", 1, 2, mailpipe);
        pclose(mailpipe);
        retval = true;
     }
     else {
         perror("Failed to invoke sendmail");
     }
     return retval;
}

bool oson::utils::valid_ascii_text( const std::string& text ) 
{
    for(char c : text)
    {
        unsigned u = c;
        if ( /*u < 0x09 ||*/ u >= 0x80 )
        {
            return false;
        }
    }
    return true;
}

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/remove_whitespace.hpp>
#include <sys/stat.h>
#include <unistd.h>

std::string oson::utils::encodebase64(const unsigned char* d,  size_t dsize)
{
    using namespace boost::archive::iterators;
    typedef base64_from_binary< transform_width< const unsigned char*, 6, 8> > base64_text;
    std::string result;
    result.reserve(4 + dsize * 4 / 3 ) ;
    std::copy(base64_text(d), base64_text(d + dsize), std::back_inserter(result) ) ;
    
    while(dsize % 3 != 0 ) {
        result += '=';
        ++dsize;
    }
    return result;
}

std::string oson::utils::encodebase64(const std::string& data)
{
    using namespace boost::archive::iterators;
    
    typedef base64_from_binary< transform_width< const char *, 6, 8 > > base64_text;
    
    std::string result;
    
    result.reserve( data.size() * 4 / 3  );
    
    std::copy(base64_text(data.c_str()), base64_text(data.c_str() + data.size()), std::back_inserter(result));
    
    std::string::size_type data_size = data.size();
    
    while( data_size % 3 != 0)
        (result += '='), (++data_size);
    
    return result;

}
std::string oson::utils::decodebase64(  std::string  data)
{
  using namespace boost::archive::iterators;
  typedef transform_width<binary_from_base64<remove_whitespace <std::string::const_iterator> >, 8, 6> base64_text;
  //typedef base64_from_binary< transform_width< std::string::const_iterator, 8, 6 > > base64_text;
  
    // If the input isn't a multiple of 4, pad with =
    size_t num_pad_chars = ((4 - data.size() % 4) % 4);
    data.append(num_pad_chars, '=');

    size_t pad_chars =  std::count(data.begin(), data.end(), '=') ;
    std::replace(data.begin(), data.end(), '=', 'A');
    std::string output(base64_text(data.begin()), base64_text(data.end()));
    output.erase(output.end() - pad_chars, output.end());
    return output;
}
static bool base64_char(int c){ return (c >= 'a' && c <= 'z') ||(  c >= 'A' && c <= 'Z' ) || (c >= '0' && c <= '9') || (c == '+') || (c == '/') || (c == '=' ) ; }

bool oson::utils::is_base64(const std::string& text)
{
    return std::all_of(text.begin(), text.end(), base64_char);
}



#include <zlib.h>
 
std::string oson::utils::make_zip(const std::string& text)
{
    std::string result;
    
    
    size_t in_data_size = text.size() + 1 ; /* +1 for terminal zero. */
    void* in_data  = (void*)text.data();
    
    const size_t BUFSIZE = 128 * 1024;
    char temp_buffer[BUFSIZE];

    z_stream strm;
    strm.zalloc = 0;
    strm.zfree = 0;
    strm.next_in = (Bytef *)(in_data);
    strm.avail_in = in_data_size;
    strm.next_out = (Bytef*)temp_buffer;
    strm.avail_out = BUFSIZE;

    deflateInit(&strm, Z_BEST_COMPRESSION);

    while (strm.avail_in != 0)
    {
        int res = deflate(&strm, Z_NO_FLUSH);
        
        assert(res == Z_OK);
        oson::ignore_unused(res);
        
        if (strm.avail_out == 0)
        {
            //buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
            result.append(temp_buffer, BUFSIZE);
            strm.next_out = (Bytef*)temp_buffer;
            strm.avail_out = BUFSIZE;
        }
    }

    int deflate_res = Z_OK;
    while (deflate_res == Z_OK)
    {
        if (strm.avail_out == 0)
        {
            //buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE);
            result.append(temp_buffer, BUFSIZE);
            strm.next_out = (Bytef*)temp_buffer;
            strm.avail_out = BUFSIZE;
        }
        
        deflate_res = deflate(&strm, Z_FINISH);
    }

    assert(deflate_res == Z_STREAM_END);
    //buffer.insert(buffer.end(), temp_buffer, temp_buffer + BUFSIZE - strm.avail_out);
    result.append(temp_buffer, BUFSIZE - strm.avail_out);
    
    deflateEnd(&strm);


    return result;
}


#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/ossl_typ.h>
#include <openssl/aes.h>
#include <openssl/rsa.h>

//
//std::string oson::utils::to_hex( const std::string& raw)
//{
//    std::string result;
//    result.reserve(raw.size() * 2);
//    for(size_t i = 0; i < raw.size(); ++i)
//    {
//        unsigned char u = raw[i];
//        unsigned char hi = u >> 4, lo = u & 15;
//        
//        result += char(hi < 10 ? hi + 48 : hi + 87);
//        result += char(lo < 10 ? lo + 48 : lo + 87);
//    }
//    return result;
//}

std::string oson::utils::to_hex(const unsigned char raw[], size_t l)
{
    std::string result( l * 2 ,'\0' );

    for(size_t i = 0; i < l; ++i)
    {
        unsigned char u = raw [ i ] ;
        unsigned char hi = u >> 4, lo = u & 15;
        
        result[ ( i << 1 ) | 0 ]  =  (hi < 10 ? hi + 48 : hi + 87);
        result[ ( i << 1 ) | 1 ]  =  (lo < 10 ? lo + 48 : lo + 87);
    }
    return result;
}


std::vector< unsigned char> oson::utils::encryptRSA(const std::string& hash,  const std::string& modulus, const std::string& exponenta)
{
    /** from http://qaru.site/questions/426357/how-do-i-import-an-rsa-public-key-from-net-into-openssl */
    
//    int rsaLen = RSA_size( pubKey ) ;
//  unsigned char* ed = (unsigned char*)malloc( rsaLen ) ;
//  
//  // RSA_public_encrypt() returns the size of the encrypted data
//  // (i.e., RSA_size(rsa)). RSA_private_decrypt() 
//  // returns the size of the recovered plaintext.
//  *resultLen = RSA_public_encrypt( dataSize, (const unsigned char*)str, ed, pubKey, PADDING ) ; 
//  if( *resultLen == -1 )
//    printf("ERROR: RSA_public_encrypt: %s\n", ERR_error_string(ERR_get_error(), NULL));
//
//  return ed ;
    
    std::vector< unsigned char> result;
    //////////////////////// 1. create RSA  key from modulus and exponenta ////////////////////////
    RSA* pubKey = NULL ;
    
    {
        BIGNUM * bn_mod = NULL;
        BIGNUM * bn_exp = NULL;

        const unsigned char* mod = (const unsigned char*)(modulus.c_str());
        const unsigned char* exp = (const unsigned char*)(exponenta.c_str());
        int mod_len = modulus.size();
        int exp_len = exponenta.size();
        
        bn_mod = BN_bin2bn( mod, mod_len, NULL ) ; // Convert both values to BIGNUM
        bn_exp = BN_bin2bn( exp, exp_len, NULL ) ;

        RSA* key = RSA_new(); // Create a new RSA key
        
        if (key == NULL )  {
            return {};
        }
        
        key->n = bn_mod; // Assign in the values
        key->e = bn_exp;
        key->d = NULL;
        key->p = NULL;
        key->q = NULL;
        
        pubKey = key;
    }
    
    // free on exit.
    struct pub_key_exit{ RSA* k; ~pub_key_exit(){ RSA_free(k); } } pub_key_ex_v = { pubKey } ;
    /////////////////////// 2. create encrypted string.   //////////////////////////////////////
    
    int rsaLen = RSA_size( pubKey );
    
    result.resize(rsaLen);
    
    int dataSize    =  hash.size();
    const char* str =  hash.c_str();
    
    unsigned char* ed = result.data();
    
    int resultLen = RSA_public_encrypt(dataSize, (const unsigned char*)str, ed, pubKey, RSA_PKCS1_PADDING ) ;
    
    if (resultLen < 0 ){
        return {};
    }
    
    if (resultLen < rsaLen ) {
        result.resize(resultLen);
    }
    
    return result;  
}

std::string oson::utils::md5_hash(const std::string& data, bool raw /* = false */ )
{
    unsigned char result[ MD5_DIGEST_LENGTH ]; // 128 bits -> 16 bytes
    
    MD5_CTX md5_ctx;
    MD5_Init(&md5_ctx);
    MD5_Update(&md5_ctx, (const void*)data.c_str(), data.size());
    MD5_Final(result, &md5_ctx);
    
    if (raw) return std::string ((const char*)result, sizeof(result) );
    
    return to_hex( result, MD5_DIGEST_LENGTH );
}

std::string oson::utils::sha1_hash(const std::string& data, bool raw /* = false */ )
{
    unsigned char result[ SHA_DIGEST_LENGTH   ]; // 160 bits --> 20 bytes
    SHA_CTX sha_ctx;
    SHA1_Init(&sha_ctx);
    SHA1_Update(&sha_ctx, (const void*)data.c_str(), data.size());
    SHA1_Final(result, &sha_ctx);
    
    if (raw) return std::string ((const char*)result, sizeof(result) );
    
    return to_hex(  result, SHA_DIGEST_LENGTH  );
}
std::string oson::utils::sha512_hash(const std::string& data, bool raw /* = false */ )
{
    unsigned char result[ SHA512_DIGEST_LENGTH  ]; // 512 bits --> 64 bytes;
    
    SHA512_CTX sha_ctx;
    SHA512_Init(&sha_ctx);
    SHA512_Update(&sha_ctx, (const void*)data.c_str(), data.size());
    SHA512_Final(result, &sha_ctx);
    
    if (raw) return std::string ((const char*)result, sizeof(result) );
    
    return  to_hex(  result,  SHA512_DIGEST_LENGTH );
}

std::string  oson::utils::sha256_hash(const std::string& data, bool raw /* = false */ )
{
    unsigned char result[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha_ctx;
    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, (const void*)data.c_str(), data.size());
    SHA256_Final(result, &sha_ctx);
    
    if (raw) return std::string ((const char*)result, sizeof(result) );
    
    return to_hex( result, SHA256_DIGEST_LENGTH );
}
 

std::string oson::utils::bin2hex(const std::string &data)
{
    static const char hex_char[] = "0123456789abcdef" ;
    
    std::string result;
    result.reserve(data.size()*2);
    for(char c: data)
    {
        unsigned u = (unsigned)c;
        unsigned low   =  ( u >> 0 )  & 0x0f ;
        unsigned high  =  ( u >> 4 )  & 0x0f ;
        
        result += hex_char[high];
        result += hex_char[low];
    }
    return result;
}

std::string oson::utils::hex2bin(const std::string& data)
{
   // assert(false);
    std::string result;
    result.reserve(data.size() / 2 + 1 ) ;
    
    size_t n = data.size() / 2 ;
    for(size_t i = 0; i < n; ++i)
    {
        char high = data[ i * 2     ] ;
        char low  = data[ i * 2 + 1 ] ;
        unsigned hu, lu;
        if (high >= '0' && high <= '9' ) 
            hu = high - '0';
        else if (high >= 'a' && high <= 'f' ) 
            hu = high - 'a' + 10;
        else if (high >= 'A' && high <= 'F' ) 
            hu = high - 'A' + 10;
        else
            hu = 0 ;
        
        if (low >='0' && low <= '9' ) 
            lu = low - '0';
        else if (low >='a' && low <='f')
            lu = low - 'a' + 10;
        else if (low >='A' && low <='F' )
            lu = low - 'A' + 10;
        else
            lu = 0;
        
        unsigned code = (hu << 4) | lu ;
        
        result += (char)code;
        
    }
    
    if ( n % 2 == 1 )
    {
        char high = data[ n - 1 ] ;
        unsigned hu, lu;
        if (high >= '0' && high <= '9' ) 
            hu = high - '0';
        else if (high >= 'a' && high <= 'f' ) 
            hu = high - 'a' + 10;
        else if (high >= 'A' && high <= 'F' ) 
            hu = high - 'A' + 10;
        else
            hu = 0 ;
        lu = 0;
        unsigned code = (hu << 4) | lu ;
        result += (char)code;
    }
    
    return result ;
}



#include <openssl/x509v3.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

std::pair<std::string,int> oson::utils::sign_sha1(const std::string& data, const std::string& key_file_path, const std::string& password)
{
#define OSON_SIGN_SHA1_CHECK(cond, ec)   if (static_cast<bool>( cond ) ){ result.second = ec; return result; }
    
    std::pair< std::string, int> result;
    
    EVP_PKEY * private_key = NULL ;
    FILE* fp               = NULL ;
//    RSA* rsa_key            = NULL ;
    //unsigned char * sigret = NULL ;
    //unsigned int siglen    = 0    ;
    
    result.second = 0;//success
    
    OpenSSL_add_all_algorithms();
    private_key = EVP_PKEY_new();
    
    OSON_SIGN_SHA1_CHECK((NULL==private_key), 1) ;
    
    BOOST_SCOPE_EXIT(&private_key){
        if (private_key){ EVP_PKEY_free(private_key); private_key = NULL; }
    }BOOST_SCOPE_EXIT_END;
    
    fp = fopen(key_file_path.c_str(), "r");
    
    OSON_SIGN_SHA1_CHECK((NULL == fp), 2);
    

    PEM_read_PrivateKey(fp, &private_key, NULL, NULL ) ;
    fclose(fp);
    
//    rsa_key = EVP_PKEY_get1_RSA(private_key);
//    
//    if (NULL == rsa_key){
//        result.second = 3; //can't get RSA
//        return result;
//    }
//    
//    BOOST_SCOPE_EXIT(&rsa_key){
//        if (rsa_key){RSA_free(rsa_key); rsa_key =NULL; }
//    }BOOST_SCOPE_EXIT_END;
//    
//    if ( 0 == RSA_check_key( rsa_key ) ) {
//        result.second = 4;//check rsa failed.
//        return result;
//    }
//    
    //siglen = RSA_size( rsa_key )   ;
    //sigret = (unsigned char*) malloc( siglen * sizeof( unsigned char ) ) ;
    //if (  NULL == sigret ) {
    //    result.second = 5;//can't allocate required memory.
    //    return result;
   // }
    
    //RSA_sign(NID_sha1, (  const unsigned char* )(data.c_str()), data.size(), sigret, &siglen, rsa_key);
    {
        int ret = 1;
        const char* msg = NULL;
        size_t msg_len = 0;
        unsigned char* sig = NULL;
        size_t slen = 0 ;
        
        /* Create the Message Digest Context */
        EVP_MD_CTX * mdctx = EVP_MD_CTX_create() ;
        BOOST_SCOPE_EXIT(&mdctx){
            if(mdctx) { EVP_MD_CTX_destroy(mdctx); mdctx = NULL; }
        }BOOST_SCOPE_EXIT_END;
        
        OSON_SIGN_SHA1_CHECK((NULL == mdctx), 6);
        
        /* Initialise the DigestSign operation - SHA-1 has been selected as the message digest function in this example */
         ret = EVP_DigestSignInit(mdctx, NULL, EVP_sha1(), NULL, private_key) ;
         OSON_SIGN_SHA1_CHECK((1 != ret), 7);
         
         msg = data.c_str();
         msg_len = data.size();
         /* Call update with the message */
         ret  = EVP_DigestSignUpdate(mdctx, msg, msg_len ) ;
         OSON_SIGN_SHA1_CHECK((1 != ret), 8);
         
         /* Finalise the DigestSign operation */
         /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
          * signature. Length is returned in slen */
         ret = EVP_DigestSignFinal(mdctx, NULL, &slen) ;
         OSON_SIGN_SHA1_CHECK((1 != ret), 9);
 
         /* Allocate memory for the signature based on size in slen */
         sig = (unsigned char*) OPENSSL_malloc(sizeof(unsigned char) * slen ) ;
         BOOST_SCOPE_EXIT(&sig){
             if (sig){ OPENSSL_free(sig); sig = NULL; }
         }BOOST_SCOPE_EXIT_END;
         
         OSON_SIGN_SHA1_CHECK((NULL == sig), 10);

         /* Obtain the signature */
         ret = EVP_DigestSignFinal(mdctx,  sig, &slen) ;

         OSON_SIGN_SHA1_CHECK((1 != ret), 11);
         
         /* Success */
         result.first = oson::utils::encodebase64(sig, slen);
    }
    
    
    //free(sigret); 
    //sigret = NULL ;
    
    //RSA_free( rsa_key ); 
    //rsa_key = NULL;
    
    //EVP_PKEY_free(private_key); 
    //private_key = NULL ;
    
    return result;

#undef OSON_SIGN_SHA1_CHECK 
    
    //  $privateKey = openssl_get_privatekey(file_get_contents($key), $keyPassword);
    //	openssl_sign($data, $signature, $privateKey, OPENSSL_ALGO_SHA1);
    //	return base64_encode($signature);
}

bool oson::utils::is_iso_date( const std::string& date )
{
    //%Y-%m-%d
    std::time_t t = str_2_time(date.c_str());
    if (t == (std::time_t)(0) || t == (std::time_t)(-1))
        return false;
    return true;

}

std::pair< std::string, int > oson::utils::sign_md5(const std::string& data, const std::string& key_file_path, const std::string& password ) 
{
#define OSON_SIGN_MD5_CHECK(cond, ec)   if (static_cast<bool>( cond ) ){ result.second = ec; return result; }
    
    std::pair< std::string, int> result;
    
    EVP_PKEY * private_key = NULL ;
    FILE* fp               = NULL ;

    result.second = 0;//success
    
    OpenSSL_add_all_algorithms();
    private_key = EVP_PKEY_new();
    
    OSON_SIGN_MD5_CHECK((NULL==private_key), 1) ;
    
    BOOST_SCOPE_EXIT(&private_key){
        if (private_key){ EVP_PKEY_free(private_key); private_key = NULL; }
    }BOOST_SCOPE_EXIT_END;
    
    fp = fopen(key_file_path.c_str(), "r");
    
    OSON_SIGN_MD5_CHECK((NULL == fp), 2);
    

    PEM_read_PrivateKey(fp, &private_key, NULL, NULL ) ;
    fclose(fp);
    
    
    //RSA_sign(NID_sha1, (  const unsigned char* )(data.c_str()), data.size(), sigret, &siglen, rsa_key);
    {
        int ret = 1;
        const char* msg = NULL;
        size_t msg_len = 0;
        unsigned char* sig = NULL;
        size_t slen = 0 ;
        
        /* Create the Message Digest Context */
        EVP_MD_CTX * mdctx = EVP_MD_CTX_create() ;
        BOOST_SCOPE_EXIT(&mdctx){
            if(mdctx) { EVP_MD_CTX_destroy(mdctx); mdctx = NULL; }
        }BOOST_SCOPE_EXIT_END;
        
        OSON_SIGN_MD5_CHECK((NULL == mdctx), 6);
        
        /* Initialise the DigestSign operation - MD5 has been selected as the message digest function in this example */
         ret = EVP_DigestSignInit(mdctx, NULL, EVP_md5(), NULL, private_key) ;
         OSON_SIGN_MD5_CHECK((1 != ret), 7);
         
         msg    = data.c_str();
         msg_len = data.size();
         /* Call update with the message */
         ret  = EVP_DigestSignUpdate(mdctx, msg, msg_len ) ;
         OSON_SIGN_MD5_CHECK((1 != ret), 8);
         
         /* Finalise the DigestSign operation */
         /* First call EVP_DigestSignFinal with a NULL sig parameter to obtain the length of the
          * signature. Length is returned in slen */
         ret = EVP_DigestSignFinal(mdctx, NULL, &slen) ;
         OSON_SIGN_MD5_CHECK((1 != ret), 9);
 
         /* Allocate memory for the signature based on size in slen */
         sig = (unsigned char*) OPENSSL_malloc(sizeof(unsigned char) * slen ) ;
         BOOST_SCOPE_EXIT(&sig){
             if (sig){ OPENSSL_free(sig); sig = NULL; }
         }BOOST_SCOPE_EXIT_END;
         
         OSON_SIGN_MD5_CHECK((NULL == sig), 10);

         /* Obtain the signature */
         ret = EVP_DigestSignFinal(mdctx,  sig, &slen) ;

         OSON_SIGN_MD5_CHECK((1 != ret), 11);
         
         /* Success */
         result.first = oson::utils::encodebase64(sig, slen);
    }
    return result;
#undef OSON_SIGN_MD5_CHECK 
}

time_t  oson::utils::last_modified_time(const std::string& path)
{
//    int stat(const char *path, struct stat *buf);
#ifdef __unix__
    struct stat buf;
    int r;
    
    r = ::stat(path.c_str(), &buf);
    if (r != 0)
        return (time_t)-1;
    return buf.st_mtim.tv_sec ;
#elif defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)  //windows
    
#else
#error "Not Implemented for this platform"
#endif 
}


std::string oson::utils::load_file_contents(const std::string& filepath  )
{
    std::ifstream fin(filepath.c_str(), std::ios::in | std::ios::binary);
    
    std::ostringstream oss;
    oss << fin.rdbuf();
    return oss.str() ;

}
bool oson::utils::file_exists(const std::string& path)
{
#ifdef __unix__
    return ::access(path.c_str(), F_OK) == 0 ;
#elif defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64) //windows
    
#else 
#error "Not implemented for this platform"
#endif 
}


std::string oson::utils::prettify_json(const std::string& json)
{
    std::string result;
    result.reserve(json.size());
    
    bool in_str = false; // current character in string or not
    char quot   = '\'';  // current string around this symbol
    int deep    = 0;     // no comment needed)
    
    for(size_t i= 0; i < json.size(); ++i)
    {
        const char cur = json[i];
        
        if ( ! in_str )
        {
            if (cur == '[' || cur == '{' || cur == ',')
            {
                result += cur;
                result += '\n';
                if (cur == '[' || cur == '{')
                    ++deep;
                result.append(deep * 3, ' ');
                continue;
            }
            
            if (cur == ']' || cur == '}')
            {
                result += '\n';
                
                --deep;
                result.append(deep * 3, ' ');
                
                result += cur;
                continue;
            
            }
            
            //skip any spaces.
            if(isspace(cur))
                continue;
            
            if (cur == ':') // separator object name
                result += ' ';
            
            result += cur;
            
            if (cur == ':')
                result += ' ';
            
            //string first symbol
            if (cur == '\"' || cur == '\'')
                in_str = true, quot = cur;
            
        }
        else // in str
        {
            result += cur;
            if (cur == quot ) // end quot
                in_str = false;
            else if(cur == '\\')
                result += json[ ++i ];//next also add
        }
    }
    
    return result;
    
}
       
//on utils_endian.cpp
void debug_check_length(const char* filename, size_t actual_length, size_t required_length);


#define DEBUG_CHECK_LENGTH(l)   debug_check_length(__PRETTY_FUNCTION__, remainBytes(), l)

ByteReader_T::ByteReader_T(const byte_t* data, size_t size)
    : m_curPos(data)
    , m_begPos(data)
    , m_endPos(data + size)
{}


ByteReader_T::~ByteReader_T()
{}

ByteReader_T& ByteReader_T::reset(){ m_curPos = m_begPos; return *this; }

uint8_t  ByteReader_T::readByte()
{
    DEBUG_CHECK_LENGTH(1);
    
    uint8_t b = *m_curPos ;
    ++m_curPos;
    return b;
}
uint16_t ByteReader_T::readByte2()
{
    DEBUG_CHECK_LENGTH(2);
    
    uint16_t high = readByte();
    uint16_t low = readByte();
    return (high << 8) | (low);
}
uint32_t ByteReader_T::readByte4()
{
    DEBUG_CHECK_LENGTH(4);
    
    uint32_t high = readByte2();
    uint32_t low = readByte2();
    return (high << 16) | (low);
}

uint64_t ByteReader_T::readByte8()
{
    DEBUG_CHECK_LENGTH(8);
    
    uint64_t high = readByte4();
    uint64_t low  = readByte4();
    return (high << 32) | (low);
}
std::string ByteReader_T::readString() {
    size_t len = readByte2();
    return readAsString(len);
}

std::vector<byte_t> ByteReader_T::readAsVector(size_t len)
{
    DEBUG_CHECK_LENGTH(len);
    
    std::vector<byte_t> result(m_curPos, m_curPos + len);
    m_curPos += len;
    return result;
}

std::string ByteReader_T::readAsString(size_t len)
{
    DEBUG_CHECK_LENGTH(len);
    
    std::string result ( (const char*)m_curPos, len); // basic_string( const Char_T* s, size_type count, allocator) constructor.
    m_curPos += len;
    return result;
}

//oson::string_view ByteReader_T::readAsStringView(size_t len)
//{
//    DEBUG_CHECK_LENGTH(len);
//    oson::string_view view((const char*)m_curPos, len);
//    m_curPos += len;
//    return view;
//}

size_t ByteReader_T::remainBytes()const
{
    assert(m_endPos >= m_curPos);
    return m_endPos - m_curPos;
}
//------------------------------------------------------------------------------------------------------------


ByteWriter_T::ByteWriter_T(byte_t* data, size_t size)
: m_curPos(data)
, m_endPos(data + size)
{}

ByteWriter_T::ByteWriter_T(std::vector<byte_t>& buf)
 : m_curPos(buf.data())
 , m_endPos(buf.data() + buf.size())
{}

ByteWriter_T::~ByteWriter_T()
{}
void ByteWriter_T::writeByte(uint8_t value)
{
    DEBUG_CHECK_LENGTH(1);
    *m_curPos ++ = (byte_t)value;
}
void ByteWriter_T::writeByte2(uint16_t value)
{
    DEBUG_CHECK_LENGTH(2);

    uint8_t high = value >> 8;
    uint8_t low  = value & 0xFF;
    writeByte(high);
    writeByte(low);
}
void ByteWriter_T::writeByte4(uint32_t value)
{
    DEBUG_CHECK_LENGTH(4);
    uint16_t high = value >> 16;
    uint16_t low = value & 0xFFFF;
    writeByte2(high);
    writeByte2(low);
}

void ByteWriter_T::writeByte8(uint64_t value)
{
    DEBUG_CHECK_LENGTH(8);
    uint32_t high = value >> 32;
    uint32_t low = value & 0xFFFFFFFF;
    writeByte4(high);
    writeByte4(low);
}

void ByteWriter_T::writeVector( const std::vector<byte_t>& v)
{
    DEBUG_CHECK_LENGTH( (v.size()) );
    memcpy(m_curPos, v.data(), v.size());
    m_curPos += v.size();
}
void ByteWriter_T::writeString( const std::string & v)
{
    DEBUG_CHECK_LENGTH( (v.size()) );
    memcpy(m_curPos,  v.c_str(), v.size());
    m_curPos += v.size();
}

 size_t ByteWriter_T::remainBytes()const
 {
     assert(m_endPos >= m_curPos);
     return m_endPos - m_curPos;
 }
 //------------------------------------------------------------------------------------------------------------
ByteStreamWriter::ByteStreamWriter()
{
    //some little optimization.
    m_buf.reserve( 128 );
}
ByteStreamWriter::~ByteStreamWriter()
{}

const std::vector<byte_t>& ByteStreamWriter::get_buf()const{ return m_buf;}

std::vector<byte_t>& ByteStreamWriter::get_buf(){ return m_buf;}

void ByteStreamWriter::writeByte(uint8_t value)
{
    m_buf.push_back((byte_t)value);
}
void ByteStreamWriter::writeByte2(uint16_t value)
{
    uint8_t high = value >> 8;
    uint8_t low  = value & 0xFF ;
    writeByte(high);
    writeByte(low);
}
void ByteStreamWriter::writeByte4(uint32_t value)
{
    uint16_t high = value >> 16;
    uint16_t low  = value & 0xFFFF;
    writeByte2(high);
    writeByte2(low);
}
void ByteStreamWriter::writeByte8(uint64_t value)
{
    uint32_t high = value >> 32;
    uint32_t low  = value & 0xFFFFFFFF;
    writeByte4(high);
    writeByte4(low);
}

void ByteStreamWriter::writeVector( const std::vector<byte_t>& v)
{
    m_buf.insert(m_buf.end(), v.begin(), v.end());
}
void ByteStreamWriter::writeString( const std::string & v)
{
    m_buf.insert(m_buf.end(), v.begin(), v.end());
}

//void ByteStreamWriter::writeStringView(oson::string_view view)
//{
//    m_buf.insert(m_buf.end(), view.begin(), view.end());
//}

