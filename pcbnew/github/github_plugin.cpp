/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 SoftPLC Corporation, Dick Hollenbeck <dick@softplc.com>
 * Copyright (C) 2013 KiCad Developers, see CHANGELOG.TXT for contributors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */


/*
*/


#ifndef WIN32_LEAN_AND_MEAN
// when WIN32_LEAN_AND_MEAN is defined, some useless includes in <window.h>
// are skipped, and this avoid some compil issues
#define WIN32_LEAN_AND_MEAN
#endif

#ifdef WIN32
 // defines needed by avhttp
 // Minimal Windows version is XP: Google for _WIN32_WINNT
 #define _WIN32_WINNT   0x0501
 #define WINVER         0x0501
#endif

#include <sstream>
#include <boost/ptr_container/ptr_map.hpp>
#include <set>

#include <wx/zipstrm.h>
#include <wx/mstream.h>
#include <wx/uri.h>

#include <fctsys.h>
// Under Windows Mingw/msys, avhttp.hpp should be included after fctsys.h
// in fact after wx/wx.h, included by fctsys.h,
// to avoid issues (perhaps due to incompatible defines)
#include <avhttp.hpp>                       // chinese SSL magic

#include <io_mgr.h>
#include <richio.h>
#include <pcb_parser.h>
#include <class_board.h>
#include <github_plugin.h>
#include <class_module.h>
#include <macros.h>
#include <fp_lib_table.h>       // ExpandSubstitutions()

using namespace std;


static const char* PRETTY_DIR = "allow_pretty_writing_to_this_dir";


typedef boost::ptr_map<string, wxZipEntry>  MODULE_MAP;
typedef MODULE_MAP::iterator                MODULE_ITER;
typedef MODULE_MAP::const_iterator          MODULE_CITER;


/**
 * Class GH_CACHE
 * assists only within GITHUB_PLUGIN and holds a map of footprint name to wxZipEntry
 */
struct GH_CACHE : public MODULE_MAP
{
    // MODULE_MAP is a boost::ptr_map template, made into a class hereby.
};


GITHUB_PLUGIN::GITHUB_PLUGIN() :
    PCB_IO(),
    m_gh_cache( 0 )
{
}


GITHUB_PLUGIN::~GITHUB_PLUGIN()
{
    delete m_gh_cache;
}


const wxString GITHUB_PLUGIN::PluginName() const
{
    return wxT( "Github" );
}


const wxString GITHUB_PLUGIN::GetFileExtension() const
{
    return wxEmptyString;
}


wxArrayString GITHUB_PLUGIN::FootprintEnumerate(
        const wxString& aLibraryPath, const PROPERTIES* aProperties )
{
    //D(printf("%s: this:%p  aLibraryPath:'%s'\n", __func__, this, TO_UTF8(aLibraryPath) );)
    cacheLib( aLibraryPath, aProperties );

    typedef std::set<wxString>      MYSET;

    MYSET   unique;

    if( m_pretty_dir.size() )
    {
        wxArrayString locals = PCB_IO::FootprintEnumerate( m_pretty_dir );

        for( unsigned i=0; i<locals.GetCount();  ++i )
            unique.insert( locals[i] );
    }

    for( MODULE_ITER it = m_gh_cache->begin();  it!=m_gh_cache->end();  ++it )
    {
        unique.insert( FROM_UTF8( it->first.c_str() ) );
    }

    wxArrayString ret;

    for( MYSET::const_iterator it = unique.begin();  it != unique.end();  ++it )
    {
        ret.Add( *it );
    }

    return ret;
}


