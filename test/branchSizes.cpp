#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <cassert>
#include <TFile.h>
#include <TTree.h>
#include <TSystem.h>
#include <TObjArray.h>
#include <TBranch.h>
#include "FWCore/FWLite/src/AutoLibraryLoader.h"

static const char * const kHelpOpt = "help";
static const char * const kHelpCommandOpt = "help,h";
static const char * const kDataFileOpt = "data-file";
static const char * const kDataFileCommandOpt = "data-file,d";
static const char * const kNoAutoLoadOpt ="no-auto-loader";
static const char * const kNoAutoLoadCommandOpt ="no-auto-loader,n";

template<typename A, typename B>
struct sortBySecond {
  bool operator()( const std::pair<A, B> & lhs, const std::pair<A, B> & rhs ) const {
    return lhs.second > rhs.second;
  }
};

int main( int argc, char * argv[] ) {
  using namespace boost::program_options;
  using namespace std;

  string programName( argv[ 0 ] );
  string descString( programName );
  descString += " [options] ";
  descString += "data_file \nAllowed options";
  options_description desc( descString );

  desc.add_options()
    ( kHelpCommandOpt, "produce help message" )
    ( kNoAutoLoadCommandOpt, "don't do autimatic library loading" )
    ( kDataFileCommandOpt, value<string>(), "data file" );

  positional_options_description p;

  p.add( kDataFileOpt, -1 );

  variables_map vm;
  try {
    store( command_line_parser(argc,argv).options(desc).positional(p).run(), vm );
    notify( vm );
  } catch( const error& ) {
    return 7000;
  }

  if( vm.count( kHelpOpt ) ) {
    cout << desc <<std::endl;
    return 0;
  }

  if( ! vm.count( kDataFileOpt ) ) {
    string shortDesc("ConfigFileNotFound");
    cerr << programName << ": no data file given" << endl;
    return 7001;
  }
  
  if( vm.count( kNoAutoLoadOpt ) == 0 ) {
    gSystem->Load( "libFWCoreFWLite" );
    AutoLibraryLoader::enable();
  }

  string fileName = vm[kDataFileOpt].as<string>();
  TFile file( fileName.c_str() );
  if( ! file.IsOpen() ) {
    cerr << programName << ": unable to open data file " << fileName << endl;
    return 7002;
  }

  TObject * o = file.Get( "Events" );
  if ( o == 0 ) {
    cerr << programName << ": no object \"Events\" found in file: " << fileName << endl;
    return 7003;
  }
  TTree * events = dynamic_cast<TTree*>( o );
  if ( events == 0 ) {
    cerr << programName << ": object \"Events\" is not a TTree in file: " << fileName << endl;
    return 7004;
  }
  TObjArray * branches = events->GetListOfBranches();
  if ( branches == 0 ) {
    cerr << programName << ": tree \"Events\" in file " << fileName 
	 << " contains no branches" << endl;
    return 7004;
  }
  typedef vector<pair<string, unsigned long> > BranchVector;
  BranchVector v;
  size_t n =  branches->GetEntries();
  cout << fileName << "has " << n << " branches" << endl;
  for( size_t i = 0; i < n; ++i ) {
    TBranch * b = dynamic_cast<TBranch*>( branches->At( i ) );
    assert( b != 0 );
    v.push_back( make_pair( b->GetName(), b->GetTotalSize() ) );
  }
  sort( v.begin(), v.end(), sortBySecond<string, unsigned long>() );
  size_t totalSize = 0;
  for( BranchVector::const_iterator b = v.begin(); b != v.end(); ++ b ) {
    cout << b->second << " bytes :" 
	 << b->first << endl;
    totalSize += b->second;
  }
  cout << "total branch size: " << totalSize << " bytes" << endl;
  return 0;
}
