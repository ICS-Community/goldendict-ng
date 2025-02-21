/* This file is (c) 2008-2012 Konstantin Isakov <ikm@goldendict.org>
 * Part of GoldenDict. Licensed under GPLv3 or later, see the LICENSE file */

#ifndef __DSL_DETAILS_HH_INCLUDED__
#define __DSL_DETAILS_HH_INCLUDED__

#include <string>
#include <list>
#include <vector>
#include <zlib.h>
#include "dictionary.hh"
#include "iconv.hh"
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
#include <QtCore5Compat/QTextCodec>
#else
#include <QTextCodec>
#endif
#include <QByteArray>
#include "utf8.hh"

// Implementation details for Dsl, not part of its interface
namespace Dsl {
namespace Details {

using std::string;
using gd::wstring;
using gd::wchar;
using std::list;
using std::vector;
using Utf8::Encoding;
using Utf8::LineFeed;



struct DSLLangCode
{
  int code_id;
  char code[ 3 ]; // ISO 639-1
};

string findCodeForDslId( int id );

bool isAtSignFirst( wstring const & str );

/// Parses the DSL language, representing it in its structural DOM form.
struct ArticleDom
{
  struct Node: public list< Node >
  {
    bool isTag; // true if it is a tag with subnodes, false if it's a leaf text
                // data.
    // Those are only used if isTag is true
    wstring tagName;
    wstring tagAttrs;
    wstring text; // This is only used if isTag is false

    class Text {};
    class Tag {};

    Node( Tag, wstring const & name, wstring const & attrs ): isTag( true ),
      tagName( name ), tagAttrs( attrs )
    {}

    Node( Text, wstring const & text_ ): isTag( false ), text( text_ )
    {}

    /// Concatenates all childen text nodes recursively to form all text
    /// the node contains stripped of any markup.
    wstring renderAsText( bool stripTrsTag = false ) const;
  };

  /// Does the parse at construction. Refer to the 'root' member variable
  /// afterwards.
  explicit ArticleDom( wstring const &, string const & dictName = string(),
              wstring const & headword_ = wstring() );

  /// Root of DOM's tree
  Node root;

private:

  void openTag( wstring const & name, wstring const & attr, list< Node * > & stack );

  void closeTag( wstring const & name, list< Node * > & stack,
                 bool warn = true );

  bool atSignFirstInLine();

  wchar const * stringPos, * lineStartPos;

  class eot: std::exception {};

  wchar ch;
  bool escaped;
  unsigned transcriptionCount; // >0 = inside a [t] tag
  unsigned mediaCount; // >0 = inside a [s] tag

  void nextChar() ;

  /// Information for diagnostic purposes
  string dictionaryName;
  wstring headword;
};

/// Opens the .dsl or .dsl.dz file and allows line-by-line reading. Auto-detects
/// the encoding, and reads all headers by itself.
class DslScanner
{
  gzFile f;
  Encoding encoding;
  QTextCodec* codec;
  wstring dictionaryName;
  wstring langFrom, langTo;
  wstring soundDictionary;
  char readBuffer[ 65536 ];
  char * readBufferPtr;
  LineFeed lineFeed;
  size_t readBufferLeft;
  //qint64 pos;
  unsigned linesRead;

public:

  DEF_EX( Ex, "Dsl scanner exception", Dictionary::Ex )
  DEF_EX_STR( exCantOpen, "Can't open .dsl file", Ex )
  DEF_EX( exCantReadDslFile, "Can't read .dsl file", Ex )
  DEF_EX_STR( exMalformedDslFile, "The .dsl file is malformed:", Ex )
  DEF_EX( exUnknownCodePage, "The .dsl file specified an unknown code page", Ex )
  DEF_EX( exEncodingError, "Encoding error", Ex ) // Should never happen really

  explicit DslScanner( string const & fileName ) ;
  ~DslScanner() noexcept;

  /// Returns the detected encoding of this file.
  Encoding getEncoding() const
  { return encoding; }

  /// Returns the dictionary's name, as was read from file's headers.
  wstring const & getDictionaryName() const
  { return dictionaryName; }

  /// Returns the dictionary's source language, as was read from file's headers.
  wstring const & getLangFrom() const
  { return langFrom; }

  /// Returns the dictionary's target language, as was read from file's headers.
  wstring const & getLangTo() const
  { return langTo; }

  /// Returns the preferred external dictionary with sounds, as was read from file's headers.
  wstring const & getSoundDictionaryName() const
  { return soundDictionary; }

  /// Reads next line from the file. Returns true if reading succeeded --
  /// the string gets stored in the one passed, along with its physical
  /// file offset in the file (the uncompressed one if the file is compressed).
  /// If end of file is reached, false is returned.
  /// Reading begins from the first line after the headers (ones which start
  /// with #).
  bool readNextLine( wstring &, size_t & offset, bool only_head_word = false ) ;

  /// Similar readNextLine but strip all DSL comments {{...}}
  bool readNextLineWithoutComments( wstring &, size_t & offset, bool only_headword = false ) ;

  /// Returns the number of lines read so far from the file.
  unsigned getLinesRead() const
  { return linesRead; }

  /// Converts the given number of characters to the number of bytes they
  /// would occupy in the file, knowing its encoding. It's possible to know
  /// that because no multibyte encodings are supported in .dsls.
  inline size_t distanceToBytes( size_t ) const;
};

/// This function either removes parts of string enclosed in braces, or leaves
/// them intact. The braces themselves are removed always, though.
void processUnsortedParts( wstring & str, bool strip );

/// Expands optional parts of a headword (ones marked with parentheses),
/// producing all possible combinations where they are present or absent.
void expandOptionalParts( wstring & str, list< wstring > * result,
                          size_t x = 0, bool inside_recurse = false );

/// Expands all unescaped tildes, inserting tildeReplacement text instead of
/// them.
void expandTildes( wstring & str, wstring const & tildeReplacement );

/// Unescapes any escaped chars. Be sure to handle all their special meanings
/// before unescaping them.
void unescapeDsl( wstring & str );

/// Normalizes the headword. Currently turns any sequences of consecutive spaces
/// into a single space.
void normalizeHeadword( wstring & );

/// Strip DSL {{...}} comments
void stripComments( wstring &, bool & );

inline size_t DslScanner::distanceToBytes( size_t x ) const
{
  switch( encoding )
  {
    case Utf8::Utf16LE:
    case Utf8::Utf16BE:
      return x*2;
    default:
      return x;
  }
}

/// Converts the given language name taken from Dsl header (i.e. getLangFrom(),
/// getLangTo()) to its proper language id.
quint32 dslLanguageToId( wstring const & name );

}
}

#endif