MODULE* GITHUB_PLUGIN::FootprintLoad( const wxString& aLibraryPath,
        const wxString& aFootprintName, const PROPERTIES* aProperties )
{
    // D(printf("%s: this:%p  aLibraryPath:'%s'\n", __func__, this, TO_UTF8(aLibraryPath) );)

    // clear or set to valid the variable m_pretty_dir
    cacheLib( aLibraryPath, aProperties );

    if( m_pretty_dir.size() )
    {
        // API has FootprintLoad() *not* throwing an exception if footprint not found.
        MODULE* local = PCB_IO::FootprintLoad( m_pretty_dir, aFootprintName, aProperties );

        if( local )
            return local;
    }


    string fp_name = TO_UTF8( aFootprintName );

    MODULE_CITER it = m_gh_cache->find( fp_name );

    if( it != m_gh_cache->end() )  // fp_name is present
    {
        wxMemoryInputStream mis( &m_zip_image[0], m_zip_image.size() );

        // This decoder should always be UTF8, since it was saved that way by git.
        // That is, since pretty footprints are UTF8, and they were pushed to the
        // github repo, they are still UTF8.
        wxZipInputStream    zis( mis, wxConvUTF8 );
        wxZipEntry*         entry = (wxZipEntry*) it->second;   // remove "const"-ness

        if( zis.OpenEntry( *entry ) )
        {
            INPUTSTREAM_LINE_READER reader( &zis );
            PCB_PARSER              parser( &reader );

            MODULE* ret = (MODULE*) parser.Parse();

            // Dude, the footprint name comes from the file name in
            // a github library.  Zero out the library name, we don't know it here.
            // Caller always has to set the library nickname if it knows it.
            ret->SetFPID( fp_name );

            return ret;
        }
    }

    return NULL;    // this API function returns NULL for "not found", per spec.
}


bool GITHUB_PLUGIN::IsFootprintLibWritable( const wxString& aLibraryPath )
{
    if( m_pretty_dir.size() )
        return PCB_IO::IsFootprintLibWritable( m_pretty_dir );
    else
        return false;
}


void GITHUB_PLUGIN::FootprintSave( const wxString& aLibraryPath,
        const MODULE* aFootprint, const PROPERTIES* aProperties )
{
    // set m_pretty_dir to either empty or something in aProperties
    cacheLib( aLibraryPath, aProperties );

    if( GITHUB_PLUGIN::IsFootprintLibWritable( aLibraryPath ) )
    {
        PCB_IO::FootprintSave( m_pretty_dir, aFootprint, aProperties );
    }
    else
    {
        // This typically will not happen if the caller first properly calls
        // IsFootprintLibWritable() to determine if calling FootprintSave() is
        // even legal, so I spend no time on internationalization here:

        string msg = StrPrintf( "Github library\n'%s'\nis only writable if you set option '%s' in Library Tables dialog.",
                (const char*) TO_UTF8( aLibraryPath ), PRETTY_DIR );

        THROW_IO_ERROR( msg );
    }
}


void GITHUB_PLUGIN::FootprintDelete( const wxString& aLibraryPath, const wxString& aFootprintName,
        const PROPERTIES* aProperties )
{
    // set m_pretty_dir to either empty or something in aProperties
    cacheLib( aLibraryPath, aProperties );

    if( GITHUB_PLUGIN::IsFootprintLibWritable( aLibraryPath ) )
    {
        // Does the PCB_IO base class have this footprint?
        // We cannot write to github.

        wxArrayString pretties = PCB_IO::FootprintEnumerate( m_pretty_dir, aProperties );

        if( pretties.Index( aFootprintName ) != wxNOT_FOUND )
        {
            PCB_IO::FootprintDelete( m_pretty_dir, aFootprintName, aProperties );
        }
        else
        {
            wxString msg = wxString::Format(
                    _( "Footprint\n'%s'\nis not in the writable portion of this Github library\n'%s'" ),
                    GetChars( aFootprintName ),
                    GetChars( aLibraryPath )
                    );

            THROW_IO_ERROR( msg );
        }
    }
    else
    {
        // This typically will not happen if the caller first properly calls
        // IsFootprintLibWritable() to determine if calling FootprintSave() is
        // even legal, so I spend no time on internationalization here:

        string msg = StrPrintf( "Github library\n'%s'\nis only writable if you set option '%s' in Library Tables dialog.",
                (const char*) TO_UTF8( aLibraryPath ), PRETTY_DIR );

        THROW_IO_ERROR( msg );
    }
}


