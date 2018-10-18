/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */



#include "icons.h"

#include "application.h"
#include "DB_T.h"
#include "utils.h"
#include "log.h"


oson::icons::info::info()
    : id( 0 )
    , location()
    , path_hash()
    , kind(0)
    , ts()
    , size(0)
    , sha1_sum()
{}

oson::icons::info::text   oson::icons::info::make_path_hash(const oson::icons::info::text& name ) 
{
    return oson::utils::bin2hex( name )  + ".png";
}



oson::icons::table::table( DB_T& db ) 
    : m_db(db)
{}

oson::icons::table::~table()
{}


std::int64_t   oson::icons::table::add (const oson::icons::info& icon)
{
    SCOPE_LOG(slog);
    std::string query = 
            "INSERT INTO icons (id, location, path_hash, kind, ts, size, sha1_sum) VALUES ( "
            "DEFAULT, "            + 
            escape(icon.location)  + ", " + 
            escape(icon.path_hash) + ", " + 
            escape(icon.kind)      + ", " + 
            escape(icon.ts )       + ", " + 
            escape(icon.size )     + ", " +
            escape(icon.sha1_sum)  + "  " +
            " ) RETURNING id ; " ;

    DB_T::statement st(m_db);

    Error_T ec = Error_OK ;
    st.prepare(query, /*out*/ ec );
    if (ec) return 0;
    std::int64_t id = 0;

    st.row(0) >> id ;
    return id;
}

int  oson::icons::table::edit( std::int64_t id,  const oson::icons::info& icon)
{
    SCOPE_LOG(slog);
    
    std::string query = 
            "UPDATE icons SET "
            "location = "  + escape(icon.location)  + ", "
            "path_hash = " + escape(icon.path_hash) + ", "
            "kind = "      + escape(icon.kind)      + ", "
            "ts = "        + escape(icon.ts )       + ", "
            "size = "      + escape(icon.size)      + ", "
            "sha1_sum = "  + escape(icon.sha1_sum)  + "  "
            "WHERE id = "  + escape(id) ;
    
    DB_T::statement st(m_db);
    
    st.prepare(query);
    
    return st.affected_rows();
}

oson::icons::info   oson::icons::table::get( std::int64_t id ) 
{
    SCOPE_LOG(slog);
    std::string query = "SELECT id, location, path_hash, kind, ts, size, sha1_sum FROM icons WHERE id = " + escape(id);
    
    DB_T::statement st(m_db);
    st.prepare(query);
    
    oson::icons::info info;
    
    if (st.rows_count() == 1 ) {
        st.row(0) >> info.id >> info.location >> info.path_hash >> info.kind >> info.ts >> info.size >> info.sha1_sum ;
    } else {
        slog.WarningLog("Can't find icon!");
    }
    return info;
}

int  oson::icons::table::del(std::int64_t id)
{
    SCOPE_LOG(slog);
    std::string query = "DELETE FROM icons WHERE id = " + escape(id);

    DB_T::statement st(m_db);
    
    Error_T ec = Error_OK ;
    st.prepare(query, ec );
    
    if (ec) return 0;
    
    return st.affected_rows();
}

    
 


oson::icons::manager::manager()
{}

oson::icons::manager::~manager()
{}

