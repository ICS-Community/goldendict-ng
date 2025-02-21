/* This file is (c) 2013 Timon Wong <timon86.wang@gmail.com>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#include "voiceengines.hh"
#include "audiolink.hh"
#include "htmlescape.hh"
#include "utf8.hh"
#include "wstring_qt.hh"

#include <string>
#include <map>

#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>

#include "utils.hh"

namespace VoiceEngines
{

using namespace Dictionary;
using std::string;
using std::map;

inline string toMd5( QByteArray const & b )
{
  return string( QCryptographicHash::hash( b, QCryptographicHash::Md5 ).toHex().constData() );
}

class VoiceEnginesDictionary: public Dictionary::Class
{
private:

  Config::VoiceEngine voiceEngine;

public:

  VoiceEnginesDictionary( Config::VoiceEngine const & voiceEngine ):
    Dictionary::Class(
      toMd5( voiceEngine.name.toUtf8() ),
      vector< string >() ),
    voiceEngine( voiceEngine )
  {
  }

  string getName() noexcept override
  { return voiceEngine.name.toUtf8().data(); }

  map< Property, string > getProperties() noexcept override
  { return map< Property, string >(); }

  unsigned long getArticleCount() noexcept override
  { return 0; }

  unsigned long getWordCount() noexcept override
  { return 0; }

  sptr< WordSearchRequest > prefixMatch( wstring const & word,
                                                 unsigned long maxResults ) override
    ;

  sptr< DataRequest > getArticle( wstring const &,
                                          vector< wstring > const & alts,
                                          wstring const &, bool ) override
    ;

protected:

  void loadIcon() noexcept override;
};

sptr< WordSearchRequest > VoiceEnginesDictionary::prefixMatch( wstring const & /*word*/,
                                                               unsigned long /*maxResults*/ )
  
{
  WordSearchRequestInstant * sr = new WordSearchRequestInstant();
  sr->setUncertain( true );
  return std::shared_ptr<WordSearchRequestInstant>(sr);
}

sptr< Dictionary::DataRequest > VoiceEnginesDictionary::getArticle(
  wstring const & word, vector< wstring > const &, wstring const &, bool )
  
{
  string result;
  string wordUtf8( Utf8::encode( word ) );

  result += "<table class=\"voiceengines_play\"><tr>";

  QUrl url;
  url.setScheme( "gdtts" );
  url.setHost( "localhost" );
  url.setPath( Utils::Url::ensureLeadingSlash( QString::fromUtf8( wordUtf8.c_str() ) ) );
  QList< QPair<QString, QString> > query;
  query.push_back( QPair<QString, QString>( "engine", QString::fromStdString( getId() ) ) );
  Utils::Url::setQueryItems( url, query );

  string encodedUrl = url.toEncoded().data();
  string ref = string( "\"" ) + encodedUrl + "\"";
  result += addAudioLink( ref, getId() );

  result += "<td><a href=" + ref + R"(><img src="qrc:///icons/playsound.png" border="0" alt="Play"/></a></td>)";
  result += "<td><a href=" + ref + ">" + Html::escape( wordUtf8 ) + "</a></td>";
  result += "</tr></table>";

  sptr< DataRequestInstant > ret = std::make_shared<DataRequestInstant>( true );
  ret->getData().resize( result.size() );
  memcpy( &( ret->getData().front() ), result.data(), result.size() );
  return ret;
}

void VoiceEnginesDictionary::loadIcon() noexcept
{
  if ( dictionaryIconLoaded )
    return;

  if ( !voiceEngine.iconFilename.isEmpty() )
  {
    QFileInfo fInfo(  QDir( Config::getConfigDir() ), voiceEngine.iconFilename );
    if ( fInfo.isFile() )
      loadIconFromFile( fInfo.absoluteFilePath(), true );
  }
  if ( dictionaryIcon.isNull() )
    dictionaryIcon = dictionaryNativeIcon = QIcon( ":/icons/text2speech.svg" );
  dictionaryIconLoaded = true;
}

vector< sptr< Dictionary::Class > > makeDictionaries(
  Config::VoiceEngines const & voiceEngines )
  
{
  vector< sptr< Dictionary::Class > > result;

  for ( Config::VoiceEngines::const_iterator i = voiceEngines.begin(); i != voiceEngines.end(); ++i )
  {
    if ( i->enabled )
      result.push_back( std::make_shared<VoiceEnginesDictionary>( *i ) );
  }

  return result;
}

}