void GITHUB_PLUGIN::FootprintLibCreate( const wxString& aLibraryPath, const PROPERTIES* aProperties )
{
    // set m_pretty_dir to either empty or something in aProperties
    cacheLib( aLibraryPath, aProperties );

    if( m_pretty_dir.size() )
    {
        PCB_IO::FootprintLibCreate( m_pretty_dir, aProperties );
    }
    else
    {
        // THROW_IO_ERROR()   @todo
    }
}


bool GITHUB_PLUGIN::FootprintLibDelete( const wxString& aLibraryPath, const PROPERTIES* aProperties )
{
    // set m_pretty_dir to either empty or something in aProperties
    cacheLib( aLibraryPath, aProperties );

    if( m_pretty_dir.size() )
    {
        return PCB_IO::FootprintLibDelete( m_pretty_dir, aProperties );
    }
    else
    {
        // THROW_IO_ERROR()   @todo
        return false;
    }
}


void GITHUB_PLUGIN::FootprintLibOptions( PROPERTIES* aListToAppendTo ) const
{
    // inherit options supported by all PLUGINs.
    PLUGIN::FootprintLibOptions( aListToAppendTo );

    (*aListToAppendTo)[ PRETTY_DIR ] = wxString( _(
        "Set this property to a directory where footprints are to be written as pretty "
        "footprints when saving to this library. Anything saved will take precedence over "
        "footprints by the same name in the github repo.  These saved footprints can then "
        "be sent to the library maintainer as updates. "
        "<p>The directory <b>must</b> have a <b>.pretty</b> file extension because the "
        "format of the save is pretty.</p>"
        )).utf8_str();

    /*
    (*aListToAppendTo)["cache_github_zip_in_this_dir"] = wxString( _(
        "Set this property to a directory where the github *.zip file will be cached. "
        "This should speed up subsequent visits to this library."
        )).utf8_str();
    */
}


void GITHUB_PLUGIN::cacheLib( const wxString& aLibraryPath, const PROPERTIES* aProperties ) throw( IO_ERROR )
{
    // This is edge triggered based on a change in 'aLibraryPath',
    // usually it does nothing.  When the edge fires, m_pretty_dir is set
    // to either:
    // 1) empty or
    // 2) a verified and validated, writable, *.pretty directory.

    if( !m_gh_cache || m_lib_path != aLibraryPath )
    {
        delete m_gh_cache;
        m_gh_cache = 0;

        m_pretty_dir.clear();

        if( aProperties )
        {
            string  pretty_dir;

            if( aProperties->Value( PRETTY_DIR, &pretty_dir ) )
            {
                wxString    wx_pretty_dir = FROM_UTF8( pretty_dir.c_str() );

                wx_pretty_dir = FP_LIB_TABLE::ExpandSubstitutions( wx_pretty_dir );

                wxFileName wx_pretty_fn = wx_pretty_dir;

                if( !wx_pretty_fn.IsOk() ||
                    !wx_pretty_fn.IsDirWritable() ||
                    wx_pretty_fn.GetExt() != wxT( "pretty" )
                  )
                {
                    wxString msg = wxString::Format(
                            _( "option '%s' for Github library '%s' must point to a writable directory ending with '.pretty'." ),
                            GetChars( FROM_UTF8( PRETTY_DIR ) ),
                            GetChars( aLibraryPath )
                            );

                    THROW_IO_ERROR( msg );
                }

                m_pretty_dir = wx_pretty_dir;
            }
        }

        // operator==( wxString, wxChar* ) does not exist, construct wxString once here.
        const wxString    kicad_mod( wxT( "kicad_mod" ) );

        //D(printf("%s: this:%p  m_lib_path:'%s'  aLibraryPath:'%s'\n", __func__, this, TO_UTF8( m_lib_path), TO_UTF8(aLibraryPath) );)
        m_gh_cache = new GH_CACHE();

        // INIT_LOGGER( "/tmp", "test.log" );
        remote_get_zip( aLibraryPath );
        // UNINIT_LOGGER();

        m_lib_path = aLibraryPath;

        wxMemoryInputStream mis( &m_zip_image[0], m_zip_image.size() );

        // @todo: generalize this name encoding from a PROPERTY (option) later
        wxZipInputStream    zis( mis, wxConvUTF8 );

        wxZipEntry* entry;

        while( ( entry = zis.GetNextEntry() ) != NULL )
        {
            wxFileName  fn( entry->GetName() );     // chop long name into parts

            if( fn.GetExt() == kicad_mod )
            {
                string fp_name = TO_UTF8( fn.GetName() );   // omit extension & path

                m_gh_cache->insert( fp_name, entry );
            }
            else
                delete entry;
        }
    }
}