oson::icons::info oson::icons::manager::save_icon(const content& icon_content,  Kind kind , std::int64_t old_icon_id  ) 
{
    SCOPE_LOG(slog);

    /***3. make info and save it to db*/
    info res;
    
    res.id        = 0;
    res.kind      = static_cast< info::integer > (kind);
    //res.location  = location;
    res.ts        = formatted_time_now_iso_S();
    //res.path_hash = info::make_path_hash(file_name);
    res.size      = icon_content.image.size();
    res.sha1_sum  = oson::utils::sha1_hash(icon_content.image);
    
    if (res.size == 0 ) // an empty
    {
        slog.ErrorLog("an empty image!");
        return res;
    }
    
    oson::icons::table table( oson_this_db ) ;
    
    if ( old_icon_id != 0 ) {
        info old_res = table.get(old_icon_id);
        
        
        const bool identical_res = (old_res.id  != 0 ) && (old_res.kind == res.kind ) && (old_res.sha1_sum == res.sha1_sum);
        
        if (identical_res ) {
            if (  oson::utils::file_exists(old_res.location) )
            {
                return old_res;//there nobody are changed.
            }
        }
    }
    
    res.id = table.add(res);
    
    if ( 0 == res.id ){
        return info();//en empty.
    }

    
    /***1. generate non-exists file-name*/
    std::string file_name, location;
    
    file_name = to_str( res.id ) + "_" + oson::utils::sha1_hash(to_str(res.id));
    
    location = "/etc/oson/img/" + file_name + ".png";
    
    /***2. save image to file*/
    FILE * file = fopen(location.c_str(), "wb"); // open a binary format
    if (NULL == file)
        return info();//empty info
        
    fwrite(icon_content.image.c_str(), 1/*sizeof char*/, icon_content.image.size(), file ) ; 
    fclose(file);
    
    
    /****2.b  copy it to www also*/
    std::string www_location = "/var/www/oson.client/img/" + file_name + ".png";
    file = fopen(www_location.c_str(), "wb");
    if (NULL != file ){
        fwrite(icon_content.image.c_str(), 1 /*sizeof char*/ , icon_content.image.size(), file );
        fclose(file);
    }
    
    
    /**3. edit  */
    res.location   = location;
    res.path_hash = info::make_path_hash(file_name);
    
    table.edit(res.id, res);
    
    return res;
}

int oson::icons::manager::remove_icon(std::int64_t icon_id ) 
{
    SCOPE_LOG(slog);
    
    if ( ! icon_id ) 
        return 0;
    
    oson::icons::table table(oson_this_db);
    struct oson::icons::info info = table.get(icon_id);
    
    if ( ! info.id ) 
        return 0;
    
    table.del(icon_id);
    
    ::remove(info.location.c_str());
    {
        char sep = '/';
        size_t sep_pos = info.location.rfind( sep );
        if (sep_pos != std::string::npos )
        {
            std::string filename = info.location.substr( sep_pos + 1 );
            std::string www_location = "/var/www/oson.client/img/"+filename;
            ::remove(www_location.c_str());
        }
    }
    return 1;
}


int oson::icons::manager::load_icon(std::int64_t icon_id, content& icon_content)
{
    SCOPE_LOG(slog);
    
    if ( ! icon_id ) {
        slog.WarningLog("icon_id is zero!");
        return 0;
    }
    
    oson::icons::table table(oson_this_db);
    struct oson::icons::info info = table.get(icon_id);
    if ( ! info.id ) {
        //need't write a log, because oson::icons::table::get will write warning log.
        return 0;
    }
    
    if (! oson::utils::file_exists(info.location))
    {
        slog.WarningLog("File not found: %.*s", ::std::min<int>(1024, info.location.size()), info.location.c_str());
        return 0;
    }
    FILE * file = fopen(info.location.c_str(), "rb");
    if ( NULL == file ){
        slog.WarningLog("file can't open: %.*s", ::std::min<int>(1024, info.location.size()), info.location.c_str() );
        return 0;
    }
    
    char buf[512];
    int nbuf;
    size_t const LIMIT_ICON_SIZE = 1 << 20 ;//1 MB
    while( true )
    {
        nbuf = fread(buf, 1, sizeof(buf) , file);
        if ( ! nbuf ) 
            break;
        
        icon_content.image.append(buf, nbuf);
        
        if (icon_content.image.size() > LIMIT_ICON_SIZE )
        {
            slog.WarningLog("The file size limit exceeded. limit: %zu", LIMIT_ICON_SIZE);
            break;
        }
    }
    
    fclose(file);
     
    return 1;
    
}
