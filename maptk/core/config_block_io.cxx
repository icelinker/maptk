/*ckwg +5
 * Copyright 2011-2013 by Kitware, Inc. All Rights Reserved. Please refer to
 * KITWARE_LICENSE.TXT for licensing information, or contact General Counsel,
 * Kitware, Inc., 28 Corporate Drive, Clifton Park, NY 12065.
 */

#include "config_block_io.h"

#include <fstream>
#include <iostream>
#include <vector>

#include <maptk/core/exceptions.h>

#include <boost/algorithm/string/join.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix.hpp>

namespace qi = boost::spirit::qi;
namespace phoenix = boost::phoenix;

namespace maptk
{

/// Basic configuration key/value encapsulation structure
struct config_block_value_s {
  /// the configuration path within the block structure
  config_block_keys_t key_path;

  /// value associated with the given key
  config_block_value_t value;
};

/// The type that represents multiple stored configuration values.
typedef std::vector<config_block_value_s> config_block_value_set_t;

}

// Adapting structure to boost fusion tuples for use in spirit::qi
BOOST_FUSION_ADAPT_STRUCT(
  maptk::config_block_value_s,
  (maptk::config_block_keys_t, key_path)
  (maptk::config_block_value_t, value)
)

namespace maptk
{

namespace
{

/// Token representing the beginning of a block declaration
static token_t const config_block_start = token_t("block");
/// Token representing the end of a block declaration
static token_t const config_block_end = token_t("endblock");
/// Token representing the start of a comment
static token_t const config_comment_start = token_t("#");

/// Grammar definition for configuration files
template <typename Iterator>
class config_block_grammar
  : public qi::grammar<Iterator, config_block_value_set_t()>
{
  public:
    config_block_grammar();
    ~config_block_grammar();

  private:
    /// Optional whitespace (spaces/tabs)
    qi::rule<Iterator> opt_whitespace;
    /// Required whitespace (at least one space/tab)
    qi::rule<Iterator> whitespace;
    /// End-of-line rule (single EoL)
    qi::rule<Iterator> eol;
    /// Optional eol rule (0 or more)
    qi::rule<Iterator> opt_line_end;
    /// Required eol rule (1 or more)
    qi::rule<Iterator> line_end;

    /// Matches a single config_block key element
    qi::rule<Iterator, config_block_key_t()> config_block_key;
    /// Matches full config_block key path
    qi::rule<Iterator, config_block_keys_t()> config_block_key_path;
    /// Matches any valid "value" (printable characters with joining spaces)
    qi::rule<Iterator, config_block_value_t()> config_block_value;
    /// A key/value pair
    qi::rule<Iterator, config_block_value_s()> config_block_value_full;

    /// A comment within the config file
    qi::rule<Iterator> comment;

    /// Result set of config block key/value pairs
    qi::rule<Iterator, config_block_value_set_t()> config_block_value_set;
};

template <typename Iterator>
config_block_grammar<Iterator>
::config_block_grammar()
  : opt_whitespace()
  , whitespace()
  , eol()
  , opt_line_end()
  , line_end()
  , config_block_key()
  , config_block_key_path()
  , config_block_value()
  , config_block_value_full()
  , config_block_value_set()
  , comment()
  , config_block_grammar::base_type(config_block_value_set)
{
  using namespace qi::labels;
  using phoenix::push_back;

  opt_whitespace.name("opt-whitespace");
  opt_whitespace %= *( qi::blank );

  whitespace.name("whitespace");
  whitespace %= +( qi::blank );

  eol.name("eol");
  eol %= qi::lit("\r\n")
       | qi::lit("\n");

  opt_line_end.name("opt-line-end");
  opt_line_end %= *( eol );

  line_end.name("line-end");
  line_end %= +( eol );


  config_block_key.name("config-key");
  config_block_key %=
    +( qi::alnum | qi::char_("_") );

  config_block_key_path.name("config-key-path");
  config_block_key_path %=
    config_block_key
    >> *( qi::lit(maptk::config_block::block_sep)
        > config_block_key
        );

  config_block_value.name("config-value");
  config_block_value %=
    +( qi::graph
     | qi::hold[*qi::blank >> qi::graph]
     );

  config_block_value_full.name("config-value-full");
  config_block_value_full %=
    ( (opt_whitespace >> config_block_key_path >> opt_whitespace)
    > '='
    > (opt_whitespace >> config_block_value >> opt_whitespace) > eol
    );


  comment.name("comment");
  comment %=
    +( opt_whitespace >> config_comment_start >> opt_whitespace >> config_block_value >> eol );


  // Main grammar parsing rule
  config_block_value_set.name("config-values");
  config_block_value_set =
    +( (opt_whitespace >> eol)  // empty line with extra whitespace
     | line_end                 // one or more empty lines
     | config_block_value_full[push_back(_val, _1)]  // keypath/value specification
     | comment                  // comment lines
     );

}

template <typename Iterator>
config_block_grammar<Iterator>
::~config_block_grammar()
{
}

}

/// Read in a configuration file, producing a \c config object.
config_block_t read_config_file(path_t const& file_path)
{
  // Check that file exists
  if( ! boost::filesystem::exists(file_path) )
  {
    throw file_not_found_exception(file_path, "File does not exist.");
  }
  else if ( ! boost::filesystem::is_regular_file(file_path) )
  {
    throw file_not_found_exception(file_path, "Path given doesn't point to "
                                              "a regular file!");
  }

  // Reading in input file data
  std::fstream input_stream(file_path.c_str(), std::fstream::in);
  if( ! input_stream )
  {
    throw file_not_read_exception(file_path, "Could not open file at given "
                                             "path.");
  }
  std::string storage;
  input_stream.unsetf(std::ios::skipws);
  std::copy(std::istream_iterator<char>(input_stream),
            std::istream_iterator<char>(),
            std::back_inserter(storage));
  // std::cerr << "Storage string:" << std::endl
  //           << storage << std::endl;

  // Commence parsing!
  typedef std::string::const_iterator str_iter;
  config_block_value_set_t config_block_values;
  config_block_grammar<str_iter> grammar;
  std::string::const_iterator s_begin = storage.begin();
  std::string::const_iterator s_end = storage.end();

  try
  {
    bool r = qi::parse(s_begin, s_end, grammar, config_block_values);

    if( r && s_begin == s_end )
    {
      std::cerr << "File parsed! Contained " << config_block_values.size() << " entries." << std::endl;
    }
    else if (!r)
    {
      throw file_not_read_exception(file_path, "File not parsed, or parsed completely!");
    }
  }
  catch (qi::expectation_failure<str_iter> const& e)
  {
    throw file_not_parsed_exception(file_path, "Grammar expectation failure");
  }

  // Now that we have the various key/value pairs, construct the config
  // object.
  //using std::cerr;
  //using std::endl;
  //cerr << "Constructing config_block from file:" << endl;
  config_block_t conf = config_block::empty_config();
  BOOST_FOREACH( config_block_value_s kv, config_block_values)
  {
    config_block_key_t key_path = boost::algorithm::join(kv.key_path, config_block::block_sep);
    // std::cerr << "KEY: " << key_path << std::endl;
    // std::cerr << "VALUE: " << kv.value << std::endl << std::endl;
    // add key/value to config object here
    conf->set_value(key_path, kv.value);
    //cerr << "\t`" << key_path << "` -> `" << kv.value << "`" << endl;
  }

  return conf;
}

/// Output to file the given \c config object to the specified file path
void write_config_block_file(config_block_t const& config,
                             path_t const& file_path)
{
}

}
