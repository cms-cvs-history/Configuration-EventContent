/** measure branch sizes
 *
 * \author Luca Lista, INFN
 *
 * \version $Revision: 1.4 $
 *
 */
#include <boost/shared_ptr.hpp>
#include <boost/program_options.hpp>
#include <string>
#include <iostream>
#include <vector>
#include <utility>
#include <cassert>
#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>
#include <TSystem.h>
#include <TStyle.h>
#include <TObjArray.h>
#include <TBranch.h>
#include <TH1.h>
#include <TCanvas.h>
#include <Riostream.h>
#include "FWCore/FWLite/src/AutoLibraryLoader.h"

static const char * const kHelpOpt = "help";
static const char * const kHelpCommandOpt = "help,h";
static const char * const kDataFileOpt = "data-file";
static const char * const kDataFileCommandOpt = "data-file,d";
static const char * const kAutoLoadOpt ="auto-loader";
static const char * const kAutoLoadCommandOpt ="auto-loader,a";
static const char * const kPlotOpt ="plot";
static const char * const kPlotCommandOpt ="plot,p";
static const char * const kSavePlotOpt ="save-plot";
static const char * const kSavePlotCommandOpt ="save-plot,s";
static const char * const kPlotTopOpt ="plot-top";
static const char * const kPlotTopCommandOpt ="plot-top,t";

template<typename A, typename B>
struct sortBySecond {
  bool operator()( const std::pair<A, B> & lhs, const std::pair<A, B> & rhs ) const {
    return lhs.second > rhs.second;
  }
};

size_t GetTotalSize( TBranch * );

size_t GetBasketSize( TBranch * );

size_t GetBasketSize( TObjArray * branches ) {
  size_t result = 0, n = branches->GetEntries();
  for( size_t i = 0; i < n; ++ i ) {
    result += GetBasketSize( dynamic_cast<TBranch*>( branches->At( i ) ) );
  }
  return result;
}

size_t GetBasketSize( TBranch * b ) {
  size_t result = 0;
  if ( b != 0 ) {
    if ( b->GetZipBytes() > 0 ) 
      result = b->GetTotBytes();
    result += GetBasketSize( b->GetListOfBranches() );
  }
  return result;
}

size_t GetTotalSize( TBranch * br ) {
  TBuffer buf( TBuffer::kWrite, 10000 );
  TBranch::Class()->WriteBuffer( buf, br );
  return GetBasketSize( br ) + buf.Length();
}

size_t GetTotalSize( TObjArray * branches ) {
  size_t result = 0, n = branches->GetEntries();
  for( size_t i = 0; i < n; ++ i )
    result += GetTotalSize( dynamic_cast<TBranch*>( branches->At( i ) ) );
  return result;
}

size_t GetTotalSize( TTree *t ) {
  return GetTotalSize( t->GetListOfBranches() );
} 

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
    ( kAutoLoadCommandOpt, "automatic library loading (avoid root warnings)" )
    ( kDataFileCommandOpt, value<string>(), "data file" )
    ( kPlotCommandOpt, value<string>(), "summary plot" )
    ( kPlotTopCommandOpt, value<int>(), "plot only the <arg> top size branches" )
    ( kSavePlotCommandOpt, value<string>(), "save plot into root file <arg>" );

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

  gROOT->SetBatch();
  
  if( vm.count( kAutoLoadOpt ) != 0 ) {
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
  const size_t n =  branches->GetEntries();
  cout << fileName << " has " << n << " branches" << endl;
  for( size_t i = 0; i < n; ++i ) {
    TBranch * b = dynamic_cast<TBranch*>( branches->At( i ) );
    assert( b != 0 );
    string name( b->GetName() );
    if ( name == "EventAux" ) continue;
    size_t s = GetTotalSize( b );
    v.push_back( make_pair( b->GetName(), s ) );
  }
  sort( v.begin(), v.end(), sortBySecond<string, unsigned long>() );
  bool plot = ( vm.count( kPlotOpt ) > 0 );
  bool save = ( vm.count( kSavePlotOpt ) > 0 );
  int top = n;
  if( vm.count( kPlotTopOpt ) > 0 ) top = vm[ kPlotTopOpt ].as<int>();
  TH1F histo( "histo", "branch sizes (uncompressed)", top, -0.5, - 0.5 + top );
  int x = 0;
  size_t totalSize = 0;
  TAxis * xAxis = histo.GetXaxis();
  for( BranchVector::const_iterator b = v.begin(); b != v.end(); ++ b ) {
    string name = b->first.c_str();
    size_t s = b->second;
    cout << s << " bytes :" 
	 << b->first << endl;
    xAxis->SetBinLabel( x + 1, name.c_str() );
    histo.Fill( x ++, s );
    totalSize += s;
  }
  cout << "total branches size: " << GetTotalSize( events ) << " bytes (uncompressed)" << endl;
  xAxis->SetLabelOffset( -0.3 );
  xAxis->LabelsOption( "d" );
  xAxis->SetLabelSize( 0.035 );
  histo.GetYaxis()->SetTitle( "Bytes" );
  histo.SetFillColor( kRed );
  histo.SetLineWidth( 2 );
  if( plot ) {
    string plotName = vm[kPlotOpt].as<string>();
    gROOT->SetStyle( "Plain" );
    gStyle->SetOptStat( kFALSE );
    gStyle->SetOptLogy();
    TCanvas c;
    histo.Draw();
    c.SaveAs( plotName.c_str() );
  }
  if ( save ) {
    string fileName = vm[kSavePlotOpt].as<string>();
    TFile f( fileName.c_str(), "RECREATE" );
    histo.Write();
    f.Close();
  }
  return 0;
}