bool GITHUB_PLUGIN::repoURL_zipURL( const wxString& aRepoURL, string* aZipURL )
{
    // e.g. "https://github.com/liftoff-sr/pretty_footprints"
    //D(printf("aRepoURL:%s\n", TO_UTF8( aRepoURL ) );)

    wxURI   repo( aRepoURL );

    if( repo.HasServer() && repo.HasPath() )
    {
        // goal: "https://github.com/liftoff-sr/pretty_footprints/archive/master.zip"
        wxString    zip_url( wxT( "https://" ) );

#if 0   // Github issues a redirect for this "master.zip".  i.e.
        //  "https://github.com/liftoff-sr/pretty_footprints/archive/master.zip"
        // would be redirected to:
        //  "https://codeload.github.com/liftoff-sr/pretty_footprints/zip/master"

        // The alternate code path below uses the redirected URL on first attempt
        // to save one HTTP GET hit.  avhttp does the redirect behind the scenes normally.

        zip_url += repo.GetServer();
        zip_url += repo.GetPath();
        zip_url += wxT( '/' );
        zip_url += wxT( "archive/master.zip" );
#else
        zip_url += wxT( "codeload.github.com" );
        zip_url += repo.GetPath();
        zip_url += wxT( '/' );
        zip_url += wxT( "zip/master" );
#endif

        *aZipURL = zip_url.utf8_str();
        return true;
    }
    return false;
}


void GITHUB_PLUGIN::remote_get_zip( const wxString& aRepoURL ) throw( IO_ERROR )
{
    string  zip_url;

    if( !repoURL_zipURL( aRepoURL, &zip_url ) )
    {
        wxString msg = wxString::Format( _( "Unable to parse URL:\n'%s'" ), GetChars( aRepoURL ) );
        THROW_IO_ERROR( msg );
    }

    boost::asio::io_service io;
    avhttp::http_stream     h( io );
    avhttp::request_opts    options;

    options.insert( "Accept",       "application/zip" );
    options.insert( "User-Agent",   "http://kicad-pcb.org" );   // THAT WOULD BE ME.
    h.request_options( options );

    try
    {
        ostringstream os;

        h.open( zip_url );      // only one file, therefore do it synchronously.
        os << &h;

        // Keep zip file byte image in RAM.  That plus the MODULE_MAP will constitute
        // the cache.  We don't cache the MODULEs per se, we parse those as needed from
        // this zip file image.
        m_zip_image = os.str();

        // 4 lines, using SSL, top that.
    }
    catch( boost::system::system_error& e )
    {
        // https "GET" has faild, report this to API caller.
        wxString fmt( _( "Cannot GET zip: '%s'\nfor lib-path: '%s'.\nWhat: '%s'" ) );

        string msg = StrPrintf( TO_UTF8( fmt ),
                zip_url.c_str(),
                TO_UTF8( aRepoURL ),
                e.what() );

        THROW_IO_ERROR( msg );
    }
}


#if 0 && defined(STANDALONE)

int main( int argc, char** argv )
{
    INIT_LOGGER( ".", "test.log" );

    GITHUB_PLUGIN gh;

    try
    {
        wxArrayString fps = gh.FootprintEnumerate(
                wxT( "https://github.com/liftoff-sr/pretty_footprints" ),
                NULL
                );

        for( int i=0; i<(int)fps.Count(); ++i )
        {
            printf("[%d]:%s\n", i, TO_UTF8( fps[i] ) );
        }
    }
    catch( IO_ERROR ioe )
    {
        printf( "%s\n", TO_UTF8(ioe.errorText) );
    }

    UNINIT_LOGGER();

    return 0;
}

#endif
