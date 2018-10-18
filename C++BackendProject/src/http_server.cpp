
#include <signal.h>
#include <utility>
#include <vector>

#include <fstream>
#include <sstream>
#include <string>


#include "http_server.h"

#include "log.h"

namespace sr = http::server;

namespace //noname namespace has internal linkage
{ 
    
namespace mime_types
{

struct mapping
{
  const char* extension;
  const char* mime_type;
} mappings[] =
{
  { "gif", "image/gif" },
  { "htm", "text/html" },
  { "html", "text/html" },
  { "jpg", "image/jpeg" },
  { "png", "image/png" }
};

static std::string extension_to_type(const std::string& extension)
{
  for (mapping m: mappings)
  {
    if (m.extension == extension)
    {
      return m.mime_type;
    }
  }

  return "text/plain";
}

} // mime_types
} // unname namespace
 



sr::request_parser::request_parser()
  : state_(method_start)
{
}

void sr::request_parser::reset()
{
  state_ = method_start;
}

sr::request_parser::result_type sr::request_parser::consume(request& req, char input)
{
 //  'GET <path> HTTP/1.0\r\n<headers>\r\n\r\n<content>
  switch (state_)
  {
  case method_start:
    if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      state_ = method;
      req.method.push_back(input);
      return indeterminate;
    }
  case method:
    if (input == ' ')
    {
      state_ = uri;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.method.push_back(input);
      return indeterminate;
    }
  case uri:
    if (input == ' ')
    {
      state_ = http_version_h;
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      req.uri.push_back(input);
      return indeterminate;
    }
  case http_version_h:
    if (input == 'H')
    {
      state_ = http_version_t_1;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_t_1:
    if (input == 'T')
    {
      state_ = http_version_t_2;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_t_2:
    if (input == 'T')
    {
      state_ = http_version_p;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_p:
    if (input == 'P')
    {
      state_ = http_version_slash;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_slash:
    if (input == '/')
    {
      req.http_version_major = 0;
      req.http_version_minor = 0;
      state_ = http_version_major_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_major_start:
    if (is_digit(input))
    {
      req.http_version_major = req.http_version_major * 10 + input - '0';
      state_ = http_version_major;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_major:
    if (input == '.')
    {
      state_ = http_version_minor_start;
      return indeterminate;
    }
    else if (is_digit(input))
    {
      req.http_version_major = req.http_version_major * 10 + input - '0';
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_minor_start:
    if (is_digit(input))
    {
      req.http_version_minor = req.http_version_minor * 10 + input - '0';
      state_ = http_version_minor;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case http_version_minor:
    if (input == '\r')
    {
      state_ = expecting_newline_1;
      return indeterminate;
    }
    else if (is_digit(input))
    {
      req.http_version_minor = req.http_version_minor * 10 + input - '0';
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case expecting_newline_1:
    if (input == '\n')
    {
      state_ = header_line_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case header_line_start:
    if (input == '\r')
    {
      state_ = expecting_newline_3;
      return indeterminate;
    }
    else if (!req.headers.empty() && (input == ' ' || input == '\t'))
    {
      state_ = header_lws;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.headers.push_back(header());
      req.headers.back().name.push_back(input);
      state_ = header_name;
      return indeterminate;
    }
  case header_lws:
    if (input == '\r')
    {
      state_ = expecting_newline_2;
      return indeterminate;
    }
    else if (input == ' ' || input == '\t')
    {
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      state_ = header_value;
      req.headers.back().value.push_back(input);
      return indeterminate;
    }
  case header_name:
    if (input == ':')
    {
      state_ = space_before_header_value;
      return indeterminate;
    }
    else if (!is_char(input) || is_ctl(input) || is_tspecial(input))
    {
      return bad;
    }
    else
    {
      req.headers.back().name.push_back(input);
      return indeterminate;
    }
  case space_before_header_value:
    if (input == ' ')
    {
      state_ = header_value;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case header_value:
    if (input == '\r')
    {
      state_ = expecting_newline_2;
      return indeterminate;
    }
    else if (is_ctl(input))
    {
      return bad;
    }
    else
    {
      req.headers.back().value.push_back(input);
      return indeterminate;
    }
  case expecting_newline_2:
    if (input == '\n')
    {
      state_ = header_line_start;
      return indeterminate;
    }
    else
    {
      return bad;
    }
  case expecting_newline_3:
    return (input == '\n') ? good : bad;
  default:
    return bad;
  }
}

bool sr::request_parser::is_char(int c)
{
  return c >= 0 && c <= 127;
}

bool sr::request_parser::is_ctl(int c)
{
  return (c >= 0 && c <= 31) || (c == 127);
}

bool sr::request_parser::is_tspecial(int c)
{
  switch (c)
  {
  case '(': case ')': case '<': case '>': case '@':
  case ',': case ';': case ':': case '\\': case '"':
  case '/': case '[': case ']': case '?': case '=':
  case '{': case '}': case ' ': case '\t':
    return true;
  default:
    return false;
  }
}

bool sr::request_parser::is_digit(int c)
{
  return c >= '0' && c <= '9';
}
 





sr::request_handler::request_handler(const std::string& doc_root)
  : doc_root_(doc_root)
{
}

void sr::request_handler::handle_request(const request& req, reply& rep)
{
    SCOPE_LOGD(slog);
    
    slog.InfoLog("req{ method: '%s'  uri: '%s', HTTP version: '%d/%d'", req.method.c_str(), req.uri.c_str(), req.http_version_minor, req.http_version_major) ;
    
    for( const auto& h : req.headers)
    {
        slog.InfoLog("header: {  %s: %s } ", h.name.c_str(), h.value.c_str());
    }
    
    slog.InfoLog("req content: %s", req.content.c_str());
    
  // Decode url to path.
  std::string request_path;
  if (!url_decode(req.uri, request_path))
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // Request path must be absolute and not contain "..".
  if (request_path.empty() || request_path[0] != '/'
      || request_path.find("..") != std::string::npos)
  {
    rep = reply::stock_reply(reply::bad_request);
    return;
  }

  // If path ends in slash (i.e. is a directory) then add "index.html".
  if (request_path[request_path.size() - 1] == '/')
  {
    request_path += "index.html";
  }

  // Determine the file extension.
  std::size_t last_slash_pos = request_path.find_last_of("/");
  std::size_t last_dot_pos = request_path.find_last_of(".");
  std::string extension;
  if (last_dot_pos != std::string::npos && last_dot_pos > last_slash_pos)
  {
    extension = request_path.substr(last_dot_pos + 1);
  }

  // Open the file to send back.
  std::string full_path = doc_root_ + request_path;
  std::ifstream is(full_path.c_str(), std::ios::in | std::ios::binary);
  if (!is)
  {
    rep = reply::stock_reply(reply::not_found);
    return;
  }

  // Fill out the reply to be sent to the client.
  rep.status = reply::ok;
  char buf[512];
  while (is.read(buf, sizeof(buf)).gcount() > 0)
    rep.content.append(buf, is.gcount());
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = std::to_string(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = mime_types::extension_to_type(extension);
}

bool sr::request_handler::url_decode(const std::string& in, std::string& out)
{
  out.clear();
  out.reserve(in.size());
  for (std::size_t i = 0; i < in.size(); ++i)
  {
    if (in[i] == '%')
    {
      if (i + 3 <= in.size())
      {
        int value = 0;
        //%20  or %37
        int la = in[ i + 1 ], lb = in[ i + 2 ] ;
        
        if ( la >= '0' && la <= '9' ) 
            value = la -'0';
        else if (la >= 'a' && la <= 'f' )
            value = la -'a' + 10;
        else if (la >='A' && la <= 'F' ) 
            value = la - 'A' + 10 ;
        else
            return false;
        
        if ( lb >= '0' && lb <= '9' ) 
            value = value * 16 + (lb -'0' ) ;
        else if ( lb >='a' && lb <= 'f' ) 
            value = value * 16 + (lb - 'a' + 10);
        else if(lb >= 'A' && lb <= 'F' )
            value = value * 16 + (lb - 'A' + 10 ) ;
        else
            return false;
        
        out += static_cast< char >( value ) ;
        i += 2;
      }
      else
      {
        return false;
      }
    }
    else if (in[i] == '+')
    {
      out += ' ';
    }
    else
    {
      out += in[i];
    }
  }
  return true;
}

namespace { //unname namespace
    
namespace status_strings {

const std::string ok =
  "HTTP/1.0 200 OK\r\n";
const std::string created =
  "HTTP/1.0 201 Created\r\n";
const std::string accepted =
  "HTTP/1.0 202 Accepted\r\n";
const std::string no_content =
  "HTTP/1.0 204 No Content\r\n";
const std::string multiple_choices =
  "HTTP/1.0 300 Multiple Choices\r\n";
const std::string moved_permanently =
  "HTTP/1.0 301 Moved Permanently\r\n";
const std::string moved_temporarily =
  "HTTP/1.0 302 Moved Temporarily\r\n";
const std::string not_modified =
  "HTTP/1.0 304 Not Modified\r\n";
const std::string bad_request =
  "HTTP/1.0 400 Bad Request\r\n";
const std::string unauthorized =
  "HTTP/1.0 401 Unauthorized\r\n";
const std::string forbidden =
  "HTTP/1.0 403 Forbidden\r\n";
const std::string not_found =
  "HTTP/1.0 404 Not Found\r\n";
const std::string internal_server_error =
  "HTTP/1.0 500 Internal Server Error\r\n";
const std::string not_implemented =
  "HTTP/1.0 501 Not Implemented\r\n";
const std::string bad_gateway =
  "HTTP/1.0 502 Bad Gateway\r\n";
const std::string service_unavailable =
  "HTTP/1.0 503 Service Unavailable\r\n";

static boost::asio::const_buffer to_buffer( sr::reply::status_type status)
{
  using sr::reply;
    
  switch (status)
  {
  case reply::ok:
    return boost::asio::buffer(ok);
  case reply::created:
    return boost::asio::buffer(created);
  case reply::accepted:
    return boost::asio::buffer(accepted);
  case reply::no_content:
    return boost::asio::buffer(no_content);
  case reply::multiple_choices:
    return boost::asio::buffer(multiple_choices);
  case reply::moved_permanently:
    return boost::asio::buffer(moved_permanently);
  case reply::moved_temporarily:
    return boost::asio::buffer(moved_temporarily);
  case reply::not_modified:
    return boost::asio::buffer(not_modified);
  case reply::bad_request:
    return boost::asio::buffer(bad_request);
  case reply::unauthorized:
    return boost::asio::buffer(unauthorized);
  case reply::forbidden:
    return boost::asio::buffer(forbidden);
  case reply::not_found:
    return boost::asio::buffer(not_found);
  case reply::internal_server_error:
    return boost::asio::buffer(internal_server_error);
  case reply::not_implemented:
    return boost::asio::buffer(not_implemented);
  case reply::bad_gateway:
    return boost::asio::buffer(bad_gateway);
  case reply::service_unavailable:
    return boost::asio::buffer(service_unavailable);
  default:
    return boost::asio::buffer(internal_server_error);
  }
}

} // namespace status_strings

namespace misc_strings {

const char name_value_separator[] = { ':', ' ' };
const char crlf[] = { '\r', '\n' };

} // namespace misc_strings
} // end unname namespace

std::vector<boost::asio::const_buffer>  sr::reply::to_buffers()
{
  std::vector<boost::asio::const_buffer> buffers;
  buffers.push_back( status_strings::to_buffer(status));
  for (std::size_t i = 0; i < headers.size(); ++i)
  {
    header& h = headers[i];
    buffers.push_back(boost::asio::buffer(h.name));
    buffers.push_back(boost::asio::buffer(misc_strings::name_value_separator));
    buffers.push_back(boost::asio::buffer(h.value));
    buffers.push_back(boost::asio::buffer(misc_strings::crlf));
  }
  buffers.push_back(boost::asio::buffer(misc_strings::crlf));
  buffers.push_back(boost::asio::buffer(content));
  return buffers;
}

namespace { // unname namespace
namespace stock_replies {

const char ok[] = "";
const char created[] =
  "<html>"
  "<head><title>Created</title></head>"
  "<body><h1>201 Created</h1></body>"
  "</html>";
const char accepted[] =
  "<html>"
  "<head><title>Accepted</title></head>"
  "<body><h1>202 Accepted</h1></body>"
  "</html>";
const char no_content[] =
  "<html>"
  "<head><title>No Content</title></head>"
  "<body><h1>204 Content</h1></body>"
  "</html>";
const char multiple_choices[] =
  "<html>"
  "<head><title>Multiple Choices</title></head>"
  "<body><h1>300 Multiple Choices</h1></body>"
  "</html>";
const char moved_permanently[] =
  "<html>"
  "<head><title>Moved Permanently</title></head>"
  "<body><h1>301 Moved Permanently</h1></body>"
  "</html>";
const char moved_temporarily[] =
  "<html>"
  "<head><title>Moved Temporarily</title></head>"
  "<body><h1>302 Moved Temporarily</h1></body>"
  "</html>";
const char not_modified[] =
  "<html>"
  "<head><title>Not Modified</title></head>"
  "<body><h1>304 Not Modified</h1></body>"
  "</html>";
const char bad_request[] =
  "<html>"
  "<head><title>Bad Request</title></head>"
  "<body><h1>400 Bad Request</h1></body>"
  "</html>";
const char unauthorized[] =
  "<html>"
  "<head><title>Unauthorized</title></head>"
  "<body><h1>401 Unauthorized</h1></body>"
  "</html>";
const char forbidden[] =
  "<html>"
  "<head><title>Forbidden</title></head>"
  "<body><h1>403 Forbidden</h1></body>"
  "</html>";
const char not_found[] =
  "<html>"
  "<head><title>Not Found</title></head>"
  "<body><h1>404 Not Found</h1></body>"
  "</html>";
const char internal_server_error[] =
  "<html>"
  "<head><title>Internal Server Error</title></head>"
  "<body><h1>500 Internal Server Error</h1></body>"
  "</html>";
const char not_implemented[] =
  "<html>"
  "<head><title>Not Implemented</title></head>"
  "<body><h1>501 Not Implemented</h1></body>"
  "</html>";
const char bad_gateway[] =
  "<html>"
  "<head><title>Bad Gateway</title></head>"
  "<body><h1>502 Bad Gateway</h1></body>"
  "</html>";
const char service_unavailable[] =
  "<html>"
  "<head><title>Service Unavailable</title></head>"
  "<body><h1>503 Service Unavailable</h1></body>"
  "</html>";

static std::string to_string( sr::reply::status_type status)
{
  using sr::reply;
    
  switch (status)
  {
  case reply::ok:
    return ok;
  case reply::created:
    return created;
  case reply::accepted:
    return accepted;
  case reply::no_content:
    return no_content;
  case reply::multiple_choices:
    return multiple_choices;
  case reply::moved_permanently:
    return moved_permanently;
  case reply::moved_temporarily:
    return moved_temporarily;
  case reply::not_modified:
    return not_modified;
  case reply::bad_request:
    return bad_request;
  case reply::unauthorized:
    return unauthorized;
  case reply::forbidden:
    return forbidden;
  case reply::not_found:
    return not_found;
  case reply::internal_server_error:
    return internal_server_error;
  case reply::not_implemented:
    return not_implemented;
  case reply::bad_gateway:
    return bad_gateway;
  case reply::service_unavailable:
    return service_unavailable;
  default:
    return internal_server_error;
  }
}

} // namespace stock_replies

} // unname namespace 



sr::reply sr::reply::stock_reply(reply::status_type status)
{
  reply rep;
  rep.status = status;
  rep.content = stock_replies::to_string(status);
  rep.headers.resize(2);
  rep.headers[0].name = "Content-Length";
  rep.headers[0].value = std::to_string(rep.content.size());
  rep.headers[1].name = "Content-Type";
  rep.headers[1].value = "text/html";
  return rep;
}


sr::connection::connection( 
                  std::shared_ptr< boost::asio::io_service> io_service,
                  boost::asio::ssl::context & context,
                  connection_manager& manager,
                  request_handler& handler
  )
  : socket_( *io_service, context  ),
    connection_manager_(manager),
    request_handler_(handler)
{
    SCOPE_LOGD(slog);
}

sr::connection::~connection()
{
    SCOPE_LOGD(slog);
}

void sr::connection::start()
{
    SCOPE_LOGD(slog);
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
        std::bind(&connection::handle_handshake, shared_from_this(),
          std::placeholders::_1  ) );
}
 
void sr::connection::handle_handshake(const boost::system::error_code& ec )
{
    SCOPE_LOGD(slog);
    if (!ec)
    {
        do_read();
    }  else {
        slog.ErrorLog("ec : %d  message: %s", ec.value(), ec.message().c_str());
    }
}

void sr::connection::stop()
{
    SCOPE_LOGD(slog);
    socket().close();
}

sr::ssl_socket::lowest_layer_type&  sr::connection::socket()
{
    return socket_.lowest_layer() ;
}

void sr::connection::on_read(boost::system::error_code ec, std::size_t bytes_transferred)
{
    SCOPE_LOGD(slog);
    if (!ec)
    {
        slog.WriteArray(LogLevel_Info, "babang", (unsigned char*)( buffer_.data() ), bytes_transferred ) ;
        request_parser::result_type result;
        std::tie(result, std::ignore) = request_parser_.parse(
            request_, buffer_.data(), buffer_.data() + bytes_transferred);
        
            
        if (result == request_parser::good)
        {
            slog.InfoLog("result is good parsed!");
            request_handler_.handle_request(request_, reply_);
            do_write();
        }
        else if (result == request_parser::bad)
        {
            slog.WarningLog("result is bad parsed.");
            reply_ = reply::stock_reply(reply::bad_request);
            do_write();
        }
        else
        {
            slog.DebugLog("continue read...");
           do_read();
        }
    }
    else if (ec != boost::asio::error::operation_aborted)
    {
        slog.ErrorLog("ec.value: %d, message: %s", ec.value(), ec.message().c_str());
        connection_manager_.stop(shared_from_this());
    } else {
        slog.ErrorLog("operation aborted!");
    }
}
  
void sr::connection::do_read()
{
   SCOPE_LOGD(slog);
    auto self =  shared_from_this() ;
    socket_.async_read_some(boost::asio::buffer(buffer_), std::bind(&connection::on_read, self, std::placeholders::_1, std::placeholders::_2) ) ; 
}


void sr::connection::on_write(boost::system::error_code ec, std::size_t)
{
    SCOPE_LOGD(slog);
    if (!ec)
    {
      // Initiate graceful connection closure.
//      boost::system::error_code ignored_ec;
//      socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
//        ignored_ec);
    }
    else
    if (ec != boost::asio::error::operation_aborted)
    {
        slog.ErrorLog("ec.value: %d, message: %s", ec.value(), ec.message().c_str());
      connection_manager_.stop( shared_from_this() ) ;
    } else {
        slog.ErrorLog("Operation aborted!");
    }

}
void sr::connection::do_write()
{
    SCOPE_LOGD(slog);
  auto self =   shared_from_this() ;
  
  boost::asio::async_write( socket_, reply_.to_buffers(), std::bind( & connection::on_write, self, std::placeholders::_1, std::placeholders::_2 ) ) ;
}
  
sr::connection_manager::connection_manager()
{
}

void sr::connection_manager::start(connection_ptr c)
{
    SCOPE_LOGD(slog);
  connections_.insert(c);
  c->start();
}

void sr::connection_manager::stop(connection_ptr c)
{
    SCOPE_LOGD(slog);
  connections_.erase(c);
  c->stop();
}

void sr::connection_manager::stop_all()
{
  for (auto c: connections_)
    c->stop();
  connections_.clear();
}
 




sr::server::server( 
    std::shared_ptr< boost::asio::io_service   > io_service,
    const struct runtime_option& opt )
  : io_service_(io_service),
    acceptor_( *io_service,
                boost::asio::ip::tcp::endpoint(
                boost::asio::ip::tcp::v6() , opt.port ) ),
    
    context_( boost::asio::ssl::context::sslv23 ),    
    connection_manager_(),

    request_handler_(opt.doc_root)
{
    SCOPE_LOGD(slog);
    
    password_ = opt.password; 
    
    context_.set_options(
          boost::asio::ssl::context::default_workarounds
        | boost::asio::ssl::context::no_sslv2
        | boost::asio::ssl::context::single_dh_use);
    
    
    context_.set_password_callback( std::bind(&sr::server::get_password , this ) );
    
    context_.use_certificate_chain_file( opt.cert_chain );
    context_.use_private_key_file( opt.private_key_file, boost::asio::ssl::context::pem);
    context_.use_tmp_dh_file( opt.dh_file );

  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
//  boost::asio::ip::tcp::resolver resolver( *io_service_);
//  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({opt.ip , std::to_string(opt.port) });
//  acceptor_.open(endpoint.protocol());
//  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
//  acceptor_.bind(endpoint);
//  acceptor_.listen();

  do_accept();
}
 std::string sr::server::get_password()const{ return password_; } 
  
 void sr::server::on_accept(std::shared_ptr< connection > new_connection, boost::system::error_code ec)
 {
     SCOPE_LOGD(slog);
    // Check whether the server was stopped by a signal before this
    // completion handler had a chance to run.
    if (!acceptor_.is_open())
    {
      return;
    }

    if (!ec)
    {
       connection_manager_.start( new_connection );
    }

    do_accept();

 }
void sr::server::do_accept()
{
    SCOPE_LOGD(slog);
    
    auto new_connection = std::make_shared< connection> ( io_service_, std::ref(context_), std::ref( connection_manager_), std::ref(request_handler_) ) ;
    
    acceptor_.async_accept( new_connection->socket(), std::bind(&server::on_accept, this, new_connection, std::placeholders::_1 ) ) ; 
}

void sr::server::do_await_stop()
{
//  signals_.async_wait(
//      [this](boost::system::error_code /*ec*/, int /*signo*/)
//      {
//        // The server is stopped by cancelling all outstanding asynchronous
//        // operations. Once all operations have finished the io_service::run()
//        // call will exit.
//        acceptor_.close();
//        connection_manager_.stop_all();
//      });
}
 