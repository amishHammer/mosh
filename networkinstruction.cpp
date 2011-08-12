#include <endian.h>
#include <assert.h>

#include "networktransport.hpp"

using namespace Network;

static string network_order_string( uint64_t host_order )
{
  uint64_t net_int = htobe64( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

static string network_order_string( uint16_t host_order )
{
  uint16_t net_int = htobe16( host_order );
  return string( (char *)&net_int, sizeof( net_int ) );
}

string Instruction::tostring( void )
{
  string ret;
  
  ret += network_order_string( old_num );
  ret += network_order_string( new_num );
  ret += network_order_string( ack_num );
  ret += network_order_string( throwaway_num );

  assert( !( fragment_num & 0x8000 ) );

  uint16_t combined_fragment_num = ( final << 15 ) | fragment_num;

  ret += network_order_string( combined_fragment_num );

  assert( ret.size() == inst_header_len );

  ret += diff;

  return ret;
}

Instruction::Instruction( string &x )
  : old_num( -1 ), new_num( -1 ), ack_num( -1 ), throwaway_num( -1 ), fragment_num( -1 ), final( false ), diff()
{
  assert( x.size() >= inst_header_len );
  uint64_t *data = (uint64_t *)x.data();
  uint16_t *data16 = (uint16_t *)x.data();
  old_num = be64toh( data[ 0 ] );
  new_num = be64toh( data[ 1 ] );
  ack_num = be64toh( data[ 2 ] );
  throwaway_num = be64toh( data[ 3 ] );
  fragment_num = be16toh( data16[ 16 ] );
  final = ( fragment_num & 0x8000 ) >> 15;
  fragment_num &= 0x7FFF;

  diff = string( x.begin() + inst_header_len, x.end() );
}

bool FragmentAssembly::same_template( Instruction &a, Instruction &b )
{
  return ( a.old_num == b.old_num ) && ( a.new_num == b.new_num ) && ( a.ack_num == b.ack_num )
    && ( a.throwaway_num == b.throwaway_num );
}

bool FragmentAssembly::add_fragment( Instruction &inst )
{
  /* see if this is a totally new packet */
  if ( !same_template( inst, current_template ) ) {
    fragments.clear();
    current_template = inst;
    fragments.resize( inst.fragment_num + 1 );
    fragments.at( inst.fragment_num ) = inst;
    fragments_arrived = 1;
    fragments_total = -1;
  } else { /* not a new packet */
    /* see if we already have this fragment */
    if ( (fragments.size() > inst.fragment_num)
	 && (fragments.at( inst.fragment_num ).old_num != uint64_t(-1)) ) {
      assert( fragments.at( inst.fragment_num ) == inst );
    } else {
      if ( (int)fragments.size() < inst.fragment_num + 1 ) {
	fragments.resize( inst.fragment_num + 1 );
      }
      fragments.at( inst.fragment_num ) = inst;
      fragments_arrived++;
    }
  }

  if ( inst.final ) {
    fragments_total = inst.fragment_num + 1;
    fragments.resize( fragments_total );
  }

  if ( fragments_total != -1 ) {
    assert( fragments_arrived <= fragments_total );
  }

  /* see if we're done */
  return ( fragments_arrived == fragments_total );
}

Instruction FragmentAssembly::get_assembly( void )
{
  assert( fragments_arrived == fragments_total );

  Instruction ret( current_template );
  ret.diff = "";

  for ( int i = 0; i < fragments_total; i++ ) {
    ret.diff += fragments.at( i ).diff;
  }

  fragments.clear();
  fragments_arrived = 0;
  fragments_total = -1;

  return ret;
}

bool Instruction::operator==( const Instruction &x )
{
  return ( old_num == x.old_num ) && ( new_num == x.new_num )
    && ( ack_num == x.ack_num ) && ( throwaway_num == x.throwaway_num )
    && ( fragment_num == x.fragment_num ) && ( diff == x.diff );
}
