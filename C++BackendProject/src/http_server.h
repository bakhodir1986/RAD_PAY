


#ifndef OSON_HTTP_SERVER_H_INCLUDED
#define OSON_HTTP_SERVER_H_INCLUDED 1


#include <string>
#include <set>
#include <array>
#include <memory>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace http {
namespace server {


struct header
{
  std::string name;
  std::string value;
};




/// A request received from a client.
struct request
{
  std::string method;
  std::string uri;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
  std::string content;
};




/// A reply to be sent to a client.
struct reply
{
  /// The status of the reply.
  enum status_type
  {
    ok                = 200,
    created           = 201,
    accepted          = 202,
    no_content        = 204,
    multiple_choices  = 300,
    moved_permanently = 301,
    moved_temporarily = 302,
    not_modified      = 304,
    bad_request       = 400,
    unauthorized      = 401,
    forbidden         = 403,
    not_found         = 404,
    internal_server_error = 500,
    not_implemented   = 501,
    bad_gateway       = 502,
    service_unavailable = 503
  } status;

  /// The headers to be included in the reply.
  std::vector< header > headers;

  /// The content to be sent in the reply.
  std::string content;

  /// Convert the reply into a vector of buffers. The buffers do not own the
  /// underlying memory blocks, therefore the reply object must remain valid and
  /// not be changed until the write operation has completed.
  std::vector<boost::asio::const_buffer> to_buffers();

  /// Get a stock reply.
  static reply stock_reply(status_type status);
};



struct request;

/// Parser for incoming requests.
class request_parser
{
public:
  /// Construct ready to parse the request method.
  request_parser();

  /// Reset to initial parser state.
  void reset();

  /// Result of parse.
  enum result_type { good, bad, indeterminate };

  /// Parse some data. The enum return value is good when a complete request has
  /// been parsed, bad if the data is invalid, indeterminate when more data is
  /// required. The InputIterator return value indicates how much of the input
  /// has been consumed.
  template <typename InputIterator>
  std::pair<result_type, InputIterator> parse(request& req,
      InputIterator begin, InputIterator end)
  {
    while (begin != end)
    {
      result_type result = consume(req, *begin++);
      if (result == good || result == bad)
      {
          if ( result == good )
          {
             req.content.assign(begin, end);
          }
        return std::make_pair(result, begin);
      }
    }
    return std::make_pair(indeterminate, begin);
  }

private:
  /// Handle the next character of input.
  result_type consume(request& req, char input);

  /// Check if a byte is an HTTP character.
  static bool is_char(int c);

  /// Check if a byte is an HTTP control character.
  static bool is_ctl(int c);

  /// Check if a byte is defined as an HTTP tspecial character.
  static bool is_tspecial(int c);

  /// Check if a byte is a digit.
  static bool is_digit(int c);

  /// The current state of the parser.
  enum state
  {
    method_start,
    method,
    uri,
    http_version_h,
    http_version_t_1,
    http_version_t_2,
    http_version_p,
    http_version_slash,
    http_version_major_start,
    http_version_major,
    http_version_minor_start,
    http_version_minor,
    expecting_newline_1,
    header_line_start,
    header_lws,
    header_name,
    space_before_header_value,
    header_value,
    expecting_newline_2,
    expecting_newline_3
  } state_;
};


struct reply;
struct request;

/// The common handler for all incoming requests.
class request_handler
{
public:
  request_handler(const request_handler&) = delete;
  request_handler& operator=(const request_handler&) = delete;

  /// Construct with a directory containing files to be served.
  explicit request_handler(const std::string& doc_root);

  /// Handle a request and produce a reply.
  void handle_request(const request& req, reply& rep);

private:
  /// The directory containing the files to be served.
  std::string doc_root_;

  /// Perform URL-decoding on a string. Returns false if the encoding was
  /// invalid.
  static bool url_decode(const std::string& in, std::string& out);
};


typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class connection_manager;
class connection;

typedef std::shared_ptr<connection> connection_ptr;

/// Represents a single connection from a client.
class connection
  : public std::enable_shared_from_this<connection>
{
public:
  connection(const connection&) = delete;
  connection& operator=(const connection&) = delete;

  /// Construct a connection with the given socket.
  explicit connection(
  std::shared_ptr< boost::asio::io_service> io_service,
                  boost::asio::ssl::context & context,
                  connection_manager& manager,
                  request_handler& handler);

  ~connection();
  /// Start the first asynchronous operation for the connection.
  void start();

  
  /// Stop all asynchronous operations associated with the connection.
  void stop();

  ssl_socket::lowest_layer_type& socket();
private:
  
  void handle_handshake(const boost::system::error_code& error);
    /// Perform an asynchronous read operation.
  void do_read();

  void on_read(boost::system::error_code ec, std::size_t bytes_transferred);
  
  /// Perform an asynchronous write operation.
  void do_write();

  void on_write(boost::system::error_code ec, std::size_t);
  
  /// Socket for the connection.
  //boost::asio::stboost::asio::ip::tcp::socket socket_;
    
   ssl_socket socket_;
  
  /// The manager for this connection.
  connection_manager& connection_manager_;

  /// The handler used to process the incoming request.
  request_handler& request_handler_;

  /// Buffer for incoming data.
  std::array<char, 8192> buffer_;

  /// The incoming request.
  request request_;

  /// The parser for the incoming request.
  request_parser request_parser_;

  /// The reply to be sent back to the client.
  reply reply_;
};







/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class connection_manager
{
public:
  connection_manager(const connection_manager&) = delete;
  connection_manager& operator=(const connection_manager&) = delete;

  /// Construct a connection manager.
  connection_manager();

  /// Add the specified connection to the manager and start it.
  void start(connection_ptr c);

  /// Stop the specified connection.
  void stop(connection_ptr c);

  /// Stop all connections.
  void stop_all();

private:
  /// The managed connections.
  std::set<connection_ptr> connections_;
};


struct runtime_option
{
    std::string cert_chain       ;
    std::string private_key_file ;
    std::string dh_file          ;
    std::string password         ;
    std::string ip               ;
    unsigned short  port         ;
    
    std::string doc_root         ;
};

/// The top-level class of the HTTP server.
class server
{
public:
  server(const server&) = delete;
  server& operator=(const server&) = delete;

  /// Construct the server to listen on the specified TCP address and port, and
  /// serve up files from the given directory.
  explicit server( 
    std::shared_ptr< boost::asio::io_service   > io_service,
    const struct runtime_option& opt ) ;


private:
  /// Perform an asynchronous accept operation.
  void do_accept();

  void on_accept( std::shared_ptr< connection > new_connection,  boost::system::error_code ec);
  
  /// Wait for a request to stop the server.
  void do_await_stop();

  std::string get_password()const;
  
  /// The io_service used to perform asynchronous operations.
  std::shared_ptr< boost::asio::io_service > io_service_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  boost::asio::ssl::context  context_;
  
  /// The connection manager which owns all live connections.
  connection_manager connection_manager_;

  /// The handler for all incoming requests.
  request_handler request_handler_;
  
  std::string password_;
};



} // namespace server
} // namespace http









#endif // OSON_HTTP_SERSVER_H_INCLUDED